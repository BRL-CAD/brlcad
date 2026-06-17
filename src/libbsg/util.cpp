/*                      U T I L . C P P
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
/** @file util.cpp
 *
 * Utility functions for operating on BRL-CAD views
 *
 */

#include "common.h"
#include <queue>
#include <string.h>
#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bn/mat.h"
#include "bg/plane.h"
#include "bsg/appearance.h"
#include "bsg/defines.h"
#include "bsg/draw_intent.h"
#include "bsg/draw_source.h"
#include "bsg/geometry.h"
#include "bsg/identity.h"
#include "bsg/material.h"
#include "bsg/node.h"
#include "bsg/object.h"
#include "bsg/overlay.h"
#include "payload_typed_private.h"
#include "bsg/selection.h"
#include "bsg/snap.h"
#include "bsg/snap_action.h"
#include "bsg/util.h"
#include "bsg/scene_object.h"
#include "bsg/view_set.h"
#include "bsg/view_state.h"
#include "bsg/defines.h"
#include "appearance_private.h"
#include "draw_intent_private.h"
#include "draw_set_private.h"
#include "draw_source_private.h"
#include "feature_private.h"
#include "field_private.h"
#include "geometry_private.h"
#include "node_private.h"
#include "object_private.h"
#include "overlay_private.h"
#include "payload_private.h"
#include "scene_object_private.h"
#include "scene_store_private.h"
#include "./bsg_private.h"
#include "node_storage_private.h"
#include "view_state_private.h"

#define VIEW_NAME_MAXTRIES 100000
#define DM_DEFAULT_FONT_SIZE 20
#define BSG_INDEPENDENT_SCOPE_NAME "_independent_db_scope"

static const char *
_bsg_vname(struct bsg_view *v)
{
    if (!v)
	return "NULL";
    if (!BU_VLS_IS_INITIALIZED(&v->gv_name))
	return "<unnamed>";
    return bu_vls_cstr(&v->gv_name);
}

static int
_bsg_is_independent_scope(struct bsg_node *s, const struct bsg_view *owner)
{
    if (!s)
	return 0;
    if (!bsg_node_type_is_a(s, bsg_view_scope_type()))
	return 0;
    if (bsg_node_get_view(s) != owner)
	return 0;
    return BU_STR_EQUAL(bsg_node_name(s), BSG_INDEPENDENT_SCOPE_NAME);
}

static int
_bsg_view_scope_visible(struct bsg_node *scope, struct bsg_view *v)
{
    if (!scope || !bsg_node_type_is_a(scope, bsg_view_scope_type()))
	return 0;
    if (!bsg_node_get_view(scope))
	return 1;
    return (bsg_node_get_view(scope) == v) ? 1 : 0;
}

static struct bsg_node *
_bsg_independent_scope_find(struct bsg_node *root, const struct bsg_view *owner)
{
    if (!root || !owner)
	return NULL;

    for (size_t i = 0; i < BU_PTBL_LEN(&root->children); i++) {
	struct bsg_node *c = (struct bsg_node *)BU_PTBL_GET(&root->children, i);
	if (_bsg_is_independent_scope(c, owner))
	    return c;
    }

    return NULL;
}

static void
_bsg_scope_free_recursive(struct bsg_node *node)
{
    if (!node)
	return;

    size_t i = BU_PTBL_LEN(&node->children);
    while (i > 0) {
	i--;
	struct bsg_node *child = (struct bsg_node *)BU_PTBL_GET(&node->children, i);
	if (!child)
	    continue;
	bu_ptbl_rm(&node->children, (long *)child);
	_bsg_scope_free_recursive(child);
    }

    bsg_scene_node_release(node);
}

static int
_bsg_independent_root_skip_child(struct bsg_view *v, struct bsg_node *parent, struct bsg_node *child)
{
    if (!v || !parent || !child)
	return 0;
    if (!bsg_view_is_independent(v))
	return 0;
    if (parent != bsg_view_scene_root(v))
	return 0;
    if (bsg_node_type_is_a(child, bsg_view_scope_type()))
	return 0;
    return BU_STR_EQUAL("_overlays", bsg_node_name(child)) ? 0 : 1;
}

int
bsg_view_is_independent(const struct bsg_view *v)
{
    if (!v)
	return 0;

    if (bsg_view_scene_root(v)) {
	struct bsg_node *root = bsg_view_scene_root(v);
	return (_bsg_independent_scope_find(root, v) != NULL) ? 1 : 0;
    }

    /* No retained scene anchor means the view is not yet registered; treat it as shared. */
    return 0;
}

static struct bsg_node *
_bsg_view_independent_scope(struct bsg_view *v, int create)
{
    if (!v || !bsg_view_scene_root(v))
	return NULL;

    struct bsg_node *root = bsg_view_scene_root(v);
    struct bsg_node *scope = _bsg_independent_scope_find(root, v);
    if (scope || !create)
	return scope;

    scope = bsg_scene_node_create_detached(v, BSG_SOURCE_CHILD | BSG_SOURCE_LOCAL);
    if (!scope)
	return NULL;

    bsg_node_set_object_type(scope, bsg_view_scope_type());
    bsg_node_set_visible_state(scope, 1);
    bsg_node_set_view(scope, v);
    bsg_node_set_name(scope, BSG_INDEPENDENT_SCOPE_NAME);
    bsg_node_add_child(root, scope);

    return scope;
}

bsg_scene_ref
bsg_view_independent_scope_ref(struct bsg_view *v, int create)
{
    bsg_scene_ref ref = BSG_SCENE_REF_NULL_INIT;
    ref.opaque = (void *)_bsg_view_independent_scope(v, create);
    return ref;
}

void
bsg_view_independent_scope_destroy(struct bsg_view *v)
{
    if (!v || !bsg_view_scene_root(v))
	return;

    struct bsg_node *root = bsg_view_scene_root(v);
    struct bsg_node *scope = _bsg_independent_scope_find(root, v);
    if (!scope)
	return;

    bu_ptbl_rm(&root->children, (long *)scope);
    _bsg_scope_free_recursive(scope);
}

