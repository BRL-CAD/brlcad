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
#include <limits>
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
#include "bg/polygon.h"
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

struct fast_surface_metrics {
    double width = 0.0;
    double height = 0.0;
    double u_scale = 0.0;
    double v_scale = 0.0;
    bool size_valid = false;
    bool sampling_valid = false;
};

static void
fast_surface_metrics_get(const ON_Surface *surface,
	fast_surface_metrics *metrics)
{
    if (!surface || !metrics)
	return;
    metrics->size_valid = surface->GetSurfaceSize(&metrics->width,
	&metrics->height);
    if (!metrics->size_valid)
	return;

    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    if (!udom.IsIncreasing() || !vdom.IsIncreasing())
	return;
    metrics->u_scale = metrics->width / udom.Length();
    metrics->v_scale = metrics->height / vdom.Length();
    metrics->sampling_valid = true;
}

static double
fast_brep_diagonal(const ON_Brep *brep)
{
    ON_BoundingBox bbox;
    return (brep && brep->GetTightBoundingBox(bbox)) ?
	bbox.Diagonal().Length() : 0.0;
}

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

    const ON_Surface *s = trim.Face() ?
	static_cast<const ON_Surface *>(trim.Face()) : trim.SurfaceOf();
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

static bool
fast_add_trim_sample(const ON_BrepTrim &trim, double t,
	fast_trim_point_map &param_points)
{
    if (!std::isfinite(t) || param_points.find(t) != param_points.end())
	return false;

    const ON_Surface *surface = trim.Face() ?
	static_cast<const ON_Surface *>(trim.Face()) : trim.SurfaceOf();
    if (!surface)
	return false;

    ON_3dPoint uv = trim.PointAt(t);
    ON_3dPoint point = ON_3dPoint::UnsetPoint;
    ON_3dVector tangent = ON_3dVector::UnsetVector;
    ON_3dVector normal = ON_3dVector::UnsetVector;
    if (!uv.IsValid() || !surface_EvNormal(surface, uv.x, uv.y, point,
	    normal))
	return false;
    trim.EvTangent(t, uv, tangent);

    BrepTrimPoint *sample = new BrepTrimPoint();
    sample->p3d = new ON_3dPoint(point);
    sample->n3d = NULL;
    sample->p2d = uv;
    sample->normal = normal;
    sample->tangent = tangent;
    sample->t = t;
    sample->e = ON_UNSET_VALUE;
    sample->trim_ind = trim.m_trim_index;
    sample->edge_ind = trim.m_ei;
    param_points[t] = sample;
    return true;
}

static void
fast_ensure_topology_samples(const ON_BrepTrim &trim,
	fast_trim_point_map &param_points)
{
    if (trim.Degree() <= 1 || param_points.size() >= 5)
	return;

    const ON_Interval domain = trim.Domain();
    if (!domain.IsIncreasing())
	return;

    /* Flatness is a geometric criterion, but a loop also needs enough
     * topological vertices to describe its boundary.  Two nearly coincident
     * nonlinear trims form a legitimate thin lens even when both curves are
     * individually indistinguishable from their chords at display tolerance.
     * Likewise, a single closed nonlinear trim needs three distinct vertices
     * after its duplicate closure point is removed. */
    if (trim.IsClosed()) {
	fast_add_trim_sample(trim, domain.ParameterAt(0.25), param_points);
	fast_add_trim_sample(trim, domain.ParameterAt(0.50), param_points);
	fast_add_trim_sample(trim, domain.ParameterAt(0.75), param_points);
    } else if (param_points.size() < 3) {
	fast_add_trim_sample(trim, domain.Mid(), param_points);
    }
}

static void
fast_free_pullback(PBCData *data)
{
    if (!data)
	return;
    if (data->segments) {
	while (!data->segments->empty()) {
	    delete data->segments->front();
	    data->segments->pop_front();
	}
	delete data->segments;
    }
    delete data;
}

static bool
fast_pullback_candidate(const ON_BrepLoop *loop, const ON_BrepTrim &trim,
	const struct bn_tol *tol, double model_diagonal)
{
    if (!loop || trim.m_type == ON_BrepTrim::singular || !trim.Edge() ||
	    !trim.Edge()->EdgeCurveOf() || !std::isfinite(trim.m_tolerance[0]) ||
	    trim.m_tolerance[0] <= 0.0)
	return false;

    std::vector<double> tolerances;
    tolerances.reserve((size_t)loop->TrimCount());
    for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	const ON_BrepTrim *candidate = loop->Trim(ti);
	if (candidate && std::isfinite(candidate->m_tolerance[0]) &&
		candidate->m_tolerance[0] > 0.0)
	    tolerances.push_back(candidate->m_tolerance[0]);
    }
    if (tolerances.size() < 2)
	return false;
    std::sort(tolerances.begin(), tolerances.end());
    const double median = tolerances[tolerances.size() / 2];
    const double scale_floor = std::max(tol->dist,
	std::max(model_diagonal, 1.0) * 1.0e-10);
    const double baseline = std::max(median, scale_floor);
    return trim.m_tolerance[0] > 8.0 * baseline;
}

static bool
fast_append_pullback_samples(ON_SimpleArray<BrepTrimPoint> *points,
	const ON_BrepTrim &trim, bool omit_last, const struct bn_tol *tol,
	double model_diagonal, fast_face_scratch &scratch)
{
    const ON_Surface *surface = trim.Face() ?
	static_cast<const ON_Surface *>(trim.Face()) : trim.SurfaceOf();
    const ON_BrepEdge *edge = trim.Edge();
    const ON_Curve *edge_curve = edge ? edge->EdgeCurveOf() : NULL;
    if (!points || !surface || !edge_curve)
	return false;

    const double distance_cap = std::max(16.0 * tol->dist,
	std::max(model_diagonal, 1.0) * 1.0e-4);
    const double repair_tolerance = std::min(trim.m_tolerance[0],
	distance_cap);
    if (!(repair_tolerance > 0.0) || !std::isfinite(repair_tolerance))
	return false;

    PBCData *data = pullback_samples(surface, edge_curve, repair_tolerance,
	1.0e-3, BREP_SAME_POINT_TOLERANCE, repair_tolerance);
    if (!data || !data->samples_source_validated || !data->segments ||
	    data->segments->empty() || data->rejected_projection_samples) {
	fast_free_pullback(data);
	return false;
    }

    std::vector<ON_2dPoint> samples;
    for (const ON_2dPointArray *segment : *data->segments) {
	if (!segment)
	    continue;
	for (int i = 0; i < segment->Count(); ++i) {
	    const ON_2dPoint sample = (*segment)[i];
	    if (!sample.IsValid()) {
		fast_free_pullback(data);
		return false;
	    }
	    if (samples.empty() || !V2NEAR_EQUAL(samples.back(), sample,
		    BREP_SAME_POINT_TOLERANCE))
		samples.push_back(sample);
	}
    }
    fast_free_pullback(data);
    if (samples.size() < 2 || samples.size() > FAST_CDT_MAX_TRIM_SAMPLES)
	return false;

    const ON_Interval trim_domain = trim.Domain();
    const ON_2dPoint trim_start = trim.PointAt(trim_domain.Min());
    const ON_2dPoint trim_end = trim.PointAt(trim_domain.Max());
    const ON_2dPoint source_start_uv = UnwrapUVPoint(surface, trim_start,
	BREP_SAME_POINT_TOLERANCE);
    const ON_2dPoint source_end_uv = UnwrapUVPoint(surface, trim_end,
	BREP_SAME_POINT_TOLERANCE);
    const ON_2dPoint pullback_start_uv = UnwrapUVPoint(surface,
	samples.front(), BREP_SAME_POINT_TOLERANCE);
    const ON_2dPoint pullback_end_uv = UnwrapUVPoint(surface,
	samples.back(), BREP_SAME_POINT_TOLERANCE);
    const ON_3dPoint source_start = surface->PointAt(source_start_uv.x,
	source_start_uv.y);
    const ON_3dPoint source_end = surface->PointAt(source_end_uv.x,
	source_end_uv.y);
    const ON_3dPoint pullback_start = surface->PointAt(pullback_start_uv.x,
	pullback_start_uv.y);
    const ON_3dPoint pullback_end = surface->PointAt(pullback_end_uv.x,
	pullback_end_uv.y);
    const double forward_error = source_start.DistanceTo(pullback_start) +
	source_end.DistanceTo(pullback_end);
    const double reverse_error = source_start.DistanceTo(pullback_end) +
	source_end.DistanceTo(pullback_start);
    if (reverse_error < forward_error)
	std::reverse(samples.begin(), samples.end());
    if (std::min(forward_error, reverse_error) > 2.0 * repair_tolerance)
	return false;

    const size_t append_count = samples.size() - (omit_last ? 1 : 0);
    std::vector<ON_3dPoint> lifted_points;
    std::vector<ON_3dVector> lifted_normals;
    lifted_points.reserve(append_count);
    lifted_normals.reserve(append_count);
    for (size_t i = 0; i < append_count; ++i) {
	const ON_2dPoint uv = UnwrapUVPoint(surface, samples[i],
	    BREP_SAME_POINT_TOLERANCE);
	ON_3dPoint point = ON_3dPoint::UnsetPoint;
	ON_3dVector normal = ON_3dVector::UnsetVector;
	if (!surface_EvNormal(surface, uv.x, uv.y, point,
		normal))
	    return false;
	lifted_points.push_back(point);
	lifted_normals.push_back(normal);
    }
    for (size_t i = 0; i < append_count; ++i) {
	BrepTrimPoint sample = {};
	sample.p3d = scratch.make_point(lifted_points[i]);
	sample.n3d = NULL;
	sample.p2d = samples[i];
	sample.normal = lifted_normals[i];
	sample.tangent = ON_3dVector::UnsetVector;
	sample.t = ON_UNSET_VALUE;
	sample.e = ON_UNSET_VALUE;
	sample.trim_ind = trim.m_trim_index;
	sample.edge_ind = trim.m_ei;
	points->Append(sample);
    }
    return append_count > 0;
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

    const ON_Surface *s = trim.Face() ?
	static_cast<const ON_Surface *>(trim.Face()) : trim.SurfaceOf();

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

    fast_ensure_topology_samples(trim, *param_points);

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
		 const fast_surface_metrics &metrics,
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

    ON_2dPoint p2d(0.0, 0.0);
    ON_3dPoint p[4] = {ON_3dPoint(), ON_3dPoint(), ON_3dPoint(), ON_3dPoint()};
    ON_3dVector norm[4] = {ON_3dVector(), ON_3dVector(), ON_3dVector(), ON_3dVector()};
    ON_3dPoint mid(0.0, 0.0, 0.0);
    ON_3dVector norm_mid(0.0, 0.0, 0.0);
    fastf_t u = (u1 + u2) / 2.0;
    fastf_t v = (v1 + v2) / 2.0;
    fastf_t udist = u2 - u1;
    fastf_t vdist = v2 - v1;
    const fastf_t u_metric_dist = udist * metrics.u_scale;
    const fastf_t v_metric_dist = vdist * metrics.v_scale;

    if ((u_metric_dist < min_dist + ON_ZERO_TOLERANCE)
	|| (v_metric_dist < min_dist + ON_ZERO_TOLERANCE)) {
	return;
    }

    /* Do not split a parameter rectangle merely to make its cells square.
     * A geometrically flat, narrow strip may have an arbitrarily large
     * aspect ratio, and filling it with isotropic cells is unrelated to the
     * requested distance or normal tolerances.  The tests below subdivide
     * only when surface evaluation demonstrates geometric error. */
    if ((surface_EvNormal(s, u1, v1, p[0], norm[0]))
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
			     cos_within_ang, metrics, on_surf_points, left, below);
	    getSurfacePoints(s, u1, u, v, v2, min_dist, within_dist,
			     cos_within_ang, metrics, on_surf_points, left, false);
	    getSurfacePoints(s, u, u2, v1, v, min_dist, within_dist,
			     cos_within_ang, metrics, on_surf_points, false, below);
	    getSurfacePoints(s, u, u2, v, v2, min_dist, within_dist,
			     cos_within_ang, metrics, on_surf_points, false, false);
	} else if (udot < cos_within_ang - ON_ZERO_TOLERANCE) {
	    if (below) {
		p2d.Set(u, v1);
		on_surf_points.Append(p2d);
	    }
	    //top
	    p2d.Set(u, v2);
	    on_surf_points.Append(p2d);
	    getSurfacePoints(s, u1, u, v1, v2, min_dist, within_dist,
			     cos_within_ang, metrics, on_surf_points, left, below);
	    getSurfacePoints(s, u, u2, v1, v2, min_dist, within_dist,
			     cos_within_ang, metrics, on_surf_points, false, below);
	} else if (vdot < cos_within_ang - ON_ZERO_TOLERANCE) {
	    if (left) {
		p2d.Set(u1, v);
		on_surf_points.Append(p2d);
	    }
	    //right
	    p2d.Set(u2, v);
	    on_surf_points.Append(p2d);

	    getSurfacePoints(s, u1, u2, v1, v, min_dist, within_dist,
			     cos_within_ang, metrics, on_surf_points, left, below);
	    getSurfacePoints(s, u1, u2, v, v2, min_dist, within_dist,
			     cos_within_ang, metrics, on_surf_points, left, false);
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
				 cos_within_ang, metrics, on_surf_points, left, below);
		getSurfacePoints(s, u1, u, v, v2, min_dist, within_dist,
				 cos_within_ang, metrics, on_surf_points, left, false);
		getSurfacePoints(s, u, u2, v1, v, min_dist, within_dist,
				 cos_within_ang, metrics, on_surf_points, false, below);
		getSurfacePoints(s, u, u2, v, v2, min_dist, within_dist,
				 cos_within_ang, metrics, on_surf_points, false, false);
	    }
	}
    }
}


