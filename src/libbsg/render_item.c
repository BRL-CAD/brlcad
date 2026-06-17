/*                  R E N D E R _ I T E M . C
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
/** @file libbsg/render_item.c
 *
 * Render-item lifecycle.
 */

#include "common.h"

#include <string.h>

#include "bu/malloc.h"
#include "vmath.h"

#include "bsg/geometry.h"
#include "bsg/render_item.h"


struct bsg_render_item *
bsg_render_item_create(void)
{
    struct bsg_render_item *item;
    BU_ALLOC(item, struct bsg_render_item);
    memset(item, 0, sizeof(struct bsg_render_item));
    /* Default: identity transform, fully opaque, phase opaque */
    MAT_IDN(item->model_mat);
    MAT_IDN(item->context.model2view);
    MAT_IDN(item->context.view2model);
    MAT_IDN(item->context.projection);
    VSET(item->context.clipmin, -1.0, -1.0, -1.0);
    VSET(item->context.clipmax, 1.0, 1.0, 1.0);
    item->appearance.transparency = 1.0;
    item->visible      = 1;
    item->lod_level    = -1;
    item->phase        = BSG_RENDER_PHASE_OPAQUE;
    return item;
}

static int
_render_line_move_cmd(int cmd)
{
    return cmd == BSG_GEOMETRY_LINE_MOVE;
}

static int
_render_line_draw_cmd(int cmd)
{
    return cmd == BSG_GEOMETRY_LINE_DRAW;
}

static int
_render_line_command(const struct bsg_render_geometry *geometry, size_t idx)
{
    if (geometry && idx < geometry->arrays.command_count &&
	    geometry->arrays.commands)
	return geometry->arrays.commands[idx];
    return (idx % 2) ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE;
}

static size_t
_render_item_foreach_indexed_face_wire_segment(const struct bsg_render_geometry *geometry,
					       bsg_render_segment_cb cb,
					       void *data)
{
    size_t count = 0;
    int first_idx = -1;
    int prev_idx = -1;
    size_t face_vertices = 0;

    if (!geometry || !cb ||
	    !geometry->surface.point_count || !geometry->surface.points ||
	    !geometry->surface.index_count || !geometry->surface.indices)
	return 0;

    for (size_t i = 0; i < geometry->surface.index_count; i++) {
	int idx = geometry->surface.indices[i];
	if (idx < 0) {
	    if (face_vertices > 1 && first_idx >= 0 && prev_idx >= 0 &&
		    prev_idx != first_idx) {
		if (!cb(geometry->surface.points[prev_idx],
			    geometry->surface.points[first_idx], data))
		    return count;
		count++;
	    }
	    first_idx = -1;
	    prev_idx = -1;
	    face_vertices = 0;
	    continue;
	}
	if ((size_t)idx >= geometry->surface.point_count)
	    continue;
	if (!face_vertices) {
	    first_idx = idx;
	    prev_idx = idx;
	    face_vertices = 1;
	    continue;
	}
	if (prev_idx >= 0) {
	    if (!cb(geometry->surface.points[prev_idx],
			geometry->surface.points[idx], data))
		return count;
	    count++;
	}
	prev_idx = idx;
	face_vertices++;
    }

    if (face_vertices > 1 && first_idx >= 0 && prev_idx >= 0 &&
	    prev_idx != first_idx) {
	if (!cb(geometry->surface.points[prev_idx],
		    geometry->surface.points[first_idx], data))
	    return count;
	count++;
    }

    return count;
}

