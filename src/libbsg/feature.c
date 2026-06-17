/*                    F E A T U R E . C
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
/** @file libbsg/feature.c
 *
 * Typed view-feature facade over the BSG view-scope storage tree.
 */

#include "common.h"

#include <string.h>

#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "vmath.h"

#include "bsg/appearance.h"
#include "bsg/faceplate.h"
#include "bsg/feature.h"
#include "bsg/field.h"
#include "bsg/material.h"
#include "bsg/node.h"
#include "bsg/object.h"
#include "bsg/overlay.h"
#include "payload_typed_private.h"
#include "bsg/scene_object.h"
#include "bsg/tcl_data.h"
#include "bsg/util.h"
#include "bsg_private.h"
#include "appearance_private.h"
#include "draw_source_private.h"
#include "feature_private.h"
#include "field_private.h"
#include "geometry_private.h"
#include "material_private.h"
#include "node_private.h"
#include "object_private.h"
#include "node_storage_private.h"
#include "overlay_private.h"
#include "payload_private.h"
#include "scene_object_private.h"

#define BSG_INDEPENDENT_SCOPE_NAME "_independent_db_scope"


static bsg_feature_ref
_feature_ref(struct bsg_node *node)
{
    bsg_feature_ref ref = BSG_FEATURE_REF_NULL_INIT;
    if (!node)
	return ref;
    ref.token = (uintptr_t)node;
    ref.revision = bsg_node_revision(node);
    return ref;
}


struct bsg_node *
bsg_feature_node(bsg_feature_ref ref)
{
    return (ref.token) ? (struct bsg_node *)ref.token : NULL;
}


static enum bsg_feature_scope
_feature_scope(struct bsg_node *node)
{
    if (!node)
	return BSG_FEATURE_SCOPE_SHARED;
    if (bsg_node_is_local_source(node))
	return BSG_FEATURE_SCOPE_LOCAL;
    if (node->parent && bsg_node_get_view(node->parent))
	return BSG_FEATURE_SCOPE_LOCAL;
    return BSG_FEATURE_SCOPE_SHARED;
}


static void
bsg_feature_meta_clear(struct bsg_feature_meta *meta)
{
    if (!meta)
	return;
    if (meta->display_name)
	bu_free(meta->display_name, "bsg feature display name");
    if (meta->owner)
	bu_free(meta->owner, "bsg feature owner");
    bu_free(meta, "bsg feature metadata");
}


static struct bsg_feature_meta *
_feature_meta(struct bsg_node *node)
{
    return (node && node->i) ? node->i->feature_meta : NULL;
}


static const struct bsg_feature_meta *
_feature_meta_const(const struct bsg_node *node)
{
    return (node && node->i) ? node->i->feature_meta : NULL;
}


void
bsg_feature_node_meta_clear(struct bsg_node *node)
{
    if (!node || !node->i)
	return;
    bsg_feature_meta_clear(node->i->feature_meta);
    node->i->feature_meta = NULL;
}


static struct bsg_feature_meta *
_feature_meta_ensure(struct bsg_node *node)
{
    if (!node)
	return NULL;
    struct bsg_feature_meta *meta = _feature_meta(node);
    if (meta)
	return meta;
    meta = (struct bsg_feature_meta *)bu_calloc(1, sizeof(struct bsg_feature_meta), "bsg feature metadata");
    bsg_node_internal_ensure(node)->feature_meta = meta;
    return meta;
}


static void
_feature_meta_set_string(char **dst, const char *src, const char *what)
{
    if (*dst) {
	bu_free(*dst, what);
	*dst = NULL;
    }
    if (src && strlen(src))
	*dst = bu_strdup(src);
}


static void
_feature_tag(struct bsg_node *node, const struct bsg_feature_opts *opts)
{
    if (!node)
	return;
    struct bsg_feature_meta *meta = _feature_meta_ensure(node);
    if (!meta)
	return;

    enum bsg_feature_family family = BSG_FEATURE_UNKNOWN;
    if (opts)
	family = opts->family;
    if (family == BSG_FEATURE_UNKNOWN)
	family = bsg_feature_family(_feature_ref(node));
    meta->family = (int)family;

    if (opts && opts->display_name) {
	_feature_meta_set_string(&meta->display_name, opts->display_name, "bsg feature display name");
    }
    if (opts && opts->owner) {
	_feature_meta_set_string(&meta->owner, opts->owner, "bsg feature owner");
    }
}


static enum bsg_feature_family
_feature_family_from_node(struct bsg_node *node)
{
    if (!node)
	return BSG_FEATURE_UNKNOWN;
    const struct bsg_feature_meta *meta = _feature_meta_const(node);
    if (meta && meta->family)
	return (enum bsg_feature_family)meta->family;
    if (bsg_node_has_geometry_role(node, BSG_GEOMETRY_AXES_WIDGET))
	return BSG_FEATURE_AXES;
    if (bsg_node_has_geometry_role(node, BSG_GEOMETRY_TEXT_LABELS))
	return BSG_FEATURE_LABEL;
    if (bsg_shape_arrows_enabled(node))
	return BSG_FEATURE_ARROW;
    return BSG_FEATURE_OVERLAY;
}


enum bsg_feature_family
bsg_feature_node_family(const struct bsg_node *node)
{
    return _feature_family_from_node((struct bsg_node *)node);
}


static int
_feature_is_view_scope(struct bsg_node *scope)
{
    return bsg_node_type_is_a(scope, bsg_view_scope_type());
}


static int
_feature_scope_is_independent(struct bsg_node *scope, const struct bsg_view *owner)
{
    if (!scope)
	return 0;
    if (!_feature_is_view_scope(scope))
	return 0;
    if (bsg_node_get_view(scope) != owner)
	return 0;
    return BU_STR_EQUAL(bsg_node_name(scope), BSG_INDEPENDENT_SCOPE_NAME);
}


static int
_feature_scope_visible(struct bsg_node *scope, struct bsg_view *v)
{
    if (!_feature_is_view_scope(scope))
	return 0;
    if (!bsg_node_get_view(scope))
	return 1;
    return (bsg_node_get_view(scope) == v) ? 1 : 0;
}


static struct bsg_node *
_feature_scope_find(struct bsg_node *root, struct bsg_view *owner)
{
    if (!root)
	return NULL;

    for (size_t i = 0; i < BU_PTBL_LEN(&root->children); i++) {
	struct bsg_node *scope = (struct bsg_node *)BU_PTBL_GET(&root->children, i);
	if (!_feature_is_view_scope(scope))
	    continue;
	if (_feature_scope_is_independent(scope, owner))
	    continue;
	if (bsg_node_get_view(scope) == owner)
	    return scope;
    }

    return NULL;
}


static struct bsg_node *
_feature_scope_ensure(struct bsg_view *v, int local)
{
    if (!v || !bsg_view_scene_root(v))
	return NULL;

    struct bsg_node *root = bsg_view_scene_root(v);
    struct bsg_view *owner = local ? v : NULL;
    struct bsg_node *scope = _feature_scope_find(root, owner);
    if (scope)
	return scope;

    scope = bsg_scene_node_create_detached(v, BSG_SOURCE_CHILD | (local ? BSG_SOURCE_LOCAL : 0));
    if (!scope)
	return NULL;
    bsg_node_set_object_type(scope, bsg_view_scope_type());
    bsg_node_set_view(scope, owner);
    bsg_node_set_visible_state(scope, 1);
    bsg_node_set_name(scope, local ? "_view_obj_scope_local" : "_view_obj_scope_shared");
    bsg_node_add_child(root, scope);
    bsg_node_touch(scope);
    return scope;
}


