/*                   B R E P _ C D T . C P P
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
/** @addtogroup libbrep */
/** @{ */
/** @file cdt/fast.cpp
 *
 * Original non-watertight fast implementation of Constrained Delaunay
 * Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"

#include <vector>
#include <atomic>
#include <cmath>
#include <list>
#include <map>
#include <new>
#include <stack>
#include <iostream>
#include <algorithm>
#include <set>
#include <unordered_map>
#include <utility>

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#endif
#include "../../libbg/detria.hpp"
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

#include "assert.h"

#include "vmath.h"
#include "bu/parallel.h"
#include "bu/env.h"

#include "bu/cv.h"
#include "bu/opt.h"
#include "bu/datetime.h"
#include "bn/dvec.h"
#include "brep.h"
#include "./cdt.h"

#define YELLOW 255, 255, 0

typedef std::map<double, BrepTrimPoint *> fast_trim_point_map;

/* All scratch data used while realizing one face belongs to that face.  The
 * old implementation stored these maps in ON_BrepTrim::m_trim_user, which
 * mutated a const input BRep, made concurrent calls unsafe, and paired the
 * allocation with type-incorrect cleanup. */
struct fast_face_scratch {
    std::map<int, fast_trim_point_map *> trim_points;
    std::vector<ON_3dPoint *> loose_points;
    bool hit_sample_limit = false;

    ~fast_face_scratch()
    {
	for (auto &entry : trim_points) {
	    fast_trim_point_map *points = entry.second;
	    if (!points)
		continue;
	    for (auto &sample : *points) {
		BrepTrimPoint *point = sample.second;
		if (!point)
		    continue;
		delete point->p3d;
		delete point->n3d;
		delete point;
	    }
	    delete points;
	}
	for (ON_3dPoint *point : loose_points)
	    delete point;
    }

    fast_trim_point_map *find(int trim_index)
    {
	auto entry = trim_points.find(trim_index);
	return (entry == trim_points.end()) ? NULL : entry->second;
    }

    fast_trim_point_map *create(int trim_index)
    {
	fast_trim_point_map *points = new fast_trim_point_map;
	trim_points[trim_index] = points;
	return points;
    }

    ON_3dPoint *make_point(const ON_3dPoint &point)
    {
	ON_3dPoint *owned = new ON_3dPoint(point);
	loose_points.push_back(owned);
	return owned;
    }
};

struct fast_recursion_guard {
    int &depth;
    explicit fast_recursion_guard(int &d) : depth(d) { depth++; }
    ~fast_recursion_guard() { depth--; }
};

struct fast_line_store {
    std::vector<ON_Line *> lines;
    ~fast_line_store()
    {
	for (ON_Line *line : lines)
	    delete line;
    }
};

struct fast_bridge_store {
    std::vector<std::map<double, ON_3dPoint *> *> maps;

    ~fast_bridge_store()
    {
	for (std::map<double, ON_3dPoint *> *map : maps) {
	    if (!map)
		continue;
	    for (const auto &sample : *map)
		delete sample.second;
	    delete map;
	}
    }

    void push_back(std::map<double, ON_3dPoint *> *map)
    {
	maps.push_back(map);
    }
};

struct fast_loop_point_store {
    ON_SimpleArray<BrepTrimPoint> **points = NULL;
    int count;

    explicit fast_loop_point_store(int loop_count) : count(loop_count)
    {
	if (count <= 0)
	    return;
	points = (ON_SimpleArray<BrepTrimPoint> **)bu_calloc(count,
	    sizeof(ON_SimpleArray<BrepTrimPoint> *), "loop_pnts");
	try {
	    for (int i = 0; i < count; i++)
		points[i] = new ON_SimpleArray<BrepTrimPoint>;
	} catch (...) {
	    for (int i = 0; i < count; i++)
		delete points[i];
	    bu_free(points, "brep_loop_points");
	    points = NULL;
	    throw;
	}
    }

    ~fast_loop_point_store()
    {
	for (int i = 0; i < count; i++)
	    delete points[i];
	if (points)
	    bu_free(points, "brep_loop_points");
    }
};

static const int FAST_CDT_MAX_RECURSION = 64;
static const int FAST_CDT_MAX_TRIM_SAMPLES = 16384;
static const int FAST_CDT_MAX_SURFACE_SAMPLES = 262144;

// Digest tessellation tolerances...
static void
CDT_Tol_Set(struct brep_cdt_tol *cdt, double dist, fastf_t md, const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    fastf_t min_dist, max_dist, within_dist, cos_within_ang;

    max_dist = md;

    if (ttol->abs < tol->dist + ON_ZERO_TOLERANCE) {
	min_dist = tol->dist;
    } else {
	min_dist = ttol->abs;
    }

    double rel = 0.0;
    if (ttol->rel > 0.0 + ON_ZERO_TOLERANCE) {
	rel = ttol->rel * dist;
	if (max_dist < rel * 10.0) {
	    max_dist = rel * 10.0;
	}
	within_dist = rel < min_dist ? min_dist : rel;
    } else if (ttol->abs > 0.0 + ON_ZERO_TOLERANCE) {
	within_dist = min_dist;
    } else {
	within_dist = 0.01 * dist; // default to 1% of dist
    }

    if (ttol->norm > 0.0 + ON_ZERO_TOLERANCE) {
	cos_within_ang = cos(ttol->norm);
    } else {
	cos_within_ang = cos(ON_PI / 2.0);
    }

    cdt->min_dist = min_dist;
    cdt->max_dist = max_dist;
    cdt->within_dist = within_dist;
    cdt->cos_within_ang = cos_within_ang;
}


static void
getEdgePoints(const ON_BrepTrim &trim,
	      BrepTrimPoint *sbtp,
              BrepTrimPoint *ebtp,
	      const struct brep_cdt_tol *cdt_tol,
	      std::map<double, BrepTrimPoint *> &param_points,
	      fast_face_scratch &scratch)
{
    static thread_local int recursion_depth = 0;
    if (recursion_depth >= FAST_CDT_MAX_RECURSION) {
	scratch.hit_sample_limit = true;
	return;
    }
    if (param_points.size() >= FAST_CDT_MAX_TRIM_SAMPLES) {
	scratch.hit_sample_limit = true;
	return;
    }
    fast_recursion_guard guard(recursion_depth);

    const ON_Surface *s = trim.SurfaceOf();
    ON_3dPoint mid_2d = ON_3dPoint::UnsetPoint;
    ON_3dPoint mid_3d = ON_3dPoint::UnsetPoint;
    ON_3dVector mid_norm = ON_3dVector::UnsetVector;
    ON_3dVector mid_tang = ON_3dVector::UnsetVector;
    fastf_t t = (sbtp->t + ebtp->t) / 2.0;
    if (!std::isfinite(t) || !(t > sbtp->t) || !(t < ebtp->t))
	return;

    int etrim = (trim.EvTangent(t, mid_2d, mid_tang) && surface_EvNormal(s, mid_2d.x, mid_2d.y, mid_3d, mid_norm)) ? 1 : 0;
    int leval = 0;

    if (etrim) {
	ON_Line line3d(*(sbtp->p3d), *(ebtp->p3d));
	double dist3d = mid_3d.DistanceTo(line3d.ClosestPointTo(mid_3d));
	int leval_1 = 0;
	// TODO - I know this is less efficient than doing the tests in the if
	// statement because we can't short-circuit in the true OR case, but
	// leaving it this way temporarily for readability
	leval += (line3d.Length() > cdt_tol->max_dist) ? 1 : 0;
	leval += (dist3d > (cdt_tol->within_dist + ON_ZERO_TOLERANCE)) ? 1 : 0;
	leval_1 += ((sbtp->tangent * ebtp->tangent) < cdt_tol->cos_within_ang - ON_ZERO_TOLERANCE) ? 1 : 0;
	leval_1 += ((sbtp->normal * ebtp->normal) < cdt_tol->cos_within_ang - ON_ZERO_TOLERANCE) ? 1 : 0;
	leval += (leval_1 && (dist3d > cdt_tol->min_dist + ON_ZERO_TOLERANCE)) ? 1 : 0;
    }

    if (etrim && leval) {
	BrepTrimPoint *nbtp = new BrepTrimPoint;
	nbtp->p3d = new ON_3dPoint(mid_3d);
	nbtp->n3d = NULL;
	nbtp->p2d = mid_2d;
	nbtp->normal = mid_norm;
	nbtp->tangent = mid_tang;
	nbtp->t = t;
	param_points[nbtp->t] = nbtp;
	getEdgePoints(trim, sbtp, nbtp, cdt_tol, param_points, scratch);
	getEdgePoints(trim, nbtp, ebtp, cdt_tol, param_points, scratch);
	return;
    }

    int udir = 0;
    int vdir = 0;
    ON_2dPoint start = sbtp->p2d;
    ON_2dPoint end = ebtp->p2d;

    if (ConsecutivePointsCrossClosedSeam(s, start, end, udir, vdir, BREP_SAME_POINT_TOLERANCE)) {
	double seam_t;
	ON_2dPoint from = ON_2dPoint::UnsetPoint;
	ON_2dPoint to = ON_2dPoint::UnsetPoint;
	if (FindTrimSeamCrossing(trim, sbtp->t, ebtp->t, seam_t, from, to, BREP_SAME_POINT_TOLERANCE)) {
	    ON_2dPoint seam_2d = trim.PointAt(seam_t);
	    ON_3dPoint seam_3d = s->PointAt(seam_2d.x, seam_2d.y);
	    if (param_points.find(seam_t) == param_points.end()) {
		BrepTrimPoint *nbtp = new BrepTrimPoint;
		nbtp->p3d = new ON_3dPoint(seam_3d);
		nbtp->n3d = NULL;
		nbtp->p2d = seam_2d;
		// Note - by this point we shouldn't need tangents and normals...
		nbtp->t = seam_t;
		param_points[nbtp->t] = nbtp;
	    }
	}
    }
}

