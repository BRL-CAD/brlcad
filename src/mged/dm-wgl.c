/*                        D M - W G L . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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
/** @file mged/dm-wgl.c
 *
 * Routines specific to MGED's use of LIBDM's OpenGl display manager.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "bio.h"

#ifdef HAVE_GL_GLX_H
#  include <GL/glx.h>
#endif
#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif
#ifdef HAVE_GL_DEVICE_H
#  include <gl/device.h>
#endif

#ifdef HAVE_TK
#  include "tk.h"
#endif

#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "ged.h"
#include "dm/dm_xvars.h"
#include "fb.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"

/* external sp_hook functions */

static int Wgl_doevent();

/*
  This routine is being called from doEvent() to handle Expose events.
*/
static int
Wgl_doevent(ClientData clientData,
	    XEvent *eventPtr)
{
    if (!wglMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,
			((struct wgl_vars *)dmp->dm_vars.priv_vars)->glxc))
	    return TCL_OK;

    if (eventPtr->type == Expose && eventPtr->xexpose.count == 0) {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	dirty = 1;

	/* no further processing of this event */
	return TCL_RETURN;
    }

    /* allow further processing of this event */
    return TCL_OK;
}

int
Wgl_dm_init(struct dm_list *o_dm_list,
	    int argc,
	    char *argv[])
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    dm_var_init(o_dm_list);

    /* register application provided routines */
    cmd_hook = dm_commands;

    Tk_DeleteGenericHandler(doEvent, (ClientData)NULL);

    if ((dmp = dm_open(INTERP, DM_TYPE_WGL, argc-1, argv)) == DM_NULL)
	return TCL_ERROR;

    /*XXXX this eventually needs to move into Wgl's private structure */
    dm_set_vp(dmp, &view_state->vs_gvp->gv_scale);
    dm_set_perspective(dmp, mged_variables->mv_perspective_mode);

    eventHandler = Wgl_doevent;
    Tk_CreateGenericHandler(doEvent, (ClientData)NULL);
    (void)dm_configure_win(dmp, 0);

    bu_vls_printf(&vls, "mged_bind_dm %s", bu_vls_addr(dm_get_pathname(dmp)));
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
