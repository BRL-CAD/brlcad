/*                    E X P O R T . C
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
/** @file libbsg/export.c */

#include "common.h"

#include <string.h>

#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bsg/action.h"
#include "bsg/backend_adapter.h"
#include "bsg/backend_scene.h"
#include "bsg/export.h"
#include "bsg/feature.h"
#include "bsg/geometry.h"
#include "bsg/render.h"
#include "action_private.h"

static unsigned int
_export_capability_gaps(const struct bsg_render_item *item, unsigned int caps)
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

static unsigned int
_export_record_roles(const struct bsg_render_item *item)
{
    unsigned int roles = BSG_EXPORT_RECORD_SCENE;

    if (!item)
	return roles | BSG_EXPORT_RECORD_DIAGNOSTIC;

    switch (item->geometry.kind) {
	case BSG_RENDER_GEOMETRY_LINE_SET:
	case BSG_RENDER_GEOMETRY_INDEXED_LINE_SET:
	case BSG_RENDER_GEOMETRY_POINT_SET:
	case BSG_RENDER_GEOMETRY_INDEXED_FACE_SET:
	case BSG_RENDER_GEOMETRY_MESH:
	case BSG_RENDER_GEOMETRY_CSG_PROXY:
	case BSG_RENDER_GEOMETRY_BREP_PROXY:
	case BSG_RENDER_GEOMETRY_EDIT_PREVIEW:
	case BSG_RENDER_GEOMETRY_EXTERNAL_PROXY:
	    roles |= BSG_EXPORT_RECORD_GEOMETRY;
	    break;
	case BSG_RENDER_GEOMETRY_TEXT:
	    roles |= BSG_EXPORT_RECORD_GEOMETRY;
	    roles |= BSG_EXPORT_RECORD_LABEL;
	    break;
	case BSG_RENDER_GEOMETRY_IMAGE:
	    roles |= BSG_EXPORT_RECORD_GEOMETRY;
	    break;
	case BSG_RENDER_GEOMETRY_OVERLAY:
	case BSG_RENDER_GEOMETRY_HUD:
	case BSG_RENDER_GEOMETRY_ANNOTATION:
	    roles |= BSG_EXPORT_RECORD_GEOMETRY;
	    roles |= BSG_EXPORT_RECORD_ANNOTATION;
	    break;
	default:
	    break;
    }

    if (item->has_bounds)
	roles |= BSG_EXPORT_RECORD_BOUNDS;
    if (item->realization_status == BSG_REALIZE_STATUS_FAILED ||
	    item->stale_reason != BSG_PAYLOAD_STALE_NONE)
	roles |= BSG_EXPORT_RECORD_DIAGNOSTIC;

    return roles;
}

static void
_export_annotation_segment_clear(struct bsg_annotation_segment *seg)
{
    if (!seg)
	return;
    if (seg->kind == BSG_ANNOTATION_SEGMENT_NURB) {
	if (seg->data.nurb.knots)
	    bu_free(seg->data.nurb.knots, "bsg export annotation nurb knots");
	if (seg->data.nurb.control_points)
	    bu_free(seg->data.nurb.control_points, "bsg export annotation nurb control points");
	if (seg->data.nurb.weights)
	    bu_free(seg->data.nurb.weights, "bsg export annotation nurb weights");
    } else if (seg->kind == BSG_ANNOTATION_SEGMENT_BEZIER) {
	if (seg->data.bezier.control_points)
	    bu_free(seg->data.bezier.control_points, "bsg export annotation bezier control points");
    } else if (seg->kind == BSG_ANNOTATION_SEGMENT_TEXT) {
	if (seg->data.text.text)
	    bu_free(seg->data.text.text, "bsg export annotation text segment");
    }
}

static void
_export_annotation_segment_copy(struct bsg_annotation_segment *dst,
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
			sizeof(fastf_t), "bsg export annotation nurb knots");
		memcpy(dst->data.nurb.knots, src->data.nurb.knots,
			src->data.nurb.knot_count * sizeof(fastf_t));
	    }
	    if (src->data.nurb.control_point_count && src->data.nurb.control_points) {
		dst->data.nurb.control_points = (int *)bu_calloc(
			src->data.nurb.control_point_count, sizeof(int),
			"bsg export annotation nurb control points");
		memcpy(dst->data.nurb.control_points, src->data.nurb.control_points,
			src->data.nurb.control_point_count * sizeof(int));
	    }
	    if (src->data.nurb.control_point_count && src->data.nurb.weights) {
		dst->data.nurb.weights = (fastf_t *)bu_calloc(
			src->data.nurb.control_point_count, sizeof(fastf_t),
			"bsg export annotation nurb weights");
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
			"bsg export annotation bezier control points");
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

