/**
 *                P R O F I L E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/**
 * @file profile.cpp
 *
 * Toolkit-free BRL-CAD profile parsing for the runtime-loaded backend.
 */

#include "common.h"

#include <stdarg.h>
#include <stdio.h>

extern "C" {
#include "bu.h"
#include "raytrace.h"
#include "wdb.h"
}

#include "frontend_api.h"
#include "profile_controls.h"

#define PROFILE_MESSAGE_MAX 4096

static struct creo_brl_frontend_api frontend_api = {0};

static void profile_status(const char *format, ...);
static const char *profile_input_value(int index);
static int profile_radio_button_index(const char *resource, const char *value);
static void apply_profile_value(const char *resource, const char *type, const char *value, const char *default_value);
static void load_profile_defaults(const char *material_directory);


extern "C" __declspec(dllexport) void
creo_brl_core_set_frontend_api(const struct creo_brl_frontend_api *api)
{
    if (api) {
        frontend_api = *api;
        return;
    }

    frontend_api.show_status = NULL;
    frontend_api.show_popup = NULL;
    frontend_api.apply_control = NULL;
}


static void
profile_status(const char *format, ...)
{
    char message[PROFILE_MESSAGE_MAX] = {'\0'};
    va_list arguments;

    va_start(arguments, format);
    vsprintf_s(message, sizeof(message), format, arguments);
    va_end(arguments);

    if (frontend_api.show_status)
        frontend_api.show_status(message);
}


static const char *
profile_input_value(int index)
{
    for (const struct creo_profile_input *input = creo_profile_inputs; input->value; input++) {
        if (input->index == index)
            return input->value;
    }

    return NULL;
}


static int
profile_radio_button_index(const char *resource, const char *value)
{
    if (!value)
        return -1;

    for (const struct creo_profile_control *control = creo_profile_controls; control->resource; control++) {
        if (bu_strcmp(resource, control->resource) != 0)
            continue;

        for (size_t index = 0; index < CREO_PROFILE_RADIO_BUTTON_MAX; index++) {
            const char *candidate = profile_input_value(control->input_indices[index]);

            if (candidate && bu_strcmp(candidate, value) == 0)
                return (int)index;
        }
    }

    return -1;
}


static int
is_material_file_control(const struct creo_profile_control *control)
{
    return bu_strcmp(control->resource, creo_profile_material_resource) == 0;
}


static const char *
profile_default_value(const struct creo_profile_control *control, const char *material_directory, struct bu_vls *material_path)
{
    const char *default_value = profile_input_value(control->input_indices[0]);

    if (!is_material_file_control(control) || !material_directory || !material_directory[0])
        return default_value;

    bu_vls_sprintf(material_path, "%s\\%s", material_directory, creo_profile_bundle_material_file_name);
    return bu_vls_cstr(material_path);
}


static void
apply_profile_value(const char *resource, const char *type, const char *value, const char *default_value)
{
    const char *value_to_apply = value;

    if (bu_strcmp(type, "RAD") == 0) {
        if (profile_radio_button_index(resource, value) <= 0)
            value_to_apply = default_value;
    } else if (bu_strcmp(type, "STR") != 0 && bu_strcmp(type, "BOX") != 0) {
        profile_status("Unknown control type: %s", type);
        return;
    }

    if (!frontend_api.apply_control) {
        profile_status("Unable to apply profile value for %s: no frontend is available", resource);
        return;
    }

    frontend_api.apply_control(resource, type, value_to_apply, default_value);
}


static void
load_profile_defaults(const char *material_directory)
{
    struct bu_vls material_path = BU_VLS_INIT_ZERO;

    for (const struct creo_profile_control *control = creo_profile_controls; control->attribute; control++) {
        const char *default_value = profile_default_value(control, material_directory, &material_path);

        apply_profile_value(control->resource, control->type, default_value, default_value);
    }

    bu_vls_free(&material_path);
}


static struct bu_vls *
profile_path(const char *profile_directory)
{
    struct bu_vls *path = NULL;

    if (!profile_directory || !profile_directory[0]) {
        profile_status("Unable to determine the bundle profile directory");
        return NULL;
    }

    BU_GET(path, struct bu_vls);
    bu_vls_init(path);
    bu_vls_sprintf(path, "%s\\%s", profile_directory, creo_profile_file_name);

    if (!bu_file_exists(bu_vls_cstr(path), NULL)) {
        profile_status("Unable to locate bundle profile \"%s\"", bu_vls_addr(path));
        bu_vls_free(path);
        BU_PUT(path, struct bu_vls);
        return NULL;
    }

    profile_status("Bundle profile is \"%s\"", bu_vls_addr(path));
    return path;
}


static void
load_profile_from_directory(const char *profile_directory, const char *material_directory)
{
    struct bu_attribute_value_set attributes;
    struct bu_vls *profile = profile_path(profile_directory);
    struct bu_vls material_path = BU_VLS_INIT_ZERO;
    struct db_i *database = NULL;
    struct directory *global_directory = NULL;

    if (!profile) {
        profile_status("Unable to locate user profile");
        load_profile_defaults(material_directory);
        return;
    }

    profile_status("User profile is \"%s\"", bu_vls_addr(profile));
    database = db_open(bu_vls_cstr(profile), DB_OPEN_READONLY);
    if (database == DBI_NULL) {
        profile_status("\"db_open\" failed to open the user profile");
        load_profile_defaults(material_directory);
        goto cleanup;
    }

    RT_CK_DBI(database);
    if (db_dirbuild(database) < 0) {
        profile_status("\"db_dirbuild\" failed to build \"%s\"", bu_vls_addr(profile));
        load_profile_defaults(material_directory);
        goto cleanup;
    }

    global_directory = db_lookup(database, DB5_GLOBAL_OBJECT_NAME, LOOKUP_QUIET);
    if (global_directory == RT_DIR_NULL) {
        profile_status("Failed to find the _GLOBAL profile record");
        load_profile_defaults(material_directory);
        goto cleanup;
    }

    if (db5_get_attributes(database, &attributes, global_directory)) {
        profile_status("Failed to find any _GLOBAL attributes");
        bu_avs_free(&attributes);
        load_profile_defaults(material_directory);
        goto cleanup;
    }

    if (attributes.count) {
        for (const struct creo_profile_control *control = creo_profile_controls; control->attribute; control++) {
            const char *default_value = profile_default_value(control, material_directory, &material_path);
            const char *value = bu_avs_get(&attributes, control->attribute);

            if (value && is_material_file_control(control) && material_directory && material_directory[0] &&
                bu_strcmp(value, creo_profile_legacy_material_file_path) == 0)
                value = default_value;

            apply_profile_value(control->resource, control->type, value ? value : default_value, default_value);
        }
        bu_avs_free(&attributes);
    }

cleanup:
    if (database)
        db_close(database);
    bu_vls_free(&material_path);
    bu_vls_free(profile);
    BU_PUT(profile, struct bu_vls);
}


extern "C" __declspec(dllexport) void
creo_brl_core_load_profile(const char *profile_directory, const char *material_directory)
{
    load_profile_from_directory(profile_directory, material_directory);
}