static void
getSurfacePoints(const ON_BrepFace &face,
		 const struct bg_tess_tol *ttol,
		 const struct bn_tol *tol,
		 ON_2dPointArray &on_surf_points,
		 const fast_surface_metrics &metrics,
		 double model_diagonal)
{
    const ON_Surface *s = &face;

    if (s && metrics.sampling_valid) {
	double dist = model_diagonal;
	double min_dist = 0.0;
	double within_dist = 0.0;
	double  cos_within_ang = 0.0;

	if ((metrics.width < tol->dist) || (metrics.height < tol->dist)) {
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
			     cos_within_ang, metrics, on_surf_points, true, true);

	    //bottom midx
	    p.Set(midx, min.y);
	    on_surf_points.Append(p);

	    //midx midy
	    p.Set(midx, midy);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, midx, max.x, min.y, midy, min_dist, within_dist,
			     cos_within_ang, metrics, on_surf_points, false, true);

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
			     cos_within_ang, metrics, on_surf_points, true, false);

	    //top midx
	    p.Set(midx, max.y);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, midx, max.x, midy, max.y, min_dist, within_dist,
			     cos_within_ang, metrics, on_surf_points, false, false);

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
			     within_dist, cos_within_ang, metrics, on_surf_points, true, true);

	    //bottom midx
	    p.Set(midx, min.y);
	    on_surf_points.Append(p);

	    //top midx
	    p.Set(midx, max.y);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, midx, max.x, min.y, max.y, min_dist,
			     within_dist, cos_within_ang, metrics, on_surf_points, false, true);

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
			     within_dist, cos_within_ang, metrics, on_surf_points, true, true);

	    //bottom right
	    p.Set(max.x, min.y);
	    on_surf_points.Append(p);

	    //right midy
	    p.Set(max.x, midy);
	    on_surf_points.Append(p);

	    getSurfacePoints(s, min.x, max.x, midy, max.y, min_dist,
			     within_dist, cos_within_ang, metrics, on_surf_points, true, false);

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
			     within_dist, cos_within_ang, metrics, on_surf_points, true, true);

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
	fast_face_scratch &scratch,
	double model_diagonal,
	bool repair_pcurves)
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

	if (repair_pcurves && fast_pullback_candidate(loop, *trim, tol,
		model_diagonal) && fast_append_pullback_samples(points, *trim,
		lti < trim_count - 1, tol, model_diagonal, scratch))
	    continue;

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

