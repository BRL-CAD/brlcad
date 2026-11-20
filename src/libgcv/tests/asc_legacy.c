/*                    A S C _ L E G A C Y . C
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

#include <stdio.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/mime_runtime.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "gcv.h"
#include "wdb.h"

#define CHECK(condition) do { \
    if (!(condition)) { \
	bu_log("Legacy ASC check failed at line %d: %s\n", __LINE__, #condition); \
	failed = 1; \
	goto done; \
    } \
} while (0)

static const struct gcv_filter *
asc_filter(struct gcv_context *context, enum gcv_filter_type type)
{
    const struct bu_ptbl *filters = gcv_list_filters(context);
    const char *name = type == GCV_FILTER_READ ? "ASC Reader" : "ASC Writer";
    for (size_t i = 0; i < BU_PTBL_LEN(filters); ++i) {
	const struct gcv_filter *filter = (const struct gcv_filter *)BU_PTBL_GET(filters, i);
	if (filter->filter_type == type && BU_STR_EQUAL(filter->name, name))
	    return filter;
    }
    return NULL;
}

static int
check_region(struct db_i *database)
{
    struct rt_db_internal object;
    struct directory *entry = db_lookup(database, "first.r", LOOKUP_QUIET);
    if (!entry || rt_db_get_internal(&object, entry, database, NULL) < 0)
	return 0;
    struct rt_comb_internal *combination = (struct rt_comb_internal *)object.idb_ptr;
    int valid = object.idb_type == ID_COMBINATION && combination->region_id == 37 &&
	db_tree_nleaves(combination->tree) == 2;
    rt_db_free_internal(&object);
    return valid;
}

int
main(int UNUSED(argc), const char **argv)
{
    struct gcv_context source, imported;
    char database_path[MAXPATHLEN] = {0};
    char ascii_path[MAXPATHLEN] = {0};
    struct wmember members;
    FILE *file = NULL;
    struct rt_wdb *writer;
    point_t center = {1, 2, 3};
    const struct gcv_filter *filter;
    int ident_count = 0, solid_count = 0, combination_count = 0, member_count = 0;
    char line[4096];
    int failed = 0;
    int close_status;
    const struct bu_mime_trait syntax[] = {BU_MIME_TRAIT_STRING("asc.syntax", "legacy-records")};
    const struct bu_mime_traits traits = {syntax, sizeof(syntax) / sizeof(syntax[0])};

    bu_setprogname(argv[0]);
    gcv_context_init(&source);
    gcv_context_init(&imported);
    BU_LIST_INIT(&members.l);
    file = bu_temp_file(database_path, sizeof(database_path));
    CHECK(file);
    close_status = fclose(file);
    file = NULL;
    CHECK(!close_status);
    file = bu_temp_file(ascii_path, sizeof(ascii_path));
    CHECK(file);
    close_status = fclose(file);
    file = NULL;
    CHECK(!close_status);
    writer = wdb_fopen_v(database_path, 4);
    CHECK(writer);
    db_close(source.dbip);
    source.dbip = writer->dbip;
    CHECK(!mk_id_units(writer, "Legacy ASC round trip", "cm"));
    CHECK(!mk_sph(writer, "first.s", center, 2));
    CHECK(mk_addmember("first.s", &members.l, NULL, WMOP_UNION));
    CHECK(mk_addmember("first.s", &members.l, NULL, WMOP_UNION));
    CHECK(!mk_lrcomb(writer, "first.r", &members, 1, NULL, NULL, NULL, 37, 0, 0, 100, 0));
    CHECK(!mk_sph(writer, "last.s", center, 3));
    CHECK(!mk_comb1(writer, "last.r", "last.s", 1));
    filter = asc_filter(&source, GCV_FILTER_WRITE);
    CHECK(filter && gcv_execute_variant(&source, filter, "application/vnd.brlcad.script",
	&traits, NULL, 0, NULL, ascii_path));
    file = fopen(ascii_path, "rb");
    CHECK(file);
    while (bu_fgets(line, sizeof(line), file)) {
	if (line[1] != ' ') continue;
	switch (line[0]) {
	    case 'I': ++ident_count; break;
	    case 'S': ++solid_count; break;
	    case 'C': ++combination_count; break;
	    case 'M': ++member_count; break;
	}
    }
    CHECK(!ferror(file));
    close_status = fclose(file);
    file = NULL;
    CHECK(!close_status);
    CHECK(ident_count == 1 && solid_count == 2 && combination_count == 2 && member_count == 3);
    filter = asc_filter(&imported, GCV_FILTER_READ);
    {
	const struct bu_mime_trait unknown = BU_MIME_TRAIT_STRING("fixture.unestablished", "assertion");
	const struct bu_mime_traits invalid = {&unknown, 1};
	CHECK(!db_update_ident(imported.dbip, "Before legacy assertion", 10));
	const size_t before = db_directory_size(imported.dbip);
	CHECK(filter && !gcv_execute_variant(&imported, filter, "application/vnd.brlcad.script",
	    &invalid, NULL, 0, NULL, ascii_path));
	CHECK(db_directory_size(imported.dbip) == before && BU_STR_EQUAL(imported.dbip->dbi_title, "Before legacy assertion") &&
	    EQUAL(imported.dbip->dbi_local2base, 10));
    }
    CHECK(filter && gcv_execute_variant(&imported, filter, "application/vnd.brlcad.script",
	&traits, NULL, 0, NULL, ascii_path));
    CHECK(db_lookup(imported.dbip, "first.s", LOOKUP_QUIET));
    CHECK(db_lookup(imported.dbip, "last.s", LOOKUP_QUIET));
    CHECK(db_lookup(imported.dbip, "last.r", LOOKUP_QUIET));
    CHECK(check_region(imported.dbip));
    CHECK(BU_STR_EQUAL(imported.dbip->dbi_title, "Legacy ASC round trip"));
    CHECK(EQUAL(imported.dbip->dbi_local2base, 10));

done:
    if (file) fclose(file);
    mk_freemembers(&members.l);
    gcv_context_destroy(&imported);
    gcv_context_destroy(&source);
    if (database_path[0]) bu_file_delete(database_path);
    if (ascii_path[0]) bu_file_delete(ascii_path);
    return failed;
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
