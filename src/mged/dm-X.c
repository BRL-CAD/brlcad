/*                          D M - X . C
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
/** @file mged/dm-X.c
 *
 * Routines specific to MGED's use of LIBDM's X display manager.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#ifdef HAVE_TK
#  include "tk.h"
#endif

#include "bu.h"
#include "vmath.h"
#include "mater.h"
#include "raytrace.h"
#include "dm/dm_xvars.h"
#include "fb.h"

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"


struct bu_structparse_map x_vparse_map[] = {
    {"bound",		dirty_hook  		  },
    {"useBound",	dirty_hook  	  	  },
    {"zclip",		zclip_hook		  },
    {(char *)0,		BU_STRUCTPARSE_FUNC_NULL  }
};

/*
  This routine is being called from doEvent() to handle Expose events.
*/
static int
X_doevent(ClientData UNUSED(clientData), XEvent *eventPtr)
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
X_dm(int argc, const char *argv[])
{
    struct dm_hook_data mged_dm_hook;
    if (BU_STR_EQUAL(argv[0], "set")) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	if (argc < 2) {
	    /* Bare set command, print out current settings */
	    bu_vls_struct_print2(&vls, "dm_X internal variables", dm_get_vparse(dmp), (const char *)dmp);
	} else if (argc == 2) {
	    bu_vls_struct_item_named(&vls, dm_get_vparse(dmp), argv[1], (const char *)dmp, COMMA);
	} else {
	    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	    int ret;
	    struct mged_view_hook_state global_hs;
	    void *data = set_hook_data(&global_hs);

	    ret = dm_set_hook(x_vparse_map, argv[1], data, &mged_dm_hook);

	    bu_vls_printf(&tmp_vls, "%s=\"", argv[1]);
	    bu_vls_from_argv(&tmp_vls, argc-2, (const char **)argv+2);
	    bu_vls_putc(&tmp_vls, '\"');
	    ret = bu_struct_parse(&tmp_vls, dm_get_vparse(dmp), (char *)dmp, (void *)(&mged_dm_hook));
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


int
X_dm_init(struct dm_list *o_dm_list,
	  int argc,
	  const char *argv[])
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    dm_var_init(o_dm_list);

    /* register application provided routines */
    cmd_hook = X_dm;

#ifdef HAVE_TK
    Tk_DeleteGenericHandler(doEvent, (ClientData)NULL);
#endif

    if ((dmp = dm_open(INTERP, DM_TYPE_X, argc-1, argv)) == DM_NULL)
	return TCL_ERROR;

    /* keep display manager in sync */
    dm_set_perspective(dmp, mged_variables->mv_perspective_mode);

    eventHandler = X_doevent;

#ifdef HAVE_TK
    Tk_CreateGenericHandler(doEvent, (ClientData)NULL);
#endif

    (void)dm_configure_win(dmp, 0);

    bu_vls_printf(&vls, "mged_bind_dm %s", bu_vls_addr(dm_get_pathname(dmp)));
    Tcl_Eval(INTERP, bu_vls_addr(&vls));
    bu_vls_free(&vls);

    return TCL_OK;
}


void
X_fb_open(void)
{
    fbp = dm_get_fb(dmp);
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