static bool
fast_reconstruct_planar_domain_loop(const ON_Surface *surface,
	const ON_BrepFace &face, ON_SimpleArray<BrepTrimPoint> &points,
	fast_face_scratch &scratch)
{
    const ON_Brep *brep = face.Brep();
    const ON_BrepLoop *loop = face.LoopCount() == 1 ? face.Loop(0) : NULL;
    if (!surface || !brep || !loop || loop->TrimCount() != 0 ||
	    brep->m_F.Count() != 1 || points.Count() != 0 ||
	    !surface->IsPlanar(NULL, BREP_PLANAR_TOL))
	return false;
    const ON_Interval u = surface->Domain(0);
    const ON_Interval v = surface->Domain(1);
    if (!u.IsIncreasing() || !v.IsIncreasing())
	return false;
    const ON_2dPoint corners[5] = {
	ON_2dPoint(u.Min(), v.Min()), ON_2dPoint(u.Max(), v.Min()),
	ON_2dPoint(u.Max(), v.Max()), ON_2dPoint(u.Min(), v.Max()),
	ON_2dPoint(u.Min(), v.Min())
    };
    ON_SimpleArray<BrepTrimPoint> replacement;
    for (int i = 0; i < 5; ++i) {
	ON_3dPoint point = ON_3dPoint::UnsetPoint;
	ON_3dVector normal = ON_3dVector::UnsetVector;
	if (!surface_EvNormal(surface, corners[i].x, corners[i].y,
		point, normal))
	    return false;
	BrepTrimPoint sample = {};
	sample.p3d = scratch.make_point(point);
	sample.n3d = NULL;
	sample.p2d = corners[i];
	sample.normal = normal;
	sample.tangent = ON_3dVector::UnsetVector;
	sample.t = ON_UNSET_VALUE;
	sample.e = ON_UNSET_VALUE;
	sample.trim_ind = -1;
	sample.edge_ind = -1;
	sample.from_singular = -1;
	replacement.Append(sample);
    }
    points = replacement;
    return true;
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


static bool
TrimLoopToSinglePeriod(const ON_Surface *s,
	ON_SimpleArray<BrepTrimPoint> &points, double tolerance)
{
    const int count = points.Count();
    if (count < 4 || V2NEAR_EQUAL(points[0].p2d,
	    points[count - 1].p2d, tolerance))
	return false;

    int best_begin = -1;
    int best_end = -1;
    long double best_area = 0.0L;
    for (int begin = 0; begin < count - 2; begin++) {
	for (int end = begin + 2; end < count; end++) {
	    bool shifted = false;
	    bool equivalent = true;
	    for (int dir = 0; dir < 2; dir++) {
		const double delta = points[end].p2d[dir] -
		    points[begin].p2d[dir];
		if (!s->IsClosed(dir)) {
		    if (fabs(delta) > tolerance)
			equivalent = false;
		    continue;
		}
		const double period = s->Domain(dir).Length();
		if (!(period > ON_ZERO_TOLERANCE) || !std::isfinite(period)) {
		    equivalent = false;
		    continue;
		}
		const long turns = std::lround(delta / period);
		if (labs(turns) > 1 ||
			fabs(delta - turns * period) > tolerance) {
		    equivalent = false;
		    continue;
		}
		shifted = shifted || turns != 0;
	    }
	    if (!equivalent || !shifted)
		continue;

	    long double twice_area = 0.0L;
	    for (int i = begin; i < end; i++) {
		twice_area += (long double)points[i].p2d.x *
		    points[i + 1].p2d.y -
		    (long double)points[i + 1].p2d.x *
		    points[i].p2d.y;
	    }
	    twice_area += (long double)points[end].p2d.x *
		points[begin].p2d.y -
		(long double)points[begin].p2d.x * points[end].p2d.y;
	    const long double area = std::fabs(twice_area);
	    if (area > best_area) {
		best_area = area;
		best_begin = begin;
		best_end = end;
	    }
	}
    }
    if (best_begin < 0 || !(best_area > tolerance * tolerance))
	return false;

    ON_SimpleArray<BrepTrimPoint> trimmed;
    trimmed.Append(best_end - best_begin + 1, &points[best_begin]);
    points.Empty();
    points.Append(trimmed.Count(), trimmed.Array());

    for (int dir = 0; dir < 2; dir++) {
	if (!s->IsClosed(dir))
	    continue;
	const ON_Interval domain = s->Domain(dir);
	const double period = domain.Length();
	double min_coord = INFINITY;
	double max_coord = -INFINITY;
	for (int i = 0; i < points.Count(); i++) {
	    min_coord = std::min(min_coord, points[i].p2d[dir]);
	    max_coord = std::max(max_coord, points[i].p2d[dir]);
	}
	const double center = 0.5 * (min_coord + max_coord);
	const double shift = std::round((domain.Mid() - center) / period) *
	    period;
	for (int i = 0; i < points.Count(); i++)
	    points[i].p2d[dir] += shift;
    }
    return true;
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
	    if (TrimLoopToSinglePeriod(s, *brep_loop_points[li],
		    same_point_tolerance))
		continue;
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
	    bool periodic_closed = true;
	    bool crosses_period = false;
	    for (int dir = 0; dir < 2; dir++) {
		const double delta = brep_loop_end[dir] -
		    brep_loop_begin[dir];
		if (!s->IsClosed(dir)) {
		    periodic_closed = periodic_closed &&
			fabs(delta) <= same_point_tolerance;
		    continue;
		}
		const double period = s->Domain(dir).Length();
		if (!(period > ON_ZERO_TOLERANCE) || !std::isfinite(period)) {
		    periodic_closed = false;
		    continue;
		}
		const long turns = std::lround(delta / period);
		periodic_closed = periodic_closed && labs(turns) <= 1 &&
		    fabs(delta - turns * period) <= same_point_tolerance;
		crosses_period = crosses_period || turns != 0;
	    }
	    if (periodic_closed && crosses_period)
		continue;
	    ON_2dPoint begin_uv = UnwrapUVPoint(s, brep_loop_begin,
		same_point_tolerance);
	    ON_2dPoint end_uv = UnwrapUVPoint(s, brep_loop_end,
		same_point_tolerance);
	    ON_3dPoint brep_loop_begin3d = s->PointAt(begin_uv.x, begin_uv.y);
	    ON_3dPoint brep_loop_end3d = s->PointAt(end_uv.x, end_uv.y);

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

static bool
fast_append_synthetic_uv(ON_SimpleArray<BrepTrimPoint> &points,
	const ON_Surface *surface, const ON_2dPoint &uv, int singular_side,
	fast_face_scratch &scratch)
{
    const ON_2dPoint evaluation_uv = UnwrapUVPoint(surface, uv,
	BREP_SAME_POINT_TOLERANCE);
    ON_3dPoint point = ON_3dPoint::UnsetPoint;
    ON_3dVector normal = ON_3dVector::UnsetVector;
    if (!surface_EvNormal(surface, evaluation_uv.x, evaluation_uv.y, point,
	    normal)) {
	if (singular_side < 0)
	    return false;
	point = surface->PointAt(evaluation_uv.x, evaluation_uv.y);
	if (!point.IsValid())
	    return false;
	ON_2dPoint inward_uv = evaluation_uv;
	const int inward_dir = (singular_side == 0 || singular_side == 2) ?
	    1 : 0;
	const ON_Interval inward_domain = surface->Domain(inward_dir);
	const double inset = std::max(ON_ZERO_TOLERANCE,
	    inward_domain.Length() * 1.0e-6);
	if (singular_side == 0 || singular_side == 3)
	    inward_uv[inward_dir] = inward_domain.Min() + inset;
	else
	    inward_uv[inward_dir] = inward_domain.Max() - inset;
	ON_3dPoint inward_point = ON_3dPoint::UnsetPoint;
	if (!surface_EvNormal(surface, inward_uv.x, inward_uv.y,
		inward_point, normal))
	    return false;
    }

    BrepTrimPoint sample = {};
    sample.p3d = scratch.make_point(point);
    sample.n3d = NULL;
    sample.p2d = uv;
    sample.normal = normal;
    sample.tangent = ON_3dVector::UnsetVector;
    sample.t = ON_UNSET_VALUE;
    sample.e = ON_UNSET_VALUE;
    sample.trim_ind = -1;
    sample.edge_ind = -1;
    sample.from_singular = singular_side;
    points.Append(sample);
    return true;
}

static bool
fast_reconstruct_full_periodic_face(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double tolerance, fast_face_scratch &scratch, int *closed_direction,
	int *outer_loop_index)
{
    if (closed_direction)
	*closed_direction = -1;
    if (outer_loop_index)
	*outer_loop_index = -1;
    if (!surface || !brep_loop_points)
	return false;

    int outer_index = -1;
    for (int li = 0; li < face.LoopCount(); ++li) {
	const ON_BrepLoop *candidate = face.Loop(li);
	if (!candidate || candidate->m_type != ON_BrepLoop::outer)
	    continue;
	if (outer_index >= 0)
	    return false;
	outer_index = li;
    }
    if (outer_index < 0 || !brep_loop_points[outer_index] ||
	    brep_loop_points[outer_index]->Count() < 3)
	return false;
    const ON_BrepLoop *outer_loop = face.Loop(outer_index);
    if (!outer_loop || outer_loop->TrimCount() != 2)
	return false;
    const ON_BrepTrim *first_trim = outer_loop->Trim(0);
    const ON_BrepTrim *second_trim = outer_loop->Trim(1);
    if (!first_trim || !second_trim ||
	    first_trim->m_type != ON_BrepTrim::seam ||
	    second_trim->m_type != ON_BrepTrim::seam ||
	    !first_trim->Edge() || first_trim->Edge() != second_trim->Edge())
	return false;

    ON_SimpleArray<BrepTrimPoint> &points =
	*brep_loop_points[outer_index];
    for (int closed_dir = 0; closed_dir < 2; ++closed_dir) {
	if (!surface->IsClosed(closed_dir))
	    continue;
	const int open_dir = 1 - closed_dir;
	const int low_side = open_dir == 0 ? 3 : 0;
	const int high_side = open_dir == 0 ? 1 : 2;

	const ON_Interval closed_domain = surface->Domain(closed_dir);
	const ON_Interval open_domain = surface->Domain(open_dir);
	const double period = closed_domain.Length();
	const double open_length = open_domain.Length();
	if (!(period > ON_ZERO_TOLERANCE) ||
		!(open_length > ON_ZERO_TOLERANCE))
	    continue;

	double closed_min = INFINITY;
	double closed_max = -INFINITY;
	double open_min = INFINITY;
	double open_max = -INFINITY;
	for (int i = 0; i < points.Count(); ++i) {
	    closed_min = std::min(closed_min, points[i].p2d[closed_dir]);
	    closed_max = std::max(closed_max, points[i].p2d[closed_dir]);
	    open_min = std::min(open_min, points[i].p2d[open_dir]);
	    open_max = std::max(open_max, points[i].p2d[open_dir]);
	}
	const double parameter_tolerance = std::max(tolerance,
	    period * 1.0e-4);
	if (closed_max - closed_min > parameter_tolerance ||
		open_max - open_min < 0.90 * open_length)
	    continue;

	int open_direction = 0;
	for (int i = 1; i < points.Count(); ++i) {
	    const double delta = points[i].p2d[open_dir] -
		points[0].p2d[open_dir];
	    if (fabs(delta) > parameter_tolerance) {
		open_direction = delta > 0.0 ? 1 : -1;
		break;
	    }
	}
	if (!open_direction)
	    continue;

	const double open_start = open_direction > 0 ?
	    open_domain.Min() : open_domain.Max();
	const double open_end = open_direction > 0 ?
	    open_domain.Max() : open_domain.Min();
	const double closed_start = points[0].p2d[closed_dir];
	const double closed_end = closed_start - open_direction * period;
	ON_2dPoint corners[5];
	for (int i = 0; i < 5; ++i)
	    corners[i] = points[0].p2d;
	corners[0][closed_dir] = corners[1][closed_dir] = closed_start;
	corners[2][closed_dir] = corners[3][closed_dir] = closed_end;
	corners[4][closed_dir] = closed_start;
	corners[0][open_dir] = corners[3][open_dir] =
	    corners[4][open_dir] = open_start;
	corners[1][open_dir] = corners[2][open_dir] = open_end;

	ON_SimpleArray<BrepTrimPoint> replacement;
	int start_side = open_direction > 0 ? low_side : high_side;
	int end_side = open_direction > 0 ? high_side : low_side;
	if (!surface->IsSingular(start_side))
	    start_side = -1;
	if (!surface->IsSingular(end_side))
	    end_side = -1;
	for (int i = 0; i < 5; ++i) {
	    const int singular_side = (i == 0 || i == 3 || i == 4) ?
		start_side : end_side;
	    if (!fast_append_synthetic_uv(replacement, surface, corners[i],
		    singular_side, scratch))
		return false;
	}
	points = replacement;

	/* Put each hole into the same unwrapped period as the reconstructed
	 * outer rectangle.  Seam preprocessing usually does this already, but
	 * a zero-width outer loop provides no usable polygon for that test. */
	const double closed_low = std::min(closed_start, closed_end);
	const double closed_high = std::max(closed_start, closed_end);
	const double closed_mid = 0.5 * (closed_low + closed_high);
	for (int li = 0; li < face.LoopCount(); ++li) {
	    if (li == outer_index || !brep_loop_points[li] ||
		    brep_loop_points[li]->Count() == 0)
		continue;
	    ON_SimpleArray<BrepTrimPoint> &hole = *brep_loop_points[li];
	    double hole_min = INFINITY;
	    double hole_max = -INFINITY;
	    double hole_sum = 0.0;
	    for (int pi = 0; pi < hole.Count(); ++pi) {
		hole_min = std::min(hole_min, hole[pi].p2d[closed_dir]);
		hole_max = std::max(hole_max, hole[pi].p2d[closed_dir]);
		hole_sum += hole[pi].p2d[closed_dir];
	    }
	    const double hole_mid = hole_sum / hole.Count();
	    const double shift = std::round((closed_mid - hole_mid) / period) *
		period;
	    if (hole_min + shift < closed_low - parameter_tolerance ||
		    hole_max + shift > closed_high + parameter_tolerance)
		continue;
	    for (int pi = 0; pi < hole.Count(); ++pi)
		hole[pi].p2d[closed_dir] += shift;
	}
	if (closed_direction)
	    *closed_direction = closed_dir;
	if (outer_loop_index)
	    *outer_loop_index = outer_index;
	return true;
    }
    return false;
}

static bool
fast_reconstruct_periodic_singular_face(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double tolerance, fast_face_scratch &scratch, int *closed_direction,
	int *outer_loop_index)
{
    if (!surface || face.LoopCount() != 1 || !brep_loop_points ||
	    !brep_loop_points[0] || brep_loop_points[0]->Count() < 4)
	return false;
    const ON_BrepLoop *loop = face.Loop(0);
    if (!loop || loop->m_type != ON_BrepLoop::outer ||
	    loop->TrimCount() < 4)
	return false;

    int seam_trims = 0;
    int singular_trims = 0;
    int closed_boundary_trims = 0;
    for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	const ON_BrepTrim *trim = loop->Trim(ti);
	if (!trim)
	    return false;
	if (trim->m_type == ON_BrepTrim::seam)
	    seam_trims++;
	else if (trim->m_type == ON_BrepTrim::singular)
	    singular_trims++;
	else if (trim->Edge() && trim->m_vi[0] == trim->m_vi[1])
	    closed_boundary_trims++;
	else
	    return false;
    }
    if (seam_trims < 2 || singular_trims < 1 ||
	    closed_boundary_trims != 1)
	return false;

    ON_SimpleArray<BrepTrimPoint> &points = *brep_loop_points[0];
    for (int closed_dir = 0; closed_dir < 2; ++closed_dir) {
	if (!surface->IsClosed(closed_dir))
	    continue;
	const int open_dir = 1 - closed_dir;
	const int low_side = open_dir == 0 ? 3 : 0;
	const int high_side = open_dir == 0 ? 1 : 2;
	const bool low_singular = surface->IsSingular(low_side);
	const bool high_singular = surface->IsSingular(high_side);
	if (low_singular == high_singular)
	    continue;

	const ON_Interval closed_domain = surface->Domain(closed_dir);
	const ON_Interval open_domain = surface->Domain(open_dir);
	const double period = closed_domain.Length();
	const double open_length = open_domain.Length();
	if (!(period > ON_ZERO_TOLERANCE) ||
		!(open_length > ON_ZERO_TOLERANCE))
	    continue;

	double closed_min = INFINITY;
	double closed_max = -INFINITY;
	double open_min = INFINITY;
	double open_max = -INFINITY;
	for (int pi = 0; pi < points.Count(); ++pi) {
	    closed_min = std::min(closed_min, points[pi].p2d[closed_dir]);
	    closed_max = std::max(closed_max, points[pi].p2d[closed_dir]);
	    open_min = std::min(open_min, points[pi].p2d[open_dir]);
	    open_max = std::max(open_max, points[pi].p2d[open_dir]);
	}
	if (closed_max - closed_min < 0.90 * period ||
		open_max - open_min < 0.90 * open_length)
	    continue;

	const double parameter_tolerance = std::max(tolerance,
	    std::max(period, open_length) * 1.0e-4);
	if (closed_min < closed_domain.Min() - parameter_tolerance ||
		closed_max > closed_domain.Max() + parameter_tolerance ||
		open_min < open_domain.Min() - parameter_tolerance ||
		open_max > open_domain.Max() + parameter_tolerance)
	    continue;

	const int singular_side = low_singular ? low_side : high_side;
	const double open_start = low_singular ? open_domain.Max() :
	    open_domain.Min();
	const double open_end = low_singular ? open_domain.Min() :
	    open_domain.Max();
	ON_2dPoint corners[5];
	for (int ci = 0; ci < 5; ++ci)
	    corners[ci] = points[0].p2d;
	corners[0][open_dir] = corners[3][open_dir] =
	    corners[4][open_dir] = open_start;
	corners[1][open_dir] = corners[2][open_dir] = open_end;
	corners[0][closed_dir] = corners[1][closed_dir] =
	    corners[4][closed_dir] = closed_domain.Min();
	corners[2][closed_dir] = corners[3][closed_dir] =
	    closed_domain.Max();

	ON_SimpleArray<BrepTrimPoint> replacement;
	for (int ci = 0; ci < 5; ++ci) {
	    const int corner_singularity = (ci == 1 || ci == 2) ?
		singular_side : -1;
	    if (!fast_append_synthetic_uv(replacement, surface, corners[ci],
		    corner_singularity, scratch))
		return false;
	}
	points = replacement;
	if (closed_direction)
	    *closed_direction = closed_dir;
	if (outer_loop_index)
	    *outer_loop_index = 0;
	return true;
    }
    return false;
}

static bool
fast_reconstruct_paired_periodic_strip(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double tolerance, fast_face_scratch &scratch, int *closed_direction,
	int *outer_loop_index)
{
    if (!surface || face.LoopCount() != 1 || !brep_loop_points ||
	    !brep_loop_points[0] || brep_loop_points[0]->Count() < 6)
	return false;
    const ON_BrepLoop *loop = face.Loop(0);
    if (!loop || loop->m_type != ON_BrepLoop::outer ||
	    loop->TrimCount() != 6)
	return false;

    std::vector<const ON_BrepTrim *> seam_trims;
    std::vector<const ON_BrepTrim *> boundary_trims;
    for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	const ON_BrepTrim *trim = loop->Trim(ti);
	if (!trim || trim->m_type == ON_BrepTrim::singular)
	    return false;
	if (trim->m_type == ON_BrepTrim::seam)
	    seam_trims.push_back(trim);
	else
	    boundary_trims.push_back(trim);
    }
    if (seam_trims.size() != 2 || boundary_trims.size() != 4 ||
	    !seam_trims[0]->Edge() ||
	    seam_trims[0]->Edge() != seam_trims[1]->Edge())
	return false;

    struct boundary_info {
	const ON_BrepTrim *trim;
	double open_coordinate;
    };
    ON_SimpleArray<BrepTrimPoint> &points = *brep_loop_points[0];
    for (int closed_dir = 0; closed_dir < 2; ++closed_dir) {
	if (!surface->IsClosed(closed_dir))
	    continue;
	const int open_dir = 1 - closed_dir;
	const double period = surface->Domain(closed_dir).Length();
	const double open_length = surface->Domain(open_dir).Length();
	if (!(period > ON_ZERO_TOLERANCE) ||
		!(open_length > ON_ZERO_TOLERANCE))
	    continue;
	const double parameter_tolerance = std::max(tolerance,
	    std::max(period, open_length) * 1.0e-4);
	const double isocurve_tolerance = std::max(parameter_tolerance,
	    open_length * 1.0e-3);

	bool seams_valid = true;
	for (const ON_BrepTrim *trim : seam_trims) {
	    const ON_Interval domain = trim->Domain();
	    const ON_2dPoint start = trim->PointAt(domain.Min());
	    const ON_2dPoint end = trim->PointAt(domain.Max());
	    if (!start.IsValid() || !end.IsValid() ||
		    fabs(end[closed_dir] - start[closed_dir]) >
			parameter_tolerance ||
		    fabs(end[open_dir] - start[open_dir]) <=
			parameter_tolerance) {
		seams_valid = false;
		break;
	    }
	}
	if (!seams_valid)
	    continue;

	std::vector<boundary_info> boundaries;
	boundaries.reserve(boundary_trims.size());
	bool boundaries_valid = true;
	for (const ON_BrepTrim *trim : boundary_trims) {
	    const ON_Interval domain = trim->Domain();
	    const ON_2dPoint start = trim->PointAt(domain.Min());
	    const ON_2dPoint end = trim->PointAt(domain.Max());
	    if (!start.IsValid() || !end.IsValid() ||
		    fabs(end[open_dir] - start[open_dir]) >
			isocurve_tolerance || !trim->Edge()) {
		boundaries_valid = false;
		break;
	    }
	    boundaries.push_back({trim,
		0.5 * (start[open_dir] + end[open_dir])});
	}
	if (!boundaries_valid)
	    continue;
	std::sort(boundaries.begin(), boundaries.end(),
	    [](const boundary_info &a, const boundary_info &b) {
		return a.open_coordinate < b.open_coordinate;
	    });
	if (fabs(boundaries[0].open_coordinate -
		boundaries[1].open_coordinate) > isocurve_tolerance ||
		fabs(boundaries[2].open_coordinate -
		    boundaries[3].open_coordinate) > isocurve_tolerance)
	    continue;
	const double open_low = 0.5 * (boundaries[0].open_coordinate +
	    boundaries[1].open_coordinate);
	const double open_high = 0.5 * (boundaries[2].open_coordinate +
	    boundaries[3].open_coordinate);
	if (open_high - open_low <= isocurve_tolerance)
	    continue;

	auto reversed_pair = [](const ON_BrepTrim *first,
		const ON_BrepTrim *second) {
	    return first->Edge() != second->Edge() &&
		first->m_vi[0] == second->m_vi[1] &&
		first->m_vi[1] == second->m_vi[0];
	};
	if (!reversed_pair(boundaries[0].trim, boundaries[1].trim) ||
		!reversed_pair(boundaries[2].trim, boundaries[3].trim))
	    continue;

	bool seam_joins_boundaries = true;
	for (const ON_BrepTrim *trim : seam_trims) {
	    const ON_Interval domain = trim->Domain();
	    const double first = trim->PointAt(domain.Min())[open_dir];
	    const double second = trim->PointAt(domain.Max())[open_dir];
	    if (fabs(std::min(first, second) - open_low) >
		    isocurve_tolerance ||
		    fabs(std::max(first, second) - open_high) >
			isocurve_tolerance) {
		seam_joins_boundaries = false;
		break;
	    }
	}
	if (!seam_joins_boundaries)
	    continue;

	double closed_min[2] = {INFINITY, INFINITY};
	double closed_max[2] = {-INFINITY, -INFINITY};
	const double open_coordinates[2] = {open_low, open_high};
	for (int pi = 0; pi < points.Count(); ++pi) {
	    for (int bi = 0; bi < 2; ++bi) {
		if (fabs(points[pi].p2d[open_dir] -
			open_coordinates[bi]) > isocurve_tolerance)
		    continue;
		closed_min[bi] = std::min(closed_min[bi],
		    points[pi].p2d[closed_dir]);
		closed_max[bi] = std::max(closed_max[bi],
		    points[pi].p2d[closed_dir]);
	    }
	}
	if (closed_max[0] - closed_min[0] < 0.90 * period ||
		closed_max[0] - closed_min[0] > 1.10 * period ||
		closed_max[1] - closed_min[1] < 0.90 * period ||
		closed_max[1] - closed_min[1] > 1.10 * period)
	    continue;

	const ON_BrepTrim *seam = seam_trims[0];
	const ON_Interval seam_domain = seam->Domain();
	const ON_2dPoint seam_start = seam->PointAt(seam_domain.Min());
	const ON_2dPoint seam_end = seam->PointAt(seam_domain.Max());
	const int open_winding = seam_end[open_dir] >
	    seam_start[open_dir] ? 1 : -1;
	const double closed_start = seam_start[closed_dir];
	const double closed_end = closed_start - open_winding * period;
	ON_2dPoint corners[5];
	for (int ci = 0; ci < 5; ++ci)
	    corners[ci] = seam_start;
	corners[0][closed_dir] = corners[1][closed_dir] =
	    corners[4][closed_dir] = closed_start;
	corners[2][closed_dir] = corners[3][closed_dir] = closed_end;
	corners[0][open_dir] = corners[3][open_dir] =
	    corners[4][open_dir] = seam_start[open_dir];
	corners[1][open_dir] = corners[2][open_dir] = seam_end[open_dir];

	ON_SimpleArray<BrepTrimPoint> replacement;
	for (int ci = 0; ci < 5; ++ci) {
	    if (!fast_append_synthetic_uv(replacement, surface, corners[ci],
		    -1, scratch))
		return false;
	}
	points = replacement;
	if (closed_direction)
	    *closed_direction = closed_dir;
	if (outer_loop_index)
	    *outer_loop_index = 0;
	return true;
    }
    return false;
}

