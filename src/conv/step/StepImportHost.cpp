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
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/parallel.h"
#include "bu/vls.h"
#include "vmath.h"

namespace {

struct OutputFile {
    char *filename = NULL;
    int overwrite = 0;
};

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
parse_output_overwrite(struct bu_vls *error, size_t argc, const char **argv, void *data)
{
    BU_OPT_CHECK_ARGV0(error, argc, argv, "-O");
    OutputFile *output = static_cast<OutputFile *>(data);
    if (!output) return -1;
    output->overwrite = 1;
    return bu_opt_str(error, argc, argv, &output->filename);
}

int
parse_entity_ids(struct bu_vls *error, size_t argc, const char **argv, void *data)
{
    BU_OPT_CHECK_ARGV0(error, argc, argv, "entity ID list");
    std::set<int64_t> *identifiers = static_cast<std::set<int64_t> *>(data);
    std::string detail;
    if (!identifiers || !brlcad::step::parse_entity_id_list(argv[0], *identifiers, &detail)) {
	bu_vls_printf(error, "%s\n", detail.c_str());
	return -1;
    }
    return 1;
}

void
usage(const struct bu_opt_desc *options)
{
    char *help = bu_opt_help(options, "step-g",
	"input.step [output.g]\n       step-g [options] -o output.g input.step",
	"Convert STEP geometry to a BRL-CAD database");
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

    OutputFile output;
    int help = 0;
    int dry_run = 0;
    int verbose = 0;
    int exact = 0;
    int strict = 0;
    int reject_invalid = 0;
    int skip_open_shells = 0;
    int no_item_budget = 0;
    int jobs = default_jobs();
    fastf_t absolute_tolerance = 0.0;
    fastf_t budget_scale = 0.0;
    fastf_t item_budget = 0.0;
    fastf_t stall_timeout = 0.0;
    char *repair = NULL;
    char *report = NULL;
    char *summary = NULL;
    char *schema_override = NULL;
    std::set<int64_t> entity_set;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    struct bu_opt_desc options[] = {
	{"h", "help", "", NULL, &help, "print help and exit"},
	{"?", "", "", NULL, &help, ""},
	{"D", "dry-run", "", NULL, &dry_run, "validate without writing a database"},
	{"v", "verbose", "", NULL, &verbose, "report entity-level diagnostics"},
	{"", "exact", "", NULL, &exact, "strictly enforce the declared model tolerance"},
	{"", "strict", "", NULL, &strict, "reject partial output"},
	{"", "reject-invalid-objs", "", NULL, &reject_invalid, "reject invalid B-reps"},
	{"", "skip-open-shells", "", NULL, &skip_open_shells,
	    "exclude standalone open-shell surface geometry"},
	{"", "abs-tol", "MM", bu_opt_fastf_t, &absolute_tolerance, "absolute output tolerance"},
	{"", "budget-scale", "FACTOR", bu_opt_fastf_t, &budget_scale, "manual budget scale"},
	{"", "item-budget", "SECONDS", bu_opt_fastf_t, &item_budget, "per-item CPU-work budget"},
	{"", "no-item-budget", "", NULL, &no_item_budget, "disable per-item CPU budgets"},
	{"", "stall-timeout", "SECONDS", bu_opt_fastf_t, &stall_timeout, "no-progress timeout"},
	{"", "repair", "MODE", bu_opt_str, &repair, "none or safe (default: safe)"},
	{"", "report", "FILE", bu_opt_str, &report, "structured JSON report"},
	{"f", "force", "", NULL, &output.overwrite, "overwrite output"},
	{"O", "output-overwrite", "FILE", parse_output_overwrite, &output, "output file (overwrite)"},
	{"o", "output", "FILE", bu_opt_str, &output.filename, "output file"},
	{"S", "summary", "FILE", bu_opt_str, &summary, "legacy entity summary"},
	{"", "schema", "AP", bu_opt_str, &schema_override,
	    "schema edition override (ap242e1 through ap242e4, or auto)"},
	{"e", "entity", "ID[,ID...]", parse_entity_ids, &entity_set, "selected representation-item IDs"},
	{"j", "jobs", "N", bu_opt_int, &jobs, "geometry worker count"},
	BU_OPT_DESC_NULL
    };

    ++argv;
    --argc;
    argc = bu_opt_parse(&messages, argc, argv, options);
    if (bu_vls_strlen(&messages)) bu_log("%s\n", bu_vls_cstr(&messages));
    if (help) {
	usage(options);
	bu_vls_free(&messages);
	return 0;
    }
    if (bu_vls_strlen(&messages) || jobs < 1 || absolute_tolerance < 0.0 ||
	budget_scale < 0.0 || item_budget < 0.0 || stall_timeout < 0.0) {
	usage(options);
	bu_vls_free(&messages);
	return 2;
    }
    const bool option_output = output.filename && output.filename[0];
    if ((option_output && argc != 1) ||
	(!option_output && argc != 2 && !(dry_run && argc == 1))) {
	bu_log("ERROR: specify one input and one output (or only an input with -D)\n");
	usage(options);
	bu_vls_free(&messages);
	return 2;
    }
    if (!option_output && argc == 2) output.filename = const_cast<char *>(argv[1]);
    bu_vls_free(&messages);

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
    import_options.overwrite = output.overwrite;
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
    request.output_path = output.filename ? output.filename : "";
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
