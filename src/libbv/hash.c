/*                      H A S H . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file hash.c
 *
 * Calculate hashes of bv containers
 *
 */

#include "common.h"
#include <string.h>
#include "vmath.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bn/mat.h"
#include "bv/vlist.h"
#include "bv/defines.h"
#include "bv/util.h"
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"

static void
_bv_adc_state_hash(XXH64_state_t *state, struct bv_adc_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->draw, sizeof(int));
    XXH64_update(state, &v->dv_x, sizeof(int));
    XXH64_update(state, &v->dv_y, sizeof(int));
    XXH64_update(state, &v->dv_a1, sizeof(int));
    XXH64_update(state, &v->dv_a2, sizeof(int));
    XXH64_update(state, &v->dv_dist, sizeof(int));
    XXH64_update(state, &v->pos_model, sizeof(fastf_t[3]));
    XXH64_update(state, &v->pos_view, sizeof(fastf_t[3]));
    XXH64_update(state, &v->pos_grid, sizeof(fastf_t[3]));
    XXH64_update(state, &v->a1, sizeof(fastf_t));
    XXH64_update(state, &v->a2, sizeof(fastf_t));
    XXH64_update(state, &v->dst, sizeof(fastf_t));
    XXH64_update(state, &v->anchor_pos, sizeof(int));
    XXH64_update(state, &v->anchor_a1, sizeof(int));
    XXH64_update(state, &v->anchor_a2, sizeof(int));
    XXH64_update(state, &v->anchor_dst, sizeof(int));
    XXH64_update(state, &v->anchor_pt_a1, sizeof(fastf_t[3]));
    XXH64_update(state, &v->anchor_pt_a2, sizeof(fastf_t[3]));
    XXH64_update(state, &v->anchor_pt_dst, sizeof(fastf_t[3]));
    XXH64_update(state, &v->line_color, sizeof(int[3]));
    XXH64_update(state, &v->tick_color, sizeof(int[3]));
    XXH64_update(state, &v->line_width, sizeof(int));
}

static void
_bv_axes_hash(XXH64_state_t *state, struct bv_axes *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->draw, sizeof(int));
    XXH64_update(state, &v->axes_pos, sizeof(point_t));
    XXH64_update(state, &v->axes_size, sizeof(fastf_t));
    XXH64_update(state, &v->line_width, sizeof(int));
    XXH64_update(state, &v->pos_only, sizeof(int));
    XXH64_update(state, &v->axes_color, sizeof(int[3]));
    XXH64_update(state, &v->label_color, sizeof(int[3]));
    XXH64_update(state, &v->triple_color, sizeof(int));
    XXH64_update(state, &v->tick_enabled, sizeof(int));
    XXH64_update(state, &v->tick_length, sizeof(int));
    XXH64_update(state, &v->tick_major_length, sizeof(int));
    XXH64_update(state, &v->tick_interval, sizeof(fastf_t));
    XXH64_update(state, &v->ticks_per_major, sizeof(int));
    XXH64_update(state, &v->tick_threshold, sizeof(int));
    XXH64_update(state, &v->tick_color, sizeof(int[3]));
    XXH64_update(state, &v->tick_major_color, sizeof(int[3]));
}

static void
_bv_data_arrow_state_hash(XXH64_state_t *state, struct bv_data_arrow_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->gdas_draw, sizeof(int));
    XXH64_update(state, &v->gdas_color, sizeof(int[3]));
    XXH64_update(state, &v->gdas_line_width, sizeof(int));
    XXH64_update(state, &v->gdas_tip_length, sizeof(int));
    XXH64_update(state, &v->gdas_tip_width, sizeof(int));
    XXH64_update(state, &v->gdas_num_points, sizeof(int));
    for (int i = 0; i < v->gdas_num_points; i++) {
	XXH64_update(state, &v->gdas_points[i], sizeof(point_t));
    }
}

