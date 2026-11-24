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
#include <unordered_set>
#include <vector>

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/file.h"
#include "bu/path.h"
#include "wdb.h"

#include "../ged_private.h"

#define TESS_OPTS_IMPLEMENTATION
#include "./tess_opts.h"
#include "./ged_facetize.h"
#include "./transfer.h"

static const char FACETIZE_OUTPUT_FILE_SUFFIX[] = "_outputs.g";

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

struct facetize_parse_args {
    int print_help = 0;
    long verbosity = 0;
    int quiet = 0;
    int make_nmg = 0;
    int regions = 0;
    int leaves = 0;
    const char *suffix = NULL;
    const char *prefix = NULL;
    int in_place = 0;
    int max_time = 0;
    int max_pnts = 0;
    int resume = 0;
    method_options_t *method_options = NULL;
    int no_empty = 0;
    const char *log_file = NULL;
    int nmg_booleval = 0;
    int no_fixup = 0;
    int force_perturb = 0;
    int disable_perturb = 0;
    int nonovlp_brep = 0;
    fastf_t nonovlp_threshold = 0.0;
    fastf_t perturb_sa_tol = 10.0;
    fastf_t perturb_vol_tol = 10.0;
    int tolerate_failures = 0;
    int max_workers = 0;
};

static const struct bu_arg_schema *facetize_schema(void);


static void
facetize_show_schema_help(struct ged *gedp, bool extended)
{
    char *help = bu_arg_schema_help_mode(facetize_schema(), "facetize",
	extended ? BU_ARG_HELP_EXTENDED : BU_ARG_HELP_DEFAULT);

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "facetize native schema help");
    }
}

static int
facetize_commit_outputs(struct _ged_facetize_state *s,
	const std::vector<std::string> &output_names, const char *new_root = nullptr)
{
    if (!s || output_names.empty())
	return BRLCAD_ERROR;

    std::string keep_file = std::string(bu_vls_cstr(s->wfile)) +
	FACETIZE_OUTPUT_FILE_SUFFIX;
    (void)bu_file_delete(keep_file.c_str());
    struct ged *working_gedp = ged_open("db", bu_vls_cstr(s->wfile), 1);
    if (!working_gedp) {
	facetize_failure(s, "unable to open staged database '%s'",
		bu_vls_cstr(s->wfile));
	return BRLCAD_ERROR;
    }

    std::vector<const char *> keep_argv;
    keep_argv.reserve(output_names.size() + 2);
    keep_argv.push_back("keep");
    keep_argv.push_back(keep_file.c_str());
    for (const std::string &output_name : output_names)
	keep_argv.push_back(output_name.c_str());
    int keep_result = ged_exec_keep(working_gedp, (int)keep_argv.size(),
	    keep_argv.data());
    ged_close(working_gedp);
    if (keep_result != BRLCAD_OK) {
	facetize_failure(s, "unable to stage facetize outputs for import");
	(void)bu_file_delete(keep_file.c_str());
	return BRLCAD_ERROR;
    }

    const std::string affix = new_root ? std::string(new_root) + "_" : "";
    const char *concat_argv[] = {"dbconcat", new_root ? "-L" : "-O",
	keep_file.c_str(), new_root ? affix.c_str() : NULL, NULL};
    int concat_result = ged_exec_dbconcat(s->gedp, new_root ? 4 : 3, concat_argv);
    (void)bu_file_delete(keep_file.c_str());
    if (concat_result != BRLCAD_OK ||
	    (new_root && !db_lookup(s->dbip, new_root, LOOKUP_QUIET))) {
	facetize_failure(s, "unable to import staged facetize outputs");
	return BRLCAD_ERROR;
    }
    bu_vls_trunc(s->gedp->ged_result_str, 0);
    db_update_nref(s->dbip);
    return BRLCAD_OK;
}

static bool
facetize_leaf_outputs(std::vector<std::string> &outputs,
	struct _ged_facetize_state *s, const struct bu_ptbl &leaves)
{
    struct db_i *work = db_open(bu_vls_cstr(s->wfile), DB_OPEN_READWRITE);
    if (!work || db_dirbuild(work) < 0) {
	if (work)
	    db_close(work);
	facetize_failure(s, "unable to open the completed leaf database");
	return false;
    }