static unsigned long long
_feature_shape_flags(enum bsg_feature_family family)
{
    switch (family) {
	case BSG_FEATURE_AXES:
	    return BSG_GEOMETRY_AXES_WIDGET;
	case BSG_FEATURE_LABEL:
	case BSG_FEATURE_LABEL_LEADER:
	case BSG_FEATURE_TEXT:
	case BSG_FEATURE_ANNOTATION:
	    return BSG_GEOMETRY_TEXT_LABELS;
	case BSG_FEATURE_POLYGON:
	case BSG_FEATURE_SKETCH:
	case BSG_FEATURE_FACE_SET:
	    return BSG_GEOMETRY_VIEW_OVERLAY;
	default:
	    return 0;
    }
}

static int
_feature_family_uses_line_geometry_node(enum bsg_feature_family family)
{
    switch (family) {
	case BSG_FEATURE_LINES:
	case BSG_FEATURE_ARROW:
	case BSG_FEATURE_POLYGON:
	case BSG_FEATURE_SKETCH:
	case BSG_FEATURE_SNAP_HINT:
	case BSG_FEATURE_EDIT_HANDLE:
	case BSG_FEATURE_TRANSIENT_PREVIEW:
	case BSG_FEATURE_MEASUREMENT:
	case BSG_FEATURE_HUD:
	case BSG_FEATURE_FACEPLATE:
	case BSG_FEATURE_OVERLAY:
	    return 1;
	default:
	    return 0;
    }
}

static int
_feature_line_geometry_command_valid(int command)
{
    switch (command) {
	case BSG_GEOMETRY_LINE_MOVE:
	case BSG_GEOMETRY_LINE_DRAW:
	case BSG_GEOMETRY_POINT_DRAW:
	    return 1;
	default:
	    return 0;
    }
}

static void
_feature_mark_view_object(struct bsg_node *node)
{
    if (!node)
	return;

    bsg_shape_set_non_database_source(node, 1);
    bsg_node_set_payload_type(node,
	    bsg_node_get_payload_type(node) | BSG_PAYLOAD_OVERLAY);
}

static int
_feature_line_geometry_from_points(struct bsg_node *node, const point_t *points, const int *cmds, size_t point_count)
{
    point_t *line_points = NULL;
    int *line_cmds = NULL;
    size_t line_count = 0;
    int ret;

    if (!node)
	return 0;

    if (point_count) {
	line_points = (point_t *)bu_calloc(point_count, sizeof(point_t), "bsg feature line geometry points");
	line_cmds = (int *)bu_calloc(point_count, sizeof(int), "bsg feature line geometry commands");
    }

    for (size_t i = 0; i < point_count; i++) {
	int line_cmd = (i % 2) ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE;
	if (cmds) {
	    if (!_feature_line_geometry_command_valid(cmds[i]))
		continue;
	    line_cmd = cmds[i];
	}
	if (points)
	    VMOVE(line_points[line_count], points[i]);
	line_cmds[line_count] = line_cmd;
	line_count++;
    }

    if (bsg_payload_edit_preview_get_data(bsg_node_get_payload(node)))
	ret = bsg_geometry_node_set_line_set_fields(node,
		(const point_t *)line_points, line_cmds, line_count);
    else
	ret = bsg_geometry_node_set_line_set(node,
		(const point_t *)line_points, line_cmds, line_count);

    if (line_points)
	bu_free(line_points, "bsg feature line geometry points");
    if (line_cmds)
	bu_free(line_cmds, "bsg feature line geometry commands");

    if (ret)
	_feature_mark_view_object(node);

    return ret;
}

static int
_feature_line_geometry_points_copy(struct bsg_node *node,
				   point_t **points_out,
				   int **cmds_out,
				   size_t *point_count_out)
{
    if (!node || bsg_geometry_node_kind_get(node) != BSG_GEOMETRY_NODE_LINE_SET)
	return 0;

    bsg_node_ref node_ref = bsg_node_ref_from_object(bsg_object_ref_from_node(node));
    bsg_line_set_ref lines = bsg_node_ref_as_line_set(node_ref);
    if (bsg_node_ref_is_null(bsg_line_set_ref_as_node(lines)))
	return 0;

    size_t cnt = bsg_line_set_ref_point_count(lines);
    if (point_count_out)
	*point_count_out = cnt;
    if (!cnt)
	return 1;

    point_t *pts = NULL;
    int *cmds = NULL;
    if (points_out)
	pts = (point_t *)bu_calloc(cnt, sizeof(point_t), "bsg feature geometry points copy");
    if (cmds_out)
	cmds = (int *)bu_calloc(cnt, sizeof(int), "bsg feature geometry cmds copy");

    for (size_t i = 0; i < cnt; i++) {
	if (pts && !bsg_line_set_ref_point_at(lines, i, pts[i]))
	    goto fail;
	if (cmds && !bsg_line_set_ref_command_at(lines, i, &cmds[i]))
	    goto fail;
    }

    if (points_out)
	*points_out = pts;
    if (cmds_out)
	*cmds_out = cmds;
    return 1;

fail:
    if (pts)
	bu_free(pts, "bsg feature geometry points copy");
    if (cmds)
	bu_free(cmds, "bsg feature geometry cmds copy");
    if (point_count_out)
	*point_count_out = 0;
    return 0;
}


static size_t
_feature_line_geometry_command_count(struct bsg_node *node)
{
    if (!node || bsg_geometry_node_kind_get(node) != BSG_GEOMETRY_NODE_LINE_SET)
	return 0;

    bsg_node_ref node_ref = bsg_node_ref_from_object(bsg_object_ref_from_node(node));
    bsg_line_set_ref lines = bsg_node_ref_as_line_set(node_ref);
    if (bsg_node_ref_is_null(bsg_line_set_ref_as_node(lines)))
	return 0;

    return bsg_line_set_ref_point_count(lines);
}

static size_t
_feature_line_geometry_tree_command_count(struct bsg_node *node)
{
    if (!node)
	return 0;

    size_t count = _feature_line_geometry_command_count(node);
    for (size_t i = 0; i < BU_PTBL_LEN(&node->children); i++) {
	struct bsg_node *child = (struct bsg_node *)BU_PTBL_GET(&node->children, i);
	count += _feature_line_geometry_tree_command_count(child);
    }
    return count;
}


static void
_feature_attach_default_payload(struct bsg_node *node, enum bsg_feature_family family)
{
    if (!node)
	return;

    switch (family) {
	case BSG_FEATURE_AXES: {
	    struct bsg_axes *axes;
	    BU_GET(axes, struct bsg_axes);
	    memset(axes, 0, sizeof(*axes));
	    (void)bsg_geometry_node_set_axes_overlay(node, axes);
	    BU_PUT(axes, struct bsg_axes);
	    break;
	}
	case BSG_FEATURE_GRID: {
	    struct bsg_grid_state grid;
	    memset(&grid, 0, sizeof(grid));
	    (void)bsg_geometry_node_set_grid_overlay(node, &grid);
	    break;
	}
	case BSG_FEATURE_FACE_SET:
	    (void)bsg_geometry_node_set_indexed_face_set(node, NULL, 0, NULL, 0, NULL, 0);
	    break;
	case BSG_FEATURE_LINES:
	case BSG_FEATURE_ARROW:
	case BSG_FEATURE_POLYGON:
	case BSG_FEATURE_SKETCH:
	case BSG_FEATURE_MEASUREMENT:
	case BSG_FEATURE_SNAP_HINT:
	case BSG_FEATURE_EDIT_HANDLE:
	case BSG_FEATURE_TRANSIENT_PREVIEW:
	case BSG_FEATURE_HUD:
	case BSG_FEATURE_FACEPLATE:
	case BSG_FEATURE_OVERLAY:
	    (void)bsg_geometry_node_set_line_set(node, NULL, NULL, 0);
	    break;
	case BSG_FEATURE_UNKNOWN:
	default:
	    (void)bsg_geometry_node_set_line_set(node, NULL, NULL, 0);
	    break;
    }
}