static void
fast_seed_full_periodic_face(const ON_Surface *surface,
	const ON_SimpleArray<BrepTrimPoint> &boundary,
	const struct bg_tess_tol *ttol, double model_diagonal,
	int closed_dir, ON_2dPointArray &surface_points)
{
    if (!surface || boundary.Count() != 5 || !ttol)
	return;
    if (closed_dir < 0 || closed_dir > 1 ||
	    !surface->IsClosed(closed_dir))
	return;
    const int open_dir = 1 - closed_dir;

    double relative_tolerance = ttol->rel > 0.0 ? ttol->rel : 0.01;
    if (ttol->abs > 0.0 && model_diagonal > ON_ZERO_TOLERANCE)
	relative_tolerance = std::min(relative_tolerance,
	    ttol->abs / model_diagonal);
    relative_tolerance = std::max(relative_tolerance, 1.0e-5);
    double angular_step = sqrt(8.0 * relative_tolerance);
    if (ttol->norm > 0.0)
	angular_step = std::min(angular_step, (double)ttol->norm);
    angular_step = std::max(angular_step, ON_PI / 256.0);
    size_t closed_steps = (size_t)ceil(2.0 * ON_PI / angular_step);
    closed_steps = std::max((size_t)16,
	std::min((size_t)256, closed_steps));
    const size_t open_steps = std::max((size_t)8, closed_steps / 2);

    const double closed_start = boundary[0].p2d[closed_dir];
    const double closed_delta = boundary[2].p2d[closed_dir] -
	boundary[1].p2d[closed_dir];
    const double open_start = boundary[0].p2d[open_dir];
    const double open_delta = boundary[1].p2d[open_dir] -
	boundary[0].p2d[open_dir];
    for (size_t i = 1; i < closed_steps; ++i) {
	for (size_t j = 1; j < open_steps; ++j) {
	    if (surface_points.Count() >= FAST_CDT_MAX_SURFACE_SAMPLES)
		return;
	    ON_2dPoint uv = boundary[0].p2d;
	    uv[closed_dir] = closed_start + closed_delta *
		(double)i / (double)closed_steps;
	    uv[open_dir] = open_start + open_delta *
		(double)j / (double)open_steps;
	    surface_points.Append(uv);
	}
    }
}

