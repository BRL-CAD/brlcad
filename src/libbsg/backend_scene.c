/*              B A C K E N D _ S C E N E . C
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
/** @file libbsg/backend_scene.c */

#include "common.h"

#include <string.h>

#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bsg/action.h"
#include "bsg/backend_scene.h"
#include "bsg/render.h"
#include "action_private.h"

struct bsg_backend_scene {
    struct bsg_backend_adapter adapter;
    struct bsg_backend_scene_node *nodes;
    struct bsg_backend_scene_stats stats;
    struct bsg_backend_scene_camera camera;
    struct bsg_backend_scene_clip clip;
    struct bsg_backend_scene_lights lights;
    unsigned int capabilities;
};

static const unsigned int BSG_BACKEND_SCENE_DEFAULT_CAPABILITIES =
    BSG_ADAPTER_CAP_TRANSPARENCY |
    BSG_ADAPTER_CAP_WIREFRAME |
    BSG_ADAPTER_CAP_SHADED |
    BSG_ADAPTER_CAP_HUD |
    BSG_ADAPTER_CAP_SORTED_ALPHA;

static bsg_backend_scene_record_kind
_scene_kind_from_item(const struct bsg_render_item *item)
{
    if (!item)
	return BSG_BACKEND_SCENE_NODE_GEOMETRY;
    if (item->geometry.kind == BSG_RENDER_GEOMETRY_TEXT)
	return BSG_BACKEND_SCENE_NODE_TEXT;
    if (item->geometry.kind == BSG_RENDER_GEOMETRY_IMAGE)
	return BSG_BACKEND_SCENE_NODE_IMAGE;
    if (item->phase == BSG_RENDER_PHASE_HUD ||
	    item->geometry.kind == BSG_RENDER_GEOMETRY_HUD)
	return BSG_BACKEND_SCENE_NODE_HUD;
    if (item->phase == BSG_RENDER_PHASE_OVERLAY ||
	    item->geometry.kind == BSG_RENDER_GEOMETRY_OVERLAY ||
	    item->geometry.kind == BSG_RENDER_GEOMETRY_ANNOTATION)
	return BSG_BACKEND_SCENE_NODE_OVERLAY;
    return BSG_BACKEND_SCENE_NODE_GEOMETRY;
}

static uint64_t
_source_identity_from_item(const struct bsg_render_item *item)
{
    if (!item)
	return 0;
    if (item->source.source_id)
	return item->source.source_id;
    if (item->geometry.source_identity)
	return item->geometry.source_identity;
    return item->cache_identity;
}

static uint64_t
_lod_identity_from_item(const struct bsg_render_item *item)
{
    return item ? item->source.lod_id : 0;
}

static unsigned int
_semantic_request_flags(unsigned int flags)
{
    return flags & ~BSG_RENDER_FLAG_COLLECT_ITEMS;
}

static unsigned int
_capability_gaps(const struct bsg_render_item *item, unsigned int caps)
{
    unsigned int gaps = BSG_BACKEND_SCENE_GAP_NONE;

    if (!item)
	return gaps;

    if (item->appearance.transparency < 1.0 &&
	    !(caps & BSG_ADAPTER_CAP_TRANSPARENCY))
	gaps |= BSG_BACKEND_SCENE_GAP_TRANSPARENCY;
    if (item->appearance.draw_mode == 0 &&
	    !(caps & BSG_ADAPTER_CAP_WIREFRAME))
	gaps |= BSG_BACKEND_SCENE_GAP_WIREFRAME;
    if (item->appearance.draw_mode > 0 &&
	    !(caps & BSG_ADAPTER_CAP_SHADED))
	gaps |= BSG_BACKEND_SCENE_GAP_SHADED;
    if ((item->phase == BSG_RENDER_PHASE_HUD ||
		item->geometry.kind == BSG_RENDER_GEOMETRY_HUD) &&
	    !(caps & BSG_ADAPTER_CAP_HUD))
	gaps |= BSG_BACKEND_SCENE_GAP_HUD;
    if (item->phase == BSG_RENDER_PHASE_TRANSPARENT &&
	    (item->context.request_flags & BSG_RENDER_FLAG_SORTED_ALPHA) &&
	    !(caps & BSG_ADAPTER_CAP_SORTED_ALPHA))
	gaps |= BSG_BACKEND_SCENE_GAP_SORTED_ALPHA;

    return gaps;
}

static void
_annotation_segment_clear(struct bsg_annotation_segment *seg)
{
    if (!seg)
	return;
    if (seg->kind == BSG_ANNOTATION_SEGMENT_NURB) {
	if (seg->data.nurb.knots)
	    bu_free(seg->data.nurb.knots, "bsg backend scene annotation nurb knots");
	if (seg->data.nurb.control_points)
	    bu_free(seg->data.nurb.control_points, "bsg backend scene annotation nurb control points");
	if (seg->data.nurb.weights)
	    bu_free(seg->data.nurb.weights, "bsg backend scene annotation nurb weights");
    } else if (seg->kind == BSG_ANNOTATION_SEGMENT_BEZIER) {
	if (seg->data.bezier.control_points)
	    bu_free(seg->data.bezier.control_points, "bsg backend scene annotation bezier control points");
    } else if (seg->kind == BSG_ANNOTATION_SEGMENT_TEXT) {
	if (seg->data.text.text)
	    bu_free(seg->data.text.text, "bsg backend scene annotation text segment");
    }
}

static void
_annotation_segment_copy(struct bsg_annotation_segment *dst,
			 const struct bsg_annotation_segment *src)
{
    if (!dst || !src)
	return;
    memset(dst, 0, sizeof(*dst));
    dst->kind = src->kind;
    dst->reverse = src->reverse;
    switch (src->kind) {
	case BSG_ANNOTATION_SEGMENT_LINE:
	    dst->data.line = src->data.line;
	    break;
	case BSG_ANNOTATION_SEGMENT_CARC:
	    dst->data.carc = src->data.carc;
	    break;
	case BSG_ANNOTATION_SEGMENT_NURB:
	    dst->data.nurb.order = src->data.nurb.order;
	    dst->data.nurb.point_type = src->data.nurb.point_type;
	    dst->data.nurb.knot_count = src->data.nurb.knot_count;
	    dst->data.nurb.control_point_count = src->data.nurb.control_point_count;
	    if (src->data.nurb.knot_count && src->data.nurb.knots) {
		dst->data.nurb.knots = (fastf_t *)bu_calloc(src->data.nurb.knot_count,
			sizeof(fastf_t), "bsg backend scene annotation nurb knots");
		memcpy(dst->data.nurb.knots, src->data.nurb.knots,
			src->data.nurb.knot_count * sizeof(fastf_t));
	    }
	    if (src->data.nurb.control_point_count && src->data.nurb.control_points) {
		dst->data.nurb.control_points = (int *)bu_calloc(
			src->data.nurb.control_point_count, sizeof(int),
			"bsg backend scene annotation nurb control points");
		memcpy(dst->data.nurb.control_points, src->data.nurb.control_points,
			src->data.nurb.control_point_count * sizeof(int));
	    }
	    if (src->data.nurb.control_point_count && src->data.nurb.weights) {
		dst->data.nurb.weights = (fastf_t *)bu_calloc(
			src->data.nurb.control_point_count, sizeof(fastf_t),
			"bsg backend scene annotation nurb weights");
		memcpy(dst->data.nurb.weights, src->data.nurb.weights,
			src->data.nurb.control_point_count * sizeof(fastf_t));
	    }
	    break;
	case BSG_ANNOTATION_SEGMENT_BEZIER:
	    dst->data.bezier.degree = src->data.bezier.degree;
	    dst->data.bezier.control_point_count = src->data.bezier.control_point_count;
	    if (src->data.bezier.control_point_count && src->data.bezier.control_points) {
		dst->data.bezier.control_points = (int *)bu_calloc(
			src->data.bezier.control_point_count, sizeof(int),
			"bsg backend scene annotation bezier control points");
		memcpy(dst->data.bezier.control_points, src->data.bezier.control_points,
			src->data.bezier.control_point_count * sizeof(int));
	    }
	    break;
	case BSG_ANNOTATION_SEGMENT_TEXT:
	    dst->data.text.ref_pt = src->data.text.ref_pt;
	    dst->data.text.relative_position = src->data.text.relative_position;
	    dst->data.text.size = src->data.text.size;
	    dst->data.text.rotation = src->data.text.rotation;
	    if (src->data.text.text)
		dst->data.text.text = bu_strdup(src->data.text.text);
	    break;
	default:
	    break;
    }
}

