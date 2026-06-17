/*                      A C T I O N . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file libbsg/action.c
 *
 * BSG action framework — shared stateful tree traversal plus legacy actions.
 */

#include "common.h"

#include <string.h>

#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "vmath.h"
#include "bn/mat.h"

#include "bsg/defines.h"
#include "bsg/action.h"
#include "bsg/appearance.h"
#include "bsg/camera.h"
#include "bsg/lod.h"
#include "bsg/field.h"
#include "bsg/light.h"
#include "bsg/material.h"
#include "bsg/node.h"
#include "bsg/object.h"
#include "bsg/render_settings.h"
#include "bsg/scene_object.h"
#include "bsg_private.h"
#include "bsg/util.h"
#include "bsg/visibility.h"
#include "action_private.h"
#include "appearance_private.h"
#include "field_private.h"
#include "lod_private.h"
#include "material_private.h"
#include "node_private.h"
#include "node_storage_private.h"
#include "object_private.h"
#include "state_private.h"
#include "visibility_private.h"

struct bsg_action {
    bsg_type_id type;
    bsg_action_apply_cb apply_cb;
    bsg_action_data_destroy_cb destroy_cb;
    void *data;
    const char *label;
};

struct bsg_action_method {
    bsg_type_id action_type;
    bsg_type_id node_type;
    bsg_action_method_cb cb;
    void *userdata;
};

#define BSG_ACTION_METHOD_MAX 128

static struct bsg_action_method bsg_action_methods[BSG_ACTION_METHOD_MAX];
static size_t bsg_action_method_count = 0;

static bsg_type_id bsg_type_bbox_action = BSG_TYPE_INVALID;
static bsg_type_id bsg_type_collect_action = BSG_TYPE_INVALID;
static bsg_type_id bsg_type_render_action = BSG_TYPE_INVALID;
static bsg_type_id bsg_type_backend_scene_action = BSG_TYPE_INVALID;
static bsg_type_id bsg_type_pick_action = BSG_TYPE_INVALID;
static bsg_type_id bsg_type_snap_action = BSG_TYPE_INVALID;
static bsg_type_id bsg_type_measure_action = BSG_TYPE_INVALID;
static bsg_type_id bsg_type_search_action = BSG_TYPE_INVALID;
static bsg_type_id bsg_type_export_records_action = BSG_TYPE_INVALID;

static int
_node_is_type(const bsg_node *node, bsg_type_id type)
{
    return bsg_node_type_is_a(node, type);
}

static bsg_type_id
_action_subtype(bsg_type_id *slot, const char *name)
{
    if (!slot || !name)
	return BSG_TYPE_INVALID;
    if (*slot == BSG_TYPE_INVALID)
	*slot = bsg_type_register(name, bsg_action_type());
    return *slot;
}

bsg_type_id
bsg_bbox_action_type(void)
{
    return _action_subtype(&bsg_type_bbox_action, "BSGBboxAction");
}

bsg_type_id
bsg_collect_action_type(void)
{
    return _action_subtype(&bsg_type_collect_action, "BSGCollectAction");
}

bsg_type_id
bsg_render_action_type(void)
{
    return _action_subtype(&bsg_type_render_action, "BSGRenderAction");
}

bsg_type_id
bsg_backend_scene_action_type(void)
{
    return _action_subtype(&bsg_type_backend_scene_action, "BSGBackendSceneAction");
}

bsg_type_id
bsg_pick_action_type(void)
{
    return _action_subtype(&bsg_type_pick_action, "BSGPickAction");
}

bsg_type_id
bsg_snap_action_type(void)
{
    return _action_subtype(&bsg_type_snap_action, "BSGSnapAction");
}

bsg_type_id
bsg_measure_action_type(void)
{
    return _action_subtype(&bsg_type_measure_action, "BSGMeasureAction");
}

bsg_type_id
bsg_search_action_type(void)
{
    return _action_subtype(&bsg_type_search_action, "BSGSearchAction");
}

bsg_type_id
bsg_export_records_action_type(void)
{
    return _action_subtype(&bsg_type_export_records_action, "BSGExportRecordsAction");
}

bsg_node *
bsg_action_node_from_ref(bsg_node_ref ref)
{
    return bsg_object_ref_node(bsg_node_ref_object(ref));
}

bsg_node_ref
bsg_action_node_ref_from_node(bsg_node *node)
{
    return bsg_node_ref_from_object(bsg_object_ref_from_node(node));
}

