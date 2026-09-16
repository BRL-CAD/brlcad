/*                        R E G D E F . C
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
/** @file regdef.c
 *
 * Regression test for the regdef command and new-region material defaults.
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/file.h"
#include "ged.h"
#include "raytrace.h"
#include "rt/nongeom.h"


static int
run_cmd(struct ged *gedp, int argc, const char *argv[])
{
    bu_vls_trunc(gedp->ged_result_str, 0);
    if (ged_exec(gedp, argc, argv) != BRLCAD_OK) {
	bu_log("Error: command '%s' failed: %s\n", argv[0], bu_vls_cstr(gedp->ged_result_str));
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


static int
expect_cmd_result(struct ged *gedp, int argc, const char *argv[], const char *expected)
{
    if (run_cmd(gedp, argc, argv) != BRLCAD_OK)
	return BRLCAD_ERROR;

    if (!BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), expected)) {
	bu_log("Error: expected '%s', got '%s'\n", expected, bu_vls_cstr(gedp->ged_result_str));
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}


static int
check_region_material(struct ged *gedp, const char *name, int expected_material)
{
    struct directory *dp = db_lookup(gedp->dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_log("Error: '%s' not found\n", name);
	return BRLCAD_ERROR;
    }

    struct rt_db_internal intern;
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, gedp->dbip, NULL) < 0) {
	bu_log("Error: rt_db_get_internal failed for '%s'\n", name);
	return BRLCAD_ERROR;
    }

    if (intern.idb_type != ID_COMBINATION) {
	bu_log("Error: '%s' is type %d, expected combination\n", name, intern.idb_type);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    struct rt_comb_internal *comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (!comb->region_flag) {
	bu_log("Error: '%s' is not marked as a region\n", name);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    if (comb->GIFTmater != expected_material) {
	bu_log("Error: '%s' material is %ld, expected %d\n", name, comb->GIFTmater, expected_material);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    rt_db_free_internal(&intern);
    return BRLCAD_OK;
}


int
main(int argc, char *argv[])
{
    struct ged *gedp;
    const char *gname = "ged_regdef_test.g";
    const char *get_regdef[] = {"regdef"};
    const char *make_reg1[] = {"make", "regdef_s1.s", "sph"};
    const char *make_reg2[] = {"make", "regdef_s2.s", "sph"};
    const char *mk_reg1[] = {"r", "reg1.r", "u", "regdef_s1.s"};
    const char *mk_reg2[] = {"r", "reg2.r", "u", "regdef_s2.s"};
    const char *set_regdef[] = {"regdef", "2000", "0", "100", "7"};

    bu_setprogname(argv[0]);

    if (argc != 1) {
	fprintf(stderr, "Usage: %s\n", argv[0]);
	return 1;
    }

    if (bu_file_exists(gname, NULL)) {
	fprintf(stderr, "Error: %s already exists\n", gname);
	return 1;
    }

    gedp = ged_open("db", gname, 0);
    if (!gedp) {
	bu_log("Error: ged_open failed for %s\n", gname);
	return 1;
    }

    if (expect_cmd_result(gedp, 1, get_regdef, "ident 1000 air 0 los 100 material 0") != BRLCAD_OK)
	goto ged_test_fail;

    if (run_cmd(gedp, 3, make_reg1) != BRLCAD_OK)
	goto ged_test_fail;
    if (run_cmd(gedp, 4, mk_reg1) != BRLCAD_OK)
	goto ged_test_fail;
    if (check_region_material(gedp, "reg1.r", 0) != BRLCAD_OK)
	goto ged_test_fail;
    if (expect_cmd_result(gedp, 1, get_regdef, "ident 1001 air 0 los 100 material 0") != BRLCAD_OK)
	goto ged_test_fail;

    if (run_cmd(gedp, 5, set_regdef) != BRLCAD_OK)
	goto ged_test_fail;
    if (expect_cmd_result(gedp, 1, get_regdef, "ident 2000 air 0 los 100 material 7") != BRLCAD_OK)
	goto ged_test_fail;

    if (run_cmd(gedp, 3, make_reg2) != BRLCAD_OK)
	goto ged_test_fail;
    if (run_cmd(gedp, 4, mk_reg2) != BRLCAD_OK)
	goto ged_test_fail;
    if (check_region_material(gedp, "reg2.r", 7) != BRLCAD_OK)
	goto ged_test_fail;
    if (expect_cmd_result(gedp, 1, get_regdef, "ident 2001 air 0 los 100 material 7") != BRLCAD_OK)
	goto ged_test_fail;

    ged_close(gedp);
    return 0;

 ged_test_fail:
    ged_close(gedp);
    return 1;
}