static void
_export_geometry_arrays_free(struct bsg_render_geometry *geometry)
{
    if (!geometry)
	return;
    if (geometry->arrays.points)
	bu_free(geometry->arrays.points, "bsg export geometry points");
    if (geometry->arrays.normals)
	bu_free(geometry->arrays.normals, "bsg export geometry normals");
    if (geometry->arrays.commands)
	bu_free(geometry->arrays.commands, "bsg export geometry commands");
    if (geometry->arrays.indices)
	bu_free(geometry->arrays.indices, "bsg export geometry indices");
    memset(&geometry->arrays, 0, sizeof(geometry->arrays));
    if (geometry->surface.points)
	bu_free(geometry->surface.points, "bsg export surface points");
    if (geometry->surface.normals)
	bu_free(geometry->surface.normals, "bsg export surface normals");
    if (geometry->surface.indices)
	bu_free(geometry->surface.indices, "bsg export surface indices");
    memset(&geometry->surface, 0, sizeof(geometry->surface));
    if (geometry->text.label)
	bu_free(geometry->text.label, "bsg export geometry text");
    memset(&geometry->text, 0, sizeof(geometry->text));
    if (geometry->image.pixels)
	bu_free(geometry->image.pixels, "bsg export image pixels");
    memset(&geometry->image, 0, sizeof(geometry->image));
    if (geometry->annotation.text)
	bu_free(geometry->annotation.text, "bsg export annotation text");
    if (geometry->annotation.points)
	bu_free(geometry->annotation.points, "bsg export annotation points");
    if (geometry->annotation.segments) {
	for (size_t i = 0; i < geometry->annotation.segment_count; i++)
	    _export_annotation_segment_clear(&geometry->annotation.segments[i]);
	bu_free(geometry->annotation.segments, "bsg export annotation segments");
    }
    memset(&geometry->annotation, 0, sizeof(geometry->annotation));
}

static int
_export_geometry_arrays_copy(struct bsg_render_geometry *dst,
			     const struct bsg_render_geometry *src)
{
    if (!dst || !src)
	return 0;

    memset(&dst->arrays, 0, sizeof(dst->arrays));
    memset(&dst->surface, 0, sizeof(dst->surface));
    memset(&dst->text, 0, sizeof(dst->text));
    memset(&dst->image, 0, sizeof(dst->image));
    memset(&dst->annotation, 0, sizeof(dst->annotation));

    if (src->arrays.point_count && src->arrays.points) {
	dst->arrays.points = (point_t *)bu_calloc(src->arrays.point_count,
		sizeof(point_t), "bsg export geometry points");
	for (size_t i = 0; i < src->arrays.point_count; i++)
	    VMOVE(dst->arrays.points[i], src->arrays.points[i]);
	dst->arrays.point_count = src->arrays.point_count;
    }
    if (src->arrays.normal_count && src->arrays.normals) {
	dst->arrays.normals = (point_t *)bu_calloc(src->arrays.normal_count,
		sizeof(point_t), "bsg export geometry normals");
	for (size_t i = 0; i < src->arrays.normal_count; i++)
	    VMOVE(dst->arrays.normals[i], src->arrays.normals[i]);
	dst->arrays.normal_count = src->arrays.normal_count;
    }
    if (src->arrays.command_count && src->arrays.commands) {
	dst->arrays.commands = (int *)bu_calloc(src->arrays.command_count,
		sizeof(int), "bsg export geometry commands");
	memcpy(dst->arrays.commands, src->arrays.commands,
		src->arrays.command_count * sizeof(int));
	dst->arrays.command_count = src->arrays.command_count;
    }
    if (src->arrays.index_count && src->arrays.indices) {
	dst->arrays.indices = (int *)bu_calloc(src->arrays.index_count,
		sizeof(int), "bsg export geometry indices");
	memcpy(dst->arrays.indices, src->arrays.indices,
		src->arrays.index_count * sizeof(int));
	dst->arrays.index_count = src->arrays.index_count;
    }
    if (src->surface.point_count && src->surface.points) {
	dst->surface.points = (point_t *)bu_calloc(src->surface.point_count,
		sizeof(point_t), "bsg export surface points");
	for (size_t i = 0; i < src->surface.point_count; i++)
	    VMOVE(dst->surface.points[i], src->surface.points[i]);
	dst->surface.point_count = src->surface.point_count;
    }
    if (src->surface.normal_count && src->surface.normals) {
	dst->surface.normals = (vect_t *)bu_calloc(src->surface.normal_count,
		sizeof(vect_t), "bsg export surface normals");
	for (size_t i = 0; i < src->surface.normal_count; i++)
	    VMOVE(dst->surface.normals[i], src->surface.normals[i]);
	dst->surface.normal_count = src->surface.normal_count;
    }
    if (src->surface.index_count && src->surface.indices) {
	dst->surface.indices = (int *)bu_calloc(src->surface.index_count,
		sizeof(int), "bsg export surface indices");
	memcpy(dst->surface.indices, src->surface.indices,
		src->surface.index_count * sizeof(int));
	dst->surface.index_count = src->surface.index_count;
    }
    dst->surface.face_count = src->surface.face_count;
    dst->surface.normal_kind = src->surface.normal_kind;
    dst->surface.material_valid = src->surface.material_valid;
    dst->surface.material = src->surface.material;
    if (src->text.label) {
	dst->text.label = bu_strdup(src->text.label);
	VMOVE(dst->text.position, src->text.position);
	VMOVE(dst->text.target, src->text.target);
	dst->text.size = src->text.size;
	dst->text.line_flag = src->text.line_flag;
	dst->text.anchor = src->text.anchor;
	dst->text.arrow = src->text.arrow;
    }
    dst->image.kind = src->image.kind;
    dst->image.width = src->image.width;
    dst->image.height = src->image.height;
    dst->image.channels = src->image.channels;
    dst->image.pixel_count = src->image.pixel_count;
    dst->image.framebuffer_mode = src->image.framebuffer_mode;
    if (src->image.pixel_count && src->image.pixels) {
	dst->image.pixels = (unsigned char *)bu_malloc(src->image.pixel_count,
		"bsg export image pixels");
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
		sizeof(point_t), "bsg export annotation points");
	for (size_t i = 0; i < src->annotation.point_count; i++)
	    VMOVE(dst->annotation.points[i], src->annotation.points[i]);
	dst->annotation.point_count = src->annotation.point_count;
    }
    if (src->annotation.segment_count && src->annotation.segments) {
	dst->annotation.segments = (struct bsg_annotation_segment *)bu_calloc(
		src->annotation.segment_count, sizeof(struct bsg_annotation_segment),
		"bsg export annotation segments");
	for (size_t i = 0; i < src->annotation.segment_count; i++)
	    _export_annotation_segment_copy(&dst->annotation.segments[i],
		    &src->annotation.segments[i]);
	dst->annotation.segment_count = src->annotation.segment_count;
    }

    return 1;
}