const char *
bsg_feature_family_name(enum bsg_feature_family family)
{
    switch (family) {
	case BSG_FEATURE_ANNOTATION: return "annotation";
	case BSG_FEATURE_TEXT: return "text";
	case BSG_FEATURE_LABEL: return "label";
	case BSG_FEATURE_LABEL_LEADER: return "label_leader";
	case BSG_FEATURE_AXES: return "axes";
	case BSG_FEATURE_GRID: return "grid";
	case BSG_FEATURE_LINES: return "lines";
	case BSG_FEATURE_ARROW: return "arrow";
	case BSG_FEATURE_POLYGON: return "polygon";
	case BSG_FEATURE_FACE_SET: return "face_set";
	case BSG_FEATURE_SKETCH: return "sketch";
	case BSG_FEATURE_MEASUREMENT: return "measurement";
	case BSG_FEATURE_SNAP_HINT: return "snap_hint";
	case BSG_FEATURE_EDIT_HANDLE: return "edit_handle";
	case BSG_FEATURE_TRANSIENT_PREVIEW: return "transient_preview";
	case BSG_FEATURE_HUD: return "hud";
	case BSG_FEATURE_FACEPLATE: return "faceplate";
	case BSG_FEATURE_OVERLAY: return "overlay";
	case BSG_FEATURE_UNKNOWN:
	default: return "unknown";
    }
}


enum bsg_feature_family
bsg_feature_family(bsg_feature_ref ref)
{
    return _feature_family_from_node(bsg_feature_node(ref));
}


int
bsg_feature_ref_is_null(bsg_feature_ref ref)
{
    return ref.token ? 0 : 1;
}


static struct bsg_node *
_feature_create_node(struct bsg_view *v, const char *name, const struct bsg_feature_opts *opts)
{
    enum bsg_feature_family family = opts ? opts->family : BSG_FEATURE_OVERLAY;
    int local = opts ? opts->local : 0;
    struct bsg_node *scope = _feature_scope_ensure(v, local);
    if (!scope)
	return NULL;

    struct bsg_node *node = bsg_scene_node_create_detached(v, BSG_SOURCE_VIEW | (local ? BSG_SOURCE_LOCAL : 0));
    if (!node)
	return NULL;

    bsg_node_add_child(scope, node);
    unsigned int geometry_roles = BSG_GEOMETRY_VIEW_OVERLAY | _feature_shape_flags(family);
    bsg_node_set_object_type(node, bsg_geometry_type());
    bsg_node_add_geometry_roles(node, geometry_roles);
    bsg_node_set_view(node, v);
    if (name && strlen(name))
	bsg_node_set_name(node, name);
    bsg_node_set_visible_state(node, 1);
    _feature_attach_default_payload(node, family);
    _feature_mark_view_object(node);
    bsg_node_touch(node);
    return node;
}


static int
_clamp_byte(int v)
{
    if (v < 0)
	return 0;
    if (v > 255)
	return 255;
    return v;
}


bsg_feature_ref
bsg_feature_create(struct bsg_view *v, const char *name, const struct bsg_feature_opts *opts)
{
    struct bsg_feature_opts local_opts = BSG_FEATURE_OPTS_INIT;
    if (opts)
	local_opts = *opts;
    if (local_opts.family == BSG_FEATURE_UNKNOWN)
	local_opts.family = BSG_FEATURE_OVERLAY;
    if (local_opts.family == BSG_FEATURE_ARROW)
	local_opts.arrow = 1;

    struct bsg_node *node = _feature_create_node(v, name, &local_opts);
    if (!node)
	return _feature_ref(NULL);

    if (local_opts.arrow)
	bsg_shape_set_arrows_enabled(node, 1);
    _feature_tag(node, &local_opts);

    if (local_opts.color_valid)
	bsg_feature_set_color(_feature_ref(node), local_opts.color[0], local_opts.color[1], local_opts.color[2]);
    if (local_opts.line_width >= 0)
	bsg_feature_set_line_width(_feature_ref(node), local_opts.line_width);
    if (local_opts.visible >= 0)
	bsg_feature_set_visible(_feature_ref(node), local_opts.visible);

    return _feature_ref(node);
}


bsg_feature_ref
bsg_feature_create_axes(struct bsg_view *v, const char *name, int local)
{
    struct bsg_feature_opts opts = BSG_FEATURE_OPTS_INIT;
    opts.family = BSG_FEATURE_AXES;
    opts.local = local;
    return bsg_feature_create(v, name, &opts);
}


bsg_feature_ref
bsg_feature_create_lines(struct bsg_view *v, const char *name, int local)
{
    struct bsg_feature_opts opts = BSG_FEATURE_OPTS_INIT;
    opts.family = BSG_FEATURE_LINES;
    opts.local = local;
    return bsg_feature_create(v, name, &opts);
}


bsg_feature_ref
bsg_feature_create_label(struct bsg_view *v, const char *name, int local)
{
    struct bsg_feature_opts opts = BSG_FEATURE_OPTS_INIT;
    opts.family = BSG_FEATURE_LABEL;
    opts.local = local;
    return bsg_feature_create(v, name, &opts);
}


bsg_feature_ref
bsg_feature_create_arrow(struct bsg_view *v, const char *name, int local)
{
    struct bsg_feature_opts opts = BSG_FEATURE_OPTS_INIT;
    opts.family = BSG_FEATURE_ARROW;
    opts.local = local;
    opts.arrow = 1;
    return bsg_feature_create(v, name, &opts);
}


bsg_feature_ref
bsg_feature_create_overlay(struct bsg_view *v, const char *name, int local)
{
    struct bsg_feature_opts opts = BSG_FEATURE_OPTS_INIT;
    opts.family = BSG_FEATURE_OVERLAY;
    opts.local = local;
    return bsg_feature_create(v, name, &opts);
}


bsg_feature_ref
bsg_feature_create_polygon(struct bsg_view *v, const char *name, int local)
{
    struct bsg_feature_opts opts = BSG_FEATURE_OPTS_INIT;
    opts.family = BSG_FEATURE_POLYGON;
    opts.local = local;
    return bsg_feature_create(v, name, &opts);
}


bsg_feature_ref
bsg_feature_create_face_set(struct bsg_view *v, const char *name, int local)
{
    struct bsg_feature_opts opts = BSG_FEATURE_OPTS_INIT;
    opts.family = BSG_FEATURE_FACE_SET;
    opts.local = local;
    return bsg_feature_create(v, name, &opts);
}


bsg_feature_ref
bsg_feature_create_preview(struct bsg_view *v, const char *name, int local)
{
    struct bsg_feature_opts opts = BSG_FEATURE_OPTS_INIT;
    opts.family = BSG_FEATURE_TRANSIENT_PREVIEW;
    opts.local = local;
    return bsg_feature_create(v, name, &opts);
}


bsg_feature_ref
bsg_feature_find(struct bsg_view *v, const char *name)
{
    if (!v || !name || !strlen(name) || !bsg_view_scene_root(v))
	return _feature_ref(NULL);

    struct bsg_node *root = bsg_view_scene_root(v);
    for (size_t i = 0; i < BU_PTBL_LEN(&root->children); i++) {
	struct bsg_node *scope = (struct bsg_node *)BU_PTBL_GET(&root->children, i);
	if (!_feature_is_view_scope(scope))
	    continue;
	if (_feature_scope_is_independent(scope, v))
	    continue;
	if (!_feature_scope_visible(scope, v))
	    continue;
	for (size_t j = 0; j < BU_PTBL_LEN(&scope->children); j++) {
	    struct bsg_node *obj = (struct bsg_node *)BU_PTBL_GET(&scope->children, j);
	    if (obj && BU_STR_EQUAL(name, bsg_node_name(obj)))
		return _feature_ref(obj);
	}
    }

    return _feature_ref(NULL);
}