void
bsg_data_tclcad_init(struct bsg_data_tclcad *d)
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
bsg_init(struct bsg_view *gvp, struct bsg_view_set *s)
{
    if (!gvp)
	return;

    gvp->magic = BSG_VIEW_MAGIC;
    gvp->vset = s;

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
    struct bu_ptbl *views = bsg_set_views(s);
    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	struct bsg_view *nv = (struct bsg_view *)BU_PTBL_GET(views, i);
	if (!bu_vls_strcmp(&nv->gv_name, &gvp->gv_name)) {
	    name_collide = true;
	    break;
	}
    }
    while (name_collide && view_try_cnt < VIEW_NAME_MAXTRIES) {
	bu_vls_incr(&gvp->gv_name, NULL, "0:0:0:0", NULL, NULL);
	name_collide = false;
	for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	    struct bsg_view *nv = (struct bsg_view *)BU_PTBL_GET(views, i);
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

    /* Independent state is tracked by a BSG independent-scope node. */
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
    BU_ALLOC(gvp->settings_local, struct bsg_view_settings);
    bsg_settings_init(gvp->settings_local);

    gvp->settings_local->gv_selected = bsg_selection_create();

    /* Out of the gate we don't have any shared settings */
    gvp->settings_active = gvp->settings_local;

    gvp->gv_state = bsg_view_state_create();
    bsg_view_state_sync_from_view(gvp->gv_state, gvp);

    /* FIXME: this causes the shaders.sh regression to fail */
    /* bsg_mat_aet(gvp); */


    // Until the app tells us differently, we use local scene-object storage.
    bsg_scene_view_obj_pool_init(&gvp->gv_objs);

    // Out of the gate we don't have callbacks
    gvp->callbacks = NULL;
    gvp->gv_callback = NULL;
    gvp->gv_bounds_update= NULL;

    // Also don't have a display manager
    // TODO - What the heck Archer??? Initializing this to NULL causes
    // problems even without the gv_name setting logic above?
    //gvp->dmp = NULL;

    // Initial scaling factors are 1
    gvp->gv_base2local = 1.0;
    gvp->gv_local2base = 1.0;

    // Initialize knob vars
    bsg_knobs_reset(&gvp->k, BSG_KNOBS_ALL);
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

    // TclCAD owns Tcl-specific view data.  Non-Tcl views leave this pointer
    // NULL; libtclcad binds it to tclcad_view_data while the view is live.
    gvp->gv_tcl = NULL;

    // bsg_init creates the retained scene anchor below after all view
    // storage needed by scene nodes has been initialized.
    gvp->gv_scene = NULL;
    gvp->gv_hud_root = NULL;

    // No edit-mode matrix override until explicitly set by the renderer
    gvp->gv_edit_mat = NULL;

    (void)bsg_view_scene_separator_ref(gvp, 1);

    bsg_update(gvp);
}

static int
_bsg_view_root_shared_by_set(const struct bsg_view *gvp, const bsg_node *root)
{
    if (!gvp || !root || !gvp->vset)
	return 0;

    struct bu_ptbl *views = bsg_set_views(gvp->vset);
    if (!views)
	return 0;

    for (size_t i = 0; i < BU_PTBL_LEN(views); i++) {
	struct bsg_view *v = (struct bsg_view *)BU_PTBL_GET(views, i);
	if (v && v != gvp && bsg_view_scene_root(v) == root)
	    return 1;
    }

    return 0;
}

void
bsg_free(struct bsg_view *gvp)
{
    if (!gvp)
	return;

    bu_vls_free(&gvp->gv_name);

    bsg_node *root = bsg_view_scene_root(gvp);
    if (root && bsg_node_get_view(root) == gvp &&
	    !_bsg_view_root_shared_by_set(gvp, root)) {
	bsg_node_destroy(root);
	gvp->gv_scene = NULL;
    } else {
	gvp->gv_scene = NULL;
    }

    bsg_scene_view_obj_pool_free(&gvp->gv_objs);
    if (gvp->settings_local && gvp->settings_local->gv_selected) {
	bsg_selection_destroy(gvp->settings_local->gv_selected);
	gvp->settings_local->gv_selected = NULL;
    }
    if (gvp->settings_local) {
	BU_PUT(gvp->settings_local, struct bsg_view_settings);
	gvp->settings_local = NULL;
    }
    gvp->settings_active = NULL;

    bsg_view_state_destroy(gvp->gv_state);
    gvp->gv_state = NULL;

    if (gvp->callbacks) {
	bu_ptbl_free(gvp->callbacks);
	BU_PUT(gvp->callbacks, struct bu_ptbl);
    }
}

static int
_node_autoview_cube(point_t minus, point_t plus, struct bsg_node *node, struct bsg_view *v)
{
    point_t bmin, bmax;
    if (!bsg_scene_node_update_bounds(node, v) || !bsg_node_bounds(node, bmin, bmax))
	return 0;

    point_t center;
    fastf_t size = bmax[X] - bmin[X];
    V_MAX(size, bmax[Y] - bmin[Y]);
    V_MAX(size, bmax[Z] - bmin[Z]);
    VSET(center,
	    (bmin[X] + bmax[X]) * 0.5,
	    (bmin[Y] + bmax[Y]) * 0.5,
	    (bmin[Z] + bmax[Z]) * 0.5);
    VSET(minus, center[X] - size, center[Y] - size, center[Z] - size);
    VSET(plus, center[X] + size, center[Y] + size, center[Z] + size);
    return 1;
}

static void
_bound_objs(int *is_empty, int *have_geom_objs, vect_t min, vect_t max, struct bu_ptbl *so, struct bsg_view *v)
{
    vect_t minus, plus;
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bsg_node *g = (struct bsg_node *)BU_PTBL_GET(so, i);
	_bound_objs(is_empty, have_geom_objs, min, max, &g->children, v);
	if (_node_autoview_cube(minus, plus, g, v)) {
	    (*is_empty) = 0;
	    (*have_geom_objs) = 1;
	    VMIN(min, minus);
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
	struct bsg_node *s = (struct bsg_node *)BU_PTBL_GET(so, i);
	_find_view_geom(have_geom_objs, &s->children);
	if (bsg_node_has_geometry_role(s, BSG_GEOMETRY_DB_OBJECT) ||
		bsg_node_has_geometry_role(s, BSG_GEOMETRY_POLYGON_REGION) ||
		bsg_node_has_geometry_role(s, BSG_GEOMETRY_TEXT_LABELS)) {
	    (*have_geom_objs) = 1;
	    break;
	}
    }
}

static void
_bound_objs_view(int *is_empty, vect_t min, vect_t max, struct bu_ptbl *so, struct bsg_view *v, int have_geom_objs, int all_view_objs)
{
    vect_t minus, plus;
    for (size_t i = 0; i < BU_PTBL_LEN(so); i++) {
	struct bsg_node *s = (struct bsg_node *)BU_PTBL_GET(so, i);
	_bound_objs_view(is_empty, min, max, &s->children, v, have_geom_objs, all_view_objs);
	if (have_geom_objs && !all_view_objs) {
	    if (!bsg_node_has_geometry_role(s, BSG_GEOMETRY_DB_OBJECT) &&
		!bsg_node_has_geometry_role(s, BSG_GEOMETRY_POLYGON_REGION) &&
		!bsg_node_has_geometry_role(s, BSG_GEOMETRY_TEXT_LABELS))
		continue;
	}
	if (_node_autoview_cube(minus, plus, s, v)) {
	    (*is_empty) = 0;
	    VMIN(min, minus);
	    VMAX(max, plus);
	}
    }
}


/* Context for bsg_autoview's database-source visit callback. */
struct _bsg_autoview_db_ctx {
    int *is_empty;
    int *have_geom_objs;
    vect_t min;
    vect_t max;
    struct bsg_view *v;
};

static int
_bsg_autoview_db_cb(struct bsg_node *s, void *data)
{
    struct _bsg_autoview_db_ctx *ctx = (struct _bsg_autoview_db_ctx *)data;
    vect_t minus, plus;
    /* For non-BSG top-level groups, recurse into their children first */
    _bound_objs(ctx->is_empty, ctx->have_geom_objs, ctx->min, ctx->max,
		&s->children, ctx->v);
    /* Check this object's own bounds */
    if (_node_autoview_cube(minus, plus, s, ctx->v)) {
	(*ctx->is_empty) = 0;
	(*ctx->have_geom_objs) = 1;
	VMIN(ctx->min, minus);
	VMAX(ctx->max, plus);
    }
    return 1;
}