static int
_geometry_line_command(const struct bsg_render_geometry *geometry, size_t idx)
{
    if (geometry && idx < geometry->arrays.command_count &&
	    geometry->arrays.commands)
	return geometry->arrays.commands[idx];
    return (idx % 2) ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE;
}

static int
_line_geometry_move_cmd(int cmd)
{
    return cmd == BSG_GEOMETRY_LINE_MOVE;
}

static int
_line_geometry_draw_cmd(int cmd)
{
    return cmd == BSG_GEOMETRY_LINE_DRAW;
}

static const char *
_geometry_command_description(int cmd)
{
    switch (cmd) {
	case BSG_GEOMETRY_LINE_MOVE:
	    return "line move";
	case BSG_GEOMETRY_LINE_DRAW:
	    return "line draw";
	case BSG_GEOMETRY_POINT_DRAW:
	    return "point draw";
	default:
	    return "unknown";
    }
}

void
bsg_export_record_geometry_report(const struct bsg_export_record *record,
				  struct bu_vls *out)
{
    if (!record || !out)
	return;

    if ((record->geometry.kind == BSG_RENDER_GEOMETRY_LINE_SET ||
		record->geometry.kind == BSG_RENDER_GEOMETRY_POINT_SET) &&
	    record->geometry.arrays.point_count &&
	    record->geometry.arrays.points) {
	for (size_t i = 0; i < record->geometry.arrays.point_count; i++) {
	    int cmd = (record->geometry.kind == BSG_RENDER_GEOMETRY_POINT_SET) ?
		BSG_GEOMETRY_POINT_DRAW : _geometry_line_command(&record->geometry, i);
	    const point_t *pt = &record->geometry.arrays.points[i];
	    bu_vls_printf(out, "  %s (%g, %g, %g)\n",
		    _geometry_command_description(cmd),
		    V3ARGS(*pt));
	}
	return;
    }

    if (record->geometry.kind == BSG_RENDER_GEOMETRY_INDEXED_LINE_SET &&
	    record->geometry.arrays.point_count &&
	    record->geometry.arrays.points &&
	    record->geometry.arrays.index_count &&
	    record->geometry.arrays.indices) {
	int have_current = 0;
	for (size_t i = 0; i < record->geometry.arrays.index_count; i++) {
	    int idx = record->geometry.arrays.indices[i];
	    if (idx < 0) {
		have_current = 0;
		continue;
	    }
	    if ((size_t)idx >= record->geometry.arrays.point_count)
		continue;
	    int cmd = have_current ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE;
	    const point_t *pt = &record->geometry.arrays.points[idx];
	    bu_vls_printf(out, "  %s (%g, %g, %g)\n",
		    _geometry_command_description(cmd),
		    V3ARGS(*pt));
	    have_current = 1;
	}
	return;
    }

    if (record->geometry.kind == BSG_RENDER_GEOMETRY_INDEXED_FACE_SET &&
	    record->geometry.surface.point_count &&
	    record->geometry.surface.points &&
	    record->geometry.surface.index_count &&
	    record->geometry.surface.indices) {
	size_t face_vertex = 0;
	for (size_t i = 0; i < record->geometry.surface.index_count; i++) {
	    int idx = record->geometry.surface.indices[i];
	    if (idx < 0) {
		face_vertex = 0;
		continue;
	    }
	    if ((size_t)idx >= record->geometry.surface.point_count)
		continue;
	    const point_t *pt = &record->geometry.surface.points[idx];
	    bu_vls_printf(out, "  indexed face %zu (%g, %g, %g)\n",
		    face_vertex, V3ARGS(*pt));
	    face_vertex++;
	}
	return;
    }
}