    bool valid = true;
    for (size_t i = 0; i < leaves.end; ++i) {
	auto *source = reinterpret_cast<struct directory *>(BU_PTBL_GET(&leaves, i));
	const char *name = source->d_namep;
	struct directory *result = db_lookup(work, name, LOOKUP_QUIET);
	struct bu_attribute_value_set attrs = BU_AVS_INIT_ZERO;
	if (!result || db5_get_attributes(work, &attrs, result) < 0) {
	    facetize_failure(s, "unable to read leaf result '%s'", name);
	    bu_avs_free(&attrs);
	    valid = false;
	    break;
	}
	const char *method = bu_avs_get(&attrs, FACETIZE_METHOD_ATTR);
	const bool failed = method && BU_STR_EQUAL(method, "FAIL");
	const bool bot = result->d_major_type == DB5_MAJORTYPE_BRLCAD &&
	    result->d_minor_type == ID_BOT;
	const bool partition = method && BU_STR_EQUAL(method, FACETIZE_PARTITION_METHOD) &&
	    result->d_major_type == DB5_MAJORTYPE_BRLCAD && result->d_minor_type == ID_COMBINATION;
	if (failed || (!bot && !partition)) {
	    bu_avs_free(&attrs);
	    if (s->tolerate_failures) {
		// The worker has already recorded a diagnosed tessellation failure.
		if (!failed)
		    facetize_tolerated_failure(s, "no facetized leaf for '%s'; leaf left unchanged", name);
		continue;
	    }
	    facetize_failure(s, "no facetized leaf for '%s'", name);
	    valid = false;
	    break;
	}

	/* Preserve the original object's display and material attributes. */
	struct bu_attribute_value_set original = BU_AVS_INIT_ZERO;
	if (db5_get_attributes(s->dbip, &original, source) < 0) {
	    facetize_failure(s, "unable to read attributes of '%s'", name);
	    bu_avs_free(&attrs);
	    bu_avs_free(&original);
	    valid = false;
	    break;
	}
	if (method)
	    bu_avs_add(&original, FACETIZE_METHOD_ATTR, method);
	bu_avs_free(&attrs);
	if (original.count && db5_update_attributes(result, &original, work) < 0) {
	    facetize_failure(s, "unable to preserve attributes of '%s'", name);
	    bu_avs_free(&original);
	    valid = false;
	    break;
	}
	bu_avs_free(&original);
	outputs.emplace_back(name);
    }
    db_close(work);
    return valid;
}

static bool
facetize_find_leaf_objects(std::vector<struct directory *> &leaves,
	struct db_i *dbip, std::vector<struct directory *> pending)
{
    std::unordered_set<struct directory *> visited;
    while (!pending.empty()) {
	struct directory *dp = pending.back();
	pending.pop_back();
	if (!visited.insert(dp).second)
	    continue;

	if (!(dp->d_flags & RT_DIR_COMB)) {
	    leaves.push_back(dp);
	    continue;
	}

	// A combination can be instanced many times. Its leaves need only one
	// mesh per object, since the original instance matrices stay in the tree.
	struct rt_db_internal intern;
	RT_DB_INTERNAL_INIT(&intern);
	if (rt_db_get_internal(&intern, dp, dbip, NULL) != ID_COMBINATION) {
	    rt_db_free_internal(&intern);
	    return false;
	}
	struct directory **children = NULL;
	const int count = db_comb_children(dbip,
	    static_cast<struct rt_comb_internal *>(intern.idb_ptr), &children, NULL, NULL);
	rt_db_free_internal(&intern);
	if (count)
	    pending.insert(pending.end(), children, children + count);
	if (children)
	    bu_free(children, "facetize combination children");
    }
    return true;
}

static bool
facetize_leaf_output_root(struct _ged_facetize_state *s, const FacetizePlan &plan)
{
    struct ged *work = ged_open("db", bu_vls_cstr(s->wfile), 1);
    if (!work)
	return false;

    // Reserve the requested name before workers choose partition child names.
    std::vector<const char *> args = {"g", plan.output.c_str()};
    for (const std::string &input : plan.inputs)
	args.push_back(input.c_str());
    const bool exists = s->resume &&
	db_lookup(work->dbip, plan.output.c_str(), LOOKUP_QUIET);
    const bool ok = exists || ged_exec_g(work, args.size(), args.data()) == BRLCAD_OK;
    ged_close(work);
    return ok;
}

