/*                     T E S T _ R E P A I R . C
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

#include "bu.h"
#include "ged.h"
#include "raytrace.h"
#include "wdb.h"


static int
check_arb(struct ged *gedp, const char *name, const point_t *original, int repaired)
{
    struct rt_db_internal intern;
    struct rt_arb_internal *arb;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    struct directory *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    int failed = 0;

    if (dp == RT_DIR_NULL || rt_db_get_internal(&intern, dp, gedp->dbip, NULL) < 0) {
	bu_log("FAIL: could not read %s\n", name);
	return 1;
    }
    if (intern.idb_type != ID_ARB8) {
	bu_log("FAIL: %s is not an ARB\n", name);
	rt_db_free_internal(&intern);
	return 1;
    }

    arb = (struct rt_arb_internal *)intern.idb_ptr;
    if (repaired) {
	failed = rt_arb_validate(NULL, arb, &tol, NULL) != 0;
	/* Repairing the storage order must preserve all six unique vertices. */
	for (int i = 0; i < 8; i++) {
	    int found_original = 0;
	    int found_repaired = 0;
	    for (int j = 0; j < 8; j++) {
		if (VNEAR_EQUAL(arb->pt[i], original[j], tol.dist))
		    found_original = 1;
		if (VNEAR_EQUAL(original[i], arb->pt[j], tol.dist))
		    found_repaired = 1;
	    }
	    if (!found_original || !found_repaired)
		failed = 1;
	}
    } else {
	failed = memcmp(arb->pt, original, sizeof(arb->pt)) != 0;
    }
    if (failed)
	bu_log("FAIL: %s has unexpected geometry\n", name);
    rt_db_free_internal(&intern);
    return failed;
}


static int
check_command(struct ged *gedp, const char **args, int expected, const char *message)
{
    int argc = 0;
    int ret;

    while (args[argc])
	argc++;
    const char **original = (const char **)bu_malloc((size_t)argc * sizeof(*original),
	"repair argument snapshot");
    memcpy(original, args, (size_t)argc * sizeof(*original));
    ret = ged_exec_repair(gedp, argc, args);
    int reordered = memcmp(original, args, (size_t)argc * sizeof(*original)) != 0;
    bu_free(original, "repair argument snapshot");
    if (reordered) {
	bu_log("FAIL: repair reordered the caller's arguments\n");
	return 1;
    }
    if (ret != expected || !strstr(bu_vls_cstr(gedp->ged_result_str), message)) {
	bu_log("FAIL: %s returned %d: %s\n", args[0], ret,
		bu_vls_cstr(gedp->ged_result_str));
	return 1;
    }
    return 0;
}


int
main(int argc, char **argv)
{
    enum { MAX_REPAIR_ARGS = 7 };
    /* blastsectionD from bldg391.g has two duplicate pairs in nonstandard
     * slots.  It can be repaired by reordering without moving any vertex. */
    const point_t vertices[8] = {
	{-318, 224, 0}, {-248, 298, 0}, {-248, 298, 180}, {-318, 224, 180},
	{-318, 298, 0}, {-248, 298, 0}, {-248, 298, 180}, {-318, 298, 180}
    };
    const struct {
	const char *args[MAX_REPAIR_ARGS];
	int expected;
	const char *message;
	const char *output;
    } cases[] = {
	{{"repair", "-o", "arbfixed.s", "blastsectionD", NULL},
	    BRLCAD_OK, "success", "arbfixed.s"},
	{{"repair", "-t", "0.01", "-o", "with_tol.s", "blastsectionD", NULL},
	    BRLCAD_OK, "success", "with_tol.s"},
	{{"repair", "blastsectionD", "--output-name=long_option.s", NULL},
	    BRLCAD_OK, "success", "long_option.s"},
	{{"repair", "blastsectionD", "-t0.01", "-oattached.s", NULL},
	    BRLCAD_OK, "success", "attached.s"},
	{{"repair", "-o", "delimited.s", "--", "blastsectionD", NULL},
	    BRLCAD_OK, "success", "delimited.s"},
	{{"repair", "--options-json", "-o", "query.s", "blastsectionD", NULL},
	    BRLCAD_OK, "options", NULL},
	{{"repair", "-o", "rejected.s", "--invalid-option", "blastsectionD", NULL},
	    BRLCAD_ERROR, "Invalid repair options", NULL},
	{{"repair", "-o", "rejected.s", "blastsectionD", "extra", NULL},
	    BRLCAD_ERROR, "Invalid repair options", NULL},
	{{"repair", "-o", "rejected.s", "-t", "bad", "blastsectionD", NULL},
	    BRLCAD_ERROR, "Invalid repair options", NULL},
	{{"repair", "-o", "rejected.s", "-p", "5", "blastsectionD", NULL},
	    BRLCAD_ERROR, "Invalid repair options", NULL},
	{{"repair", "-o", "arbfixed.s", "blastsectionD", NULL},
	    BRLCAD_ERROR, "already exists", NULL},
	{{"repair", "-o", NULL}, BRLCAD_ERROR, "Invalid repair options", NULL},
	{{"repair", "missing.s", NULL}, BRLCAD_ERROR, "No valid object", NULL},
	{{"repair", "-h", NULL}, GED_HELP, "Usage:", NULL},
	{{"repair", NULL}, GED_HELP, "Usage:", NULL}
    };
    char database_path[MAXPATHLEN] = {0};
    struct rt_wdb *wdbp;
    struct ged *gedp;
    FILE *fp;
    int failures = 0;

    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    fp = bu_temp_file(database_path, MAXPATHLEN);
    if (!fp)
	bu_exit(1, "Could not create temporary database path\n");
    fclose(fp);
    wdbp = wdb_fopen(database_path);
    if (!wdbp || mk_arb8(wdbp, "blastsectionD", &vertices[0][0]) < 0)
	bu_exit(1, "Could not create repair fixture\n");
    db_close(wdbp->dbip);

    gedp = ged_open("db", database_path, 1);
    if (!gedp)
	bu_exit(1, "Could not open repair fixture\n");

    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
	const char *args[sizeof(cases[i].args) / sizeof(cases[i].args[0])];
	memcpy(args, cases[i].args, sizeof(args));
	failures += check_command(gedp, args, cases[i].expected, cases[i].message);
	if (cases[i].output)
	    failures += check_arb(gedp, cases[i].output, vertices, 1);
	failures += check_arb(gedp, "blastsectionD", vertices, 0);
    }
    if (db_lookup(gedp->dbip, "query.s", LOOKUP_QUIET) != RT_DIR_NULL ||
	db_lookup(gedp->dbip, "rejected.s", LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_log("FAIL: query or failed repair created an output object\n");
	failures++;
    }

    {
	const char *args[] = {"repair", "blastsectionD", NULL};
	failures += check_command(gedp, args, BRLCAD_OK, "success");
	failures += check_arb(gedp, "blastsectionD", vertices, 1);
    }

    ged_close(gedp);
    bu_file_delete(database_path);
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
