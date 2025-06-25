/*                      U T I L . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2025 United States Government as represented by
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
#include "bu/time.h"
#include "bu/vls.h"
#include "bn/mat.h"
#include "bg/plane.h"
#include "bv/defines.h"
#include "bv/snap.h"
#include "bv/util.h"
#include "bv/vlist.h"
#include "./bv_private.h"

#define VIEW_NAME_MAXTRIES 100000
#define DM_DEFAULT_FONT_SIZE 20

static void
_data_tclcad_init(struct bv_data_tclcad *d)
{
    d->gv_polygon_mode = 0;
    d->gv_hide = 0;

    d->gv_data_arrows.gdas_draw = 0;
    d->gv_data_arrows.gdas_color[0] = 0;
    d->gv_data_arrows.gdas_color[1] = 0;
    d->gv_data_arrows.gdas_color[2] = 0;
    d->gv_data_arrows.gdas_line_width = 0;
    d->gv_data_arrows.gdas_tip_length = 0;
    d->gv_data_arrows.gdas_tip_width = 0;
    d->gv_data_arrows.gdas_num_points = 0;
    d->gv_data_arrows.gdas_points = NULL;

    d->gv_data_axes.draw = 0;
    d->gv_data_axes.color[0] = 0;
    d->gv_data_axes.color[1] = 0;
    d->gv_data_axes.color[2] = 0;
    d->gv_data_axes.line_width = 0;
    d->gv_data_axes.size = 0;
    d->gv_data_axes.num_points = 0;
    d->gv_data_axes.points = NULL;

    d->gv_data_labels.gdls_draw = 0;
    d->gv_data_labels.gdls_color[0] = 0;
    d->gv_data_labels.gdls_color[1] = 0;
    d->gv_data_labels.gdls_color[2] = 0;
    d->gv_data_labels.gdls_num_labels = 0;
    d->gv_data_labels.gdls_size = 0;
    d->gv_data_labels.gdls_labels = NULL;
    d->gv_data_labels.gdls_points = NULL;

    d->gv_data_lines.gdls_draw = 0;
    d->gv_data_lines.gdls_color[0] = 0;
    d->gv_data_lines.gdls_color[1] = 0;
    d->gv_data_lines.gdls_color[2] = 0;
    d->gv_data_lines.gdls_line_width = 0;
    d->gv_data_lines.gdls_num_points = 0;
    d->gv_data_lines.gdls_points = NULL;

    d->gv_data_polygons.gdps_draw = 0;
    d->gv_data_polygons.gdps_moveAll = 0;
    d->gv_data_polygons.gdps_color[0] = 0;
    d->gv_data_polygons.gdps_color[1] = 0;
    d->gv_data_polygons.gdps_color[2] = 0;
    d->gv_data_polygons.gdps_line_width = 0;
    d->gv_data_polygons.gdps_line_style = 0;
    d->gv_data_polygons.gdps_cflag = 0;
    d->gv_data_polygons.gdps_target_polygon_i = 0;
    d->gv_data_polygons.gdps_curr_polygon_i = 0;
    d->gv_data_polygons.gdps_curr_point_i = 0;
    d->gv_data_polygons.gdps_prev_point[0] = 0;
    d->gv_data_polygons.gdps_prev_point[1] = 0;
    d->gv_data_polygons.gdps_prev_point[2] = 0;
    d->gv_data_polygons.gdps_clip_type = bg_Union;
    d->gv_data_polygons.gdps_scale = 0;
    d->gv_data_polygons.gdps_origin[0] = 0;
    d->gv_data_polygons.gdps_origin[1] = 0;
    d->gv_data_polygons.gdps_origin[2] = 0;
    MAT_ZERO(d->gv_data_polygons.gdps_rotation);
    MAT_ZERO(d->gv_data_polygons.gdps_view2model);
    MAT_ZERO(d->gv_data_polygons.gdps_model2view);
    d->gv_data_polygons.gdps_polygons.num_polygons = 0;
    d->gv_data_polygons.gdps_polygons.polygon = NULL;
    d->gv_data_polygons.gdps_data_vZ = 0;

    d->gv_sdata_arrows.gdas_draw = 0;
    d->gv_sdata_arrows.gdas_color[0] = 0;
    d->gv_sdata_arrows.gdas_color[1] = 0;
    d->gv_sdata_arrows.gdas_color[2] = 0;
    d->gv_sdata_arrows.gdas_line_width = 0;
    d->gv_sdata_arrows.gdas_tip_length = 0;
    d->gv_sdata_arrows.gdas_tip_width = 0;
    d->gv_sdata_arrows.gdas_num_points = 0;
    d->gv_sdata_arrows.gdas_points = NULL;

    d->gv_sdata_axes.draw = 0;
    d->gv_sdata_axes.color[0] = 0;
    d->gv_sdata_axes.color[1] = 0;
    d->gv_sdata_axes.color[2] = 0;
    d->gv_sdata_axes.line_width = 0;
    d->gv_sdata_axes.size = 0;
    d->gv_sdata_axes.num_points = 0;
    d->gv_sdata_axes.points = NULL;

    d->gv_sdata_labels.gdls_draw = 0;
    d->gv_sdata_labels.gdls_color[0] = 0;
    d->gv_sdata_labels.gdls_color[1] = 0;
    d->gv_sdata_labels.gdls_color[2] = 0;
    d->gv_sdata_labels.gdls_num_labels = 0;
    d->gv_sdata_labels.gdls_size = 0;
    d->gv_sdata_labels.gdls_labels = NULL;
    d->gv_sdata_labels.gdls_points = NULL;

    d->gv_sdata_lines.gdls_draw = 0;
    d->gv_sdata_lines.gdls_color[0] = 0;
    d->gv_sdata_lines.gdls_color[1] = 0;
    d->gv_sdata_lines.gdls_color[2] = 0;
    d->gv_sdata_lines.gdls_line_width = 0;
    d->gv_sdata_lines.gdls_num_points = 0;
    d->gv_sdata_lines.gdls_points = NULL;

    d->gv_sdata_polygons.gdps_draw = 0;
    d->gv_sdata_polygons.gdps_moveAll = 0;
    d->gv_sdata_polygons.gdps_color[0] = 0;
    d->gv_sdata_polygons.gdps_color[1] = 0;
    d->gv_sdata_polygons.gdps_color[2] = 0;
    d->gv_sdata_polygons.gdps_line_width = 0;
    d->gv_sdata_polygons.gdps_line_style = 0;
    d->gv_sdata_polygons.gdps_cflag = 0;
    d->gv_sdata_polygons.gdps_target_polygon_i = 0;
    d->gv_sdata_polygons.gdps_curr_polygon_i = 0;
    d->gv_sdata_polygons.gdps_curr_point_i = 0;
    d->gv_sdata_polygons.gdps_prev_point[0] = 0;
    d->gv_sdata_polygons.gdps_prev_point[1] = 0;
    d->gv_sdata_polygons.gdps_prev_point[2] = 0;
    d->gv_sdata_polygons.gdps_clip_type = bg_Union;
    d->gv_sdata_polygons.gdps_scale = 0;
    d->gv_sdata_polygons.gdps_origin[0] = 0;
    d->gv_sdata_polygons.gdps_origin[1] = 0;
    d->gv_sdata_polygons.gdps_origin[2] = 0;
    MAT_ZERO(d->gv_sdata_polygons.gdps_rotation);
    MAT_ZERO(d->gv_sdata_polygons.gdps_view2model);
    MAT_ZERO(d->gv_sdata_polygons.gdps_model2view);
    d->gv_sdata_polygons.gdps_polygons.num_polygons = 0;
    d->gv_sdata_polygons.gdps_polygons.polygon = NULL;
    d->gv_sdata_polygons.gdps_data_vZ = 0;

    d->gv_prim_labels.gos_draw = 0;
    d->gv_prim_labels.gos_font_size = DM_DEFAULT_FONT_SIZE;
    d->gv_prim_labels.gos_line_color[0] = 0;
    d->gv_prim_labels.gos_line_color[1] = 0;
    d->gv_prim_labels.gos_line_color[2] = 0;
    d->gv_prim_labels.gos_text_color[0] = 0;
    d->gv_prim_labels.gos_text_color[1] = 0;
    d->gv_prim_labels.gos_text_color[2] = 0;
}

void
bv_init(struct bview *gvp)
{
    if (!gvp)
	return;

    gvp->magic = BV_MAGIC;

    if (!BU_VLS_IS_INITIALIZED(&gvp->gv_name)) {
	bu_vls_init(&gvp->gv_name);
    }

    // TODO - Archer doesn't seem to like it when we set initial
    // view names
#if 0
    // If we have a non-null set, go ahead and generate a unique
    // view name to start out with.  App may override, but make
    // sure we at least start out with a unique name
    bu_vls_sprintf(&gvp->gv_name, "V0");
    bool name_collide = false;
    int view_try_cnt = 0;
    struct bu_ptbl *views = bv_set_views(s);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	struct bview *nv = (struct bview *)BU_PTBL_GET(views, i);
	if (!bu_vls_strcmp(&nv->gv_name, &gvp->gv_name)) {
	    name_collide = true;
	    break;
	}
    }
    while (name_collide && view_try_cnt < VIEW_NAME_MAXTRIES) {
	bu_vls_incr(&gvp->gv_name, NULL, "0:0:0:0", NULL, NULL);
	name_collide = false;
	for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	    struct bview *nv = (struct bview *)BU_PTBL_GET(views, i);
	    if (!bu_vls_strcmp(&nv->gv_name, &gvp->gv_name)) {
		name_collide = true;
		break;
	    }
	    view_try_cnt++;
	}
    }
    if (view_try_cnt >= VIEW_NAME_MAXTRIES) {
	bu_log("Warning - unable to generate view name unique to view set\n");
    }
#endif

    gvp->gv_scale = 500.0;
    gvp->gv_i_scale = gvp->gv_scale;
    gvp->gv_a_scale = 1.0 - gvp->gv_scale / gvp->gv_i_scale;
    gvp->gv_size = 2.0 * gvp->gv_scale;
    gvp->gv_isize = 1.0 / gvp->gv_size;
    VSET(gvp->gv_aet, 35.0, 25.0, 0.0);
    VSET(gvp->gv_eye_pos, 0.0, 0.0, 1.0);
    MAT_IDN(gvp->gv_rotation);
    MAT_IDN(gvp->gv_center);
    MAT_IDN(gvp->gv_view2model);
    MAT_IDN(gvp->gv_model2view);
    VSETALL(gvp->gv_keypoint, 0.0);
    gvp->gv_coord = 'v';
    gvp->gv_rotate_about = 'v';
    gvp->gv_minMouseDelta = -20;
    gvp->gv_maxMouseDelta = 20;
    gvp->gv_rscale = 0.4;
    gvp->gv_sscale = 2.0;
    gvp->gv_perspective = 0.0;

    gvp->gv_prevMouseX = 0;
    gvp->gv_prevMouseY = 0;
    gvp->gv_mouse_x = 0;
    gvp->gv_mouse_y = 0;
    VSETALL(gvp->gv_prev_point, 0.0);
    VSETALL(gvp->gv_point, 0.0);

    /* Initialize local settings */
    bv_settings_init(&gvp->gv_ls);

    // TODO - unimplemented
    gvp->gv_ls.gv_selected = NULL;

    /* Out of the gate we don't have any shared settings */
    gvp->gv_s = &gvp->gv_ls;

    /* FIXME: this causes the shaders.sh regression to fail */
    /* bv_mat_aet(gvp); */

    gvp->gv_tcl.gv_prim_labels.gos_draw = 0;
    gvp->gv_tcl.gv_prim_labels.gos_font_size = DM_DEFAULT_FONT_SIZE;
    VSET(gvp->gv_tcl.gv_prim_labels.gos_text_color, 255, 255, 0);

    // Out of the gate we don't have callbacks
    gvp->callbacks = NULL;
    gvp->gv_callback = NULL;
    gvp->gv_bounds_update= NULL;

    // Also don't have a display manager
    // TODO - What the heck Archer??? Initializing this to NULL causes
    // problems even without the gv_name setting logic above?
    //gvp->dmp = NULL;
    gvp->dm_draw_sobj = NULL;

    // Initial scaling factors are 1
    gvp->gv_base2local = 1.0;
    gvp->gv_local2base = 1.0;

    // Initialize knob vars
    bv_knobs_reset(&gvp->k, BV_KNOBS_ALL);
    gvp->k.origin_m = '\0';
    gvp->k.origin_o = '\0';
    gvp->k.origin_v = '\0';
    gvp->k.rot_m_udata = NULL;
    gvp->k.rot_o_udata = NULL;
    gvp->k.rot_v_udata = NULL;
    gvp->k.sca_udata = NULL;
    gvp->k.tra_m_udata = NULL;
    gvp->k.tra_v_udata = NULL;

    // Initialize trackball pos
    MAT_DELTAS_GET_NEG(gvp->orig_pos, gvp->gv_center);

    // Initialize tclcad specific data (primarily doing this so hashing calculations
    // can succeed)
    _data_tclcad_init(&gvp->gv_tcl);

    bv_update(gvp);
}

