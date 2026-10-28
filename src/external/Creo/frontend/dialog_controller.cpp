/**
 *         D I A L O G _ C O N T R O L L E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/**
 * @file frontend/dialog_controller.cpp
 *
 * Creo-facing dialog and conversion controller implementation.
 */

#include "common.h"
#include <stdarg.h>
#include <windows.h>

#include "frontend/creo_toolkit.h"
#include "shared/conversion_snapshot.h"
#include "frontend/dialog_controller.h"
#include "frontend/model_capture.h"
#include "frontend/profile_ui.h"
#include "shared/core_api.h"

extern "C" void creo_brl_show_status(const char *message);
extern "C" void creo_brl_show_popup(const char *title, const char *message);
extern "C" void creo_brl_core_load_profile_shim(void);
extern "C" int creo_brl_core_convert_shim(const char *snapshot_path);

static const wchar_t FRONTEND_SNAPSHOT_PREFIX[] = L"cbr";
static const size_t FRONTEND_SNAPSHOT_UTF8_PATH_SIZE = MAX_PATH * 4;
static const char FRONTEND_STATUS_COMPONENT[] = "conv_status";
static const char FRONTEND_RESTORATION_WARNING_TITLE[] = "WARNING:  Restoration Failure";
static const char FRONTEND_RESTORATION_WARNING_MESSAGE[] =
    "During the conversion, one or more parts contained features that were\n"
    "suppressed.  After the conversion was complete, attempts were made to\n"
    "resume these same features.  One or more of these attempts has failed\n"
    "to resume them, so some features remain suppressed.\n\n"
    "To avoid permanent changes to the current Creo model, please exit\n"
    "without saving any changes.";

static void
frontend_set_dialog_status(const char *message)
{
    wchar_t wide_message[CREO_MSG_MAX] = {L'\0'};

    if (!message)
        return;

    ProStringToWstring(wide_message, (char *)message);
    (void)ProUILabelTextSet(CREO_UI_NAME, (char *)FRONTEND_STATUS_COMPONENT, wide_message);
}

static void
frontend_status(const char *fmt, ...)
{
    va_list ap;
    char msg[CREO_MSG_MAX] = {'\0'};

    va_start(ap, fmt);
    vsprintf_s(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    creo_brl_show_status(msg);
    frontend_set_dialog_status(msg);
}

static void
frontend_error(const char *title, const char *message)
{
    if (!message)
        return;

    frontend_status("%s", message);
    creo_brl_show_popup(title ? title : "Creo-BRL Error", message);
}


static int
frontend_set_text(void *UNUSED(context), const char *resource, const char *value)
{
    wchar_t text[CREO_NAME_MAX];

    if (!resource || !value)
        return 0;

    ProStringToWstring(text, (char *)value);
    return ProUIInputpanelValueSet(CREO_UI_NAME, (char *)resource, text) == PRO_TK_NO_ERROR;
}


static int
frontend_set_check(void *UNUSED(context), const char *resource, int checked)
{
    if (!resource)
        return 0;

    return (checked ? ProUICheckbuttonSet(CREO_UI_NAME, (char *)resource) :
                      ProUICheckbuttonUnset(CREO_UI_NAME, (char *)resource)) == PRO_TK_NO_ERROR;
}


static int
frontend_set_radio(void *UNUSED(context), const char *resource, const char *selection)
{
    char *selected_name = (char *)selection;

    return resource && selection &&
           ProUIRadiogroupSelectednamesSet(CREO_UI_NAME, (char *)resource, 1,
                                            &selected_name) == PRO_TK_NO_ERROR;
}


static int
frontend_get_text(void *UNUSED(context), const char *name, std::string *text)
{
    wchar_t *wide_text = NULL;
    std::string converted_text;
    int size = 0;
    const ProError err = ProUIInputpanelValueGet(CREO_UI_NAME, (char *)name, &wide_text);
    int converted = 0;

    if (err == PRO_TK_NO_ERROR && wide_text && text) {
        size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_text, -1,
                                   NULL, 0, NULL, NULL);
        if (size > 0) {
            converted_text.resize((size_t)size);
            converted = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_text, -1,
                                             &converted_text[0], size, NULL, NULL) == size;
            if (converted) {
                converted_text.resize((size_t)size - 1);
                *text = converted_text;
            }
        }
    }

    if (wide_text)
        (void)ProWstringFree(wide_text);

    return converted;
}


static int
frontend_get_check(void *UNUSED(context), const char *name, int *checked)
{
    ProBoolean state = PRO_B_FALSE;

    if (!checked || ProUICheckbuttonGetState(CREO_UI_NAME, (char *)name, &state) != PRO_TK_NO_ERROR)
        return 0;

    *checked = state != PRO_B_FALSE;
    return 1;
}


