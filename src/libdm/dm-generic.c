/*                    D M - G E N E R I C . C
 * BRL-CAD
 *
 * Copyright (c) 1999-2020 United States Government as represented by
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

#include <string.h>

#ifdef DM_X
#  include "dm/dm_xvars.h"
#  include <X11/Xutil.h>
#endif

#include "png.h"
#include "tcl.h"

#include "vmath.h"
#include "dm.h"
#include "dm_private.h"

#include "dm-Null.h"
#include "rt/solid.h"

/* TODO: this doesn't belong in here, move to a globals.c or eliminate */
int vectorThreshold = 100000;

extern dm *plot_open(Tcl_Interp *interp, int argc, const char *argv[]);
extern dm *ps_open(Tcl_Interp *interp, int argc, const char *argv[]);
extern dm *txt_open(Tcl_Interp *interp, int argc, const char **argv);

#ifdef DM_X
#  if defined(HAVE_TK)
extern dm *X_open_dm(Tcl_Interp *interp, int argc, const char **argv);
#  endif
#endif /* DM_X */

#ifdef DM_TK
extern dm *tk_open_dm(Tcl_Interp *interp, int argc, const char **argv);
#endif /* DM_TK */

#ifdef DM_OGL
#  if defined(HAVE_TK)
extern dm *ogl_open(Tcl_Interp *interp, int argc, const char **argv);
extern void ogl_fogHint(dm *dmp, int fastfog);
extern int ogl_share_dlist(dm *dmp1, dm *dmp2);
#  endif
#endif /* DM_OGL */

#ifdef DM_OSG
extern dm *osg_open(Tcl_Interp *interp, int argc, const char **argv);
extern void osg_fogHint(dm *dmp, int fastfog);
extern int osg_share_dlist(dm *dmp1, dm *dmp2);
#endif /* DM_OSG*/

#ifdef DM_OSGL
extern dm *osgl_open(Tcl_Interp *interp, int argc, const char **argv);
extern void osgl_fogHint(dm *dmp, int fastfog);
extern int osgl_share_dlist(dm *dmp1, dm *dmp2);
#endif /* DM_OSGL*/

#ifdef DM_RTGL
extern dm *rtgl_open(Tcl_Interp *interp, int argc, const char **argv);
extern void rtgl_fogHint(dm *dmp, int fastfog);
extern int rtgl_share_dlist(dm *dmp1, dm *dmp2);
#endif /* DM_RTGL */

#ifdef DM_WGL
extern dm *wgl_open(Tcl_Interp *interp, int argc, const char **argv);
extern void wgl_fogHint(dm *dmp, int fastfog);
extern int wgl_share_dlist(dm *dmp1, dm *dmp2);
#endif /* DM_WGL */

#ifdef DM_QT
extern dm *qt_open(Tcl_Interp *interp, int argc, const char **argv);
#endif /* DM_QT */

HIDDEN dm *
null_dm_open(Tcl_Interp *interp, int argc, const char *argv[])
{
    dm *dmp = DM_NULL;

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
	    return null_dm_open(interp, argc, argv);
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
#ifdef DM_OSGL
	case DM_TYPE_OSGL:
	    return osgl_open(interp, argc, argv);
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

void *
dm_interp(dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return (void *)dmp->dm_interp;
}

/*
 * Provides a way to (un)share display lists. If dmp2 is
 * NULL, then dmp1 will no longer share its display lists.
 */
int
dm_share_dlist(dm *dmp1, dm *dmp2)
{
    if (UNLIKELY(!dmp1) || UNLIKELY(!dmp2)) return BRLCAD_ERROR;

    /*
     * Only display managers of the same type and using the
     * same OGL server are allowed to share display lists.
     *
     * XXX - need a better way to check if using the same OGL server.
     */
    if (dmp2 != DM_NULL)
	if (dmp1->dm_type != dmp2->dm_type ||
	    bu_vls_strcmp(&dmp1->dm_dName, &dmp2->dm_dName))
	    return BRLCAD_ERROR;

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
	    return BRLCAD_ERROR;
    }
}

