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

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "wdb.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
	bu_log("DB5 save test failed at line %d: %s\n", __LINE__, #expr); \
	return 1; \
    } \
} while (0)

using Database = std::unique_ptr<struct db_i, decltype(&db_close)>;
using Save = std::unique_ptr<struct db5_save, decltype(&db5_save_free)>;

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
    Save first(db5_save_begin(base.get(), &error), db5_save_free);
    if (!first && BU_STR_EQUAL(bu_vls_addr(&error), "Atomic DB5 replacement is unavailable on this platform")) {
	bu_vls_free(&error);
	return 77;
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

    bu_vls_free(&error);
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
