/*                          V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2007-2026 United States Government as represented by
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

#include "common.h"

#include <math.h>
#include <string.h>

#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/time.h"
#include "bu/units.h"
#include "bu/vls.h"
#include "bn.h"
#include "bsg/defines.h"
#include "bsg/appearance.h"
#include "bsg/draw_source.h"
#include "bsg/geometry.h"
#include "bsg/hud.h"
#include "bsg/render.h"
#include "bsg/render_item.h"
#include "bsg/backend_adapter.h"
#include "bsg/material.h"
#include "bsg/node.h"
#include "bsg/lod.h"
#include "bsg/util.h"
#include "bsg/scene_object.h"
#include "bsg/view_scope.h"
#include "bsg/view_state.h"
#include "dm.h"

void
dm_draw_arrow(struct dm *dmp, point_t A, point_t B, fastf_t tip_length, fastf_t tip_width, fastf_t sf)
{
    point_t points[16];
    point_t BmA;
    point_t offset;
    point_t perp1, perp2;
    point_t a_base;
    point_t a_pt1, a_pt2, a_pt3, a_pt4;

    VSUB2(BmA, B, A);

    VUNITIZE(BmA);
    VSCALE(offset, BmA, -tip_length * sf);

    bn_vec_perp(perp1, BmA);
    VUNITIZE(perp1);

    VCROSS(perp2, BmA, perp1);
    VUNITIZE(perp2);

    VSCALE(perp1, perp1, tip_width * sf);
    VSCALE(perp2, perp2, tip_width * sf);

    VADD2(a_base, B, offset);
    VADD2(a_pt1, a_base, perp1);
    VADD2(a_pt2, a_base, perp2);
    VSUB2(a_pt3, a_base, perp1);
    VSUB2(a_pt4, a_base, perp2);

    VMOVE(points[0], B);
    VMOVE(points[1], a_pt1);
    VMOVE(points[2], B);
    VMOVE(points[3], a_pt2);
    VMOVE(points[4], B);
    VMOVE(points[5], a_pt3);
    VMOVE(points[6], B);
    VMOVE(points[7], a_pt4);
    VMOVE(points[8], a_pt1);
    VMOVE(points[9], a_pt2);
    VMOVE(points[10], a_pt2);
    VMOVE(points[11], a_pt3);
    VMOVE(points[12], a_pt3);
    VMOVE(points[13], a_pt4);
    VMOVE(points[14], a_pt4);
    VMOVE(points[15], a_pt1);

    (void)dm_draw_lines_3d(dmp, 16, points, 0);
}


static void _dm_draw_label_resolved(struct dm *dmp, const struct bsg_render_item *item);

struct dm_arrow_trace {
    point_t a;
    point_t b;
    int have_current;
    int have_segment;
};

static void
dm_arrow_trace_flush(struct dm *dmp, struct dm_arrow_trace *trace,
	fastf_t tip_length, fastf_t tip_width)
{
    if (!dmp || !trace || !trace->have_segment)
	return;
    dm_draw_arrow(dmp, trace->a, trace->b, tip_length, tip_width, 1.0);
    trace->have_segment = 0;
}

static void
dm_arrow_trace_move(struct dm *dmp, struct dm_arrow_trace *trace,
	const point_t pt, fastf_t tip_length, fastf_t tip_width)
{
    if (!trace)
	return;
    dm_arrow_trace_flush(dmp, trace, tip_length, tip_width);
    VMOVE(trace->b, pt);
    trace->have_current = 1;
}

static void
dm_arrow_trace_draw(struct dm_arrow_trace *trace, const point_t pt)
{
    if (!trace)
	return;
    if (!trace->have_current) {
	VMOVE(trace->b, pt);
	trace->have_current = 1;
	return;
    }
    VMOVE(trace->a, trace->b);
    VMOVE(trace->b, pt);
    trace->have_segment = 1;
}

struct dm_arrow_segment_data {
    struct dm *dmp;
    struct dm_arrow_trace trace;
    fastf_t tip_length;
    fastf_t tip_width;
};

static int
dm_add_arrow_segment_cb(const point_t a, const point_t b, void *data)
{
    struct dm_arrow_segment_data *asd = (struct dm_arrow_segment_data *)data;
    if (!asd || !asd->dmp)
	return 0;
    if (!asd->trace.have_current ||
	    !VNEAR_EQUAL(asd->trace.b, a, SMALL_FASTF))
	dm_arrow_trace_move(asd->dmp, &asd->trace, a,
		asd->tip_length, asd->tip_width);
    dm_arrow_trace_draw(&asd->trace, b);
    return 1;
}

// Draw an arrow head for each MOVE+LAST_DRAW pairing
static void
dm_add_item_arrows(struct dm *dmp, const struct bsg_render_item *item)
{
    if (!dmp || !item || !(item->features.flags & BSG_RENDER_FEATURE_ARROWS))
	return;
    fastf_t tip_length = item->features.arrow_tip_length;
    fastf_t tip_width = item->features.arrow_tip_width;
    if (NEAR_ZERO(tip_length, SMALL_FASTF) || NEAR_ZERO(tip_width, SMALL_FASTF))
	return;
    if (!bsg_render_item_has_line_segments(item))
	return;

    struct dm_arrow_segment_data asd;
    memset(&asd, 0, sizeof(asd));
    asd.dmp = dmp;
    asd.tip_length = tip_length;
    asd.tip_width = tip_width;
    (void)bsg_render_item_foreach_line_segment(item, dm_add_arrow_segment_cb, &asd);
    dm_arrow_trace_flush(dmp, &asd.trace, tip_length, tip_width);
}

static void
_dm_draw_hud_axes_feature(struct dm *dmp, struct bsg_view *v, const struct bsg_axes *src, int model_axes)
{
    if (!src)
	return;

    struct bsg_axes axes = *src;
    if (model_axes) {
	point_t map;
	point_t save_map;

	VMOVE(save_map, axes.axes_pos);
	VSCALE(map, axes.axes_pos, v->gv_local2base);
	MAT4X3PNT(axes.axes_pos, v->gv_model2view, map);
	dm_draw_hud_axes(dmp, v->gv_size, v->gv_rotation, &axes);
	VMOVE(axes.axes_pos, save_map);
	return;
    }

    int width = dm_get_width(dmp);
    int height = dm_get_height(dmp);
    fastf_t inv_aspect = (width > 0) ? (fastf_t)height / (fastf_t)width : 1.0;
    axes.axes_pos[Y] = axes.axes_pos[Y] * inv_aspect;
    dm_draw_hud_axes(dmp, v->gv_size, v->gv_rotation, &axes);
}


static void
_dm_draw_hud_grid(struct dm *dmp, struct bsg_view *v, const struct bsg_grid_state *grid)
{
    if (!grid)
	return;
    dm_draw_grid(dmp, (struct bsg_grid_state *)grid, v->gv_scale, v->gv_model2view, v->gv_base2local);
}


static void
_dm_draw_hud_framebuffer(struct dm *dmp)
{
    if (!dmp || !dm_get_fb(dmp))
	return;

    int zbuff_restore = dm_get_zbuffer(dmp);
    dm_set_zbuffer(dmp, 0);
    struct fb *fbp = dm_get_fb(dmp);
    int rw = dm_get_width(dmp);
    int rh = dm_get_height(dmp);
    if (fbp) {
	int fbw = fb_getwidth(fbp);
	int fbh = fb_getheight(fbp);
	if (fbw > 0 && fbw < rw) rw = fbw;
	if (fbh > 0 && fbh < rh) rh = fbh;
    }
    if (rw > 0 && rh > 0)
	fb_refresh(fbp, 0, 0, rw, rh);
    if (zbuff_restore)
	dm_set_zbuffer(dmp, 1);
}


static void
_dm_draw_line_set_2d_arrays(struct dm *dmp, const struct bsg_render_item *item)
{
    if (!dmp || !item || !item->geometry.arrays.points ||
	    item->geometry.arrays.point_count < 1)
	return;

    int save_lw = dm_get_linewidth(dmp);
    int save_ls = dm_get_linestyle(dmp);
    dm_set_line_attr(dmp, item->appearance.line_width, item->appearance.line_style);
    dm_set_fg(dmp, item->appearance.color[0], item->appearance.color[1], item->appearance.color[2], 1, 1.0);

    point_t prev = VINIT_ZERO;
    int have_prev = 0;
    for (size_t i = 0; i < item->geometry.arrays.point_count; i++) {
	int cmd = (item->geometry.arrays.commands &&
		i < item->geometry.arrays.command_count) ?
	    item->geometry.arrays.commands[i] :
	    ((i % 2) ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE);
	if (cmd == BSG_GEOMETRY_LINE_MOVE) {
	    VMOVE(prev, item->geometry.arrays.points[i]);
	    have_prev = 1;
	    continue;
	}
	if (cmd == BSG_GEOMETRY_LINE_DRAW && have_prev) {
	    dm_draw_line_2d(dmp, prev[X], prev[Y],
		    item->geometry.arrays.points[i][X],
		    item->geometry.arrays.points[i][Y]);
	    VMOVE(prev, item->geometry.arrays.points[i]);
	    continue;
	}
	if (cmd == BSG_GEOMETRY_POINT_DRAW)
	    dm_draw_point_2d(dmp, item->geometry.arrays.points[i][X],
		    item->geometry.arrays.points[i][Y]);
    }

    dm_set_line_attr(dmp, save_lw, save_ls);
}

static void
_dm_draw_label_2d_item(struct dm *dmp, const struct bsg_render_item *item)
{
    if (!dmp || !item || !item->geometry.text.label)
	return;

    int ofontsize = dm_get_fontsize(dmp);
    dm_set_fg(dmp, item->appearance.color[0], item->appearance.color[1], item->appearance.color[2], 1, 1.0);
    dm_set_fontsize(dmp, item->geometry.text.size);
    dm_draw_string_2d(dmp, item->geometry.text.label,
	    item->geometry.text.position[X],
	    item->geometry.text.position[Y],
	    item->geometry.text.size, 0);
    dm_set_fontsize(dmp, ofontsize);
}

static int
_dm_annotation_local_point(const struct bsg_render_item *item, int idx,
	point_t out)
{
    if (!item || idx < 0 ||
	    (size_t)idx >= item->geometry.annotation.point_count ||
	    !item->geometry.annotation.points)
	return 0;
    VMOVE(out, item->geometry.annotation.points[idx]);
    return 1;
}

static void
_dm_annotation_display_scale(struct dm *dmp, fastf_t *sx, fastf_t *sy)
{
    int w = dm_get_width(dmp);
    int h = dm_get_height(dmp);
    if (w <= 0)
	w = 1;
    if (h <= 0)
	h = 1;
    *sx = 2.0 * 3.78 / (fastf_t)w;
    *sy = 2.0 * 3.78 / (fastf_t)h;
}

static void
_dm_annotation_display_point(struct dm *dmp,
	const struct bsg_render_item *item, const point_t local, point_t out)
{
    mat_t model2view;
    point_t anchor_view;
    point_t display_local;
    fastf_t sx, sy;

    bn_mat_mul(model2view, item->view->gv_model2view, item->model_mat);
    MAT4X3PNT(anchor_view, model2view, item->geometry.annotation.anchor);
    MAT4X3PNT(display_local, item->geometry.annotation.display_mat, local);
    _dm_annotation_display_scale(dmp, &sx, &sy);
    VSET(out, anchor_view[X] + display_local[X] * sx,
	    anchor_view[Y] + display_local[Y] * sy,
	    anchor_view[Z] + display_local[Z]);
}

static void
_dm_annotation_model_point(const struct bsg_render_item *item,
	const point_t local, point_t out)
{
    MAT4X3PNT(out, item->geometry.annotation.model_mat, local);
}

static void
_dm_annotation_text_extents(struct dm *dmp, const char *str, int size,
	fastf_t *width, fastf_t *height)
{
    int w = dm_get_width(dmp);
    int h = dm_get_height(dmp);
    if (w <= 0)
	w = 1;
    if (h <= 0)
	h = 1;
    if (size <= 0)
	size = dm_get_fontsize(dmp);
    if (size <= 0)
	size = 12;

    size_t len = str ? strlen(str) : 0;
    if (height)
	*height = 2.0 * (fastf_t)size / (fastf_t)h;
    if (width)
	*width = 2.0 * (fastf_t)size * 0.6 * (fastf_t)len / (fastf_t)w;
}

static void
_dm_annotation_adjust_text_position(struct dm *dmp,
	const struct bsg_annotation_segment *seg, point_t pos, int size)
{
    fastf_t width = 0.0;
    fastf_t height = 0.0;
    _dm_annotation_text_extents(dmp, seg->data.text.text, size, &width, &height);

    switch ((bsg_annotation_text_position)seg->data.text.relative_position) {
	case BSG_ANNOTATION_TEXT_POS_BOTTOM_LEFT:
	    break;
	case BSG_ANNOTATION_TEXT_POS_BOTTOM_CENTER:
	    pos[X] -= width * 0.5;
	    break;
	case BSG_ANNOTATION_TEXT_POS_BOTTOM_RIGHT:
	    pos[X] -= width;
	    break;
	case BSG_ANNOTATION_TEXT_POS_MIDDLE_LEFT:
	    pos[Y] -= height * 0.5;
	    break;
	case BSG_ANNOTATION_TEXT_POS_MIDDLE_CENTER:
	    pos[X] -= width * 0.5;
	    pos[Y] -= height * 0.5;
	    break;
	case BSG_ANNOTATION_TEXT_POS_MIDDLE_RIGHT:
	    pos[X] -= width;
	    pos[Y] -= height * 0.5;
	    break;
	case BSG_ANNOTATION_TEXT_POS_TOP_LEFT:
	    pos[Y] -= height;
	    break;
	case BSG_ANNOTATION_TEXT_POS_TOP_CENTER:
	    pos[X] -= width * 0.5;
	    pos[Y] -= height;
	    break;
	case BSG_ANNOTATION_TEXT_POS_TOP_RIGHT:
	    pos[X] -= width;
	    pos[Y] -= height;
	    break;
	case BSG_ANNOTATION_TEXT_POS_DEFAULT:
	default:
	    break;
    }
}

static void
_dm_annotation_draw_local_line(struct dm *dmp,
	const struct bsg_render_item *item, const point_t a, const point_t b)
{
    point_t da, db;
    if (item->geometry.annotation.space == BSG_ANNOTATION_SPACE_DISPLAY) {
	_dm_annotation_display_point(dmp, item, a, da);
	_dm_annotation_display_point(dmp, item, b, db);
	dm_draw_line_2d(dmp, da[X], da[Y], db[X], db[Y]);
	return;
    }

    _dm_annotation_model_point(item, a, da);
    _dm_annotation_model_point(item, b, db);
    dm_draw_line_3d(dmp, da, db);
}

static void
_dm_annotation_draw_line_segment(struct dm *dmp,
	const struct bsg_render_item *item,
	const struct bsg_annotation_segment *seg)
{
    point_t a, b;
    if (!_dm_annotation_local_point(item, seg->data.line.start, a) ||
	    !_dm_annotation_local_point(item, seg->data.line.end, b))
	return;
    _dm_annotation_draw_local_line(dmp, item, a, b);
}

static void
_dm_annotation_draw_carc_segment(struct dm *dmp,
	const struct bsg_render_item *item,
	const struct bsg_annotation_segment *seg)
{
    point_t start, end, center;
    if (!_dm_annotation_local_point(item, seg->data.carc.start, start) ||
	    !_dm_annotation_local_point(item, seg->data.carc.end, end))
	return;

    if (seg->data.carc.radius <= 0.0) {
	VMOVE(center, end);
	fastf_t radius = DIST_PNT_PNT(start, center);
	if (radius <= SMALL_FASTF)
	    return;
	fastf_t start_ang = atan2(start[Y] - center[Y], start[X] - center[X]);
	int nsegs = 32;
	point_t prev;
	VMOVE(prev, start);
	for (int i = 1; i <= nsegs; i++) {
	    fastf_t dir = seg->data.carc.orientation ? -1.0 : 1.0;
	    fastf_t ang = start_ang + dir * (2.0 * M_PI) *
		(fastf_t)i / (fastf_t)nsegs;
	    point_t next;
	    VSET(next, center[X] + radius * cos(ang),
		    center[Y] + radius * sin(ang), start[Z]);
	    _dm_annotation_draw_local_line(dmp, item, prev, next);
	    VMOVE(prev, next);
	}
	return;
    }

    fastf_t radius = seg->data.carc.radius;
    if (radius <= SMALL_FASTF)
	return;

    vect_t mid_pt, s2m, dir;
    VBLEND2(mid_pt, 0.5, start, 0.5, end);
    VSUB2(s2m, mid_pt, start);
    VSET(dir, -s2m[Y], s2m[X], 0.0);
    fastf_t s2m_len_sq = s2m[X] * s2m[X] + s2m[Y] * s2m[Y];
    if (s2m_len_sq <= SMALL_FASTF)
	return;
    fastf_t len_sq = radius * radius - s2m_len_sq;
    if (len_sq < 0.0)
	return;
    fastf_t dir_len = sqrt(dir[X] * dir[X] + dir[Y] * dir[Y]);
    if (dir_len <= SMALL_FASTF)
	return;
    VSCALE(dir, dir, 1.0 / dir_len);
    fastf_t offset = sqrt(len_sq);
    VJOIN1(center, mid_pt, offset, dir);

    fastf_t cross_z = (end[X] - start[X]) * (center[Y] - start[Y]) -
	(end[Y] - start[Y]) * (center[X] - start[X]);
    if (!(cross_z > 0.0 && seg->data.carc.center_is_left))
	VJOIN1(center, mid_pt, -offset, dir);

    fastf_t start_ang = atan2(start[Y] - center[Y], start[X] - center[X]);
    fastf_t end_ang = atan2(end[Y] - center[Y], end[X] - center[X]);
    if (seg->data.carc.orientation) {
	while (end_ang > start_ang)
	    end_ang -= 2.0 * M_PI;
    } else {
	while (end_ang < start_ang)
	    end_ang += 2.0 * M_PI;
    }
    fastf_t total_ang = end_ang - start_ang;
    fastf_t nsegs_est = ceil(fabs(total_ang) / (M_PI / 16.0));
    int nsegs = (int)nsegs_est;
    if (nsegs < 3)
	nsegs = 3;

    point_t prev;
    VMOVE(prev, start);
    for (int i = 1; i <= nsegs; i++) {
	fastf_t ang = start_ang + total_ang * (fastf_t)i / (fastf_t)nsegs;
	point_t next;
	VSET(next, center[X] + radius * cos(ang),
		center[Y] + radius * sin(ang), start[Z]);
	_dm_annotation_draw_local_line(dmp, item, prev, next);
	VMOVE(prev, next);
    }
}

static void
_dm_annotation_bezier_eval(point_t out, const point_t *controls, size_t n,
	fastf_t t)
{
    point_t *work = (point_t *)bu_calloc(n, sizeof(point_t),
	    "annotation bezier work");
    for (size_t i = 0; i < n; i++)
	VMOVE(work[i], controls[i]);
    for (size_t r = 1; r < n; r++) {
	for (size_t i = 0; i < n - r; i++) {
	    work[i][X] = (1.0 - t) * work[i][X] + t * work[i + 1][X];
	    work[i][Y] = (1.0 - t) * work[i][Y] + t * work[i + 1][Y];
	    work[i][Z] = (1.0 - t) * work[i][Z] + t * work[i + 1][Z];
	}
    }
    VMOVE(out, work[0]);
    bu_free(work, "annotation bezier work");
}

static void
_dm_annotation_draw_bezier_segment(struct dm *dmp,
	const struct bsg_render_item *item,
	const struct bsg_annotation_segment *seg)
{
    size_t n = seg->data.bezier.control_point_count;
    if (!n && seg->data.bezier.degree >= 0)
	n = (size_t)seg->data.bezier.degree + 1;
    if (n < 2 || !seg->data.bezier.control_points)
	return;

    point_t *controls = (point_t *)bu_calloc(n, sizeof(point_t),
	    "annotation bezier controls");
    for (size_t i = 0; i < n; i++) {
	if (!_dm_annotation_local_point(item, seg->data.bezier.control_points[i],
		    controls[i])) {
	    bu_free(controls, "annotation bezier controls");
	    return;
	}
    }

    int nsegs = 32;
    point_t prev;
    VMOVE(prev, controls[0]);
    for (int i = 1; i <= nsegs; i++) {
	point_t next;
	_dm_annotation_bezier_eval(next, (const point_t *)controls, n,
		(fastf_t)i / (fastf_t)nsegs);
	_dm_annotation_draw_local_line(dmp, item, prev, next);
	VMOVE(prev, next);
    }
    bu_free(controls, "annotation bezier controls");
}

static fastf_t
_dm_annotation_nurb_basis(size_t i, int degree, fastf_t t,
	const fastf_t *knots, size_t knot_count)
{
    if (!knots || degree < 0 || i + (size_t)degree + 1 >= knot_count)
	return 0.0;
    if (degree == 0) {
	if ((knots[i] <= t && t < knots[i + 1]) ||
		(NEAR_EQUAL(t, knots[knot_count - 1], SMALL_FASTF) &&
		 t >= knots[i] && t <= knots[i + 1]))
	    return 1.0;
	return 0.0;
    }

    fastf_t left = 0.0;
    fastf_t denom = knots[i + (size_t)degree] - knots[i];
    if (!NEAR_ZERO(denom, SMALL_FASTF))
	left = (t - knots[i]) / denom *
	    _dm_annotation_nurb_basis(i, degree - 1, t, knots, knot_count);

    fastf_t right = 0.0;
    denom = knots[i + (size_t)degree + 1] - knots[i + 1];
    if (!NEAR_ZERO(denom, SMALL_FASTF))
	right = (knots[i + (size_t)degree + 1] - t) / denom *
	    _dm_annotation_nurb_basis(i + 1, degree - 1, t, knots, knot_count);
    return left + right;
}

static int
_dm_annotation_nurb_eval(point_t out, const struct bsg_render_item *item,
	const struct bsg_annotation_segment *seg, fastf_t t)
{
    int degree = seg->data.nurb.order - 1;
    size_t n = seg->data.nurb.control_point_count;
    if (degree < 0 || n < 1 || !seg->data.nurb.control_points ||
	    !seg->data.nurb.knots || seg->data.nurb.knot_count < n + 1)
	return 0;

    VSETALL(out, 0.0);
    fastf_t denom = 0.0;
    for (size_t i = 0; i < n; i++) {
	point_t cp;
	if (!_dm_annotation_local_point(item, seg->data.nurb.control_points[i], cp))
	    return 0;
	fastf_t basis = _dm_annotation_nurb_basis(i, degree, t,
		seg->data.nurb.knots, seg->data.nurb.knot_count);
	fastf_t weight = seg->data.nurb.weights ? seg->data.nurb.weights[i] : 1.0;
	fastf_t scaled = basis * weight;
	VJOIN1(out, out, scaled, cp);
	denom += scaled;
    }
    if (seg->data.nurb.weights && !NEAR_ZERO(denom, SMALL_FASTF))
	VSCALE(out, out, 1.0 / denom);
    return 1;
}

static void
_dm_annotation_draw_nurb_segment(struct dm *dmp,
	const struct bsg_render_item *item,
	const struct bsg_annotation_segment *seg)
{
    size_t n = seg->data.nurb.control_point_count;
    if (n < 2 || !seg->data.nurb.control_points)
	return;
    if (seg->data.nurb.order < 3 || !seg->data.nurb.knots ||
	    seg->data.nurb.knot_count < 2) {
	point_t prev, next;
	if (!_dm_annotation_local_point(item, seg->data.nurb.control_points[0],
		    prev))
	    return;
	for (size_t i = 1; i < n; i++) {
	    if (!_dm_annotation_local_point(item,
			seg->data.nurb.control_points[i], next))
		return;
	    _dm_annotation_draw_local_line(dmp, item, prev, next);
	    VMOVE(prev, next);
	}
	return;
    }

    fastf_t start = seg->data.nurb.knots[0];
    fastf_t end = seg->data.nurb.knots[seg->data.nurb.knot_count - 1];
    if (end <= start)
	return;
    int nsegs = 32;
    point_t prev;
    if (!_dm_annotation_nurb_eval(prev, item, seg, start))
	return;
    for (int i = 1; i <= nsegs; i++) {
	point_t next;
	fastf_t t = start + (end - start) * (fastf_t)i / (fastf_t)nsegs;
	if (!_dm_annotation_nurb_eval(next, item, seg, t))
	    return;
	_dm_annotation_draw_local_line(dmp, item, prev, next);
	VMOVE(prev, next);
    }
}

static void
_dm_annotation_draw_text_segment(struct dm *dmp,
	const struct bsg_render_item *item,
	const struct bsg_annotation_segment *seg)
{
    if (!seg->data.text.text || !seg->data.text.text[0])
	return;

    point_t local = VINIT_ZERO;
    if (seg->data.text.ref_pt >= 0)
	(void)_dm_annotation_local_point(item, seg->data.text.ref_pt, local);

    int size = (int)seg->data.text.size;
    if (size <= 0)
	size = dm_get_fontsize(dmp);

    if (item->geometry.annotation.space == BSG_ANNOTATION_SPACE_DISPLAY) {
	point_t pos;
	_dm_annotation_display_point(dmp, item, local, pos);
	_dm_annotation_adjust_text_position(dmp, seg, pos, size);
	dm_draw_string_2d_rot(dmp, seg->data.text.text, pos[X], pos[Y],
		size, 1, seg->data.text.rotation);
	return;
    }

    mat_t model2view;
    point_t model_pt, pos;
    _dm_annotation_model_point(item, local, model_pt);
    bn_mat_mul(model2view, item->view->gv_model2view, item->model_mat);
    MAT4X3PNT(pos, model2view, model_pt);
    _dm_annotation_adjust_text_position(dmp, seg, pos, size);
    (void)dm_hud_begin(dmp);
    dm_draw_string_2d_rot(dmp, seg->data.text.text, pos[X], pos[Y],
	    size, 1, seg->data.text.rotation);
    (void)dm_hud_end(dmp);
}

static void
_dm_draw_annotation_resolved(struct dm *dmp, const struct bsg_render_item *item)
{
    if (!dmp || !item || !item->view)
	return;

    int ofontsize = dm_get_fontsize(dmp);
    dm_set_fg(dmp, item->appearance.color[0], item->appearance.color[1],
	    item->appearance.color[2], 1, 1.0);

    int display_space =
	item->geometry.annotation.space == BSG_ANNOTATION_SPACE_DISPLAY;
    if (display_space)
	(void)dm_hud_begin(dmp);

    for (size_t i = 0; i < item->geometry.annotation.segment_count; i++) {
	const struct bsg_annotation_segment *seg =
	    &item->geometry.annotation.segments[i];
	switch (seg->kind) {
	    case BSG_ANNOTATION_SEGMENT_LINE:
		_dm_annotation_draw_line_segment(dmp, item, seg);
		break;
	    case BSG_ANNOTATION_SEGMENT_CARC:
		_dm_annotation_draw_carc_segment(dmp, item, seg);
		break;
	    case BSG_ANNOTATION_SEGMENT_BEZIER:
		_dm_annotation_draw_bezier_segment(dmp, item, seg);
		break;
	    case BSG_ANNOTATION_SEGMENT_NURB:
		_dm_annotation_draw_nurb_segment(dmp, item, seg);
		break;
	    case BSG_ANNOTATION_SEGMENT_TEXT:
		dm_set_fontsize(dmp, (seg->data.text.size > 0) ?
			(int)seg->data.text.size : ofontsize);
		_dm_annotation_draw_text_segment(dmp, item, seg);
		break;
	    default:
		break;
	}
    }

    if (display_space)
	(void)dm_hud_end(dmp);
    dm_set_fontsize(dmp, ofontsize);
}

static int
_dm_item_is_axes_overlay(const struct bsg_render_item *item)
{
    if (!item || item->geometry.kind != BSG_RENDER_GEOMETRY_OVERLAY)
	return 0;
    return item->geometry.overlay.kind == BSG_RENDER_OVERLAY_GEOMETRY_AXES;
}

static int
_dm_item_is_grid_overlay(const struct bsg_render_item *item)
{
    return item && item->geometry.kind == BSG_RENDER_GEOMETRY_OVERLAY &&
	item->geometry.overlay.kind == BSG_RENDER_OVERLAY_GEOMETRY_GRID;
}

static void
_dm_hud_draw_item(void *dmp_ptr, const struct bsg_render_item *item)
{
    struct dm *dmp = (struct dm *)dmp_ptr;
    if (!dmp || !item || !item->view)
	return;

    struct bsg_view *v = item->view;

    if (item->geometry.kind == BSG_RENDER_GEOMETRY_LINE_SET) {
	_dm_draw_line_set_2d_arrays(dmp, item);
	return;
    }

    if (item->geometry.kind == BSG_RENDER_GEOMETRY_HUD) {
	_dm_draw_label_2d_item(dmp, item);
	return;
    }

    if (_dm_item_is_axes_overlay(item)) {
	_dm_draw_hud_axes_feature(dmp, v, &item->geometry.overlay.axes,
		item->features.hud_feature_type == BSG_HUD_FEATURE_MODEL_AXES);
	return;
    }

    if (_dm_item_is_grid_overlay(item)) {
	_dm_draw_hud_grid(dmp, v, &item->geometry.overlay.grid);
    }
}

static void
_dm_framebuffer_draw_item(void *dmp_ptr, const struct bsg_render_item *item)
{
    struct dm *dmp = (struct dm *)dmp_ptr;
    if (!dmp || !item)
	return;

    if (item->geometry.kind != BSG_RENDER_GEOMETRY_IMAGE ||
	    item->geometry.image.kind != BSG_RENDER_IMAGE_FRAMEBUFFER ||
	    item->geometry.image.framebuffer_mode == 0 ||
	    item->features.hud_feature_type != BSG_HUD_FEATURE_FRAMEBUFFER)
	return;

    _dm_draw_hud_framebuffer(dmp);
}


static int
_dm_hud_render_request(struct bsg_view *v, struct bsg_backend_adapter *adapter)
{
    if (!v || !v->dmp)
	return 0;
    if (bsg_hud_sync(v) != 0)
	return 0;

    struct bsg_render_request *req = bsg_render_request_create(v, v->dmp);
    if (!req)
	return 0;
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_HUD_PASS);
    bsg_render_request_set_adapter(req, adapter);
    struct bsg_render_batch *batch = bsg_render_batch_create();
    int ret = 0;
    if (batch) {
	ret = bsg_render_request_collect(req, batch);
	if (ret >= 0)
	    ret = bsg_render_batch_dispatch(req, batch);
	bsg_render_batch_destroy(batch);
    }
    bsg_render_request_destroy(req);
    return ret;
}

static int
_dm_scene_prepare_item(void *dmp_ptr, const struct bsg_render_item *item)
{
    (void)dmp_ptr;
    return item && item->geometry.kind != BSG_RENDER_GEOMETRY_NONE ? 1 : 0;
}

static void
_dm_scene_draw_item(void *dmp_ptr, const struct bsg_render_item *item)
{
    struct dm *dmp = (struct dm *)dmp_ptr;
    if (!dmp || !item)
	return;

    struct bsg_view *v = item->view;
    if (!v)
	return;

    if (dm_get_transparency(dmp))
	(void)dm_set_depth_mask(dmp, (item->phase == BSG_RENDER_PHASE_TRANSPARENT) ? 0 : 1);

    if (dm_get_bound_flag(dmp)
	&& !item->non_database_source
	&& v->gv_isize > 0
	&& item->bounds_radius > SMALL_FASTF
	&& (item->bounds_radius * v->gv_isize) < 0.001) {
	return;
    }

    mat_t model2view;
    bn_mat_mul(model2view, v->gv_model2view, item->model_mat);
    dm_loadmatrix(dmp, model2view, 0);
    if (item->appearance.highlighted && v->gv_edit_mat)
	dm_loadmatrix(dmp, v->gv_edit_mat, 0);

    if (item->appearance.highlighted) {
	(void)dm_set_fg(dmp, 255, 255, 255, 0, item->appearance.transparency);
    } else {
	/* Command overrides, geometry-default color, and base material color
	 * are resolved into item->appearance.color, so the backend reads the
	 * final color directly instead of re-deriving it. */
	(void)dm_set_fg(dmp, item->appearance.color[0], item->appearance.color[1], item->appearance.color[2], 0, item->appearance.transparency);
    }

    int lw = item->appearance.line_width;
    if (lw <= 0)
	lw = dm_get_linewidth(dmp);
    (void)dm_set_line_attr(dmp, lw, item->appearance.line_style);

    (void)dm_backend_draw_item(dmp, item);

    dm_add_item_arrows(dmp, item);
    if (_dm_item_is_axes_overlay(item))
	dm_draw_scene_axes_payload(dmp, &item->geometry.overlay.axes);
    if (item->geometry.kind == BSG_RENDER_GEOMETRY_TEXT)
	_dm_draw_label_resolved(dmp, item);
    if (item->geometry.kind == BSG_RENDER_GEOMETRY_ANNOTATION)
	_dm_draw_annotation_resolved(dmp, item);
}