void
dm_flush(dm *dmp)
{
    if (UNLIKELY(!dmp)) return;
#ifdef DM_X
    XSync(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy, 0);
#endif
}

void *
dm_os_window(dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
#if defined(DM_X) || defined(DM_OGL) || defined(DM_OGL) || defined(DM_WGL)
    return (void *)&(((struct dm_xvars *)dmp->dm_vars.pub_vars)->xtkwin);
#endif
    return NULL;
}


fastf_t
dm_Xx2Normal(dm *dmp, int x)
{
    if (UNLIKELY(!dmp)) return 0.0;
    return ((x / (fastf_t)dmp->dm_width - 0.5) * 2.0);
}

int
dm_Normal2Xx(dm *dmp, fastf_t f)
{
    if (UNLIKELY(!dmp)) return 0.0;
    return (f * 0.5 + 0.5) * dmp->dm_width;
}

fastf_t
dm_Xy2Normal(dm *dmp, int y, int use_aspect)
{
    if (UNLIKELY(!dmp)) return 0.0;
    if (use_aspect)
	return ((0.5 - y / (fastf_t)dmp->dm_height) / dmp->dm_aspect * 2.0);
    else
	return ((0.5 - y / (fastf_t)dmp->dm_height) * 2.0);
}

int
dm_Normal2Xy(dm *dmp, fastf_t f, int use_aspect)
{
    if (UNLIKELY(!dmp)) return 0.0;
    if (use_aspect)
	return (0.5 - f * 0.5 * dmp->dm_aspect) * dmp->dm_height;
    else
	return (0.5 - f * 0.5) * dmp->dm_height;
}

void
dm_fogHint(dm *dmp, int fastfog)
{
    if (UNLIKELY(!dmp)) {
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
    if (dmp && dmp != DM_NULL) {
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
    if (UNLIKELY(!dmp)) return;
    *dmp = dm_null;
}

fb *
dm_get_fb(dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    if (dmp->fbp == FB_NULL)
	dmp->dm_openFb(dmp);
    return dmp->fbp;
}

void *
dm_get_xvars(dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return (void *)(dmp->dm_vars.pub_vars);
}

const char *
dm_get_dm_name(dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return dmp->dm_name;
}

const char *
dm_get_dm_lname(dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return dmp->dm_lname;
}

int
dm_get_width(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_width;
}

int
dm_get_height(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_height;
}

void
dm_set_width(dm *dmp, int width)
{
    if (UNLIKELY(!dmp)) return;
    dmp->dm_width = width;
}

void
dm_set_height(dm *dmp, int height)
{
    if (UNLIKELY(!dmp)) return;
    dmp->dm_height = height;
}


int
dm_get_type(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_type;
}

int
dm_get_displaylist(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_displaylist;
}

fastf_t
dm_get_aspect(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_aspect;
}

int
dm_get_fontsize(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_fontsize;
}

void
dm_set_fontsize(dm *dmp, int size)
{
    if (UNLIKELY(!dmp)) return;
    dmp->dm_fontsize = size;
}

int
dm_get_light_flag(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_light;
}

void
dm_set_light_flag(dm *dmp, int val)
{
    if (UNLIKELY(!dmp)) return;
    dmp->dm_light = val;
}

int
dm_close(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_close(dmp);
}

unsigned char *
dm_get_bg(dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return dmp->dm_bg;
}

int
dm_set_bg(dm *dmp, unsigned char r, unsigned char g, unsigned char b)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_setBGColor(dmp, r, g, b);
}

unsigned char *
dm_get_fg(dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return dmp->dm_fg;
}

int
dm_set_fg(dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict, fastf_t transparency)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_setFGColor(dmp, r, g, b, strict, transparency);
}

int
dm_reshape(dm *dmp, int width, int height)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_reshape(dmp, width, height);
}

int
dm_make_current(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_makeCurrent(dmp);
}

vect_t *
dm_get_clipmin(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return  &(dmp->dm_clipmin);
}