bsg_feature_ref
bsg_feature_ref_from_scene(bsg_scene_ref ref)
{
    struct bsg_node *node = bsg_object_ref_node(bsg_object_ref_from_scene_ref(ref));
    if (!node || !_feature_meta(node))
	return _feature_ref(NULL);
    return _feature_ref(node);
}


bsg_scene_ref
bsg_feature_ref_as_scene(bsg_feature_ref ref)
{
    bsg_scene_ref scene = BSG_SCENE_REF_NULL_INIT;
    struct bsg_node *node = bsg_feature_node(ref);
    if (node)
	scene = bsg_scene_ref_from_object_ref(bsg_object_ref_from_node(node));
    return scene;
}


int
bsg_feature_remove(struct bsg_view *v, const char *name)
{
    if (!v || !name || !strlen(name) || !bsg_view_scene_root(v))
	return 0;

    struct bsg_node *root = bsg_view_scene_root(v);
    for (size_t i = 0; i < BU_PTBL_LEN(&root->children); i++) {
	struct bsg_node *scope = (struct bsg_node *)BU_PTBL_GET(&root->children, i);
	if (!_feature_is_view_scope(scope))
	    continue;
	if (_feature_scope_is_independent(scope, v))
	    continue;
	if (!_feature_scope_visible(scope, v))
	    continue;
	size_t j = BU_PTBL_LEN(&scope->children);
	while (j > 0) {
	    j--;
	    struct bsg_node *obj = (struct bsg_node *)BU_PTBL_GET(&scope->children, j);
	    if (!obj || !BU_STR_EQUAL(name, bsg_node_name(obj)))
		continue;
	    bu_ptbl_rm(&scope->children, (long *)obj);
	    bsg_scene_node_release(obj);
	    bsg_node_touch(scope);
	    if (!BU_PTBL_LEN(&scope->children)) {
		bu_ptbl_rm(&root->children, (long *)scope);
		bsg_scene_node_release(scope);
	    }
	    bsg_node_touch(root);
	    return 1;
	}
    }

    return 0;
}


size_t
bsg_feature_remove_all(struct bsg_view *v, int scope_mask)
{
    if (!v || !bsg_view_scene_root(v))
	return 0;
    if (!scope_mask)
	scope_mask = BSG_FEATURE_SCOPE_ALL;

    struct bsg_node *root = bsg_view_scene_root(v);
    size_t removed = 0;
    size_t i = BU_PTBL_LEN(&root->children);
    while (i > 0) {
	i--;
	struct bsg_node *scope = (struct bsg_node *)BU_PTBL_GET(&root->children, i);
	if (!_feature_is_view_scope(scope))
	    continue;
	if (_feature_scope_is_independent(scope, v))
	    continue;
	int is_local = bsg_node_get_view(scope) ? 1 : 0;
	if (is_local && !(scope_mask & BSG_FEATURE_SCOPE_LOCAL))
	    continue;
	if (!is_local && !(scope_mask & BSG_FEATURE_SCOPE_SHARED))
	    continue;
	if (!_feature_scope_visible(scope, v))
	    continue;
	size_t j = BU_PTBL_LEN(&scope->children);
	while (j > 0) {
	    j--;
	    struct bsg_node *obj = (struct bsg_node *)BU_PTBL_GET(&scope->children, j);
	    if (!obj)
		continue;
	    bu_ptbl_rm(&scope->children, (long *)obj);
	    bsg_scene_node_release(obj);
	    removed++;
	}
	if (!BU_PTBL_LEN(&scope->children)) {
	    bu_ptbl_rm(&root->children, (long *)scope);
	    bsg_scene_node_release(scope);
	}
    }
    if (removed)
	bsg_node_touch(root);
    return removed;
}


struct feature_visit_state {
    bsg_feature_visit_cb cb;
    void *data;
};


static int
_feature_visit_cb(struct bsg_node *node, void *data)
{
    struct feature_visit_state *state = (struct feature_visit_state *)data;
    struct bsg_feature_record record;
    bsg_feature_ref ref = _feature_ref(node);
    if (!state || !state->cb)
	return 0;
    if (!bsg_feature_record_get(ref, &record))
	return 1;
    return state->cb(ref, &record, state->data);
}


void
bsg_feature_visit(struct bsg_view *v, int scope_mask, bsg_feature_visit_cb cb, void *data)
{
    if (!cb)
	return;
    if (!v || !bsg_view_scene_root(v))
	return;
    if (!scope_mask)
	scope_mask = BSG_FEATURE_SCOPE_ALL;

    struct feature_visit_state state;
    state.cb = cb;
    state.data = data;
    bsg_feature_visit_nodes(v, scope_mask, _feature_visit_cb, &state);
}


void
bsg_feature_visit_nodes(struct bsg_view *v, int scope_mask, bsg_feature_node_visit_cb cb, void *data)
{
    if (!v || !cb || !bsg_view_scene_root(v))
	return;
    if (!scope_mask)
	scope_mask = BSG_FEATURE_SCOPE_ALL;

    struct bsg_node *root = bsg_view_scene_root(v);
    for (size_t i = 0; i < BU_PTBL_LEN(&root->children); i++) {
	struct bsg_node *scope = (struct bsg_node *)BU_PTBL_GET(&root->children, i);
	if (!_feature_is_view_scope(scope))
	    continue;
	if (_feature_scope_is_independent(scope, v))
	    continue;
	int is_local = bsg_node_get_view(scope) ? 1 : 0;
	if (is_local && !(scope_mask & BSG_FEATURE_SCOPE_LOCAL))
	    continue;
	if (!is_local && !(scope_mask & BSG_FEATURE_SCOPE_SHARED))
	    continue;
	if (!_feature_scope_visible(scope, v))
	    continue;
	for (size_t j = 0; j < BU_PTBL_LEN(&scope->children); j++) {
	    struct bsg_node *obj = (struct bsg_node *)BU_PTBL_GET(&scope->children, j);
	    if (obj && !cb(obj, data))
		return;
	}
    }
}


int
bsg_feature_record_get(bsg_feature_ref ref, struct bsg_feature_record *record)
{
    if (!record)
	return 0;
    memset(record, 0, sizeof(*record));

    struct bsg_node *node = bsg_feature_node(ref);
    if (!node)
	return 0;

    record->ref = _feature_ref(node);
    record->family = _feature_family_from_node(node);
    record->scope = _feature_scope(node);
    record->name = bsg_node_name(node);
    const struct bsg_feature_meta *meta = _feature_meta_const(node);
    record->display_name = (meta && meta->display_name) ? meta->display_name : record->name;
    record->owner = meta ? meta->owner : NULL;
    record->visible = bsg_appearance_visible(node) || bsg_appearance_force_draw(node);
    bsg_material_get_rgb(node, &record->color[0], &record->color[1], &record->color[2]);
    record->line_width = bsg_appearance_line_width(node);
    record->child_count = bsg_node_child_count(node);
    record->geometry_command_count = _feature_line_geometry_tree_command_count(node);
    if (!record->geometry_command_count) {
	struct bsg_payload_line_set *ls = bsg_payload_line_set_get(bsg_node_get_payload(node));
	if (ls)
	    record->geometry_command_count = ls->point_cnt;
    }
    return 1;
}


bsg_feature_ref
bsg_feature_ref_from_node(struct bsg_node *node, enum bsg_feature_family fallback_family)
{
    if (!node)
	return _feature_ref(NULL);
    const struct bsg_feature_meta *meta = _feature_meta_const(node);
    if (!meta || !meta->family) {
	struct bsg_feature_opts opts = BSG_FEATURE_OPTS_INIT;
	opts.family = fallback_family ? fallback_family : _feature_family_from_node(node);
	_feature_tag(node, &opts);
    }
    return _feature_ref(node);
}