int
_ged_facetize_leaves(struct _ged_facetize_state *s, const FacetizePlan &plan)
{
    std::vector<struct directory *> roots;
    roots.reserve(plan.inputs.size());
    for (const std::string &name : plan.inputs) {
	struct directory *dp = db_lookup(s->dbip, name.c_str(), LOOKUP_QUIET);
	if (!dp) {
	    facetize_failure(s, "input object '%s' disappeared before leaf tessellation", name.c_str());
	    return BRLCAD_ERROR;
	}
	roots.push_back(dp);
    }

    std::vector<struct directory *> candidates;
    if (!facetize_find_leaf_objects(candidates, s->dbip, std::move(roots))) {
	facetize_failure(s, "unable to read a combination while finding input primitives");
	return BRLCAD_ERROR;
    }
    if (candidates.empty())
	return BRLCAD_OK;
    struct bu_ptbl leaves = BU_PTBL_INIT_ZERO;
    const int count = db_search(&leaves, DB_SEARCH_FLAT | DB_SEARCH_HIDDEN,
	"-type shape -or -type pnts", candidates.size(), candidates.data(), s->dbip,
	NULL, NULL, NULL);
    if (count <= 0) {
	bu_ptbl_free(&leaves);
	if (count < 0) {
	    facetize_failure(s, "unable to find the input primitives for leaf tessellation");
	    return BRLCAD_ERROR;
	}
	return BRLCAD_OK;
    }

    if (_ged_facetize_working_file_setup(s, &leaves) != BRLCAD_OK) {
	bu_ptbl_free(&leaves);
	facetize_failure(s, "unable to prepare the leaf tessellation database");
	return BRLCAD_ERROR;
    }

    const bool in_place = plan.execution.writes_in_place();
    if (!in_place && !facetize_leaf_output_root(s, plan)) {
	bu_ptbl_free(&leaves);
	facetize_failure(s, "unable to create output combination '%s'", plan.output.c_str());
	return BRLCAD_ERROR;
    }

    const int tessellated = _ged_facetize_leaves_tri(s, s->dbip, &leaves);
    std::vector<std::string> outputs;
    const bool valid = tessellated == BRLCAD_OK && facetize_leaf_outputs(outputs, s, leaves);
    bu_ptbl_free(&leaves);
    facetize_collect_primitive_summary(s);
    if (!valid)
	return BRLCAD_ERROR;
    if (outputs.empty()) {
	facetize_failure(s, "no primitive leaves completed tessellation");
	return BRLCAD_ERROR;
    }
    if (!in_place)
	outputs = {plan.output};
    const int committed = facetize_commit_outputs(s, outputs,
	in_place ? nullptr : plan.output.c_str());
    if (committed == BRLCAD_OK)
	s->cleanup_workspace = true;
    return committed;
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
	if (_ged_facetize_working_file_setup(s, NULL) != BRLCAD_OK) {
	    facetize_failure(s,
		    "unable to prepare the NMG Boolean working database");
	    goto booleval_cleanup;
	}
	struct db_i *work_dbip = db_open(bu_vls_cstr(s->wfile),
		DB_OPEN_READWRITE);
	if (!work_dbip || db_dirbuild(work_dbip) < 0) {
	    if (work_dbip)
		db_close(work_dbip);
	    facetize_failure(s,
		    "unable to open the NMG Boolean working database");
	    goto booleval_cleanup;
	}

	std::vector<std::string> successful_outputs;
	if (!plan.execution.writes_in_place()) {
	    ret = _ged_facetize_nmgeval(s, work_dbip,
		    s->dbip->dbi_filename, plan.inputs, output_name);
	    if (ret == BRLCAD_OK)
		successful_outputs.emplace_back(output_name);
	} else {
	    for (int i = 0; i < argc; i++) {
		std::vector<std::string> object_input(1, plan.inputs[i]);
		ret = _ged_facetize_nmgeval(s, work_dbip,
			s->dbip->dbi_filename, object_input, argv[i]);
		if (ret == BRLCAD_ERROR && s->tolerate_failures) {
		    facetize_tolerated_failure(s,
			    "object '%s' failed during NMG boolean evaluation and was skipped",
			    argv[i]);
		    continue;
		}
		if (ret == BRLCAD_ERROR)
		    break;
		successful_outputs.emplace_back(argv[i]);
	    }
	    if (s->tolerate_failures && !successful_outputs.empty())
		ret = BRLCAD_OK;
	}
	db_close(work_dbip);
	if (ret == BRLCAD_OK)
	    ret = facetize_commit_outputs(s, successful_outputs);
	if (ret == BRLCAD_OK)
	    s->cleanup_workspace = true;
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

    s->cleanup_workspace = true;

booleval_cleanup:
    bu_free(dpa, "facetize input directory array");

    return ret;
}