static int
frontend_get_radio(void *UNUSED(context), const char *name, std::string *selection)
{
    char **names = NULL;
    int count = 0;
    const ProError err = ProUIRadiogroupSelectednamesGet(CREO_UI_NAME, (char *)name, &count, &names);

    if (err != PRO_TK_NO_ERROR || count != 1 || !names || !names[0]) {
        if (names)
            ProStringarrayFree(names, count);
        return 0;
    }

    *selection = names[0];
    ProStringarrayFree(names, count);
    return 1;
}


static const struct creo_brl_frontend_control_api frontend_controls = {
    NULL,
    frontend_set_text,
    frontend_set_check,
    frontend_set_radio,
    frontend_get_text,
    frontend_get_check,
    frontend_get_radio
};


static int
capture_settings(struct creo_brl_snapshot_settings *settings, snapshot_settings_strings *strings)
{
    struct creo_brl_frontend_snapshot_options captured = {};

    if (!settings || !strings ||
        !creo_brl_frontend_capture_snapshot_options(&frontend_controls, &captured))
        return 0;

    *settings = captured.settings;
    strings->output_path = captured.output_path;
    strings->log_path = captured.log_path;
    strings->material_path = captured.material_path;
    strings->rename_parameters = captured.rename_parameters;
    strings->preserve_parameters = captured.preserve_parameters;
    return 1;
}


static int
frontend_capture_snapshot(const char *snapshot_path)
{
    struct creo_brl_snapshot_settings settings = {};
    struct snapshot_settings_strings settings_strings;
    ProMdl model = NULL;
    ProMdlType model_type;

    if (!snapshot_path || !snapshot_path[0])
        return CREO_BRL_SNAPSHOT_CAPTURE_INVALID_REQUEST;
    if (!capture_settings(&settings, &settings_strings))
        return CREO_BRL_SNAPSHOT_CAPTURE_DIALOG_FAILURE;
    if (ProMdlCurrentGet(&model) != PRO_TK_NO_ERROR)
        return CREO_BRL_SNAPSHOT_CAPTURE_NO_ACTIVE_MODEL;
    if (ProMdlTypeGet(model, &model_type) != PRO_TK_NO_ERROR)
        return CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE;

    if (model_type == PRO_MDL_PART || model_type == PRO_MDL_ASSEMBLY) {
        struct snapshot_scene_capture capture = {};
        const int result = creo_brl_frontend_capture_model_scene(
            model, model_type, &settings, &settings_strings,
            &capture, &capture.root_type, &capture.root_index);

        if (result != CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS)
            return result;

        return creo_brl_snapshot_write(snapshot_path, capture) ==
                       CREO_BRL_SNAPSHOT_IO_SUCCESS ?
                   CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS :
                   CREO_BRL_SNAPSHOT_CAPTURE_WRITE_FAILURE;
    }

    return CREO_BRL_SNAPSHOT_CAPTURE_UNSUPPORTED_MODEL;
}


void
creo_brl_frontend_dialog_apply_options(const struct creo_brl_conversion_options *options)
{
    if (!creo_brl_frontend_apply_options_to_controls(options, &frontend_controls))
        frontend_error("Creo-BRL Profile Warning",
                       "FAILURE: Unable to apply Creo-BRL profile settings.");
}


static int
frontend_snapshot_path(wchar_t *wide_path, size_t wide_path_size,
                       char *utf8_path, size_t utf8_path_size)
{
    wchar_t temporary_directory[MAX_PATH] = {L'\0'};
    const DWORD directory_length = GetTempPathW(_countof(temporary_directory), temporary_directory);
    DWORD file_number = 0;
    int utf8_length = 0;

    if (!wide_path || !utf8_path || wide_path_size < MAX_PATH || utf8_path_size == 0 ||
        directory_length == 0 || directory_length >= _countof(temporary_directory))
        return 0;

    file_number = GetTempFileNameW(temporary_directory, FRONTEND_SNAPSHOT_PREFIX, 0, wide_path);
    if (file_number == 0)
        return 0;

    utf8_length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_path, -1,
                                      utf8_path, (int)utf8_path_size, NULL, NULL);
    if (utf8_length <= 0) {
        (void)DeleteFileW(wide_path);
        return 0;
    }

    return 1;
}