static bool
fast_reconstruct_singular_cap(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double tolerance, double model_diagonal,
	fast_face_scratch &scratch)
{
    if (!surface || face.LoopCount() != 1 || !brep_loop_points ||
	    !brep_loop_points[0] || brep_loop_points[0]->Count() < 2)
	return false;

    ON_SimpleArray<BrepTrimPoint> &points = *brep_loop_points[0];
    const ON_2dPoint start = points[0].p2d;
    const ON_2dPoint end = points[points.Count() - 1].p2d;
    int closed_dir = -1;
    int open_dir = -1;
    long winding = 0;
    for (int dir = 0; dir < 2; ++dir) {
	if (!surface->IsClosed(dir))
	    continue;
	const double period = surface->Domain(dir).Length();
	if (!(period > ON_ZERO_TOLERANCE) || !std::isfinite(period))
	    continue;
	const double delta = end[dir] - start[dir];
	const long turns = std::lround(delta / period);
	const double parameter_tolerance = std::max(tolerance,
	    period * 1.0e-4);
	if (labs(turns) == 1 &&
		fabs(delta - turns * period) <= parameter_tolerance) {
	    const ON_2dPoint start_uv = UnwrapUVPoint(surface, start,
		BREP_SAME_POINT_TOLERANCE);
	    const ON_2dPoint end_uv = UnwrapUVPoint(surface, end,
		BREP_SAME_POINT_TOLERANCE);
	    const ON_3dPoint start_3d = surface->PointAt(start_uv.x,
		start_uv.y);
	    const ON_3dPoint end_3d = surface->PointAt(end_uv.x, end_uv.y);
	    const double closure_tolerance = std::max(ON_ZERO_TOLERANCE,
		std::max(model_diagonal, 1.0) * 1.0e-4);
	    if (start_3d.DistanceTo(end_3d) > closure_tolerance)
		continue;
	    closed_dir = dir;
	    open_dir = 1 - dir;
	    winding = turns;
	    break;
	}
    }
    if (closed_dir < 0)
	return false;

    double open_min = INFINITY;
    double open_max = -INFINITY;
    for (int i = 0; i < points.Count(); ++i) {
	open_min = std::min(open_min, points[i].p2d[open_dir]);
	open_max = std::max(open_max, points[i].p2d[open_dir]);
    }
    if (!std::isfinite(open_min) || !std::isfinite(open_max) ||
	    open_max - open_min > tolerance)
	return false;

    /* OpenNURBS side numbering is south, east, north, west.  With the
     * standard active-region-on-the-left trim convention, increasing U
     * selects north and increasing V selects west. */
    int singular_side = -1;
    if (closed_dir == 0)
	singular_side = winding > 0 ? 2 : 0;
    else
	singular_side = winding > 0 ? 3 : 1;
    const ON_BrepLoop *loop = face.Loop(0);
    if (loop && loop->m_type == ON_BrepLoop::inner) {
	if (singular_side == 0)
	    singular_side = 2;
	else if (singular_side == 2)
	    singular_side = 0;
	else if (singular_side == 1)
	    singular_side = 3;
	else if (singular_side == 3)
	    singular_side = 1;
    }
    if (!surface->IsSingular(singular_side))
	return false;

    const ON_Interval open_domain = surface->Domain(open_dir);
    const double pole_coordinate =
	(singular_side == 0 || singular_side == 3) ?
	open_domain.Min() : open_domain.Max();
    ON_2dPoint end_pole = end;
    ON_2dPoint start_pole = start;
    end_pole[open_dir] = pole_coordinate;
    start_pole[open_dir] = pole_coordinate;

    const int old_count = points.Count();
    if (!fast_append_synthetic_uv(points, surface, end_pole,
	    singular_side, scratch) ||
	    !fast_append_synthetic_uv(points, surface, start_pole,
	    singular_side, scratch)) {
	points.SetCount(old_count);
	return false;
    }
    return true;
}

struct fast_periodic_loop_info {
    int closed_dir = -1;
    int open_dir = -1;
    int winding = 0;
    double open_coordinate = 0.0;
};

static bool
fast_periodic_boundary_loop(const ON_Surface *surface,
	const ON_SimpleArray<BrepTrimPoint> &points,
	fast_periodic_loop_info &info)
{
    if (!surface || points.Count() < 2)
	return false;

    for (int dir = 0; dir < 2; ++dir) {
	if (!surface->IsClosed(dir))
	    continue;
	const int open_dir = 1 - dir;
	const double period = surface->Domain(dir).Length();
	const double open_length = surface->Domain(open_dir).Length();
	if (!(period > ON_ZERO_TOLERANCE) ||
		!(open_length > ON_ZERO_TOLERANCE))
	    continue;
	double closed_min = INFINITY;
	double closed_max = -INFINITY;
	double open_min = INFINITY;
	double open_max = -INFINITY;
	for (int i = 0; i < points.Count(); ++i) {
	    closed_min = std::min(closed_min, points[i].p2d[dir]);
	    closed_max = std::max(closed_max, points[i].p2d[dir]);
	    open_min = std::min(open_min, points[i].p2d[open_dir]);
	    open_max = std::max(open_max, points[i].p2d[open_dir]);
	}
	const double delta = points[points.Count() - 1].p2d[dir] -
	    points[0].p2d[dir];
	if (closed_max - closed_min < 0.90 * period ||
		closed_max - closed_min > 1.10 * period ||
		fabs(delta) < 0.90 * period ||
		open_max - open_min > 0.01 * open_length)
	    continue;
	info.closed_dir = dir;
	info.open_dir = open_dir;
	info.winding = delta > 0.0 ? 1 : -1;
	info.open_coordinate = 0.5 * (open_min + open_max);
	return true;
    }
    return false;
}

static bool
fast_reconstruct_periodic_strip(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double model_diagonal)
{
    if (!surface || face.LoopCount() != 2 || !brep_loop_points ||
	    !brep_loop_points[0] || !brep_loop_points[1])
	return false;

    fast_periodic_loop_info first_info;
    fast_periodic_loop_info second_info;
    ON_SimpleArray<BrepTrimPoint> &first = *brep_loop_points[0];
    ON_SimpleArray<BrepTrimPoint> &second = *brep_loop_points[1];
    if (!fast_periodic_boundary_loop(surface, first, first_info) ||
	    !fast_periodic_boundary_loop(surface, second, second_info) ||
	    first_info.closed_dir != second_info.closed_dir ||
	    fabs(first_info.open_coordinate - second_info.open_coordinate) <=
		BREP_SAME_POINT_TOLERANCE)
	return false;

    const int closed_dir = first_info.closed_dir;
    const double period = surface->Domain(closed_dir).Length();
    ON_SimpleArray<BrepTrimPoint> opposite;
    if (first_info.winding == second_info.winding) {
	for (int i = second.Count() - 1; i >= 0; --i)
	    opposite.Append(second[i]);
    } else {
	opposite.Append(second.Count(), second.Array());
    }
    if (opposite.Count() < 2)
	return false;

    const double closure_tolerance = std::max(ON_ZERO_TOLERANCE,
	std::max(model_diagonal, 1.0) * 0.01);
    const ON_2dPoint first_start_uv = UnwrapUVPoint(surface,
	first[0].p2d, BREP_SAME_POINT_TOLERANCE);
    const ON_2dPoint first_end_uv = UnwrapUVPoint(surface,
	first[first.Count() - 1].p2d, BREP_SAME_POINT_TOLERANCE);
    const ON_2dPoint second_start_uv = UnwrapUVPoint(surface,
	opposite[0].p2d, BREP_SAME_POINT_TOLERANCE);
    const ON_2dPoint second_end_uv = UnwrapUVPoint(surface,
	opposite[opposite.Count() - 1].p2d, BREP_SAME_POINT_TOLERANCE);
    const ON_3dPoint first_start = surface->PointAt(first_start_uv.x,
	first_start_uv.y);
    const ON_3dPoint first_end = surface->PointAt(first_end_uv.x,
	first_end_uv.y);
    const ON_3dPoint second_start = surface->PointAt(second_start_uv.x,
	second_start_uv.y);
    const ON_3dPoint second_end = surface->PointAt(second_end_uv.x,
	second_end_uv.y);
    if (first_start.DistanceTo(first_end) > closure_tolerance ||
	    second_start.DistanceTo(second_end) > closure_tolerance)
	return false;

    const double shift = std::round((
	first[first.Count() - 1].p2d[closed_dir] -
	opposite[0].p2d[closed_dir]) / period) * period;
    for (int i = 0; i < opposite.Count(); ++i)
	opposite[i].p2d[closed_dir] += shift;
    const double seam_tolerance = 0.05 * period;
    if (fabs(first[first.Count() - 1].p2d[closed_dir] -
	    opposite[0].p2d[closed_dir]) > seam_tolerance ||
	    fabs(first[0].p2d[closed_dir] -
	    opposite[opposite.Count() - 1].p2d[closed_dir]) >
	    seam_tolerance)
	return false;

    const int open_dir = 1 - closed_dir;
    const double first_open = 0.5 * (first[0].p2d[open_dir] +
	first[first.Count() - 1].p2d[open_dir]);
    const double second_open = 0.5 * (opposite[0].p2d[open_dir] +
	opposite[opposite.Count() - 1].p2d[open_dir]);
    const double closed_start = first[0].p2d[closed_dir];
    const double closed_end = closed_start + first_info.winding * period;
    first[0].p2d[open_dir] = first_open;
    first[0].p2d[closed_dir] = closed_start;
    first[first.Count() - 1].p2d[open_dir] = first_open;
    first[first.Count() - 1].p2d[closed_dir] = closed_end;
    opposite[0].p2d[open_dir] = second_open;
    opposite[0].p2d[closed_dir] = closed_end;
    opposite[opposite.Count() - 1].p2d[open_dir] = second_open;
    opposite[opposite.Count() - 1].p2d[closed_dir] = closed_start;
    const int endpoint_indices[4] = {0, first.Count() - 1, 0,
	opposite.Count() - 1};
    ON_SimpleArray<BrepTrimPoint> *endpoint_arrays[4] = {
	&first, &first, &opposite, &opposite
    };
    for (int i = 0; i < 4; ++i) {
	BrepTrimPoint &endpoint =
	    (*endpoint_arrays[i])[endpoint_indices[i]];
	if (endpoint.p3d) {
	    const ON_2dPoint uv = UnwrapUVPoint(surface, endpoint.p2d,
		BREP_SAME_POINT_TOLERANCE);
	    *endpoint.p3d = surface->PointAt(uv.x, uv.y);
	}
    }

    first.Append(opposite.Count(), opposite.Array());
    second.Empty();
    return true;
}

