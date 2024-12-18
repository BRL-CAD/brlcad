/*                         L I N T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2024 United States Government as represented by
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
/** @file libged/lint.cpp
 *
 * The lint command for finding and reporting problems in .g files.
 */

#include "common.h"

#include <set>
#include <map>
#include <string>
#include <sstream>
#include <fstream>
#include "json.hpp"

extern "C" {
#include "bu/opt.h"
#include "wdb.h"
}
#include "./ged_lint.h"

lint_data::lint_data()
{
    color = NULL;
    vlfree = &RTG.rtg_vlfree;
    vbp = bv_vlblock_init(vlfree, 32);
}

lint_data::~lint_data()
{
    bv_vlblock_free(vbp);
    vbp = NULL;
    vlfree = NULL;
}

std::string
lint_data::summary()
{
    if (verbosity < 0)
	return std::string("");

    std::map<std::string, std::set<std::string>> categories;
    std::map<std::string, std::set<std::string>> obj_problems;

    for(nlohmann::json::const_iterator it = j.begin(); it != j.end(); ++it) {
	const nlohmann::json &pdata = *it;
	if (!pdata.contains("problem_type")) {
	    bu_log("Unexpected JSON entry\n");
	    continue;
	}
	std::string ptype(pdata["problem_type"]);

	if (ptype == std::string("cyclic_path")) {
	    if (!pdata.contains("path")) {
		bu_log("Error - malformed cyclic_path JSON data\n");
		continue;
	    }
	    std::string cpath(pdata["path"]);
	    categories[ptype].insert(cpath);
	    continue;
	}
	if (!ptype.compare(0, 7, std::string("missing"))) {
	    if (!pdata.contains("path")) {
		bu_log("Error - malformed missing reference JSON data\n");
		continue;
	    }
	    std::string mpath(pdata["path"]);
	    categories[std::string("missing")].insert(mpath);
	    continue;
	}

	// If it's not one of the above types, we're looking for an
	// object name and type
	if (!pdata.contains("object_name")) {
	    bu_log("JSON entry missing object name\n");
	    continue;
	}
	std::string oname(pdata["object_name"]);

	if (!pdata.contains("object_type")) {
	    bu_log("JSON entry missing object type\n");
	    continue;
	}
	std::string otype(pdata["object_type"]);

	categories[std::string("invalid")].insert(oname);
	std::string prob_type = otype + std::string(":") + ptype;
	obj_problems[oname].insert(prob_type);
    }

    std::string ostr;
    std::set<std::string>::iterator s_it, o_it;
    std::map<std::string, std::set<std::string>>::iterator c_it;
    c_it = categories.find(std::string("cyclic_path"));
    if (c_it != categories.end()) {
	const std::set<std::string> &cpaths = c_it->second;
	ostr.append(std::string("Found cyclic paths:\n"));
	for (s_it = cpaths.begin(); s_it != cpaths.end(); s_it++)
	    ostr.append(std::string("\t") + *s_it + std::string("\n"));
    }
    c_it = categories.find(std::string("missing"));
    if (c_it != categories.end()) {
	const std::set<std::string> &mpaths = c_it->second;
	ostr.append(std::string("Found references to missing objects or files:\n"));
	for (s_it = mpaths.begin(); s_it != mpaths.end(); s_it++)
	    ostr.append(std::string("\t") + *s_it + std::string("\n"));
    }

    c_it = categories.find(std::string("invalid"));
    if (c_it != categories.end()) {
	const std::set<std::string> &invobjs = c_it->second;
	ostr.append(std::string("Found invalid objects:\n"));
	for (s_it = invobjs.begin(); s_it != invobjs.end(); s_it++) {
	    ostr.append(std::string("\t") + *s_it);
	    ostr.append(std::string(" ["));
	    for (o_it = obj_problems[*s_it].begin(); o_it != obj_problems[*s_it].end(); o_it++) {
		ostr.append(*o_it + std::string(","));
	    }
	    ostr.pop_back();
	    ostr.append(std::string("]"));
	    ostr.append(std::string("\n"));
	}
    }

    return ostr;
}