static void
_bv_data_axes_state_hash(XXH64_state_t *state, struct bv_data_axes_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->draw, sizeof(int));
    XXH64_update(state, &v->color, sizeof(int[3]));
    XXH64_update(state, &v->line_width, sizeof(int));
    XXH64_update(state, &v->size, sizeof(fastf_t));
    XXH64_update(state, &v->num_points, sizeof(int));
    for (int i = 0; i < v->num_points; i++) {
	XXH64_update(state, &v->points[i], sizeof(point_t));
    }
}

static void
_bv_data_label_state_hash(XXH64_state_t *state, struct bv_data_label_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->gdls_draw, sizeof(int));
    XXH64_update(state, &v->gdls_color, sizeof(int[3]));
    XXH64_update(state, &v->gdls_num_labels, sizeof(int));
    XXH64_update(state, &v->gdls_size, sizeof(int));
    for (int i = 0; i < v->gdls_num_labels; i++) {
	if (strlen(v->gdls_labels[i]))
	    XXH64_update(state, v->gdls_labels[i], strlen(v->gdls_labels[i]));
    }
    for (int i = 0; i < v->gdls_size; i++) {
	XXH64_update(state, &v->gdls_points[i], sizeof(point_t));
    }
}


static void
_bv_data_line_state_hash(XXH64_state_t *state, struct bv_data_line_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->gdls_draw, sizeof(int));
    XXH64_update(state, &v->gdls_color, sizeof(int[3]));
    XXH64_update(state, &v->gdls_line_width, sizeof(int));
    XXH64_update(state, &v->gdls_num_points, sizeof(int));
    for (int i = 0; i < v->gdls_num_points; i++) {
	XXH64_update(state, &v->gdls_points[i], sizeof(point_t));
    }
}

static void
_bg_poly_contour_hash(XXH64_state_t *state, struct bg_poly_contour *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->num_points, sizeof(size_t));
    for (size_t i = 0; i < v->num_points; i++) {
	XXH64_update(state, &v->point[i], sizeof(point_t));
    }
}

static void
_bg_polygon_hash(XXH64_state_t *state, struct bg_polygon *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->num_contours, sizeof(size_t));
    XXH64_update(state, &v->gp_color, sizeof(int[3]));
    XXH64_update(state, &v->gp_line_width, sizeof(int));
    XXH64_update(state, &v->gp_line_style, sizeof(int));

    for (size_t i = 0; i < v->num_contours; i++) {
	_bg_poly_contour_hash(state, &v->contour[i]);
    }
    if (v->hole) {
	for (size_t i = 0; i < v->num_contours; i++) {
	    XXH64_update(state, &v->hole[i], sizeof(int));
	}
    }
}

static void
_bg_polygons_hash(XXH64_state_t *state, struct bg_polygons *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->num_polygons, sizeof(size_t));
    for (size_t i = 0; i < v->num_polygons; i++) {
	_bg_polygon_hash(state, &v->polygon[i]);
    }
}

static void
_bv_data_polygon_state_hash(XXH64_state_t *state, bv_data_polygon_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->gdps_draw, sizeof(int));
    XXH64_update(state, &v->gdps_moveAll, sizeof(int));
    XXH64_update(state, &v->gdps_color, sizeof(int[3]));
    XXH64_update(state, &v->gdps_line_width, sizeof(int));
    XXH64_update(state, &v->gdps_line_style, sizeof(int));
    XXH64_update(state, &v->gdps_cflag, sizeof(int));
    XXH64_update(state, &v->gdps_target_polygon_i, sizeof(size_t));
    XXH64_update(state, &v->gdps_curr_polygon_i, sizeof(size_t));
    XXH64_update(state, &v->gdps_curr_point_i, sizeof(size_t));
    XXH64_update(state, &v->gdps_prev_point, sizeof(point_t));
    XXH64_update(state, &v->gdps_clip_type, sizeof(bg_clip_t));
    XXH64_update(state, &v->gdps_scale, sizeof(fastf_t));
    XXH64_update(state, &v->gdps_origin, sizeof(point_t));
    XXH64_update(state, &v->gdps_rotation, sizeof(mat_t));
    XXH64_update(state, &v->gdps_view2model, sizeof(mat_t));
    XXH64_update(state, &v->gdps_model2view, sizeof(mat_t));
    _bg_polygons_hash(state, &v->gdps_polygons);
    XXH64_update(state, &v->gdps_data_vZ, sizeof(fastf_t));
}

