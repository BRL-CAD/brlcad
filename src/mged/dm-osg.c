/*                          D M - O S G . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2014 United States Government as represented by
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
/** @file mged/dm-osg.c
 *
 * Routines specific to MGED's use of LIBDM's X display manager.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_TK
#  include "tk.h"
#endif

#include "dm/dm_xvars.h"
#include "dm/dm-osg.h"
#include "../libdm/dm_private.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"

static Tk_GenericProc Osg_doevent;

/*
  This routine is being called from doEvent() to handle Expose events.
*/
static int
Osg_doevent(ClientData UNUSED(clientData), XEvent *eventPtr)
{
    if (eventPtr->type == Expose && eventPtr->xexpose.count == 0) {
	dirty = 1;

	/* no further processing of this event */
	return TCL_RETURN;
    }

    /* allow further processing of this event */
    return TCL_OK;
}

int
Osg_dm_init(struct dm_list *o_dm_list,
	  int argc,
	  const char *argv[])
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    dm_var_init(o_dm_list);

    /* register application provided routines */
    cmd_hook = dm_commands;

    Tk_DeleteGenericHandler(doEvent, (ClientData)NULL);
    if ((dmp = dm_open(INTERP, DM_TYPE_OSG, argc-1, argv)) == DM_NULL)
	return TCL_ERROR;

    /* keep display manager in sync */
    dmp->dm_perspective = mged_variables->mv_perspective_mode;

    eventHandler = Osg_doevent;
    Tk_CreateGenericHandler(doEvent, (ClientData)NULL);

    (void)dm_configure_win(dmp, 0);

    bu_vls_printf(&vls, "mged_bind_dm %s", bu_vls_addr(&dmp->dm_pathName));
    Tcl_Eval(INTERP, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
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