/* Context for bsg_autoview's feature-scope pass.  All output fields are
 * pointers so the callback updates the caller's storage in-place. */
struct _bsg_autoview_view_ctx {
    int *is_empty;
    fastf_t *min;   /* vect_t — fastf_t[3] */
    fastf_t *max;   /* vect_t — fastf_t[3] */
    struct bsg_view *v;
    int have_geom_objs;
    int all_view_objs;
};

/* Pass 1 callback: set have_geom_objs if any shape node has a geometric type. */
static int
_bsg_find_view_geom_visit_cb(struct bsg_node *s, void *data)
{
    int *have_geom_objs = (int *)data;
    _find_view_geom(have_geom_objs, &s->children);
    if (!(*have_geom_objs)) {
	if (bsg_node_has_geometry_role(s, BSG_GEOMETRY_DB_OBJECT) ||
	    bsg_node_has_geometry_role(s, BSG_GEOMETRY_POLYGON_REGION) ||
	    bsg_node_has_geometry_role(s, BSG_GEOMETRY_TEXT_LABELS))
	    (*have_geom_objs) = 1;
    }
    return 1;
}

/* Pass 2 callback: bound each shape node and its children. */
static int
_bsg_bound_view_obj_cb(struct bsg_node *s, void *data)
{
    struct _bsg_autoview_view_ctx *ctx = (struct _bsg_autoview_view_ctx *)data;
    vect_t minus, plus;
    _bound_objs_view(ctx->is_empty, ctx->min, ctx->max, &s->children,
		     ctx->v, ctx->have_geom_objs, ctx->all_view_objs);
    if (ctx->have_geom_objs && !ctx->all_view_objs) {
	if (!bsg_node_has_geometry_role(s, BSG_GEOMETRY_DB_OBJECT) &&
	    !bsg_node_has_geometry_role(s, BSG_GEOMETRY_POLYGON_REGION) &&
	    !bsg_node_has_geometry_role(s, BSG_GEOMETRY_TEXT_LABELS))
	    return 1;
    }
    if (_node_autoview_cube(minus, plus, s, ctx->v)) {
	(*ctx->is_empty) = 0;
	VMIN(ctx->min, minus);
	VMAX(ctx->max, plus);
    }
    return 1;
}

void
bsg_autoview_bounds(struct bsg_view *v, fastf_t factor, const point_t min, const point_t max)
{
    vect_t center = VINIT_ZERO;
    vect_t radial;
    vect_t sqrt_small;

    /* set the default if unset or insane */
    if (factor < SQRT_SMALL_FASTF) {
	factor = 2.0; /* 2 is half the view */
    }

    VSETALL(sqrt_small, SQRT_SMALL_FASTF);

    VADD2SCALE(center, max, min, 0.5);
    VSUB2(radial, max, center);

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
    bsg_update(v);
}

void
bsg_autoview(struct bsg_view *v, fastf_t factor, int all_view_objs)
{
    vect_t min, max;
    int is_empty = 1;
    int have_geom_objs = 0;

    /* calculate the bounding for all solids and polygons being displayed */
    VSETALL(min,  INFINITY);
    VSETALL(max, -INFINITY);

    /* Visit database sources through the retained scene when available. */
    struct _bsg_autoview_db_ctx bav_ctx;
    bav_ctx.is_empty = &is_empty;
    bav_ctx.have_geom_objs = &have_geom_objs;
    VSETALL(bav_ctx.min,  INFINITY);
    VSETALL(bav_ctx.max, -INFINITY);
    bav_ctx.v = v;
    bsg_scene_visit_db(v, _bsg_autoview_db_cb, &bav_ctx);
    if (!is_empty) {
	VMOVE(min, bav_ctx.min);
	VMOVE(max, bav_ctx.max);
    }

    // When it comes to view-scoped features, normally we will only include those
    // that are db object based, polygons or labels, unless the flag to
    // consider all objects is set.   However, there is an exception - if there
    // are NO such objects in the scene (have_geom_objs == 0) and we do have
    // view objs (for example, when overlaying a plot file on an empty view)
    // then basing autoview on the view-scoped features is more intuitive than just
    // using the default view settings.

    /* Two passes: collect have_geom_objs across all feature nodes first, then
     * bound them so the geom-filter logic has complete information. */
    bsg_feature_visit_nodes(v, BSG_FEATURE_SCOPE_ALL, _bsg_find_view_geom_visit_cb, &have_geom_objs);
    {
	struct _bsg_autoview_view_ctx vctx;
	vctx.is_empty = &is_empty;
	vctx.min = min;
	vctx.max = max;
	vctx.v = v;
	vctx.have_geom_objs = have_geom_objs;
	vctx.all_view_objs = all_view_objs;
	bsg_feature_visit_nodes(v, BSG_FEATURE_SCOPE_ALL, _bsg_bound_view_obj_cb, &vctx);
    }

    if (is_empty) {
	/* Nothing is in view - frame a default region centered on the
	 * origin (equivalent to the historical radial extent of 1000). */
	VSETALL(min, -1000.0);
	VSETALL(max,  1000.0);
    }

    bsg_autoview_bounds(v, factor, min, max);
}

/**
 * FIXME: this routine is suspect and needs investigating.  if run
 * during view initialization, the shaders regression test fails.
 */
void
bsg_mat_aet(struct bsg_view *v)
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

/* --- Camera accessor implementations --- */

fastf_t
bsg_view_get_scale(const struct bsg_view *v)
{
    if (!v) return 0.0;
    return v->gv_scale;
}

void
bsg_view_set_scale(struct bsg_view *v, fastf_t scale)
{
    if (!v) return;
    v->gv_scale = scale;
    v->gv_size  = scale * 2.0;
    v->gv_isize = (v->gv_size > 0.0) ? 1.0 / v->gv_size : 0.0;
}

fastf_t
bsg_view_get_size(const struct bsg_view *v)
{
    if (!v) return 0.0;
    return v->gv_size;
}

void
bsg_view_set_size(struct bsg_view *v, fastf_t size)
{
    if (!v) return;
    v->gv_size  = size;
    v->gv_scale = size * 0.5;
    v->gv_isize = (size > 0.0) ? 1.0 / size : 0.0;
}

fastf_t
bsg_view_get_perspective(const struct bsg_view *v)
{
    if (!v) return 0.0;
    return v->gv_perspective;
}

void
bsg_view_set_perspective(struct bsg_view *v, fastf_t perspective)
{
    if (!v) return;
    v->gv_perspective = perspective;
}

void
bsg_view_get_aet(const struct bsg_view *v, vect_t aet)
{
    if (!v) { VSETALL(aet, 0.0); return; }
    VMOVE(aet, v->gv_aet);
}

void
bsg_view_set_aet(struct bsg_view *v, const vect_t aet)
{
    if (!v) return;
    VMOVE(v->gv_aet, aet);
    bsg_mat_aet(v);
}