extern "C" int
ged_facetize_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret = BRLCAD_OK;
    int need_help = 0;
    bool use_perturbation = false;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    FacetizeSession session(gedp);
    struct _ged_facetize_state *s = session.state();
    method_options_t *method_options = session.method_options();
    FacetizePlan plan;
    struct facetize_parse_args args;
    args.method_options = method_options;
    args.perturb_sa_tol = s->perturb_sa_tol;
    args.perturb_vol_tol = s->perturb_vol_tol;

    /* skip command name argv[0] */
    argc-=(argc>0); argv+=(argc>0);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Parse the canonical native option schema. */
    struct bu_vls omsg = BU_VLS_INIT_ZERO;
    int operand_start = bu_arg_schema_parse(facetize_schema(), &args, &omsg, argc, argv);
    if (operand_start < 0) {
	bu_vls_printf(gedp->ged_result_str, "option parsing failed: %s\n", bu_vls_cstr(&omsg));
	ret = BRLCAD_ERROR;
	bu_vls_free(&omsg);
	goto ged_facetize_done;
    }
    bu_vls_free(&omsg);
    argc -= operand_start;
    argv += operand_start;

    s->max_time = args.max_time;
    s->max_pnts = args.max_pnts;
    s->max_workers = args.max_workers;
    s->resume = args.resume;
    s->no_empty = args.no_empty;
    s->no_fixup = args.no_fixup;
    s->nonovlp_threshold = args.nonovlp_threshold;
    s->perturb_sa_tol = args.perturb_sa_tol;
    s->perturb_vol_tol = args.perturb_vol_tol;
    s->tolerate_failures = args.tolerate_failures;
    if (args.suffix)
	bu_vls_strcpy(s->suffix, args.suffix);
    if (args.prefix)
	bu_vls_strcpy(s->prefix, args.prefix);
    if (args.log_file)
	bu_vls_strcpy(s->log_file, args.log_file);

    // Sanity
    if (args.force_perturb && args.disable_perturb) {
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

    if (args.regions && args.nonovlp_brep) {
	bu_vls_printf(gedp->ged_result_str,
		"--regions and -B select different processing scopes and cannot be combined\n");
	ret = BRLCAD_ERROR;
	goto ged_facetize_done;
    }

    if (args.leaves && (args.regions || args.nonovlp_brep || args.make_nmg ||
	    args.nmg_booleval || args.force_perturb || db_version(gedp->dbip) < 5)) {
	bu_vls_printf(gedp->ged_result_str,
		"--leaves uses primitive BoT tessellation in a version 5 database.\n");
	ret = BRLCAD_ERROR;
	goto ged_facetize_done;
    }

    s->execution.output_format = args.make_nmg ?
	FacetizeOutputFormat::Nmg : FacetizeOutputFormat::Bot;
    s->execution.boolean_engine = (args.make_nmg || args.nmg_booleval) ?
	FacetizeBooleanEngine::Nmg : FacetizeBooleanEngine::Manifold;
    s->execution.scope = args.nonovlp_brep ? FacetizeScope::NonOverlappingBrep :
	(args.regions ? FacetizeScope::Regions :
	(args.leaves ? FacetizeScope::Leaves : FacetizeScope::Objects));
    s->execution.commit_mode = args.in_place ?
	FacetizeCommitMode::InPlace : FacetizeCommitMode::NewObject;
    use_perturbation = args.regions;
    if (args.disable_perturb)
	use_perturbation = false;
    if (args.force_perturb)
	use_perturbation = true;
    s->execution.perturb_mode = use_perturbation ?
	FacetizePerturbMode::Enabled : FacetizePerturbMode::Disabled;

    s->verbosity = (int)args.verbosity;

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
    if (args.quiet)
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
    if (args.print_help || need_help) {
	facetize_show_schema_help(gedp, args.print_help > 1);
	ret = args.print_help ? BRLCAD_OK : BRLCAD_ERROR;
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

static const char * const facetize_perturb_options[] = {
    "perturb", "no-perturb", NULL
};
static const char * const facetize_method_names[] = {
    "NMG", "MDC", "SPSR", NULL
};
static const struct bu_arg_shape facetize_methods_shape = {
    BU_ARG_SHAPE_COMMA_LIST, 1, 1, "comma-separated tessellation methods", NULL, NULL
};

static int
facetize_help_from_str(struct bu_vls *msg, const char *arg, void *storage)
{
    int mode = 1;

    if (arg && !BU_STR_EQUAL(arg, "all")) {
	if (msg)
	    bu_vls_strcat(msg, "--help accepts only the optional value 'all'\n");
	return -1;
    }
    if (arg)
	mode = 2;
    if (storage)
	*((int *)storage) = mode;
    return 0;
}

static const struct bu_arg_constraint facetize_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT, BU_ARG_CONDITION_ALWAYS, facetize_perturb_options, 0, 1,
	"--perturb and --no-perturb cannot be used together"),
    BU_ARG_CONSTRAINT_NULL
};

