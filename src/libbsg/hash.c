/*                      H A S H . C
 * BRL-CAD
 *
 * Copyright (c) 2020-2026 United States Government as represented by
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
#include "bu/hash.h"
#include "bu/log.h"
#include "bn/mat.h"
#include "bsg/defines.h"
#include "bsg/field.h"
#include "bsg/geometry.h"
#include "bsg/node.h"
#include "bsg/util.h"
#include "bsg/scene_object.h"
#include "bsg/view_set.h"
#include "appearance_private.h"
#include "draw_source_private.h"
#include "feature_private.h"
#include "field_private.h"
#include "geometry_private.h"
#include "material_private.h"
#include "node_private.h"
#include "node_storage_private.h"
#include "object_private.h"
#include "payload_private.h"
#include "payload_typed_private.h"
#include "view_state_private.h"

static void
_bsg_adc_state_hash(struct bu_data_hash_state *state, struct bsg_adc_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    bu_data_hash_update(state, v, sizeof(struct bsg_adc_state));
}

static void
_bsg_axes_hash(struct bu_data_hash_state *state, struct bsg_axes *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    bu_data_hash_update(state, v, sizeof(struct bsg_axes));
}

/* View-scoped feature geometry contributes to the view hash through the
 * feature visit below; feature payload revisions are the renderable state. */

static void
_bsg_grid_state_hash(struct bu_data_hash_state *state, struct bsg_grid_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    bu_data_hash_update(state, v, sizeof(struct bsg_grid_state));
}

static void
_bsg_params_state_hash(struct bu_data_hash_state *state, struct bsg_params_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    bu_data_hash_update(state, v, sizeof(struct bsg_params_state));
}

static void
_bsg_other_state_hash(struct bu_data_hash_state *state, struct bsg_other_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    bu_data_hash_update(state, v, sizeof(struct bsg_other_state));
}


static void
_bsg_interactive_rect_state_hash(struct bu_data_hash_state *state, struct bsg_interactive_rect_state *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    bu_data_hash_update(state, v, sizeof(struct bsg_interactive_rect_state));
}

static void
_bsg_appearance_settings_hash(struct bu_data_hash_state *state, const struct bsg_appearance_settings *v)
{
    /* First, do sanity checks */
    if (!v || !state)
	return;

    bu_data_hash_update(state, v, sizeof(struct bsg_appearance_settings));
}

static void
_bsg_hash_cstr(struct bu_data_hash_state *state, const char *str)
{
    size_t len = str ? strlen(str) : 0;
    bu_data_hash_update(state, &len, sizeof(len));
    if (len)
	bu_data_hash_update(state, str, len);
}

static void
_bsg_hash_field_revision(struct bu_data_hash_state *state,
			 struct bsg_node *node,
			 bsg_field_id_t field_id)
{
    uint64_t revision = bsg_field_revision(bsg_node_field_ref(node, field_id));
    bu_data_hash_update(state, &revision, sizeof(revision));
}

static void
_bsg_hash_payload_line_set(struct bu_data_hash_state *state,
			   const struct bsg_payload_line_set *ls)
{
    size_t point_cnt = ls ? ls->point_cnt : 0;
    bu_data_hash_update(state, &point_cnt, sizeof(point_cnt));
    if (!ls || !point_cnt)
	return;
    if (ls->points)
	bu_data_hash_update(state, ls->points, point_cnt * sizeof(point_t));
    if (ls->cmds)
	bu_data_hash_update(state, ls->cmds, point_cnt * sizeof(int));
}