static const char *
frontend_capture_failure_message(int result)
{
    switch (result) {
        case CREO_BRL_SNAPSHOT_CAPTURE_DIALOG_FAILURE:
            return "FAILURE: Unable to read the current Creo-BRL dialog settings.";
        case CREO_BRL_SNAPSHOT_CAPTURE_NO_ACTIVE_MODEL:
            return "FAILURE: No active Creo model is available for conversion.";
        case CREO_BRL_SNAPSHOT_CAPTURE_UNSUPPORTED_MODEL:
            return "FAILURE: Only active Creo parts and assemblies can be converted.";
        case CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE:
            return "FAILURE: Unable to read active model metadata or supported length units.";
        case CREO_BRL_SNAPSHOT_CAPTURE_RESTORATION_FAILURE:
            return "FAILURE: Unable to restore temporarily suppressed Creo features.";
        case CREO_BRL_SNAPSHOT_CAPTURE_EMPTY_MODEL:
            return "FAILURE: The active model contains no convertible solid geometry.";
        case CREO_BRL_SNAPSHOT_CAPTURE_WRITE_FAILURE:
            return "FAILURE: Unable to create the temporary conversion request.";
        default:
            return "FAILURE: Unable to capture the current conversion request.";
    }
}


static const char *
frontend_core_failure_message(int result)
{
    switch (result) {
        case CREO_BRL_CORE_CONVERT_OPEN_FAILED:
            return "FAILURE: The Creo-BRL runtime core is unavailable.";
        case CREO_BRL_CORE_CONVERT_INVALID_SNAPSHOT:
            return "FAILURE: The captured conversion request was invalid.";
        case CREO_BRL_CORE_CONVERT_NOT_IMPLEMENTED:
            return "FAILURE: The requested conversion path is not implemented.";
        case CREO_BRL_CORE_CONVERT_OUTPUT_FAILED:
            return "FAILURE: The BRL-CAD output file could not be written.";
        default:
            return "FAILURE: The runtime core failed to convert the active model.";
    }
}


/* Activates small feature settings */
static void
activate_small_feats(char *dialog_name, char *button_name, ProAppData UNUSED(data))
{
    ProBoolean state;

    if (ProUICheckbuttonGetState(dialog_name, button_name, &state) != PRO_TK_NO_ERROR) {
        frontend_error("Creo-BRL UI Error",
                       "FAILURE: Unable to obtain check button state: \"Ignore minimum sizes\"");
        return;
    }

    if (state) {
        if (ProUIInputpanelEditable(dialog_name, "min_hole") != PRO_TK_NO_ERROR) {
            frontend_error("Creo-BRL UI Error",
                           "FAILURE: Unable to activate: \"Hole diameter\"");
            return;
        }
        if (ProUIInputpanelEditable(dialog_name, "min_chamfer") != PRO_TK_NO_ERROR) {
            frontend_error("Creo-BRL UI Error",
                           "FAILURE: Unable to activate: \"Chamfer dimension\"");
            return;
        }
        if (ProUIInputpanelEditable(dialog_name, "min_round") != PRO_TK_NO_ERROR) {
            frontend_error("Creo-BRL UI Error",
                           "FAILURE: Unable to activate: \"Blend radius\"");
            return;
        }
    } else {
        if (ProUIInputpanelReadOnly(dialog_name, "min_hole") != PRO_TK_NO_ERROR) {
            frontend_error("Creo-BRL UI Error",
                           "FAILURE: Unable to de-activate: \"Hole diameter\"");
            return;
        }
        if (ProUIInputpanelReadOnly(dialog_name, "min_chamfer") != PRO_TK_NO_ERROR) {
            frontend_error("Creo-BRL UI Error",
                           "FAILURE: Unable to de-activate: \"Chamfer dimension\"");
            return;
        }
        if (ProUIInputpanelReadOnly(dialog_name, "min_round") != PRO_TK_NO_ERROR) {
            frontend_error("Creo-BRL UI Error",
                           "FAILURE: Unable to de-activate: \"Blend radius\"");
            return;
        }
    }
}


/* Exit the converter dialog */
static void
do_quit(char *UNUSED(dialog), char *UNUSED(component), ProAppData UNUSED(appdata))
{
    ProUIDialogDestroy(CREO_UI_NAME);
}


static void
frontend_convert(char *dialog, char *component, ProAppData appdata)
{
    wchar_t snapshot_path_wide[MAX_PATH] = {L'\0'};
    char snapshot_path[FRONTEND_SNAPSHOT_UTF8_PATH_SIZE] = {'\0'};
    int capture_result = CREO_BRL_SNAPSHOT_CAPTURE_INVALID_REQUEST;
    int core_result = CREO_BRL_CORE_CONVERT_INVALID_REQUEST;

    (void)dialog;
    (void)component;
    (void)appdata;

    if (!frontend_snapshot_path(snapshot_path_wide, _countof(snapshot_path_wide),
                                snapshot_path, sizeof(snapshot_path))) {
        frontend_error("Creo-BRL Conversion Failure",
                       "FAILURE: Unable to create a temporary conversion request path.");
        return;
    }

    frontend_status("Capturing active Creo model for BRL-CAD conversion...");
    capture_result = frontend_capture_snapshot(snapshot_path);
    if (capture_result != CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS) {
        (void)DeleteFileW(snapshot_path_wide);
        if (capture_result == CREO_BRL_SNAPSHOT_CAPTURE_RESTORATION_FAILURE) {
            frontend_status("%s", frontend_capture_failure_message(capture_result));
            creo_brl_show_popup(FRONTEND_RESTORATION_WARNING_TITLE,
                                FRONTEND_RESTORATION_WARNING_MESSAGE);
        } else {
            frontend_error("Creo-BRL Capture Failure",
                           frontend_capture_failure_message(capture_result));
        }
        return;
    }

    frontend_status("Writing BRL-CAD geometry...");
    core_result = creo_brl_core_convert_shim(snapshot_path);
    (void)DeleteFileW(snapshot_path_wide);
    if (core_result != CREO_BRL_CORE_CONVERT_SUCCESS) {
        frontend_error("Creo-BRL Conversion Failure",
                       frontend_core_failure_message(core_result));
        return;
    }

    creo_brl_show_status("Status: Conversion complete, (Convert/Quit)?");
    frontend_set_dialog_status("Status: Conversion complete, (Convert/Quit)?");
}