static
std::map<double, BrepTrimPoint *> *
getEdgePoints(ON_BrepTrim &trim,
	      fastf_t max_dist,
	      const struct bg_tess_tol *ttol,
	      const struct bn_tol *tol,
	      fast_face_scratch &scratch)
{
    struct brep_cdt_tol cdt_tol = BREP_CDT_TOL_ZERO;
    std::map<double, BrepTrimPoint *> *param_points = NULL;

    double dist = 1000.0;

    const ON_Surface *s = trim.SurfaceOf();

    bool bGrowBox = false;
    ON_3dPoint min, max;

    /* If we've already got the points, just return them */
    if ((param_points = scratch.find(trim.m_trim_index)) != NULL) {
	return param_points;
    }

    /* Establish tolerances */
    if (trim.GetBoundingBox(min, max, bGrowBox)) {
	dist = DIST_PNT_PNT(min, max);
    }
    CDT_Tol_Set(&cdt_tol, dist, max_dist, ttol, tol);

    /* Begin point collection */
    int evals = 0;
    ON_3dPoint start_2d(0.0, 0.0, 0.0);
    ON_3dPoint start_3d(0.0, 0.0, 0.0);
    ON_3dVector start_tang(0.0, 0.0, 0.0);
    ON_3dVector start_norm(0.0, 0.0, 0.0);
    ON_3dPoint end_2d(0.0, 0.0, 0.0);
    ON_3dPoint end_3d(0.0, 0.0, 0.0);
    ON_3dVector end_tang(0.0, 0.0, 0.0);
    ON_3dVector end_norm(0.0, 0.0, 0.0);

    param_points = scratch.create(trim.m_trim_index);
    ON_Interval range = trim.Domain();
    if (s->IsClosed(0) || s->IsClosed(1)) {
	ON_BoundingBox trim_bbox = ON_BoundingBox::EmptyBoundingBox;
	trim.GetBoundingBox(trim_bbox, false);
    }

    evals += (trim.EvTangent(range.m_t[0], start_2d, start_tang)) ? 1 : 0;
    evals += (trim.EvTangent(range.m_t[1], end_2d, end_tang)) ? 1 : 0;
    evals += (surface_EvNormal(s, start_2d.x, start_2d.y, start_3d, start_norm)) ? 1 : 0;
    evals += (surface_EvNormal(s, end_2d.x, end_2d.y, end_3d, end_norm)) ? 1 : 0;

    if (evals != 4) {
	start_2d = trim.PointAt(range.m_t[0]);
	end_2d = trim.PointAt(range.m_t[1]);
	start_3d = s->PointAt(start_2d.x,start_2d.y);
	end_3d = s->PointAt(end_2d.x,end_2d.y);
	evals = 4;
    }

    BrepTrimPoint *sbtp = new BrepTrimPoint;
    sbtp->p3d = new ON_3dPoint(start_3d);
    sbtp->n3d = NULL;
    sbtp->p2d = start_2d;
    sbtp->tangent = start_tang;
    sbtp->normal = start_norm;
    sbtp->t = range.m_t[0];
    (*param_points)[sbtp->t] = sbtp;

    BrepTrimPoint *ebtp = new BrepTrimPoint;
    ebtp->p3d = new ON_3dPoint(end_3d);
    ebtp->n3d = NULL;
    ebtp->p2d = end_2d;
    ebtp->tangent = end_tang;
    ebtp->normal = end_norm;
    ebtp->t = range.m_t[1];
    (*param_points)[ebtp->t] = ebtp;


    if (trim.IsClosed()) {

	double mid_range = (range.m_t[0] + range.m_t[1]) / 2.0;
	ON_3dPoint mid_2d(0.0, 0.0, 0.0);
	ON_3dPoint mid_3d(0.0, 0.0, 0.0);
	ON_3dVector mid_tang(0.0, 0.0, 0.0);
	ON_3dVector mid_norm(0.0, 0.0, 0.0);

	evals += (trim.EvTangent(mid_range, mid_2d, mid_tang)) ? 1 : 0;
	evals += (surface_EvNormal(s, mid_2d.x, mid_2d.y, mid_3d, mid_norm)) ? 1 : 0;
	if (evals != 6) {
	    mid_2d = trim.PointAt(mid_range);
	    mid_3d = s->PointAt(mid_2d.x, mid_2d.y);
	}

	BrepTrimPoint *mbtp = new BrepTrimPoint;
	mbtp->p3d = new ON_3dPoint(mid_3d);
	mbtp->n3d = NULL;
	mbtp->p2d = mid_2d;
	mbtp->tangent = mid_tang;
	mbtp->normal = mid_norm;
	mbtp->t = mid_range;
	(*param_points)[mbtp->t] = mbtp;

	getEdgePoints(trim, sbtp, mbtp, &cdt_tol, *param_points, scratch);
	getEdgePoints(trim, mbtp, ebtp, &cdt_tol, *param_points, scratch);

    } else {

	getEdgePoints(trim, sbtp, ebtp, &cdt_tol, *param_points, scratch);

    }

    /* Boundary points shared by two faces must be taken from their common
     * 3-D BREP edge, not independently from the two surface lifts.  Valid
     * trims may differ from that edge by their asserted edge tolerance; using
     * the lifts directly opens a visible crack in the fast shaded-display
     * mesh.  A trim and its edge are not required to use identical parameter
     * domains, so verify the normalized correspondence at every sample before
     * changing any point.  If it is not valid for the entire trim, leave the
     * trim on the existing general path rather than producing a mixture of
     * surface and edge points. */
    const ON_BrepEdge *edge = trim.Edge();
    const ON_Curve *edge_curve = edge ? edge->EdgeCurveOf() : NULL;
    const ON_Interval edge_domain = edge_curve ? edge_curve->Domain() :
	ON_Interval::EmptyInterval;
    const double edge_tolerance = edge ? std::max(tol->dist,
	edge->m_tolerance) : tol->dist;

    bool edge_parameterization_matches = edge_curve && range.IsIncreasing() &&
	edge_domain.IsIncreasing();
    if (edge_parameterization_matches) {
	for (std::map<double, BrepTrimPoint *>::iterator sample =
		param_points->begin(); sample != param_points->end(); ++sample) {
	    BrepTrimPoint *point = sample->second;
	    if (!point || !point->p3d) {
		edge_parameterization_matches = false;
		break;
	    }
	    double fraction = range.NormalizedParameterAt(point->t);
	    if (trim.m_bRev3d)
		fraction = 1.0 - fraction;
	    const ON_3dPoint edge_point = edge_curve->PointAt(
		edge_domain.ParameterAt(fraction));
	    if (!edge_point.IsValid() ||
		    edge_point.DistanceTo(*point->p3d) > edge_tolerance) {
		edge_parameterization_matches = false;
		break;
	    }
	}
    }
    if (edge_parameterization_matches) {
	for (std::map<double, BrepTrimPoint *>::iterator sample =
		param_points->begin(); sample != param_points->end(); ++sample) {
	    BrepTrimPoint *point = sample->second;
	    double fraction = range.NormalizedParameterAt(point->t);
	    if (trim.m_bRev3d)
		fraction = 1.0 - fraction;
	    *point->p3d = edge_curve->PointAt(edge_domain.ParameterAt(fraction));
	}
    }

    return param_points;
}

static void
getSurfacePoints(const ON_Surface *s,
		 fastf_t u1,
		 fastf_t u2,
		 fastf_t v1,
		 fastf_t v2,
		 fastf_t min_dist,
		 fastf_t within_dist,
		 fastf_t cos_within_ang,
		 ON_2dPointArray &on_surf_points,
		 bool left,
		 bool below)
{
    static thread_local int recursion_depth = 0;
    if (recursion_depth >= FAST_CDT_MAX_RECURSION ||
	    on_surf_points.Count() >= FAST_CDT_MAX_SURFACE_SAMPLES ||
	    !std::isfinite(u1) || !std::isfinite(u2) ||
	    !std::isfinite(v1) || !std::isfinite(v2) ||
	    !(u2 > u1) || !(v2 > v1))
	return;
    fast_recursion_guard guard(recursion_depth);

    double ldfactor = 2.0;
    ON_2dPoint p2d(0.0, 0.0);
    ON_3dPoint p[4] = {ON_3dPoint(), ON_3dPoint(), ON_3dPoint(), ON_3dPoint()};
    ON_3dVector norm[4] = {ON_3dVector(), ON_3dVector(), ON_3dVector(), ON_3dVector()};
    ON_3dPoint mid(0.0, 0.0, 0.0);
    ON_3dVector norm_mid(0.0, 0.0, 0.0);
    fastf_t u = (u1 + u2) / 2.0;
    fastf_t v = (v1 + v2) / 2.0;
    fastf_t udist = u2 - u1;
    fastf_t vdist = v2 - v1;
    double surface_width = 0.0;
    double surface_height = 0.0;
    const ON_Interval udom = s->Domain(0);
    const ON_Interval vdom = s->Domain(1);
    if (!s->GetSurfaceSize(&surface_width, &surface_height) ||
	    !udom.IsIncreasing() || !vdom.IsIncreasing())
	return;
    const fastf_t u_metric_dist = udist * surface_width / udom.Length();
    const fastf_t v_metric_dist = vdist * surface_height / vdom.Length();

    if ((u_metric_dist < min_dist + ON_ZERO_TOLERANCE)
	|| (v_metric_dist < min_dist + ON_ZERO_TOLERANCE)) {
	return;
    }

    if (u_metric_dist > ldfactor * v_metric_dist) {
	const double requested = u_metric_dist / v_metric_dist / ldfactor * 2.0;
	const int remaining = FAST_CDT_MAX_SURFACE_SAMPLES -
	    on_surf_points.Count();
	if (!std::isfinite(requested) || remaining < 2)
	    return;
	int isteps = (int)std::min(requested, (double)(remaining / 2));
	if (isteps < 1)
	    return;
	fastf_t step = udist / (fastf_t) isteps;

	fastf_t step_u;
	for (int i = 1; i <= isteps; i++) {
	    if (on_surf_points.Count() >= FAST_CDT_MAX_SURFACE_SAMPLES)
		return;
	    step_u = u1 + i * step;
	    if ((below) && (i < isteps)) {
		p2d.Set(step_u, v1);
		on_surf_points.Append(p2d);
	    }
	    if (i == 1) {
		getSurfacePoints(s, u1, u1 + step, v1, v2, min_dist,
				 within_dist, cos_within_ang, on_surf_points, left,
				 below);
	    } else if (i == isteps) {
		getSurfacePoints(s, u2 - step, u2, v1, v2, min_dist,
				 within_dist, cos_within_ang, on_surf_points, left,
				 below);
	    } else {
		getSurfacePoints(s, step_u - step, step_u, v1, v2, min_dist, within_dist,
				 cos_within_ang, on_surf_points, left, below);
	    }
	    left = false;

	    if (i < isteps) {
		//top
		p2d.Set(step_u, v2);
		on_surf_points.Append(p2d);
	    }
	}
    } else if (v_metric_dist > ldfactor * u_metric_dist) {
	const double requested = v_metric_dist / u_metric_dist / ldfactor * 2.0;
	const int remaining = FAST_CDT_MAX_SURFACE_SAMPLES -
	    on_surf_points.Count();
	if (!std::isfinite(requested) || remaining < 2)
	    return;
	int isteps = (int)std::min(requested, (double)(remaining / 2));
	if (isteps < 1)
	    return;
	fastf_t step = vdist / (fastf_t) isteps;
	fastf_t step_v;
	for (int i = 1; i <= isteps; i++) {
	    if (on_surf_points.Count() >= FAST_CDT_MAX_SURFACE_SAMPLES)
		return;
	    step_v = v1 + i * step;
	    if ((left) && (i < isteps)) {
		p2d.Set(u1, step_v);
		on_surf_points.Append(p2d);
	    }

	    if (i == 1) {
		getSurfacePoints(s, u1, u2, v1, v1 + step, min_dist,
				 within_dist, cos_within_ang, on_surf_points, left,
				 below);
	    } else if (i == isteps) {
		getSurfacePoints(s, u1, u2, v2 - step, v2, min_dist,
				 within_dist, cos_within_ang, on_surf_points, left,
				 below);
	    } else {
		getSurfacePoints(s, u1, u2, step_v - step, step_v, min_dist, within_dist,
				 cos_within_ang, on_surf_points, left, below);
	    }

	    below = false;

	    if (i < isteps) {
		//right
		p2d.Set(u2, step_v);
		on_surf_points.Append(p2d);
	    }
	}
    } else if ((surface_EvNormal(s, u1, v1, p[0], norm[0]))
	       && (surface_EvNormal(s, u2, v1, p[1], norm[1])) // for u
	       && (surface_EvNormal(s, u2, v2, p[2], norm[2]))
	       && (surface_EvNormal(s, u1, v2, p[3], norm[3]))
	       && (surface_EvNormal(s, u, v, mid, norm_mid))) {
	double udot;
	double vdot;
	ON_Line line1(p[0], p[2]);
	ON_Line line2(p[1], p[3]);
	double dist = mid.DistanceTo(line1.ClosestPointTo(mid));
	V_MAX(dist, mid.DistanceTo(line2.ClosestPointTo(mid)));

	if (dist < min_dist + ON_ZERO_TOLERANCE) {
	    return;
	}

	if (VNEAR_EQUAL(norm[0], norm[1], ON_ZERO_TOLERANCE)) {
	    udot = 1.0;
	} else {
	    udot = norm[0] * norm[1];
	}
	if (VNEAR_EQUAL(norm[0], norm[3], ON_ZERO_TOLERANCE)) {
	    vdot = 1.0;
	} else {
	    vdot = norm[0] * norm[3];
	}
	if ((udot < cos_within_ang - ON_ZERO_TOLERANCE)
	    && (vdot < cos_within_ang - ON_ZERO_TOLERANCE)) {
	    if (left) {
		p2d.Set(u1, v);
		on_surf_points.Append(p2d);
	    }
	    if (below) {
		p2d.Set(u, v1);
		on_surf_points.Append(p2d);
	    }
	    //center
	    p2d.Set(u, v);
	    on_surf_points.Append(p2d);
	    //right
	    p2d.Set(u2, v);
	    on_surf_points.Append(p2d);
	    //top
	    p2d.Set(u, v2);
	    on_surf_points.Append(p2d);

	    getSurfacePoints(s, u1, u, v1, v, min_dist, within_dist,
			     cos_within_ang, on_surf_points, left, below);
	    getSurfacePoints(s, u1, u, v, v2, min_dist, within_dist,
			     cos_within_ang, on_surf_points, left, false);
	    getSurfacePoints(s, u, u2, v1, v, min_dist, within_dist,
			     cos_within_ang, on_surf_points, false, below);
	    getSurfacePoints(s, u, u2, v, v2, min_dist, within_dist,
			     cos_within_ang, on_surf_points, false, false);
	} else if (udot < cos_within_ang - ON_ZERO_TOLERANCE) {
	    if (below) {
		p2d.Set(u, v1);
		on_surf_points.Append(p2d);
	    }
	    //top
	    p2d.Set(u, v2);
	    on_surf_points.Append(p2d);
	    getSurfacePoints(s, u1, u, v1, v2, min_dist, within_dist,
			     cos_within_ang, on_surf_points, left, below);
	    getSurfacePoints(s, u, u2, v1, v2, min_dist, within_dist,
			     cos_within_ang, on_surf_points, false, below);
	} else if (vdot < cos_within_ang - ON_ZERO_TOLERANCE) {
	    if (left) {
		p2d.Set(u1, v);
		on_surf_points.Append(p2d);
	    }
	    //right
	    p2d.Set(u2, v);
	    on_surf_points.Append(p2d);

	    getSurfacePoints(s, u1, u2, v1, v, min_dist, within_dist,
			     cos_within_ang, on_surf_points, left, below);
	    getSurfacePoints(s, u1, u2, v, v2, min_dist, within_dist,
			     cos_within_ang, on_surf_points, left, false);
	} else {
	    if (left) {
		p2d.Set(u1, v);
		on_surf_points.Append(p2d);
	    }
	    if (below) {
		p2d.Set(u, v1);
		on_surf_points.Append(p2d);
	    }
	    //center
	    p2d.Set(u, v);
	    on_surf_points.Append(p2d);
	    //right
	    p2d.Set(u2, v);
	    on_surf_points.Append(p2d);
	    //top
	    p2d.Set(u, v2);
	    on_surf_points.Append(p2d);

	    if (dist > within_dist + ON_ZERO_TOLERANCE) {

		getSurfacePoints(s, u1, u, v1, v, min_dist, within_dist,
				 cos_within_ang, on_surf_points, left, below);
		getSurfacePoints(s, u1, u, v, v2, min_dist, within_dist,
				 cos_within_ang, on_surf_points, left, false);
		getSurfacePoints(s, u, u2, v1, v, min_dist, within_dist,
				 cos_within_ang, on_surf_points, false, below);
		getSurfacePoints(s, u, u2, v, v2, min_dist, within_dist,
				 cos_within_ang, on_surf_points, false, false);
	    }
	}
    }
}


