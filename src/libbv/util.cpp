/*                      U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file util.cpp
 *
 * Utility functions for operating on BRL-CAD views
 *
 */

#include "common.h"
#include <string.h>
#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bn/mat.h"
#include "bv/defines.h"
#include "bv/vlist.h"
#include "bv/util.h"
#include "bv/view_sets.h"
#include "./bv_private.h"

void
bv_init(struct bview *gvp, struct bview_set *s)
{
    if (!gvp)
	return;

    gvp->magic = BV_MAGIC;

    gvp->vset = s;

    if (!BU_VLS_IS_INITIALIZED(&gvp->gv_name)) {
	bu_vls_init(&gvp->gv_name);
    }

    // Independent has to be the default, since we don't
    // have shared containers until they are supplied from
    // an outside source.
    gvp->independent = 1;

    gvp->gv_scale = 500.0;
    gvp->gv_i_scale = gvp->gv_scale;
    gvp->gv_a_scale = 1.0 - gvp->gv_scale / gvp->gv_i_scale;
    gvp->gv_size = 2.0 * gvp->gv_scale;
    gvp->gv_isize = 1.0 / gvp->gv_size;
    VSET(gvp->gv_aet, 35.0, 25.0, 0.0);
    VSET(gvp->gv_eye_pos, 0.0, 0.0, 1.0);
    MAT_IDN(gvp->gv_rotation);
    MAT_IDN(gvp->gv_center);
    VSETALL(gvp->gv_keypoint, 0.0);
    gvp->gv_coord = 'v';
    gvp->gv_rotate_about = 'v';
    gvp->gv_minMouseDelta = -20;
    gvp->gv_maxMouseDelta = 20;
    gvp->gv_rscale = 0.4;
    gvp->gv_sscale = 2.0;

    gvp->gv_data_vZ = 0.0;

    /* Initialize local settings */
    bv_settings_init(&gvp->gv_ls);

    // TODO - unimplemented
    gvp->gv_ls.gv_selected = NULL;

    /* Out of the gate we don't have any shared settings */
    gvp->gv_s = &gvp->gv_ls;

    /* FIXME: this causes the shaders.sh regression to fail */
    /* bv_mat_aet(gvp); */

    gvp->gv_tcl.gv_prim_labels.gos_draw = 0;
    VSET(gvp->gv_tcl.gv_prim_labels.gos_text_color, 255, 255, 0);


    // gv_objs.db_objs is local to this view and thus is controlled
    // by the bv init and free routines.
    BU_GET(gvp->gv_objs.db_objs, struct bu_ptbl);
    bu_ptbl_init(gvp->gv_objs.db_objs, 8, "view_objs init");

    // gv_objs.view_objs is local to this view and thus is controlled
    // by the bv init and free routines.
    BU_GET(gvp->gv_objs.view_objs, struct bu_ptbl);
    bu_ptbl_init(gvp->gv_objs.view_objs, 8, "view_objs init");

    // Until the app tells us differently, we need to use our local
    // containers
    BU_GET(gvp->gv_objs.free_scene_obj, struct bv_scene_obj);
    BU_LIST_INIT(&gvp->gv_objs.free_scene_obj->l);
    BU_LIST_INIT(&gvp->gv_objs.gv_vlfree);

    // Out of the gate we don't have callbacks
    gvp->callbacks = NULL;
    gvp->gv_callback = NULL;
    gvp->gv_bounds_update= NULL;

    bv_update(gvp);
}

void
bv_free(struct bview *gvp)
{
    if (!gvp)
	return;

    bu_vls_free(&gvp->gv_name);
    bu_ptbl_free(gvp->gv_objs.db_objs);
    BU_PUT(gvp->gv_objs.db_objs, struct bu_ptbl);
    bu_ptbl_free(gvp->gv_objs.view_objs);
    BU_PUT(gvp->gv_objs.view_objs, struct bu_ptbl);

    // TODO - clean up local vlfree list contents
    struct bv_scene_obj *sp, *nsp;
    sp = BU_LIST_NEXT(bv_scene_obj, &gvp->gv_objs.free_scene_obj->l);
    while (BU_LIST_NOT_HEAD(sp, &gvp->gv_objs.free_scene_obj->l)) {
	nsp = BU_LIST_PNEXT(bv_scene_obj, sp);
	BU_LIST_DEQUEUE(&((sp)->l));
	if (sp->s_free_callback)
	    (*sp->s_free_callback)(sp);
	if (sp->s_dlist_free_callback)
	    (*sp->s_dlist_free_callback)(sp);
	bu_ptbl_free(&sp->children);
	BU_PUT(sp, struct bv_scene_obj);
	sp = nsp;
    }
    BU_PUT(gvp->gv_objs.free_scene_obj, struct bv_scene_obj);

    if (gvp->gv_ls.gv_selected) {
	bu_ptbl_free(gvp->gv_ls.gv_selected);
	BU_PUT(gvp->gv_ls.gv_selected, struct bu_ptbl);
	gvp->gv_ls.gv_selected = NULL;
    }

    if (gvp->callbacks) {
	bu_ptbl_free(gvp->callbacks);
	BU_PUT(gvp->callbacks, struct bu_ptbl);
    }
}