static bool
fast_reconstruct_periodic_strip_domain(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double tolerance, fast_face_scratch &scratch, int *closed_direction,
	int *outer_loop_index)
{
    if (!surface || face.LoopCount() != 2 || !brep_loop_points)
	return false;
    int outer_index = -1;
    int inner_index = -1;
    for (int li = 0; li < 2; ++li) {
	const ON_BrepLoop *loop = face.Loop(li);
	if (!loop || loop->TrimCount() != 1 || !brep_loop_points[li])
	    return false;
	if (loop->m_type == ON_BrepLoop::outer)
	    outer_index = li;
	else if (loop->m_type == ON_BrepLoop::inner)
	    inner_index = li;
    }
    if (outer_index < 0 || inner_index < 0)
	return false;

    const ON_BrepTrim *outer_trim = face.Loop(outer_index)->Trim(0);
    const ON_BrepTrim *inner_trim = face.Loop(inner_index)->Trim(0);
    if (!outer_trim || !inner_trim)
	return false;
    const ON_Interval outer_domain = outer_trim->Domain();
    const ON_Interval inner_domain = inner_trim->Domain();
    const ON_2dPoint outer_start = outer_trim->PointAt(outer_domain.Min());
    const ON_2dPoint outer_end = outer_trim->PointAt(outer_domain.Max());
    const ON_2dPoint inner_start = inner_trim->PointAt(inner_domain.Min());
    const ON_2dPoint inner_end = inner_trim->PointAt(inner_domain.Max());
    if (!outer_start.IsValid() || !outer_end.IsValid() ||
	    !inner_start.IsValid() || !inner_end.IsValid())
	return false;

    for (int closed_dir = 0; closed_dir < 2; ++closed_dir) {
	if (!surface->IsClosed(closed_dir))
	    continue;
	const int open_dir = 1 - closed_dir;
	const double period = surface->Domain(closed_dir).Length();
	const double open_length = surface->Domain(open_dir).Length();
	if (!(period > ON_ZERO_TOLERANCE) ||
		!(open_length > ON_ZERO_TOLERANCE))
	    continue;
	const double parameter_tolerance = std::max(tolerance,
	    std::max(period, open_length) * 1.0e-4);
	const double outer_delta = outer_end[closed_dir] -
	    outer_start[closed_dir];
	const double inner_delta = inner_end[closed_dir] -
	    inner_start[closed_dir];
	double outer_closed_min = INFINITY;
	double outer_closed_max = -INFINITY;
	double outer_open_min = INFINITY;
	double outer_open_max = -INFINITY;
	double inner_closed_min = INFINITY;
	double inner_closed_max = -INFINITY;
	double inner_open_min = INFINITY;
	double inner_open_max = -INFINITY;
	ON_SimpleArray<BrepTrimPoint> &outer_points =
	    *brep_loop_points[outer_index];
	ON_SimpleArray<BrepTrimPoint> &inner_points =
	    *brep_loop_points[inner_index];
	for (int pi = 0; pi < outer_points.Count(); ++pi) {
	    outer_closed_min = std::min(outer_closed_min,
		outer_points[pi].p2d[closed_dir]);
	    outer_closed_max = std::max(outer_closed_max,
		outer_points[pi].p2d[closed_dir]);
	    outer_open_min = std::min(outer_open_min,
		outer_points[pi].p2d[open_dir]);
	    outer_open_max = std::max(outer_open_max,
		outer_points[pi].p2d[open_dir]);
	}
	for (int pi = 0; pi < inner_points.Count(); ++pi) {
	    inner_closed_min = std::min(inner_closed_min,
		inner_points[pi].p2d[closed_dir]);
	    inner_closed_max = std::max(inner_closed_max,
		inner_points[pi].p2d[closed_dir]);
	    inner_open_min = std::min(inner_open_min,
		inner_points[pi].p2d[open_dir]);
	    inner_open_max = std::max(inner_open_max,
		inner_points[pi].p2d[open_dir]);
	}
	auto trim_matches_periodic_isocurve = [&](const ON_BrepTrim *trim,
		double open_coordinate) {
	    const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
	    if (!trim || !edge || trim->m_vi[0] != trim->m_vi[1])
		return false;
	    const ON_Interval edge_domain = edge->Domain();
	    const ON_Interval trim_domain = trim->Domain();
	    if (!edge_domain.IsIncreasing() || !trim_domain.IsIncreasing())
		return false;
	    const ON_2dPoint trim_start = trim->PointAt(trim_domain.Min());
	    const double match_tolerance = std::max(tolerance,
		std::max(edge->m_tolerance,
		    std::max(trim->m_tolerance[0], trim->m_tolerance[1])));
	    for (int winding : {-1, 1}) {
		bool matches = true;
		for (int sample = 0; sample <= 8; ++sample) {
		    const double fraction = (double)sample / 8.0;
		    const ON_3dPoint edge_point = edge->PointAt(
			edge_domain.ParameterAt(fraction));
		    ON_2dPoint uv = trim_start;
		    uv[closed_dir] = trim_start[closed_dir] +
			winding * fraction * period;
		    uv[open_dir] = open_coordinate;
		    uv = UnwrapUVPoint(surface, uv,
			BREP_SAME_POINT_TOLERANCE);
		    const ON_3dPoint surface_point = surface->PointAt(uv.x, uv.y);
		    if (!edge_point.IsValid() || !surface_point.IsValid() ||
			    edge_point.DistanceTo(surface_point) > match_tolerance) {
			matches = false;
			break;
		    }
		}
		if (matches)
		    return true;
	    }
	    return false;
	};
	const double outer_sample_open = 0.5 * (outer_open_min +
	    outer_open_max);
	const double inner_sample_open = 0.5 * (inner_open_min +
	    inner_open_max);
	const bool outer_spans_period =
	    fabs(fabs(outer_delta) - period) <= parameter_tolerance ||
	    outer_closed_max - outer_closed_min >= 0.90 * period ||
	    trim_matches_periodic_isocurve(outer_trim, outer_sample_open);
	const bool inner_spans_period =
	    fabs(fabs(inner_delta) - period) <= parameter_tolerance ||
	    inner_closed_max - inner_closed_min >= 0.90 * period ||
	    trim_matches_periodic_isocurve(inner_trim, inner_sample_open);
	const double isocurve_tolerance = std::max(parameter_tolerance,
	    open_length * 1.0e-3);
	if (!outer_spans_period || !inner_spans_period ||
		outer_open_max - outer_open_min > isocurve_tolerance ||
		inner_open_max - inner_open_min > isocurve_tolerance)
	    continue;

	const double outer_open = outer_sample_open;
	const double inner_open = inner_sample_open;
	if (fabs(outer_open - inner_open) <= parameter_tolerance)
	    continue;
	const double closed_start = fabs(outer_delta) > 0.90 * period ?
	    outer_start[closed_dir] : outer_closed_min;
	const double closed_end = closed_start +
	    (outer_delta < -0.90 * period ? -period : period);
	ON_2dPoint corners[5];
	for (int ci = 0; ci < 5; ++ci)
	    corners[ci] = outer_start;
	corners[0][closed_dir] = corners[1][closed_dir] =
	    corners[4][closed_dir] = closed_start;
	corners[2][closed_dir] = corners[3][closed_dir] = closed_end;
	corners[0][open_dir] = corners[3][open_dir] =
	    corners[4][open_dir] = outer_open;
	corners[1][open_dir] = corners[2][open_dir] = inner_open;

	ON_SimpleArray<BrepTrimPoint> replacement;
	for (int ci = 0; ci < 5; ++ci) {
	    if (!fast_append_synthetic_uv(replacement, surface, corners[ci],
		    -1, scratch))
		return false;
	}
	*brep_loop_points[outer_index] = replacement;
	brep_loop_points[inner_index]->Empty();
	if (closed_direction)
	    *closed_direction = closed_dir;
	if (outer_loop_index)
	    *outer_loop_index = outer_index;
	return true;
    }
    return false;
}

