/*                        D M - O G L . C
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
/** @file mged/dm-ogl.c
 *
 * Routines specific to MGED's use of LIBDM's OpenGl display manager.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

/* glx.h on Mac OS X (and perhaps elsewhere) defines a slew of
 * parameter names that shadow system symbols.  protect the system
 * symbols by redefining the parameters prior to header inclusion.
 */
#define j1 J1
#define y1 Y1
#define read rd
#define index idx
#define access acs
#define remainder rem
#ifdef HAVE_GL_GLX_H
#  include <GL/glx.h>
#endif
#ifdef HAVE_GL_GL_H
#  include <GL/gl.h>
#endif
#undef remainder
#undef access
#undef index
#undef read
#undef y1
#undef j1

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
#include "dm_xvars.h"
#include "dm-ogl.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"


extern void dm_var_init();		/* defined in attach.c */
extern void cs_set_bg();		/* defined in color_scheme.c */

static int Ogl_dm();
static int Ogl_doevent();
static void Ogl_colorchange();
static void establish_zbuffer();
static void establish_lighting();
static void establish_transparency();
static void dirty_hook();
static void zclip_hook();
static void debug_hook();
static void bound_hook();
static void boundFlag_hook();
static void do_fogHint();

struct bu_structparse Ogl_vparse[] = {
    {"%d",  1, "depthcue",		Ogl_MV_O(cueing_on),	Ogl_colorchange, NULL, NULL },
    {"%d",  1, "zclip",		Ogl_MV_O(zclipping_on),	zclip_hook, NULL, NULL },
    {"%d",  1, "zbuffer",		Ogl_MV_O(zbuffer_on),	establish_zbuffer, NULL, NULL },
    {"%d",  1, "lighting",		Ogl_MV_O(lighting_on),	establish_lighting, NULL, NULL },
    {"%d",  1, "transparency",	Ogl_MV_O(transparency_on), establish_transparency, NULL, NULL },
    {"%d",  1, "fastfog",		Ogl_MV_O(fastfog),	do_fogHint, NULL, NULL },
    {"%f",  1, "density",		Ogl_MV_O(fogdensity),	dirty_hook, NULL, NULL },
    {"%d",  1, "has_zbuf",		Ogl_MV_O(zbuf),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",  1, "has_rgb",		Ogl_MV_O(rgb),		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",  1, "has_doublebuffer",	Ogl_MV_O(doublebuffer), BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",  1, "depth",		Ogl_MV_O(depth),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL },
    {"%d",  1, "debug",		Ogl_MV_O(debug),	debug_hook, NULL, NULL },
    {"%f",  1, "bound",		Ogl_MV_O(bound),	bound_hook, NULL, NULL },
    {"%d",  1, "useBound",		Ogl_MV_O(boundFlag),	boundFlag_hook, NULL, NULL },
    {"",	0,  (char *)0,		0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


int
Ogl_dm_init(struct dm_list *o_dm_list,
	    int argc,
	    const char *argv[])
{
    struct bu_vls vls;

    dm_var_init(o_dm_list);

    /* register application provided routines */
    cmd_hook = Ogl_dm;

    Tk_DeleteGenericHandler(doEvent, (ClientData)NULL);

    if ((dmp = dm_open(INTERP, DM_TYPE_OGL, argc-1, argv)) == DM_NULL)
	return TCL_ERROR;

    /*XXXX this eventually needs to move into Ogl's private structure */
    dmp->dm_vp = &view_state->vs_gvp->gv_scale;
    dmp->dm_perspective = mged_variables->mv_perspective_mode;

    eventHandler = Ogl_doevent;
    Tk_CreateGenericHandler(doEvent, (ClientData)NULL);
    (void)DM_CONFIGURE_WIN(dmp, 0);

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "mged_bind_dm %s", bu_vls_addr(&pathName));
    Tcl_Eval(INTERP, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}


void
Ogl_fb_open()
{
    char *ogl_name = "/dev/ogl";

    if ((fbp = (FBIO *)calloc(sizeof(FBIO), 1)) == FBIO_NULL) {
	Tcl_AppendResult(INTERP, "Ogl_fb_open: failed to allocate framebuffer memory\n",
			 (char *)NULL);
	return;
    }

    *fbp = ogl_interface; /* struct copy */

    fbp->if_name = bu_malloc((unsigned)strlen(ogl_name)+1, "if_name");
    bu_strlcpy(fbp->if_name, ogl_name, strlen(ogl_name)+1);

    /* Mark OK by filling in magic number */
    fbp->if_magic = FB_MAGIC;
    _ogl_open_existing(fbp,
		       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
		       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
		       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->cmap,
		       ((struct dm_xvars *)dmp->dm_vars.pub_vars)->vip,
		       dmp->dm_width, dmp->dm_height,
		       ((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc,
		       ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.doublebuffer, 0);
}


/*
  This routine is being called from doEvent() to handle Expose events.
*/
static int
Ogl_doevent(ClientData UNUSED(clientData),
	    XEvent *eventPtr)
{
    if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc))
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


/*
 * O G L _ D M
 *
 * Implement display-manager specific commands, from MGED "dm" command.
 */
static int
Ogl_dm(int argc,
       const char *argv[])
{
    if (BU_STR_EQUAL(argv[0], "set")) {
	struct bu_vls vls;

	bu_vls_init(&vls);

	if (argc < 2) {
	    /* Bare set command, print out current settings */
	    bu_vls_struct_print2(&vls,
				 "dm_ogl internal variables",
				 Ogl_vparse,
				 (const char *)&((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars);
	} else if (argc == 2) {
	    bu_vls_struct_item_named(&vls,
				     Ogl_vparse,
				     argv[1],
				     (const char *)&((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars,
				     COMMA);
	} else {
	    struct bu_vls tmp_vls;

	    bu_vls_init(&tmp_vls);
	    bu_vls_printf(&tmp_vls, "%s=\"", argv[1]);
	    bu_vls_from_argv(&tmp_vls, argc-2, (const char **)argv+2);
	    bu_vls_putc(&tmp_vls, '\"');
	    bu_struct_parse(&tmp_vls,
			    Ogl_vparse,
			    (char *)&((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars);
	    bu_vls_free(&tmp_vls);
	}

	Tcl_AppendResult(INTERP, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    return common_dm(argc, argv);
}


static void
Ogl_colorchange()
{
    if (((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.cueing_on) {
	glEnable(GL_FOG);
    } else {
	glDisable(GL_FOG);
    }

    view_state->vs_flag = 1;
}


static void
establish_zbuffer()
{
    (void)DM_SET_ZBUFFER(dmp, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on);
    view_state->vs_flag = 1;
}


static void
establish_lighting()
{
    (void)DM_SET_LIGHT(dmp, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.lighting_on);
    view_state->vs_flag = 1;
}


static void
establish_transparency()
{
    (void)DM_SET_TRANSPARENCY(dmp, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.transparency_on);
    view_state->vs_flag = 1;
}


static void
do_fogHint()
{
    dm_fogHint(dmp, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.fastfog);
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
    dmp->dm_zclip = ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.zclipping_on;
    view_state->vs_gvp->gv_zclip = dmp->dm_zclip;
    dirty_hook();
}


static void
debug_hook()
{
    DM_DEBUG(dmp, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.debug);
}


static void
bound_hook()
{
    dmp->dm_bound =
	((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.bound;
    dirty_hook();
}


static void
boundFlag_hook()
{
    dmp->dm_boundFlag =
	((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.boundFlag;
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