static struct bsg_action *
_action_from_ref(bsg_action_ref ref)
{
    return (struct bsg_action *)ref.opaque;
}

bsg_action_ref
bsg_action_ref_null(void)
{
    bsg_action_ref ref = BSG_ACTION_REF_NULL_INIT;
    return ref;
}

int
bsg_action_ref_is_null(bsg_action_ref action)
{
    return action.opaque ? 0 : 1;
}

bsg_type_id
bsg_action_ref_type(bsg_action_ref action)
{
    struct bsg_action *a = _action_from_ref(action);
    return a ? a->type : BSG_TYPE_INVALID;
}

bsg_action_ref
bsg_action_ref_create_internal(bsg_type_id action_type,
			       bsg_action_apply_cb apply_cb,
			       bsg_action_data_destroy_cb destroy_cb,
			       void *data,
			       const char *label)
{
    bsg_action_ref ref = BSG_ACTION_REF_NULL_INIT;
    if (!bsg_type_is_a(action_type, bsg_action_type()))
	return ref;
    struct bsg_action *a;
    BU_ALLOC(a, struct bsg_action);
    memset(a, 0, sizeof(*a));
    a->type = action_type;
    a->apply_cb = apply_cb;
    a->destroy_cb = destroy_cb;
    a->data = data;
    a->label = label;
    ref.opaque = a;
    return ref;
}

void *
bsg_action_ref_data(bsg_action_ref action)
{
    struct bsg_action *a = _action_from_ref(action);
    return a ? a->data : NULL;
}

static int
_dispatch_registered_methods(bsg_action_ref action,
			     bsg_node *node,
			     bsg_state_ref state)
{
    if (!node)
	return 1;
    struct bsg_action *a = _action_from_ref(action);
    if (!a)
	return 0;

    int invoked = 0;
    bsg_node_ref node_ref = bsg_action_node_ref_from_node(node);
    bsg_type_id node_type = bsg_node_ref_type(node_ref);
    for (size_t i = 0; i < bsg_action_method_count; i++) {
	struct bsg_action_method *m = &bsg_action_methods[i];
	if (!m->cb)
	    continue;
	if (!bsg_type_is_a(a->type, m->action_type))
	    continue;
	if (!bsg_type_is_a(node_type, m->node_type))
	    continue;
	invoked = 1;
	if (!m->cb(action, node_ref, state, m->userdata))
	    return 0;
    }
    return invoked ? 1 : 1;
}

static int
_generic_action_enter_cb(bsg_node *node,
			 const struct bsg_action_state *state,
			 void *userdata)
{
    bsg_action_ref *action = (bsg_action_ref *)userdata;
    return _dispatch_registered_methods(action ? *action : bsg_action_ref_null(),
	    node, state ? state->state : bsg_state_ref_null());
}

static int
_generic_action_apply(bsg_action_ref action, bsg_node_ref root, void *UNUSED(data))
{
    bsg_node *node = bsg_action_node_from_ref(root);
    if (!node)
	return 0;
    struct bsg_action_traversal traversal;
    memset(&traversal, 0, sizeof(traversal));
    traversal.root = node;
    traversal.enter_cb = _generic_action_enter_cb;
    traversal.userdata = &action;
    return bsg_action_traverse(&traversal);
}

bsg_action_ref
bsg_action_ref_create(bsg_type_id action_type)
{
    return bsg_action_ref_create_internal(action_type, _generic_action_apply,
	    NULL, NULL, "generic action");
}

void
bsg_action_ref_destroy(bsg_action_ref action)
{
    struct bsg_action *a = _action_from_ref(action);
    if (!a)
	return;
    if (a->destroy_cb)
	a->destroy_cb(action, a->data);
    bu_free(a, "bsg_action");
}

int
bsg_action_apply(bsg_action_ref action, bsg_node_ref root)
{
    struct bsg_action *a = _action_from_ref(action);
    if (!a || !a->apply_cb)
	return 0;
    return a->apply_cb(action, root, a->data);
}

int
bsg_action_method_register(bsg_type_id action_type,
			   bsg_type_id node_type,
			   bsg_action_method_cb cb,
			   void *userdata)
{
    if (!cb || !bsg_type_is_a(action_type, bsg_action_type()) ||
	    !bsg_type_is_a(node_type, bsg_node_type()))
	return 0;
    if (bsg_action_method_count >= BSG_ACTION_METHOD_MAX)
	return 0;
    struct bsg_action_method *m = &bsg_action_methods[bsg_action_method_count++];
    m->action_type = action_type;
    m->node_type = node_type;
    m->cb = cb;
    m->userdata = userdata;
    return 1;
}

