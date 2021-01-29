/*                    P U S H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020 United States Government as represented by
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
/** @file push.cpp
 *
 * Testing logic for the push command.
 *
 */

#include "common.h"

#include <fstream>
#include <iostream>
#include <iomanip>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/path.h"
#include "bu/str.h"
#include "rt/db_diff.h"
#include "ged.h"

/* Push checking is a bit quirky - we want to check matrices and tree structure in
 * comb instances, but not instance names.  We also need to compare solid leaves,
 * but not names.  The following is a custom routine for this purpose */
static void check_walk(bool *diff, struct db_i *dbip, struct directory *dp1, struct directory *dp2);
static void
check_walk_subtree(bool *diff, struct db_i *dbip, union tree *tp1, union tree *tp2)
{
    bool idn1, idn2;
    struct directory *dp1, *dp2;

    if (!diff)
       	return;

    if ((!tp1 && tp2) || (tp1 && !tp2)) {
	(*diff) = true;
	return;
    }

    if (tp1->tr_op != tp2->tr_op) {
	(*diff) = true;
	return;
    }

    switch (tp1->tr_op) {
	case OP_DB_LEAF:
	    idn1 = (!tp1->tr_l.tl_mat || bn_mat_is_identity(tp1->tr_l.tl_mat));
	    idn2 = (!tp2->tr_l.tl_mat || bn_mat_is_identity(tp2->tr_l.tl_mat));
	    if (idn1 != idn2) {
		(*diff) = true;
		return;
	    }
	    if (tp1->tr_l.tl_mat && tp2->tr_l.tl_mat) {
		if (!bn_mat_is_equal(tp1->tr_l.tl_mat, tp2->tr_l.tl_mat, &dbip->dbi_wdbp->wdb_tol)) {
		    (*diff) = true;
		    return;
		}
	    }

	    dp1 = db_lookup(dbip, tp1->tr_l.tl_name, LOOKUP_NOISY);
	    dp2 = db_lookup(dbip, tp2->tr_l.tl_name, LOOKUP_NOISY);

	    check_walk(diff, dbip, dp1, dp2);

	    break;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    check_walk_subtree(diff, dbip, tp1->tr_b.tb_left, tp2->tr_b.tb_left);
	    check_walk_subtree(diff, dbip, tp1->tr_b.tb_right, tp2->tr_b.tb_right);
	    break;
	default:
	    bu_log("check_walk_subtree: unrecognized operator %d\n", tp1->tr_op);
	    bu_bomb("check_walk_subtree: unrecognized operator\n");
    }
}

static void
check_walk(bool *diff,
	struct db_i *dbip,
	struct directory *dp1,
	struct directory *dp2)
{
    if ((!dp1 && !dp2) || !diff || (*diff)) {
	return; /* nothing to do */
    }
    if ((dp1 == RT_DIR_NULL && dp2 != RT_DIR_NULL) || (dp1 != RT_DIR_NULL && dp2 == RT_DIR_NULL)) {
	*diff = true;
	return;
    }
    if (dp1 == RT_DIR_NULL && dp2 == RT_DIR_NULL) {
	return;
    }
    if (dp1->d_flags != dp2->d_flags) {
	*diff = true;
	return;
    }

    if (dp1->d_flags & RT_DIR_COMB) {

	struct rt_db_internal in1, in2;
	struct rt_comb_internal *comb1, *comb2;

	if (rt_db_get_internal5(&in1, dp1, dbip, NULL, &rt_uniresource) < 0) {
	    *diff = true;
	    return;
	}

	if (rt_db_get_internal5(&in2, dp2, dbip, NULL, &rt_uniresource) < 0) {
	    *diff = true;
	    return;
	}

	comb1 = (struct rt_comb_internal *)in1.idb_ptr;
	comb2 = (struct rt_comb_internal *)in2.idb_ptr;
	check_walk_subtree(diff, dbip, comb1->tree, comb2->tree);
	rt_db_free_internal(&in1);
	rt_db_free_internal(&in2);

	return;
    }

    /* If we have two solids, use db_diff_dp */
    int dr = db_diff_dp(dbip, dbip, dp1, dp2, &dbip->dbi_wdbp->wdb_tol, DB_COMPARE_ALL, NULL);
    if (dr != DIFF_UNCHANGED) {
	std::cout << "solids " << dp1->d_namep << " and " << dp2->d_namep << " differ\n";
	*diff = true;
    }
}