static void
_bound_objs(int *is_empty, int *have_geom_objs, vect_t min, vect_t max, struct bu_ptbl *so, struct bview *v)
{
    vect_t minus, plus;
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bv_scene_group *g = (struct bv_scene_group *)BU_PTBL_GET(so, i);
	_bound_objs(is_empty, have_geom_objs, min, max, &g->children, v);
	if (bv_scene_obj_bound(g, v)) {
	    (*is_empty) = 0;
	    (*have_geom_objs) = 1;
	    minus[X] = g->s_center[X] - g->s_size;
	    minus[Y] = g->s_center[Y] - g->s_size;
	    minus[Z] = g->s_center[Z] - g->s_size;
	    VMIN(min, minus);
	    plus[X] = g->s_center[X] + g->s_size;
	    plus[Y] = g->s_center[Y] + g->s_size;
	    plus[Z] = g->s_center[Z] + g->s_size;
	    VMAX(max, plus);
	}
    }
}

static void
_find_view_geom(int *have_geom_objs, struct bu_ptbl *so)
{
    if (*have_geom_objs)
	return;

    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(so, i);
	_find_view_geom(have_geom_objs, &s->children);
	if ((s->s_type_flags & BV_DBOBJ_BASED) ||
		(s->s_type_flags & BV_POLYGONS) ||
		(s->s_type_flags & BV_LABELS)) {
	    (*have_geom_objs) = 1;
	    break;
	}
    }
}

static void
_bound_objs_view(int *is_empty, vect_t min, vect_t max, struct bu_ptbl *so, struct bview *v, int have_geom_objs, int all_view_objs)
{
    vect_t minus, plus;
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(so, i);
	_bound_objs_view(is_empty, min, max, &s->children, v, have_geom_objs, all_view_objs);
        if (have_geom_objs && !all_view_objs) {
            if (!(s->s_type_flags & BV_DBOBJ_BASED) &&
                !(s->s_type_flags & BV_POLYGONS) &&
                !(s->s_type_flags & BV_LABELS))
                continue;
        }
        if (bv_scene_obj_bound(s, v)) {
            is_empty = 0;
            minus[X] = s->s_center[X] - s->s_size;
            minus[Y] = s->s_center[Y] - s->s_size;
            minus[Z] = s->s_center[Z] - s->s_size;
            VMIN(min, minus);
            plus[X] = s->s_center[X] + s->s_size;
            plus[Y] = s->s_center[Y] + s->s_size;
            plus[Z] = s->s_center[Z] + s->s_size;
            VMAX(max, plus);
        }
    }
}


void
bv_autoview(struct bview *v, double factor, int all_view_objs)
{
    vect_t min, max;
    vect_t center = VINIT_ZERO;
    vect_t radial;
    vect_t sqrt_small;
    int is_empty = 1;
    int have_geom_objs = 0;

    /* set the default if unset or insane */
    if (factor < SQRT_SMALL_FASTF) {
	factor = 2.0; /* 2 is half the view */
    }

    VSETALL(sqrt_small, SQRT_SMALL_FASTF);

    /* calculate the bounding for all solids and polygons being displayed */
    VSETALL(min,  INFINITY);
    VSETALL(max, -INFINITY);

    struct bu_ptbl *so = bv_view_objs(v, BV_DB_OBJS);
    _bound_objs(&is_empty, &have_geom_objs, min, max, so, v);

    // When it comes to view-only objects, normally we will only include those
    // that are db object based, polygons or labels, unless the flag to
    // consider all objects is set.   However, there is an exception - if there
    // are NO such objects in the scene (have_geom_objs == 0) and we do have
    // view objs (for example, when overlaying a plot file on an empty view)
    // then basing autoview on the view-only objs is more intuitive than just
    // using the default view settings.
    so = bv_view_objs(v, BV_VIEW_OBJS);
    _find_view_geom(&have_geom_objs, so);
    _bound_objs_view(&is_empty,min, max, so, v, have_geom_objs, all_view_objs);

    if (is_empty) {
	/* Nothing is in view */
	VSETALL(radial, 1000.0);
    } else {
	VADD2SCALE(center, max, min, 0.5);
	VSUB2(radial, max, center);
    }

    /* make sure it's not inverted */
    VMAX(radial, sqrt_small);

    /* make sure it's not too small */
    if (VNEAR_ZERO(radial, SQRT_SMALL_FASTF))
	VSETALL(radial, 1.0);

    MAT_IDN(v->gv_center);
    MAT_DELTAS_VEC_NEG(v->gv_center, center);
    v->gv_scale = radial[X];
    V_MAX(v->gv_scale, radial[Y]);
    V_MAX(v->gv_scale, radial[Z]);

    v->gv_size = factor * v->gv_scale;
    v->gv_isize = 1.0 / v->gv_size;
    bv_update(v);
}

/**
 * FIXME: this routine is suspect and needs investigating.  if run
 * during view initialization, the shaders regression test fails.
 */
void
bv_mat_aet(struct bview *v)
{
    mat_t tmat;
    fastf_t twist;
    fastf_t c_twist;
    fastf_t s_twist;

    bn_mat_angles(v->gv_rotation,
                  270.0 + v->gv_aet[1],
                  0.0,
                  270.0 - v->gv_aet[0]);

    twist = -v->gv_aet[2] * DEG2RAD;
    c_twist = cos(twist);
    s_twist = sin(twist);
    bn_mat_zrot(tmat, s_twist, c_twist);
    bn_mat_mul2(tmat, v->gv_rotation);
}

