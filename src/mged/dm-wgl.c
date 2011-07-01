/*                        D M - W G L . C
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
#include "dm_xvars.h"
#include "dm-wgl.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"


extern void dm_var_init();		/* defined in attach.c */
extern void cs_set_bg();		/* defined in color_scheme.c */

static int Wgl_dm();
static int Wgl_doevent();
static void Wgl_colorchange();
static void establish_zbuffer();
static void establish_lighting();
static void establish_transparency();
static void dirty_hook();
static void zclip_hook();
static void debug_hook();
static void bound_hook();
static void boundFlag_hook();
static void do_fogHint();

struct bu_structparse Wgl_vparse[] = {
    {"%d",	1, "depthcue",		Wgl_MV_O(cueing_on),	Wgl_colorchange },
    {"%d",  1, "zclip",		Wgl_MV_O(zclipping_on),	zclip_hook },
    {"%d",  1, "zbuffer",		Wgl_MV_O(zbuffer_on),	establish_zbuffer },
    {"%d",  1, "lighting",		Wgl_MV_O(lighting_on),	establish_lighting },
    {"%d",  1, "transparency",	Wgl_MV_O(transparency_on), establish_transparency },
    {"%d",  1, "fastfog",		Wgl_MV_O(fastfog),	do_fogHint },
    {"%f",  1, "density",		Wgl_MV_O(fogdensity),	dirty_hook },
    {"%d",  1, "has_zbuf",		Wgl_MV_O(zbuf),		BU_STRUCTPARSE_FUNC_NULL },
    {"%d",  1, "has_rgb",		Wgl_MV_O(rgb),		BU_STRUCTPARSE_FUNC_NULL },
    {"%d",  1, "has_doublebuffer",	Wgl_MV_O(doublebuffer), BU_STRUCTPARSE_FUNC_NULL },
    {"%d",  1, "depth",		Wgl_MV_O(depth),	BU_STRUCTPARSE_FUNC_NULL },
    {"%d",  1, "debug",		Wgl_MV_O(debug),	debug_hook },
    {"%f",  1, "bound",		Wgl_MV_O(bound),	bound_hook },
    {"%d",  1, "useBound",		Wgl_MV_O(boundFlag),	boundFlag_hook },
    {"",	0,  (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL }
};


int
Wgl_dm_init(struct dm_list *o_dm_list,
	    int argc,
	    char *argv[])
{
    struct bu_vls vls;

    dm_var_init(o_dm_list);

    /* register application provided routines */
    cmd_hook = Wgl_dm;

    Tk_DeleteGenericHandler(doEvent, (ClientData)NULL);

    if ((dmp = dm_open(INTERP, DM_TYPE_WGL, argc-1, argv)) == DM_NULL)
	return TCL_ERROR;

    /*XXXX this eventually needs to move into Wgl's private structure */
    dmp->dm_vp = &view_state->vs_gvp->gv_scale;
    dmp->dm_perspective = mged_variables->mv_perspective_mode;

    eventHandler = Wgl_doevent;
    Tk_CreateGenericHandler(doEvent, (ClientData)NULL);
    (void)DM_CONFIGURE_WIN(dmp, 0);

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "mged_bind_dm %s", bu_vls_addr(&pathName));
    Tcl_Eval(INTERP, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}


void
Wgl_fb_open()
{
    char *wgl_name = "/dev/wgl";

    if ((fbp = (FBIO *)calloc(sizeof(FBIO), 1)) == FBIO_NULL) {
	Tcl_AppendResult(INTERP, "Wgl_fb_open: failed to allocate framebuffer memory\n",
			 (char *)NULL);
	return;
    }

    *fbp = wgl_interface; /* struct copy */

    fbp->if_name = bu_malloc((unsigned)strlen(wgl_name)+1, "if_name");
    bu_strlcpy(fbp->if_name, wgl_name, strlen(wgl_name)+1);

    /* Mark OK by filling in magic number */
    fbp->if_magic = FB_MAGIC;
    _wgl_open_existing(fbp,
		       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
		       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap,
		       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->vip,
		       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->hdc,
		       dmp->dm_width, dmp->dm_height,
		       ((struct wgl_vars *)dmp->dm_vars.priv_vars)->glxc,
		       ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer, 0);
}


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


/*
 * W G L _ D M
 *
 * Implement display-manager specific commands, from MGED "dm" command.
 */
static int
Wgl_dm(int argc,
       const char *argv[])
{
    if (BU_STR_EQUAL(argv[0], "set")) {
	struct bu_vls vls;

	bu_vls_init(&vls);

	if (argc < 2) {
	    /* Bare set command, print out current settings */
	    bu_vls_struct_print2(&vls,
				 "dm_wgl internal variables",
				 Wgl_vparse,
				 (const char *)&((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars);
	} else if (argc == 2) {
	    bu_vls_struct_item_named(&vls,
				     Wgl_vparse,
				     argv[1],
				     (const char *)&((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars,
				     COMMA);
	} else {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "%s=\"", argv[1]);
	    bu_vls_from_argv(&tmp_vls, argc-2, (const char **)argv+2);
	    bu_vls_putc(&tmp_vls, '\"');
	    bu_struct_parse(&tmp_vls,
			    Wgl_vparse,
			    (char *)&((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars);
	    bu_vls_free(&tmp_vls);
	}

	Tcl_AppendResult(INTERP, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    return common_dm(argc, argv);
}


static void
Wgl_colorchange()
{
    if (((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.cueing_on) {
	glEnable(GL_FOG);
    } else {
	glDisable(GL_FOG);
    }

    view_state->vs_flag = 1;
}


static void
establish_zbuffer()
{
    (void)DM_SET_ZBUFFER(dmp, ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on);
    view_state->vs_flag = 1;
}


static void
establish_lighting()
{
    (void)DM_SET_LIGHT(dmp, ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.lighting_on);
    view_state->vs_flag = 1;
}


static void
establish_transparency()
{
    (void)DM_SET_TRANSPARENCY(dmp, ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.transparency_on);
    view_state->vs_flag = 1;
}


static void
do_fogHint()
{
    dm_fogHint(dmp, ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.fastfog);
    view_state->vs_flag = 1;
}


static void
dirty_hook()
{
    dirty = 1;
}


static void
zclip_hook()
{
    dmp->dm_zclip = ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.zclipping_on;
    view_state->vs_gvp->gv_zclip = dmp->dm_zclip;
    dirty_hook();
}


static void
debug_hook()
{
    DM_DEBUG(dmp, ((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.debug);
}


static void
bound_hook()
{
    dmp->dm_bound =
	((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.bound;
    dirty_hook();
}


static void
boundFlag_hook()
{
    dmp->dm_boundFlag =
	((struct wgl_vars *)dmp->dm_vars.priv_vars)->mvars.boundFlag;
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
