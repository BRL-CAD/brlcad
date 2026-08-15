/*                   F U L L P A T H . C P P
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
#include <string.h>

#include "bu/app.h"
#include "ged.h"
#include "raytrace.h"
#include "wdb.h"
#include "../librt_private.h"


static int
expect_path(const char *label, const struct db_full_path *path, size_t len,
	    const char *first, const char *last)
{
    if (path->fp_len != len || (len && !BU_STR_EQUAL(path->fp_names[0]->d_namep, first)) ||
	(len > 1 && !BU_STR_EQUAL(path->fp_names[len - 1]->d_namep, last))) {
	fprintf(stderr, "%s: unexpected decoded path\n", label);
	return 1;
    }
    return 0;
}


static int
expect_encoded(const char *label, const struct db_full_path *path, const char *expected)
{
    struct bu_vls encoded = BU_VLS_INIT_ZERO;
    int failures = 0;

    if (db_full_path_encode(&encoded, path) != DB_FULL_PATH_OK ||
	!BU_STR_EQUAL(bu_vls_cstr(&encoded), expected)) {
	fprintf(stderr, "%s: expected %s, got %s\n", label, expected, bu_vls_cstr(&encoded));
	failures = 1;
    }
    bu_vls_free(&encoded);
    return failures;
}


static int
expect_geometry_completion(const char *label, struct db_i *dbip, const char *seed,
		   const char *expected_prefix, const char *expected_candidate)
{
    const char **completions = NULL;
    struct bu_vls prefix = BU_VLS_INIT_ZERO;
    int failures = 0;
    int count = ged_geom_completions(&completions, &prefix, dbip, seed);
    int found = 0;

    for (int i = 0; i < count; i++)
	if (BU_STR_EQUAL(completions[i], expected_candidate))
	    found = 1;
    if (!found || !BU_STR_EQUAL(bu_vls_cstr(&prefix), expected_prefix)) {
	fprintf(stderr, "%s: expected prefix %s and candidate %s\n",
		label, expected_prefix, expected_candidate);
	failures = 1;
    }

    if (completions)
	bu_argv_free((size_t)count, (char **)completions);
    bu_vls_free(&prefix);
    return failures;
}


int
main(int argc, const char **argv)
{
    (void)argc;
    bu_setprogname(argv[0]);

    int failures = 0;
    point_t center = VINIT_ZERO;
    struct db_i *dbip = db_create_inmem();
    struct rt_wdb *wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
    struct wmember members;
    struct wmember repeated_members;
    struct db_full_path path = DB_FULL_PATH_INIT_ZERO;

    if (!dbip || !wdbp) {
	fprintf(stderr, "unable to create in-memory database\n");
	return 1;
    }

    if (mk_sph(wdbp, "child/@part", center, 1.0) ||
	mk_sph(wdbp, "all.g/", center, 1.0) ||
	mk_sph(wdbp, "back\\slash", center, 1.0)) {
	fprintf(stderr, "unable to create test objects\n");
	wdb_close(wdbp);
	return 1;
    }
    BU_LIST_INIT(&members.l);
    if (mk_addmember("child/@part", &members.l, NULL, WMOP_UNION) == WMEMBER_NULL ||
	mk_lcomb(wdbp, "all.g", &members, 0, NULL, NULL, NULL, 0)) {
	fprintf(stderr, "unable to create test combination\n");
	wdb_close(wdbp);
	return 1;
    }
    BU_LIST_INIT(&repeated_members.l);
    if (mk_sph(wdbp, "bolt.s", center, 1.0) ||
	mk_addmember("bolt.s", &repeated_members.l, NULL, WMOP_UNION) == WMEMBER_NULL ||
	mk_addmember("bolt.s", &repeated_members.l, NULL, WMOP_UNION) == WMEMBER_NULL ||
	mk_lcomb(wdbp, "repeat.c", &repeated_members, 0, NULL, NULL, NULL, 0)) {
	fprintf(stderr, "unable to create repeated-member combination\n");
	wdb_close(wdbp);
	return 1;
    }

    failures += expect_geometry_completion("literal slash object", dbip, "all.g\\",
	"all.g\\", "all.g\\/");
    failures += expect_geometry_completion("escaped child prefix", dbip, "/all.g/child\\",
	"child\\", "child\\/\\@part");
    failures += expect_geometry_completion("completed escaped child prefix", dbip, "/all.g/child\\/",
	"child\\/", "child\\/\\@part");

    if (db_full_path_decode(&path, dbip, "/all.g/child\\/\\@part") != DB_FULL_PATH_OK)
	failures++;
    failures += expect_path("escaped hierarchy", &path, 2, "all.g", "child/@part");
    failures += expect_encoded("escaped hierarchy", &path, "/all.g/child\\/\\@part");

    {
	struct db_tree_state state;
	struct db_full_path followed = DB_FULL_PATH_INIT_ZERO;
	db_init_db_tree_state(&state, dbip);
	if (db_follow_path_for_state(&state, &followed, "/all.g/child\\/\\@part", 0) != 0)
	    failures++;
	failures += expect_path("tree walker escaped hierarchy", &followed, 2, "all.g", "child/@part");
	db_free_full_path(&followed);
	db_free_db_tree_state(&state);
    }

    if (db_full_path_decode(&path, dbip, "/all.g\\/") != DB_FULL_PATH_OK)
	failures++;
    failures += expect_path("trailing slash name", &path, 1, "all.g/", "all.g/");
    failures += expect_encoded("trailing slash name", &path, "/all.g\\/");

    if (db_full_path_decode(&path, dbip, "/back\\\\slash") != DB_FULL_PATH_OK)
	failures++;
    failures += expect_encoded("backslash name", &path, "/back\\\\slash");

    if (db_full_path_decode(&path, dbip, "/all.g/child\\/\\@part") != DB_FULL_PATH_OK)
	failures++;
    if (db_full_path_decode(&path, dbip, "/all.g/") != DB_FULL_PATH_SYNTAX)
	failures++;
    failures += expect_encoded("failed decode preserves output", &path, "/all.g/child\\/\\@part");

    if (db_full_path_decode(&path, dbip, "/all.g/does-not-exist") != DB_FULL_PATH_LOOKUP)
	failures++;
    failures += expect_encoded("lookup failure preserves output", &path, "/all.g/child\\/\\@part");

    if (db_full_path_decode(&path, dbip, "/child\\/\\@part/all.g") != DB_FULL_PATH_RELATION)
	failures++;
    if (db_full_path_decode(&path, dbip, "/all.g\\q") != DB_FULL_PATH_SYNTAX)
	failures++;
    if (db_full_path_decode(&path, dbip, "") != DB_FULL_PATH_SYNTAX)
	failures++;

    dbip->i->dbi_use_comb_instance_ids = 1;
    if (db_full_path_decode(&path, dbip, "/repeat.c/bolt.s@1") != DB_FULL_PATH_OK)
	failures++;
    if (path.fp_len != 2 || DB_FULL_PATH_CUR_COMB_INST(&path) != 1) {
	fprintf(stderr, "combination instance was not preserved\n");
	failures++;
    }
    failures += expect_encoded("combination instance", &path, "/repeat.c/bolt.s@1");
    if (db_full_path_decode(&path, dbip, "/repeat.c/bolt.s@2") != DB_FULL_PATH_INSTANCE)
	failures++;

    {
	struct bu_vls preserved = BU_VLS_INIT_ZERO;
	bu_vls_strcpy(&preserved, "keep");
	int saved_instance = path.fp_cinst[0];
	path.fp_cinst[0] = 1;
	if (db_full_path_encode(&preserved, &path) != DB_FULL_PATH_INVALID ||
	    !BU_STR_EQUAL(bu_vls_cstr(&preserved), "keep"))
	    failures++;
	path.fp_cinst[0] = saved_instance;
	bu_vls_free(&preserved);
    }

    if (db_full_path_decode(&path, dbip, "/") != DB_FULL_PATH_OK)
	failures++;
    failures += expect_path("root", &path, 0, NULL, NULL);
    failures += expect_encoded("root", &path, "/");

    db_free_full_path(&path);
    wdb_close(wdbp);
    return failures ? 1 : 0;
}
