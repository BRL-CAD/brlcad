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

#include "dm-Null.h"

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

dm *
dm_get()
{
    struct dm_internal *new_dm = DM_NULL;
    BU_GET(new_dm, struct dm_internal);
    bu_vls_init(&new_dm->dm_pathName);
    bu_vls_init(&new_dm->dm_dName);

    return new_dm;
}

void
dm_put(dm *dmp)
{
    if (dmp != DM_NULL) {
	bu_vls_free(&dmp->dm_pathName);
	bu_vls_free(&dmp->dm_dName);
	if (dmp->fbp) fb_put(dmp->fbp);
	if (dmp->dm_put_internal)
	    dmp->dm_put_internal(dmp);
	BU_PUT(dmp, struct dm_internal);
    }
}

void
dm_set_null(dm *dmp)
{
    *dmp = dm_null;
}

fb *
dm_get_fb(dm *dmp)
{
    if (!dmp) return NULL;
    if (dmp->fbp == FB_NULL)
	dmp->dm_openFb(dmp);
    return dmp->fbp;
}

const char *
dm_get_dm_name(dm *dmp)
{
    if (!dmp) return NULL;
    return dmp->dm_name;
}

const char *
dm_get_dm_lname(dm *dmp)
{
    if (!dmp) return NULL;
    return dmp->dm_lname;
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
dm_get_type(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_type;
}

int
dm_get_displaylist(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_displaylist;
}

fastf_t
dm_get_aspect(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_aspect;
}

int
dm_get_fontsize(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_fontsize;
}

void
dm_set_fontsize(dm *dmp, int size)
{
    if (!dmp) return;
    dmp->dm_fontsize = size;
}

int
dm_get_light_flag(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_light;
}

void
dm_set_light_flag(dm *dmp, int val)
{
    if (!dmp) return;
    dmp->dm_light = val;
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

unsigned char *
dm_get_fg(dm *dmp)
{
    if (!dmp) return NULL;
    return dmp->dm_fg;
}

int
dm_set_fg(dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    if (!dmp) return 0;
    return dmp->dm_setFGColor(dmp, r, g, b, strict, transparency);
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

int
dm_get_bound_flag(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_boundFlag;
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
dm_get_stereo(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_stereo;
}
int
dm_configure_win(dm *dmp, int force)
{
    if (!dmp) return 0;
    return dmp->dm_configureWin(dmp, force);
}

struct bu_vls * 
dm_get_pathname(dm *dmp)
{
    if (!dmp) return NULL;
    return &(dmp->dm_pathName);
}


struct bu_vls * 
dm_get_dname(dm *dmp)
{
    if (!dmp) return NULL;
    return &(dmp->dm_dName);
}

struct bu_vls * 
dm_get_tkname(dm *dmp)
{
    if (!dmp) return NULL;
    return &(dmp->dm_tkName);
}

unsigned long
dm_get_id(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_id;
}

void
dm_set_id(dm *dmp, unsigned long new_id)
{
    if (!dmp) return;
    dmp->dm_id = new_id;
}



int
dm_set_light(dm *dmp, int light)
{
    if (!dmp) return 0;
    return dmp->dm_setLight(dmp, light);
}

void *
dm_get_public_vars(dm *dmp)
{
    if (!dmp) return NULL;
    return dmp->dm_vars.pub_vars;
}

void *
dm_get_private_vars(dm *dmp)
{
    if (!dmp) return NULL;
    return dmp->dm_vars.priv_vars;
}

int
dm_get_transparency(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_transparency;
}

int
dm_set_transparency(dm *dmp, int transparency)
{
    if (!dmp) return 0;
    return dmp->dm_setTransparency(dmp, transparency);
}

int
dm_get_zbuffer(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_zbuffer;
}

int
dm_set_zbuffer(dm *dmp, int zbuffer)
{
    if (!dmp) return 0;
    return dmp->dm_setZBuffer(dmp, zbuffer);
}

int
dm_get_linewidth(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_lineWidth;
}

void
dm_set_linewidth(dm *dmp, int linewidth)
{
    if (!dmp) return;
    dmp->dm_lineWidth = linewidth;
}

int
dm_get_linestyle(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_lineStyle;
}

void
dm_set_linestyle(dm *dmp, int linestyle)
{
    if (!dmp) return;
    dmp->dm_lineStyle = linestyle;
}

int
dm_set_line_attr(dm *dmp, int width, int style)
{
    if (!dmp) return 0;
    return dmp->dm_setLineAttr(dmp, width, style);
}


int
dm_get_zclip(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_zclip;
}

void
dm_set_zclip(dm *dmp, int zclip)
{
    if (!dmp) return;
    dmp->dm_zclip = zclip;
}

int
dm_get_perspective(dm *dmp)
{
    if (!dmp) return 0;
    return dmp->dm_perspective;
}

void
dm_set_perspective(dm *dmp, fastf_t perspective)
{
    if (!dmp) return;
    dmp->dm_perspective = perspective;
}

int
dm_get_display_image(struct dm_internal *dmp, unsigned char **image)
{
    return dmp->dm_getDisplayImage(dmp, image);
}

int
dm_gen_dlists(struct dm_internal *dmp, size_t range)
{
    return dmp->dm_genDLists(dmp, range);
}

int 
dm_begin_dlist(struct dm_internal *dmp, unsigned int list)
{
    return dmp->dm_beginDList(dmp, list);
}
void
dm_draw_dlist(struct dm_internal *dmp, unsigned int list)
{
    dmp->dm_drawDList(list);
}
int
dm_end_dlist(struct dm_internal *dmp)
{
    return dmp->dm_endDList(dmp);
}
int
dm_free_dlists(struct dm_internal *dmp, unsigned int list, int range)
{
    return dmp->dm_freeDLists(dmp, list, range);
}

int
dm_draw_vlist(struct dm_internal *dmp, struct bn_vlist *vp)
{
    return dmp->dm_drawVList(dmp, vp);
}

int
dm_draw_vlist_hidden_line(struct dm_internal *dmp, struct bn_vlist *vp)
{
    return dmp->dm_drawVListHiddenLine(dmp, vp);
}
int
dm_draw_begin(dm *dmp)
{
    return dmp->dm_drawBegin(dmp);
}
int  
dm_draw_end(dm *dmp) 
{
    return dmp->dm_drawEnd(dmp);
}
int
dm_normal(dm *dmp)
{
    return dmp->dm_normal(dmp);
}
int
dm_loadmatrix(dm *dmp, fastf_t *mat, int eye)
{
    return dmp->dm_loadMatrix(dmp, mat, eye);
}
int
dm_loadpmatrix(dm *dmp, fastf_t *mat)
{
    return dmp->dm_loadPMatrix(dmp, mat);
}
int
dm_draw_string_2d(dm *dmp, const char *str, fastf_t x,  fastf_t y, int size, int use_aspect)
{
    return dmp->dm_drawString2D(dmp, str, x, y, size, use_aspect);
}
int
dm_draw_line_2d(dm *dmp, fastf_t x1, fastf_t y1_2d, fastf_t x2, fastf_t y2)
{
    return dmp->dm_drawLine2D(dmp, x1, y1_2d, x2, y2);
}
int
dm_draw_line_3d(dm *dmp, point_t pt1, point_t pt2)
{
    return dmp->dm_drawLine3D(dmp, pt1, pt2);
}
int
dm_draw_lines_3d(dm *dmp, int npoints, point_t *points, int sflag)
{
    return dmp->dm_drawLines3D(dmp, npoints, points, sflag);
}
int
dm_draw_point_2d(dm *dmp, fastf_t x, fastf_t y)
{
    return dmp->dm_drawPoint2D(dmp, x, y);
}
int
dm_draw_point_3d(dm *dmp, point_t pt)
{
    return dmp->dm_drawPoint3D(dmp, pt);
}
int
dm_draw_points_3d(dm *dmp, int npoints, point_t *points)
{
    return dmp->dm_drawPoints3D(dmp, npoints, points);
}
int
dm_draw(dm *dmp, struct bn_vlist *(*callback)(void *), void **data)
{
    return dmp->dm_draw(dmp, callback, data);
}
int
dm_set_depth_mask(dm *dmp, int d_on)
{
    return dmp->dm_setDepthMask(dmp, d_on);
}
int
dm_debug(dm *dmp, int lvl)
{
    return dmp->dm_debug(dmp, lvl);
}
int
dm_logfile(dm *dmp, const char *filename)
{
    return dmp->dm_logfile(dmp, filename);
}

fastf_t *
dm_get_vp(dm *dmp)
{
    if (!dmp) return NULL;
    return dmp->dm_vp;
}

void
dm_set_vp(dm *dmp, fastf_t *vp)
{
    dmp->dm_vp = vp;
}

/* This is the generic "catch-all" hook that is used
 * to run any user supplied callbacks.  If more side
 * effects are needed at the libdm level, a task-specific
 * hook should be defined using this function as a
 * starting point and adding in whatever additional
 * logic is needed.  The full dm structure should always
 * be accessable as a slot in the modifiable variables
 * structure passed in here as "base" */
void
dm_generic_hook(const struct bu_structparse *sdp,
	const char *name, void *base, const char *value, void *data)
{
    if (data) {
	struct dm_hook_data *hook= (struct dm_hook_data *)data;

	/* Call hook function(if it exists) to carry out the
	 * application requested logic */
	if (hook->dm_hook)
	    hook->dm_hook(sdp, name, base, value, hook->dm_hook_data);
    }
}

int
dm_set_hook(const struct bu_structparse_map *map,
       	const char *key, void *data, struct dm_hook_data *hook)
{
    if (UNLIKELY(!map || !key || !hook)) return -1;
    hook->dm_hook = BU_STRUCTPARSE_FUNC_NULL;
    hook->dm_hook_data = NULL;
    for (; map->sp_name != (char *)0; map++) {
	if (BU_STR_EQUAL(map->sp_name, key)) {
	    hook->dm_hook = map->sp_hook;
	    if (data)
		hook->dm_hook_data = data;
	    return 0;
	}
    }
    return 1;
}

struct bu_structparse *
dm_get_vparse(dm *dmp)
{
    if (!dmp) return NULL;
    return dmp->vparse;
}

void *
dm_get_mvars(dm *dmp)
{
    if (!dmp) return NULL;
    if (!dmp->m_vars) return (void *)dmp;
    return dmp->m_vars;
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
