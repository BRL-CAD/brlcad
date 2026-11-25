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

#include <filesystem>
#include <memory>
#include <string>

#include "lmdb.h"

#include "bu/app.h"
#include "bu/file.h"
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

struct TestDirectory {
    ~TestDirectory()
    {
	std::error_code error;
	std::filesystem::remove_all(path, error);
    }

    std::filesystem::path path;
};

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
test_workspace(const std::string &path, const Pack &pack, const Manifest &expected,
	       const std::string &name, const Bytes &expected_record, struct bu_vls *message)
{
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
    constexpr size_t required_low_byte = 19;
    constexpr size_t artifact_low_byte = 21;
    CHECK(change_bootstrap(path, major_low_byte, 2));
    CHECK(!Workspace::open(path.c_str(), message) && BU_STR_EQUAL(bu_vls_addr(message), "Unsupported BRL-CAD workspace schema"));
    CHECK(change_bootstrap(path, major_low_byte, 1));
    CHECK(change_bootstrap(path, required_low_byte, 1));
    CHECK(!Workspace::open(path.c_str(), message) &&
	BU_STR_EQUAL(bu_vls_addr(message), "Unsupported BRL-CAD workspace schema, features or artifact encoding"));
    CHECK(change_bootstrap(path, required_low_byte, 0));
    CHECK(change_bootstrap(path, artifact_low_byte, 2));
    CHECK(!Workspace::open(path.c_str(), message) &&
	BU_STR_EQUAL(bu_vls_addr(message), "Unsupported BRL-CAD workspace schema, features or artifact encoding"));
    CHECK(change_bootstrap(path, artifact_low_byte, 1));
    workspace = Workspace::open(path.c_str(), message);
    CHECK(workspace && workspace->read_directory(head, model, message));
    CHECK(head.revision == pack.selected_revision && model == expected);
    return 0;
}

int
main(int argc, const char **argv)
{
    if (argc > 0)
	bu_setprogname(argv[0]);

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
    CHECK(mk_sph(wdb_dbopen(writer.get(), RT_WDB_TYPE_DB_DEFAULT), name.c_str(), center, 2.0) == 0);
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
    auto *directory = db_lookup(source_db.get(), name.c_str(), LOOKUP_QUIET);
    struct bu_external raw = BU_EXTERNAL_INIT_ZERO;
    CHECK(directory && db_get_external(&raw, directory, source_db.get()) == 0);
    const Bytes record(raw.ext_buf, raw.ext_buf + raw.ext_nbytes);
    bu_free_external(&raw);

    struct bu_vls message = BU_VLS_INIT_ZERO;
    const int result = test_workspace((fixture.path / "workspace").string(), edited.pack,
	expected, name, record, &message);
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