static void
npush_usage(struct bu_vls *str, struct bu_opt_desc *d) {
    char *option_help = bu_opt_describe(d, NULL);
    bu_vls_sprintf(str, "Usage: ged_push_test [options] control.g working.g\n");
    bu_vls_printf(str, "\nRuns the specified push configuration on a series of pre-defined objects in the working.g file, then compares the results to those from the control.g file.\n  \n");
    if (option_help) {
	bu_vls_printf(str, "Options:\n%s\n", option_help);
	bu_free(option_help, "help str");
    }
}

int
main(int argc, const char **argv)
{

    int print_help = 0;
    int xpush = 0;
    int to_regions = 0;
    int to_solids = 0;
    int max_depth = 0;
    int verbosity = 0;
    int local_changes_only = 0;
    int dry_run = 0;
    struct bu_opt_desc d[11];
    BU_OPT(d[0], "h", "help",      "",   NULL,         &print_help,  "Print help and exit");
    BU_OPT(d[1], "?", "",          "",   NULL,         &print_help,  "");
    BU_OPT(d[2], "v", "verbosity",  "",  &bu_opt_incr_long, &verbosity,     "Increase output verbosity (multiple specifications of -v increase verbosity)");
    BU_OPT(d[3], "f", "force",     "",   NULL,         &xpush,       "Create new objects if needed to push matrices (xpush)");
    BU_OPT(d[4], "x", "xpush",     "",   NULL,         &xpush,       "");
    BU_OPT(d[5], "r", "regions",   "",   NULL,         &to_regions,  "Halt push at regions (matrix will be above region reference)");
    BU_OPT(d[6], "s", "solids",    "",   NULL,         &to_solids,   "Halt push at solids (matrix will be above solid reference)");
    BU_OPT(d[7], "d", "max-depth", "",   &bu_opt_int,  &max_depth,   "Halt at depth # from tree root (matrix will be above item # layers deep)");
    BU_OPT(d[8], "L", "local", "",       NULL,  &local_changes_only,   "Ensure push operations do not impact geometry outside the specified trees.");
    BU_OPT(d[9], "D", "dry-run", "",       NULL,  &dry_run,   "Calculate the changes but do not apply them.");
    BU_OPT_NULL(d[10]);

    bu_setprogname(argv[0]);

    // We will reuse most of the argv array when we actually call the ged command, but we also need
    // to process it here so we know what to look for when comparing the control and the output.  Make
    // a duplicate for later use.
    int gargc = argc;
    char **gargv = bu_argv_dup(argc, argv);

    /* parse standard options */
    argc--;argv++;
    int opt_ret = bu_opt_parse(NULL, argc, argv, d);

    /* adjust argc to match the leftovers of the options parsing */
    argc = opt_ret;

    if (argc != 2 || print_help) {
	struct bu_vls npush_help = BU_VLS_INIT_ZERO;
	npush_usage(&npush_help, d);
	bu_log("%s", bu_vls_cstr(&npush_help));
	bu_vls_free(&npush_help);
	return -1;
    }

    if (BU_STR_EQUAL(argv[0], argv[1])) {
	bu_log("Error - control and working .g file cannot match.\n");
	return -1;
    }

    if (!bu_file_exists(argv[0], NULL)) {
	bu_exit(1, "%s does not exist", argv[0]);
    }
    if (!bu_file_exists(argv[1], NULL)) {
	bu_exit(1, "%s does not exist", argv[1]);
    }

    struct bu_vls wdir = BU_VLS_INIT_ZERO;
    bu_vls_printf(&wdir, "%s", bu_dir(NULL, 0, BU_DIR_CURR, NULL));
    struct bu_vls gbasename = BU_VLS_INIT_ZERO;
    if (!bu_path_component(&gbasename, argv[1], BU_PATH_BASENAME)) {
	bu_vls_free(&wdir);
	bu_exit(1, "Could not identify basename in geometry file path \"%s\"", argv[1]);
    }
    struct bu_vls gfile = BU_VLS_INIT_ZERO;
    bu_vls_printf(&gfile, "%s_push", bu_vls_cstr(&gbasename));

    // Put a copy of the non-control .g file in the working directory
    std::ifstream sgfile(argv[1], std::ios::binary);
    std::ofstream dgfile;
    const char *ofile = bu_dir(NULL, 0, bu_vls_cstr(&wdir), bu_vls_cstr(&gfile), NULL);
    dgfile.open(ofile, std::ios::binary);
    dgfile << sgfile.rdbuf();
    dgfile.close();
    sgfile.close();

    // Adjust the duplicate argv copy for ged purposes.
    bu_free(gargv[0], "free copy of argv[0]");
    bu_free(gargv[gargc-2], "free copy of argv[argc-1]");
    bu_free(gargv[gargc-1], "free copy of argv[argc-1]");
    gargv[0] = bu_strdup("npush");
    gargv[gargc-2] = NULL;
    gargv[gargc-1] = NULL;

    // All set - open up the .g file and go to work.
    struct ged *gedp = ged_open("db", bu_vls_cstr(&gfile), 1);
    if (gedp == GED_NULL) {
	bu_exit(1, "Failed to open \"%s\" ", bu_vls_cstr(&gbasename));
    }

    // Make sure our reference counts are up to date
    db_update_nref(gedp->ged_wdbp->dbip, &rt_uniresource);

    // TODO - cache the initial list of tops objects - should not change,
    // regardless of what push operations are used

    // Perform the specified push operation on all example objects
    for (size_t i = 0; i <= 16; i++) {
	std::ostringstream ss;
	ss << "sph_";
	ss << std::setfill('0') << std::setw(3) << i;
	std::string oname = ss.str();
	std::cout << oname << "\n";
	char *gobj = bu_strdup(oname.c_str());
	gargv[gargc-2] = gobj;
	ged_npush(gedp, gargc - 1, (const char **)gargv);
	bu_free(gobj, "free objname");
	gargv[gargc-2] = NULL;
    }

    // To avoid problems with bu_argv_free, make very sure the last two entires
    // are not pointing to something else.
    gargv[gargc-2] = NULL;
    gargv[gargc-1] = NULL;
    bu_argv_free((size_t)gargc, gargv);

    // TODO - re-read tops objects, compare lists - should match


    // dbconcat the control file into the current database, with a prefix
    const char *dbcargv[4];
    dbcargv[0] = "dbconcat";
    dbcargv[1] = argv[0];
    dbcargv[2] = "ctrl_";
    dbcargv[3] = NULL;
    ged_concat(gedp, 3, (const char **)dbcargv);

    // object names may not be identical - what we are concerned with is that
    // the geometry shapes remain unchanged from what is expected (or, in a few
    // specific cases, we check that differences are found.) Use the libged
    // raytracing based gdiff and a parallel tree walk to check this.
    bool have_diff_vol = false;
    bool have_diff_struct = false;
    const char *gdiffargv[4];
    gdiffargv[0] = "gdiff";
    for (size_t i = 0; i <= 16; i++) {
	std::ostringstream ss;
	ss << "sph_";
	ss << std::setfill('0') << std::setw(3) << i;
	std::string oname = ss.str();
	char *gobj = bu_strdup(oname.c_str());
	std::string cname = std::string("ctrl_") + oname;
	char *cobj = bu_strdup(cname.c_str());
	gdiffargv[1] = cobj;
	gdiffargv[2] = gobj;
	bu_vls_trunc(gedp->ged_result_str, 0);
	ged_gdiff(gedp, 3, (const char **)gdiffargv);
	if (!BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), "0")) {
	    std::cout << cobj << " and " << gobj << " define different volumes\n";
	    have_diff_vol = true;
	}
	struct directory *dp1 = db_lookup(gedp->ged_wdbp->dbip, cobj, LOOKUP_NOISY);
	struct directory *dp2 = db_lookup(gedp->ged_wdbp->dbip, gobj, LOOKUP_NOISY);
	check_walk(&have_diff_struct, gedp->ged_wdbp->dbip, dp1, dp2);
	if (have_diff_struct)
	    std::cout << cobj << " and " << gobj << " differ structurally\n";
	bu_free(cobj, "ctrl objname");
	bu_free(gobj, "push objname");
    }


    if (!have_diff_vol && !have_diff_struct) {
	// Remove the copy of the .g file
	//std::remove(bu_vls_cstr(&gfile));
    }

    // Clean up
    bu_vls_free(&gfile);
    bu_vls_free(&gbasename);
    bu_vls_free(&wdir);

    if (have_diff_vol || have_diff_struct) {
	return -1;
    }

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
