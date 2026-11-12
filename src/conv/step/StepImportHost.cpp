/*                       S T E P I M P O R T H O S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * Schema-neutral public step-g host.  It reads only the Part 21 header,
 * selects one fixed plugin, and forwards a versioned request.
 */

#include "common.h"

#include "STEPHeaderSchema.h"
#include "STEPString.h"
#include "StepPluginHost.h"
#include "step_plugin.h"

#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/vls.h"
#include "vmath.h"

namespace {

struct TemporaryInput {
    std::string path;
    ~TemporaryInput() { if (!path.empty()) bu_file_delete(path.c_str()); }
};

int
default_jobs()
{
    return static_cast<int>(std::max<size_t>(1, std::min<size_t>(8, bu_avail_cpus())));
}

void
plugin_log(int, const char *message)
{
    if (message) bu_log("%s\n", message);
}

int
parse_entity_ids(struct bu_vls *error, const char *arg, void *data)
{
    std::set<int64_t> **identifier_ptr = static_cast<std::set<int64_t> **>(data);
    std::set<int64_t> validation_ids;
    std::set<int64_t> *identifiers = identifier_ptr ? *identifier_ptr : &validation_ids;
    std::string detail;
    if (!identifiers || !arg || !brlcad::step::parse_entity_id_list(arg, *identifiers, &detail)) {
	if (error) bu_vls_printf(error, "%s\n", detail.c_str());
	return -1;
    }
    return 0;
}

struct step_import_args {
    int help = 0, dry_run = 0, verbose = 0, exact = 0, strict = 0;
    int reject_invalid = 0, skip_open_shells = 0, no_item_budget = 0;
    int force = 0, jobs = 1;
    fastf_t absolute_tolerance = 0.0, budget_scale = 0.0;
    fastf_t item_budget = 0.0, stall_timeout = 0.0;
    const char *repair = NULL, *report = NULL, *output_overwrite = NULL;
    const char *output = NULL, *summary = NULL, *schema_override = NULL;
    std::set<int64_t> *entity_set = NULL;
};

static const struct bu_arg_value_keyword repair_values[] = {
    {"none", NULL, "Disable repair"}, {"safe", NULL, "Use safe repair"},
    {NULL, NULL, NULL}
};

static const struct bu_arg_option step_import_options[] = {
    BU_ARG_FLAG("h", "help", struct step_import_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_FLAG("D", "dry-run", struct step_import_args, dry_run, "Validate without writing a database"),
    BU_ARG_FLAG("v", "verbose", struct step_import_args, verbose, "Report entity-level diagnostics"),
    BU_ARG_FLAG(NULL, "exact", struct step_import_args, exact, "Strictly enforce the declared model tolerance"),
    BU_ARG_FLAG(NULL, "strict", struct step_import_args, strict, "Reject partial output"),
    BU_ARG_FLAG(NULL, "reject-invalid-objs", struct step_import_args, reject_invalid, "Reject invalid B-reps"),
    BU_ARG_FLAG(NULL, "skip-open-shells", struct step_import_args, skip_open_shells, "Exclude standalone open-shell surface geometry"),
    BU_ARG_NUMBER_RANGE(NULL, "abs-tol", struct step_import_args, absolute_tolerance, 1, 0.0, 0, 0.0, "MM", "Absolute output tolerance"),
    BU_ARG_NUMBER_RANGE(NULL, "budget-scale", struct step_import_args, budget_scale, 1, 0.0, 0, 0.0, "FACTOR", "Manual budget scale"),
    BU_ARG_NUMBER_RANGE(NULL, "item-budget", struct step_import_args, item_budget, 1, 0.0, 0, 0.0, "SECONDS", "Per-item CPU-work budget"),
    BU_ARG_FLAG(NULL, "no-item-budget", struct step_import_args, no_item_budget, "Disable per-item CPU budgets"),
    BU_ARG_NUMBER_RANGE(NULL, "stall-timeout", struct step_import_args, stall_timeout, 1, 0.0, 0, 0.0, "SECONDS", "No-progress timeout"),
    BU_ARG_KEYWORD_VALUES(NULL, "repair", struct step_import_args, repair, "MODE", "Repair mode (default: safe)", repair_values),
    BU_ARG_VALUE(NULL, "report", struct step_import_args, report, BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "FILE", "Structured JSON report"),
    BU_ARG_FLAG("f", "force", struct step_import_args, force, "Overwrite output"),
    BU_ARG_VALUE("O", "output-overwrite", struct step_import_args, output_overwrite, BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "FILE", "Output file and overwrite it"),
    BU_ARG_VALUE("o", "output", struct step_import_args, output, BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "FILE", "Output file"),
    BU_ARG_VALUE("S", "summary", struct step_import_args, summary, BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "FILE", "Legacy entity summary"),
    BU_ARG_VALUE(NULL, "schema", struct step_import_args, schema_override, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "AP", "Schema edition override (ap242e1 through ap242e4, or auto)"),
    BU_ARG_CUSTOM("e", "entity", struct step_import_args, entity_set, parse_entity_ids, "ID[,ID...]", "Selected representation-item IDs"),
    BU_ARG_INTEGER_RANGE("j", "jobs", struct step_import_args, jobs, 1, 1, 0, 0, "N", "Geometry worker count"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand step_import_operands[] = {
    BU_ARG_OPERAND("input.step", BU_ARG_VALUE_FILE, 1, 1, "Input STEP file, or - for standard input", NULL),
    BU_ARG_OPERAND("output.g", BU_ARG_VALUE_FILE, 0, 1, "Output BRL-CAD database", NULL),
    BU_ARG_OPERAND_NULL
};
static const char * const output_options[] = {"output", "output-overwrite", NULL};
static const struct bu_arg_constraint step_import_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT, BU_ARG_CONDITION_ALWAYS, output_options, 0, 1, "select at most one output option"),
    BU_ARG_CONSTRAINT_NULL
};
static const struct bu_arg_schema step_import_schema = BU_ARG_SCHEMA(
    "step-g", "Convert STEP geometry to a BRL-CAD database", step_import_options,
    step_import_operands, BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, step_import_constraints, NULL, NULL, NULL));

void
usage(const char *invocation)
{
    char *help = bu_arg_schema_help(&step_import_schema, invocation);
    if (help)
	bu_log("%s", help);
    bu_free(help, "option description");
}

bool
spool_standard_input(std::string &path)
{
    char name[MAXPATHLEN] = {0};
    FILE *output = bu_temp_file(name, sizeof(name));
    if (!output) return false;
    char buffer[16384];
    bool ok = true;
    while (!std::feof(stdin)) {
	const size_t count = std::fread(buffer, 1, sizeof(buffer), stdin);
	if (count && std::fwrite(buffer, 1, count, output) != count) {
	    ok = false;
	    break;
	}
	if (std::ferror(stdin)) {
	    ok = false;
	    break;
	}
    }
    if (std::fclose(output) != 0) ok = false;
    path = name;
    if (!ok) bu_file_delete(path.c_str());
    return ok;
}

} // namespace

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);

    const char *invocation = argv[0];
    ++argv;
    --argc;
    std::set<int64_t> entity_set;
    step_import_args args;
    args.jobs = default_jobs();
    args.entity_set = &entity_set;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    const int operand_index = bu_arg_schema_parse_complete(&step_import_schema,
	&args, &messages, argc, argv);
    if (operand_index < 0) {
	if (bu_vls_strlen(&messages)) bu_log("%s\n", bu_vls_cstr(&messages));
	bu_vls_free(&messages);
	usage(invocation);
	return 2;
    }
    bu_vls_free(&messages);
    argc -= operand_index;
    argv += operand_index;
    if (args.help) {
	usage(invocation);
	return 0;
    }

    const char *output_filename = args.output_overwrite ?
	args.output_overwrite : args.output;
    const bool option_output = output_filename && output_filename[0];
    if ((option_output && argc != 1) ||
	    (!option_output && argc != 2 && !(args.dry_run && argc == 1))) {
	bu_log("ERROR: specify one input and one output (or only an input with -D)\n");
	usage(invocation);
	return 2;
    }
    if (!option_output && argc == 2) output_filename = argv[1];
    const int overwrite = args.force || args.output_overwrite;
    const int dry_run = args.dry_run;
    const int verbose = args.verbose;
    const int exact = args.exact;
    const int strict = args.strict;
    const int reject_invalid = args.reject_invalid;
    const int skip_open_shells = args.skip_open_shells;
    const int no_item_budget = args.no_item_budget;
    const int jobs = args.jobs;
    const fastf_t absolute_tolerance = args.absolute_tolerance;
    const fastf_t budget_scale = args.budget_scale;
    const fastf_t item_budget = args.item_budget;
    const fastf_t stall_timeout = args.stall_timeout;
    const char *repair = args.repair;
    const char *report = args.report;
    const char *summary = args.summary;
    const char *schema_override = args.schema_override;
    const std::string requested_input = argv[0];
    std::string input = requested_input;
    TemporaryInput temporary;
    if (requested_input == "-") {
	if (!spool_standard_input(input)) {
	    bu_log("ERROR: unable to spool standard input\n");
	    return 2;
	}
	temporary.path = input;
    }

    const brlcad::step::HeaderSchemaInfo schema =
	brlcad::step::STEPHeaderSchema::inspect_file(input);
    if (!schema.recognized) {
	bu_log("ERROR: %s\n", schema.error.c_str());
	return 2;
    }
    const std::string schema_key = brlcad::step::STEPHeaderSchema::key(schema.schema);
    if (schema.schema == brlcad::step::HeaderSchema::IFC) {
	bu_log("ERROR: recognized STEP schema '%s' requires the separate ifc-g converter\n",
	       schema.identifiers.front().c_str());
	return 2;
    }
    if (schema.legacy_identifier)
	bu_log("WARNING: accepting legacy/interim FILE_SCHEMA identifier or profile as %s\n",
	       schema_key.c_str());
    if (!schema.unrecognized_identifiers.empty()) {
	bu_log("WARNING: FILE_SCHEMA includes %zu unrecognized companion schema%s; "
	       "selecting the supported %s registry\n",
	       schema.unrecognized_identifiers.size(),
	       schema.unrecognized_identifiers.size() == 1 ? "" : "s",
	       schema_key.c_str());
    }