void
bsg_feature_set_family(bsg_feature_ref ref, enum bsg_feature_family family)
{
    struct bsg_node *node = bsg_feature_node(ref);
    if (!node)
	return;
    struct bsg_feature_opts opts = BSG_FEATURE_OPTS_INIT;
    opts.family = family;
    _feature_tag(node, &opts);
    bsg_node_touch(node);
    bsg_scene_node_invalidate(node);
}


int
bsg_feature_style_get(bsg_feature_ref ref, struct bsg_feature_style *style)
{
    if (!style)
	return 0;
    struct bsg_node *node = bsg_feature_node(ref);
    if (!node)
	return 0;

    struct bsg_feature_style local = BSG_FEATURE_STYLE_INIT;
    local.visible = bsg_node_visible(node);
    local.color_valid = 1;
    bsg_material_get_rgb(node, &local.color[0], &local.color[1], &local.color[2]);
    local.line_width = bsg_appearance_line_width(node);
    local.line_style = bsg_appearance_line_style(node);
    local.arrow = bsg_shape_arrows_enabled(node);
    local.arrow_tip_length = bsg_shape_arrow_tip_length(node);
    local.arrow_tip_width = bsg_shape_arrow_tip_width(node);
    *style = local;
    return 1;
}


int
bsg_feature_style_apply(bsg_feature_ref ref, const struct bsg_feature_style *style)
{
    if (!style)
	return 0;
    struct bsg_node *node = bsg_feature_node(ref);
    if (!node)
	return 0;

    if (style->color_valid)
	bsg_feature_set_color(ref, style->color[0], style->color[1], style->color[2]);
    if (style->line_width >= 0)
	bsg_feature_set_line_width(ref, style->line_width);
    if (style->line_style >= 0)
	bsg_appearance_set_line_style(node, style->line_style);
    if (style->visible >= 0)
	bsg_feature_set_visible(ref, style->visible);
    if (style->arrow >= 0)
	bsg_shape_set_arrows_enabled(node, style->arrow);
    if (style->arrow_tip_length >= 0.0)
	bsg_shape_set_arrow_tip_length(node, style->arrow_tip_length);
    if (style->arrow_tip_width >= 0.0)
	bsg_shape_set_arrow_tip_width(node, style->arrow_tip_width);

    bsg_node_touch(node);
    bsg_scene_node_invalidate(node);
    return 1;
}


static int
_feature_style_apply_recursive_node(struct bsg_node *node, const struct bsg_feature_style *style)
{
    if (!node || !style)
	return 0;

    int changed = bsg_feature_style_apply(_feature_ref(node), style);
    for (size_t i = 0; i < BU_PTBL_LEN(&node->children); i++) {
	struct bsg_node *child = (struct bsg_node *)BU_PTBL_GET(&node->children, i);
	if (_feature_style_apply_recursive_node(child, style))
	    changed = 1;
    }
    return changed;
}


int
bsg_feature_style_apply_recursive(bsg_feature_ref ref, const struct bsg_feature_style *style)
{
    return _feature_style_apply_recursive_node(bsg_feature_node(ref), style);
}


void
bsg_feature_set_color(bsg_feature_ref ref, int r, int g, int b)
{
    struct bsg_node *node = bsg_feature_node(ref);
    if (!node)
	return;
    bsg_material_set_rgb(node, (unsigned char)_clamp_byte(r), (unsigned char)_clamp_byte(g), (unsigned char)_clamp_byte(b));
    bsg_node_touch(node);
    bsg_scene_node_invalidate(node);
}


void
bsg_feature_set_line_width(bsg_feature_ref ref, int line_width)
{
    struct bsg_node *node = bsg_feature_node(ref);
    if (!node)
	return;
    if (line_width < 0)
	line_width = 0;
    bsg_appearance_set_line_width(node, line_width);
    bsg_node_touch(node);
    bsg_scene_node_invalidate(node);
}


void
bsg_feature_set_visible(bsg_feature_ref ref, int visible)
{
    struct bsg_node *node = bsg_feature_node(ref);
    if (!node)
	return;
    bsg_appearance_set_visible(node, visible);
    bsg_node_touch(node);
    bsg_scene_node_invalidate(node);
}

int
bsg_feature_touch(bsg_feature_ref ref)
{
    struct bsg_node *node = bsg_feature_node(ref);
    if (!node)
	return 0;
    bsg_node_touch(node);
    bsg_scene_node_invalidate(node);
    return 1;
}


static int
_feature_realize_node(struct bsg_node *node, struct bsg_view *v, int recursive)
{
    if (!node)
	return 0;

    int changed = 0;
    if (recursive) {
	for (size_t i = 0; i < BU_PTBL_LEN(&node->children); i++) {
	    struct bsg_node *child = (struct bsg_node *)BU_PTBL_GET(&node->children, i);
	    if (_feature_realize_node(child, v, recursive))
		changed = 1;
	}
    }

    bsg_scene_ref ref = BSG_SCENE_REF_NULL_INIT;
    ref.opaque = node;
    bsg_appearance_set_changed(node, 0);
    bsg_scene_set_view(ref, v);
    bsg_scene_payload_update(ref, v);
    return changed || 1;
}


int
bsg_feature_realize(bsg_feature_ref ref, struct bsg_view *v, int recursive)
{
    return _feature_realize_node(bsg_feature_node(ref), v, recursive);
}

static struct bsg_payload *
_feature_payload(bsg_feature_ref ref)
{
    struct bsg_node *node = bsg_feature_node(ref);
    return node ? bsg_node_get_payload(node) : NULL;
}

static void
_feature_set_private_realization(bsg_feature_ref ref, struct bsg_payload *payload)
{
    struct bsg_node *node = bsg_feature_node(ref);
    if (!node)
	return;
    (void)bsg_geometry_node_set_private_realization(node, payload);
    bsg_node_touch(node);
    bsg_scene_node_invalidate(node);
}

static struct bsg_payload *
_feature_edit_preview_payload(bsg_feature_ref ref)
{
    struct bsg_payload *payload = _feature_payload(ref);
    return bsg_payload_edit_preview_get_data(payload) ? payload : NULL;
}

int
bsg_feature_edit_preview_attach(bsg_feature_ref ref,
				void *editor_ctx,
				void *aux_ctx,
				const struct bsg_edit_preview_ops *ops)
{
    if (!bsg_feature_node(ref))
	return 0;

    struct bsg_payload *payload = bsg_payload_edit_preview_create(editor_ctx, aux_ctx);
    if (!payload)
	return 0;

    if (ops && !bsg_payload_edit_preview_set_ops(payload,
		ops->preview_ctx,
		ops->owns_preview_ctx,
		ops->revision_cb,
		ops->update_cb,
		ops->bounds_cb,
		ops->pick_cb,
		ops->snap_cb,
		ops->free_cb)) {
	bsg_payload_free(payload);
	return 0;
    }

    _feature_set_private_realization(ref, payload);
    return 1;
}

int
bsg_feature_edit_preview_set_ops(bsg_feature_ref ref,
				 const struct bsg_edit_preview_ops *ops)
{
    struct bsg_payload *payload = _feature_edit_preview_payload(ref);
    if (!payload || !ops)
	return 0;

    int ret = bsg_payload_edit_preview_set_ops(payload,
	    ops->preview_ctx,
	    ops->owns_preview_ctx,
	    ops->revision_cb,
	    ops->update_cb,
	    ops->bounds_cb,
	    ops->pick_cb,
	    ops->snap_cb,
	    ops->free_cb);
    if (ret)
	(void)bsg_feature_touch(ref);
    return ret;
}

int
bsg_feature_edit_preview_touch(bsg_feature_ref ref)
{
    struct bsg_payload *payload = _feature_edit_preview_payload(ref);
    if (!payload)
	return 0;
    bsg_payload_bump_revision(payload);
    return bsg_feature_touch(ref);
}

uint64_t
bsg_feature_edit_preview_revision(bsg_feature_ref ref)
{
    struct bsg_payload *payload = _feature_edit_preview_payload(ref);
    return payload ? bsg_payload_edit_preview_revision(payload) : 0;
}