static void
_dm_scene_invalidate_item(void *dmp_ptr, const struct bsg_render_item *item,
			   unsigned int reason_mask)
{
    struct dm *dmp = (struct dm *)dmp_ptr;
    if (!dmp || !item)
	return;
    dm_backend_invalidate_item(dmp, item, reason_mask);
}

static void
_dm_scene_free_item(void *dmp_ptr, const struct bsg_render_item *item)
{
    struct dm *dmp = (struct dm *)dmp_ptr;
    if (!dmp || !item)
	return;
    dm_backend_release_source(dmp, item->source.source_id ? item->source.source_id : item->geometry.source_identity);
}

static unsigned int
_dm_scene_capabilities(void *UNUSED(dmp_ptr))
{
    return BSG_ADAPTER_CAP_TRANSPARENCY |
	   BSG_ADAPTER_CAP_WIREFRAME |
	   BSG_ADAPTER_CAP_SHADED |
	   BSG_ADAPTER_CAP_HUD |
	   BSG_ADAPTER_CAP_SORTED_ALPHA;
}

static int
_dm_scene_begin_request(void *dmp_ptr, struct bsg_render_request *req)
{
    struct dm *dmp = (struct dm *)dmp_ptr;
    if (!dmp || !req)
	return 0;

    unsigned char *bg1 = NULL;
    unsigned char *bg2 = NULL;
    (void)dm_get_bg(&bg1, &bg2, dmp);
    bsg_render_request_set_background(req, bg1, bg2);

    unsigned char *gdc = dm_get_geometry_default_color(dmp);
    bsg_render_request_set_geometry_default_color(req, gdc);
    vect_t *clipmin = dm_get_clipmin(dmp);
    vect_t *clipmax = dm_get_clipmax(dmp);
    bsg_render_request_set_clip(req, clipmin, clipmax, dm_get_zclip(dmp));
    bsg_render_request_set_lighting(req, dm_get_light(dmp));
    return 1;
}