void
dm_set_clipmin(dm *dmp, vect_t clipmin)
{
    if (UNLIKELY(!dmp)) return;
    VMOVE(dmp->dm_clipmin, clipmin);
}

vect_t *
dm_get_clipmax(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return  &(dmp->dm_clipmax);
}

void
dm_set_clipmax(dm *dmp, vect_t clipmax)
{
    if (UNLIKELY(!dmp)) return;
    VMOVE(dmp->dm_clipmax, clipmax);
}

int
dm_get_bound_flag(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_boundFlag;
}

void
dm_set_bound(dm *dmp, fastf_t val)
{
    if (UNLIKELY(!dmp)) return;
    dmp->dm_bound = val;
}

int
dm_set_win_bounds(dm *dmp, fastf_t *w)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_setWinBounds(dmp, w);
}

int
dm_get_stereo(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_stereo;
}
int
dm_configure_win(dm *dmp, int force)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_configureWin(dmp, force);
}

struct bu_vls *
dm_get_pathname(dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return &(dmp->dm_pathName);
}


struct bu_vls *
dm_get_dname(dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return &(dmp->dm_dName);
}

struct bu_vls *
dm_get_tkname(dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return &(dmp->dm_tkName);
}

unsigned long
dm_get_id(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_id;
}

void
dm_set_id(dm *dmp, unsigned long new_id)
{
    if (UNLIKELY(!dmp)) return;
    dmp->dm_id = new_id;
}

int
dm_set_light(dm *dmp, int light)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_setLight(dmp, light);
}

void *
dm_get_public_vars(dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return dmp->dm_vars.pub_vars;
}

void *
dm_get_private_vars(dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return dmp->dm_vars.priv_vars;
}

int
dm_get_transparency(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_transparency;
}

int
dm_set_transparency(dm *dmp, int transparency)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_setTransparency(dmp, transparency);
}

int
dm_get_zbuffer(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_zbuffer;
}

int
dm_set_zbuffer(dm *dmp, int zbuffer)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_setZBuffer(dmp, zbuffer);
}

int
dm_get_linewidth(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_lineWidth;
}

void
dm_set_linewidth(dm *dmp, int linewidth)
{
    if (UNLIKELY(!dmp)) return;
    dmp->dm_lineWidth = linewidth;
}

int
dm_get_linestyle(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_lineStyle;
}

void
dm_set_linestyle(dm *dmp, int linestyle)
{
    if (UNLIKELY(!dmp)) return;
    dmp->dm_lineStyle = linestyle;
}

int
dm_set_line_attr(dm *dmp, int width, int style)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_setLineAttr(dmp, width, style);
}


int
dm_get_zclip(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_zclip;
}

void
dm_set_zclip(dm *dmp, int zclip)
{
    if (UNLIKELY(!dmp)) return;
    dmp->dm_zclip = zclip;
}

int
dm_get_perspective(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_perspective;
}

void
dm_set_perspective(dm *dmp, fastf_t perspective)
{
    if (UNLIKELY(!dmp)) return;
    dmp->dm_perspective = perspective;
}

int
dm_get_display_image(struct dm_internal *dmp, unsigned char **image)
{
    if (!dmp || !image) return 0;
    return dmp->dm_getDisplayImage(dmp, image);
}

int
dm_gen_dlists(struct dm_internal *dmp, size_t range)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_genDLists(dmp, range);
}

int
dm_begin_dlist(struct dm_internal *dmp, unsigned int list)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_beginDList(dmp, list);
}
int
dm_draw_dlist(struct dm_internal *dmp, unsigned int list)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_drawDList(list);
}
int
dm_end_dlist(struct dm_internal *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_endDList(dmp);
}
int
dm_free_dlists(struct dm_internal *dmp, unsigned int list, int range)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_freeDLists(dmp, list, range);
}

int
dm_draw_vlist(struct dm_internal *dmp, struct bn_vlist *vp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_drawVList(dmp, vp);
}

