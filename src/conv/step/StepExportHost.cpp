/*                       S T E P E X P O R T H O S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include "StepPluginHost.h"
#include "step_plugin.h"

#include <string>
#include <vector>

#include "bu/app.h"
#include "bu/arg.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/units.h"
#include "bu/vls.h"
#include "vmath.h"

#include <cmath>

#ifndef STEP_EXPORT_DEFAULT_SCHEMA
#  define STEP_EXPORT_DEFAULT_SCHEMA "ap203"
#endif

namespace {

struct step_export_args {
    int help = 0;
    const char *schema_name = NULL;
    int list_schemas = 0;
    int native_csg = 0;
    const char *length_unit = NULL;
    const char *plane_angle_unit = NULL;
    fastf_t uncertainty = 0.0;
    int strict = 0;
    const char *report = NULL;
    const char *output = NULL;
};

static const char * const degree_aliases[] = {"deg", "degrees", NULL};
static const char * const radian_aliases[] = {"rad", "radians", NULL};
static const struct bu_arg_value_keyword angle_unit_values[] = {
    {"degree", degree_aliases, "Write plane angles in degrees"},
    {"radian", radian_aliases, "Write plane angles in radians"},
    {NULL, NULL, NULL}
};

static const struct bu_arg_option step_export_options[] = {
    BU_ARG_FLAG("h", "help", struct step_export_args, help, "Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_VALUE(NULL, "schema", struct step_export_args, schema_name, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "AP", "ap203, ap203e2, ap214, ap242 (e4), ap242e1 through ap242e4, or auto"),
    BU_ARG_FLAG(NULL, "list-schemas", struct step_export_args, list_schemas, "List installed schema plugins"),
    BU_ARG_FLAG(NULL, "native-csg", struct step_export_args, native_csg, "Preserve supported AP203e2/AP214/AP242 boolean CSG trees"),
    BU_ARG_VALUE(NULL, "output-units", struct step_export_args, length_unit, BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "UNIT", "Write geometry in this recognized BRL-CAD length unit (default mm)"),
    BU_ARG_KEYWORD_VALUES(NULL, "angle-units", struct step_export_args, plane_angle_unit, "UNIT", "Write plane angles as degree or radian (default degree)", angle_unit_values),
    BU_ARG_NUMBER_RANGE(NULL, "uncertainty", struct step_export_args, uncertainty, 1, 0.0, 0, 0.0, "VALUE", "Positive STEP uncertainty in the selected output length unit"),
    BU_ARG_FLAG(NULL, "strict", struct step_export_args, strict, "Fail without publishing partial STEP output"),
    BU_ARG_VALUE(NULL, "report", struct step_export_args, report, BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "FILE", "Write a JSON export coverage report"),
    BU_ARG_VALUE("o", "output", struct step_export_args, output, BU_ARG_VALUE_FILE, BU_ARG_REQUIRED, "FILE", "Output STEP file"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand step_export_operands[] = {
    BU_ARG_OPERAND("input.g", BU_ARG_VALUE_FILE, 1, 1, "Input BRL-CAD database", NULL),
    BU_ARG_OPERAND("object", BU_ARG_VALUE_DB_OBJECT, 0, BU_ARG_COUNT_UNLIMITED, "Database object to export", NULL),
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_operand step_export_list_operands[] = {BU_ARG_OPERAND_NULL};
static const char * const step_export_list_options[] = {"list-schemas", NULL};
static const struct bu_arg_schema_case step_export_cases[] = {
    BU_ARG_SCHEMA_CASE("list-schemas", "List available schema plugins", BU_ARG_CONDITION_ANY_OPTION_PRESENT, step_export_list_options, step_export_list_operands, NULL),
    BU_ARG_SCHEMA_CASE_DEFAULT("export", "Export geometry", step_export_operands, NULL),
    BU_ARG_SCHEMA_CASE_NULL
};

static const struct bu_arg_schema step_export_schema = BU_ARG_SCHEMA(
    "g-step", "Export BRL-CAD geometry as STEP", step_export_options, NULL,
    BU_ARG_PARSE_INTERSPERSED,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, step_export_cases));

void
plugin_log(int, const char *message)
{
    if (message) bu_log("%s\n", message);
}

void
usage(const char *invocation)
{
    char *help = bu_arg_schema_help(&step_export_schema, invocation);
    if (help)
	bu_log("%s", help);
    bu_free(help, "option description");
}

} // namespace

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    const struct bu_arg_grammar grammar = {&step_export_schema, NULL, NULL};
    struct bu_vls schema_output = BU_VLS_INIT_ZERO;
    const int schema_status = bu_arg_introspect(&schema_output, &grammar,
	argc - 1, argv + 1);
    if (schema_status != 0) {
	bu_flog(schema_status > 0 ? stdout : stderr, "%s", bu_vls_cstr(&schema_output));
	bu_vls_free(&schema_output);
	return schema_status > 0 ? 0 : 1;
    }
    bu_vls_free(&schema_output);

    const char *invocation = argv[0];
    ++argv;
    --argc;
    step_export_args args;
    const bool uncertainty_set = bu_arg_schema_option_present(&step_export_schema,
	argc, argv, "uncertainty");
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    const int operand_index = bu_arg_schema_parse_complete(&step_export_schema,
	&args, &messages, argc, argv);
    if (operand_index < 0) {
	if (bu_vls_strlen(&messages)) bu_log("%s\n", bu_vls_cstr(&messages));
	bu_vls_free(&messages);
	usage(invocation);
	return 1;
    }
    bu_vls_free(&messages);
    argc -= operand_index;
    argv += operand_index;
    if (args.list_schemas) {
	for (const std::string &schema : brlcad::step::available_schema_plugins())
	    bu_log("%s\n", schema.c_str());
	return 0;
    }
    if (args.help) {
	usage(invocation);
	return 0;
    }
    if (!args.output || (uncertainty_set && !(args.uncertainty > 0.0))) {
	if (uncertainty_set)
	    bu_log("ERROR: --uncertainty must be positive\n");
	usage(invocation);
	return 1;
    }

    const int native_csg = args.native_csg;
    const int strict = args.strict;
    const char *schema_name = args.schema_name;
    const char *length_unit = args.length_unit;
    const char *plane_angle_unit = args.plane_angle_unit;
    const char *output = args.output;
    const char *report = args.report;
    std::string schema = schema_name ? schema_name : STEP_EXPORT_DEFAULT_SCHEMA;
    if (schema == "auto") schema = STEP_EXPORT_DEFAULT_SCHEMA;
    if (schema == "ap242") schema = "ap242e4";
    if (native_csg && schema == "ap203") {
	bu_log("ERROR: --native-csg requires --schema ap203e2, ap214, or AP242\n");
	return 1;
    }
    const double length_unit_mm = length_unit ?
	bu_units_conversion(length_unit) : 1.0;
    if (!(length_unit_mm > 0.0) || !std::isfinite(length_unit_mm)) {
	bu_log("ERROR: --output-units requires a recognized length unit\n");
	return 1;
    }
    std::string angle_unit = plane_angle_unit ? plane_angle_unit : "degree";
    if (angle_unit == "deg" || angle_unit == "degrees") angle_unit = "degree";
    if (angle_unit == "rad" || angle_unit == "radians") angle_unit = "radian";
    if (angle_unit != "degree" && angle_unit != "radian") {
	bu_log("ERROR: --angle-units must be degree or radian\n");
	return 1;
    }
#ifdef STEP_EXPORT_FORCED_SCHEMA
    if (schema != STEP_EXPORT_FORCED_SCHEMA) {
	bu_log("ERROR: %s is fixed to --schema %s\n", bu_getprogname(), STEP_EXPORT_FORCED_SCHEMA);
	return 1;
    }
#endif
    if (!brlcad::step::schema_plugin_available(schema)) {
	bu_log("ERROR: unknown or unavailable STEP schema '%s'\n", schema.c_str());
	return 1;
    }
    std::string load_error;
    bu_plugin_set_logger(plugin_log);
    if (!brlcad::step::load_schema_plugin(schema, load_error)) {
	bu_log("ERROR: %s\n", load_error.c_str());
	return 1;
    }

    std::vector<const char *> objects;
    for (int i = 1; i < argc; ++i) objects.push_back(argv[i]);
    struct step_export_options export_options = {};
    export_options.api_version = STEP_PLUGIN_EXPORT_OPTIONS_VERSION;
    export_options.struct_size = sizeof(export_options);
    export_options.native_csg = native_csg ? 1u : 0u;
    export_options.strict = strict ? 1u : 0u;
    export_options.length_unit_mm = length_unit_mm;
    export_options.uncertainty = uncertainty_set ? args.uncertainty : 0.0;
    export_options.length_unit = length_unit ? length_unit : "mm";
    export_options.plane_angle_unit = angle_unit.c_str();
    export_options.report_path = report;
    export_options.object_count = objects.size();
    export_options.object_names = objects.empty() ? NULL : objects.data();
    struct step_plugin_request request = {};
    request.api_version = STEP_PLUGIN_REQUEST_VERSION;
    request.struct_size = sizeof(request);
    request.operation = STEP_OPERATION_EXPORT;
    request.input_path = argv[0];
    request.output_path = output;
    request.schema = schema.c_str();
    request.export_options = &export_options;

    char diagnostic[1024] = {0};
    struct step_plugin_result result = {
	STEP_PLUGIN_RESULT_VERSION, sizeof(struct step_plugin_result), 0, 0,
	0, 0, diagnostic, sizeof(diagnostic), 0
    };
    const std::string command = brlcad::step::schema_plugin_command(schema, false);
    const int status = step_plugin_host_call(command.c_str(), &request, &result);
    if ((status == STEP_PLUGIN_STATUS_PARTIAL || status == STEP_PLUGIN_STATUS_EMPTY) &&
	    result.diagnostic_length)
	bu_log("WARNING: %s\n", diagnostic);
    else if (status && result.diagnostic_length)
	bu_log("ERROR: %s\n", diagnostic);
    bu_plugin_shutdown();
    return status;
}