void
bsg_feature_edit_preview_mark_source_revision(bsg_feature_ref ref,
					      uint64_t source_revision,
					      bsg_edit_preview_stale_reason reason)
{
    struct bsg_payload *payload = _feature_edit_preview_payload(ref);
    if (payload && bsg_payload_edit_preview_mark_source_revision(payload,
		source_revision, reason))
	(void)bsg_feature_touch(ref);
}

void
bsg_feature_edit_preview_mark_inputs_revision(bsg_feature_ref ref,
					      uint64_t inputs_revision,
					      bsg_edit_preview_stale_reason reason)
{
    struct bsg_payload *payload = _feature_edit_preview_payload(ref);
    if (payload && bsg_payload_edit_preview_mark_inputs_revision(payload,
		inputs_revision, reason))
	(void)bsg_feature_touch(ref);
}

int
bsg_feature_edit_preview_is_stale(bsg_feature_ref ref)
{
    struct bsg_payload *payload = _feature_edit_preview_payload(ref);
    return payload ? bsg_payload_edit_preview_is_stale(payload) : 0;
}

bsg_edit_preview_stale_reason
bsg_feature_edit_preview_stale_reason(bsg_feature_ref ref)
{
    struct bsg_payload *payload = _feature_edit_preview_payload(ref);
    return payload ? bsg_payload_edit_preview_stale_reason(payload) : BSG_EDIT_PREVIEW_STALE_NONE;
}

int
bsg_feature_edit_preview_realize(bsg_feature_ref ref, struct bsg_view *v)
{
    struct bsg_payload *payload = _feature_edit_preview_payload(ref);
    return payload ? bsg_payload_edit_preview_realize(payload, v) : -1;
}

int
bsg_feature_edit_preview_bounds(bsg_feature_ref ref, point_t *bmin, point_t *bmax)
{
    struct bsg_payload *payload = _feature_edit_preview_payload(ref);
    return payload ? bsg_payload_edit_preview_bounds(payload, bmin, bmax) : 0;
}

int
bsg_feature_edit_preview_pick(bsg_feature_ref ref, struct bsg_view *v,
			      int x, int y, void *pick_out)
{
    struct bsg_payload *payload = _feature_edit_preview_payload(ref);
    return payload ? bsg_payload_edit_preview_pick(payload, v, x, y, pick_out) : 0;
}

int
bsg_feature_edit_preview_snap(bsg_feature_ref ref, struct bsg_view *v,
			      const point_t sample_pt, point_t out_pt)
{
    struct bsg_payload *payload = _feature_edit_preview_payload(ref);
    return payload ? bsg_payload_edit_preview_snap(payload, v, sample_pt, out_pt) : 0;
}

fastf_t
bsg_feature_view_depth(bsg_feature_ref ref, struct bsg_view *v, int mode)
{
    struct bsg_node *node = bsg_feature_node(ref);
    return node ? bsg_scene_node_view_depth(node, v, mode) : 0.0;
}


int
bsg_feature_points_replace(bsg_feature_ref ref,
			   enum bsg_feature_family family,
			   const point_t *points,
			   const int *cmds,
			   size_t point_count)
{
    struct bsg_node *node = bsg_feature_node(ref);
    if (!node)
	return 0;

    if (family != BSG_FEATURE_UNKNOWN)
	bsg_feature_set_family(ref, family);

    if (_feature_family_uses_line_geometry_node(_feature_family_from_node(node))) {
	if (!_feature_line_geometry_from_points(node, points, cmds, point_count))
	    return 0;
    } else {
	struct bsg_payload *pl = bsg_node_get_payload(node);
	if (!pl || pl->pl_type != BSG_PL_LINE_SET) {
	    pl = bsg_payload_line_set_create((point_t *)points, cmds, point_count);
	    bsg_node_set_payload(node, pl);
	} else {
	    bsg_payload_line_set_replace(pl, points, cmds, point_count);
	}
    }
    bsg_node_touch(node);
    bsg_scene_node_invalidate(node);
    return 1;
}


int
bsg_feature_points_copy(bsg_feature_ref ref,
			point_t **points_out,
			int **cmds_out,
			size_t *point_count_out)
{
    if (points_out)
	*points_out = NULL;
    if (cmds_out)
	*cmds_out = NULL;
    if (point_count_out)
	*point_count_out = 0;

    struct bsg_node *node = bsg_feature_node(ref);
    if (!node)
	return 0;

    if (_feature_line_geometry_points_copy(node, points_out, cmds_out, point_count_out))
	return 1;

    struct bsg_payload_line_set *ls = bsg_payload_line_set_get(bsg_node_get_payload(node));
    if (ls) {
	if (point_count_out)
	    *point_count_out = ls->point_cnt;
	if (!ls->point_cnt)
	    return 1;
	if (points_out) {
	    *points_out = (point_t *)bu_calloc(ls->point_cnt, sizeof(point_t), "bsg feature points copy");
	    for (size_t i = 0; i < ls->point_cnt; i++)
		VMOVE((*points_out)[i], ls->points[i]);
	}
	if (cmds_out) {
	    *cmds_out = (int *)bu_calloc(ls->point_cnt, sizeof(int), "bsg feature cmds copy");
	    for (size_t i = 0; i < ls->point_cnt; i++)
		(*cmds_out)[i] = ls->cmds ? ls->cmds[i] : ((i % 2) ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE);
	}
	return 1;
    }

    return 1;
}


static bsg_feature_ref
_feature_replace_points(struct bsg_view *v,
			const char *name,
			int local,
			enum bsg_feature_family family,
			int arrow,
			const point_t *points,
			const int *cmds,
			size_t point_count,
			const struct bsg_feature_style *style)
{
    if (!v || !name)
	return _feature_ref(NULL);

    bsg_feature_remove(v, name);
    if (!points || !point_count)
	return _feature_ref(NULL);

    struct bsg_feature_opts opts = BSG_FEATURE_OPTS_INIT;
    opts.family = family;
    opts.local = local;
    opts.arrow = arrow;
    bsg_feature_ref ref = bsg_feature_create(v, name, &opts);
    if (!ref.token)
	return ref;
    bsg_feature_points_replace(ref, family, points, cmds, point_count);
    if (style)
	bsg_feature_style_apply(ref, style);
    return ref;
}


bsg_feature_ref
bsg_feature_replace_lines(struct bsg_view *v,
			  const char *name,
			  int local,
			  const point_t *points,
			  size_t point_count,
			  const struct bsg_feature_style *style)
{
    return _feature_replace_points(v, name, local, BSG_FEATURE_LINES, 0, points, NULL, point_count, style);
}


static struct bsg_node *
_feature_line_layer_child_create(struct bsg_node *parent,
				 const char *fallback_parent_name,
				 size_t index,
				 const char *name)
{
    if (!parent)
	return NULL;

    struct bsg_node *child = bsg_scene_node_create_child(parent);
    if (!child)
	return NULL;

    bsg_node_set_object_type(child, bsg_geometry_type());
    bsg_node_add_geometry_roles(child, BSG_GEOMETRY_VIEW_OVERLAY | BSG_GEOMETRY_LINE_SET);
    _feature_mark_view_object(child);
    bsg_node_set_visible_state(child, 1);

    struct bu_vls child_name = BU_VLS_INIT_ZERO;
    if (name && strlen(name)) {
	bu_vls_sprintf(&child_name, "%s", name);
    } else if (fallback_parent_name && strlen(fallback_parent_name)) {
	bu_vls_sprintf(&child_name, "%s_layer_%zu", fallback_parent_name, index);
    }
    if (bu_vls_strlen(&child_name))
	bsg_node_set_name(child, bu_vls_cstr(&child_name));
    bu_vls_free(&child_name);

    struct bsg_feature_opts opts = BSG_FEATURE_OPTS_INIT;
    opts.family = BSG_FEATURE_LINES;
    _feature_tag(child, &opts);
    return child;
}