void
bv_settings_init(struct bview_settings *s)
{
    struct bv_obj_settings defaults = BV_OBJ_SETTINGS_INIT;
    bv_obj_settings_sync(&s->obj_s, &defaults);

    s->gv_cleared = 1;

    s->gv_adc.a1 = 45.0;
    s->gv_adc.a2 = 45.0;
    VSET(s->gv_adc.line_color, 255, 255, 0);
    VSET(s->gv_adc.tick_color, 255, 255, 255);

    VSET(s->gv_grid.anchor, 0.0, 0.0, 0.0);
    s->gv_grid.res_h = 1.0;
    s->gv_grid.res_v = 1.0;
    s->gv_grid.res_major_h = 5;
    s->gv_grid.res_major_v = 5;
    VSET(s->gv_grid.color, 255, 255, 255);

    s->gv_rect.draw = 0;
    s->gv_rect.pos[0] = 128;
    s->gv_rect.pos[1] = 128;
    s->gv_rect.dim[0] = 256;
    s->gv_rect.dim[1] = 256;
    VSET(s->gv_rect.color, 255, 255, 255);

    s->gv_view_axes.draw = 0;
    VSET(s->gv_view_axes.axes_pos, 0.80, -0.80, 0.0);
    s->gv_view_axes.axes_size = 0.2;
    s->gv_view_axes.line_width = 0;
    s->gv_view_axes.pos_only = 1;
    VSET(s->gv_view_axes.axes_color, 255, 255, 255);
    s->gv_view_axes.label_flag = 1;
    VSET(s->gv_view_axes.label_color, 255, 255, 0);
    s->gv_view_axes.triple_color = 1;

    s->gv_model_axes.draw = 0;
    VSET(s->gv_model_axes.axes_pos, 0.0, 0.0, 0.0);
    s->gv_model_axes.axes_size = 2.0;
    s->gv_model_axes.line_width = 0;
    s->gv_model_axes.pos_only = 0;
    VSET(s->gv_model_axes.axes_color, 255, 255, 255);
    s->gv_model_axes.label_flag = 1;
    VSET(s->gv_model_axes.label_color, 255, 255, 0);
    s->gv_model_axes.triple_color = 0;
    s->gv_model_axes.tick_enabled = 1;
    s->gv_model_axes.tick_length = 4;
    s->gv_model_axes.tick_major_length = 8;
    s->gv_model_axes.tick_interval = 100;
    s->gv_model_axes.ticks_per_major = 10;
    s->gv_model_axes.tick_threshold = 8;
    VSET(s->gv_model_axes.tick_color, 255, 255, 0);
    VSET(s->gv_model_axes.tick_major_color, 255, 0, 0);

    s->gv_center_dot.gos_draw = 0;
    VSET(s->gv_center_dot.gos_line_color, 255, 255, 0);

    s->gv_view_params.gos_draw = 0;
    VSET(s->gv_view_params.gos_text_color, 255, 255, 0);

    s->gv_view_scale.gos_draw = 0;
    VSET(s->gv_view_scale.gos_line_color, 255, 255, 0);
    VSET(s->gv_view_scale.gos_text_color, 255, 255, 255);

    s->gv_fps = 0;
    s->gv_frametime = 1;
    s->gv_fb_mode = 0;

    s->gv_autoview = 1;

    s->adaptive_plot_mesh = 0;
    s->adaptive_plot_csg = 0;
    s->redraw_on_zoom = 0;
    s->point_scale = 1;
    s->curve_scale = 1;
    s->bot_threshold = 0;
    s->lod_scale = 1.0;

    // Higher values indicate more aggressive behavior (i.e. points further away will be snapped).
    s->gv_snap_tol_factor = 10;
    s->gv_snap_lines = 0;
}

// TODO - investigate saveview/loadview logic, see if anything
// makes sense to move here
void
bv_sync(struct bview *dest, struct bview *src)
{
    if (!src || !dest)
	return;

    /* Size info */
    dest->gv_i_scale = src->gv_i_scale;
    dest->gv_a_scale = src->gv_a_scale;
    dest->gv_scale = src->gv_scale;
    dest->gv_size = src->gv_size;
    dest->gv_isize = src->gv_isize;
    dest->gv_width = src->gv_width;
    dest->gv_height = src->gv_height;
    dest->gv_base2local = src->gv_base2local;
    dest->gv_rscale = src->gv_rscale;
    dest->gv_sscale = src->gv_sscale;

    /* Camera info */
    dest->gv_perspective = src->gv_perspective;
    VMOVE(dest->gv_aet, src->gv_aet);
    VMOVE(dest->gv_eye_pos, src->gv_eye_pos);
    VMOVE(dest->gv_keypoint, src->gv_keypoint);
    dest->gv_coord = src->gv_coord;
    dest->gv_rotate_about = src->gv_rotate_about;
    MAT_COPY(dest->gv_rotation, src->gv_rotation);
    MAT_COPY(dest->gv_center, src->gv_center);
    MAT_COPY(dest->gv_model2view, src->gv_model2view);
    MAT_COPY(dest->gv_pmodel2view, src->gv_pmodel2view);
    MAT_COPY(dest->gv_view2model, src->gv_view2model);
    MAT_COPY(dest->gv_pmat, src->gv_pmat);
}