static void
getSurfacePoints(const ON_BrepFace &face,
		 const struct bg_tess_tol *ttol,
		 const struct bn_tol *tol,
		 ON_2dPointArray &on_surf_points)
{
    double surface_width, surface_height;
    const ON_Surface *s = face.SurfaceOf();
    const ON_Brep *brep = face.Brep();

    if (s->GetSurfaceSize(&surface_width, &surface_height)) {
	double dist = 0.0;
	double min_dist = 0.0;
	double within_dist = 0.0;
	double  cos_within_ang = 0.0;

	if ((surface_width < tol->dist) || (surface_height < tol->dist)) {
	    return;
	}

	// may be a smaller trimmed subset of surface so worth getting
	// face boundary
	bool bGrowBox = false;
	ON_3dPoint min, max;
	for (int li = 0; li < face.LoopCount(); li++) {
	    for (int ti = 0; ti < face.Loop(li)->TrimCount(); ti++) {
		const ON_BrepTrim *trim = face.Loop(li)->Trim(ti);
		trim->GetBoundingBox(min, max, bGrowBox);
		bGrowBox = true;
	    }
	}

	// Sanity
	if (!bGrowBox)
	    return;

	ON_BoundingBox tight_bbox;
	if (brep->GetTightBoundingBox(tight_bbox)) {
	    dist = DIST_PNT_PNT(tight_bbox.m_min, tight_bbox.m_max);
	}

	if (ttol->abs < tol->dist + ON_ZERO_TOLERANCE) {
	    min_dist = tol->dist;
	} else {
	    min_dist = ttol->abs;
	}

	double rel = 0.0;
	if (ttol->rel > 0.0 + ON_ZERO_TOLERANCE) {
	    rel = ttol->rel * dist;
	    within_dist = rel < min_dist ? min_dist : rel;
	    //if (ttol->abs < tol->dist + ON_ZERO_TOLERANCE) {
	    //    min_dist = within_dist;
	    //}
	} else if ((ttol->abs > 0.0 + ON_ZERO_TOLERANCE)
		   && (ttol->norm < 0.0 + ON_ZERO_TOLERANCE)) {
	    within_dist = min_dist;
	} else if ((ttol->abs > 0.0 + ON_ZERO_TOLERANCE)
		   || (ttol->norm > 0.0 + ON_ZERO_TOLERANCE)) {
	    within_dist = dist;
	} else {
	    within_dist = 0.01 * dist; // default to 1% minimum surface distance
	}

	if (ttol->norm > 0.0 + ON_ZERO_TOLERANCE) {
	    cos_within_ang = cos(ttol->norm);
	} else {
	    cos_within_ang = cos(ON_PI / 2.0);
	}
	bool uclosed = s->IsClosed(0);
	bool vclosed = s->IsClosed(1);
	if (uclosed && vclosed) {
	    ON_2dPoint p(0.0, 0.0);
	    double midx = (min.x + max.x) / 2.0;
	    double midy = (min.y + max.y) / 2.0;

	    //bottom left
	    p.Set(min.x, min.y);
	    on_surf_points.Append(p);

	    //midy left
	    p.Set(min.x, midy);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, min.x, midx, min.y, midy, min_dist, within_dist,
			     cos_within_ang, on_surf_points, true, true);

	    //bottom midx
	    p.Set(midx, min.y);
	    on_surf_points.Append(p);

	    //midx midy
	    p.Set(midx, midy);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, midx, max.x, min.y, midy, min_dist, within_dist,
			     cos_within_ang, on_surf_points, false, true);

	    //bottom right
	    p.Set(max.x, min.y);
	    on_surf_points.Append(p);

	    //right  midy
	    p.Set(max.x, midy);
	    on_surf_points.Append(p);

	    //top left
	    p.Set(min.x, max.y);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, min.x, midx, midy, max.y, min_dist, within_dist,
			     cos_within_ang, on_surf_points, true, false);

	    //top midx
	    p.Set(midx, max.y);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, midx, max.x, midy, max.y, min_dist, within_dist,
			     cos_within_ang, on_surf_points, false, false);

	    //top left
	    p.Set(max.x, max.y);
	    on_surf_points.Append(p);
	} else if (uclosed) {
	    ON_2dPoint p(0.0, 0.0);
	    double midx = (min.x + max.x) / 2.0;

	    //bottom left
	    p.Set(min.x, min.y);
	    on_surf_points.Append(p);

	    //top left
	    p.Set(min.x, max.y);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, min.x, midx, min.y, max.y, min_dist,
			     within_dist, cos_within_ang, on_surf_points, true, true);

	    //bottom midx
	    p.Set(midx, min.y);
	    on_surf_points.Append(p);

	    //top midx
	    p.Set(midx, max.y);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, midx, max.x, min.y, max.y, min_dist,
			     within_dist, cos_within_ang, on_surf_points, false, true);

	    //bottom right
	    p.Set(max.x, min.y);
	    on_surf_points.Append(p);

	    //top right
	    p.Set(max.x, max.y);
	    on_surf_points.Append(p);
	} else if (vclosed) {
	    ON_2dPoint p(0.0, 0.0);
	    double midy = (min.y + max.y) / 2.0;

	    //bottom left
	    p.Set(min.x, min.y);
	    on_surf_points.Append(p);

	    //left midy
	    p.Set(min.x, midy);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, min.x, max.x, min.y, midy, min_dist,
			     within_dist, cos_within_ang, on_surf_points, true, true);

	    //bottom right
	    p.Set(max.x, min.y);
	    on_surf_points.Append(p);

	    //right midy
	    p.Set(max.x, midy);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, min.x, max.x, midy, max.y, min_dist,
			     within_dist, cos_within_ang, on_surf_points, true, false);

	    // top left
	    p.Set(min.x, max.y);
	    on_surf_points.Append(p);

	    //top right
	    p.Set(max.x, max.y);
	    on_surf_points.Append(p);
	} else {
	    ON_2dPoint p(0.0, 0.0);

	    //bottom left
	    p.Set(min.x, min.y);
	    on_surf_points.Append(p);

	    //top left
	    p.Set(min.x, max.y);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, min.x, max.x, min.y, max.y, min_dist,
			     within_dist, cos_within_ang, on_surf_points, true, true);

	    //bottom right
	    p.Set(max.x, min.y);
	    on_surf_points.Append(p);

	    //top right
	    p.Set(max.x, max.y);
	    on_surf_points.Append(p);
	}
    }
}


static void
getUVCurveSamples(const ON_Surface *s,
		  const ON_Curve *curve,
		  fastf_t t1,
		  const ON_3dPoint &start_2d,
		  const ON_3dVector &start_tang,
		  const ON_3dPoint &start_3d,
		  const ON_3dVector &start_norm,
		  fastf_t t2,
		  const ON_3dPoint &end_2d,
		  const ON_3dVector &end_tang,
		  const ON_3dPoint &end_3d,
		  const ON_3dVector &end_norm,
		  fastf_t min_dist,
		  fastf_t max_dist,
		  fastf_t within_dist,
		  fastf_t cos_within_ang,
		  std::map<double, ON_3dPoint *> &param_points)
{
    static thread_local int recursion_depth = 0;
    if (recursion_depth >= FAST_CDT_MAX_RECURSION ||
	    param_points.size() >= FAST_CDT_MAX_TRIM_SAMPLES ||
	    !std::isfinite(t1) || !std::isfinite(t2) || !(t2 > t1))
	return;
    fast_recursion_guard guard(recursion_depth);

    ON_Interval range = curve->Domain();
    ON_3dPoint mid_2d(0.0, 0.0, 0.0);
    ON_3dPoint mid_3d(0.0, 0.0, 0.0);
    ON_3dVector mid_norm(0.0, 0.0, 0.0);
    ON_3dVector mid_tang(0.0, 0.0, 0.0);
    fastf_t t = (t1 + t2) / 2.0;
    if (!range.IsIncreasing() || !std::isfinite(t) ||
	    !(t > t1) || !(t < t2))
	return;

    if (curve->EvTangent(t, mid_2d, mid_tang)
	&& surface_EvNormal(s, mid_2d.x, mid_2d.y, mid_3d, mid_norm)) {
	ON_Line line3d(start_3d, end_3d);
	double dist3d;

	if ((line3d.Length() > max_dist)
	    || ((dist3d = mid_3d.DistanceTo(line3d.ClosestPointTo(mid_3d)))
		> within_dist + ON_ZERO_TOLERANCE)
	    || ((((start_tang * end_tang)
		  < cos_within_ang - ON_ZERO_TOLERANCE)
		 || ((start_norm * end_norm)
		     < cos_within_ang - ON_ZERO_TOLERANCE))
		&& (dist3d > min_dist + ON_ZERO_TOLERANCE))) {
	    getUVCurveSamples(s, curve, t1, start_2d, start_tang, start_3d, start_norm,
			      t, mid_2d, mid_tang, mid_3d, mid_norm, min_dist, max_dist,
			      within_dist, cos_within_ang, param_points);
	    param_points[(t - range.m_t[0]) / (range.m_t[1] - range.m_t[0])] =
		new ON_3dPoint(mid_3d);
	    getUVCurveSamples(s, curve, t, mid_2d, mid_tang, mid_3d, mid_norm, t2,
			      end_2d, end_tang, end_3d, end_norm, min_dist, max_dist,
			      within_dist, cos_within_ang, param_points);
	}
    }
}


