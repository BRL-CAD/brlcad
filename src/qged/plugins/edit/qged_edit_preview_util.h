/*             Q G E D _ E D I T _ P R E V I E W _ U T I L . H
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

#ifndef QGED_EDIT_PREVIEW_UTIL_H
#define QGED_EDIT_PREVIEW_UTIL_H

#include "common.h"

#include <math.h>

#include "bg/defines.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bsg/feature.h"
#include "bsg/geometry.h"
#include "vmath.h"
#include "rt/db_internal.h"
#include "rt/geom.h"


#define QGED_EDIT_ELL_PREVIEW_SEGMENTS 64
#define QGED_EDIT_CURVE_PREVIEW_SEGMENTS 64
#define QGED_EDIT_CURVE_PREVIEW_MAX_SEGMENTS 256


struct qged_edit_preview_lines {
    point_t *points;
    int *cmds;
    size_t count;
    size_t capacity;
};


static inline void
qged_edit_preview_lines_init(struct qged_edit_preview_lines *lines)
{
    if (!lines)
	return;
    lines->points = NULL;
    lines->cmds = NULL;
    lines->count = 0;
    lines->capacity = 0;
}


static inline void
qged_edit_preview_lines_free(struct qged_edit_preview_lines *lines)
{
    if (!lines)
	return;
    if (lines->points)
	bu_free(lines->points, "qged edit preview typed points");
    if (lines->cmds)
	bu_free(lines->cmds, "qged edit preview typed commands");
    qged_edit_preview_lines_init(lines);
}


static inline int
qged_edit_preview_lines_reserve(struct qged_edit_preview_lines *lines,
				size_t needed)
{
    if (!lines)
	return 0;
    if (needed <= lines->capacity)
	return 1;

    size_t new_capacity = lines->capacity ? lines->capacity : 64;
    while (new_capacity < needed)
	new_capacity *= 2;

    lines->points = (point_t *)bu_realloc(lines->points,
	    new_capacity * sizeof(point_t),
	    "qged edit preview typed points");
    lines->cmds = (int *)bu_realloc(lines->cmds,
	    new_capacity * sizeof(int),
	    "qged edit preview typed commands");
    lines->capacity = new_capacity;
    return 1;
}


static inline int
qged_edit_preview_lines_append(struct qged_edit_preview_lines *lines,
			       const point_t pt,
			       int cmd)
{
    if (!qged_edit_preview_lines_reserve(lines, lines->count + 1))
	return 0;
    VMOVE(lines->points[lines->count], pt);
    lines->cmds[lines->count] = cmd;
    lines->count++;
    return 1;
}


static inline int
qged_edit_preview_lines_append_line(struct qged_edit_preview_lines *lines,
				    const point_t a,
				    const point_t b)
{
    return qged_edit_preview_lines_append(lines, a, BSG_GEOMETRY_LINE_MOVE) &&
	qged_edit_preview_lines_append(lines, b, BSG_GEOMETRY_LINE_DRAW);
}


static inline int
qged_edit_preview_lines_replace(bsg_feature_ref ref,
				enum bsg_feature_family family,
				const struct qged_edit_preview_lines *lines)
{
    size_t count = lines ? lines->count : 0;
    return bsg_feature_points_replace(ref, family,
	    count ? (const point_t *)lines->points : NULL,
	    count ? lines->cmds : NULL,
	    count);
}


static inline int
qged_edit_feature_clear_geometry(bsg_feature_ref ref)
{
    return bsg_feature_points_replace(ref, BSG_FEATURE_UNKNOWN, NULL, NULL, 0);
}


static inline int
qged_edit_feature_replace_bot_face_lines(bsg_feature_ref ref,
					 enum bsg_feature_family family,
					 const struct rt_bot_internal *bot)
{
    if (!bot)
	return 0;
    RT_BOT_CK_MAGIC(bot);

    if (!bot->num_faces || !bot->faces || !bot->num_vertices || !bot->vertices)
	return bsg_feature_points_replace(ref, family, NULL, NULL, 0);

    size_t point_count = bot->num_faces * 4;
    point_t *points = (point_t *)bu_calloc(point_count, sizeof(point_t),
	    "qged BOT edit preview points");
    int *cmds = (int *)bu_calloc(point_count, sizeof(int),
	    "qged BOT edit preview commands");
    size_t point_idx = 0;

    for (size_t i = 0; i < bot->num_faces; i++) {
	int f0 = bot->faces[i * 3 + 0];
	int f1 = bot->faces[i * 3 + 1];
	int f2 = bot->faces[i * 3 + 2];

	if (f0 < 0 || f1 < 0 || f2 < 0 ||
		(size_t)f0 >= bot->num_vertices ||
		(size_t)f1 >= bot->num_vertices ||
		(size_t)f2 >= bot->num_vertices) {
	    bu_free(points, "qged BOT edit preview points");
	    bu_free(cmds, "qged BOT edit preview commands");
	    return 0;
	}

	const fastf_t *p0 = &bot->vertices[(size_t)f0 * 3];
	const fastf_t *p1 = &bot->vertices[(size_t)f1 * 3];
	const fastf_t *p2 = &bot->vertices[(size_t)f2 * 3];

	VMOVE(points[point_idx], p0);
	cmds[point_idx++] = BSG_GEOMETRY_LINE_MOVE;
	VMOVE(points[point_idx], p1);
	cmds[point_idx++] = BSG_GEOMETRY_LINE_DRAW;
	VMOVE(points[point_idx], p2);
	cmds[point_idx++] = BSG_GEOMETRY_LINE_DRAW;
	VMOVE(points[point_idx], p0);
	cmds[point_idx++] = BSG_GEOMETRY_LINE_DRAW;
    }

    int ret = bsg_feature_points_replace(ref, family,
	    (const point_t *)points, cmds, point_idx);
    bu_free(points, "qged BOT edit preview points");
    bu_free(cmds, "qged BOT edit preview commands");
    return ret;
}


static inline void
qged_edit_ell_append_loop(point_t *points,
			  int *cmds,
			  size_t *point_idx,
			  const struct rt_ell_internal *ell,
			  const vect_t axis_a,
			  const vect_t axis_b)
{
    if (!points || !cmds || !point_idx || !ell)
	return;

    for (size_t i = 0; i <= QGED_EDIT_ELL_PREVIEW_SEGMENTS; i++) {
	double theta = M_2PI * (double)i / (double)QGED_EDIT_ELL_PREVIEW_SEGMENTS;
	VJOIN2(points[*point_idx], ell->v, cos(theta), axis_a,
		sin(theta), axis_b);
	cmds[*point_idx] = (i == 0) ? BSG_GEOMETRY_LINE_MOVE :
	    BSG_GEOMETRY_LINE_DRAW;
	(*point_idx)++;
    }
}


static inline int
qged_edit_feature_replace_ell_wireframe(bsg_feature_ref ref,
					enum bsg_feature_family family,
					const struct rt_ell_internal *ell)
{
    if (!ell)
	return 0;
    RT_ELL_CK_MAGIC(ell);

    size_t point_count = 3 * (QGED_EDIT_ELL_PREVIEW_SEGMENTS + 1);
    point_t *points = (point_t *)bu_calloc(point_count, sizeof(point_t),
	    "qged ELL edit preview points");
    int *cmds = (int *)bu_calloc(point_count, sizeof(int),
	    "qged ELL edit preview commands");
    size_t point_idx = 0;

    qged_edit_ell_append_loop(points, cmds, &point_idx, ell, ell->a, ell->b);
    qged_edit_ell_append_loop(points, cmds, &point_idx, ell, ell->a, ell->c);
    qged_edit_ell_append_loop(points, cmds, &point_idx, ell, ell->b, ell->c);

    int ret = bsg_feature_points_replace(ref, family,
	    (const point_t *)points, cmds, point_idx);
    bu_free(points, "qged ELL edit preview points");
    bu_free(cmds, "qged ELL edit preview commands");
    return ret;
}


static inline int
qged_edit_sketch_vertex_valid(const struct rt_sketch_internal *sketch,
			      int idx)
{
    return sketch && idx >= 0 && (size_t)idx < sketch->vert_count;
}


static inline void
qged_edit_sketch_point(point_t pt,
		       const point_t V,
		       const vect_t u_vec,
		       const vect_t v_vec,
		       const struct rt_sketch_internal *sketch,
		       int idx)
{
    VJOIN2(pt, V, sketch->verts[idx][0], u_vec, sketch->verts[idx][1], v_vec);
}


static inline size_t
qged_edit_curve_segments_for_span(const struct bg_tess_tol *ttol,
				  fastf_t span)
{
    size_t segments = QGED_EDIT_CURVE_PREVIEW_SEGMENTS;

    if (span > SMALL_FASTF && ttol) {
	if (ttol->abs > SMALL_FASTF) {
	    size_t abs_segments = (size_t)ceil(span / ttol->abs);
	    if (abs_segments > segments)
		segments = abs_segments;
	}
	if (ttol->rel > SMALL_FASTF && ttol->rel < 1.0) {
	    size_t rel_segments = (size_t)ceil(1.0 / ttol->rel);
	    if (rel_segments > segments)
		segments = rel_segments;
	}
    }

    if (segments < 4)
	segments = 4;
    if (segments > QGED_EDIT_CURVE_PREVIEW_MAX_SEGMENTS)
	segments = QGED_EDIT_CURVE_PREVIEW_MAX_SEGMENTS;
    return segments;
}


static inline fastf_t
qged_edit_arc_delta(const struct bg_tess_tol *ttol, fastf_t radius)
{
    fastf_t delta = M_PI_4;

    if (radius <= SMALL_FASTF)
	return delta;

    if (ttol && ttol->abs > 0.0) {
	fastf_t ratio = ttol->abs / radius;
	if (ratio < 1.0) {
	    fastf_t tmp_delta = 2.0 * acos(1.0 - ratio);
	    if (tmp_delta < delta)
		delta = tmp_delta;
	}
    }
    if (ttol && ttol->rel > 0.0 && ttol->rel < 1.0) {
	fastf_t tmp_delta = 2.0 * acos(1.0 - ttol->rel);
	if (tmp_delta < delta)
	    delta = tmp_delta;
    }
    if (ttol && ttol->norm > 0.0) {
	fastf_t normal = ttol->norm * DEG2RAD;
	if (normal < delta)
	    delta = normal;
    }

    if (delta <= SMALL_FASTF)
	delta = M_PI_4;
    return delta;
}


static inline int
qged_edit_sketch_append_line_seg(struct qged_edit_preview_lines *lines,
				 const point_t V,
				 const vect_t u_vec,
				 const vect_t v_vec,
				 const struct rt_sketch_internal *sketch,
				 const struct line_seg *lsg)
{
    point_t start_pt, end_pt;

    if (!qged_edit_sketch_vertex_valid(sketch, lsg->start) ||
	    !qged_edit_sketch_vertex_valid(sketch, lsg->end))
	return 0;

    qged_edit_sketch_point(start_pt, V, u_vec, v_vec, sketch, lsg->start);
    qged_edit_sketch_point(end_pt, V, u_vec, v_vec, sketch, lsg->end);
    return qged_edit_preview_lines_append_line(lines, start_pt, end_pt);
}


static inline int
qged_edit_sketch_append_carc_seg(struct qged_edit_preview_lines *lines,
				 const struct bg_tess_tol *ttol,
				 const point_t V,
				 const vect_t u_vec,
				 const vect_t v_vec,
				 const struct rt_sketch_internal *sketch,
				 const struct carc_seg *csg)
{
    point2d_t mid_pt, start2d, end2d, center2d, s2m, dir;
    point_t center = VINIT_ZERO;
    point_t start_pt = VINIT_ZERO;
    point_t pt = VINIT_ZERO;
    vect_t semi_a = VINIT_ZERO;
    vect_t semi_b = VINIT_ZERO;
    vect_t norm = VINIT_ZERO;
    fastf_t radius = csg->radius;
    fastf_t delta;
    fastf_t s2m_len_sq, len_sq, tmp_len, cross_z;
    fastf_t start_ang, end_ang, tot_ang, cosdel, sindel;
    fastf_t oldu, oldv, newu, newv;
    int nsegs;

    if (!qged_edit_sketch_vertex_valid(sketch, csg->start) ||
	    !qged_edit_sketch_vertex_valid(sketch, csg->end))
	return 0;

    if (radius <= 0.0) {
	qged_edit_sketch_point(center, V, u_vec, v_vec, sketch, csg->end);
	qged_edit_sketch_point(pt, V, u_vec, v_vec, sketch, csg->start);
	VSUB2(semi_a, pt, center);
	VCROSS(norm, u_vec, v_vec);
	VCROSS(semi_b, norm, semi_a);
	VUNITIZE(semi_b);
	radius = MAGNITUDE(semi_a);
	if (radius <= SMALL_FASTF)
	    return 0;
	VSCALE(semi_b, semi_b, radius);
    } else if (radius <= SMALL_FASTF) {
	return 0;
    }

    delta = qged_edit_arc_delta(ttol, radius);

    if (csg->radius <= 0.0) {
	nsegs = (int)ceil(M_2PI / delta);
	if (nsegs < 4)
	    nsegs = 4;
	delta = M_2PI / (double)nsegs;
	cosdel = cos(delta);
	sindel = sin(delta);
	oldu = 1.0;
	oldv = 0.0;
	VJOIN2(start_pt, center, oldu, semi_a, oldv, semi_b);
	if (!qged_edit_preview_lines_append(lines, start_pt,
		BSG_GEOMETRY_LINE_MOVE))
	    return 0;
	for (int i = 1; i < nsegs; i++) {
	    newu = oldu * cosdel - oldv * sindel;
	    newv = oldu * sindel + oldv * cosdel;
	    VJOIN2(pt, center, newu, semi_a, newv, semi_b);
	    if (!qged_edit_preview_lines_append(lines, pt,
		    BSG_GEOMETRY_LINE_DRAW))
		return 0;
	    oldu = newu;
	    oldv = newv;
	}
	return qged_edit_preview_lines_append(lines, start_pt,
		BSG_GEOMETRY_LINE_DRAW);
    }

    V2MOVE(start2d, sketch->verts[csg->start]);
    V2MOVE(end2d, sketch->verts[csg->end]);
    mid_pt[0] = (start2d[0] + end2d[0]) * 0.5;
    mid_pt[1] = (start2d[1] + end2d[1]) * 0.5;
    V2SUB2(s2m, mid_pt, start2d);
    dir[0] = -s2m[1];
    dir[1] = s2m[0];
    s2m_len_sq = s2m[0] * s2m[0] + s2m[1] * s2m[1];
    if (s2m_len_sq <= SMALL_FASTF)
	return 0;
    len_sq = radius * radius - s2m_len_sq;
    if (len_sq < 0.0)
	return 0;
    tmp_len = sqrt(dir[0] * dir[0] + dir[1] * dir[1]);
    if (tmp_len <= SMALL_FASTF)
	return 0;
    dir[0] = dir[0] / tmp_len;
    dir[1] = dir[1] / tmp_len;
    tmp_len = sqrt(len_sq);
    V2JOIN1(center2d, mid_pt, tmp_len, dir);

    cross_z = (end2d[X] - start2d[X]) * (center2d[Y] - start2d[Y]) -
	(end2d[Y] - start2d[Y]) * (center2d[X] - start2d[X]);
    if (!(cross_z > 0.0 && csg->center_is_left))
	V2JOIN1(center2d, mid_pt, -tmp_len, dir);
    start_ang = atan2(start2d[Y] - center2d[Y], start2d[X] - center2d[X]);
    end_ang = atan2(end2d[Y] - center2d[Y], end2d[X] - center2d[X]);
    if (csg->orientation) {
	while (end_ang > start_ang)
	    end_ang -= M_2PI;
    } else {
	while (end_ang < start_ang)
	    end_ang += M_2PI;
    }
    tot_ang = end_ang - start_ang;
    nsegs = (int)ceil(tot_ang / delta);
    if (nsegs < 0)
	nsegs = -nsegs;
    if (nsegs < 3)
	nsegs = 3;
    delta = tot_ang / nsegs;
    cosdel = cos(delta);
    sindel = sin(delta);
    VJOIN2(center, V, center2d[0], u_vec, center2d[1], v_vec);
    VJOIN2(start_pt, V, start2d[0], u_vec, start2d[1], v_vec);
    oldu = (start2d[0] - center2d[0]);
    oldv = (start2d[1] - center2d[1]);
    if (!qged_edit_preview_lines_append(lines, start_pt,
	    BSG_GEOMETRY_LINE_MOVE))
	return 0;
    for (int i = 0; i < nsegs; i++) {
	newu = oldu * cosdel - oldv * sindel;
	newv = oldu * sindel + oldv * cosdel;
	VJOIN2(pt, center, newu, u_vec, newv, v_vec);
	if (!qged_edit_preview_lines_append(lines, pt,
		BSG_GEOMETRY_LINE_DRAW))
	    return 0;
	oldu = newu;
	oldv = newv;
    }

    return 1;
}


static inline int
qged_edit_sketch_append_bezier_seg(struct qged_edit_preview_lines *lines,
				   const struct bg_tess_tol *ttol,
				   const point_t V,
				   const vect_t u_vec,
				   const vect_t v_vec,
				   const struct rt_sketch_internal *sketch,
				   const struct bezier_seg *bsg)
{
    point2d_t *ctl = NULL;
    point2d_t *work = NULL;
    fastf_t span = 0.0;
    size_t segments;
    int ret = 0;

    if (bsg->degree < 1)
	return 0;
    for (int i = 0; i <= bsg->degree; i++) {
	if (!qged_edit_sketch_vertex_valid(sketch, bsg->ctl_points[i]))
	    return 0;
    }

    ctl = (point2d_t *)bu_calloc((size_t)bsg->degree + 1, sizeof(point2d_t),
	    "qged edit preview bezier ctl");
    work = (point2d_t *)bu_calloc((size_t)bsg->degree + 1, sizeof(point2d_t),
	    "qged edit preview bezier work");
    for (int i = 0; i <= bsg->degree; i++) {
	V2MOVE(ctl[i], sketch->verts[bsg->ctl_points[i]]);
	if (i > 0) {
	    point2d_t diff;
	    V2SUB2(diff, ctl[i], ctl[i - 1]);
	    span += sqrt(MAG2SQ(diff));
	}
    }

    segments = qged_edit_curve_segments_for_span(ttol, span);
    for (size_t si = 0; si <= segments; si++) {
	fastf_t t = (fastf_t)si / (fastf_t)segments;
	point_t pt;

	for (int i = 0; i <= bsg->degree; i++)
	    V2MOVE(work[i], ctl[i]);
	for (int r = 1; r <= bsg->degree; r++) {
	    for (int i = 0; i <= bsg->degree - r; i++) {
		work[i][X] = (1.0 - t) * work[i][X] + t * work[i + 1][X];
		work[i][Y] = (1.0 - t) * work[i][Y] + t * work[i + 1][Y];
	    }
	}
	VJOIN2(pt, V, work[0][X], u_vec, work[0][Y], v_vec);
	if (!qged_edit_preview_lines_append(lines, pt,
		si ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE))
	    goto cleanup;
    }
    ret = 1;

cleanup:
    bu_free(ctl, "qged edit preview bezier ctl");
    bu_free(work, "qged edit preview bezier work");
    return ret;
}


static inline fastf_t
qged_edit_nurb_basis(const fastf_t *knots,
		     int k_size,
		     int i,
		     int degree,
		     fastf_t t,
		     fastf_t domain_end)
{
    if (!knots || i < 0 || i + 1 >= k_size)
	return 0.0;

    if (degree == 0) {
	if ((knots[i] <= t && t < knots[i + 1]) ||
		(fabs(t - domain_end) <= SMALL_FASTF &&
		 knots[i] <= t && t <= knots[i + 1]))
	    return 1.0;
	return 0.0;
    }

    fastf_t left = 0.0;
    fastf_t right = 0.0;
    if (i + degree < k_size) {
	fastf_t denom = knots[i + degree] - knots[i];
	if (fabs(denom) > SMALL_FASTF)
	    left = ((t - knots[i]) / denom) *
		qged_edit_nurb_basis(knots, k_size, i, degree - 1, t,
			domain_end);
    }
    if (i + degree + 1 < k_size) {
	fastf_t denom = knots[i + degree + 1] - knots[i + 1];
	if (fabs(denom) > SMALL_FASTF)
	    right = ((knots[i + degree + 1] - t) / denom) *
		qged_edit_nurb_basis(knots, k_size, i + 1, degree - 1, t,
			domain_end);
    }

    return left + right;
}


static inline int
qged_edit_nurb_eval2d(point2d_t out,
		      const struct rt_sketch_internal *sketch,
		      const struct nurb_seg *nsg,
		      fastf_t t,
		      fastf_t domain_end)
{
    int degree = nsg->order - 1;
    int rational = nsg->pt_type & 0x1;
    fastf_t denom = 0.0;

    V2SETALL(out, 0.0);

    for (int i = 0; i < nsg->c_size; i++) {
	fastf_t basis, weight, coeff;
	if (!qged_edit_sketch_vertex_valid(sketch, nsg->ctl_points[i]))
	    return 0;
	basis = qged_edit_nurb_basis(nsg->k.knots, nsg->k.k_size, i,
		degree, t, domain_end);
	weight = (rational && nsg->weights) ? nsg->weights[i] : 1.0;
	coeff = basis * weight;
	out[X] += coeff * sketch->verts[nsg->ctl_points[i]][X];
	out[Y] += coeff * sketch->verts[nsg->ctl_points[i]][Y];
	denom += coeff;
    }

    if (fabs(denom) <= SMALL_FASTF)
	return 0;
    out[X] /= denom;
    out[Y] /= denom;
    return 1;
}


static inline int
qged_edit_sketch_append_nurb_seg(struct qged_edit_preview_lines *lines,
				 const struct bg_tess_tol *ttol,
				 const point_t V,
				 const vect_t u_vec,
				 const vect_t v_vec,
				 const struct rt_sketch_internal *sketch,
				 const struct nurb_seg *nsg)
{
    fastf_t domain_start, domain_end, span = 0.0;
    size_t segments;

    if (nsg->c_size < 1 || nsg->order < 1)
	return 0;
    for (int i = 0; i < nsg->c_size; i++) {
	if (!qged_edit_sketch_vertex_valid(sketch, nsg->ctl_points[i]))
	    return 0;
	if (i > 0) {
	    point2d_t diff;
	    V2SUB2(diff, sketch->verts[nsg->ctl_points[i]],
		    sketch->verts[nsg->ctl_points[i - 1]]);
	    span += sqrt(MAG2SQ(diff));
	}
    }

    if (nsg->order < 3) {
	for (int i = 0; i < nsg->c_size; i++) {
	    point_t pt;
	    qged_edit_sketch_point(pt, V, u_vec, v_vec, sketch,
		    nsg->ctl_points[i]);
	    if (!qged_edit_preview_lines_append(lines, pt,
		    i ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE))
		return 0;
	}
	return 1;
    }

    if (!nsg->k.knots || nsg->k.k_size < nsg->c_size + nsg->order)
	return 0;
    domain_start = nsg->k.knots[nsg->order - 1];
    domain_end = nsg->k.knots[nsg->c_size];
    if (domain_end <= domain_start) {
	domain_start = nsg->k.knots[0];
	domain_end = nsg->k.knots[nsg->k.k_size - 1];
    }
    if (domain_end <= domain_start)
	return 0;

    segments = qged_edit_curve_segments_for_span(ttol, span);
    for (size_t si = 0; si <= segments; si++) {
	fastf_t t = domain_start +
	    ((domain_end - domain_start) * (fastf_t)si / (fastf_t)segments);
	point2d_t uv;
	point_t pt;
	if (!qged_edit_nurb_eval2d(uv, sketch, nsg, t, domain_end))
	    return 0;
	VJOIN2(pt, V, uv[X], u_vec, uv[Y], v_vec);
	if (!qged_edit_preview_lines_append(lines, pt,
		si ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE))
	    return 0;
    }

    return 1;
}


static inline int
qged_edit_sketch_append_segment(struct qged_edit_preview_lines *lines,
				const struct bg_tess_tol *ttol,
				const point_t V,
				const vect_t u_vec,
				const vect_t v_vec,
				const struct rt_sketch_internal *sketch,
				const void *seg)
{
    const uint32_t *magic = (const uint32_t *)seg;

    if (!magic)
	return 0;

    switch (*magic) {
	case CURVE_LSEG_MAGIC:
	    return qged_edit_sketch_append_line_seg(lines, V, u_vec, v_vec,
		    sketch, (const struct line_seg *)seg);
	case CURVE_CARC_MAGIC:
	    return qged_edit_sketch_append_carc_seg(lines, ttol, V, u_vec,
		    v_vec, sketch, (const struct carc_seg *)seg);
	case CURVE_BEZIER_MAGIC:
	    return qged_edit_sketch_append_bezier_seg(lines, ttol, V, u_vec,
		    v_vec, sketch, (const struct bezier_seg *)seg);
	case CURVE_NURB_MAGIC:
	    return qged_edit_sketch_append_nurb_seg(lines, ttol, V, u_vec,
		    v_vec, sketch, (const struct nurb_seg *)seg);
	default:
	    break;
    }

    return 0;
}


static inline int
qged_edit_sketch_append_curve(struct qged_edit_preview_lines *lines,
			      const struct bg_tess_tol *ttol,
			      const point_t V,
			      const vect_t u_vec,
			      const vect_t v_vec,
			      const struct rt_sketch_internal *sketch,
			      const struct rt_curve *curve)
{
    if (!sketch || !curve)
	return 0;
    for (size_t seg_no = 0; seg_no < curve->count; seg_no++) {
	if (!qged_edit_sketch_append_segment(lines, ttol, V, u_vec, v_vec,
		sketch, curve->segment[seg_no]))
	    return 0;
    }
    return 1;
}


static inline int
qged_edit_feature_replace_extrude_wireframe(bsg_feature_ref ref,
					    enum bsg_feature_family family,
					    const struct rt_extrude_internal *extrude,
					    const struct bg_tess_tol *ttol)
{
    struct qged_edit_preview_lines lines;
    const struct rt_sketch_internal *sketch;
    size_t bottom_start, bottom_count, top_start, top_count;
    point_t end_of_h;
    int ret = 0;

    if (!extrude)
	return 0;
    RT_EXTRUDE_CK_MAGIC(extrude);

    qged_edit_preview_lines_init(&lines);
    sketch = extrude->skt;
    if (!sketch || sketch->curve.count == 0) {
	if (!qged_edit_preview_lines_append_line(&lines, extrude->V,
		extrude->V))
	    goto cleanup;
	ret = qged_edit_preview_lines_replace(ref, family, &lines);
	goto cleanup;
    }
    RT_SKETCH_CK_MAGIC(sketch);

    bottom_start = lines.count;
    if (!qged_edit_sketch_append_curve(&lines, ttol, extrude->V,
	    extrude->u_vec, extrude->v_vec, sketch, &sketch->curve))
	goto cleanup;
    bottom_count = lines.count - bottom_start;

    VADD2(end_of_h, extrude->V, extrude->h);
    top_start = lines.count;
    if (!qged_edit_sketch_append_curve(&lines, ttol, end_of_h,
	    extrude->u_vec, extrude->v_vec, sketch, &sketch->curve))
	goto cleanup;
    top_count = lines.count - top_start;

    if (bottom_count != top_count)
	goto cleanup;
    for (size_t i = 0; i < bottom_count; i++) {
	if (!qged_edit_preview_lines_append_line(&lines,
		lines.points[bottom_start + i],
		lines.points[top_start + i]))
	    goto cleanup;
    }

    ret = qged_edit_preview_lines_replace(ref, family, &lines);

cleanup:
    qged_edit_preview_lines_free(&lines);
    return ret;
}


static inline int
qged_edit_revolve_segment_endpoint_counts(int *endcount,
					  size_t nvert,
					  const struct rt_curve *curve)
{
    if (!endcount || !curve)
	return 0;

    for (size_t i = 0; i < curve->count; i++) {
	const uint32_t *magic = (const uint32_t *)curve->segment[i];
	if (!magic)
	    return 0;
	switch (*magic) {
	    case CURVE_LSEG_MAGIC: {
		const struct line_seg *lsg =
		    (const struct line_seg *)curve->segment[i];
		if (lsg->start < 0 || lsg->end < 0 ||
			(size_t)lsg->start >= nvert ||
			(size_t)lsg->end >= nvert)
		    return 0;
		endcount[lsg->start]++;
		endcount[lsg->end]++;
		break;
	    }
	    case CURVE_CARC_MAGIC: {
		const struct carc_seg *csg =
		    (const struct carc_seg *)curve->segment[i];
		if (csg->radius <= 0.0)
		    break;
		if (csg->start < 0 || csg->end < 0 ||
			(size_t)csg->start >= nvert ||
			(size_t)csg->end >= nvert)
		    return 0;
		endcount[csg->start]++;
		endcount[csg->end]++;
		break;
	    }
	    case CURVE_BEZIER_MAGIC: {
		const struct bezier_seg *bsg =
		    (const struct bezier_seg *)curve->segment[i];
		if (bsg->degree < 1 || bsg->ctl_points[0] < 0 ||
			bsg->ctl_points[bsg->degree] < 0 ||
			(size_t)bsg->ctl_points[0] >= nvert ||
			(size_t)bsg->ctl_points[bsg->degree] >= nvert)
		    return 0;
		endcount[bsg->ctl_points[0]]++;
		endcount[bsg->ctl_points[bsg->degree]]++;
		break;
	    }
	    case CURVE_NURB_MAGIC: {
		const struct nurb_seg *nsg =
		    (const struct nurb_seg *)curve->segment[i];
		if (nsg->c_size < 1 || nsg->ctl_points[0] < 0 ||
			nsg->ctl_points[nsg->c_size - 1] < 0 ||
			(size_t)nsg->ctl_points[0] >= nvert ||
			(size_t)nsg->ctl_points[nsg->c_size - 1] >= nvert)
		    return 0;
		endcount[nsg->ctl_points[0]]++;
		endcount[nsg->ctl_points[nsg->c_size - 1]]++;
		break;
	    }
	    default:
		return 0;
	}
    }

    return 1;
}


static inline int
qged_edit_revolve_append_end_lines(struct qged_edit_preview_lines *lines,
				   const struct rt_revolve_internal *revolve,
				   const int *endcount,
				   size_t nadd,
				   const vect_t radial)
{
    const point2d_t *verts = revolve->skt->verts;
    point_t add, add2, add3;

    for (size_t j = 0; j < nadd; j++) {
	if (j + 1 < nadd &&
		ZERO(verts[endcount[j]][Y] - verts[endcount[j + 1]][Y])) {
	    VJOIN1(add, revolve->v3d, verts[endcount[j]][Y],
		    revolve->axis3d);
	    VJOIN1(add2, add, verts[endcount[j]][X], radial);
	    VJOIN1(add3, add, verts[endcount[j + 1]][X], radial);
	    if (!qged_edit_preview_lines_append_line(lines, add2, add3))
		return 0;
	    j++;
	} else {
	    VJOIN1(add, revolve->v3d, verts[endcount[j]][Y],
		    revolve->axis3d);
	    VJOIN1(add2, add, verts[endcount[j]][X], radial);
	    if (!qged_edit_preview_lines_append_line(lines, add, add2))
		return 0;
	}
    }

    return 1;
}


static inline int
qged_edit_feature_replace_revolve_wireframe(bsg_feature_ref ref,
					    enum bsg_feature_family family,
					    const struct rt_revolve_internal *revolve,
					    const struct bg_tess_tol *ttol)
{
    struct qged_edit_preview_lines lines;
    const struct rt_sketch_internal *sketch;
    size_t nvert, narc, nadd = 0;
    point2d_t *verts;
    struct rt_curve *curve;
    vect_t ell[16];
    vect_t cir[16], ucir[16], height, xdir, ydir, ux, uy, uz, rEnd, xEnd, yEnd;
    fastf_t cos22_5 = 0.9238795325112867385;
    fastf_t cos67_5 = 0.3826834323650898373;
    int *endcount = NULL;
    int ang_sign;
    int ret = 0;

    if (!revolve)
	return 0;
    RT_REVOLVE_CK_MAGIC(revolve);
    sketch = revolve->skt;
    if (!sketch)
	return bsg_feature_points_replace(ref, family, NULL, NULL, 0);
    RT_SKETCH_CK_MAGIC(sketch);

    qged_edit_preview_lines_init(&lines);
    nvert = sketch->vert_count;
    curve = (struct rt_curve *)&sketch->curve;
    verts = sketch->verts;

    if (curve->count && !nvert)
	goto cleanup;

    if (revolve->ang < M_2PI)
	narc = (size_t)ceil(fabs(revolve->ang * 8 * M_1_PI));
    else
	narc = 16;
    if (narc > 16)
	narc = 16;

    VSCALE(xEnd, revolve->r, cos(revolve->ang));
    VCROSS(rEnd, revolve->axis3d, revolve->r);
    VSCALE(yEnd, rEnd, sin(revolve->ang));
    VADD2(rEnd, xEnd, yEnd);

    VMOVE(uz, revolve->axis3d);
    VMOVE(ux, revolve->r);
    VCROSS(uy, uz, ux);
    VUNITIZE(ux);
    VUNITIZE(uy);
    VUNITIZE(uz);

    ang_sign = revolve->ang < 0 ? -1 : 1;
    VMOVE(ucir[0], ux);
    VREVERSE(ucir[8], ucir[0]);

    VSCALE(xdir, ux, cos22_5);
    VSCALE(ydir, uy, ang_sign * cos67_5);
    VADD2(ucir[1], xdir, ydir);
    VREVERSE(ucir[9], ucir[1]);
    VREVERSE(xdir, xdir);
    VADD2(ucir[7], xdir, ydir);
    VREVERSE(ucir[15], ucir[7]);

    VSCALE(xdir, ux, M_SQRT1_2);
    VSCALE(ydir, uy, ang_sign * M_SQRT1_2);
    VADD2(ucir[2], xdir, ydir);
    VREVERSE(ucir[10], ucir[2]);
    VREVERSE(xdir, xdir);
    VADD2(ucir[6], xdir, ydir);
    VREVERSE(ucir[14], ucir[6]);

    VSCALE(xdir, ux, cos67_5);
    VSCALE(ydir, uy, ang_sign * cos22_5);
    VADD2(ucir[3], xdir, ydir);
    VREVERSE(ucir[11], ucir[3]);
    VREVERSE(xdir, xdir);
    VADD2(ucir[5], xdir, ydir);
    VREVERSE(ucir[13], ucir[5]);

    VSCALE(ydir, uy, ang_sign);
    VMOVE(ucir[4], ydir);
    VREVERSE(ucir[12], ucir[4]);

    if (nvert)
	endcount = (int *)bu_calloc(nvert, sizeof(int),
		"qged revolve preview endpoint counts");
    if (nvert && !qged_edit_revolve_segment_endpoint_counts(endcount, nvert,
	    curve))
	goto cleanup;

    for (size_t i = 0; i < nvert; i++) {
	if (endcount[i] == 0)
	    continue;
	VSCALE(height, uz, verts[i][Y]);
	for (size_t j = 0; j < narc; j++) {
	    VSCALE(cir[j], ucir[j], verts[i][X]);
	    VADD3(ell[j], revolve->v3d, cir[j], height);
	}
	if (narc > 0) {
	    if (!qged_edit_preview_lines_append(&lines, ell[0],
		    BSG_GEOMETRY_LINE_MOVE))
		goto cleanup;
	    for (size_t j = 1; j < narc; j++) {
		if (!qged_edit_preview_lines_append(&lines, ell[j],
			BSG_GEOMETRY_LINE_DRAW))
		    goto cleanup;
	    }
	}
	if (narc < 16) {
	    VSCALE(cir[narc], rEnd, verts[i][X]);
	    VADD3(ell[narc], revolve->v3d, cir[narc], height);
	    if (!qged_edit_preview_lines_append(&lines, ell[narc],
		    narc ? BSG_GEOMETRY_LINE_DRAW : BSG_GEOMETRY_LINE_MOVE))
		goto cleanup;
	} else if (narc > 0) {
	    if (!qged_edit_preview_lines_append(&lines, ell[0],
		    BSG_GEOMETRY_LINE_DRAW))
		goto cleanup;
	}
    }

    for (size_t i = 0, j = 0; i < nvert; i++) {
	if (endcount[i] % 2 != 0) {
	    size_t k;
	    for (k = j; k > 0; k--) {
		if ((ZERO(verts[i][Y] - verts[endcount[k - 1]][Y]) &&
			verts[i][X] > verts[endcount[k - 1]][X]) ||
			(!ZERO(verts[i][Y] - verts[endcount[k - 1]][Y]) &&
			 verts[i][Y] < verts[endcount[k - 1]][Y])) {
		    endcount[k] = endcount[k - 1];
		} else {
		    break;
		}
	    }
	    endcount[k] = (int)i;
	    j++;
	    nadd = j;
	}
    }
    for (size_t j = nadd; j < nvert; j++)
	endcount[j] = -1;

    for (size_t i = 0; i < narc; i++) {
	if (!qged_edit_sketch_append_curve(&lines, ttol, revolve->v3d,
		ucir[i], uz, sketch, curve))
	    goto cleanup;
	if (!qged_edit_revolve_append_end_lines(&lines, revolve, endcount,
		nadd, ucir[i]))
	    goto cleanup;
    }
    if (narc < 16) {
	if (!qged_edit_sketch_append_curve(&lines, ttol, revolve->v3d,
		rEnd, uz, sketch, curve))
	    goto cleanup;
	if (!qged_edit_revolve_append_end_lines(&lines, revolve, endcount,
		nadd, rEnd))
	    goto cleanup;
	for (size_t j = 0; j + 1 < nadd; j += 2) {
	    point_t add, add2;
	    if (!ZERO(verts[endcount[j]][Y] - verts[endcount[j + 1]][Y])) {
		VJOIN1(add, revolve->v3d, verts[endcount[j]][Y],
			revolve->axis3d);
		VJOIN1(add2, revolve->v3d, verts[endcount[j + 1]][Y],
			revolve->axis3d);
		if (!qged_edit_preview_lines_append_line(&lines, add, add2))
		    goto cleanup;
	    }
	}
    }

    ret = qged_edit_preview_lines_replace(ref, family, &lines);

cleanup:
    if (endcount)
	bu_free(endcount, "qged revolve preview endpoint counts");
    qged_edit_preview_lines_free(&lines);
    return ret;
}


#endif /* QGED_EDIT_PREVIEW_UTIL_H */