static size_t
_export_record_foreach_indexed_face_segment(const struct bsg_export_record *record,
					    bsg_export_segment_cb cb,
					    void *data)
{
    size_t count = 0;
    int first_idx = -1;
    int prev_idx = -1;
    size_t face_vertices = 0;

    if (!record || !cb ||
	    record->geometry.kind != BSG_RENDER_GEOMETRY_INDEXED_FACE_SET ||
	    !record->geometry.surface.point_count ||
	    !record->geometry.surface.points ||
	    !record->geometry.surface.index_count ||
	    !record->geometry.surface.indices)
	return 0;

    for (size_t i = 0; i < record->geometry.surface.index_count; i++) {
	int idx = record->geometry.surface.indices[i];
	if (idx < 0) {
	    if (face_vertices > 1 && first_idx >= 0 && prev_idx >= 0 &&
		    prev_idx != first_idx) {
		if (!cb(record->geometry.surface.points[prev_idx],
			    record->geometry.surface.points[first_idx], data))
		    return count;
		count++;
	    }
	    first_idx = -1;
	    prev_idx = -1;
	    face_vertices = 0;
	    continue;
	}
	if ((size_t)idx >= record->geometry.surface.point_count)
	    continue;
	if (!face_vertices) {
	    first_idx = idx;
	    prev_idx = idx;
	    face_vertices = 1;
	    continue;
	}
	if (prev_idx >= 0) {
	    if (!cb(record->geometry.surface.points[prev_idx],
			record->geometry.surface.points[idx], data))
		return count;
	    count++;
	}
	prev_idx = idx;
	face_vertices++;
    }

    if (face_vertices > 1 && first_idx >= 0 && prev_idx >= 0 &&
	    prev_idx != first_idx) {
	if (!cb(record->geometry.surface.points[prev_idx],
		    record->geometry.surface.points[first_idx], data))
	    return count;
	count++;
    }

    return count;
}

size_t
bsg_export_record_foreach_segment(const struct bsg_export_record *record,
				  bsg_export_segment_cb cb,
				  void *data)
{
    size_t count = 0;

    if (!record || !cb ||
	    record->geometry.kind == BSG_RENDER_GEOMETRY_NONE)
	return 0;

    point_t last = VINIT_ZERO;
    point_t fin = VINIT_ZERO;

    if (record->geometry.kind == BSG_RENDER_GEOMETRY_LINE_SET &&
	    record->geometry.arrays.point_count &&
	    record->geometry.arrays.points) {
	for (size_t i = 0; i < record->geometry.arrays.point_count; i++) {
	    int cmd = _geometry_line_command(&record->geometry, i);
	    if (_line_geometry_move_cmd(cmd)) {
		VMOVE(last, record->geometry.arrays.points[i]);
		continue;
	    }
	    if (_line_geometry_draw_cmd(cmd)) {
		VMOVE(fin, record->geometry.arrays.points[i]);
	    } else {
		continue;
	    }
	    if (!cb(last, fin, data))
		return count;
	    count++;
	    VMOVE(last, fin);
	}
	return count;
    }

    if (record->geometry.kind == BSG_RENDER_GEOMETRY_INDEXED_LINE_SET &&
	    record->geometry.arrays.point_count &&
	    record->geometry.arrays.points &&
	    record->geometry.arrays.index_count &&
	    record->geometry.arrays.indices) {
	int have_last = 0;
	for (size_t i = 0; i < record->geometry.arrays.index_count; i++) {
	    int idx = record->geometry.arrays.indices[i];
	    if (idx < 0) {
		have_last = 0;
		continue;
	    }
	    if ((size_t)idx >= record->geometry.arrays.point_count)
		continue;
	    if (!have_last) {
		VMOVE(last, record->geometry.arrays.points[idx]);
		have_last = 1;
		continue;
	    }
	    VMOVE(fin, record->geometry.arrays.points[idx]);
	    if (!cb(last, fin, data))
		return count;
	    count++;
	    VMOVE(last, fin);
	}
	return count;
    }

    if (record->geometry.kind == BSG_RENDER_GEOMETRY_INDEXED_FACE_SET)
	return _export_record_foreach_indexed_face_segment(record, cb, data);

    return 0;
}

struct _has_segment_data {
    int seen;
};

static int
_has_segment_cb(const point_t UNUSED(a), const point_t UNUSED(b), void *data)
{
    struct _has_segment_data *hsd = (struct _has_segment_data *)data;
    if (hsd)
	hsd->seen = 1;
    return 0;
}

int
bsg_export_record_has_segments(const struct bsg_export_record *record)
{
    struct _has_segment_data hsd;
    hsd.seen = 0;
    (void)bsg_export_record_foreach_segment(record, _has_segment_cb, &hsd);
    return hsd.seen;
}

