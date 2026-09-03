/*                     F A C E T I Z E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/facetize.cpp
 *
 * The facetize command.
 *
 */

#include "common.h"

#include <charconv>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/path.h"
#include "bu/opt.h"
#include "wdb.h"

#include "../ged_private.h"

#define TESS_OPTS_IMPLEMENTATION
#include "./tess_opts.h"
#include "./ged_facetize.h"

static int
facetize_process_output(struct bu_vls *output, const char **command)
{
    if (!output || !command || !command[0])
	return BRLCAD_ERROR;

    bu_vls_trunc(output, 0);
    struct bu_process *process = NULL;
    bu_process_create(&process, command,
	    BU_PROCESS_HIDE_WINDOW | BU_PROCESS_OUT_EQ_ERR);
    if (!process)
	return BRLCAD_ERROR;

    char buffer[BUFSIZ];
    int count = 0;
    while ((count = bu_process_read_n(process, BU_PROCESS_STDOUT,
		    (int)sizeof(buffer), buffer)) > 0)
	bu_vls_strncat(output, buffer, (size_t)count);

    int status = bu_process_wait_n(&process, 0);
    return (status == 0 && bu_vls_strlen(output)) ?
	BRLCAD_OK : BRLCAD_ERROR;
}

void
_facetize_methods_help(struct ged *gedp)
{
    // Build up the path to the ged_exec executable
    char tess_exec[MAXPATHLEN];
    bu_dir(tess_exec, MAXPATHLEN, BU_DIR_BIN, "ged_exec", BU_DIR_EXT, NULL);

    const char *tess_cmd[5] = {NULL};
    tess_cmd[0] = tess_exec;
    tess_cmd[1] = "facetize_process";
    tess_cmd[2] = "--list-methods";
    tess_cmd[3] = NULL;

    struct bu_vls method_output = BU_VLS_INIT_ZERO;
    if (facetize_process_output(&method_output, tess_cmd) != BRLCAD_OK) {
	bu_vls_free(&method_output);
	return;
    }
    bu_vls_printf(gedp->ged_result_str,
	    "Available BoT tessellation methods: %s\n",
	    bu_vls_cstr(&method_output));

    tess_cmd[3] = "-h";
    tess_cmd[4] = NULL;

    if (facetize_process_output(&method_output, tess_cmd) == BRLCAD_OK)
	bu_vls_printf(gedp->ged_result_str,
		"\nMethod specific options:\n\n%s\n",
		bu_vls_cstr(&method_output));
    bu_vls_free(&method_output);
}

int
_ged_facetize_objs(struct _ged_facetize_state *s, const FacetizePlan &plan)
{
    int ret = BRLCAD_ERROR;
    int ok_cnt = 0;
    struct db_i *dbip = s->dbip;
    RT_CHECK_DBI(dbip);

    int argc = (int)plan.inputs.size();
    std::vector<const char *> argv = plan.input_argv();
    struct directory **dpa = (struct directory **)bu_calloc(argc,
	    sizeof(struct directory *), "facetize input directory array");
    for (int i = 0; i < argc; i++) {
	dpa[i] = db_lookup(dbip, argv[i], LOOKUP_QUIET);
	if (!dpa[i]) {
	    facetize_failure(s, "input object '%s' disappeared before evaluation", argv[i]);
	    bu_free(dpa, "facetize input directory array");
	    return BRLCAD_ERROR;
	}
    }

    const char *output_name = plan.execution.writes_in_place() ?
	NULL : plan.output.c_str();

    if (plan.execution.uses_nmg_boolean()) {
	if (!plan.execution.writes_in_place()) {
	    ret = _ged_facetize_nmgeval(s, argc, argv.data(), output_name);
	    goto booleval_cleanup;
	}
	for (int i = 0; i < argc; i++) {
	    const char *object_argv[] = {argv[i], NULL};
	    ret = _ged_facetize_nmgeval(s, 1, object_argv, argv[i]);
	    if (ret == BRLCAD_ERROR && s->tolerate_failures) {
		facetize_tolerated_failure(s,
			"object '%s' failed during NMG boolean evaluation and was skipped",
			argv[i]);
		continue;
	    }
	    if (ret == BRLCAD_ERROR)
		goto booleval_cleanup;
	    ok_cnt++;
	}
	if (s->tolerate_failures && ok_cnt > 0)
	    ret = BRLCAD_OK;
	goto booleval_cleanup;
    }

    if (!plan.execution.writes_in_place()) {
	ret = _ged_facetize_booleval(s, argc, dpa, output_name, false, false);
    } else {
	for (int i = 0; i < argc; i++) {
	    struct directory *object_dpa[] = {dpa[i], NULL};
	    ret = _ged_facetize_booleval(s, 1, object_dpa, argv[i], false, false);
	    if (ret == BRLCAD_ERROR && s->tolerate_failures) {
		facetize_tolerated_failure(s,
			"object '%s' failed during BoT boolean evaluation and was skipped",
			argv[i]);
		continue;
	    }
	    if (ret == BRLCAD_ERROR)
		goto booleval_cleanup;
	    ok_cnt++;
	}
	if (s->tolerate_failures && ok_cnt > 0)
	    ret = BRLCAD_OK;
    }

    // Report on the primitive processing
    facetize_collect_primitive_summary(s);

    // After collecting info for summary, we can now clean up working files
    bu_dirclear(s->wdir);

booleval_cleanup:
    bu_free(dpa, "facetize input directory array");

    return ret;
}

