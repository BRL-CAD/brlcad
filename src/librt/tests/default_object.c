/*                  D E F A U L T _ O B J E C T . C
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
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "wdb.h"


static struct db_i *
test_db(const char *filename)
{
    struct db_i *dbip = db_create_inmem();

    if (!dbip)
	return DBI_NULL;

    if (filename) {
	if (dbip->dbi_filename)
	    bu_free(dbip->dbi_filename, "dbi_filename");
	dbip->dbi_filename = bu_strdup(filename);
    }

    return dbip;
}


static int
add_sphere(struct rt_wdb *wdbp, const char *name)
{
    point_t center = VINIT_ZERO;

    return mk_sph(wdbp, name, center, 1.0);
}


static int
add_comb(struct rt_wdb *wdbp, const char *comb_name, const char *member_name)
{
    struct wmember wm;

    BU_LIST_INIT(&wm.l);
    (void)mk_addmember(member_name, &wm.l, NULL, WMOP_UNION);

    return mk_lcomb(wdbp, comb_name, &wm, 0, NULL, NULL, NULL, 0);
}


static int
add_comb_with_sphere(struct rt_wdb *wdbp, const char *comb_name, const char *sphere_name)
{
    if (add_sphere(wdbp, sphere_name))
	return 1;

    return add_comb(wdbp, comb_name, sphere_name);
}


static int
expect_selected(const char *label, struct db_i *dbip, const char *expected)
{
    struct directory *dp = RT_DIR_NULL;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int ret;
    int failures = 0;

    ret = db_default_object(dbip, &dp, &msg);
    if (ret != 1) {
	bu_log("%s: expected selection %s, got return %d and message:\n%s",
	       label, expected, ret, bu_vls_cstr(&msg));
	failures++;
    } else if (!dp || !BU_STR_EQUAL(dp->d_namep, expected)) {
	bu_log("%s: expected selection %s, got %s\n",
	       label, expected, dp ? dp->d_namep : "(null)");
	failures++;
    }

    if (ret == 1 && bu_vls_strlen(&msg) != 0) {
	bu_log("%s: success should not append message, got:\n%s",
	       label, bu_vls_cstr(&msg));
	failures++;
    }

    bu_vls_free(&msg);
    return failures;
}


static int
expect_no_selection(const char *label, struct db_i *dbip, const char *text1, const char *text2)
{
    struct directory *dp = RT_DIR_NULL;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    int ret;
    int failures = 0;

    ret = db_default_object(dbip, &dp, &msg);
    if (ret != 0) {
	bu_log("%s: expected no selection, got return %d\n", label, ret);
	failures++;
    }

    if (dp != RT_DIR_NULL) {
	bu_log("%s: expected NULL dp, got %s\n", label, dp->d_namep);
	failures++;
    }

    if (text1 && !strstr(bu_vls_cstr(&msg), text1)) {
	bu_log("%s: message missing [%s]:\n%s", label, text1, bu_vls_cstr(&msg));
	failures++;
    }

    if (text2 && !strstr(bu_vls_cstr(&msg), text2)) {
	bu_log("%s: message missing [%s]:\n%s", label, text2, bu_vls_cstr(&msg));
	failures++;
    }

    bu_vls_free(&msg);
    return failures;
}


static int
test_global_override(void)
{
    struct db_i *dbip = test_db(NULL);
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    int failures = 0;

    failures += add_sphere(wdbp, "target.s");
    failures += add_sphere(wdbp, "other.s");
    failures += db5_update_attribute(DB5_GLOBAL_OBJECT_NAME, DB_DEFAULT_OBJECT_ATTR, "target.s", dbip);
    failures += expect_selected("global override", dbip, "target.s");

    db_close(dbip);
    return failures;
}


static int
test_global_disable(void)
{
    struct db_i *dbip = test_db(NULL);
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    int failures = 0;

    failures += add_sphere(wdbp, "only.s");
    failures += db5_update_attribute(DB5_GLOBAL_OBJECT_NAME, DB_DEFAULT_OBJECT_ATTR, "false", dbip);
    failures += expect_no_selection("global disable", dbip, "disabled", "Available top-level objects");

    db_close(dbip);
    return failures;
}


static int
test_global_invalid(void)
{
    struct db_i *dbip = test_db(NULL);
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    int failures = 0;

    failures += add_sphere(wdbp, "only.s");
    failures += db5_update_attribute(DB5_GLOBAL_OBJECT_NAME, DB_DEFAULT_OBJECT_ATTR, "missing.s", dbip);
    failures += expect_no_selection("global invalid", dbip, "does not name a geometry object", "only.s");

    db_close(dbip);
    return failures;
}


static int
test_single_top(void)
{
    struct db_i *dbip = test_db(NULL);
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    int failures = 0;

    failures += add_sphere(wdbp, "single.s");
    failures += expect_selected("single top", dbip, "single.s");

    db_close(dbip);
    return failures;
}


static int
test_single_comb(void)
{
    struct db_i *dbip = test_db(NULL);
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    int failures = 0;

    failures += add_comb_with_sphere(wdbp, "only.c", "leaf.s");
    failures += add_sphere(wdbp, "top.s");
    failures += expect_selected("single comb", dbip, "only.c");

    db_close(dbip);
    return failures;
}


static int
test_filename_match(void)
{
    struct db_i *dbip = test_db("/tmp/model.g");
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    int failures = 0;

    failures += add_comb_with_sphere(wdbp, "model.c", "model_leaf.s");
    failures += add_comb_with_sphere(wdbp, "other.c", "other_leaf.s");
    failures += expect_selected("filename match", dbip, "model.c");

    db_close(dbip);
    return failures;
}


static int
test_filename_ambiguous(void)
{
    struct db_i *dbip = test_db("/tmp/model.g");
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    int failures = 0;

    failures += add_comb_with_sphere(wdbp, "model.c", "model_c_leaf.s");
    failures += add_comb_with_sphere(wdbp, "model.r", "model_r_leaf.s");
    failures += expect_no_selection("filename ambiguous", dbip, "ambiguous", "model.c");

    db_close(dbip);
    return failures;
}


static int
test_all_scene_match(void)
{
    struct db_i *dbip = test_db("/tmp/nomatch.g");
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    int failures = 0;

    failures += add_comb_with_sphere(wdbp, "ALL.g", "all_leaf.s");
    failures += add_comb_with_sphere(wdbp, "other.c", "other_leaf.s");
    failures += expect_selected("all scene match", dbip, "ALL.g");

    db_close(dbip);
    return failures;
}


static int
test_all_scene_ambiguous(void)
{
    struct db_i *dbip = test_db("/tmp/nomatch.g");
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    int failures = 0;

    failures += add_comb_with_sphere(wdbp, "all.g", "all_leaf.s");
    failures += add_comb_with_sphere(wdbp, "scene.c", "scene_leaf.s");
    failures += expect_no_selection("all scene ambiguous", dbip, "ambiguous", "scene.c");

    db_close(dbip);
    return failures;
}


static int
test_no_match_sorted_tops(void)
{
    struct db_i *dbip = test_db(NULL);
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    struct directory *dp = RT_DIR_NULL;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const char *a_pos;
    const char *z_pos;
    int ret;
    int failures = 0;

    failures += add_sphere(wdbp, "z.s");
    failures += add_sphere(wdbp, "a.s");

    ret = db_default_object(dbip, &dp, &msg);
    if (ret != 0 || dp != RT_DIR_NULL) {
	bu_log("no match sorted tops: expected no selection, got return %d and dp %s\n",
	       ret, dp ? dp->d_namep : "(null)");
	failures++;
    }

    a_pos = strstr(bu_vls_cstr(&msg), "  a.s\n");
    z_pos = strstr(bu_vls_cstr(&msg), "  z.s\n");
    if (!a_pos || !z_pos || a_pos > z_pos) {
	bu_log("no match sorted tops: tops are not sorted as expected:\n%s",
	       bu_vls_cstr(&msg));
	failures++;
    }

    bu_vls_free(&msg);
    db_close(dbip);
    return failures;
}


int
main(int argc, char *argv[])
{
    int failures = 0;

    bu_setprogname(argv[0]);

    if (argc != 1) {
	bu_log("Usage: %s\n", argv[0]);
	return 1;
    }

    failures += test_global_override();
    failures += test_global_disable();
    failures += test_global_invalid();
    failures += test_single_top();
    failures += test_single_comb();
    failures += test_filename_match();
    failures += test_filename_ambiguous();
    failures += test_all_scene_match();
    failures += test_all_scene_ambiguous();
    failures += test_no_match_sorted_tops();

    if (failures)
	bu_log("rt_default_object: %d failure(s)\n", failures);

    return failures ? 1 : 0;
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
