/*                   F A S T _ F A C E . C P P
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
/** @file cdt/fast_face.cpp
 *
 * Face-local sampling, boundary recovery, chart triangulation, and
 * surface realization for display tessellation.
 *
 */

#include "common.h"

#include <vector>
#include <cmath>
#include <cstdio>
#include <list>
#include <limits>
#include <map>
#include <memory>
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
#include "bu/env.h"

#include "bu/cv.h"
#include "bu/str.h"
#include "bu/datetime.h"
#include "bn/dvec.h"
#include "bg/polygon.h"
#include "brep.h"
#include "./cdt.h"
#include "fast_face.h"
#include "surface.h"

#define YELLOW 255, 255, 0

typedef std::map<double, BrepTrimPoint *> fast_trim_point_map;

/* Sample addresses remain stable while loop copies borrow their geometry.
 * Ownership stays local to the face rather than in the const source BRep. */
struct fast_face_scratch {
    struct owned_sample {
	BrepTrimPoint sample;
	ON_3dPoint point;
	explicit owned_sample(const ON_3dPoint &p) : point(p)
	{
	    sample.p3d = &point;
	}
    };

    std::map<int, fast_trim_point_map> trim_points;
    std::vector<std::unique_ptr<owned_sample>> samples;
    std::vector<std::unique_ptr<ON_3dPoint>> loose_points;
    bool hit_sample_limit = false;
    bool invalid_constraints = false;

    fast_trim_point_map *find(int trim_index)
    {
	auto entry = trim_points.find(trim_index);
	return (entry == trim_points.end()) ? NULL : &entry->second;
    }

    fast_trim_point_map *create(int trim_index)
    {
	return &trim_points[trim_index];
    }

    BrepTrimPoint *make_sample(const ON_3dPoint &point)
    {
	samples.push_back(std::make_unique<owned_sample>(point));
	return &samples.back()->sample;
    }

    ON_3dPoint *make_point(const ON_3dPoint &point)
    {
	loose_points.push_back(std::make_unique<ON_3dPoint>(point));
	return loose_points.back().get();
    }
};

struct fast_recursion_guard {
    int &depth;
    explicit fast_recursion_guard(int &d) : depth(d) { depth++; }
    ~fast_recursion_guard() { depth--; }
};

/* Keep trim segments and their spatial index together: sampled interior
 * points must not duplicate a constrained boundary, including its closing
 * segment when the sampler has not repeated the first UV point. */
class fast_trim_index {
public:
    void add_boundary(const ON_SimpleArray<BrepTrimPoint> &points,
	    bool uv_closed)
    {
	for (int i = 1; i < points.Count(); ++i)
	    add_segment(points[i - 1].p2d, points[i].p2d);
	if (!uv_closed && points.Count())
	    add_segment(points[points.Count() - 1].p2d, points[0].p2d);
    }

    void append_interior(std::vector<detria::PointD> &points,
	    const ON_2dPointArray &samples, double tolerance) const
    {
	for (int i = 0; i < samples.Count(); ++i) {
	    const ON_2dPoint &point = samples[i];
	    ON_SimpleArray<void *> candidates;
	    m_tree.Search2d((const double *)point, (const double *)point,
		candidates);
	    bool on_edge = false;
	    for (int candidate = 0; candidate < candidates.Count(); ++candidate) {
		const ON_Line *line = (const ON_Line *)candidates[candidate];
		if (NEAR_ZERO(line->MinimumDistanceTo(point), tolerance)) {
		    on_edge = true;
		    break;
		}
	    }
	    if (!on_edge)
		points.push_back({point.x, point.y});
	}
    }

    void clear()
    {
	m_tree.RemoveAll();
	m_lines.clear();
    }

private:
    void add_segment(const ON_2dPoint &start, const ON_2dPoint &end)
    {
	m_lines.push_back(std::make_unique<ON_Line>(start, end));
	ON_Line *line = m_lines.back().get();
	ON_BoundingBox bounds = line->BoundingBox();
	for (int axis = 0; axis < 3; ++axis) {
	    bounds.m_max[axis] += ON_ZERO_TOLERANCE;
	    bounds.m_min[axis] -= ON_ZERO_TOLERANCE;
	}
	m_tree.Insert2d(bounds.Min(), bounds.Max(), line);
    }

    std::vector<std::unique_ptr<ON_Line>> m_lines;
    ON_RTree m_tree;
};

using fast_bridge_samples = std::map<double, ON_3dPoint>;

struct fast_bridge_store {
    std::vector<std::unique_ptr<fast_bridge_samples>> maps;
    bool invalid = false;

    fast_bridge_samples *create()
    {
	maps.push_back(std::make_unique<fast_bridge_samples>());
	return maps.back().get();
    }
};

class fast_loop_point_store {
public:
    explicit fast_loop_point_store(int count) : m_loops((size_t)count)
    {
	m_points.reserve(m_loops.size());
	for (auto &loop : m_loops)
	    m_points.push_back(&loop);
    }

    fast_loop_point_store(const fast_loop_point_store &) = delete;
    fast_loop_point_store &operator=(const fast_loop_point_store &) = delete;

    ON_SimpleArray<BrepTrimPoint> **data() { return m_points.data(); }

private:
    std::vector<ON_SimpleArray<BrepTrimPoint>> m_loops;
    std::vector<ON_SimpleArray<BrepTrimPoint> *> m_points;
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

double
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

    int etrim = (trim.EvTangent(t, mid_2d, mid_tang) &&
	cdt_surface_normal(s, mid_2d, mid_3d, mid_norm)) ? 1 : 0;
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
	BrepTrimPoint *nbtp = scratch.make_sample(mid_3d);
	nbtp->n3d = NULL;
	nbtp->p2d = mid_2d;
	nbtp->normal = mid_norm;
	nbtp->tangent = mid_tang;
	nbtp->t = t;
	nbtp->trim_ind = trim.m_trim_index;
	nbtp->edge_ind = trim.m_ei;
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
	    ON_3dPoint seam_3d = cdt_surface_point(s, seam_2d);
	    if (param_points.find(seam_t) == param_points.end()) {
		BrepTrimPoint *nbtp = scratch.make_sample(seam_3d);
		nbtp->n3d = NULL;
		nbtp->p2d = seam_2d;
		nbtp->trim_ind = trim.m_trim_index;
		nbtp->edge_ind = trim.m_ei;
		// Note - by this point we shouldn't need tangents and normals...
		nbtp->t = seam_t;
		param_points[nbtp->t] = nbtp;
	    }
	}
    }
}

static bool
fast_add_trim_sample(const ON_BrepTrim &trim, double t,
	fast_trim_point_map &param_points, fast_face_scratch &scratch)
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
    if (!uv.IsValid() || !cdt_surface_normal(surface, uv, point, normal))
	return false;
    trim.EvTangent(t, uv, tangent);

    BrepTrimPoint *sample = scratch.make_sample(point);
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
	fast_trim_point_map &param_points, fast_face_scratch &scratch)
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
	fast_add_trim_sample(trim, domain.ParameterAt(0.25), param_points, scratch);
	fast_add_trim_sample(trim, domain.ParameterAt(0.50), param_points, scratch);
	fast_add_trim_sample(trim, domain.ParameterAt(0.75), param_points, scratch);
    } else if (param_points.size() < 3) {
	fast_add_trim_sample(trim, domain.Mid(), param_points, scratch);
    }
}

