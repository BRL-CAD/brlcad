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

#include "common.h"

#define BU_PLUGIN_IMPLEMENTATION
#include "step_plugin.h"
#include "StepPluginHost.h"
#include "STEPHeaderSchema.h"
#include "StepMime.h"
#include "StepPluginConfig.h"

#include <algorithm>
#include <exception>
#include <string>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bu/vls.h"

namespace {

std::string permitted_plugin_path;

int
allow_selected_plugin(const char *path)
{
    return path && permitted_plugin_path == path;
}

struct SchemaPlugin {
    const char *schema;
    const char *basename;
};

const std::vector<SchemaPlugin> &
configured_plugins()
{
    static const std::vector<SchemaPlugin> plugins = {
#if STEP_PLUGIN_HAVE_AP203
	{"ap203", STEP_PLUGIN_AP203_BASENAME},
#endif
#if STEP_PLUGIN_HAVE_AP203E2
	{"ap203e2", STEP_PLUGIN_AP203E2_BASENAME},
#endif
#if STEP_PLUGIN_HAVE_AP214
	{"ap214", STEP_PLUGIN_AP214_BASENAME},
#endif
#if STEP_PLUGIN_HAVE_AP242E1
	{"ap242e1", STEP_PLUGIN_AP242E1_BASENAME},
#endif
#if STEP_PLUGIN_HAVE_AP242E2
	{"ap242e2", STEP_PLUGIN_AP242E2_BASENAME},
#endif
#if STEP_PLUGIN_HAVE_AP242E3
	{"ap242e3", STEP_PLUGIN_AP242E3_BASENAME},
#endif
#if STEP_PLUGIN_HAVE_AP242E4
	{"ap242e4", STEP_PLUGIN_AP242E4_BASENAME},
#endif
    };
    return plugins;
}

const char *
plugin_basename(const std::string &schema)
{
    for (const auto &plugin : configured_plugins())
	if (schema == plugin.schema) return plugin.basename;
    return NULL;
}

} // namespace

bool
brlcad::step::schema_mime_capability(const std::string &schema, const char *type,
    unsigned operation, bu_mime_capability &capability)
{
    if (!schema_plugin_available(schema) || !type ||
	(operation != BU_MIME_READ && operation != BU_MIME_WRITE)) return false;
    if (operation == BU_MIME_READ && !BU_STR_EQUAL(type, "model/step")) return false;
    MimeTraits emitted, constraints;
    if (!append_output_schema_traits(schema, type, emitted.value)) return false;
    if (operation == BU_MIME_READ) {
	const char *ap = bu_mime_traits_get(&emitted.value, "step.ap");
	if (!ap || !bu_mime_traits_add(&constraints.value, "step.ap", ap)) return false;
	/* AP203 and its modular extension have separate routing. AP214 accepts
	 * its earlier declarations; AP242 permits an explicit binding override
	 * and physical-layout fallback when the source edition is unknown. */
	if (BU_STR_EQUAL(ap, "203")) {
	    const char *edition = bu_mime_traits_get(&emitted.value, "step.edition");
	    if (!edition || !bu_mime_traits_add(&constraints.value, "step.edition", edition)) return false;
	}
    }
    const auto &traits = operation == BU_MIME_READ ? constraints.value : emitted.value;
    bu_mime_capability profile = BU_MIME_CAPABILITY(type, traits.items, traits.count, operation);
    if (operation == BU_MIME_READ) profile.flags = BU_MIME_DEFER_INPUT_VALIDATION;
    return bu_mime_capability_copy(&capability, &profile) != 0;
}

