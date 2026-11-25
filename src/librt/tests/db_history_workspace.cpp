/*           D B _ H I S T O R Y _ W O R K S P A C E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
#include "common.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>

#include "lmdb.h"

#include "bu/app.h"
#include "bu/file.h"
#include "bu/process.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "wdb.h"
#include "../db_history_db5.h"
#include "../db_history_workspace.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
	bu_log("History workspace test failed at line %d: %s\n", __LINE__, #expr); \
	return 1; \
    } \
} while (0)

using namespace db_history;
using Database = std::unique_ptr<struct db_i, decltype(&db_close)>;
using Edit = std::unique_ptr<struct db_edit, decltype(&db_edit_abort)>;

constexpr char region_name[] = "workspace.r";
constexpr char pin_ready = 'P';
constexpr int child_timeout_ms = 10000;
constexpr char pruning_unavailable[] = "Workspace pruning is unavailable on this platform";

struct PinProcess {
    ~PinProcess() { stop(); }

    bool start(const char *program, const char *path)
    {
	const char *args[] = {program, "--pin-revision", path, nullptr};
	bu_process_create(&process, args, BU_PROCESS_OUT_EQ_ERR);
	char received = 0;
	return process && bu_process_read_n(process, BU_PROCESS_STDOUT, sizeof(received), &received) == 1 &&
	    received == pin_ready;
    }

    void stop()
    {
	if (!process)
	    return;
	bu_process_terminate(process);
	bu_process_wait_n(&process, child_timeout_ms);
    }

    struct bu_process *process = nullptr;
};

struct TestDirectory {
    ~TestDirectory()
    {
	std::error_code error;
	std::filesystem::remove_all(path, error);
    }

    std::filesystem::path path;
};

static Bytes
read_record(struct db_i *db, const char *name)
{
    const auto *directory = db_lookup(db, name, LOOKUP_QUIET);
    struct bu_external raw = BU_EXTERNAL_INIT_ZERO;
    if (!directory || db_get_external(&raw, directory, db) != 0)
	return {};
    Bytes record(raw.ext_buf, raw.ext_buf + raw.ext_nbytes);
    bu_free_external(&raw);
    return record;
}

static bool
change_bootstrap(const std::string &path, size_t index, uint8_t replacement)
{
    MDB_env *env = nullptr;
    if (mdb_env_create(&env) != MDB_SUCCESS)
	return false;
    const std::unique_ptr<MDB_env, decltype(&mdb_env_close)> environment(env, mdb_env_close);
    if (mdb_env_open(env, path.c_str(), 0, 0660) != MDB_SUCCESS)
	return false;
    MDB_txn *raw = nullptr;
    if (mdb_txn_begin(env, nullptr, 0, &raw) != MDB_SUCCESS)
	return false;
    std::unique_ptr<MDB_txn, decltype(&mdb_txn_abort)> txn(raw, mdb_txn_abort);
    MDB_dbi db;
    char name[] = "workspace";
    MDB_val key{sizeof(name) - 1, name}, found;
    if (mdb_dbi_open(raw, nullptr, 0, &db) != MDB_SUCCESS ||
	mdb_get(raw, db, &key, &found) != MDB_SUCCESS || index >= found.mv_size)
	return false;
    const auto *data = static_cast<const uint8_t *>(found.mv_data);
    Bytes format(data, data + found.mv_size);
    format[index] = replacement;
    MDB_val updated{format.size(), format.data()};
    return mdb_put(raw, db, &key, &updated, 0) == MDB_SUCCESS &&
	mdb_txn_commit(txn.release()) == MDB_SUCCESS;
}

static int
checkout_parent(const char *path, struct bu_vls *message)
{
    auto workspace = Workspace::open(path, message, true);
    WorkspaceHead head, selected;
    Manifest model;
    Bytes payload;
    Revision revision;
    CHECK(workspace && workspace->read_directory(head, model, message));
    CHECK(workspace->fetch_artifact({Kind::Revision, head.revision}, payload, message) &&
	decode_revision(payload, revision) && revision.parent != absent);
    CHECK(workspace->checkout(head, revision.parent, selected, message) == WorkspaceWriteStatus::Ok);
    CHECK(selected.generation == head.generation + 1 && selected.revision == revision.parent);
    return 0;
}

static bool
write_binary(struct db_i *db, const char *name, ByteView payload)
{
    struct bu_external body = BU_EXTERNAL_INIT_ZERO;
    body.ext_buf = const_cast<uint8_t *>(payload.data);
    body.ext_nbytes = payload.size;
    struct bu_external record = BU_EXTERNAL_INIT_ZERO;
    db5_export_object3(&record, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT, name, 0,
	nullptr, &body, DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_8BITINT_U,
	DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED);
    const int status = wdb_export_external(wdb_dbopen(db, RT_WDB_TYPE_DB_DEFAULT), &record,
	name, RT_DIR_NON_GEOM, DB5_MINORTYPE_BINU_8BITINT_U);
    bu_free_external(&record);
    return status == 0;
}

static int
test_bad_global(Workspace &writer, const WorkspaceHead &base, Manifest model, struct bu_vls *message)
{
    uint8_t unterminated[] = {'t', 'i', 't', 'l', 'e'};
    struct bu_external attributes = BU_EXTERNAL_INIT_ZERO;
    attributes.ext_buf = unterminated;
    attributes.ext_nbytes = sizeof(unterminated);
    struct bu_external raw = BU_EXTERNAL_INIT_ZERO;
    db5_export_object3(&raw, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
	DB5_GLOBAL_OBJECT_NAME, DB5HDR_HFLAGS_HIDDEN_OBJECT, &attributes, nullptr,
	DB5_MAJORTYPE_ATTRIBUTE_ONLY, DB5_MINORTYPE_RESERVED,
	DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED);
    RecordArtifact object;
    const bool factored = factor_record({raw.ext_buf, raw.ext_nbytes}, object);
    bu_free_external(&raw);
    CHECK(factored);
    Revision invalid;
    invalid.parent = base.revision;
    invalid.label = "Invalid database attributes";
    auto &global = model.at(DB5_GLOBAL_OBJECT_NAME);
    invalid.changes.emplace(DB5_GLOBAL_OBJECT_NAME, Change{global, object.object_id});
    global = object.object_id;
    Bytes manifest;
    CHECK(encode_manifest(model, manifest));
    invalid.model = identify(Kind::Manifest, manifest);
    const ArtifactIndex artifacts{{{Kind::Object, object.object_id}, object.object}};
    WorkspaceHead actual;
    CHECK(writer.commit(base, invalid, artifacts, actual, message) == WorkspaceWriteStatus::Error);
    CHECK(!writer.fetch_artifact({Kind::Object, object.object_id}, manifest, message));
    return 0;
}

static int
ray_hit(struct application *UNUSED(ap), struct partition *UNUSED(parts), struct seg *UNUSED(segments))
{
    return 1;
}

static int
ray_miss(struct application *UNUSED(ap))
{
    return 0;
}

static int
test_native_geometry(struct db_i *db)
{
    CHECK(db_dirbuild(db) == 0 && db_version(db) == 5);
    const auto *region = db_lookup(db, region_name, LOOKUP_QUIET);
    CHECK(region && (region->d_flags & RT_DIR_REGION));

    const std::unique_ptr<struct rt_i, decltype(&rt_i_destroy)> rtip(rt_i_create(db), rt_i_destroy);
    CHECK(rtip && rt_gettree(rtip.get(), region_name) == 0);
    rt_prep(rtip.get());
    struct application ray;
    RT_APPLICATION_INIT(&ray);
    ray.a_rt_i = rtip.get();
    ray.a_hit = ray_hit;
    ray.a_miss = ray_miss;
    VSET(ray.a_ray.r_pt, -5.0, 0.0, 0.0);
    VSET(ray.a_ray.r_dir, 1.0, 0.0, 0.0);
    CHECK(rt_shootray(&ray) == 1);
    return 0;
}

static int
test_writes(const char *path, const char *program, struct db_i *source, const EmbeddedPack &initial,
	    const Pack &edited, struct bu_vls *message)
{
    CHECK(Workspace::create(path, initial.pack, message));
    auto reader = Workspace::open(path, message);
    auto writer = Workspace::open(path, message, true);
    auto second = Workspace::open(path, message, true);
    CHECK(reader && writer && second);
    WorkspaceHead base, actual, selected;
    Manifest model;
    CHECK(reader->read_directory(base, model, message) && model == initial.model);
    CHECK(test_bad_global(*writer, base, model, message) == 0);

    // The portable DB5 writer and the workspace use identical artifacts.
    Database pinned(Workspace::open_snapshot(path, actual, message), db_close);
    CHECK(pinned && actual.revision == base.revision && test_native_geometry(pinned.get()) == 0);
    const std::string original_title(pinned->dbi_title);
    const Bytes original_global = read_record(pinned.get(), DB5_GLOBAL_OBJECT_NAME);
    CHECK(!original_global.empty());
    auto *region = db_lookup(pinned.get(), region_name, LOOKUP_QUIET);
    CHECK(region && db_dirdelete(pinned.get(), region) < 0);
    Edit native_edit(db_edit_begin(pinned.get(), "Change the title"), db_edit_abort);
    CHECK(native_edit && db5_update_ident(db_edit_view(native_edit.get()), "Edited title", 1.0) == 0);
    Db5HistoryDraft native_revision;
    CHECK(append_db5_history(initial, native_edit.get(), 1, native_revision) == AppendStatus::Ready);
    CHECK(native_revision.pack.selected_revision == edited.selected_revision);
    Revision edit;
    CHECK(read_revision(edited, edited.selected_revision, edit));
    CHECK(reader->commit(base, edit, edited.artifacts, actual, message) == WorkspaceWriteStatus::Error);
    CHECK(writer->commit(base, edit, native_revision.pack.artifacts, selected, message) == WorkspaceWriteStatus::Ok);
    CHECK(selected.revision == edited.selected_revision && selected.model == edited.selected_model &&
	selected.generation == base.generation + 1);
    Database newer(Workspace::open_snapshot(path, actual, message), db_close);
    CHECK(newer && actual.revision == selected.revision && BU_STR_EQUAL(newer->dbi_title, "Edited title") &&
	original_title == pinned->dbi_title);
    const Database historical(Workspace::open_revision(path, base.revision, actual, message), db_close);
    CHECK(historical && actual.revision == base.revision && actual.model == base.model &&
	historical->dbi_read_only && original_title == historical->dbi_title);
    CHECK(read_record(historical.get(), DB5_GLOBAL_OBJECT_NAME) == original_global);
    CHECK(reader->read_directory(actual, model, message) && actual.revision == selected.revision);
    CHECK(read_record(pinned.get(), DB5_GLOBAL_OBJECT_NAME) == original_global);
    CHECK(test_native_geometry(pinned.get()) == 0);
    CHECK(second->checkout(base, base.revision, actual, message) == WorkspaceWriteStatus::Stale);
    const std::string conflict = bu_vls_addr(message);
    CHECK(actual.revision == selected.revision && conflict.find(id_string(base.revision)) != std::string::npos &&
	conflict.find(id_string(selected.revision)) != std::string::npos);

    WorkspaceHead back;
    CHECK(second->checkout(selected, base.revision, back, message) == WorkspaceWriteStatus::Ok);
    CHECK(back.revision == base.revision && back.model == base.model && back.generation == base.generation + 2);
    CHECK(writer->commit(base, edit, edited.artifacts, actual, message) == WorkspaceWriteStatus::Stale);
    CHECK(actual.revision == base.revision && actual.generation == back.generation);
    CHECK(writer->checkout(back, back.revision, actual, message) == WorkspaceWriteStatus::Ok);
    CHECK(actual.generation == back.generation);

    // Stage two additions. The first body exceeds the initial workspace
    // map size and forces a retry of the complete write transaction.
    constexpr size_t body_size = 24 * 1024 * 1024;
    constexpr uint8_t body_byte = 0x2a;
    const Bytes large_body(body_size, body_byte);
    const Bytes short_body{body_byte};
    constexpr char first[] = "a-large.binary";
    constexpr char last[] = "z-small.binary";
    Edit additions(db_edit_begin(source, "Add binary records"), db_edit_abort);
    CHECK(additions && write_binary(db_edit_view(additions.get()), first, large_body) &&
	write_binary(db_edit_view(additions.get()), last, short_body));
    Db5HistoryDraft batch;
    CHECK(append_db5_history(initial, additions.get(), 2, batch) == AppendStatus::Ready);
    CHECK(read_revision(batch.pack, batch.pack.selected_revision, edit));
    const ArtifactKey first_object{Kind::Object, edit.changes.at(first).after};
    const ArtifactKey last_object{Kind::Object, edit.changes.at(last).after};
    auto incomplete = batch.pack.artifacts;
    CHECK(incomplete.erase(last_object) == 1);
    CHECK(writer->commit(back, edit, incomplete, actual, message) == WorkspaceWriteStatus::Error);
    CHECK(reader->read_directory(actual, model, message) && actual.generation == back.generation && model == initial.model);
    Bytes payload;
    CHECK(!reader->fetch_artifact(first_object, payload, message));
    CHECK(writer->commit(back, edit, batch.pack.artifacts, selected, message) == WorkspaceWriteStatus::Ok);
    CHECK(selected.revision == batch.pack.selected_revision && selected.generation == back.generation + 1);
    CHECK(reader->read_directory(actual, model, message) && actual.revision == selected.revision &&
	model.at(first) == first_object.second && model.at(last) == last_object.second);
    Bytes record;
    CHECK(reader->fetch_record(first_object.second, record, message));
    RecordArtifact factored;
    CHECK(factor_record(record, factored) && factored.body_id == identify(Kind::Body, large_body));

    WorkspaceRetention policy{1, {}};
    WorkspacePrunePreview preview;
    const bool pruning = writer->preview_prune(selected, policy, preview, message);
    CHECK(pruning || BU_STR_EQUAL(bu_vls_addr(message), pruning_unavailable));
    PinProcess pin;
    if (pruning)
	CHECK(pin.start(program, path));

    // An independently opened process sees the entire write and
    // publishes its checkout for these open readers to observe.
    const char *args[] = {program, "--checkout-parent", path, nullptr};
    struct bu_process *child = nullptr;
    bu_process_create(&child, args, BU_PROCESS_OUT_EQ_ERR);
    CHECK(child && bu_process_wait_n(&child, child_timeout_ms) == 0);
    CHECK(reader->read_directory(actual, model, message) && actual.revision == base.revision &&
	actual.generation == selected.generation + 1 && model == initial.model);
    if (!pruning)
	return 0;

    // A reader process pins the branch it opened even after checkout.
    policy.drop.insert(selected.revision);
    CHECK(!writer->preview_prune(actual, policy, preview, message) &&
	std::string_view(bu_vls_addr(message)).find("reader-pinned") != std::string_view::npos);
    pin.stop();
    CHECK(!pin.process);
    CHECK(writer->preview_prune(actual, policy, preview, message) &&
	preview.expired_revisions == policy.drop && preview.expired_bytes >= body_size);
    const auto previous_token = id_string(preview.token);
    WorkspaceHead marked, pruned;
    CHECK(writer->mark(actual, "root", false, marked, message) == WorkspaceWriteStatus::Ok);
    CHECK(writer->prune(marked, policy, previous_token, preview, pruned, message) == WorkspaceWriteStatus::Error);
    CHECK(writer->preview_prune(marked, policy, preview, message));
    CHECK(writer->prune(marked, policy, id_string(preview.token), preview, pruned, message) == WorkspaceWriteStatus::Ok);
    CHECK(pruned.generation == marked.generation + 1 && pruned.revision == base.revision);
    CHECK(!reader->fetch_artifact({Kind::Body, factored.body_id}, payload, message));
    CHECK(writer->verify(message) && test_native_geometry(newer.get()) == 0);
    return 0;
}

static int
test_compact_checkout(const char *path, Edit stage, const Db5HistoryDraft &edited, struct bu_vls *message)
{
    EmbeddedPack prior;
    prior.pack = edited.pack;
    CHECK(read_manifest(prior.pack, prior.pack.selected_model, prior.model));
    Database source(db_edit_commit(stage.get()), db_close);
    CHECK(source);
    stage.release();
    Edit edit(db_edit_begin(source.get(), "Third title"), db_edit_abort);
    CHECK(edit && db5_update_ident(db_edit_view(edit.get()), "Third title", 1.0) == 0);
    Db5HistoryDraft next;
    CHECK(append_db5_history(prior, edit.get(), 3, next) == AppendStatus::Ready);
    const auto initial = next.pack.references.at("before");
    next.pack.references.clear();
    const ArtifactKey reused_model{Kind::Manifest, prior.pack.selected_model};
    CHECK(next.pack.artifacts.erase(reused_model) == 1);
    CHECK(Workspace::create(path, next.pack, message));
    auto workspace = Workspace::open(path, message, true);
    WorkspaceHead latest, selected;
    Manifest model;
    CHECK(workspace && workspace->read_directory(latest, model, message));
    Bytes payload;
    CHECK(!workspace->fetch_artifact(reused_model, payload, message));
    WorkspaceRetention policy{2, {}};
    WorkspacePrunePreview preview;
    if (workspace->preview_prune(latest, policy, preview, message)) {
	CHECK(preview.expired_revisions == std::set<Id>{initial} &&
	    preview.boundaries == std::set<Id>{prior.pack.selected_revision});
	CHECK(workspace->prune(latest, policy, id_string(preview.token), preview, selected, message) == WorkspaceWriteStatus::Ok);
	CHECK(workspace->verify(message) && workspace->fetch_artifact(reused_model, payload, message));
	latest = selected;
    } else {
	CHECK(BU_STR_EQUAL(bu_vls_addr(message), pruning_unavailable));
    }
    CHECK(workspace->checkout(latest, prior.pack.selected_revision, selected, message) == WorkspaceWriteStatus::Ok);
    CHECK(workspace->read_directory(selected, model, message) && model == prior.model);
    CHECK(selected.model == reused_model.second && selected.revision == prior.pack.selected_revision &&
	selected.generation == latest.generation + 1);
    CHECK(workspace->fetch_artifact(reused_model, payload, message));
    return 0;
}

static int
test_workspace(const std::string &path, const Pack &pack, const Manifest &expected,
	       const std::string &name, const Bytes &expected_record, struct bu_vls *message)
{
    Pack incomplete = pack;
    const auto body = std::find_if(incomplete.artifacts.begin(), incomplete.artifacts.end(),
	[](const auto &item) { return item.first.first == Kind::Body; });
    CHECK(body != incomplete.artifacts.end());
    incomplete.artifacts.erase(body);
    const auto invalid = path + "-incomplete";
    CHECK(!Workspace::create(invalid.c_str(), incomplete, message) && !std::filesystem::exists(invalid));
    CHECK(Workspace::create(path.c_str(), pack, message));
    CHECK(!Workspace::create(path.c_str(), pack, message));
    auto workspace = Workspace::open(path.c_str(), message);
    auto another = Workspace::open(path.c_str(), message);
    CHECK(workspace && another);
    workspace.reset();

    WorkspaceHead head;
    Manifest model;
    CHECK(another->read_directory(head, model, message));
    CHECK(head.revision == pack.selected_revision && head.model == pack.selected_model &&
	head.generation == 1 && model == expected);
    Id id;
    CHECK(another->lookup(name, id, message) && id == model.at(name));
    Bytes record;
    CHECK(another->fetch_record(id, record, message) && record == expected_record);
    CHECK(another->lookup("missing.s", id, message) && id == absent);

    Bytes archived;
    const auto parent = pack.references.at("before");
    Revision loaded;
    CHECK(another->fetch_artifact({Kind::Revision, parent}, archived, message) &&
	decode_revision(archived, loaded) && loaded.parent == absent);
    another.reset();

    constexpr size_t major_low_byte = 9;
    constexpr size_t minor_low_byte = 11;
    constexpr size_t required_low_byte = 19;
    constexpr size_t artifact_low_byte = 21;
    constexpr uint8_t reader_pin_bit = 1;
    constexpr uint8_t unknown_feature_bit = 2;
    CHECK(change_bootstrap(path, major_low_byte, 2));
    CHECK(!Workspace::open(path.c_str(), message) && BU_STR_EQUAL(bu_vls_addr(message), "Unsupported BRL-CAD workspace schema"));
    CHECK(change_bootstrap(path, major_low_byte, 1));
    CHECK(change_bootstrap(path, required_low_byte, unknown_feature_bit));
    CHECK(!Workspace::open(path.c_str(), message) &&
	BU_STR_EQUAL(bu_vls_addr(message), "Unsupported BRL-CAD workspace schema, features or artifact encoding"));
    CHECK(change_bootstrap(path, required_low_byte, reader_pin_bit));
    CHECK(change_bootstrap(path, artifact_low_byte, 2));
    CHECK(!Workspace::open(path.c_str(), message) &&
	BU_STR_EQUAL(bu_vls_addr(message), "Unsupported BRL-CAD workspace schema, features or artifact encoding"));
    CHECK(change_bootstrap(path, artifact_low_byte, 1));
    workspace = Workspace::open(path.c_str(), message);
    CHECK(workspace && workspace->read_directory(head, model, message));
    CHECK(head.revision == pack.selected_revision && model == expected);
    workspace.reset();

    // A 1.0 workspace can still be read and copied into the new format.
    CHECK(change_bootstrap(path, minor_low_byte, 0) && change_bootstrap(path, required_low_byte, 0));
    workspace = Workspace::open(path.c_str(), message);
    CHECK(workspace && workspace->read_directory(head, model, message));
    WorkspaceRetention policy{1, {}};
    WorkspacePrunePreview preview;
    CHECK(!workspace->preview_prune(head, policy, preview, message));
    const std::string migration_message = bu_vls_addr(message);
    const bool pruning = migration_message != pruning_unavailable;
    CHECK(!pruning || migration_message.find("Copy this workspace") != std::string::npos);
    const auto destination = path + "-migrated";
    CHECK(workspace->copy_to(head, destination.c_str(), message));
    auto migrated = Workspace::open(destination.c_str(), message);
    WorkspaceHead copied;
    CHECK(migrated && migrated->read_directory(copied, model, message) &&
	copied.revision == head.revision && copied.model == head.model && model == expected && migrated->verify(message));
    CHECK(!workspace->preview_prune(head, policy, preview, message));
    if (pruning)
	CHECK(migrated->preview_prune(copied, policy, preview, message));
    return 0;
}

int
main(int argc, const char **argv)
{
    if (argc > 0)
	bu_setprogname(argv[0]);

    if (argc == 3 && BU_STR_EQUAL(argv[1], "--checkout-parent"))
	return checkout_parent(argv[2], nullptr);
    if (argc == 3 && BU_STR_EQUAL(argv[1], "--pin-revision")) {
	WorkspaceHead head;
	Database pinned(Workspace::open_snapshot(argv[2], head, nullptr), db_close);
	if (!pinned || fputc(pin_ready, stdout) == EOF || fflush(stdout) == EOF)
	    return 1;
	(void)fgetc(stdin);
	return 0;
    }

    char path[MAXPATHLEN] = {};
    FILE *temporary = bu_temp_file(path, sizeof(path));
    CHECK(temporary && fclose(temporary) == 0 && bu_file_delete(path));
    CHECK(std::filesystem::create_directory(path));
    const TestDirectory fixture{path};
    const auto source = (fixture.path / "source.g").string();
    Database writer(db_create(source.c_str(), 5), db_close);
    CHECK(writer);
    // Names longer than the typical LMDB key limit round-trip intact.
    const std::string name = std::string(700, 'a') + ".s";
    const point_t center = VINIT_ZERO;
    auto *wdb = wdb_dbopen(writer.get(), RT_WDB_TYPE_DB_DEFAULT);
    CHECK(mk_sph(wdb, name.c_str(), center, 2.0) == 0);
    CHECK(mk_region1(wdb, region_name, name.c_str(), nullptr, nullptr, nullptr) == 0);
    writer.reset();

    Database source_db(db_open(source.c_str(), DB_OPEN_READONLY), db_close);
    CHECK(source_db && db_dirbuild(source_db.get()) == 0);
    Db5HistoryDraft seed;
    CHECK(seed_db5_history(source_db.get(), "Initial model", 0, seed));
    EmbeddedPack initial;
    initial.pack = seed.pack;
    CHECK(read_manifest(initial.pack, initial.pack.selected_model, initial.model));
    Edit edit(db_edit_begin(source_db.get(), "Change the title"), db_edit_abort);
    CHECK(edit && db5_update_ident(db_edit_view(edit.get()), "Edited title", 1.0) == 0);
    Db5HistoryDraft edited;
    CHECK(append_db5_history(initial, edit.get(), 1, edited) == AppendStatus::Ready);
    edited.pack.references.emplace("before", initial.pack.selected_revision);
    Manifest expected;
    CHECK(read_manifest(edited.pack, edited.pack.selected_model, expected));
    const Bytes record = read_record(source_db.get(), name.c_str());
    CHECK(!record.empty());

    struct bu_vls message = BU_VLS_INIT_ZERO;
    int result = test_workspace((fixture.path / "workspace").string(), edited.pack,
	expected, name, record, &message);
    if (!result)
	result = test_writes((fixture.path / "writer").string().c_str(), argv[0], source_db.get(),
	    initial, edited.pack, &message);
    if (!result)
	result = test_compact_checkout((fixture.path / "compact").string().c_str(), std::move(edit), edited, &message);
    if (result)
	bu_log("%s\n", bu_vls_addr(&message));
    bu_vls_free(&message);
    return result;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