static void
fast_ensure_periodic_winding_samples(const ON_BrepTrim &trim,
	fast_trim_point_map &param_points, fast_face_scratch &scratch)
{
    const ON_Surface *surface = trim.Face() ?
	static_cast<const ON_Surface *>(trim.Face()) : trim.SurfaceOf();
    if (!surface)
	return;

    ON_BoundingBox bbox = ON_BoundingBox::EmptyBoundingBox;
    if (!trim.GetBoundingBox(bbox, false))
	return;
    double maximum_windings = 0.0;
    for (int dir = 0; dir < 2; ++dir) {
	if (!surface->IsClosed(dir))
	    continue;
	const double period = surface->Domain(dir).Length();
	const double extent = bbox.m_max[dir] - bbox.m_min[dir];
	if (!(period > ON_ZERO_TOLERANCE) || !std::isfinite(extent))
	    return;
	maximum_windings = std::max(maximum_windings, extent / period);
    }
    if (maximum_windings <= 0.5)
	return;
    if (maximum_windings > 64.0) {
	scratch.hit_sample_limit = true;
	return;
    }

    /* Midpoint flatness tests can alias an integral number of turns: the
     * endpoints and midpoint of a many-turn helix may be collinear even
     * though the curve is not.  Guarantee angular coverage independently of
     * that geometric recursion. */
    const size_t segment_count = std::max((size_t)8,
	(size_t)ceil(maximum_windings * 16.0));
    if (segment_count + 1 >= FAST_CDT_MAX_TRIM_SAMPLES) {
	scratch.hit_sample_limit = true;
	return;
    }
    const ON_Interval domain = trim.Domain();
    if (!domain.IsIncreasing())
	return;
    for (size_t sample = 1; sample < segment_count; ++sample) {
	if (param_points.size() >= FAST_CDT_MAX_TRIM_SAMPLES) {
	    scratch.hit_sample_limit = true;
	    return;
	}
	fast_add_trim_sample(trim,
	    domain.ParameterAt((double)sample / (double)segment_count),
	    param_points, scratch);
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
fast_collapsed_closed_pcurve(const ON_BrepLoop *loop,
	const ON_BrepTrim &trim,
	const struct bn_tol *tol, double model_diagonal)
{
    if (!loop || trim.m_type == ON_BrepTrim::singular || !trim.Edge() ||
	    !trim.Edge()->EdgeCurveOf() || loop->TrimCount() != 1 ||
	    trim.m_vi[0] != trim.m_vi[1])
	return false;

    /* A closed 3D edge represented by a collapsed singleton pcurve is a
     * recurring STEP conversion failure.  Compare the spatial extents of
     * the pcurve lifted onto the surface and the edge itself.  This avoids
     * relying on the (often equally corrupt) declared trim tolerance. */
    const ON_Surface *surface = trim.Face() ?
	static_cast<const ON_Surface *>(trim.Face()) : trim.SurfaceOf();
    const ON_Curve *edge_curve = trim.Edge()->EdgeCurveOf();
    const ON_Interval trim_domain = trim.Domain();
    const ON_Interval edge_domain = edge_curve->Domain();
    if (!surface || !trim_domain.IsIncreasing() ||
	    !edge_domain.IsIncreasing())
	return false;

    const ON_2dPoint start_uv = trim.PointAt(trim_domain.Min());
    const ON_3dPoint trim_start = cdt_surface_point(surface, start_uv);
    const ON_3dPoint edge_start = edge_curve->PointAt(edge_domain.Min());
    double trim_extent = 0.0;
    double edge_extent = 0.0;
    bool samples_valid = trim_start.IsValid() && edge_start.IsValid();
    for (int si = 1; samples_valid && si <= 16; ++si) {
	const double fraction = (double)si / 16.0;
	const ON_2dPoint uv = trim.PointAt(
	    trim_domain.ParameterAt(fraction));
	const ON_3dPoint trim_point = cdt_surface_point(surface, uv);
	const ON_3dPoint edge_point = edge_curve->PointAt(
	    edge_domain.ParameterAt(fraction));
	samples_valid = uv.IsValid() && trim_point.IsValid() &&
	    edge_point.IsValid();
	if (samples_valid) {
	    trim_extent = std::max(trim_extent,
		trim_start.DistanceTo(trim_point));
	    edge_extent = std::max(edge_extent,
		edge_start.DistanceTo(edge_point));
	}
    }

    const double model_floor = std::max(tol->dist,
	std::max(model_diagonal, 1.0) * 1.0e-10);
    return samples_valid && edge_extent > 16.0 * model_floor &&
	trim_extent < 0.01 * edge_extent;
}

static bool
fast_pcurve_edge_mismatch(const ON_BrepTrim &trim,
	const struct bn_tol *tol, double model_diagonal)
{
    if (!tol)
	return false;
    const ON_Surface *surface = trim.Face() ?
	static_cast<const ON_Surface *>(trim.Face()) : trim.SurfaceOf();
    const ON_BrepEdge *edge = trim.Edge();
    const ON_Curve *edge_curve = edge ? edge->EdgeCurveOf() : NULL;
    const ON_Interval trim_domain = trim.Domain();
    const ON_Interval edge_domain = edge_curve ? edge_curve->Domain() :
	ON_Interval::EmptyInterval;
    if (!surface || !edge_curve || !trim_domain.IsIncreasing() ||
	    !edge_domain.IsIncreasing())
	return false;

    const double mismatch_tolerance = std::max(8.0 * tol->dist,
	std::max(model_diagonal, 1.0) * 1.0e-5);
    for (int sample = 0; sample <= 8; ++sample) {
	double fraction = (double)sample / 8.0;
	const ON_2dPoint uv = trim.PointAt(
	    trim_domain.ParameterAt(fraction));
	if (trim.m_bRev3d)
	    fraction = 1.0 - fraction;
	const ON_3dPoint edge_point = edge_curve->PointAt(
	    edge_domain.ParameterAt(fraction));
	const ON_2dPoint surface_uv = UnwrapUVPoint(surface, uv,
	    BREP_SAME_POINT_TOLERANCE);
	const ON_3dPoint surface_point = surface->PointAt(surface_uv.x,
	    surface_uv.y);
	if (!uv.IsValid() || !edge_point.IsValid() ||
		!surface_point.IsValid())
	    return false;
	if (edge_point.DistanceTo(surface_point) > mismatch_tolerance)
	    return true;
    }
    return false;
}

static bool
fast_pullback_candidate(const ON_BrepLoop *loop, const ON_BrepTrim &trim,
	const struct bn_tol *tol, double model_diagonal)
{
    if (!loop || trim.m_type == ON_BrepTrim::singular || !trim.Edge() ||
	    !trim.Edge()->EdgeCurveOf() || !std::isfinite(trim.m_tolerance[0]) ||
	    trim.m_tolerance[0] <= 0.0)
	return false;

    if (fast_collapsed_closed_pcurve(loop, trim, tol, model_diagonal))
	return true;

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

template <typename Point>
static double
fast_point_segment_distance(const Point &point, const Point &start,
	const Point &end)
{
    const auto segment = end - start;
    const double length_squared = segment * segment;
    if (!(length_squared > 0.0) || !std::isfinite(length_squared))
	return point.DistanceTo(start);
    const double fraction = std::max(0.0, std::min(1.0,
	((point - start) * segment) / length_squared));
    return point.DistanceTo(start + fraction * segment);
}

static double
fast_scaled_uv_segment_distance(const ON_2dPoint &point,
	const ON_2dPoint &start, const ON_2dPoint &end,
	double u_scale, double v_scale)
{
    const ON_2dPoint scaled_point(point.x * u_scale, point.y * v_scale);
    const ON_2dPoint scaled_start(start.x * u_scale, start.y * v_scale);
    const ON_2dPoint scaled_end(end.x * u_scale, end.y * v_scale);
    return fast_point_segment_distance(scaled_point, scaled_start,
	scaled_end);
}

/* Pullback repair can expose every knot of an already approximated edge even
 * when most are redundant at its declared tolerance.  A model-space chord
 * bound protects geometric fidelity, while a surface-scaled UV bound avoids
 * introducing a topologically distant constraint on distorted surfaces. */
static void
fast_simplify_pullback_samples(const ON_Surface *surface,
	std::vector<ON_2dPoint> &samples,
	const std::vector<ON_3dPoint> &model_points, double tolerance)
{
    if (!surface || samples.size() < 3 ||
	    samples.size() != model_points.size() ||
	    !(tolerance > 0.0) || !std::isfinite(tolerance))
	return;

    fast_surface_metrics metrics;
    fast_surface_metrics_get(surface, &metrics);
    if (!metrics.sampling_valid || !std::isfinite(metrics.u_scale) ||
	    !std::isfinite(metrics.v_scale) || !(metrics.u_scale > 0.0) ||
	    !(metrics.v_scale > 0.0))
	return;

    const size_t last = samples.size() - 1;
    const bool closed = samples.size() >= 4 &&
	samples.front().DistanceTo(samples.back()) <=
	    BREP_SAME_POINT_TOLERANCE &&
	model_points.front().DistanceTo(model_points.back()) <= tolerance;
    std::vector<bool> keep(samples.size(), false);
    keep[0] = true;
    keep[last] = true;

    std::vector<std::pair<size_t, size_t>> ranges;
    if (closed) {
	size_t opposite = 0;
	double greatest_distance = 0.0;
	for (size_t i = 1; i < last; ++i) {
	    const double distance = std::max(
		model_points[i].DistanceTo(model_points[0]),
		fast_scaled_uv_segment_distance(samples[i], samples[0],
		    samples[0], metrics.u_scale, metrics.v_scale));
	    if (distance > greatest_distance) {
		greatest_distance = distance;
		opposite = i;
	    }
	}
	if (!opposite)
	    return;
	keep[opposite] = true;
	ranges.push_back(std::make_pair((size_t)0, opposite));
	ranges.push_back(std::make_pair(opposite, last));
    } else {
	ranges.push_back(std::make_pair((size_t)0, last));
    }

    while (!ranges.empty()) {
	const std::pair<size_t, size_t> range = ranges.back();
	ranges.pop_back();
	if (range.second <= range.first + 1)
	    continue;
	size_t split = range.first;
	double greatest_error = 0.0;
	for (size_t i = range.first + 1; i < range.second; ++i) {
	    const double error = std::max(
		fast_point_segment_distance(model_points[i],
		    model_points[range.first], model_points[range.second]),
		fast_scaled_uv_segment_distance(samples[i],
		    samples[range.first], samples[range.second],
		    metrics.u_scale, metrics.v_scale));
	    if (error > greatest_error) {
		greatest_error = error;
		split = i;
	    }
	}
	if (greatest_error <= tolerance || split == range.first)
	    continue;
	keep[split] = true;
	ranges.push_back(std::make_pair(range.first, split));
	ranges.push_back(std::make_pair(split, range.second));
    }

    std::vector<ON_2dPoint> simplified_samples;
    simplified_samples.reserve(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
	if (!keep[i])
	    continue;
	simplified_samples.push_back(samples[i]);
    }
    const size_t minimum_count = closed ? 4 : 2;
    if (simplified_samples.size() < minimum_count)
	return;
    samples.swap(simplified_samples);
}

static bool
fast_append_surface_samples(ON_SimpleArray<BrepTrimPoint> &points,
	const ON_BrepTrim &trim, const std::vector<ON_2dPoint> &samples,
	bool omit_last, fast_face_scratch &scratch)
{
    const ON_Surface *surface = trim.SurfaceOf();
    if (!surface || samples.size() < 2)
	return false;
    const size_t append_count = samples.size() - (omit_last ? 1 : 0);
    ON_SimpleArray<BrepTrimPoint> candidate;
    for (size_t i = 0; i < append_count; ++i) {
	const ON_2dPoint uv = UnwrapUVPoint(surface, samples[i],
	    BREP_SAME_POINT_TOLERANCE);
	ON_3dPoint point = ON_3dPoint::UnsetPoint;
	ON_3dVector normal = ON_3dVector::UnsetVector;
	if (!surface_EvNormal(surface, uv.x, uv.y, point, normal))
	    return false;
	BrepTrimPoint sample = {};
	sample.p3d = scratch.make_point(point);
	sample.n3d = NULL;
	sample.p2d = samples[i];
	sample.normal = normal;
	sample.tangent = ON_3dVector::UnsetVector;
	sample.t = ON_UNSET_VALUE;
	sample.e = ON_UNSET_VALUE;
	sample.trim_ind = trim.m_trim_index;
	sample.edge_ind = trim.m_ei;
	candidate.Append(sample);
    }
    points.Append(candidate.Count(), candidate.Array());
    return true;
}

static bool
fast_append_pullback_samples(ON_SimpleArray<BrepTrimPoint> *points,
	const ON_BrepTrim &trim, bool omit_last, const struct bn_tol *tol,
	double model_diagonal, fast_face_scratch &scratch, bool preserve_samples)
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

    /* Display recovery rejects the whole pullback after any source mismatch.
     * Further global projections cannot make that candidate usable. */
    std::unique_ptr<PBCData, decltype(&fast_free_pullback)> data(
	pullback_samples(surface, edge_curve, repair_tolerance,
	1.0e-3, BREP_SAME_POINT_TOLERANCE, repair_tolerance,
	std::shared_ptr<brlcad::PullbackContext>(), true), fast_free_pullback);
    if (!data || !data->samples_source_validated || !data->segments ||
	    data->segments->empty() || data->rejected_projection_samples)
	return false;

    std::vector<ON_2dPoint> samples;
    for (const ON_2dPointArray *segment : *data->segments) {
	if (!segment)
	    continue;
	for (int i = 0; i < segment->Count(); ++i) {
	    const ON_2dPoint sample = (*segment)[i];
	    if (!sample.IsValid())
		return false;
	    if (samples.empty() || !V2NEAR_EQUAL(samples.back(), sample,
		    BREP_SAME_POINT_TOLERANCE))
		samples.push_back(sample);
	}
    }
    data.reset();
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
    const bool collapsed_pcurve = fast_collapsed_closed_pcurve(trim.Loop(),
	trim, tol, model_diagonal);
    if (reverse_error < forward_error || (collapsed_pcurve &&
	    trim.m_bRev3d))
	std::reverse(samples.begin(), samples.end());
    if (!collapsed_pcurve && std::min(forward_error, reverse_error) >
	    2.0 * repair_tolerance)
	return false;

    std::vector<ON_3dPoint> lifted_points;
    lifted_points.reserve(samples.size());
    for (size_t i = 0; i < samples.size(); ++i) {
	const ON_2dPoint uv = UnwrapUVPoint(surface, samples[i],
	    BREP_SAME_POINT_TOLERANCE);
	const ON_3dPoint point = surface->PointAt(uv.x, uv.y);
	if (!point.IsValid())
	    return false;
	lifted_points.push_back(point);
    }
    if (!preserve_samples)
	fast_simplify_pullback_samples(surface, samples, lifted_points,
	    repair_tolerance);

    return fast_append_surface_samples(*points, trim, samples, omit_last,
	scratch);
}

/* A polyline pcurve can interpolate across a jump between periodic images.
 * Translating complete segments preserves their surface locus; only the
 * segments across a jump need a new geometric interpretation.  Accept those
 * segments only when their lifted samples agree with the existing 3-D edge. */
static std::unique_ptr<ON_Curve>
fast_unwrap_periodic_polyline(const ON_BrepTrim &trim,
	const struct bn_tol *tol, double model_diagonal)
{
    const ON_Surface *surface = trim.SurfaceOf();
    const ON_BrepEdge *edge = trim.Edge();
    const ON_PolylineCurve *source = ON_PolylineCurve::Cast(trim.TrimCurveOf());
    if (!surface || !edge || !source || source->PointCount() < 3 ||
	(size_t)source->PointCount() > FAST_CDT_MAX_TRIM_SAMPLES)
	return nullptr;

    const double periods[2] = {
	surface->IsPeriodic(0) ? surface->Domain(0).Length() : 0.0,
	surface->IsPeriodic(1) ? surface->Domain(1).Length() : 0.0
    };
    bool crosses_seam = false;
    for (int dir = 0; dir < 2; ++dir) {
	if (!(periods[dir] > ON_ZERO_TOLERANCE) || !std::isfinite(periods[dir]))
	    continue;
	for (int pi = 1; pi < source->PointCount(); ++pi)
	    crosses_seam = crosses_seam || fabs(source->m_pline[pi][dir] -
		source->m_pline[pi - 1][dir]) > 0.5 * periods[dir];
    }
    if (!crosses_seam)
	return nullptr;

    std::unique_ptr<ON_Curve> curve(trim.DuplicateCurve());
    ON_PolylineCurve *polyline = ON_PolylineCurve::Cast(curve.get());
    if (!polyline || polyline->PointCount() < 3 ||
	polyline->m_t.Count() != polyline->PointCount())
	return nullptr;
    std::vector<bool> changed((size_t)polyline->PointCount(), false);
    for (int dir = 0; dir < 2; ++dir) {
	const double period = periods[dir];
	if (!(period > ON_ZERO_TOLERANCE) || !std::isfinite(period))
	    continue;
	double previous_shift = 0.0;
	for (int pi = 1; pi < polyline->PointCount(); ++pi) {
	    const double delta = polyline->m_pline[pi - 1][dir] -
		polyline->m_pline[pi][dir];
	    const double shift = std::round(delta / period) * period;
	    const double half_period_guard = period * ON_SQRT_EPSILON;
	    if (!std::isfinite(shift) ||
		fabs(fabs(delta - shift) - 0.5 * period) <= half_period_guard)
		return nullptr;
	    changed[(size_t)pi] = changed[(size_t)pi] ||
		fabs(shift - previous_shift) > half_period_guard;
	    polyline->m_pline[pi][dir] += shift;
	    previous_shift = shift;
	}
    }
    polyline->DestroyRuntimeCache();
    if (!polyline->IsValid())
	return nullptr;

    const ON_Interval domain = polyline->Domain();
    const ON_Interval edge_domain = edge->Domain();
    const double distance_cap = std::max(16.0 * tol->dist,
	std::max(model_diagonal, 1.0) * 1.0e-4);
    const double tolerance = std::min(distance_cap,
	std::max(tol->dist, trim.m_tolerance[0]));
    if (!domain.IsIncreasing() || !edge_domain.IsIncreasing() ||
	!std::isfinite(tolerance) || !(tolerance > 0.0))
	return nullptr;

    const int validation_segments = 16;
    for (int pi = 1; pi < polyline->PointCount(); ++pi) {
	if (!changed[(size_t)pi])
	    continue;
	const ON_Interval interval(polyline->m_t[pi - 1], polyline->m_t[pi]);
	if (!interval.IsIncreasing())
	    return nullptr;
	for (int sample = 0; sample <= validation_segments; ++sample) {
	    const double t = interval.ParameterAt((double)sample /
		validation_segments);
	    double fraction = domain.NormalizedParameterAt(t);
	    if (trim.m_bRev3d)
		fraction = 1.0 - fraction;
	    const ON_2dPoint uv = UnwrapUVPoint(surface, polyline->PointAt(t),
		BREP_SAME_POINT_TOLERANCE);
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    const ON_3dPoint reference = edge->PointAt(
		edge_domain.ParameterAt(fraction));
	    if (!lift.IsValid() || !reference.IsValid() ||
		lift.DistanceTo(reference) > tolerance)
		return nullptr;
	}
    }
    return curve;
}

static bool
fast_append_periodic_polyline(ON_SimpleArray<BrepTrimPoint> &points,
	const ON_BrepTrim &trim, bool omit_last, const struct bn_tol *tol,
	double model_diagonal, fast_face_scratch &scratch, bool preserve_samples)
{
    std::unique_ptr<ON_Curve> curve = fast_unwrap_periodic_polyline(trim,
	tol, model_diagonal);
    const ON_PolylineCurve *polyline = ON_PolylineCurve::Cast(curve.get());
    if (!polyline)
	return false;
    const ON_Surface *surface = trim.SurfaceOf();
    std::vector<ON_2dPoint> samples;
    std::vector<ON_3dPoint> lifted_points;
    for (int i = 0; i < polyline->PointCount(); ++i) {
	const ON_2dPoint uv = UnwrapUVPoint(surface, polyline->m_pline[i],
	    BREP_SAME_POINT_TOLERANCE);
	const ON_3dPoint point = surface->PointAt(uv.x, uv.y);
	if (!point.IsValid())
	    return false;
	samples.push_back(polyline->m_pline[i]);
	lifted_points.push_back(point);
    }
    if (!preserve_samples)
	fast_simplify_pullback_samples(surface, samples, lifted_points, tol->dist);
    return fast_append_surface_samples(points, trim, samples, omit_last,
	scratch);
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
    /* UV extents do not measure model-space tessellation error.  A helical
     * trim may span dozens of periods in parameter space while occupying a
     * small model-space box; scaling its tolerance from the UV box can reduce
     * the entire helix to its endpoints. */
    const ON_BrepEdge *metric_edge = trim.Edge();
    const ON_Curve *metric_curve = metric_edge ?
	metric_edge->EdgeCurveOf() : NULL;
    if (metric_curve && metric_curve->GetBoundingBox(min, max,
	    bGrowBox)) {
	dist = DIST_PNT_PNT(min, max);
    } else if (trim.GetBoundingBox(min, max, bGrowBox)) {
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
    evals += cdt_surface_normal(s, start_2d, start_3d, start_norm) ? 1 : 0;
    evals += cdt_surface_normal(s, end_2d, end_3d, end_norm) ? 1 : 0;

    if (evals != 4) {
	start_2d = trim.PointAt(range.m_t[0]);
	end_2d = trim.PointAt(range.m_t[1]);
	start_3d = cdt_surface_point(s, start_2d);
	end_3d = cdt_surface_point(s, end_2d);
	evals = 4;
    }

    BrepTrimPoint *sbtp = scratch.make_sample(start_3d);
    sbtp->n3d = NULL;
    sbtp->p2d = start_2d;
    sbtp->tangent = start_tang;
    sbtp->normal = start_norm;
    sbtp->t = range.m_t[0];
    sbtp->trim_ind = trim.m_trim_index;
    sbtp->edge_ind = trim.m_ei;
    (*param_points)[sbtp->t] = sbtp;

    BrepTrimPoint *ebtp = scratch.make_sample(end_3d);
    ebtp->n3d = NULL;
    ebtp->p2d = end_2d;
    ebtp->tangent = end_tang;
    ebtp->normal = end_norm;
    ebtp->t = range.m_t[1];
    ebtp->trim_ind = trim.m_trim_index;
    ebtp->edge_ind = trim.m_ei;
    (*param_points)[ebtp->t] = ebtp;


    if (trim.IsClosed()) {

	double mid_range = (range.m_t[0] + range.m_t[1]) / 2.0;
	ON_3dPoint mid_2d(0.0, 0.0, 0.0);
	ON_3dPoint mid_3d(0.0, 0.0, 0.0);
	ON_3dVector mid_tang(0.0, 0.0, 0.0);
	ON_3dVector mid_norm(0.0, 0.0, 0.0);

	evals += (trim.EvTangent(mid_range, mid_2d, mid_tang)) ? 1 : 0;
	evals += cdt_surface_normal(s, mid_2d, mid_3d, mid_norm) ? 1 : 0;
	if (evals != 6) {
	    mid_2d = trim.PointAt(mid_range);
	    mid_3d = cdt_surface_point(s, mid_2d);
	}

	BrepTrimPoint *mbtp = scratch.make_sample(mid_3d);
	mbtp->n3d = NULL;
	mbtp->p2d = mid_2d;
	mbtp->tangent = mid_tang;
	mbtp->normal = mid_norm;
	mbtp->t = mid_range;
	mbtp->trim_ind = trim.m_trim_index;
	mbtp->edge_ind = trim.m_ei;
	(*param_points)[mbtp->t] = mbtp;

	getEdgePoints(trim, sbtp, mbtp, &cdt_tol, *param_points, scratch);
	getEdgePoints(trim, mbtp, ebtp, &cdt_tol, *param_points, scratch);

    } else {

	getEdgePoints(trim, sbtp, ebtp, &cdt_tol, *param_points, scratch);

    }

    fast_ensure_periodic_winding_samples(trim, *param_points, scratch);
    fast_ensure_topology_samples(trim, *param_points, scratch);

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


struct fast_uv_curve_sample {
    double fraction = 0.0;
    ON_3dPoint uv;
    ON_3dPoint point;
    ON_3dVector tangent;
    ON_3dVector normal;
};

static bool
fast_uv_curve_evaluate(const ON_Surface *surface, const ON_Curve *curve,
	double fraction, fast_uv_curve_sample &sample)
{
    sample.fraction = fraction;
    return curve->EvTangent(curve->Domain().ParameterAt(fraction),
	sample.uv, sample.tangent) &&
	cdt_surface_normal(surface, sample.uv, sample.point, sample.normal);
}

static bool
fast_sample_uv_interval(const ON_Surface *surface, const ON_Curve *curve,
	const fast_uv_curve_sample &start, const fast_uv_curve_sample &end,
	const struct brep_cdt_tol &tolerance, int depth,
	fast_bridge_samples &points)
{
    const double fraction = 0.5 * (start.fraction + end.fraction);
    if (!(fraction > start.fraction) || !(fraction < end.fraction))
	return false;
    fast_uv_curve_sample mid;
    if (!fast_uv_curve_evaluate(surface, curve, fraction, mid))
	return false;
    const ON_Line chord(start.point, end.point);
    const double deviation = mid.point.DistanceTo(chord.ClosestPointTo(mid.point));
    const bool angle_split = start.tangent * end.tangent <
	tolerance.cos_within_ang - ON_ZERO_TOLERANCE ||
	start.normal * end.normal < tolerance.cos_within_ang - ON_ZERO_TOLERANCE;
    const bool split = chord.Length() > tolerance.max_dist ||
	deviation > tolerance.within_dist + ON_ZERO_TOLERANCE ||
	(angle_split && deviation > tolerance.min_dist + ON_ZERO_TOLERANCE);
    if (!split)
	return true;
    if (depth >= FAST_CDT_MAX_RECURSION ||
	points.size() >= FAST_CDT_MAX_TRIM_SAMPLES)
	return false;
    points.emplace(fraction, mid.point);
    return fast_sample_uv_interval(surface, curve, start, mid, tolerance,
	depth + 1, points) &&
	fast_sample_uv_interval(surface, curve, mid, end, tolerance,
	    depth + 1, points);
}

static fast_bridge_samples *
getUVCurveSamples(const ON_Surface *surface, const ON_Curve *curve,
	fastf_t max_dist, const struct bg_tess_tol *ttol,
	const struct bn_tol *tol, fast_bridge_store &storage)
{
    fast_bridge_samples *points = storage.create();
    const auto fail = [&]() {
	points->clear();
	storage.invalid = true;
	return points;
    };
    if (!surface || !curve || !curve->Domain().IsIncreasing())
	return fail();

    /* Seed each possible turn at quarter intervals so coincident periodic
     * endpoints cannot hide a curved bridge.  UV extents count turns only;
     * distances and refinement tolerances come from the lifted 3-D points. */
    const int segments_per_turn = 4;
    int segments = segments_per_turn;
    ON_BoundingBox uv_box;
    if (!curve->GetBoundingBox(uv_box) || !uv_box.IsValid())
	return fail();
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction))
	    continue;
	const double period = surface->Domain(direction).Length();
	if (!(period > 0.0) || !std::isfinite(period))
	    return fail();
	const double turns = std::ceil((uv_box.m_max[direction] -
	    uv_box.m_min[direction]) / period);
	const double required = std::max(1.0, turns) * segments_per_turn;
	if (!std::isfinite(required) || required >= FAST_CDT_MAX_TRIM_SAMPLES)
	    return fail();
	segments = std::max(segments, (int)required);
    }
    std::vector<fast_uv_curve_sample> seeds((size_t)segments + 1);
    ON_BoundingBox model_box;
    for (int i = 0; i <= segments; ++i) {
	fast_uv_curve_sample &sample = seeds[(size_t)i];
	if (!fast_uv_curve_evaluate(surface, curve, (double)i / segments, sample))
	    return fail();
	model_box.Set(sample.point, i != 0);
	points->emplace(sample.fraction, sample.point);
    }
    const double model_extent = model_box.Diagonal().Length();
    if (!std::isfinite(model_extent))
	return fail();
    struct brep_cdt_tol tolerance;
    CDT_Tol_Set(&tolerance, model_extent, max_dist, ttol, tol);
    for (int i = 0; i < segments; ++i) {
	if (!fast_sample_uv_interval(surface, curve, seeds[(size_t)i],
		seeds[(size_t)i + 1], tolerance, 0, *points))
	    return fail();
    }
    return points;
}


int
cdt_test_periodic_uv_sampling(void)
{
    const double major_radius = 9.0;
    const double minor_radius = 2.5;
    ON_Torus torus(ON_Circle(ON_xy_plane, major_radius), minor_radius);
    ON_NurbsSurface reference_surface;
    if (!torus.GetNurbForm(reference_surface))
	return 1;
    struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
    struct bn_tol tol = BN_TOL_INIT_TOL;
    const double max_segment_length = 1000.0;
    for (int direction = 0; direction < 2; ++direction) {
	fast_bridge_samples reference;
	for (double scale : {1.0, 1.0 / 1024.0, 1024.0}) {
	    for (int copy : {0, -2, 3}) {
		ON_NurbsSurface surface(reference_surface);
		/* Independent axis scales expose accidental use of UV distance
		 * as a 3-D tolerance, without changing the torus geometry. */
		for (int axis = 0; axis < 2; ++axis) {
		    const double length = reference_surface.Domain(axis).Length() *
			(axis ? 1.0 / scale : scale);
		    if (!surface.SetDomain(axis, 0.0, length))
			return 2;
		}
		ON_2dPoint start, end;
		for (int axis = 0; axis < 2; ++axis) {
		    const ON_Interval domain = surface.Domain(axis);
		    start[axis] = domain.ParameterAt(axis == direction ? 0.0 : 0.125) +
			copy * domain.Length();
		    end[axis] = start[axis] + (axis == direction ? domain.Length() : 0.0);
		}
		ON_LineCurve curve(start, end);
		const ON__UINT32 crc = surface.DataCRC(0);
		fast_bridge_store storage;
		const auto *points = getUVCurveSamples(&surface, &curve,
		    max_segment_length, &ttol, &tol, storage);
		if (storage.invalid || points->size() < 4 || surface.DataCRC(0) != crc)
		    return 3;
		if (reference.empty())
		    reference = *points;
		if (points->size() != reference.size())
		    return 4;
		auto expected = reference.begin();
		double length = 0.0;
		ON_3dPoint previous = points->begin()->second;
		for (const auto &entry : *points) {
		    if (!NEAR_EQUAL(entry.first, expected->first, ON_ZERO_TOLERANCE) ||
			entry.second.DistanceTo(expected->second) > tol.dist)
			return 5;
		    ON_2dPoint uv;
		    for (int axis = 0; axis < 2; ++axis)
			uv[axis] = reference_surface.Domain(axis).ParameterAt(
			    axis == direction ? entry.first : 0.125);
		    if (entry.second.DistanceTo(reference_surface.PointAt(uv.x, uv.y)) > tol.dist)
			return 6;
		    length += previous.DistanceTo(entry.second);
		    previous = entry.second;
		    ++expected;
		}
		const double radius = direction ? minor_radius :
		    major_radius + minor_radius * std::sqrt(0.5);
		const double circumference = M_2PI * radius;
		const double relative_length_tolerance = 0.03;
		if (std::fabs(length - circumference) >
		    relative_length_tolerance * circumference)
		    return 7;
	    }
	}
    }
    /* Excessive winding must fail as a bounded sampling request. */
    const ON_Interval domain = reference_surface.Domain(0);
    ON_LineCurve excessive(ON_2dPoint(domain.Min(), 0.0),
	ON_2dPoint(domain.Min() + FAST_CDT_MAX_TRIM_SAMPLES * domain.Length(), 0.0));
    fast_bridge_store storage;
    const auto *points = getUVCurveSamples(&reference_surface, &excessive,
	max_segment_length, &ttol, &tol, storage);
    return storage.invalid && points->empty() ? 0 : 8;
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
    const double uhalf = 0.5 * ulength;
    const double vhalf = 0.5 * vlength;
    const double ufuzz = std::max(ON_ZERO_TOLERANCE,
	ulength * 1.0e-12);
    const double vfuzz = std::max(ON_ZERO_TOLERANCE,
	vlength * 1.0e-12);
    for (int i = 1; i < num_points; i++) {
	if (surf->IsClosed(0)) {
	    double delta = brep_loop_points[i].p2d.x - brep_loop_points[i - 1].p2d.x;
	    const bool same_trim = brep_loop_points[i].trim_ind >= 0 &&
		brep_loop_points[i].trim_ind ==
		brep_loop_points[i - 1].trim_ind;
	    const bool full_period_step = same_trim &&
		fabs(fabs(delta) - ulength) <=
		std::max(BREP_SAME_POINT_TOLERANCE, ulength * 1.0e-4);
	    int shifts = 0;
	    while (!full_period_step && std::isfinite(delta) &&
		    fabs(delta) > uhalf + ufuzz &&
		    shifts++ < 64) {
		if (delta < 0.0) {
		    brep_loop_points[i].p2d.x += ulength; // east bound
		} else {
		    brep_loop_points[i].p2d.x -= ulength;; // west bound
		}
		delta = brep_loop_points[i].p2d.x - brep_loop_points[i - 1].p2d.x;
	    }
	    if (!std::isfinite(delta) || (!full_period_step &&
		    fabs(delta) > uhalf + ufuzz))
		return false;
	}
	if (surf->IsClosed(1)) {
	    double delta = brep_loop_points[i].p2d.y - brep_loop_points[i - 1].p2d.y;
	    const bool same_trim = brep_loop_points[i].trim_ind >= 0 &&
		brep_loop_points[i].trim_ind ==
		brep_loop_points[i - 1].trim_ind;
	    const bool full_period_step = same_trim &&
		fabs(fabs(delta) - vlength) <=
		std::max(BREP_SAME_POINT_TOLERANCE, vlength * 1.0e-4);
	    int shifts = 0;
	    while (!full_period_step && std::isfinite(delta) &&
		    fabs(delta) > vhalf + vfuzz &&
		    shifts++ < 64) {
		if (delta < 0.0) {
		    brep_loop_points[i].p2d.y += vlength; // north bound
		} else {
		    brep_loop_points[i].p2d.y -= vlength;; // south bound
		}
		delta = brep_loop_points[i].p2d.y - brep_loop_points[i - 1].p2d.y;
	    }
	    if (!std::isfinite(delta) || (!full_period_step &&
		    fabs(delta) > vhalf + vfuzz))
		return false;
	}
    }

    return true;
}

static bool
fast_normalize_periodic_trim_sequence(const ON_Surface *surface,
	const ON_BrepLoop *loop, ON_SimpleArray<BrepTrimPoint> &points)
{
    if (!surface || !loop || loop->TrimCount() < 2 || points.Count() < 2)
	return false;
    bool normalized = false;
    for (int closed_dir = 0; closed_dir < 2; ++closed_dir) {
	if (!surface->IsClosed(closed_dir))
	    continue;
	const int open_dir = 1 - closed_dir;
	const double period = surface->Domain(closed_dir).Length();
	const double open_length = surface->Domain(open_dir).Length();
	if (!(period > ON_ZERO_TOLERANCE) ||
		!(open_length > ON_ZERO_TOLERANCE))
	    continue;
	const double open_join_tolerance = std::max(
	    BREP_SAME_POINT_TOLERANCE, open_length * 1.0e-2);
	const double closed_join_tolerance = std::max(
	    BREP_SAME_POINT_TOLERANCE, period * 0.25);
	ON_2dPoint first_start = ON_2dPoint::UnsetPoint;
	double current_closed = ON_UNSET_VALUE;
	double current_open = ON_UNSET_VALUE;
	std::unordered_map<int, double> trim_shifts;
	bool valid = true;
	for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	    const ON_BrepTrim *trim = loop->Trim(ti);
	    const ON_BrepTrim *next = loop->Trim(
		(ti + 1) % loop->TrimCount());
	    if (!trim || !next ||
		    trim->m_type == ON_BrepTrim::singular ||
		    trim->m_vi[1] != next->m_vi[0]) {
		valid = false;
		break;
	    }
	    const ON_Interval domain = trim->Domain();
	    if (!domain.IsIncreasing()) {
		valid = false;
		break;
	    }
	    const ON_2dPoint start = trim->PointAt(domain.Min());
	    const ON_2dPoint end = trim->PointAt(domain.Max());
	    if (!start.IsValid() || !end.IsValid()) {
		valid = false;
		break;
	    }
	    if (ti == 0) {
		first_start = start;
		current_closed = start[closed_dir];
		current_open = start[open_dir];
	    }
	    if (fabs(start[open_dir] - current_open) >
		    open_join_tolerance) {
		valid = false;
		break;
	    }
	    const double aligned_start = start[closed_dir] +
		std::round((current_closed - start[closed_dir]) / period) *
		period;
	    if (fabs(aligned_start - current_closed) >
		    closed_join_tolerance) {
		valid = false;
		break;
	    }
	    trim_shifts[trim->m_trim_index] =
		aligned_start - start[closed_dir];
	    current_closed = aligned_start +
		end[closed_dir] - start[closed_dir];
	    current_open = end[open_dir];
	}
	if (!valid || fabs(current_open - first_start[open_dir]) >
		open_join_tolerance)
	    continue;
	const long turns = std::lround(
	    (current_closed - first_start[closed_dir]) / period);
	const double winding_tolerance = std::max(
	    BREP_SAME_POINT_TOLERANCE, period * 1.0e-2);
	if (labs(turns) != 1 ||
		fabs(current_closed - first_start[closed_dir] -
		    turns * period) > winding_tolerance)
	    continue;

	int previous_trim = -1;
	double previous_coordinate = ON_UNSET_VALUE;
	for (int pi = 0; pi < points.Count(); ++pi) {
	    std::unordered_map<int, double>::const_iterator shift =
		trim_shifts.find(points[pi].trim_ind);
	    if (shift == trim_shifts.end()) {
		previous_trim = -1;
		continue;
	    }
	    points[pi].p2d[closed_dir] += shift->second;
	    if (points[pi].trim_ind == previous_trim) {
		const double delta = points[pi].p2d[closed_dir] -
		    previous_coordinate;
		const bool full_period_step =
		    fabs(fabs(delta) - period) <= winding_tolerance;
		if (!full_period_step)
		    points[pi].p2d[closed_dir] += std::round(
			(previous_coordinate -
			 points[pi].p2d[closed_dir]) / period) * period;
	    }
	    previous_trim = points[pi].trim_ind;
	    previous_coordinate = points[pi].p2d[closed_dir];
	}
	normalized = true;
    }
    return normalized;
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
	bool repair_pcurves,
	const struct brep_cdt_fast_options *options)
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
	    btp.trim_ind = trim->m_trim_index;
	    btp.edge_ind = trim->m_ei;
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

	const size_t constrained_count = options &&
	    options->trim_sample_count && options->trim_sample ?
	    options->trim_sample_count(face.m_face_index,
		trim->m_trim_index, options->trim_sample_data) : 0;
	if (constrained_count >= 2) {
	    if (constrained_count > FAST_CDT_MAX_TRIM_SAMPLES) {
		scratch.hit_sample_limit = true;
		return;
	    }
	    const size_t output_count = lti < trim_count - 1 ?
		constrained_count - 1 : constrained_count;
	    for (size_t sample_index = 0; sample_index < output_count;
		    ++sample_index) {
		fastf_t trim_parameter = ON_UNSET_VALUE;
		point2d_t uv;
		point_t point;
		if (options->trim_sample(face.m_face_index,
			trim->m_trim_index, sample_index, &trim_parameter,
			uv, point, options->trim_sample_data) != 0 ||
			!std::isfinite(trim_parameter) ||
			!std::isfinite(uv[X]) || !std::isfinite(uv[Y]) ||
			!std::isfinite(point[X]) || !std::isfinite(point[Y]) ||
			!std::isfinite(point[Z])) {
		    scratch.invalid_constraints = true;
		    return;
		}
		BrepTrimPoint constrained;
		constrained.trim_ind = trim->m_trim_index;
		constrained.edge_ind = trim->m_ei;
		constrained.p2d = ON_2dPoint(uv[X], uv[Y]);
		constrained.p3d = scratch.make_point(ON_3dPoint(
		    point[X], point[Y], point[Z]));
		constrained.t = trim_parameter;
		if (options->trim_sample_source) {
		    const void *source = options->trim_sample_source(
			face.m_face_index, trim->m_trim_index, sample_index,
			options->trim_sample_data);
		    constrained.source_id = source;
		}
		constrained.n3d = NULL;
		constrained.e = ON_UNSET_VALUE;
		points->Append(constrained);
	    }
	    continue;
	}

	if (fast_append_periodic_polyline(*points, *trim,
		lti < trim_count - 1, tol, model_diagonal, scratch,
		options && options->preserve_pullback_samples))
	    continue;

	if (repair_pcurves && (fast_pullback_candidate(loop, *trim, tol,
		model_diagonal) || fast_pcurve_edge_mismatch(*trim, tol,
		model_diagonal)) && fast_append_pullback_samples(points, *trim,
		lti < trim_count - 1, tol, model_diagonal, scratch,
		options && options->preserve_pullback_samples))
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
fast_reconstruct_untrimmed_domain_loop(const ON_Surface *surface,
	const ON_BrepFace &face, ON_SimpleArray<BrepTrimPoint> &points,
	fast_face_scratch &scratch)
{
    const ON_BrepLoop *loop = face.LoopCount() == 1 ? face.Loop(0) : NULL;
    if (!surface || !loop || loop->TrimCount() != 0 || points.Count() != 0)
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

    /* ON_NurbsSurface::IsClosed compares entire control-point rows.  This
     * search is quadratic in the loop sample count, so asking IsClosed for
     * every candidate pair can turn a cheap chart normalization into seconds
     * of repeated control-net scans.  Surface periodicity and domains are
     * immutable throughout the search. */
    const bool closed[2] = {s->IsClosed(0), s->IsClosed(1)};
    const ON_Interval domains[2] = {s->Domain(0), s->Domain(1)};
    const double periods[2] = {domains[0].Length(), domains[1].Length()};

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
		if (!closed[dir]) {
		    if (fabs(delta) > tolerance)
			equivalent = false;
		    continue;
		}
		const double period = periods[dir];
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
	if (!closed[dir])
	    continue;
	const ON_Interval &domain = domains[dir];
	const double period = periods[dir];
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
		bool periodic_closed = true;
		bool crosses_period = false;
		for (int dir = 0; dir < 2; ++dir) {
		    const double delta = brep_loop_end[dir] -
			brep_loop_begin[dir];
		    if (!s->IsClosed(dir)) {
			periodic_closed = periodic_closed &&
			    fabs(delta) <= same_point_tolerance;
			continue;
		    }
		    const double period = s->Domain(dir).Length();
		    if (!(period > ON_ZERO_TOLERANCE) ||
			    !std::isfinite(period)) {
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
				    fast_bridge_samples *linepoints3d = getUVCurveSamples(s, &line1, 1000.0, ttol, tol, bridgePoints);
				    line1.GetDomain(&t0, &t1);
				    fast_bridge_samples::iterator i;
				    for (i = linepoints3d->begin();
					 i != linepoints3d->end(); i++) {
					BrepTrimPoint btp;

					// skips first point
					if (i == linepoints3d->begin())
					    continue;

					btp.t = (*i).first;
					btp.p3d = &(*i).second;
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
				    linepoints3d = getUVCurveSamples(s, &line2, 1000.0, ttol, tol, bridgePoints);
				    line2.GetDomain(&t0, &t1);

				    for (i = linepoints3d->begin();
					 i != linepoints3d->end(); i++) {
					BrepTrimPoint btp;
					// skips first point
					if (i == linepoints3d->begin())
					    continue;

					btp.t = (*i).first;
					btp.p3d = &(*i).second;
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
				    fast_bridge_samples *linepoints3d = getUVCurveSamples(s, &line1, 1000.0, ttol, tol, bridgePoints);
				    line1.GetDomain(&t0, &t1);
				    fast_bridge_samples::iterator i;
				    for (i = linepoints3d->begin();
					 i != linepoints3d->end(); i++) {
					BrepTrimPoint btp;

					// skips first point
					if (i == linepoints3d->begin())
					    continue;

					btp.t = (*i).first;
					btp.p3d = &(*i).second;
					btp.n3d = NULL;
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li]->Append(btp);
				    }
				    line1.SetStartPoint(p);
				    p.x = u.m_t[1];
				    line1.SetEndPoint(p);
				    linepoints3d = getUVCurveSamples(s, &line1, 1000.0, ttol, tol, bridgePoints);
				    line1.GetDomain(&t0, &t1);
				    for (i = linepoints3d->begin();
					 i != linepoints3d->end(); i++) {
					BrepTrimPoint btp;

					// skips first point
					if (i == linepoints3d->begin())
					    continue;

					btp.t = (*i).first;
					btp.p3d = &(*i).second;
					btp.n3d = NULL;
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li]->Append(btp);
				    }
				    line1.SetStartPoint(p);
				    line1.SetEndPoint(brep_loop_begin);
				    linepoints3d = getUVCurveSamples(s, &line1, 1000.0, ttol, tol, bridgePoints);
				    line1.GetDomain(&t0, &t1);
				    for (i = linepoints3d->begin();
					 i != linepoints3d->end(); i++) {
					BrepTrimPoint btp;

					// skips first point
					if (i == linepoints3d->begin())
					    continue;

					btp.t = (*i).first;
					btp.p3d = &(*i).second;
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
				    fast_bridge_samples *linepoints3d = getUVCurveSamples(s, &line1, 1000.0, ttol, tol, bridgePoints);
				    line1.GetDomain(&t0, &t1);
				    fast_bridge_samples::iterator i;
				    for (i = linepoints3d->begin();
					 i != linepoints3d->end(); i++) {
					BrepTrimPoint btp;

					// skips first point
					if (i == linepoints3d->begin())
					    continue;

					btp.t = (*i).first;
					btp.p3d = &(*i).second;
					btp.n3d = NULL;
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li]->Append(btp);
				    }
				    line1.SetStartPoint(p);
				    p.x = u.m_t[0];
				    line1.SetEndPoint(p);
				    linepoints3d = getUVCurveSamples(s, &line1, 1000.0, ttol, tol, bridgePoints);
				    line1.GetDomain(&t0, &t1);
				    for (i = linepoints3d->begin();
					 i != linepoints3d->end(); i++) {
					BrepTrimPoint btp;

					// skips first point
					if (i == linepoints3d->begin())
					    continue;

					btp.t = (*i).first;
					btp.p3d = &(*i).second;
					btp.n3d = NULL;
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li]->Append(btp);
				    }
				    line1.SetStartPoint(p);
				    line1.SetEndPoint(brep_loop_begin);
				    linepoints3d = getUVCurveSamples(s, &line1, 1000.0, ttol, tol, bridgePoints);
				    line1.GetDomain(&t0, &t1);
				    for (i = linepoints3d->begin();
					 i != linepoints3d->end(); i++) {
					BrepTrimPoint btp;

					// skips first point
					if (i == linepoints3d->begin())
					    continue;

					btp.t = (*i).first;
					btp.p3d = &(*i).second;
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

struct fast_uv_bounds {
    ON_2dPoint minimum = ON_2dPoint(INFINITY, INFINITY);
    ON_2dPoint maximum = ON_2dPoint(-INFINITY, -INFINITY);

    explicit fast_uv_bounds(const ON_SimpleArray<BrepTrimPoint> &points)
    {
	for (int pi = 0; pi < points.Count(); ++pi) {
	    for (int dir = 0; dir < 2; ++dir) {
		minimum[dir] = std::min(minimum[dir], points[pi].p2d[dir]);
		maximum[dir] = std::max(maximum[dir], points[pi].p2d[dir]);
	    }
	}
    }
    double span(int dir) const { return maximum[dir] - minimum[dir]; }
    double midpoint(int dir) const { return 0.5 * (minimum[dir] + maximum[dir]); }
};

enum class fast_periodic_boundary_kind {
    unchanged,
    rectangle,
    constrained_band,
    stitched
};

struct fast_periodic_recovery {
    fast_periodic_boundary_kind kind = fast_periodic_boundary_kind::unchanged;
    int closed_dir = -1;
    int outer_index = -1;

    bool recovered() const { return kind != fast_periodic_boundary_kind::unchanged; }
    bool needs_grid() const
    {
	return kind == fast_periodic_boundary_kind::rectangle ||
	    kind == fast_periodic_boundary_kind::constrained_band;
    }
};

struct fast_periodic_rectangle {
    int closed_dir;
    ON_Interval closed;
    ON_Interval transverse;
    int start_singularity = -1;
    int end_singularity = -1;
};

/* Intervals retain their directed endpoints: sorting them would lose the
 * winding selected by the recognizer.  Publish only a complete boundary. */
static bool
fast_build_periodic_rectangle(const ON_Surface *surface,
	const fast_periodic_rectangle &rectangle, fast_face_scratch &scratch,
	ON_SimpleArray<BrepTrimPoint> &points)
{
    ON_SimpleArray<BrepTrimPoint> candidate;
    for (int corner = 0; corner < 5; ++corner) {
	const bool at_start = corner == 0 || corner == 3 || corner == 4;
	ON_2dPoint uv;
	uv[rectangle.closed_dir] = rectangle.closed[
	    (corner == 2 || corner == 3) ? 1 : 0];
	uv[1 - rectangle.closed_dir] = rectangle.transverse[at_start ? 0 : 1];
	const int singular_side = at_start ? rectangle.start_singularity :
	    rectangle.end_singularity;
	if (!fast_append_synthetic_uv(candidate, surface, uv, singular_side, scratch))
	    return false;
    }
    points = candidate;
    return true;
}

/* Translate whole holes into the selected period without changing their
 * local winding, source identities, or geometry.  A hole that does not fit
 * remains available to the subsequent constraint validation. */
static void
fast_align_periodic_holes(ON_SimpleArray<BrepTrimPoint> **loops,
	int count, int outer_index, int closed_dir, double period, double tolerance)
{
    const fast_uv_bounds outer(*loops[outer_index]);
    const double low = outer.minimum[closed_dir];
    const double high = outer.maximum[closed_dir];
    const double midpoint = 0.5 * (low + high);
    for (int li = 0; li < count; ++li) {
	if (li == outer_index || !loops[li] || !loops[li]->Count())
	    continue;
	auto &hole = *loops[li];
	const fast_uv_bounds bounds(hole);
	double sum = 0.0;
	for (int pi = 0; pi < hole.Count(); ++pi)
	    sum += hole[pi].p2d[closed_dir];
	const double shift = std::round((midpoint - sum / hole.Count()) / period) *
	    period;
	if (bounds.minimum[closed_dir] + shift < low - tolerance ||
	    bounds.maximum[closed_dir] + shift > high + tolerance)
	    continue;
	for (int pi = 0; pi < hole.Count(); ++pi)
	    hole[pi].p2d[closed_dir] += shift;
    }
}

static bool
fast_reconstruct_periodic_trim_boundary(const ON_Surface *surface,
	const ON_BrepLoop *loop, ON_SimpleArray<BrepTrimPoint> &points,
	double tolerance, fast_face_scratch &scratch)
{
    if (!surface || !loop || loop->TrimCount() != 1 ||
	    points.Count() < 2)
	return false;
    const ON_BrepTrim *trim = loop->Trim(0);
    if (!trim || !trim->Edge() || trim->m_vi[0] != trim->m_vi[1] ||
	    trim->m_type == ON_BrepTrim::seam ||
	    trim->m_type == ON_BrepTrim::singular)
	return false;
    const ON_Interval trim_domain = trim->Domain();
    if (!trim_domain.IsIncreasing())
	return false;
    const ON_2dPoint start = trim->PointAt(trim_domain.Min());
    const ON_2dPoint end = trim->PointAt(trim_domain.Max());
    if (!start.IsValid() || !end.IsValid())
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
	const double delta = end[closed_dir] - start[closed_dir];
	const long turns = std::lround(delta / period);
	if (labs(turns) != 1 ||
		fabs(delta - turns * period) > parameter_tolerance ||
		fabs(end[open_dir] - start[open_dir]) >
		    parameter_tolerance)
	    continue;
	const double open_coordinate = 0.5 *
	    (start[open_dir] + end[open_dir]);
	const double isocurve_tolerance = std::max(parameter_tolerance,
	    open_length * 1.0e-3);
	bool isocurve = true;
	for (int pi = 0; pi < points.Count(); ++pi) {
	    if (!points[pi].p2d.IsValid() ||
		    fabs(points[pi].p2d[open_dir] - open_coordinate) >
			isocurve_tolerance) {
		isocurve = false;
		break;
	    }
	}
	if (!isocurve)
	    continue;

	ON_SimpleArray<BrepTrimPoint> replacement;
	for (int sample = 0; sample <= 16; ++sample) {
	    ON_2dPoint uv = start;
	    uv[closed_dir] = start[closed_dir] + turns * period *
		(double)sample / 16.0;
	    uv[open_dir] = open_coordinate;
	    if (!fast_append_synthetic_uv(replacement, surface, uv, -1,
		    scratch))
		return false;
	}
	points = replacement;
	return true;
    }
    return false;
}

static bool
fast_reconstruct_full_periodic_face(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double tolerance, fast_face_scratch &scratch, fast_periodic_recovery &recovery)
{
    recovery = {};
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

	const fast_uv_bounds bounds(points);
	const double parameter_tolerance = std::max(tolerance,
	    period * 1.0e-4);
	if (bounds.span(closed_dir) > parameter_tolerance ||
		bounds.span(open_dir) < 0.90 * open_length)
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
	int start_side = open_direction > 0 ? low_side : high_side;
	int end_side = open_direction > 0 ? high_side : low_side;
	if (!surface->IsSingular(start_side))
	    start_side = -1;
	if (!surface->IsSingular(end_side))
	    end_side = -1;
	const fast_periodic_rectangle rectangle = {closed_dir,
	    ON_Interval(closed_start, closed_end), ON_Interval(open_start, open_end),
	    start_side, end_side};
	if (!fast_build_periodic_rectangle(surface, rectangle, scratch, points))
	    return false;
	/* The collapsed original outline could not locate holes during seam
	 * preprocessing; align them against the reconstructed domain now. */
	fast_align_periodic_holes(brep_loop_points, face.LoopCount(), outer_index,
	    closed_dir, period, parameter_tolerance);
	recovery = {fast_periodic_boundary_kind::rectangle, closed_dir, outer_index};
	return true;
    }
    return false;
}

static bool
fast_boundary_has_resolvable_area(
	const ON_SimpleArray<BrepTrimPoint> &points);

static long double
fast_boundary_signed_area(const ON_SimpleArray<BrepTrimPoint> &points);

static bool
fast_loop_is_provably_degenerate(const ON_Surface *surface,
	const ON_BrepLoop *loop, const struct bn_tol *tol);

static bool
fast_reconstruct_implicit_periodic_domain(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	const struct bn_tol *tol, double tolerance, fast_face_scratch &scratch,
	fast_periodic_recovery &recovery,
	std::vector<ON_SimpleArray<BrepTrimPoint>> &reconstructed_holes)
{
    if (!surface || !brep_loop_points || face.LoopCount() < 1)
	return false;

    /* Some invalid exchanges omit the outer-loop designation and store only
     * exclusions on an otherwise closed support surface.  Recover that
     * interpretation only when every nondegenerate loop is an ordinary closed
     * hole in one continuous image of the periodic domain. */
    const double maximum_contractible_span_fraction = 0.90;
    const double parameter_tolerance_fraction = 1.0e-4;
    struct hole_record {
	ON_SimpleArray<BrepTrimPoint> points;
	double minimum[2] = {INFINITY, INFINITY};
	double maximum[2] = {-INFINITY, -INFINITY};
    };
    std::vector<hole_record> holes((size_t)face.LoopCount());
    double parameter_scale = 1.0;
    int first_closed_dir = -1;
    for (int dir = 0; dir < 2; ++dir) {
	const double length = surface->Domain(dir).Length();
	if (!(length > ON_ZERO_TOLERANCE))
	    return false;
	parameter_scale = std::max(parameter_scale, length);
	if (first_closed_dir < 0 && surface->IsClosed(dir))
	    first_closed_dir = dir;
    }
    if (first_closed_dir < 0)
	return false;
    const double parameter_tolerance = std::max(tolerance,
	parameter_scale * parameter_tolerance_fraction);

    for (int li = 0; li < face.LoopCount(); ++li) {
	const ON_BrepLoop *loop = face.Loop(li);
	if (!loop || loop->m_type != ON_BrepLoop::inner ||
		!brep_loop_points[li])
	    return false;
	if (fast_loop_is_provably_degenerate(surface, loop, tol))
	    continue;
	if (brep_loop_points[li]->Count() < 3 ||
		!fast_boundary_has_resolvable_area(*brep_loop_points[li]))
	    return false;
	const ON_2dPoint &first = (*brep_loop_points[li])[0].p2d;
	const ON_2dPoint &last = (*brep_loop_points[li])[
	    brep_loop_points[li]->Count() - 1].p2d;
	if (!first.IsValid() || !last.IsValid())
	    return false;

	hole_record &hole = holes[(size_t)li];
	hole.points = *brep_loop_points[li];
	for (int pi = 0; pi < hole.points.Count(); ++pi) {
	    for (int dir = 0; dir < 2; ++dir) {
		hole.minimum[dir] = std::min(hole.minimum[dir],
		    hole.points[pi].p2d[dir]);
		hole.maximum[dir] = std::max(hole.maximum[dir],
		    hole.points[pi].p2d[dir]);
	    }
	}
	const ON_2dVector delta = last - first;
	for (int dir = 0; dir < 2; ++dir) {
	    const ON_Interval domain = surface->Domain(dir);
	    if (fabs(delta[dir]) > parameter_tolerance)
		return false;
	    if (surface->IsClosed(dir)) {
		if (hole.maximum[dir] - hole.minimum[dir] >=
			maximum_contractible_span_fraction * domain.Length())
		    return false;
	    } else if (hole.minimum[dir] < domain.Min() -
		    parameter_tolerance ||
		    hole.maximum[dir] > domain.Max() +
		    parameter_tolerance) {
		return false;
	    }
	}
    }

    double boundary_minimum[2];
    double boundary_maximum[2];
    for (int dir = 0; dir < 2; ++dir) {
	const ON_Interval domain = surface->Domain(dir);
	if (!surface->IsClosed(dir)) {
	    boundary_minimum[dir] = domain.Min();
	    boundary_maximum[dir] = domain.Max();
	    continue;
	}

	std::vector<double> seam_candidates = {domain.Min()};
	for (const hole_record &hole : holes) {
	    if (hole.points.Count())
		seam_candidates.push_back(hole.maximum[dir] +
		    parameter_tolerance);
	}
	bool found_seam = false;
	for (double seam : seam_candidates) {
	    bool fits = true;
	    for (const hole_record &hole : holes) {
		if (!hole.points.Count())
		    continue;
		const double midpoint = 0.5 * (hole.minimum[dir] +
		    hole.maximum[dir]);
		const double shift = std::round((seam +
		    0.5 * domain.Length() - midpoint) /
		    domain.Length()) * domain.Length();
		fits = fits && hole.minimum[dir] + shift >= seam -
		    parameter_tolerance &&
		    hole.maximum[dir] + shift <= seam + domain.Length() +
		    parameter_tolerance;
		if (!fits)
		    break;
	    }
	    if (!fits)
		continue;

	    boundary_minimum[dir] = seam;
	    boundary_maximum[dir] = seam + domain.Length();
	    for (hole_record &hole : holes) {
		if (!hole.points.Count())
		    continue;
		const double midpoint = 0.5 * (hole.minimum[dir] +
		    hole.maximum[dir]);
		const double shift = std::round((seam +
		    0.5 * domain.Length() - midpoint) /
		    domain.Length()) * domain.Length();
		for (int pi = 0; pi < hole.points.Count(); ++pi)
		    hole.points[pi].p2d[dir] += shift;
		hole.minimum[dir] += shift;
		hole.maximum[dir] += shift;
	    }
	    found_seam = true;
	    break;
	}
	if (!found_seam)
	    return false;
    }

    const int open_dir = 1 - first_closed_dir;
    const int open_low_side = open_dir == 0 ? 3 : 0;
    const int open_high_side = open_dir == 0 ? 1 : 2;
    const int start_side = !surface->IsClosed(open_dir) &&
	surface->IsSingular(open_low_side) ? open_low_side : -1;
    const int end_side = !surface->IsClosed(open_dir) &&
	surface->IsSingular(open_high_side) ? open_high_side : -1;
    const fast_periodic_rectangle rectangle = {first_closed_dir,
	ON_Interval(boundary_minimum[first_closed_dir],
	    boundary_maximum[first_closed_dir]),
	ON_Interval(boundary_minimum[open_dir], boundary_maximum[open_dir]),
	start_side, end_side};
    ON_SimpleArray<BrepTrimPoint> boundary;
    if (!fast_build_periodic_rectangle(surface, rectangle, scratch, boundary))
	return false;

    *brep_loop_points[0] = boundary;
    for (int li = 1; li < face.LoopCount(); ++li)
	brep_loop_points[li]->Empty();
    reconstructed_holes.clear();
    reconstructed_holes.reserve(holes.size());
    for (hole_record &hole : holes) {
	if (hole.points.Count())
	    reconstructed_holes.push_back(hole.points);
    }
    recovery = {fast_periodic_boundary_kind::rectangle, first_closed_dir, 0};
    return true;
}

static bool
fast_reconstruct_periodic_singular_face(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double tolerance, fast_face_scratch &scratch, fast_periodic_recovery &recovery)
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
    std::vector<const ON_BrepTrim *> closed_boundary_trims;
    for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	const ON_BrepTrim *trim = loop->Trim(ti);
	if (!trim)
	    return false;
	if (trim->m_type == ON_BrepTrim::seam)
	    seam_trims++;
	else if (trim->m_type == ON_BrepTrim::singular)
	    singular_trims++;
	else if (trim->Edge() && trim->m_vi[0] == trim->m_vi[1])
	    closed_boundary_trims.push_back(trim);
	else
	    return false;
    }
    if (seam_trims < 2 || singular_trims < 1 ||
	    closed_boundary_trims.empty())
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

	const fast_uv_bounds bounds(points);
	const double parameter_tolerance = std::max(tolerance,
	    std::max(period, open_length) * 1.0e-4);
	if (bounds.span(closed_dir) < 0.90 * period ||
		bounds.span(open_dir) <= parameter_tolerance)
	    continue;
	const double singular_boundary = low_singular ? open_domain.Min() :
	    open_domain.Max();
	const double sampled_singular_boundary = low_singular ? bounds.minimum[open_dir] :
	    bounds.maximum[open_dir];
	if (fabs(sampled_singular_boundary - singular_boundary) >
		parameter_tolerance)
	    continue;
	/* Some invalid imports repeat the same seam, singular, and closed-edge
	 * circuit.  Accept repeated closed trims only when every copy lies on the
	 * same nonsingular boundary selected by the sampled circuit. */
	const double nonsingular_boundary = low_singular ?
	    bounds.maximum[open_dir] : bounds.minimum[open_dir];
	bool boundary_group_valid = true;
	for (const ON_BrepTrim *trim : closed_boundary_trims) {
	    const ON_Interval trim_domain = trim->Domain();
	    const ON_2dPoint trim_start = trim->PointAt(trim_domain.Min());
	    const ON_2dPoint trim_end = trim->PointAt(trim_domain.Max());
	    if (!trim_start.IsValid() || !trim_end.IsValid() ||
		    fabs(trim_start[open_dir] - nonsingular_boundary) >
			parameter_tolerance ||
		    fabs(trim_end[open_dir] - nonsingular_boundary) >
			parameter_tolerance) {
		boundary_group_valid = false;
		break;
	    }
	}
	if (!boundary_group_valid)
	    continue;
	if (bounds.minimum[closed_dir] < closed_domain.Min() - parameter_tolerance ||
		bounds.maximum[closed_dir] > closed_domain.Max() + parameter_tolerance ||
		bounds.minimum[open_dir] < open_domain.Min() - parameter_tolerance ||
		bounds.maximum[open_dir] > open_domain.Max() + parameter_tolerance)
	    continue;

	const int singular_side = low_singular ? low_side : high_side;
	const double open_start = nonsingular_boundary;
	const double open_end = singular_boundary;
	const fast_periodic_rectangle rectangle = {closed_dir,
	    ON_Interval(closed_domain.Min(), closed_domain.Max()),
	    ON_Interval(open_start, open_end), -1, singular_side};
	if (!fast_build_periodic_rectangle(surface, rectangle, scratch, points))
	    return false;
	recovery = {fast_periodic_boundary_kind::rectangle, closed_dir, 0};
	return true;
    }
    return false;
}