int
dm_draw_vlist_hidden_line(struct dm_internal *dmp, struct bn_vlist *vp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_drawVListHiddenLine(dmp, vp);
}
int
dm_draw_begin(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_drawBegin(dmp);
}
int
dm_draw_end(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_drawEnd(dmp);
}
int
dm_normal(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_normal(dmp);
}
int
dm_loadmatrix(dm *dmp, fastf_t *mat, int eye)
{
    if (!dmp || !mat) return 0;
    return dmp->dm_loadMatrix(dmp, mat, eye);
}
int
dm_loadpmatrix(dm *dmp, fastf_t *mat)
{
    if (!dmp || !mat) return 0;
    return dmp->dm_loadPMatrix(dmp, mat);
}
int
dm_draw_string_2d(dm *dmp, const char *str, fastf_t x,  fastf_t y, int size, int use_aspect)
{
    if (!dmp || !str) return 0;
    return dmp->dm_drawString2D(dmp, str, x, y, size, use_aspect);
}
int
dm_draw_line_2d(dm *dmp, fastf_t x1, fastf_t y1_2d, fastf_t x2, fastf_t y2)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_drawLine2D(dmp, x1, y1_2d, x2, y2);
}
int
dm_draw_line_3d(dm *dmp, point_t pt1, point_t pt2)
{
    if (UNLIKELY(!dmp)) return 0;
    if (!!pt1 || !pt2) return 0;
    return dmp->dm_drawLine3D(dmp, pt1, pt2);
}
int
dm_draw_lines_3d(dm *dmp, int npoints, point_t *points, int sflag)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_drawLines3D(dmp, npoints, points, sflag);
}
int
dm_draw_point_2d(dm *dmp, fastf_t x, fastf_t y)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_drawPoint2D(dmp, x, y);
}
int
dm_draw_point_3d(dm *dmp, point_t pt)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_drawPoint3D(dmp, pt);
}
int
dm_draw_points_3d(dm *dmp, int npoints, point_t *points)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_drawPoints3D(dmp, npoints, points);
}
int
dm_draw(dm *dmp, struct bn_vlist *(*callback)(void *), void **data)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_draw(dmp, callback, data);
}
int
dm_draw_obj(dm *dmp, struct display_list *obj)
{
    if (!dmp || !obj) return 0;
    return dmp->dm_draw_obj(dmp, obj);
}
int
dm_get_depth_mask(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_depthMask;
}
int
dm_set_depth_mask(dm *dmp, int d_on)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_setDepthMask(dmp, d_on);
}
int
dm_get_debug(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_debugLevel;
}

int
dm_debug(dm *dmp, int lvl)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_debug(dmp, lvl);
}

const char *
dm_get_logfile(dm *dmp)
{
    if (!dmp) return NULL;
    return bu_vls_cstr(&(dmp->dm_log));
}

int
dm_logfile(dm *dmp, const char *filename)
{
    if (!dmp || !filename) return 0;
    return dmp->dm_logfile(dmp, filename);
}

fastf_t *
dm_get_vp(dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return dmp->dm_vp;
}

void
dm_set_vp(dm *dmp, fastf_t *vp)
{
    if (UNLIKELY(!dmp)) return;
    dmp->dm_vp = vp;
}

int
dm_get_clearbufferafter(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_clearBufferAfter;
}

void
dm_set_clearbufferafter(dm *dmp, int clearBufferAfter)
{
    if (UNLIKELY(!dmp)) return;
    dmp->dm_clearBufferAfter = clearBufferAfter;
}

/* This is the generic "catch-all" hook that is used
 * to run any user supplied callbacks.  If more side
 * effects are needed at the libdm level, a task-specific
 * hook should be defined using this function as a
 * starting point and adding in whatever additional
 * logic is needed.  The full dm structure should always
 * be accessible as a slot in the modifiable variables
 * structure passed in here as "base" */
