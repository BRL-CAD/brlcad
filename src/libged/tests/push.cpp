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
 * TODO - see if a version of this can be added to the npush command itself as
 * a -V (structural validate) option - (or -VV for both volumetric and
 * structural validation).
 *
 * Ideally, should have -V take an optional argument so we can compare to an
 * existing tree instead of keeping out the target tree.
 *
 * Note that -V would be incompatible with the dry-run option, since the point
 * is to evaluate the final generated geometry.
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
	char *gobj = bu_strdup(oname.c_str());
	gargv[gargc-2] = gobj;
	char **av = bu_argv_dup(gargc-1, (const char **)gargv);
	ged_npush(gedp, gargc - 1, (const char **)av);
	bu_argv_free((size_t)gargc-1, av);
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
	std::cout << "\nChecking: " << cobj << " and " << gobj << ":\n";
	gdiffargv[1] = cobj;
	gdiffargv[2] = gobj;
	bu_vls_trunc(gedp->ged_result_str, 0);
	ged_gdiff(gedp, 3, (const char **)gdiffargv);
	if (!BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), "0")) {
	    std::cout << "***VOL DIFF***\n";
	    have_diff_vol = true;
	} else {
	    std::cout << "VOL match\n";
	}
	gdiffargv[1] = "-S";
	gdiffargv[2] = cobj;
	gdiffargv[3] = gobj;
	bu_vls_trunc(gedp->ged_result_str, 0);
	ged_gdiff(gedp, 4, (const char **)gdiffargv);
	if (!BU_STR_EQUAL(bu_vls_cstr(gedp->ged_result_str), "0")) {
	    std::cout << "***STRUCT DIFF***\n";
	    have_diff_struct = true;
	} else {
	    std::cout << "STRUCT match\n";
	}

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
	bu_log("Found differences.\n");
	return -1;
    } else {
	bu_log("Push results match.\n");
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
