/*                      H A S H . C
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
#include "bv/view_sets.h"
#define XXH_STATIC_LINKING_ONLY
#define XXH_IMPLEMENTATION
#include "xxhash.h"

static void
_bv_adc_state_hash(XXH64_state_t *state, struct bv_adc_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, v, sizeof(struct bv_adc_state));
}

static void
_bv_axes_hash(XXH64_state_t *state, struct bv_axes *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, v, sizeof(struct bv_axes));
}

static void
_bv_data_arrow_state_hash(XXH64_state_t *state, struct bv_data_arrow_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, v, sizeof(struct bv_data_arrow_state));
}

static void
_bv_data_axes_state_hash(XXH64_state_t *state, struct bv_data_axes_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, v, sizeof(struct bv_data_axes_state));
    if (v->num_points)
	XXH64_update(state, v->points, v->num_points * sizeof(point_t));
}

static void
_bv_data_label_state_hash(XXH64_state_t *state, struct bv_data_label_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, v, sizeof(struct bv_data_label_state));
    if (v->gdls_size)
	XXH64_update(state, v->gdls_points, v->gdls_size * sizeof(point_t));
    for (int i = 0; i < v->gdls_num_labels; i++) {
	if (strlen(v->gdls_labels[i]))
	    XXH64_update(state, v->gdls_labels[i], strlen(v->gdls_labels[i]));
    }
}


static void
_bv_data_line_state_hash(XXH64_state_t *state, struct bv_data_line_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, v, sizeof(struct bv_data_line_state));
    if (v->gdls_num_points)
	XXH64_update(state, v->gdls_points, v->gdls_num_points * sizeof(point_t));
}

static void
_bg_poly_contour_hash(XXH64_state_t *state, struct bg_poly_contour *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, v, sizeof(struct bg_poly_contour));
    if (v->num_points)
	XXH64_update(state, v->point, v->num_points * sizeof(point_t));
}

static void
_bg_polygon_hash(XXH64_state_t *state, struct bg_polygon *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, v, sizeof(struct bg_polygon));

    for (size_t i = 0; i < v->num_contours; i++) {
	_bg_poly_contour_hash(state, &v->contour[i]);
    }
    if (v->hole && v->num_contours) {
	XXH64_update(state, v->hole, v->num_contours * sizeof(int));
    }
}

static void
_bg_polygons_hash(XXH64_state_t *state, struct bg_polygons *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, v, sizeof(struct bg_polygons));
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

    XXH64_update(state, v, sizeof(bv_data_polygon_state));
    _bg_polygons_hash(state, &v->gdps_polygons);
}

static void
_bv_grid_state_hash(XXH64_state_t *state, struct bv_grid_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, v, sizeof(struct bv_grid_state));
}

static void
_bv_other_state_hash(XXH64_state_t *state, struct bv_other_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, v, sizeof(struct bv_other_state));
}


static void
_bv_interactive_rect_state_hash(XXH64_state_t *state, struct bv_interactive_rect_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, v, sizeof(struct bv_interactive_rect_state));
}

static void
_bv_obj_settings_hash(XXH64_state_t *state, struct bv_obj_settings *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    XXH64_update(state, v, sizeof(struct bv_obj_settings));
}

void
bv_scene_obj_hash(XXH64_state_t *state, struct bv_scene_obj *s)
{
    /* First, do sanity checks */
    if (!s || !state)
	return;

    XXH64_update(state, s, sizeof(struct bv_scene_obj));
    struct bv_vlist *tvp;
    for (BU_LIST_FOR(tvp, bv_vlist, &((struct bv_vlist *)&s->s_vlist)->l)) {
	XXH64_update(state, tvp, sizeof(struct bv_vlist));
    }
    if (s->s_os)
	_bv_obj_settings_hash(state, s->s_os);
    _bv_obj_settings_hash(state, &s->s_local_os);
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

	XXH64_update(state, gdlp, sizeof(struct display_list));
	XXH64_update(state, bu_vls_cstr(&gdlp->dl_path), bu_vls_strlen(&gdlp->dl_path));

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    bv_scene_obj_hash(state, sp);
	}

	gdlp = next_gdlp;
    }

    hash_val = XXH64_digest(state);
    XXH64_freeState(state);

    return (unsigned long long)hash_val;
}

void
bv_settings_hash(XXH64_state_t *state, struct bview_settings *s)
{
    XXH64_update(state, s, sizeof(struct bview_settings));

    _bv_obj_settings_hash(state, &s->obj_s);
    _bv_adc_state_hash(state, &s->gv_adc);
    _bv_axes_hash(state, &s->gv_model_axes);
    _bv_axes_hash(state, &s->gv_view_axes);
    _bv_grid_state_hash(state, &s->gv_grid);
    _bv_other_state_hash(state, &s->gv_center_dot);
    _bv_other_state_hash(state, &s->gv_view_params);
    _bv_other_state_hash(state, &s->gv_view_scale);
    _bv_interactive_rect_state_hash(state, &s->gv_rect);

#if 0
    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_selected); i++) {
	long *p = BU_PTBL_GET(v->gv_selected, i);
	XXH64_update(state, p, sizeof(long *));
    }
#endif

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
    XXH64_update(state, v, sizeof(struct bview));

    if (v->gv_s)
	bv_settings_hash(state, v->gv_s);
    bv_settings_hash(state, &v->gv_ls);

    _bv_data_arrow_state_hash(state, &v->gv_tcl.gv_data_arrows);
    _bv_data_axes_state_hash(state, &v->gv_tcl.gv_data_axes);
    _bv_data_label_state_hash(state, &v->gv_tcl.gv_data_labels);
    _bv_data_line_state_hash(state, &v->gv_tcl.gv_data_lines);
    _bv_data_polygon_state_hash(state, &v->gv_tcl.gv_data_polygons);
    _bv_data_arrow_state_hash(state, &v->gv_tcl.gv_sdata_arrows);
    _bv_data_axes_state_hash(state, &v->gv_tcl.gv_sdata_axes);
    _bv_data_label_state_hash(state, &v->gv_tcl.gv_sdata_labels);
    _bv_data_line_state_hash(state, &v->gv_tcl.gv_sdata_lines);
    _bv_data_polygon_state_hash(state, &v->gv_tcl.gv_sdata_polygons);
    _bv_other_state_hash(state, &v->gv_tcl.gv_prim_labels);

    struct bu_ptbl *tbls[4];
    tbls[0] = bv_view_objs(v, BV_DB_OBJS);
    tbls[1] = bv_view_objs(v, BV_DB_OBJS | BV_LOCAL_OBJS);
    tbls[2] = bv_view_objs(v, BV_VIEW_OBJS);
    tbls[3] = bv_view_objs(v, BV_VIEW_OBJS | BV_LOCAL_OBJS);
    for (int t = 0; t < 4; t++) {
	for (size_t i = 0; i < BU_PTBL_LEN(tbls[t]); i++) {
	    struct bv_scene_group *g = (struct bv_scene_group *)BU_PTBL_GET(tbls[t], i);
	    if (BU_PTBL_IS_INITIALIZED(&g->children)) {
		for (size_t j = 0; j < BU_PTBL_LEN(&g->children); j++) {
		    struct bv_scene_obj *s_c = (struct bv_scene_obj *)BU_PTBL_GET(&g->children, j);
		    bv_scene_obj_hash(state, s_c);
		}
	    }
	    bv_scene_obj_hash(state, g);
	}
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