static int
_independent_root_skip_child(const bsg_node *node)
{
    if (!node)
	return 1;
    if (_node_is_type(node, bsg_view_scope_type()))
	return 0;
    return BU_STR_EQUAL("_overlays", bsg_node_name(node)) ? 0 : 1;
}

static void
_apply_material_node_state(bsg_node *node, bsg_state_ref state_ref)
{
    unsigned char color[3] = {255, 255, 255};
    double alpha = 1.0;
    uint32_t material_revision = bsg_material_revision(node);
    uint32_t appearance_revision = 0;
    bsg_field_ref diffuse = bsg_node_field_ref(node, BSG_FIELD_MATERIAL_DIFFUSE_COLOR);
    bsg_field_ref alpha_field = bsg_node_field_ref(node, BSG_FIELD_MATERIAL_ALPHA);

    (void)bsg_field_get_color3(diffuse, color);
    (void)bsg_field_get_double(alpha_field, &alpha);
    appearance_revision += (uint32_t)bsg_field_revision(diffuse);
    appearance_revision += (uint32_t)bsg_field_revision(alpha_field);
    appearance_revision += (uint32_t)bsg_field_revision(bsg_node_field_ref(node, BSG_FIELD_MATERIAL_SPECULAR_COLOR));
    appearance_revision += (uint32_t)bsg_field_revision(bsg_node_field_ref(node, BSG_FIELD_MATERIAL_EMISSIVE_COLOR));
    appearance_revision += (uint32_t)bsg_field_revision(bsg_node_field_ref(node, BSG_FIELD_MATERIAL_SHININESS));
    appearance_revision += (uint32_t)bsg_field_revision(bsg_node_field_ref(node, BSG_FIELD_MATERIAL_LINE_COLOR));
    if (!material_revision)
	material_revision = appearance_revision;
    bsg_state_ref_set_material_color(state_ref, color);
    bsg_state_ref_set_material_transparency(state_ref, alpha);
    bsg_state_ref_set_appearance_revisions(state_ref, material_revision, appearance_revision);
}

static void
_apply_draw_style_node_state(bsg_node *node, bsg_state_ref state_ref)
{
    int draw_mode = 0;
    int line_width = 1;
    int line_style = 0;
    (void)bsg_field_get_enum(bsg_node_field_ref(node, BSG_FIELD_DRAW_FILL_MODE), &draw_mode);
    (void)bsg_field_get_int(bsg_node_field_ref(node, BSG_FIELD_DRAW_LINE_WIDTH), &line_width);
    (void)bsg_field_get_int(bsg_node_field_ref(node, BSG_FIELD_DRAW_LINE_STYLE), &line_style);
    bsg_state_ref_set_draw_style(state_ref, draw_mode, line_width, line_style);
}

static void
_apply_complexity_node_state(bsg_node *node, bsg_state_ref state_ref)
{
    double complexity = 0.5;
    int lod_policy = BSG_LOD_AUTO;
    int lod_level = -1;
    (void)bsg_field_get_double(bsg_node_field_ref(node, BSG_FIELD_COMPLEXITY_VALUE), &complexity);
    (void)bsg_field_get_enum(bsg_node_field_ref(node, BSG_FIELD_COMPLEXITY_LOD_POLICY), &lod_policy);
    (void)bsg_field_get_int(bsg_node_field_ref(node, BSG_FIELD_COMPLEXITY_LOD_LEVEL), &lod_level);
    bsg_state_ref_set_complexity(state_ref, complexity);
    if (lod_policy == BSG_LOD_FORCE_LEVEL && lod_level >= 0)
	bsg_state_ref_set_lod(state_ref, bsg_state_ref_lod_source(state_ref), lod_level,
		bsg_state_ref_lod_level_count(state_ref));
}