static void
_bv_grid_state_hash(XXH64_state_t *state, struct bv_grid_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->rc, sizeof(int));
    XXH64_update(state, &v->draw, sizeof(int));
    XXH64_update(state, &v->snap, sizeof(int));
    XXH64_update(state, &v->anchor, sizeof(fastf_t[3]));
    XXH64_update(state, &v->res_h, sizeof(fastf_t));
    XXH64_update(state, &v->res_v, sizeof(fastf_t));
    XXH64_update(state, &v->res_major_h, sizeof(int));
    XXH64_update(state, &v->res_major_v, sizeof(int));
    XXH64_update(state, &v->color, sizeof(int[3]));
}

static void
_bv_other_state_hash(XXH64_state_t *state, struct bv_other_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->gos_draw, sizeof(int));
    XXH64_update(state, &v->gos_line_color, sizeof(int[3]));
    XXH64_update(state, &v->gos_text_color, sizeof(int[3]));
}


static void
_bv_interactive_rect_state_hash(XXH64_state_t *state, struct bv_interactive_rect_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->active, sizeof(int));
    XXH64_update(state, &v->draw, sizeof(int));
    XXH64_update(state, &v->line_width, sizeof(int));
    XXH64_update(state, &v->line_style, sizeof(int));
    XXH64_update(state, &v->pos, sizeof(int[2]));
    XXH64_update(state, &v->dim, sizeof(int[2]));
    XXH64_update(state, &v->x, sizeof(fastf_t));
    XXH64_update(state, &v->y, sizeof(fastf_t));
    XXH64_update(state, &v->width, sizeof(fastf_t));
    XXH64_update(state, &v->height, sizeof(fastf_t));
    XXH64_update(state, &v->bg, sizeof(int[3]));
    XXH64_update(state, &v->color, sizeof(int[3]));
    XXH64_update(state, &v->cdim, sizeof(int[2]));
    XXH64_update(state, &v->aspect, sizeof(fastf_t));
}

static void
_bv_settings_hash(XXH64_state_t *state, struct bv_settings *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, &v->draw_non_subtract_only, sizeof(int));
    XXH64_update(state, &v->draw_solid_lines_only, sizeof(int));
    XXH64_update(state, &v->s_arrow_tip_length, sizeof(fastf_t));
    XXH64_update(state, &v->s_arrow_tip_width, sizeof(fastf_t));
    XXH64_update(state, &v->s_dmode, sizeof(int));
    XXH64_update(state, &v->s_line_width, sizeof(int));
    XXH64_update(state, &v->transparency, sizeof(fastf_t));
}

void
bv_scene_obj_hash(XXH64_state_t *state, struct bv_scene_obj *s)
{
    XXH64_update(state, &s->s_size, sizeof(fastf_t));
    XXH64_update(state, &s->s_csize, sizeof(fastf_t));
    XXH64_update(state, &s->s_center, sizeof(vect_t));
    struct bv_vlist *tvp;
    for (BU_LIST_FOR(tvp, bv_vlist, &((struct bv_vlist *)&s->s_vlist)->l)) {
	XXH64_update(state, &tvp->nused, sizeof(size_t));
	XXH64_update(state, &tvp->cmd, sizeof(int[BV_VLIST_CHUNK]));
	XXH64_update(state, &tvp->pt, sizeof(point_t[BV_VLIST_CHUNK]));
    }
    XXH64_update(state, &s->s_vlen, sizeof(int));
    XXH64_update(state, &s->s_flag, sizeof(char));
    XXH64_update(state, &s->s_iflag, sizeof(char));
    XXH64_update(state, &s->s_arrow, sizeof(int));
    XXH64_update(state, &s->s_soldash, sizeof(char));
    XXH64_update(state, &s->s_old.s_Eflag, sizeof(char));
    XXH64_update(state, &s->s_old.s_uflag, sizeof(char));
    XXH64_update(state, &s->s_old.s_dflag, sizeof(char));
    XXH64_update(state, &s->s_old.s_cflag, sizeof(char));
    XXH64_update(state, &s->s_old.s_wflag, sizeof(char));
    XXH64_update(state, &s->s_changed, sizeof(int));
    XXH64_update(state, &s->s_old.s_basecolor, sizeof(unsigned char[3]));
    XXH64_update(state, &s->s_color, sizeof(unsigned char[3]));
    XXH64_update(state, &s->s_old.s_regionid, sizeof(short));
    XXH64_update(state, &s->s_dlist, sizeof(unsigned int));
    XXH64_update(state, &s->s_mat, sizeof(mat_t));
    XXH64_update(state, &s->adaptive_wireframe, sizeof(int));
    XXH64_update(state, &s->view_scale, sizeof(fastf_t));
    XXH64_update(state, &s->bot_threshold, sizeof(size_t));
    XXH64_update(state, &s->curve_scale, sizeof(fastf_t));
    XXH64_update(state, &s->point_scale, sizeof(fastf_t));

    _bv_settings_hash(state, &s->s_os);
}

