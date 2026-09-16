/*                    S T E P S C H E M A P L U G I N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * Thin C-ABI adapter shared textually by each schema module.  All generated
 * schema objects and converter-owned C++ state remain inside the module.
 */

#include "step_plugin.h"
#include "StepExportPlan.h"

#include "bu/units.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <string>
#include <vector>

#ifndef STEP_PLUGIN_SCHEMA
#  error STEP_PLUGIN_SCHEMA must name the normalized schema key
#endif
#ifndef STEP_PLUGIN_COMMAND_PREFIX
#  error STEP_PLUGIN_COMMAND_PREFIX must name the plugin command prefix
#endif
#ifndef STEP_PLUGIN_IMPORT_CLI
#  error STEP_PLUGIN_IMPORT_CLI must name the callable importer
#endif
#ifndef STEP_PLUGIN_EXPORT_CLI
#  error STEP_PLUGIN_EXPORT_CLI must name the callable exporter
#endif

extern "C" int STEP_PLUGIN_IMPORT_CLI(int, const char *[]);
extern "C" int STEP_PLUGIN_EXPORT_CLI(int, char *[]);

namespace {

static void
append_flag(std::vector<std::string> &arguments, const char *flag, bool enabled)
{
    if (enabled) arguments.emplace_back(flag);
}

static void
append_value(std::vector<std::string> &arguments, const char *flag, const std::string &value)
{
    arguments.emplace_back(flag);
    arguments.push_back(value);
}

static std::vector<const char *>
const_argv(const std::vector<std::string> &arguments)
{
    std::vector<const char *> result;
    result.reserve(arguments.size() + 1);
    for (const std::string &argument : arguments) result.push_back(argument.c_str());
    result.push_back(NULL);
    return result;
}

static std::vector<char *>
mutable_argv(std::vector<std::string> &arguments)
{
    std::vector<char *> result;
    result.reserve(arguments.size() + 1);
    for (std::string &argument : arguments) result.push_back(&argument[0]);
    result.push_back(NULL);
    return result;
}

static int
finish(struct step_plugin_result *result, int status, const char *operation)
{
    if (!result->diagnostic_length) {
        std::string message = std::string("STEP ") + operation + " completed";
        if (status == STEP_PLUGIN_STATUS_PARTIAL)
            message += " with partial output";
        else if (status == STEP_PLUGIN_STATUS_EMPTY)
            message += " without geometric output";
        else if (status)
            message = std::string("STEP ") + operation + " failed";
        step_plugin_result_message(result, status, message.c_str());
    } else {
        result->status = status;
        if (status == STEP_PLUGIN_STATUS_PARTIAL ||
                status == STEP_PLUGIN_STATUS_EMPTY) {
            if (!result->warning_count) result->warning_count = 1;
        } else if (status && !result->error_count) {
            result->error_count = 1;
        }
    }
    return status;
}

static int
reject(struct step_plugin_result *result, int status, const char *message)
{
    step_plugin_result_message(result, status, message);
    return status;
}

static int
import_command(const struct step_plugin_request *request, struct step_plugin_result *result)
{
    if (!step_plugin_validate_result(result)) return STEP_PLUGIN_STATUS_ABI;
    if (!step_plugin_validate_request(request, result)) return result->status;
    if (request->operation != STEP_OPERATION_IMPORT)
        return reject(result, STEP_PLUGIN_STATUS_USAGE, "import command received an export request");
    if (std::string(request->schema) != STEP_PLUGIN_SCHEMA)
        return reject(result, STEP_PLUGIN_STATUS_USAGE, "request schema does not match loaded STEP plugin");

    try {
        std::vector<std::string> arguments;
        arguments.emplace_back("step-g");
        const struct step_import_options *options = request->import_options;
        if (options) {
            append_flag(arguments, "-D", options->dry_run != 0);
            append_flag(arguments, "-v", options->verbose != 0);
            append_flag(arguments, "--exact", options->exact != 0);
            append_flag(arguments, "--strict", options->strict != 0);
            append_flag(arguments, "--reject-invalid-objs", options->reject_invalid_objects != 0);
            append_flag(arguments, "--skip-open-shells", options->skip_open_shells != 0);
            append_flag(arguments, "--no-item-budget", options->disable_item_budgets != 0);
            if (options->requested_jobs)
                append_value(arguments, "-j", std::to_string(options->requested_jobs));
            if (options->absolute_tolerance_mm > 0.0)
                append_value(arguments, "--abs-tol", std::to_string(options->absolute_tolerance_mm));
            if (options->budget_scale > 0.0)
                append_value(arguments, "--budget-scale", std::to_string(options->budget_scale));
            if (options->item_budget_seconds > 0.0)
                append_value(arguments, "--item-budget", std::to_string(options->item_budget_seconds));
            if (options->stall_timeout_seconds > 0.0)
                append_value(arguments, "--stall-timeout", std::to_string(options->stall_timeout_seconds));
            if (options->repair_mode)
                append_value(arguments, "--repair", options->repair_mode);
            if (options->report_path)
                append_value(arguments, "--report", options->report_path);
            if (options->summary_path)
                append_value(arguments, "-S", options->summary_path);
            for (size_t i = 0; i < options->selected_entity_count; ++i)
                append_value(arguments, "--entity", std::to_string(options->selected_entity_ids[i]));
        }

        if (request->output_path[0])
            append_value(arguments,
                         options && options->overwrite ? "-O" : "-o",
                         request->output_path);
        for (int i = 0; i < request->argc; ++i) arguments.emplace_back(request->argv[i]);
        arguments.emplace_back(request->input_path);

        std::vector<const char *> argv = const_argv(arguments);
        return finish(result,
                      STEP_PLUGIN_IMPORT_CLI(static_cast<int>(arguments.size()), argv.data()),
                      "import");
    } catch (const std::exception &e) {
        return reject(result, STEP_PLUGIN_STATUS_EXCEPTION, e.what());
    } catch (...) {
        return reject(result, STEP_PLUGIN_STATUS_EXCEPTION, "STEP importer threw an unknown exception");
    }
}

static int
export_command(const struct step_plugin_request *request, struct step_plugin_result *result)
{
    if (!step_plugin_validate_result(result)) return STEP_PLUGIN_STATUS_ABI;
    if (!step_plugin_validate_request(request, result)) return result->status;
    if (request->operation != STEP_OPERATION_EXPORT)
        return reject(result, STEP_PLUGIN_STATUS_USAGE, "export command received an import request");
    if (std::string(request->schema) != STEP_PLUGIN_SCHEMA)
        return reject(result, STEP_PLUGIN_STATUS_USAGE, "request schema does not match loaded STEP plugin");

    const struct step_export_options *options = request->export_options;
    if (options && (options->dry_run || options->overwrite || options->verbose))
        return reject(result, STEP_PLUGIN_STATUS_USAGE,
                      "requested export option is not implemented by this schema emitter");
    if (options && ((!std::isfinite(options->length_unit_mm) ||
	    options->length_unit_mm < 0.0) ||
	    (!std::isfinite(options->uncertainty) || options->uncertainty < 0.0)))
	return reject(result, STEP_PLUGIN_STATUS_USAGE,
	              "STEP export unit factors and uncertainty must be finite and nonnegative");

    try {
        if (options && options->native_csg &&
            std::string(STEP_PLUGIN_SCHEMA) != "ap203e2" &&
            std::string(STEP_PLUGIN_SCHEMA) != "ap214" &&
            std::string(STEP_PLUGIN_SCHEMA) != "ap242e1" &&
            std::string(STEP_PLUGIN_SCHEMA) != "ap242e2" &&
            std::string(STEP_PLUGIN_SCHEMA) != "ap242e3" &&
            std::string(STEP_PLUGIN_SCHEMA) != "ap242e4")
            return reject(result, STEP_PLUGIN_STATUS_USAGE,
                          "native CSG export is supported only for AP203e2, AP214, and AP242");
        std::vector<std::string> requested_objects;
        if (options) {
            for (size_t i = 0; i < options->object_count; ++i)
                requested_objects.emplace_back(options->object_names[i]);
        }
        brlcad::step::StepExportPlan plan;
        std::string plan_error;
        if (!brlcad::step::BuildStepExportPlan(plan, request->input_path,
                                               requested_objects, plan_error))
            return reject(result, STEP_PLUGIN_STATUS_INPUT, plan_error.c_str());

        std::string length_unit;
        if (options && options->length_unit) {
	    length_unit = options->length_unit;
	    const double factor = bu_units_conversion(length_unit.c_str());
	    if (!(factor > 0.0) || (options->length_unit_mm > 0.0 &&
		    std::fabs(factor - options->length_unit_mm) >
		    std::max(factor, options->length_unit_mm) * 1.0e-12))
		return reject(result, STEP_PLUGIN_STATUS_USAGE,
		              "STEP output unit name and conversion factor disagree");
	} else if (options && options->length_unit_mm > 0.0) {
	    const char *canonical = bu_units_string(options->length_unit_mm);
	    if (!canonical)
		return reject(result, STEP_PLUGIN_STATUS_USAGE,
		              "STEP output length factor has no recognized unit name");
	    length_unit = canonical;
	}

        std::vector<std::string> arguments;
        arguments.emplace_back("g-step");
        append_value(arguments, "-o", request->output_path);
        if (options && options->native_csg) arguments.emplace_back("-C");
        if (options && options->strict) arguments.emplace_back("-s");
        if (!length_unit.empty()) append_value(arguments, "-u", length_unit);
        if (options && options->plane_angle_unit)
            append_value(arguments, "-a", options->plane_angle_unit);
        if (options && options->uncertainty > 0.0)
            append_value(arguments, "-t", std::to_string(options->uncertainty));
        if (options && options->report_path)
            append_value(arguments, "-R", options->report_path);
        for (int i = 0; i < request->argc; ++i) arguments.emplace_back(request->argv[i]);
        arguments.emplace_back(request->input_path);
        for (size_t root : plan.roots) arguments.push_back(plan.objects[root].name);
        std::vector<char *> argv = mutable_argv(arguments);
        return finish(result,
                      STEP_PLUGIN_EXPORT_CLI(static_cast<int>(arguments.size()), argv.data()),
                      "export");
    } catch (const std::exception &e) {
        return reject(result, STEP_PLUGIN_STATUS_EXCEPTION, e.what());
    } catch (...) {
        return reject(result, STEP_PLUGIN_STATUS_EXCEPTION, "STEP exporter threw an unknown exception");
    }
}

static const bu_plugin_cmd commands[] = {
    {STEP_PLUGIN_COMMAND_PREFIX ".import", import_command},
    {STEP_PLUGIN_COMMAND_PREFIX ".export", export_command}
};

static const bu_plugin_manifest manifest = {
    STEP_PLUGIN_COMMAND_PREFIX,
    1,
    2,
    commands,
    BU_PLUGIN_ABI_VERSION,
    sizeof(bu_plugin_manifest)
};

} // namespace

BU_PLUGIN_DECLARE_MANIFEST(manifest)