static void
_apply_camera_node_state(bsg_node *node, bsg_state_ref state_ref)
{
    int projection = BSG_CAMERA_ORTHOGRAPHIC;
    double perspective = 0.0;
    point_t position = VINIT_ZERO;
    mat_t orientation;
    double near_distance = 0.0;
    double far_distance = 0.0;
    MAT_IDN(orientation);
    (void)bsg_field_get_enum(bsg_node_field_ref(node, BSG_FIELD_CAMERA_PROJECTION), &projection);
    (void)bsg_field_get_double(bsg_node_field_ref(node, BSG_FIELD_CAMERA_PERSPECTIVE), &perspective);
    (void)bsg_field_get_vec3(bsg_node_field_ref(node, BSG_FIELD_CAMERA_POSITION), position);
    (void)bsg_field_get_matrix(bsg_node_field_ref(node, BSG_FIELD_CAMERA_ORIENTATION), orientation);
    (void)bsg_field_get_double(bsg_node_field_ref(node, BSG_FIELD_CAMERA_NEAR_DISTANCE), &near_distance);
    (void)bsg_field_get_double(bsg_node_field_ref(node, BSG_FIELD_CAMERA_FAR_DISTANCE), &far_distance);
    bsg_state_ref_set_camera(state_ref, projection, perspective, position,
	    orientation, near_distance, far_distance);
}

static void
_apply_light_node_state(bsg_node *node, bsg_state_ref state_ref)
{
    int type = BSG_LIGHT_DIRECTIONAL;
    point_t position = VINIT_ZERO;
    vect_t direction = VINIT_ZERO;
    unsigned char diffuse[3] = {255, 255, 255};
    unsigned char specular[3] = {255, 255, 255};
    unsigned char ambient[3] = {51, 51, 51};
    double intensity = 1.0;
    int active = 1;
    (void)bsg_field_get_enum(bsg_node_field_ref(node, BSG_FIELD_LIGHT_TYPE), &type);
    (void)bsg_field_get_vec3(bsg_node_field_ref(node, BSG_FIELD_LIGHT_POSITION), position);
    (void)bsg_field_get_vec3(bsg_node_field_ref(node, BSG_FIELD_LIGHT_DIRECTION), direction);
    (void)bsg_field_get_color3(bsg_node_field_ref(node, BSG_FIELD_LIGHT_DIFFUSE_COLOR), diffuse);
    (void)bsg_field_get_color3(bsg_node_field_ref(node, BSG_FIELD_LIGHT_SPECULAR_COLOR), specular);
    (void)bsg_field_get_color3(bsg_node_field_ref(node, BSG_FIELD_LIGHT_AMBIENT_COLOR), ambient);
    (void)bsg_field_get_double(bsg_node_field_ref(node, BSG_FIELD_LIGHT_INTENSITY), &intensity);
    (void)bsg_field_get_bool(bsg_node_field_ref(node, BSG_FIELD_LIGHT_ACTIVE), &active);
    bsg_state_ref_set_light(state_ref, type, position, direction, diffuse,
	    specular, ambient, intensity, active);
}

static void
_apply_state_node(bsg_node *node, bsg_state_ref state_ref)
{
    if (bsg_node_type_is_a(node, bsg_material_type()))
	_apply_material_node_state(node, state_ref);
    else if (bsg_node_type_is_a(node, bsg_draw_style_type()))
	_apply_draw_style_node_state(node, state_ref);
    else if (bsg_node_type_is_a(node, bsg_complexity_type()))
	_apply_complexity_node_state(node, state_ref);
    else if (bsg_node_type_is_a(node, bsg_camera_type()))
	_apply_camera_node_state(node, state_ref);
    else if (bsg_node_type_is_a(node, bsg_light_type()))
	_apply_light_node_state(node, state_ref);
}

static int
_is_state_node(const bsg_node *node)
{
    if (!node)
	return 0;
    return bsg_node_type_is_a(node, bsg_material_type()) ||
	bsg_node_type_is_a(node, bsg_draw_style_type()) ||
	bsg_node_type_is_a(node, bsg_complexity_type()) ||
	bsg_node_type_is_a(node, bsg_camera_type()) ||
	bsg_node_type_is_a(node, bsg_light_type());
}


static int
_complexity_lod_level(bsg_state_ref state_ref, int nlevels)
{
    if (nlevels <= 0 || !bsg_state_ref_has_complexity(state_ref))
	return -1;
    double complexity = bsg_state_ref_complexity(state_ref);
    if (complexity < 0.0)
	complexity = 0.0;
    if (complexity > 1.0)
	complexity = 1.0;
    return (int)((1.0 - complexity) * (double)(nlevels - 1) + 0.5);
}