/* Driver routine for converting Creo to BRL-CAD */
int
creo_brl_frontend_dialog_command(uiCmdCmdId UNUSED(command), uiCmdValue *UNUSED(p_value),
                                 void *UNUSED(p_push_cmd_data))
{
    char status[CREO_MSG_MAX] = {'\0'};
    int destroy_dialog = 0;
    int err;

    creo_brl_show_status("Launching Creo to BRL-CAD converter...");

    if (ProUIDialogCreate(CREO_UI_NAME, CREO_UI_NAME) != PRO_TK_NO_ERROR) {
        sprintf_s(status, sizeof(status), "FAILURE: Unable to create dialog box for creo-brl");
        goto print_msg;
    }

    creo_brl_core_load_profile_shim();

    if (ProUICheckbuttonActivateActionSet(CREO_UI_NAME, "elim_small", activate_small_feats, NULL) != PRO_TK_NO_ERROR) {
        sprintf_s(status, sizeof(status), "FAILURE: Unable to set action for \"Ignore minimum sizes\" checkbutton");
        destroy_dialog = 1;
        goto print_msg;
    }

    if (ProUIPushbuttonActivateActionSet(CREO_UI_NAME, "doit", frontend_convert, NULL) != PRO_TK_NO_ERROR) {
        sprintf_s(status, sizeof(status), "FAILURE: Unable to set action for \"Convert\" button");
        destroy_dialog = 1;
        goto print_msg;
    }

    if (ProUIPushbuttonActivateActionSet(CREO_UI_NAME, "quit", do_quit, NULL) != PRO_TK_NO_ERROR) {
        sprintf_s(status, sizeof(status), "FAILURE: Unable to set action for \"Quit\" button");
        destroy_dialog = 1;
        goto print_msg;
    }

    ProUIInputpanelMaxlenSet(CREO_UI_NAME, "out_fname", MAXPATHLEN - 1);
    ProUIInputpanelMaxlenSet(CREO_UI_NAME, "log_fname", MAXPATHLEN - 1);
    ProUIInputpanelMaxlenSet(CREO_UI_NAME, "mtl_fname", MAXPATHLEN - 1);

    ProUIInputpanelMaxlenSet(CREO_UI_NAME, "param_rename", MAXPATHLEN - 1);
    ProUIInputpanelMaxlenSet(CREO_UI_NAME, "param_save", MAXPATHLEN - 1);

    ProMdl model;
    if (ProMdlCurrentGet(&model) != PRO_TK_BAD_CONTEXT) {
        wchar_t wname[CREO_NAME_MAX];
        if (ProMdlMdlnameGet(model, wname) == PRO_TK_NO_ERROR) {
            char name[CREO_NAME_MAX] = {'\0'};
            std::string output_path;
            std::string log_path;
            wchar_t wout[CREO_NAME_MAX];

            ProWstringToString(name, wname);
            if (creo_brl_frontend_default_output_paths(name, &output_path, &log_path)) {
                ProStringToWstring(wout, (char *)output_path.c_str());
                ProUIInputpanelValueSet(CREO_UI_NAME, "out_fname", wout);

                ProStringToWstring(wout, (char *)log_path.c_str());
                ProUIInputpanelValueSet(CREO_UI_NAME, "log_fname", wout);
            }
        }
    }

    if (ProUIDialogActivate(CREO_UI_NAME, &err) != PRO_TK_NO_ERROR)
        sprintf_s(status, sizeof(status), "FAILURE: Unexpected error in creo-brl dialog returned %d", err);

print_msg:
    if (status[0] != '\0')
        frontend_error("Creo-BRL Dialog Failure", status);
    if (destroy_dialog)
        ProUIDialogDestroy(CREO_UI_NAME);

    return 0;
}
