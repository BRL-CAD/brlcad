/*                        D M - O G L . C
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
#include "dm/dm_xvars.h"
#include "dm/dm-ogl.h"
#include "fb.h"
#include "../libdm/dm_private.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"

extern void dm_var_init();		/* defined in attach.c */

/* external sp_hook functions */
extern void cs_set_bg(const struct bu_structparse *, const char *, void *, const char *, void *); /* defined in color_scheme.c */

static int Ogl_dm();
static int Ogl_doevent();

/* local sp_hook functions */
static void establish_zbuffer(const struct bu_structparse *, const char *, void *, const char *, void *);
static void establish_lighting(const struct bu_structparse *, const char *, void *, const char *, void *);
static void establish_transparency(const struct bu_structparse *, const char *, void *, const char *, void *);
static void do_fogHint(const struct bu_structparse *, const char *, void *, const char *, void *);
static void dirty_hook(const struct bu_structparse *, const char *, void *, const char *, void *);
static void debug_hook(const struct bu_structparse *, const char *, void *, const char *, void *);
static void logfile_hook(const struct bu_structparse *, const char *, void *, const char *, void *);
static void bound_hook(const struct bu_structparse *, const char *, void *, const char *, void *);
static void boundFlag_hook(const struct bu_structparse *, const char *, void *, const char *, void *);

struct mged_view_hook_state {
    dm *hs_dmp;
    struct _view_state *vs;
    int *dirty_global;
};

static void
view_state_hook(const struct bu_structparse *UNUSED(sdp),
		const char *UNUSED(name),
		void *UNUSED(base),
		const char *UNUSED(value),
                void *data)
{
    struct mged_view_hook_state *hs = (struct mged_view_hook_state *)data;
    if (hs->vs)
	hs->vs->vs_flag = 1;
}

static void
dirty_hook(const struct bu_structparse *UNUSED(sdp),
	const char *UNUSED(name),
	void *UNUSED(base),
	const char *UNUSED(value),
	void *data)
{
    struct mged_view_hook_state *hs = (struct mged_view_hook_state *)data;
    *(hs->dirty_global) = 1;
}

static void
zclip_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    struct mged_view_hook_state *hs = (struct mged_view_hook_state *)data;
    hs->vs->vs_gvp->gv_zclip = dm_get_zclip(hs->hs_dmp);
    dirty_hook(sdp, name, base, value, data);
}


struct bu_structparse_map ogl_vparse_map[] = {
    {"depthcue",	view_state_hook		  },
    {"zclip",		zclip_hook		  },
    {"zbuffer",		establish_zbuffer	  },
    {"lighting",	establish_lighting 	  },
    {"transparency",	establish_transparency    },
    {"fastfog",		do_fogHint  		  },
    {"density",		dirty_hook  		  },
    {"debug",		debug_hook  		  },
    {"log",		logfile_hook  		  },
    {"bound",		bound_hook  		  },
    {"useBound",	boundFlag_hook  	  },
    {(char *)0,		BU_STRUCTPARSE_FUNC_NULL  }
};


void *
set_hook_data(struct mged_view_hook_state *hs) {
    hs->hs_dmp = dmp;
    hs->vs = view_state;
    hs->dirty_global = &(dirty);
    return (void *)hs;
}

int
Ogl_dm_init(struct dm_list *o_dm_list,
	    int argc,
	    const char *argv[])
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

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
    (void)dm_configure_win(dmp, 0);

    bu_vls_printf(&vls, "mged_bind_dm %s", bu_vls_addr(&dmp->dm_pathName));
    Tcl_Eval(INTERP, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}


void
Ogl_fb_open()
{
    fbp = dm_get_fb(dmp);
}


/*
  This routine is being called from doEvent() to handle Expose events.
*/
static int
Ogl_doevent(ClientData UNUSED(clientData),
	    XEvent *eventPtr)
{
    if (eventPtr->type == Expose && eventPtr->xexpose.count == 0) {
	if (!glXMakeCurrent(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			    ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			    ((struct ogl_vars *)dmp->dm_vars.priv_vars)->glxc))
	    /* allow further processing of this event */
	    return TCL_OK;

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	dirty = 1;

	/* no further processing of this event */
	return TCL_RETURN;
    }

    /* allow further processing of this event */
    return TCL_OK;
}


