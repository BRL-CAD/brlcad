/**
 *                 F R O N T E N D . C P P
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
 * @file frontend.cpp
 *
 * Creo-facing dialog/controller routines that must live in the registered
 * facade DLL.
 */

#include "common.h"
#include <stdarg.h>
#include <string.h>
#include <windows.h>

#include "creo-brl.h"
#include "conversion_snapshot.h"
#include "snapshot_writer.h"

extern "C" void creo_brl_show_status(const char *message);
extern "C" void creo_brl_core_load_profile_shim(void);
#if defined(CREO_EXEC_PLUGIN)
extern "C" void creo_brl_core_doit_shim(char *dialog, char *component, ProAppData appdata);
#else
extern "C" int creo_brl_core_convert_shim(const char *snapshot_path);
#endif

#if defined(CREO_EXEC_PLUGIN)
static const bool FRONTEND_LOAD_PROFILE_ON_OPEN = true;
#else
static const bool FRONTEND_LOAD_PROFILE_ON_OPEN = true;
#endif

static const wchar_t FRONTEND_SNAPSHOT_PREFIX[] = L"cbr";
static const size_t FRONTEND_SNAPSHOT_UTF8_PATH_SIZE = MAX_PATH * 4;

static void
frontend_status(const char *fmt, ...)
{
    va_list ap;
    char msg[CREO_MSG_MAX] = {'\0'};

    va_start(ap, fmt);
    vsprintf_s(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    creo_brl_show_status(msg);
}


extern "C" void __cdecl
creo_brl_frontend_apply_control(
    const char *resource,
    const char *type,
    const char *value,
    const char *UNUSED(default_value))
{
    ProError err = PRO_TK_GENERAL_ERROR;

    if (!resource || !type || !value) {
        frontend_status("FAILURE: Unable to apply an invalid profile control value");
        return;
    }

    if (strcmp(type, "STR") == 0) {
        wchar_t text[CREO_NAME_MAX];

        ProStringToWstring(text, (char *)value);
        err = ProUIInputpanelValueSet(CREO_UI_NAME, (char *)resource, text);
    } else if (strcmp(type, "BOX") == 0) {
        if (strcmp(value, "on") == 0)
            err = ProUICheckbuttonSet(CREO_UI_NAME, (char *)resource);
        else
            err = ProUICheckbuttonUnset(CREO_UI_NAME, (char *)resource);
    } else if (strcmp(type, "RAD") == 0) {
        char *selected_name = (char *)value;

        err = ProUIRadiogroupSelectednamesSet(CREO_UI_NAME, (char *)resource, 1, &selected_name);
    } else {
        frontend_status("Unknown control type: %s", type);
        return;
    }

    if (err != PRO_TK_NO_ERROR)
        frontend_status("FAILURE: Unable to apply profile value for %s (%d)", resource, err);
}


static void
frontend_load_profile(void)
{
    creo_brl_core_load_profile_shim();
}


#if !defined(CREO_EXEC_PLUGIN)
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
            return "FAILURE: Only an active Creo part can be converted; assemblies and drawings are not supported yet.";
        case CREO_BRL_SNAPSHOT_CAPTURE_MODEL_FAILURE:
            return "FAILURE: Unable to read the active part name or supported length units.";
        case CREO_BRL_SNAPSHOT_CAPTURE_TESSELLATION_FAILURE:
            return "FAILURE: Unable to tessellate the active part; enable bounding-box fallback or adjust tessellation controls.";
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
        case CREO_BRL_CORE_CONVERT_UNSUPPORTED_SNAPSHOT:
            return "FAILURE: The captured conversion request is unsupported.";
        case CREO_BRL_CORE_CONVERT_NOT_IMPLEMENTED:
            return "FAILURE: The requested conversion path is not implemented.";
        case CREO_BRL_CORE_CONVERT_OUTPUT_FAILED:
            return "FAILURE: The BRL-CAD output file could not be written.";
        default:
            return "FAILURE: The runtime core failed to convert the active part.";
    }
}
#endif