static int
_annotation_segment_equal(const struct bsg_annotation_segment *a,
			  const struct bsg_annotation_segment *b)
{
    if (!a || !b)
	return a == b;
    if (a->kind != b->kind || a->reverse != b->reverse)
	return 0;
    switch (a->kind) {
	case BSG_ANNOTATION_SEGMENT_LINE:
	    return a->data.line.start == b->data.line.start &&
		a->data.line.end == b->data.line.end;
	case BSG_ANNOTATION_SEGMENT_CARC:
	    return memcmp(&a->data.carc, &b->data.carc, sizeof(a->data.carc)) == 0;
	case BSG_ANNOTATION_SEGMENT_NURB:
	    if (a->data.nurb.order != b->data.nurb.order ||
		    a->data.nurb.point_type != b->data.nurb.point_type ||
		    a->data.nurb.knot_count != b->data.nurb.knot_count ||
		    a->data.nurb.control_point_count != b->data.nurb.control_point_count)
		return 0;
	    if (a->data.nurb.knot_count &&
		    memcmp(a->data.nurb.knots, b->data.nurb.knots,
			a->data.nurb.knot_count * sizeof(fastf_t)) != 0)
		return 0;
	    if (a->data.nurb.control_point_count &&
		    memcmp(a->data.nurb.control_points, b->data.nurb.control_points,
			a->data.nurb.control_point_count * sizeof(int)) != 0)
		return 0;
	    if (!!a->data.nurb.weights != !!b->data.nurb.weights)
		return 0;
	    if (a->data.nurb.weights &&
		    memcmp(a->data.nurb.weights, b->data.nurb.weights,
			a->data.nurb.control_point_count * sizeof(fastf_t)) != 0)
		return 0;
	    return 1;
	case BSG_ANNOTATION_SEGMENT_BEZIER:
	    if (a->data.bezier.degree != b->data.bezier.degree ||
		    a->data.bezier.control_point_count != b->data.bezier.control_point_count)
		return 0;
	    if (a->data.bezier.control_point_count &&
		    memcmp(a->data.bezier.control_points, b->data.bezier.control_points,
			a->data.bezier.control_point_count * sizeof(int)) != 0)
		return 0;
	    return 1;
	case BSG_ANNOTATION_SEGMENT_TEXT:
	    return a->data.text.ref_pt == b->data.text.ref_pt &&
		a->data.text.relative_position == b->data.text.relative_position &&
		NEAR_EQUAL(a->data.text.size, b->data.text.size, SMALL_FASTF) &&
		NEAR_EQUAL(a->data.text.rotation, b->data.text.rotation, SMALL_FASTF) &&
		BU_STR_EQUAL(a->data.text.text ? a->data.text.text : "",
		    b->data.text.text ? b->data.text.text : "");
	default:
	    return 1;
    }
}

static void
_geometry_arrays_free(struct bsg_backend_scene_geometry *geometry)
{
    if (!geometry)
	return;
    if (geometry->arrays.points)
	bu_free(geometry->arrays.points, "bsg backend scene geometry points");
    if (geometry->arrays.normals)
	bu_free(geometry->arrays.normals, "bsg backend scene geometry normals");
    if (geometry->arrays.commands)
	bu_free(geometry->arrays.commands, "bsg backend scene geometry commands");
    if (geometry->arrays.indices)
	bu_free(geometry->arrays.indices, "bsg backend scene geometry indices");
    memset(&geometry->arrays, 0, sizeof(geometry->arrays));
    if (geometry->surface.points)
	bu_free(geometry->surface.points, "bsg backend scene surface points");
    if (geometry->surface.normals)
	bu_free(geometry->surface.normals, "bsg backend scene surface normals");
    if (geometry->surface.indices)
	bu_free(geometry->surface.indices, "bsg backend scene surface indices");
    memset(&geometry->surface, 0, sizeof(geometry->surface));
    if (geometry->image.pixels)
	bu_free(geometry->image.pixels, "bsg backend scene image pixels");
    memset(&geometry->image, 0, sizeof(geometry->image));
    if (geometry->annotation.text)
	bu_free(geometry->annotation.text, "bsg backend scene annotation text");
    if (geometry->annotation.points)
	bu_free(geometry->annotation.points, "bsg backend scene annotation points");
    if (geometry->annotation.segments) {
	for (size_t i = 0; i < geometry->annotation.segment_count; i++)
	    _annotation_segment_clear(&geometry->annotation.segments[i]);
	bu_free(geometry->annotation.segments, "bsg backend scene annotation segments");
    }
    memset(&geometry->annotation, 0, sizeof(geometry->annotation));
}

static void
_text_record_free(struct bsg_backend_scene_text *text)
{
    if (!text)
	return;
    if (text->label)
	bu_free(text->label, "bsg backend scene text label");
    memset(text, 0, sizeof(*text));
}

static void
_node_free(struct bsg_backend_scene_node *node)
{
    if (!node)
	return;
    _geometry_arrays_free(&node->geometry);
    _text_record_free(&node->text);
    BU_PUT(node, struct bsg_backend_scene_node);
}

static void
_text_record_copy(struct bsg_backend_scene_text *dst,
		  const struct bsg_render_item *item)
{
    if (!dst)
	return;
    _text_record_free(dst);
    if (!item)
	return;
    dst->hud_feature_type = item->features.hud_feature_type;
    if (item->geometry.text.label) {
	dst->label = bu_strdup(item->geometry.text.label);
	VMOVE(dst->position, item->geometry.text.position);
	VMOVE(dst->target, item->geometry.text.target);
	dst->size = item->geometry.text.size;
	dst->line_flag = item->geometry.text.line_flag;
	dst->anchor = item->geometry.text.anchor;
	dst->arrow = item->geometry.text.arrow;
    }
}

static int
_text_record_matches(const struct bsg_backend_scene_text *text,
		     const struct bsg_render_item *item)
{
    if (!text || !item)
	return 0;
    if (text->hud_feature_type != item->features.hud_feature_type)
	return 0;
    if (!!text->label != !!item->geometry.text.label)
	return 0;
    if (text->label && !BU_STR_EQUAL(text->label, item->geometry.text.label))
	return 0;
    if (!VNEAR_EQUAL(text->position, item->geometry.text.position, SMALL_FASTF))
	return 0;
    if (!VNEAR_EQUAL(text->target, item->geometry.text.target, SMALL_FASTF))
	return 0;
    if (text->size != item->geometry.text.size)
	return 0;
    if (text->line_flag != item->geometry.text.line_flag)
	return 0;
    if (text->anchor != item->geometry.text.anchor)
	return 0;
    if (text->arrow != item->geometry.text.arrow)
	return 0;
    return 1;
}

static int
_axes_record_matches(const struct bsg_axes *a, const struct bsg_axes *b)
{
    if (!a || !b)
	return 0;
    return a->draw == b->draw &&
	VNEAR_EQUAL(a->axes_pos, b->axes_pos, SMALL_FASTF) &&
	NEAR_EQUAL(a->axes_size, b->axes_size, SMALL_FASTF) &&
	a->line_width == b->line_width &&
	a->axes_color[0] == b->axes_color[0] &&
	a->axes_color[1] == b->axes_color[1] &&
	a->axes_color[2] == b->axes_color[2] &&
	a->pos_only == b->pos_only &&
	a->label_flag == b->label_flag &&
	a->label_color[0] == b->label_color[0] &&
	a->label_color[1] == b->label_color[1] &&
	a->label_color[2] == b->label_color[2] &&
	a->triple_color == b->triple_color &&
	a->tick_enabled == b->tick_enabled &&
	a->tick_length == b->tick_length &&
	a->tick_major_length == b->tick_major_length &&
	NEAR_EQUAL(a->tick_interval, b->tick_interval, SMALL_FASTF) &&
	a->ticks_per_major == b->ticks_per_major &&
	a->tick_threshold == b->tick_threshold &&
	a->tick_color[0] == b->tick_color[0] &&
	a->tick_color[1] == b->tick_color[1] &&
	a->tick_color[2] == b->tick_color[2] &&
	a->tick_major_color[0] == b->tick_major_color[0] &&
	a->tick_major_color[1] == b->tick_major_color[1] &&
	a->tick_major_color[2] == b->tick_major_color[2];
}