static size_t
_render_item_foreach_segment(const struct bsg_render_item *item,
			     bsg_render_segment_cb cb,
			     void *data,
			     int wire)
{
    size_t count = 0;
    point_t last = VINIT_ZERO;
    point_t fin = VINIT_ZERO;

    if (!item || !cb || item->geometry.kind == BSG_RENDER_GEOMETRY_NONE)
	return 0;

    if (item->geometry.kind == BSG_RENDER_GEOMETRY_LINE_SET &&
	    item->geometry.arrays.point_count &&
	    item->geometry.arrays.points) {
	for (size_t i = 0; i < item->geometry.arrays.point_count; i++) {
	    int cmd = _render_line_command(&item->geometry, i);
	    if (_render_line_move_cmd(cmd)) {
		VMOVE(last, item->geometry.arrays.points[i]);
		continue;
	    }
	    if (_render_line_draw_cmd(cmd)) {
		VMOVE(fin, item->geometry.arrays.points[i]);
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

    if (item->geometry.kind == BSG_RENDER_GEOMETRY_INDEXED_LINE_SET &&
	    item->geometry.arrays.point_count &&
	    item->geometry.arrays.points &&
	    item->geometry.arrays.index_count &&
	    item->geometry.arrays.indices) {
	int have_last = 0;
	for (size_t i = 0; i < item->geometry.arrays.index_count; i++) {
	    int idx = item->geometry.arrays.indices[i];
	    if (idx < 0) {
		have_last = 0;
		continue;
	    }
	    if ((size_t)idx >= item->geometry.arrays.point_count)
		continue;
	    if (!have_last) {
		VMOVE(last, item->geometry.arrays.points[idx]);
		have_last = 1;
		continue;
	    }
	    VMOVE(fin, item->geometry.arrays.points[idx]);
	    if (!cb(last, fin, data))
		return count;
	    count++;
	    VMOVE(last, fin);
	}
	return count;
    }

    if (wire && item->geometry.kind == BSG_RENDER_GEOMETRY_INDEXED_FACE_SET)
	return _render_item_foreach_indexed_face_wire_segment(&item->geometry,
		cb, data);

    return 0;
}

size_t
bsg_render_item_foreach_line_segment(const struct bsg_render_item *item,
				     bsg_render_segment_cb cb,
				     void *data)
{
    return _render_item_foreach_segment(item, cb, data, 0);
}

size_t
bsg_render_item_foreach_wire_segment(const struct bsg_render_item *item,
				     bsg_render_segment_cb cb,
				     void *data)
{
    return _render_item_foreach_segment(item, cb, data, 1);
}

struct _render_has_segment_data {
    int seen;
};

static int
_render_has_segment_cb(const point_t UNUSED(a), const point_t UNUSED(b), void *data)
{
    struct _render_has_segment_data *hsd =
	(struct _render_has_segment_data *)data;
    if (hsd)
	hsd->seen = 1;
    return 0;
}

int
bsg_render_item_has_line_segments(const struct bsg_render_item *item)
{
    struct _render_has_segment_data hsd;
    hsd.seen = 0;
    (void)bsg_render_item_foreach_line_segment(item, _render_has_segment_cb, &hsd);
    return hsd.seen;
}

int
bsg_render_item_has_wire_segments(const struct bsg_render_item *item)
{
    struct _render_has_segment_data hsd;
    hsd.seen = 0;
    (void)bsg_render_item_foreach_wire_segment(item, _render_has_segment_cb, &hsd);
    return hsd.seen;
}

static void
_render_annotation_segment_clear(struct bsg_annotation_segment *seg)
{
    if (!seg)
	return;
    if (seg->kind == BSG_ANNOTATION_SEGMENT_NURB) {
	if (seg->data.nurb.knots)
	    bu_free(seg->data.nurb.knots, "bsg render annotation nurb knots");
	if (seg->data.nurb.control_points)
	    bu_free(seg->data.nurb.control_points, "bsg render annotation nurb control points");
	if (seg->data.nurb.weights)
	    bu_free(seg->data.nurb.weights, "bsg render annotation nurb weights");
    } else if (seg->kind == BSG_ANNOTATION_SEGMENT_BEZIER) {
	if (seg->data.bezier.control_points)
	    bu_free(seg->data.bezier.control_points, "bsg render annotation bezier control points");
    } else if (seg->kind == BSG_ANNOTATION_SEGMENT_TEXT) {
	if (seg->data.text.text)
	    bu_free(seg->data.text.text, "bsg render annotation text segment");
    }
}


void
bsg_render_item_free(struct bsg_render_item *item)
{
    if (!item)
	return;
    if (item->geometry.arrays.points)
	bu_free(item->geometry.arrays.points, "bsg render geometry points");
    if (item->geometry.arrays.normals)
	bu_free(item->geometry.arrays.normals, "bsg render geometry normals");
    if (item->geometry.arrays.commands)
	bu_free(item->geometry.arrays.commands, "bsg render geometry commands");
    if (item->geometry.arrays.indices)
	bu_free(item->geometry.arrays.indices, "bsg render geometry indices");
    if (item->geometry.text.label)
	bu_free(item->geometry.text.label, "bsg render geometry text");
    if (item->geometry.image.pixels)
	bu_free(item->geometry.image.pixels, "bsg render image pixels");
    if (item->geometry.surface.points)
	bu_free(item->geometry.surface.points, "bsg render surface points");
    if (item->geometry.surface.normals)
	bu_free(item->geometry.surface.normals, "bsg render surface normals");
    if (item->geometry.surface.indices)
	bu_free(item->geometry.surface.indices, "bsg render surface indices");
    if (item->geometry.annotation.text)
	bu_free(item->geometry.annotation.text, "bsg render annotation text");
    if (item->geometry.annotation.points)
	bu_free(item->geometry.annotation.points, "bsg render annotation points");
    if (item->geometry.annotation.segments) {
	for (size_t i = 0; i < item->geometry.annotation.segment_count; i++)
	    _render_annotation_segment_clear(&item->geometry.annotation.segments[i]);
	bu_free(item->geometry.annotation.segments, "bsg render annotation segments");
    }
    bu_free(item, "bsg_render_item");
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