/*
 * Implement display-manager specific commands, from MGED "dm" command.
 */
static int
Ogl_dm(int argc,
       const char *argv[])
{
    struct dm_hook_data mged_dm_hook;
    if (BU_STR_EQUAL(argv[0], "set")) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	if (argc < 2) {
	    /* Bare set command, print out current settings */
	    bu_vls_struct_print2(&vls,
				 "dm_ogl internal variables",
				 dm_get_vparse(dmp),
				 (const char *)dm_get_mvars(dmp));
	} else if (argc == 2) {
	    /* TODO - need to add hook func support to this func, since the one in the libdm structparse isn't enough by itself */
	    bu_vls_struct_item_named(&vls,
				     dm_get_vparse(dmp),
				     argv[1],
				     (const char *)dm_get_mvars(dmp),
				     COMMA);
	} else {
	    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	    int ret;
	    struct mged_view_hook_state global_hs;
	    void *data = set_hook_data(&global_hs);

	    ret = dm_set_hook(ogl_vparse_map, argv[1], data, &mged_dm_hook);

	    bu_vls_printf(&tmp_vls, "%s=\"", argv[1]);
	    bu_vls_from_argv(&tmp_vls, argc-2, (const char **)argv+2);
	    bu_vls_putc(&tmp_vls, '\"');
	    ret = bu_struct_parse(&tmp_vls, dm_get_vparse(dmp), (char *)dm_get_mvars(dmp), (void *)(&mged_dm_hook));
	    bu_vls_free(&tmp_vls);
	    if (ret < 0) {
	      bu_vls_free(&vls);
	      return TCL_ERROR;
	    }
	}

	Tcl_AppendResult(INTERP, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
    }

    return common_dm(argc, argv);
}



    static void
establish_zbuffer(const struct bu_structparse *UNUSED(sdp),
	const char *UNUSED(name),
	void *UNUSED(base),
	const char *UNUSED(value),
	void *UNUSED(data))
{
    (void)dm_make_current(dmp);
    (void)dm_set_zbuffer(dmp, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.zbuffer_on);
    view_state->vs_flag = 1;
}


    static void
establish_lighting(const struct bu_structparse *UNUSED(sdp),
	const char *UNUSED(name),
	void *UNUSED(base),
	const char *UNUSED(value),
	void *UNUSED(data))
{
    (void)dm_make_current(dmp);
    (void)dm_set_light(dmp, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.lighting_on);
    view_state->vs_flag = 1;
}


    static void
establish_transparency(const struct bu_structparse *UNUSED(sdp),
	const char *UNUSED(name),
	void *UNUSED(base),
	const char *UNUSED(value),
	void *UNUSED(data))
{
    (void)dm_make_current(dmp);
    (void)dm_set_transparency(dmp, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.transparency_on);
    view_state->vs_flag = 1;
}


    static void
do_fogHint(const struct bu_structparse *UNUSED(sdp),
	const char *UNUSED(name),
	void *UNUSED(base),
	const char *UNUSED(value),
	void *UNUSED(data))
{
    dm_fogHint(dmp, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.fastfog);
    view_state->vs_flag = 1;
}






    static void
debug_hook(const struct bu_structparse *UNUSED(sdp),
	const char *UNUSED(name),
	void *UNUSED(base),
	const char *UNUSED(value),
	void *UNUSED(data))
{
    dm_debug(dmp, ((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.debug);
}

    static void
logfile_hook(const struct bu_structparse *UNUSED(sdp),
	const char *UNUSED(name),
	void *UNUSED(base),
	const char *UNUSED(value),
	void *UNUSED(data))
{
    dm_logfile(dmp, bu_vls_addr(&((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.log));
}

static void
bound_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    dmp->dm_bound =
	((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.bound;
    dirty_hook(sdp, name, base, value, data);
}


    static void
boundFlag_hook(const struct bu_structparse *sdp,
	const char *name,
	void *base,
	const char *value,
	void *data)
{
    dmp->dm_boundFlag =
	((struct ogl_vars *)dmp->dm_vars.priv_vars)->mvars.boundFlag;
    dirty_hook(sdp, name, base, value, data);
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