static
std::map<double, ON_3dPoint *> *
getUVCurveSamples(const ON_Surface *surf,
		  const ON_Curve *curve,
		  fastf_t max_dist,
		  const struct bg_tess_tol *ttol,
		  const struct bn_tol *tol)
{
    fastf_t min_dist, within_dist, cos_within_ang;

    double dist = 1000.0;

    bool bGrowBox = false;
    ON_3dPoint min, max;
    if (curve->GetBoundingBox(min, max, bGrowBox)) {
	dist = DIST_PNT_PNT(min, max);
    }

    if (ttol->abs < tol->dist + ON_ZERO_TOLERANCE) {
	min_dist = tol->dist;
    } else {
	min_dist = ttol->abs;
    }

    double rel = 0.0;
    if (ttol->rel > 0.0 + ON_ZERO_TOLERANCE) {
	rel = ttol->rel * dist;
	if (max_dist < rel * 10.0) {
	    max_dist = rel * 10.0;
	}
	within_dist = rel < min_dist ? min_dist : rel;
    } else if (ttol->abs > 0.0 + ON_ZERO_TOLERANCE) {
	within_dist = min_dist;
    } else {
	within_dist = 0.01 * dist; // default to 1% minimum surface distance
    }

    if (ttol->norm > 0.0 + ON_ZERO_TOLERANCE) {
	cos_within_ang = cos(ttol->norm);
    } else {
	cos_within_ang = cos(ON_PI / 2.0);
    }

    std::map<double, ON_3dPoint *> *param_points = new std::map<double, ON_3dPoint *>();
    ON_Interval range = curve->Domain();

    if (curve->IsClosed()) {
	double mid_range = (range.m_t[0] + range.m_t[1]) / 2.0;
	ON_3dPoint start_2d(0.0, 0.0, 0.0);
	ON_3dPoint start_3d(0.0, 0.0, 0.0);
	ON_3dVector start_tang(0.0, 0.0, 0.0);
	ON_3dVector start_norm(0.0, 0.0, 0.0);
	ON_3dPoint mid_2d(0.0, 0.0, 0.0);
	ON_3dPoint mid_3d(0.0, 0.0, 0.0);
	ON_3dVector mid_tang(0.0, 0.0, 0.0);
	ON_3dVector mid_norm(0.0, 0.0, 0.0);
	ON_3dPoint end_2d(0.0, 0.0, 0.0);
	ON_3dPoint end_3d(0.0, 0.0, 0.0);
	ON_3dVector end_tang(0.0, 0.0, 0.0);
	ON_3dVector end_norm(0.0, 0.0, 0.0);

	if (curve->EvTangent(range.m_t[0], start_2d, start_tang)
	    && curve->EvTangent(mid_range, mid_2d, mid_tang)
	    && curve->EvTangent(range.m_t[1], end_2d, end_tang)
	    && surface_EvNormal(surf, mid_2d.x, mid_2d.y, mid_3d, mid_norm)
	    && surface_EvNormal(surf, start_2d.x, start_2d.y, start_3d, start_norm)
	    && surface_EvNormal(surf, end_2d.x, end_2d.y, end_3d, end_norm))
	{
	    (*param_points)[0.0] = new ON_3dPoint(
		surf->PointAt(curve->PointAt(range.m_t[0]).x,
			      curve->PointAt(range.m_t[0]).y));
	    getUVCurveSamples(surf, curve, range.m_t[0], start_2d, start_tang,
			      start_3d, start_norm, mid_range, mid_2d, mid_tang,
			      mid_3d, mid_norm, min_dist, max_dist, within_dist,
			      cos_within_ang, *param_points);
	    (*param_points)[0.5] = new ON_3dPoint(
		surf->PointAt(curve->PointAt(mid_range).x,
			      curve->PointAt(mid_range).y));
	    getUVCurveSamples(surf, curve, mid_range, mid_2d, mid_tang, mid_3d,
			      mid_norm, range.m_t[1], end_2d, end_tang, end_3d,
			      end_norm, min_dist, max_dist, within_dist,
			      cos_within_ang, *param_points);
	    (*param_points)[1.0] = new ON_3dPoint(
		surf->PointAt(curve->PointAt(range.m_t[1]).x,
			      curve->PointAt(range.m_t[1]).y));
	}
    } else {
	ON_3dPoint start_2d(0.0, 0.0, 0.0);
	ON_3dPoint start_3d(0.0, 0.0, 0.0);
	ON_3dVector start_tang(0.0, 0.0, 0.0);
	ON_3dVector start_norm(0.0, 0.0, 0.0);
	ON_3dPoint end_2d(0.0, 0.0, 0.0);
	ON_3dPoint end_3d(0.0, 0.0, 0.0);
	ON_3dVector end_tang(0.0, 0.0, 0.0);
	ON_3dVector end_norm(0.0, 0.0, 0.0);

	if (curve->EvTangent(range.m_t[0], start_2d, start_tang)
	    && curve->EvTangent(range.m_t[1], end_2d, end_tang)
	    && surface_EvNormal(surf, start_2d.x, start_2d.y, start_3d, start_norm)
	    && surface_EvNormal(surf, end_2d.x, end_2d.y, end_3d, end_norm))
	{
	    (*param_points)[0.0] = new ON_3dPoint(start_3d);
	    getUVCurveSamples(surf, curve, range.m_t[0], start_2d, start_tang,
			      start_3d, start_norm, range.m_t[1], end_2d, end_tang,
			      end_3d, end_norm, min_dist, max_dist, within_dist,
			      cos_within_ang, *param_points);
	    (*param_points)[1.0] = new ON_3dPoint(end_3d);
	}
    }


    return param_points;
}


/*
 * number_of_seam_crossings
 */
static int
number_of_seam_crossings(const ON_Surface *surf,  ON_SimpleArray<BrepTrimPoint> &brep_trim_points)
{
    int rc = 0;
    const ON_2dPoint *prev_non_seam_pt = NULL;
    for (int i = 0; i < brep_trim_points.Count(); i++) {
	const ON_2dPoint *pt = &brep_trim_points[i].p2d;
	if (!IsAtSeam(surf, *pt, BREP_SAME_POINT_TOLERANCE)) {
	    int udir = 0;
	    int vdir = 0;
	    if (prev_non_seam_pt != NULL) {
		if (ConsecutivePointsCrossClosedSeam(surf, *prev_non_seam_pt, *pt, udir, vdir, BREP_SAME_POINT_TOLERANCE)) {
		    rc++;
		}
	    }
	    prev_non_seam_pt = pt;
	}
    }

    return rc;
}


static bool
LoopStraddlesDomain(const ON_Surface *surf,  ON_SimpleArray<BrepTrimPoint> &brep_loop_points)
{
    if (surf->IsClosed(0) || surf->IsClosed(1)) {
	int num_crossings = number_of_seam_crossings(surf, brep_loop_points);
	if (num_crossings == 1) {
	    return true;
	}
    }
    return false;
}


/*
 * entering - 1
 * exiting - 2
 * contained - 0
 */
static int
is_entering(const ON_Surface *surf,  const ON_SimpleArray<BrepTrimPoint> &brep_loop_points)
{
    int numpoints = brep_loop_points.Count();
    for (int i = 1; i < numpoints - 1; i++) {
	int seam = 0;
	ON_2dPoint p = brep_loop_points[i].p2d;
	if ((seam = IsAtSeam(surf, p, BREP_SAME_POINT_TOLERANCE)) > 0) {
	    ON_2dPoint unwrapped = UnwrapUVPoint(surf, p, BREP_SAME_POINT_TOLERANCE);
	    if (seam == 1) {
		bool right_seam = unwrapped.x > surf->Domain(0).Mid();
		bool decreasing = (brep_loop_points[numpoints - 1].p2d.x - brep_loop_points[0].p2d.x) < 0;
		if (right_seam != decreasing) { // basically XOR'ing here
		    return 2;
		} else {
		    return 1;
		}
	    } else {
		bool top_seam = unwrapped.y > surf->Domain(1).Mid();
		bool decreasing = (brep_loop_points[numpoints - 1].p2d.y - brep_loop_points[0].p2d.y) < 0;
		if (top_seam != decreasing) { // basically XOR'ing here
		    return 2;
		} else {
		    return 1;
		}
	    }
	}
    }
    return 0;
}

/*
 * shift_closed_curve_split_over_seam
 */
static bool
shift_loop_straddled_over_seam(const ON_Surface *surf,  ON_SimpleArray<BrepTrimPoint> &brep_loop_points, double same_point_tolerance)
{
    if (surf->IsClosed(0) || surf->IsClosed(1)) {
	ON_Interval dom[2];
	int entering = is_entering(surf, brep_loop_points);

	dom[0] = surf->Domain(0);
	dom[1] = surf->Domain(1);

	int seam = 0;
	int i;
	BrepTrimPoint btp;
	BrepTrimPoint end_btp;
	ON_SimpleArray<BrepTrimPoint> part1;
	ON_SimpleArray<BrepTrimPoint> part2;

	end_btp.p2d = ON_2dPoint::UnsetPoint;
	int numpoints = brep_loop_points.Count();
	bool first_seam_pt = true;
	for (i = 0; i < numpoints; i++) {
	    btp = brep_loop_points[i];
	    seam = IsAtSeam(surf, btp.p2d, same_point_tolerance);
	    if (seam > 0) {
		if (first_seam_pt) {
		    part1.Append(btp);
		    first_seam_pt = false;
		}
		end_btp = btp;
		SwapUVSeamPoint(surf, end_btp.p2d);
		part2.Append(end_btp);
	    } else {
		if (dom[0].Includes(btp.p2d.x, false) && dom[1].Includes(btp.p2d.y, false)) {
		    part1.Append(brep_loop_points[i]);
		} else {
		    btp = brep_loop_points[i];
		    btp.p2d = UnwrapUVPoint(surf, brep_loop_points[i].p2d, same_point_tolerance);
		    part2.Append(btp);
		}
	    }
	}

	brep_loop_points.Empty();
	if (entering == 1) {
	    brep_loop_points.Append(part1.Count() - 1, part1.Array());
	    brep_loop_points.Append(part2.Count(), part2.Array());
	} else {
	    brep_loop_points.Append(part2.Count() - 1, part2.Array());
	    brep_loop_points.Append(part1.Count(), part1.Array());
	}
    }
    return true;
}


/*
 * extend_over_seam_crossings
 */
static bool
extend_over_seam_crossings(const ON_Surface *surf,  ON_SimpleArray<BrepTrimPoint> &brep_loop_points)
{
    int num_points = brep_loop_points.Count();
    double ulength = surf->Domain(0).Length();
    double vlength = surf->Domain(1).Length();
    if ((surf->IsClosed(0) &&
	    (!(ulength > ON_ZERO_TOLERANCE) || !std::isfinite(ulength))) ||
	    (surf->IsClosed(1) &&
	    (!(vlength > ON_ZERO_TOLERANCE) || !std::isfinite(vlength))))
	return false;
    for (int i = 1; i < num_points; i++) {
	if (surf->IsClosed(0)) {
	    double delta = brep_loop_points[i].p2d.x - brep_loop_points[i - 1].p2d.x;
	    int shifts = 0;
	    while (std::isfinite(delta) && fabs(delta) > ulength / 2.0 &&
		    shifts++ < 64) {
		if (delta < 0.0) {
		    brep_loop_points[i].p2d.x += ulength; // east bound
		} else {
		    brep_loop_points[i].p2d.x -= ulength;; // west bound
		}
		delta = brep_loop_points[i].p2d.x - brep_loop_points[i - 1].p2d.x;
	    }
	    if (!std::isfinite(delta) || fabs(delta) > ulength / 2.0)
		return false;
	}
	if (surf->IsClosed(1)) {
	    double delta = brep_loop_points[i].p2d.y - brep_loop_points[i - 1].p2d.y;
	    int shifts = 0;
	    while (std::isfinite(delta) && fabs(delta) > vlength / 2.0 &&
		    shifts++ < 64) {
		if (delta < 0.0) {
		    brep_loop_points[i].p2d.y += vlength; // north bound
		} else {
		    brep_loop_points[i].p2d.y -= vlength;; // south bound
		}
		delta = brep_loop_points[i].p2d.y - brep_loop_points[i - 1].p2d.y;
	    }
	    if (!std::isfinite(delta) || fabs(delta) > vlength / 2.0)
		return false;
	}
    }

    return true;
}

static void
get_loop_sample_points(
	ON_SimpleArray<BrepTrimPoint> *points,
	const ON_BrepFace &face,
	const ON_BrepLoop *loop,
	fastf_t max_dist,
	const struct bg_tess_tol *ttol,
	const struct bn_tol *tol,
	fast_face_scratch &scratch)
{
    if (!loop)
	return;
    int trim_count = loop->TrimCount();

    for (int lti = 0; lti < trim_count; lti++) {
	ON_BrepTrim *trim = loop->Trim(lti);
	if (!trim)
	    continue;
	//ON_BrepEdge *edge = trim->Edge();

	if (trim->m_type == ON_BrepTrim::singular) {
	    BrepTrimPoint btp;
	    const ON_BrepVertex& v1 = face.Brep()->m_V[trim->m_vi[0]];
	    ON_3dPoint *p3d = scratch.make_point(v1.Point());
	    //ON_2dPoint p2d_begin = trim->PointAt(trim->Domain().m_t[0]);
	    //ON_2dPoint p2d_end = trim->PointAt(trim->Domain().m_t[1]);
	    double delta =  trim->Domain().Length() / 10.0;

	    for (int i = 1; i <= 10; i++) {
		btp.p3d = p3d;
		btp.n3d = NULL;
		btp.p2d = v1.Point();
		btp.t = trim->Domain().m_t[0] + (i - 1) * delta;
		btp.p2d = trim->PointAt(btp.t);
		btp.e = ON_UNSET_VALUE;
		points->Append(btp);
	    }
	    // skip last point of trim if not last trim
	    if (lti < trim_count - 1)
		continue;

	    const ON_BrepVertex& v2 = face.Brep()->m_V[trim->m_vi[1]];
	    btp.p3d = p3d;
	    btp.n3d = NULL;
	    btp.p2d = v2.Point();
	    btp.t = trim->Domain().m_t[1];
	    btp.p2d = trim->PointAt(btp.t);
	    btp.e = ON_UNSET_VALUE;
	    points->Append(btp);

	    continue;
	}

	fast_trim_point_map *param_points3d = getEdgePoints(*trim, max_dist,
		ttol, tol, scratch);
	if (param_points3d) {
	    //bu_log("Trim %d (associated with Edge %d) point count: %zd\n", trim->m_trim_index, trim->Edge()->m_edge_index, param_points3d->size());

	    ON_3dPoint boxmin;
	    ON_3dPoint boxmax;

	    if (trim->GetBoundingBox(boxmin, boxmax, false)) {
		double t0, t1;

		std::map<double, BrepTrimPoint*>::const_iterator i;

		trim->GetDomain(&t0, &t1);
		for (i = param_points3d->begin(); i != param_points3d->end();) {
		    BrepTrimPoint *btp = (*i).second;
		    // skip last point of trim if not last trim
		    if ((++i == param_points3d->end()) && (lti < trim_count - 1)) {
			continue;
		    }
		    points->Append(*btp);
		}
	    }
	}
    }
}


