/*                       Q U A L I T Y . C P P
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
/** @file libbrep/quality.cpp */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <set>
#include <vector>

#include "bu/vls.h"
#include "brep/quality.h"


static double
sqr(double v)
{
    return v * v;
}


static double
point_dist(const ON_3dPoint &a, const ON_3dPoint &b)
{
    return std::sqrt(sqr(a.x - b.x) + sqr(a.y - b.y) + sqr(a.z - b.z));
}


static double
point_dist2d(const ON_2dPoint &a, const ON_2dPoint &b)
{
    return std::sqrt(sqr(a.x - b.x) + sqr(a.y - b.y));
}


static bool
bbox_overlap(const ON_BoundingBox &a, const ON_BoundingBox &b, double tol)
{
    if (!a.IsValid() || !b.IsValid())
	return false;

    return a.m_min.x <= b.m_max.x + tol && a.m_max.x + tol >= b.m_min.x &&
	a.m_min.y <= b.m_max.y + tol && a.m_max.y + tol >= b.m_min.y &&
	a.m_min.z <= b.m_max.z + tol && a.m_max.z + tol >= b.m_min.z;
}


static bool
bbox_contains(const ON_BoundingBox &bb, const ON_3dPoint &p, double tol)
{
    return bb.IsValid() &&
	p.x >= bb.m_min.x - tol && p.x <= bb.m_max.x + tol &&
	p.y >= bb.m_min.y - tol && p.y <= bb.m_max.y + tol &&
	p.z >= bb.m_min.z - tol && p.z <= bb.m_max.z + tol;
}


static double
bbox_aspect(const ON_BoundingBox &bb, double tol)
{
    if (!bb.IsValid())
	return 0.0;

    std::vector<double> dims;
    dims.push_back(std::fabs(bb.m_max.x - bb.m_min.x));
    dims.push_back(std::fabs(bb.m_max.y - bb.m_min.y));
    dims.push_back(std::fabs(bb.m_max.z - bb.m_min.z));
    std::sort(dims.begin(), dims.end());

    double min_dim = 0.0;
    for (double d : dims) {
	if (d > tol) {
	    min_dim = d;
	    break;
	}
    }

    return min_dim > 0.0 ? dims.back() / min_dim : 0.0;
}


static bool
faces_share_topology(const ON_BrepFace &a, const ON_BrepFace &b)
{
    std::set<int> a_edges;
    std::set<int> a_vertices;
    for (int li = 0; li < a.LoopCount(); ++li) {
	const ON_BrepLoop *loop = a.Loop(li);
	if (!loop)
	    continue;
	for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	    const ON_BrepTrim *trim = loop->Trim(ti);
	    if (!trim)
		continue;
	    if (trim->m_ei >= 0)
		a_edges.insert(trim->m_ei);
	    if (trim->m_vi[0] >= 0)
		a_vertices.insert(trim->m_vi[0]);
	    if (trim->m_vi[1] >= 0)
		a_vertices.insert(trim->m_vi[1]);
	}
    }

    for (int li = 0; li < b.LoopCount(); ++li) {
	const ON_BrepLoop *loop = b.Loop(li);
	if (!loop)
	    continue;
	for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	    const ON_BrepTrim *trim = loop->Trim(ti);
	    if (trim && trim->m_ei >= 0 && a_edges.find(trim->m_ei) != a_edges.end())
		return true;
	    if (trim && trim->m_vi[0] >= 0 && a_vertices.find(trim->m_vi[0]) != a_vertices.end())
		return true;
	    if (trim && trim->m_vi[1] >= 0 && a_vertices.find(trim->m_vi[1]) != a_vertices.end())
		return true;
	}
    }

    return false;
}


static ON_3dPoint
face_midpoint(const ON_BrepFace &face)
{
    const ON_Surface *s = face.SurfaceOf();
    if (!s)
	return ON_3dPoint::UnsetPoint;

    ON_Interval u = s->Domain(0);
    ON_Interval v = s->Domain(1);
    return s->PointAt(0.5 * (u.m_t[0] + u.m_t[1]), 0.5 * (v.m_t[0] + v.m_t[1]));
}


static void
quality_msg(struct bu_vls *msgs, const char *kind, const char *fmt, ...)
{
    if (!msgs)
	return;

    va_list ap;
    va_start(ap, fmt);
    bu_vls_printf(msgs, "BREP quality %s: ", kind);
    bu_vls_vprintf(msgs, fmt, ap);
    bu_vls_putc(msgs, '\n');
    va_end(ap);
}


void
ON_Brep_Quality_Defaults(struct brep_quality_options *opts)
{
    if (!opts)
	return;

    opts->tolerance = ON_ZERO_TOLERANCE;
    opts->duplicate_vertex_tolerance = 1.0e-8;
    opts->min_edge_length = 1.0e-8;
    opts->min_trim_length = 1.0e-10;
    opts->max_aspect_ratio = 1.0e5;
    opts->self_intersection_samples = 1;
    opts->check_self_intersection_risk = true;
}