size_t
bsg_export_record_foreach_point(const struct bsg_export_record *record,
				bsg_export_point_cb cb,
				void *data)
{
    size_t count = 0;

    if (!record || !cb ||
	    record->geometry.kind == BSG_RENDER_GEOMETRY_NONE)
	return 0;

    if (record->geometry.kind == BSG_RENDER_GEOMETRY_INDEXED_FACE_SET &&
	    record->geometry.surface.point_count &&
	    record->geometry.surface.points) {
	for (size_t i = 0; i < record->geometry.surface.point_count; i++) {
	    if (!cb(record->geometry.surface.points[i], data))
		return count;
	    count++;
	}
	return count;
    }

    if ((record->geometry.kind == BSG_RENDER_GEOMETRY_LINE_SET ||
		record->geometry.kind == BSG_RENDER_GEOMETRY_POINT_SET ||
		record->geometry.kind == BSG_RENDER_GEOMETRY_INDEXED_LINE_SET) &&
	    record->geometry.arrays.point_count &&
	    record->geometry.arrays.points) {
	for (size_t i = 0; i < record->geometry.arrays.point_count; i++) {
	    if (!cb(record->geometry.arrays.points[i], data))
		return count;
	    count++;
	}
	return count;
    }

    return 0;
}

struct bsg_export_result *
bsg_export_result_create(void)
{
    struct bsg_export_result *result;
    BU_GET(result, struct bsg_export_result);
    bu_ptbl_init(&result->records, 8, "bsg_export_result records");
    return result;
}

static void
_export_record_free(struct bsg_export_record *record)
{
    if (!record)
	return;
    _export_geometry_arrays_free(&record->geometry);
    bu_vls_free(&record->path);
    BU_PUT(record, struct bsg_export_record);
}

void
bsg_export_result_free(struct bsg_export_result *result)
{
    if (!result)
	return;
    for (size_t i = 0; i < BU_PTBL_LEN(&result->records); i++)
	_export_record_free((struct bsg_export_record *)BU_PTBL_GET(&result->records, i));
    bu_ptbl_free(&result->records);
    BU_PUT(result, struct bsg_export_result);
}

size_t
bsg_export_result_count(const struct bsg_export_result *result)
{
    return result ? BU_PTBL_LEN(&result->records) : 0;
}

const struct bsg_export_record *
bsg_export_result_get(const struct bsg_export_result *result, size_t idx)
{
    if (!result || idx >= BU_PTBL_LEN(&result->records))
	return NULL;
    return (const struct bsg_export_record *)BU_PTBL_GET(&result->records, idx);
}

static struct bsg_export_record *
_export_record_from_item(const struct bsg_render_item *item)
{
    if (!item)
	return NULL;

    struct bsg_export_record *record;
    BU_GET(record, struct bsg_export_record);
    memset(record, 0, sizeof(*record));
    BU_VLS_INIT(&record->path);

    const char *path = item->source.name ? item->source.name : item->name;
    if (path)
	bu_vls_sprintf(&record->path, "%s", path);
    record->source = item->source;
    record->source.name = bu_vls_cstr(&record->path);
    MAT_COPY(record->model_mat, item->model_mat);
    record->roles = _export_record_roles(item);
    record->phase = item->phase;
    record->geometry = item->geometry;
    (void)_export_geometry_arrays_copy(&record->geometry, &item->geometry);
    record->realization_kind = item->realization_kind;
    record->realization_status = item->realization_status;
    record->stale_reason = item->stale_reason;
    record->geometry_revision = item->geometry.revision;
    record->payload_revision = item->payload_revision;
    record->cache_identity = item->cache_identity;
    record->settings_hash = item->context.settings_hash;
    record->request_flags = item->context.request_flags;
    record->backend_capabilities = item->context.backend_capabilities;
    record->capability_gaps = _export_capability_gaps(item, item->context.backend_capabilities);
    record->non_database_source = item->non_database_source;
    record->visible = item->visible;
    record->selected = item->selected;
    record->highlighted = item->highlighted;
    record->draw_mode = item->appearance.draw_mode;
    record->line_style = item->appearance.line_style;
    record->evaluated_region = item->appearance.evaluated_region;
    record->color[0] = item->appearance.color[0];
    record->color[1] = item->appearance.color[1];
    record->color[2] = item->appearance.color[2];
    record->transparency = item->appearance.transparency;
    VMOVE(record->bounds_center, item->bounds_center);
    record->bounds_radius = item->bounds_radius;
    record->has_bounds = item->has_bounds;
    record->graph_depth = item->graph_depth;
    record->sequence = item->sequence;
    record->lod_level = item->lod_level;
    record->lod_level_count = item->lod_level_count;
    record->lod_identity = item->source.lod_id;
    record->lod_policy_identity = item->source.lod_policy_id;
    if (item->geometry.kind == BSG_RENDER_GEOMETRY_LINE_SET) {
	if (record->geometry.arrays.point_count) {
	    record->vlist_command_count = record->geometry.arrays.point_count;
	    record->vlist_point_count = record->geometry.arrays.point_count;
	}
    }

    return record;
}

