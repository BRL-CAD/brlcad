/*                         L I N T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by
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
#include "../libbu/json.hpp"

extern "C" {
#include "wdb.h"
}
#include "bu/cmdschema.h"
#include "./ged_lint.h"

lint_data::lint_data()
{
    color = NULL;
    vlfree = &rt_vlfree;
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

    /* Raytrace result collections */
    struct rt_entry {
	std::string name;
	std::string type;
	std::string reason;
	double sa_err_pct  = -1.0;
	double vol_err_pct = -1.0;
    };
    std::vector<rt_entry> rt_mismatches;
    std::vector<rt_entry> rt_facetize_failed;
    std::vector<rt_entry> rt_skips;
    std::vector<rt_entry> rt_ok;

    for(nlohmann::json::const_iterator it = j.begin(); it != j.end(); ++it) {
	const nlohmann::json &pdata = *it;
	if (!pdata.contains("problem_type")) {
	    bu_log("Unexpected JSON entry\n");
	    continue;
	}
	std::string ptype(pdata["problem_type"]);

	/* ---- Raytrace result types ---- */
	if (!ptype.compare(0, 9, std::string("raytrace_"))) {
	    if (!pdata.contains("object_name")) continue;
	    rt_entry e;
	    e.name = std::string(pdata["object_name"]);
	    e.type = pdata.contains("object_type") ?
		std::string(pdata["object_type"]) : std::string("?");
	    if (pdata.contains("reason"))
		e.reason = std::string(pdata["reason"]);
	    if (pdata.contains("sa_err_pct"))
		e.sa_err_pct = pdata["sa_err_pct"].get<double>();
	    if (pdata.contains("vol_err_pct"))
		e.vol_err_pct = pdata["vol_err_pct"].get<double>();

	    if (ptype == std::string("raytrace_ok")) {
		rt_ok.push_back(e);
	    } else if (ptype == std::string("raytrace_mismatch")) {
		rt_mismatches.push_back(e);
	    } else if (ptype == std::string("raytrace_facetize_failed")) {
		rt_facetize_failed.push_back(e);
	    } else if (ptype == std::string("raytrace_skip")) {
		rt_skips.push_back(e);
	    }
	    continue;
	}

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

    /* ---- Raytrace results section ---- */
    size_t rt_n_ok   = rt_ok.size();
    size_t rt_n_fail = rt_mismatches.size();
    size_t rt_n_skip = rt_skips.size() + rt_facetize_failed.size();
    if (rt_n_ok + rt_n_fail + rt_n_skip > 0) {
	ostr.append(std::string("Raytrace validation: "));
	ostr.append(std::to_string(rt_n_ok) + std::string(" passed, "));
	ostr.append(std::to_string(rt_n_fail) + std::string(" failed, "));
	ostr.append(std::to_string(rt_n_skip) + std::string(" skipped\n"));

	if (!rt_mismatches.empty()) {
	    ostr.append(std::string("Raytrace mismatches (CSG raytracer disagrees with reference):\n"));
	    for (const auto &e : rt_mismatches) {
		ostr.append(std::string("\t") + e.name);
		ostr.append(std::string(" [") + e.type + std::string("]"));
		if (e.sa_err_pct >= 0.0 || e.vol_err_pct >= 0.0) {
		    ostr.append(std::string("  SA_err="));
		    if (e.sa_err_pct >= 0.0) {
			char buf[32];
			snprintf(buf, sizeof(buf), "%.1f%%", e.sa_err_pct);
			ostr.append(buf);
		    } else {
			ostr.append(std::string("N/A"));
		    }
		    ostr.append(std::string("  VOL_err="));
		    if (e.vol_err_pct >= 0.0) {
			char buf[32];
			snprintf(buf, sizeof(buf), "%.1f%%", e.vol_err_pct);
			ostr.append(buf);
		    } else {
			ostr.append(std::string("N/A"));
		    }
		}
		if (!e.reason.empty())
		    ostr.append(std::string("  (") + e.reason + std::string(")"));
		ostr.append(std::string("\n"));
	    }
	}

	if (!rt_facetize_failed.empty()) {
	    ostr.append(std::string("Raytrace checks skipped (facetize failed):\n"));
	    for (const auto &e : rt_facetize_failed)
		ostr.append(std::string("\t") + e.name +
			    std::string(" [") + e.type + std::string("]\n"));
	}

	/* Verbose: also show skipped objects and passing objects */
	if (verbosity > 0) {
	    if (!rt_skips.empty()) {
		ostr.append(std::string("Raytrace skipped objects:\n"));
		for (const auto &e : rt_skips) {
		    ostr.append(std::string("\t") + e.name);
		    if (!e.reason.empty())
			ostr.append(std::string(" (") + e.reason + std::string(")"));
		    ostr.append(std::string("\n"));
		}
	    }
	    if (!rt_ok.empty()) {
		ostr.append(std::string("Raytrace passing objects:\n"));
		for (const auto &e : rt_ok) {
		    ostr.append(std::string("\t") + e.name);
		    ostr.append(std::string(" [") + e.type + std::string("]"));
		    if (e.sa_err_pct >= 0.0) {
			char buf[64];
			snprintf(buf, sizeof(buf),
				 "  SA_err=%.2f%%  VOL_err=%.2f%%",
				 e.sa_err_pct, e.vol_err_pct);
			ostr.append(buf);
		    }
		    ostr.append(std::string("\n"));
		}
	    }
	}
    }

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

    /* Schema validation deliberately calls consumers with NULL storage. */
    if (!m)
	return 0;

    m->do_invalid = 1;

    if (argc) {
	if (!strchr(argv[0], ':'))
	    return 0;

	return techniques_parse(m->im_techniques, argv[0]);
    }

    return 0;
}