void
bv_free(struct bview *gvp)
{
    if (!gvp)
	return;

    bu_vls_free(&gvp->gv_name);

    if (gvp->gv_s)
	bu_ptbl_free(&gvp->gv_s->gv_snap_objs);
    if (gvp->gv_s != &gvp->gv_ls)
	bu_ptbl_free(&gvp->gv_ls.gv_snap_objs);

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
	if (g->have_bbox || bv_scene_obj_bound(g)) {
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
	if (bv_scene_obj_bound(s)) {
	    (*is_empty) = 0;
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
bv_autoview(struct bview *v, struct bu_ptbl *so, double factor)
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

    if (so)
	_bound_objs(&is_empty, &have_geom_objs, min, max, so, v);

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
    s->gv_cleared = 1;

    s->gv_adc.draw = 0;
    s->gv_adc.a1 = 45.0;
    s->gv_adc.a2 = 45.0;
    VSET(s->gv_adc.line_color, 255, 255, 0);
    VSET(s->gv_adc.tick_color, 255, 255, 255);

    s->gv_grid.draw = 0;
    s->gv_grid.adaptive = 0;
    s->gv_grid.snap = 0;
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
    s->gv_center_dot.gos_font_size = DM_DEFAULT_FONT_SIZE;
    VSET(s->gv_center_dot.gos_line_color, 255, 255, 0);

    s->gv_view_params.draw = 0;
    s->gv_view_params.draw_size = 1;
    s->gv_view_params.draw_center = 1;
    s->gv_view_params.draw_az = 1;
    s->gv_view_params.draw_el = 1;
    s->gv_view_params.draw_tw = 1;
    s->gv_view_params.draw_fps = 0;
    VSET(s->gv_view_params.color, 255, 255, 0);
    s->gv_view_params.font_size = DM_DEFAULT_FONT_SIZE;

    s->gv_view_scale.gos_draw = 0;
    s->gv_view_scale.gos_font_size = DM_DEFAULT_FONT_SIZE;
    VSET(s->gv_view_scale.gos_line_color, 255, 255, 0);
    VSET(s->gv_view_scale.gos_text_color, 255, 255, 255);

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
    BU_PTBL_INIT(&s->gv_snap_objs);
    s->gv_snap_flags = 0;
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

int
bv_obj_settings_sync(struct bv_obj_settings *dest, struct bv_obj_settings *src)
{
    int ret = 0;
    if (!dest || !src)
	return ret;

    if (dest->s_line_width != src->s_line_width) {
	dest->s_line_width = src->s_line_width;
	ret = 1;
    }
    if (!NEAR_EQUAL(dest->s_arrow_tip_length, src->s_arrow_tip_length, SMALL_FASTF)) {
	dest->s_arrow_tip_length = src->s_arrow_tip_length;
	ret = 1;
    }
    if (!NEAR_EQUAL(dest->s_arrow_tip_width, src->s_arrow_tip_width, SMALL_FASTF)) {
	dest->s_arrow_tip_width = src->s_arrow_tip_width;
	ret = 1;
    }
    if (!NEAR_EQUAL(dest->transparency, src->transparency, SMALL_FASTF)) {
	dest->transparency = src->transparency;
	ret = 1;
    }
    if (dest->s_dmode != src->s_dmode) {
	dest->s_dmode = src->s_dmode;
	ret = 1;
    }
    if (dest->color_override != src->color_override) {
	dest->color_override = src->color_override;
	ret = 1;
    }
    if (!VNEAR_EQUAL(dest->color, src->color, SMALL_FASTF)) {
	VMOVE(dest->color, src->color);
	ret = 1;
    }
    if (dest->draw_solid_lines_only != src->draw_solid_lines_only) {
	dest->draw_solid_lines_only = src->draw_solid_lines_only;
	ret = 1;
    }
    if (dest->draw_non_subtract_only != src->draw_non_subtract_only) {
	dest->draw_non_subtract_only = src->draw_non_subtract_only;
	ret = 1;
    }

    return ret;
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

// TODO - snapping needs to be a post-processing step
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

#if 0
    // If snapping is enabled, apply it
    int snapped = 0;
    if (v->gv_s) {
	if (v->gv_s->gv_snap_lines) {
	    snapped = bv_snap_lines_2d(v, sobjs, fx, fy);
	}
	if (!snapped && v->gv_s->gv_grid.snap) {
	    bv_snap_grid_2d(v, fx, fy);
	}
    }
#endif

    return 0;
}

int
bv_screen_pt(point_t *p, fastf_t x, fastf_t y, struct bview *v)
{
    if (!p || !v)
	return -1;

    if (!v->gv_width || !v->gv_height)
	return -1;

    fastf_t tx, ty;
    if (bv_screen_to_view(v, &tx, &ty, x, y))
	return -1;

    point_t vpt;
    VSET(vpt, tx, ty, 0);
    MAT4X3PNT(*p, v->gv_view2model, vpt);
    return 0;
}

int
bv_view_plane(plane_t *p, struct bview *v)
{
    if (!p || !v)
	return -1;

    point_t cpt = VINIT_ZERO;
    vect_t vnrml = VINIT_ZERO;

    MAT_DELTAS_GET_NEG(cpt, v->gv_center);
    VMOVEN(vnrml, v->gv_rotation + 8, 3);
    VUNITIZE(vnrml);
    VSCALE(vnrml, vnrml, -1.0);

    return bg_plane_pt_nrml(p, cpt, vnrml);
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


// If a higher level of the code wants to use bu_list to reuse
// scene objects, it will look like this:
//
// Get:
// s = BU_LIST_NEXT(bv_scene_obj, &free_scene_obj->l);
// BU_LIST_DEQUEUE(&((s)->l));
// s->free_scene_obj = free_scene_obj;
// s->vlfree = vlfree;
//
// Put:
// FREE_BV_SCENE_OBJ(s, &s->free_scene_obj->l);
struct bv_scene_obj *
bv_obj_create(int type, struct bv_scene_obj *free_objs)
{
    struct bv_scene_obj *s = NULL;

    if (free_objs) {
	s = BU_LIST_NEXT(bv_scene_obj, &free_objs->l);
	BU_LIST_DEQUEUE(&((s)->l));
	s->free_scene_obj = free_objs;
    } else {
	BU_ALLOC(s, struct bv_scene_obj);
	s->i = new bv_scene_obj_internal;
    }

    // Zero out callback pointers
    s->s_type_flags = 0;
    s->s_free_callback = NULL;
    s->s_dlist_free_callback = NULL;

    // Use reset to do most of the initialization
    bv_obj_reset(s);

    // Set the type flag(s) on the object itself
    s->s_type_flags = type;

    return s;
}

struct bv_scene_obj *
bv_obj_create_child(struct bv_scene_obj *sp)
{
    if (!sp)
	return NULL;

    bv_log(1, "bv_obj_create_child %s(%s)", bu_vls_cstr(&sp->s_name), bu_vls_cstr(&sp->s_v->gv_name));

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

    // Use reset to do most of the initialization
    bv_obj_reset(s);

    bu_vls_sprintf(&s->s_name, "child:%s:%zd", bu_vls_cstr(&sp->s_name), BU_PTBL_LEN(&sp->children));

    s->s_v = sp->s_v;
    s->dp = sp->dp;
    s->free_scene_obj = sp->free_scene_obj;
    s->vlfree = sp->vlfree;

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
    } else {
	BU_PTBL_INIT(&s->children);
    }
    bu_ptbl_reset(&s->children);

    // If we have a callback for the internal data, use it
    if (s->s_free_callback)
	(*s->s_free_callback)(s);
    s->s_free_callback = NULL;

    // If we have a callback for the display list data, use it
    if (s->s_dlist_free_callback)
	(*s->s_dlist_free_callback)(s);
    s->s_dlist_free_callback = NULL;

    // If we have a label, do the label freeing steps
    // TODO - this should be using the free callback rather
    // than special casing...
    if (s->s_type_flags & BV_LABELS) {
	struct bv_label *la = (struct bv_label *)s->s_i_data;
	bu_vls_free(&la->label);
	BU_PUT(la, struct bv_label);
    }

    // Free vlist data.  If we have an associated vlfree list for
    // reuse use that, otherwise free it for real.
    if (BU_LIST_IS_INITIALIZED(&s->s_vlist)) {
	if (s->vlfree) {
	    BV_FREE_VLIST(s->vlfree, &s->s_vlist);
	} else {
	    struct bu_list *p;
	    while (BU_LIST_WHILE(p, bu_list, &s->s_vlist)) {
		BU_LIST_DEQUEUE(p);
		struct bv_vlist *pv = (struct bv_vlist *)p;
		BU_FREE(pv, struct bv_vlist);
	    }

	}
    }
    BU_LIST_INIT(&(s->s_vlist));

    if (!BU_VLS_IS_INITIALIZED(&s->s_name))
	BU_VLS_INIT(&s->s_name);
    bu_vls_trunc(&s->s_name, 0);

    struct bv_obj_settings defaults = BV_OBJ_SETTINGS_INIT;
    bv_obj_settings_sync(&s->s_local_os, &defaults);
    s->s_os = &s->s_local_os;
    s->s_override_child_settings = 0;

    MAT_IDN(s->s_mat);
    VSET(s->s_color, 255, 0, 0);
    VSETALL(s->bmax, -INFINITY);
    VSETALL(s->bmin, INFINITY);
    VSETALL(s->s_center, 0);
    s->adaptive_wireframe = 0;
    s->bot_threshold = 0;
    s->csg_obj = 0;
    s->current = 0;
    s->curve_scale = 0;
    s->dp = NULL;
    s->draw_data = NULL;
    s->have_bbox = 0;
    s->mesh_obj = 0;
    s->point_scale = 0;
    s->s_arrow = 0;
    s->s_csize = 0;
    s->s_flag = UP;
    s->s_force_draw = 0;
    s->s_i_data = NULL;
    s->s_iflag = DOWN;
    s->s_path = NULL;
    s->s_size = 0;
    s->s_soldash = 0;
    s->s_update_callback = NULL;
    s->s_v = NULL;
    s->view_scale = 0;

    // Set timestamp
    s->timestamp = bu_gettime();
}

#define FREE_BV_SCENE_OBJ(p, fp) { \
    BU_LIST_APPEND(fp, &((p)->l)); }

void
bv_obj_put(struct bv_scene_obj *s)
{
    bv_log(1, "bv_obj_put %s[%s]", bu_vls_cstr(&s->s_name), (s->s_v) ? bu_vls_cstr(&s->s_v->gv_name) : "NULL");
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_group *cg = (struct bv_scene_group *)BU_PTBL_GET(&s->children, i);
	bv_obj_put(cg);
    }

    // If this object was selected for snapping, it is no longer a valid candidate
    // TODO - manage this at a higher level
    if (s->s_v)
	bu_ptbl_rm(&s->s_v->gv_s->gv_snap_objs, (long *)s);

    bv_obj_reset(s);

    if (s->free_scene_obj) {
	FREE_BV_SCENE_OBJ(s, &s->free_scene_obj->l);
    } else {
	bu_vls_free(&s->s_name);
	BU_FREE(s, struct bv_scene_obj);
    }
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

    return NULL;
}

int
bv_scene_obj_bound(struct bv_scene_obj *s)
{
    int cmd;
    VSET(s->bmin, INFINITY, INFINITY, INFINITY);
    VSET(s->bmax, -INFINITY, -INFINITY, -INFINITY);
    int calc = 0;
    if (s->s_type_flags & BV_MESH_LOD) {
	struct bv_mesh_lod *i = (struct bv_mesh_lod *)s->draw_data;
	if (i) {
	    point_t obmin, obmax;
	    VMOVE(obmin, i->bmin);
	    VMOVE(obmax, i->bmax);
	    // Apply the scene matrix to the bounding box values to bound this
	    // instance, since the BV_MESH_LOD data is based on the
	    // non-instanced mesh.
	    MAT4X3PNT(s->bmin, s->s_mat, obmin);
	    MAT4X3PNT(s->bmax, s->s_mat, obmax);
	    calc = 1;
	}
    } else if (bu_list_len(&s->s_vlist)) {
	int dismode;
	cmd = bv_vlist_bbox(&s->s_vlist, &s->bmin, &s->bmax, NULL, &dismode);
	if (cmd) {
	    bu_log("unknown vlist op %d\n", cmd);
	}
	s->s_displayobj = dismode;
	calc = 1;
    }
    if (!calc) {
	// If nothing else has given us an answer, see if other views
	// can help
	std::unordered_map<struct bview *, struct bv_scene_obj *>::iterator vo_it;
	for (vo_it = s->i->vobjs.begin(); vo_it != s->i->vobjs.end(); vo_it++) {
	    struct bv_scene_obj *lv = vo_it->second;
	    if (lv->s_type_flags & BV_MESH_LOD) {
		struct bv_mesh_lod *i = (struct bv_mesh_lod *)lv->draw_data;
		if (i) {
		    point_t obmin, obmax;
		    VMOVE(obmin, i->bmin);
		    VMOVE(obmax, i->bmax);
		    // Apply the scene matrix to the bounding box values to bound this
		    // instance, since the BV_MESH_LOD data is based on the
		    // non-instanced mesh.
		    MAT4X3PNT(lv->bmin, lv->s_mat, obmin);
		    MAT4X3PNT(lv->bmax, lv->s_mat, obmax);
		    calc = 1;
		}
	    } else if (bu_list_len(&lv->s_vlist)) {
		int dismode;
		cmd = bv_vlist_bbox(&lv->s_vlist, &lv->bmin, &lv->bmax, NULL, &dismode);
		if (cmd) {
		    bu_log("unknown vlist op %d\n", cmd);
		}
		calc = 1;
	    }
	    if (calc) {
		VMOVE(s->bmin, lv->bmin);
		VMOVE(s->bmax, lv->bmax);
		break;
	    }
	}
    }
    if (calc) {
	s->s_center[X] = (s->bmin[X] + s->bmax[X]) * 0.5;
	s->s_center[Y] = (s->bmin[Y] + s->bmax[Y]) * 0.5;
	s->s_center[Z] = (s->bmin[Z] + s->bmax[Z]) * 0.5;

	s->s_size = s->bmax[X] - s->bmin[X];
	V_MAX(s->s_size, s->bmax[Y] - s->bmin[Y]);
	V_MAX(s->s_size, s->bmax[Z] - s->bmin[Z]);

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

int
bv_illum_obj(struct bv_scene_obj *s, char ill_state)
{
    bool changed = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, i);
	int cchanged = bv_illum_obj(s_c, ill_state);
	if (cchanged)
	    changed = 1;
    }
    if (ill_state != s->s_iflag) {
	changed = 1;
	s->s_iflag = ill_state;
	//bv_obj_stale(s);
    }
    std::unordered_map<struct bview *, struct bv_scene_obj *>::iterator vo_it;
    for (vo_it = s->i->vobjs.begin(); vo_it != s->i->vobjs.end(); vo_it++) {
	int cchanged = bv_illum_obj(vo_it->second, ill_state);
	if (cchanged)
	    changed = 1;
    }

    return changed;
}

//#define USE_BV_LOG
void
#ifdef USE_BV_LOG
bv_log(int level, const char *fmt, ...)
#else
bv_log(int UNUSED(level), const char *UNUSED(fmt), ...)
#endif
{
#ifdef USE_BV_LOG
    if (level < 0 || !fmt)
	return;
    const char *brsig = getenv("BV_LOG");
    if (!brsig)
	return;
    if (brsig) {
	int blev = atoi(brsig);
	if (blev < level)
	    return;
    }

    va_list ap;
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    va_start(ap, fmt);
    bu_vls_vprintf(&msg, fmt, ap);
    bu_log("%s\n", bu_vls_cstr(&msg));
    bu_vls_free(&msg);
    va_end(ap);
#endif
}

void
bv_view_print(const char *title, struct bview *v, int UNUSED(verbosity))
{
    if (!v)
	return;

    struct bu_vls vtitle = BU_VLS_INIT_ZERO;
    if (title) {
	bu_vls_sprintf(&vtitle, "%s", title);
    } else {
	bu_vls_sprintf(&vtitle, "%s", bu_vls_cstr(&v->gv_name));
    }

    bu_log("%s\n", bu_vls_cstr(&vtitle));
    bu_vls_free(&vtitle);

    bu_log("Size info:\n");
    bu_log("  i_scale:      %f\n", v->gv_i_scale);
    bu_log("  a_scale:      %f\n", v->gv_a_scale);
    bu_log("  scale:        %f\n", v->gv_scale);
    bu_log("  size:         %f\n", v->gv_size);
    bu_log("  isize:        %f\n", v->gv_isize);
    bu_log("  base2local:   %f\n", v->gv_base2local);
    bu_log("  local2base:   %f\n", v->gv_local2base);
    bu_log("  rscale:       %f\n", v->gv_rscale);
    bu_log("  sscale:       %f\n", v->gv_sscale);

    bu_log("Window info:");
    bu_log("  width:        %d\n", v->gv_width);
    bu_log("  height:       %d\n", v->gv_height);
    bu_log("  wmin:         %f\n, %f", v->gv_wmin[0], v->gv_wmin[1]);
    bu_log("  wmax:         %f\n, %f", v->gv_wmax[0], v->gv_wmax[1]);

    bu_log("Camera info:");
    bu_log("  perspective:  %f\n", v->gv_perspective);
    bu_log("  aet:          %f %f %f\n", V3ARGS(v->gv_aet));
    bu_log("  eye_pos:      %f %f %f\n", V3ARGS(v->gv_eye_pos));
    bu_log("  keypoint:     %f %f %f\n", V3ARGS(v->gv_keypoint));
    bu_log("  coord:        %c\n", v->gv_coord);
    bu_log("  rotate_about: %c\n", v->gv_rotate_about);
    bn_mat_print("rotation", v->gv_rotation);
    bn_mat_print("center", v->gv_center);
    bn_mat_print("model2view", v->gv_model2view);
    bn_mat_print("pmodel2view", v->gv_pmodel2view);
    bn_mat_print("view2model", v->gv_view2model);
    bn_mat_print("perspective", v->gv_pmat);

    bu_log("Keyboard/mouse info:");
    bu_log("  prevMouseX:   %f\n", v->gv_prevMouseX);
    bu_log("  prevMouseY:   %f\n", v->gv_prevMouseY);
    bu_log("  mouse_x:      %d\n", v->gv_mouse_x);
    bu_log("  mouse_y:      %d\n", v->gv_mouse_y);
    bu_log("  gv_prev_point:%f %f %f\n", V3ARGS(v->gv_prev_point));
    bu_log("  gv_point:     %f %f %f\n", V3ARGS(v->gv_point));
    bu_log("  key:          %c\n", v->gv_key);
    bu_log("  mod_flags:    %ld\n", v->gv_mod_flags);
    bu_log("  minMousedelta:%f\n", v->gv_minMouseDelta);
    bu_log("  maxMousedelta:%f\n", v->gv_maxMouseDelta);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