void
bsg_scene_obj_hash(struct bu_data_hash_state *state, struct bsg_node *s)
{
    /* First, do sanity checks */
    if (!s || !state)
	return;

    bsg_type_id object_type = bsg_node_object_type(s);
    unsigned int source_flags = bsg_node_source_flags(s);
    unsigned int geometry_roles = bsg_node_geometry_roles(s);
    unsigned long long payload_type = bsg_node_get_payload_type(s);
    bsg_geometry_node_kind geometry_kind = bsg_geometry_node_kind_get(s);
    int visible = bsg_node_visible(s);
    int force_draw = bsg_appearance_force_draw(s);
    int inherited_settings = bsg_appearance_inherited_by_children(s);
    int highlighted = bsg_appearance_is_highlighted(s);
    int default_color = bsg_appearance_uses_default_color(s);
    int evaluated_region = bsg_appearance_legacy_eval_flag(s);
    int region_id = bsg_appearance_legacy_region_id(s);
    uint64_t node_revision = bsg_node_revision(s);
    uint64_t drawn_revision = bsg_appearance_drawn_rev(s);
    uint32_t material_revision = bsg_material_revision(s);
    fastf_t draw_size = bsg_node_draw_size(s);
    vect_t draw_center = VINIT_ZERO;
    mat_t transform;
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;
    int bounds_valid = 0;
    struct bsg_appearance_settings appearance;
    unsigned char material_rgb[3] = {0, 0, 0};

    bsg_node_get_draw_center(s, draw_center);
    MAT_IDN(transform);
    bsg_node_transform(s, transform);
    bounds_valid = bsg_node_bounds(s, bmin, bmax);
    bsg_appearance_settings_for_node(s, &appearance);
    bsg_material_get_rgb(s, &material_rgb[0], &material_rgb[1], &material_rgb[2]);

    bu_data_hash_update(state, &object_type, sizeof(object_type));
    bu_data_hash_update(state, &source_flags, sizeof(source_flags));
    bu_data_hash_update(state, &geometry_roles, sizeof(geometry_roles));
    bu_data_hash_update(state, &payload_type, sizeof(payload_type));
    bu_data_hash_update(state, &geometry_kind, sizeof(geometry_kind));
    bu_data_hash_update(state, &visible, sizeof(visible));
    bu_data_hash_update(state, &force_draw, sizeof(force_draw));
    bu_data_hash_update(state, &inherited_settings, sizeof(inherited_settings));
    bu_data_hash_update(state, &highlighted, sizeof(highlighted));
    bu_data_hash_update(state, &default_color, sizeof(default_color));
    bu_data_hash_update(state, &evaluated_region, sizeof(evaluated_region));
    bu_data_hash_update(state, &region_id, sizeof(region_id));
    bu_data_hash_update(state, &node_revision, sizeof(node_revision));
    bu_data_hash_update(state, &drawn_revision, sizeof(drawn_revision));
    bu_data_hash_update(state, &material_revision, sizeof(material_revision));
    bu_data_hash_update(state, &draw_size, sizeof(draw_size));
    bu_data_hash_update(state, draw_center, sizeof(vect_t));
    bu_data_hash_update(state, transform, sizeof(mat_t));
    bu_data_hash_update(state, &bounds_valid, sizeof(bounds_valid));
    if (bounds_valid) {
	bu_data_hash_update(state, bmin, sizeof(point_t));
	bu_data_hash_update(state, bmax, sizeof(point_t));
    }
    bu_data_hash_update(state, material_rgb, sizeof(material_rgb));
    _bsg_appearance_settings_hash(state, &appearance);
    _bsg_hash_cstr(state, bsg_node_name(s));
    _bsg_hash_field_revision(state, s, BSG_FIELD_NAME);
    _bsg_hash_field_revision(state, s, BSG_FIELD_VISIBILITY);
    _bsg_hash_field_revision(state, s, BSG_FIELD_TRANSFORM);
    _bsg_hash_field_revision(state, s, BSG_FIELD_BOUNDS);
    _bsg_hash_field_revision(state, s, BSG_FIELD_SWITCH_WHICH);
    _bsg_hash_field_revision(state, s, BSG_FIELD_MATERIAL_DIFFUSE_COLOR);
    _bsg_hash_field_revision(state, s, BSG_FIELD_MATERIAL_ALPHA);
    _bsg_hash_field_revision(state, s, BSG_FIELD_DRAW_LINE_WIDTH);
    _bsg_hash_field_revision(state, s, BSG_FIELD_DRAW_LINE_STYLE);
    _bsg_hash_field_revision(state, s, BSG_FIELD_DRAW_FILL_MODE);
    _bsg_hash_field_revision(state, s, BSG_FIELD_GEOMETRY_KIND);
    _bsg_hash_field_revision(state, s, BSG_FIELD_GEOMETRY_COORDINATES);
    _bsg_hash_field_revision(state, s, BSG_FIELD_GEOMETRY_NORMALS);
    _bsg_hash_field_revision(state, s, BSG_FIELD_GEOMETRY_COLORS);
    _bsg_hash_field_revision(state, s, BSG_FIELD_GEOMETRY_INDICES);
    _bsg_hash_field_revision(state, s, BSG_FIELD_GEOMETRY_PRIMITIVE_SETS);
    _bsg_hash_field_revision(state, s, BSG_FIELD_GEOMETRY_TEXT);
    _bsg_hash_field_revision(state, s, BSG_FIELD_GEOMETRY_SOURCE_REVISION);

    struct bsg_payload *payload = bsg_node_get_payload(s);
    if (payload && payload->pl_type == BSG_PL_LINE_SET)
	_bsg_hash_payload_line_set(state, bsg_payload_line_set_get(payload));
    if (payload && payload->pl_type == BSG_PL_SKETCH) {
	struct bsg_payload *geometry = bsg_payload_sketch_geometry(payload);
	if (geometry && geometry->pl_type == BSG_PL_LINE_SET)
	    _bsg_hash_payload_line_set(state, bsg_payload_line_set_get(geometry));
    }

}