static void
_dm_scene_end_request(void *UNUSED(dmp_ptr), const struct bsg_render_request *UNUSED(req))
{
}


void
dm_draw_faceplate(struct bsg_view *v)
{
    static struct bsg_backend_adapter hud_adapter = {NULL, NULL, _dm_hud_draw_item, NULL, NULL, NULL, NULL};
    (void)_dm_hud_render_request(v, &hud_adapter);
}

static void
_dm_draw_label_resolved(struct dm *dmp, const struct bsg_render_item *item)
{
    if (!dmp || !item || !item->geometry.text.label)
	return;
    struct bsg_view *v = item->view;
    if (!v)
	return;

    (void)dm_set_fg(dmp, item->appearance.color[0], item->appearance.color[1], item->appearance.color[2], 1, item->appearance.transparency);

    point_t vpoint;
    MAT4X3PNT(vpoint, v->gv_model2view, item->geometry.text.position);

    // Check that we can calculate the bbox before drawing text
    vect2d_t bmin = V2INIT_ZERO;
    vect2d_t bmax = V2INIT_ZERO;
    (void)dm_hud_begin(dmp);
    int txt_ok = dm_string_bbox_2d(dmp, &bmin, &bmax,
	    item->geometry.text.label, vpoint[X], vpoint[Y], 1, 1);
    (void)dm_hud_end(dmp);

    // Have bbox - go ahead and write the label
    if (txt_ok == BRLCAD_OK) {
	(void)dm_hud_begin(dmp);
	(void)dm_draw_string_2d(dmp, item->geometry.text.label,
		vpoint[X], vpoint[Y], 0, 1);
	(void)dm_hud_end(dmp);
    }

    if (!item->geometry.text.line_flag)
	return;

    point_t l3d = VINIT_ZERO;
    point_t mpt = VINIT_ZERO;

    if (txt_ok == BRLCAD_OK) {
	vect2d_t bmid;
	bmid[0] = (bmax[0] - bmin[0]) * 0.5 + bmin[0];
	bmid[1] = (bmax[1] - bmin[1]) * 0.5 + bmin[1];

	vect2d_t anchor = V2INIT_ZERO;
	if (item->geometry.text.anchor == BSG_ANCHOR_AUTO) {
	    fastf_t xvals[3];
	    fastf_t yvals[3];
	    xvals[0] = bmin[0];
	    xvals[1] = bmid[0];
	    xvals[2] = bmax[0];
	    yvals[0] = bmin[1];
	    yvals[1] = bmid[1];
	    yvals[2] = bmax[1];
	    fastf_t closest_dist = MAX_FASTF;
	    for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
		    point_t t3d, tpt;
		    if (bsg_screen_to_view(v, &t3d[0], &t3d[1], (int)xvals[i], (int)yvals[j]) < 0) {
			return;
		    }
		    t3d[2] = 0;
		    MAT4X3PNT(tpt, v->gv_view2model, t3d);
		    double dsq = DIST_PNT_PNT_SQ(tpt, item->geometry.text.target);
		    if (dsq < closest_dist) {
			V2SET(anchor, xvals[i], yvals[j]);
			closest_dist = dsq;
		    }
		}
	    }
	} else {
	    switch (item->geometry.text.anchor) {
		case BSG_ANCHOR_BOTTOM_LEFT:
		    V2SET(anchor, bmin[0], bmin[1]);
		    break;
		case BSG_ANCHOR_BOTTOM_CENTER:
		    V2SET(anchor, bmid[0], bmin[1]);
		    break;
		case BSG_ANCHOR_BOTTOM_RIGHT:
		    V2SET(anchor, bmax[0], bmin[1]);
		    break;
		case BSG_ANCHOR_MIDDLE_LEFT:
		    V2SET(anchor, bmin[0], bmid[1]);
		    break;
		case BSG_ANCHOR_MIDDLE_CENTER:
		    V2SET(anchor, bmid[0], bmid[1]);
		    break;
		case BSG_ANCHOR_MIDDLE_RIGHT:
		    V2SET(anchor, bmax[0], bmid[1]);
		    break;
		case BSG_ANCHOR_TOP_LEFT:
		    V2SET(anchor, bmin[0], bmax[1]);
		    break;
		case BSG_ANCHOR_TOP_CENTER:
		    V2SET(anchor, bmid[0], bmax[1]);
		    break;
		case BSG_ANCHOR_TOP_RIGHT:
		    V2SET(anchor, bmax[0], bmax[1]);
		    break;
		default:
		    bu_log("Unhandled anchor case: %d\n", item->geometry.text.anchor);
		    return;
	    }
	}
	bsg_screen_to_view(v, &l3d[0], &l3d[1], (int)anchor[0], (int)anchor[1]);
	MAT4X3PNT(mpt, v->gv_view2model, l3d);
    } else {
	VMOVE(mpt, item->geometry.text.position);
    }

    point_t target;
    VMOVE(target, item->geometry.text.target);
    if (item->geometry.text.arrow) {
	dm_draw_arrow(dmp, mpt, target, item->features.arrow_tip_length, item->features.arrow_tip_width, 1.0);
    } else {
	dm_draw_line_3d(dmp, mpt, target);
    }
}

