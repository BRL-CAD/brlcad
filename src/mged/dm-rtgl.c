/*                        D M - R T G L . C
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
/** @file mged/dm-rtgl.c
 *
 * Routines specific to MGED's use of LIBDM's Ray Tracing OpenGl display manager.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

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
#include "raytrace.h"
#include "dm/dm_xvars.h"
#include "dm/dm-rtgl.h"
#include "../libdm/dm_private.h"
#include "fb/fb_platform_specific.h"
#include "fb.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"

static int Rtgl_doevent();

/* local sp_hook functions */
static void Rtgl_colorchange(const struct bu_structparse *, const char *, void *, const char *);
static void zclip_hook(const struct bu_structparse *, const char *, void *, const char *);
static void establish_zbuffer(const struct bu_structparse *, const char *, void *, const char *)
static void establish_lighting(const struct bu_structparse *, const char *, void *, const char *);
static void establish_transparency(const struct bu_structparse *, const char *, void *, const char *);
static void do_fogHint(const struct bu_structparse *, const char *, void *, const char *);
static void dirty_hook(const struct bu_structparse *, const char *, void *, const char *);
static void debug_hook(const struct bu_structparse *, const char *, void *, const char *);
static void bound_hook(const struct bu_structparse *, const char *, void *, const char *);
static void boundFlag_hook(const struct bu_structparse *, const char *, void *, const char *);

int
Rtgl_dm_init(struct dm_list *o_dm_list,
	     int argc,
	     char *argv[])
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    dm_var_init(o_dm_list);

    /* register application provided routines */
    cmd_hook = dm_commands;

    Tk_DeleteGenericHandler(doEvent, (ClientData)NULL);

    if ((dmp = dm_open(INTERP, DM_TYPE_RTGL, argc-1, argv)) == DM_NULL)
	return TCL_ERROR;

    /*XXXX this eventually needs to move into Rtgl's private structure */
    dmp->dm_vp = &view_state->vs_gvp->gv_scale;
    dmp->dm_perspective = mged_variables->mv_perspective_mode;

    eventHandler = Rtgl_doevent;
    Tk_CreateGenericHandler(doEvent, (ClientData)NULL);
    (void)dm_configure_win(dmp, 0);

    bu_vls_printf(&vls, "mged_bind_dm %s", bu_vls_addr(&dmp->dm_pathName));
    Tcl_Eval(INTERP, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}

/*
  This routine is being called from doEvent() to handle Expose events.
*/
static int
Rtgl_doevent(ClientData clientData,
	     XEvent *eventPtr)
{
    if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			((struct rtgl_vars *)dmp->dm_vars.priv_vars)->glxc))
	/* allow further processing of this event */
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


static void
Rtgl_colorchange(const struct bu_structparse *UNUSED(sdp),
		 const char *UNUSED(name),
		 void *UNUSED(base),
		 const char *UNUSED(value))
{
    if (((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.cueing_on) {
	glEnable(GL_FOG);
    } else {
	glDisable(GL_FOG);
    }

    view_state->vs_flag = 1;
}


static void
establish_zbuffer(const struct bu_structparse *UNUSED(sdp),
		  const char *UNUSED(name),
		  void *UNUSED(base),
		  const char *UNUSED(value))
{
    (void)dm_set_zbuffer(dmp, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on);
    view_state->vs_flag = 1;
}


static void
establish_lighting(const struct bu_structparse *UNUSED(sdp),
		   const char *UNUSED(name),
		   void *UNUSED(base),
		   const char *UNUSED(value))
{
    (void)dm_set_light(dmp, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.lighting_on);
    view_state->vs_flag = 1;
}


static void
establish_transparency(const struct bu_structparse *UNUSED(sdp),
		       const char *UNUSED(name),
		       void *UNUSED(base),
		       const char *UNUSED(value))
{
    (void)dm_set_transparency(dmp, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.transparency_on);
    view_state->vs_flag = 1;
}


static void
do_fogHint(const struct bu_structparse *UNUSED(sdp),
	   const char *UNUSED(name),
	   void *UNUSED(base),
	   const char *UNUSED(value))
{
    dm_fogHint(dmp, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.fastfog);
    view_state->vs_flag = 1;
}

static void
debug_hook(const struct bu_structparse *UNUSED(sdp),
	   const char *UNUSED(name),
	   void *UNUSED(base),
	   const char *UNUSED(value))
{
    dm_debug(dmp, ((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.debug);
}


static void
bound_hook(const struct bu_structparse *sdp,
	   const char *name,
	   void *base,
	   const char *value)
{
    dmp->dm_bound =
	((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.bound;
    dirty_hook(sdp, name, base, value);
}


static void
boundFlag_hook(const struct bu_structparse *sdp,
	       const char *name,
	       void *base,
	       const char *value)
{
    dmp->dm_boundFlag =
	((struct rtgl_vars *)dmp->dm_vars.priv_vars)->mvars.boundFlag;
    dirty_hook(sdp, name, base, value);
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
