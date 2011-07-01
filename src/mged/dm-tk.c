/*                          D M - T K . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file mged/dm-tk.c
 *
 * Routines specific to MGED's use of LIBDM's Tk display manager.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_TK
#  include "tk.h"
#endif

#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "dm_xvars.h"
#include "dm-tk.h"
#include "fbio.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"

extern int _tk_open_existing();	/* XXX TJM will be defined in libfb/if_tk.c */
extern void dm_var_init(struct dm_list *initial_dm_list);		/* defined in attach.c */

static int tk_dm(int argc, const char *argv[]);
static void dirty_hook(void);
static void zclip_hook(void);

static Tk_GenericProc tk_doevent;

struct bu_structparse tk_vparse[] = {
    {"%f",  1, "bound",		 DM_O(dm_bound),	dirty_hook, NULL, NULL},
    {"%d",  1, "useBound",	 DM_O(dm_boundFlag),	dirty_hook, NULL, NULL},
    {"%d",  1, "zclip",		 DM_O(dm_zclip),	zclip_hook, NULL, NULL},
    {"%d",  1, "debug",		 DM_O(dm_debugLevel),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"",    0, NULL,		 0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};


int
tk_dm_init(struct dm_list *o_dm_list,
	   int argc,
	   const char *argv[])
{
    struct bu_vls vls;

    dm_var_init(o_dm_list);

    /* register application provided routines */
    cmd_hook = tk_dm;

    Tk_DeleteGenericHandler(doEvent, (ClientData)NULL);
    dmp = dm_open(INTERP, DM_TYPE_TK, argc-1, argv);
    if (dmp == DM_NULL)
	return TCL_ERROR;

    /* keep display manager in sync */
    dmp->dm_perspective = mged_variables->mv_perspective_mode;

    eventHandler = tk_doevent;
    Tk_CreateGenericHandler(doEvent, (ClientData)NULL);
    (void)DM_CONFIGURE_WIN(dmp, 0);

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "mged_bind_dm %s", bu_vls_addr(&pathName));
    Tcl_Eval(INTERP, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}


void
tk_fb_open(void)
{
    fbp = (FBIO *)calloc(sizeof(FBIO), 1);
    if (fbp == FBIO_NULL) {
	Tcl_AppendResult(INTERP, "tk_dm_init: failed to allocate framebuffer memory\n",
			 (char *)NULL);
	return;
    }
}


/*
  This routine is being called from doEvent() to handle Expose events.
*/
static int
tk_doevent(ClientData UNUSED(clientData), XEvent *eventPtr)
{
    if (eventPtr->type == Expose && eventPtr->xexpose.count == 0) {
	dirty = 1;

	/* no further processing of this event */
	return TCL_RETURN;
    }

    /* allow further processing of this event */
    return TCL_OK;
}


static int
tk_dm(int argc, const char *argv[])
{
    if (BU_STR_EQUAL(argv[0], "set")) {
	struct bu_vls vls;

	bu_vls_init(&vls);

	if (argc < 2) {
	    /* Bare set command, print out current settings */
	    bu_vls_struct_print2(&vls, "dm_Tk internal variables", tk_vparse, (const char *)dmp);
	} else if (argc == 2) {
	    bu_vls_struct_item_named(&vls, tk_vparse, argv[1], (const char *)dmp, COMMA);
	} else {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "%s=\"", argv[1]);
	    bu_vls_from_argv(&tmp_vls, argc-2, (const char **)argv+2);
	    bu_vls_putc(&tmp_vls, '\"');
	    bu_struct_parse(&tmp_vls, tk_vparse, (char *)dmp);
	    bu_vls_free(&tmp_vls);
	}

	Tcl_AppendResult(INTERP, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    return common_dm(argc, argv);
}


static void
dirty_hook(void)
{
    dirty = 1;
}


static void
zclip_hook(void)
{
    view_state->vs_gvp->gv_zclip = dmp->dm_zclip;
    dirty_hook();
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