unsigned long long
bv_dl_hash(struct display_list *dl)
{
    if (!dl)
	return 0;

    XXH64_hash_t hash_val;
    XXH64_state_t *state;
    state = XXH64_createState();
    if (!state)
	return 0;
    XXH64_reset(state, 0);

    struct display_list *gdlp;
    struct display_list *next_gdlp;
    struct bv_scene_obj *sp;

    gdlp = BU_LIST_NEXT(display_list, (struct bu_list *)dl);
    while (BU_LIST_NOT_HEAD(gdlp, dl)) {
	next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	XXH64_update(state, bu_vls_cstr(&gdlp->dl_path), bu_vls_strlen(&gdlp->dl_path));
	XXH64_update(state, &gdlp->dl_wflag, sizeof(int));

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    bv_scene_obj_hash(state, sp);
	}

	gdlp = next_gdlp;
    }

    hash_val = XXH64_digest(state);
    XXH64_freeState(state);

    return (unsigned long long)hash_val;
}


unsigned long long
bv_hash(struct bview *v)
{
    if (!v)
	return 0;

    XXH64_hash_t hash_val;
    XXH64_state_t *state;
    state = XXH64_createState();
    if (!state)
	return 0;
    XXH64_reset(state, 0);

    // Deliberately not checking name - a rename doesn't change the view
    XXH64_update(state, &v->gv_i_scale, sizeof(fastf_t));
    XXH64_update(state, &v->gv_a_scale, sizeof(fastf_t));
    XXH64_update(state, &v->gv_scale, sizeof(fastf_t));
    XXH64_update(state, &v->gv_size, sizeof(fastf_t));
    XXH64_update(state, &v->gv_isize, sizeof(fastf_t));
    XXH64_update(state, &v->gv_perspective, sizeof(fastf_t));
    XXH64_update(state, &v->gv_aet, sizeof(vect_t));
    XXH64_update(state, &v->gv_eye_pos, sizeof(vect_t));
    XXH64_update(state, &v->gv_keypoint, sizeof(vect_t));
    XXH64_update(state, &v->gv_coord, sizeof(char));
    XXH64_update(state, &v->gv_rotate_about, sizeof(char));
    XXH64_update(state, &v->gv_rotation, sizeof(mat_t));
    XXH64_update(state, &v->gv_center, sizeof(mat_t));
    XXH64_update(state, &v->gv_model2view, sizeof(mat_t));
    XXH64_update(state, &v->gv_pmodel2view, sizeof(mat_t));
    XXH64_update(state, &v->gv_view2model, sizeof(mat_t));
    XXH64_update(state, &v->gv_pmat, sizeof(mat_t));
    XXH64_update(state, &v->gv_width, sizeof(int));
    XXH64_update(state, &v->gv_height, sizeof(int));
    XXH64_update(state, &v->gv_prevMouseX, sizeof(fastf_t));
    XXH64_update(state, &v->gv_prevMouseY, sizeof(fastf_t));
    XXH64_update(state, &v->gv_minMouseDelta, sizeof(fastf_t));
    XXH64_update(state, &v->gv_maxMouseDelta, sizeof(fastf_t));
    XXH64_update(state, &v->gv_rscale, sizeof(fastf_t));
    XXH64_update(state, &v->gv_sscale, sizeof(fastf_t));

    XXH64_update(state, &v->adaptive_plot, sizeof(int));
    XXH64_update(state, &v->bot_threshold, sizeof(size_t));
    XXH64_update(state, &v->curve_scale, sizeof(fastf_t));
    XXH64_update(state, &v->point_scale, sizeof(fastf_t));
    XXH64_update(state, &v->redraw_on_zoom, sizeof(int));

   _bv_settings_hash(state, &v->gvs);
    XXH64_update(state, &v->gv_zclip, sizeof(int));
    XXH64_update(state, &v->gv_cleared, sizeof(int));
    _bv_adc_state_hash(state, &v->gv_adc);
    _bv_axes_hash(state, &v->gv_model_axes);
    _bv_axes_hash(state, &v->gv_view_axes);
    _bv_data_arrow_state_hash(state, &v->gv_tcl.gv_data_arrows);
    _bv_data_axes_state_hash(state, &v->gv_tcl.gv_data_axes);
    _bv_data_label_state_hash(state, &v->gv_tcl.gv_data_labels);
    _bv_data_line_state_hash(state, &v->gv_tcl.gv_data_lines);
    _bv_data_polygon_state_hash(state, &v->gv_tcl.gv_data_polygons);
    _bv_data_arrow_state_hash(state, &v->gv_tcl.gv_sdata_arrows);
    _bv_data_axes_state_hash(state, &v->gv_tcl.gv_sdata_axes);
    _bv_data_label_state_hash(state, &v->gv_tcl.gv_sdata_labels);
    _bv_data_line_state_hash(state, &v->gv_tcl.gv_sdata_lines);
    XXH64_update(state, &v->gv_snap_lines, sizeof(int));
    XXH64_update(state, &v->gv_snap_tol_factor, sizeof(double));
    _bv_data_polygon_state_hash(state, &v->gv_tcl.gv_sdata_polygons);
    _bv_grid_state_hash(state, &v->gv_grid);
    _bv_other_state_hash(state, &v->gv_center_dot);
    _bv_other_state_hash(state, &v->gv_tcl.gv_prim_labels);
    _bv_other_state_hash(state, &v->gv_view_params);
    _bv_other_state_hash(state, &v->gv_view_scale);
    _bv_interactive_rect_state_hash(state, &v->gv_rect);
    XXH64_update(state, &v->gv_fps, sizeof(int));
    XXH64_update(state, &v->gv_data_vZ, sizeof(fastf_t));

    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_selected); i++) {
	long *p = BU_PTBL_GET(v->gv_selected, i);
	XXH64_update(state, p, sizeof(long *));
    }

    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_db_grps); i++) {
	struct bv_scene_group *g = (struct bv_scene_group *)BU_PTBL_GET(v->gv_db_grps, i);
	if (BU_PTBL_IS_INITIALIZED(&g->g->children)) {
	    for (size_t j = 0; j < BU_PTBL_LEN(&g->g->children); j++) {
		struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&g->g->children, j);
		bv_scene_obj_hash(state, s_c);
	    }
	}
	bv_scene_obj_hash(state, g->g);
    }

    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_view_objs); i++) {
	struct bv_scene_obj *s = (struct bv_scene_obj *)BU_PTBL_GET(v->gv_view_objs, i);
	if (BU_PTBL_IS_INITIALIZED(&s->children)) {
	    for (size_t j = 0; j < BU_PTBL_LEN(&s->children); j++) {
		struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&s->children, j);
		bv_scene_obj_hash(state, s_c);
	    }
	}
	bv_scene_obj_hash(state, s);
    }

    hash_val = XXH64_digest(state);
    XXH64_freeState(state);

    return (unsigned long long)hash_val;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