static bsg_render_geometry_role
_feature_geometry_role(enum bsg_feature_family family)
{
    switch (family) {
	case BSG_FEATURE_AXES:
	    return BSG_RENDER_GEOMETRY_ROLE_AXES_WIDGET;
	case BSG_FEATURE_LABEL:
	case BSG_FEATURE_LABEL_LEADER:
	case BSG_FEATURE_TEXT:
	case BSG_FEATURE_ANNOTATION:
	    return BSG_RENDER_GEOMETRY_ROLE_TEXT_LABEL;
	case BSG_FEATURE_POLYGON:
	case BSG_FEATURE_SKETCH:
	    return BSG_RENDER_GEOMETRY_ROLE_POLYGON_REGION;
	case BSG_FEATURE_LINES:
	case BSG_FEATURE_ARROW:
	case BSG_FEATURE_MEASUREMENT:
	    return BSG_RENDER_GEOMETRY_ROLE_LINE_SET;
	default:
	    return BSG_RENDER_GEOMETRY_ROLE_VIEW_OVERLAY;
    }
}

static bsg_render_geometry_kind
_feature_geometry_kind(const struct bsg_feature_record *feature)
{
    if (!feature)
	return BSG_RENDER_GEOMETRY_NONE;
    switch (feature->family) {
	case BSG_FEATURE_LABEL:
	case BSG_FEATURE_LABEL_LEADER:
	case BSG_FEATURE_TEXT:
	    return BSG_RENDER_GEOMETRY_TEXT;
	case BSG_FEATURE_LINES:
	case BSG_FEATURE_ARROW:
	case BSG_FEATURE_SNAP_HINT:
	case BSG_FEATURE_EDIT_HANDLE:
	case BSG_FEATURE_TRANSIENT_PREVIEW:
	case BSG_FEATURE_MEASUREMENT:
	case BSG_FEATURE_FACEPLATE:
	case BSG_FEATURE_POLYGON:
	case BSG_FEATURE_SKETCH:
	    return feature->geometry_command_count ?
		BSG_RENDER_GEOMETRY_LINE_SET : BSG_RENDER_GEOMETRY_NONE;
	case BSG_FEATURE_HUD:
	    return BSG_RENDER_GEOMETRY_HUD;
	case BSG_FEATURE_AXES:
	case BSG_FEATURE_GRID:
	case BSG_FEATURE_OVERLAY:
	    return BSG_RENDER_GEOMETRY_OVERLAY;
	case BSG_FEATURE_ANNOTATION:
	    return BSG_RENDER_GEOMETRY_ANNOTATION;
	default:
	    return feature->geometry_command_count ?
		BSG_RENDER_GEOMETRY_LINE_SET : BSG_RENDER_GEOMETRY_NONE;
    }
}

static bsg_render_draw_intent
_feature_draw_intent(enum bsg_feature_family family)
{
    switch (family) {
	case BSG_FEATURE_HUD:
	case BSG_FEATURE_FACEPLATE:
	    return BSG_RENDER_DRAW_INTENT_HUD;
	case BSG_FEATURE_OVERLAY:
	case BSG_FEATURE_MEASUREMENT:
	case BSG_FEATURE_SNAP_HINT:
	    return BSG_RENDER_DRAW_INTENT_OVERLAY;
	case BSG_FEATURE_EDIT_HANDLE:
	case BSG_FEATURE_TRANSIENT_PREVIEW:
	case BSG_FEATURE_SKETCH:
	    return BSG_RENDER_DRAW_INTENT_EDIT_PREVIEW;
	default:
	    return BSG_RENDER_DRAW_INTENT_VIEW_FEATURE;
    }
}

static struct bsg_export_record *
_export_record_from_feature(const struct bsg_feature_record *feature)
{
    if (!feature)
	return NULL;

    struct bsg_export_record *record;
    BU_GET(record, struct bsg_export_record);
    memset(record, 0, sizeof(*record));
    BU_VLS_INIT(&record->path);
    if (feature->name)
	bu_vls_sprintf(&record->path, "%s", feature->name);

    record->source.source_id = feature->ref.token;
    record->source.name = bu_vls_cstr(&record->path);
    MAT_IDN(record->model_mat);
    record->roles = BSG_EXPORT_RECORD_SCENE;
    record->geometry.kind = _feature_geometry_kind(feature);
    record->geometry.element_count = feature->geometry_command_count;
    if (record->geometry.kind != BSG_RENDER_GEOMETRY_NONE)
	record->roles |= BSG_EXPORT_RECORD_GEOMETRY;
    if (feature->family == BSG_FEATURE_LABEL ||
	    feature->family == BSG_FEATURE_LABEL_LEADER ||
	    feature->family == BSG_FEATURE_TEXT)
	record->roles |= BSG_EXPORT_RECORD_LABEL;
    if (feature->family == BSG_FEATURE_ANNOTATION ||
	    feature->family == BSG_FEATURE_OVERLAY ||
	    feature->family == BSG_FEATURE_HUD ||
	    feature->family == BSG_FEATURE_FACEPLATE)
	record->roles |= BSG_EXPORT_RECORD_ANNOTATION;
    record->phase = BSG_RENDER_PHASE_OVERLAY;
    record->realization_kind = BSG_REALIZE_NONE;
    record->realization_status = BSG_REALIZE_STATUS_CURRENT;
    record->stale_reason = BSG_PAYLOAD_STALE_NONE;
    record->source.scope = (feature->scope == BSG_FEATURE_SCOPE_LOCAL) ?
	BSG_RENDER_SOURCE_SCOPE_VIEW_LOCAL : BSG_RENDER_SOURCE_SCOPE_VIEW_SHARED;
    record->source.geometry_role = _feature_geometry_role(feature->family);
    record->source.draw_intent = _feature_draw_intent(feature->family);
    record->source.feature_family = feature->family;
    record->source.overlay_role = BSG_OVERLAY_ROLE_MODEL;
    record->source.overlay_class = BSG_OVERLAY_CLASS_COMMAND_RESULT;
    record->non_database_source = 0;
    record->source.non_database_source = 0;
    record->visible = feature->visible;
    record->draw_mode = 0;
    record->color[0] = feature->color[0];
    record->color[1] = feature->color[1];
    record->color[2] = feature->color[2];
    record->transparency = 1.0;
    record->lod_level = -1;
    record->vlist_command_count = feature->geometry_command_count;
    record->vlist_point_count = feature->geometry_command_count;
    record->vlist_structure_count = feature->geometry_command_count ? 1 : 0;
    return record;
}