static int
_grid_record_matches(const struct bsg_grid_state *a,
		     const struct bsg_grid_state *b)
{
    if (!a || !b)
	return 0;
    return a->rc == b->rc &&
	a->draw == b->draw &&
	a->adaptive == b->adaptive &&
	a->snap == b->snap &&
	VNEAR_EQUAL(a->anchor, b->anchor, SMALL_FASTF) &&
	NEAR_EQUAL(a->res_h, b->res_h, SMALL_FASTF) &&
	NEAR_EQUAL(a->res_v, b->res_v, SMALL_FASTF) &&
	a->res_major_h == b->res_major_h &&
	a->res_major_v == b->res_major_v &&
	a->color[0] == b->color[0] &&
	a->color[1] == b->color[1] &&
	a->color[2] == b->color[2];
}

static int
_overlay_record_matches(const struct bsg_backend_scene_overlay *overlay,
			const struct bsg_render_item *item)
{
    if (!overlay || !item)
	return 0;
    if (overlay->phase != item->phase ||
	    overlay->geometry_kind != item->geometry.overlay.kind)
	return 0;
    switch (overlay->geometry_kind) {
	case BSG_RENDER_OVERLAY_GEOMETRY_AXES:
	    return _axes_record_matches(&overlay->axes, &item->geometry.overlay.axes);
	case BSG_RENDER_OVERLAY_GEOMETRY_GRID:
	    return _grid_record_matches(&overlay->grid, &item->geometry.overlay.grid);
	default:
	    return 1;
    }
}

static int
_geometry_arrays_copy(struct bsg_backend_scene_geometry *dst,
		      const struct bsg_render_geometry *src)
{
    if (!dst || !src)
	return 0;

    _geometry_arrays_free(dst);

    if (src->arrays.point_count && src->arrays.points) {
	dst->arrays.points = (point_t *)bu_calloc(src->arrays.point_count,
		sizeof(point_t), "bsg backend scene geometry points");
	for (size_t i = 0; i < src->arrays.point_count; i++)
	    VMOVE(dst->arrays.points[i], src->arrays.points[i]);
	dst->arrays.point_count = src->arrays.point_count;
    }
    if (src->arrays.normal_count && src->arrays.normals) {
	dst->arrays.normals = (point_t *)bu_calloc(src->arrays.normal_count,
		sizeof(point_t), "bsg backend scene geometry normals");
	for (size_t i = 0; i < src->arrays.normal_count; i++)
	    VMOVE(dst->arrays.normals[i], src->arrays.normals[i]);
	dst->arrays.normal_count = src->arrays.normal_count;
    }
    if (src->arrays.command_count && src->arrays.commands) {
	dst->arrays.commands = (int *)bu_calloc(src->arrays.command_count,
		sizeof(int), "bsg backend scene geometry commands");
	memcpy(dst->arrays.commands, src->arrays.commands,
		src->arrays.command_count * sizeof(int));
	dst->arrays.command_count = src->arrays.command_count;
    }
    if (src->arrays.index_count && src->arrays.indices) {
	dst->arrays.indices = (int *)bu_calloc(src->arrays.index_count,
		sizeof(int), "bsg backend scene geometry indices");
	memcpy(dst->arrays.indices, src->arrays.indices,
		src->arrays.index_count * sizeof(int));
	dst->arrays.index_count = src->arrays.index_count;
    }
    if (src->surface.point_count && src->surface.points) {
	dst->surface.points = (point_t *)bu_calloc(src->surface.point_count,
		sizeof(point_t), "bsg backend scene surface points");
	for (size_t i = 0; i < src->surface.point_count; i++)
	    VMOVE(dst->surface.points[i], src->surface.points[i]);
	dst->surface.point_count = src->surface.point_count;
    }
    if (src->surface.normal_count && src->surface.normals) {
	dst->surface.normals = (vect_t *)bu_calloc(src->surface.normal_count,
		sizeof(vect_t), "bsg backend scene surface normals");
	for (size_t i = 0; i < src->surface.normal_count; i++)
	    VMOVE(dst->surface.normals[i], src->surface.normals[i]);
	dst->surface.normal_count = src->surface.normal_count;
    }
    if (src->surface.index_count && src->surface.indices) {
	dst->surface.indices = (int *)bu_calloc(src->surface.index_count,
		sizeof(int), "bsg backend scene surface indices");
	memcpy(dst->surface.indices, src->surface.indices,
		src->surface.index_count * sizeof(int));
	dst->surface.index_count = src->surface.index_count;
    }
    dst->surface.face_count = src->surface.face_count;
    dst->surface.normal_kind = src->surface.normal_kind;
    dst->surface.material_valid = src->surface.material_valid;
    dst->surface.material = src->surface.material;
    dst->proxy = src->proxy;
    dst->image.kind = src->image.kind;
    dst->image.width = src->image.width;
    dst->image.height = src->image.height;
    dst->image.channels = src->image.channels;
    dst->image.pixel_count = src->image.pixel_count;
    dst->image.framebuffer_mode = src->image.framebuffer_mode;
    if (src->image.pixel_count && src->image.pixels) {
	dst->image.pixels = (unsigned char *)bu_malloc(src->image.pixel_count,
		"bsg backend scene image pixels");
	memcpy(dst->image.pixels, src->image.pixels, src->image.pixel_count);
    }
    if (src->annotation.text)
	dst->annotation.text = bu_strdup(src->annotation.text);
    dst->annotation.space = src->annotation.space;
    VMOVE(dst->annotation.anchor, src->annotation.anchor);
    MAT_COPY(dst->annotation.model_mat, src->annotation.model_mat);
    MAT_COPY(dst->annotation.display_mat, src->annotation.display_mat);
    if (src->annotation.point_count && src->annotation.points) {
	dst->annotation.points = (point_t *)bu_calloc(src->annotation.point_count,
		sizeof(point_t), "bsg backend scene annotation points");
	for (size_t i = 0; i < src->annotation.point_count; i++)
	    VMOVE(dst->annotation.points[i], src->annotation.points[i]);
	dst->annotation.point_count = src->annotation.point_count;
    }
    if (src->annotation.segment_count && src->annotation.segments) {
	dst->annotation.segments = (struct bsg_annotation_segment *)bu_calloc(
		src->annotation.segment_count, sizeof(struct bsg_annotation_segment),
		"bsg backend scene annotation segments");
	for (size_t i = 0; i < src->annotation.segment_count; i++)
	    _annotation_segment_copy(&dst->annotation.segments[i],
		    &src->annotation.segments[i]);
	dst->annotation.segment_count = src->annotation.segment_count;
    }

    return 1;
}