/* force near seam points to seam */
static void
ForceNearSeamPointsToSeam(
	const ON_Surface *s,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double same_point_tolerance)
{
    int loop_cnt = face.LoopCount();
    for (int li = 0; li < loop_cnt; li++) {
	int num_loop_points = brep_loop_points[li]->Count();
	if (num_loop_points > 1) {
	    for (int i = 0; i < num_loop_points; i++) {
		ON_2dPoint &p = (*brep_loop_points[li])[i].p2d;
		if (IsAtSeam(s, p, same_point_tolerance)) {
		    ForceToClosestSeam(s, p, same_point_tolerance);
		}
	    }
	}
    }
}


static void
ExtendPointsOverClosedSeam(
	const ON_Surface *s,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points)
{
    int loop_cnt = face.LoopCount();
    // extend loop points over seam if needed.
    for (int li = 0; li < loop_cnt; li++) {
	int num_loop_points = brep_loop_points[li]->Count();
	if (num_loop_points > 1) {
	    if (!extend_over_seam_crossings(s, *brep_loop_points[li])) {
		std::cerr << "Error: Face(" << face.m_face_index << ") cannot extend loops over closed seams." << std::endl;
	    }
	}
    }
}


// process through loops checking for straddle condition.
static void
ShiftLoopsThatStraddleSeam(
	const ON_Surface *s,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double same_point_tolerance)
{
    int loop_cnt = face.LoopCount();
    for (int li = 0; li < loop_cnt; li++) {
	int num_loop_points = brep_loop_points[li]->Count();
	if (num_loop_points > 1) {
	    ON_2dPoint brep_loop_begin = (*brep_loop_points[li])[0].p2d;
	    ON_2dPoint brep_loop_end = (*brep_loop_points[li])[num_loop_points - 1].p2d;

	    if (!V2NEAR_EQUAL(brep_loop_begin, brep_loop_end, same_point_tolerance)) {
		if (LoopStraddlesDomain(s, *brep_loop_points[li])) {
		    // reorder loop points
		    shift_loop_straddled_over_seam(s, *brep_loop_points[li], same_point_tolerance);
		}
	    }
	}
    }
}


// process through closing open loops that begin and end on closed seam
static void
CloseOpenLoops(
	const ON_Surface *s,
	const ON_BrepFace &face,
	const struct bg_tess_tol *ttol,
	const struct bn_tol *tol,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double same_point_tolerance,
	fast_bridge_store &bridgePoints)
{
    int loop_cnt = face.LoopCount();
    for (int li = 0; li < loop_cnt; li++) {
	int num_loop_points = brep_loop_points[li]->Count();
	if (num_loop_points > 1) {
	    ON_2dPoint brep_loop_begin = (*brep_loop_points[li])[0].p2d;
	    ON_2dPoint brep_loop_end = (*brep_loop_points[li])[num_loop_points - 1].p2d;
	    ON_3dPoint brep_loop_begin3d = s->PointAt(brep_loop_begin.x, brep_loop_begin.y);
	    ON_3dPoint brep_loop_end3d = s->PointAt(brep_loop_end.x, brep_loop_end.y);

	    if (!V2NEAR_EQUAL(brep_loop_begin, brep_loop_end, same_point_tolerance) &&
		VNEAR_EQUAL(brep_loop_begin3d, brep_loop_end3d, same_point_tolerance)) {
		int seam_begin = 0;
		int seam_end = 0;
		if ((seam_begin = IsAtSeam(s, brep_loop_begin, same_point_tolerance)) &&
		    (seam_end = IsAtSeam(s, brep_loop_end, same_point_tolerance))) {
		    bool loop_not_closed = true;
		    if ((li + 1) < loop_cnt) {
			// close using remaining loops
			for (int rli = li + 1; rli < loop_cnt; rli++) {
			    int rnum_loop_points = brep_loop_points[rli]->Count();
			    if (rnum_loop_points < 2)
				continue;
			    ON_2dPoint rbrep_loop_begin = (*brep_loop_points[rli])[0].p2d;
			    ON_2dPoint rbrep_loop_end = (*brep_loop_points[rli])[rnum_loop_points - 1].p2d;
			    if (!V2NEAR_EQUAL(rbrep_loop_begin, rbrep_loop_end, same_point_tolerance)) {
				if (IsAtSeam(s, rbrep_loop_begin, same_point_tolerance) && IsAtSeam(s, rbrep_loop_end, same_point_tolerance)) {
				    double t0, t1;
				    ON_LineCurve line1(brep_loop_end, rbrep_loop_begin);
				    std::map<double, ON_3dPoint *> *linepoints3d = getUVCurveSamples(s, &line1, 1000.0, ttol, tol);
				    bridgePoints.push_back(linepoints3d);
				    line1.GetDomain(&t0, &t1);
				    std::map<double, ON_3dPoint*>::const_iterator i;
				    for (i = linepoints3d->begin();
					 i != linepoints3d->end(); i++) {
					BrepTrimPoint btp;

					// skips first point
					if (i == linepoints3d->begin())
					    continue;

					btp.t = (*i).first;
					btp.p3d = (*i).second;
					btp.n3d = NULL;
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li]->Append(btp);
				    }
				    //brep_loop_points[li].Append(brep_loop_points[rli].Count(),brep_loop_points[rli].Array());
				    for (int j = 1; j < rnum_loop_points; j++) {
					brep_loop_points[li]->Append((*brep_loop_points[rli])[j]);
				    }
				    ON_LineCurve line2(rbrep_loop_end, brep_loop_begin);
				    linepoints3d = getUVCurveSamples(s, &line2, 1000.0, ttol, tol);
				    bridgePoints.push_back(linepoints3d);
				    line2.GetDomain(&t0, &t1);

				    for (i = linepoints3d->begin();
					 i != linepoints3d->end(); i++) {
					BrepTrimPoint btp;
					// skips first point
					if (i == linepoints3d->begin())
					    continue;

					btp.t = (*i).first;
					btp.p3d = (*i).second;
					btp.n3d = NULL;
					btp.p2d = line2.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li]->Append(btp);
				    }
				    brep_loop_points[rli]->Empty();
				    loop_not_closed = false;
				}
			    }
			}
		    }
		    if (loop_not_closed) {
			// no matching loops found that would close so use domain boundary
			ON_Interval u = s->Domain(0);
			ON_Interval v = s->Domain(1);
			if (seam_end == 1) {
			    if (NEAR_EQUAL(brep_loop_end.x, u.m_t[0], same_point_tolerance)) {
				// low end so decreasing

				// now where do we have to close to
				if (seam_begin == 1) {
				    // has to be on opposite seam
				    double t0, t1;
				    ON_2dPoint p = brep_loop_end;
				    p.y = v.m_t[0];
				    ON_LineCurve line1(brep_loop_end, p);
				    std::map<double, ON_3dPoint *> *linepoints3d = getUVCurveSamples(s, &line1, 1000.0, ttol, tol);
				    bridgePoints.push_back(linepoints3d);
				    line1.GetDomain(&t0, &t1);
				    std::map<double, ON_3dPoint*>::const_iterator i;
				    for (i = linepoints3d->begin();
					 i != linepoints3d->end(); i++) {
					BrepTrimPoint btp;

					// skips first point
					if (i == linepoints3d->begin())
					    continue;

					btp.t = (*i).first;
					btp.p3d = (*i).second;
					btp.n3d = NULL;
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li]->Append(btp);
				    }
				    line1.SetStartPoint(p);
				    p.x = u.m_t[1];
				    line1.SetEndPoint(p);
				    linepoints3d = getUVCurveSamples(s, &line1, 1000.0, ttol, tol);
				    bridgePoints.push_back(linepoints3d);
				    line1.GetDomain(&t0, &t1);
				    for (i = linepoints3d->begin();
					 i != linepoints3d->end(); i++) {
					BrepTrimPoint btp;

					// skips first point
					if (i == linepoints3d->begin())
					    continue;

					btp.t = (*i).first;
					btp.p3d = (*i).second;
					btp.n3d = NULL;
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li]->Append(btp);
				    }
				    line1.SetStartPoint(p);
				    line1.SetEndPoint(brep_loop_begin);
				    linepoints3d = getUVCurveSamples(s, &line1, 1000.0, ttol, tol);
				    bridgePoints.push_back(linepoints3d);
				    line1.GetDomain(&t0, &t1);
				    for (i = linepoints3d->begin();
					 i != linepoints3d->end(); i++) {
					BrepTrimPoint btp;

					// skips first point
					if (i == linepoints3d->begin())
					    continue;

					btp.t = (*i).first;
					btp.p3d = (*i).second;
					btp.n3d = NULL;
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li]->Append(btp);
				    }

				} else if (seam_begin == 2) {

				} else {
				    //both needed
				}

			    } else { //assume on other end
				// high end so increasing
				// now where do we have to close to
				if (seam_begin == 1) {
				    // has to be on opposite seam
				    double t0, t1;
				    ON_2dPoint p = brep_loop_end;
				    p.y = v.m_t[1];
				    ON_LineCurve line1(brep_loop_end, p);
				    std::map<double, ON_3dPoint *> *linepoints3d = getUVCurveSamples(s, &line1, 1000.0, ttol, tol);
				    bridgePoints.push_back(linepoints3d);
				    line1.GetDomain(&t0, &t1);
				    std::map<double, ON_3dPoint*>::const_iterator i;
				    for (i = linepoints3d->begin();
					 i != linepoints3d->end(); i++) {
					BrepTrimPoint btp;

					// skips first point
					if (i == linepoints3d->begin())
					    continue;

					btp.t = (*i).first;
					btp.p3d = (*i).second;
					btp.n3d = NULL;
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li]->Append(btp);
				    }
				    line1.SetStartPoint(p);
				    p.x = u.m_t[0];
				    line1.SetEndPoint(p);
				    linepoints3d = getUVCurveSamples(s, &line1, 1000.0, ttol, tol);
				    bridgePoints.push_back(linepoints3d);
				    line1.GetDomain(&t0, &t1);
				    for (i = linepoints3d->begin();
					 i != linepoints3d->end(); i++) {
					BrepTrimPoint btp;

					// skips first point
					if (i == linepoints3d->begin())
					    continue;

					btp.t = (*i).first;
					btp.p3d = (*i).second;
					btp.n3d = NULL;
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li]->Append(btp);
				    }
				    line1.SetStartPoint(p);
				    line1.SetEndPoint(brep_loop_begin);
				    linepoints3d = getUVCurveSamples(s, &line1, 1000.0, ttol, tol);
				    bridgePoints.push_back(linepoints3d);
				    line1.GetDomain(&t0, &t1);
				    for (i = linepoints3d->begin();
					 i != linepoints3d->end(); i++) {
					BrepTrimPoint btp;

					// skips first point
					if (i == linepoints3d->begin())
					    continue;

					btp.t = (*i).first;
					btp.p3d = (*i).second;
					btp.n3d = NULL;
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li]->Append(btp);
				    }
				} else if (seam_begin == 2) {

				} else {
				    //both
				}
			    }
			} else if (seam_end == 2) {
			    if (NEAR_EQUAL(brep_loop_end.y, v.m_t[0], same_point_tolerance)) {

			    } else { //assume on other end

			    }
			} else {
			    //both
			}
		    }
		}
	    }
	}
    }
}


/*
 * lifted from Clipper private function and rework for brep_loop_points
 */