void
bv_update(struct bview *gvp)
{
    vect_t work, work1;
    vect_t temp, temp1;

    if (!gvp)
        return;

    bn_mat_mul(gvp->gv_model2view,
               gvp->gv_rotation,
               gvp->gv_center);
    gvp->gv_model2view[15] = gvp->gv_scale;
    bn_mat_inv(gvp->gv_view2model, gvp->gv_model2view);

    /* Find current azimuth, elevation, and twist angles */
    VSET(work, 0.0, 0.0, 1.0);       /* view z-direction */
    MAT4X3VEC(temp, gvp->gv_view2model, work);
    VSET(work1, 1.0, 0.0, 0.0);      /* view x-direction */
    MAT4X3VEC(temp1, gvp->gv_view2model, work1);

    /* calculate angles using accuracy of 0.005, since display
     * shows 2 digits right of decimal point */
    bn_aet_vec(&gvp->gv_aet[0],
               &gvp->gv_aet[1],
               &gvp->gv_aet[2],
               temp, temp1, (fastf_t)0.005);

    /* Force azimuth range to be [0, 360] */
    if ((NEAR_EQUAL(gvp->gv_aet[1], 90.0, (fastf_t)0.005) ||
         NEAR_EQUAL(gvp->gv_aet[1], -90.0, (fastf_t)0.005)) &&
        gvp->gv_aet[0] < 0 &&
        !NEAR_ZERO(gvp->gv_aet[0], (fastf_t)0.005))
        gvp->gv_aet[0] += 360.0;
    else if (NEAR_ZERO(gvp->gv_aet[0], (fastf_t)0.005))
        gvp->gv_aet[0] = 0.0;

    /* apply the perspective angle to model2view */
    bn_mat_mul(gvp->gv_pmodel2view, gvp->gv_pmat, gvp->gv_model2view);

    /* Update obb, if the caller has told us how to */
    if (gvp->gv_bounds_update) {
	(*gvp->gv_bounds_update)(gvp);
    }

    if (gvp->gv_callback) {

	if (gvp->callbacks) {
	    if (bu_ptbl_locate(gvp->callbacks, (long *)(uintptr_t)gvp->gv_callback) != -1) {
		bu_log("Recursive callback (bv_update and gvp->gv_callback)");
	    }
	    bu_ptbl_ins_unique(gvp->callbacks, (long *)(uintptr_t)gvp->gv_callback);
	}

	(*gvp->gv_callback)(gvp, gvp->gv_clientData);

	if (gvp->callbacks) {
	    bu_ptbl_rm(gvp->callbacks, (long *)(uintptr_t)gvp->gv_callback);
	}

    }
}

void
bv_obj_settings_sync(struct bv_obj_settings *dest, struct bv_obj_settings *src)
{
    dest->s_line_width = src->s_line_width;
    dest->s_arrow_tip_length = src->s_arrow_tip_length;
    dest->s_arrow_tip_width = src->s_arrow_tip_width;
    dest->transparency = src->transparency;
    dest->s_dmode = src->s_dmode;
    dest->color_override = src->color_override;
    VMOVE(dest->color, src->color);
    dest->draw_solid_lines_only = src->draw_solid_lines_only;
    dest->draw_non_subtract_only = src->draw_non_subtract_only;
}

int
bv_update_selected(struct bview *gvp)
{
    int ret = 0;
    if (!gvp)
	return 0;
#if 0
    for(size_t i = 0; i < BU_PTBL_LEN(gvp->gv_selected); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(gvp->gv_selected, i);
	if (s->s_update_callback) {
	    ret += (*s->s_update_callback)(s);
	}
    }
#endif
    return (ret > 0) ? 1 : 0;
}

// TODO - support constraints
int
_bv_rot(struct bview *v, int dx, int dy, point_t keypoint, unsigned long long UNUSED(flags))
{
    if (!v)
	return 0;

    point_t rot_pt;
    point_t new_origin;
    mat_t viewchg, viewchginv;
    point_t new_cent_view;
    point_t new_cent_model;

    fastf_t rdx = (fastf_t)dx * 0.25;
    fastf_t rdy = (fastf_t)dy * 0.25;
    mat_t newrot, newinv;
    bn_mat_angles(newrot, rdx, rdy, 0);
    bn_mat_inv(newinv, newrot);
    MAT4X3PNT(rot_pt, v->gv_model2view, keypoint);  /* point to rotate around */

    bn_mat_xform_about_pnt(viewchg, newrot, rot_pt);
    bn_mat_inv(viewchginv, viewchg);
    VSET(new_origin, 0.0, 0.0, 0.0);
    MAT4X3PNT(new_cent_view, viewchginv, new_origin);
    MAT4X3PNT(new_cent_model, v->gv_view2model, new_cent_view);
    MAT_DELTAS_VEC_NEG(v->gv_center, new_cent_model);

    /* Update the rotation component of the model2view matrix */
    bn_mat_mul2(newrot, v->gv_rotation); /* pure rotation */

    /* gv_rotation is updated, now sync other bv values */
    bv_update(v);

    return 1;
}

int
_bv_trans(struct bview *v, int dx, int dy, point_t UNUSED(keypoint), unsigned long long UNUSED(flags))
{
    if (!v)
	return 0;

    fastf_t aspect = (fastf_t)v->gv_width / (fastf_t)v->gv_height;
    fastf_t fx = (fastf_t)dx / (fastf_t)v->gv_width * 2.0;
    fastf_t fy = -dy / (fastf_t)v->gv_height / aspect * 2.0;

    vect_t tt;
    point_t delta;
    point_t work;
    point_t vc, nvc;

    VSET(tt, fx, fy, 0);
    MAT4X3PNT(work, v->gv_view2model, tt);
    MAT_DELTAS_GET_NEG(vc, v->gv_center);
    VSUB2(delta, work, vc);
    VSUB2(nvc, vc, delta);
    MAT_DELTAS_VEC_NEG(v->gv_center, nvc);
    bv_update(v);

    return 1;
}

int
_bv_scale(struct bview *v, int sensitivity, int factor, point_t UNUSED(keypoint), unsigned long long UNUSED(flags))
{
    if (!v)
	return 0;

    double f = (double)factor/(double)sensitivity;
    v->gv_scale /= f;
    if (v->gv_scale < BV_MINVIEWSCALE)
        v->gv_scale = BV_MINVIEWSCALE;
    v->gv_size = 2.0 * v->gv_scale;
    v->gv_isize = 1.0 / v->gv_size;

    /* scale factors are set, now sync other bv values */
    bv_update(v);

    return 1;
}