static bool
fast_reconstruct_paired_periodic_strip(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double tolerance, fast_face_scratch &scratch, fast_periodic_recovery &recovery)
{
    if (!surface || face.LoopCount() != 1 || !brep_loop_points ||
	    !brep_loop_points[0] || brep_loop_points[0]->Count() < 4)
	return false;
    const ON_BrepLoop *loop = face.Loop(0);
    if (!loop || loop->m_type != ON_BrepLoop::outer ||
	    loop->TrimCount() < 4 || loop->TrimCount() > 6)
	return false;

    std::vector<const ON_BrepTrim *> seam_trims;
    std::vector<const ON_BrepTrim *> boundary_trims;
    for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	const ON_BrepTrim *trim = loop->Trim(ti);
	const ON_BrepTrim *next = loop->Trim(
	    (ti + 1) % loop->TrimCount());
	if (!trim || !next || trim->m_type == ON_BrepTrim::singular ||
		trim->m_vi[1] != next->m_vi[0])
	    return false;
	if (trim->m_type == ON_BrepTrim::seam)
	    seam_trims.push_back(trim);
	else
	    boundary_trims.push_back(trim);
    }
    if (seam_trims.size() != 2 || boundary_trims.size() < 2 ||
	    boundary_trims.size() > 4 ||
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
	size_t split = 0;
	double largest_gap = -INFINITY;
	for (size_t bi = 1; bi < boundaries.size(); ++bi) {
	    const double gap = boundaries[bi].open_coordinate -
		boundaries[bi - 1].open_coordinate;
	    if (gap > largest_gap) {
		largest_gap = gap;
		split = bi;
	    }
	}
	if (largest_gap <= isocurve_tolerance || split < 1 ||
		split > 2 || boundaries.size() - split < 1 ||
		boundaries.size() - split > 2 ||
		boundaries[split - 1].open_coordinate -
		    boundaries[0].open_coordinate > isocurve_tolerance ||
		boundaries.back().open_coordinate -
		    boundaries[split].open_coordinate > isocurve_tolerance)
	    continue;
	double open_low = 0.0;
	for (size_t bi = 0; bi < split; ++bi)
	    open_low += boundaries[bi].open_coordinate;
	open_low /= split;
	double open_high = 0.0;
	for (size_t bi = split; bi < boundaries.size(); ++bi)
	    open_high += boundaries[bi].open_coordinate;
	open_high /= boundaries.size() - split;
	if (open_high - open_low <= isocurve_tolerance)
	    continue;

	auto reversed_pair = [](const ON_BrepTrim *first,
		const ON_BrepTrim *second) {
	    return first->Edge() != second->Edge() &&
		first->m_vi[0] == second->m_vi[1] &&
		first->m_vi[1] == second->m_vi[0];
	};
	auto valid_boundary_group = [&](size_t begin, size_t end) {
	    const size_t count = end - begin;
	    if (count == 1)
		return boundaries[begin].trim->m_vi[0] ==
		    boundaries[begin].trim->m_vi[1];
	    return count == 2 && reversed_pair(boundaries[begin].trim,
		boundaries[begin + 1].trim);
	};
	if (!valid_boundary_group(0, split) ||
		!valid_boundary_group(split, boundaries.size()))
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
	const fast_periodic_rectangle rectangle = {closed_dir,
	    ON_Interval(closed_start, closed_end),
	    ON_Interval(seam_start[open_dir], seam_end[open_dir])};
	if (!fast_build_periodic_rectangle(surface, rectangle, scratch, points))
	    return false;
	recovery = {fast_periodic_boundary_kind::rectangle, closed_dir, 0};
	return true;
    }
    return false;
}