static int
_geometry_arrays_match(const struct bsg_backend_scene_geometry *scene_geometry,
		       const struct bsg_render_geometry *item_geometry)
{
    if (!scene_geometry || !item_geometry)
	return 0;
    if (scene_geometry->arrays.point_count != item_geometry->arrays.point_count ||
	    scene_geometry->arrays.normal_count != item_geometry->arrays.normal_count ||
	    scene_geometry->arrays.command_count != item_geometry->arrays.command_count ||
	    scene_geometry->arrays.index_count != item_geometry->arrays.index_count)
	return 0;
    if ((scene_geometry->arrays.point_count && (!scene_geometry->arrays.points ||
			!item_geometry->arrays.points)) ||
	    (scene_geometry->arrays.normal_count && (!scene_geometry->arrays.normals ||
			!item_geometry->arrays.normals)) ||
	    (scene_geometry->arrays.command_count && (!scene_geometry->arrays.commands ||
			!item_geometry->arrays.commands)) ||
	    (scene_geometry->arrays.index_count && (!scene_geometry->arrays.indices ||
			!item_geometry->arrays.indices)))
	return 0;
    for (size_t i = 0; i < scene_geometry->arrays.point_count; i++) {
	if (!VNEAR_EQUAL(scene_geometry->arrays.points[i],
		    item_geometry->arrays.points[i], SMALL_FASTF))
	    return 0;
    }
    for (size_t i = 0; i < scene_geometry->arrays.normal_count; i++) {
	if (!VNEAR_EQUAL(scene_geometry->arrays.normals[i],
		    item_geometry->arrays.normals[i], SMALL_FASTF))
	    return 0;
    }
    if (scene_geometry->arrays.command_count &&
	    memcmp(scene_geometry->arrays.commands, item_geometry->arrays.commands,
		scene_geometry->arrays.command_count * sizeof(int)) != 0)
	return 0;
    if (scene_geometry->arrays.index_count &&
	    memcmp(scene_geometry->arrays.indices, item_geometry->arrays.indices,
		scene_geometry->arrays.index_count * sizeof(int)) != 0)
	return 0;
    if (scene_geometry->surface.point_count != item_geometry->surface.point_count ||
	    scene_geometry->surface.normal_count != item_geometry->surface.normal_count ||
	    scene_geometry->surface.index_count != item_geometry->surface.index_count ||
	    scene_geometry->surface.face_count != item_geometry->surface.face_count ||
	    scene_geometry->surface.normal_kind != item_geometry->surface.normal_kind ||
	    scene_geometry->surface.material_valid != item_geometry->surface.material_valid)
	return 0;
    if ((scene_geometry->surface.point_count && (!scene_geometry->surface.points ||
			!item_geometry->surface.points)) ||
	    (scene_geometry->surface.normal_count && (!scene_geometry->surface.normals ||
			!item_geometry->surface.normals)) ||
	    (scene_geometry->surface.index_count && (!scene_geometry->surface.indices ||
			!item_geometry->surface.indices)))
	return 0;
    for (size_t i = 0; i < scene_geometry->surface.point_count; i++) {
	if (!VNEAR_EQUAL(scene_geometry->surface.points[i],
		    item_geometry->surface.points[i], SMALL_FASTF))
	    return 0;
    }
    for (size_t i = 0; i < scene_geometry->surface.normal_count; i++) {
	if (!VNEAR_EQUAL(scene_geometry->surface.normals[i],
		    item_geometry->surface.normals[i], SMALL_FASTF))
	    return 0;
    }
    if (scene_geometry->surface.index_count &&
	    memcmp(scene_geometry->surface.indices, item_geometry->surface.indices,
		scene_geometry->surface.index_count * sizeof(int)) != 0)
	return 0;
    if (scene_geometry->surface.material_valid) {
	if (memcmp(scene_geometry->surface.material.color,
		    item_geometry->surface.material.color,
		    sizeof(scene_geometry->surface.material.color)) != 0 ||
		!NEAR_EQUAL(scene_geometry->surface.material.transparency,
		    item_geometry->surface.material.transparency, SMALL_FASTF) ||
		scene_geometry->surface.material.draw_mode != item_geometry->surface.material.draw_mode ||
		scene_geometry->surface.material.line_width != item_geometry->surface.material.line_width ||
		scene_geometry->surface.material.line_style != item_geometry->surface.material.line_style ||
		scene_geometry->surface.material.evaluated_region != item_geometry->surface.material.evaluated_region ||
		scene_geometry->surface.material.highlighted != item_geometry->surface.material.highlighted ||
		scene_geometry->surface.material.active_layers != item_geometry->surface.material.active_layers ||
		scene_geometry->surface.material.material_rev != item_geometry->surface.material.material_rev ||
		scene_geometry->surface.material.appearance_rev != item_geometry->surface.material.appearance_rev)
	    return 0;
    }
    if (scene_geometry->proxy.kind != item_geometry->proxy.kind ||
	    scene_geometry->proxy.source_identity != item_geometry->proxy.source_identity ||
	    scene_geometry->proxy.revision != item_geometry->proxy.revision)
	return 0;
    if (scene_geometry->image.kind != item_geometry->image.kind ||
	    scene_geometry->image.width != item_geometry->image.width ||
	    scene_geometry->image.height != item_geometry->image.height ||
	    scene_geometry->image.channels != item_geometry->image.channels ||
	    scene_geometry->image.pixel_count != item_geometry->image.pixel_count ||
	    scene_geometry->image.framebuffer_mode != item_geometry->image.framebuffer_mode)
	return 0;
    if (scene_geometry->image.pixel_count &&
	    (!scene_geometry->image.pixels || !item_geometry->image.pixels))
	return 0;
    if (scene_geometry->image.pixel_count &&
	    memcmp(scene_geometry->image.pixels, item_geometry->image.pixels,
		scene_geometry->image.pixel_count) != 0)
	return 0;
    if (!!scene_geometry->annotation.text != !!item_geometry->annotation.text)
	return 0;
    if (scene_geometry->annotation.text &&
	    !BU_STR_EQUAL(scene_geometry->annotation.text,
		item_geometry->annotation.text))
	return 0;
    if (scene_geometry->annotation.space != item_geometry->annotation.space)
	return 0;
    if (!VNEAR_EQUAL(scene_geometry->annotation.anchor,
		item_geometry->annotation.anchor, SMALL_FASTF))
	return 0;
    if (memcmp(scene_geometry->annotation.model_mat,
		item_geometry->annotation.model_mat, sizeof(mat_t)) != 0)
	return 0;
    if (memcmp(scene_geometry->annotation.display_mat,
		item_geometry->annotation.display_mat, sizeof(mat_t)) != 0)
	return 0;
    if (scene_geometry->annotation.point_count !=
	    item_geometry->annotation.point_count)
	return 0;
    if (scene_geometry->annotation.point_count &&
	    (!scene_geometry->annotation.points ||
		!item_geometry->annotation.points))
	return 0;
    for (size_t i = 0; i < scene_geometry->annotation.point_count; i++) {
	if (!VNEAR_EQUAL(scene_geometry->annotation.points[i],
		    item_geometry->annotation.points[i], SMALL_FASTF))
	    return 0;
    }
    if (scene_geometry->annotation.segment_count !=
	    item_geometry->annotation.segment_count)
	return 0;
    if (scene_geometry->annotation.segment_count &&
	    (!scene_geometry->annotation.segments ||
		!item_geometry->annotation.segments))
	return 0;
    for (size_t i = 0; i < scene_geometry->annotation.segment_count; i++) {
	if (!_annotation_segment_equal(&scene_geometry->annotation.segments[i],
		    &item_geometry->annotation.segments[i]))
	    return 0;
    }
    return 1;
}

static struct bsg_backend_scene_node *
_scene_find_mutable(struct bsg_backend_scene *scene, uint64_t cache_identity)
{
    if (!scene || !cache_identity)
	return NULL;
    for (struct bsg_backend_scene_node *n = scene->nodes; n; n = n->next) {
	if (n->cache_identity == cache_identity)
	    return n;
    }
    return NULL;
}