/* Preserve lint's long-standing optional-argument rule: a selector is only
 * consumed when it has the primitive:technique form.  A following object name
 * remains a positional operand, as in `lint -I all.g`. */
static size_t
invalid_opt_token_count(size_t available, const char **argv)
{
    if (!available || !argv || !argv[0] || !strchr(argv[0], ':'))
	return 0;
    return 1;
}

static const struct bu_cmd_arg_shape invalid_opt_shape = {
    BU_CMD_ARG_SHAPE_CUSTOM, 0, 1,
    "optional primitive:technique selector", invalid_opt_token_count
};


struct lint_args {
    int print_help;
    long verbosity;
    int cyclic_check;
    int missing_check;
    struct invalid_shape_methods invalid;
    struct bu_vls filter;
    struct bu_vls ofile;
    int visualize;
    fastf_t ftol;
    struct bu_vls gname;
    fastf_t min_tri_area;
    int do_raytrace;
    fastf_t rt_tol_pct;
    int do_rt_perturb;
};

static const struct bu_cmd_option lint_schema_options[] = {
    BU_CMD_FLAG("h", "help", lint_args, print_help, "Print help and exit"),
    BU_CMD_COUNTING_LONG_FLAG("v", "verbose", lint_args, verbosity,
	"Increase reporting verbosity"),
    BU_CMD_FLAG("C", "cyclic", lint_args, cyclic_check, "Check cyclic paths"),
    BU_CMD_FLAG("M", "missing", lint_args, missing_check, "Check missing references"),
    BU_CMD_OPTION_SHAPED("I", "invalid-shape", "invalid-shape", lint_args, invalid,
	BU_CMD_VALUE_CUSTOM, "check", "Check invalid shapes, optionally selecting techniques",
	BU_CMD_ARG_OPTIONAL, &invalid_opt_shape, invalid_opt_read),
    BU_CMD_VLS_APPEND("F", "filter", lint_args, filter, "pattern",
	"Apply search-style object filters"),
    BU_CMD_VLS_APPEND("j", "json-file", lint_args, ofile, "file",
	"Write full results as JSON"),
    BU_CMD_FLAG("V", "visualize", lint_args, visualize,
	"Visualize reported problems"),
    BU_CMD_NUMBER("t", "tol", lint_args, ftol, "number", "Numerical tolerance"),
    BU_CMD_VLS_APPEND("g", "group", lint_args, gname, "name",
	"Group problematic shapes"),
    BU_CMD_NUMBER(NULL, "min-tri-area", lint_args, min_tri_area, "number",
	"Minimum triangle area to report"),
    BU_CMD_FLAG(NULL, "raytrace", lint_args, do_raytrace,
	"Use raytrace validation mode"),
    BU_CMD_NUMBER(NULL, "raytrace-tol", lint_args, rt_tol_pct, "fraction",
	"Raytrace comparison tolerance"),
    BU_CMD_FLAG(NULL, "perturb", lint_args, do_rt_perturb,
	"Use perturbation during raytrace facetization"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand lint_schema_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_DB_PATH, 0, BU_CMD_COUNT_UNLIMITED,
	"Objects to inspect (defaults to all objects)", "ged.db_path"),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema lint_cmd_schema = {
    "lint", "Check database structure and geometry validity", lint_schema_options,
    lint_schema_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


static void
lint_help(struct ged *gedp, const char *usage)
{
    char *option_help = bu_cmd_schema_describe(&lint_cmd_schema);

    bu_vls_printf(gedp->ged_result_str, "%s", usage);
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "Options:\n%s\n", option_help);
	bu_free(option_help, "lint native option help");
    }
}