static bool
fast_sample_full_periodic_face(const ON_Surface *surface,
	ON_SimpleArray<BrepTrimPoint> &boundary,
	const struct bg_tess_tol *ttol, double model_diagonal,
	int closed_dir, ON_2dPointArray &surface_points,
	fast_face_scratch &scratch)
{
    if (!surface || boundary.Count() < 5 || !ttol)
	return false;
    if (closed_dir < 0 || closed_dir > 1 ||
	    !surface->IsClosed(closed_dir))
	return false;
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

    double parameter_min[2] = {INFINITY, INFINITY};
    double parameter_max[2] = {-INFINITY, -INFINITY};
    for (int i = 0; i < boundary.Count(); ++i) {
	for (int dir = 0; dir < 2; ++dir) {
	    parameter_min[dir] = std::min(parameter_min[dir],
		boundary[i].p2d[dir]);
	    parameter_max[dir] = std::max(parameter_max[dir],
		boundary[i].p2d[dir]);
	}
    }
    const double closed_start = parameter_min[closed_dir];
    const double closed_delta = parameter_max[closed_dir] - closed_start;
    const double open_start = parameter_min[open_dir];
    const double open_delta = parameter_max[open_dir] - open_start;
    if (!(closed_delta > ON_ZERO_TOLERANCE) ||
	    !(open_delta > ON_ZERO_TOLERANCE))
	return false;

    /* A rectangle's periodic sides span a complete curved boundary.  Four
     * corner constraints make their 3-D chords collapse and allow triangles
     * to bridge across the surface.  Sample the boundary on the same grid as
     * its interior before constructing the triangulation constraints. */
    ON_SimpleArray<BrepTrimPoint> sampled_boundary;
    for (int side = 0; side < boundary.Count() - 1; ++side) {
	const ON_2dPoint start = boundary[side].p2d;
	const ON_2dPoint delta = boundary[side + 1].p2d - start;
	/* Shared physical edges already have authoritative sampling.  Adding
	 * points here would introduce hanging vertices against rigorous faces. */
	if (boundary[side].source_id && boundary[side + 1].source_id &&
		NEAR_ZERO(delta[open_dir], ON_ZERO_TOLERANCE)) {
	    sampled_boundary.Append(boundary[side]);
	    continue;
	}
	const size_t steps = std::max((size_t)1, (size_t)ceil(std::max(
	    fabs(delta[closed_dir]) * closed_steps / closed_delta,
	    fabs(delta[open_dir]) * open_steps / open_delta)));
	for (size_t sample = 0; sample < steps; ++sample) {
	    if (!sample) {
		sampled_boundary.Append(boundary[side]);
		continue;
	    }
	    const ON_2dPoint uv = start + delta * ((double)sample / steps);
	    const int singular_side = boundary[side].from_singular ==
		boundary[side + 1].from_singular ?
		boundary[side].from_singular : -1;
	    if (!fast_append_synthetic_uv(sampled_boundary, surface, uv,
		    singular_side, scratch))
		return false;
	}
    }
    sampled_boundary.Append(boundary[boundary.Count() - 1]);
    for (size_t i = 1; i < closed_steps; ++i) {
	for (size_t j = 1; j < open_steps; ++j) {
	    if (surface_points.Count() >= FAST_CDT_MAX_SURFACE_SAMPLES)
		return false;
	    ON_2dPoint uv = boundary[0].p2d;
	    uv[closed_dir] = closed_start + closed_delta *
		(double)i / (double)closed_steps;
	    uv[open_dir] = open_start + open_delta *
		(double)j / (double)open_steps;
	    surface_points.Append(uv);
	}
    }
    boundary = sampled_boundary;
    return true;
}

static bool
fast_reconstruct_periodic_cap(const ON_Surface *surface,
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
    const ON_Interval open_domain = surface->Domain(open_dir);
    const double boundary_coordinate =
	(singular_side == 0 || singular_side == 3) ?
	open_domain.Min() : open_domain.Max();
    ON_2dPoint end_boundary = end;
    ON_2dPoint start_boundary = start;
    end_boundary[open_dir] = boundary_coordinate;
    start_boundary[open_dir] = boundary_coordinate;

    const int old_count = points.Count();
    /* Imported periodic patches often trim a narrow band against a natural
     * nonsingular surface boundary.  The winding still selects that boundary
     * unambiguously; only mark its vertices singular when the surface agrees. */
    const int collapsed_side = surface->IsSingular(singular_side) ?
	singular_side : -1;
    if (!fast_append_synthetic_uv(points, surface, end_boundary,
	    collapsed_side, scratch) ||
	    !fast_append_synthetic_uv(points, surface, start_boundary,
	    collapsed_side, scratch)) {
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
	const fast_uv_bounds bounds(points);
	const double delta = points[points.Count() - 1].p2d[dir] -
	    points[0].p2d[dir];
	/* A one-turn boundary may locally backtrack across a converted trim
	 * junction.  Keep enough headroom for that excursion without accepting
	 * a second winding. */
	const double maximum_extent = 1.25 * period;
	if (bounds.span(dir) < 0.90 * period ||
		bounds.span(dir) > maximum_extent ||
		fabs(delta) < 0.90 * period ||
		bounds.span(open_dir) > 0.50 * open_length)
	    continue;
	info.closed_dir = dir;
	info.open_dir = open_dir;
	info.winding = delta > 0.0 ? 1 : -1;
	info.open_coordinate = bounds.midpoint(open_dir);
	return true;
    }
    return false;
}

static bool
fast_rotate_periodic_boundary(const ON_Surface *surface,
	ON_SimpleArray<BrepTrimPoint> &points, int closed_dir,
	double period, double target_start, fast_face_scratch &scratch)
{
    if (!surface || points.Count() < 2 || !(period > 0.0))
	return false;
    const double start = points[0].p2d[closed_dir];
    const double end = points[points.Count() - 1].p2d[closed_dir];
    const int winding = end > start ? 1 : -1;
    double cut = target_start;
    if (winding > 0) {
	cut += ceil((start - cut) / period) * period;
	if (cut > end + 0.05 * period)
	    return false;
    } else {
	cut += floor((start - cut) / period) * period;
	if (cut < end - 0.05 * period)
	    return false;
    }

    /* Period translation can move an endpoint a few ulps outside the sampled
     * interval.  Keep the exact boundary point instead of missing its segment
     * or interpolating an unnecessary new seam vertex. */
    const double translation_roundoff_units = 16.0;
    const double cut_tolerance = translation_roundoff_units *
	std::numeric_limits<double>::epsilon() * std::max({period,
	    fabs(start), fabs(end), fabs(target_start)});
    if (fabs(cut - start) <= cut_tolerance)
	cut = start;
    else if (fabs(cut - end) <= cut_tolerance)
	cut = end;

    int cut_segment = -1;
    double fraction = 0.0;
    for (int pi = 0; pi < points.Count() - 1; ++pi) {
	const double first = points[pi].p2d[closed_dir];
	const double second = points[pi + 1].p2d[closed_dir];
	if ((winding > 0 && first <= cut && cut <= second) ||
		(winding < 0 && first >= cut && cut >= second)) {
	    const double delta = second - first;
	    if (fabs(delta) <= ON_ZERO_TOLERANCE)
		continue;
	    cut_segment = pi;
	    fraction = (cut - first) / delta;
	    break;
	}
    }
    if (cut_segment < 0)
	return false;

    ON_2dPoint cut_uv = points[cut_segment].p2d + fraction *
	(points[cut_segment + 1].p2d - points[cut_segment].p2d);
    cut_uv[closed_dir] = target_start;
    ON_SimpleArray<BrepTrimPoint> replacement;
    if (!fast_append_synthetic_uv(replacement, surface, cut_uv, -1,
	    scratch))
	return false;

    auto append_shifted = [&](const BrepTrimPoint &source, double shift) {
	BrepTrimPoint point = source;
	point.p2d[closed_dir] += shift;
	if (!V2NEAR_EQUAL(replacement[replacement.Count() - 1].p2d,
		point.p2d, BREP_SAME_POINT_TOLERANCE))
	    replacement.Append(point);
    };
    const double first_shift = target_start - cut;
    for (int pi = cut_segment + 1; pi < points.Count(); ++pi)
	append_shifted(points[pi], first_shift);
    const double wrapped_shift = first_shift + winding * period;
    for (int pi = 1; pi <= cut_segment; ++pi)
	append_shifted(points[pi], wrapped_shift);

    cut_uv[closed_dir] = target_start + winding * period;
    if (!V2NEAR_EQUAL(replacement[replacement.Count() - 1].p2d,
	    cut_uv, BREP_SAME_POINT_TOLERANCE) &&
	    !fast_append_synthetic_uv(replacement, surface, cut_uv, -1,
		scratch))
	return false;
    points = replacement;
    return points.Count() >= 2;
}

