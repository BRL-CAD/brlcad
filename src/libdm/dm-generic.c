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

#include <string.h>

#include "tcl.h"

#include "bu/units.h"
#include "vmath.h"
#include "dm.h"
#include "dm_private.h"

#include "dm-Null.h"
#include "solid.h"

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

#ifdef DM_OSGL
extern dm *osgl_open();
extern void osgl_fogHint();
extern int osgl_share_dlist();
#endif /* DM_OSGL*/

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
    if (UNLIKELY(!dmp1) || UNLIKELY(!dmp2)) return TCL_ERROR;

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


vect_t *
dm_get_clipmax(dm *dmp)
{
    if (UNLIKELY(!dmp)) return 0;
    return  &(dmp->dm_clipmax);
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
void
dm_draw_dlist(struct dm_internal *dmp, unsigned int list)
{
    if (UNLIKELY(!dmp)) return;
    dmp->dm_drawDList(list);
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
dm_set_depth_mask(dm *dmp, int d_on)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_setDepthMask(dmp, d_on);
}
int
dm_debug(dm *dmp, int lvl)
{
    if (UNLIKELY(!dmp)) return 0;
    return dmp->dm_debug(dmp, lvl);
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
        if (dm_draw_vlist(dmp, (struct bn_vlist *)&sp->s_vlist) == TCL_OK) {
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

    gdlp = BU_LIST_NEXT(display_list, dl);
    while (BU_LIST_NOT_HEAD(gdlp, dl)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->dl_headSolid) {
	    if (solids_down) sp->s_flag = DOWN;              /* Not drawn yet */

	    /* If part of object edit, will be drawn below */
	    if ((sp->s_iflag == UP && !draw_edit) || (sp->s_iflag != UP && draw_edit))
		continue;

	    if (!((sp->s_transparency > transparency_threshold) || (EQUAL(sp->s_transparency, transparency_threshold))))
		continue;

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
		    if (dm_draw_vlist(dmp, (struct bn_vlist *)&sp->s_vlist) == TCL_OK) {
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

/* Generic DM functions from libtclcad - add these to the "need to
 * sort out the API" list, but in the meantime put them in dm
 * instead of tclcad so they're more generally usable */

fastf_t
dm_screen_to_view_x(dm *dmp, fastf_t x)
{
    int width = dm_get_width(dmp);
    return x / (fastf_t)width * 2.0 - 1.0;
}


fastf_t
dm_screen_to_view_y(dm *dmp, fastf_t y)
{
    int height = dm_get_height(dmp);
    return (y / (fastf_t)height * -2.0 + 1.0) / dm_get_aspect(dmp);
}

#define DM_DRAW_POLY(_dmp, _gdpsp, _i, _last_poly, _mode) {  \
        size_t _j; \
        \
        /* set color */ \
        (void)dm_set_fg((_dmp), \
                             (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_color[0], \
                             (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_color[1], \
                             (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_color[2], \
                             1, 1.0);                                   \
        \
        /* set the linewidth and linestyle for polygon i */ \
        (void)dm_set_line_attr((_dmp), \
                               (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_line_width, \
                               (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_line_style); \
        \
        for (_j = 0; _j < (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_num_contours; ++_j) { \
            size_t _last = (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_contour[_j].gpc_num_points-1; \
            \
            (void)dm_draw_lines_3d((_dmp),                              \
                                   (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_contour[_j].gpc_num_points, \
                                   (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_contour[_j].gpc_point, 1); \
            \
            if (_mode != DM_POLY_CONTOUR_MODE || _i != _last_poly || (_gdpsp)->gdps_cflag == 0) { \
                (void)dm_draw_line_3d((_dmp),                           \
                                      (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_contour[_j].gpc_point[_last], \
                                      (_gdpsp)->gdps_polygons.gp_polygon[_i].gp_contour[_j].gpc_point[0]); \
            } \
        }}


HIDDEN void
dm_draw_polys(dm *dmp, bview_data_polygon_state *gdpsp, int mode)
{
    register size_t i, last_poly;
    int saveLineWidth;
    int saveLineStyle;

    if (gdpsp->gdps_polygons.gp_num_polygons < 1)
	return;

    saveLineWidth = dm_get_linewidth(dmp);
    saveLineStyle = dm_get_linestyle(dmp);

    last_poly = gdpsp->gdps_polygons.gp_num_polygons - 1;
    for (i = 0; i < gdpsp->gdps_polygons.gp_num_polygons; ++i) {
	if (i == gdpsp->gdps_target_polygon_i)
	    continue;

	DM_DRAW_POLY(dmp, gdpsp, i, last_poly, mode);
    }

    /* draw the target poly last */
    DM_DRAW_POLY(dmp, gdpsp, gdpsp->gdps_target_polygon_i, last_poly, mode);

    /* Restore the line attributes */
    (void)dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);
}

void
dm_draw_arrows(dm *dmp, struct bview_data_arrow_state *gdasp, fastf_t sf)
{
    register int i;
    int saveLineWidth;
    int saveLineStyle;

    if (gdasp->gdas_num_points < 1)
        return;

    saveLineWidth = dm_get_linewidth(dmp);
    saveLineStyle = dm_get_linestyle(dmp);

    /* set color */
    (void)dm_set_fg(dmp,
                         gdasp->gdas_color[0],
                         gdasp->gdas_color[1],
                         gdasp->gdas_color[2], 1, 1.0);

    /* set linewidth */
    (void)dm_set_line_attr(dmp, gdasp->gdas_line_width, 0);  /* solid lines */

    (void)dm_draw_lines_3d(dmp,
                           gdasp->gdas_num_points,
                           gdasp->gdas_points, 0);

    for (i = 0; i < gdasp->gdas_num_points; i += 2) {
        point_t points[16];
        point_t A, B;
        point_t BmA;
        point_t offset;
        point_t perp1, perp2;
        point_t a_base;
        point_t a_pt1, a_pt2, a_pt3, a_pt4;

        VMOVE(A, gdasp->gdas_points[i]);
        VMOVE(B, gdasp->gdas_points[i+1]);
        VSUB2(BmA, B, A);

        VUNITIZE(BmA);
        VSCALE(offset, BmA, -gdasp->gdas_tip_length * sf);

        bn_vec_perp(perp1, BmA);
        VUNITIZE(perp1);

        VCROSS(perp2, BmA, perp1);
        VUNITIZE(perp2);

        VSCALE(perp1, perp1, gdasp->gdas_tip_width * sf);
        VSCALE(perp2, perp2, gdasp->gdas_tip_width * sf);

        VADD2(a_base, B, offset);
        VADD2(a_pt1, a_base, perp1);
        VADD2(a_pt2, a_base, perp2);
        VSUB2(a_pt3, a_base, perp1);
        VSUB2(a_pt4, a_base, perp2);

        VMOVE(points[0], B);
        VMOVE(points[1], a_pt1);
        VMOVE(points[2], B);
        VMOVE(points[3], a_pt2);
        VMOVE(points[4], B);
        VMOVE(points[5], a_pt3);
        VMOVE(points[6], B);
        VMOVE(points[7], a_pt4);
        VMOVE(points[8], a_pt1);
        VMOVE(points[9], a_pt2);
        VMOVE(points[10], a_pt2);
        VMOVE(points[11], a_pt3);
        VMOVE(points[12], a_pt3);
        VMOVE(points[13], a_pt4);
        VMOVE(points[14], a_pt4);
        VMOVE(points[15], a_pt1);

        (void)dm_draw_lines_3d(dmp, 16, points, 0);
    }

    /* Restore the line attributes */
    (void)dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);
}

void
dm_draw_labels2(dm *dmp, struct bview_data_label_state *gdlsp, matp_t m2vmat)
{
    register int i;

    /* set color */
    (void)dm_set_fg(dmp,
                         gdlsp->gdls_color[0],
                         gdlsp->gdls_color[1],
                         gdlsp->gdls_color[2], 1, 1.0);

    for (i = 0; i < gdlsp->gdls_num_labels; ++i) {
        point_t vpoint;

        MAT4X3PNT(vpoint, m2vmat,
                  gdlsp->gdls_points[i]);
        (void)dm_draw_string_2d(dmp, gdlsp->gdls_labels[i],
                                vpoint[X], vpoint[Y], 0, 1);
    }
}


void
dm_draw_lines(dm *dmp, struct bview_data_line_state *gdlsp)
{
    int saveLineWidth;
    int saveLineStyle;

    if (gdlsp->gdls_num_points < 1)
        return;

    saveLineWidth = dm_get_linewidth(dmp);
    saveLineStyle = dm_get_linestyle(dmp);

    /* set color */
    (void)dm_set_fg(dmp,
                         gdlsp->gdls_color[0],
                         gdlsp->gdls_color[1],
                         gdlsp->gdls_color[2], 1, 1.0);

    /* set linewidth */
    (void)dm_set_line_attr(dmp, gdlsp->gdls_line_width, 0);  /* solid lines */

    (void)dm_draw_lines_3d(dmp,
                           gdlsp->gdls_num_points,
                           gdlsp->gdls_points, 0);

    /* Restore the line attributes */
    (void)dm_set_line_attr(dmp, saveLineWidth, saveLineStyle);
}

void
dm_draw_faceplate(dm *dmp, struct bview *view, double local2base, double base2local)
{
    /* Center dot */
    if (view->gv_center_dot.gos_draw) {
	(void)dm_set_fg(dmp,
		view->gv_center_dot.gos_line_color[0],
		view->gv_center_dot.gos_line_color[1],
		view->gv_center_dot.gos_line_color[2],
		1, 1.0);
	(void)dm_draw_point_2d(dmp, 0.0, 0.0);
    }

    /* Model axes */
    if (view->gv_model_axes.draw) {
	point_t map;
	point_t save_map;

	VMOVE(save_map, view->gv_model_axes.axes_pos);
	VSCALE(map, view->gv_model_axes.axes_pos, local2base);
	MAT4X3PNT(view->gv_model_axes.axes_pos, view->gv_model2view, map);

	dm_draw_axes(dmp,
		view->gv_size,
		view->gv_rotation,
		&view->gv_model_axes);

	VMOVE(view->gv_model_axes.axes_pos, save_map);
    }

    /* View axes */
    if (view->gv_view_axes.draw) {
	int width, height;
	fastf_t inv_aspect;
	fastf_t save_ypos;

	save_ypos = view->gv_view_axes.axes_pos[Y];
	width = dm_get_width(dmp);
	height = dm_get_height(dmp);
	inv_aspect = (fastf_t)height / (fastf_t)width;
	view->gv_view_axes.axes_pos[Y] = save_ypos * inv_aspect;
	dm_draw_axes(dmp,
		view->gv_size,
		view->gv_rotation,
		&view->gv_view_axes);

	view->gv_view_axes.axes_pos[Y] = save_ypos;
    }


    /* View scale */
    if (view->gv_view_scale.gos_draw)
	dm_draw_scale(dmp,
		view->gv_size*base2local,
		view->gv_view_scale.gos_line_color,
		view->gv_view_params.gos_text_color);

    /* View parameters */
    if (view->gv_view_params.gos_draw) {
	struct bu_vls vls = BU_VLS_INIT_ZERO;
	point_t center;
	char *ustr;

	MAT_DELTAS_GET_NEG(center, view->gv_center);
	VSCALE(center, center, base2local);

	ustr = (char *)bu_units_string(local2base);
	bu_vls_printf(&vls, "units:%s  size:%.2f  center:(%.2f, %.2f, %.2f) az:%.2f  el:%.2f  tw::%.2f",
		ustr,
		view->gv_size * base2local,
		V3ARGS(center),
		V3ARGS(view->gv_aet));
	(void)dm_set_fg(dmp,
		view->gv_view_params.gos_text_color[0],
		view->gv_view_params.gos_text_color[1],
		view->gv_view_params.gos_text_color[2],
		1, 1.0);
	(void)dm_draw_string_2d(dmp, bu_vls_addr(&vls), -0.98, -0.965, 10, 0);
	bu_vls_free(&vls);
    }

    /* Draw the angle distance cursor */
    if (view->gv_adc.draw)
	dm_draw_adc(dmp, &(view->gv_adc), view->gv_view2model, view->gv_model2view);

    /* Draw grid */
    if (view->gv_grid.draw)
	dm_draw_grid(dmp, &view->gv_grid, view->gv_scale, view->gv_model2view, base2local);

    /* Draw rect */
    if (view->gv_rect.draw && view->gv_rect.line_width)
	dm_draw_rect(dmp, &view->gv_rect);
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
