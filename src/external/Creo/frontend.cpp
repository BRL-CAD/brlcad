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

#include "creo-brl.h"

extern "C" void creo_brl_show_status(const char *message);
extern "C" void creo_brl_core_load_profile_shim(void);
extern "C" void creo_brl_core_doit_shim(char *dialog, char *component, ProAppData appdata);

static void
frontend_status(const char *fmt, ...)
{
    va_list ap;
    char msg[CREO_MSG_MAX] = {'\0'};

    va_start(ap, fmt);
    vsprintf(msg, fmt, ap);
    va_end(ap);

    creo_brl_show_status(msg);
}


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

    creo_brl_core_load_profile_shim();

    if (ProUICheckbuttonActivateActionSet(CREO_UI_NAME, "elim_small", activate_small_feats, NULL) != PRO_TK_NO_ERROR) {
        sprintf_s(status, sizeof(status), "FAILURE: Unable to set action for \"Ignore minimum sizes\" checkbutton");
        goto print_msg;
    }

    if (ProUICheckbuttonActivateActionSet(CREO_UI_NAME, "facets_only", activate_export_stl, NULL) != PRO_TK_NO_ERROR) {
        sprintf_s(status, sizeof(status), "FAILURE: Unable to set action for \"Facetize everything, (no CSG)\" checkbutton");
        goto print_msg;
    }

    if (ProUIPushbuttonActivateActionSet(CREO_UI_NAME, "doit", creo_brl_core_doit_shim, NULL) != PRO_TK_NO_ERROR) {
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