void
bsg_view_get_rotation(const struct bsg_view *v, mat_t rot)
{
    if (!v) { MAT_IDN(rot); return; }
    MAT_COPY(rot, v->gv_rotation);
}

void
bsg_view_set_rotation(struct bsg_view *v, const mat_t rot)
{
    if (!v) return;
    MAT_COPY(v->gv_rotation, rot);
}

void
bsg_view_get_center_vec(const struct bsg_view *v, point_t center)
{
    if (!v) { VSETALL(center, 0.0); return; }
    MAT_DELTAS_GET_NEG(center, v->gv_center);
}

void
bsg_view_set_center_vec(struct bsg_view *v, const point_t center)
{
    if (!v) return;
    MAT_DELTAS_VEC_NEG(v->gv_center, center);
}

/* --- end camera accessors --- */

void
bsg_settings_init(struct bsg_view_settings *s)
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

    s->lod_source_policy.policy = BSG_LOD_AUTO;
    s->lod_source_policy.forced_level = 0;
    s->lod_source_policy.mesh_enabled = 0;
    s->lod_source_policy.csg_enabled = 0;
    s->lod_source_policy.zoom_refresh = 0;
    s->lod_source_policy.scale = 1.0;
    s->lod_source_policy.bot_threshold = 0;
    s->lod_source_policy.curve_scale = 1.0;
    s->lod_source_policy.point_scale = 1.0;

    // Higher values indicate more aggressive behavior (i.e. points further away will be snapped).
    s->gv_snap_tol_factor = 10;
    s->gv_snap_lines = 0;
    s->gv_snap_flags = 0;
    bsg_feature_ref null_ref = BSG_FEATURE_REF_NULL_INIT;
    s->gv_snap_exclude_feature = null_ref;
}

// TODO - investigate saveview/loadview logic, see if anything
// makes sense to move here
void
bsg_sync(struct bsg_view *dest, struct bsg_view *src)
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
    dest->gv_local2base = src->gv_local2base;
    dest->gv_rscale = src->gv_rscale;
    dest->gv_sscale = src->gv_sscale;
    V2MOVE(dest->gv_wmin, src->gv_wmin);
    V2MOVE(dest->gv_wmax, src->gv_wmax);

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

static void
_bsg_view_selection_copy(struct bsg_selection *dest,
	const struct bsg_selection *src)
{
    if (!dest)
	return;

    bsg_selection_clear(dest);
    if (!src)
	return;

    for (size_t i = 0; i < bsg_selection_count(src); i++) {
	const struct bsg_interaction_record *record =
	    bsg_selection_record(src, i);
	if (record)
	    bsg_selection_add_record(dest, record);
    }
}

static void
_bsg_view_settings_copy(struct bsg_view_settings *dest,
	const struct bsg_view_settings *src)
{
    if (!dest || !src)
	return;

    struct bsg_selection *selection = dest->gv_selected;
    *dest = *src;
    dest->gv_selected = selection ? selection : bsg_selection_create();
    _bsg_view_selection_copy(dest->gv_selected, src->gv_selected);
}

void
bsg_view_init_copy(struct bsg_view *dest,
	const struct bsg_view *src,
	struct bsg_view_set *s)
{
    if (!dest)
	return;

    bsg_view_init(dest, s);
    if (!src)
	return;

    if (BU_VLS_IS_INITIALIZED(&src->gv_name))
	bu_vls_sprintf(&dest->gv_name, "%s", bu_vls_cstr(&src->gv_name));

    bsg_sync(dest, (struct bsg_view *)src);

    dest->gv_prevMouseX = src->gv_prevMouseX;
    dest->gv_prevMouseY = src->gv_prevMouseY;
    dest->gv_mouse_x = src->gv_mouse_x;
    dest->gv_mouse_y = src->gv_mouse_y;
    VMOVE(dest->gv_prev_point, src->gv_prev_point);
    VMOVE(dest->gv_point, src->gv_point);
    dest->gv_key = src->gv_key;
    dest->gv_mod_flags = src->gv_mod_flags;
    dest->gv_minMouseDelta = src->gv_minMouseDelta;
    dest->gv_maxMouseDelta = src->gv_maxMouseDelta;

    VMOVE(dest->obb_center, src->obb_center);
    VMOVE(dest->obb_extent1, src->obb_extent1);
    VMOVE(dest->obb_extent2, src->obb_extent2);
    VMOVE(dest->obb_extent3, src->obb_extent3);
    VMOVE(dest->gv_vc_backout, src->gv_vc_backout);
    VMOVE(dest->gv_lookat, src->gv_lookat);
    dest->radius = src->radius;
    dest->k = src->k;
    VMOVE(dest->orig_pos, src->orig_pos);
    dest->gv_frame_rev = src->gv_frame_rev;
    dest->gv_camera_node = src->gv_camera_node;

    _bsg_view_settings_copy(dest->settings_local,
	    bsg_view_settings_active_const(src));
    dest->settings_active = dest->settings_local;

    struct bsg_view_refresh_state refresh;
    if (bsg_view_refresh_get(src, &refresh))
	bsg_view_refresh_set(dest, &refresh);

    bsg_view_state_sync_from_view(dest->gv_state, dest);
}

void
bsg_update(struct bsg_view *gvp)
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
		bu_log("Recursive callback (bsg_update and gvp->gv_callback)");
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
bsg_appearance_settings_sync(struct bsg_appearance_settings *dest, const struct bsg_appearance_settings *src)
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
    if (dest->draw_mode != src->draw_mode) {
	dest->draw_mode = src->draw_mode;
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
    if (dest->strict_fallback != src->strict_fallback) {
	dest->strict_fallback = src->strict_fallback;
	ret = 1;
    }

    return ret;
}

int
bsg_update_selected(struct bsg_view *gvp)
{
    int ret = 0;
    if (!gvp)
	return 0;
    return (ret > 0) ? 1 : 0;
}

// TODO - support constraints
int
_bsg_rot(struct bsg_view *v, int dx, int dy, point_t keypoint, unsigned long long UNUSED(flags))
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
    bsg_update(v);

    return 1;
}

int
_bsg_trans(struct bsg_view *v, int dx, int dy, point_t UNUSED(keypoint), unsigned long long UNUSED(flags))
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
    bsg_update(v);

    return 1;
}

int
_bsg_scale(struct bsg_view *v, int sensitivity, int factor, point_t UNUSED(keypoint), unsigned long long UNUSED(flags))
{
    if (!v)
	return 0;

    double f = (double)factor/(double)sensitivity;
    v->gv_scale /= f;
    if (v->gv_scale < BSG_MINVIEWSCALE)
	v->gv_scale = BSG_MINVIEWSCALE;
    v->gv_size = 2.0 * v->gv_scale;
    v->gv_isize = 1.0 / v->gv_size;

    /* scale factors are set, now sync other bv values */
    bsg_update(v);

    return 1;
}

int
_bsg_center(struct bsg_view *v, int vx, int vy, point_t UNUSED(keypoint), unsigned long long UNUSED(flags))
{
    if (!v)
	return 0;

    point_t vpt, center;
    fastf_t fx = 0.0;
    fastf_t fy = 0.0;
    bsg_screen_to_view(v, &fx, &fy, (fastf_t)vx, (fastf_t)vy);
    VSET(vpt, fx, fy, 0);
    MAT4X3PNT(center, v->gv_view2model, vpt);
    MAT_DELTAS_VEC_NEG(v->gv_center, center);
    bsg_update(v);
    return 1;
}