extern "C" int
ged_lint_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = BRLCAD_OK;
    static const char *usage = "Usage: lint [-h] [-v[v...]] [ -CMS ] [-F <filter>] [--raytrace [--perturb]] [obj1] [obj2] [...]\n";
    struct bn_tol lint_default_tol = BN_TOL_INIT_TOL;
    struct directory **dpa = NULL;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    // Primary data container to hold results
    lint_data ldata;
    ldata.gedp = gedp;

    struct lint_args args = {};
    args.ftol = lint_default_tol.dist;
    args.rt_tol_pct = 0.10;
    args.invalid.im_techniques = &ldata.im_techniques;
    bu_vls_init(&args.filter);
    bu_vls_init(&args.ofile);
    bu_vls_init(&args.gname);

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Parse the same native schema completion and validation use. */
    int operand_index = bu_cmd_schema_parse(&lint_cmd_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (operand_index >= 0) {
	argc -= operand_index;
	argv += operand_index;
    }

    if (args.print_help || operand_index < 0) {
	lint_help(gedp, usage);
	// TODO - autogenerate this list rather than hard coding...
	bu_vls_printf(gedp->ged_result_str, "\nInvalidity checks:\n");
	bu_vls_printf(gedp->ged_result_str, "\tbot:close_face\n");
	bu_vls_printf(gedp->ged_result_str, "\tbot:empty\n");
	bu_vls_printf(gedp->ged_result_str, "\tbot:non_solid\n");
	bu_vls_printf(gedp->ged_result_str, "\tbot:thin_volume\n");
	bu_vls_printf(gedp->ged_result_str, "\tbot:unexpected_hit\n");
	bu_vls_printf(gedp->ged_result_str, "\tbot:unexpected_miss\n");
	bu_vls_printf(gedp->ged_result_str, "\tarb:concave\n");
	bu_vls_printf(gedp->ged_result_str, "\tarb:non_coplanar_face\n");
	bu_vls_printf(gedp->ged_result_str, "\tarb:non_standard_ordering\n");
	bu_vls_printf(gedp->ged_result_str, "\tarb:twisted\n");
	bu_vls_printf(gedp->ged_result_str, "\tbrep:opennurbs\n");
	bu_vls_free(&args.filter);
	bu_vls_free(&args.ofile);
	bu_vls_free(&args.gname);
	return BRLCAD_OK;
    }

    if (bu_vls_strlen(&args.gname)) {
	if (gedp->dbip->dbi_read_only) {
	    bu_vls_printf(gedp->ged_result_str, "Database is read only, cannot write output comb %s\n", bu_vls_cstr(&args.gname));
	    bu_vls_free(&args.filter);
	    bu_vls_free(&args.ofile);
	    bu_vls_free(&args.gname);
	    return BRLCAD_ERROR;
	}
	if (db_lookup(gedp->dbip, bu_vls_cstr(&args.gname), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Output comb %s already exists in the database\n", bu_vls_cstr(&args.gname));
	    bu_vls_free(&args.filter);
	    bu_vls_free(&args.ofile);
	    bu_vls_free(&args.gname);
	    return BRLCAD_ERROR;
	}
    }

    if (bu_vls_strlen(&args.filter))
	ldata.filter = std::string(bu_vls_cstr(&args.filter));
    bu_vls_free(&args.filter);

    ldata.do_plot = (args.visualize) ? true : false;

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
	    bu_vls_free(&args.gname);
	    bu_vls_free(&args.ofile);
	    return BRLCAD_ERROR;
	}
    }

    ldata.argc = argc;
    ldata.dpa = dpa;
    ldata.verbosity = (int)args.verbosity;
    ldata.ftol = args.ftol;
    ldata.min_tri_area = args.min_tri_area;
    ldata.do_raytrace = (args.do_raytrace != 0);
    ldata.rt_do_perturb = (args.do_rt_perturb != 0);
    ldata.rt_tol_pct = (double)args.rt_tol_pct;

    int have_specific_test = args.cyclic_check + args.missing_check + args.invalid.do_invalid;

    /* --raytrace disables all other checks */
    if (ldata.do_raytrace) {
	bu_log("Running raytrace validation checks...\n");
	if (_ged_raytrace_check(&ldata) != BRLCAD_OK)
	    ret = BRLCAD_ERROR;
    } else {
	if (!have_specific_test || args.cyclic_check) {
	    bu_log("Checking for cyclic paths...\n");
	    if (_ged_cyclic_check(&ldata) != BRLCAD_OK)
		ret = BRLCAD_ERROR;
	}

	if (!have_specific_test || args.missing_check) {
	    bu_log("Checking for references to non-extant objects...\n");
	    if (_ged_missing_check(&ldata) != BRLCAD_OK)
		ret = BRLCAD_ERROR;
	}

	if (!have_specific_test || args.invalid.do_invalid) {
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
    }

    if (args.visualize) {
	struct bview *view = gedp->ged_gvp;
	if (gedp->new_cmd_forms) {
	    bv_vlblock_obj(ldata.vbp, view, "lint_visual");
	} else {
	    _ged_cvt_vlblock_to_solids(gedp, ldata.vbp, "lint_visual", 0);
	}
    }

    if (dpa)
	bu_free(dpa, "dp array");

    std::string report = ldata.summary();
    bu_vls_printf(gedp->ged_result_str, "%s", report.c_str());

    if (bu_vls_strlen(&args.ofile)) {
	std::ofstream jfile(bu_vls_cstr(&args.ofile));
	jfile << std::setw(2) << ldata.j << "\n";
	jfile.close();
    }

    if (bu_vls_strlen(&args.gname)) {
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
	    /* Only include raytrace failures, not passing or skipped objects */
	    if (!ptype.compare(0, 9, std::string("raytrace_"))) {
		if (ptype != std::string("raytrace_mismatch") &&
		    ptype != std::string("raytrace_facetize_failed"))
		    continue;
	    }
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
	    mk_lcomb(wdbp, bu_vls_cstr(&args.gname), &wcomb, 1, NULL, NULL, NULL, 0);
	}
    }

    bu_vls_free(&args.gname);
    bu_vls_free(&args.ofile);
    return ret;
}

#include "../include/plugin.h"

#define GED_LINT_COMMANDS(X, XID) \
    X(lint, ged_lint_core, GED_CMD_DEFAULT, &lint_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_LINT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_lint", 1, GED_LINT_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