static int
_node_matches_item(const struct bsg_backend_scene_node *n,
		   const struct bsg_render_item *item)
{
    if (!n || !item)
	return 0;
    if (n->source_identity != _source_identity_from_item(item))
	return 0;
    if (n->payload_revision != item->payload_revision)
	return 0;
    if (n->geometry_revision != item->geometry.revision)
	return 0;
    if (n->geometry.kind != item->geometry.kind)
	return 0;
    if (n->source.geometry_id != item->source.geometry_id ||
	    n->source.lod_id != item->source.lod_id ||
	    n->source.lod_policy_id != item->source.lod_policy_id ||
	    n->source.scope != item->source.scope ||
	    n->source.geometry_role != item->source.geometry_role ||
	    n->source.draw_intent != item->source.draw_intent ||
	    n->source.feature_family != item->source.feature_family ||
	    n->source.overlay_role != item->source.overlay_role ||
	    n->source.overlay_class != item->source.overlay_class ||
	    n->source.non_database_source != item->non_database_source ||
	    n->source.graph_depth != item->graph_depth ||
	    n->source.sequence != item->sequence ||
	    n->source.order_flags != item->order_flags)
	return 0;
    if (n->geometry.source_identity != item->geometry.source_identity)
	return 0;
    if (n->geometry.revision != item->geometry.revision)
	return 0;
    if (n->geometry.element_count != item->geometry.element_count)
	return 0;
    if (!_geometry_arrays_match(&n->geometry, &item->geometry))
	return 0;
    if (n->geometry.realization_kind != item->realization_kind ||
	    n->geometry.realization_status != item->realization_status ||
	    n->geometry.stale_reason != item->stale_reason)
	return 0;
    if (n->features.flags != item->features.flags ||
	    n->features.hud_feature_type != item->features.hud_feature_type ||
	    !NEAR_EQUAL(n->features.arrow_tip_length, item->features.arrow_tip_length, SMALL_FASTF) ||
	    !NEAR_EQUAL(n->features.arrow_tip_width, item->features.arrow_tip_width, SMALL_FASTF))
	return 0;
    if (!_text_record_matches(&n->text, item))
	return 0;
    if (!_overlay_record_matches(&n->overlay, item))
	return 0;
    if (n->selection.visible != item->visible ||
	    n->selection.force_draw != item->force_draw ||
	    n->selection.highlighted != item->highlighted ||
	    n->selection.selected != item->selected)
	return 0;
    if (n->lod.level != item->lod_level ||
	    n->lod.level_count != item->lod_level_count ||
	    n->lod.identity != _lod_identity_from_item(item))
	return 0;
    if (n->settings_hash != item->context.settings_hash ||
	    _semantic_request_flags(n->request_flags) !=
	    _semantic_request_flags(item->context.request_flags))
	return 0;
    if (item->context.backend_capabilities &&
	    n->backend_capabilities != item->context.backend_capabilities)
	return 0;
    if (n->capability_gaps != _capability_gaps(item, n->backend_capabilities))
	return 0;
    if (!NEAR_EQUAL(n->transform.bounds_radius, item->bounds_radius, SMALL_FASTF))
	return 0;
    if (!VNEAR_EQUAL(n->transform.bounds_center, item->bounds_center, SMALL_FASTF))
	return 0;
    if (memcmp(n->transform.model, item->model_mat, sizeof(mat_t)) != 0)
	return 0;
    if (memcmp(n->material.color, item->appearance.color,
		sizeof(n->material.color)) != 0)
	return 0;
    if (!NEAR_EQUAL(n->material.transparency,
		item->appearance.transparency, SMALL_FASTF))
	return 0;
    if (n->material.draw_mode != item->appearance.draw_mode ||
	    n->material.line_width != item->appearance.line_width ||
	    n->material.line_style != item->appearance.line_style ||
	    n->material.evaluated_region != item->appearance.evaluated_region ||
	    n->material.active_layers != item->appearance.active_layers ||
	    n->material.material_revision != item->appearance.material_rev ||
	    n->material.appearance_revision != item->appearance.appearance_rev)
	return 0;
    return 1;
}

static void
_node_copy_item(struct bsg_backend_scene_node *n,
		const struct bsg_render_item *item)
{
    n->cache_identity = item->cache_identity;
    n->source_identity = _source_identity_from_item(item);
    n->payload_revision = item->payload_revision;
    n->geometry_revision = item->geometry.revision;
    n->scene_kind = _scene_kind_from_item(item);
    n->source.geometry_id = item->source.geometry_id;
    n->source.lod_id = item->source.lod_id;
    n->source.lod_policy_id = item->source.lod_policy_id;
    n->source.scope = item->source.scope;
    n->source.geometry_role = item->source.geometry_role;
    n->source.draw_intent = item->source.draw_intent;
    n->source.feature_family = item->source.feature_family;
    n->source.overlay_role = item->source.overlay_role;
    n->source.overlay_class = item->source.overlay_class;
    n->source.non_database_source = item->non_database_source;
    n->source.graph_depth = item->graph_depth;
    n->source.sequence = item->sequence;
    n->source.order_flags = item->order_flags;
    n->geometry.kind = item->geometry.kind;
    n->geometry.source_identity = item->geometry.source_identity;
    n->geometry.revision = item->geometry.revision;
    n->geometry.element_count = item->geometry.element_count;
    (void)_geometry_arrays_copy(&n->geometry, &item->geometry);
    n->geometry.realization_kind = item->realization_kind;
    n->geometry.realization_status = item->realization_status;
    n->geometry.stale_reason = item->stale_reason;
    n->features.flags = item->features.flags;
    n->features.arrow_tip_length = item->features.arrow_tip_length;
    n->features.arrow_tip_width = item->features.arrow_tip_width;
    n->features.hud_feature_type = item->features.hud_feature_type;
    memcpy(n->material.color, item->appearance.color,
	    sizeof(n->material.color));
    n->material.transparency = item->appearance.transparency;
    n->material.draw_mode = item->appearance.draw_mode;
    n->material.line_width = item->appearance.line_width;
    n->material.line_style = item->appearance.line_style;
    n->material.evaluated_region = item->appearance.evaluated_region;
    n->material.active_layers = item->appearance.active_layers;
    n->material.material_revision = item->appearance.material_rev;
    n->material.appearance_revision = item->appearance.appearance_rev;
    MAT_COPY(n->transform.model, item->model_mat);
    VMOVE(n->transform.bounds_center, item->bounds_center);
    n->transform.bounds_radius = item->bounds_radius;
    _text_record_copy(&n->text, item);
    n->overlay.overlay_pass = item->context.overlay_pass;
    n->overlay.hud_pass = item->context.hud_pass;
    n->overlay.phase = item->phase;
    n->overlay.sort_key = item->sort_key;
    n->overlay.geometry_kind = item->geometry.overlay.kind;
    n->overlay.axes = item->geometry.overlay.axes;
    n->overlay.grid = item->geometry.overlay.grid;
    n->selection.visible = item->visible;
    n->selection.force_draw = item->force_draw;
    n->selection.highlighted = item->highlighted;
    n->selection.selected = item->selected;
    n->lod.level = item->lod_level;
    n->lod.level_count = item->lod_level_count;
    n->lod.identity = _lod_identity_from_item(item);
    n->settings_hash = item->context.settings_hash;
    n->request_flags = item->context.request_flags;
    n->backend_capabilities = item->context.backend_capabilities;
    n->capability_gaps = _capability_gaps(item, n->backend_capabilities);
}

static void
_scene_copy_frame(struct bsg_backend_scene *scene,
		  const struct bsg_render_context *ctx)
{
    if (!scene || !ctx)
	return;
    MAT_COPY(scene->camera.model2view, ctx->model2view);
    MAT_COPY(scene->camera.view2model, ctx->view2model);
    MAT_COPY(scene->camera.projection, ctx->projection);
    scene->camera.has_projection = ctx->has_projection;
    scene->camera.viewport_width = ctx->viewport_width;
    scene->camera.viewport_height = ctx->viewport_height;
    memcpy(scene->camera.background1, ctx->background1,
	    sizeof(scene->camera.background1));
    memcpy(scene->camera.background2, ctx->background2,
	    sizeof(scene->camera.background2));
    VMOVE(scene->clip.min, ctx->clipmin);
    VMOVE(scene->clip.max, ctx->clipmax);
    scene->clip.enabled = ctx->zclip;
    scene->lights.enabled = ctx->lighting;
    scene->lights.count = ctx->lighting ? 1 : 0;
}

static struct bsg_backend_scene_node *
_scene_upsert(struct bsg_backend_scene *scene,
	      const struct bsg_render_item *item)
{
    if (!scene || !item || !item->cache_identity)
	return NULL;

    _scene_copy_frame(scene, &item->context);

    struct bsg_backend_scene_node *n =
	_scene_find_mutable(scene, item->cache_identity);
    if (!n) {
	BU_GET(n, struct bsg_backend_scene_node);
	memset(n, 0, sizeof(*n));
	n->next = scene->nodes;
	scene->nodes = n;
	scene->stats.node_count++;
	scene->stats.created++;
	_node_copy_item(n, item);
	n->update_count = 1;
    } else if (n->stale || !_node_matches_item(n, item)) {
	_node_copy_item(n, item);
	n->update_count++;
	n->stale = 0;
	scene->stats.updated++;
    } else {
	scene->stats.reused++;
    }

    n->last_seen_generation = scene->stats.generation;
    if (n->capability_gaps)
	scene->stats.capability_gaps++;
    return n;
}

static int
_adapter_begin(void *ctx, struct bsg_render_request *UNUSED(req))
{
    struct bsg_backend_scene *scene = (struct bsg_backend_scene *)ctx;
    if (!scene)
	return 0;
    scene->stats.generation++;
    scene->stats.created = 0;
    scene->stats.updated = 0;
    scene->stats.reused = 0;
    scene->stats.removed = 0;
    scene->stats.drawn = 0;
    scene->stats.capability_gaps = 0;
    return 1;
}

static int
_adapter_prepare(void *ctx, const struct bsg_render_item *item)
{
    return _scene_upsert((struct bsg_backend_scene *)ctx, item) ? 1 : 0;
}

