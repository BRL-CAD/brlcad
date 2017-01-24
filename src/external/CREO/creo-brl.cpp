/*                   C R E O - B R L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2017 United States Government as represented by
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
/** @file creo-brl.cpp
 *
 */

#include "common.h"

#include <set>

#ifndef _WSTUDIO_DEFINED
# define _WSTUDIO_DEFINED
#endif
#include <ProToolkit.h>
#include <ProMessage.h>
#include <ProUIMessage.h>
#include <ProArray.h>
#include <ProMenuBar.h>
#include <ProMessage.h>
#include <ProUtil.h>
#include <ProUICmd.h>
#include <ProUIDialog.h>
#include <ProWindows.h>
#include <PtApplsUnicodeUtils.h>
#include "vmath.h"
#include "bu.h"
#include "bn.h"

struct StrCmp {
    bool operator()(struct bu_vls *str1, struct bu_vls *str2) const {
	return (strcmp(bu_vls_addr(str1), bu_vls_addr(str2)) < 0);
    }
};


struct WStrCmp {
    bool operator()(wchar_t *str1, wchar_t *str2) const {
	return (wcscmp(str1, str2) < 0);
    }
};

ProError ShowMsg()
{
    ProUIMessageButton* button;
    ProUIMessageButton bresult;
    ProArrayAlloc(1, sizeof(ProUIMessageButton), 1, (ProArray*)&button);
    button[0] = PRO_UI_MESSAGE_OK;
    ProUIMessageDialogDisplay(PROUIMESSAGE_INFO, L"Hello World", L"Hello world!", button, PRO_UI_MESSAGE_OK, &bresult);
    ProArrayFree((ProArray*)&button);
    return PRO_TK_NO_ERROR;
}

/* driver routine for converting CREO to BRL-CAD */
extern "C" int
creo_brl( uiCmdCmdId command, uiCmdValue *p_value, void *p_push_cmd_data )
{
    ProFileName msgfil;
    ProError status;
    int ret_status=0;

    ProStringToWstring(msgfil, "usermsg.txt");

    ProMessageDisplay(msgfil, "USER_INFO", "Launching creo_brl...");

#if 0
    /* use UI dialog */
    status = ProUIDialogCreate( "creo_brl", "creo_brl" );
    if ( status != PRO_TK_NO_ERROR ) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "Failed to create dialog box for creo-brl, error = %d\n", status );
	ProMessageDisplay(msgfil, "USER_INFO", bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return 0;
    }
    status = ProUICheckbuttonActivateActionSet( "creo_brl", "elim_small", elim_small_activate, NULL );
    if ( status != PRO_TK_NO_ERROR ) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "Failed to set action for \"eliminate small features\" checkbutton, error = %d\n", status );
	ProMessageDisplay(msgfil, "USER_INFO", bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return 0;
    }

    status = ProUIPushbuttonActivateActionSet( "creo_brl", "doit", doit, NULL );
    if ( status != PRO_TK_NO_ERROR ) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "Failed to set action for 'Go' button\n" );
	ProMessageDisplay(msgfil, "USER_INFO", bu_vls_addr(&vls));
	ProUIDialogDestroy( "creo_brl" );
	bu_vls_free(&vls);
	return 0;
    }

    status = ProUIPushbuttonActivateActionSet( "creo_brl", "quit", do_quit, NULL );
    if ( status != PRO_TK_NO_ERROR ) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "Failed to set action for 'Quit' button\n" );
	ProMessageDisplay(msgfil, "USER_INFO", bu_vls_addr(&vls));
	ProUIDialogDestroy( "creo_brl" );
	bu_vls_free(&vls);
	return 0;
    }
    status = ProUIDialogActivate( "creo_brl", &ret_status );
    if ( status != PRO_TK_NO_ERROR ) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	bu_vls_printf(&vls, "Error in creo-brl Dialog, error = %d\n",
		status );
	bu_vls_printf(&vls, "\t dialog returned %d\n", ret_status );
	ProMessageDisplay(msgfil, "USER_INFO", bu_vls_addr(&vls));
	bu_vls_free(&vls);
    }
#endif

    return 0;
}

/* this routine determines whether the "proe-brl" menu item in Pro/E
 * should be displayed as available or greyed out
 */
extern "C" static uiCmdAccessState
creo_brl_access( uiCmdAccessMode access_mode )
{
    /* doing the correct checks appears to be unreliable */
    return ACCESS_AVAILABLE;
}

extern "C" int user_initialize()
{
    ProError status;
    ProCharLine astr;
    ProFileName msgfil;
    int i;
    uiCmdCmdId cmd_id;
    wchar_t errbuf[80];

    ProStringToWstring(msgfil, "usermsg.txt");

    /* Pro/E says always check the size of w_char */
    status = ProWcharSizeVerify (sizeof (wchar_t), &i);
    if ( status != PRO_TK_NO_ERROR || (i != sizeof (wchar_t)) ) {
	sprintf(astr, "ERROR wchar_t Incorrect size (%d). Should be: %d",
		sizeof(wchar_t), i );
	status = ProMessageDisplay(msgfil, "USER_ERROR", astr);
	printf("%s\n", astr);
	ProStringToWstring(errbuf, astr);
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	return -1;
    }

    /* add a command that calls our creo-brl routine */
    status = ProCmdActionAdd( "CREO-BRL", (uiCmdCmdActFn)creo_brl, uiProe2ndImmediate,
	    creo_brl_access, PRO_B_FALSE, PRO_B_FALSE, &cmd_id );
    if ( status != PRO_TK_NO_ERROR ) {
	sprintf( astr, "Failed to add creo-brl action" );
	fprintf( stderr, "%s\n", astr);
	ProMessageDisplay(msgfil, "USER_ERROR", astr);
	ProStringToWstring(errbuf, astr);
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	return -1;
    }

    /* add a menu item that runs the new command */
    status = ProMenubarmenuPushbuttonAdd( "File", "CREO-BRL", "CREO-BRL", "CREO-BRL-HELP",
	    "File.psh_exit", PRO_B_FALSE, cmd_id, msgfil );
    if ( status != PRO_TK_NO_ERROR ) {
	sprintf( astr, "Failed to add creo-brl menu button" );
	fprintf( stderr, "%s\n", astr);
	ProMessageDisplay(msgfil, "USER_ERROR", astr);
	ProStringToWstring(errbuf, astr);
	(void)ProWindowRefresh( PRO_VALUE_UNUSED );
	return -1;
    }

    ShowMsg();

    /* let user know we are here */
    ProMessageDisplay( msgfil, "OK" );
    (void)ProWindowRefresh( PRO_VALUE_UNUSED );

    return 0;
}

extern "C" void user_terminate()
{
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