int
_bv_center(struct bview *v, int vx, int vy, point_t UNUSED(keypoint), unsigned long long UNUSED(flags))
{
    if (!v)
	return 0;

    point_t vpt, center;
    fastf_t fx = 0.0;
    fastf_t fy = 0.0;
    bv_screen_to_view(v, &fx, &fy, (fastf_t)vx, (fastf_t)vy);
    VSET(vpt, fx, fy, 0);
    MAT4X3PNT(center, v->gv_view2model, vpt);
    MAT_DELTAS_VEC_NEG(v->gv_center, center);
    bv_update(v);
    return 1;
}

int
bv_adjust(struct bview *v, int dx, int dy, point_t keypoint, int UNUSED(mode), unsigned long long flags)
{
    if (flags == BV_IDLE)
	return 0;

    // TODO - figure out why these need to be flipped for qdm to do the right thing...
    if (flags & BV_ROT)
	return _bv_rot(v, dy, dx, keypoint, flags);

    if (flags & BV_TRANS)
	return _bv_trans(v, dx, dy, keypoint, flags);

    if (flags & BV_SCALE)
	return _bv_scale(v, dx, dy, keypoint, flags);

    if (flags & BV_CENTER)
	return _bv_center(v, dx, dy, keypoint, flags);


    return 0;
}


int
bv_screen_to_view(struct bview *v, fastf_t *fx, fastf_t *fy, fastf_t x, fastf_t y)
{
    if (!v)
	return -1;

    if (!v->gv_width || !v->gv_height)
	return -1;

    if (fx) {
	fastf_t tx = x / (fastf_t)v->gv_width * 2.0 - 1.0;
	(*fx) = tx;
    }

    if (fy) {
	fastf_t aspect = (fastf_t)v->gv_width / (fastf_t)v->gv_height;
	fastf_t ty = (y / (fastf_t)v->gv_height * -2.0 + 1.0) / aspect;
	(*fy) = ty;
    }

    return 0;
}

size_t
bv_clear(struct bview *v, int flags)
{
    if (!flags || flags & BV_DB_OBJS) {
	struct bu_ptbl *sg = bv_view_objs(v, BV_DB_OBJS | (flags & ~BV_VIEW_OBJS));
	if (sg) {
	    for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
		struct bv_scene_obj *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
		bv_obj_put(cg);
	    }
	    bu_ptbl_reset(sg);
	}
    }
    if (!flags || flags & BV_VIEW_OBJS) {
	struct bu_ptbl *sv = bv_view_objs(v, BV_VIEW_OBJS | (flags & ~BV_DB_OBJS));
	if (sv) {
	    for (long i = (long)BU_PTBL_LEN(sv) - 1; i >= 0; i--) {
		struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(sv, i);
		bv_obj_put(s);
	    }
	    bu_ptbl_reset(sv);
	}
    }

    if (!flags || flags & BV_LOCAL_OBJS || v->independent) {
	if (!flags || flags & BV_DB_OBJS) {
	    struct bu_ptbl *sg = bv_view_objs(v, BV_DB_OBJS | (flags & ~BV_VIEW_OBJS) | BV_LOCAL_OBJS);
	    if (sg) {
		for (size_t i = 0; i < BU_PTBL_LEN(sg); i++) {
		    struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(sg, i);
		    bv_obj_put(cg);
		}
		bu_ptbl_reset(sg);
	    }
	}
	if (!flags || flags & BV_VIEW_OBJS) {
	    struct bu_ptbl *sv = bv_view_objs(v, BV_VIEW_OBJS | (flags & ~BV_DB_OBJS) | BV_LOCAL_OBJS);
	    if (sv) {
		for (long i = (long)BU_PTBL_LEN(sv) - 1; i >= 0; i--) {
		    struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(sv, i);
		    bv_obj_put(s);
		}
		bu_ptbl_reset(sv);
	    }
	}
    }

    struct bu_ptbl *sg = bv_view_objs(v, BV_DB_OBJS);
    struct bu_ptbl *sgl = bv_view_objs(v, BV_DB_OBJS | BV_LOCAL_OBJS);

    struct bu_ptbl *sv = bv_view_objs(v, BV_VIEW_OBJS);
    struct bu_ptbl *svl = bv_view_objs(v, BV_VIEW_OBJS | BV_LOCAL_OBJS);

    size_t ocnt = 0;
    ocnt += (sg) ? BU_PTBL_LEN(sg) : 0;
    ocnt += (sgl && sgl != sg) ? BU_PTBL_LEN(sgl) : 0;
    ocnt += (sv) ? BU_PTBL_LEN(sv) : 0;
    ocnt += (svl && svl != sv) ? BU_PTBL_LEN(svl) : 0;
    return ocnt ;
}

void
bv_obj_stale(struct bv_scene_obj *s)
{
    s->s_dlist_stale = 1;

    if (BU_PTBL_IS_INITIALIZED(&s->children)) {
	for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	    struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	    bv_obj_stale(s_c);
	}
    }

    std::unordered_map<struct bview *, struct bv_scene_obj *>::iterator vo_it;
    for (vo_it = s->i->vobjs.begin(); vo_it != s->i->vobjs.end(); vo_it++) {
	struct bv_scene_obj *sv = vo_it->second; 
	bv_obj_stale(sv);
    }
}