void
bsg_settings_hash(struct bu_data_hash_state *state, struct bsg_view_settings *s)
{
    bu_data_hash_update(state, s, sizeof(struct bsg_view_settings));

    _bsg_adc_state_hash(state, &s->gv_adc);
    _bsg_axes_hash(state, &s->gv_model_axes);
    _bsg_axes_hash(state, &s->gv_view_axes);
    _bsg_grid_state_hash(state, &s->gv_grid);
    _bsg_other_state_hash(state, &s->gv_center_dot);
    _bsg_params_state_hash(state, &s->gv_view_params);
    _bsg_other_state_hash(state, &s->gv_view_scale);
    _bsg_interactive_rect_state_hash(state, &s->gv_rect);

#if 0
    for (size_t i = 0; i < BU_PTBL_LEN(v->gv_selected); i++) {
	long *p = BU_PTBL_GET(v->gv_selected, i);
	bu_data_hash_update(state, p, sizeof(long *));
    }
#endif

}

static void
_bsg_view_hash(struct bu_data_hash_state *state, struct bsg_view *v)
{
    if (!state || !v)
	return;

    bu_data_hash_update(state, &v->magic, sizeof(v->magic));
    bu_data_hash_update(state, &v->gv_i_scale, sizeof(v->gv_i_scale));
    bu_data_hash_update(state, &v->gv_a_scale, sizeof(v->gv_a_scale));
    bu_data_hash_update(state, &v->gv_scale, sizeof(v->gv_scale));
    bu_data_hash_update(state, &v->gv_size, sizeof(v->gv_size));
    bu_data_hash_update(state, &v->gv_isize, sizeof(v->gv_isize));
    bu_data_hash_update(state, &v->gv_base2local, sizeof(v->gv_base2local));
    bu_data_hash_update(state, &v->gv_local2base, sizeof(v->gv_local2base));
    bu_data_hash_update(state, &v->gv_rscale, sizeof(v->gv_rscale));
    bu_data_hash_update(state, &v->gv_sscale, sizeof(v->gv_sscale));
    bu_data_hash_update(state, &v->gv_width, sizeof(v->gv_width));
    bu_data_hash_update(state, &v->gv_height, sizeof(v->gv_height));
    bu_data_hash_update(state, v->gv_wmin, sizeof(v->gv_wmin));
    bu_data_hash_update(state, v->gv_wmax, sizeof(v->gv_wmax));
    bu_data_hash_update(state, &v->gv_perspective, sizeof(v->gv_perspective));
    bu_data_hash_update(state, v->gv_aet, sizeof(v->gv_aet));
    bu_data_hash_update(state, v->gv_eye_pos, sizeof(v->gv_eye_pos));
    bu_data_hash_update(state, v->gv_keypoint, sizeof(v->gv_keypoint));
    bu_data_hash_update(state, &v->gv_coord, sizeof(v->gv_coord));
    bu_data_hash_update(state, &v->gv_rotate_about, sizeof(v->gv_rotate_about));
    bu_data_hash_update(state, v->gv_rotation, sizeof(v->gv_rotation));
    bu_data_hash_update(state, v->gv_center, sizeof(v->gv_center));
    bu_data_hash_update(state, v->gv_model2view, sizeof(v->gv_model2view));
    bu_data_hash_update(state, v->gv_pmodel2view, sizeof(v->gv_pmodel2view));
    bu_data_hash_update(state, v->gv_view2model, sizeof(v->gv_view2model));
    bu_data_hash_update(state, v->gv_pmat, sizeof(v->gv_pmat));
    bu_data_hash_update(state, &v->gv_prevMouseX, sizeof(v->gv_prevMouseX));
    bu_data_hash_update(state, &v->gv_prevMouseY, sizeof(v->gv_prevMouseY));
    bu_data_hash_update(state, &v->gv_mouse_x, sizeof(v->gv_mouse_x));
    bu_data_hash_update(state, &v->gv_mouse_y, sizeof(v->gv_mouse_y));
    bu_data_hash_update(state, v->gv_prev_point, sizeof(v->gv_prev_point));
    bu_data_hash_update(state, v->gv_point, sizeof(v->gv_point));
    bu_data_hash_update(state, &v->gv_key, sizeof(v->gv_key));
    bu_data_hash_update(state, &v->gv_mod_flags, sizeof(v->gv_mod_flags));
    bu_data_hash_update(state, &v->gv_minMouseDelta, sizeof(v->gv_minMouseDelta));
    bu_data_hash_update(state, &v->gv_maxMouseDelta, sizeof(v->gv_maxMouseDelta));
    bu_data_hash_update(state, v->obb_center, sizeof(v->obb_center));
    bu_data_hash_update(state, v->obb_extent1, sizeof(v->obb_extent1));
    bu_data_hash_update(state, v->obb_extent2, sizeof(v->obb_extent2));
    bu_data_hash_update(state, v->obb_extent3, sizeof(v->obb_extent3));
    bu_data_hash_update(state, v->gv_vc_backout, sizeof(v->gv_vc_backout));
    bu_data_hash_update(state, v->gv_lookat, sizeof(v->gv_lookat));
    bu_data_hash_update(state, &v->radius, sizeof(v->radius));
    bsg_knobs_hash(&v->k, state);
    bu_data_hash_update(state, v->orig_pos, sizeof(v->orig_pos));
    bu_data_hash_update(state, &v->gv_frame_rev, sizeof(v->gv_frame_rev));
}