int
bsg_adjust(struct bsg_view *v, int dx, int dy, point_t keypoint, int UNUSED(mode), unsigned long long flags)
{
    if (flags == BSG_IDLE)
	return 0;

    // TODO - figure out why these need to be flipped for qdm to do the right thing...
    if (flags & BSG_ROT)
	return _bsg_rot(v, dy, dx, keypoint, flags);

    if (flags & BSG_TRANS)
	return _bsg_trans(v, dx, dy, keypoint, flags);

    if (flags & BSG_SCALE)
	return _bsg_scale(v, dx, dy, keypoint, flags);

    if (flags & BSG_CENTER)
	return _bsg_center(v, dx, dy, keypoint, flags);


    return 0;
}


int
bsg_screen_to_view(struct bsg_view *v, fastf_t *fx, fastf_t *fy, fastf_t x, fastf_t y)
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

    // If snapping is enabled, apply it
    bsg_snap_kind_mask kinds = bsg_view_snap_kind_mask(v);
    if (kinds && fx && fy)
	bsg_snap_point_2d(v, fx, fy, kinds);

    return 0;
}

int
bsg_screen_pt(point_t *p, fastf_t x, fastf_t y, struct bsg_view *v)
{
    if (!p || !v)
	return -1;

    if (!v->gv_width || !v->gv_height)
	return -1;

    fastf_t tx, ty;
    if (bsg_screen_to_view(v, &tx, &ty, x, y))
	return -1;

    point_t vpt;
    VSET(vpt, tx, ty, 0);
    MAT4X3PNT(*p, v->gv_view2model, vpt);
    return 0;
}

int
bsg_view_plane(plane_t *p, struct bsg_view *v)
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

/* Count callback for bsg_clear's feature-scope pass. */
static int
_bsg_count_view_obj_cb(struct bsg_node *UNUSED(obj), void *data)
{
    size_t *count = (size_t *)data;
    (*count)++;
    return 1;
}

size_t
bsg_clear(struct bsg_view *v, int flags)
{
    if (!flags || flags & BSG_SOURCE_DB) {
	bsg_scene_store_db_nodes_release(v, BSG_SOURCE_DB | (flags & ~BSG_SOURCE_VIEW));
    }

    /* View-only objects live in BSG view-scope feature nodes. */
    if (!flags || flags & BSG_SOURCE_VIEW) {
	int scope_mask = 0;
	if (!flags) {
	    scope_mask = BSG_FEATURE_SCOPE_ALL;
	} else if (flags & BSG_SOURCE_LOCAL) {
	    scope_mask = BSG_FEATURE_SCOPE_LOCAL;
	} else {
	    scope_mask = BSG_FEATURE_SCOPE_SHARED;
	}
	bsg_feature_remove_all(v, scope_mask);
    }

    if (!flags || flags & BSG_SOURCE_LOCAL || bsg_view_is_independent(v)) {
	if (!flags || flags & BSG_SOURCE_DB) {
	    bsg_scene_store_db_nodes_release(v, BSG_SOURCE_DB | (flags & ~BSG_SOURCE_VIEW) | BSG_SOURCE_LOCAL);
	}
	/* VIEW_OBJS local clear already handled above via scope_mask */
    }

    /* Count view-scoped features via the feature-scope walk. */
    size_t vo_count = 0;
    bsg_feature_visit_nodes(v, BSG_FEATURE_SCOPE_ALL, _bsg_count_view_obj_cb, &vo_count);

    size_t ocnt = 0;
    ocnt += bsg_scene_store_db_nodes_count(v, BSG_SOURCE_DB);
    if (!bsg_view_is_independent(v))
	ocnt += bsg_scene_store_db_nodes_count(v, BSG_SOURCE_DB | BSG_SOURCE_LOCAL);
    ocnt += vo_count;
    return ocnt;
}

void
bsg_scene_node_invalidate(struct bsg_node *s)
{
    if (BU_PTBL_IS_INITIALIZED(&s->children)) {
	for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	    struct bsg_node *s_c = (struct bsg_node *)BU_PTBL_GET(&s->children, i);
	    bsg_scene_node_invalidate(s_c);
	}
    }
}

struct bsg_node *
bsg_scene_node_create(struct bsg_view *v, int type)
{
    if (!v)
	return NULL;

    bsg_log(1, "bsg_scene_node_create (%s)", _bsg_vname(v));

    struct bsg_node *s = NULL;

    struct bsg_scene_recycle_pool *recycle_pool = NULL;
    recycle_pool = bsg_scene_recycle_pool_for_create(v, type);
    if (!recycle_pool)
	return NULL;

    // We know where we're going to get the object from - get it
    s = bsg_scene_recycle_acquire(recycle_pool);
    if (!s)
	return NULL;

    // Use reset to do most of the initialization
    bsg_scene_node_reset(s);

    // Set view
    bsg_node_set_view(s, v);
    bsg_node_set_source_flags(s, (unsigned int)type);

    return s;
}

struct bsg_node *
bsg_scene_node_create_registered(struct bsg_view *v, int type)
{
    if (!v)
	return NULL;

    bsg_log(1, "bsg_scene_node_create_registered %d(%s)", type, _bsg_vname(v));

   int ltype = type;
   if (bsg_view_is_independent(v))
	ltype |= BSG_SOURCE_LOCAL;

    struct bsg_node *s = bsg_scene_node_create(v, ltype);
    if (!s)
	return NULL;

    bsg_scene_store_node_register_db_record(s);

    return s;
}

struct bsg_node *
bsg_scene_node_create_detached(struct bsg_view *v, int type)
{
    /* Allocates a shape node without registering it in scene-store DB
     * records.  Used for leaves that are owned exclusively by the retained
     * draw scene.  The caller is responsible for freeing via
     * bsg_scene_node_release. */
    if (!v)
	return NULL;

    bsg_log(1, "bsg_scene_node_create_detached %d(%s)", type, _bsg_vname(v));

    int ltype = type;
    if (bsg_view_is_independent(v))
	ltype |= BSG_SOURCE_LOCAL;

    struct bsg_node *s = bsg_scene_node_create(v, ltype);
    if (!s)
	return NULL;

    /* Intentionally do NOT register this node in scene-store DB records.  It
     * is owned by the retained scene tree instead. */

    return s;
}

struct bsg_node *
bsg_scene_node_create_child(struct bsg_node *sp)
{
    if (!sp)
	return NULL;

    bsg_log(1, "bsg_scene_node_create_child %s(%s)", bsg_node_name(sp), _bsg_vname(bsg_node_get_view(sp)));

    struct bsg_node *s = NULL;

    // Children use their parent's storage pool.
    s = bsg_scene_recycle_acquire(bsg_scene_node_recycle_pool(sp));
    if (!s)
	return NULL;

    // Use reset to do most of the initialization
    bsg_scene_node_reset(s);

    struct bu_vls child_name = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&child_name, "child:%s:%zd", bsg_node_name(sp), BU_PTBL_LEN(&sp->children));
    bsg_node_set_name(s, bu_vls_cstr(&child_name));
    bu_vls_free(&child_name);

    bsg_node_set_view(s, bsg_node_get_view(sp));
    bsg_node_set_source_flags(s, bsg_node_source_flags(sp));
    bsg_node_set_object_type(s, bsg_shape_type());

    bsg_node_add_child(sp, s);

    return s;
}