struct bv_scene_obj *
bv_obj_create(struct bview *v, int type)
{
    if (!v)
	return NULL;

    struct bv_scene_obj *s = NULL;
    struct bu_ptbl *otbl = NULL;

    // What we get and from where is based on the requested obj type and the
    // view type.  If the caller is not asking for a local object, we will try
    // to get a shared object.  If the view has no associated set then the only
    // available storage is the local storage, and that will be used instead.
    // If a local object is requested, then the local storage is used
    // regardless of whether or not a shared repository is available.
    struct bv_scene_obj *free_scene_obj = NULL;
    struct bu_list *vlfree = NULL;
    if (type & BV_LOCAL_OBJS || v->independent || !v->vset)  {
	free_scene_obj = v->gv_objs.free_scene_obj;
	if (type & BV_DB_OBJS) {
	    otbl = v->gv_objs.db_objs;
	} else {
	    otbl = v->gv_objs.view_objs;
	}
	vlfree = &v->gv_objs.gv_vlfree;
    } else {
	free_scene_obj = v->vset->i->free_scene_obj;
	if (type & BV_DB_OBJS) {
	    otbl = &v->vset->i->shared_db_objs;
	} else {
	    otbl = &v->vset->i->shared_view_objs;
	}
	vlfree = &v->vset->i->vlfree;
    }
    if (!free_scene_obj)
	return NULL;

    // We know where we're going to get the object from - get it
    if (BU_LIST_IS_EMPTY(&free_scene_obj->l)) {
	BU_ALLOC(s, struct bv_scene_obj);
	s->i = new bv_scene_obj_internal;
    } else {
	s = BU_LIST_NEXT(bv_scene_obj, &free_scene_obj->l);
	BU_LIST_DEQUEUE(&((s)->l));
    }
    BU_LIST_INIT( &((s)->s_vlist) );

    // Do the initializations
    s->s_type_flags = 0;
    BU_LIST_INIT(&(s->s_vlist));
    BU_VLS_INIT(&s->s_name);
    bu_vls_trunc(&s->s_name, 0);
    s->s_path = NULL;
    BU_VLS_INIT(&s->s_uuid);
    if (type & BV_LOCAL_OBJS) {
	if (type & BV_DB_OBJS) {
	    bu_vls_sprintf(&s->s_uuid, "sl:%zd", BU_PTBL_LEN(otbl));
	} else {
	    bu_vls_sprintf(&s->s_uuid, "vl:%zd", BU_PTBL_LEN(otbl));
	}
    } else {
	if (type & BV_DB_OBJS) {
	    bu_vls_sprintf(&s->s_uuid, "s:%zd", BU_PTBL_LEN(otbl));
	} else {
	    bu_vls_sprintf(&s->s_uuid, "v:%zd", BU_PTBL_LEN(otbl));
	}
    }
    MAT_IDN(s->s_mat);
    s->s_size = 0;
    s->s_v = v;

    s->s_i_data = NULL;
    s->s_update_callback = NULL;
    s->s_free_callback = NULL;

    s->adaptive_wireframe = 0;
    s->view_scale = 0.0;

    s->s_flag = UP;
    s->s_iflag = DOWN;
    VSET(s->s_color, 255, 0, 0);
    s->s_soldash = 0;
    s->s_arrow = 0;

    struct bv_obj_settings defaults = BV_OBJ_SETTINGS_INIT;
    bv_obj_settings_sync(&s->s_local_os, &defaults);
    s->s_os = &s->s_local_os;

    if (!BU_PTBL_IS_INITIALIZED(&s->children)) {
	BU_PTBL_INIT(&s->children);
    }
    bu_ptbl_reset(&s->children);

    s->free_scene_obj = free_scene_obj;
    s->vlfree = vlfree;
    s->otbl = otbl;
    s->current = 0;

    return s;
}

struct bv_scene_obj *
bv_obj_get(struct bview *v, int type)
{
    if (!v)
	return NULL;

    struct bv_scene_obj *s = bv_obj_create(v, type);
    if (!s)
	return NULL;

    bu_ptbl_ins(s->otbl, (long *)s);

    return s;
}

struct bv_scene_obj *
bv_obj_get_child(struct bv_scene_obj *sp)
{
    if (!sp)
	return NULL;

    struct bv_scene_obj *s = NULL;

    // Children use their parent's info
    if (BU_LIST_IS_EMPTY(&sp->free_scene_obj->l)) {
	BU_ALLOC((s), struct bv_scene_obj);
	s->i = new bv_scene_obj_internal;
    } else {
	s = BU_LIST_NEXT(bv_scene_obj, &sp->free_scene_obj->l);
	if (!s) {
	    BU_ALLOC((s), struct bv_scene_obj);
	    s->i = new bv_scene_obj_internal;
	} else {
	    BU_LIST_DEQUEUE(&((s)->l));
	}
    }
    BU_LIST_INIT( &((s)->s_vlist) );

    // Do the initializations
    s->s_type_flags = 0;
    BU_LIST_INIT(&(s->s_vlist));
    if (!BU_VLS_IS_INITIALIZED(&s->s_name))
	BU_VLS_INIT(&s->s_name);
    bu_vls_trunc(&s->s_name, 0);
    s->s_path = NULL;
    if (!BU_VLS_IS_INITIALIZED(&s->s_uuid))
	BU_VLS_INIT(&s->s_uuid);
    bu_vls_sprintf(&s->s_uuid, "child:%s:%zd", bu_vls_cstr(&sp->s_uuid), BU_PTBL_LEN(&sp->children));
    MAT_IDN(s->s_mat);

    s->s_v = sp->s_v;

    s->s_i_data = NULL;
    s->s_update_callback = NULL;
    s->s_free_callback = NULL;

    s->adaptive_wireframe = 0;
    s->view_scale = 0.0;

    s->s_flag = UP;
    s->s_iflag = DOWN;
    VSET(s->s_color, 255, 0, 0);
    s->s_soldash = 0;
    s->s_arrow = 0;

    struct bv_obj_settings defaults = BV_OBJ_SETTINGS_INIT;
    bv_obj_settings_sync(&s->s_local_os, &defaults);
    s->s_os = &s->s_local_os;

    if (!BU_PTBL_IS_INITIALIZED(&s->children)) {
	BU_PTBL_INIT(&s->children);
    }
    bu_ptbl_reset(&s->children);

    s->free_scene_obj = sp->free_scene_obj;
    s->vlfree = sp->vlfree;
    s->otbl = &sp->children;

    bu_ptbl_ins(&sp->children, (long *)s);

    return s;
}