void
dm_generic_hook(const struct bu_structparse *sdp,
		const char *name, void *base, const char *value, void *data)
{
    if (data) {
	struct dm_hook_data *hook= (struct dm_hook_data *)data;

	/* Call hook function(if it exists) to carry out the
	 * application requested logic */
	if (hook && hook->dm_hook)
	    hook->dm_hook(sdp, name, base, value, hook->dmh_data);
    }
}

int
dm_set_hook(const struct bu_structparse_map *map,
	    const char *key, void *data, struct dm_hook_data *hook)
{
    if (UNLIKELY(!map || !key || !hook)) return -1;
    hook->dm_hook = BU_STRUCTPARSE_FUNC_NULL;
    hook->dmh_data = NULL;
    for (; map->sp_name != (char *)0; map++) {
	if (BU_STR_EQUAL(map->sp_name, key)) {
	    hook->dm_hook = map->sp_hook;
	    if (data)
		hook->dmh_data = data;
	    return 0;
	}
    }
    return 1;
}

struct bu_structparse *
dm_get_vparse(dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    return dmp->vparse;
}

void *
dm_get_mvars(dm *dmp)
{
    if (UNLIKELY(!dmp)) return NULL;
    if (!dmp->m_vars) return (void *)dmp;
    return dmp->m_vars;
}


/* Routines for drawing based on a list of display_list
 * structures.  This will probably need to be a struct dm
 * entry to allow it to be customized for various dm
 * backends, but as a first step get it out of MGED
 * and into libdm. */
static int
dm_drawSolid(dm *dmp,
	     struct solid *sp,
	     short r,
	     short g,
	     short b,
	     int draw_style,
	     unsigned char *gdc)
{
    int ndrawn = 0;

    if (sp->s_cflag) {
	if (!DM_SAME_COLOR(r, g, b, (short)gdc[0], (short)gdc[1], (short)gdc[2])) {
	    dm_set_fg(dmp, (short)gdc[0], (short)gdc[1], (short)gdc[2], 0, sp->s_transparency);
	    DM_COPY_COLOR(r, g, b, (short)gdc[0], (short)gdc[1], (short)gdc[2]);
	}
    } else {
	if (!DM_SAME_COLOR(r, g, b, (short)sp->s_color[0], (short)sp->s_color[1], (short)sp->s_color[2])) {
	    dm_set_fg(dmp, (short)sp->s_color[0], (short)sp->s_color[1], (short)sp->s_color[2], 0, sp->s_transparency);
	    DM_COPY_COLOR(r, g, b, (short)sp->s_color[0], (short)sp->s_color[1], (short)sp->s_color[2]);
	}
    }

    if (dm_get_displaylist(dmp) && draw_style) {
	dm_draw_dlist(dmp, sp->s_dlist);
	sp->s_flag = UP;
	ndrawn++;
    } else {
	if (dm_draw_vlist(dmp, (struct bn_vlist *)&sp->s_vlist) == BRLCAD_OK) {
	    sp->s_flag = UP;
	    ndrawn++;
	}
    }

    return ndrawn;
}