static bool
fast_reconstruct_periodic_strip(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double model_diagonal, fast_face_scratch &scratch,
	fast_periodic_recovery &recovery)
{
    recovery.outer_index = -1;
    if (!surface || face.LoopCount() < 2 || !brep_loop_points)
	return false;

    int outer_index = -1;
    std::vector<int> boundary_indices;
    std::vector<fast_periodic_loop_info> boundary_info;
    fast_periodic_loop_info first_info;
    fast_periodic_loop_info second_info;
    for (int li = 0; li < face.LoopCount(); ++li) {
	const ON_BrepLoop *loop = face.Loop(li);
	if (!loop || !brep_loop_points[li])
	    return false;
	if (loop->m_type == ON_BrepLoop::outer && outer_index < 0)
	    outer_index = li;
	fast_periodic_loop_info candidate;
	if (fast_periodic_boundary_loop(surface, *brep_loop_points[li],
		candidate)) {
	    boundary_indices.push_back(li);
	    boundary_info.push_back(candidate);
	}
    }
    if (boundary_indices.size() < 2)
	return false;

    std::vector<size_t> boundary_order(boundary_indices.size());
    for (size_t bi = 0; bi < boundary_order.size(); ++bi)
	boundary_order[bi] = bi;
    std::sort(boundary_order.begin(), boundary_order.end(),
	[&](size_t a, size_t b) {
	    return boundary_info[a].open_coordinate <
		boundary_info[b].open_coordinate;
	});
    const int boundary_direction =
	boundary_info[boundary_order.front()].closed_dir;
    for (size_t bi : boundary_order) {
	if (boundary_info[bi].closed_dir != boundary_direction)
	    return false;
    }

    const size_t low_boundary = boundary_order.front();
    const size_t high_boundary = boundary_order.back();
    int first_index = boundary_indices[low_boundary];
    int opposite_index = boundary_indices[high_boundary];
    first_info = boundary_info[low_boundary];
    second_info = boundary_info[high_boundary];
    if (opposite_index == outer_index) {
	std::swap(first_index, opposite_index);
	std::swap(first_info, second_info);
    }
    if (fabs(first_info.open_coordinate -
	    second_info.open_coordinate) <= BREP_SAME_POINT_TOLERANCE)
	return false;

    ON_SimpleArray<BrepTrimPoint> &first =
	*brep_loop_points[first_index];
    ON_SimpleArray<BrepTrimPoint> &second =
	*brep_loop_points[opposite_index];
    ON_SimpleArray<BrepTrimPoint> original_outer;
    if (outer_index >= 0 && first_index != outer_index)
	original_outer = *brep_loop_points[outer_index];

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

    if (!fast_rotate_periodic_boundary(surface, opposite, closed_dir,
	    period, first[first.Count() - 1].p2d[closed_dir], scratch))
	return false;
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
    for (size_t bi : boundary_order) {
	const int redundant_index = boundary_indices[bi];
	if (redundant_index != first_index &&
		redundant_index != opposite_index)
	    brep_loop_points[redundant_index]->Empty();
    }
    int reconstructed_outer_index = first_index;
    if (outer_index >= 0 && first_index != outer_index) {
	*brep_loop_points[outer_index] = first;
	first = original_outer;
	reconstructed_outer_index = outer_index;
    }

    const double parameter_tolerance = std::max(
	BREP_SAME_POINT_TOLERANCE, period * 1.0e-4);
    fast_align_periodic_holes(brep_loop_points, face.LoopCount(),
	reconstructed_outer_index, closed_dir, period, parameter_tolerance);
    recovery = {fast_periodic_boundary_kind::stitched, closed_dir,
	reconstructed_outer_index};
    return true;
}

static bool
fast_reconstruct_periodic_empty_boundary(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double tolerance, fast_face_scratch &scratch, fast_periodic_recovery &recovery)
{
    if (!surface || face.LoopCount() != 2 || !brep_loop_points)
	return false;
    int boundary_index = -1;
    int empty_index = -1;
    for (int li = 0; li < 2; ++li) {
	const ON_BrepLoop *loop = face.Loop(li);
	if (!loop || !brep_loop_points[li])
	    return false;
	if (loop->TrimCount() == 0 && brep_loop_points[li]->Count() == 0)
	    empty_index = li;
	else if (loop->TrimCount() == 1 &&
		brep_loop_points[li]->Count() >= 2)
	    boundary_index = li;
	else
	    return false;
    }
    if (boundary_index < 0 || empty_index < 0)
	return false;

    const ON_BrepTrim *trim = face.Loop(boundary_index)->Trim(0);
    if (!trim || !trim->Edge() || trim->m_vi[0] != trim->m_vi[1])
	return false;
    fast_periodic_loop_info info;
    ON_SimpleArray<BrepTrimPoint> &boundary =
	*brep_loop_points[boundary_index];
    if (!fast_periodic_boundary_loop(surface, boundary, info))
	return false;

    const ON_Interval closed_domain = surface->Domain(info.closed_dir);
    const ON_Interval open_domain = surface->Domain(info.open_dir);
    const double parameter_tolerance = std::max(tolerance,
	open_domain.Length() * 1.0e-4);
    if (fabs(info.open_coordinate - open_domain.Min()) >
	    parameter_tolerance &&
	    fabs(info.open_coordinate - open_domain.Max()) >
	    parameter_tolerance)
	return false;

    const fast_periodic_rectangle rectangle = {info.closed_dir,
	closed_domain, open_domain};
    if (!fast_build_periodic_rectangle(surface, rectangle, scratch, boundary))
	return false;
    recovery = {fast_periodic_boundary_kind::rectangle, info.closed_dir, boundary_index};
    return true;
}

static bool
fast_reconstruct_periodic_strip_domain(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double tolerance, fast_face_scratch &scratch, fast_periodic_recovery &recovery)
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
	const fast_uv_bounds outer_bounds(*brep_loop_points[outer_index]);
	const fast_uv_bounds inner_bounds(*brep_loop_points[inner_index]);
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
	const double outer_sample_open = outer_bounds.midpoint(open_dir);
	const double inner_sample_open = inner_bounds.midpoint(open_dir);
	const bool outer_spans_period =
	    fabs(fabs(outer_delta) - period) <= parameter_tolerance ||
	    outer_bounds.span(closed_dir) >= 0.90 * period ||
	    trim_matches_periodic_isocurve(outer_trim, outer_sample_open);
	const bool inner_spans_period =
	    fabs(fabs(inner_delta) - period) <= parameter_tolerance ||
	    inner_bounds.span(closed_dir) >= 0.90 * period ||
	    trim_matches_periodic_isocurve(inner_trim, inner_sample_open);
	const double isocurve_tolerance = std::max(parameter_tolerance,
	    open_length * 1.0e-3);
	if (!outer_spans_period || !inner_spans_period ||
		outer_bounds.span(open_dir) > isocurve_tolerance ||
		inner_bounds.span(open_dir) > isocurve_tolerance)
	    continue;

	const double outer_open = outer_sample_open;
	const double inner_open = inner_sample_open;
	if (fabs(outer_open - inner_open) <= parameter_tolerance)
	    continue;
	const double closed_start = fabs(outer_delta) > 0.90 * period ?
	    outer_start[closed_dir] : outer_bounds.minimum[closed_dir];
	const double closed_end = closed_start +
	    (outer_delta < -0.90 * period ? -period : period);
	const fast_periodic_rectangle rectangle = {closed_dir,
	    ON_Interval(closed_start, closed_end),
	    ON_Interval(outer_open, inner_open)};
	if (!fast_build_periodic_rectangle(surface, rectangle, scratch,
		*brep_loop_points[outer_index]))
	    return false;
	brep_loop_points[inner_index]->Empty();
	recovery = {fast_periodic_boundary_kind::rectangle, closed_dir, outer_index};
	return true;
    }
    return false;
}

static bool
fast_constrained_periodic_band(
	const ON_SimpleArray<BrepTrimPoint> &outer,
	const ON_SimpleArray<BrepTrimPoint> &inner, int closed_dir,
	double period, double outer_open, double inner_open,
	ON_SimpleArray<BrepTrimPoint> &boundary)
{
    const ON_SimpleArray<BrepTrimPoint> *rings[2] = {&outer, &inner};
    for (const auto *ring : rings) {
	if (ring->Count() < 4 || (*ring)[0].source_id !=
		(*ring)[ring->Count() - 1].source_id)
	    return false;
	for (int point = 0; point < ring->Count(); ++point) {
	    if (!(*ring)[point].source_id)
		return false;
	}
    }
    /* Put the artificial seam at existing samples on both rings.  Splitting
     * a shared physical edge to choose another cut would require its
     * rigorous neighbor to be remeshed as well. */
    const double roundoff = 256.0 * std::numeric_limits<double>::epsilon() *
	std::max({period, fabs(outer[0].p2d[closed_dir]),
	    fabs(inner[0].p2d[closed_dir])});
    const auto periodic_offset = [period](double coordinate) {
	double offset = std::fmod(coordinate, period);
	return offset < 0.0 ? offset + period : offset;
    };
    std::vector<std::pair<double, int>> inner_phases;
    for (int point = 0; point < inner.Count() - 1; ++point)
	inner_phases.emplace_back(periodic_offset(inner[point].p2d[closed_dir]),
	    point);
    std::sort(inner_phases.begin(), inner_phases.end());
    int cuts[2] = {-1, -1};
    for (int first = 0; first < outer.Count() - 1 && cuts[0] < 0; ++first) {
	const auto next = std::lower_bound(inner_phases.begin(), inner_phases.end(),
	    std::make_pair(periodic_offset(outer[first].p2d[closed_dir]), -1));
	const size_t position = (size_t)(next - inner_phases.begin());
	const size_t candidates[2] = {position % inner_phases.size(),
	    (position + inner_phases.size() - 1) % inner_phases.size()};
	for (size_t candidate : candidates) {
	    const int second = inner_phases[candidate].second;
	    if (fabs(std::remainder(outer[first].p2d[closed_dir] -
		    inner[second].p2d[closed_dir], period)) <= roundoff) {
		cuts[0] = first;
		cuts[1] = second;
		break;
	    }
	}
    }
    if (cuts[0] < 0)
	return false;
    const double cut = outer[cuts[0]].p2d[closed_dir];
    ON_SimpleArray<BrepTrimPoint> candidate;
    for (int row = 0; row < 2; ++row) {
	const auto &ring = *rings[row];
	const int count = ring.Count() - 1;
	const int direction = row ? -1 : 1;
	const double winding = ring[count].p2d[closed_dir] -
	    ring[0].p2d[closed_dir];
	const int step = (winding > 0.0 ? 1 : -1) * direction;
	double previous = row ? cut + period : cut;
	for (int point = 0; point <= count; ++point) {
	    const int index = (cuts[row] + step * point + count) % count;
	    BrepTrimPoint sample = ring[index];
	    double parameter = cut + periodic_offset(sample.p2d[closed_dir] - cut);
	    if (!point || point == count)
		parameter = cut + ((row == 0 && point == count) ||
		    (row == 1 && point == 0) ? period : 0.0);
	    if (direction * (parameter - previous) < -roundoff)
		return false;
	    sample.p2d[closed_dir] = parameter;
	    sample.p2d[1 - closed_dir] = row ? inner_open : outer_open;
	    candidate.Append(sample);
	    previous = parameter;
	}
    }
    candidate.Append(candidate[0]);
    boundary = candidate;
    return true;
}

static bool
fast_reconstruct_periodic_boundary_loops(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double tolerance, const struct bn_tol *tol,
	fast_face_scratch &scratch, fast_periodic_recovery &recovery)
{
    if (!surface || face.LoopCount() != 2 || !brep_loop_points)
	return false;
    int outer_index = -1;
    int inner_index = -1;
    for (int li = 0; li < 2; ++li) {
	const ON_BrepLoop *loop = face.Loop(li);
	if (!loop || loop->TrimCount() < 1 || !brep_loop_points[li] ||
		brep_loop_points[li]->Count() < 2)
	    return false;
	for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	    const ON_BrepTrim *trim = loop->Trim(ti);
	    const ON_BrepTrim *next = loop->Trim(
		(ti + 1) % loop->TrimCount());
	    if (!trim || !next || !trim->Edge() ||
		    trim->m_type == ON_BrepTrim::seam ||
		    trim->m_type == ON_BrepTrim::singular ||
		    trim->m_vi[1] != next->m_vi[0])
		return false;
	}
	if (loop->m_type == ON_BrepLoop::outer)
	    outer_index = li;
	else if (loop->m_type == ON_BrepLoop::inner)
	    inner_index = li;
    }
    if (outer_index < 0 || inner_index < 0)
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
	const double isocurve_tolerance = std::max(parameter_tolerance,
	    open_length * 1.0e-3);
	const bool transverse_closed = surface->IsClosed(open_dir);
	double open_coordinates[2] = {0.0, 0.0};
	int winding[2] = {0, 0};
	bool loops_valid = true;
	for (int li = 0; li < 2; ++li) {
	    const ON_SimpleArray<BrepTrimPoint> &points =
		*brep_loop_points[li];
	    double closed_min = INFINITY;
	    double closed_max = -INFINITY;
	    double open_min = INFINITY;
	    double open_max = -INFINITY;
	    double closed_coordinate = points[0].p2d[closed_dir];
	    for (int pi = 0; pi < points.Count(); ++pi) {
		if (!points[pi].p2d.IsValid()) {
		    loops_valid = false;
		    break;
		}
		double open_coordinate = points[pi].p2d[open_dir];
		if (transverse_closed) {
		    /* Equivalent copies on a doubly closed surface do not widen
		     * an isocurve.  Lift each walk continuously in its winding
		     * direction and place its transverse samples near one copy. */
		    open_coordinate -= std::round((open_coordinate -
			points[0].p2d[open_dir]) / open_length) * open_length;
		    if (pi) {
			const double step = std::remainder(
			    points[pi].p2d[closed_dir] -
			    points[pi - 1].p2d[closed_dir], period);
			/* Half-period steps do not identify a direction. */
			if (fabs(step) >= 0.5 * period - parameter_tolerance) {
			    loops_valid = false;
			    break;
			}
			closed_coordinate += step;
		    }
		} else {
		    closed_coordinate = points[pi].p2d[closed_dir];
		}
		closed_min = std::min(closed_min, closed_coordinate);
		closed_max = std::max(closed_max, closed_coordinate);
		open_min = std::min(open_min, open_coordinate);
		open_max = std::max(open_max, open_coordinate);
	    }
	    if (!loops_valid || closed_max - closed_min < 0.90 * period ||
		    closed_max - closed_min > 1.10 * period ||
		    open_max - open_min > isocurve_tolerance) {
		loops_valid = false;
		break;
	    }
	    open_coordinates[li] = 0.5 * (open_min + open_max);
	    if (transverse_closed) {
		const double delta = closed_coordinate - points[0].p2d[closed_dir];
		if (fabs(fabs(delta) - period) > parameter_tolerance) {
		    loops_valid = false;
		    break;
		}
		winding[li] = delta > 0.0 ? 1 : -1;
	    }
	}
	if (!loops_valid || fabs(open_coordinates[outer_index] -
		open_coordinates[inner_index]) <= isocurve_tolerance)
	    continue;

	const double closed_start = surface->Domain(closed_dir).Min();
	const double closed_end = surface->Domain(closed_dir).Max();
	const double outer_open = open_coordinates[outer_index];
	double inner_open = open_coordinates[inner_index];
	if (transverse_closed) {
	    if (winding[outer_index] != -winding[inner_index] || !tol ||
		    !(tol->dist > 0.0))
		continue;
	    /* The interior is to the left of the directed outer boundary.
	     * Winding, not the shortest periodic distance, selects the band. */
	    const int side = (closed_dir == 0 ? 1 : -1) * winding[outer_index];
	    double width = side * (inner_open - outer_open);
	    width -= std::floor(width / open_length) * open_length;
	    if (width <= isocurve_tolerance ||
		    open_length - width <= isocurve_tolerance)
		continue;
	    inner_open = outer_open + side * width;
	    /* Flatten only within model tolerance.  Imported pcurves may already
	     * differ from their edges by the source's declared edge tolerance;
	     * preserve that agreement without enlarging the allowed adjustment. */
	    for (int li = 0; loops_valid && li < 2; ++li) {
		const auto &points = *brep_loop_points[li];
		for (int pi = 0; pi < points.Count(); ++pi) {
		    const ON_2dPoint original_uv = UnwrapUVPoint(surface,
			points[pi].p2d, BREP_SAME_POINT_TOLERANCE);
		    const ON_3dPoint original = surface->PointAt(original_uv.x,
			original_uv.y);
		    ON_2dPoint uv = points[pi].p2d;
		    uv[open_dir] = li == outer_index ? outer_open : inner_open;
		    uv = UnwrapUVPoint(surface, uv, BREP_SAME_POINT_TOLERANCE);
		    const ON_3dPoint point = surface->PointAt(uv.x, uv.y);
		    double edge_tolerance = tol->dist;
		    const int ei = points[pi].edge_ind;
		    if (face.Brep() && ei >= 0 && ei < face.Brep()->m_E.Count()) {
			const double source_tolerance = face.Brep()->m_E[ei].m_tolerance;
			if (std::isfinite(source_tolerance))
			    edge_tolerance = std::max(edge_tolerance, source_tolerance);
		    }
		    if (!points[pi].p3d || !points[pi].p3d->IsValid() ||
			    !point.IsValid() || !original.IsValid() ||
			    point.DistanceTo(original) > tol->dist ||
			    point.DistanceTo(*points[pi].p3d) > edge_tolerance) {
			loops_valid = false;
			break;
		    }
		}
	    }
	    if (!loops_valid)
		continue;
	}
	const fast_periodic_rectangle rectangle = {closed_dir,
	    ON_Interval(closed_start, closed_end), ON_Interval(outer_open, inner_open)};
	ON_SimpleArray<BrepTrimPoint> replacement;
	const bool retained = transverse_closed &&
	    fast_constrained_periodic_band(*brep_loop_points[outer_index],
		*brep_loop_points[inner_index], closed_dir, period,
		outer_open, inner_open, replacement);
	if (!retained &&
	    !fast_build_periodic_rectangle(surface, rectangle, scratch, replacement))
	    return false;
	*brep_loop_points[outer_index] = replacement;
	brep_loop_points[inner_index]->Empty();
	recovery = {retained ? fast_periodic_boundary_kind::constrained_band :
	    fast_periodic_boundary_kind::rectangle, closed_dir, outer_index};
	return true;
    }
    return false;
}

