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

#include "./mged.h"
#include "./sedit.h"
#include "./mged_dm.h"

extern void dm_var_init(struct dm_list *initial_dm_list);		/* defined in attach.c */

static void
dirty_hook(const struct bu_structparse *UNUSED(sdp),
	   const char *UNUSED(name),
	   void *UNUSED(base),
	   const char *UNUSED(value))
{
    dirty = 1;
}


static void
zclip_hook(const struct bu_structparse *sdp,
	   const char *name,
	   void *base,
	   const char *value)
{
    fastf_t bounds[6] = { GED_MIN, GED_MAX, GED_MIN, GED_MAX, GED_MIN, GED_MAX };

    view_state->vs_gvp->gv_zclip = dmp->dm_zclip;
    dirty_hook(sdp, name, base, value);

    if (dmp->dm_zclip) {
	bounds[4] = -1.0;
	bounds[5] = 1.0;
    }

    DM_SET_WIN_BOUNDS(dmp, bounds);
}

#if 0
static void
logfile_hook(const struct bu_structparse *UNUSED(sdp),
	const char *UNUSED(name),
	void *UNUSED(base),
	const char *UNUSED(value))
{
    DM_LOGFILE(dmp, bu_vls_addr(&((struct osg_vars *)dmp->dm_vars.priv_vars)->mvars.log));
}
#endif

static Tk_GenericProc Osg_doevent;

struct bu_structparse osg_vparse[] = {
    {"%g",  1, "bound",		 DM_O(dm_bound),	dirty_hook, NULL, NULL},
    {"%d",  1, "useBound",	 DM_O(dm_boundFlag),	dirty_hook, NULL, NULL},
    {"%d",  1, "zclip",		 DM_O(dm_zclip),	zclip_hook, NULL, NULL},
    {"%d",  1, "debug",		 DM_O(dm_debugLevel),	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
    {"",    0, NULL,		 0,			BU_STRUCTPARSE_FUNC_NULL, NULL, NULL}
};

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

static int
Osg_dm(int argc, const char *argv[])
{
    if (BU_STR_EQUAL(argv[0], "set")) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;

	if (argc < 2) {
	    /* Bare set command, print out current settings */
	    bu_vls_struct_print2(&vls, "dm_osg internal variables", osg_vparse, (const char *)dmp);
	} else if (argc == 2) {
	    bu_vls_struct_item_named(&vls, osg_vparse, argv[1], (const char *)dmp, COMMA);
	} else {
	    struct bu_vls tmp_vls = BU_VLS_INIT_ZERO;
	    int ret;

	    bu_vls_printf(&tmp_vls, "%s=\"", argv[1]);
	    bu_vls_from_argv(&tmp_vls, argc-2, (const char **)argv+2);
	    bu_vls_putc(&tmp_vls, '\"');
	    ret = bu_struct_parse(&tmp_vls, osg_vparse, (char *)dmp);
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
Osg_dm_init(struct dm_list *o_dm_list,
	  int argc,
	  const char *argv[])
{
    struct bu_vls vls = BU_VLS_INIT_ZERO;

    dm_var_init(o_dm_list);

    /* register application provided routines */
    cmd_hook = Osg_dm;

    Tk_DeleteGenericHandler(doEvent, (ClientData)NULL);
    if ((dmp = dm_open(INTERP, DM_TYPE_OSG, argc-1, argv)) == DM_NULL)
	return TCL_ERROR;

    /* keep display manager in sync */
    dmp->dm_perspective = mged_variables->mv_perspective_mode;

    eventHandler = Osg_doevent;
    Tk_CreateGenericHandler(doEvent, (ClientData)NULL);

    (void)DM_CONFIGURE_WIN(dmp, 0);

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