void
bv_obj_reset(struct bv_scene_obj *s)
{
    // handle children
    if (BU_PTBL_IS_INITIALIZED(&s->children)) {
	for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	    struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	    bv_obj_put(s_c);
	}
	bu_ptbl_reset(&s->children);
    }

    if (s->i) {
	s->i->vobjs.clear();
    }

    // If we have a callback for the internal data, use it
    if (s->s_free_callback)
	(*s->s_free_callback)(s);

    // If we have a callback for the display list data, use it
    if (s->s_dlist_free_callback)
	(*s->s_dlist_free_callback)(s);

    // If we have a label, do the label freeing steps
    // TODO - properly speaking this should be using the free callback rather
    // than special casing...
    if (s->s_type_flags & BV_LABELS) {
	struct bv_label *la = (struct bv_label *)s->s_i_data;
	bu_vls_free(&la->label);
	BU_PUT(la, struct bv_label);
    }

    // free vlist
    if (BU_LIST_IS_INITIALIZED(&s->s_vlist)) {
	BV_FREE_VLIST(s->vlfree, &s->s_vlist);
    }
    BU_LIST_INIT(&(s->s_vlist));

    s->s_v = NULL;
    s->current = 0;
    VSETALL(s->bmin, INFINITY);
    VSETALL(s->bmax, -INFINITY);
    s->s_size = 0;
    s->s_csize = 0;
    VSETALL(s->s_center, 0);
    s->adaptive_wireframe = 0;
    s->view_scale = 0;
    s->bot_threshold = 0;
    s->curve_scale = 0;
    s->point_scale = 0;

    s->draw_data = NULL;
}

#define FREE_BV_SCENE_OBJ(p, fp) { \
    BU_LIST_APPEND(fp, &((p)->l)); }

void
bv_obj_put(struct bv_scene_obj *s)
{
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(&s->children, i);
	bv_obj_put(cg);
    }

    bv_obj_reset(s);

    // Clear names
    bu_vls_trunc(&s->s_uuid, 0);
    bu_vls_trunc(&s->s_name, 0);
    s->s_path = NULL;

    if (s->otbl)
	bu_ptbl_rm(s->otbl, (long *)s);

    s->otbl = NULL;

    struct bv_scene_obj *fs = s->free_scene_obj;
    s->free_scene_obj = NULL;
    if (fs)
	FREE_BV_SCENE_OBJ(s, &fs->l);
}

struct bv_scene_obj *
bv_find_obj(struct bview *v, const char *name)
{
    if (!v || !name)
	return NULL;

    // First look for matches in shared sets, if any are defined
    if (!v->independent && v->vset) {
	for (size_t i = 0; i < BU_PTBL_LEN(&v->vset->i->shared_db_objs); i++) {
	    struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&v->vset->i->shared_db_objs, i);
	    if (!bu_path_match(name, bu_vls_cstr(&s_c->s_name), 0))
		return s_c;
	}
	for (size_t i = 0; i < BU_PTBL_LEN(&v->vset->i->shared_view_objs); i++) {
	    struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&v->vset->i->shared_view_objs, i);
	    if (!bu_path_match(name, bu_vls_cstr(&s_c->s_name), 0))
		return s_c;
	}
    }

    // Next look locally
    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_objs.db_objs); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_objs.db_objs, i);
	if (!bu_path_match(name, bu_vls_cstr(&s_c->s_name), 0))
	    return s_c;
    }

    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_objs.view_objs); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_objs.view_objs, i);
	if (!bu_path_match(name, bu_vls_cstr(&s_c->s_name), 0))
	    return s_c;
    }

    // If none of the names matched, check uuids
    if (!v->independent && v->vset) {
	for (size_t i = 0; i < BU_PTBL_LEN(&v->vset->i->shared_db_objs); i++) {
	    struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&v->vset->i->shared_db_objs, i);
	    if (!bu_path_match(name, bu_vls_cstr(&s_c->s_uuid), 0))
		return s_c;
	}
	for (size_t i = 0; i < BU_PTBL_LEN(&v->vset->i->shared_view_objs); i++) {
	    struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&v->vset->i->shared_view_objs, i);
	    if (!bu_path_match(name, bu_vls_cstr(&s_c->s_uuid), 0))
		return s_c;
	}
    }
    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_objs.db_objs); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_objs.db_objs, i);
	if (!bu_path_match(name, bu_vls_cstr(&s_c->s_uuid), 0))
	    return s_c;
    }
    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_objs.view_objs); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_objs.view_objs, i);
	if (!bu_path_match(name, bu_vls_cstr(&s_c->s_uuid), 0))
	    return s_c;
    }

    return NULL;
}

struct bv_scene_obj *
bv_obj_for_view(struct bv_scene_obj *s, struct bview *v)
{
    if (!v || !s || !s->i)
	return s;

    std::unordered_map<struct bview *, struct bv_scene_obj *>::iterator vo_it;
    vo_it = s->i->vobjs.find(v);
    if (vo_it == s->i->vobjs.end())
	return s;
    return vo_it->second;
}