void
detria_CDT(struct bu_list *vhead,
	     const ON_BrepFace &face,
	     const struct bg_tess_tol *ttol,
	     const struct bn_tol *tol,
	     struct bu_list *vlfree,
	     int plottype,
	     int UNUSED(num_points),
	     double model_diagonal)
{
    fast_face_scratch scratch;
    fast_line_store line_store;
    ON_RTree rt_trims;
    ON_2dPointArray on_surf_points;
    const ON_Surface *s = &face;
    if (!s)
	return;
    fast_surface_metrics metrics;
    int fi = face.m_face_index;

    fastf_t max_dist = 0.0;
    fast_surface_metrics_get(s, &metrics);
    if (metrics.size_valid) {
	if (metrics.width < tol->dist || metrics.height < tol->dist)
	    return;
	max_dist = sqrt(metrics.width * metrics.width +
	    metrics.height * metrics.height) / 10.0;
    }


    int loop_cnt = face.LoopCount();
    ON_2dPointArray on_loop_points;
    fast_loop_point_store loop_points(loop_cnt);
    ON_SimpleArray<BrepTrimPoint> **brep_loop_points = loop_points.points;

    // first simply load loop point samples
    for (int li = 0; li < loop_cnt; li++) {
	const ON_BrepLoop *loop = face.Loop(li);
	get_loop_sample_points(brep_loop_points[li], face, loop, max_dist,
		ttol, tol, scratch, model_diagonal, false);
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
	    const bool uv_closed = V2NEAR_EQUAL(
		(*brep_loop_points[li])[0].p2d,
		(*brep_loop_points[li])[num_loop_points - 1].p2d,
		BREP_SAME_POINT_TOLERANCE);
	    const int first_point = uv_closed ? 1 : 0;
	    for (int i = first_point; i < num_loop_points; i++) {
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
	    if (!uv_closed) {
		ON_Line *line = new ON_Line(
		    (*brep_loop_points[li])[num_loop_points - 1].p2d,
		    (*brep_loop_points[li])[0].p2d);
		line_store.lines.push_back(line);
		ON_BoundingBox bb = line->BoundingBox();
		bb.m_max.x += ON_ZERO_TOLERANCE;
		bb.m_max.y += ON_ZERO_TOLERANCE;
		bb.m_max.z += ON_ZERO_TOLERANCE;
		bb.m_min.x -= ON_ZERO_TOLERANCE;
		bb.m_min.y -= ON_ZERO_TOLERANCE;
		bb.m_min.z -= ON_ZERO_TOLERANCE;
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

    getSurfacePoints(face, ttol, tol, on_surf_points, metrics,
	model_diagonal);
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

    const double model_diagonal = fast_brep_diagonal(brep);

    if (index == -1) {
        for (index = 0; index < brep->m_F.Count(); index++) {
            const ON_BrepFace& face = brep->m_F[index];
            detria_CDT(vhead, face, ttol, tol, vlfree, plottype,
		num_points, model_diagonal);
        }
    } else if (index < brep->m_F.Count()) {
        const ON_BrepFaceArray& faces = brep->m_F;
        if (index < faces.Count()) {
            const ON_BrepFace& face = faces[index];
            face.Dump(tl);
            detria_CDT(vhead, face, ttol, tol, vlfree, plottype,
		num_points, model_diagonal);
        }
    }

    if (vls) {
	bu_vls_printf(vls, "%s", ON_String(wstr).Array());
    }

    return 0;
}

static bool
fast_loop_uv_closed(const ON_Surface *surface, const ON_BrepLoop *loop,
	const ON_SimpleArray<BrepTrimPoint> &points,
	const struct bn_tol *tol)
{
    if (points.Count() < 2)
	return false;
    const BrepTrimPoint &first_point = points[0];
    const BrepTrimPoint &last_point = points[points.Count() - 1];
    if (V2NEAR_EQUAL(first_point.p2d, last_point.p2d,
	    BREP_SAME_POINT_TOLERANCE))
	return true;
    if (!surface || !loop || loop->TrimCount() < 1 ||
	    !first_point.p3d || !last_point.p3d)
	return false;

    const ON_BrepTrim *first_trim = loop->Trim(0);
    const ON_BrepTrim *last_trim = loop->Trim(loop->TrimCount() - 1);
    if (!first_trim || !last_trim ||
	    first_trim->m_vi[0] != last_trim->m_vi[1])
	return false;
    for (int dir = 0; dir < 2; ++dir) {
	const double domain_length = surface->Domain(dir).Length();
	const double parameter_tolerance = std::max(
	    BREP_SAME_POINT_TOLERANCE,
	    std::isfinite(domain_length) ? domain_length * 1.0e-4 : 0.0);
	if (fabs(first_point.p2d[dir] - last_point.p2d[dir]) >
		parameter_tolerance)
	    return false;
    }

    double model_tolerance = tol ? tol->dist : ON_ZERO_TOLERANCE;
    const ON_BrepTrim *endpoint_trims[2] = {first_trim, last_trim};
    for (const ON_BrepTrim *trim : endpoint_trims) {
	model_tolerance = std::max(model_tolerance,
	    std::max(trim->m_tolerance[0], trim->m_tolerance[1]));
	if (trim->Edge())
	    model_tolerance = std::max(model_tolerance,
		trim->Edge()->m_tolerance);
    }
    return first_point.p3d->DistanceTo(*last_point.p3d) <=
	model_tolerance;
}

static bool
bg_CDT_attempt(std::vector<int> &faces, std::vector<fastf_t> &pnt_norms,
	std::vector<fastf_t> &pnts,
	const ON_BrepFace &face,
	const struct bg_tess_tol *ttol,
	const struct bn_tol *tol,
	double model_diagonal,
	bool repair_pcurves)
{
    fast_face_scratch scratch;
    fast_line_store line_store;
    ON_RTree rt_trims;
    ON_2dPointArray on_surf_points;
    const ON_Surface *s = &face;
    if (!s)
	return false;
    fast_surface_metrics metrics;
    int fi = face.m_face_index;

    fastf_t max_dist = 0.0;
    fast_surface_metrics_get(s, &metrics);
    if (metrics.size_valid) {
	if (metrics.width < tol->dist || metrics.height < tol->dist)
	    return false;
	max_dist = sqrt(metrics.width * metrics.width +
	    metrics.height * metrics.height) / 10.0;
    }

    int loop_cnt = face.LoopCount();
    ON_2dPointArray on_loop_points;
    fast_loop_point_store loop_points(loop_cnt);
    ON_SimpleArray<BrepTrimPoint> **brep_loop_points = loop_points.points;

    // first simply load loop point samples
    for (int li = 0; li < loop_cnt; li++) {
	const ON_BrepLoop *loop = face.Loop(li);
	get_loop_sample_points(brep_loop_points[li], face, loop, max_dist,
		ttol, tol, scratch, model_diagonal, repair_pcurves);
    }
    if (loop_cnt == 1)
	fast_reconstruct_planar_domain_loop(s, face, *brep_loop_points[0],
	    scratch);
    if (scratch.hit_sample_limit) {
	return false;
    }

    fast_bridge_store bridgePoints;
    if (s->IsClosed(0) || s->IsClosed(1))
	PerformClosedSurfaceChecks(s, face, ttol, tol, brep_loop_points,
	    BREP_SAME_POINT_TOLERANCE, bridgePoints);
    int full_periodic_closed_dir = -1;
    int full_periodic_outer_index = -1;
    bool full_periodic_face = fast_reconstruct_full_periodic_face(s,
	face, brep_loop_points, BREP_SAME_POINT_TOLERANCE, scratch,
	&full_periodic_closed_dir, &full_periodic_outer_index);
    if (!full_periodic_face)
	full_periodic_face = fast_reconstruct_periodic_singular_face(s, face,
	    brep_loop_points, BREP_SAME_POINT_TOLERANCE, scratch,
	    &full_periodic_closed_dir, &full_periodic_outer_index);
    if (!full_periodic_face)
	full_periodic_face = fast_reconstruct_paired_periodic_strip(s, face,
	    brep_loop_points, BREP_SAME_POINT_TOLERANCE, scratch,
	    &full_periodic_closed_dir, &full_periodic_outer_index);
    const bool singular_cap_face = fast_reconstruct_singular_cap(s, face,
	brep_loop_points,
	BREP_SAME_POINT_TOLERANCE, model_diagonal, scratch);
    if (!full_periodic_face)
	full_periodic_face = fast_reconstruct_periodic_strip_domain(s, face,
	    brep_loop_points, BREP_SAME_POINT_TOLERANCE, scratch,
	    &full_periodic_closed_dir, &full_periodic_outer_index);
    if (!full_periodic_face)
	fast_reconstruct_periodic_strip(s, face, brep_loop_points,
	    model_diagonal);

    // process through loops building polygons.
    std::vector<detria::PointD> tpnts;
    std::vector<int> outer_polyline;
    std::vector<std::vector<int>> holes;
    std::unordered_map<int, BrepTrimPoint *> pointmap;
    std::unordered_map<int, size_t> pind_map;
    bool have_outer = false;

    for (int li = 0; li < loop_cnt; li++) {
	const ON_BrepLoop *face_loop = face.Loop(li);
	if (!face_loop)
	    continue;
	std::vector<int> polyline;
	int num_loop_points = brep_loop_points[li]->Count();
	if (num_loop_points <= 2)
	    continue;
	const bool uv_closed = fast_loop_uv_closed(s, face_loop,
	    *brep_loop_points[li], tol);
	const int first_point = uv_closed ? 1 : 0;
	for (int i = first_point; i < num_loop_points; i++) {
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
	if (!uv_closed) {
	    ON_Line *line = new ON_Line(
		(*brep_loop_points[li])[num_loop_points - 1].p2d,
		(*brep_loop_points[li])[0].p2d);
	    line_store.lines.push_back(line);
	    ON_BoundingBox bb = line->BoundingBox();
	    bb.m_max.x += ON_ZERO_TOLERANCE;
	    bb.m_max.y += ON_ZERO_TOLERANCE;
	    bb.m_max.z += ON_ZERO_TOLERANCE;
	    bb.m_min.x -= ON_ZERO_TOLERANCE;
	    bb.m_min.y -= ON_ZERO_TOLERANCE;
	    bb.m_min.z -= ON_ZERO_TOLERANCE;
	    rt_trims.Insert2d(bb.Min(), bb.Max(), line);
	}
	if (face_loop->m_type == ON_BrepLoop::outer ||
		(singular_cap_face && li == 0)) {
	    if (have_outer)
		return false;
	    outer_polyline = polyline;
	    have_outer = true;
	} else {
	    holes.push_back(polyline);
	}
    }

    if (!have_outer) {
	std::cerr << "Error: Face(" << fi << ") cannot evaluate its outer loop and will not be facetized." << std::endl;
	return false;
    }

    const size_t boundary_point_count = tpnts.size();

    getSurfacePoints(face, ttol, tol, on_surf_points, metrics,
	model_diagonal);
    if (full_periodic_face) {
	on_surf_points.Empty();
	fast_seed_full_periodic_face(s,
	    *brep_loop_points[full_periodic_outer_index], ttol,
	    model_diagonal, full_periodic_closed_dir, on_surf_points);
    }
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
	    tri_success = false;
	}
    }

    std::vector<int> cleaned_triangles;
    if (!tri_success) {
	/* The direct path deliberately avoids preprocessing overhead.  If its
	 * constraints are invalid, use libbg's Clipper-based sanitizer to merge
	 * duplicate UV points, resolve intersecting loops, and remove surface
	 * samples that lie on constrained edges. */
	std::vector<fastf_t> input_points(tpnts.size() * 2);
	for (size_t i = 0; i < tpnts.size(); i++) {
	    input_points[2 * i] = tpnts[i].x;
	    input_points[2 * i + 1] = tpnts[i].y;
	}
	std::vector<const int *> hole_arrays;
	std::vector<size_t> hole_counts;
	hole_arrays.reserve(holes.size());
	hole_counts.reserve(holes.size());
	for (const std::vector<int> &hole : holes) {
	    hole_arrays.push_back(hole.data());
	    hole_counts.push_back(hole.size());
	}
	std::vector<int> steiner;
	steiner.reserve(tpnts.size() - boundary_point_count);
	for (size_t i = boundary_point_count; i < tpnts.size(); i++)
	    steiner.push_back((int)i);

	int *clean_faces = NULL;
	int clean_face_count = 0;
	point2d_t *clean_points = NULL;
	int clean_point_count = 0;
	const int clean_ret = bg_nested_poly_triangulate_clean(&clean_faces,
	    &clean_face_count, &clean_points, &clean_point_count,
	    outer_polyline.data(), outer_polyline.size(),
	    hole_arrays.empty() ? NULL : hole_arrays.data(),
	    hole_counts.empty() ? NULL : hole_counts.data(), holes.size(),
	    steiner.empty() ? NULL : steiner.data(), steiner.size(),
	    (const point2d_t *)input_points.data(), tpnts.size());
	if (clean_ret != BRLCAD_OK || !clean_faces || !clean_points ||
		clean_face_count <= 0 || clean_point_count <= 0 ||
		clean_point_count >= FAST_CDT_MAX_SURFACE_SAMPLES) {
	    if (clean_faces)
		bu_free(clean_faces, "clipped detria faces");
	    if (clean_points)
		bu_free(clean_points, "clipped detria points");
	    return false;
	}

	cleaned_triangles.assign(clean_faces,
	    clean_faces + (size_t)clean_face_count * 3);
	std::unordered_map<int, BrepTrimPoint *> clean_pointmap;
	std::unordered_map<int, size_t> clean_pind_map;
	double uv_min[2] = {INFINITY, INFINITY};
	double uv_max[2] = {-INFINITY, -INFINITY};
	for (const detria::PointD &point : tpnts) {
	    uv_min[X] = std::min(uv_min[X], point.x);
	    uv_min[Y] = std::min(uv_min[Y], point.y);
	    uv_max[X] = std::max(uv_max[X], point.x);
	    uv_max[Y] = std::max(uv_max[Y], point.y);
	}
	const double uv_match_tol = std::max(BREP_SAME_POINT_TOLERANCE,
	    4.0 * std::max(uv_max[X] - uv_min[X],
		uv_max[Y] - uv_min[Y]) / CLIPPER_MAX);
	for (int i = 0; i < clean_point_count; i++) {
	    for (size_t j = 0; j < boundary_point_count; j++) {
		if (!NEAR_EQUAL(clean_points[i][X], tpnts[j].x,
			uv_match_tol) ||
			!NEAR_EQUAL(clean_points[i][Y], tpnts[j].y,
			uv_match_tol))
		    continue;
		auto pfound = pointmap.find((int)j);
		if (pfound != pointmap.end())
		    clean_pointmap[i] = pfound->second;
		auto ifound = pind_map.find((int)j);
		if (ifound != pind_map.end())
		    clean_pind_map[i] = ifound->second;
		break;
	    }
	}
	tpnts.clear();
	tpnts.reserve((size_t)clean_point_count);
	for (int i = 0; i < clean_point_count; i++) {
	    detria::PointD p;
	    p.x = clean_points[i][X];
	    p.y = clean_points[i][Y];
	    tpnts.push_back(p);
	}
	bu_free(clean_faces, "clipped detria faces");
	bu_free(clean_points, "clipped detria points");

	/* Sanitization may replace and reorder boundary points.  Retain exact
	 * edge samples where possible; Clipper-created intersections are realized
	 * from their cleaned surface parameters. */
	faces.clear();
	pnt_norms.clear();
	pind_map.swap(clean_pind_map);
	pointmap.swap(clean_pointmap);
    }

    std::map<int, size_t> singular_pind_map;
    std::vector<std::pair<ON_3dPoint, size_t>> seam_pind_map;
    auto emit_triangle = [&](int t0, int t1, int t2) {
	const int tris[3] = {t0, t1, t2};
	ON_3dPoint triangle_points[3];
	ON_3dVector triangle_normals[3];
	ON_2dPoint triangle_uvs[3];
	for (size_t j = 0; j < 3; j++) {
	    std::unordered_map<int, BrepTrimPoint *>::const_iterator bt_it =
		pointmap.find(tris[j]);
	    triangle_uvs[j] = UnwrapUVPoint(s,
		ON_2dPoint(tpnts[tris[j]].x, tpnts[tris[j]].y),
		BREP_SAME_POINT_TOLERANCE);
	    if (!surface_EvNormal(s, triangle_uvs[j].x, triangle_uvs[j].y,
		    triangle_points[j], triangle_normals[j])) {
		if (bt_it == pointmap.end() || !bt_it->second->p3d ||
			bt_it->second->from_singular < 0 ||
			!bt_it->second->normal.IsValid())
		    return;
		triangle_points[j] = *bt_it->second->p3d;
		triangle_normals[j] = bt_it->second->normal;
	    }
	}
	const ON_3dVector cross = ON_CrossProduct(
	    triangle_points[1] - triangle_points[0],
	    triangle_points[2] - triangle_points[0]);
	const double cross_length = cross.Length();
	if (!std::isfinite(cross_length) ||
		cross_length <= std::numeric_limits<double>::min())
	    return;

	for (size_t j = 0; j < 3; j++) {
	    const int singular_side = IsAtSingularity(s, triangle_uvs[j],
		BREP_SAME_POINT_TOLERANCE);
	    const bool seam_point = IsAtSeam(s, triangle_uvs[j],
		BREP_SAME_POINT_TOLERANCE) > 0;
	    std::unordered_map<int, size_t>::const_iterator existing =
		pind_map.find(tris[j]);
	    std::map<int, size_t>::const_iterator singular =
		singular_pind_map.find(singular_side);
	    size_t point_index = SIZE_MAX;
	    if (singular_side >= 0 && singular != singular_pind_map.end()) {
		point_index = singular->second;
	    } else if (seam_point) {
		const double seam_tolerance = std::max(ON_ZERO_TOLERANCE,
		    std::max(model_diagonal, 1.0) * 1.0e-10);
		for (const std::pair<ON_3dPoint, size_t> &seam :
			seam_pind_map) {
		    if (seam.first.DistanceTo(triangle_points[j]) <=
			    seam_tolerance) {
			point_index = seam.second;
			break;
		    }
		}
	    }
	    if (point_index != SIZE_MAX) {
		faces.push_back(point_index);
	    } else if (existing != pind_map.end()) {
		point_index = existing->second;
		faces.push_back(point_index);
		if (singular_side >= 0)
		    singular_pind_map[singular_side] = point_index;
	    } else {
		pnts.push_back(triangle_points[j].x);
		pnts.push_back(triangle_points[j].y);
		pnts.push_back(triangle_points[j].z);
		pind_map[tris[j]] = pnts.size()/3 - 1;
		point_index = pind_map[tris[j]];
		faces.push_back(point_index);
		if (singular_side >= 0)
		    singular_pind_map[singular_side] = point_index;
	    }
	    if (seam_point && point_index != SIZE_MAX) {
		bool recorded = false;
		for (const std::pair<ON_3dPoint, size_t> &seam :
			seam_pind_map)
		    recorded = recorded || seam.second == point_index;
		if (!recorded)
		    seam_pind_map.push_back(std::make_pair(
			triangle_points[j], point_index));
	    }

	    std::unordered_map<int, BrepTrimPoint *>::const_iterator bt_it =
		pointmap.find(tris[j]);
	    if (bt_it != pointmap.end() && bt_it->second->n3d)
		triangle_normals[j] = *(bt_it->second->n3d);
	    if (face.m_bRev)
		triangle_normals[j] = triangle_normals[j] * -1.0;
	    pnt_norms.push_back(triangle_normals[j].x);
	    pnt_norms.push_back(triangle_normals[j].y);
	    pnt_norms.push_back(triangle_normals[j].z);
	}
    };

    if (tri_success) {
	tri.forEachTriangle([&](const detria::Triangle<int> triangle) {
	    emit_triangle(triangle.x, triangle.y, triangle.z);
	}, true);
    } else {
	for (size_t i = 0; i < cleaned_triangles.size(); i += 3)
	    emit_triangle(cleaned_triangles[i], cleaned_triangles[i + 1],
		cleaned_triangles[i + 2]);
    }

    return !faces.empty() && faces.size() % 3 == 0 &&
	pnts.size() % 3 == 0 && pnt_norms.size() == faces.size() * 3;
}

enum fast_face_outcome {
    FAST_FACE_FAILED = 0,
    FAST_FACE_COMPLETED,
    FAST_FACE_SKIPPED_DEGENERATE
};

static bool
fast_face_is_provably_degenerate(const ON_BrepFace &face)
{
    if (face.LoopCount() != 1)
	return false;
    const ON_BrepLoop *loop = face.Loop(0);
    const ON_Surface *surface = face.SurfaceOf();
    if (!loop || !surface || loop->TrimCount() < 1)
	return false;
    if (loop->TrimCount() > 1) {
	/* Collinear trim loops have exactly zero parametric area.  Closed
	 * surfaces are excluded because a coincident seam pair can bound an
	 * entire surface on the quotient domain. */
	if (surface->IsClosed(0) || surface->IsClosed(1))
	    return false;
	std::vector<ON_2dPoint> endpoints;
	for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	    const ON_BrepTrim *candidate = loop->Trim(ti);
	    if (!candidate || candidate->m_type == ON_BrepTrim::singular ||
		    candidate->Degree() != 1 || candidate->SpanCount() != 1)
		return false;
	    const ON_Interval domain = candidate->Domain();
	    const ON_2dPoint start = candidate->PointAt(domain.Min());
	    const ON_2dPoint end = candidate->PointAt(domain.Max());
	    if (!start.IsValid() || !end.IsValid())
		return false;
	    endpoints.push_back(start);
	    endpoints.push_back(end);
	}
	const double parameter_scale = std::max(1.0,
	    std::max(surface->Domain(0).Length(),
		surface->Domain(1).Length()));
	const double parameter_tolerance = std::max(
	    BREP_SAME_POINT_TOLERANCE, parameter_scale * 1.0e-9);
	size_t direction_index = 1;
	while (direction_index < endpoints.size() &&
		endpoints[0].DistanceTo(endpoints[direction_index]) <=
		parameter_tolerance)
	    direction_index++;
	if (direction_index == endpoints.size())
	    return true;
	const ON_2dVector direction =
	    endpoints[direction_index] - endpoints[0];
	const double direction_length = direction.Length();
	for (const ON_2dPoint &point : endpoints) {
	    const ON_2dVector offset = point - endpoints[0];
	    const double cross = direction.x * offset.y -
		direction.y * offset.x;
	    if (fabs(cross) > parameter_tolerance * direction_length)
		return false;
	}
	return true;
    }
    const ON_BrepTrim *trim = loop->Trim(0);
    if (!trim || trim->m_type == ON_BrepTrim::singular ||
	    trim->m_vi[0] != trim->m_vi[1] || trim->Degree() != 1 ||
	    trim->SpanCount() != 1)
	return false;

    const ON_Interval domain = trim->Domain();
    const ON_2dPoint start = trim->PointAt(domain.Min());
    const ON_2dPoint end = trim->PointAt(domain.Max());
    if (!start.IsValid() || !end.IsValid() ||
	    V2NEAR_EQUAL(start, end, BREP_SAME_POINT_TOLERANCE))
	return false;

    /* A straight segment can represent a closed curve on a quotient surface
     * when its endpoints differ by one complete period.  Those loops bound
     * real caps or bands and must reach the periodic reconstruction path. */
    for (int dir = 0; dir < 2; ++dir) {
	if (!surface->IsClosed(dir))
	    continue;
	const double period = surface->Domain(dir).Length();
	const double delta = end[dir] - start[dir];
	const double parameter_tolerance = std::max(
	    BREP_SAME_POINT_TOLERANCE, period * 1.0e-4);
	if (period > ON_ZERO_TOLERANCE &&
		fabs(fabs(delta) - period) <= parameter_tolerance)
	    return false;
    }
    return true;
}

static fast_face_outcome
bg_CDT(std::vector<int> &faces, std::vector<fastf_t> &pnt_norms,
	std::vector<fastf_t> &pnts, const ON_BrepFace &face,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol,
	double model_diagonal)
{
    if (fast_face_is_provably_degenerate(face))
	return FAST_FACE_SKIPPED_DEGENERATE;
    if (bg_CDT_attempt(faces, pnt_norms, pnts, face, ttol, tol,
	    model_diagonal, false))
	return FAST_FACE_COMPLETED;

    faces.clear();
    pnt_norms.clear();
    pnts.clear();
    return bg_CDT_attempt(faces, pnt_norms, pnts, face, ttol, tol,
	model_diagonal, true) ? FAST_FACE_COMPLETED : FAST_FACE_FAILED;
}


struct fast_cdt_face_result {
    std::vector<int> faces;
    std::vector<fastf_t> norms;
    std::vector<fastf_t> pnts;
    bool completed = false;
    bool failed = false;
    bool skipped_degenerate = false;
};

struct fast_cdt_parallel_state {
    const ON_Brep *brep;
    const struct bg_tess_tol *ttol;
    const struct bn_tol *tol;
    double model_diagonal;
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
	fast_face_outcome outcome = FAST_FACE_FAILED;
	try {
	    outcome = bg_CDT(result.faces, result.norms, result.pnts, face,
		state->ttol, state->tol, state->model_diagonal);
	} catch (const std::bad_alloc &) {
	    state->hit_memory_limit = true;
	    state->stop = true;
	} catch (...) {
	    outcome = FAST_FACE_FAILED;
	}
	if (outcome == FAST_FACE_FAILED) {
	    result.failed = true;
	    result.faces.clear();
	    result.norms.clear();
	    result.pnts.clear();
	    if (!state->allow_partial)
		state->stop = true;
	    continue;
	}
	if (outcome == FAST_FACE_SKIPPED_DEGENERATE) {
	    result.completed = true;
	    result.skipped_degenerate = true;
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
    options->face_status = NULL;
    options->face_status_data = NULL;
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
	options.face_status = user_options->face_status;
	options.face_status_data = user_options->face_status_data;
    }
    options.max_workers = std::max((size_t)1,
	std::min(options.max_workers, (size_t)brep_face_count));

    ssize_t available = bu_mem(BU_MEM_AVAIL, NULL);
    if (available > 0)
	options.max_result_bytes = std::min(options.max_result_bytes,
	    (size_t)available / 4);

    const int64_t deadline = options.max_time_ms > 0 ?
	bu_gettime() + (int64_t)options.max_time_ms * 1000 : 0;
    const double model_diagonal = fast_brep_diagonal(brep);

    std::vector<int> all_faces;
    std::vector<fastf_t> all_norms;
    std::vector<fastf_t> all_pnts;

    const int first_face = (index == -1) ? 0 : index;
    const int end_face = (index == -1) ? brep_face_count : index + 1;
    std::vector<fast_cdt_face_result> face_results((size_t)brep_face_count);
    bool hit_time_limit = false;
    bool hit_memory_limit = false;
    bool hit_point_limit = false;

    if (index == -1) {
	/* Each face produces an independent set of indices and points.  Keep
	 * those results separate while workers run, then concatenate them in
	 * face order.  This avoids locking the hot CDT path, keeps output
	 * stable across runs, and leaves the input BRep unchanged. */
	fast_cdt_parallel_state state = {
	    brep, ttol, tol, model_diagonal, &face_results, 0, 0, 0,
	    false, false,
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
	    fast_face_outcome outcome = FAST_FACE_FAILED;
	    try {
		outcome = bg_CDT(result.faces, result.norms, result.pnts,
		    brep->m_F[index], ttol, tol, model_diagonal);
	    } catch (const std::bad_alloc &) {
		hit_memory_limit = true;
	    } catch (...) {
		outcome = FAST_FACE_FAILED;
	    }
	    if (outcome == FAST_FACE_FAILED) {
		result.failed = true;
	    } else if (outcome == FAST_FACE_SKIPPED_DEGENERATE) {
		result.completed = true;
		result.skipped_degenerate = true;
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
    int skipped_degenerate_faces = 0;
    for (int fi = first_face; fi < end_face; fi++) {
	fast_cdt_face_result &result = face_results[(size_t)fi];
	if (!result.completed) {
	    failed_faces++;
	    if (options.face_status)
		options.face_status(fi, result.failed ?
		    BREP_CDT_FAST_FACE_FAILED :
		    BREP_CDT_FAST_FACE_NOT_PROCESSED,
		    options.face_status_data);
	    continue;
	}
	completed_faces++;
	if (result.skipped_degenerate) {
	    skipped_degenerate_faces++;
	    if (options.face_status)
		options.face_status(fi,
		    BREP_CDT_FAST_FACE_SKIPPED_DEGENERATE,
		    options.face_status_data);
	    continue;
	}
	if (options.face_status)
	    options.face_status(fi, BREP_CDT_FAST_FACE_COMPLETED,
		options.face_status_data);
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
	report->skipped_degenerate_faces = skipped_degenerate_faces;
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

    /* A BRep containing only proven zero-area faces has no drawable output,
     * but the request was still handled completely. */
    if (!all_faces.size() || !all_pnts.size() || !all_norms.size())
	return (!failed_faces && completed_faces == skipped_degenerate_faces) ?
	    BREP_CDT_FAST_OK : BREP_CDT_FAST_ERROR;

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