int
dm_draw_display_list(dm *dmp,
		     struct bu_list *dl,
		     fastf_t transparency_threshold,
		     fastf_t inv_viewsize,
		     short r, short g, short b,
		     int line_width,
		     int draw_style,
		     int draw_edit,
		     unsigned char *gdc,
		     int solids_down,
		     int mv_dlist
		    )
{
    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct solid *sp;
    fastf_t ratio;
    int ndrawn = 0;
    int opaque = 0;
    int opaque_only = EQUAL(transparency_threshold, 1.0);

    gdlp = BU_LIST_NEXT(display_list, dl);
    while (BU_LIST_NOT_HEAD(gdlp, dl)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
	    if (solids_down) sp->s_flag = DOWN;              /* Not drawn yet */

	    /* If part of object edit, will be drawn below */
	    if ((sp->s_iflag == UP && !draw_edit) || (sp->s_iflag != UP && draw_edit))
		continue;

	    opaque = EQUAL(sp->s_transparency, 1.0);
	    if (opaque_only) {
		if (!opaque) {
		    continue;
		}
	    } else {
		/* transparent only */
		if (opaque || !(sp->s_transparency > transparency_threshold || EQUAL(sp->s_transparency, transparency_threshold))) {
		    continue;
		}
	    }

	    if (dm_get_bound_flag(dmp)) {
		ratio = sp->s_size * inv_viewsize;

		/*
		 * Check for this object being bigger than
		 * dmp->dm_bound * the window size, or smaller than a speck.
		 */
		if (ratio < 0.001)
		    continue;
	    }

	    dm_set_line_attr(dmp, line_width, sp->s_soldash);

	    if (!draw_edit) {
		ndrawn += dm_drawSolid(dmp, sp, r, g, b, draw_style, gdc);
	    } else {
		if (dm_get_displaylist(dmp) && mv_dlist) {
		    dm_draw_dlist(dmp, sp->s_dlist);
		    sp->s_flag = UP;
		    ndrawn++;
		} else {
		    /* draw in immediate mode */
		    if (dm_draw_vlist(dmp, (struct bn_vlist *)&sp->s_vlist) == BRLCAD_OK) {
			sp->s_flag = UP;
			ndrawn++;
		    }
		}
	    }
	}

	gdlp = next_gdlp;
    }

    return ndrawn;
}

struct bu_vls *
dm_list_types(const char separator)
{
    struct bu_vls *list;
    char sep = ' ';
    if (separator) sep = separator;
    BU_GET(list, struct bu_vls);
    bu_vls_init(list);

    bu_vls_trunc(list, 0);

#ifdef DM_OSGL
    if (strlen(bu_vls_addr(list)) > 0) bu_vls_printf(list, "%c", sep);
    bu_vls_printf(list, "osgl");
#endif /* DM_OSGL*/

#ifdef DM_WGL
    if (strlen(bu_vls_addr(list)) > 0) bu_vls_printf(list, "%c", sep);
    bu_vls_printf(list, "wgl");
#endif /* DM_WGL */

#ifdef DM_OGL
    if (strlen(bu_vls_addr(list)) > 0) bu_vls_printf(list, "%c", sep);
    bu_vls_printf(list, "ogl");
#endif /* DM_OGL */

#ifdef DM_QT
    if (strlen(bu_vls_addr(list)) > 0) bu_vls_printf(list, "%c", sep);
    bu_vls_printf(list, "Qt");
#endif /* DM_QT */

#ifdef DM_X
    if (strlen(bu_vls_addr(list)) > 0) bu_vls_printf(list, "%c", sep);
    bu_vls_printf(list, "X");
#endif /* DM_X */

#ifdef DM_X
    if (strlen(bu_vls_addr(list)) > 0) bu_vls_printf(list, "%c", sep);
    bu_vls_printf(list, "tk");
#endif /* DM_X */

    if (strlen(bu_vls_addr(list)) > 0) bu_vls_printf(list, "%c", sep);
    bu_vls_printf(list, "txt");
    bu_vls_printf(list, "%c", sep);
    bu_vls_printf(list, "plot");
    bu_vls_printf(list, "%c", sep);
    bu_vls_printf(list, "ps");
    bu_vls_printf(list, "%c", sep);
    bu_vls_printf(list, "null");
    return list;
}

#if defined(DM_X) || defined(DM_OGL)

#ifdef DM_X
#  include "dm/dm_xvars.h"
#  include <X11/Xutil.h>
#endif /* DM_X */