static int
_export_record_matches(const struct bsg_export_request *request,
		       const struct bsg_export_record *record)
{
    if (!request || !record)
	return 0;

    unsigned int type_filter = request->query_flags &
	(BSG_EXPORT_QUERY_DB_OBJECTS | BSG_EXPORT_QUERY_VIEW_OBJECTS);
    if (type_filter) {
	int type_match = 0;
	if ((type_filter & BSG_EXPORT_QUERY_DB_OBJECTS) &&
		record->source.scope == BSG_RENDER_SOURCE_SCOPE_DATABASE)
	    type_match = 1;
	if ((type_filter & BSG_EXPORT_QUERY_VIEW_OBJECTS) &&
		(record->source.scope == BSG_RENDER_SOURCE_SCOPE_VIEW_SHARED ||
		 record->source.scope == BSG_RENDER_SOURCE_SCOPE_VIEW_LOCAL ||
		 record->source.draw_intent == BSG_RENDER_DRAW_INTENT_OVERLAY ||
		 record->source.draw_intent == BSG_RENDER_DRAW_INTENT_HUD))
	    type_match = 1;
	if (!type_match)
	    return 0;
    }
    if ((request->query_flags & BSG_EXPORT_QUERY_LOCAL_ONLY) &&
	    record->source.scope != BSG_RENDER_SOURCE_SCOPE_VIEW_LOCAL)
	return 0;
    if (request->draw_mode != BSG_EXPORT_DRAW_MODE_ANY &&
	    record->draw_mode != request->draw_mode)
	return 0;
    if (request->glob && request->glob[0]) {
	const char *path = bu_vls_cstr(&record->path);
	if (!path || bu_path_match(request->glob, path, 0) != 0)
	    return 0;
    }

    return 1;
}

static int
_export_result_has_path(const struct bsg_export_result *result, const char *path)
{
    if (!result || !path)
	return 0;
    for (size_t i = 0; i < bsg_export_result_count(result); i++) {
	const struct bsg_export_record *rec = bsg_export_result_get(result, i);
	const char *rpath = rec ? bu_vls_cstr(&rec->path) : NULL;
	if (rpath && BU_STR_EQUAL(rpath, path))
	    return 1;
    }
    return 0;
}

struct _feature_export_ctx {
    const struct bsg_export_request *request;
    struct bsg_export_result *result;
};

static int
_feature_export_cb(bsg_feature_ref UNUSED(ref), const struct bsg_feature_record *feature, void *data)
{
    struct _feature_export_ctx *ctx = (struct _feature_export_ctx *)data;
    if (!ctx || !ctx->request || !ctx->result || !feature)
	return 1;

    struct bsg_export_record *record = _export_record_from_feature(feature);
    if (!record)
	return 1;
    const char *path = bu_vls_cstr(&record->path);
    if (!_export_result_has_path(ctx->result, path) &&
	    _export_record_matches(ctx->request, record)) {
	bu_ptbl_ins(&ctx->result->records, (long *)record);
	return 1;
    }
    _export_record_free(record);
    return 1;
}

static void
_export_append_feature_records(const struct bsg_export_request *request,
			       struct bsg_export_result *result)
{
    if (!request || !result || !request->view)
	return;
    if (!(request->query_flags & BSG_EXPORT_QUERY_VIEW_OBJECTS))
	return;
    if (request->query_flags & BSG_EXPORT_QUERY_VISIBLE_ONLY)
	return;

    int scope = BSG_FEATURE_SCOPE_ALL;
    if (request->query_flags & BSG_EXPORT_QUERY_LOCAL_ONLY)
	scope = BSG_FEATURE_SCOPE_LOCAL;
    struct _feature_export_ctx ctx;
    ctx.request = request;
    ctx.result = result;
    bsg_feature_visit(request->view, scope, _feature_export_cb, &ctx);
}

void
bsg_export_request_init(struct bsg_export_request *request,
			struct bsg_view *view)
{
    if (!request)
	return;
    memset(request, 0, sizeof(*request));
    request->view = view;
    request->query_flags = BSG_EXPORT_QUERY_VISIBLE_ONLY;
    request->render_flags = BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE;
    request->glob = NULL;
    request->draw_mode = BSG_EXPORT_DRAW_MODE_ANY;
}