static bool
PointInPolygon(const ON_2dPoint &pt, ON_SimpleArray<BrepTrimPoint> &brep_loop_points)
{
    bool result = false;

    for( int i = 0; i < brep_loop_points.Count(); i++){
	ON_2dPoint curr_pt = brep_loop_points[i].p2d;
	ON_2dPoint prev_pt = ON_2dPoint::UnsetPoint;
	if (i == 0) {
	    prev_pt = brep_loop_points[brep_loop_points.Count()-1].p2d;
	} else {
	    prev_pt = brep_loop_points[i-1].p2d;
	}
	if ((((curr_pt.y <= pt.y) && (pt.y < prev_pt.y)) ||
		((prev_pt.y <= pt.y) && (pt.y < curr_pt.y))) &&
		(pt.x < (prev_pt.x - curr_pt.x) * (pt.y - curr_pt.y) /
			(prev_pt.y - curr_pt.y) + curr_pt.x ))
	    result = !result;
    }
    return result;
}


static void
ShiftPoints(ON_SimpleArray<BrepTrimPoint> &brep_loop_points, double ushift, double vshift)
{
    for( int i = 0; i < brep_loop_points.Count(); i++){
	brep_loop_points[i].p2d.x += ushift;
	brep_loop_points[i].p2d.y += vshift;
    }
}


// process through to make sure inner hole loops are actually inside of outer polygon
// need to make sure that any hole polygons are properly shifted over correct closed seams
// going to try and do an inside test on hole vertex
static void
ShiftInnerLoops(
	const ON_Surface *s,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points)
{
    int loop_cnt = face.LoopCount();
    if (loop_cnt > 1) { // has inner loops or holes
	for( int li = 1; li < loop_cnt; li++) {
	    if (!brep_loop_points[li]->Count())
		continue;
	    ON_2dPoint p2d((*brep_loop_points[li])[0].p2d.x, (*brep_loop_points[li])[0].p2d.y);
	    if (!PointInPolygon(p2d, *brep_loop_points[0])) {
		double ulength = s->Domain(0).Length();
		double vlength = s->Domain(1).Length();
		ON_2dPoint sftd_pt = p2d;

		//do shift until inside
		if (s->IsClosed(0) && s->IsClosed(1)) {
		    // First just U
		    for(int iu = 0; iu < 2; iu++) {
			double ushift = 0.0;
			if (iu == 0) {
			    ushift = -ulength;
			} else {
			    ushift =  ulength;
			}
			sftd_pt.x = p2d.x + ushift;
			if (PointInPolygon(sftd_pt, *brep_loop_points[0])) {
			    // shift all U accordingly
			    ShiftPoints(*brep_loop_points[li], ushift, 0.0);
			    break;
			}
		    }
		    // Second just V
		    for(int iv = 0; iv < 2; iv++) {
			double vshift = 0.0;
			if (iv == 0) {
			    vshift = -vlength;
			} else {
			    vshift = vlength;
			}
			sftd_pt.y = p2d.y + vshift;
			if (PointInPolygon(sftd_pt, *brep_loop_points[0])) {
			    // shift all V accordingly
			    ShiftPoints(*brep_loop_points[li], 0.0, vshift);
			    break;
			}
		    }
		    // Third both U & V
		    for(int iu = 0; iu < 2; iu++) {
			double ushift = 0.0;
			if (iu == 0) {
			    ushift = -ulength;
			} else {
			    ushift =  ulength;
			}
			sftd_pt.x = p2d.x + ushift;
			for(int iv = 0; iv < 2; iv++) {
			    double vshift = 0.0;
			    if (iv == 0) {
				vshift = -vlength;
			    } else {
				vshift = vlength;
			    }
			    sftd_pt.y = p2d.y + vshift;
			    if (PointInPolygon(sftd_pt, *brep_loop_points[0])) {
				// shift all U & V accordingly
				ShiftPoints(*brep_loop_points[li], ushift, vshift);
				break;
			    }
			}
		    }
		} else if (s->IsClosed(0)) {
		    // just U
		    for(int iu = 0; iu < 2; iu++) {
			double ushift = 0.0;
			if (iu == 0) {
			    ushift = -ulength;
			} else {
			    ushift =  ulength;
			}
			sftd_pt.x = p2d.x + ushift;
			if (PointInPolygon(sftd_pt, *brep_loop_points[0])) {
			    // shift all U accordingly
			    ShiftPoints(*brep_loop_points[li], ushift, 0.0);
			    break;
			}
		    }
		} else if (s->IsClosed(1)) {
		    // just V
		    for(int iv = 0; iv < 2; iv++) {
			double vshift = 0.0;
			if (iv == 0) {
			    vshift = -vlength;
			} else {
			    vshift = vlength;
			}
			sftd_pt.y = p2d.y + vshift;
			if (PointInPolygon(sftd_pt, *brep_loop_points[0])) {
			    // shift all V accordingly
			    ShiftPoints(*brep_loop_points[li], 0.0, vshift);
			    break;
			}
		    }
		}
	    }
	}
    }
}


static void
PerformClosedSurfaceChecks(
	const ON_Surface *s,
	const ON_BrepFace &face,
	const struct bg_tess_tol *ttol,
	const struct bn_tol *tol,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double same_point_tolerance,
	fast_bridge_store &bridge_store)
{
    // force near seam points to seam.
    ForceNearSeamPointsToSeam(s, face, brep_loop_points, same_point_tolerance);

    // extend loop points over closed seam if needed.
    ExtendPointsOverClosedSeam(s, face, brep_loop_points);

    // shift open loops that straddle a closed seam with the intent of closure at the surface boundary.
    ShiftLoopsThatStraddleSeam(s, face, brep_loop_points, same_point_tolerance);

    // process through closing open loops that begin and end on closed seam
    CloseOpenLoops(s, face, ttol, tol, brep_loop_points,
	same_point_tolerance, bridge_store);

    // process through to make sure inner hole loops are actually inside of outer polygon
    // need to make sure that any hole polygons are properly shifted over correct closed seams
    ShiftInnerLoops(s, face, brep_loop_points);
}