void
bsg_scene_node_reset(struct bsg_node *s)
{
    // handle children
    if (BU_PTBL_IS_INITIALIZED(&s->children)) {
	for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	    struct bsg_node *s_c = (struct bsg_node *)BU_PTBL_GET(&s->children, i);
	    bsg_scene_node_release(s_c);
	}
    } else {
	BU_PTBL_INIT(&s->children);
    }
    bu_ptbl_reset(&s->children);

    // If we have a release callback for internal data, use it.
    bsg_node_invoke_release_callback(s);

    // Free typed payload.
    bsg_node_set_payload(s, NULL);

    // Free draw-intent metadata.
    bsg_node_set_draw_intent(s, NULL);

    bsg_feature_node_meta_clear(s);

    bsg_overlay_info_clear(s);

    struct bsg_node_internal *ni = bsg_node_internal_ensure(s);
    ni->changed = 0;
    ni->revision = 0;
    ni->drawn_revision = 0;
    ni->material_revision = 0;
    struct bsg_appearance_settings defaults = BSG_APPEARANCE_SETTINGS_INIT;
    ni->appearance.settings = defaults;
    ni->appearance.force_draw = 0;
    ni->appearance.inherit_settings = 0;
    ni->appearance.uses_default_color = 0;
    ni->appearance.work_flag = 0;
    ni->appearance.user_color = 0;
    ni->appearance.default_color = 0;
    ni->appearance.basecolor[0] = 0;
    ni->appearance.basecolor[1] = 0;
    ni->appearance.basecolor[2] = 0;
    ni->appearance.eval_flag = 0;
    ni->appearance.region_id = 0;
    ni->appearance.valid = 1;
    VSETALL(ni->draw_extent.center, 0.0);
    ni->draw_extent.size = 0.0;
    ni->draw_extent.valid = 0;
    ni->view_binding.view = NULL;
    ni->view_binding.valid = 0;
    ni->source_flags = 0;
    ni->source_flags_valid = 0;
    ni->geometry_roles = 0;
    ni->geometry_roles_valid = 0;
    ni->payload_flags = 0;
    ni->payload_flags_valid = 0;
    ni->non_database_source = 0;
    ni->non_database_source_valid = 1;
    ni->arrows_enabled = 0;
    ni->bbox_cache.valid = 0;
    ni->release_callback = NULL;
    memset(&ni->source_realization, 0, sizeof(ni->source_realization));
    ni->hud_meta = NULL;
    ni->hud_payload = NULL;
    ni->lod_payload = NULL;
    ni->draw_ctx = NULL;
    ni->interaction.highlight_state = DOWN;
    bsg_node_set_object_type(s, bsg_node_type());
    bsg_node_fields_reset(s);
}

void
bsg_scene_node_release(struct bsg_node *s)
{
    bsg_log(1, "bsg_scene_node_release %s[%s]", bsg_node_name(s), _bsg_vname(bsg_node_get_view(s)));
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bsg_node *cg = (struct bsg_node *)BU_PTBL_GET(&s->children, i);
	bsg_scene_node_release(cg);
    }

    struct bsg_scene_recycle_pool *fs = bsg_scene_node_recycle_pool(s);
    int had_db_record = bsg_scene_store_node_unregister_db_record(s);
    bsg_scene_node_reset(s);

    /* For parent-owned overlay nodes, remove from the parent scope's children
     * so no stale pointer remains. */
    if (!had_db_record && s->parent) {
	bu_ptbl_rm(&s->parent->children, (long *)s);
	s->parent = NULL;
    }

    if (fs)
	bsg_scene_recycle_release(fs, s);
}

struct bsg_node *
bsg_scene_node_find(struct bsg_view *v, const char *name)
{
    if (!v || !name)
	return NULL;

    struct bsg_node *db_match = bsg_scene_store_db_node_find(v, name, 1);
    if (db_match)
	return db_match;

    if (!bsg_view_scene_root(v))
	return NULL;

    std::queue<struct bsg_node *> nqueue;
    nqueue.push(bsg_view_scene_root(v));
    while (!nqueue.empty()) {
	struct bsg_node *n = nqueue.front();
	nqueue.pop();
	for (size_t i = 0; i < BU_PTBL_LEN(&n->children); i++) {
	    struct bsg_node *c = (struct bsg_node *)BU_PTBL_GET(&n->children, i);
	    if (!c)
		continue;
	    if (_bsg_independent_root_skip_child(v, n, c))
		continue;
	    if (bsg_node_type_is_a(c, bsg_view_scope_type()) && !_bsg_view_scope_visible(c, v))
		continue;
	    if (!bu_path_match(name, bsg_node_name(c), 0))
		return c;
	    nqueue.push(c);
	}
    }

    return NULL;
}

static bool
_uniq_name(const char *name, struct bsg_view *v)
{
    if (bsg_scene_node_find(v, name))
	return false;

    if (bsg_scene_store_db_node_name_exists(v, name))
	return false;

    /* View-scoped features are found through the retained-scene walk above. */

    return true;
}

void
bsg_uniq_obj_name(struct bu_vls *oname, const char *seed, struct bsg_view *v)
{
    if (!oname || !v)
	return;

    struct bu_vls vseed = BU_VLS_INIT_ZERO;
    if (seed) {
	bu_vls_sprintf(&vseed, "%s", seed);
    } else {
	bu_vls_sprintf(&vseed, "%s:obj_0", bu_vls_cstr(&v->gv_name));
    }


    const char *npattern = "([-_:]*[0-9]+[-_:]*)[^0-9]*$";
    long int loop_guard = 0;
    bool is_uniq = _uniq_name(bu_vls_cstr(&vseed), v);
    while (!is_uniq && loop_guard < LONG_MAX) {
	(void)bu_vls_incr(&vseed, npattern, NULL, NULL, NULL);
	is_uniq = _uniq_name(bu_vls_cstr(&vseed), v);
	loop_guard++;
    }

    bu_vls_sprintf(oname, "%s", bu_vls_cstr(&vseed));
    bu_vls_free(&vseed);
}

struct bsg_node *
bsg_scene_child_find(struct bsg_node *s, const char *vname)
{
    if (!s || !vname || !BU_PTBL_IS_INITIALIZED(&s->children))
	return NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(&s->children); i++) {
	struct bsg_node *s_c = (struct bsg_node *)BU_PTBL_GET(&s->children, i);
	if (!bu_path_match(vname, bsg_node_name(s_c), 0))
	    return s_c;
    }

    return NULL;
}

static int
_bsg_node_is_lod_type(const struct bsg_node *node)
{
    return bsg_node_type_is_a(node, bsg_lod_type());
}