/* Activates small feature settings */
extern "C" void
activate_small_feats(char *dialog_name, char *button_name, ProAppData UNUSED(data))
{
    ProBoolean state;

    if (ProUICheckbuttonGetState(dialog_name, button_name, &state) != PRO_TK_NO_ERROR) {
        frontend_status("FAILURE: Unable to obtain check button state: \"Ignore minimum sizes\"");
        return;
    }

    if (state) {
        if (ProUIInputpanelEditable(dialog_name, "min_hole") != PRO_TK_NO_ERROR) {
            frontend_status("FAILURE: Unable to activate: \"Hole diameter\"");
            return;
        }
        if (ProUIInputpanelEditable(dialog_name, "min_chamfer") != PRO_TK_NO_ERROR) {
            frontend_status("FAILURE: Unable to activate: \"Chamfer dimension\"");
            return;
        }
        if (ProUIInputpanelEditable(dialog_name, "min_round") != PRO_TK_NO_ERROR) {
            frontend_status("FAILURE: Unable to activate: \"Blend radius\"");
            return;
        }
    } else {
        if (ProUIInputpanelReadOnly(dialog_name, "min_hole") != PRO_TK_NO_ERROR) {
            frontend_status("FAILURE: Unable to de-activate: \"Hole diameter\"");
            return;
        }
        if (ProUIInputpanelReadOnly(dialog_name, "min_chamfer") != PRO_TK_NO_ERROR) {
            frontend_status("FAILURE: Unable to de-activate: \"Chamfer dimension\"");
            return;
        }
        if (ProUIInputpanelReadOnly(dialog_name, "min_round") != PRO_TK_NO_ERROR) {
            frontend_status("FAILURE: Unable to de-activate: \"Blend radius\"");
            return;
        }
    }
}


/* Activates export STL setting */
extern "C" void
activate_export_stl(char *dialog_name, char *button_name, ProAppData UNUSED(data))
{
    ProBoolean state;

    if (ProUICheckbuttonGetState(dialog_name, button_name, &state) != PRO_TK_NO_ERROR) {
        frontend_status("FAILURE: Unable to obtain check button state: \"Facetize everything, (no CSG)\"");
        return;
    }

    if (state) {
        if (ProUICheckbuttonEnable(dialog_name, "export_stl") != PRO_TK_NO_ERROR) {
            frontend_status("FAILURE: Unable to activate: \"Export facets to STL file\"");
            return;
        }
    } else {
        if (ProUICheckbuttonDisable(dialog_name, "export_stl") != PRO_TK_NO_ERROR) {
            frontend_status("FAILURE: Unable to de-activate: \"Export facets to STL file\"");
            return;
        }
    }
}


/* Exit the converter dialog */
extern "C" void
do_quit(char *UNUSED(dialog), char *UNUSED(compnent), ProAppData UNUSED(appdata))
{
    ProUIDialogDestroy(CREO_UI_NAME);
}