static int
_field_lod_level_from_state(bsg_node *node,
			    const struct bsg_action_traversal *traversal,
			    bsg_state_ref state_ref)
{
    if (!node || bsg_lod_node_ops(node))
	return -1;

    int selection = BSG_LOD_SELECT_AUTO;
    int active = -1;
    (void)bsg_field_get_enum(bsg_node_field_ref(node, BSG_FIELD_LOD_SELECTION), &selection);
    (void)bsg_field_get_int(bsg_node_field_ref(node, BSG_FIELD_LOD_ACTIVE_LEVEL), &active);
    if (selection == BSG_LOD_SELECT_FORCE_LEVEL)
	return (active < 0) ? 0 : active;

    size_t range_cnt = bsg_field_multi_count(bsg_node_field_ref(node, BSG_FIELD_LOD_RANGES));
    if (range_cnt == 0)
	return -1;

    double metric = traversal && traversal->view ? traversal->view->gv_size : 0.0;
    if (bsg_state_ref_has_camera(state_ref)) {
	point_t bmin = VINIT_ZERO;
	point_t bmax = VINIT_ZERO;
	if (bsg_node_bounds(node, bmin, bmax)) {
	    point_t center_local;
	    point_t center_world;
	    point_t camera;
	    mat_t model_mat;
	    VSET(center_local,
		    0.5 * (bmin[X] + bmax[X]),
		    0.5 * (bmin[Y] + bmax[Y]),
		    0.5 * (bmin[Z] + bmax[Z]));
	    bsg_state_ref_transform(state_ref, model_mat);
	    MAT4X3PNT(center_world, model_mat, center_local);
	    bsg_state_ref_camera_position(state_ref, camera);
	    metric = DIST_PNT_PNT(center_world, camera);
	}
    }

    int level = 0;
    for (size_t i = 0; i < range_cnt; i++) {
	double range = 0.0;
	if (!bsg_field_multi_double_at(bsg_node_field_ref(node, BSG_FIELD_LOD_RANGES), i, &range))
	    continue;
	if (metric <= range)
	    return level;
	level++;
    }

    return level;
}


static void
_state_inherit_appearance_from_node(struct bsg_state *state, bsg_node *node)
{
    if (!state || !node)
	return;

    bsg_appearance_settings_for_node(node, &state->inherited_settings_storage);
    state->inherited_settings = &state->inherited_settings_storage;
}


