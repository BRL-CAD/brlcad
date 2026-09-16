/*                    S T E P P L U G I N H O S T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#define BU_PLUGIN_IMPLEMENTATION
#include "step_plugin.h"
#include "StepPluginHost.h"
#include "StepPluginConfig.h"

#include <exception>
#include <string>

#include "bu/app.h"

namespace {

std::string permitted_plugin_path;

int
allow_selected_plugin(const char *path)
{
    return path && permitted_plugin_path == path;
}

const char *
plugin_basename(const std::string &schema)
{
#if STEP_PLUGIN_HAVE_AP203
    if (schema == "ap203") return STEP_PLUGIN_AP203_BASENAME;
#endif
#if STEP_PLUGIN_HAVE_AP203E2
    if (schema == "ap203e2") return STEP_PLUGIN_AP203E2_BASENAME;
#endif
#if STEP_PLUGIN_HAVE_AP214
    if (schema == "ap214") return STEP_PLUGIN_AP214_BASENAME;
#endif
#if STEP_PLUGIN_HAVE_AP242E1
    if (schema == "ap242e1") return STEP_PLUGIN_AP242E1_BASENAME;
#endif
#if STEP_PLUGIN_HAVE_AP242E2
    if (schema == "ap242e2") return STEP_PLUGIN_AP242E2_BASENAME;
#endif
#if STEP_PLUGIN_HAVE_AP242E3
    if (schema == "ap242e3") return STEP_PLUGIN_AP242E3_BASENAME;
#endif
#if STEP_PLUGIN_HAVE_AP242E4
    if (schema == "ap242e4") return STEP_PLUGIN_AP242E4_BASENAME;
#endif
    return NULL;
}

} // namespace

bool
brlcad::step::schema_plugin_available(const std::string &schema)
{
    return plugin_basename(schema) != NULL;
}

std::vector<std::string>
brlcad::step::available_schema_plugins()
{
    std::vector<std::string> schemas;
#if STEP_PLUGIN_HAVE_AP203
    schemas.emplace_back("ap203");
#endif
#if STEP_PLUGIN_HAVE_AP203E2
    schemas.emplace_back("ap203e2");
#endif
#if STEP_PLUGIN_HAVE_AP214
    schemas.emplace_back("ap214");
#endif
#if STEP_PLUGIN_HAVE_AP242E1
    schemas.emplace_back("ap242e1");
#endif
#if STEP_PLUGIN_HAVE_AP242E2
    schemas.emplace_back("ap242e2");
#endif
#if STEP_PLUGIN_HAVE_AP242E3
    schemas.emplace_back("ap242e3");
#endif
#if STEP_PLUGIN_HAVE_AP242E4
    schemas.emplace_back("ap242e4");
#endif
    return schemas;
}

std::string
brlcad::step::schema_plugin_command(const std::string &schema, bool import_operation)
{
    return std::string("step.") + schema + (import_operation ? ".import" : ".export");
}

bool
brlcad::step::load_schema_plugin(const std::string &schema, std::string &error)
{
    const char *basename = plugin_basename(schema);
    if (!basename) {
	error = "STEP schema '" + schema + "' is recognized but its plugin is unavailable";
	return false;
    }
    if (bu_plugin_loaded_modules_count() != 0) {
	error = "a STEP schema plugin is already loaded in this process";
	return false;
    }

    char path[MAXPATHLEN] = {0};
    const std::string filename = std::string(basename) + STEP_PLUGIN_MODULE_SUFFIX;
    if (!bu_dir(path, MAXPATHLEN, BU_DIR_LIBEXEC, "step", filename.c_str(), NULL)) {
	error = "cannot resolve the installed STEP plugin directory";
	return false;
    }
    permitted_plugin_path = path;
    bu_plugin_set_path_allow(allow_selected_plugin);
    if (bu_plugin_init() != 0 || bu_plugin_load(path) != 2) {
	error = "unable to load STEP schema plugin '" + schema + "' from " + path;
	return false;
    }
    return true;
}

extern "C" int
step_plugin_host_call(const char *command,
                      const struct step_plugin_request *request,
                      struct step_plugin_result *result)
{
    if (!step_plugin_validate_result(result)) return STEP_PLUGIN_STATUS_ABI;

    result->status = STEP_PLUGIN_STATUS_OK;
    result->warning_count = 0;
    result->error_count = 0;
    result->diagnostic_length = 0;
    if (result->diagnostic_buffer && result->diagnostic_capacity)
        result->diagnostic_buffer[0] = '\0';

    if (!step_plugin_validate_request(request, result)) return result->status;
    if (!command || !command[0]) {
        step_plugin_result_message(result, STEP_PLUGIN_STATUS_USAGE,
                                   "missing STEP plugin command");
        return result->status;
    }

    bu_plugin_cmd_impl implementation = bu_plugin_cmd_get(command);
    if (!implementation) {
        step_plugin_result_message(result, STEP_PLUGIN_STATUS_INTERNAL,
                                   "requested STEP plugin command is not registered");
        return result->status;
    }

    try {
        const int status = implementation(request, result);
        result->status = status;
        return status;
    } catch (const std::exception &e) {
        step_plugin_result_message(result, STEP_PLUGIN_STATUS_EXCEPTION, e.what());
    } catch (...) {
        step_plugin_result_message(result, STEP_PLUGIN_STATUS_EXCEPTION,
                                   "STEP plugin threw an unknown exception");
    }
    return result->status;
}