static int
_export_query_into_result(const struct bsg_export_request *request,
			  struct bsg_export_result *result)
{
    if (!request || !request->view || !result)
	return -1;

    struct bsg_render_request *req = bsg_render_request_create(request->view, NULL);
    if (!req)
	return -1;
    struct bsg_render_batch *batch = bsg_render_batch_create();
    if (!batch) {
	bsg_render_request_destroy(req);
	return -1;
    }
    unsigned int flags = request->render_flags;
    if (!flags) {
	flags = BSG_RENDER_FLAG_PAYLOAD_PREPARE;
	if (request->query_flags & BSG_EXPORT_QUERY_VISIBLE_ONLY)
	    flags |= BSG_RENDER_FLAG_VISIBLE_ONLY;
    }
    bsg_render_request_set_flags(req, flags & ~BSG_RENDER_FLAG_COLLECT_ITEMS);
    (void)bsg_render_request_collect(req, batch);
    bsg_render_request_destroy(req);

    for (size_t i = 0; i < bsg_render_batch_count(batch); i++) {
	const struct bsg_render_item *item = bsg_render_batch_get(batch, i);
	struct bsg_export_record *record = _export_record_from_item(item);
	if (record && _export_record_matches(request, record)) {
	    bu_ptbl_ins(&result->records, (long *)record);
	    record = NULL;
	}
	_export_record_free(record);
    }
    bsg_render_batch_destroy(batch);
    _export_append_feature_records(request, result);
    return 0;
}

struct export_records_action_data {
    const struct bsg_export_request *request;
    struct bsg_export_result *result;
};

static int
_export_records_action_apply(bsg_action_ref UNUSED(action),
			     bsg_node_ref UNUSED(root),
			     void *data)
{
    struct export_records_action_data *st =
	(struct export_records_action_data *)data;
    if (!st)
	return -1;
    return _export_query_into_result(st->request, st->result);
}

static void
_export_records_action_destroy(bsg_action_ref UNUSED(action), void *data)
{
    if (data)
	bu_free(data, "export records action data");
}

bsg_action_ref
bsg_export_records_action_create(const struct bsg_export_request *request,
				 struct bsg_export_result *result)
{
    struct export_records_action_data *st;
    BU_ALLOC(st, struct export_records_action_data);
    st->request = request;
    st->result = result;
    return bsg_action_ref_create_internal(bsg_export_records_action_type(),
	    _export_records_action_apply, _export_records_action_destroy, st,
	    "export records action");
}

struct bsg_export_result *
bsg_export_query(const struct bsg_export_request *request)
{
    if (!request || !request->view)
	return NULL;

    struct bsg_export_result *result = bsg_export_result_create();
    if (!result)
	return NULL;

    bsg_action_ref action = bsg_export_records_action_create(request, result);
    if (bsg_action_apply(action, bsg_node_ref_null()) < 0) {
	bsg_action_ref_destroy(action);
	bsg_export_result_free(result);
	return NULL;
    }
    bsg_action_ref_destroy(action);
    return result;
}

struct bsg_export_result *
bsg_export_scene(struct bsg_view *view, unsigned int flags)
{
    struct bsg_export_request request;
    bsg_export_request_init(&request, view);
    request.render_flags = flags ? flags : (BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    request.query_flags = (request.render_flags & BSG_RENDER_FLAG_VISIBLE_ONLY) ?
	BSG_EXPORT_QUERY_VISIBLE_ONLY : 0;
    return bsg_export_query(&request);
}

void
bsg_export_result_serialize(const struct bsg_export_result *result,
			    struct bu_vls *out)
{
    if (!out)
	return;
    bu_vls_printf(out, "[");
    size_t cnt = bsg_export_result_count(result);
    for (size_t i = 0; i < cnt; i++) {
	const struct bsg_export_record *record = bsg_export_result_get(result, i);
	if (i)
	    bu_vls_printf(out, ",");
	if (!record) {
	    bu_vls_printf(out, "{\"valid\":0}");
	    continue;
	}
	bu_vls_printf(out,
		"{\"path\":\"%s\",\"roles\":%u,\"source\":%llu,\"geometry\":%d,\"geometry_revision\":%llu,\"lod_policy\":%llu,\"realization\":%d,\"visible\":%d,\"selected\":%d,\"highlighted\":%d,\"draw_mode\":%d,\"line_style\":%d,\"evaluated_region\":%d,\"capability_gaps\":%u,\"vlist_structures\":%zu,\"vlist_commands\":%zu,\"vlist_points\":%zu}",
		bu_vls_cstr(&record->path),
		record->roles,
		(unsigned long long)record->source.source_id,
		(int)record->geometry.kind,
		(unsigned long long)record->geometry.revision,
		(unsigned long long)record->lod_policy_identity,
		(int)record->realization_status,
		record->visible,
		record->selected,
		record->highlighted,
		record->draw_mode,
		record->line_style,
		record->evaluated_region,
		record->capability_gaps,
		record->vlist_structure_count,
		record->vlist_command_count,
		record->vlist_point_count);
    }
    bu_vls_printf(out, "]");
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