static bool
fast_reconstruct_winding_periodic_strip(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	double tolerance, fast_face_scratch &scratch, fast_periodic_recovery &recovery)
{
    /* An essential inner loop may backtrack through many periods while its
     * net winding still matches a one-turn outer boundary.  Doubly periodic
     * surfaces can also encode the same strip with oppositely oriented cuts
     * whose other coordinate spans one period.  Neither is a planar hole.
     * Cut both loops at matching physical parameters and stitch them into one
     * outline in the universal cover. */
    if (!surface || face.LoopCount() != 2 || !brep_loop_points)
	return false;
    int outer_index = -1;
    int inner_index = -1;
    for (int li = 0; li < 2; ++li) {
	const ON_BrepLoop *loop = face.Loop(li);
	if (!loop || !brep_loop_points[li] ||
		brep_loop_points[li]->Count() < 2)
	    return false;
	if (loop->m_type == ON_BrepLoop::outer)
	    outer_index = li;
	else if (loop->m_type == ON_BrepLoop::inner)
	    inner_index = li;
    }
    if (outer_index < 0 || inner_index < 0)
	return false;

    const ON_BrepLoop *outer_loop = face.Loop(outer_index);
    const ON_BrepLoop *inner_loop = face.Loop(inner_index);
    if (!outer_loop || !inner_loop || inner_loop->TrimCount() < 2)
	return false;
    const ON_BrepLoop *candidate_loops[2] = {outer_loop, inner_loop};
    for (const ON_BrepLoop *candidate : candidate_loops) {
	for (int ti = 0; ti < candidate->TrimCount(); ++ti) {
	    const ON_BrepTrim *trim = candidate->Trim(ti);
	    const ON_BrepTrim *next = candidate->Trim(
		(ti + 1) % candidate->TrimCount());
	    if (!trim || !next || trim->m_vi[1] != next->m_vi[0])
		return false;
	}
    }

    fast_periodic_loop_info outer_info;
    ON_SimpleArray<BrepTrimPoint> &outer =
	*brep_loop_points[outer_index];
    ON_SimpleArray<BrepTrimPoint> &inner =
	*brep_loop_points[inner_index];
    if (!fast_periodic_boundary_loop(surface, outer, outer_info) ||
	    inner.Count() < 4)
	return false;

    const int closed_dir = outer_info.closed_dir;
    const int open_dir = outer_info.open_dir;
    const double period = surface->Domain(closed_dir).Length();
    const double open_length = surface->Domain(open_dir).Length();
    const double parameter_tolerance = std::max(tolerance,
	std::max(period, open_length) * 1.0e-4);
    /* Seam snapping may make the sampled endpoints numerically equal.  The
     * sum of the directed trim displacements retains the topological winding
     * and tells us which endpoint copy belongs in the universal cover. */
    double topology_delta = 0.0;
    for (int ti = 0; ti < inner_loop->TrimCount(); ++ti) {
	const ON_BrepTrim *trim = inner_loop->Trim(ti);
	const ON_Interval domain = trim->Domain();
	if (!domain.IsIncreasing())
	    return false;
	const ON_2dPoint start = trim->PointAt(domain.Min());
	const ON_2dPoint end = trim->PointAt(domain.Max());
	if (!start.IsValid() || !end.IsValid())
	    return false;
	topology_delta += end[closed_dir] - start[closed_dir];
    }
    double inner_delta =
	inner[inner.Count() - 1].p2d[closed_dir] -
	inner[0].p2d[closed_dir];
    const long topology_winding = std::lround(topology_delta / period);
    const long sampled_winding = std::lround(inner_delta / period);
    int inner_winding = 0;
    if (labs(topology_winding) == 1 &&
	    fabs(topology_delta - topology_winding * period) <=
		parameter_tolerance)
	inner_winding = topology_winding > 0 ? 1 : -1;
    else if (surface->IsClosed(open_dir) &&
	    sampled_winding == -outer_info.winding &&
	    fabs(inner_delta - sampled_winding * period) <=
		parameter_tolerance)
	inner_winding = sampled_winding > 0 ? 1 : -1;
    if (!inner_winding)
	return false;
    bool lift_inner_end = false;
    if (fabs(inner_delta) <= parameter_tolerance) {
	lift_inner_end = true;
	inner_delta = inner_winding * period;
    }
    const bool opposite_winding = inner_winding == -outer_info.winding;
    if ((inner_winding != outer_info.winding && !opposite_winding) ||
	    fabs(inner_delta - inner_winding * period) >
		parameter_tolerance ||
	    fabs(inner[inner.Count() - 1].p2d[open_dir] -
		inner[0].p2d[open_dir]) > parameter_tolerance)
	return false;
    if (outer_loop->TrimCount() != 1 && !opposite_winding)
	return false;

    double inner_open_min = INFINITY;
    double inner_open_max = -INFINITY;
    double inner_closed_min = INFINITY;
    double inner_closed_max = -INFINITY;
    for (int pi = 0; pi < inner.Count(); ++pi) {
	if (!inner[pi].p2d.IsValid())
	    return false;
	const double closed_coordinate =
	    lift_inner_end && pi == inner.Count() - 1 ?
	    inner[0].p2d[closed_dir] + inner_winding * period :
	    inner[pi].p2d[closed_dir];
	inner_closed_min = std::min(inner_closed_min,
	    closed_coordinate);
	inner_closed_max = std::max(inner_closed_max,
	    closed_coordinate);
	inner_open_min = std::min(inner_open_min,
	    inner[pi].p2d[open_dir]);
	inner_open_max = std::max(inner_open_max,
	    inner[pi].p2d[open_dir]);
    }
    if (!opposite_winding) {
	if (outer_info.open_coordinate >
		inner_open_min - parameter_tolerance &&
		outer_info.open_coordinate <
		    inner_open_max + parameter_tolerance)
	    return false;
    } else if (!surface->IsClosed(open_dir) ||
	    inner_open_max - inner_open_min < 0.90 * open_length ||
	    inner_open_max - inner_open_min > 1.10 * open_length) {
	return false;
    }

    if (inner_closed_max - inner_closed_min > 64.0 * period)
	return false;

    ON_SimpleArray<BrepTrimPoint> replacement = outer;
    if (opposite_winding) {
	const double shift = std::round((
	    outer[outer.Count() - 1].p2d[closed_dir] -
	    inner[0].p2d[closed_dir]) / period) * period;
	if (fabs(outer[outer.Count() - 1].p2d[closed_dir] -
		(inner[0].p2d[closed_dir] + shift)) >
		parameter_tolerance ||
	    fabs(outer[0].p2d[closed_dir] -
		(inner[inner.Count() - 1].p2d[closed_dir] + shift)) >
		parameter_tolerance)
	    return false;
	for (int pi = 0; pi < inner.Count(); ++pi) {
	    BrepTrimPoint point = inner[pi];
	    if (lift_inner_end && pi == inner.Count() - 1)
		point.p2d[closed_dir] = inner[0].p2d[closed_dir] +
		    inner_winding * period;
	    point.p2d[closed_dir] += shift;
	    if (!V2NEAR_EQUAL(replacement[replacement.Count() - 1].p2d,
		    point.p2d, BREP_SAME_POINT_TOLERANCE))
		replacement.Append(point);
	}
    } else {
	const double shift = std::round((outer[0].p2d[closed_dir] -
	    inner[0].p2d[closed_dir]) / period) * period;
	const double target_start = inner[0].p2d[closed_dir] + shift;
	if (!fast_rotate_periodic_boundary(surface, outer, closed_dir, period,
		target_start, scratch))
	    return false;
	replacement = outer;
	for (int pi = inner.Count() - 1; pi >= 0; --pi) {
	    BrepTrimPoint point = inner[pi];
	    if (lift_inner_end && pi == inner.Count() - 1)
		point.p2d[closed_dir] = inner[0].p2d[closed_dir] +
		    inner_winding * period;
	    point.p2d[closed_dir] += shift;
	    if (!V2NEAR_EQUAL(replacement[replacement.Count() - 1].p2d,
		    point.p2d, BREP_SAME_POINT_TOLERANCE))
		replacement.Append(point);
	}
    }
    if (replacement.Count() < 6)
	return false;
    outer = replacement;
    inner.Empty();
    recovery = {fast_periodic_boundary_kind::stitched, closed_dir, outer_index};
    return true;
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
    fast_trim_index trim_index;
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
    ON_SimpleArray<BrepTrimPoint> **brep_loop_points = loop_points.data();

    // first simply load loop point samples
    for (int li = 0; li < loop_cnt; li++) {
	const ON_BrepLoop *loop = face.Loop(li);
	get_loop_sample_points(brep_loop_points[li], face, loop, max_dist,
		ttol, tol, scratch, model_diagonal, false, NULL);
    }
    if (scratch.hit_sample_limit) {
	return;
    }

    fast_bridge_store bridgePoints;
    if (s->IsClosed(0) || s->IsClosed(1)) {
	PerformClosedSurfaceChecks(s, face, ttol, tol, brep_loop_points,
	    BREP_SAME_POINT_TOLERANCE, bridgePoints);
    }
    if (bridgePoints.invalid)
	return;
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
	    trim_index.add_boundary(*brep_loop_points[li], uv_closed);
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

    trim_index.append_interior(tpnts, on_surf_points, tol->dist);
    trim_index.clear();

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

static int
fast_closed_surface_implicit_outer_loop(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	const struct bn_tol *tol)
{
    if (!surface || !brep_loop_points ||
	    (!surface->IsClosed(0) && !surface->IsClosed(1)))
	return -1;
    for (int li = 0; li < face.LoopCount(); ++li) {
	const ON_BrepLoop *loop = face.Loop(li);
	if (loop && loop->m_type == ON_BrepLoop::outer)
	    return -1;
    }

    int largest_index = -1;
    long double largest_area = 0.0L;
    for (int li = 0; li < face.LoopCount(); ++li) {
	const ON_BrepLoop *loop = face.Loop(li);
	if (!loop || fast_loop_is_provably_degenerate(surface, loop, tol))
	    continue;
	ON_SimpleArray<BrepTrimPoint> *points = brep_loop_points[li];
	if (!points || points->Count() < 3 ||
		!fast_boundary_has_resolvable_area(*points))
	    continue;
	const long double area = fast_boundary_signed_area(*points);
	if (std::isfinite(area) && area > largest_area) {
	    largest_area = area;
	    largest_index = li;
	}
    }
    if (largest_index < 0 || !(largest_area > 0.0))
	return -1;

    ON_SimpleArray<BrepTrimPoint> &candidate =
	*brep_loop_points[largest_index];
    for (int li = 0; li < face.LoopCount(); ++li) {
	if (li == largest_index)
	    continue;
	const ON_BrepLoop *loop = face.Loop(li);
	if (!loop || !brep_loop_points[li] ||
		brep_loop_points[li]->Count() < 3 ||
		!fast_boundary_has_resolvable_area(*brep_loop_points[li]))
	    return -1;
	if (!(fast_boundary_signed_area(*brep_loop_points[li]) < 0.0L))
	    return -1;
	if (!PointInPolygon((*brep_loop_points[li])[0].p2d, candidate))
	    return -1;
    }
    return largest_index;
}


static long double
fast_boundary_signed_area(const ON_SimpleArray<BrepTrimPoint> &points)
{
    if (points.Count() < 3)
	return 0.0L;
    const ON_2dPoint &origin = points[0].p2d;
    long double twice_area = 0.0L;
    for (int i = 1; i + 1 < points.Count(); ++i) {
	const long double first_x = points[i].p2d.x - origin.x;
	const long double first_y = points[i].p2d.y - origin.y;
	const long double second_x = points[i + 1].p2d.x - origin.x;
	const long double second_y = points[i + 1].p2d.y - origin.y;
	twice_area += first_x * second_y - second_x * first_y;
    }
    return 0.5L * twice_area;
}


static bool
fast_boundary_has_resolvable_area(
	const ON_SimpleArray<BrepTrimPoint> &points)
{
    if (points.Count() < 3)
	return false;
    const ON_2dPoint &origin = points[0].p2d;
    long double twice_area = 0.0L;
    long double term_magnitude = 0.0L;
    for (int i = 1; i + 1 < points.Count(); ++i) {
	const long double first_x = points[i].p2d.x - origin.x;
	const long double first_y = points[i].p2d.y - origin.y;
	const long double second_x = points[i + 1].p2d.x - origin.x;
	const long double second_y = points[i + 1].p2d.y - origin.y;
	const long double cross = first_x * second_y - second_x * first_y;
	twice_area += cross;
	term_magnitude += std::abs(cross);
    }
    if (!std::isfinite(twice_area) || term_magnitude <= 0.0L)
	return false;
    const long double roundoff = 64.0L *
	std::numeric_limits<long double>::epsilon() * term_magnitude;
    return std::abs(twice_area) > roundoff;
}

enum fast_boundary_inference {
    FAST_BOUNDARY_NOT_APPLICABLE = 0,
    FAST_BOUNDARY_RESOLVED,
    FAST_BOUNDARY_MISSING_OUTER,
    FAST_BOUNDARY_ORPHANED_HOLE,
    FAST_BOUNDARY_CONFLICTING_NESTING
};

static fast_boundary_inference
fast_infer_inner_boundary_roles(const ON_Surface *surface,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> **brep_loop_points,
	const struct bn_tol *tol, std::vector<bool> &usable)
{
    if (!surface || !brep_loop_points || face.LoopCount() < 1 ||
	    surface->IsClosed(0) || surface->IsClosed(1))
	return FAST_BOUNDARY_NOT_APPLICABLE;

    const int loop_count = face.LoopCount();
    usable.assign((size_t)loop_count, false);
    std::vector<long double> areas((size_t)loop_count, 0.0L);
    bool have_boundary = false;
    bool have_outer = false;
    for (int li = 0; li < loop_count; ++li) {
	const ON_BrepLoop *loop = face.Loop(li);
	if (!loop || !brep_loop_points[li] ||
		loop->m_type != ON_BrepLoop::inner)
	    return FAST_BOUNDARY_NOT_APPLICABLE;
	if (fast_loop_is_provably_degenerate(surface, loop, tol))
	    continue;
	if (!fast_boundary_has_resolvable_area(*brep_loop_points[li]) ||
		!fast_loop_uv_closed(surface, loop, *brep_loop_points[li],
		    tol))
	    return FAST_BOUNDARY_NOT_APPLICABLE;
	have_boundary = true;
	areas[(size_t)li] =
	    fast_boundary_signed_area(*brep_loop_points[li]);
	have_outer = have_outer || areas[(size_t)li] > 0.0L;
    }
    if (!have_boundary)
	return FAST_BOUNDARY_NOT_APPLICABLE;
    if (!have_outer)
	return FAST_BOUNDARY_MISSING_OUTER;

    /* Loop type metadata may be wrong while geometric winding remains useful.
     * Each root must be an outline and every containment level must alternate
     * between filled and excluded regions. */
    for (int li = 0; li < loop_count; ++li) {
	const long double area = areas[(size_t)li];
	const long double area_magnitude = std::abs(area);
	if (!(area_magnitude > 0.0L))
	    continue;
	ON_SimpleArray<BrepTrimPoint> &points = *brep_loop_points[li];
	int parent = -1;
	long double parent_area =
	    std::numeric_limits<long double>::infinity();
	for (int oi = 0; oi < loop_count; ++oi) {
	    const long double outline_area = std::abs(areas[(size_t)oi]);
	    if (!(outline_area > area_magnitude) ||
		    !(outline_area < parent_area))
		continue;
	    ON_SimpleArray<BrepTrimPoint> &outline =
		*brep_loop_points[oi];
	    if (PointInPolygon(points[0].p2d, outline)) {
		parent = oi;
		parent_area = outline_area;
	    }
	}
	if (parent < 0 && area < 0.0L)
	    return FAST_BOUNDARY_ORPHANED_HOLE;
	if (parent >= 0 && (area > 0.0L) ==
		(areas[(size_t)parent] > 0.0L))
	    return FAST_BOUNDARY_CONFLICTING_NESTING;
	usable[(size_t)li] = true;
    }
    return FAST_BOUNDARY_RESOLVED;
}


static bool
fast_split_touching_periodic_loop(const ON_Surface *surface,
	const ON_BrepFace &face, ON_SimpleArray<BrepTrimPoint> &points,
	ON_SimpleArray<BrepTrimPoint> &hole)
{
    /* Some converted periodic faces encode an outer boundary and a cutout as
     * one topological loop: each closed walk starts at the same seam vertex.
     * The resulting figure eight is not a valid triangulation outline.  Split
     * the two walks before the Clipper fallback so it can resolve the shared
     * boundary point as an outer polygon plus a hole. */
    if (!surface || face.LoopCount() != 1 || points.Count() < 6 ||
	    (!surface->IsClosed(0) && !surface->IsClosed(1)))
	return false;
    const ON_BrepLoop *loop = face.Loop(0);
    if (!loop || loop->m_type != ON_BrepLoop::outer ||
	    loop->TrimCount() < 4)
	return false;
    const ON_BrepTrim *first_trim = loop->Trim(0);
    if (!first_trim)
	return false;
    const int start_vertex = first_trim->m_vi[0];
    int split_trim_index = -1;
    for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	const ON_BrepTrim *trim = loop->Trim(ti);
	const ON_BrepTrim *next = loop->Trim(
	    (ti + 1) % loop->TrimCount());
	if (!trim || !next || trim->m_vi[1] != next->m_vi[0])
	    return false;
	if (ti < loop->TrimCount() - 1 && trim->m_vi[1] == start_vertex) {
	    if (split_trim_index >= 0)
		return false;
	    split_trim_index = ti + 1;
	}
    }
    if (split_trim_index <= 0)
	return false;

    const ON_BrepTrim *split_trim = loop->Trim(split_trim_index);
    int split_point_index = -1;
    for (int pi = 1; pi < points.Count() - 1; ++pi) {
	if (split_trim && points[pi].trim_ind == split_trim->m_trim_index) {
	    split_point_index = pi;
	    break;
	}
    }
    if (split_point_index < 2 ||
	    points.Count() - split_point_index < 3)
	return false;
    if (!V2NEAR_EQUAL(points[0].p2d, points[split_point_index].p2d,
	    BREP_SAME_POINT_TOLERANCE))
	return false;

    ON_SimpleArray<BrepTrimPoint> first_cycle;
    ON_SimpleArray<BrepTrimPoint> second_cycle;
    for (int pi = 0; pi <= split_point_index; ++pi)
	first_cycle.Append(points[pi]);
    for (int pi = split_point_index; pi < points.Count(); ++pi)
	second_cycle.Append(points[pi]);

    const long double first_area = fast_boundary_signed_area(first_cycle);
    const long double second_area = fast_boundary_signed_area(second_cycle);
    const double parameter_scale = std::max(1.0,
	std::max(surface->Domain(0).Length(),
	    surface->Domain(1).Length()));
    const double minimum_cycle_area =
	BREP_SAME_POINT_TOLERANCE * parameter_scale;
    if (!std::isfinite(first_area) || !std::isfinite(second_area) ||
	    std::abs(first_area) <= minimum_cycle_area ||
	    std::abs(second_area) <= minimum_cycle_area ||
	    first_area * second_area >= 0.0)
	return false;

    ON_SimpleArray<BrepTrimPoint> *outer = &first_cycle;
    ON_SimpleArray<BrepTrimPoint> *inner = &second_cycle;
    if (std::abs(second_area) > std::abs(first_area)) {
	outer = &second_cycle;
	inner = &first_cycle;
    }
    ON_2dPoint centroid(0.0, 0.0);
    for (int pi = 0; pi < inner->Count(); ++pi)
	centroid += (*inner)[pi].p2d;
    centroid /= (double)inner->Count();
    if (!PointInPolygon(centroid, *outer))
	return false;

    points = *outer;
    hole = *inner;
    return true;
}

static bool
fast_split_bridged_inner_loop(const ON_Surface *surface,
	const ON_BrepLoop *loop, ON_SimpleArray<BrepTrimPoint> &points,
	std::vector<ON_SimpleArray<BrepTrimPoint>> &extra_holes)
{
    /* STEP conversion can encode several hole rings as one Euler walk,
     * connected by edges which are immediately retraced.  Detria requires
     * disjoint simple hole constraints.  Extract the closed topological
     * cycles and discard the zero-area bridge cycles. */
    if (!surface || !loop || loop->m_type != ON_BrepLoop::inner ||
	    loop->TrimCount() < 4 || points.Count() < 6)
	return false;

    std::vector<int> path_vertices;
    std::vector<int> path_trims;
    std::vector<std::vector<int>> cycles;
    const ON_BrepTrim *first = loop->Trim(0);
    if (!first)
	return false;
    path_vertices.push_back(first->m_vi[0]);
    for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	const ON_BrepTrim *trim = loop->Trim(ti);
	if (!trim || path_vertices.empty() ||
		trim->m_vi[0] != path_vertices.back())
	    return false;
	path_trims.push_back(ti);
	std::vector<int>::const_iterator repeated = std::find(
	    path_vertices.begin(), path_vertices.end(), trim->m_vi[1]);
	if (repeated == path_vertices.end()) {
	    path_vertices.push_back(trim->m_vi[1]);
	    continue;
	}
	const size_t cycle_start = repeated - path_vertices.begin();
	cycles.emplace_back(path_trims.begin() + cycle_start,
	    path_trims.end());
	path_trims.resize(cycle_start);
	path_vertices.resize(cycle_start + 1);
    }
    if (!path_trims.empty() || cycles.size() < 2)
	return false;

    const double parameter_scale = std::max(1.0,
	std::max(surface->Domain(0).Length(),
	    surface->Domain(1).Length()));
    const double minimum_area =
	BREP_SAME_POINT_TOLERANCE * parameter_scale;
    std::vector<ON_SimpleArray<BrepTrimPoint>> nonzero_cycles;
    for (const std::vector<int> &cycle_trims : cycles) {
	ON_SimpleArray<BrepTrimPoint> cycle;
	for (int ti : cycle_trims) {
	    const ON_BrepTrim *trim = loop->Trim(ti);
	    if (!trim)
		return false;
	    bool found_trim = false;
	    for (int pi = 0; pi < points.Count(); ++pi) {
		if (points[pi].trim_ind != trim->m_trim_index)
		    continue;
		found_trim = true;
		if (!cycle.Count() || !V2NEAR_EQUAL(
			cycle[cycle.Count() - 1].p2d, points[pi].p2d,
			BREP_SAME_POINT_TOLERANCE))
		    cycle.Append(points[pi]);
	    }
	    if (!found_trim)
		return false;
	}
	if (cycle.Count() < 3)
	    continue;
	if (!V2NEAR_EQUAL(cycle[0].p2d,
		cycle[cycle.Count() - 1].p2d,
		BREP_SAME_POINT_TOLERANCE))
	    cycle.Append(cycle[0]);
	double twice_area = 0.0;
	for (int pi = 0; pi < cycle.Count() - 1; ++pi) {
	    twice_area += cycle[pi].p2d.x * cycle[pi + 1].p2d.y -
		cycle[pi + 1].p2d.x * cycle[pi].p2d.y;
	}
	if (std::isfinite(twice_area) &&
		fabs(0.5 * twice_area) > minimum_area)
	    nonzero_cycles.push_back(cycle);
    }
    if (nonzero_cycles.size() < 2)
	return false;
    points = nonzero_cycles[0];
    for (size_t ci = 1; ci < nonzero_cycles.size(); ++ci)
	extra_holes.push_back(nonzero_cycles[ci]);
    return true;
}

static bool
fast_face_report_set(struct bg_triangulation_report *report,
	enum bg_triangulation_reason reason, const char *message)
{
    if (report) {
	report->reason = reason;
	report->input_index = -1;
	bu_strlcpy(report->message, message ? message : "",
	    sizeof(report->message));
    }
    return reason == BG_TRIANGULATION_OK;
}

static void
fast_triangle_display_normals(const ON_3dPoint points[3],
	ON_3dVector normals[3])
{
    ON_3dVector geometric = ON_CrossProduct(points[1] - points[0],
	points[2] - points[0]);
    if (!geometric.IsValid() || !geometric.Unitize())
	return;
    /* Coarse chords and exact boundary samples can disagree with the
     * source tangent plane.  An opposing smooth normal cannot light the
     * realized facet consistently; use its geometric normal at that corner. */
    for (int corner = 0; corner < 3; ++corner) {
	if (!normals[corner].IsValid() ||
		!(geometric * normals[corner] > 0.0))
	    normals[corner] = geometric;
    }
}