void
detria_CDT(struct bu_list *vhead,
	     const ON_BrepFace &face,
	     const struct bg_tess_tol *ttol,
	     const struct bn_tol *tol,
	     struct bu_list *vlfree,
	     int plottype,
	     int UNUSED(num_points))
{
    fast_face_scratch scratch;
    fast_line_store line_store;
    ON_RTree rt_trims;
    ON_2dPointArray on_surf_points;
    const ON_Surface *s = face.SurfaceOf();
    if (!s)
	return;
    double surface_width, surface_height;
    int fi = face.m_face_index;

    fastf_t max_dist = 0.0;
    if (s->GetSurfaceSize(&surface_width, &surface_height)) {
	if ((surface_width < tol->dist) || (surface_height < tol->dist)) {
	    return;
	}
	max_dist = sqrt(surface_width * surface_width + surface_height *
		surface_height) / 10.0;
    }


    int loop_cnt = face.LoopCount();
    ON_2dPointArray on_loop_points;
    fast_loop_point_store loop_points(loop_cnt);
    ON_SimpleArray<BrepTrimPoint> **brep_loop_points = loop_points.points;

    // first simply load loop point samples
    for (int li = 0; li < loop_cnt; li++) {
	const ON_BrepLoop *loop = face.Loop(li);
	get_loop_sample_points(brep_loop_points[li], face, loop, max_dist,
		ttol, tol, scratch);
    }
    if (scratch.hit_sample_limit) {
	return;
    }

    fast_bridge_store bridgePoints;
    if (s->IsClosed(0) || s->IsClosed(1)) {
	PerformClosedSurfaceChecks(s, face, ttol, tol, brep_loop_points,
	    BREP_SAME_POINT_TOLERANCE, bridgePoints);

    }
    // process through loops building polygons.
    std::vector<detria::PointD> tpnts;
    std::vector<int> outer_polyline;
    std::vector<std::vector<int>> holes;
    std::map<size_t, ON_3dPoint *> pointmap;
    bool outer = true;
    for (int li = 0; li < loop_cnt; li++) {
	std::vector<int> polyline;
	int num_loop_points = brep_loop_points[li]->Count();
	if (num_loop_points > 2) {
	    for (int i = 1; i < num_loop_points; i++) {
		// map point to last entry to 3d point
		detria::PointD npt;
		npt.x = (*brep_loop_points[li])[i].p2d.x;
		npt.y = (*brep_loop_points[li])[i].p2d.y;
		tpnts.push_back(npt);
		polyline.push_back(tpnts.size() - 1);
		pointmap[tpnts.size() - 1] = (*brep_loop_points[li])[i].p3d;
	    }
	    for (int i = 1; i < brep_loop_points[li]->Count(); i++) {
		// map point to last entry to 3d point
		ON_Line *line = new ON_Line((*brep_loop_points[li])[i - 1].p2d, (*brep_loop_points[li])[i].p2d);
		line_store.lines.push_back(line);
		ON_BoundingBox bb = line->BoundingBox();

		bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
		bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
		bb.m_max.z = bb.m_max.z + ON_ZERO_TOLERANCE;
		bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
		bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;
		bb.m_min.z = bb.m_min.z - ON_ZERO_TOLERANCE;

		rt_trims.Insert2d(bb.Min(), bb.Max(), line);
	    }
	    if (outer) {
		outer_polyline = polyline;
		outer = false;
	    } else {
		holes.push_back(polyline);
	    }
	}
    }

    if (outer) {
	std::cerr << "Error: Face(" << fi << ") cannot evaluate its outer loop and will not be facetized." << std::endl;
	return;
    }

    getSurfacePoints(face, ttol, tol, on_surf_points);
    if (on_surf_points.Count() >= FAST_CDT_MAX_SURFACE_SAMPLES) {
	return;
    }

    for (int i = 0; i < on_surf_points.Count(); i++) {
	ON_SimpleArray<void*> results;
	const ON_2dPoint *p = on_surf_points.At(i);

	rt_trims.Search2d((const double *) p, (const double *) p, results);

	if (results.Count() > 0) {
	    bool on_edge = false;
	    for (int ri = 0; ri < results.Count(); ri++) {
		double dist;
		const ON_Line *l = (const ON_Line *) *results.At(ri);
		dist = l->MinimumDistanceTo(*p);
		if (NEAR_ZERO(dist, tol->dist)) {
		    on_edge = true;
		    break;
		}
	    }
	    if (!on_edge) {
		detria::PointD npt;
		npt.x = p->x;
		npt.y = p->y;
		tpnts.push_back(npt);
	    }
	} else {
	    detria::PointD npt;
	    npt.x = p->x;
	    npt.y = p->y;
	    tpnts.push_back(npt);
	}
    }

    ON_SimpleArray<void*> results;
    ON_BoundingBox bb = rt_trims.BoundingBox();

    rt_trims.Search2d((const double *) bb.m_min, (const double *) bb.m_max, results);

    rt_trims.RemoveAll();

    // Run the core triangulation routine
    detria::Triangulation<detria::PointD, int> tri;
    tri.setPoints(tpnts);
    tri.addOutline(outer_polyline);
    for (size_t i = 0; i < holes.size(); i++)
	tri.addHole(holes[i]);

    bool tri_success = false;
    {
	try {
	    tri_success = tri.triangulate(true);
	}
	catch (...) {
	    return;
	}
    }

    if (!tri_success)
	return;

    if (plottype < 3) {
	if (plottype == 0) { // shaded tris 3d
            ON_3dPoint pnt[3] = {ON_3dPoint(), ON_3dPoint(), ON_3dPoint()};
            ON_3dVector norm[3] = {ON_3dVector(), ON_3dVector(), ON_3dVector()};
            point_t pt[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
            vect_t nv[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
            tri.forEachTriangle([&](const detria::Triangle<int> triangle)
            {
                int tris[3];
                tris[0] = triangle.x;
                tris[1] = triangle.y;
                tris[2] = triangle.z;
                for (size_t j = 0; j < 3; j++) {
                    if (surface_EvNormal(s, tpnts[tris[j]].x, tpnts[tris[j]].y, pnt[j], norm[j])) {
                        std::map<size_t, ON_3dPoint *>::const_iterator ii = pointmap.find(tris[j]);
                        if (ii != pointmap.end()) {
                	    pnt[j] = *((*ii).second);
                	}
                	if (face.m_bRev) {
                	    norm[j] = norm[j] * -1.0;
                	}
                	VMOVE(pt[j], pnt[j]);
                	VMOVE(nv[j], norm[j]);
                    }
                }
                //tri one
                BV_ADD_VLIST(vlfree, vhead, nv[0], BV_VLIST_TRI_START);
                BV_ADD_VLIST(vlfree, vhead, nv[0], BV_VLIST_TRI_VERTNORM);
                BV_ADD_VLIST(vlfree, vhead, pt[0], BV_VLIST_TRI_MOVE);
                BV_ADD_VLIST(vlfree, vhead, nv[1], BV_VLIST_TRI_VERTNORM);
                BV_ADD_VLIST(vlfree, vhead, pt[1], BV_VLIST_TRI_DRAW);
                BV_ADD_VLIST(vlfree, vhead, nv[2], BV_VLIST_TRI_VERTNORM);
                BV_ADD_VLIST(vlfree, vhead, pt[2], BV_VLIST_TRI_DRAW);
                BV_ADD_VLIST(vlfree, vhead, pt[0], BV_VLIST_TRI_END);
            }, true);
	} else if (plottype == 1) { // tris 3d wire
	    ON_3dPoint pnt[3] = {ON_3dPoint(), ON_3dPoint(), ON_3dPoint()};;
	    ON_3dVector norm[3] = {ON_3dVector(), ON_3dVector(), ON_3dVector()};;
	    point_t pt[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
            tri.forEachTriangle([&](const detria::Triangle<int> triangle)
            {
                int tris[3];
                tris[0] = triangle.x;
                tris[1] = triangle.y;
                tris[2] = triangle.z;
                for (size_t j = 0; j < 3; j++) {
                    if (surface_EvNormal(s, tpnts[tris[j]].x, tpnts[tris[j]].y, pnt[j], norm[j])) {
                        std::map<size_t, ON_3dPoint *>::const_iterator ii = pointmap.find(tris[j]);
                        if (ii != pointmap.end()) {
                	    pnt[j] = *((*ii).second);
                	}
                	if (face.m_bRev) {
                	    norm[j] = norm[j] * -1.0;
                	}
                	VMOVE(pt[j], pnt[j]);
                    }
                }
		//tri one
		BV_ADD_VLIST(vlfree, vhead, pt[0], BV_VLIST_LINE_MOVE);
		BV_ADD_VLIST(vlfree, vhead, pt[1], BV_VLIST_LINE_DRAW);
		BV_ADD_VLIST(vlfree, vhead, pt[2], BV_VLIST_LINE_DRAW);
		BV_ADD_VLIST(vlfree, vhead, pt[0], BV_VLIST_LINE_DRAW);
	    }, true);
	} else if (plottype == 2) { // tris 2d
	    point_t pt1 = VINIT_ZERO;
	    point_t pt2 = VINIT_ZERO;
	    tri.forEachTriangle([&](const detria::Triangle<int> triangle)
	    {
                int tris[3];
                tris[0] = triangle.x;
                tris[1] = triangle.y;
                tris[2] = triangle.z;
		int p;
                for (size_t j = 0; j < 3; j++) {
           	    if (j == 0) {
			p = 2;
		    } else {
			p = j - 1;
		    }
	            pt1[0] = tpnts[tris[p]].x;
		    pt1[1] = tpnts[tris[p]].y;
		    pt1[2] = 0.0;
		    pt2[0] = tpnts[tris[j]].x;
		    pt2[1] = tpnts[tris[j]].y;
		    pt2[2] = 0.0;
		    BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
		    BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
		}
   	    }, true);
	}
    } else if (plottype == 3) {
	point_t pt1 = VINIT_ZERO;
	point_t pt2 = VINIT_ZERO;
        tri.forEachTriangle([&](const detria::Triangle<int> triangle)
	{
            int tris[3];
            tris[0] = triangle.x;
            tris[1] = triangle.y;
            tris[2] = triangle.z;
	    int p;
            for (size_t j = 0; j < 3; j++) {
                if (j == 0) {
	    	p = 2;
	        } else {
	    	p = j - 1;
	        }
	        pt1[0] = tpnts[tris[p]].x;
	        pt1[1] = tpnts[tris[p]].y;
	        pt1[2] = 0.0;
	        pt2[0] = tpnts[tris[j]].x;
	        pt2[1] = tpnts[tris[j]].y;
	        pt2[2] = 0.0;
	        BV_ADD_VLIST(vlfree, vhead, pt1, BV_VLIST_LINE_MOVE);
	        BV_ADD_VLIST(vlfree, vhead, pt2, BV_VLIST_LINE_DRAW);
	    }
   	}, true);
    } else if (plottype == 4) {
	point_t pt = VINIT_ZERO;
	for (size_t i = 0; i < tpnts.size(); i++) {
	    pt[0] = tpnts[i].x;
	    pt[1] = tpnts[i].y;
	    pt[2] = 0.0;
	    BV_ADD_VLIST(vlfree, vhead, pt, BV_VLIST_POINT_DRAW);
	}
    }

    return;
}

int
brep_facecdt_plot(struct bu_vls *vls, const char *solid_name,
                      const struct bg_tess_tol *ttol, const struct bn_tol *tol,
                      const ON_Brep *brep, struct bu_list *p_vhead,
                      struct bv_vlblock *vbp, struct bu_list *vlfree,
		      int index, int plottype, int num_points)
{
    if (plottype == INT_MAX || num_points == INT_MAX)
	return -1;

    struct bu_list *vhead = p_vhead;
    if (!vhead) {
	vhead = bv_vlblock_find(vbp, YELLOW);
    }
    ON_wString wstr;
    ON_TextLog tl(wstr);

    if (brep == NULL) {
	// Nothing to draw
	return -1;
    }

    if (!brep->IsValid(&tl)) {
	//for now try to draw even if it's invalid, but report if the
	//user is listening
	if (vls) {
	    if (wstr.Length() > 0) {
		ON_String onstr = ON_String(wstr);
		const char *isvalidinfo = onstr.Array();
		bu_vls_strcat(vls, "brep (");
		bu_vls_strcat(vls, solid_name);
		bu_vls_strcat(vls, ") is NOT valid:");
		bu_vls_strcat(vls, isvalidinfo);
	    } else {
		bu_vls_strcat(vls, "brep (");
		bu_vls_strcat(vls, solid_name);
		bu_vls_strcat(vls, ") is NOT valid.");
	    }
	}
    }

    if (index == -1) {
        for (index = 0; index < brep->m_F.Count(); index++) {
            const ON_BrepFace& face = brep->m_F[index];
            detria_CDT(vhead, face, ttol, tol, vlfree, plottype, num_points);
        }
    } else if (index < brep->m_F.Count()) {
        const ON_BrepFaceArray& faces = brep->m_F;
        if (index < faces.Count()) {
            const ON_BrepFace& face = faces[index];
            face.Dump(tl);
            detria_CDT(vhead, face, ttol, tol, vlfree, plottype, num_points);
        }
    }

    if (vls) {
	bu_vls_printf(vls, "%s", ON_String(wstr).Array());
    }

    return 0;
}

static bool
bg_CDT(std::vector<int> &faces, std::vector<fastf_t> &pnt_norms, std::vector<fastf_t> &pnts,
	const ON_BrepFace &face,
	const struct bg_tess_tol *ttol,
	const struct bn_tol *tol)
{
    fast_face_scratch scratch;
    fast_line_store line_store;
    ON_RTree rt_trims;
    ON_2dPointArray on_surf_points;
    const ON_Surface *s = face.SurfaceOf();
    if (!s)
	return false;
    double surface_width, surface_height;
    int fi = face.m_face_index;

    fastf_t max_dist = 0.0;
    if (s->GetSurfaceSize(&surface_width, &surface_height)) {
	if ((surface_width < tol->dist) || (surface_height < tol->dist))
	    return false;
	max_dist = sqrt(surface_width * surface_width + surface_height * surface_height) / 10.0;
    }

    int loop_cnt = face.LoopCount();
    ON_2dPointArray on_loop_points;
    fast_loop_point_store loop_points(loop_cnt);
    ON_SimpleArray<BrepTrimPoint> **brep_loop_points = loop_points.points;

    // first simply load loop point samples
    for (int li = 0; li < loop_cnt; li++) {
	const ON_BrepLoop *loop = face.Loop(li);
	get_loop_sample_points(brep_loop_points[li], face, loop, max_dist,
		ttol, tol, scratch);
    }
    if (scratch.hit_sample_limit) {
	return false;
    }

    fast_bridge_store bridgePoints;
    if (s->IsClosed(0) || s->IsClosed(1))
	PerformClosedSurfaceChecks(s, face, ttol, tol, brep_loop_points,
	    BREP_SAME_POINT_TOLERANCE, bridgePoints);

    // process through loops building polygons.
    std::vector<detria::PointD> tpnts;
    std::vector<int> outer_polyline;
    std::vector<std::vector<int>> holes;
    std::unordered_map<int, BrepTrimPoint *> pointmap;
    std::unordered_map<int, size_t> pind_map;
    bool outer = true;

    for (int li = 0; li < loop_cnt; li++) {
	std::vector<int> polyline;
	int num_loop_points = brep_loop_points[li]->Count();
	if (num_loop_points <= 2)
	    continue;
	for (int i = 1; i < num_loop_points; i++) {
	    // map point to last entry to 3d point
	    detria::PointD npt;
	    npt.x = (*brep_loop_points[li])[i].p2d.x;
	    npt.y = (*brep_loop_points[li])[i].p2d.y;
	    tpnts.push_back(npt);
	    pointmap[tpnts.size()-1] = &((*brep_loop_points[li])[i]);
	    polyline.push_back(tpnts.size()-1);
	    pnts.push_back((*brep_loop_points[li])[i].p3d->x);
	    pnts.push_back((*brep_loop_points[li])[i].p3d->y);
	    pnts.push_back((*brep_loop_points[li])[i].p3d->z);
	    pind_map[tpnts.size()-1] = pnts.size()/3 - 1;
	}
	for (int i = 1; i < brep_loop_points[li]->Count(); i++) {
	    // Add the polylines to the tree so we can ensure no points from
	    // the surface sample end up on them
	    ON_Line *line = new ON_Line((*brep_loop_points[li])[i - 1].p2d, (*brep_loop_points[li])[i].p2d);
	    line_store.lines.push_back(line);
	    ON_BoundingBox bb = line->BoundingBox();

	    bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
	    bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
	    bb.m_max.z = bb.m_max.z + ON_ZERO_TOLERANCE;
	    bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
	    bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;
	    bb.m_min.z = bb.m_min.z - ON_ZERO_TOLERANCE;

	    rt_trims.Insert2d(bb.Min(), bb.Max(), line);
	}
	if (outer) {
	    outer_polyline = polyline;
	    outer = false;
	} else {
	    holes.push_back(polyline);
	}
    }

    if (outer) {
	std::cerr << "Error: Face(" << fi << ") cannot evaluate its outer loop and will not be facetized." << std::endl;
	return false;
    }

    getSurfacePoints(face, ttol, tol, on_surf_points);
    if (on_surf_points.Count() >= FAST_CDT_MAX_SURFACE_SAMPLES) {
	return false;
    }

    // Not all surface point samples may end up being used in the triangulation,
    // so they are not added to the point map at this stage
    for (int i = 0; i < on_surf_points.Count(); i++) {
	ON_SimpleArray<void*> results;
	const ON_2dPoint *p = on_surf_points.At(i);

	rt_trims.Search2d((const double *) p, (const double *) p, results);

	if (results.Count() > 0) {
	    bool on_edge = false;
	    for (int ri = 0; ri < results.Count(); ri++) {
		double dist;
		const ON_Line *l = (const ON_Line *) *results.At(ri);
		dist = l->MinimumDistanceTo(*p);
		if (NEAR_ZERO(dist, tol->dist)) {
		    on_edge = true;
		    break;
		}
	    }
	    if (!on_edge) {
		detria::PointD npt;
		npt.x = p->x;
		npt.y = p->y;
		tpnts.push_back(npt);
	    }
	} else {
	    detria::PointD npt;
	    npt.x = p->x;
	    npt.y = p->y;
	    tpnts.push_back(npt);
	}
    }

    ON_SimpleArray<void*> results;
    ON_BoundingBox bb = rt_trims.BoundingBox();

    rt_trims.Search2d((const double *) bb.m_min, (const double *) bb.m_max, results);

    rt_trims.RemoveAll();

    // Run the core triangulation routine
    detria::Triangulation<detria::PointD, int> tri;
    tri.setPoints(tpnts);
    tri.addOutline(outer_polyline);
    for (size_t i = 0; i < holes.size(); i++)
	tri.addHole(holes[i]);

    bool tri_success = false;
    {
	try {
	    tri_success = tri.triangulate(true);
	}
	catch (...) {
	    return false;
	}
    }

    if (!tri_success) {
	return false;
    }

    tri.forEachTriangle([&](const detria::Triangle<int> triangle)
    {
        int tris[3];
	tris[0] = triangle.x;
	tris[1] = triangle.y;
	tris[2] = triangle.z;
	for (size_t j = 0; j < 3; j++) {
	    ON_3dPoint pnt;
	    ON_3dVector norm(0.0, 0.0, 0.0);
	    if (surface_EvNormal(s, tpnts[tris[j]].x, tpnts[tris[j]].y, pnt, norm)) {
		// Vertex points are shared with other faces
		std::unordered_map<int, size_t>::const_iterator ii = pind_map.find(tris[j]);
		if (ii != pind_map.end()) {
		    faces.push_back(ii->second);
		} else {
		    pnts.push_back(pnt.x);
		    pnts.push_back(pnt.y);
		    pnts.push_back(pnt.z);
		    pind_map[tris[j]] = pnts.size()/3 - 1;
		    faces.push_back(pind_map[tris[j]]);
		}

		// Normals are NOT shared with other faces, so we store full
		// vectors rather than indices to points
		std::unordered_map<int, BrepTrimPoint *>::const_iterator bt_it = pointmap.find(tris[j]);
		if (bt_it != pointmap.end() && bt_it->second->n3d)
		    norm = *(bt_it->second->n3d);
		if (face.m_bRev)
		    norm = norm * -1.0;
		pnt_norms.push_back(norm.x);
		pnt_norms.push_back(norm.y);
		pnt_norms.push_back(norm.z);
	    }
	}
    }, true);

    return !faces.empty() && faces.size() % 3 == 0 &&
	pnts.size() % 3 == 0 && pnt_norms.size() == faces.size() * 3;
}


struct fast_cdt_face_result {
    std::vector<int> faces;
    std::vector<fastf_t> norms;
    std::vector<fastf_t> pnts;
    bool completed = false;
    bool failed = false;
};

struct fast_cdt_parallel_state {
    const ON_Brep *brep;
    const struct bg_tess_tol *ttol;
    const struct bn_tol *tol;
    std::vector<fast_cdt_face_result> *results;
    std::atomic<int> next_face;
    std::atomic<size_t> result_bytes;
    std::atomic<size_t> result_points;
    std::atomic<bool> stop;
    std::atomic<bool> hit_time_limit;
    std::atomic<bool> hit_memory_limit;
    std::atomic<bool> hit_point_limit;
    size_t max_result_bytes;
    size_t max_points;
    int64_t deadline;
    bool allow_partial;
};

static void
fast_cdt_face_worker(int UNUSED(cpu), void *data)
{
    fast_cdt_parallel_state *state = (fast_cdt_parallel_state *)data;
    const int face_cnt = state->brep->m_F.Count();

    for (;;) {
	if (state->stop.load())
	    return;
	if (state->deadline > 0 && bu_gettime() >= state->deadline) {
	    state->hit_time_limit = true;
	    state->stop = true;
	    return;
	}
	const int face_index = state->next_face.fetch_add(1);
	if (face_index >= face_cnt)
	    return;

	const ON_BrepFace& face = state->brep->m_F[face_index];
	fast_cdt_face_result &result = (*state->results)[face_index];
	bool success = false;
	try {
	    success = bg_CDT(result.faces, result.norms, result.pnts, face,
		state->ttol, state->tol);
	} catch (const std::bad_alloc &) {
	    state->hit_memory_limit = true;
	    state->stop = true;
	} catch (...) {
	    success = false;
	}
	if (!success) {
	    result.failed = true;
	    result.faces.clear();
	    result.norms.clear();
	    result.pnts.clear();
	    if (!state->allow_partial)
		state->stop = true;
	    continue;
	}

	const size_t result_bytes = result.faces.size() * sizeof(int) +
	    (result.norms.size() + result.pnts.size()) * sizeof(fastf_t);
	const size_t result_points = result.pnts.size() / 3;
	if (state->result_bytes.fetch_add(result_bytes) + result_bytes >
		state->max_result_bytes) {
	    state->result_bytes.fetch_sub(result_bytes);
	    state->hit_memory_limit = true;
	    state->stop = true;
	    result.faces.clear();
	    result.norms.clear();
	    result.pnts.clear();
	    result.failed = true;
	    return;
	}
	if (state->result_points.fetch_add(result_points) + result_points >
		state->max_points) {
	    state->result_points.fetch_sub(result_points);
	    state->result_bytes.fetch_sub(result_bytes);
	    state->hit_point_limit = true;
	    state->stop = true;
	    result.faces.clear();
	    result.norms.clear();
	    result.pnts.clear();
	    result.failed = true;
	    return;
	}
	result.completed = true;
    }
}

void
brep_cdt_fast_options_default(struct brep_cdt_fast_options *options)
{
    if (!options)
	return;
    options->max_workers = std::min((size_t)8, bu_avail_cpus());
    if (!options->max_workers)
	options->max_workers = 1;
    options->max_result_bytes = (size_t)512 * 1024 * 1024;
    options->max_points = (size_t)16 * 1024 * 1024;
    options->max_time_ms = 0;
    options->allow_partial = 1;
}

int
brep_cdt_fast_ex(int **faces, int *face_cnt, vect_t **pnt_norms,
	point_t **pnts, int *pntcnt, const ON_Brep *brep, int index,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol,
	const struct brep_cdt_fast_options *user_options,
	struct brep_cdt_fast_report *report)
{
    if (!faces || !face_cnt || !pnt_norms || !pnts || !pntcnt)
	return BREP_CDT_FAST_ERROR;

    *faces = NULL;
    *face_cnt = 0;
    *pnt_norms = NULL;
    *pnts = NULL;
    *pntcnt = 0;
    if (report)
	memset(report, 0, sizeof(*report));

    if (!brep || !ttol || !tol)
	return BREP_CDT_FAST_ERROR;

    const int brep_face_count = brep->m_F.Count();
    if (brep_face_count <= 0 || index < -1 || index >= brep_face_count)
	return BREP_CDT_FAST_ERROR;

    struct brep_cdt_fast_options options;
    brep_cdt_fast_options_default(&options);
    if (user_options) {
	if (user_options->max_workers)
	    options.max_workers = user_options->max_workers;
	if (user_options->max_result_bytes)
	    options.max_result_bytes = user_options->max_result_bytes;
	if (user_options->max_points)
	    options.max_points = user_options->max_points;
	options.max_time_ms = user_options->max_time_ms;
	options.allow_partial = user_options->allow_partial;
    }
    options.max_workers = std::max((size_t)1,
	std::min(options.max_workers, (size_t)brep_face_count));

    ssize_t available = bu_mem(BU_MEM_AVAIL, NULL);
    if (available > 0)
	options.max_result_bytes = std::min(options.max_result_bytes,
	    (size_t)available / 4);

    std::vector<int> all_faces;
    std::vector<fastf_t> all_norms;
    std::vector<fastf_t> all_pnts;

    const int first_face = (index == -1) ? 0 : index;
    const int end_face = (index == -1) ? brep_face_count : index + 1;
    std::vector<fast_cdt_face_result> face_results((size_t)brep_face_count);
    const int64_t deadline = options.max_time_ms > 0 ?
	bu_gettime() + (int64_t)options.max_time_ms * 1000 : 0;
    bool hit_time_limit = false;
    bool hit_memory_limit = false;
    bool hit_point_limit = false;

    if (index == -1) {
	/* Each face produces an independent set of indices and points.  Keep
	 * those results separate while workers run, then concatenate them in
	 * face order.  This avoids locking the hot CDT path, keeps output
	 * stable across runs, and leaves the input BRep unchanged. */
	fast_cdt_parallel_state state = {
	    brep, ttol, tol, &face_results, 0, 0, 0, false, false,
	    false, false, options.max_result_bytes, options.max_points,
	    deadline, (bool)options.allow_partial
	};
	bu_parallel(fast_cdt_face_worker, options.max_workers, &state);
	hit_time_limit = state.hit_time_limit.load();
	hit_memory_limit = state.hit_memory_limit.load();
	hit_point_limit = state.hit_point_limit.load();
    } else {
	fast_cdt_face_result &result = face_results[(size_t)index];
	if (deadline > 0 && bu_gettime() >= deadline) {
	    hit_time_limit = true;
	    result.failed = true;
	} else {
	    bool success = false;
	    try {
		success = bg_CDT(result.faces, result.norms, result.pnts,
		    brep->m_F[index], ttol, tol);
	    } catch (const std::bad_alloc &) {
		hit_memory_limit = true;
	    } catch (...) {
		success = false;
	    }
	    if (!success) {
		result.failed = true;
	    } else {
		const size_t bytes = result.faces.size() * sizeof(int) +
		    (result.norms.size() + result.pnts.size()) * sizeof(fastf_t);
		if (bytes > options.max_result_bytes) {
		    hit_memory_limit = true;
		    result.failed = true;
		} else if (result.pnts.size() / 3 > options.max_points) {
		    hit_point_limit = true;
		    result.failed = true;
		} else {
		    result.completed = true;
		}
	    }
	}
	if (result.failed) {
	    result.faces.clear();
	    result.norms.clear();
	    result.pnts.clear();
	}
    }

    int completed_faces = 0;
    int failed_faces = 0;
    for (int fi = first_face; fi < end_face; fi++) {
	fast_cdt_face_result &result = face_results[(size_t)fi];
	if (!result.completed) {
	    failed_faces++;
	    continue;
	}
	completed_faces++;
	const size_t point_offset = all_pnts.size() / 3;
	for (size_t i = 0; i < result.faces.size(); i++)
	    all_faces.push_back((int)point_offset + result.faces[i]);
	all_norms.insert(all_norms.end(), result.norms.begin(),
	    result.norms.end());
	all_pnts.insert(all_pnts.end(), result.pnts.begin(), result.pnts.end());
    }

    if (report) {
	report->requested_faces = end_face - first_face;
	report->completed_faces = completed_faces;
	report->failed_faces = failed_faces;
	report->result_bytes = all_faces.size() * sizeof(int) +
	    (all_norms.size() + all_pnts.size()) * sizeof(fastf_t);
	report->hit_time_limit = hit_time_limit;
	report->hit_memory_limit = hit_memory_limit;
	report->hit_point_limit = hit_point_limit;
    }

    const bool hit_limit = hit_time_limit || hit_memory_limit ||
	hit_point_limit;
    if (failed_faces && (!options.allow_partial || !completed_faces))
	return hit_limit ? BREP_CDT_FAST_LIMIT : BREP_CDT_FAST_ERROR;

    /* If we got nothing, we're done here */
    if (!all_faces.size() || !all_pnts.size() || !all_norms.size())
	return BREP_CDT_FAST_ERROR;

    if (all_faces.size() / 3 > INT_MAX || all_pnts.size() / 3 > INT_MAX)
	return BREP_CDT_FAST_LIMIT;

    (*face_cnt) = (int)all_faces.size()/3;
    (*pntcnt) = (int)all_pnts.size()/3;
    (*faces) = (int *)bu_calloc(all_faces.size(), sizeof(int), "faces");
    (*pnt_norms) = (vect_t *)bu_calloc(all_norms.size(), sizeof(fastf_t), "normals");
    (*pnts) = (point_t *)bu_calloc(all_pnts.size()/3, sizeof(point_t), "pnts");
    for(size_t i = 0; i < all_faces.size(); i++)
	(*faces)[i] = all_faces[i];
    for(size_t i = 0; i < all_norms.size()/3; i++) {
	(*pnt_norms)[i][X] = all_norms[3*i+0];
	(*pnt_norms)[i][Y] = all_norms[3*i+1];
	(*pnt_norms)[i][Z] = all_norms[3*i+2];
    }
    for(size_t i = 0; i < all_pnts.size()/3; i++) {
	(*pnts)[i][X] = all_pnts[3*i+0];
	(*pnts)[i][Y] = all_pnts[3*i+1];
	(*pnts)[i][Z] = all_pnts[3*i+2];
    }
    if (hit_limit)
	return BREP_CDT_FAST_LIMIT;
    return failed_faces ? BREP_CDT_FAST_PARTIAL : BREP_CDT_FAST_OK;
}

int
brep_cdt_fast(int **faces, int *face_cnt, vect_t **pnt_norms,
	point_t **pnts, int *pntcnt, const ON_Brep *brep, int index,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol)
{
    struct brep_cdt_fast_options options;
    struct brep_cdt_fast_report report;
    brep_cdt_fast_options_default(&options);
    options.allow_partial = 1;
    int ret = brep_cdt_fast_ex(faces, face_cnt, pnt_norms, pnts, pntcnt,
	brep, index, ttol, tol, &options, &report);
    return (ret == BREP_CDT_FAST_OK || ret == BREP_CDT_FAST_PARTIAL ||
	(ret == BREP_CDT_FAST_LIMIT && *face_cnt > 0)) ?
	BRLCAD_OK : BRLCAD_ERROR;
}

/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
