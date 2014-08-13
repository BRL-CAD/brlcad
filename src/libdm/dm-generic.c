/*                    D M - G E N E R I C . C
 * BRL-CAD
 *
 * Copyright (c) 1999-2014 United States Government as represented by
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
#include "dm_private.h"

#include "dm/dm-Null.h"

extern dm *plot_open(Tcl_Interp *interp, int argc, const char *argv[]);
extern dm *ps_open(Tcl_Interp *interp, int argc, const char *argv[]);
extern dm *txt_open(Tcl_Interp *interp, int argc, const char **argv);

#ifdef DM_X
#  if defined(HAVE_TK)
extern dm *X_open_dm();
#  endif
#endif /* DM_X */

#ifdef DM_TK
extern dm *tk_open_dm();
#endif /* DM_TK */

#ifdef DM_OGL
#  if defined(HAVE_TK)
extern dm *ogl_open();
extern void ogl_fogHint();
extern int ogl_share_dlist();
#  endif
#endif /* DM_OGL */

#ifdef DM_OSG
extern dm *osg_open();
extern void osg_fogHint();
extern int osg_share_dlist();
#endif /* DM_OSG*/

#ifdef DM_RTGL
extern dm *rtgl_open();
extern void rtgl_fogHint();
extern int rtgl_share_dlist();
#endif /* DM_RTGL */

#ifdef DM_WGL
extern dm *wgl_open();
extern void wgl_fogHint();
extern int wgl_share_dlist();
#endif /* DM_WGL */

#ifdef DM_QT
extern dm *qt_open();
#endif /* DM_QT */

HIDDEN dm *
null_open(Tcl_Interp *interp, int argc, const char *argv[])
{
    dm *dmp;

    if (argc < 0 || !argv)
	return DM_NULL;

    BU_ALLOC(dmp, struct dm_internal);

    *dmp = dm_null;
    dmp->dm_interp = interp;

    return dmp;
}


dm *
dm_open(Tcl_Interp *interp, int type, int argc, const char *argv[])
{
    switch (type) {
	case DM_TYPE_NULL:
	    return null_open(interp, argc, argv);
	case DM_TYPE_TXT:
	    return txt_open(interp, argc, argv);
	case DM_TYPE_PLOT:
	    return plot_open(interp, argc, argv);
	case DM_TYPE_PS:
	    return ps_open(interp, argc, argv);
#ifdef DM_X
#  if defined(HAVE_TK)
	case DM_TYPE_X:
	    return X_open_dm(interp, argc, argv);
#  endif
#endif
#ifdef DM_TK
	case DM_TYPE_TK:
	    return tk_open_dm(interp, argc, argv);
#endif
#ifdef DM_OGL
#  if defined(HAVE_TK)
	case DM_TYPE_OGL:
	    return ogl_open(interp, argc, argv);
#  endif
#endif
#ifdef DM_OSG
	case DM_TYPE_OSG:
	    return osg_open(interp, argc, argv);
#endif
#ifdef DM_RTGL
	case DM_TYPE_RTGL:
	    return rtgl_open(interp, argc, argv);
#endif
#ifdef DM_WGL
	case DM_TYPE_WGL:
	    return wgl_open(interp, argc, argv);
#endif
#ifdef DM_QT
	case DM_TYPE_QT:
	    return qt_open(interp, argc, argv);
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
dm_share_dlist(dm *dmp1, dm *dmp2)
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
#  if defined(HAVE_TK)
	case DM_TYPE_OGL:
	    return ogl_share_dlist(dmp1, dmp2);
#  endif
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
dm_Xx2Normal(dm *dmp, int x)
{
    return ((x / (fastf_t)dmp->dm_width - 0.5) * 2.0);
}

int
dm_Normal2Xx(dm *dmp, fastf_t f)
{
    return (f * 0.5 + 0.5) * dmp->dm_width;
}

fastf_t
dm_Xy2Normal(dm *dmp, int y, int use_aspect)
{
    if (use_aspect)
	return ((0.5 - y / (fastf_t)dmp->dm_height) / dmp->dm_aspect * 2.0);
    else
	return ((0.5 - y / (fastf_t)dmp->dm_height) * 2.0);
}

int
dm_Normal2Xy(dm *dmp, fastf_t f, int use_aspect)
{
    if (use_aspect)
	return (0.5 - f * 0.5 * dmp->dm_aspect) * dmp->dm_height;
    else
	return (0.5 - f * 0.5) * dmp->dm_height;
}

void
dm_fogHint(dm *dmp, int fastfog)
{
    if (!dmp) {
	bu_log("WARNING: NULL display (fastfog => %d)\n", fastfog);
	return;
    }

    switch (dmp->dm_type) {
#ifdef DM_OGL
#  if defined(HAVE_TK)
	case DM_TYPE_OGL:
	    ogl_fogHint(dmp, fastfog);
	    return;
#  endif
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

int
dm_get_width(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_width;
}

int
dm_get_height(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_height;
}

int
dm_get_aspect(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_aspect;
}

int
dm_close(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_close(dmp);
}

unsigned char *
dm_get_bg(dm *dmp)
{
    if (!dmp) return NULL;
    return dmp->dm_bg;
}

int
dm_set_bg(dm *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    if (!dmp) return 0;
    return dmp->dm_setBGColor(dmp, r, g, b);
}

int
dm_make_current(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_makeCurrent(dmp);
}

vect_t *
dm_get_clipmin(dm *dmp)
{
    if (!dmp) return 0;
    return  &(dmp->dm_clipmin);
}


vect_t *
dm_get_clipmax(dm *dmp)
{
    if (!dmp) return 0;
    return  &(dmp->dm_clipmax);
}

void
dm_set_bound(dm *dmp, fastf_t val)
{
    if (!dmp) return;
    dmp->dm_bound = val;
}

int
dm_set_win_bounds(dm *dmp, fastf_t *w)
{
    if (!dmp) return 0;
    return dmp->dm_setWinBounds(dmp, w);
}

int
dm_configure_win(dm *dmp, int force)
{
    if (!dmp) return 0;
    return dmp->dm_configureWin(dmp, force);
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