bsg_feature_ref
bsg_feature_replace_line_layers(struct bsg_view *v,
				const char *name,
				int local,
				const struct bsg_feature_line_layer *layers,
				size_t layer_count,
				const struct bsg_feature_style *style)
{
    if (!v || !name)
	return _feature_ref(NULL);

    bsg_feature_remove(v, name);
    if (!layers || !layer_count)
	return _feature_ref(NULL);

    size_t live_layers = 0;
    for (size_t i = 0; i < layer_count; i++) {
	if (layers[i].points && layers[i].point_count)
	    live_layers++;
    }
    if (!live_layers)
	return _feature_ref(NULL);

    bsg_feature_ref parent_ref = bsg_feature_create_overlay(v, name, local);
    struct bsg_node *parent = bsg_feature_node(parent_ref);
    if (!parent)
	return parent_ref;
    if (style)
	bsg_feature_style_apply(parent_ref, style);

    size_t created = 0;
    for (size_t i = 0; i < layer_count; i++) {
	if (!layers[i].points || !layers[i].point_count)
	    continue;
	struct bsg_node *child = _feature_line_layer_child_create(parent, name, created, layers[i].name);
	if (!child)
	    continue;
	if (!_feature_line_geometry_from_points(child, layers[i].points, layers[i].commands,
		    layers[i].point_count)) {
	    bsg_scene_node_release(child);
	    continue;
	}
	bsg_feature_ref child_ref = bsg_feature_ref_from_node(child, BSG_FEATURE_LINES);
	if (style)
	    (void)bsg_feature_style_apply(child_ref, style);
	(void)bsg_feature_style_apply(child_ref, &layers[i].style);
	created++;
    }

    if (!created) {
	bsg_feature_remove(v, name);
	return _feature_ref(NULL);
    }

    bsg_node_touch(parent);
    bsg_scene_node_invalidate(parent);
    return parent_ref;
}


bsg_feature_ref
bsg_feature_replace_indexed_face_set(struct bsg_view *v,
				     const char *name,
				     int local,
				     const point_t *points,
				     size_t point_count,
				     const vect_t *normals,
				     size_t normal_count,
				     const int *indices,
				     size_t index_count,
				     const struct bsg_feature_style *style)
{
    if (!v || !name)
	return _feature_ref(NULL);

    bsg_feature_remove(v, name);
    if (!points || point_count < 3 || !indices || index_count < 3)
	return _feature_ref(NULL);

    bsg_feature_ref ref = bsg_feature_create_face_set(v, name, local);
    struct bsg_node *node = bsg_feature_node(ref);
    if (!node)
	return ref;

    if (!bsg_geometry_node_set_indexed_face_set(node,
		points, point_count,
		normals, normal_count,
		indices, index_count)) {
	bsg_feature_remove(v, name);
	return _feature_ref(NULL);
    }

    if (style)
	bsg_feature_style_apply(ref, style);
    bsg_node_touch(node);
    bsg_scene_node_invalidate(node);
    return ref;
}


bsg_feature_ref
bsg_feature_replace_arrow(struct bsg_view *v,
			  const char *name,
			  int local,
			  const point_t *points,
			  size_t point_count,
			  const struct bsg_feature_style *style)
{
    return _feature_replace_points(v, name, local, BSG_FEATURE_ARROW, 1, points, NULL, point_count, style);
}


bsg_feature_ref
bsg_feature_replace_axes(struct bsg_view *v,
			 const char *name,
			 int local,
			 const point_t *centers,
			 size_t center_count,
			 fastf_t half_axes_size,
			 const struct bsg_feature_style *style)
{
    if (!centers || !center_count)
	return _feature_ref(NULL);

    size_t point_count = center_count * 6;
    point_t *points = (point_t *)bu_calloc(point_count, sizeof(point_t), "bsg feature axes points");
    int *cmds = (int *)bu_calloc(point_count, sizeof(int), "bsg feature axes cmds");
    for (size_t i = 0; i < center_count; i++) {
	size_t k = i * 6;
	VSET(points[k + 0], centers[i][X] - half_axes_size, centers[i][Y], centers[i][Z]);
	VSET(points[k + 1], centers[i][X] + half_axes_size, centers[i][Y], centers[i][Z]);
	VSET(points[k + 2], centers[i][X], centers[i][Y] - half_axes_size, centers[i][Z]);
	VSET(points[k + 3], centers[i][X], centers[i][Y] + half_axes_size, centers[i][Z]);
	VSET(points[k + 4], centers[i][X], centers[i][Y], centers[i][Z] - half_axes_size);
	VSET(points[k + 5], centers[i][X], centers[i][Y], centers[i][Z] + half_axes_size);
	for (size_t j = 0; j < 6; j++)
	    cmds[k + j] = (j % 2) ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE;
    }

    bsg_feature_ref ref = _feature_replace_points(v, name, local, BSG_FEATURE_AXES, 0,
	    (const point_t *)points, cmds, point_count, style);
    bu_free(points, "bsg feature axes points");
    bu_free(cmds, "bsg feature axes cmds");
    return ref;
}


int
bsg_feature_axes_state_get(bsg_feature_ref ref, struct bsg_axes *axes_out)
{
    struct bsg_payload *payload;
    struct bsg_axes *axes;
    if (!axes_out)
	return 0;
    payload = _feature_payload(ref);
    axes = bsg_payload_axes_get(payload);
    if (!axes)
	return 0;
    memcpy(axes_out, axes, sizeof(*axes_out));
    return 1;
}


int
bsg_feature_axes_state_replace(bsg_feature_ref ref, const struct bsg_axes *axes)
{
    struct bsg_node *node;
    if (!axes)
	return 0;
    node = bsg_feature_node(ref);
    if (!node)
	return 0;
    if (!bsg_geometry_node_set_axes_overlay(node, axes))
	return 0;
    bsg_node_touch(node);
    bsg_scene_node_invalidate(node);
    return 1;
}


int
bsg_feature_axes_centers_copy(bsg_feature_ref ref,
			      point_t **centers_out,
			      size_t *center_count_out)
{
    if (centers_out)
	*centers_out = NULL;
    if (center_count_out)
	*center_count_out = 0;

    point_t *points = NULL;
    size_t point_count = 0;
    if (!bsg_feature_points_copy(ref, &points, NULL, &point_count))
	return 0;
    if (!points || point_count < 6 || (point_count % 6) != 0) {
	if (points)
	    bu_free(points, "bsg feature points copy");
	return 1;
    }

    size_t center_count = point_count / 6;
    point_t *centers = (point_t *)bu_calloc(center_count, sizeof(point_t), "bsg feature axes centers copy");
    for (size_t i = 0; i < center_count; i++) {
	centers[i][X] = (points[6*i+0][X] + points[6*i+1][X]) * 0.5;
	centers[i][Y] = points[6*i+0][Y];
	centers[i][Z] = points[6*i+0][Z];
    }
    bu_free(points, "bsg feature points copy");
    if (centers_out)
	*centers_out = centers;
    else
	bu_free(centers, "bsg feature axes centers copy");
    if (center_count_out)
	*center_count_out = center_count;
    return 1;
}


int
bsg_feature_arrow_tip_set(bsg_feature_ref ref, fastf_t tip_length, fastf_t tip_width)
{
    struct bsg_node *node = bsg_feature_node(ref);
    if (!node)
	return 0;
    bsg_shape_set_arrow_tip_length(node, tip_length);
    bsg_shape_set_arrow_tip_width(node, tip_width);
    bsg_node_touch(node);
    bsg_scene_node_invalidate(node);
    return 1;
}


