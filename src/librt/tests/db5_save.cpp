/*                     D B 5 _ S A V E . C P P
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

#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "bio.h"
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__)
#  include <winioctl.h>
#endif
#include "bu/app.h"
#include "bu/file.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "wdb.h"
#include "../db_history_db5.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
	bu_log("DB5 save test failed at line %d: %s\n", __LINE__, #expr); \
	return 1; \
    } \
} while (0)

using Database = std::unique_ptr<struct db_i, decltype(&db_close)>;
using Save = std::unique_ptr<struct db5_save, decltype(&db5_save_free)>;
static auto close_file = [](FILE *file) { fclose(file); };
using File = std::unique_ptr<FILE, decltype(close_file)>;

static constexpr int test_skipped = 77;
static constexpr char replacement_unavailable[] = "Atomic DB5 replacement is unavailable on this platform";

static std::vector<uint8_t>
record_bytes(const struct db_i *dbip, const char *name)
{
    struct bu_external record = BU_EXTERNAL_INIT_ZERO;
    const auto *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (!dp || db_get_external(&record, dp, dbip) < 0)
	return {};
    std::vector<uint8_t> bytes(record.ext_buf, record.ext_buf + record.ext_nbytes);
    bu_free_external(&record);
    return bytes;
}

static struct bu_external
borrow_record(const std::vector<uint8_t> &bytes)
{
    struct bu_external record = BU_EXTERNAL_INIT_ZERO;
    record.ext_buf = const_cast<uint8_t *>(bytes.data());
    record.ext_nbytes = bytes.size();
    return record;
}

static int
verify_export(const struct db_i *base, const std::vector<uint8_t> &original_record,
	      struct bu_vls *error)
{
    char path[MAXPATHLEN] = {};
    FILE *file = bu_temp_file(path, sizeof(path));
    CHECK(file && fclose(file) == 0 && bu_file_delete(path));
    const auto status = db_history::export_db5_snapshot(base, path, nullptr, error);
    if (status == DB5_SAVE_ERROR &&
	BU_STR_EQUAL(bu_vls_addr(error), "Atomic DB5 export is unavailable on this platform"))
	return test_skipped;
    CHECK(status == DB5_SAVE_OK);
    Database exported(db_open(path, DB_OPEN_READONLY), db_close);
    CHECK(exported && db_dirbuild(exported.get()) == 0);
    CHECK(record_bytes(exported.get(), "shape.s") == original_record);
    exported.reset();
    CHECK(bu_file_delete(path));

    constexpr int sentinel = 'X';
    file = fopen(path, "wb");
    CHECK(file && fputc(sentinel, file) == sentinel && fclose(file) == 0);
    CHECK(db_history::export_db5_snapshot(base, path, nullptr, error) == DB5_SAVE_ERROR);
    file = fopen(path, "rb");
    CHECK(file);
    CHECK(fgetc(file) == sentinel && fgetc(file) == EOF && fclose(file) == 0);
    CHECK(bu_file_delete(path));
    return 0;
}

static int
verify_embedded(const char *path, struct db_i *initial, const struct db_i *different, struct bu_vls *error)
{
    using namespace db_history;

    Database plain(db_open(path, DB_OPEN_READONLY), db_close);
    CHECK(plain && db_dirbuild(plain.get()) == 0);
    EmbeddedPack archive;
    CHECK(read_db5_history(plain.get(), archive) == EmbeddedStatus::Missing);
    Db5HistoryDraft root, wrong;
    CHECK(seed_db5_history(plain.get(), "Initial model", 0, root));
    CHECK(seed_db5_history(different, "Different model", 0, wrong));
    Save first(db5_save_begin(initial, error), db5_save_free);
    if (!first && BU_STR_EQUAL(bu_vls_addr(error), replacement_unavailable))
	return test_skipped;
    struct db_i *replacement = nullptr;
    CHECK(first && save_db5_history(first.get(), plain.get(), wrong.pack, &replacement, error) == DB5_SAVE_ERROR);
    CHECK(!replacement && std::string_view(bu_vls_addr(error)).find("differs") != std::string_view::npos);
    CHECK(save_db5_history(first.get(), plain.get(), root.pack, &replacement, error) == DB5_SAVE_OK);
    Database installed(replacement, db_close);
    CHECK(installed && read_db5_history(plain.get(), archive) == EmbeddedStatus::Missing);

    Database first_reader(db_open(path, DB_OPEN_READONLY), db_close);
    CHECK(first_reader && db_dirbuild(first_reader.get()) == 0);
    CHECK(read_db5_history(first_reader.get(), archive) == EmbeddedStatus::Current);
    CHECK(archive.pack.selected_revision == root.pack.selected_revision);
    const auto *entry = db_lookup(first_reader.get(), archive_name, LOOKUP_QUIET);
    CHECK(entry && (entry->d_flags & RT_DIR_HIDDEN) && (entry->d_flags & RT_DIR_NON_GEOM));
    const auto before = record_bytes(first_reader.get(), "shape.s");
    CHECK(before == record_bytes(plain.get(), "shape.s"));

    using Edit = std::unique_ptr<struct db_edit, decltype(&db_edit_abort)>;
    {
	Edit no_op(db_edit_begin(first_reader.get(), "No model change"), db_edit_abort);
	Db5HistoryDraft unchanged;
	CHECK(no_op && append_db5_history(archive, no_op.get(), 1, unchanged) == AppendStatus::Unchanged);

	Edit attributes(db_edit_begin(first_reader.get(), "Update an attribute"), db_edit_abort);
	CHECK(attributes && db5_update_attribute("shape.s", "comment", "metadata", db_edit_view(attributes.get())) == 0);
	Db5HistoryDraft shared;
	CHECK(append_db5_history(archive, attributes.get(), 1, shared) == AppendStatus::Ready);
	CHECK(shared.pack.artifacts.size() == archive.pack.artifacts.size() + 3);
	for (const auto &artifact : shared.owned)
	    CHECK(artifact.first.first != Kind::Body);
    }
    Edit edit(db_edit_begin(first_reader.get(), "Change radius"), db_edit_abort);
    const point_t center = VINIT_ZERO;
    CHECK(edit && mk_sph(wdb_dbopen(db_edit_view(edit.get()), RT_WDB_TYPE_DB_DEFAULT), "shape.s", center, 3.0) == 0);
    const auto after = record_bytes(db_edit_view(edit.get()), "shape.s");
    CHECK(!after.empty() && after != before);
    Db5HistoryDraft next;
    Revision revision;
    CHECK(append_db5_history(archive, edit.get(), 1, next) == AppendStatus::Ready);
    CHECK(read_revision(next.pack, next.pack.selected_revision, revision));
    CHECK(revision.parent == archive.pack.selected_revision && revision.changes.count("shape.s") == 1);
    Manifest model;
    CHECK(read_manifest(next.pack, next.pack.selected_model, model));

    Database staged(db_edit_commit(edit.get()), db_close);
    CHECK(staged);
    edit.release();
    Save second(db5_save_begin(installed.get(), error), db5_save_free);
    CHECK(second && save_db5_history(second.get(), staged.get(), next.pack, &replacement, error) == DB5_SAVE_OK);
    Database newest(replacement, db_close);
    CHECK(newest && record_bytes(newest.get(), "shape.s") == after);
    CHECK(record_bytes(first_reader.get(), "shape.s") == before);
    staged.reset();

    EmbeddedPack opened;
    CHECK(read_db5_history(newest.get(), opened) == EmbeddedStatus::Current);
    CHECK(opened.pack.selected_revision == next.pack.selected_revision && opened.model == model);
    Revision prior;
    CHECK(read_revision(opened.pack, revision.parent, prior) && prior.label == "Initial model");
    CHECK(verify_pack(opened.pack, error));
    {
	const auto child = next.pack.selected_revision;
	const auto parent = root.pack.selected_revision;
	Pack broken = opened.pack;
	broken.preferences.emplace(child, parent);
	CHECK(!verify_pack(broken, error) && std::string_view(bu_vls_addr(error)).find("preferred redo") != std::string_view::npos);
	broken = opened.pack;
	broken.artifacts.erase({Kind::Revision, parent});
	CHECK(!verify_pack(broken, error) && std::string_view(bu_vls_addr(error)).find("missing its recorded parent") != std::string_view::npos);
	broken.boundaries.insert(child);
	CHECK(verify_pack(broken, error));

	broken = opened.pack;
	const auto body = broken.artifacts.lower_bound({Kind::Body, absent});
	CHECK(body != broken.artifacts.end() && body->first.first == Kind::Body);
	Bytes damaged(body->second.data, body->second.data + body->second.size);
	damaged.front() ^= 1;
	body->second = damaged;
	CHECK(!verify_pack(broken, error) && std::string_view(bu_vls_addr(error)).find("does not match its ID") != std::string_view::npos);
	broken.artifacts.erase(body);
	CHECK(!verify_pack(broken, error) && std::string_view(bu_vls_addr(error)).find("missing its body") != std::string_view::npos);

	Db5HistoryDraft bad_change;
	bad_change.pack = opened.pack;
	Manifest before_model;
	CHECK(read_manifest(opened.pack, prior.model, before_model));
	Revision wrong_parent = revision;
	auto &change = wrong_parent.changes.at("shape.s");
	std::swap(change.before, change.after);
	CHECK(bad_change.record(before_model, wrong_parent));
	CHECK(!verify_pack(bad_change.pack, error) && std::string_view(bu_vls_addr(error)).find("recorded parent") != std::string_view::npos);
    }

    for (const auto &target : {root.pack.selected_revision, next.pack.selected_revision}) {
	Database reader(db_open(path, DB_OPEN_READONLY), db_close);
	CHECK(reader && db_dirbuild(reader.get()) == 0);
	Edit travel(db_edit_begin(reader.get(), "Move selected revision"), db_edit_abort);
	Db5HistoryDraft checked_out;
	CHECK(travel && stage_db5_checkout(opened, target, travel.get(), checked_out));
	Database checkout(db_edit_commit(travel.get()), db_close);
	CHECK(checkout);
	travel.release();
	Save move(db5_save_begin(newest.get(), error), db5_save_free);
	CHECK(move && save_db5_history(move.get(), checkout.get(), checked_out.pack,
		&replacement, error) == DB5_SAVE_OK);
	newest.reset(replacement);
	CHECK(read_db5_history(newest.get(), opened) == EmbeddedStatus::Current);
	CHECK(opened.pack.selected_revision == target);
	CHECK(record_bytes(newest.get(), "shape.s") == (target == root.pack.selected_revision ? before : after));
    }
    CHECK(opened.pack.preferences.at(root.pack.selected_revision) == next.pack.selected_revision);

    // A legacy edit changes the model independently of the hidden pack.
    CHECK(mk_sph(wdb_dbopen(newest.get(), RT_WDB_TYPE_DB_DEFAULT), "shape.s", center, 4.0) == 0);
    db_sync(newest.get());
    CHECK(read_db5_history(newest.get(), opened) == EmbeddedStatus::ExternalEdits);
    CHECK(opened.pack.selected_revision == next.pack.selected_revision && opened.model == model);
    const auto legacy_record = record_bytes(newest.get(), "shape.s");
    CHECK(legacy_record != after);

    // Unsupported versions remain unchanged by a history-aware save.
    auto newer = record_bytes(newest.get(), archive_name);
    struct db5_raw_internal raw;
    CHECK(db5_get_raw_internal_bounded(&raw, newer.data(), newer.size()));
    const size_t signature_offset = static_cast<size_t>(raw.body.ext_buf - newer.data());
    constexpr size_t selected_id_offset = 28;
    newer[signature_offset + selected_id_offset] ^= 1;
    auto external = borrow_record(newer);
    auto *archive_entry = db_lookup(newest.get(), archive_name, LOOKUP_QUIET);
    CHECK(archive_entry && db_put_external5(&external, archive_entry, newest.get()) == 0);
    db_sync(newest.get());
    CHECK(read_db5_history(newest.get(), opened) == EmbeddedStatus::Malformed);
    newer[signature_offset + selected_id_offset] ^= 1;
    constexpr size_t major_version_low_byte = 9;
    newer[signature_offset + major_version_low_byte]++;
    CHECK(archive_entry && db_put_external5(&external, archive_entry, newest.get()) == 0);
    db_sync(newest.get());
    CHECK(read_db5_history(newest.get(), opened) == EmbeddedStatus::Unsupported);
    Save unsupported(db5_save_begin(newest.get(), error), db5_save_free);
    CHECK(unsupported && save_db5_history(unsupported.get(), newest.get(), next.pack, &replacement, error) == DB5_SAVE_ERROR);
    CHECK(!replacement && record_bytes(newest.get(), archive_name) == newer);
    CHECK(record_bytes(newest.get(), "shape.s") == legacy_record);

    char export_path[MAXPATHLEN] = {};
    FILE *target = bu_temp_file(export_path, sizeof(export_path));
    CHECK(target && fclose(target) == 0 && bu_file_delete(export_path));
    CHECK(export_db5_snapshot(newest.get(), export_path, nullptr, error) == DB5_SAVE_OK);
    Database plain_export(db_open(export_path, DB_OPEN_READONLY), db_close);
    CHECK(plain_export && db_dirbuild(plain_export.get()) == 0);
    CHECK(!db_lookup(plain_export.get(), archive_name, LOOKUP_QUIET));
    CHECK(record_bytes(plain_export.get(), "shape.s") == legacy_record);
    plain_export.reset();

    // A user object with the archive wrapper but no signature is a collision.
    newer[signature_offset] ^= 1;
    external = borrow_record(newer);
    CHECK(db_put_external5(&external, archive_entry, newest.get()) == 0);
    db_sync(newest.get());
    CHECK(read_db5_history(newest.get(), opened) == EmbeddedStatus::Collision);
    Save collision(db5_save_begin(newest.get(), error), db5_save_free);
    CHECK(collision && save_db5_history(collision.get(), newest.get(), next.pack, &replacement, error) == DB5_SAVE_ERROR);
    CHECK(!replacement && record_bytes(newest.get(), archive_name) == newer);
    CHECK(bu_file_delete(export_path) && export_db5_snapshot(newest.get(), export_path, nullptr, error) == DB5_SAVE_OK);
    Database collision_export(db_open(export_path, DB_OPEN_READONLY), db_close);
    CHECK(collision_export && db_dirbuild(collision_export.get()) == 0);
    CHECK(record_bytes(collision_export.get(), archive_name) == newer);
    return 0;
}

static int
verify_directory_scan(struct db_i *base)
{
    constexpr long payload_size = 256 * 1024;
    constexpr uint8_t payload_byte = 0x5a;
    constexpr char payload_name[] = "large.bin";
    constexpr char region_name[] = "shape.r";
    const std::vector<uint8_t> payload(payload_size, payload_byte);
    struct rt_wdb *wdb = wdb_dbopen(base, RT_WDB_TYPE_DB_DEFAULT);
    CHECK(wdb && mk_binunif(wdb, payload_name, payload.data(), WDB_BINUNIF_UINT8, payload_size) == 0);
    CHECK(mk_region1(wdb, region_name, "shape.s", nullptr, nullptr, nullptr) == 0);
    const std::string note(payload_size, 'x');
    CHECK(db5_update_attribute(region_name, "comment", note.c_str(), base) == 0);
    db_sync(base);

    const auto payload_record = record_bytes(base, payload_name);
    const auto region_record = record_bytes(base, region_name);
    CHECK(payload_record.size() > BUFSIZ && region_record.size() > BUFSIZ);
    const auto *payload_dp = db_lookup(base, payload_name, LOOKUP_QUIET);
    CHECK(payload_dp && payload_dp->d_addr >= 0);

    {
	Database writable(db_open(base->dbi_filename, DB_OPEN_READWRITE), db_close);
	CHECK(writable && db_dirbuild(writable.get()) == 0);
	const auto *region = db_lookup(writable.get(), region_name, LOOKUP_QUIET);
	CHECK(region && (region->d_flags & RT_DIR_REGION));
	CHECK(record_bytes(writable.get(), payload_name) == payload_record);
	CHECK(record_bytes(writable.get(), region_name) == region_record);
    }

    File file(fopen(base->dbi_filename, "rb+"), close_file);
    CHECK(file && bu_fseek(file.get(), payload_dp->d_addr, SEEK_SET) == 0);
    struct db5_raw_internal raw;
    CHECK(db5_get_raw_internal_fp(&raw, file.get()) == 0);
    CHECK(raw.buf && raw.body.ext_buf && raw.object_length == payload_record.size());
    const bool exact = !memcmp(raw.buf, payload_record.data(), payload_record.size());
    const auto body_offset = raw.body.ext_buf - raw.buf;
    const size_t length_width = static_cast<size_t>(1) << raw.b_width;
    bu_free(raw.buf, "raw v5 object");
    CHECK(exact && body_offset >= static_cast<ptrdiff_t>(length_width));

    // The declared body now extends beyond the record's intact trailer.
    const std::vector<uint8_t> invalid_length(length_width, std::numeric_limits<uint8_t>::max());
    const b_off_t length_offset = payload_dp->d_addr + body_offset - static_cast<b_off_t>(length_width);
    CHECK(bu_fseek(file.get(), length_offset, SEEK_SET) == 0);
    CHECK(fwrite(invalid_length.data(), 1, length_width, file.get()) == length_width);
    CHECK(fclose(file.release()) == 0);
    for (const char *mode : {DB_OPEN_READWRITE, DB_OPEN_READONLY}) {
	Database malformed(db_open(base->dbi_filename, mode), db_close);
	CHECK(malformed && db_dirbuild(malformed.get()) < 0);
    }
    return 0;
}

static int
verify_wide_offsets(struct db_i *seed, const std::vector<uint8_t> &global,
		    const std::vector<uint8_t> &shape, struct bu_vls *error)
{
    constexpr uint64_t gibibyte = 1024 * 1024 * 1024;
    constexpr uint64_t extent = 5 * gibibyte;
    unsigned char header[8];
    if (extent > SIZE_MAX || extent + sizeof(header) > static_cast<uint64_t>(std::numeric_limits<b_off_t>::max())) {
	bu_log("Skipping wide DB5 offsets on a platform with narrow file sizes or offsets\n");
	return test_skipped;
    }
    const auto trailer = static_cast<b_off_t>(extent + sizeof(header) - 1);
    File source(fopen(seed->dbi_filename, "rb"), close_file);
    CHECK(source && fread(header, 1, sizeof(header), source.get()) == sizeof(header));
    CHECK(db5_header_is_valid(header));
    source.reset();

    char path[MAXPATHLEN] = {};
    File fixture(bu_temp_file(path, sizeof(path)), close_file);
    CHECK(fixture);
#if defined(_WIN32) && !defined(__CYGWIN__) && !defined(__MSYS__)
    const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(fixture.get())));
    DWORD returned;
    if (!DeviceIoControl(handle, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned, nullptr)) {
	bu_log("Skipping wide DB5 offsets: could not mark the fixture sparse (Windows error %lu)\n", GetLastError());
	return test_skipped;
    }
#endif
    CHECK(fwrite(header, 1, sizeof(header), fixture.get()) == sizeof(header));
    struct bu_external free_header = BU_EXTERNAL_INIT_ZERO;
    db5_make_free_object_hdr(&free_header, static_cast<size_t>(extent));
    const bool wrote_header = fwrite(free_header.ext_buf, 1, free_header.ext_nbytes, fixture.get()) == free_header.ext_nbytes;
    bu_free_external(&free_header);
    CHECK(wrote_header && bu_fseek(fixture.get(), trailer, SEEK_SET) == 0);
    CHECK(fputc(0, fixture.get()) == 0);
    CHECK(fwrite(global.data(), 1, global.size(), fixture.get()) == global.size());
    CHECK(fwrite(shape.data(), 1, shape.size(), fixture.get()) == shape.size());
    CHECK(fclose(fixture.release()) == 0);

    // Seeking past the unused storage must still reject a bad trailer.
    Database malformed(db_open(path, DB_OPEN_READWRITE), db_close);
    CHECK(malformed && db_dirbuild(malformed.get()) < 0);
    malformed.reset();

    fixture.reset(fopen(path, "rb+"));
    CHECK(fixture && bu_fseek(fixture.get(), trailer, SEEK_SET) == 0);
    CHECK(fputc(DB5HDR_MAGIC2, fixture.get()) == DB5HDR_MAGIC2 && fclose(fixture.release()) == 0);

    Database writable(db_open(path, DB_OPEN_READWRITE), db_close);
    CHECK(writable && db_dirbuild(writable.get()) == 0);
    const auto *dp = db_lookup(writable.get(), "shape.s", LOOKUP_QUIET);
    CHECK(dp && dp->d_addr == trailer + 1 + static_cast<b_off_t>(global.size()));
    CHECK(record_bytes(writable.get(), DB5_GLOBAL_OBJECT_NAME) == global);
    CHECK(record_bytes(writable.get(), "shape.s") == shape);

    CHECK(db5_update_ident(seed, "Different model", seed->dbi_local2base) == 0);
    return verify_embedded(path, writable.get(), seed, error);
}

int
main(int argc, char *argv[])
{
    if (argc > 0)
	bu_setprogname(argv[0]);

    char path[MAXPATHLEN] = {};
    FILE *file = bu_temp_file(path, sizeof(path));
    CHECK(file && fclose(file) == 0);
    Database base(db_create(path, 5), db_close);
    CHECK(base);
    const point_t center = VINIT_ZERO;
    CHECK(mk_sph(wdb_dbopen(base.get(), RT_WDB_TYPE_DB_DEFAULT), "shape.s", center, 1.0) == 0);
    db_sync(base.get());
    const auto original_record = record_bytes(base.get(), "shape.s");
    const auto global = record_bytes(base.get(), DB5_GLOBAL_OBJECT_NAME);
    CHECK(!original_record.empty() && !global.empty());

    struct bu_vls error = BU_VLS_INIT_ZERO;
    if (argc == 2) {
	int result = 1;
	if (BU_STR_EQUAL(argv[1], "--export"))
	    result = verify_export(base.get(), original_record, &error);
	else if (BU_STR_EQUAL(argv[1], "--wide-offsets"))
	    result = verify_wide_offsets(base.get(), global, original_record, &error);
	else if (BU_STR_EQUAL(argv[1], "--directory-scan"))
	    result = verify_directory_scan(base.get());
	bu_vls_free(&error);
	return result;
    }
    Save first(db5_save_begin(base.get(), &error), db5_save_free);
    if (!first && BU_STR_EQUAL(bu_vls_addr(&error), replacement_unavailable)) {
	bu_vls_free(&error);
	return test_skipped;
    }
    CHECK(first);
    Save concurrent(db5_save_begin(base.get(), &error), db5_save_free);
    CHECK(concurrent);
    Database old_reader(db_open(path, DB_OPEN_READONLY), db_close);
    CHECK(old_reader && db_dirbuild(old_reader.get()) == 0);
    const auto original_mtime = std::filesystem::last_write_time(path);
    const auto original_size = std::filesystem::file_size(path);
    const auto original_permissions = std::filesystem::status(path).permissions();

    auto *edit = db_edit_begin(old_reader.get(), "Change radius");
    CHECK(edit);
    auto *view = db_edit_view(edit);
    CHECK(mk_sph(wdb_dbopen(view, RT_WDB_TYPE_DB_DEFAULT), "shape.s", center, 2.0) == 0);
    const auto updated_record = record_bytes(view, "shape.s");
    CHECK(original_record.size() == updated_record.size() && original_record != updated_record);
    Database snapshot(db_edit_commit(edit), db_close);
    CHECK(snapshot && record_bytes(snapshot.get(), "shape.s") == updated_record);

    // Serialization failure leaves the file and the save token usable.
    Database invalid(db_open_inmem(), db_close);
    struct db_i *replacement = DBI_NULL;
    CHECK(db5_save_replace(first.get(), invalid.get(), &replacement, &error) == DB5_SAVE_ERROR);
    CHECK(!replacement && record_bytes(old_reader.get(), "shape.s") == original_record);

    Database malformed(db_create_inmem(), db_close);
    CHECK(malformed);
    uint8_t unterminated[] = {'n', 'a', 'm', 'e'};
    struct bu_external attributes = BU_EXTERNAL_INIT_ZERO;
    attributes.ext_buf = unterminated;
    attributes.ext_nbytes = sizeof(unterminated);
    struct bu_external bad_global = BU_EXTERNAL_INIT_ZERO;
    db5_export_object3(&bad_global, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
	DB5_GLOBAL_OBJECT_NAME, DB5HDR_HFLAGS_HIDDEN_OBJECT, &attributes, nullptr,
	DB5_MAJORTYPE_ATTRIBUTE_ONLY, DB5_MINORTYPE_RESERVED,
	DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED);
    auto *global_dp = db_lookup(malformed.get(), DB5_GLOBAL_OBJECT_NAME, LOOKUP_QUIET);
    CHECK(global_dp && db_put_external5(&bad_global, global_dp, malformed.get()) == 0);
    bu_free_external(&bad_global);
    CHECK(db5_save_replace(first.get(), malformed.get(), &replacement, &error) == DB5_SAVE_ERROR);
    CHECK(!replacement);

    CHECK(db5_save_replace(first.get(), snapshot.get(), &replacement, &error) == DB5_SAVE_OK);
    Database installed(replacement, db_close);
    CHECK(installed && BU_STR_EQUAL(installed->dbi_filename, base->dbi_filename));
    CHECK(record_bytes(installed.get(), "shape.s") == updated_record);
    CHECK(record_bytes(installed.get(), DB5_GLOBAL_OBJECT_NAME) == global);
    CHECK(std::filesystem::file_size(path) == original_size);
    CHECK(std::filesystem::status(path).permissions() == original_permissions);

    // Deliberately collide on size and mtime. A fresh mapped open must
    // observe a different file, and the old mapping must keep its data.
    std::filesystem::last_write_time(path, original_mtime);
    Database new_reader(db_open(path, DB_OPEN_READONLY), db_close);
    CHECK(new_reader && new_reader.get() != old_reader.get() && db_dirbuild(new_reader.get()) == 0);
    CHECK(record_bytes(new_reader.get(), "shape.s") == updated_record);
    CHECK(record_bytes(old_reader.get(), "shape.s") == original_record);
    CHECK(db5_save_replace(concurrent.get(), snapshot.get(), &replacement, &error) == DB5_SAVE_STALE);
    CHECK(!replacement);

    Save external_edit(db5_save_begin(installed.get(), &error), db5_save_free);
    CHECK(external_edit);
    CHECK(mk_sph(wdb_dbopen(installed.get(), RT_WDB_TYPE_DB_DEFAULT), "extra.s", center, 1.0) == 0);
    db_sync(installed.get());
    CHECK(db5_save_replace(external_edit.get(), snapshot.get(), &replacement, &error) == DB5_SAVE_STALE);
    CHECK(!replacement && !record_bytes(installed.get(), "extra.s").empty());

    // A replay must compare its expected bytes before changing the view.
    Database replay_base(db_open(path, DB_OPEN_READONLY), db_close);
    CHECK(replay_base && db_dirbuild(replay_base.get()) == 0);
    struct bu_external previous = borrow_record(original_record);
    struct bu_external current = borrow_record(updated_record);
    edit = db_edit_begin(replay_base.get(), "Conflicting replay");
    CHECK(edit && db_edit_apply(edit, "shape.s", &previous, nullptr) != 0);
    CHECK(!db_edit_commit(edit));
    db_edit_abort(edit);

    Save undo(db5_save_begin(installed.get(), &error), db5_save_free);
    CHECK(undo);
    edit = db_edit_begin(replay_base.get(), "Inverse edit");
    CHECK(edit && db_edit_apply(edit, "shape.s", &current, &previous) == 0);
    Database inverted(db_edit_commit(edit), db_close);
    CHECK(inverted);
    CHECK(db5_save_replace(undo.get(), inverted.get(), &replacement, &error) == DB5_SAVE_OK);
    Database restored(replacement, db_close);
    CHECK(record_bytes(restored.get(), "shape.s") == original_record);
    CHECK(!record_bytes(restored.get(), "extra.s").empty());

    const int embedded_status = verify_embedded(path, restored.get(), old_reader.get(), &error);
    bu_vls_free(&error);
    return embedded_status;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