int
dm_png_write(dm *dmp, FILE *fp, struct bu_vls *msgs)
{
    png_structp png_p;
    png_infop info_p;
    XImage *ximage_p;
    unsigned char **rows;
    unsigned char *idata;
    unsigned char *irow;
    int bytes_per_pixel;
    int bits_per_channel = 8;  /* bits per color channel */
    int i, j, k;
    unsigned char *dbyte0, *dbyte1, *dbyte2, *dbyte3;
    int red_shift;
    int green_shift;
    int blue_shift;
    int red_bits;
    int green_bits;
    int blue_bits;

    png_p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_p) {
	if (msgs) {
	    bu_vls_printf(msgs, "png: could not create PNG write structure\n");
	}
	return BRLCAD_ERROR;
    }

    info_p = png_create_info_struct(png_p);
    if (!info_p) {
	if (msgs) {
	    bu_vls_printf(msgs, "png: could not create PNG info structure\n");
	}
	fclose(fp);
	return BRLCAD_ERROR;
    }

    ximage_p = XGetImage(((struct dm_xvars *)dmp->dm_vars.pub_vars)->dpy,
			 ((struct dm_xvars *)dmp->dm_vars.pub_vars)->win,
			 0, 0,
			 dmp->dm_width,
			 dmp->dm_height,
			 ~0, ZPixmap);
    if (!ximage_p) {
	if (msgs) {
	    bu_vls_printf(msgs, "png: could not get XImage\n");
	}
	return BRLCAD_ERROR;
    }

    bytes_per_pixel = ximage_p->bytes_per_line / ximage_p->width;

    if (bytes_per_pixel == 4) {
	unsigned long mask;
	unsigned long tmask;

	/* This section assumes 8 bits per channel */

	mask = ximage_p->red_mask;
	tmask = 1;
	for (red_shift = 0; red_shift < 32; red_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}

	mask = ximage_p->green_mask;
	tmask = 1;
	for (green_shift = 0; green_shift < 32; green_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}

	mask = ximage_p->blue_mask;
	tmask = 1;
	for (blue_shift = 0; blue_shift < 32; blue_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}

	/*
	 * We need to reverse things if the image byte order
	 * is different from the system's byte order.
	 */
	if (((bu_byteorder() == BU_BIG_ENDIAN) && (ximage_p->byte_order == LSBFirst)) ||
	    ((bu_byteorder() == BU_LITTLE_ENDIAN) && (ximage_p->byte_order == MSBFirst))) {
	    DM_REVERSE_COLOR_BYTE_ORDER(red_shift, ximage_p->red_mask);
	    DM_REVERSE_COLOR_BYTE_ORDER(green_shift, ximage_p->green_mask);
	    DM_REVERSE_COLOR_BYTE_ORDER(blue_shift, ximage_p->blue_mask);
	}

    } else if (bytes_per_pixel == 2) {
	unsigned long mask;
	unsigned long tmask;
	int bpb = 8;   /* bits per byte */

	/*XXX
	 * This section probably needs logic similar
	 * to the previous section (i.e. bytes_per_pixel == 4).
	 * That is we may need to reverse things depending on
	 * the image byte order and the system's byte order.
	 */

	mask = ximage_p->red_mask;
	tmask = 1;
	for (red_shift = 0; red_shift < 16; red_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}
	for (red_bits = red_shift; red_bits < 16; red_bits++) {
	    if (!(tmask & mask))
		break;
	    tmask = tmask << 1;
	}

	red_bits = red_bits - red_shift;
	if (red_shift == 0)
	    red_shift = red_bits - bpb;
	else
	    red_shift = red_shift - (bpb - red_bits);

	mask = ximage_p->green_mask;
	tmask = 1;
	for (green_shift = 0; green_shift < 16; green_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}
	for (green_bits = green_shift; green_bits < 16; green_bits++) {
	    if (!(tmask & mask))
		break;
	    tmask = tmask << 1;
	}

	green_bits = green_bits - green_shift;
	green_shift = green_shift - (bpb - green_bits);

	mask = ximage_p->blue_mask;
	tmask = 1;
	for (blue_shift = 0; blue_shift < 16; blue_shift++) {
	    if (tmask & mask)
		break;
	    tmask = tmask << 1;
	}
	for (blue_bits = blue_shift; blue_bits < 16; blue_bits++) {
	    if (!(tmask & mask))
		break;
	    tmask = tmask << 1;
	}
	blue_bits = blue_bits - blue_shift;

	if (blue_shift == 0)
	    blue_shift = blue_bits - bpb;
	else
	    blue_shift = blue_shift - (bpb - blue_bits);
    } else {
	if (msgs) {
	    bu_vls_printf(msgs, "png: %d bytes per pixel is not yet supported\n", bytes_per_pixel);
	}
	return BRLCAD_ERROR;
    }

    rows = (unsigned char **)bu_calloc(ximage_p->height, sizeof(unsigned char *), "rows");
    idata = (unsigned char *)bu_calloc(ximage_p->height * ximage_p->width, 4, "png data");

    /* for each scanline */
    for (i = ximage_p->height - 1, j = 0; 0 <= i; --i, ++j) {
	/* irow points to the current scanline in ximage_p */
	irow = (unsigned char *)(ximage_p->data + ((ximage_p->height-i-1)*ximage_p->bytes_per_line));

	if (bytes_per_pixel == 4) {
	    unsigned int pixel;

	    /* rows[j] points to the current scanline in idata */
	    rows[j] = (unsigned char *)(idata + ((ximage_p->height-i-1)*ximage_p->bytes_per_line));

	    /* for each pixel in current scanline of ximage_p */
	    for (k = 0; k < ximage_p->bytes_per_line; k += bytes_per_pixel) {
		pixel = *((unsigned int *)(irow + k));

		dbyte0 = rows[j] + k;
		dbyte1 = dbyte0 + 1;
		dbyte2 = dbyte0 + 2;
		dbyte3 = dbyte0 + 3;

		*dbyte0 = (pixel & ximage_p->red_mask) >> red_shift;
		*dbyte1 = (pixel & ximage_p->green_mask) >> green_shift;
		*dbyte2 = (pixel & ximage_p->blue_mask) >> blue_shift;
		*dbyte3 = 255;
	    }
	} else if (bytes_per_pixel == 2) {
	    unsigned short spixel;
	    unsigned long pixel;

	    /* rows[j] points to the current scanline in idata */
	    rows[j] = (unsigned char *)(idata + ((ximage_p->height-i-1)*ximage_p->bytes_per_line*2));

	    /* for each pixel in current scanline of ximage_p */
	    for (k = 0; k < ximage_p->bytes_per_line; k += bytes_per_pixel) {
		spixel = *((unsigned short *)(irow + k));
		pixel = spixel;

		dbyte0 = rows[j] + k*2;
		dbyte1 = dbyte0 + 1;
		dbyte2 = dbyte0 + 2;
		dbyte3 = dbyte0 + 3;

		if (0 <= red_shift)
		    *dbyte0 = (pixel & ximage_p->red_mask) >> red_shift;
		else
		    *dbyte0 = (pixel & ximage_p->red_mask) << -red_shift;

		*dbyte1 = (pixel & ximage_p->green_mask) >> green_shift;

		if (0 <= blue_shift)
		    *dbyte2 = (pixel & ximage_p->blue_mask) >> blue_shift;
		else
		    *dbyte2 = (pixel & ximage_p->blue_mask) << -blue_shift;

		*dbyte3 = 255;
	    }
	} else {
	    bu_free(rows, "rows");
	    bu_free(idata, "image data");
	    fclose(fp);

	    if (msgs) {
		bu_vls_printf(msgs, "png: not supported for this platform\n");
	    }
	    return BRLCAD_ERROR;
	}
    }

    png_init_io(png_p, fp);
    png_set_filter(png_p, 0, PNG_FILTER_NONE);
    png_set_compression_level(png_p, 9);
    png_set_IHDR(png_p, info_p, ximage_p->width, ximage_p->height, bits_per_channel,
		 PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
		 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_set_gAMA(png_p, info_p, 0.77);
    png_write_info(png_p, info_p);
    png_write_image(png_p, rows);
    png_write_end(png_p, NULL);

    bu_free(rows, "rows");
    bu_free(idata, "image data");

    return BRLCAD_OK;
}
#else
int
dm_png_write(dm *UNUSED(dmp), FILE *UNUSED(fp), struct bu_vls *msgs)
{
    bu_vls_printf("PNG output not supported\n");
}
#endif // defined(DM_X) || defined(DM_OGL)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