struct invalid_shape_methods {
    int do_invalid = 0;
    std::map<std::string, std::set<std::string>> *im_techniques;
};

static int
techniques_parse(std::map<std::string, std::set<std::string>> *im_techniques, const char *instr)
{
    if (!im_techniques || !instr)
	return -1;

    // Split out individual specifiers
    std::string av0 = std::string(instr);
    std::stringstream astream(av0);
    std::string s;
    std::vector<std::string> methods;
    if (av0.find(' ') != std::string::npos) {
	while (std::getline(astream, s, ' ')) {
	    if (s.length())
		methods.push_back(s);
	}
    } else {
	methods.push_back(av0);
    }

    // For each specifier, we should have a primitive type and technique name
    for (size_t i = 0; i < methods.size(); i++) {
	std::string wopt = methods[i];
	if (wopt.find(':') == std::string::npos)
	    continue;
	std::stringstream ostream(wopt);
	std::string optstr;
	std::vector<std::string> key_val;
	while (std::getline(ostream, optstr, ':')) {
	    key_val.push_back(optstr);
	}
	if (key_val.size() != 2)
	    continue;
	(*im_techniques)[key_val[0]].insert(key_val[1]);
    }

    return (im_techniques->size() > 0) ? 1 : 0;
}

static
int invalid_opt_read(struct bu_vls *UNUSED(msg), size_t argc, const char **argv, void *set_var)
{
    struct invalid_shape_methods *m = (struct invalid_shape_methods *)set_var;
    m->do_invalid = 1;

    if (argc) {
	if (!strchr(argv[0], ':'))
	    return 0;

	return techniques_parse(m->im_techniques, argv[0]);
    }

    return 0;
}