static int
_bsg_lod_source_active_level(struct bsg_node *lod, struct bsg_view *v)
{
    if (!_bsg_node_is_lod_type(lod) || !v)
	return -1;

    struct bsg_lod_payload *pl = bsg_lod_node_payload(lod);
    if (!pl)
	return -1;

    for (size_t i = 0; i < pl->cursor_count; i++) {
	if (pl->cursors[i].v == v)
	    return pl->cursors[i].level;
    }

    return -1;
}

static const struct bsg_payload_line_set *
_bsg_node_payload_line_set(const struct bsg_node *node)
{
    struct bsg_payload *payload = bsg_node_get_payload(node);
    if (!payload)
	return NULL;
    if (payload->pl_type == BSG_PL_LINE_SET)
	return bsg_payload_line_set_get(payload);
    if (payload->pl_type == BSG_PL_SKETCH) {
	struct bsg_payload *geometry = bsg_payload_sketch_geometry(payload);
	return bsg_payload_line_set_get(geometry);
    }
    return NULL;
}

static int
_bsg_payload_line_set_bounds(const struct bsg_payload_line_set *ls,
			     point_t bmin,
			     point_t bmax)
{
    if (!ls || !ls->points || !ls->point_cnt)
	return 0;

    VSETALL(bmin, INFINITY);
    VSETALL(bmax, -INFINITY);
    for (size_t i = 0; i < ls->point_cnt; i++)
	VMINMAX(bmin, bmax, ls->points[i]);
    return 1;
}


static void
_bsg_scene_cache_draw_bounds(struct bsg_node *node,
			     const point_t bmin,
			     const point_t bmax)
{
    if (!node)
	return;

    bsg_node_set_bounds(node, bmin, bmax, 1);
    bsg_node_bbox_cache_set(node, bmin, bmax);

    point_t center;
    VSET(center,
	    (bmin[X] + bmax[X]) * 0.5,
	    (bmin[Y] + bmax[Y]) * 0.5,
	    (bmin[Z] + bmax[Z]) * 0.5);
    fastf_t size = bmax[X] - bmin[X];
    V_MAX(size, bmax[Y] - bmin[Y]);
    V_MAX(size, bmax[Z] - bmin[Z]);
    bsg_node_set_draw_center(node, center);
    bsg_node_set_draw_size(node, size);
}


static int
_bsg_scene_field_draw_bounds(struct bsg_node *node,
			     point_t bmin,
			     point_t bmax)
{
    if (!node || BU_PTBL_LEN(&node->children) != 0 || bsg_shape_is_mesh(node))
	return 0;

    const struct bsg_payload_line_set *ls = _bsg_node_payload_line_set(node);
    if (ls && ls->point_cnt)
	return 0;

    return bsg_node_bounds(node, bmin, bmax);
}


static int
_bsg_scene_lod_active_child_bounds(struct bsg_node *node,
				   struct bsg_view *v,
				   struct bsg_node **source,
				   point_t bmin,
				   point_t bmax)
{
    if (!node || !source)
	return 0;

    struct bsg_node *lod = NULL;
    if (_bsg_node_is_lod_type(node)) {
	lod = node;
    } else {
	struct bsg_node *p = (struct bsg_node *)node->parent;
	if (_bsg_node_is_lod_type(p))
	    lod = p;
    }

    if (!lod)
	return 0;

    int active = _bsg_lod_source_active_level(lod, v);
    int nlevels = (int)BU_PTBL_LEN(&lod->children);
    if (nlevels <= 0)
	return 0;

    if (active < 0 || active >= nlevels)
	active = 0;

    struct bsg_node *child =
	(struct bsg_node *)BU_PTBL_GET(&lod->children, active);
    if (!child)
	return 0;

    *source = child;
    if (bsg_node_bounds(child, bmin, bmax))
	return 1;

    if (bsg_node_bbox_cache_get(child, bmin, bmax) &&
	    isfinite(bmin[X]) && isfinite(bmin[Y]) && isfinite(bmin[Z]) &&
	    isfinite(bmax[X]) && isfinite(bmax[Y]) && isfinite(bmax[Z])) {
	return 1;
    }

    return 0;
}


static int
_bsg_scene_mesh_draw_bounds(struct bsg_node *node,
			    point_t bmin,
			    point_t bmax)
{
    if (!node || !bsg_shape_is_mesh(node))
	return 0;

    struct bsg_mesh_lod *mesh =
	bsg_payload_mesh_get(bsg_node_get_payload(node));
    if (mesh) {
	point_t obmin, obmax;
	VMOVE(obmin, mesh->bmin);
	VMOVE(obmax, mesh->bmax);

	/* Mesh LoD data is stored in non-instanced coordinates. */
	mat_t node_mat;
	MAT_IDN(node_mat);
	bsg_node_transform(node, node_mat);
	MAT4X3PNT(bmin, node_mat, obmin);
	MAT4X3PNT(bmax, node_mat, obmax);
	return 1;
    }

    return bsg_node_bounds(node, bmin, bmax);
}


static int
_bsg_scene_payload_line_draw_bounds(struct bsg_node *node,
				    point_t bmin,
				    point_t bmax)
{
    const struct bsg_payload_line_set *ls = _bsg_node_payload_line_set(node);

    return _bsg_payload_line_set_bounds(ls, bmin, bmax);
}


int
bsg_scene_node_update_bounds(struct bsg_node *sp, struct bsg_view *v)
{
    if (!sp)
	return 0;

    point_t calc_bmin;
    point_t calc_bmax;
    VSET(calc_bmin, INFINITY, INFINITY, INFINITY);
    VSET(calc_bmax, -INFINITY, -INFINITY, -INFINITY);

    if (_bsg_scene_field_draw_bounds(sp, calc_bmin, calc_bmax)) {
	_bsg_scene_cache_draw_bounds(sp, calc_bmin, calc_bmax);
	return 1;
    }

    struct bsg_node *source = sp;
    int have_bounds =
	_bsg_scene_lod_active_child_bounds(sp, v, &source, calc_bmin, calc_bmax);
    if (!have_bounds)
	have_bounds = _bsg_scene_mesh_draw_bounds(source, calc_bmin, calc_bmax);
    if (!have_bounds)
	have_bounds = _bsg_scene_payload_line_draw_bounds(source, calc_bmin, calc_bmax);
    if (!have_bounds)
	return 0;

    _bsg_scene_cache_draw_bounds(source, calc_bmin, calc_bmax);

    if (sp != source)
	_bsg_scene_cache_draw_bounds(sp, calc_bmin, calc_bmax);

    return 1;
}

static void
_bsg_view_depth_include_point(struct bsg_view *v,
			      int calc_mode,
			      const point_t pt,
			      double *calc_val,
			      int *have_val)
{
    vect_t vpt;

    if (!v || !calc_val || !have_val)
	return;

    MAT4X3PNT(vpt, v->gv_model2view, pt);
    if (calc_mode) {
	if (vpt[Z] > *calc_val) {
	    *calc_val = vpt[Z];
	    *have_val = 1;
	}
    } else {
	if (vpt[Z] < *calc_val) {
	    *calc_val = vpt[Z];
	    *have_val = 1;
	}
    }
}