// Interactive visuals arrive as retained payload and overlay records before
// libdm resolves the view to render items.
void
dm_draw_objs(struct bsg_view *v)
{
    bsg_log(3, "libdm:dm_draw_objs");

    struct dm *dmp = (struct dm *)v->dmp;
    if (!dmp) {
	bu_log("Warning - dm_draw_objs called when view has no associated display manager\n");
	return;
    }

    /* Bump the frame generation counter.  Every shape rendered below stamps
     * its drawn revision to this value, so callers can detect "drawn this
     * frame" by equality against v->gv_frame_rev. */
    v->gv_frame_rev++;

    // This is the start of a draw cycle - start the stopwatch to time the
    // frame.  If the faceplate fps display is enabled, the faceplate draw at
    // the end of the cycle will need this start time.
    dmp->start_time = bu_gettime();
    (void)dm_backend_begin_frame(dmp);

    // If we're drawing the framebuffer, that's the first order of business.
    // The rest of the drawing layers manipulate the OpenGL view and projection
    // matrices, but the framebuffer is always aligned to the view.  We also
    // can't have the zbuffer enabled or the fb image won't draw correctly.
    int fb_mode = bsg_view_framebuffer_mode(v);
    if (fb_mode && dm_get_fb(dmp)) {
	static struct bsg_backend_adapter framebuffer_adapter = {NULL, NULL, _dm_framebuffer_draw_item, NULL, NULL, NULL, NULL};
	(void)_dm_hud_render_request(v, &framebuffer_adapter);
	if (fb_mode == 1) {
	    // In overlay mode, it's just the fb - skip all the rest
	    dm_backend_end_frame(dmp);
	    return;
	}
    }

    // On to the scene objects - for drawing those we need the view matrix
    matp_t mat = v->gv_model2view;
    dm_loadmatrix(dmp, mat, 0);


    // Set up to render using current perspective settings
    if (SMALL_FASTF < v->gv_perspective)
	(void)dm_loadpmatrix(dmp, v->gv_pmat);
    else {
	(void)dm_loadpmatrix(dmp, NULL);
    }


    static struct bsg_backend_adapter scene_adapter = {
	_dm_scene_begin_request,
	_dm_scene_prepare_item,
	_dm_scene_draw_item,
	_dm_scene_invalidate_item,
	_dm_scene_free_item,
	_dm_scene_capabilities,
	_dm_scene_end_request
    };
    struct bsg_render_request *req = bsg_render_request_create(v, dmp);
    if (req) {
	bsg_render_request_set_flags(req, BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE);
	if (dm_get_transparency(dmp))
	    bsg_render_request_add_flags(req, BSG_RENDER_FLAG_SORTED_ALPHA);
	/* Hand the backend's geometry-default color to the render settings so
	 * bsg_appearance_resolve models the default-color layer and the scene
	 * adapter can read the resolved color directly from item->appearance.color. */
	bsg_render_request_set_geometry_default_color(req, dm_get_geometry_default_color(dmp));
	bsg_render_request_set_adapter(req, &scene_adapter);
	struct bsg_render_batch *batch = bsg_render_batch_create();
	if (batch) {
	    if (bsg_render_request_collect(req, batch) >= 0)
		(void)bsg_render_batch_dispatch(req, batch);
	    bsg_render_batch_destroy(batch);
	}
	bsg_render_request_destroy(req);
	(void)dm_set_depth_mask(dmp, 1);
    }

    // Done with perspective/orthogonal drawing
    dm_pop_pmatrix(dmp);

    /* And finally, faceplate.  Set up matrices for HUD drawing, rather than 3D
     * scene drawing. */
    (void)dm_hud_begin(dmp);

    /* Draw faceplate elements based on their current enable/disable settings */
    dm_draw_faceplate(v);

    /* Restore non-HUD settings. */
    (void)dm_hud_end(dmp);

    dm_backend_end_frame(dmp);
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