int
cdt_test_fast_display_normals(void)
{
    for (bool reverse : {false, true}) {
	ON_3dPoint points[3] = {
	    ON_3dPoint(0, 0, 0), ON_3dPoint(2, 0, 0), ON_3dPoint(0, 3, 0)
	};
	if (reverse)
	    std::swap(points[1], points[2]);
	const double direction = reverse ? -1.0 : 1.0;
	const ON_3dVector expected(0, 0, direction);
	const ON_3dVector smooth(0, direction, direction);
	ON_3dVector normals[3] = {
	    smooth, ON_3dVector(0, direction, -direction), ON_3dVector(1, 0, 0)
	};
	fast_triangle_display_normals(points, normals);
	if (normals[0] != smooth || normals[1] != expected ||
		normals[2] != expected)
	    return 1;
	normals[0] = ON_3dVector::UnsetVector;
	fast_triangle_display_normals(points, normals);
	if (normals[0] != expected)
	    return 2;
	points[2] = points[1];
	normals[0] = smooth;
	fast_triangle_display_normals(points, normals);
	if (normals[0] != smooth)
	    return 3;
    }
    return 0;
}


struct fast_triangulation_deleter {
    void operator()(void *memory) const
    {
	bu_free(memory, "fast triangulation output");
    }
};

struct fast_boundary_recovery {
    fast_periodic_recovery periodic;
    int implicit_outer_index = -1;
    bool periodic_cap = false;
    bool split_touching_loop = false;
    ON_SimpleArray<BrepTrimPoint> touching_hole;
    std::vector<ON_SimpleArray<BrepTrimPoint>> bridged_holes;
    std::vector<ON_SimpleArray<BrepTrimPoint>> implicit_holes;
    fast_boundary_inference inference = FAST_BOUNDARY_NOT_APPLICABLE;
    std::vector<bool> usable;

    bool inferred_roles() const { return inference == FAST_BOUNDARY_RESOLVED; }
};

struct fast_face_samples {
    explicit fast_face_samples(int loop_count) : loops(loop_count) {}

    fast_face_scratch scratch;
    fast_loop_point_store loops;
    fast_bridge_store bridges;
    fast_surface_metrics metrics;
    fast_boundary_recovery recovery;
    ON_2dPointArray surface_points;
};

struct fast_face_chart {
    std::vector<detria::PointD> points;
    std::vector<int> outline;
    std::vector<std::vector<int>> inferred_outlines;
    std::vector<std::vector<int>> holes;
    std::unordered_map<int, BrepTrimPoint *> boundary_points;
    std::unordered_map<int, size_t> mesh_indices;
    size_t boundary_point_count = 0;
    bool direct = false;
    std::vector<int> cleaned_triangles;
};

static bool
fast_prepare_face_samples(fast_face_samples &samples, const ON_BrepFace &face,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol,
	double model_diagonal, bool repair_pcurves,
	const struct brep_cdt_fast_options *options, bool *skipped_tolerance,
	struct bg_triangulation_report *diagnostic)
{
    auto &scratch = samples.scratch;
    auto &metrics = samples.metrics;
    auto &on_surf_points = samples.surface_points;
    auto &recovery = samples.recovery;
    auto &periodic = recovery.periodic;
    const ON_Surface *s = &face;
    int fi = face.m_face_index;
    const bool debug_provenance = getenv(
	"BRLCAD_CDT_DEBUG_FAST_PROVENANCE") != NULL;
    const auto log_provenance = [&](const char *stage,
	    ON_SimpleArray<BrepTrimPoint> **loop_points, int loop_count) {
	if (!debug_provenance)
	    return;
	size_t total = 0;
	size_t identified = 0;
	for (int li = 0; li < loop_count; ++li) {
	    if (!loop_points[li])
		continue;
	    total += (size_t)loop_points[li]->Count();
	    for (int pi = 0; pi < loop_points[li]->Count(); ++pi)
		identified += (*loop_points[li])[pi].source_id ? 1 : 0;
	}
	bu_log("Fast face %d: %s retained %zu/%zu constraint identities\n",
	    fi, stage, identified, total);
    };

    fastf_t max_dist = 0.0;
    fast_surface_metrics_get(s, &metrics);
    if (metrics.size_valid) {
	if (metrics.width < tol->dist || metrics.height < tol->dist) {
	    *skipped_tolerance = true;
	    return false;
	}
	max_dist = sqrt(metrics.width * metrics.width +
	    metrics.height * metrics.height) / 10.0;
    }

    int loop_cnt = face.LoopCount();
    ON_SimpleArray<BrepTrimPoint> **brep_loop_points = samples.loops.data();

    // first simply load loop point samples
    for (int li = 0; li < loop_cnt; li++) {
	const ON_BrepLoop *loop = face.Loop(li);
	get_loop_sample_points(brep_loop_points[li], face, loop, max_dist,
		ttol, tol, scratch, model_diagonal, repair_pcurves, options);
    }
    log_provenance("sampling", brep_loop_points, loop_cnt);
    const bool untrimmed_domain_face = loop_cnt == 1 &&
	fast_reconstruct_untrimmed_domain_loop(s, face,
	    *brep_loop_points[0], scratch);
    if (scratch.hit_sample_limit || scratch.invalid_constraints) {
	return fast_face_report_set(diagnostic, BG_TRIANGULATION_INVALID_PSLG,
	    scratch.hit_sample_limit ? "trim sampling exceeded its point limit" :
	    "trim sampling produced invalid constraints");
    }
    if (!untrimmed_domain_face && (s->IsClosed(0) || s->IsClosed(1))) {
	for (int li = 0; li < loop_cnt; ++li) {
	    fast_reconstruct_periodic_trim_boundary(s, face.Loop(li),
		*brep_loop_points[li], BREP_SAME_POINT_TOLERANCE, scratch);
	    fast_normalize_periodic_trim_sequence(s, face.Loop(li),
		*brep_loop_points[li]);
	}
    }
    log_provenance("periodic normalization", brep_loop_points, loop_cnt);

    auto &bridgePoints = samples.bridges;
    if (!untrimmed_domain_face && (s->IsClosed(0) || s->IsClosed(1)))
	PerformClosedSurfaceChecks(s, face, ttol, tol, brep_loop_points,
	    BREP_SAME_POINT_TOLERANCE, bridgePoints);
    if (bridgePoints.invalid)
	return fast_face_report_set(diagnostic, BG_TRIANGULATION_INVALID_PSLG,
	    "periodic seam bridge sampling failed");
    log_provenance("closed surface checks", brep_loop_points, loop_cnt);
    auto &reconstructed_inner_holes = recovery.implicit_holes;
    if (untrimmed_domain_face && (s->IsClosed(0) || s->IsClosed(1))) {
	periodic = {fast_periodic_boundary_kind::rectangle,
	    s->IsClosed(0) ? 0 : 1, 0};
    } else if (!untrimmed_domain_face) {
	fast_reconstruct_full_periodic_face(s, face,
	    brep_loop_points, BREP_SAME_POINT_TOLERANCE, scratch,
	    periodic);
    }
    if (!untrimmed_domain_face && !periodic.recovered())
	fast_reconstruct_periodic_singular_face(s, face,
	    brep_loop_points, BREP_SAME_POINT_TOLERANCE, scratch,
	    periodic);
    if (!untrimmed_domain_face && !periodic.recovered())
	fast_reconstruct_paired_periodic_strip(s, face,
	    brep_loop_points, BREP_SAME_POINT_TOLERANCE, scratch,
	    periodic);
    /* Cap extension has its own winding test and runs at this point even
     * when an earlier recognizer succeeded.  Preserve that ordering. */
    recovery.periodic_cap = !untrimmed_domain_face &&
	fast_reconstruct_periodic_cap(s, face, brep_loop_points,
	    BREP_SAME_POINT_TOLERANCE, model_diagonal, scratch);
    if (!untrimmed_domain_face && !periodic.recovered())
	fast_reconstruct_periodic_strip_domain(s, face,
	    brep_loop_points, BREP_SAME_POINT_TOLERANCE, scratch,
	    periodic);
    if (!untrimmed_domain_face && !periodic.recovered())
	fast_reconstruct_periodic_boundary_loops(s, face,
	    brep_loop_points, BREP_SAME_POINT_TOLERANCE, tol, scratch,
	    periodic);
    if (!untrimmed_domain_face && !periodic.recovered())
	fast_reconstruct_winding_periodic_strip(s, face,
	    brep_loop_points, BREP_SAME_POINT_TOLERANCE, scratch,
	    periodic);
    if (!untrimmed_domain_face && !periodic.recovered())
	fast_reconstruct_implicit_periodic_domain(s,
	    face, brep_loop_points, tol, BREP_SAME_POINT_TOLERANCE, scratch,
	    periodic,
	    reconstructed_inner_holes);
    if (!untrimmed_domain_face && !periodic.recovered())
	fast_reconstruct_periodic_empty_boundary(s, face,
	    brep_loop_points, BREP_SAME_POINT_TOLERANCE, scratch,
	    periodic);
    if (!untrimmed_domain_face && !periodic.recovered())
	fast_reconstruct_periodic_strip(s, face, brep_loop_points,
	    model_diagonal, scratch, periodic);
    log_provenance("periodic reconstruction", brep_loop_points, loop_cnt);
    recovery.split_touching_loop = loop_cnt == 1 &&
	fast_split_touching_periodic_loop(s, face, *brep_loop_points[0],
	    recovery.touching_hole);
    auto &bridged_inner_holes = recovery.bridged_holes;
    for (int li = 0; li < loop_cnt; ++li) {
	const ON_BrepLoop *loop = face.Loop(li);
	if (loop && loop->m_type == ON_BrepLoop::inner &&
		li != periodic.outer_index)
	    fast_split_bridged_inner_loop(s, loop, *brep_loop_points[li],
		bridged_inner_holes);
    }
    recovery.implicit_outer_index =
	fast_closed_surface_implicit_outer_loop(s, face, brep_loop_points, tol);
    recovery.usable.assign((size_t)loop_cnt, false);
    if (recovery.implicit_outer_index < 0 && periodic.outer_index < 0 &&
	    !recovery.periodic_cap && !recovery.split_touching_loop &&
	    bridged_inner_holes.empty() && reconstructed_inner_holes.empty()) {
	recovery.inference = fast_infer_inner_boundary_roles(s, face,
	    brep_loop_points, tol, recovery.usable);
    }
    if (periodic.needs_grid() &&
	    !fast_sample_full_periodic_face(s,
	    *brep_loop_points[periodic.outer_index], ttol,
	    model_diagonal, periodic.closed_dir, on_surf_points, scratch))
	return fast_face_report_set(diagnostic, BG_TRIANGULATION_INVALID_INPUT,
	    "periodic surface boundary sampling failed");

    return true;
}


static bool
fast_prepare_face_chart(fast_face_chart &chart, fast_face_mesh &mesh,
	fast_face_samples &samples, const ON_BrepFace &face,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol,
	double model_diagonal, bool track_point_sources, bool *skipped_tolerance,
	struct bg_triangulation_report *diagnostic)
{
    fast_trim_index trim_index;
    const ON_Surface *s = &face;
    const int fi = face.m_face_index;
    const int loop_cnt = face.LoopCount();
    auto **brep_loop_points = samples.loops.data();
    auto &recovery = samples.recovery;
    const bool auto_detect_boundaries = recovery.inferred_roles();
    auto &on_surf_points = samples.surface_points;
    const auto &metrics = samples.metrics;
    // process through loops building polygons.
    bool have_outer = false;

    int sampled_outer_count = 0;
    int sampled_outer_index = -1;
    for (int li = 0; li < loop_cnt; ++li) {
	const ON_BrepLoop *face_loop = face.Loop(li);
	if (!face_loop || face_loop->m_type != ON_BrepLoop::outer ||
		fast_loop_is_provably_degenerate(s, face_loop, tol) ||
		!fast_boundary_has_resolvable_area(*brep_loop_points[li]))
	    continue;
	sampled_outer_count++;
	sampled_outer_index = li;
    }

    auto append_boundary = [&](ON_SimpleArray<BrepTrimPoint> &boundary_points,
	    bool uv_closed, bool is_outer, bool auto_detect = false) {
	std::vector<int> polyline;
	const int num_loop_points = boundary_points.Count();
	if (num_loop_points <= 2)
	    return true;
	const int first_point = uv_closed ? 1 : 0;
	for (int i = first_point; i < num_loop_points; i++) {
	    if (!boundary_points[i].p3d)
		return false;
	    // map point to last entry to 3d point
	    detria::PointD npt;
	    npt.x = boundary_points[i].p2d.x;
	    npt.y = boundary_points[i].p2d.y;
	    chart.points.push_back(npt);
	    chart.boundary_points[chart.points.size()-1] = &boundary_points[i];
	    polyline.push_back(chart.points.size()-1);
	    mesh.points.push_back(boundary_points[i].p3d->x);
	    mesh.points.push_back(boundary_points[i].p3d->y);
	    mesh.points.push_back(boundary_points[i].p3d->z);
	    if (track_point_sources)
		mesh.sources.push_back(boundary_points[i].source_id);
	    chart.mesh_indices[chart.points.size()-1] = mesh.points.size()/3 - 1;
	}
	trim_index.add_boundary(boundary_points, uv_closed);
	if (auto_detect) {
	    chart.inferred_outlines.push_back(polyline);
	} else if (is_outer) {
	    if (have_outer)
		return false;
	    chart.outline = polyline;
	    have_outer = true;
	} else {
	    chart.holes.push_back(polyline);
	}
	return true;
    };

    for (int li = 0; li < loop_cnt; li++) {
	const ON_BrepLoop *face_loop = face.Loop(li);
	if (!face_loop)
	    continue;
	if (auto_detect_boundaries && !recovery.usable[(size_t)li])
	    continue;
	/* Invalid imports sometimes retain a seam retrace as a second outer
	 * loop.  It bounds no chart area and must not displace the one usable
	 * outer boundary. */
	if (face_loop->m_type == ON_BrepLoop::outer &&
		sampled_outer_count == 1 && li != sampled_outer_index)
	    continue;
	if (face_loop->m_type != ON_BrepLoop::outer &&
		fast_loop_is_provably_degenerate(s, face_loop, tol))
	    continue;
	const bool uv_closed = fast_loop_uv_closed(s, face_loop,
	    *brep_loop_points[li], tol);
	const bool is_outer = face_loop->m_type == ON_BrepLoop::outer ||
	    (recovery.periodic_cap && li == 0) || li == recovery.implicit_outer_index ||
	    li == recovery.periodic.outer_index;
	if (!append_boundary(*brep_loop_points[li], uv_closed, is_outer,
		auto_detect_boundaries))
	    return fast_face_report_set(diagnostic,
		BG_TRIANGULATION_INVALID_PSLG,
		"a sampled trim boundary could not be assembled");
    }
    if (recovery.split_touching_loop) {
	const bool hole_uv_closed = V2NEAR_EQUAL(recovery.touching_hole[0].p2d,
	    recovery.touching_hole[recovery.touching_hole.Count() - 1].p2d,
	    BREP_SAME_POINT_TOLERANCE);
	if (!append_boundary(recovery.touching_hole, hole_uv_closed, false))
	    return fast_face_report_set(diagnostic,
		BG_TRIANGULATION_INVALID_PSLG,
		"a reconstructed hole boundary could not be assembled");
    }
    for (ON_SimpleArray<BrepTrimPoint> &hole : recovery.bridged_holes) {
	const bool uv_closed = V2NEAR_EQUAL(hole[0].p2d,
	    hole[hole.Count() - 1].p2d, BREP_SAME_POINT_TOLERANCE);
	if (!append_boundary(hole, uv_closed, false))
	    return fast_face_report_set(diagnostic,
		BG_TRIANGULATION_INVALID_PSLG,
		"a split inner boundary could not be assembled");
    }
    for (ON_SimpleArray<BrepTrimPoint> &hole : recovery.implicit_holes) {
	const bool uv_closed = V2NEAR_EQUAL(hole[0].p2d,
	    hole[hole.Count() - 1].p2d, BREP_SAME_POINT_TOLERANCE);
	if (!append_boundary(hole, uv_closed, false))
	    return fast_face_report_set(diagnostic,
		BG_TRIANGULATION_INVALID_PSLG,
		"an implicit inner boundary could not be assembled");
    }

    if (!have_outer && chart.inferred_outlines.empty()) {
	std::cerr << "Error: Face(" << fi << ") cannot evaluate its outer loop and will not be facetized." << std::endl;
	return fast_face_report_set(diagnostic, BG_TRIANGULATION_INVALID_NESTING,
	    recovery.inference == FAST_BOUNDARY_CONFLICTING_NESTING ?
	    "inferred boundary nesting has conflicting winding" :
	    recovery.inference == FAST_BOUNDARY_ORPHANED_HOLE ?
	    "an inner-oriented boundary lies outside every inferred outer" :
	    recovery.inference == FAST_BOUNDARY_MISSING_OUTER ?
	    "face has only inner-oriented boundaries and no outer domain" :
	    "face has no usable outer trim boundary");
    }
    if (have_outer && chart.outline.size() < 3) {
	/* The trim sampler closed a real outer loop with fewer than three
	 * distinct vertices.  This is a successful empty display result at the
	 * requested tolerance, not proof that the analytic face has zero area. */
	*skipped_tolerance = true;
	return false;
    }

    chart.boundary_point_count = chart.points.size();

    if (!recovery.periodic.needs_grid())
	getSurfacePoints(face, ttol, tol, on_surf_points, metrics,
	    model_diagonal);
    if (on_surf_points.Count() >= FAST_CDT_MAX_SURFACE_SAMPLES) {
	return fast_face_report_set(diagnostic, BG_TRIANGULATION_INVALID_INPUT,
	    "surface sampling exceeded its point limit");
    }

    // Not all surface point samples may end up being used in the triangulation,
    // so they are not added to the point map at this stage
    trim_index.append_interior(chart.points, on_surf_points, tol->dist);
    trim_index.clear();

    return true;
}


static bool
fast_triangulate_face_chart(fast_face_chart &chart,
	detria::Triangulation<detria::PointD, int> &tri, fast_face_mesh &mesh,
	struct bg_triangulation_report *diagnostic)
{
    const size_t boundary_point_count = chart.boundary_point_count;
    tri.setPoints(chart.points);
    if (!chart.inferred_outlines.empty()) {
	for (const std::vector<int> &polyline : chart.inferred_outlines)
	    tri.addPolylineAutoDetectType(polyline);
    } else {
	tri.addOutline(chart.outline);
	for (const std::vector<int> &hole : chart.holes)
	    tri.addHole(hole);
    }

    bool tri_success = false;
    bool tri_threw = false;
    {
	try {
	    tri_success = tri.triangulate(true);
	}
	catch (...) {
	    tri_success = false;
	    tri_threw = true;
	}
    }

    if (!tri_success) {
	if (!chart.inferred_outlines.empty()) {
	    return fast_face_report_set(diagnostic,
		BG_TRIANGULATION_DETRIA_FAILED, tri_threw ?
		"detria threw while nesting inferred boundaries" :
		tri.getErrorMessage().c_str());
	}
	/* The direct path deliberately avoids preprocessing overhead.  If its
	 * constraints are invalid, use libbg's Clipper-based sanitizer to merge
	 * duplicate UV points, resolve intersecting loops, and remove surface
	 * samples that lie on constrained edges. */
	std::vector<fastf_t> input_points(chart.points.size() * 2);
	for (size_t i = 0; i < chart.points.size(); i++) {
	    input_points[2 * i] = chart.points[i].x;
	    input_points[2 * i + 1] = chart.points[i].y;
	}
	std::vector<const int *> hole_arrays;
	std::vector<size_t> hole_counts;
	hole_arrays.reserve(chart.holes.size());
	hole_counts.reserve(chart.holes.size());
	for (const std::vector<int> &hole : chart.holes) {
	    hole_arrays.push_back(hole.data());
	    hole_counts.push_back(hole.size());
	}
	std::vector<int> steiner;
	steiner.reserve(chart.points.size() - boundary_point_count);
	for (size_t i = boundary_point_count; i < chart.points.size(); i++)
	    steiner.push_back((int)i);

	int *clean_faces = NULL;
	int clean_face_count = 0;
	point2d_t *clean_points = NULL;
	int clean_point_count = 0;
	const int clean_ret = bg_nested_poly_triangulate_clean(&clean_faces,
	    &clean_face_count, &clean_points, &clean_point_count,
	    chart.outline.data(), chart.outline.size(),
	    hole_arrays.empty() ? NULL : hole_arrays.data(),
	    hole_counts.empty() ? NULL : hole_counts.data(), chart.holes.size(),
	    steiner.empty() ? NULL : steiner.data(), steiner.size(),
	    NULL, 0,
	    (const point2d_t *)input_points.data(), chart.points.size(), diagnostic);
	std::unique_ptr<int, fast_triangulation_deleter> owned_faces(clean_faces);
	std::unique_ptr<point2d_t, fast_triangulation_deleter> owned_points(clean_points);
	if (clean_ret != BRLCAD_OK || !clean_faces || !clean_points ||
		clean_face_count <= 0 || clean_point_count <= 0 ||
		clean_point_count >= FAST_CDT_MAX_SURFACE_SAMPLES) {
	    if (clean_ret == BRLCAD_OK)
		fast_face_report_set(diagnostic,
		    BG_TRIANGULATION_POSTCONDITION_FAILED,
		    "cleaned chart returned incomplete output arrays");
	    return false;
	}

	chart.cleaned_triangles.assign(clean_faces,
	    clean_faces + (size_t)clean_face_count * 3);
	std::unordered_map<int, BrepTrimPoint *> clean_pointmap;
	std::unordered_map<int, size_t> clean_pind_map;
	double uv_min[2] = {INFINITY, INFINITY};
	double uv_max[2] = {-INFINITY, -INFINITY};
	for (const detria::PointD &point : chart.points) {
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
		if (!NEAR_EQUAL(clean_points[i][X], chart.points[j].x,
			uv_match_tol) ||
			!NEAR_EQUAL(clean_points[i][Y], chart.points[j].y,
			uv_match_tol))
		    continue;
		auto pfound = chart.boundary_points.find((int)j);
		if (pfound != chart.boundary_points.end())
		    clean_pointmap[i] = pfound->second;
		auto ifound = chart.mesh_indices.find((int)j);
		if (ifound != chart.mesh_indices.end())
		    clean_pind_map[i] = ifound->second;
		break;
	    }
	}
	chart.points.clear();
	chart.points.reserve((size_t)clean_point_count);
	for (int i = 0; i < clean_point_count; i++) {
	    detria::PointD p;
	    p.x = clean_points[i][X];
	    p.y = clean_points[i][Y];
	    chart.points.push_back(p);
	}