static void
_adapter_draw(void *ctx, const struct bsg_render_item *item)
{
    struct bsg_backend_scene *scene = (struct bsg_backend_scene *)ctx;
    struct bsg_backend_scene_node *n = _scene_find_mutable(scene,
	    item ? item->cache_identity : 0);
    if (!n)
	n = _scene_upsert(scene, item);
    if (!n)
	return;
    n->draw_count++;
    scene->stats.drawn++;
}

static void
_adapter_invalidate(void *ctx, const struct bsg_render_item *item,
		    unsigned int reason_mask)
{
    bsg_backend_scene_invalidate_item((struct bsg_backend_scene *)ctx,
	    item, reason_mask);
}

static void
_adapter_free(void *ctx, const struct bsg_render_item *item)
{
    if (!item)
	return;
    bsg_backend_scene_release_source((struct bsg_backend_scene *)ctx,
	    _source_identity_from_item(item));
}

static unsigned int
_adapter_capabilities(void *ctx)
{
    struct bsg_backend_scene *scene = (struct bsg_backend_scene *)ctx;
    return scene ? scene->capabilities : 0;
}

static void
_adapter_end(void *ctx, const struct bsg_render_request *UNUSED(req))
{
    struct bsg_backend_scene *scene = (struct bsg_backend_scene *)ctx;
    if (!scene)
	return;
    struct bsg_backend_scene_node **prev = &scene->nodes;
    struct bsg_backend_scene_node *n = scene->nodes;
    while (n) {
	struct bsg_backend_scene_node *next = n->next;
	if (n->last_seen_generation != scene->stats.generation) {
	    *prev = next;
	    _node_free(n);
	    scene->stats.node_count--;
	    scene->stats.removed++;
	} else {
	    prev = &n->next;
	}
	n = next;
    }
}

struct bsg_backend_scene *
bsg_backend_scene_create_with_capabilities(unsigned int capabilities)
{
    struct bsg_backend_scene *scene;
    BU_GET(scene, struct bsg_backend_scene);
    memset(scene, 0, sizeof(*scene));
    scene->capabilities = capabilities;
    scene->adapter.begin_request = _adapter_begin;
    scene->adapter.prepare = _adapter_prepare;
    scene->adapter.draw = _adapter_draw;
    scene->adapter.invalidate = _adapter_invalidate;
    scene->adapter.free = _adapter_free;
    scene->adapter.capabilities = _adapter_capabilities;
    scene->adapter.end_request = _adapter_end;
    return scene;
}

struct bsg_backend_scene *
bsg_backend_scene_create(void)
{
    return bsg_backend_scene_create_with_capabilities(
	    BSG_BACKEND_SCENE_DEFAULT_CAPABILITIES);
}

void
bsg_backend_scene_destroy(struct bsg_backend_scene *scene)
{
    if (!scene)
	return;
    struct bsg_backend_scene_node *n = scene->nodes;
    while (n) {
	struct bsg_backend_scene_node *next = n->next;
	_node_free(n);
	n = next;
    }
    BU_PUT(scene, struct bsg_backend_scene);
}

struct bsg_backend_adapter *
bsg_backend_scene_adapter(struct bsg_backend_scene *scene)
{
    return scene ? &scene->adapter : NULL;
}

struct bsg_backend_adapter *
bsg_backend_scene_select_adapter(bsg_backend_scene_adapter_kind kind,
				 struct bsg_backend_scene *scene,
				 struct bsg_backend_adapter *external,
				 struct bu_vls *diagnostics)
{
    switch (kind) {
	case BSG_BACKEND_SCENE_ADAPTER_EXTERNAL:
	    if (!external && diagnostics)
		bu_vls_printf(diagnostics, "backend-select external: no adapter supplied\n");
	    return external;
	case BSG_BACKEND_SCENE_ADAPTER_RETAINED_REFERENCE:
	    if (!scene) {
		if (diagnostics)
		    bu_vls_printf(diagnostics, "backend-select retained-reference: no retained scene supplied\n");
		return NULL;
	    }
	    return bsg_backend_scene_adapter(scene);
	case BSG_BACKEND_SCENE_ADAPTER_OBOL_RESERVED:
	    if (diagnostics)
		bu_vls_printf(diagnostics, "backend-select obol: adapter boundary reserved; supply an external Obol adapter consuming render batches or retained-scene records\n");
	    return NULL;
	default:
	    if (diagnostics)
		bu_vls_printf(diagnostics, "backend-select unknown kind=%d\n", (int)kind);
	    return NULL;
    }
}

const struct bsg_backend_scene_node *
bsg_backend_scene_find(const struct bsg_backend_scene *scene,
		       uint64_t cache_identity)
{
    if (!scene || !cache_identity)
	return NULL;
    for (const struct bsg_backend_scene_node *n = scene->nodes; n; n = n->next) {
	if (n->cache_identity == cache_identity)
	    return n;
    }
    return NULL;
}

int
bsg_backend_scene_foreach_node(const struct bsg_backend_scene *scene,
			       bsg_backend_scene_node_cb cb,
			       void *userdata)
{
    if (!scene || !cb)
	return 0;
    int visited = 0;
    for (const struct bsg_backend_scene_node *n = scene->nodes; n; n = n->next) {
	if (!cb(n, userdata))
	    break;
	visited++;
    }
    return visited;
}

const struct bsg_backend_scene_camera *
bsg_backend_scene_get_camera(const struct bsg_backend_scene *scene)
{
    return scene ? &scene->camera : NULL;
}

const struct bsg_backend_scene_clip *
bsg_backend_scene_get_clip(const struct bsg_backend_scene *scene)
{
    return scene ? &scene->clip : NULL;
}

const struct bsg_backend_scene_lights *
bsg_backend_scene_get_lights(const struct bsg_backend_scene *scene)
{
    return scene ? &scene->lights : NULL;
}

size_t
bsg_backend_scene_count(const struct bsg_backend_scene *scene)
{
    return scene ? scene->stats.node_count : 0;
}

void
bsg_backend_scene_get_stats(const struct bsg_backend_scene *scene,
			    struct bsg_backend_scene_stats *stats)
{
    if (!stats)
	return;
    memset(stats, 0, sizeof(*stats));
    if (scene)
	*stats = scene->stats;
}

void
bsg_backend_scene_invalidate_item(struct bsg_backend_scene *scene,
				  const struct bsg_render_item *item,
				  unsigned int UNUSED(reason_mask))
{
    if (!scene || !item)
	return;
    struct bsg_backend_scene_node *n =
	_scene_find_mutable(scene, item->cache_identity);
    if (n)
	n->stale = 1;
}

void
bsg_backend_scene_release_source(struct bsg_backend_scene *scene,
				 uint64_t source_identity)
{
    if (!scene || !source_identity)
	return;
    struct bsg_backend_scene_node **prev = &scene->nodes;
    struct bsg_backend_scene_node *n = scene->nodes;
    while (n) {
	struct bsg_backend_scene_node *next = n->next;
	if (n->source_identity == source_identity) {
	    *prev = next;
	    _node_free(n);
	    scene->stats.node_count--;
	    scene->stats.removed++;
	} else {
	    prev = &n->next;
	}
	n = next;
    }
}

unsigned int
bsg_backend_scene_capabilities(const struct bsg_backend_scene *scene)
{
    return scene ? scene->capabilities : 0;
}

void
bsg_backend_scene_set_capabilities(struct bsg_backend_scene *scene,
				   unsigned int capabilities)
{
    if (scene)
	scene->capabilities = capabilities;
}

int
bsg_backend_scene_render_request(struct bsg_view *view,
				 struct bsg_backend_scene *scene,
				 unsigned int flags)
{
    return bsg_backend_scene_update(scene, view, flags, NULL, NULL);
}

struct backend_scene_action_data {
    struct bsg_backend_scene *scene;
    struct bsg_view *view;
    unsigned int flags;
    int owns_scene;
    struct bsg_backend_scene_update_result result_storage;
    struct bsg_backend_scene_update_result *result;
    struct bu_vls *diagnostics;
};