static void
frontend_convert(char *dialog, char *component, ProAppData appdata)
{
#if defined(CREO_EXEC_PLUGIN)
    creo_brl_core_doit_shim(dialog, component, appdata);
#else
    wchar_t snapshot_path_wide[MAX_PATH] = {L'\0'};
    char snapshot_path[FRONTEND_SNAPSHOT_UTF8_PATH_SIZE] = {'\0'};
    int capture_result = CREO_BRL_SNAPSHOT_CAPTURE_INVALID_REQUEST;
    int core_result = CREO_BRL_CORE_CONVERT_INVALID_REQUEST;

    (void)dialog;
    (void)component;
    (void)appdata;

    if (!frontend_snapshot_path(snapshot_path_wide, _countof(snapshot_path_wide),
                                snapshot_path, sizeof(snapshot_path))) {
        frontend_status("FAILURE: Unable to create a temporary conversion request path.");
        return;
    }

    frontend_status("Capturing active Creo part for BRL-CAD conversion...");
    capture_result = creo_brl_frontend_capture_single_part_snapshot(snapshot_path);
    if (capture_result != CREO_BRL_SNAPSHOT_CAPTURE_SUCCESS) {
        (void)DeleteFileW(snapshot_path_wide);
        frontend_status(frontend_capture_failure_message(capture_result));
        return;
    }

    frontend_status("Writing BRL-CAD geometry...");
    core_result = creo_brl_core_convert_shim(snapshot_path);
    (void)DeleteFileW(snapshot_path_wide);
    if (core_result != CREO_BRL_CORE_CONVERT_SUCCESS) {
        frontend_status(frontend_core_failure_message(core_result));
        return;
    }

    frontend_status("Creo part conversion completed.");
#endif
}


/* Driver routine for converting Creo to BRL-CAD */
extern "C" int
creo_brl_frontend_command(uiCmdCmdId UNUSED(command), uiCmdValue *UNUSED(p_value), void *UNUSED(p_push_cmd_data))
{
    char status[CREO_MSG_MAX] = {'\0'};
    int destroy_dialog = 0;
    int err;

    frontend_status("Launching Creo to BRL-CAD converter...");

    if (ProUIDialogCreate(CREO_UI_NAME, CREO_UI_NAME) != PRO_TK_NO_ERROR) {
        sprintf_s(status, sizeof(status), "FAILURE: Unable to create dialog box for creo-brl");
        goto print_msg;
    }

    if (FRONTEND_LOAD_PROFILE_ON_OPEN)
        frontend_load_profile();

    if (ProUICheckbuttonActivateActionSet(CREO_UI_NAME, "elim_small", activate_small_feats, NULL) != PRO_TK_NO_ERROR) {
        sprintf_s(status, sizeof(status), "FAILURE: Unable to set action for \"Ignore minimum sizes\" checkbutton");
        goto print_msg;
    }

    if (ProUICheckbuttonActivateActionSet(CREO_UI_NAME, "facets_only", activate_export_stl, NULL) != PRO_TK_NO_ERROR) {
        sprintf_s(status, sizeof(status), "FAILURE: Unable to set action for \"Facetize everything, (no CSG)\" checkbutton");
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
            char gout[CREO_NAME_MAX] = {'\0'};
            char lout[CREO_NAME_MAX] = {'\0'};
            wchar_t wout[CREO_NAME_MAX];
            char *dot = NULL;

            ProWstringToString(name, wname);
            CharLowerBuffA(name, (DWORD)strlen(name));
            dot = strrchr(name, '.');
            if (dot)
                *dot = '\0';

            sprintf_s(gout, sizeof(gout), "%s.g", name);
            ProStringToWstring(wout, gout);
            ProUIInputpanelValueSet(CREO_UI_NAME, "out_fname", wout);

            sprintf_s(lout, sizeof(lout), "%s_log.txt", name);
            ProStringToWstring(wout, lout);
            ProUIInputpanelValueSet(CREO_UI_NAME, "log_fname", wout);
        }
    }

    if (ProUIDialogActivate(CREO_UI_NAME, &err) != PRO_TK_NO_ERROR)
        sprintf_s(status, sizeof(status), "FAILURE: Unexpected error in creo-brl dialog returned %d", err);

print_msg:
    if (status[0] != '\0')
        creo_brl_show_status(status);
    if (destroy_dialog)
        ProUIDialogDestroy(CREO_UI_NAME);

    return 0;
}


/**
 * This routine determines whether the "creo-brl" menu item in Creo
 * should be displayed as available or greyed out
 */
extern "C" uiCmdAccessState
creo_brl_frontend_access(uiCmdAccessMode UNUSED(access_mode))
{
    return ACCESS_AVAILABLE;
}