extern "C" int
ged_facetize_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = BRLCAD_OK;
    static const char *usage = "Usage: facetize [options] [old_obj1 ...] [new_obj]\n";
    int print_help = 0;
    int need_help = 0;
    int quiet = 0;
    long verbosity = 0;
    int force_perturb = 0;
    int disable_perturb = 0;
    int nmg_output = 0;
    int region_mode = 0;
    int in_place = 0;
    int nmg_boolean = 0;
    int nonoverlap_brep = 0;
    bool use_perturbation = false;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    FacetizeSession session(gedp);
    struct _ged_facetize_state *s = session.state();
    method_options_t *method_options = session.method_options();
    FacetizePlan plan;

    /* General options */
    struct bu_opt_desc d[26];
    BU_OPT(d[ 0], "h", "help",                                      "",                  NULL,           &print_help, "Print help and exit");
    BU_OPT(d[ 1], "v", "verbose",                                   "",  &bu_opt_incr_long,       &verbosity, "Verbose output (multiple flags increase verbosity)");
    BU_OPT(d[ 2], "q", "quiet",                                     "",                  NULL,                &quiet, "Suppress all output (overrides verbose flag)");
    BU_OPT(d[ 3], "n", "nmg-output",                                "",                  NULL,           &nmg_output, "Create an N-Manifold Geometry (NMG) object (default is to create a triangular BoT mesh).  Note that this will disable most other processing options and may reduce the conversion success rate.");
    BU_OPT(d[ 4], "r", "regions",                                   "",                  NULL,          &region_mode, "For combs, walk the trees and create new copies of the hierarchies with each region's CSG tree replaced by a facetized evaluation of that region.  By default, enables perturb methodology (can be disabled - see --no-perturb)");
    BU_OPT(d[ 5], "s", "suffix",                               "<str>",           &bu_opt_vls,             s->suffix, "When creating new objects for facetize outputs, use this suffix to avoid conflicts");
    BU_OPT(d[ 6], "p", "prefix",                               "<str>",           &bu_opt_vls,             s->prefix, "When creating new objects for facetize, use this prefix to avoid conflicts");
    BU_OPT(d[ 7],  "", "in-place",                                  "",                  NULL,              &in_place, "Replace the specified object(s) with their facetizations. (Warning: this option changes pre-existing geometry!)");
    BU_OPT(d[ 8],  "", "max-time",                                 "#",           &bu_opt_int,        &(s->max_time), "Maximum time to spend per object (in seconds).  Default is method specific.  Note that specifying shorter times may cut off conversions (particularly using sampling methods) that could succeed with longer runtimes.  Per-method time limits can also be adjusted to allow longer runtimes on slower methods.");
    BU_OPT(d[ 9],  "", "max-pnts",                                 "#",           &bu_opt_int,        &(s->max_pnts), "Maximum number of pnts per object to use when applying ray sampling methods.");
    BU_OPT(d[10],  "", "resume",                                    "",                  NULL,          &(s->resume), "Resume an interrupted conversion");
    BU_OPT(d[11],  "", "methods",                          "m1,m2,...", &_tess_active_methods,        method_options, "Specify methods to use when tessellating primitives into BoTs.");
    BU_OPT(d[12],  "", "method-opts",    "METHOD opt1=val opt2=val...",    &_tess_method_opts,        method_options, "For the specified method, set the specified options.");
    BU_OPT(d[13],  "", "no-empty",                                  "",                  NULL,        &(s->no_empty), "Do not output empty BoT objects if the boolean evaluation results in an empty solid.");
    BU_OPT(d[14],  "", "log-file",                        "<filename>",           &bu_opt_vls,           s->log_file, "Specify a location to use for the log file.");
    BU_OPT(d[15],  "", "nmg-booleval",                               "",                  NULL,           &nmg_boolean, "Use libnmg Boolean evaluation algorithm, even if we're producing a BoT.  Less robust, but if it succeeds it may produce cleaner output for coplanar inputs.");
    BU_OPT(d[16],  "", "disable-fixup",                             "",                  NULL,          &s->no_fixup, "Disable post-processing steps intended to improve generated meshes.");
    BU_OPT(d[17],  "", "perturb",                                   "",                  NULL,        &force_perturb, "Enable the coplanarity-avoidance perturbation step (overrides non -r option default, conflicts with --no-perturb).");
    BU_OPT(d[18],  "", "no-perturb",                                "",                  NULL,      &disable_perturb, "Disable the coplanarity-avoidance perturbation step (overrides -r option default, conflicts with --perturb).");
    BU_OPT(d[19], "B", "",                                          "",                  NULL,       &nonoverlap_brep, "EXPERIMENTAL: non-overlapping facetization to BoT objects of union-only brep comb tree.");
    BU_OPT(d[20], "t", "threshold",                                "#",       &bu_opt_fastf_t, &s->nonovlp_threshold, "EXPERIMENTAL: max ovlp threshold length for -B mode.");
    BU_OPT(d[21],  "", "perturb-sa-tol",                           "#",       &bu_opt_fastf_t,   &s->perturb_sa_tol,  "Surface-area percentage threshold (0-100) that triggers the coplanarity-avoidance perturb retry when the CSG Crofton SA differs from the BoT SA by more than this amount. Default is 10.");
    BU_OPT(d[22],  "", "perturb-vol-tol",                          "#",       &bu_opt_fastf_t,   &s->perturb_vol_tol, "Volume percentage threshold (0-100) that triggers the coplanarity-avoidance perturb retry when the CSG Crofton volume differs from the BoT volume by more than this amount. Default is 10.");
    BU_OPT(d[23],  "", "tolerate-failures",                         "",                  NULL, &s->tolerate_failures, "Continue after failed primitive or subtree evaluations and generate a partial result.  The output will not be a complete representation of the input if any failures are tolerated.");
    BU_OPT(d[24], "j", "jobs",                                     "#",           &bu_opt_int,     &s->max_workers, "Maximum number of facetize worker processes.  The default is selected conservatively from CPU and memory availability.");
    BU_OPT_NULL(d[25]);

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* parse standard options */
    struct bu_vls omsg = BU_VLS_INIT_ZERO;
    argc = bu_opt_parse(&omsg, argc, argv, d);
    if (argc < 0) {
	bu_vls_printf(gedp->ged_result_str, "option parsing failed: %s\n", bu_vls_cstr(&omsg));
	ret = BRLCAD_ERROR;
	bu_vls_free(&omsg);
	goto ged_facetize_done;
    }
    bu_vls_free(&omsg);

    // Sanity
    if (force_perturb && disable_perturb) {
    	bu_vls_printf(gedp->ged_result_str, "Can only specify one of --perturb or --no-perturb\n");
	ret = BRLCAD_ERROR;
	goto ged_facetize_done;
    }
    if (s->max_workers < 0 || s->max_workers > MAX_PSW) {
	bu_vls_printf(gedp->ged_result_str,
		"--jobs must be between 0 and %d\n", MAX_PSW);
	ret = BRLCAD_ERROR;
	goto ged_facetize_done;
    }
    if (s->max_time < 0) {
	bu_vls_printf(gedp->ged_result_str,
		"--max-time must be greater than or equal to 0\n");
	ret = BRLCAD_ERROR;
	goto ged_facetize_done;
    }
    if (s->max_pnts < 0) {
	bu_vls_printf(gedp->ged_result_str,
		"--max-pnts must be greater than or equal to 0\n");
	ret = BRLCAD_ERROR;
	goto ged_facetize_done;
    }

    if (region_mode && nonoverlap_brep) {
	bu_vls_printf(gedp->ged_result_str,
		"--regions and -B select different processing scopes and cannot be combined\n");
	ret = BRLCAD_ERROR;
	goto ged_facetize_done;
    }

    s->execution.output_format = nmg_output ?
	FacetizeOutputFormat::Nmg : FacetizeOutputFormat::Bot;
    s->execution.boolean_engine = (nmg_output || nmg_boolean) ?
	FacetizeBooleanEngine::Nmg : FacetizeBooleanEngine::Manifold;
    s->execution.scope = nonoverlap_brep ? FacetizeScope::NonOverlappingBrep :
	(region_mode ? FacetizeScope::Regions : FacetizeScope::Objects);
    s->execution.commit_mode = in_place ?
	FacetizeCommitMode::InPlace : FacetizeCommitMode::NewObject;
    use_perturbation = region_mode;
    if (disable_perturb)
	use_perturbation = false;
    if (force_perturb)
	use_perturbation = true;
    s->execution.perturb_mode = use_perturbation ?
	FacetizePerturbMode::Enabled : FacetizePerturbMode::Disabled;

    s->verbosity = (int)verbosity;

    // If we got a max-time top level arg, override any times that aren't specifically set
    // by method options
    if (s->max_time) {
	for (auto &method_time : method_options->max_time) {
	    auto options_it = method_options->options_map.find(method_time.first);
	    bool explicitly_set = options_it != method_options->options_map.end() &&
		options_it->second.find("max_time") != options_it->second.end();
	    if (!explicitly_set) {
		// max-time wasn't explicitly set by a method, and we have an option - override
		method_time.second = s->max_time;
		method_options->options_map[method_time.first]["max_time"] =
		    std::to_string(s->max_time);
	    }
	}
    }

    /* Sync -q and -v options */
    if (quiet)
	s->verbosity = -1;

    /* Don't allow incorrect type suffixes */
    if (s->execution.writes_nmg() && BU_STR_EQUAL(bu_vls_cstr(s->solid_suffix), ".bot")) {
	bu_vls_sprintf(s->solid_suffix, ".nmg");
    }
    if (!s->execution.writes_nmg() && BU_STR_EQUAL(bu_vls_cstr(s->solid_suffix), ".nmg")) {
	bu_vls_sprintf(s->solid_suffix, ".bot");
    }

    /* Check if we want/need help */
    need_help = argc < (s->execution.writes_in_place() ? 1 : 2);
    if (print_help || need_help) {
	_ged_cmd_help(gedp, usage, d);
	_facetize_methods_help(gedp);
	ret = (need_help) ? BRLCAD_ERROR : BRLCAD_OK;
	goto ged_facetize_done;
    }

    if (s->execution.processes_breps() &&
	    NEAR_ZERO(s->nonovlp_threshold, SMALL_FASTF)) {
	bu_vls_printf(gedp->ged_result_str,
		"-B option requires a specified length threshold\n");
	ret = BRLCAD_ERROR;
	goto ged_facetize_done;
    }

    if (facetize_build_plan(s, argc, argv, plan) != BRLCAD_OK ||
	    facetize_prepare_workspace(s, plan) != BRLCAD_OK) {
	ret = BRLCAD_ERROR;
	goto ged_facetize_done;
    }
    ret = facetize_execute_plan(s, plan);

ged_facetize_done:
    return ret;
}

#include "../include/plugin.h"

#define GED_FACETIZE_COMMANDS(X, XID) \
    X(facetize, ged_facetize_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_FACETIZE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_facetize", 1, GED_FACETIZE_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