static int
_backend_scene_action_apply(bsg_action_ref UNUSED(action),
			    bsg_node_ref UNUSED(root),
			    void *data)
{
    struct backend_scene_action_data *st = (struct backend_scene_action_data *)data;
    struct bsg_backend_scene *scene = st ? st->scene : NULL;
    struct bsg_view *view = st ? st->view : NULL;
    unsigned int flags = st ? st->flags : 0;
    struct bsg_backend_scene_update_result *result = st ? st->result : NULL;
    struct bu_vls *diagnostics = st ? st->diagnostics : NULL;

    if (result)
	memset(result, 0, sizeof(*result));
    if (!scene) {
	if (diagnostics)
	    bu_vls_printf(diagnostics, "retained-update: no retained scene supplied\n");
	return -1;
    }
    struct bsg_render_request *req =
	bsg_render_request_create(view, scene);
    if (!req) {
	if (diagnostics)
	    bu_vls_printf(diagnostics, "retained-update: render request create failed\n");
	return -1;
    }
    bsg_render_request_set_adapter(req, bsg_backend_scene_adapter(scene));
    bsg_render_request_set_flags(req, flags ? flags : BSG_RENDER_FLAG_VISIBLE_ONLY);
    struct bsg_render_batch *batch = bsg_render_batch_create();
    int ret = -1;
    if (batch) {
	ret = bsg_render_request_collect(req, batch);
	if (ret >= 0)
	    ret = bsg_render_batch_dispatch(req, batch);
	bsg_render_batch_destroy(batch);
    } else if (diagnostics) {
	bu_vls_printf(diagnostics, "retained-update: render batch create failed\n");
    }
    bsg_render_request_destroy(req);

    if (result) {
	result->rendered = ret;
	result->scene = scene;
	bsg_backend_scene_get_stats(scene, &result->stats);
	result->capabilities = bsg_backend_scene_capabilities(scene);
	result->capability_gaps = result->stats.capability_gaps;
    }
    if (diagnostics)
	bsg_backend_scene_diagnostics(scene, diagnostics);
    return ret;
}

static void
_backend_scene_action_destroy(bsg_action_ref UNUSED(action), void *data)
{
    struct backend_scene_action_data *st = (struct backend_scene_action_data *)data;
    if (!st)
	return;
    if (st->owns_scene && st->scene)
	bsg_backend_scene_destroy(st->scene);
    bu_free(st, "backend scene action data");
}

bsg_action_ref
bsg_backend_scene_action_create(struct bsg_backend_scene *scene,
				struct bsg_view *view,
				unsigned int flags,
				struct bsg_backend_scene_update_result *result,
				struct bu_vls *diagnostics)
{
    struct backend_scene_action_data *st;
    BU_ALLOC(st, struct backend_scene_action_data);
    memset(st, 0, sizeof(*st));
    st->scene = scene ? scene : bsg_backend_scene_create();
    st->owns_scene = scene ? 0 : 1;
    st->view = view;
    st->flags = flags;
    st->result = result ? result : &st->result_storage;
    st->diagnostics = diagnostics;
    return bsg_action_ref_create_internal(bsg_backend_scene_action_type(),
	    _backend_scene_action_apply, _backend_scene_action_destroy, st,
	    "backend scene action");
}

struct bsg_backend_scene *
bsg_backend_scene_action_result(bsg_action_ref action,
				struct bsg_backend_scene_update_result *result)
{
    if (!bsg_type_is_a(bsg_action_ref_type(action),
		bsg_backend_scene_action_type())) {
	if (result)
	    memset(result, 0, sizeof(*result));
	return NULL;
    }

    struct backend_scene_action_data *st =
	(struct backend_scene_action_data *)bsg_action_ref_data(action);
    if (!st) {
	if (result)
	    memset(result, 0, sizeof(*result));
	return NULL;
    }

    if (result)
	*result = st->result ? *st->result : st->result_storage;

    struct bsg_backend_scene *scene = st->scene;
    if (st->owns_scene) {
	st->scene = NULL;
	st->owns_scene = 0;
	if (result)
	    result->scene = scene;
    }
    return scene;
}

int
bsg_backend_scene_update(struct bsg_backend_scene *scene,
			 struct bsg_view *view,
			 unsigned int flags,
			 struct bsg_backend_scene_update_result *result,
			 struct bu_vls *diagnostics)
{
    if (!scene) {
	if (result)
	    memset(result, 0, sizeof(*result));
	if (diagnostics)
	    bu_vls_printf(diagnostics, "retained-update: no retained scene supplied\n");
	return -1;
    }
    bsg_action_ref action = bsg_backend_scene_action_create(scene, view, flags,
	    NULL, diagnostics);
    int ret = bsg_action_apply(action, bsg_node_ref_null());
    (void)bsg_backend_scene_action_result(action, result);
    bsg_action_ref_destroy(action);
    return ret;
}

static void
_report_gap(struct bu_vls *report, uint64_t cache, unsigned int gaps)
{
    if (!report || !gaps)
	return;
    bu_vls_printf(report, "capability-gap cache=%llu mask=0x%x",
	    (unsigned long long)cache, gaps);
    if (gaps & BSG_BACKEND_SCENE_GAP_TRANSPARENCY)
	bu_vls_printf(report, " transparency");
    if (gaps & BSG_BACKEND_SCENE_GAP_WIREFRAME)
	bu_vls_printf(report, " wireframe");
    if (gaps & BSG_BACKEND_SCENE_GAP_SHADED)
	bu_vls_printf(report, " shaded");
    if (gaps & BSG_BACKEND_SCENE_GAP_HUD)
	bu_vls_printf(report, " hud");
    if (gaps & BSG_BACKEND_SCENE_GAP_SORTED_ALPHA)
	bu_vls_printf(report, " sorted-alpha");
    if (gaps & BSG_BACKEND_SCENE_GAP_BREP)
	bu_vls_printf(report, " brep");
    bu_vls_printf(report, "\n");
}

static void
_compare_one(const struct bsg_backend_scene_node *n,
	     const struct bsg_render_item *item,
	     int *mismatches,
	     struct bu_vls *report)
{
    if (_node_matches_item(n, item))
	return;

#define CMP_FIELD(expr, label, fmt, a, b) \
    do { \
	if (expr) { \
	    (*mismatches)++; \
	    if (report) \
		bu_vls_printf(report, "mismatch cache=%llu field=%s scene=" fmt " item=" fmt "\n", \
			(unsigned long long)item->cache_identity, label, a, b); \
	} \
    } while (0)