extern "C" int
ged_lint_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = BRLCAD_OK;
    static const char *usage = "Usage: lint [-h] [-v[v...]] [ -CMS ] [-F <filter>] [obj1] [obj2] [...]\n";
    int print_help = 0;
    int verbosity = 0;
    int cyclic_check = 0;
    int missing_check = 0;
    int visualize = 0;
    fastf_t ftol = VUNITIZE_TOL;
    fastf_t min_tri_area = 0.0;
    struct directory **dpa = NULL;
    struct bu_vls filter = BU_VLS_INIT_ZERO;
    struct bu_vls ofile = BU_VLS_INIT_ZERO;
    struct bu_vls gname = BU_VLS_INIT_ZERO;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    // Primary data container to hold results
    lint_data ldata;
    ldata.gedp = gedp;

    struct invalid_shape_methods imethods;
    imethods.im_techniques = &ldata.im_techniques;

    struct bu_opt_desc d[12];
    BU_OPT(d[ 0],  "h", "help",                              "",  NULL,              &print_help,           "Print help and exit");
    BU_OPT(d[ 1],  "v", "verbose",                           "",  &_ged_vopt,        &verbosity,            "Verbose output (multiple flags increase verbosity)");
    BU_OPT(d[ 2],  "C", "cyclic",                            "",  NULL,              &cyclic_check,         "Check for cyclic paths (combs whose children reference their parents - potential for infinite looping)");
    BU_OPT(d[ 3],  "M", "missing",                           "",  NULL,              &missing_check,        "Check for objects referenced by other objects that are not in the database");
    BU_OPT(d[ 4],  "I", "invalid-shape",  "[check [check ...]]",  &invalid_opt_read, &imethods,             "Check for objects that are intended to be valid shapes but do not satisfy validity criteria (examples include non-solid BoTs and twisted arbs)");
    BU_OPT(d[ 5],  "F", "filter",                     "pattern",  &bu_opt_vls,       &filter,               "For checks on existing geometry objects, apply search-style filters to check only the subset of objects that satisfy the filters. Note that these filters do NOT impact cyclic and missing geometry checks.");
    BU_OPT(d[ 6],  "j", "json-file",                    "fname",  &bu_opt_vls,       &ofile,                "Write out the full lint data to a json file");
    BU_OPT(d[ 7],  "V", "visualize",                         "",  NULL,              &visualize,            "When problems can be visually represented, do so");
    BU_OPT(d[ 8],  "t", "tol",                              "#",  &bu_opt_fastf_t,   &ftol,                 "Numerical value to use when testing involves tolerances (defaults to VUNITIZE_TOL)");
    BU_OPT(d[ 9],  "g", "group",                         "name",  &bu_opt_vls,       &gname,                "Name of comb object in which to group all shape objects that report issues (will not contain cyclic paths or missing references).");
    BU_OPT(d[10],   "", "min-tri-area",                     "#",  &bu_opt_fastf_t,   &min_tri_area,         "Units are mm^2.  If specified, lint will not report any sampling problems where the seed triangle has < min-tri-area surface area (default is to report all problems).  Note that a miss of a problematically small triangle elsewhere in the shotline may still result in a shotlining error report - this filters only based on the first hit triangles.");
    BU_OPT_NULL(d[11]);

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* parse standard options */
    argc = bu_opt_parse(NULL, argc, argv, d);

    if (print_help || argc < 0) {
	_ged_cmd_help(gedp, usage, d);
	// TODO - autogenerate this list rather than hard coding...
	bu_vls_printf(gedp->ged_result_str, "\nInvalidity checks:\n");
	bu_vls_printf(gedp->ged_result_str, "\tbot:close_face\n");
	bu_vls_printf(gedp->ged_result_str, "\tbot:empty\n");
	bu_vls_printf(gedp->ged_result_str, "\tbot:non_solid\n");
	bu_vls_printf(gedp->ged_result_str, "\tbot:thin_volume\n");
	bu_vls_printf(gedp->ged_result_str, "\tbot:unexpected_hit\n");
	bu_vls_printf(gedp->ged_result_str, "\tbot:unexpected_miss\n");
	bu_vls_printf(gedp->ged_result_str, "\tbrep:opennurbs\n");
	bu_vls_free(&filter);
	bu_vls_free(&ofile);
	bu_vls_free(&gname);
	return BRLCAD_OK;
    }

    if (bu_vls_strlen(&gname)) {
	if (gedp->dbip->dbi_read_only) {
	    bu_vls_printf(gedp->ged_result_str, "Database is read only, cannot write output comb %s\n", bu_vls_cstr(&gname));
	    bu_vls_free(&filter);
	    bu_vls_free(&ofile);
	    bu_vls_free(&gname);
	    return BRLCAD_ERROR;
	}
	if (db_lookup(gedp->dbip, bu_vls_cstr(&gname), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Output comb %s already exists in the database\n", bu_vls_cstr(&gname));
	    bu_vls_free(&filter);
	    bu_vls_free(&ofile);
	    bu_vls_free(&gname);
	    return BRLCAD_ERROR;
	}
    }

    if (bu_vls_strlen(&filter))
	ldata.filter = std::string(bu_vls_cstr(&filter));
    bu_vls_free(&filter);

    ldata.do_plot = (visualize) ? true : false;

    if (argc) {
	dpa = (struct directory **)bu_calloc(argc+1, sizeof(struct directory *), "dp array");
	int nonexist_obj_cnt = _ged_sort_existing_objs(gedp->dbip, argc, argv, dpa);
	if (nonexist_obj_cnt) {
	    int i;
	    bu_vls_printf(gedp->ged_result_str, "Object argument(s) supplied to lint that do not exist in the database:\n");
	    for (i = argc - nonexist_obj_cnt - 1; i < argc; i++) {
		bu_vls_printf(gedp->ged_result_str, " %s\n", argv[i]);
	    }
	    bu_free(dpa, "dpa");
	    bu_vls_free(&gname);
	    bu_vls_free(&ofile);
	    return BRLCAD_ERROR;
	}
    }

    ldata.argc = argc;
    ldata.dpa = dpa;
    ldata.verbosity = verbosity;
    ldata.ftol = ftol;
    ldata.min_tri_area = min_tri_area;

    int have_specific_test = cyclic_check+missing_check+imethods.do_invalid;

    if (!have_specific_test || cyclic_check) {
	bu_log("Checking for cyclic paths...\n");
	if (_ged_cyclic_check(&ldata) != BRLCAD_OK)
	    ret = BRLCAD_ERROR;
    }

    if (!have_specific_test || missing_check) {
	bu_log("Checking for references to non-extant objects...\n");
	if (_ged_missing_check(&ldata) != BRLCAD_OK)
	    ret = BRLCAD_ERROR;
    }

    if (!have_specific_test || imethods.do_invalid) {
	bu_log("Checking for invalid objects...\n");

	// bu_log wipes out MGED when doing this - stash hooks
	struct bu_hook_list ohooks;
	bu_hook_list_init(&ohooks);
	bu_log_hook_save_all(&ohooks);
	bu_log_hook_delete_all();

	if (_ged_invalid_shape_check(&ldata) != BRLCAD_OK)
	    ret = BRLCAD_ERROR;

	// Restore hooks
	bu_log_hook_restore_all(&ohooks);
    }

    if (visualize) {
	const char *nview = getenv("GED_TEST_NEW_CMD_FORMS");
	struct bview *view = gedp->ged_gvp;
	if (BU_STR_EQUAL(nview, "1")) {
	    bv_vlblock_obj(ldata.vbp, view, "lint_visual");
	} else {
	    _ged_cvt_vlblock_to_solids(gedp, ldata.vbp, "lint_visual", 0);
	}
    }

    if (dpa)
	bu_free(dpa, "dp array");

    std::string report = ldata.summary();
    bu_vls_printf(gedp->ged_result_str, "%s", report.c_str());

    if (bu_vls_strlen(&ofile)) {
	std::ofstream jfile(bu_vls_cstr(&ofile));
	jfile << std::setw(2) << ldata.j << "\n";
	jfile.close();
    }

    if (bu_vls_strlen(&gname)) {
	std::set<std::string> onames;
	for(nlohmann::json::const_iterator it = ldata.j.begin(); it != ldata.j.end(); ++it) {
	    const nlohmann::json &pdata = *it;
	    if (!pdata.contains("problem_type"))
		continue;
	    std::string ptype(pdata["problem_type"]);
	    if (ptype == std::string("cyclic_path"))
		continue;
	    if (!ptype.compare(0, 7, std::string("missing")))
		continue;
	    if (!pdata.contains("object_name"))
		continue;

	    std::string oname(pdata["object_name"]);
	    onames.insert(oname);
	}

	// Make a comb to hold the problematic primitives
	if (onames.size()) {
	    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
	    struct wmember wcomb;
	    BU_LIST_INIT(&wcomb.l);
	    std::set<std::string>::iterator o_it;
	    for (o_it = onames.begin(); o_it != onames.end(); o_it++)
		(void)mk_addmember(o_it->c_str(), &(wcomb.l), NULL, DB_OP_UNION);
	    mk_lcomb(wdbp, bu_vls_cstr(&gname), &wcomb, 1, NULL, NULL, NULL, 0);
	}
    }

    bu_vls_free(&gname);
    bu_vls_free(&ofile);
    return ret;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
extern "C" {
    struct ged_cmd_impl lint_cmd_impl = { "lint", ged_lint_core, GED_CMD_DEFAULT };
    const struct ged_cmd lint_cmd = { &lint_cmd_impl };
    const struct ged_cmd *lint_cmds[] = { &lint_cmd,  NULL };

    static const struct ged_plugin pinfo = { GED_API,  lint_cmds, 1 };

    COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
    {
	return &pinfo;
    }
}
#endif

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