void
bv_set_view_obj(struct bv_scene_obj *s, struct bview *v, struct bv_scene_obj *sv)
{
    if (!v || !s || !s->i || !sv)
	return;

    std::unordered_map<struct bview *, struct bv_scene_obj *>::iterator vo_it;
    vo_it = s->i->vobjs.find(v);
    if (vo_it != s->i->vobjs.end())
	bv_obj_put(vo_it->second);
    s->i->vobjs[v] = sv;
    sv->s_os = s->s_os;
}

struct bv_scene_obj *
bv_find_child(struct bv_scene_obj *s, const char *vname)
{
    if (!s || !vname || !BU_PTBL_IS_INITIALIZED(&s->children))
	return NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	if (!bu_path_match(vname, bu_vls_cstr(&s_c->s_name), 0))
	    return s_c;
    }
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	if (!bu_path_match(vname, bu_vls_cstr(&s_c->s_uuid), 0))
	    return s_c;
    }

    return NULL;
}

int
bv_scene_obj_bound(struct bv_scene_obj *sp, struct bview *v)
{
    int cmd;
    VSET(sp->bmin, INFINITY, INFINITY, INFINITY);
    VSET(sp->bmax, -INFINITY, -INFINITY, -INFINITY);
    int calc = 0;
    if (sp->s_type_flags & BV_MESH_LOD) {
	struct bv_scene_obj *sv = bv_obj_for_view(sp, v);
	struct bv_mesh_lod *i = (struct bv_mesh_lod *)sv->draw_data;
	if (i) {
	    point_t obmin, obmax;
	    VMOVE(obmin, i->bmin);
	    VMOVE(obmax, i->bmax);
	    // Apply the scene matrix to the bounding box values to bound this
	    // instance, since the BV_MESH_LOD data is based on the
	    // non-instanced mesh.
	    MAT4X3PNT(sp->bmin, sp->s_mat, obmin);
	    MAT4X3PNT(sp->bmax, sp->s_mat, obmax);
	    calc = 1;
	}
    } else if (bu_list_len(&sp->s_vlist)) {
	int dispmode;
	cmd = bv_vlist_bbox(&sp->s_vlist, &sp->bmin, &sp->bmax, NULL, &dispmode);
	if (cmd) {
	    bu_log("unknown vlist op %d\n", cmd);
	}
	sp->s_displayobj = dispmode;
	calc = 1;
    }
    if (calc) {
	sp->s_center[X] = (sp->bmin[X] + sp->bmax[X]) * 0.5;
	sp->s_center[Y] = (sp->bmin[Y] + sp->bmax[Y]) * 0.5;
	sp->s_center[Z] = (sp->bmin[Z] + sp->bmax[Z]) * 0.5;

	sp->s_size = sp->bmax[X] - sp->bmin[X];
	V_MAX(sp->s_size, sp->bmax[Y] - sp->bmin[Y]);
	V_MAX(sp->s_size, sp->bmax[Z] - sp->bmin[Z]);
	return 1;
    }
    return 0;
}

fastf_t
bv_vZ_calc(struct bv_scene_obj *s, struct bview *v, int mode)
{
    fastf_t vZ = 0.0;
    int calc_mode = mode;
    if (!s)
	return vZ;

    if (mode < 0)
	calc_mode = 0;
    if (mode > 1)
	calc_mode = 1;

    double calc_val = (calc_mode) ? -DBL_MAX : DBL_MAX;
    int have_val = 0;
    struct bv_vlist *tvp;
    for (BU_LIST_FOR(tvp, bv_vlist, &((struct bv_vlist *)(&s->s_vlist))->l)) {
	size_t nused = tvp->nused;
	point_t *lpt = tvp->pt;
	for (size_t l = 0; l < nused; l++, lpt++) {
	    vect_t vpt;
	    MAT4X3PNT(vpt, v->gv_model2view, *lpt);
	    if (calc_mode) {
		if (vpt[Z] > calc_val) {
		    calc_val = vpt[Z];
		    have_val = 1;
		}
	    } else {
		if (vpt[Z] < calc_val) {
		    calc_val = vpt[Z];
		    have_val = 1;
		}
	    }
	}
    }
    if (have_val) {
	vZ = calc_val;
    }
    return vZ;
}


struct bu_ptbl *
bv_view_objs(struct bview *v, int type)
{
    if (type & BV_DB_OBJS) {
	if (v->independent || type & BV_LOCAL_OBJS || !v->vset) {
	    return v->gv_objs.db_objs;
	} else {
	    return &v->vset->i->shared_db_objs;
	}
    }

    if (type & BV_VIEW_OBJS) {
	if (v->independent || type & BV_LOCAL_OBJS || !v->vset) {
	    return v->gv_objs.view_objs;
	} else {
	    return &v->vset->i->shared_view_objs;
	}
    }

    return NULL;
}


void
bv_obj_sync(struct bv_scene_obj *dest, struct bv_scene_obj *src)
{
    bv_obj_settings_sync(dest->s_os, src->s_os);
    VMOVE(dest->s_center, src->s_center);
    VMOVE(dest->s_color, src->s_color);
    VMOVE(dest->bmin, src->bmin);
    VMOVE(dest->bmax, src->bmax);
    MAT_COPY(dest->s_mat, src->s_mat);
    dest->s_size = src->s_size;
    dest->s_soldash = src->s_soldash;
    dest->s_arrow = src->s_arrow;
    dest->adaptive_wireframe = src->adaptive_wireframe;
    dest->view_scale = src->view_scale;
    dest->bot_threshold = src->bot_threshold;
    dest->curve_scale = src->curve_scale;
    dest->point_scale = src->point_scale;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