/* Callback for database-source hashing. */
static int
_bsg_hash_db_obj_cb(struct bsg_node *s, void *data)
{
    struct bu_data_hash_state *state = (struct bu_data_hash_state *)data;
    /* Hash children first (view-specific adaptive objects) */
    if (BU_PTBL_IS_INITIALIZED(&s->children)) {
	for (size_t j = 0; j < BU_PTBL_LEN(&s->children); j++) {
	    struct bsg_node *s_c = (struct bsg_node *)BU_PTBL_GET(&s->children, j);
	    bsg_scene_obj_hash(state, s_c);
	}
    }
    bsg_scene_obj_hash(state, s);
    return 1;
}

/* Hash a view-scoped feature and its children. */
static int
_bsg_hash_view_obj_cb(struct bsg_node *s, void *data)
{
    struct bu_data_hash_state *state = (struct bu_data_hash_state *)data;
    if (BU_PTBL_IS_INITIALIZED(&s->children)) {
	for (size_t j = 0; j < BU_PTBL_LEN(&s->children); j++) {
	    struct bsg_node *s_c = (struct bsg_node *)BU_PTBL_GET(&s->children, j);
	    bsg_scene_obj_hash(state, s_c);
	}
    }
    bsg_scene_obj_hash(state, s);
    return 1;
}

unsigned long long
bsg_hash(struct bsg_view *v)
{
    if (!v)
	return 0;

    struct bu_data_hash_state *state = bu_data_hash_create();
    if (!state)
	return 0;

    /* Deliberately do not hash the view name, private scene-store handles,
     * callbacks, display-manager pointers, or retained-scene pointers.  They
     * do not define rendered view semantics and make the hash depend on
     * storage layout/allocation details. */
    _bsg_view_hash(state, v);

    if (v->settings_active)
	bsg_settings_hash(state, v->settings_active);
    if (v->settings_local)
	bsg_settings_hash(state, v->settings_local);

    /* TclCAD feature state is represented as view-scoped BSG features and is
     * included through the feature-scope walk below. */
    bsg_feature_visit_nodes(v, BSG_FEATURE_SCOPE_ALL, _bsg_hash_view_obj_cb, state);

    /* Hash DB-derived objects via the BSG-aware helper */
    bsg_scene_visit_db(v, _bsg_hash_db_obj_cb, state);

    unsigned long long hash_val = bu_data_hash_val(state);
    bu_data_hash_destroy(state);

    return hash_val;
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