static int
_bsg_geometry_fields_view_depth(struct bsg_node *s,
				struct bsg_view *v,
				int calc_mode,
				double *calc_val)
{
    if (!s || !v || !calc_val)
	return 0;

    bsg_field_ref coords = bsg_node_field_ref(s, BSG_FIELD_GEOMETRY_COORDINATES);
    size_t coord_count = bsg_field_multi_count(coords);
    if (!coord_count)
	return 0;

    int have_val = 0;
    switch (bsg_geometry_node_kind_get(s)) {
	case BSG_GEOMETRY_NODE_LINE_SET:
	case BSG_GEOMETRY_NODE_POINT_SET:
	    for (size_t i = 0; i < coord_count; i++) {
		point_t pt;
		if (bsg_field_multi_point_at(coords, i, pt))
		    _bsg_view_depth_include_point(v, calc_mode, pt, calc_val, &have_val);
	    }
	    return have_val;
	case BSG_GEOMETRY_NODE_INDEXED_LINE_SET:
	case BSG_GEOMETRY_NODE_INDEXED_FACE_SET: {
	    bsg_field_ref indices = bsg_node_field_ref(s, BSG_FIELD_GEOMETRY_INDICES);
	    size_t index_count = bsg_field_multi_count(indices);
	    for (size_t i = 0; i < index_count; i++) {
		int idx = -1;
		point_t pt;
		if (!bsg_field_multi_int_at(indices, i, &idx) || idx < 0)
		    continue;
		if ((size_t)idx >= coord_count)
		    continue;
		if (bsg_field_multi_point_at(coords, (size_t)idx, pt))
		    _bsg_view_depth_include_point(v, calc_mode, pt, calc_val, &have_val);
	    }
	    return have_val;
	}
	default:
	    return 0;
    }
}

fastf_t
bsg_scene_node_view_depth(struct bsg_node *s, struct bsg_view *v, int mode)
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
    const struct bsg_payload_line_set *ls = _bsg_node_payload_line_set(s);
    if (ls && ls->points && ls->point_cnt) {
	for (size_t i = 0; i < ls->point_cnt; i++) {
	    vect_t vpt;
	    MAT4X3PNT(vpt, v->gv_model2view, ls->points[i]);
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
	if (have_val)
	    return calc_val;
    }

    if (_bsg_geometry_fields_view_depth(s, v, calc_mode, &calc_val))
	vZ = calc_val;
    return vZ;
}


/* Internal DFS helper for bsg_scene_visit_db.
 * Traverses the BSG tree rooted at @p node (which is just a struct bsg_node
 * since bsg_node is a layout-compatible alias).  The callback is invoked for
 * every node classified as database-backed; returning 0 from the
 * callback stops traversal early. */
static int
_bsg_visit_db_internal(struct bsg_node *node,
		      int (*cb)(struct bsg_node *, void *),
		      void *data)
{
    if (!node)
	return 1;

    /* Call back for DB-derived shape leaves */
    if (bsg_node_is_database_source(node) &&
	    !bsg_node_type_is_a(node, bsg_view_scope_type())) {
	if (!cb(node, data))
	    return 0;
    }

    /* Recurse into children (groups and any other sub-nodes) */
    for (size_t i = 0; i < BU_PTBL_LEN(&node->children); i++) {
	struct bsg_node *child =
	    (struct bsg_node *)BU_PTBL_GET(&node->children, i);
	if (!_bsg_visit_db_internal(child, cb, data))
	    return 0;
    }

    return 1;
}


void
bsg_scene_visit_db(struct bsg_view *v,
		      int (*cb)(struct bsg_node *obj, void *data),
		      void *data)
{
    /* Iterate all database-derived shape nodes visible from @p v.  Retained
     * scenes are traversed depth-first; views without a retained scene use
     * their source tables.  Returning 0 from the callback stops traversal. */
    if (!v || !cb)
	return;

    if (bsg_view_scene_root(v)) {
	/* Traverse retained database-source records depth-first. */
	struct bsg_node *root = bsg_view_scene_root(v);
	if (bsg_view_is_independent(v)) {
	    struct bsg_node *scope = _bsg_independent_scope_find(root, v);
	    if (scope)
		_bsg_visit_db_internal(scope, cb, data);
	} else {
	    _bsg_visit_db_internal(root, cb, data);
	}
	return;
    }

    /* Fallback: non-GED / legacy consumers that store top-level objects in
     * scene-store DB records. */
    if (!bsg_scene_store_db_nodes_visit(v, BSG_SOURCE_DB, cb, data))
	return;
    if (!bsg_view_is_independent(v))
	bsg_scene_store_db_nodes_visit(v, BSG_SOURCE_DB | BSG_SOURCE_LOCAL, cb, data);
}


void
bsg_scene_node_sync(struct bsg_node *dest, struct bsg_node *src)
{
    if (!dest || !src)
	return;
    struct bsg_appearance_settings settings;
    bsg_appearance_settings_for_node(src, &settings);
    if (src->i && src->i->appearance.valid) {
	bsg_node_internal_ensure(dest)->appearance = src->i->appearance;
	dest->i->appearance.settings = settings;
    }
    bsg_appearance_apply_settings(dest, &settings);
    vect_t draw_center = VINIT_ZERO;
    bsg_node_get_draw_center(src, draw_center);
    bsg_node_set_draw_center(dest, draw_center);
    bsg_node_set_draw_size(dest, bsg_node_draw_size(src));
    bsg_node_set_view(dest, bsg_node_get_view(src));
    bsg_shape_set_non_database_source(dest, bsg_shape_is_non_database_source(src));
    if (src->i) {
	bsg_node_internal_ensure(dest)->source_realization = src->i->source_realization;
	bsg_node_internal_ensure(dest)->source_flags = src->i->source_flags;
	bsg_node_internal_ensure(dest)->source_flags_valid = src->i->source_flags_valid;
	bsg_node_internal_ensure(dest)->geometry_roles = src->i->geometry_roles;
	bsg_node_internal_ensure(dest)->geometry_roles_valid = src->i->geometry_roles_valid;
	bsg_node_internal_ensure(dest)->payload_flags = src->i->payload_flags;
	bsg_node_internal_ensure(dest)->payload_flags_valid = src->i->payload_flags_valid;
	bsg_node_internal_ensure(dest)->non_database_source = src->i->non_database_source;
	bsg_node_internal_ensure(dest)->non_database_source_valid = src->i->non_database_source_valid;
	bsg_node_internal_ensure(dest)->arrows_enabled = src->i->arrows_enabled;
	bsg_node_internal_ensure(dest)->bbox_cache = src->i->bbox_cache;
    }
}

//#define USE_BSG_LOG
void
#ifdef USE_BSG_LOG
bsg_log(int level, const char *fmt, ...)
#else
bsg_log(int UNUSED(level), const char *UNUSED(fmt), ...)
#endif
{
#ifdef USE_BSG_LOG
    if (level < 0 || !fmt)
	return;
    const char *brsig = getenv("BSG_LOG");
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
bsg_view_print(const char *title, struct bsg_view *v, int UNUSED(verbosity))
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

void
bsg_view_init(struct bsg_view *v, struct bsg_view_set *s)
{
    bsg_init(v, s);
}


void
bsg_view_free(struct bsg_view *v)
{
    bsg_free(v);
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
