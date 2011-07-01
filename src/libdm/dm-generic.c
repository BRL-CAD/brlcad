/*                    D M - G E N E R I C . C
 * BRL-CAD
 *
 * Copyright (c) 1999-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file libdm/dm-generic.c
 *
 * Generic display manager routines.
 *
 */

#include "common.h"
#include "bio.h"

#include <string.h>

#include "tcl.h"

#include "bu.h"
#include "vmath.h"
#include "dm.h"


extern struct dm *plot_open(Tcl_Interp *interp, int argc, const char *argv[]);
extern struct dm *ps_open(Tcl_Interp *interp, int argc, const char *argv[]);

#ifdef DM_X
extern struct dm *X_open_dm();
#endif /* DM_X */

#ifdef DM_TK
extern struct dm *tk_open_dm();
#endif /* DM_TK */

#ifdef DM_OGL
extern struct dm *ogl_open();
extern void ogl_fogHint();
extern int ogl_share_dlist();
#endif /* DM_OGL */

#ifdef DM_RTGL
extern struct dm *rtgl_open();
extern void rtgl_fogHint();
extern int rtgl_share_dlist();
#endif /* DM_RTGL */

#ifdef DM_WGL
extern struct dm *wgl_open();
extern void wgl_fogHint();
extern int wgl_share_dlist();
#endif /* DM_WGL */


HIDDEN struct dm *
Nu_open(Tcl_Interp *interp, int argc, const char *argv[])
{
    if (interp || argc < 0 || !argv)
	return DM_NULL;

    return DM_NULL;
}


struct dm *
dm_open(Tcl_Interp *interp, int type, int argc, const char *argv[])
{
    switch (type) {
	case DM_TYPE_NULL:
	    return Nu_open(interp, argc, argv);
	case DM_TYPE_PLOT:
	    return plot_open(interp, argc, argv);
	case DM_TYPE_PS:
	    return ps_open(interp, argc, argv);
#ifdef DM_X
	case DM_TYPE_X:
	    return X_open_dm(interp, argc, argv);
#endif
#ifdef DM_TK
	case DM_TYPE_TK:
	    return tk_open_dm(interp, argc, argv);
#endif
#ifdef DM_OGL
	case DM_TYPE_OGL:
	    return ogl_open(interp, argc, argv);
#endif
#ifdef DM_RTGL
	case DM_TYPE_RTGL:
	    return rtgl_open(interp, argc, argv);
#endif
#ifdef DM_WGL
	case DM_TYPE_WGL:
	    return wgl_open(interp, argc, argv);
#endif
	default:
	    break;
    }

    return DM_NULL;
}

/*
 * Provides a way to (un)share display lists. If dmp2 is
 * NULL, then dmp1 will no longer share its display lists.
 */
int
dm_share_dlist(struct dm *dmp1, struct dm *dmp2)
{
    if (dmp1 == DM_NULL)
	return TCL_ERROR;

    /*
     * Only display managers of the same type and using the
     * same OGL server are allowed to share display lists.
     *
     * XXX - need a better way to check if using the same OGL server.
     */
    if (dmp2 != DM_NULL)
	if (dmp1->dm_type != dmp2->dm_type ||
	    bu_vls_strcmp(&dmp1->dm_dName, &dmp2->dm_dName))
	    return TCL_ERROR;

    switch (dmp1->dm_type) {
#ifdef DM_OGL
	case DM_TYPE_OGL:
	    return ogl_share_dlist(dmp1, dmp2);
#endif
#ifdef DM_RTGL
	case DM_TYPE_RTGL:
	    return rtgl_share_dlist(dmp1, dmp2);
#endif
#ifdef DM_WGL
	case DM_TYPE_WGL:
	    return wgl_share_dlist(dmp1, dmp2);
#endif
	default:
	    return TCL_ERROR;
    }
}

fastf_t
dm_Xx2Normal(struct dm *dmp, int x)
{
    return ((x / (fastf_t)dmp->dm_width - 0.5) * 2.0);
}

int
dm_Normal2Xx(struct dm *dmp, fastf_t f)
{
    return (f * 0.5 + 0.5) * dmp->dm_width;
}

fastf_t
dm_Xy2Normal(struct dm *dmp, int y, int use_aspect)
{
    if (use_aspect)
	return ((0.5 - y / (fastf_t)dmp->dm_height) / dmp->dm_aspect * 2.0);
    else
	return ((0.5 - y / (fastf_t)dmp->dm_height) * 2.0);
}

int
dm_Normal2Xy(struct dm *dmp, fastf_t f, int use_aspect)
{
    if (use_aspect)
	return (0.5 - f * 0.5 * dmp->dm_aspect) * dmp->dm_height;
    else
	return (0.5 - f * 0.5) * dmp->dm_height;
}

void
dm_fogHint(struct dm *dmp, int fastfog)
{
    if (!dmp) {
	bu_log("WARNING: NULL display (fastfog => %d)\n", fastfog);
	return;
    }

    switch (dmp->dm_type) {
#ifdef DM_OGL
	case DM_TYPE_OGL:
	    ogl_fogHint(dmp, fastfog);
	    return;
#endif
#ifdef DM_RTGL
	case DM_TYPE_RTGL:
	    rtgl_fogHint(dmp, fastfog);
	    return;
#endif
#ifdef DM_WGL
	case DM_TYPE_WGL:
	    wgl_fogHint(dmp, fastfog);
	    return;
#endif
	default:
	    return;
    }
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