int
bsg_feature_arrow_tip_get(bsg_feature_ref ref, fastf_t *tip_length, fastf_t *tip_width)
{
    struct bsg_node *node = bsg_feature_node(ref);
    if (!node)
	return 0;
    if (tip_length)
	*tip_length = bsg_shape_arrow_tip_length(node);
    if (tip_width)
	*tip_width = bsg_shape_arrow_tip_width(node);
    return 1;
}


static void
_feature_labels_clear(struct bsg_node *parent)
{
    if (!parent)
	return;
    while (BU_PTBL_LEN(&parent->children)) {
	struct bsg_node *child = (struct bsg_node *)BU_PTBL_GET(&parent->children, 0);
	if (!child)
	    break;
	bsg_scene_node_release(child);
    }
    bu_ptbl_reset(&parent->children);
}


int
bsg_feature_labels_replace(bsg_feature_ref ref,
			   const struct bsg_feature_label_data *labels,
			   size_t label_count)
{
    struct bsg_node *parent = bsg_feature_node(ref);
    if (!parent)
	return 0;

    bsg_feature_set_family(ref, BSG_FEATURE_LABEL);
    _feature_labels_clear(parent);
    if (!labels || !label_count) {
	bsg_node_touch(parent);
	bsg_scene_node_invalidate(parent);
	return 1;
    }

    for (size_t i = 0; i < label_count; i++) {
	struct bsg_node *child = bsg_scene_node_create_child(parent);
	if (!child)
	    continue;
	bsg_node_add_geometry_roles(child, BSG_GEOMETRY_TEXT_LABELS);
	bsg_node_set_visible_state(child, 1);
	if (labels[i].color_valid) {
	    bsg_node_set_color(child, labels[i].color[0], labels[i].color[1], labels[i].color[2]);
	} else {
	    unsigned char color[3] = {0, 0, 0};
	    bsg_node_get_color(parent, &color[0], &color[1], &color[2]);
	    bsg_node_set_color(child, color[0], color[1], color[2]);
	}

	struct bsg_label *label;
	BU_GET(label, struct bsg_label);
	memset(label, 0, sizeof(*label));
	BU_VLS_INIT(&label->label);
	bu_vls_sprintf(&label->label, "%s", labels[i].text ? labels[i].text : "");
	VMOVE(label->p, labels[i].point);
	label->line_flag = labels[i].line_flag;
	VMOVE(label->target, labels[i].target);
	label->anchor = labels[i].anchor;
	label->arrow = labels[i].arrow;
	(void)bsg_geometry_node_set_text_label(child, label);
	if (BU_VLS_IS_INITIALIZED(&label->label))
	    bu_vls_free(&label->label);
	BU_PUT(label, struct bsg_label);
	bsg_node_touch(child);
    }

    bsg_node_touch(parent);
    bsg_scene_node_invalidate(parent);
    return 1;
}


size_t
bsg_feature_label_count(bsg_feature_ref ref)
{
    struct bsg_node *parent = bsg_feature_node(ref);
    return parent ? bsg_node_child_count(parent) : 0;
}


int
bsg_feature_label_copy(bsg_feature_ref ref,
		       size_t index,
		       struct bu_vls *text_out,
		       point_t point_out,
		       unsigned char color_out[3])
{
    struct bsg_node *parent = bsg_feature_node(ref);
    if (!parent)
	return 0;
    struct bsg_node *child = bsg_node_child_at(parent, index);
    if (!child)
	return 0;
    struct bsg_label *label = bsg_payload_text_get(bsg_node_get_payload(child));
    if (!label)
	return 0;
    if (text_out)
	bu_vls_sprintf(text_out, "%s", bu_vls_cstr(&label->label));
    if (point_out)
	VMOVE(point_out, label->p);
    if (color_out)
	bsg_node_get_color(child, &color_out[0], &color_out[1], &color_out[2]);
    return 1;
}


int
bsg_feature_label_point_set(bsg_feature_ref ref, size_t index, const point_t point)
{
    struct bsg_node *parent = bsg_feature_node(ref);
    if (!parent || !point)
	return 0;
    struct bsg_node *child = bsg_node_child_at(parent, index);
    if (!child)
	return 0;
    struct bsg_payload *pl = bsg_node_get_payload(child);
    struct bsg_label *label = bsg_payload_text_get(pl);
    if (!label)
	return 0;
    VMOVE(label->p, point);
    bsg_payload_bump_revision(pl);
    bsg_node_touch(child);
    bsg_node_touch(parent);
    bsg_scene_node_invalidate(parent);
    return 1;
}


int
bsg_feature_labels_color_set(bsg_feature_ref ref, const unsigned char color[3])
{
    struct bsg_node *parent = bsg_feature_node(ref);
    if (!parent || !color)
	return 0;
    bsg_feature_set_color(ref, color[0], color[1], color[2]);
    for (size_t i = 0; i < bsg_node_child_count(parent); i++) {
	struct bsg_node *child = bsg_node_child_at(parent, i);
	if (!child)
	    continue;
	bsg_node_set_color(child, color[0], color[1], color[2]);
	bsg_node_touch(child);
    }
    bsg_node_touch(parent);
    bsg_scene_node_invalidate(parent);
    return 1;
}


void
bsg_feature_set_view(bsg_feature_ref ref, struct bsg_view *v)
{
    struct bsg_node *node = bsg_feature_node(ref);
    if (!node)
	return;
    bsg_node_set_view(node, v);
    for (size_t i = 0; i < bsg_node_child_count(node); i++) {
	struct bsg_node *child = bsg_node_child_at(node, i);
	if (child)
	    bsg_node_set_view(child, v);
    }
    bsg_node_touch(node);
}


int
bsg_feature_overlay_register_owner(bsg_feature_ref ref,
				   const void *owner,
				   bsg_overlay_role role,
				   bsg_overlay_class overlay_class,
				   bsg_overlay_lifecycle lifecycle,
				   bsg_overlay_order ordering,
				   const char *source_path,
				   int sort_order)
{
    struct bsg_node *node = bsg_feature_node(ref);
    if (!node)
	return 0;
    return bsg_overlay_register_owner(node, owner, role, overlay_class, lifecycle,
	    ordering, source_path, sort_order);
}


size_t
bsg_feature_overlay_auto_remove(struct bsg_view *v, const char *source_path)
{
    if (!v || !source_path)
	return 0;
    size_t removed = bsg_overlay_auto_remove(bsg_view_scene_root(v), source_path);
    removed += bsg_overlay_auto_remove((struct bsg_node *)v->gv_hud_root, source_path);
    return removed;
}


void
bsg_feature_labels_sync(struct bsg_view *v, struct bsg_data_label_state *gdlsp, const char *name)
{
    if (!v || !gdlsp || !name)
	return;

    bsg_feature_remove(v, name);
    if (!gdlsp->gdls_draw || gdlsp->gdls_num_labels < 1)
	return;

    struct bsg_feature_opts opts = BSG_FEATURE_OPTS_INIT;
    opts.family = BSG_FEATURE_LABEL;
    opts.local = 1;
    bsg_feature_ref parent_ref = bsg_feature_create(v, name, &opts);
    if (!parent_ref.token)
	return;

    struct bsg_feature_label_data *labels = (struct bsg_feature_label_data *)bu_calloc((size_t)gdlsp->gdls_num_labels,
	    sizeof(struct bsg_feature_label_data), "bsg feature label sync");
    for (int i = 0; i < gdlsp->gdls_num_labels; i++) {
	labels[i].text = gdlsp->gdls_labels[i];
	VMOVE(labels[i].point, gdlsp->gdls_points[i]);
	labels[i].color_valid = 1;
	VSET(labels[i].color, gdlsp->gdls_color[0], gdlsp->gdls_color[1], gdlsp->gdls_color[2]);
	labels[i].anchor = BSG_ANCHOR_AUTO;
    }
    bsg_feature_labels_replace(parent_ref, labels, (size_t)gdlsp->gdls_num_labels);
    bu_free(labels, "bsg feature label sync");
}