static int
_action_recurse(bsg_node *node,
		const struct bsg_action_traversal *traversal,
		bsg_state_ref parent_state)
{
    if (!node || !traversal || bsg_state_ref_is_null(parent_state))
	return 1;

    int ret = 1;
    bsg_state_ref state_ref = bsg_state_ref_push(parent_state);
    struct bsg_state *state = bsg_state_ref_state(state_ref);
    struct bsg_state *parent = bsg_state_ref_state(parent_state);
    if (!state || !parent)
	goto done;

    state->node = node;
    state->inherited_visible = parent->visible;
    state->force_draw = parent->force_draw || bsg_appearance_force_draw(node);
    state->visible = bsg_visibility_inherited_visible(node, traversal->view,
	    parent->visible);
    state->highlighted = bsg_appearance_is_highlighted(node);
    state->selected = 0;
    if (bsg_appearance_inherited_by_children(node))
	_state_inherit_appearance_from_node(state, node);

    if (_node_is_type(node, bsg_view_scope_type())) {
	state->view_scope_visible = bsg_visibility_scope_allows_view(node, traversal->view);
	if (!state->view_scope_visible)
	    goto done;
    }

    if (_node_is_type(node, bsg_transform_type())) {
	mat_t node_mat;
	MAT_IDN(node_mat);
	bsg_node_transform(node, node_mat);
	bn_mat_mul(state->model_mat, parent->model_mat, node_mat);
    }

    _apply_state_node(node, state_ref);

    struct bsg_action_state compat;
    memset(&compat, 0, sizeof(compat));
    compat.state = state_ref;
    compat.view = state->view;
    compat.root = state->root;
    compat.node = state->node;
    MAT_COPY(compat.model_mat, state->model_mat);
    compat.inherited_settings = state->inherited_settings;
    compat.inherited_visible = state->inherited_visible;
    compat.visible = state->visible;
    compat.force_draw = state->force_draw;
    compat.lod_source = state->lod_source;
    compat.lod_level = state->lod_level;
    compat.lod_level_count = state->lod_level_count;

    if (traversal->enter_cb &&
	    !traversal->enter_cb(node, &compat, traversal->userdata)) {
	ret = 0;
	goto done;
    }

    if (_node_is_type(node, bsg_lod_type())) {
	int nlevels = bsg_lod_node_level_count(node);
	if (nlevels > 0) {
	    int active = -1;
	    bsg_lod_source_ref inherited_lod = bsg_state_ref_lod_source(state_ref);
	    if (bsg_lod_source_ref_is_null(inherited_lod) &&
		    bsg_state_ref_lod_level(state_ref) >= 0)
		active = bsg_state_ref_lod_level(state_ref);
	    if (active < 0)
		active = _complexity_lod_level(state_ref, nlevels);
	    if (active < 0)
		active = _field_lod_level_from_state(node, traversal, state_ref);
	    if (active < 0)
		active = bsg_lod_node_resolve_level(node, traversal->view);
	    int child_index = active;
	    if (child_index < 0 || child_index >= nlevels)
		child_index = 0;
	    bsg_lod_source_ref lod_source;
	    lod_source.token = (uintptr_t)node;
	    lod_source.revision = bsg_node_revision(node);
	    bsg_state_ref_set_lod(state_ref, lod_source, active, nlevels);
	    bsg_node *child = bsg_node_child_at(node, (size_t)child_index);
	    if (!_action_recurse(child, traversal, state_ref)) {
		ret = 0;
		goto done;
	    }
	}
	compat.lod_source = state->lod_source;
	compat.lod_level = state->lod_level;
	compat.lod_level_count = state->lod_level_count;
	if (traversal->leave_cb &&
		!traversal->leave_cb(node, &compat, traversal->userdata)) {
	    ret = 0;
	    goto done;
	}
	goto done;
    }

    int is_drawable = (_node_is_type(node, bsg_shape_type()) ||
	    bsg_node_is_database_source(node));
    int visible_only = (traversal->flags & BSG_ACTION_TRAVERSE_VISIBLE_ONLY) != 0;
    int drawable_visible = is_drawable &&
	(!visible_only || state->visible || state->force_draw);
    if (drawable_visible && traversal->shape_cb &&
	    !traversal->shape_cb(node, &compat, traversal->userdata)) {
	ret = 0;
	goto done;
    }

    int independent_root = 0;
    if ((traversal->flags & BSG_ACTION_TRAVERSE_INDEPENDENT_ROOT) &&
	    traversal->view && bsg_view_scene_root(traversal->view) &&
	    bsg_view_is_independent(traversal->view) &&
	    node == bsg_view_scene_root(traversal->view)) {
	independent_root = 1;
    }

    int is_switch = bsg_node_type_is_a(node, bsg_switch_type());
    for (size_t i = 0; i < BU_PTBL_LEN(&node->children); i++) {
	bsg_node *child = (bsg_node *)BU_PTBL_GET(&node->children, i);
	if (independent_root && _independent_root_skip_child(child))
	    continue;
	if (is_switch && !bsg_switch_node_child_selected(node, i))
	    continue;
	if (_is_state_node(child))
	    _apply_state_node(child, state_ref);
	if (!_action_recurse(child, traversal, state_ref)) {
	    ret = 0;
	    goto done;
	}
    }

    if (traversal->leave_cb &&
	    !traversal->leave_cb(node, &compat, traversal->userdata)) {
	ret = 0;
	goto done;
    }

done:
    bsg_state_ref_pop(state_ref);
    return ret;
}


int
bsg_action_traverse(const struct bsg_action_traversal *traversal)
{
    if (!traversal || !traversal->root)
	return 0;

    bsg_state_ref state_ref = bsg_state_ref_create();
    struct bsg_state *state = bsg_state_ref_state(state_ref);
    if (!state)
	return 0;
    state->view = traversal->view;
    state->root = traversal->root;
    state->node = traversal->root;

    int ret = _action_recurse(traversal->root, traversal, state_ref);
    bsg_state_ref_destroy(state_ref);
    return ret;
}

/* ------------------------------------------------------------------ */
/* BBOX action                                                          */
/* ------------------------------------------------------------------ */

struct bbox_state {
    point_t bmin;
    point_t bmax;
    int     valid;
};