#ifdef STEP_IMPORT_FORCED_SCHEMA
    if (schema_key != STEP_IMPORT_FORCED_SCHEMA) {
	bu_log("ERROR: %s accepts only %s input; FILE_SCHEMA selected %s\n",
	       bu_getprogname(), STEP_IMPORT_FORCED_SCHEMA, schema_key.c_str());
	return 2;
    }
#endif

    std::vector<std::string> schema_candidates;
    const std::string requested_schema = schema_override ? schema_override : "auto";
    if (schema.schema == brlcad::step::HeaderSchema::AP242) {
	if (requested_schema == "auto" || requested_schema == "ap242") {
	    /* The AP242 long-form FILE_SCHEMA name does not encode its edition.
	     * Prefer the current binding, then retry earlier physical layouts when
	     * parsing produces no usable model. */
	    schema_candidates.push_back("ap242e4");
	    schema_candidates.push_back("ap242e3");
	    schema_candidates.push_back("ap242e2");
	    schema_candidates.push_back("ap242e1");
	} else if (requested_schema == "ap242e1" ||
		requested_schema == "ap242e2" ||
		requested_schema == "ap242e3" ||
		requested_schema == "ap242e4") {
	    schema_candidates.push_back(requested_schema);
	} else {
	    bu_log("ERROR: AP242 input requires --schema auto or ap242e1 through ap242e4\n");
	    return 2;
	}
    } else {
	if (requested_schema != "auto" && requested_schema != schema_key) {
	    bu_log("ERROR: FILE_SCHEMA selected %s but --schema requested %s\n",
		schema_key.c_str(), requested_schema.c_str());
	    return 2;
	}
	schema_candidates.push_back(schema_key);
    }

    std::vector<int64_t> entity_ids(entity_set.begin(), entity_set.end());
    struct step_import_options import_options = {};
    import_options.api_version = STEP_PLUGIN_IMPORT_OPTIONS_VERSION;
    import_options.struct_size = sizeof(import_options);
    import_options.dry_run = dry_run;
    import_options.verbose = verbose;
    import_options.exact = exact;
    import_options.strict = strict;
    import_options.reject_invalid_objects = reject_invalid;
    import_options.overwrite = overwrite;
    import_options.requested_jobs = static_cast<uint32_t>(jobs);
    import_options.disable_item_budgets = no_item_budget;
    import_options.absolute_tolerance_mm = absolute_tolerance;
    import_options.budget_scale = budget_scale;
    import_options.item_budget_seconds = item_budget;
    import_options.stall_timeout_seconds = stall_timeout;
    /* Make the public ABI contract explicit instead of depending on each
     * schema CLI to interpret a null repair mode identically. */
    import_options.repair_mode = repair ? repair : "safe";
    import_options.report_path = report;
    import_options.summary_path = summary;
    import_options.selected_entity_count = entity_ids.size();
    import_options.selected_entity_ids = entity_ids.empty() ? NULL : entity_ids.data();
    import_options.skip_open_shells = skip_open_shells;

    struct step_plugin_request request = {};
    request.api_version = STEP_PLUGIN_REQUEST_VERSION;
    request.struct_size = sizeof(request);
    request.operation = STEP_OPERATION_IMPORT;
    request.input_path = input.c_str();
    request.output_path = output_filename ? output_filename : "";
    request.import_options = &import_options;

    char diagnostic[1024] = {0};
    struct step_plugin_result result = {
	STEP_PLUGIN_RESULT_VERSION, sizeof(struct step_plugin_result), 0, 0,
	0, 0, diagnostic, sizeof(diagnostic), 0
    };
    int status = STEP_PLUGIN_STATUS_INTERNAL;
    for (size_t candidate_index = 0;
	    candidate_index < schema_candidates.size(); ++candidate_index) {
	const std::string &candidate = schema_candidates[candidate_index];
	std::string load_error;
	bu_plugin_set_logger(plugin_log);
	if (!brlcad::step::load_schema_plugin(candidate, load_error)) {
	    bu_log("ERROR: %s\n", load_error.c_str());
	    return 2;
	}
	request.schema = candidate.c_str();
	const std::string command = brlcad::step::schema_plugin_command(candidate, true);
	status = step_plugin_host_call(command.c_str(), &request, &result);
	bu_plugin_shutdown();

	const bool may_retry = candidate_index + 1 < schema_candidates.size() &&
	    (status == STEP_PLUGIN_STATUS_INPUT ||
	     status == STEP_PLUGIN_STATUS_EMPTY);
	if (may_retry) {
	    bu_log("WARNING: AP242 %s binding could not produce usable geometry; "
		"retrying an earlier edition Part 21 layout\n", candidate.c_str());
	    continue;
	}
	break;
    }
    if ((status == STEP_PLUGIN_STATUS_PARTIAL || status == STEP_PLUGIN_STATUS_EMPTY) &&
	    result.diagnostic_length)
	bu_log("WARNING: %s\n", diagnostic);
    else if (status && result.diagnostic_length)
	bu_log("ERROR: %s\n", diagnostic);
    return status;
}
