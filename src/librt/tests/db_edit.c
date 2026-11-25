/*                     D B _ E D I T . C
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

#include <string.h>

#include "bu/app.h"
#include "raytrace.h"
#include "wdb.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
	bu_log("db_edit test failed at line %d: %s\n", __LINE__, #expr); \
	return 1; \
    } \
} while (0)

struct change_count {
    size_t added;
    size_t removed;
    size_t updated;
};

static int
count_change(const char *UNUSED(name), const struct bu_external *before,
	     const struct bu_external *after, void *data)
{
    struct change_count *counts = (struct change_count *)data;
    if (before && after)
	++counts->updated;
    else if (before)
	++counts->removed;
    else
	++counts->added;
    return 0;
}

static int
read_record(struct bu_external *record, struct db_i *dbip, const char *name)
{
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    return dp ? db_get_external(record, dp, dbip) : -1;
}

static int
check_record(struct db_i *dbip, const char *name, const struct bu_external *expected)
{
    struct bu_external record = BU_EXTERNAL_INIT_ZERO;
    if (read_record(&record, dbip, name) < 0)
	return 0;
    const int equal = record.ext_nbytes == expected->ext_nbytes &&
	memcmp(record.ext_buf, expected->ext_buf, record.ext_nbytes) == 0;
    bu_free_external(&record);
    return equal;
}

static int
test_abort(struct db_i *base, struct bu_external *original)
{
    struct db_edit *edit = db_edit_begin(base, "failed edit");
    CHECK(edit);
    struct db_i *view = db_edit_view(edit);
    CHECK(view);
    struct rt_wdb *wdbp = wdb_dbopen(view, RT_WDB_TYPE_DB_DEFAULT);
    const point_t center = VINIT_ZERO;
    CHECK(mk_sph(wdbp, "second.s", center, 2.0) == 0);
    CHECK(db_lookup(view, "second.s", LOOKUP_QUIET));
    CHECK(!db_lookup(base, "second.s", LOOKUP_QUIET));

    struct bu_external truncated = *original;
    --truncated.ext_nbytes;
    CHECK(db_put_external5(&truncated, db_lookup(view, "base.s", LOOKUP_QUIET), view) < 0);
    CHECK(!db_edit_commit(edit));
    db_edit_abort(edit);
    CHECK(check_record(base, "base.s", original));
    CHECK(!db_lookup(base, "second.s", LOOKUP_QUIET));
    return 0;
}

static int
test_commit(struct db_i *base, struct bu_external *original)
{
    struct db_edit *edit = db_edit_begin(base, "rename");
    CHECK(edit && BU_STR_EQUAL(db_edit_label(edit), "rename"));
    struct db_i *view = db_edit_view(edit);
    CHECK(check_record(view, "base.s", original));
    struct directory *dp = db_lookup(view, "base.s", LOOKUP_QUIET);
    CHECK(dp && db_rename(view, dp, "renamed.s") == 0);
    CHECK(!db_lookup(view, "base.s", LOOKUP_QUIET));
    CHECK(check_record(view, "renamed.s", original));
    size_t seen = 0;
    struct directory *entry;
    FOR_ALL_DIRECTORY_START(entry, view) {
	if (BU_STR_EQUAL(entry->d_namep, "renamed.s"))
	    ++seen;
    } FOR_ALL_DIRECTORY_END;
    CHECK(seen == 1);

    struct bu_external record = BU_EXTERNAL_INIT_ZERO;
    CHECK(read_record(&record, view, "renamed.s") == 0);
    CHECK(db_put_external5(&record, dp, view) == 0);
    struct change_count changes = {0, 0, 0};
    CHECK(db_edit_visit(edit, count_change, &changes) == 0);
    CHECK(changes.added == 1 && changes.removed == 1 && changes.updated == 0);
    CHECK(check_record(base, "base.s", original));
    CHECK(!db_lookup(base, "renamed.s", LOOKUP_QUIET));

    struct db_i *published = db_edit_commit(edit);
    CHECK(published && published->dbi_read_only);
    CHECK(db_rename(published, dp, "illegal.s") < 0);
    CHECK(check_record(published, "renamed.s", &record));
    bu_free_external(&record);

    edit = db_edit_begin(published, "same bytes");
    CHECK(edit);
    view = db_edit_view(edit);
    dp = db_lookup(view, "renamed.s", LOOKUP_QUIET);
    CHECK(read_record(&record, view, "renamed.s") == 0);
    CHECK(db_put_external5(&record, dp, view) == 0);
    bu_free_external(&record);
    changes.added = changes.removed = changes.updated = 0;
    CHECK(db_edit_visit(edit, count_change, &changes) == 0);
    CHECK(changes.added == 0 && changes.removed == 0 && changes.updated == 0);
    db_edit_abort(edit);
    db_close(published);
    return 0;
}

int
main(int argc, const char *argv[])
{
    if (argc > 0)
	bu_setprogname(argv[0]);

    struct db_i *base = db_create_inmem();
    CHECK(base);
    struct rt_wdb *wdbp = wdb_dbopen(base, RT_WDB_TYPE_DB_DEFAULT);
    const point_t center = VINIT_ZERO;
    CHECK(mk_sph(wdbp, "base.s", center, 1.0) == 0);
    base->dbi_read_only = 1;

    struct bu_external original = BU_EXTERNAL_INIT_ZERO;
    CHECK(read_record(&original, base, "base.s") == 0);
    CHECK(test_abort(base, &original) == 0);
    CHECK(test_commit(base, &original) == 0);
    bu_free_external(&original);
    db_close(base);
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