static int
_state_transformed_bounds(const bsg_node *node,
			  const struct bsg_action_state *state,
			  point_t bmin,
			  point_t bmax)
{
    point_t nbmin, nbmax;
    if (!bsg_node_bounds(node, nbmin, nbmax)) {
	VSETALL(bmin, 0.0);
	VSETALL(bmax, 0.0);
	return 0;
    }

    mat_t model_mat;
    bsg_state_ref_transform(state ? state->state : bsg_state_ref_null(), model_mat);
    int first = 1;
    for (int ix = 0; ix < 2; ix++) {
	for (int iy = 0; iy < 2; iy++) {
	    for (int iz = 0; iz < 2; iz++) {
		point_t c, wc;
		VSET(c, ix ? nbmax[X] : nbmin[X],
			iy ? nbmax[Y] : nbmin[Y],
			iz ? nbmax[Z] : nbmin[Z]);
		MAT4X3PNT(wc, model_mat, c);
		if (first) {
		    VMOVE(bmin, wc);
		    VMOVE(bmax, wc);
		    first = 0;
		} else {
		    VMIN(bmin, wc);
		    VMAX(bmax, wc);
		}
	    }
	}
    }
    return 1;
}

static int
bbox_cb(bsg_node *node, const struct bsg_action_state *state, void *userdata)
{
    struct bbox_state *st = (struct bbox_state *)userdata;

    /* Only accumulate leaf shape nodes that have a valid bounding box */
    if (!_node_is_type(node, bsg_shape_type()))
	return 1;   /* continue traversal */

    point_t bmin, bmax;
    if (!_state_transformed_bounds(node, state, bmin, bmax))
	return 1;

    if (!st->valid) {
	VMOVE(st->bmin, bmin);
	VMOVE(st->bmax, bmax);
	st->valid = 1;
    } else {
	VMIN(st->bmin, bmin);
	VMAX(st->bmax, bmax);
    }
    return 1;
}


/* ------------------------------------------------------------------ */
/* COLLECT action                                                       */
/* ------------------------------------------------------------------ */

struct collect_action_data {
    bsg_type_id type;
    struct bu_ptbl nodes;
};

static int
collect_enter_cb(bsg_node *node, const struct bsg_action_state *UNUSED(state), void *userdata)
{
    struct collect_action_data *st = (struct collect_action_data *)userdata;
    if (!st || !node)
	return 1;
    bsg_node_ref ref = bsg_action_node_ref_from_node(node);
    if (st->type == BSG_TYPE_INVALID || bsg_node_is_a(ref, st->type))
	bu_ptbl_ins_unique(&st->nodes, (long *)node);

    return 1;
}

struct search_action_data {
    struct bu_vls name;
    struct bu_ptbl nodes;
};

static int
search_enter_cb(bsg_node *node, const struct bsg_action_state *UNUSED(state), void *userdata)
{
    struct search_action_data *st = (struct search_action_data *)userdata;
    if (!st || !node || bu_vls_strlen(&st->name) == 0)
	return 1;
    const char *name = bsg_node_name(node);
    if (name && bu_path_match(bu_vls_cstr(&st->name), name, 0) == 0)
	bu_ptbl_ins_unique(&st->nodes, (long *)node);
    return 1;
}


/* ------------------------------------------------------------------ */
/* Built-in public actions                                             */
/* ------------------------------------------------------------------ */

static int
_bbox_action_apply(bsg_action_ref action, bsg_node_ref root, void *data)
{
    struct bbox_state *st = (struct bbox_state *)data;
    bsg_node *node = bsg_action_node_from_ref(root);
    if (!st || !node)
	return 0;
    VSETALL(st->bmin, 0.0);
    VSETALL(st->bmax, 0.0);
    st->valid = 0;
    struct bsg_action_traversal traversal;
    memset(&traversal, 0, sizeof(traversal));
    traversal.root = node;
    traversal.flags = BSG_ACTION_TRAVERSE_VISIBLE_ONLY;
    traversal.shape_cb = bbox_cb;
    traversal.userdata = st;
    (void)action;
    return bsg_action_traverse(&traversal);
}

static void
_free_simple_data(bsg_action_ref UNUSED(action), void *data)
{
    if (data)
	bu_free(data, "bsg action data");
}

bsg_action_ref
bsg_bbox_action_create(void)
{
    struct bbox_state *st;
    BU_ALLOC(st, struct bbox_state);
    memset(st, 0, sizeof(*st));
    return bsg_action_ref_create_internal(bsg_bbox_action_type(),
	    _bbox_action_apply, _free_simple_data, st, "bbox action");
}

int
bsg_bbox_action_result(bsg_action_ref action, point_t bmin, point_t bmax)
{
    struct bbox_state *st = (struct bbox_state *)bsg_action_ref_data(action);
    if (!st || !st->valid)
	return 0;
    if (bmin)
	VMOVE(bmin, st->bmin);
    if (bmax)
	VMOVE(bmax, st->bmax);
    return 1;
}