    uint64_t item_source = _source_identity_from_item(item);
    CMP_FIELD(n->source_identity != item_source, "source_identity", "%llu",
	    (unsigned long long)n->source_identity,
	    (unsigned long long)item_source);
    CMP_FIELD(n->source.geometry_id != item->source.geometry_id, "source.geometry_id", "%llu",
	    (unsigned long long)n->source.geometry_id,
	    (unsigned long long)item->source.geometry_id);
	CMP_FIELD(n->source.lod_id != item->source.lod_id, "source.lod_id", "%llu",
		(unsigned long long)n->source.lod_id,
		(unsigned long long)item->source.lod_id);
	CMP_FIELD(n->source.lod_policy_id != item->source.lod_policy_id, "source.lod_policy_id", "%llu",
		(unsigned long long)n->source.lod_policy_id,
		(unsigned long long)item->source.lod_policy_id);
    CMP_FIELD(n->source.scope != item->source.scope, "source.scope", "%d",
	    (int)n->source.scope, (int)item->source.scope);
    CMP_FIELD(n->source.geometry_role != item->source.geometry_role, "source.geometry_role", "%d",
	    (int)n->source.geometry_role, (int)item->source.geometry_role);
    CMP_FIELD(n->source.draw_intent != item->source.draw_intent, "source.draw_intent", "%d",
	    (int)n->source.draw_intent, (int)item->source.draw_intent);
    CMP_FIELD(n->source.feature_family != item->source.feature_family, "source.feature_family", "%d",
	    (int)n->source.feature_family, (int)item->source.feature_family);
    CMP_FIELD(n->source.overlay_role != item->source.overlay_role, "source.overlay_role", "%d",
	    (int)n->source.overlay_role, (int)item->source.overlay_role);
    CMP_FIELD(n->source.overlay_class != item->source.overlay_class, "source.overlay_class", "%d",
	    (int)n->source.overlay_class, (int)item->source.overlay_class);
    CMP_FIELD(n->source.non_database_source != item->non_database_source, "source.non_database_source", "%d",
	    n->source.non_database_source, item->non_database_source);
    CMP_FIELD(n->source.graph_depth != item->graph_depth, "source.graph_depth", "%d",
	    n->source.graph_depth, item->graph_depth);
    CMP_FIELD(n->source.sequence != item->sequence, "source.sequence", "%zu",
	    n->source.sequence, item->sequence);
    CMP_FIELD(n->source.order_flags != item->order_flags, "source.order_flags", "%u",
	    n->source.order_flags, item->order_flags);
    CMP_FIELD(n->overlay.phase != item->phase, "phase", "%d",
	    (int)n->overlay.phase, (int)item->phase);
    CMP_FIELD(n->geometry.kind != item->geometry.kind, "geometry.kind", "%d",
	    (int)n->geometry.kind, (int)item->geometry.kind);
    CMP_FIELD(n->geometry.source_identity != item->geometry.source_identity,
	    "geometry.source_identity", "%llu",
	    (unsigned long long)n->geometry.source_identity,
	    (unsigned long long)item->geometry.source_identity);
    CMP_FIELD(n->geometry.revision != item->geometry.revision,
	    "geometry.revision", "%llu",
	    (unsigned long long)n->geometry.revision,
	    (unsigned long long)item->geometry.revision);
    CMP_FIELD(n->geometry.element_count != item->geometry.element_count,
	    "geometry.element_count", "%zu",
	    n->geometry.element_count, item->geometry.element_count);
    CMP_FIELD(n->geometry.arrays.point_count != item->geometry.arrays.point_count,
	    "geometry.arrays.point_count", "%zu",
	    n->geometry.arrays.point_count, item->geometry.arrays.point_count);
    CMP_FIELD(n->geometry.arrays.normal_count != item->geometry.arrays.normal_count,
	    "geometry.arrays.normal_count", "%zu",
	    n->geometry.arrays.normal_count, item->geometry.arrays.normal_count);
    CMP_FIELD(n->geometry.arrays.command_count != item->geometry.arrays.command_count,
	    "geometry.arrays.command_count", "%zu",
	    n->geometry.arrays.command_count, item->geometry.arrays.command_count);
    CMP_FIELD(n->geometry.arrays.index_count != item->geometry.arrays.index_count,
	    "geometry.arrays.index_count", "%zu",
	    n->geometry.arrays.index_count, item->geometry.arrays.index_count);
    CMP_FIELD(!_geometry_arrays_match(&n->geometry, &item->geometry),
	    "geometry.arrays.contents", "%d", 1, 0);
    CMP_FIELD(n->features.flags != item->features.flags,
	    "features.flags", "%u",
	    n->features.flags, item->features.flags);
    CMP_FIELD(!NEAR_EQUAL(n->features.arrow_tip_length,
		item->features.arrow_tip_length, SMALL_FASTF),
	    "features.arrow_tip_length", "%g",
	    n->features.arrow_tip_length, item->features.arrow_tip_length);
    CMP_FIELD(!NEAR_EQUAL(n->features.arrow_tip_width,
		item->features.arrow_tip_width, SMALL_FASTF),
	    "features.arrow_tip_width", "%g",
	    n->features.arrow_tip_width, item->features.arrow_tip_width);
    CMP_FIELD(n->features.hud_feature_type != item->features.hud_feature_type,
	    "features.hud_feature_type", "%d",
	    n->features.hud_feature_type, item->features.hud_feature_type);
    CMP_FIELD(memcmp(n->transform.model, item->model_mat, sizeof(mat_t)) != 0,
	    "transform.model", "%d", 1, 0);
    CMP_FIELD(memcmp(n->material.color, item->appearance.color,
		sizeof(n->material.color)) != 0, "material.color", "%d", 1, 0);
    CMP_FIELD(!NEAR_EQUAL(n->material.transparency,
		item->appearance.transparency, SMALL_FASTF),
	    "material.transparency", "%g",
	    n->material.transparency, item->appearance.transparency);
    CMP_FIELD(n->material.evaluated_region != item->appearance.evaluated_region,
	    "material.evaluated_region", "%d",
	    n->material.evaluated_region, item->appearance.evaluated_region);
    CMP_FIELD(n->selection.selected != item->selected, "selection.selected",
	    "%d", n->selection.selected, item->selected);
    CMP_FIELD(n->selection.highlighted != item->highlighted,
	    "selection.highlighted", "%d",
	    n->selection.highlighted, item->highlighted);
    CMP_FIELD(n->lod.level != item->lod_level, "lod.level", "%d",
	    n->lod.level, item->lod_level);
    CMP_FIELD(n->lod.level_count != item->lod_level_count,
	    "lod.level_count", "%d",
	    n->lod.level_count, item->lod_level_count);
    CMP_FIELD(n->lod.identity != _lod_identity_from_item(item),
	    "lod.identity", "%llu",
	    (unsigned long long)n->lod.identity,
	    (unsigned long long)_lod_identity_from_item(item));

#undef CMP_FIELD
}

int
bsg_backend_scene_compare_items(const struct bsg_backend_scene *scene,
				const struct bu_ptbl *items,
				struct bu_vls *report)
{
    if (!scene || !items)
	return -1;

    int mismatches = 0;
    size_t seen = 0;
    for (size_t i = 0; i < BU_PTBL_LEN(items); i++) {
	const struct bsg_render_item *item =
	    (const struct bsg_render_item *)BU_PTBL_GET(items, i);
	const struct bsg_backend_scene_node *n =
	    bsg_backend_scene_find(scene, item ? item->cache_identity : 0);
	if (!n) {
	    mismatches++;
	    if (report)
		bu_vls_printf(report, "missing cache=%llu\n",
			(unsigned long long)(item ? item->cache_identity : 0));
	    continue;
	}
	seen++;
	if (!_node_matches_item(n, item)) {
	    _compare_one(n, item, &mismatches, report);
	}
	if (n->capability_gaps) {
	    mismatches++;
	    _report_gap(report, item->cache_identity, n->capability_gaps);
	}
    }

    if (seen != scene->stats.node_count) {
	mismatches += (seen < scene->stats.node_count) ?
	    (int)(scene->stats.node_count - seen) :
	    (int)(seen - scene->stats.node_count);
	if (report)
	    bu_vls_printf(report, "count scene=%zu items=%zu\n",
		    scene->stats.node_count, BU_PTBL_LEN(items));
    }

    return mismatches;
}

void
bsg_backend_scene_diagnostics(const struct bsg_backend_scene *scene,
			      struct bu_vls *out)
{
    if (!out)
	return;
    if (!scene) {
	bu_vls_printf(out, "retained-scene: null\n");
	return;
    }

    bu_vls_printf(out,
	    "retained-scene generation=%llu nodes=%zu created=%zu updated=%zu reused=%zu removed=%zu drawn=%zu capabilities=0x%x capability_gaps=%zu\n",
	    (unsigned long long)scene->stats.generation,
	    scene->stats.node_count,
	    scene->stats.created,
	    scene->stats.updated,
	    scene->stats.reused,
	    scene->stats.removed,
	    scene->stats.drawn,
	    scene->capabilities,
	    scene->stats.capability_gaps);
    bu_vls_printf(out,
	    "retained-frame viewport=%dx%d projection=%d zclip=%d lights=%zu\n",
	    scene->camera.viewport_width,
	    scene->camera.viewport_height,
	    scene->camera.has_projection,
	    scene->clip.enabled,
	    scene->lights.count);

    for (const struct bsg_backend_scene_node *n = scene->nodes; n; n = n->next) {
	bu_vls_printf(out,
		"retained-node cache=%llu source=%llu kind=%d phase=%d geom=%d scope=%d role=%d intent=%d geom_rev=%llu payload_rev=%llu selected=%d highlighted=%d lod=%d/%d gaps=0x%x\n",
		(unsigned long long)n->cache_identity,
		(unsigned long long)n->source_identity,
		(int)n->scene_kind,
		(int)n->overlay.phase,
		(int)n->geometry.kind,
		(int)n->source.scope,
		(int)n->source.geometry_role,
		(int)n->source.draw_intent,
		(unsigned long long)n->geometry_revision,
		(unsigned long long)n->payload_revision,
		n->selection.selected,
		n->selection.highlighted,
		n->lod.level,
		n->lod.level_count,
		n->capability_gaps);
	if (n->capability_gaps)
	    _report_gap(out, n->cache_identity, n->capability_gaps);
    }
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