int
ON_Brep_Quality_Check(const ON_Brep *brep, struct bu_vls *msgs, const struct brep_quality_options *uopts)
{
    struct brep_quality_options opts;
    ON_Brep_Quality_Defaults(&opts);
    if (uopts)
	opts = *uopts;

    if (!brep) {
	quality_msg(msgs, "error", "null BREP pointer");
	return 1;
    }

    int errors = 0;

    ON_BoundingBox brep_bb;
    brep->GetBoundingBox(brep_bb, false);
    const ON_3dVector diag = brep_bb.m_max - brep_bb.m_min;
    const double model_diag = brep_bb.IsValid() ? diag.Length() : 1.0;
    const double tol = std::max(opts.tolerance, model_diag * 1.0e-12);
    const double duplicate_tol = std::max(opts.duplicate_vertex_tolerance, tol);
    const double min_edge_length = std::max(opts.min_edge_length, tol);
    const double min_trim_length = std::max(opts.min_trim_length, tol * 0.01);
    for (int i = 0; i < brep->m_V.Count(); ++i) {
	const ON_3dPoint pi = brep->m_V[i].Point();
	for (int j = i + 1; j < brep->m_V.Count(); ++j) {
	    const double d = point_dist(pi, brep->m_V[j].Point());
	    if (d <= duplicate_tol) {
		quality_msg(msgs, "error", "vertices %d and %d are duplicates or unmerged within %.17g", i, j, duplicate_tol);
		++errors;
	    }
	}
    }

    for (int i = 0; i < brep->m_E.Count(); ++i) {
	const ON_BrepEdge &edge = brep->m_E[i];
	if (edge.m_vi[0] < 0 || edge.m_vi[1] < 0)
	    continue;

	const double edist = point_dist(brep->m_V[edge.m_vi[0]].Point(), brep->m_V[edge.m_vi[1]].Point());
	if (edist <= min_edge_length) {
	    quality_msg(msgs, "error", "edge %d has zero or near-zero endpoint length %.17g", i, edist);
	    ++errors;
	}
	if (edge.m_ti.Count() != 2) {
	    quality_msg(msgs, "error", "edge %d has %d trims; solid BREP quality requires two mated trims", i, edge.m_ti.Count());
	    ++errors;
	} else {
	    const ON_BrepTrim &ta = brep->m_T[edge.m_ti[0]];
	    const ON_BrepTrim &tb = brep->m_T[edge.m_ti[1]];
	    if (ta.m_bRev3d == tb.m_bRev3d) {
		quality_msg(msgs, "error", "edge %d has adjacent trims with matching 3D orientation", i);
		++errors;
	    }
	}
    }

    for (int i = 0; i < brep->m_T.Count(); ++i) {
	const ON_BrepTrim &trim = brep->m_T[i];
	if (trim.m_type == ON_BrepTrim::singular)
	    continue;

	const ON_Interval d = trim.Domain();
	const ON_2dPoint p0 = trim.PointAt(d.m_t[0]);
	const ON_2dPoint p1 = trim.PointAt(d.m_t[1]);
	const double len2d = point_dist2d(p0, p1);
	if (std::fabs(d.Length()) <= min_trim_length || len2d <= min_trim_length) {
	    quality_msg(msgs, "error", "trim %d has zero or near-zero 2D length %.17g", i, len2d);
	    ++errors;
	}
    }

    for (int i = 0; i < brep->m_F.Count(); ++i) {
	const ON_BrepFace &face = brep->m_F[i];
	ON_BoundingBox fbb;
	if (face.GetBoundingBox(fbb, false)) {
	    const double aspect = bbox_aspect(fbb, min_edge_length);
	    if (aspect > opts.max_aspect_ratio) {
		quality_msg(msgs, "error", "face %d bounding box aspect ratio %.17g exceeds %.17g", i, aspect, opts.max_aspect_ratio);
		++errors;
	    }
	}

    }

    if (opts.check_self_intersection_risk && opts.self_intersection_samples > 0) {
	size_t risk_pairs = 0;
	for (int i = 0; i < brep->m_F.Count(); ++i) {
	    const ON_BrepFace &fi = brep->m_F[i];
	    ON_BoundingBox bbi;
	    if (!fi.GetBoundingBox(bbi, false))
		continue;
	    const ON_3dPoint pi = face_midpoint(fi);
	    for (int j = i + 1; j < brep->m_F.Count(); ++j) {
		const ON_BrepFace &fj = brep->m_F[j];
		if (faces_share_topology(fi, fj))
		    continue;
		ON_BoundingBox bbj;
		if (!fj.GetBoundingBox(bbj, false) || !bbox_overlap(bbi, bbj, tol))
		    continue;
		const ON_3dPoint pj = face_midpoint(fj);
		if (bbox_contains(bbi, pj, tol) || bbox_contains(bbj, pi, tol)) {
		    ++risk_pairs;
		}
	    }
	}
	if (risk_pairs > 0)
	    quality_msg(msgs, "warning", "%zu non-adjacent face pairs have overlapping bounding boxes and sampled interior overlap; possible self-intersection risk", risk_pairs);
    }

    return errors;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * c-file-style: "stroustrup"
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
