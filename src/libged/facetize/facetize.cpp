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
#include "bu/cmdschema.h"
#include "bu/file.h"
#include "bu/path.h"
#include "wdb.h"

#include "../ged_private.h"

#define TESS_OPTS_IMPLEMENTATION
#include "./tess_opts.h"
#include "./ged_facetize.h"

static const char FACETIZE_NMG_OUTPUT_FILE_SUFFIX[] = "_nmg_outputs.g";

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

static const struct bu_cmd_schema *facetize_schema(void);


static void
facetize_show_schema_help(struct ged *gedp)
{
    char *help = bu_cmd_schema_help(facetize_schema(), "facetize");

    if (help) {
	bu_vls_strcat(gedp->ged_result_str, help);
	bu_free(help, "facetize native schema help");
    }
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

static int
facetize_commit_nmg_outputs(struct _ged_facetize_state *s,
	const std::vector<std::string> &output_names)
{
    if (!s || output_names.empty())
	return BRLCAD_ERROR;

    std::string keep_file = std::string(bu_vls_cstr(s->wfile)) +
	FACETIZE_NMG_OUTPUT_FILE_SUFFIX;
    (void)bu_file_delete(keep_file.c_str());
    struct ged *working_gedp = ged_open("db", bu_vls_cstr(s->wfile), 1);
    if (!working_gedp) {
	facetize_failure(s, "unable to open staged NMG Boolean database '%s'",
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
	facetize_failure(s, "unable to stage NMG Boolean outputs for import");
	(void)bu_file_delete(keep_file.c_str());
	return BRLCAD_ERROR;
    }

    const char *concat_argv[] = {"dbconcat", "-O", keep_file.c_str(), NULL};
    int concat_result = ged_exec_dbconcat(s->gedp, 3, concat_argv);
    (void)bu_file_delete(keep_file.c_str());
    if (concat_result != BRLCAD_OK) {
	facetize_failure(s, "unable to import staged NMG Boolean outputs");
	return BRLCAD_ERROR;
    }
    bu_vls_trunc(s->gedp->ged_result_str, 0);
    db_update_nref(s->dbip);
    return BRLCAD_OK;
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
	    ret = facetize_commit_nmg_outputs(s, successful_outputs);
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
    int operand_start = bu_cmd_schema_parse(facetize_schema(), &args, &omsg, argc, argv);
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

    s->execution.output_format = args.make_nmg ?
	FacetizeOutputFormat::Nmg : FacetizeOutputFormat::Bot;
    s->execution.boolean_engine = (args.make_nmg || args.nmg_booleval) ?
	FacetizeBooleanEngine::Nmg : FacetizeBooleanEngine::Manifold;
    s->execution.scope = args.nonovlp_brep ? FacetizeScope::NonOverlappingBrep :
	(args.regions ? FacetizeScope::Regions : FacetizeScope::Objects);
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
	facetize_show_schema_help(gedp);
	_facetize_methods_help(gedp);
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
static const struct bu_cmd_arg_shape facetize_methods_shape = {
    BU_CMD_ARG_SHAPE_COMMA_LIST, 1, 1, "comma-separated tessellation methods", NULL, NULL
};

static const struct bu_cmd_constraint facetize_constraints[] = {
    BU_CMD_CONSTRAINT_OPTIONS(facetize_perturb_options, 0, 1,
	"--perturb and --no-perturb cannot be used together"),
    BU_CMD_CONSTRAINT_NULL
};

static const struct bu_cmd_option facetize_options[] = {
    BU_CMD_FLAG("h", "help", facetize_parse_args, print_help, "Print help and exit"),
    BU_CMD_COUNTING_LONG_FLAG("v", "verbose", facetize_parse_args, verbosity, "Verbose output; repeat to increase detail"),
    BU_CMD_FLAG("q", "quiet", facetize_parse_args, quiet, "Suppress all output"),
    BU_CMD_FLAG("n", "nmg-output", facetize_parse_args, make_nmg, "Create N-Manifold Geometry output"),
    BU_CMD_FLAG("r", "regions", facetize_parse_args, regions, "Facetize region trees"),
    BU_CMD_INTEGER_RANGE("j", "jobs", facetize_parse_args, max_workers, 0, MAX_PSW,
	"count", "Maximum facetize worker processes; zero selects an automatic limit"),
    BU_CMD_STRING("s", "suffix", facetize_parse_args, suffix, "suffix", "Output suffix"),
    BU_CMD_STRING("p", "prefix", facetize_parse_args, prefix, "prefix", "Output prefix"),
    BU_CMD_FLAG(NULL, "in-place", facetize_parse_args, in_place, "Replace input objects"),
    BU_CMD_INTEGER(NULL, "max-time", facetize_parse_args, max_time, "seconds", "Maximum seconds per object"),
    BU_CMD_INTEGER(NULL, "max-pnts", facetize_parse_args, max_pnts, "count", "Maximum sample points"),
    BU_CMD_FLAG(NULL, "resume", facetize_parse_args, resume, "Resume conversion"),
    {NULL, "methods", "methods", "method[,method...]", "Comma-separated tessellation methods",
	BU_CMD_VALUE_CUSTOM, offsetof(facetize_parse_args, method_options), tess_active_methods_from_str,
	NULL, NULL, NULL, 0, 0, facetize_method_names, BU_CMD_ARG_REQUIRED,
	&facetize_methods_shape, NULL, NULL, BU_CMD_VALUE_RANGE_NONE},
    BU_CMD_CUSTOM(NULL, "method-opts", facetize_parse_args, method_options, tess_method_opts_from_str,
	"\"METHOD option=value ...\"", "Method-specific options"),
    BU_CMD_FLAG(NULL, "no-empty", facetize_parse_args, no_empty, "Suppress empty outputs"),
    BU_CMD_FILE(NULL, "log-file", facetize_parse_args, log_file, "file", "Log file"),
    BU_CMD_FLAG(NULL, "nmg-booleval", facetize_parse_args, nmg_booleval, "Use NMG Boolean evaluation"),
    BU_CMD_FLAG(NULL, "disable-fixup", facetize_parse_args, no_fixup, "Disable mesh fixups"),
    BU_CMD_FLAG(NULL, "perturb", facetize_parse_args, force_perturb, "Enable perturbation"),
    BU_CMD_FLAG(NULL, "no-perturb", facetize_parse_args, disable_perturb, "Disable perturbation"),
    BU_CMD_FLAG("B", NULL, facetize_parse_args, nonovlp_brep, "Use experimental non-overlapping BREP mode"),
    BU_CMD_NUMBER("t", "threshold", facetize_parse_args, nonovlp_threshold, "length", "Non-overlap threshold"),
    BU_CMD_NUMBER(NULL, "perturb-sa-tol", facetize_parse_args, perturb_sa_tol, "percent", "Perturbation surface-area tolerance"),
    BU_CMD_NUMBER(NULL, "perturb-vol-tol", facetize_parse_args, perturb_vol_tol, "percent", "Perturbation volume tolerance"),
    BU_CMD_FLAG(NULL, "tolerate-failures", facetize_parse_args, tolerate_failures, "Generate partial output after failures"),
    BU_CMD_OPTION_NULL
};

static const struct bu_cmd_operand facetize_operands[] = {
    BU_CMD_OPERAND("object", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	"Source database objects; in single-output mode the final name is the output object",
	NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema facetize_cmd_schema = {
    "facetize", "Convert geometry to BOT or NMG form", facetize_options, facetize_operands,
    BU_CMD_PARSE_INTERSPERSED, BU_CMD_SCHEMA_CONSTRAINTS(NULL, facetize_constraints)
};

static const struct bu_cmd_schema *
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