static const struct bu_arg_option facetize_options[] = {
    BU_ARG_ATTACHED_OPTIONAL("h", "help", "help", facetize_parse_args,
	print_help, facetize_help_from_str, "all",
	"Print help and exit; use --help=all for method and advanced options"),
    BU_ARG_COUNTING_FLAG("v", "verbose", facetize_parse_args, verbosity, BU_ARG_VALUE_LONG, "Verbose output; repeat to increase detail"),
    BU_ARG_FLAG("q", "quiet", facetize_parse_args, quiet, "Suppress all output"),
    BU_ARG_FLAG("n", "nmg-output", facetize_parse_args, make_nmg, "Create N-Manifold Geometry output"),
    BU_ARG_FLAG("r", "regions", facetize_parse_args, regions, "Facetize region trees"),
    BU_ARG_FLAG(NULL, "leaves", facetize_parse_args, leaves,
	"Facetize primitive leaves while preserving parent combinations"),
    BU_ARG_INTEGER_RANGE("j", "jobs", facetize_parse_args, max_workers, 1, 0, 1, MAX_PSW,
	"count", "Maximum facetize worker processes; zero selects an automatic limit"),
    BU_ARG_VALUE("s", "suffix", facetize_parse_args, suffix, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "suffix", "Output suffix"),
    BU_ARG_VALUE("p", "prefix", facetize_parse_args, prefix, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "prefix", "Output prefix"),
    BU_ARG_FLAG(NULL, "in-place", facetize_parse_args, in_place, "Replace input objects"),
    BU_ARG_VALUE(NULL, "max-time", facetize_parse_args, max_time, BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "seconds", "Maximum seconds per object"),
    BU_ARG_VALUE(NULL, "max-pnts", facetize_parse_args, max_pnts, BU_ARG_VALUE_INTEGER, BU_ARG_REQUIRED, "count", "Maximum sample points"),
    BU_ARG_FLAG(NULL, "resume", facetize_parse_args, resume, "Resume conversion"),
    {NULL, "methods", "methods", "method[,method...]", "Comma-separated tessellation methods",
	BU_ARG_VALUE_CUSTOM, offsetof(facetize_parse_args, method_options), tess_active_methods_from_str,
	NULL, NULL, NULL, 0, 0, facetize_method_names, BU_ARG_REQUIRED,
	&facetize_methods_shape, NULL, NULL, BU_ARG_VALUE_RANGE_NONE, BU_ARG_SHAPE_SCALAR},
    BU_ARG_CUSTOM(NULL, "method-opts", facetize_parse_args, method_options, tess_method_opts_from_str,
	"\"METHOD option=value ...\"", "Method-specific options"),
    BU_ARG_FLAG(NULL, "no-empty", facetize_parse_args, no_empty, "Suppress empty outputs"),
    BU_ARG_VALUE(NULL, "log-file", facetize_parse_args, log_file, BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "file", "Log file"),
    BU_ARG_FLAG(NULL, "nmg-booleval", facetize_parse_args, nmg_booleval, "Use NMG Boolean evaluation"),
    BU_ARG_FLAG(NULL, "disable-fixup", facetize_parse_args, no_fixup, "Disable mesh fixups"),
    BU_ARG_FLAG(NULL, "perturb", facetize_parse_args, force_perturb, "Enable perturbation"),
    BU_ARG_FLAG(NULL, "no-perturb", facetize_parse_args, disable_perturb, "Disable perturbation"),
    BU_ARG_FLAG("B", NULL, facetize_parse_args, nonovlp_brep, "Use experimental non-overlapping BREP mode"),
    BU_ARG_VALUE("t", "threshold", facetize_parse_args, nonovlp_threshold, BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "length", "Non-overlap threshold"),
    BU_ARG_VALUE(NULL, "perturb-sa-tol", facetize_parse_args, perturb_sa_tol, BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "percent", "Perturbation surface-area tolerance"),
    BU_ARG_VALUE(NULL, "perturb-vol-tol", facetize_parse_args, perturb_vol_tol, BU_ARG_VALUE_NUMBER, BU_ARG_REQUIRED, "percent", "Perturbation volume tolerance"),
    BU_ARG_FLAG(NULL, "tolerate-failures", facetize_parse_args, tolerate_failures, "Generate partial output after failures"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand facetize_operands[] = {
    BU_ARG_OPERAND("object", BU_ARG_VALUE_RAW, 0, BU_ARG_COUNT_UNLIMITED,
	"Source database objects; in single-output mode the final name is the output object",
	NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_help_group facetize_help_groups[] = {
    BU_ARG_HELP_GROUP("General", NULL, "help", BU_ARG_VISIBILITY_DEFAULT),
    BU_ARG_HELP_GROUP("Output organization", NULL, "nmg-output", BU_ARG_VISIBILITY_DEFAULT),
    BU_ARG_HELP_GROUP("Execution and naming", NULL, "jobs", BU_ARG_VISIBILITY_DEFAULT),
    BU_ARG_HELP_GROUP("Tessellation methods", NULL, "methods", BU_ARG_VISIBILITY_DEFAULT),
    BU_ARG_HELP_GROUP("Validation and recovery", NULL, "no-empty", BU_ARG_VISIBILITY_ADVANCED),
    BU_ARG_HELP_GROUP("Experimental BREP processing", NULL, "B", BU_ARG_VISIBILITY_ADVANCED),
    BU_ARG_HELP_GROUP_NULL
};
static const struct bu_arg_option_visibility facetize_option_visibility[] = {
    BU_ARG_OPTION_VISIBILITY("method-opts", BU_ARG_VISIBILITY_ADVANCED),
    BU_ARG_OPTION_VISIBILITY_NULL
};
static const struct bu_arg_presentation facetize_presentation =
    BU_ARG_PRESENTATION(facetize_help_groups, facetize_option_visibility,
	tess_method_parameter_groups());

static const struct bu_arg_schema facetize_cmd_schema = BU_ARG_SCHEMA_PRESENTED(
    "facetize", "Convert geometry to BOT or NMG form", facetize_options, facetize_operands,
    BU_ARG_PARSE_INTERSPERSED, BU_ARG_SCHEMA_CONSTRAINTS(NULL, facetize_constraints),
    &facetize_presentation);

static const struct bu_arg_schema *
facetize_schema(void)
{
    return &facetize_cmd_schema;
}

#define GED_FACETIZE_COMMANDS(X, XID) \
    X(facetize, ged_facetize_core, GED_CMD_DEFAULT, &facetize_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_FACETIZE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_facetize", 1, GED_FACETIZE_COMMANDS)

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
