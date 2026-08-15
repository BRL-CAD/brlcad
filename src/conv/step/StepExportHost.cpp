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
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/opt.h"
#include "bu/units.h"
#include "bu/vls.h"
#include "vmath.h"

#include <cmath>

#ifndef STEP_EXPORT_DEFAULT_SCHEMA
#  define STEP_EXPORT_DEFAULT_SCHEMA "ap203"
#endif

namespace {

struct PositiveFastfOption {
    fastf_t value = 0.0;
    bool set = false;
};

int
positive_fastf_option(struct bu_vls *message, size_t argc, const char **argv,
    void *setting)
{
    if (!setting) return -1;
    fastf_t value = 0.0;
    const int consumed = bu_opt_fastf_t(message, argc, argv, &value);
    if (consumed < 1) return consumed;
    if (!(value > 0.0) || !std::isfinite(value)) {
	if (message)
	    bu_vls_printf(message, "ERROR: value must be positive and finite\n");
	return -1;
    }
    PositiveFastfOption *option = static_cast<PositiveFastfOption *>(setting);
    option->value = value;
    option->set = true;
    return consumed;
}

void
plugin_log(int, const char *message)
{
    if (message) bu_log("%s\n", message);
}

void
usage(const struct bu_opt_desc *options)
{
    char *help = bu_opt_help(options, "g-step", "input.g [objects ...]",
	"Export BRL-CAD geometry as STEP");
    if (help)
	bu_log("%s", help);
    bu_free(help, "option description");
}

} // namespace

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);
    int help = 0;
    int list_schemas = 0;
    int native_csg = 0;
    int strict = 0;
    char *schema_name = NULL;
    char *length_unit = NULL;
    char *plane_angle_unit = NULL;
    char *output = NULL;
    char *report = NULL;
    PositiveFastfOption uncertainty;
    struct bu_vls messages = BU_VLS_INIT_ZERO;
    struct bu_opt_desc options[] = {
	{"h", "help", "", NULL, &help, "print help and exit"},
	{"?", "", "", NULL, &help, ""},
	{"", "schema", "AP", bu_opt_str, &schema_name,
	    "ap203, ap203e2, ap214, ap242 (e4), ap242e1 through ap242e4, or auto"},
	{"", "list-schemas", "", NULL, &list_schemas, "list installed schema plugins"},
	{"", "native-csg", "", NULL, &native_csg, "preserve supported AP203e2/AP214/AP242 boolean CSG trees"},
	{"", "output-units", "UNIT", bu_opt_str, &length_unit,
	    "write geometry in this recognized BRL-CAD length unit (default mm)"},
	{"", "angle-units", "UNIT", bu_opt_str, &plane_angle_unit,
	    "write plane angles as degree or radian (default degree)"},
	{"", "uncertainty", "VALUE", positive_fastf_option, &uncertainty,
	    "positive STEP uncertainty in the selected output length unit"},
	{"", "strict", "", NULL, &strict, "fail without publishing partial STEP output"},
	{"", "report", "FILE", bu_opt_str, &report, "write a JSON export coverage report"},
	{"o", "output", "FILE", bu_opt_str, &output, "output STEP file"},
	BU_OPT_DESC_NULL
    };
    ++argv;
    --argc;
    argc = bu_opt_parse(&messages, argc, argv, options);
    if (bu_vls_strlen(&messages)) bu_log("%s\n", bu_vls_cstr(&messages));
    if (list_schemas) {
	for (const std::string &schema : brlcad::step::available_schema_plugins())
	    bu_log("%s\n", schema.c_str());
	bu_vls_free(&messages);
	return 0;
    }
    if (help) {
	usage(options);
	bu_vls_free(&messages);
	return 0;
    }
    if (bu_vls_strlen(&messages) || argc < 1 || !output) {
	usage(options);
	bu_vls_free(&messages);
	return 1;
    }
    bu_vls_free(&messages);

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
    export_options.uncertainty = uncertainty.set ? uncertainty.value : 0.0;
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