	/* Sanitization may replace and reorder boundary points.  Retain exact
	 * edge samples where possible; Clipper-created intersections are realized
	 * from their cleaned surface parameters. */
	mesh.faces.clear();
	mesh.normals.clear();
	chart.mesh_indices.swap(clean_pind_map);
	chart.boundary_points.swap(clean_pointmap);
    }

    chart.direct = tri_success;
    return true;
}


static bool
fast_realize_face_chart(fast_face_mesh &mesh, fast_face_chart &chart,
	detria::Triangulation<detria::PointD, int> &tri, const ON_BrepFace &face,
	double model_diagonal, bool track_point_sources,
	struct bg_triangulation_report *diagnostic)
{
    const ON_Surface *s = &face;
    std::map<int, size_t> singular_pind_map;
    std::vector<std::pair<ON_3dPoint, size_t>> seam_pind_map;
    struct fast_realized_vertex {
	ON_2dPoint uv;
	ON_3dPoint point;
	ON_3dVector normal;
	int singular_side = -1;
	bool seam_point = false;
	bool evaluated = false;
	bool valid = false;
    };
    std::vector<fast_realized_vertex> realized_vertices(chart.points.size());
    size_t attempted_triangles = 0;
    size_t surface_evaluation_failures = 0;
    size_t degenerate_triangles = 0;
    const bool surface_closed[2] = {s->IsClosed(0), s->IsClosed(1)};
    const ON_Interval surface_domains[2] = {s->Domain(0), s->Domain(1)};
    const auto unwrap_surface_point = [&](const ON_2dPoint &point) {
	ON_2dPoint unwrapped = point;
	for (int dir = 0; dir < 2; ++dir) {
	    if (surface_closed[dir])
		unwrapped[dir] = cdt_surface_parameter(unwrapped[dir],
		    surface_domains[dir]);
	}
	return unwrapped;
    };
    const auto point_is_on_seam = [&](const ON_2dPoint &point) {
	for (int dir = 0; dir < 2; ++dir) {
	    if (surface_closed[dir] &&
		    (NEAR_EQUAL(point[dir], surface_domains[dir][0],
			BREP_SAME_POINT_TOLERANCE) ||
		     NEAR_EQUAL(point[dir], surface_domains[dir][1],
			BREP_SAME_POINT_TOLERANCE)))
		return true;
	}
	return false;
    };
    auto emit_triangle = [&](int t0, int t1, int t2) {
	attempted_triangles++;
	/* Both chart triangulators return clockwise UV triangles.  Match the
	 * winding to the face-oriented surface normals emitted below. */
	const int tris[3] = {t0, face.m_bRev ? t1 : t2,
	    face.m_bRev ? t2 : t1};
	ON_3dPoint triangle_points[3];
	ON_3dVector triangle_normals[3];
	for (size_t j = 0; j < 3; j++) {
	    fast_realized_vertex &realized = realized_vertices[tris[j]];
	    if (!realized.evaluated) {
		realized.evaluated = true;
		realized.uv = unwrap_surface_point(ON_2dPoint(
		    chart.points[tris[j]].x, chart.points[tris[j]].y));
		std::unordered_map<int, BrepTrimPoint *>::const_iterator bt_it =
		    chart.boundary_points.find(tris[j]);
		if (!realized.uv.IsValid()) {
		    surface_evaluation_failures++;
		    return;
		}
		if (!surface_EvNormal(s, realized.uv.x, realized.uv.y,
			realized.point, realized.normal)) {
		    if (bt_it == chart.boundary_points.end() || !bt_it->second->p3d ||
			    bt_it->second->from_singular < 0 ||
			    !bt_it->second->normal.IsValid()) {
			surface_evaluation_failures++;
			return;
		    }
		    realized.point = *bt_it->second->p3d;
		    realized.normal = bt_it->second->normal;
		}
		realized.singular_side = IsAtSingularity(s, realized.uv.x,
		    realized.uv.y, BREP_SAME_POINT_TOLERANCE);
		realized.seam_point = point_is_on_seam(realized.uv);
		realized.valid = true;
	    }
	    if (!realized.valid) {
		surface_evaluation_failures++;
		return;
	    }
	    triangle_points[j] = realized.point;
	    triangle_normals[j] = realized.normal;
	}
	const ON_3dVector cross = ON_CrossProduct(
	    triangle_points[1] - triangle_points[0],
	    triangle_points[2] - triangle_points[0]);
	const double cross_length = cross.Length();
	if (!std::isfinite(cross_length) ||
		cross_length <= std::numeric_limits<double>::min()) {
	    degenerate_triangles++;
	    return;
	}

	for (size_t j = 0; j < 3; j++) {
	    const fast_realized_vertex &realized = realized_vertices[tris[j]];
	    const int singular_side = realized.singular_side;
	    const bool seam_point = realized.seam_point;
	    std::unordered_map<int, size_t>::const_iterator existing =
		chart.mesh_indices.find(tris[j]);
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
		mesh.faces.push_back(point_index);
	    } else if (existing != chart.mesh_indices.end()) {
		point_index = existing->second;
		mesh.faces.push_back(point_index);
		if (singular_side >= 0)
		    singular_pind_map[singular_side] = point_index;
	    } else {
		mesh.points.push_back(triangle_points[j].x);
		mesh.points.push_back(triangle_points[j].y);
		mesh.points.push_back(triangle_points[j].z);
		if (track_point_sources)
		    mesh.sources.push_back(NULL);
		chart.mesh_indices[tris[j]] = mesh.points.size()/3 - 1;
		point_index = chart.mesh_indices[tris[j]];
		mesh.faces.push_back(point_index);
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
		chart.boundary_points.find(tris[j]);
	    if (bt_it != chart.boundary_points.end() && bt_it->second->n3d)
		triangle_normals[j] = *(bt_it->second->n3d);
	    if (face.m_bRev)
		triangle_normals[j] = triangle_normals[j] * -1.0;
	}
	/* Reused edge, seam, and singular indices may resolve to coordinates
	 * different from the surface evaluations used during triangulation. */
	for (size_t corner = 0; corner < 3; ++corner)
	    triangle_points[corner] = ON_3dPoint(&mesh.points[(size_t)
		mesh.faces[mesh.faces.size() - 3 + corner] * 3]);
	fast_triangle_display_normals(triangle_points, triangle_normals);
	for (const ON_3dVector &normal : triangle_normals)
	    mesh.normals.insert(mesh.normals.end(), {normal.x, normal.y, normal.z});
    };

    if (chart.direct) {
	tri.forEachTriangle([&](const detria::Triangle<int> triangle) {
	    emit_triangle(triangle.x, triangle.y, triangle.z);
	}, true);
    } else {
	for (size_t i = 0; i < chart.cleaned_triangles.size(); i += 3)
	    emit_triangle(chart.cleaned_triangles[i], chart.cleaned_triangles[i + 1],
		chart.cleaned_triangles[i + 2]);
    }

    const bool valid_output = !mesh.faces.empty() && mesh.faces.size() % 3 == 0 &&
	mesh.points.size() % 3 == 0 && mesh.normals.size() == mesh.faces.size() * 3 &&
	(!track_point_sources || mesh.sources.size() == mesh.points.size() / 3);
    if (!valid_output) {
	char message[256];
	std::snprintf(message, sizeof(message),
	    "surface realization rejected %zu of %zu triangles "
	    "(%zu surface evaluations, %zu degenerate triangles)",
	    surface_evaluation_failures + degenerate_triangles,
	    attempted_triangles, surface_evaluation_failures,
	    degenerate_triangles);
	return fast_face_report_set(diagnostic,
	    BG_TRIANGULATION_POSTCONDITION_FAILED, message);
    }
    fast_face_report_set(diagnostic, BG_TRIANGULATION_OK,
	"face triangulation completed");
    return true;
}

static bool
bg_CDT_attempt(fast_face_mesh &mesh,
	const ON_BrepFace &face,
	const struct bg_tess_tol *ttol,
	const struct bn_tol *tol,
	double model_diagonal,
	bool repair_pcurves,
	const struct brep_cdt_fast_options *options,
	bool *skipped_tolerance,
	struct bg_triangulation_report *diagnostic)
{
    fast_face_report_set(diagnostic, BG_TRIANGULATION_INVALID_INPUT,
	"face triangulation did not reach constraint cleanup");
    *skipped_tolerance = false;
    const bool track_point_sources = options && options->point_source;
    fast_face_samples samples(face.LoopCount());
    if (!fast_prepare_face_samples(samples, face, ttol, tol, model_diagonal,
	repair_pcurves, options, skipped_tolerance, diagnostic))
	return false;
    fast_face_chart chart;
    if (!fast_prepare_face_chart(chart, mesh, samples, face, ttol, tol,
	model_diagonal, track_point_sources, skipped_tolerance, diagnostic))
	return false;
    detria::Triangulation<detria::PointD, int> tri;
    if (!fast_triangulate_face_chart(chart, tri, mesh, diagnostic))
	return false;
    return fast_realize_face_chart(mesh, chart, tri, face, model_diagonal,
	track_point_sources, diagnostic);
}

static bool
fast_loop_is_provably_degenerate(const ON_Surface *surface,
	const ON_BrepLoop *loop, const struct bn_tol *tol)
{
    if (!loop || !surface || loop->TrimCount() < 1)
	return false;
    if (loop->TrimCount() > 1) {
	/* Collinear trim loops have exactly zero parametric area.  Closed
	 * surfaces need additional checks because a coincident seam pair can
	 * bound an entire surface on the quotient domain.  Two native-UV
	 * segments which directly retrace one another are still zero area,
	 * provided neither one traverses a complete period. */
	const bool closed_surface =
	    surface->IsClosed(0) || surface->IsClosed(1);
	const double parameter_scale = std::max(1.0,
	    std::max(surface->Domain(0).Length(),
		surface->Domain(1).Length()));
	if (closed_surface && loop->TrimCount() != 2)
	    return false;
	if (closed_surface) {
	    const ON_BrepTrim *first = loop->Trim(0);
	    const ON_BrepTrim *second = loop->Trim(1);
	    if (!first || !second || first->m_type == ON_BrepTrim::seam ||
		    second->m_type == ON_BrepTrim::seam ||
		    first->m_ei == second->m_ei)
		return false;
	}
	std::vector<ON_2dPoint> endpoints;
	std::vector<ON_2dPoint> trim_starts;
	std::vector<ON_2dPoint> trim_ends;
	double trim_parameter_tolerance = 0.0;
	for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	    const ON_BrepTrim *candidate = loop->Trim(ti);
	    if (!candidate || candidate->m_type == ON_BrepTrim::singular ||
		    candidate->SpanCount() < 1 ||
		    candidate->SpanCount() > 4096)
		return false;
	    const double candidate_tolerance = std::max(
		BREP_SAME_POINT_TOLERANCE,
		std::min(std::max(candidate->m_tolerance[0],
			candidate->m_tolerance[1]),
		    parameter_scale * 1.0e-4));
	    /* Converted straight pcurves are often represented as high-degree
	     * NURBS with many collinear control points. */
	    if (candidate->Degree() != 1 &&
		    !candidate->IsLinear(candidate_tolerance))
		return false;
	    std::vector<double> spans(candidate->SpanCount() + 1);
	    if (!candidate->GetSpanVector(spans.data()))
		return false;
	    for (double parameter : spans) {
		const ON_2dPoint point = candidate->PointAt(parameter);
		if (!point.IsValid())
		    return false;
		endpoints.push_back(point);
	    }
	    trim_starts.push_back(endpoints[
		endpoints.size() - spans.size()]);
	    trim_ends.push_back(endpoints.back());
	    trim_parameter_tolerance = std::max(trim_parameter_tolerance,
		std::max(candidate->m_tolerance[0],
		    candidate->m_tolerance[1]));
	    if (closed_surface) {
		const ON_2dPoint &start = trim_starts.back();
		const ON_2dPoint &end = trim_ends.back();
		for (int dir = 0; dir < 2; ++dir) {
		    if (!surface->IsClosed(dir))
			continue;
		    const double period = surface->Domain(dir).Length();
		    const double period_tolerance = std::max(
			BREP_SAME_POINT_TOLERANCE, period * 1.0e-4);
		    if (period > ON_ZERO_TOLERANCE &&
			    fabs(fabs(end[dir] - start[dir]) - period) <=
			    period_tolerance)
			return false;
		}
	    }
	}
	trim_parameter_tolerance = std::min(trim_parameter_tolerance,
	    parameter_scale * 1.0e-4);
	const double parameter_tolerance = std::max(
	    trim_parameter_tolerance, std::max(BREP_SAME_POINT_TOLERANCE,
		parameter_scale * 1.0e-9));
	if (closed_surface &&
		(trim_starts[0].DistanceTo(trim_ends[1]) >
		    parameter_tolerance ||
		 trim_ends[0].DistanceTo(trim_starts[1]) >
		    parameter_tolerance))
	    return false;
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

	/* A trim tolerance describes the pcurve's agreement with its edge; it
	 * is not a model-space width.  On a strongly scaled surface, distinct
	 * drawable boundaries can be closer than that tolerance in UV.  Confirm
	 * the candidate collapse on the surface before discarding its face. */
	if (!tol || !std::isfinite(tol->dist) || !(tol->dist > 0.0))
	    return false;
	const double direction_squared = direction * direction;
	if (!(direction_squared > 0.0) || !std::isfinite(direction_squared))
	    return false;
	for (const ON_2dPoint &point : endpoints) {
	    const ON_2dVector offset = point - endpoints[0];
	    const double fraction = (offset * direction) / direction_squared;
	    const ON_2dPoint projected = endpoints[0] + fraction * direction;
	    const ON_3dPoint model_point = cdt_surface_point(surface, point);
	    const ON_3dPoint model_projection = cdt_surface_point(surface, projected);
	    if (!model_point.IsValid() || !model_projection.IsValid() ||
		    model_point.DistanceTo(model_projection) > tol->dist)
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

static bool
fast_face_is_provably_degenerate(const ON_BrepFace &face,
	const struct bn_tol *tol)
{
    if (face.LoopCount() < 1)
	return false;
    bool all_loops_empty = face.LoopCount() > 0;
    for (int li = 0; li < face.LoopCount(); ++li) {
	const ON_BrepLoop *candidate = face.Loop(li);
	if (!candidate || candidate->TrimCount() != 0) {
	    all_loops_empty = false;
	    break;
	}
    }
    if (all_loops_empty && face.LoopCount() > 1)
	return true;
    const ON_Surface *surface = face.SurfaceOf();
    for (int li = 0; li < face.LoopCount(); ++li) {
	const ON_BrepLoop *loop = face.Loop(li);
	if (!loop || loop->TrimCount() < 1 ||
		!fast_loop_is_provably_degenerate(surface, loop, tol))
	    return false;
    }
    return true;
}

static fast_face_outcome
bg_CDT(fast_face_mesh &mesh,
	const ON_BrepFace &face,
	const struct bg_tess_tol *ttol, const struct bn_tol *tol,
	double model_diagonal,
	const struct brep_cdt_fast_options *options,
	struct bg_triangulation_report *diagnostic)
{
    if (fast_face_is_provably_degenerate(face, tol)) {
	fast_face_report_set(diagnostic, BG_TRIANGULATION_OK,
	    "face is provably degenerate");
	return FAST_FACE_SKIPPED_DEGENERATE;
    }

    bool repair_first = false;
    for (int li = 0; !repair_first && li < face.LoopCount(); ++li) {
	const ON_BrepLoop *loop = face.Loop(li);
	if (!loop)
	    continue;
	for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	    const ON_BrepTrim *trim = loop->Trim(ti);
	    if (trim && fast_pullback_candidate(loop, *trim, tol,
		    model_diagonal)) {
		repair_first = true;
		break;
	    }
	}
    }

    bool first_skipped_tolerance = false;
    struct bg_triangulation_report first_diagnostic = {};
    if (bg_CDT_attempt(mesh, face, ttol, tol,
	    model_diagonal, repair_first, options,
	    &first_skipped_tolerance, &first_diagnostic)) {
	if (diagnostic)
	    *diagnostic = first_diagnostic;
	return FAST_FACE_COMPLETED;
    }

    mesh.clear();
    bool second_skipped_tolerance = false;
    struct bg_triangulation_report second_diagnostic = {};
    if (bg_CDT_attempt(mesh, face,
	    ttol, tol, model_diagonal, !repair_first, options,
	    &second_skipped_tolerance, &second_diagnostic)) {
	if (diagnostic)
	    *diagnostic = second_diagnostic;
	return FAST_FACE_COMPLETED;
    }
    mesh.clear();
    const auto diagnostic_priority = [](enum bg_triangulation_reason reason) {
	switch (reason) {
	    case BG_TRIANGULATION_POSTCONDITION_FAILED:
		return 5;
	    case BG_TRIANGULATION_DETRIA_FAILED:
		return 4;
	    case BG_TRIANGULATION_CROSSING_CONSTRAINTS:
	    case BG_TRIANGULATION_INVALID_NESTING:
	    case BG_TRIANGULATION_INVALID_PSLG:
		return 3;
	    case BG_TRIANGULATION_INVALID_INPUT:
		return 2;
	    default:
		return 1;
	}
    };
    if (diagnostic) {
	*diagnostic = diagnostic_priority(first_diagnostic.reason) >
	    diagnostic_priority(second_diagnostic.reason) ? first_diagnostic :
	    second_diagnostic;
    }
    return first_skipped_tolerance || second_skipped_tolerance ?
	FAST_FACE_SKIPPED_TOLERANCE : FAST_FACE_FAILED;
}


static void
fast_face_result_diagnostic(fast_cdt_face_result &result,
	const struct bg_triangulation_report &diagnostic)
{
    result.diagnostic_message = diagnostic.message;
    switch (diagnostic.reason) {
	case BG_TRIANGULATION_INVALID_PSLG:
	case BG_TRIANGULATION_CROSSING_CONSTRAINTS:
	case BG_TRIANGULATION_INVALID_NESTING:
	    result.diagnostic_result = BREP_CDT_RESULT_INVALID_PSLG;
	    result.diagnostic_stage = BREP_CDT_STAGE_PSLG_VALIDATION;
	    break;
	case BG_TRIANGULATION_DETRIA_FAILED:
	    result.diagnostic_result = BREP_CDT_RESULT_DETRIA_FAILED;
	    result.diagnostic_stage = BREP_CDT_STAGE_DETRIA;
	    break;
	case BG_TRIANGULATION_POSTCONDITION_FAILED:
	    result.diagnostic_result = BREP_CDT_RESULT_CERTIFICATION_FAILED;
	    result.diagnostic_stage = BREP_CDT_STAGE_FACE_TRIANGULATION;
	    break;
	case BG_TRIANGULATION_INVALID_INPUT:
	    result.diagnostic_result = BREP_CDT_RESULT_FACE_FAILED;
	    result.diagnostic_stage = BREP_CDT_STAGE_FACE_TRIANGULATION;
	    break;
	default:
	    result.diagnostic_result = BREP_CDT_RESULT_SUCCESS;
	    result.diagnostic_stage = BREP_CDT_STAGE_NONE;
	    break;
    }
}

fast_cdt_face_result
fast_evaluate_face(const ON_BrepFace &face, const struct bg_tess_tol *ttol,
	const struct bn_tol *tol, double model_diagonal,
	const struct brep_cdt_fast_options *options)
{
    fast_cdt_face_result result;
    struct bg_triangulation_report diagnostic = {};
    try {
	result.outcome = bg_CDT(result.mesh, face, ttol, tol,
	    model_diagonal, options, &diagnostic);
    } catch (const std::bad_alloc &) {
	result.outcome = FAST_FACE_MEMORY_LIMIT;
	fast_face_report_set(&diagnostic, BG_TRIANGULATION_INVALID_INPUT,
	    "face triangulation exhausted its memory allowance");
    } catch (...) {
	result.outcome = FAST_FACE_EXCEPTION;
	fast_face_report_set(&diagnostic, BG_TRIANGULATION_INVALID_INPUT,
	    "face triangulation threw an unexpected exception");
    }
    fast_face_result_diagnostic(result, diagnostic);
    if (result.failed())
	result.mesh.clear();
    return result;
}


/** @} */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