static int
_collect_action_apply(bsg_action_ref UNUSED(action), bsg_node_ref root, void *data)
{
    struct collect_action_data *st = (struct collect_action_data *)data;
    bsg_node *node = bsg_action_node_from_ref(root);
    if (!st || !node)
	return 0;
    bu_ptbl_reset(&st->nodes);
    struct bsg_action_traversal traversal;
    memset(&traversal, 0, sizeof(traversal));
    traversal.root = node;
    traversal.enter_cb = collect_enter_cb;
    traversal.userdata = st;
    return bsg_action_traverse(&traversal);
}

static void
_collect_action_destroy(bsg_action_ref UNUSED(action), void *data)
{
    struct collect_action_data *st = (struct collect_action_data *)data;
    if (!st)
	return;
    bu_ptbl_free(&st->nodes);
    bu_free(st, "collect action data");
}

bsg_action_ref
bsg_collect_action_create(bsg_type_id type)
{
    struct collect_action_data *st;
    BU_ALLOC(st, struct collect_action_data);
    memset(st, 0, sizeof(*st));
    st->type = bsg_type_is_a(type, bsg_node_type()) ? type : BSG_TYPE_INVALID;
    bu_ptbl_init(&st->nodes, 8, "bsg collect action nodes");
    return bsg_action_ref_create_internal(bsg_collect_action_type(),
	    _collect_action_apply, _collect_action_destroy, st, "collect action");
}

size_t
bsg_collect_action_count(bsg_action_ref action)
{
    struct collect_action_data *st =
	(struct collect_action_data *)bsg_action_ref_data(action);
    return st ? BU_PTBL_LEN(&st->nodes) : 0;
}

bsg_node_ref
bsg_collect_action_get(bsg_action_ref action, size_t idx)
{
    struct collect_action_data *st =
	(struct collect_action_data *)bsg_action_ref_data(action);
    if (!st || idx >= BU_PTBL_LEN(&st->nodes))
	return bsg_node_ref_null();
    return bsg_action_node_ref_from_node((bsg_node *)BU_PTBL_GET(&st->nodes, idx));
}

static int
_search_action_apply(bsg_action_ref UNUSED(action), bsg_node_ref root, void *data)
{
    struct search_action_data *st = (struct search_action_data *)data;
    bsg_node *node = bsg_action_node_from_ref(root);
    if (!st || !node)
	return 0;
    bu_ptbl_reset(&st->nodes);
    struct bsg_action_traversal traversal;
    memset(&traversal, 0, sizeof(traversal));
    traversal.root = node;
    traversal.enter_cb = search_enter_cb;
    traversal.userdata = st;
    return bsg_action_traverse(&traversal);
}

static void
_search_action_destroy(bsg_action_ref UNUSED(action), void *data)
{
    struct search_action_data *st = (struct search_action_data *)data;
    if (!st)
	return;
    bu_vls_free(&st->name);
    bu_ptbl_free(&st->nodes);
    bu_free(st, "search action data");
}

bsg_action_ref
bsg_search_action_create(const char *name)
{
    struct search_action_data *st;
    BU_ALLOC(st, struct search_action_data);
    memset(st, 0, sizeof(*st));
    bu_vls_init(&st->name);
    if (name)
	bu_vls_sprintf(&st->name, "%s", name);
    bu_ptbl_init(&st->nodes, 8, "bsg search action nodes");
    return bsg_action_ref_create_internal(bsg_search_action_type(),
	    _search_action_apply, _search_action_destroy, st, "search action");
}

int
bsg_search_action_set_name(bsg_action_ref action, const char *name)
{
    struct search_action_data *st =
	(struct search_action_data *)bsg_action_ref_data(action);
    if (!st)
	return 0;
    bu_vls_trunc(&st->name, 0);
    if (name)
	bu_vls_sprintf(&st->name, "%s", name);
    return 1;
}

size_t
bsg_search_action_count(bsg_action_ref action)
{
    struct search_action_data *st =
	(struct search_action_data *)bsg_action_ref_data(action);
    return st ? BU_PTBL_LEN(&st->nodes) : 0;
}

bsg_node_ref
bsg_search_action_get(bsg_action_ref action, size_t idx)
{
    struct search_action_data *st =
	(struct search_action_data *)bsg_action_ref_data(action);
    if (!st || idx >= BU_PTBL_LEN(&st->nodes))
	return bsg_node_ref_null();
    return bsg_action_node_ref_from_node((bsg_node *)BU_PTBL_GET(&st->nodes, idx));
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