bool
brlcad::step::list_schema_mime_profiles(unsigned operation)
{
    if (operation != BU_MIME_READ && operation != BU_MIME_WRITE) return false;
    for (const auto &schema : available_schema_plugins())
	for (const char *type : {"model/step", "application/p21"}) {
	    if (operation == BU_MIME_READ && BU_STR_EQUAL(type, "application/p21")) continue;
	    bu_mime_capability profile = {};
	    if (!schema_mime_capability(schema, type, operation, profile)) return false;
	    const bu_mime_traits traits = {profile.traits, profile.trait_count};
	    bu_vls line = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&line, "%s %s %s", profile.mime_type,
		operation == BU_MIME_READ ? "read" : "write", schema.c_str());
	    bu_mime_traits_describe(&traits, &line);
	    bu_log("%s\n", bu_vls_cstr(&line));
	    bu_vls_free(&line);
	    bu_mime_capability_free(&profile);
	}
    return true;
}

std::vector<std::string>
brlcad::step::schema_plugin_candidates(const HeaderSchemaInfo &header, const std::string &requested, std::string &error)
{
    error.clear();
    if (!header.recognized) {
	error = header.error.empty() ? "unrecognized STEP schema" : header.error;
	return {};
    }
    const std::string key = STEPHeaderSchema::key(header.schema);
    if (header.schema != HeaderSchema::AP242) {
	if (requested == "auto" || requested == key) return {key};
	error = "FILE_SCHEMA selected " + key + " but --schema requested " + requested;
	return {};
    }
    const std::vector<std::string> supported = {"ap242e4", "ap242e3", "ap242e2", "ap242e1"};
    if (requested == "auto" || requested == "ap242") {
	if (!header.edition.empty()) return {"ap242e" + header.edition};
	return supported;
    }
    if (std::find(supported.begin(), supported.end(), requested) == supported.end()) {
	error = "AP242 input requires --schema auto or ap242e1 through ap242e4";
	return {};
    }
    /* An explicit binding override does not assert the source's edition. */
    return {requested};
}

std::string
brlcad::step::select_schema_output(const char *format, const bu_mime_traits *requested,
    const std::string &explicit_schema, const std::string &default_schema, std::string &error)
{
    error.clear();
    const std::string type = step_mime_identity(format, true);
    if (type.empty() || !bu_mime_traits_valid(requested, 1)) {
	error = "unsupported STEP output MIME identity or invalid trait request";
	return "";
    }
    const bool explicit_choice = !explicit_schema.empty() && explicit_schema != "auto";
    std::string preferred = explicit_choice ? explicit_schema : default_schema;
    if (preferred == "ap242") preferred = "ap242e4";
    const auto matches = [&](const std::string &schema) {
	bu_mime_capability capability = {};
	if (!schema_mime_capability(schema, type.c_str(), BU_MIME_WRITE, capability)) return false;
	MimeTraits resolved;
	const bu_mime_traits emitted = {capability.traits, capability.trait_count};
	/* Header sets are fixed output facts, rather than optional features. */
	const bool supported = bu_mime_traits_resolve(&resolved.value, &emitted, requested) &&
	    bu_mime_capability_match(&capability, requested, BU_MIME_OUTPUT);
	bu_mime_capability_free(&capability);
	return supported;
    };
    if (matches(preferred)) return preferred;
    if (explicit_choice || !requested || !requested->count) {
	error = "selected STEP schema cannot satisfy the requested output profile: " + preferred;
	return "";
    }
    std::string selected;
    for (const auto &candidate : available_schema_plugins()) {
	if (candidate == preferred || !matches(candidate)) continue;
	if (!selected.empty()) {
	    error = "ambiguous STEP output profile; select --schema or more specific output traits";
	    return "";
	}
	selected = candidate;
    }
    if (selected.empty()) error = "no configured STEP writer supports the requested output profile";
    return selected;
}

bool
brlcad::step::schema_plugin_available(const std::string &schema)
{
    return plugin_basename(schema) != NULL;
}

std::vector<std::string>
brlcad::step::available_schema_plugins()
{
    std::vector<std::string> schemas;
    for (const auto &plugin : configured_plugins()) schemas.emplace_back(plugin.schema);
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
