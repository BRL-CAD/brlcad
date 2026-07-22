/*               P U L L B A C K C U R V E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2009-2026 United States Government as represented by
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
/** @file step/PullbackCurve.cpp
 *
 * Pull curve back into UV space from 3D space
 *
 */

#include "common.h"

#include "vmath.h"
#include "bn/dvec.h"

#include <assert.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <list>
#include <limits>
#include <chrono>
#include <functional>
#include <set>
#include <map>
#include <mutex>
#include <string>

#include "bu/parallel.h"
#include "brep/defines.h"
#include "brep/pullback.h"
#include "brep/ray.h"

/* interface header */
#include "PullbackCurve.h"

#define RANGE_HI 0.55
#define RANGE_LO 0.45
#define UNIVERSAL_SAMPLE_COUNT 1001

namespace {

/* Closest-point subdivision is a fallback search, not an acceptance
 * tolerance.  Bound it so a singular or malformed surface cannot recurse
 * indefinitely.  A power-of-two node budget keeps the search deterministic;
 * callers still accept a result only after the normal model-tolerance lift
 * check. */
constexpr size_t kMaximumClosestPointSubdivisionNodes = 4096;
constexpr int kMaximumClosestPointSubdivisionDepth = 32;
/* The legacy span-box closest-point search can legitimately reject every
 * span when trimming a numerically narrow NURBS domain does not produce a
 * usable conservative bounding box.  On that failure path, sample the knot
 * spans and refine only the best bounded set of seeds.  This is a search
 * budget, not an acceptance tolerance: every result must still lift to the
 * source point within the caller's model tolerance. */
constexpr size_t kMaximumClosestPointFallbackSeeds = 32;
constexpr int kClosestPointFallbackSamplesPerSpan = 3;
/* A collapsed periodic pullback is retried only after the ordinary adaptive
 * path has failed.  1024 bounded parameter intervals make each Newton seed
 * local even on the large analytic cylinders seen in assembly-scale files;
 * every recovered sample must still meet the caller's lift tolerance. */
constexpr size_t kPeriodicRecoverySegments = 1024;
/* STEP spline domains commonly contain a nominal zero such as -8.88e-16.
 * Require more than 1024 scaled machine epsilons on both sides before
 * splitting a periodic pullback at zero; otherwise the split creates a
 * one-sample, zero-length trim fragment. */
constexpr double kParameterZeroSplitEpsilonScale = 1024.0;
/* A UV chord on a curved surface can lift away from its exact 3-D edge even
 * when both endpoints project correctly.  Refine such intervals at their
 * exact curve midpoint, with explicit depth and sample ceilings.  The caller
 * subsequently performs its independent dense locus validation. */
constexpr int kMaximumAdaptivePullbackDepth = 12;
constexpr size_t kMaximumAdaptivePullbackSamples = 16384;
/* A seam-crossing search brackets one scalar curve parameter.  Sixty-four
 * interval refinements exceed the mantissa resolution of a double even when
 * the distance-weighted step is not a perfect bisection.  The previous
 * unbounded loop could approach an analytic seam asymptotically without ever
 * satisfying IsAtSeam(), consuming the enclosing item's entire work budget.
 * This is only a search ceiling; a result is accepted under the unchanged UV
 * seam and model-space lift tolerances. */
constexpr int kMaximumSeamCrossingIterations = 64;
thread_local size_t closest_point_subdivision_nodes = 0;
thread_local brlcad::PullbackCancellationCallback pullback_cancellation_callback = NULL;
thread_local void *pullback_cancellation_context = NULL;
thread_local std::chrono::steady_clock::time_point pullback_deadline =
    std::chrono::steady_clock::time_point::max();
thread_local bool pullback_deadline_expired = false;
thread_local std::string pullback_seam_failure;

}

#define _DEBUG_TESTING_
#ifdef _DEBUG_TESTING_
bool _debug_print_ = false;
bool _debug_print_mged2d_points_ = true;
bool _debug_print_mged3d_points_ = false;
int _debug_point_count_=0;
#endif

/* FIXME: duplicated with opennurbs_ext.cpp */
class BSpline
{
public:
    int p; // degree
    int m; // num_knots-1
    int n; // num_samples-1 (aka number of control points)
    std::vector<double> params;
    std::vector<double> knots;
    ON_2dPointArray controls;
};


bool
isFlat(const ON_2dPoint& p1, const ON_2dPoint& m, const ON_2dPoint& p2, double flatness)
{
    ON_Line line = ON_Line(ON_3dPoint(p1), ON_3dPoint(p2));
    return line.DistanceTo(ON_3dPoint(m)) <= flatness;
}


void
utah_ray_planes(const ON_Ray &r, ON_3dVector &p1, double &p1d, ON_3dVector &p2, double &p2d)
{
    ON_3dPoint ro(r.m_origin);
    ON_3dVector rdir(r.m_dir);
    double rdx, rdy, rdz;
    double rdxmag, rdymag, rdzmag;

    rdx = rdir.x;
    rdy = rdir.y;
    rdz = rdir.z;

    rdxmag = fabs(rdx);
    rdymag = fabs(rdy);
    rdzmag = fabs(rdz);

    if (rdxmag > rdymag && rdxmag > rdzmag)
	p1 = ON_3dVector(rdy, -rdx, 0);
    else
	p1 = ON_3dVector(0, rdz, -rdy);
    p1.Unitize();

    p2 = ON_CrossProduct(p1, rdir);

    p1d = -(p1 * ro);
    p2d = -(p2 * ro);
}


enum seam_direction
seam_direction(ON_2dPoint uv1, ON_2dPoint uv2)
{
    if (NEAR_EQUAL(uv1.x, 0.0, PBC_TOL) && NEAR_EQUAL(uv2.x, 0.0, PBC_TOL)) {
	return WEST_SEAM;
    } else if (NEAR_EQUAL(uv1.x, 1.0, PBC_TOL) && NEAR_EQUAL(uv2.x, 1.0, PBC_TOL)) {
	return EAST_SEAM;
    } else if (NEAR_EQUAL(uv1.y, 0.0, PBC_TOL) && NEAR_EQUAL(uv2.y, 0.0, PBC_TOL)) {
	return SOUTH_SEAM;
    } else if (NEAR_EQUAL(uv1.y, 1.0, PBC_TOL) && NEAR_EQUAL(uv2.y, 1.0, PBC_TOL)) {
	return NORTH_SEAM;
    } else {
	return UNKNOWN_SEAM_DIRECTION;
    }
}


bool
GetDomainSplits(
    const ON_Surface *surf,
    const ON_Interval &u_interval,
    const ON_Interval &v_interval,
    ON_Interval domSplits[2][2]
    )
{
    ON_3dPoint min, max;
    ON_Interval dom[2];
    double length[2];

    // initialize intervals
    for (int i = 0; i < 2; i++) {
	for (int j = 0; j < 2; j++) {
	    domSplits[i][j] = ON_Interval::EmptyInterval;
	}
    }

    min[0] = u_interval.m_t[0];
    min[1] = v_interval.m_t[0];
    max[0] = u_interval.m_t[1];
    max[1] = v_interval.m_t[1];

    for (int i=0; i<2; i++) {
	if (surf->IsClosed(i)) {
	    dom[i] = surf->Domain(i);
	    length[i] = dom[i].Length();
	    if ((max[i] - min[i]) >= length[i]) {
		domSplits[i][0] = dom[i];
	    } else {
		double minbound = min[i];
		double maxbound = max[i];
		while (minbound < dom[i].m_t[0]) {
		    minbound += length[i];
		    maxbound += length[i];
		}
		while (minbound > dom[i].m_t[1]) {
		    minbound -= length[i];
		    maxbound -= length[i];
		}
		if (maxbound > dom[i].m_t[1]) {
		    domSplits[i][0].m_t[0] = minbound;
		    domSplits[i][0].m_t[1] = dom[i].m_t[1];
		    domSplits[i][1].m_t[0] = dom[i].m_t[0];
		    domSplits[i][1].m_t[1] = maxbound - length[i];
		} else {
		    domSplits[i][0].m_t[0] = minbound;
		    domSplits[i][0].m_t[1] = maxbound;
		}
	    }
	} else {
	    domSplits[i][0].m_t[0] = min[i];
	    domSplits[i][0].m_t[1] = max[i];
	}
    }

    return true;
}


/*
 * Wrapped OpenNURBS 'EvNormal()' because it fails when at surface singularity
 * but not on edge of domain. If fails and at singularity this wrapper will
 * reevaluate at domain edge.
 */
bool
surface_EvNormal(// returns false if unable to evaluate
    const ON_Surface *surf,
    double s, double t, // evaluation parameters (s, t)
    ON_3dPoint& point,  // returns value of surface
    ON_3dVector& normal, // unit normal
    int side,       // optional - determines which side to evaluate from
    //         0 = default
    //         1 from NE quadrant
    //         2 from NW quadrant
    //         3 from SW quadrant
    //         4 from SE quadrant
    int* hint       // optional - evaluation hint (int[2]) used to speed
    //            repeated evaluations
    )
{
    bool rc = false;

    if (!(rc=surf->EvNormal(s, t, point, normal, side, hint))) {
	side = IsAtSingularity(surf, s, t, PBC_SEAM_TOL);// 0 = south, 1 = east, 2 = north, 3 = west
	if (side >= 0) {
	    ON_Interval u = surf->Domain(0);
	    ON_Interval v = surf->Domain(1);
	    if (side == 0) {
		rc=surf->EvNormal(u.m_t[0], v.m_t[0], point, normal, side, hint);
	    } else if (side == 1) {
		rc=surf->EvNormal(u.m_t[1], v.m_t[1], point, normal, side, hint);
	    } else if (side == 2) {
		rc=surf->EvNormal(u.m_t[1], v.m_t[1], point, normal, side, hint);
	    } else if (side == 3) {
		rc=surf->EvNormal(u.m_t[0], v.m_t[0], point, normal, side, hint);
	    }
	} else {
	    /*
	     * brute force and try to solve from each side of the surface domain
	     */
	    for(int iside=1; iside <= 4; iside++) {
		rc=surf->EvNormal(s, t, point, normal, iside, hint);
		if (rc)
		    break;
	    }
	}
    }
    return rc;
}


struct PullbackSurfaceScratch {
    ON_RevSurface *rev_surface;
    ON_NurbsSurface *nurbs_surface;
    ON_Extrusion *extr_surface;
    ON_PlaneSurface *plane_surface;
    ON_SumSurface *sum_surface;
    ON_SurfaceProxy *proxy_surface;

    PullbackSurfaceScratch()
	: rev_surface(ON_RevSurface::New()),
	  nurbs_surface(ON_NurbsSurface::New()),
	  extr_surface(new ON_Extrusion()),
	  plane_surface(new ON_PlaneSurface()),
	  sum_surface(ON_SumSurface::New()),
	  proxy_surface(new ON_SurfaceProxy())
    {
    }

    ~PullbackSurfaceScratch()
    {
	delete rev_surface;
	delete nurbs_surface;
	delete extr_surface;
	delete plane_surface;
	delete sum_surface;
	delete proxy_surface;
    }

    PullbackSurfaceScratch(const PullbackSurfaceScratch &) = delete;
    PullbackSurfaceScratch &operator=(const PullbackSurfaceScratch &) = delete;
};

static thread_local PullbackSurfaceScratch pullback_surface_scratch;

bool
surface_GetBoundingBox(
    const ON_Surface *surf,
    const ON_Interval &u_interval,
    const ON_Interval &v_interval,
    ON_BoundingBox& bbox,
    bool bGrowBox
    )
{
    PullbackSurfaceScratch &scratch = pullback_surface_scratch;

    ON_Interval domSplits[2][2] = { { ON_Interval::EmptyInterval, ON_Interval::EmptyInterval }, { ON_Interval::EmptyInterval, ON_Interval::EmptyInterval }};
    if (!GetDomainSplits(surf, u_interval, v_interval, domSplits)) {
	return false;
    }

    bool growcurrent = bGrowBox != 0;
    for (int i=0; i<2; i++) {
	if (domSplits[0][i] == ON_Interval::EmptyInterval)
	    continue;

	for (int j=0; j<2; j++) {
	    if (domSplits[1][j] != ON_Interval::EmptyInterval) {
		if (dynamic_cast<ON_RevSurface * >(const_cast<ON_Surface *>(surf)) != NULL) {
		    *scratch.rev_surface = *dynamic_cast<ON_RevSurface * >(const_cast<ON_Surface *>(surf));
		    if (scratch.rev_surface->Trim(0, domSplits[0][i]) && scratch.rev_surface->Trim(1, domSplits[1][j])) {
			if (!scratch.rev_surface->GetBoundingBox(bbox, growcurrent)) {
			    return false;
			}
			growcurrent = true;
		    }
		} else if (dynamic_cast<ON_NurbsSurface * >(const_cast<ON_Surface *>(surf)) != NULL) {
		    *scratch.nurbs_surface = *dynamic_cast<ON_NurbsSurface * >(const_cast<ON_Surface *>(surf));
		    if (scratch.nurbs_surface->Trim(0, domSplits[0][i]) && scratch.nurbs_surface->Trim(1, domSplits[1][j])) {
			if (!scratch.nurbs_surface->GetBoundingBox(bbox, growcurrent)) {
			    return false;
			}
		    }
		    growcurrent = true;
		} else if (dynamic_cast<ON_Extrusion * >(const_cast<ON_Surface *>(surf)) != NULL) {
		    *scratch.extr_surface = *dynamic_cast<ON_Extrusion * >(const_cast<ON_Surface *>(surf));
		    if (scratch.extr_surface->Trim(0, domSplits[0][i]) && scratch.extr_surface->Trim(1, domSplits[1][j])) {
			if (!scratch.extr_surface->GetBoundingBox(bbox, growcurrent)) {
			    return false;
			}
		    }
		    growcurrent = true;
		} else if (dynamic_cast<ON_PlaneSurface * >(const_cast<ON_Surface *>(surf)) != NULL) {
		    *scratch.plane_surface = *dynamic_cast<ON_PlaneSurface * >(const_cast<ON_Surface *>(surf));
		    if (scratch.plane_surface->Trim(0, domSplits[0][i]) && scratch.plane_surface->Trim(1, domSplits[1][j])) {
			if (!scratch.plane_surface->GetBoundingBox(bbox, growcurrent)) {
			    return false;
			}
		    }
		    growcurrent = true;
		} else if (dynamic_cast<ON_SumSurface * >(const_cast<ON_Surface *>(surf)) != NULL) {
		    *scratch.sum_surface = *dynamic_cast<ON_SumSurface * >(const_cast<ON_Surface *>(surf));
		    if (scratch.sum_surface->Trim(0, domSplits[0][i]) && scratch.sum_surface->Trim(1, domSplits[1][j])) {
			if (!scratch.sum_surface->GetBoundingBox(bbox, growcurrent)) {
			    return false;
			}
		    }
		    growcurrent = true;
		} else if (dynamic_cast<ON_SurfaceProxy * >(const_cast<ON_Surface *>(surf)) != NULL) {
		    *scratch.proxy_surface = *dynamic_cast<ON_SurfaceProxy * >(const_cast<ON_Surface *>(surf));
		    if (scratch.proxy_surface->Trim(0, domSplits[0][i]) && scratch.proxy_surface->Trim(1, domSplits[1][j])) {
			if (!scratch.proxy_surface->GetBoundingBox(bbox, growcurrent)) {
			    return false;
			}
		    }
		    growcurrent = true;
		} else {
		    std::cerr << "Unknown Surface Type" << std::endl;
		}
	    }
	}
    }

    return true;
}


bool
face_GetBoundingBox(
    const ON_BrepFace &face,
    ON_BoundingBox& bbox,
    bool bGrowBox
    )
{
    const ON_Surface *surf = face.SurfaceOf();

    // may be a smaller trimmed subset of surface so worth getting
    // face boundary
    bool growcurrent = bGrowBox != 0;
    // ON_Geometry::GetBoundingBox treats an invalid box input as
    // empty, which is what we want for the initial calculation
    ON_3dPoint min(DBL_MAX, DBL_MAX, DBL_MAX);
    ON_3dPoint max(-DBL_MAX, -DBL_MAX, -DBL_MAX);
    for (int li = 0; li < face.LoopCount(); li++) {
	for (int ti = 0; ti < face.Loop(li)->TrimCount(); ti++) {
	    ON_BrepTrim *trim = face.Loop(li)->Trim(ti);
	    trim->GetBoundingBox(min, max, growcurrent);
	    growcurrent = true;
	}
    }

    ON_Interval u_interval(min.x, max.x);
    ON_Interval v_interval(min.y, max.y);
    if (!surface_GetBoundingBox(surf, u_interval, v_interval, bbox, growcurrent)) {
	return false;
    }

    return true;
}


bool
surface_GetIntervalMinMaxDistance(
    const ON_3dPoint& p,
    const ON_BoundingBox &bbox,
    double &min_distance,
    double &max_distance
    )
{
    min_distance = bbox.MinimumDistanceTo(p);
    max_distance = bbox.MaximumDistanceTo(p);
    return true;
}


bool
surface_GetIntervalMinMaxDistance(
    const ON_Surface *surf,
    const ON_3dPoint& p,
    const ON_Interval &u_interval,
    const ON_Interval &v_interval,
    double &min_distance,
    double &max_distance
    )
{
    ON_BoundingBox bbox;

    if (surface_GetBoundingBox(surf, u_interval, v_interval, bbox, false)) {
	min_distance = bbox.MinimumDistanceTo(p);

	max_distance = bbox.MaximumDistanceTo(p);
	return true;
    }
    return false;
}


double
surface_GetOptimalNormalUSplit(const ON_Surface *surf, const ON_Interval &u_interval, const ON_Interval &v_interval, double tol)
{
    ON_3dVector normal[4];
    double u = u_interval.Mid();

    if ((normal[0] = surf->NormalAt(u_interval.m_t[0], v_interval.m_t[0])) &&
	(normal[2] = surf->NormalAt(u_interval.m_t[0], v_interval.m_t[1]))) {
	double step = u_interval.Length()/2.0;
	double stepdir = 1.0;
	u = u_interval.m_t[0] + stepdir * step;

	while (step > tol) {
	    if ((normal[1] = surf->NormalAt(u, v_interval.m_t[0])) &&
		(normal[3] = surf->NormalAt(u, v_interval.m_t[1]))) {
		double udot_1 = normal[0] * normal[1];
		double udot_2 = normal[2] * normal[3];
		if ((udot_1 < 0.0) || (udot_2 < 0.0)) {
		    stepdir = -1.0;
		} else {
		    stepdir = 1.0;
		}
		step = step / 2.0;
		u = u + stepdir * step;
	    }
	}
    }
    return u;
}


double
surface_GetOptimalNormalVSplit(const ON_Surface *surf, const ON_Interval &u_interval, const ON_Interval &v_interval, double tol)
{
    ON_3dVector normal[4];
    double v = v_interval.Mid();

    if ((normal[0] = surf->NormalAt(u_interval.m_t[0], v_interval.m_t[0])) &&
	(normal[1] = surf->NormalAt(u_interval.m_t[1], v_interval.m_t[0]))) {
	double step = v_interval.Length()/2.0;
	double stepdir = 1.0;
	v = v_interval.m_t[0] + stepdir * step;

	while (step > tol) {
	    if ((normal[2] = surf->NormalAt(u_interval.m_t[0], v)) &&
		(normal[3] = surf->NormalAt(u_interval.m_t[1], v))) {
		double vdot_1 = normal[0] * normal[2];
		double vdot_2 = normal[1] * normal[3];
		if ((vdot_1 < 0.0) || (vdot_2 < 0.0)) {
		    stepdir = -1.0;
		} else {
		    stepdir = 1.0;
		}
		step = step / 2.0;
		v = v + stepdir * step;
	    }
	}
    }
    return v;
}


//forward for cyclic
double
surface_GetClosestPoint3dFirstOrderByRange(const ON_Surface *surf,
					   const ON_3dPoint& p,
					   const ON_Interval& u_range,
					   const ON_Interval& v_range,
					   double current_closest_dist,
					   ON_2dPoint& p2d,
					   ON_3dPoint& p3d,
					   double same_point_tol,
					   double within_distance_tol,
					   int level);

double
surface_GetClosestPoint3dFirstOrderSubdivision(const ON_Surface *surf,
					       const ON_3dPoint& p,
					       const ON_Interval &u_interval,
					       double u,
					       const ON_Interval &v_interval,
					       double v,
					       double current_closest_dist,
					       ON_2dPoint& p2d,
					       ON_3dPoint& p3d,
					       double same_point_tol,
					       double within_distance_tol,
					       int level)
{
    if (brlcad::PullbackWorkCancelled() ||
	++closest_point_subdivision_nodes >
	    kMaximumClosestPointSubdivisionNodes ||
	    level > kMaximumClosestPointSubdivisionDepth)
	return current_closest_dist;

    double min_distance = 0;
    double max_distance = 0;
    ON_Interval new_u_interval = u_interval;
    ON_Interval new_v_interval = v_interval;

    for (int iu = 0; iu < 2; iu++) {
	new_u_interval.m_t[iu] = u_interval.m_t[iu];
	new_u_interval.m_t[1 - iu] = u;
	for (int iv = 0; iv < 2; iv++) {
	    new_v_interval.m_t[iv] = v_interval.m_t[iv];
	    new_v_interval.m_t[1 - iv] = v;
	    if (surface_GetIntervalMinMaxDistance(surf, p, new_u_interval, new_v_interval, min_distance, max_distance)) {
		double distance = DBL_MAX;
		if ((min_distance < current_closest_dist) && NEAR_ZERO(min_distance, within_distance_tol)) {
		    /////////////////////////////////////////
		    // Could check normals and CV angles here
		    /////////////////////////////////////////
		    ON_3dVector normal[4];
		    if ((normal[0] = surf->NormalAt(new_u_interval.m_t[0], new_v_interval.m_t[0])) &&
			(normal[1] = surf->NormalAt(new_u_interval.m_t[1], new_v_interval.m_t[0])) &&
			(normal[2] = surf->NormalAt(new_u_interval.m_t[0], new_v_interval.m_t[1])) &&
			(normal[3] = surf->NormalAt(new_u_interval.m_t[1], new_v_interval.m_t[1]))) {

			double udot_1 = normal[0] * normal[1];
			double udot_2 = normal[2] * normal[3];
			double vdot_1 = normal[0] * normal[2];
			double vdot_2 = normal[1] * normal[3];

			if ((udot_1 < 0.0) || (udot_2 < 0.0) || (vdot_1 < 0.0) || (vdot_2 < 0.0)) {
			    double u_split, v_split;
			    if ((udot_1 < 0.0) || (udot_2 < 0.0)) {
				//get optimal U split
				u_split = surface_GetOptimalNormalUSplit(surf, new_u_interval, new_v_interval, same_point_tol);
			    } else {
				u_split = new_u_interval.Mid();
			    }
			    if ((vdot_1 < 0.0) || (vdot_2 < 0.0)) {
				//get optimal V split
				v_split = surface_GetOptimalNormalVSplit(surf, new_u_interval, new_v_interval, same_point_tol);
			    } else {
				v_split = new_v_interval.Mid();
			    }
			    distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, new_u_interval, u_split, new_v_interval, v_split, current_closest_dist, p2d, p3d, same_point_tol, within_distance_tol, level + 1);
			    if (distance < current_closest_dist) {
				current_closest_dist = distance;
				if (current_closest_dist < same_point_tol)
				    return current_closest_dist;
			    }
			} else {
			    distance = surface_GetClosestPoint3dFirstOrderByRange(surf, p, new_u_interval, new_v_interval, current_closest_dist, p2d, p3d, same_point_tol, within_distance_tol, level + 1);
			    if (distance < current_closest_dist) {
				current_closest_dist = distance;
				if (current_closest_dist < same_point_tol)
				    return current_closest_dist;
			    }
			}
		    }
		}
	    }
	}
    }
    return current_closest_dist;
}


void
brlcad::SetPullbackWorkLimit(PullbackCancellationCallback cancellation_callback,
    void *cancellation_context, uint64_t maximum_elapsed_milliseconds)
{
    pullback_cancellation_callback = cancellation_callback;
    pullback_cancellation_context = cancellation_context;
    pullback_deadline_expired = false;
    pullback_deadline = maximum_elapsed_milliseconds ?
	std::chrono::steady_clock::now() +
	    std::chrono::milliseconds(maximum_elapsed_milliseconds) :
	std::chrono::steady_clock::time_point::max();
}


void
brlcad::ClearPullbackWorkLimit()
{
    pullback_cancellation_callback = NULL;
    pullback_cancellation_context = NULL;
    pullback_deadline = std::chrono::steady_clock::time_point::max();
    pullback_deadline_expired = false;
}


bool
brlcad::PullbackWorkCancelled()
{
    if (pullback_cancellation_callback &&
	    pullback_cancellation_callback(pullback_cancellation_context))
	return true;
    if (pullback_deadline != std::chrono::steady_clock::time_point::max() &&
	    std::chrono::steady_clock::now() >= pullback_deadline) {
	pullback_deadline_expired = true;
	return true;
    }
    return false;
}


bool
brlcad::PullbackWorkDeadlineExpired()
{
    return pullback_deadline_expired;
}


uint64_t
brlcad::PullbackWorkRemainingMilliseconds()
{
    if (pullback_deadline == std::chrono::steady_clock::time_point::max())
	return UINT64_MAX;
    const std::chrono::steady_clock::time_point now =
	std::chrono::steady_clock::now();
    if (now >= pullback_deadline)
	return 0;
    const uint64_t remaining = static_cast<uint64_t>(
	std::chrono::duration_cast<std::chrono::milliseconds>(
	    pullback_deadline - now).count());
    /* A positive sub-millisecond remainder must not be mistaken for the
     * zero value which SetPullbackWorkLimit uses to disable a deadline. */
    return std::max<uint64_t>(1, remaining);
}


/* Bounded damped Gauss-Newton refinement from a known UV seed.  Parameter
 * steps are limited relative to each surface domain, which is important for
 * STEP NURBS surfaces whose legal U interval may be only a few 1e-5 wide.
 * The routine reports its best finite candidate even when it does not meet
 * tolerance, allowing callers to compare several independent seeds. */
static bool
surface_closest_point_seeded(const ON_Surface *surf, const ON_3dPoint &point,
    const ON_2dPoint &seed, ON_2dPoint &surface_point,
    ON_3dPoint &lifted_point, double &distance, double tolerance,
    const bool *cached_closed = NULL,
    const ON_Interval *cached_domains = NULL,
    double refinement_tolerance = 0.0,
    uint64_t *completed_iterations = NULL,
    uint64_t *line_searches = NULL)
{
    if (!surf || !point.IsValid() || !seed.IsValid() || !(tolerance > 0.0))
	return false;

    /* ON_NurbsSurface::IsClosed() constructs and compares full boundary
     * curves; it is not a constant-time flag lookup.  This solver used to
     * repeat that work in every line-search reduction (up to 48*12 times for
     * one point).  Surface closure and domains are immutable for the job, so
     * evaluate them once per seeded solve. */
    const bool closed[2] = {
	cached_closed ? cached_closed[0] : surf->IsClosed(0),
	cached_closed ? cached_closed[1] : surf->IsClosed(1)
    };
    const ON_Interval domains[2] = {
	cached_domains ? cached_domains[0] : surf->Domain(0),
	cached_domains ? cached_domains[1] : surf->Domain(1)
    };
    const double maximum_steps[2] = {
	0.125 * domains[0].Length(), 0.125 * domains[1].Length()
    };
    const double convergence_tolerance = refinement_tolerance > 0.0 ?
	std::min(tolerance, refinement_tolerance) : tolerance;
    ON_2dPoint uv(seed);
    int evaluation_hint[2] = {0, 0};
    double best_distance = DBL_MAX;
    for (int iteration = 0; iteration < 48; ++iteration) {
	if (completed_iterations) ++*completed_iterations;
	if (brlcad::PullbackWorkCancelled()) break;
	ON_3dPoint lift;
	ON_3dVector du, dv;
	if (!surf->Ev1Der(uv.x, uv.y, lift, du, dv, 0,
		evaluation_hint) || !lift.IsValid())
	    break;
	const ON_3dVector residual = point - lift;
	const double current_distance = residual.Length();
	if (std::isfinite(current_distance) && current_distance < best_distance) {
	    best_distance = current_distance;
	    surface_point = uv;
	    lifted_point = lift;
	}
	if (current_distance <= convergence_tolerance) {
	    distance = current_distance;
	    return true;
	}

	const double a = du * du;
	const double b = du * dv;
	const double c = dv * dv;
	const double r0 = du * residual;
	const double r1 = dv * residual;
	const double metric_scale = std::max(1.0, std::max(a, c));
	const double damping = DBL_EPSILON * metric_scale * 64.0;
	const double aa = a + damping;
	const double cc = c + damping;
	const double determinant = aa * cc - b * b;
	if (!std::isfinite(determinant) ||
		fabs(determinant) <= DBL_EPSILON * metric_scale * metric_scale)
	    break;
	double delta[2] = {(cc * r0 - b * r1) / determinant,
	    (aa * r1 - b * r0) / determinant};
	double trust_scale = 1.0;
	for (int direction = 0; direction < 2; ++direction) {
	    const double maximum_step = maximum_steps[direction];
	    if (maximum_step > ON_ZERO_TOLERANCE &&
		    fabs(delta[direction]) > maximum_step)
		trust_scale = std::min(trust_scale,
		    maximum_step / fabs(delta[direction]));
	}

	bool improved = false;
	for (int reduction = 0; reduction < 12; ++reduction) {
	    if (line_searches) ++*line_searches;
	    const double scale = trust_scale * std::ldexp(1.0, -reduction);
	    ON_2dPoint candidate(uv.x + scale * delta[0],
		uv.y + scale * delta[1]);
	    for (int direction = 0; direction < 2; ++direction) {
		if (closed[direction]) continue;
		candidate[direction] = std::max(domains[direction].Min(),
		    std::min(domains[direction].Max(), candidate[direction]));
	    }
	    ON_3dPoint candidate_lift = ON_3dPoint::UnsetPoint;
	    if (!surf->EvPoint(candidate.x, candidate.y, candidate_lift, 0,
		    evaluation_hint) || !candidate_lift.IsValid() ||
		    candidate_lift.DistanceTo(point) >= current_distance)
		continue;
	    uv = candidate;
	    improved = true;
	    break;
	}
	if (!improved) break;
    }
    distance = best_distance;
    return best_distance <= tolerance;
}


/* Robust exact fallback for cases where span bounding boxes yielded no
 * candidate.  Three samples per knot span establish Newton basins without a
 * compiler- or model-size-dependent unbounded grid; only the nearest fixed
 * number of seeds are refined. */
static bool
surface_closest_point_multiseed(const ON_Surface *surf,
    const ON_3dPoint &point, ON_2dPoint &surface_point,
    ON_3dPoint &lifted_point, double &distance, double tolerance,
    double refinement_tolerance = 0.0,
    brlcad::PullbackStatistics *statistics = NULL,
    const bool *cached_closed = NULL,
    const ON_Interval *cached_domains = NULL,
    const std::vector<double> *cached_u_spans = NULL,
    const std::vector<double> *cached_v_spans = NULL,
    const std::vector<std::vector<ON_BoundingBox> > *cached_boxes = NULL)
{
    struct Candidate {
	double distance;
	ON_2dPoint uv;
    };

    if (!surf || !point.IsValid() || !(tolerance > 0.0))
	return false;

    const double primary_distance = distance;
    const double convergence_tolerance = refinement_tolerance > 0.0 ?
	std::min(tolerance, refinement_tolerance) : tolerance;
    if (statistics && std::isfinite(distance) && distance < DBL_MAX)
	++statistics->fallback_calls_with_finite_primary;

    const bool closed[2] = {
	cached_closed ? cached_closed[0] : surf->IsClosed(0),
	cached_closed ? cached_closed[1] : surf->IsClosed(1)
    };
    const ON_Interval domains[2] = {
	cached_domains ? cached_domains[0] : surf->Domain(0),
	cached_domains ? cached_domains[1] : surf->Domain(1)
    };
    std::vector<double> owned_spans[2];
    const std::vector<double> *spans[2] = {
	cached_u_spans, cached_v_spans
    };
    for (int direction = 0; direction < 2; ++direction) {
	if (spans[direction] && spans[direction]->size() >= 2) continue;
	const int count = surf->SpanCount(direction);
	if (count > 0) {
	    owned_spans[direction].resize(static_cast<size_t>(count) + 1);
	    if (!surf->GetSpanVector(direction,
		    owned_spans[direction].data()))
		owned_spans[direction].clear();
	}
	if (owned_spans[direction].size() < 2) {
	    const ON_Interval domain = surf->Domain(direction);
	    if (!domain.IsIncreasing()) return false;
	    owned_spans[direction].push_back(domain.Min());
	    owned_spans[direction].push_back(domain.Max());
	}
	spans[direction] = &owned_spans[direction];
    }

    const size_t u_count = spans[0]->size() - 1;
    const size_t v_count = spans[1]->size() - 1;
    struct SpanCandidate {
	size_t u;
	size_t v;
    };
    std::vector<SpanCandidate> span_candidates;
    const bool finite_primary = std::isfinite(primary_distance) &&
	primary_distance < DBL_MAX;
    if (finite_primary && cached_boxes && cached_boxes->size() == u_count) {
	/* The primary search produced a finite point, so its conservative span
	 * boxes are usable.  An exact point inside the requested tolerance can
	 * only lie in a box whose minimum distance is no larger than that
	 * tolerance.  Restrict the expensive PointAt sampling to those spans.
	 * This preserves the search result while avoiding a full tensor-product
	 * surface scan for every near-solved pullback sample. */
	for (size_t u = 0; u < u_count; ++u) {
	    if ((*cached_boxes)[u].size() != v_count) {
		span_candidates.clear();
		break;
	    }
	    for (size_t v = 0; v < v_count; ++v) {
		double minimum_distance = DBL_MAX;
		double maximum_distance = DBL_MAX;
		if (surface_GetIntervalMinMaxDistance(point,
			(*cached_boxes)[u][v], minimum_distance,
			maximum_distance) && minimum_distance <= tolerance)
		    span_candidates.push_back({u, v});
	    }
	}
	/* A finite primary point necessarily came from at least one accepted
	 * box.  If none remain, the primary is already the best result justified
	 * by the prepared conservative bounds. */
	if (span_candidates.empty()) return distance <= tolerance;
    }
    if (span_candidates.empty()) {
	span_candidates.reserve(u_count * v_count);
	for (size_t u = 0; u < u_count; ++u)
	    for (size_t v = 0; v < v_count; ++v)
		span_candidates.push_back({u, v});
    }

    std::vector<Candidate> candidates;
    candidates.reserve(span_candidates.size() *
	static_cast<size_t>(kClosestPointFallbackSamplesPerSpan) *
	static_cast<size_t>(kClosestPointFallbackSamplesPerSpan));

    for (const SpanCandidate &span : span_candidates) {
	const size_t u = span.u;
	const size_t v = span.v;
	    for (int ui = 0; ui < kClosestPointFallbackSamplesPerSpan; ++ui) {
		const double uf = static_cast<double>(ui) /
		    (kClosestPointFallbackSamplesPerSpan - 1);
		const double up = (1.0 - uf) * (*spans[0])[u] +
		    uf * (*spans[0])[u + 1];
		for (int vi = 0; vi < kClosestPointFallbackSamplesPerSpan; ++vi) {
		    if (brlcad::PullbackWorkCancelled()) return false;
		    if (statistics) ++statistics->fallback_samples_evaluated;
		    const double vf = static_cast<double>(vi) /
			(kClosestPointFallbackSamplesPerSpan - 1);
		    const double vp = (1.0 - vf) * (*spans[1])[v] +
			vf * (*spans[1])[v + 1];
		    const ON_3dPoint lift = surf->PointAt(up, vp);
		    if (!lift.IsValid()) continue;
		    const double candidate_distance = lift.DistanceTo(point);
		    if (!std::isfinite(candidate_distance)) continue;
		    if (candidate_distance < distance) {
			distance = candidate_distance;
			surface_point = ON_2dPoint(up, vp);
			lifted_point = lift;
		    }
		    if (candidate_distance <= convergence_tolerance) return true;
		    candidates.push_back({candidate_distance,
			ON_2dPoint(up, vp)});
		}
	    }
    }

    const double sampled_distance = distance;
    const size_t seed_count = std::min(candidates.size(),
	kMaximumClosestPointFallbackSeeds);
    const auto record_distance_improvements = [statistics, primary_distance,
	sampled_distance](double final_distance) {
	if (!statistics || !std::isfinite(final_distance) ||
		final_distance >= DBL_MAX)
	    return;
	if (std::isfinite(primary_distance) && primary_distance < DBL_MAX &&
		final_distance < primary_distance) {
	    const double improvement = primary_distance - final_distance;
	    statistics->fallback_primary_improvement_total += improvement;
	    statistics->fallback_primary_improvement_maximum = std::max(
		statistics->fallback_primary_improvement_maximum, improvement);
	}
	if (std::isfinite(sampled_distance) && sampled_distance < DBL_MAX &&
		final_distance < sampled_distance) {
	    const double improvement = sampled_distance - final_distance;
	    statistics->fallback_refinement_improvement_total += improvement;
	    statistics->fallback_refinement_improvement_maximum = std::max(
		statistics->fallback_refinement_improvement_maximum, improvement);
	}
    };
    const auto candidate_less = [](const Candidate &left,
	const Candidate &right) {
	if (left.distance < right.distance) return true;
	if (right.distance < left.distance) return false;
	if (left.uv.x < right.uv.x) return true;
	if (right.uv.x < left.uv.x) return false;
	return left.uv.y < right.uv.y;
    };
    /* Only the nearest bounded seed set is refined.  Sorting every sample on
     * a large multi-span surface was O(n log n) work whose tail was discarded
     * immediately.  Select the same deterministic nearest 32 in linear time,
     * then order just that prefix. */
    if (seed_count < candidates.size())
	std::nth_element(candidates.begin(), candidates.begin() + seed_count,
	    candidates.end(), candidate_less);
    std::sort(candidates.begin(), candidates.begin() + seed_count,
	candidate_less);
    size_t winning_seed = seed_count;
    for (size_t seed = 0; seed < seed_count; ++seed) {
	if (statistics) ++statistics->fallback_seed_refinements;
	ON_2dPoint candidate_uv = ON_2dPoint::UnsetPoint;
	ON_3dPoint candidate_lift = ON_3dPoint::UnsetPoint;
	double candidate_distance = DBL_MAX;
	const bool accepted = surface_closest_point_seeded(surf, point,
	    candidates[seed].uv, candidate_uv, candidate_lift,
	    candidate_distance, tolerance, closed, domains,
	    convergence_tolerance);
	if (candidate_distance < distance) {
	    distance = candidate_distance;
	    surface_point = candidate_uv;
	    lifted_point = candidate_lift;
	    winning_seed = seed;
	}
	if (accepted && candidate_distance <= convergence_tolerance) {
	    record_distance_improvements(distance);
	    if (statistics && winning_seed < seed_count) {
		++statistics->fallback_refinement_improvements;
		if (winning_seed > 0)
		    ++statistics->fallback_late_seed_improvements;
		statistics->maximum_winning_seed_index = std::max(
		    statistics->maximum_winning_seed_index,
		    static_cast<uint64_t>(winning_seed));
	    }
	    return true;
	}
    }
    if (statistics && winning_seed < seed_count) {
	++statistics->fallback_refinement_improvements;
	if (winning_seed > 0)
	    ++statistics->fallback_late_seed_improvements;
	statistics->maximum_winning_seed_index = std::max(
	    statistics->maximum_winning_seed_index,
	    static_cast<uint64_t>(winning_seed));
    }
    record_distance_improvements(distance);
    return distance <= tolerance;
}


double
surface_GetClosestPoint3dFirstOrderByRange(
    const ON_Surface *surf,
    const ON_3dPoint& p,
    const ON_Interval& u_range,
    const ON_Interval& v_range,
    double current_closest_dist,
    ON_2dPoint& p2d,
    ON_3dPoint& p3d,
    double same_point_tol,
    double within_distance_tol,
    int level
    )
{
    ON_3dPoint p0;
    ON_2dPoint p2d0;
    ON_3dVector ds, dt, dss, dst, dtt;
    ON_2dPoint working_p2d;
    ON_3dPoint working_p3d;
    bool notdone = true;
    double previous_distance = DBL_MAX;
    double distance;
    int errcnt = 0;

    p2d0.x = u_range.Mid();
    p2d0.y = v_range.Mid();

    while (notdone && (surf->Ev2Der(p2d0.x, p2d0.y, p0, ds, dt, dss, dst, dtt))) {
	if ((distance = p0.DistanceTo(p)) >= previous_distance) {
	    if (++errcnt <= 10) {
		p2d0 = (p2d0 + working_p2d) / 2.0;
		continue;
	    } else {
		///////////////////////////
		// Don't Subdivide just not getting any closer
		///////////////////////////
		/*
		  double distance =
		  surface_GetClosestPoint3dFirstOrderSubdivision(surf, p,
		  u_range, u_range.Mid(), v_range, v_range.Mid(),
		  current_closest_dist, p2d, p3d, tol, level++);
		  if (distance < current_closest_dist) {
		  current_closest_dist = distance;
		  if (current_closest_dist < tol)
		  return current_closest_dist;
		  }
		*/
		break;
	    }
	} else {
	    if (distance < current_closest_dist) {
		current_closest_dist = distance;
		p3d = p0;
		p2d = p2d0;
		if (current_closest_dist < same_point_tol)
		    return current_closest_dist;
	    }
	    previous_distance = distance;
	    working_p3d = p0;
	    working_p2d = p2d0;
	    errcnt = 0;
	}
	ON_3dVector N = ON_CrossProduct(ds, dt);
	N.Unitize();
	ON_Plane plane(p0, N);
	ON_3dPoint q = plane.ClosestPointTo(p);
	ON_2dVector pullback;
	ON_3dVector vector = q - p0;
	double vlength = vector.Length();

	if (vlength > 0.0) {
	    if (ON_Pullback3dVector(vector, 0.0, ds, dt, dss, dst, dtt,
				    pullback)) {
		p2d0 = p2d0 + pullback;
		if (!u_range.Includes(p2d0.x, false)) {
		    int i = (u_range.m_t[0] <= u_range.m_t[1]) ? 0 : 1;
		    p2d0.x =
			(p2d0.x < u_range.m_t[i]) ?
			u_range.m_t[i] : u_range.m_t[1 - i];
		}
		if (!v_range.Includes(p2d0.y, false)) {
		    int i = (v_range.m_t[0] <= v_range.m_t[1]) ? 0 : 1;
		    p2d0.y =
			(p2d0.y < v_range.m_t[i]) ?
			v_range.m_t[i] : v_range.m_t[1 - i];
		}
	    } else {
		///////////////////////////
		// Subdivide and go again
		///////////////////////////
		distance =
		    surface_GetClosestPoint3dFirstOrderSubdivision(surf, p,
								   u_range, u_range.Mid(), v_range, v_range.Mid(),
								   current_closest_dist, p2d, p3d, same_point_tol, within_distance_tol, level + 1);
		if (distance < current_closest_dist) {
		    current_closest_dist = distance;
		    if (current_closest_dist < same_point_tol)
			return current_closest_dist;
		}
		break;
	    }
	} else {
	    // can't get any closer
	    break;
	}
    }
    if (previous_distance < current_closest_dist) {
	current_closest_dist = previous_distance;
	p3d = working_p3d;
	p2d = working_p2d;
    }

    return current_closest_dist;
}


struct brlcad::PullbackContext::Impl {
    struct PreparedSurface {
	struct SpanBoxNode {
	    ON_BoundingBox box;
	    int left = -1;
	    int right = -1;
	    int u = -1;
	    int v = -1;

	    bool IsLeaf() const { return u >= 0 && v >= 0; }
	};

	const ON_Surface *surface = NULL;
	int u_span_count = 0;
	int v_span_count = 0;
	int u_mid_index = 0;
	int v_mid_index = 0;
	std::vector<double> u_spans;
	std::vector<double> v_spans;
	std::vector<double> fallback_u_spans;
	std::vector<double> fallback_v_spans;
	std::vector<std::vector<ON_BoundingBox> > boxes;
	std::vector<SpanBoxNode> span_box_nodes;
	int span_box_root = -1;
	bool surface_closed[2] = {false, false};
	ON_Interval surface_domains[2];
    };

    /* A loop's edge jobs all query the same immutable surface.  Keep the
     * expensive span boxes in a shared lazy slot, while every context retains
     * its own statistics and closest-point working state.  Construction is
     * serialized once; all later reads are immutable. */
    struct SurfaceCacheSlot {
	std::mutex mutex;
	bool attempted = false;
	const ON_Surface *candidate = NULL;
	std::shared_ptr<const PreparedSurface> prepared;
    };

    std::shared_ptr<SurfaceCacheSlot> surface_cache =
	std::make_shared<SurfaceCacheSlot>();
    std::shared_ptr<const PreparedSurface> prepared;
    brlcad::PullbackStatistics statistics;

    bool Prepare(const ON_Surface *candidate, double same_point_tol)
    {
	if (prepared && candidate == prepared->surface) {
	    ++statistics.surface_cache_hits;
	    return true;
	}
	const std::chrono::steady_clock::time_point started =
	    std::chrono::steady_clock::now();
	if (!candidate)
	    return false;

	std::shared_ptr<SurfaceCacheSlot> slot = surface_cache;
	std::unique_lock<std::mutex> guard(slot->mutex);
	/* A general-purpose context may be reused for another surface.  Detach
	 * from the inherited face cache in that case rather than disturbing jobs
	 * which are still reading it. */
	if (slot->attempted && slot->candidate != candidate) {
	    guard.unlock();
	    surface_cache = std::make_shared<SurfaceCacheSlot>();
	    prepared.reset();
	    return Prepare(candidate, same_point_tol);
	}
	if (slot->attempted) {
	    prepared = slot->prepared;
	    if (prepared) ++statistics.surface_cache_hits;
	    return prepared != NULL;
	}
	slot->attempted = true;
	slot->candidate = candidate;

	std::shared_ptr<PreparedSurface> next =
	    std::make_shared<PreparedSurface>();
	next->surface = candidate;

	const int original_u_span_count = candidate->SpanCount(0);
	const int original_v_span_count = candidate->SpanCount(1);
	if (original_u_span_count < 1 || original_v_span_count < 1)
	    return false;

	next->u_span_count = original_u_span_count;
	next->v_span_count = original_v_span_count;
	next->u_spans.resize(static_cast<size_t>(original_u_span_count) + 2);
	next->v_spans.resize(static_cast<size_t>(original_v_span_count) + 2);
	if (!candidate->GetSpanVector(0, next->u_spans.data()) ||
	    !candidate->GetSpanVector(1, next->v_spans.data()))
	    return false;
	next->fallback_u_spans.assign(next->u_spans.begin(),
	    next->u_spans.begin() + original_u_span_count + 1);
	next->fallback_v_spans.assign(next->v_spans.begin(),
	    next->v_spans.begin() + original_v_span_count + 1);
	next->surface_closed[0] = candidate->IsClosed(0);
	next->surface_closed[1] = candidate->IsClosed(1);
	next->surface_domains[0] = candidate->Domain(0);
	next->surface_domains[1] = candidate->Domain(1);

	const ON_Interval u_domain = next->surface_domains[0];
	const double u_mid = u_domain.Mid();
	const double u_parameter_tolerance = DBL_EPSILON * 64.0 *
	    std::max(1.0, std::max(fabs(u_domain.Min()),
		fabs(u_domain.Max())));
	next->u_mid_index = next->u_span_count / 2;
	for (int span = 0; span < next->u_span_count + 1; ++span) {
	    if (NEAR_EQUAL(next->u_spans[span], u_mid, u_parameter_tolerance)) {
		next->u_mid_index = span;
		break;
	    }
	    if (next->u_spans[span] > u_mid) {
		for (span = next->u_span_count + 1; span > 0; --span) {
		    if (next->u_spans[span - 1] < u_mid) {
			next->u_spans[span] = u_mid;
			next->u_mid_index = span;
			++next->u_span_count;
			break;
		    }
		    next->u_spans[span] = next->u_spans[span - 1];
		}
		break;
	    }
	}

	const ON_Interval v_domain = next->surface_domains[1];
	const double v_mid = v_domain.Mid();
	const double v_parameter_tolerance = DBL_EPSILON * 64.0 *
	    std::max(1.0, std::max(fabs(v_domain.Min()),
		fabs(v_domain.Max())));
	next->v_mid_index = next->v_span_count / 2;
	for (int span = 0; span < next->v_span_count + 1; ++span) {
	    if (NEAR_EQUAL(next->v_spans[span], v_mid, v_parameter_tolerance)) {
		next->v_mid_index = span;
		break;
	    }
	    if (next->v_spans[span] > v_mid) {
		for (span = next->v_span_count + 1; span > 0; --span) {
		    if (next->v_spans[span - 1] < v_mid) {
			next->v_spans[span] = v_mid;
			next->v_mid_index = span;
			++next->v_span_count;
			break;
		    }
		    next->v_spans[span] = next->v_spans[span - 1];
		}
		break;
	    }
	}

	next->boxes.resize(static_cast<size_t>(next->u_span_count));
	for (int u = 0; u < next->u_span_count; ++u)
	    next->boxes[u].resize(static_cast<size_t>(next->v_span_count));
	for (int u = 1; u < next->u_span_count + 1; ++u) {
	    for (int v = 1; v < next->v_span_count + 1; ++v) {
		const ON_Interval u_interval(next->u_spans[u - 1],
		    next->u_spans[u]);
		const ON_Interval v_interval(next->v_spans[v - 1],
		    next->v_spans[v]);
		if (!surface_GetBoundingBox(candidate, u_interval, v_interval,
			next->boxes[u - 1][v - 1], false))
		    return false;
	    }
	}

	/* The legacy closest-point path linearly tested every span box for every
	 * projected sample.  A trimmed STEP surface can contain tens of thousands
	 * of spans, while only a handful of their conservative boxes can contain a
	 * particular in-tolerance point.  Build an immutable parameter-coherent
	 * bounding hierarchy once with the rest of the surface cache. */
	const size_t span_count = static_cast<size_t>(next->u_span_count) *
	    static_cast<size_t>(next->v_span_count);
	next->span_box_nodes.reserve(2 * span_count);
	std::function<int(int, int, int, int)> build_span_box_tree =
	    [&next, &build_span_box_tree](int u_begin, int u_end,
		int v_begin, int v_end) -> int {
	    const int node_index = static_cast<int>(next->span_box_nodes.size());
	    next->span_box_nodes.push_back(PreparedSurface::SpanBoxNode());
	    if (u_end - u_begin == 1 && v_end - v_begin == 1) {
		PreparedSurface::SpanBoxNode &leaf =
		    next->span_box_nodes[static_cast<size_t>(node_index)];
		leaf.u = u_begin;
		leaf.v = v_begin;
		leaf.box = next->boxes[static_cast<size_t>(u_begin)]
		    [static_cast<size_t>(v_begin)];
		return node_index;
	    }

	    int left = -1;
	    int right = -1;
	    if (u_end - u_begin >= v_end - v_begin && u_end - u_begin > 1) {
		const int middle = u_begin + (u_end - u_begin) / 2;
		left = build_span_box_tree(u_begin, middle, v_begin, v_end);
		right = build_span_box_tree(middle, u_end, v_begin, v_end);
	    } else {
		const int middle = v_begin + (v_end - v_begin) / 2;
		left = build_span_box_tree(u_begin, u_end, v_begin, middle);
		right = build_span_box_tree(u_begin, u_end, middle, v_end);
	    }
	    PreparedSurface::SpanBoxNode &node =
		next->span_box_nodes[static_cast<size_t>(node_index)];
	    node.left = left;
	    node.right = right;
	    node.box = next->span_box_nodes[static_cast<size_t>(left)].box;
	    node.box.Union(next->span_box_nodes[static_cast<size_t>(right)].box);
	    return node_index;
	};
	next->span_box_root = build_span_box_tree(0, next->u_span_count, 0,
	    next->v_span_count);

	slot->prepared = next;
	prepared = next;
	++statistics.surfaces_prepared;
	statistics.span_boxes_built +=
	    static_cast<uint64_t>(next->u_span_count) *
	    static_cast<uint64_t>(next->v_span_count);
	statistics.preparation_us += static_cast<uint64_t>(
	    std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - started).count());
	return true;
    }
};


brlcad::PullbackContext::PullbackContext()
    : m_impl(new Impl())
{
}


brlcad::PullbackContext::~PullbackContext() = default;


brlcad::PullbackStatistics
brlcad::PullbackContext::Statistics() const
{
    return m_impl ? m_impl->statistics : brlcad::PullbackStatistics();
}


std::shared_ptr<brlcad::PullbackContext>
brlcad::PullbackContext::ForkWithSharedSurfaceCache() const
{
    std::shared_ptr<PullbackContext> fork = std::make_shared<PullbackContext>();
    if (m_impl && fork->m_impl)
	fork->m_impl->surface_cache = m_impl->surface_cache;
    return fork;
}


bool
brlcad::PullbackContext::SurfaceClosestPointFromSeed(
    const ON_Surface *surf,
    const ON_3dPoint& point,
    const ON_2dPoint& seed,
    ON_2dPoint& surface_point,
    ON_3dPoint& lifted_point,
    double& distance,
    double tolerance,
    const bool *cached_closed,
    const ON_Interval *cached_domains,
    double refinement_tolerance)
{
    const std::chrono::steady_clock::time_point started =
	std::chrono::steady_clock::now();
    ++m_impl->statistics.continuity_seed_searches;
    uint64_t completed_iterations = 0;
    uint64_t line_searches = 0;
    const bool result = surface_closest_point_seeded(surf, point, seed,
	surface_point, lifted_point, distance, tolerance, cached_closed,
	cached_domains, refinement_tolerance, &completed_iterations,
	&line_searches);
    m_impl->statistics.continuity_seed_us += static_cast<uint64_t>(
	std::chrono::duration_cast<std::chrono::microseconds>(
	    std::chrono::steady_clock::now() - started).count());
    if (result)
	++m_impl->statistics.continuity_seed_successes;
    else
	++m_impl->statistics.continuity_seed_failures;
    if (surface_point.IsValid() && std::isfinite(distance) &&
	    distance < DBL_MAX)
	++m_impl->statistics.continuity_seed_finite_candidates;
    m_impl->statistics.continuity_seed_iterations += completed_iterations;
    m_impl->statistics.continuity_seed_line_searches += line_searches;
    m_impl->statistics.maximum_continuity_seed_iterations = std::max(
	m_impl->statistics.maximum_continuity_seed_iterations,
	completed_iterations);
    m_impl->statistics.maximum_continuity_seed_line_searches = std::max(
	m_impl->statistics.maximum_continuity_seed_line_searches, line_searches);
    return result;
}


bool
brlcad::PullbackContext::SurfaceClosestPoint(
    const ON_Surface *surf,
    const ON_3dPoint& p,
    ON_2dPoint& p2d,
    ON_3dPoint& p3d,
    double &current_distance,
    int quadrant,	// optional - determines which quadrant to evaluate from
    //         0 = default
    //         1 from NE quadrant
    //         2 from NW quadrant
    //         3 from SW quadrant
    //         4 from SE quadrant
    double same_point_tol,
    double within_distance_tol
    )
{
    bool rc = false;
    const std::chrono::steady_clock::time_point primary_started =
	std::chrono::steady_clock::now();

    ++m_impl->statistics.closest_point_queries;

    closest_point_subdivision_nodes = 0;

    current_distance = DBL_MAX;

    if (PullbackWorkCancelled())
	return false;

    if (!surf)
	return false;
    if (!m_impl->Prepare(surf, same_point_tol)) {
	m_impl->statistics.primary_search_us += static_cast<uint64_t>(
	    std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - primary_started).count());
	++m_impl->statistics.multiseed_fallbacks;
	const std::chrono::steady_clock::time_point fallback_started =
	    std::chrono::steady_clock::now();
	const bool fallback_result = surface_closest_point_multiseed(surf, p,
	    p2d, p3d, current_distance, within_distance_tol, same_point_tol,
	    &m_impl->statistics);
	m_impl->statistics.multiseed_us += static_cast<uint64_t>(
	    std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - fallback_started).count());
	if (fallback_result)
	    ++m_impl->statistics.multiseed_successes;
	else
	    ++m_impl->statistics.multiseed_failures;
	return fallback_result;
    }

    const std::shared_ptr<const Impl::PreparedSurface> preparation =
	m_impl->prepared;
    if (!preparation) return false;
    const int u_spancnt = preparation->u_span_count;
    const int v_spancnt = preparation->v_span_count;
    const int umid_index = preparation->u_mid_index;
    const int vmid_index = preparation->v_mid_index;
    const std::vector<double> &uspan = preparation->u_spans;
    const std::vector<double> &vspan = preparation->v_spans;
    const std::vector<std::vector<ON_BoundingBox> > &bbox = preparation->boxes;

    {
	if (quadrant == 0) {
	    struct CandidateSpan {
		int u;
		int v;
		double minimum_distance;
	    };
	    std::vector<CandidateSpan> candidates;
	    std::vector<int> pending;
	    if (preparation->span_box_root >= 0)
		pending.push_back(preparation->span_box_root);
	    while (!pending.empty()) {
		if (PullbackWorkCancelled()) return false;
		const int node_index = pending.back();
		pending.pop_back();
		const Impl::PreparedSurface::SpanBoxNode &node =
		    preparation->span_box_nodes[static_cast<size_t>(node_index)];
		++m_impl->statistics.span_boxes_tested;
		const double minimum_distance = node.box.MinimumDistanceTo(p);
		if (!NEAR_ZERO(minimum_distance, within_distance_tol)) continue;
		if (node.IsLeaf()) {
		    candidates.push_back({node.u, node.v, minimum_distance});
		    continue;
		}
		/* Push right first so left is visited first.  The final sort below
		 * exactly restores the historical U-major span order. */
		if (node.right >= 0) pending.push_back(node.right);
		if (node.left >= 0) pending.push_back(node.left);
	    }
	    std::sort(candidates.begin(), candidates.end(),
		[](const CandidateSpan &left, const CandidateSpan &right) {
		    if (left.u != right.u) return left.u < right.u;
		    return left.v < right.v;
		});
	    for (const CandidateSpan &candidate : candidates) {
		if (candidate.minimum_distance >= current_distance) continue;
		const int u_span_index = candidate.u + 1;
		const int v_span_index = candidate.v + 1;
		const ON_Interval u_interval(uspan[u_span_index - 1],
		    uspan[u_span_index]);
		const ON_Interval v_interval(vspan[v_span_index - 1],
		    vspan[v_span_index]);
		int level = 1;
		const double distance =
		    surface_GetClosestPoint3dFirstOrderSubdivision(surf, p,
			u_interval, u_interval.Mid(), v_interval,
			v_interval.Mid(), current_distance, p2d, p3d,
			same_point_tol, within_distance_tol, level++);
		if (distance < current_distance) {
		    current_distance = distance;
		    if (current_distance < same_point_tol) {
			rc = true;
			goto cleanup;
		    }
		}
	    }
	    if (current_distance < within_distance_tol) {
		rc = true;
		goto cleanup;
	    }
	} else if (quadrant == 1) {
	    if (surf->IsClosed(0)) { //NE, SE, NW.SW
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    } else { //NE, NW, SW, SE
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    }
	    if (current_distance < within_distance_tol) {
		rc = true;
		goto cleanup;
	    }
	} else if (quadrant == 2) {
	    if (surf->IsClosed(0)) { // NW, SW, NE, SE
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    } else { // NW, NE, SW, SE
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    }
	    if (current_distance < within_distance_tol) {
		rc = true;
		goto cleanup;
	    }
	} else if (quadrant == 3) {
	    if (surf->IsClosed(0)) { // SW, NW, SE, NE
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    } else { // SW, SE, NW, NE
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    }
	    if (current_distance < within_distance_tol) {
		rc = true;
		goto cleanup;
	    }
	} else if (quadrant == 4) {
	    if (surf->IsClosed(0)) { // SE, NE, SW, NW
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    } else { // SE, SW, NE, NW
		//         4 from SE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         3 from SW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = 1; v_span_index < vmid_index+1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         1 from NE quadrant
		for (int u_span_index = umid_index+1; u_span_index < u_spancnt + 1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
		//         2 from NW quadrant
		for (int u_span_index = 1; u_span_index < umid_index+1; u_span_index++) {
		    for (int v_span_index = vmid_index+1; v_span_index < v_spancnt + 1; v_span_index++) {
			ON_Interval u_interval(uspan[u_span_index - 1],
					       uspan[u_span_index]);
			ON_Interval v_interval(vspan[v_span_index - 1],
					       vspan[v_span_index]);
			double min_distance, max_distance;

			int level = 1;
			++m_impl->statistics.span_boxes_tested;
			if (surface_GetIntervalMinMaxDistance(p, bbox[u_span_index-1][v_span_index-1], min_distance, max_distance)) {
			    if ((min_distance < current_distance) && NEAR_ZERO(min_distance, within_distance_tol)) {
				/////////////////////////////////////////
				// Could check normals and CV angles here
				/////////////////////////////////////////
				double distance = surface_GetClosestPoint3dFirstOrderSubdivision(surf, p, u_interval, u_interval.Mid(), v_interval, v_interval.Mid(), current_distance, p2d, p3d, same_point_tol, within_distance_tol, level++);
				if (distance < current_distance) {
				    current_distance = distance;
				    if (current_distance < same_point_tol) {
					rc = true;
					goto cleanup;
				    }
				}
			    }
			}
		    }
		}
	    }
	    if (current_distance < within_distance_tol) {
		rc = true;
		goto cleanup;
	    }
	}
    }
cleanup:

    m_impl->statistics.primary_search_us += static_cast<uint64_t>(
	std::chrono::duration_cast<std::chrono::microseconds>(
	    std::chrono::steady_clock::now() - primary_started).count());
    if (rc) ++m_impl->statistics.primary_search_successes;

    if (!rc && !PullbackWorkCancelled()) {
	++m_impl->statistics.multiseed_fallbacks;
	const std::chrono::steady_clock::time_point fallback_started =
	    std::chrono::steady_clock::now();
	rc = surface_closest_point_multiseed(surf, p, p2d, p3d,
	    current_distance, within_distance_tol, same_point_tol,
	    &m_impl->statistics, preparation->surface_closed,
	    preparation->surface_domains, &preparation->u_spans,
	    &preparation->v_spans, &preparation->boxes);
	m_impl->statistics.multiseed_us += static_cast<uint64_t>(
	    std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - fallback_started).count());
	if (rc)
	    ++m_impl->statistics.multiseed_successes;
	else
	    ++m_impl->statistics.multiseed_failures;
    }
    m_impl->statistics.subdivision_nodes += closest_point_subdivision_nodes;
    m_impl->statistics.maximum_subdivision_nodes = std::max(
	m_impl->statistics.maximum_subdivision_nodes,
	static_cast<uint64_t>(closest_point_subdivision_nodes));
    return rc;
}


bool
brlcad::surface_GetClosestPoint3dFirstOrder(
    PullbackContext &context,
    const ON_Surface *surf,
    const ON_3dPoint& point,
    ON_2dPoint& surface_point,
    ON_3dPoint& lifted_point,
    double &distance,
    int quadrant,
    double same_point_tol,
    double within_distance_tol)
{
    return context.SurfaceClosestPoint(surf, point, surface_point, lifted_point,
	distance, quadrant, same_point_tol, within_distance_tol);
}


bool
surface_GetClosestPoint3dFirstOrder(
    const ON_Surface *surf,
    const ON_3dPoint& point,
    ON_2dPoint& surface_point,
    ON_3dPoint& lifted_point,
    double &distance,
    int quadrant,
    double same_point_tol,
    double within_distance_tol)
{
    brlcad::PullbackContext context;
    return brlcad::surface_GetClosestPoint3dFirstOrder(context, surf, point,
	surface_point, lifted_point, distance, quadrant, same_point_tol,
	within_distance_tol);
}


static bool
pullback_closest_point(PBCData &data, const ON_3dPoint &point,
    ON_2dPoint &surface_point, ON_3dPoint &lifted_point, double &distance,
    int quadrant, double same_point_tol, double within_distance_tol)
{
    if (!data.context)
	data.context = std::make_shared<brlcad::PullbackContext>();
    return brlcad::surface_GetClosestPoint3dFirstOrder(*data.context,
	data.surf, point, surface_point, lifted_point, distance, quadrant,
	same_point_tol, within_distance_tol);
}


/* Consecutive samples of one edge are close in both curve parameter and UV.
 * Refine from the previous UV before falling back to the global surface-box
 * search.  This is both faster and avoids losing legal points on a periodic
 * seam when a very large analytic surface makes its bounding boxes too coarse
 * to provide a useful unseeded estimate. */
static bool
pullback_closest_point_seeded(PBCData &data, const ON_3dPoint &point,
    const ON_2dPoint &seed, ON_2dPoint &surface_point,
    ON_3dPoint &lifted_point, double &distance, double tolerance)
{
    if (!data.context)
	data.context = std::make_shared<brlcad::PullbackContext>();
    return data.context->SurfaceClosestPointFromSeed(data.surf, point, seed,
	surface_point, lifted_point, distance, tolerance,
	data.surface_parameterization_cached ? data.surface_closed : NULL,
	data.surface_parameterization_cached ? data.surface_domain : NULL,
	data.declared_tolerance);
}


bool trim_GetClosestPoint3dFirstOrder(
    const ON_BrepTrim& trim,
    const ON_3dPoint& p,
    ON_2dPoint& p2d,
    double& t,
    double& distance,
    const ON_Interval* interval,
    double same_point_tol,
    double within_distance_tol
    )
{
    bool rc = false;
    const ON_Surface *surf = trim.SurfaceOf();

    double t0;
    ON_3dPoint p3d;
    ON_3dVector T, K;
    ON_BoundingBox tight_bbox;
    std::vector<ON_BoundingBox> bbox;

    ON_Curve *c = trim.Brep()->m_C2[trim.m_c2i];
    ON_NurbsCurve N;
    if (0 == c->GetNurbForm(N))
	return false;
    if (N.m_order < 2 || N.m_cv_count < N.m_order)
	return false;

    p2d = trim.PointAt(t);
    int quadrant = 0;     // optional - determines which quadrant to evaluate from
                          //         0 = default
                          //         1 from NE quadrant
                          //         2 from NW quadrant
                          //         3 from SW quadrant
                          //         4 from SE quadrant
    ON_Interval u_interval = surf->Domain(0);
    ON_Interval v_interval = surf->Domain(1);
    if (p2d.y > v_interval.Mid()) {
	// North quadrants -> 1 or 2;
	if (p2d.x > u_interval.Mid()) {
	    quadrant = 1; // NE
	} else {
	    quadrant = 2; //NW
	}
    } else {
	// South quadrants -> 3 or 4;
	if (p2d.x > u_interval.Mid()) {
	    quadrant = 4; // SE
	} else {
	    quadrant = 3; //SW
	}
    }
    if (surface_GetClosestPoint3dFirstOrder(surf, p, p2d, p3d, distance, quadrant, same_point_tol, within_distance_tol)) {
	ON_BezierCurve B;
	bool bGrowBox = false;
	ON_3dVector d1, d2;
	double max_dist_to_closest_pt = DBL_MAX;
	ON_Interval *span_interval = new ON_Interval[N.m_cv_count - N.m_order + 1];
	double *min_distance = new double[N.m_cv_count - N.m_order + 1];
	double *max_distance = new double[N.m_cv_count - N.m_order + 1];
	bool *skip = new bool[N.m_cv_count - N.m_order + 1];
	bbox.resize(N.m_cv_count - N.m_order + 1);
	for (int span_index = 0; span_index <= N.m_cv_count - N.m_order; span_index++)
	{
	    skip[span_index] = true;
	    if (!(N.m_knot[span_index + N.m_order-2] < N.m_knot[span_index + N.m_order-1]))
		continue;

	    // check for span out of interval
	    int i = (interval->m_t[0] <= interval->m_t[1]) ? 0 : 1;
	    if (N.m_knot[span_index + N.m_order-2] > interval->m_t[1-i])
		continue;
	    if (N.m_knot[span_index + N.m_order-1] < interval->m_t[i])
		continue;

	    if (!N.ConvertSpanToBezier(span_index, B))
		continue;
	    if (!B.GetTightBoundingBox(tight_bbox, bGrowBox, NULL))
		continue;
	    bbox[span_index] = tight_bbox;
	    d1 = tight_bbox.m_min - p2d;
	    d2 = tight_bbox.m_max - p2d;
	    min_distance[span_index] = tight_bbox.MinimumDistanceTo(p2d);

	    if (min_distance[span_index] > max_dist_to_closest_pt) {
		max_distance[span_index] = DBL_MAX;
		continue;
	    }
	    skip[span_index] = false;
	    span_interval[span_index].m_t[0] = ((N.m_knot[span_index + N.m_order-2]) < interval->m_t[i]) ? interval->m_t[i] : N.m_knot[span_index + N.m_order-2];
	    span_interval[span_index].m_t[1] = ((N.m_knot[span_index + N.m_order-1]) > interval->m_t[1 -i]) ? interval->m_t[1 -i] : (N.m_knot[span_index + N.m_order-1]);
	    ON_3dPoint d1sq(d1.x*d1.x, d1.y*d1.y, 0.0), d2sq(d2.x*d2.x, d2.y*d2.y, 0.0);
	    double distancesq;
	    if (d1sq.x < d2sq.x) {
		if (d1sq.y < d2sq.y) {
		    if ((d1sq.x + d2sq.y) < (d2sq.x + d1sq.y)) {
			distancesq = d1sq.x + d2sq.y;
		    } else {
			distancesq = d2sq.x + d1sq.y;
		    }
		} else {
		    if ((d1sq.x + d1sq.y) < (d2sq.x + d2sq.y)) {
			distancesq = d1sq.x + d1sq.y;
		    } else {
			distancesq = d2sq.x + d2sq.y;
		    }
		}
	    } else {
		if (d1sq.y < d2sq.y) {
		    if ((d1sq.x + d1sq.y) < (d2sq.x + d2sq.y)) {
			distancesq = d1sq.x + d1sq.y;
		    } else {
			distancesq = d2sq.x + d2sq.y;
		    }
		} else {
		    if ((d1sq.x + d2sq.y) < (d2sq.x + d1sq.y)) {
			distancesq = d1sq.x + d2sq.y;
		    } else {
			distancesq = d2sq.x + d1sq.y;
		    }
		}
	    }
	    max_distance[span_index] = sqrt(distancesq);
	    if (max_distance[span_index] < max_dist_to_closest_pt) {
		max_dist_to_closest_pt = max_distance[span_index];
	    }
	    if (max_distance[span_index] < min_distance[span_index]) {
		// should only be here for near equal fuzz
		min_distance[span_index] = max_distance[span_index];
	    }
	}
	for (int span_index = 0; span_index <= N.m_cv_count - N.m_order; span_index++)
	{

	    if (skip[span_index])
		continue;

	    if (min_distance[span_index] > max_dist_to_closest_pt) {
		skip[span_index] = true;
		continue;
	    }

	}

	ON_3dPoint q;
	ON_3dPoint point;
	double closest_distance = DBL_MAX;
	double closestT = DBL_MAX;
	for (int span_index = 0; span_index <= N.m_cv_count - N.m_order; span_index++)
	{
	    if (skip[span_index]) {
		continue;
	    }
	    t0 = span_interval[span_index].Mid();
	    bool closestfound = false;
	    bool notdone = true;
	    double span_distance = DBL_MAX;
	    double previous_distance = DBL_MAX;
	    ON_3dVector firstDervative, secondDervative;
	    while (notdone
		   && trim.Ev2Der(t0, point, firstDervative, secondDervative)
		   && ON_EvCurvature(firstDervative, secondDervative, T, K)) {
		ON_Line line(point, point + 100.0 * T);
		q = line.ClosestPointTo(p2d);
		double delta_t = (firstDervative * (q - point))
		    / (firstDervative * firstDervative);
		double new_t0 = t0 + delta_t;
		if (!span_interval[span_index].Includes(new_t0, false)) {
		    // limit to interval
		    int i = (span_interval[span_index].m_t[0] <= span_interval[span_index].m_t[1]) ? 0 : 1;
		    new_t0 =
			(new_t0 < span_interval[span_index].m_t[i]) ?
			span_interval[span_index].m_t[i] : span_interval[span_index].m_t[1 - i];
		}
		delta_t = new_t0 - t0;
		t0 = new_t0;
		point = trim.PointAt(t0);
		span_distance = point.DistanceTo(p2d);
		if (span_distance < previous_distance) {
		    closestfound = true;
		    closestT = t0;
		    previous_distance = span_distance;
		    if (fabs(delta_t) < same_point_tol) {
			notdone = false;
		    }
		} else {
		    notdone = false;
		}
	    }
	    if (closestfound && (span_distance < closest_distance)) {
		closest_distance = span_distance;
		rc = true;
		t = closestT;
	    }
	}
	delete [] span_interval;
	delete [] min_distance;
	delete [] max_distance;
	delete [] skip;

    }
    return rc;
}


bool
toUV(PBCData& data, ON_2dPoint& out_pt, double t, double knudge = 0.0, double within_distance_tol = BREP_EDGE_MISS_TOLERANCE)
{
    ON_3dPoint pointOnCurve = data.curve->PointAt(t);
    ON_3dPoint knudgedPointOnCurve = data.curve->PointAt(t + knudge);

    ON_2dPoint uv;
    if (data.surftree->getSurfacePoint((const ON_3dPoint&)pointOnCurve, uv, (const ON_3dPoint&)knudgedPointOnCurve, within_distance_tol) > 0) {
	out_pt.Set(uv.x, uv.y);
	return true;
    } else {
	return false;
    }
}


bool
toUV(brlcad::SurfaceTree *surftree, const ON_Curve *curve, ON_2dPoint& out_pt, double t, double knudge = 0.0)
{
    if (!surftree)
	return false;

    const ON_Surface *surf = surftree->getSurface();
    if (!surf)
	return false;

    ON_3dPoint pointOnCurve = curve->PointAt(t);
    ON_3dPoint knudgedPointOnCurve = curve->PointAt(t + knudge);
    ON_3dVector dt;
    curve->Ev1Der(t, pointOnCurve, dt);
    ON_3dVector tangent = curve->TangentAt(t);
    //data.surf->GetClosestPoint(pointOnCurve, &a, &b, 0.0001);
    ON_Ray r(pointOnCurve, tangent);
    plane_ray pr;
    brep_get_plane_ray(r, pr);
    ON_3dVector p1;
    double p1d;
    ON_3dVector p2;
    double p2d;

    utah_ray_planes(r, p1, p1d, p2, p2d);

    VMOVE(pr.n1, p1);
    pr.d1 = p1d;
    VMOVE(pr.n2, p2);
    pr.d2 = p2d;

    try {
	pt2d_t uv;
	ON_2dPoint uv2d = surftree->getClosestPointEstimate(knudgedPointOnCurve);
	move(uv, uv2d);

	ON_3dVector dir = surf->NormalAt(uv[0], uv[1]);
	dir.Reverse();
	ON_Ray ray(pointOnCurve, dir);
	brep_get_plane_ray(ray, pr);
	//know use this as guess to iterate to closer solution
	pt2d_t Rcurr;
	pt2d_t new_uv;
	ON_3dPoint pt;
	ON_3dVector su, sv;
#ifdef SHOW_UNUSED
	bool found = false;
#endif
	fastf_t Dlast = MAX_FASTF;
	for (int i = 0; i < 10; i++) {
	    brep_r(surf, pr, uv, pt, su, sv, Rcurr);
	    fastf_t d = v2mag(Rcurr);
	    if (d < BREP_INTERSECTION_ROOT_EPSILON) {
		TRACE1("R:" << ON_PRINT2(Rcurr));
#ifdef SHOW_UNUSED
		found = true;
		break;
#endif
	    } else if (d > Dlast) {
#ifdef SHOW_UNUSED
		found = false; //break;
#endif
		break;
		//return brep_edge_check(found, sbv, face, surf, ray, hits);
	    }
	    brep_newton_iterate(pr, Rcurr, su, sv, uv, new_uv);
	    move(uv, new_uv);
	    Dlast = d;
	}

///////////////////////////////////////
	out_pt.Set(uv[0], uv[1]);
	return true;
    } catch (...) {
	return false;
    }
}


double
randomPointFromRange(PBCData& UNUSED(data), ON_2dPoint& UNUSED(out), double lo, double hi)
{
    assert(lo < hi);
    double random_pos = drand48() * (RANGE_HI - RANGE_LO) + RANGE_LO;
    double newt = random_pos * (hi - lo) + lo;
    //assert(toUV(data, out, newt));
    return newt;
}


bool
sample(PBCData& data,
       double t1,
       double t2,
       const ON_2dPoint& p1,
       const ON_2dPoint& p2)
{
    ON_2dPoint m;
    double t = randomPointFromRange(data, m, t1, t2);
    if (!data.segments->empty()) {
	ON_2dPointArray * samples = data.segments->back();
	if (isFlat(p1, m, p2, data.flatness)) {
	    samples->Append(p2);
	} else {
	    sample(data, t1, t, p1, m);
	    sample(data, t, t2, m, p2);
	}
	return true;
    }
    return false;
}


void
generateKnots(BSpline& bspline)
{
    int num_knots = bspline.m + 1;
    bspline.knots.resize(num_knots);
    for (int i = 0; i <= bspline.p; i++) {
	bspline.knots[i] = 0.0;
    }
    for (int i = bspline.m - bspline.p; i <= bspline.m; i++) {
	bspline.knots[i] = 1.0;
    }
    for (int i = 1; i <= bspline.n - bspline.p; i++) {
	bspline.knots[bspline.p + i] = (double)i / (bspline.n - bspline.p + 1.0);
    }
}


int
getKnotInterval(BSpline& bspline, double u)
{
    int k = 0;
    while (u >= bspline.knots[k]) k++;
    k = (k == 0) ? k : k - 1;
    return k;
}


ON_NurbsCurve*
interpolateLocalCubicCurve(ON_2dPointArray &Q)
{
    int num_samples = Q.Count();
    int num_segments = Q.Count() - 1;
    int qsize = num_samples + 4;
    std::vector < ON_2dVector > qarray(qsize);

    for (int i = 1; i < Q.Count(); i++) {
	qarray[i + 1] = Q[i] - Q[i - 1];
    }
    qarray[1] = 2.0 * qarray[2] - qarray[3];
    qarray[0] = 2.0 * qarray[1] - qarray[2];

    qarray[num_samples + 1] = 2 * qarray[num_samples] - qarray[num_samples - 1];
    qarray[num_samples + 2] = 2 * qarray[num_samples + 1] - qarray[num_samples];
    qarray[num_samples + 3] = 2 * qarray[num_samples + 2] - qarray[num_samples + 1];

    std::vector < ON_2dVector > T(num_samples);
    std::vector<double> A(num_samples);
    for (int k = 0; k < num_samples; k++) {
	ON_3dVector a = ON_CrossProduct(qarray[k], qarray[k + 1]);
	ON_3dVector b = ON_CrossProduct(qarray[k + 2], qarray[k + 3]);
	double alength = a.Length();
	if (NEAR_ZERO(alength, PBC_TOL)) {
	    A[k] = 1.0;
	} else {
	    A[k] = (a.Length()) / (a.Length() + b.Length());
	}
	T[k] = (1.0 - A[k]) * qarray[k + 1] + A[k] * qarray[k + 2];
	T[k].Unitize();
    }
    std::vector < ON_2dPointArray > P(num_samples - 1);
    ON_2dPointArray control_points;
    control_points.Append(Q[0]);
    for (int i = 1; i < num_samples; i++) {
	ON_2dPoint P0 = Q[i - 1];
	ON_2dPoint P3 = Q[i];
	ON_2dVector T0 = T[i - 1];
	ON_2dVector T3 = T[i];

	double a, b, c;

	ON_2dVector vT0T3 = T0 + T3;
	ON_2dVector dP0P3 = P3 - P0;
	a = 16.0 - vT0T3.Length() * vT0T3.Length();
	b = 12.0 * (dP0P3 * vT0T3);
	c = -36.0 * dP0P3.Length() * dP0P3.Length();

	double alpha = (-b + sqrt(b * b - 4.0 * a * c)) / (2.0 * a);

	ON_2dPoint P1 = P0 + (1.0 / 3.0) * alpha * T0;
	control_points.Append(P1);
	ON_2dPoint P2 = P3 - (1.0 / 3.0) * alpha * T3;
	control_points.Append(P2);
	P[i - 1].Append(P0);
	P[i - 1].Append(P1);
	P[i - 1].Append(P2);
	P[i - 1].Append(P3);
    }
    control_points.Append(Q[num_samples - 1]);

    std::vector<double> u(num_segments + 1);
    u[0] = 0.0;
    for (int k = 0; k < num_segments; k++) {
	/* Chord length is strictly positive after duplicate samples have been
	 * removed.  Parameterizing from a tangent handle can produce a zero
	 * interval at a stationary endpoint and an invalid repeated interior
	 * openNURBS knot. */
	u[k + 1] = u[k] + (Q[k + 1] - Q[k]).Length();
    }
    int degree = 3;
    int n = control_points.Count();
    int p = degree;
    int m = n + p - 1;
    int dimension = 2;
    ON_NurbsCurve* c = ON_NurbsCurve::New(dimension, false, degree + 1, n);
    c->ReserveKnotCapacity(m);
    for (int i = 0; i < degree; i++) {
	c->SetKnot(i, 0.0);
    }
    for (int i = 1; i < num_segments; i++) {
	double knot_value = u[i] / u[num_segments];
	c->SetKnot(degree + 2 * (i - 1), knot_value);
	c->SetKnot(degree + 2 * (i - 1) + 1, knot_value);
    }
    for (int i = m - p; i < m; i++) {
	c->SetKnot(i, 1.0);
    }

    // insert the control points
    for (int i = 0; i < n; i++) {
	ON_3dPoint pnt = control_points[i];
	c->SetCV(i, pnt);
    }
    return c;
}


ON_NurbsCurve*
interpolateLocalCubicCurve(const ON_3dPointArray &Q)
{
    int num_samples = Q.Count();
    int num_segments = Q.Count() - 1;
    int qsize = num_samples + 3;
    std::vector<ON_3dVector> qarray(qsize + 1);
    ON_3dVector *q = &qarray[1];

    for (int i = 1; i < Q.Count(); i++) {
	q[i] = Q[i] - Q[i - 1];
    }
    q[0] = 2.0 * q[1] - q[2];
    q[-1] = 2.0 * q[0] - q[1];

    q[num_samples] = 2 * q[num_samples - 1] - q[num_samples - 2];
    q[num_samples + 1] = 2 * q[num_samples] - q[num_samples - 1];
    q[num_samples + 2] = 2 * q[num_samples + 1] - q[num_samples];

    std::vector<ON_3dVector> T(num_samples);
    std::vector<double> A(num_samples);
    for (int k = 0; k < num_samples; k++) {
	ON_3dVector avec = ON_CrossProduct(q[k - 1], q[k]);
	ON_3dVector bvec = ON_CrossProduct(q[k + 1], q[k + 2]);
	double alength = avec.Length();
	if (NEAR_ZERO(alength, PBC_TOL)) {
	    A[k] = 1.0;
	} else {
	    A[k] = (avec.Length()) / (avec.Length() + bvec.Length());
	}
	T[k] = (1.0 - A[k]) * q[k] + A[k] * q[k + 1];
	T[k].Unitize();
    }
    std::vector<ON_3dPointArray> P(num_samples - 1);
    ON_3dPointArray control_points;
    control_points.Append(Q[0]);
    for (int i = 1; i < num_samples; i++) {
	ON_3dPoint P0 = Q[i - 1];
	ON_3dPoint P3 = Q[i];
	ON_3dVector T0 = T[i - 1];
	ON_3dVector T3 = T[i];

	double a, b, c;

	ON_3dVector vT0T3 = T0 + T3;
	ON_3dVector dP0P3 = P3 - P0;
	a = 16.0 - vT0T3.Length() * vT0T3.Length();
	b = 12.0 * (dP0P3 * vT0T3);
	c = -36.0 * dP0P3.Length() * dP0P3.Length();

	double alpha = (-b + sqrt(b * b - 4.0 * a * c)) / (2.0 * a);

	ON_3dPoint P1 = P0 + (1.0 / 3.0) * alpha * T0;
	control_points.Append(P1);
	ON_3dPoint P2 = P3 - (1.0 / 3.0) * alpha * T3;
	control_points.Append(P2);
	P[i - 1].Append(P0);
	P[i - 1].Append(P1);
	P[i - 1].Append(P2);
	P[i - 1].Append(P3);
    }
    control_points.Append(Q[num_samples - 1]);

    std::vector<double> u(num_segments + 1);
    u[0] = 0.0;
    for (int k = 0; k < num_segments; k++) {
	u[k + 1] = u[k] + (Q[k + 1] - Q[k]).Length();
    }
    int degree = 3;
    int n = control_points.Count();
    int p = degree;
    int m = n + p - 1;
    int dimension = 3;
    ON_NurbsCurve* c = ON_NurbsCurve::New(dimension, false, degree + 1, n);
    c->ReserveKnotCapacity(m);
    for (int i = 0; i < degree; i++) {
	c->SetKnot(i, 0.0);
    }
    for (int i = 1; i < num_segments; i++) {
	double knot_value = u[i] / u[num_segments];
	c->SetKnot(degree + 2 * (i - 1), knot_value);
	c->SetKnot(degree + 2 * (i - 1) + 1, knot_value);
    }
    for (int i = m - p; i < m; i++) {
	c->SetKnot(i, 1.0);
    }

    // insert the control points
    for (int i = 0; i < n; i++) {
	ON_3dPoint pnt = control_points[i];
	c->SetCV(i, pnt);
    }
    return c;
}


ON_NurbsCurve*
newNURBSCurve(BSpline& spline, int dimension = 3)
{
    // we now have everything to complete our spline
    ON_NurbsCurve* c = ON_NurbsCurve::New(dimension,
					  false,
					  spline.p + 1,
					  spline.n + 1);
    c->ReserveKnotCapacity((int)spline.knots.size() - 2);
    for (unsigned int i = 1; i < spline.knots.size() - 1; i++) {
	c->m_knot[i - 1] = spline.knots[i];
    }

    for (int i = 0; i < spline.controls.Count(); i++) {
	c->SetCV(i, ON_3dPoint(spline.controls[i]));
    }

    return c;
}


ON_Curve*
interpolateCurve(ON_2dPointArray &samples)
{
    ON_2dPointArray clean;
    for (int i = 0; i < samples.Count(); ++i) {
	const ON_2dPoint &point = samples[i];
	if (!std::isfinite(point.x) || !std::isfinite(point.y))
	    continue;
	if (!clean.Count() || clean[clean.Count() - 1].DistanceTo(point) >
		ON_ZERO_TOLERANCE)
	    clean.Append(point);
    }
    samples = clean;
    if (samples.Count() < 2)
	return NULL;
    if (samples.Count() == 2)
	return new ON_LineCurve(samples[0], samples[1]);

    ON_NurbsCurve *nurbs = interpolateLocalCubicCurve(samples);
    if (nurbs && nurbs->IsValid()) {
	/* interpolateLocalCubicCurve extrapolates the two end tangents as if
	 * every input were open.  For a closed sample ring that can produce a
	 * geometrically closed curve whose end derivatives point in opposite
	 * directions.  Such a pcurve is structurally invalid for a closed BREP
	 * edge even though its interior samples are correct.  Retain the cubic
	 * only when its closure direction is consistent; otherwise use the
	 * already lift-bounded degree-one path through the adaptive samples. */
	const bool closed = samples[0].DistanceTo(samples[samples.Count() - 1]) <=
	    ON_ZERO_TOLERANCE;
	if (!closed)
	    return nurbs;
	ON_3dVector start_tangent = nurbs->TangentAt(nurbs->Domain().Min());
	ON_3dVector end_tangent = nurbs->TangentAt(nurbs->Domain().Max());
	if (start_tangent.Unitize() && end_tangent.Unitize() &&
		start_tangent * end_tangent >= 0.0)
	    return nurbs;
    }
    delete nurbs;

    /* The adaptive samples have already passed the surface-lift error
     * budget.  A degree-one UV curve through those samples is therefore the
     * bounded, topology-preserving fallback when cubic interpolation is
     * singular; it does not invent 3-D geometry. */
    ON_3dPointArray polyline_points;
    polyline_points.Reserve(samples.Count());
    for (int i = 0; i < samples.Count(); ++i)
	polyline_points.Append(ON_3dPoint(samples[i].x, samples[i].y, 0.0));
    ON_PolylineCurve *polyline = new ON_PolylineCurve(polyline_points);
    if (polyline->ChangeDimension(2) && polyline->IsValid())
	return polyline;
    delete polyline;
    return NULL;
}


int
IsAtSeam(const ON_Surface *surf, int dir, double u, double v, double tol)
{
    int rc = 0;
    if (!surf->IsClosed(dir))
	return rc;

    double p = (dir) ? v : u;
    if (NEAR_EQUAL(p, surf->Domain(dir)[0], tol) || NEAR_EQUAL(p, surf->Domain(dir)[1], tol))
	rc += (dir + 1);

    return rc;
}


/*
 * Similar to openNURBS's surf->IsAtSeam() function but uses tolerance to do a near check versus
 * the floating point equality used by openNURBS.
 * rc = 0 Not on seam, 1 on East/West seam(umin/umax), 2 on North/South seam(vmin/vmax), 3 seam on both U/V boundaries
 */
int
IsAtSeam(const ON_Surface *surf, double u, double v, double tol)
{
    int rc = 0;
    int i;
    for (i = 0; i < 2; i++) {
	rc += IsAtSeam(surf, i, u, v, tol);
    }

    return rc;
}


int
IsAtSeam(const ON_Surface *surf, int dir, const ON_2dPoint &pt, double tol)
{
    int rc = 0;
    ON_2dPoint unwrapped_pt = UnwrapUVPoint(surf, pt, tol);
    rc = IsAtSeam(surf, dir, unwrapped_pt.x, unwrapped_pt.y, tol);

    return rc;
}


/*
 * Similar to IsAtSeam(surf, u, v, tol) function but takes a ON_2dPoint
 * and unwraps any closed seam extents before passing on IsAtSeam(surf, u, v, tol)
 */
int
IsAtSeam(const ON_Surface *surf, const ON_2dPoint &pt, double tol)
{
    int rc = 0;
    ON_2dPoint unwrapped_pt = UnwrapUVPoint(surf, pt, tol);
    rc = IsAtSeam(surf, unwrapped_pt.x, unwrapped_pt.y, tol);

    return rc;
}


int
IsAtSingularity(const ON_Surface *surf, double u, double v, double tol)
{
    // 0 = south, 1 = east, 2 = north, 3 = west
    //std::cerr << "IsAtSingularity = u, v - " << u << ", " << v << std::endl;
    //std::cerr << "surf->Domain(0) - " << surf->Domain(0)[0] << ", " << surf->Domain(0)[1] << std::endl;
    //std::cerr << "surf->Domain(1) - " << surf->Domain(1)[0] << ", " << surf->Domain(1)[1] << std::endl;
    if (NEAR_EQUAL(u, surf->Domain(0)[0], tol)) {
	if (surf->IsSingular(3))
	    return 3;
    } else if (NEAR_EQUAL(u, surf->Domain(0)[1], tol)) {
	if (surf->IsSingular(1))
	    return 1;
    }
    if (NEAR_EQUAL(v, surf->Domain(1)[0], tol)) {
	if (surf->IsSingular(0))
	    return 0;
    } else if (NEAR_EQUAL(v, surf->Domain(1)[1], tol)) {
	if (surf->IsSingular(2))
	    return 2;
    }
    return -1;
}


int
IsAtSingularity(const ON_Surface *surf, const ON_2dPoint &pt, double tol)
{
    int rc = 0;
    ON_2dPoint unwrapped_pt = UnwrapUVPoint(surf, pt, tol);
    rc = IsAtSingularity(surf, unwrapped_pt.x, unwrapped_pt.y, tol);

    return rc;
}


ON_2dPointArray *
pullback_samples(PBCData* data,
		 double t,
		 double s,
		 double same_point_tol,
		 double within_distance_tol)
{
    if (!data)
	return NULL;

    const ON_Curve* curve = data->curve;
    ON_2dPointArray *samples = new ON_2dPointArray();
    int numKnots = curve->SpanCount();
    double *knots = new double[numKnots + 1];
    curve->GetSpanVector(knots);

    int istart = 0;
    while (t >= knots[istart])
	istart++;

    if (istart > 0) {
	istart--;
	knots[istart] = t;
    }

    int istop = numKnots;
    while (s <= knots[istop])
	istop--;

    if (istop < numKnots) {
	istop++;
	knots[istop] = s;
    }

    int samplesperknotinterval;
    int degree = curve->Degree();

    if (degree > 1) {
	samplesperknotinterval = 3 * degree;
    } else {
	samplesperknotinterval = 18 * degree;
    }
    /* Project one parameter globally, then walk away from that anchor in both
     * directions using the adjacent UV as a Newton seed.  The former code ran
     * the expensive global surface-tree subdivision independently for every
     * point and could retain a single isolated success on a perfectly valid
     * spline boundary.  Each seeded result is still accepted only when its
     * 3-D lift satisfies within_distance_tol; the global search remains the
     * fallback whenever the local solve does not converge. */
    std::vector<double> parameters;
    std::vector<bool> span_boundaries;
    for (int i = istart; i <= istop; ++i) {
	if (i == istart) {
	    parameters.push_back(knots[i]);
	    span_boundaries.push_back(true);
	    continue;
	}
	const double delta = (knots[i] - knots[i - 1]) /
	    static_cast<double>(samplesperknotinterval);
	for (int j = 1; j < samplesperknotinterval; ++j) {
	    parameters.push_back(knots[i - 1] + j * delta);
	    span_boundaries.push_back(false);
	}
	parameters.push_back(knots[i]);
	span_boundaries.push_back(true);
    }

    std::vector<ON_2dPoint> projected(parameters.size(),
	ON_2dPoint::UnsetPoint);
    std::vector<bool> valid(parameters.size(), false);
    std::vector<double> projection_distances(parameters.size(), DBL_MAX);
    data->failure_reason = PullbackFailureReason::None;
    data->projection_samples = parameters.size();
    data->rejected_projection_samples = 0;
    data->failed_projection_samples = 0;
    data->maximum_projection_distance = 0.0;
    size_t anchor = parameters.size();
    size_t fallback_anchor = parameters.size();
    double fallback_distance = DBL_MAX;
    for (size_t i = 0; i < parameters.size(); ++i) {
	if (!span_boundaries[i]) continue;
	if (brlcad::PullbackWorkCancelled()) break;
	const ON_3dPoint target = curve->PointAt(parameters[i]);
	ON_3dPoint lift = ON_3dPoint::UnsetPoint;
	double &distance = projection_distances[i];
	pullback_closest_point(*data, target,
	    projected[i], lift, distance, 0, same_point_tol,
	    same_point_tol);
	valid[i] = projected[i].IsValid() && std::isfinite(distance) &&
	    distance <= within_distance_tol;
	if (valid[i] && distance <= same_point_tol) {
	    anchor = i;
	    break;
	}
	/* If no knot boundary satisfies the active tolerance, start continuity
	 * from the globally closest finite boundary candidate.  Selecting the
	 * first finite result can choose a nearby sheet of a folded surface and
	 * greatly overstate the source mismatch. */
	if (projected[i].IsValid() && std::isfinite(distance) &&
		distance < fallback_distance) {
	    fallback_anchor = i;
	    fallback_distance = distance;
	    /* An open surface has no equivalent periodic parameter sheet.  One
	     * globally checked finite boundary point is sufficient to establish
	     * the continuity seed even when source curve/surface separation is
	     * larger than the strict first-pass tolerance.  The full sample walk
	     * still measures and rejects that separation before any safe retry. */
	    if (!data->surface_closed[0] && !data->surface_closed[1]) {
		anchor = i;
		break;
	    }
	}
    }
    if (anchor == parameters.size()) anchor = fallback_anchor;

    const auto project_from_seed = [data, curve, &parameters, &projected,
	&valid, &projection_distances, same_point_tol,
	within_distance_tol](size_t index,
	    const ON_2dPoint &seed) {
	if (brlcad::PullbackWorkCancelled()) return false;
	const ON_3dPoint target = curve->PointAt(parameters[index]);
	ON_3dPoint lift = ON_3dPoint::UnsetPoint;
	double &distance = projection_distances[index];
	pullback_closest_point_seeded(*data, target, seed, projected[index],
	    lift, distance, same_point_tol);
	bool have_candidate = projected[index].IsValid() &&
	    std::isfinite(distance) && distance < DBL_MAX;
	/* An open NURBS surface can still fold over itself and have several local
	 * closest-point basins.  Retain the continuity fast path only while its
	 * measured distance is inside the caller-authorized projection bound;
	 * otherwise a global span search must disambiguate the branch.  Safe-mode
	 * callers which have already established a scale-bounded mismatch ceiling
	 * pass that wider bound here, so ordinary source mismatch does not repeat
	 * the expensive global search merely because it exceeds the declared file
	 * uncertainty. */
	if (!have_candidate || distance > within_distance_tol) {
	    projected[index] = ON_2dPoint::UnsetPoint;
	    distance = DBL_MAX;
	    pullback_closest_point(*data, target, projected[index], lift,
		distance, 0, same_point_tol, same_point_tol);
	    have_candidate = projected[index].IsValid() &&
		std::isfinite(distance) && distance < DBL_MAX;
	}
	valid[index] = have_candidate && distance <= within_distance_tol;
	return have_candidate;
    };

    if (anchor < parameters.size()) {
	ON_2dPoint seed = projected[anchor];
	for (size_t i = anchor + 1; i < parameters.size(); ++i) {
	    if (project_from_seed(i, seed)) seed = projected[i];
	}
	seed = projected[anchor];
	for (size_t i = anchor; i > 0; --i) {
	    if (project_from_seed(i - 1, seed)) seed = projected[i - 1];
	}
    }

    /* Endpoint-only projection is insufficient for a UV polyline on a curved
     * surface: the lifted UV chord may bow away from the exact 3-D edge.  On
     * open surfaces, adaptively insert the exact curve midpoint whenever that
     * chord fails the same lift tolerance.  Periodic surfaces retain their
     * dedicated seam-aware refinement path below. */
    const bool all_projected = std::find(valid.begin(), valid.end(), false) ==
	valid.end();
    bool adaptively_refined = false;
    if (all_projected && !data->surf->IsClosed(0) &&
	    !data->surf->IsClosed(1) && parameters.size() > 1) {
	std::vector<ON_2dPoint> refined;
	refined.reserve(projected.size());
	refined.push_back(projected.front());
	bool refinement_valid = true;
	std::function<void(double, const ON_2dPoint &, double,
	    const ON_2dPoint &, int)> refine_interval;
	refine_interval = [data, curve, same_point_tol, within_distance_tol,
	    &refined, &refinement_valid, &refine_interval](double t0,
	    const ON_2dPoint &uv0, double t1, const ON_2dPoint &uv1, int depth) {
	    if (!refinement_valid) return;
	    if (brlcad::PullbackWorkCancelled() ||
		    refined.size() >= kMaximumAdaptivePullbackSamples) {
		refinement_valid = false;
		return;
	    }
	    const double midpoint_parameter = 0.5 * (t0 + t1);
	    const ON_3dPoint target = curve->PointAt(midpoint_parameter);
	    const ON_2dPoint chord_midpoint(0.5 * (uv0.x + uv1.x),
		0.5 * (uv0.y + uv1.y));
	    const ON_3dPoint chord_lift = data->surf->PointAt(
		chord_midpoint.x, chord_midpoint.y);
	    const double chord_error = target.IsValid() && chord_lift.IsValid() ?
		target.DistanceTo(chord_lift) : DBL_MAX;
	    if (chord_error <= within_distance_tol ||
		    depth >= kMaximumAdaptivePullbackDepth) {
		refined.push_back(uv1);
		return;
	    }

	    ON_2dPoint midpoint_uv = ON_2dPoint::UnsetPoint;
	    ON_3dPoint midpoint_lift = ON_3dPoint::UnsetPoint;
	    double midpoint_distance = DBL_MAX;
	    ++data->projection_samples;
	    pullback_closest_point_seeded(*data, target,
		chord_midpoint, midpoint_uv, midpoint_lift, midpoint_distance,
		same_point_tol);
	    bool projected_midpoint = midpoint_uv.IsValid() &&
		std::isfinite(midpoint_distance) && midpoint_distance < DBL_MAX;
	    if (!projected_midpoint) {
		pullback_closest_point(*data, target, midpoint_uv, midpoint_lift,
		    midpoint_distance, 0, same_point_tol, same_point_tol);
		projected_midpoint = midpoint_uv.IsValid() &&
		    std::isfinite(midpoint_distance) && midpoint_distance < DBL_MAX;
	    }
	    if (!projected_midpoint || midpoint_distance > within_distance_tol) {
		++data->rejected_projection_samples;
		if (!std::isfinite(midpoint_distance) || midpoint_distance >= DBL_MAX)
		    ++data->failed_projection_samples;
		else
		    data->maximum_projection_distance = std::max(
			data->maximum_projection_distance, midpoint_distance);
		refinement_valid = false;
		return;
	    }
	    if (std::isfinite(midpoint_distance) && midpoint_distance < DBL_MAX)
		data->maximum_projection_distance = std::max(
		    data->maximum_projection_distance, midpoint_distance);
	    refine_interval(t0, uv0, midpoint_parameter, midpoint_uv, depth + 1);
	    refine_interval(midpoint_parameter, midpoint_uv, t1, uv1, depth + 1);
	};
	for (size_t interval = 1; refinement_valid &&
		interval < parameters.size(); ++interval)
	    refine_interval(parameters[interval - 1], projected[interval - 1],
		parameters[interval], projected[interval], 0);
	if (refinement_valid) {
	    projected.swap(refined);
	    adaptively_refined = true;
	}
    }


    /* Adaptive refinement can insert points, so projected may now be larger
     * than the original per-parameter diagnostic arrays.  Account for the
     * original projections using their own extent and append the refined
     * samples independently.  Indexing projection_distances or valid with the
     * refined extent is an out-of-bounds read on any interval which needed a
     * midpoint. */
    for (size_t i = 0; i < projection_distances.size(); ++i) {
	if (std::isfinite(projection_distances[i]) &&
		projection_distances[i] < DBL_MAX)
	    data->maximum_projection_distance = std::max(
		data->maximum_projection_distance, projection_distances[i]);
	if (!adaptively_refined && (all_projected || valid[i])) {
	    samples->Append(projected[i]);
	    continue;
	}
	if (adaptively_refined)
	    continue;
	++data->rejected_projection_samples;
	if (!std::isfinite(projection_distances[i]) ||
		projection_distances[i] >= DBL_MAX)
	    ++data->failed_projection_samples;
	else
	    data->maximum_projection_distance = std::max(
		data->maximum_projection_distance, projection_distances[i]);
    }
    if (adaptively_refined) {
	for (size_t i = 0; i < projected.size(); ++i)
	    samples->Append(projected[i]);
    }
    if (brlcad::PullbackWorkCancelled())
	data->failure_reason = PullbackFailureReason::Cancelled;
    else if (data->failed_projection_samples)
	data->failure_reason = PullbackFailureReason::ProjectionFailed;
    else if (data->rejected_projection_samples &&
	data->maximum_projection_distance > within_distance_tol)
	data->failure_reason = PullbackFailureReason::SurfaceDistanceExceeded;
    else if (data->rejected_projection_samples)
	data->failure_reason = PullbackFailureReason::ProjectionFailed;
    delete[] knots;
    return samples;
}


/*
 * Unwrap 2D UV point values to within actual surface UV. Points often wrap around the closed seam.
 */
ON_2dPoint
UnwrapUVPoint(const ON_Surface *surf, const ON_2dPoint &pt, double tol)
{
    ON_2dPoint p = pt;

    for (int i=0; i<2; i++) {
	if (!surf->IsClosed(i))
	    continue;
	double length = surf->Domain(i).Length();
	double dom_min = surf->Domain(i).Min() - ON_ZERO_TOLERANCE;
	double dom_max = surf->Domain(i).Max() + ON_ZERO_TOLERANCE;

	if (p[i] < surf->Domain(i).m_t[0] - tol) {
	    int domains_away = (int)(((dom_min - p[i]) / length) + 1.0);
	    p[i] += length*domains_away;
	} else if (p[i] >= surf->Domain(i).m_t[1] + tol) {
	    int domains_away = (int)(((p[i] - dom_max) / length) + 1.0);
	    p[i] -= length * domains_away;
	}
    }

    return p;
}


double
DistToNearestClosedSeam(const ON_Surface *surf, const ON_2dPoint &pt)
{
    double dist = -1.0;
    ON_2dPoint unwrapped_pt = UnwrapUVPoint(surf, pt);
    for (int i=0; i<2; i++) {
	if (!surf->IsClosed(i))
	    continue;
	dist = fabs(unwrapped_pt[i] - surf->Domain(i)[0]);
	V_MIN(dist, fabs(surf->Domain(i)[1]-unwrapped_pt[i]));
    }
    return dist;
}


void
GetClosestExtendedPoint(const ON_Surface *surf, ON_2dPoint &pt, ON_2dPoint &prev_pt, double UNUSED(tol)) {
    if (surf->IsClosed(0)) {
	double length = surf->Domain(0).Length();
	double delta=pt.x-prev_pt.x;
	while (fabs(delta) > length/2.0) {
	    if (delta > length/2.0) {
		pt.x = pt.x - length;
		delta=pt.x-prev_pt.x;
	    } else {
		pt.x = pt.x + length;
		delta=pt.x - prev_pt.x;
	    }
	}
    }

    if (surf->IsClosed(1)) {
	double length = surf->Domain(1).Length();
	double delta=pt.y-prev_pt.y;
	while (fabs(delta) > length/2.0) {
	    if (delta > length/2.0) {
		pt.y = pt.y - length;
		delta=pt.y - prev_pt.y;
	    } else {
		pt.y = pt.y + length;
		delta=pt.y -prev_pt.y;
	    }
	}
    }
}


/*
 * Simple check to determine if two consecutive points pulled back from 3d curve sampling
 * to 2d UV parameter space crosses the seam of the closed UV. The assumption here is that
 * the sampling of the 3d curve is a small fraction of the UV domain.
 *
 *  // dir - 0 = not crossing, 1 = south/east bound, 2 = north/west bound
 */
bool
ConsecutivePointsCrossClosedSeam(const ON_Surface *surf, const ON_2dPoint &pt, const ON_2dPoint &prev_pt, int &udir, int &vdir, double tol)
{
    bool rc = false;

    /*
     * if one of the points is at a seam then not crossing
     */
    int dir =0;

    if (!IsAtSeam(surf, dir, pt, tol) && !IsAtSeam(surf, dir, prev_pt, tol)) {
	udir = vdir = 0;
	if (surf->IsClosed(0)) {
	    ON_2dPoint unwrapped_pt = UnwrapUVPoint(surf, pt);
	    ON_2dPoint unwrapped_prev_pt = UnwrapUVPoint(surf, prev_pt);
	    double delta=unwrapped_pt.x-unwrapped_prev_pt.x;
	    if (fabs(delta) > surf->Domain(0).Length()/2.0) {
		if (delta < 0.0) {
		    udir = 1; // east bound
		} else {
		    udir= 2; // west bound
		}
		rc = true;
	    }
	}
    }
    dir = 1;
    if (!IsAtSeam(surf, dir, pt, tol) && !IsAtSeam(surf, dir, prev_pt, tol)) {
	if (surf->IsClosed(1)) {
	    ON_2dPoint unwrapped_pt = UnwrapUVPoint(surf, pt);
	    ON_2dPoint unwrapped_prev_pt = UnwrapUVPoint(surf, prev_pt);
	    double delta=unwrapped_pt.y-unwrapped_prev_pt.y;
	    if (fabs(delta) > surf->Domain(1).Length()/2.0) {
		if (delta < 0.0) {
		    vdir = 2; // north bound
		} else {
		    vdir= 1; // south bound
		}
		rc = true;
	    }
	}
    }

    return rc;
}


/*
 * If within UV tolerance to a seam force force to actually seam value so surface
 * seam function can be used.
 */
void
ForceToClosestSeam(const ON_Surface *surf, ON_2dPoint &pt, double tol)
{
    int seam;
    ON_2dPoint unwrapped_pt = UnwrapUVPoint(surf, pt, tol);
    ON_2dVector wrap = ON_2dVector::ZeroVector;
    ON_Interval dom[2] = { ON_Interval::EmptyInterval, ON_Interval::EmptyInterval };
    double length[2] = { ON_UNSET_VALUE, ON_UNSET_VALUE};

    for (int i=0; i<2; i++) {
	dom[i] = surf->Domain(i);
	length[i] = dom[i].Length();
	if (!surf->IsClosed(i))
	    continue;
	if (pt[i] > (dom[i].m_t[1] + tol)) {
	    ON_2dPoint p = pt;
	    while (p[i] > (dom[i].m_t[1] + tol)) {
		p[i] -= length[i];
		wrap[i] += 1.0;
	    }
	} else if (pt[i] < (dom[i].m_t[0] - tol)) {
	    ON_2dPoint p = pt;
	    while (p[i] < (dom[i].m_t[0] - tol)) {
		p[i] += length[i];
		wrap[i] -= 1.0;
	    }
	}
	wrap[i] = wrap[i] * length[i];
    }

    if ((seam=IsAtSeam(surf, unwrapped_pt, tol)) > 0) {
	if (seam == 1) { // east/west seam
	    if (fabs(unwrapped_pt.x - dom[0].m_t[0]) < length[0]/2.0) {
		unwrapped_pt.x = dom[0].m_t[0]; // on east swap to west seam
	    } else {
		unwrapped_pt.x = dom[0].m_t[1]; // on west swap to east seam
	    }
	} else if (seam == 2) { // north/south seam
	    if (fabs(unwrapped_pt.y - dom[1].m_t[0]) < length[1]/2.0) {
		unwrapped_pt.y = dom[1].m_t[0]; // on north swap to south seam
	    } else {
		unwrapped_pt.y = dom[1].m_t[1]; // on south swap to north seam
	    }
	} else { //on both seams
	    if (fabs(unwrapped_pt.x - dom[0].m_t[0]) < length[0]/2.0) {
		unwrapped_pt.x = dom[0].m_t[0]; // on east swap to west seam
	    } else {
		unwrapped_pt.x = dom[0].m_t[1]; // on west swap to east seam
	    }
	    if (fabs(pt.y - dom[1].m_t[0]) < length[1]/2.0) {
		unwrapped_pt.y = dom[1].m_t[0]; // on north swap to south seam
	    } else {
		unwrapped_pt.y = dom[1].m_t[1]; // on south swap to north seam
	    }
	}
    }
    pt = unwrapped_pt + wrap;
}


/*
 * If point lies on a seam(s) swap to opposite side of UV.
 * hint = 1 swap E/W, 2 swap N/S or default 3 swap both
 */
void
SwapUVSeamPoint(const ON_Surface *surf, ON_2dPoint &p, int hint)
{
    int seam;
    ON_Interval dom[2];
    dom[0] = surf->Domain(0);
    dom[1] = surf->Domain(1);

    if ((seam=surf->IsAtSeam(p.x, p.y)) > 0) {
	if (seam == 1) { // east/west seam
	    if (fabs(p.x - dom[0].m_t[0]) > dom[0].Length()/2.0) {
		p.x = dom[0].m_t[0]; // on east swap to west seam
	    } else {
		p.x = dom[0].m_t[1]; // on west swap to east seam
	    }
	} else if (seam == 2) { // north/south seam
	    if (fabs(p.y - dom[1].m_t[0]) > dom[1].Length()/2.0) {
		p.y = dom[1].m_t[0]; // on north swap to south seam
	    } else {
		p.y = dom[1].m_t[1]; // on south swap to north seam
	    }
	} else { //on both seams check hint 1=east/west only, 2=north/south, 3 = both
	    if (hint == 1) {
		if (fabs(p.x - dom[0].m_t[0]) > dom[0].Length()/2.0) {
		    p.x = dom[0].m_t[0]; // on east swap to west seam
		} else {
		    p.x = dom[0].m_t[1]; // on west swap to east seam
		}
	    } else if (hint == 2) {
		if (fabs(p.y - dom[1].m_t[0]) > dom[1].Length()/2.0) {
		    p.y = dom[1].m_t[0]; // on north swap to south seam
		} else {
		    p.y = dom[1].m_t[1]; // on south swap to north seam
		}
	    } else {
		if (fabs(p.x - dom[0].m_t[0]) > dom[0].Length()/2.0) {
		    p.x = dom[0].m_t[0]; // on east swap to west seam
		} else {
		    p.x = dom[0].m_t[1]; // on west swap to east seam
		}
		if (fabs(p.y - dom[1].m_t[0]) > dom[1].Length()/2.0) {
		    p.y = dom[1].m_t[0]; // on north swap to south seam
		} else {
		    p.y = dom[1].m_t[1]; // on south swap to north seam
		}
	    }
	}
    }
}


/*
 * Find where Pullback of 3d curve crosses closed seam of surface UV
 */
bool
Find3DCurveSeamCrossing(PBCData &data, double t0, double t1, double UNUSED(offset), double &seam_t, ON_2dPoint &from, ON_2dPoint &to, double tol, double same_point_tol, double within_distance_tol)
{
    bool rc = true;
    const ON_Surface *surf = data.surf;

    // quick bail out is surface not closed
    if (surf->IsClosed(0) || surf->IsClosed(1)) {
	ON_2dPoint p0_2d = ON_2dPoint::UnsetPoint;
	ON_2dPoint p1_2d = ON_2dPoint::UnsetPoint;
	ON_3dPoint p0_3d = data.curve->PointAt(t0);
	ON_3dPoint p1_3d = data.curve->PointAt(t1);
	ON_Interval dom[2];
	double p0_distance;
	double p1_distance;

	dom[0] = surf->Domain(0);
	dom[1] = surf->Domain(1);

	ON_3dPoint check_pt_3d = ON_3dPoint::UnsetPoint;
	int udir=0;
	int vdir=0;
	if (pullback_closest_point(data, p0_3d, p0_2d, check_pt_3d,
		p0_distance, 0, same_point_tol, within_distance_tol) &&
	    pullback_closest_point(data, p1_3d, p1_2d, check_pt_3d,
		p1_distance, 0, same_point_tol, within_distance_tol)) {
	    if (ConsecutivePointsCrossClosedSeam(surf, p0_2d, p1_2d, udir, vdir, tol)) {
		ON_2dPoint p_2d;
		//lets check to see if p0 || p1 are already on a seam
		int seam0=0;
		if ((seam0 = IsAtSeam(surf, p0_2d, tol)) > 0) {
		    ForceToClosestSeam(surf, p0_2d, tol);
		}
		int seam1 = 0;
		if ((seam1 = IsAtSeam(surf, p1_2d, tol)) > 0) {
		    ForceToClosestSeam(surf, p1_2d, tol);
		}
		if (seam0 > 0) {
		    if (seam1 > 0) { // both p0 & p1 on seam shouldn't happen report error and return false
			rc = false;
		    } else { // just p0 on seam
			from = to = p0_2d;
			seam_t = t0;
			SwapUVSeamPoint(surf, to);
		    }
		} else if (seam1 > 0) { // only p1 on seam
		    from = to = p1_2d;
		    seam_t = t1;
		    SwapUVSeamPoint(surf, from);
		} else { // crosses the seam somewhere in between the two points
		    bool seam_not_found = true;
		    for (int iteration = 0; seam_not_found &&
			    iteration < kMaximumSeamCrossingIterations; ++iteration) {
			if (brlcad::PullbackWorkCancelled()) {
			    rc = false;
			    break;
			}
			double d0 = DistToNearestClosedSeam(surf, p0_2d);
			double d1 = DistToNearestClosedSeam(surf, p1_2d);
			if ((d0 > 0.0) && (d1 > 0.0)) {
			    double t = t0 + (t1 - t0)*(d0/(d0+d1));
			    const double parameter_guard = DBL_EPSILON * std::max(1.0,
				std::max(fabs(t0), fabs(t1)));
			    if (fabs(t - t0) <= parameter_guard ||
				    fabs(t - t1) <= parameter_guard) {
				seam_not_found = false;
				rc = false;
				continue;
			    }
			    int seam;
			    ON_3dPoint p_3d = data.curve->PointAt(t);
			    double distance;

			    if (pullback_closest_point(data, p_3d, p_2d,
				    check_pt_3d, distance, 0, same_point_tol,
				    within_distance_tol)) {
				seam = IsAtSeam(surf, p_2d, tol);
				if (seam > 0) {
				    ForceToClosestSeam(surf, p_2d, tol);
				    from = to = p_2d;
				    seam_t = t;
				    if (p0_2d.DistanceTo(p_2d) < p1_2d.DistanceTo(p_2d)) {
					SwapUVSeamPoint(surf, to);
				    } else {
					SwapUVSeamPoint(surf, from);
				    }
				    seam_not_found=false;
				    rc = true;
				} else {
				    if (ConsecutivePointsCrossClosedSeam(surf, p0_2d, p_2d, udir, vdir, tol)) {
					p1_2d = p_2d;
					t1 = t;
				    } else if (ConsecutivePointsCrossClosedSeam(surf, p_2d, p1_2d, udir, vdir, tol)) {
					p0_2d = p_2d;
					t0=t;
				    } else {
					seam_not_found=false;
					rc = false;
				    }
				}
			    } else {
				seam_not_found=false;
				rc = false;
			    }
			} else {
			    seam_not_found=false;
			    rc = false;
			}
		    }
		    if (seam_not_found)
			rc = false;
		}
	    } else {
		rc = false;
	    }
	} else {
	   rc = false;
	}
    } else {
	rc = false;
    }

    return rc;
}


/*
 * Find where 2D trim curve crosses closed seam of surface UV
 */
bool
FindTrimSeamCrossing(const ON_BrepTrim &trim, double t0, double t1, double &seam_t, ON_2dPoint &from, ON_2dPoint &to, double tol)
{
    bool rc = true;
    const ON_Surface *surf = trim.SurfaceOf();

    // quick bail out is surface not closed
    if (surf->IsClosed(0) || surf->IsClosed(1)) {
	ON_2dPoint p0 = trim.PointAt(t0);
	ON_2dPoint p1 = trim.PointAt(t1);
	ON_Interval dom[2];
	dom[0] = surf->Domain(0);
	dom[1] = surf->Domain(1);

	p0 = UnwrapUVPoint(surf, p0);
	p1 = UnwrapUVPoint(surf, p1);

        int udir=0;
        int vdir=0;
	if (ConsecutivePointsCrossClosedSeam(surf, p0, p1, udir, vdir, tol)) {
	    ON_2dPoint p;
	    //lets check to see if p0 || p1 are already on a seam
	    int seam0=0;
	    if ((seam0 = IsAtSeam(surf, p0, tol)) > 0) {
		ForceToClosestSeam(surf, p0, tol);
	    }
	    int seam1 = 0;
	    if ((seam1 = IsAtSeam(surf, p1, tol)) > 0) {
		ForceToClosestSeam(surf, p1, tol);
	    }
	    if (seam0 > 0) {
		if (seam1 > 0) { // both p0 & p1 on seam shouldn't happen report error and return false
		    rc = false;
		} else { // just p0 on seam
		    from = to = p0;
		    seam_t = t0;
		    SwapUVSeamPoint(surf, to);
		}
	    } else if (seam1 > 0) { // only p1 on seam
		from = to = p1;
		seam_t = t1;
		SwapUVSeamPoint(surf, from);
	    } else { // crosses the seam somewhere in between the two points
		bool seam_not_found = true;
		while (seam_not_found) {
		    double d0 = DistToNearestClosedSeam(surf, p0);
		    double d1 = DistToNearestClosedSeam(surf, p1);
		    if ((d0 > tol) && (d1 > tol)) {
			double t = t0 + (t1 - t0)*(d0/(d0+d1));
			int seam;
			p = trim.PointAt(t);
			seam = IsAtSeam(surf, p, tol);
			if (seam > 0) {
			    ForceToClosestSeam(surf, p, tol);
			    from = to = p;
			    seam_t = t;

			    if (p0.DistanceTo(p) < p1.DistanceTo(p)) {
				SwapUVSeamPoint(surf, to);
			    } else {
				SwapUVSeamPoint(surf, from);
			    }
			    seam_not_found=false;
			    rc = true;
			} else {
			    if (ConsecutivePointsCrossClosedSeam(surf, p0, p, udir, vdir, tol)) {
				p1 = p;
				t1 = t;
			    } else if (ConsecutivePointsCrossClosedSeam(surf, p, p1, udir, vdir, tol)) {
				p0 = p;
				t0=t;
			    } else {
				seam_not_found=false;
				rc = false;
			    }
			}
		    } else {
			seam_not_found=false;
			rc = false;
		    }
		}
	    }
	}
    }

    return rc;
}


void
pullback_samples_from_closed_surface(PBCData* data,
				     double t,
				     double s,
				     double same_point_tol,
				     double within_distance_tol)
{
    if (!data)
	return;

    if (!data->surf || !data->curve)
	return;

    const ON_Curve* curve= data->curve;
    const ON_Surface *surf = data->surf;

    size_t numKnots = curve->SpanCount();
    if (numKnots > INT_MAX - 1) {
	bu_log("Error - more than INT_MAX - 1 knots in curve\n");
	return;
    }
    ON_2dPointArray *samples= new ON_2dPointArray();
    double *knots = new double[(unsigned int)(numKnots+1)];

    curve->GetSpanVector(knots);

    size_t istart = 0;
    while ((istart < (numKnots+1)) && (t >= knots[istart]))
	istart++;

    if (istart > 0) {
	knots[--istart] = t;
    }

    size_t istop = numKnots;
    while ((istop > 0) && (s <= knots[istop]))
	istop--;

    if (istop < numKnots) {
	knots[++istop] = s;
    }

    size_t degree = curve->Degree();
    size_t samplesperknotinterval=18*degree;

    ON_2dPoint pt;
    ON_2dPoint prev_pt;
    bool have_previous = false;
    std::vector<double> sample_parameters;
    std::vector<bool> span_boundaries;
    double delta;
    for (size_t i = istart; i < istop; ++i) {
	delta = (knots[i + 1] - knots[i]) /
	    static_cast<double>(samplesperknotinterval);
	for (size_t j = 0; j <= samplesperknotinterval; ++j) {
	    if (j == samplesperknotinterval && i < istop - 1) continue;
	    const double parameter = knots[i] + j * delta;
	    if (!sample_parameters.empty() && NEAR_EQUAL(parameter,
		    sample_parameters.back(), DBL_EPSILON))
		continue;
	    sample_parameters.push_back(parameter);
	    span_boundaries.push_back(j == 0 || j == samplesperknotinterval);
	}
    }

    std::vector<ON_2dPoint> projected(sample_parameters.size(),
	ON_2dPoint::UnsetPoint);
    std::vector<double> projection_distances(sample_parameters.size(),
	DBL_MAX);
    std::vector<bool> valid(sample_parameters.size(), false);
    data->failure_reason = PullbackFailureReason::None;
    data->projection_samples = sample_parameters.size();
    data->rejected_projection_samples = 0;
    data->failed_projection_samples = 0;
    data->maximum_projection_distance = 0.0;

    /* Establish the surface sheet at a globally checked knot boundary before
     * following local UV continuity.  Endpoints are tested first because
     * STEP topology normally constrains them most strongly; if none satisfies
     * the active tolerance, the closest finite boundary candidate is still a
     * deterministic seed for measuring the source mismatch. */
    std::vector<size_t> anchor_candidates;
    if (!sample_parameters.empty()) anchor_candidates.push_back(0);
    if (sample_parameters.size() > 1)
	anchor_candidates.push_back(sample_parameters.size() - 1);
    for (size_t i = 1; i + 1 < sample_parameters.size(); ++i) {
	if (span_boundaries[i]) anchor_candidates.push_back(i);
    }
    size_t anchor = sample_parameters.size();
    double anchor_distance = DBL_MAX;
    for (std::vector<size_t>::const_iterator candidate =
	    anchor_candidates.begin(); candidate != anchor_candidates.end();
	    ++candidate) {
	if (brlcad::PullbackWorkCancelled()) break;
	const size_t index = *candidate;
	ON_3dPoint lift = ON_3dPoint::UnsetPoint;
	pullback_closest_point(*data, curve->PointAt(sample_parameters[index]),
	    projected[index], lift, projection_distances[index], 0,
	    same_point_tol, same_point_tol);
	const double distance = projection_distances[index];
	if (!projected[index].IsValid() || !std::isfinite(distance) ||
		distance >= anchor_distance)
	    continue;
	anchor = index;
	anchor_distance = distance;
	if (distance <= same_point_tol) break;
    }

    const auto project_from_seed = [data, curve, &sample_parameters,
	&projected, &projection_distances, &valid, same_point_tol,
	within_distance_tol](size_t index, const ON_2dPoint &seed) {
	if (brlcad::PullbackWorkCancelled()) return false;
	ON_3dPoint lift = ON_3dPoint::UnsetPoint;
	double &distance = projection_distances[index];
	projected[index] = ON_2dPoint::UnsetPoint;
	distance = DBL_MAX;
	pullback_closest_point_seeded(*data,
	    curve->PointAt(sample_parameters[index]), seed, projected[index],
	    lift, distance, same_point_tol);
	bool have_candidate = projected[index].IsValid() &&
	    std::isfinite(distance) && distance < DBL_MAX;
	if (!have_candidate || distance > within_distance_tol) {
	    projected[index] = ON_2dPoint::UnsetPoint;
	    distance = DBL_MAX;
	    pullback_closest_point(*data,
		curve->PointAt(sample_parameters[index]), projected[index], lift,
		distance, 0, same_point_tol, same_point_tol);
	    have_candidate = projected[index].IsValid() &&
		std::isfinite(distance) && distance < DBL_MAX;
	}
	valid[index] = have_candidate && distance <= within_distance_tol;
	return have_candidate;
    };
    if (anchor < sample_parameters.size()) {
	valid[anchor] = anchor_distance <= within_distance_tol;
	ON_2dPoint seed = projected[anchor];
	for (size_t i = anchor + 1; i < sample_parameters.size(); ++i) {
	    if (project_from_seed(i, seed)) seed = projected[i];
	}
	seed = projected[anchor];
	for (size_t i = anchor; i > 0; --i) {
	    if (project_from_seed(i - 1, seed)) seed = projected[i - 1];
	}
    }

    double prev_t = knots[istart];
    double offset = 0.0;
    size_t sample_index = 0;
    for (size_t i=istart; i<istop; i++) {
	delta = (knots[i+1] - knots[i])/(double)samplesperknotinterval;
	for (size_t j=0; j<=samplesperknotinterval; j++) {
	    if ((j == samplesperknotinterval) && (i < istop - 1))
		continue;

	    double curr_t = knots[i]+j*delta;
	    if (sample_index >= sample_parameters.size()) continue;
	    curr_t = sample_parameters[sample_index];
	    if (curr_t < (s-t)/2.0) {
		offset = PBC_FROM_OFFSET;
	    } else {
		offset = -PBC_FROM_OFFSET;
	    }
	    pt = projected[sample_index];
	    const double distance = projection_distances[sample_index];
	    const bool pulled = valid[sample_index];
	    ++sample_index;
	    if (std::isfinite(distance) && distance < DBL_MAX)
		data->maximum_projection_distance = std::max(
		    data->maximum_projection_distance, distance);
	    if (pulled) {
		if (IsAtSeam(surf, pt, PBC_SEAM_TOL) > 0) {
		    ForceToClosestSeam(surf, pt, PBC_SEAM_TOL);
		}
		if (!have_previous) {
		    // first point just append and set reference in prev_pt
		    samples->Append(pt);
		    prev_pt = pt;
		    prev_t = curr_t;
		    have_previous = true;
		    continue;
		}
		int udir= 0;
		int vdir= 0;
		if (ConsecutivePointsCrossClosedSeam(surf, pt, prev_pt, udir, vdir, PBC_SEAM_TOL)) {
		    int pt_seam = surf->IsAtSeam(pt.x, pt.y);
		    int prev_pt_seam = surf->IsAtSeam(prev_pt.x, prev_pt.y);
		    if (pt_seam > 0) {
			if ((prev_pt_seam > 0) && (samples->Count() == 1)) {
			    samples->Empty();
			    SwapUVSeamPoint(surf, prev_pt, pt_seam);
			    samples->Append(prev_pt);
			} else {
			    if (pt_seam == 3) {
				if (prev_pt_seam == 1) {
				    pt.x = prev_pt.x;
				    if (ConsecutivePointsCrossClosedSeam(surf, pt, prev_pt, udir, vdir, PBC_SEAM_TOL)) {
					SwapUVSeamPoint(surf, pt, 2);
				    }
				} else if (prev_pt_seam == 2) {
				    pt.y = prev_pt.y;
				    if (ConsecutivePointsCrossClosedSeam(surf, pt, prev_pt, udir, vdir, PBC_SEAM_TOL)) {
					SwapUVSeamPoint(surf, pt, 1);
				    }
				}
			    } else {
				SwapUVSeamPoint(surf, pt, prev_pt_seam);
			    }
			}
		    } else if (prev_pt_seam > 0) {
			if (samples->Count() == 1) {
			    samples->Empty();
			    SwapUVSeamPoint(surf, prev_pt);
			    samples->Append(prev_pt);
			}
		    } else if (data->curve->IsClosed()) {
			ON_2dPoint from, to;
			double seam_t;
			if (Find3DCurveSeamCrossing(*data, prev_t, curr_t, offset, seam_t, from, to, PBC_TOL, same_point_tol, within_distance_tol)) {
			    samples->Append(from);
			    data->segments->push_back(samples);
			    samples= new ON_2dPointArray();
			    samples->Append(to);
			    prev_pt = to;
			} else {
			    ++data->failed_seam_crossing_searches;
			}
		    }
		}
		samples->Append(pt);

		prev_pt = pt;
		prev_t = curr_t;
	    } else {
		++data->rejected_projection_samples;
		if (!std::isfinite(distance) || distance >= DBL_MAX)
		    ++data->failed_projection_samples;
		else
		    data->maximum_projection_distance = std::max(
			data->maximum_projection_distance, distance);
	    }
	}
    }

    if (brlcad::PullbackWorkCancelled())
	data->failure_reason = PullbackFailureReason::Cancelled;
    else if (data->failed_projection_samples)
	data->failure_reason = PullbackFailureReason::ProjectionFailed;
    else if (data->rejected_projection_samples &&
	data->maximum_projection_distance > within_distance_tol)
	data->failure_reason = PullbackFailureReason::SurfaceDistanceExceeded;
    else if (data->rejected_projection_samples)
	data->failure_reason = PullbackFailureReason::ProjectionFailed;

    /* If the unseeded closed-surface search found only one usable point, it
     * may have found that anchor late in curve order.  Recover both parameter
     * directions from the anchor.  Every point must pass the same strict
     * closest-point distance used above; otherwise retain the original
     * collapsed result so the caller can reject or repair it explicitly. */
    if (samples->Count() < 2 && have_previous && sample_parameters.size() > 1 &&
	!brlcad::PullbackWorkCancelled()) {
	size_t recovery_anchor = 0;
	double anchor_delta = DBL_MAX;
	for (size_t i = 0; i < sample_parameters.size(); ++i) {
	    const double candidate_delta = fabs(sample_parameters[i] - prev_t);
	    if (candidate_delta < anchor_delta) {
		recovery_anchor = i;
		anchor_delta = candidate_delta;
	    }
	}
	std::vector<ON_2dPoint> recovered(sample_parameters.size(),
	    ON_2dPoint::UnsetPoint);
	std::vector<bool> recovery_valid(sample_parameters.size(), false);
	recovered[recovery_anchor] = prev_pt;
	recovery_valid[recovery_anchor] = true;
	const auto recover = [data, &curve, &sample_parameters, &recovered,
		&recovery_valid, same_point_tol, within_distance_tol](size_t index,
		    const ON_2dPoint &seed) {
	    if (brlcad::PullbackWorkCancelled()) return false;
	    const ON_3dPoint target = curve->PointAt(sample_parameters[index]);
	    ON_3dPoint lift = ON_3dPoint::UnsetPoint;
	    double recovered_distance = DBL_MAX;
	    pullback_closest_point_seeded(*data, target, seed,
		recovered[index], lift, recovered_distance, same_point_tol);
	    bool recovery_projected = recovered[index].IsValid() &&
		std::isfinite(recovered_distance) &&
		recovered_distance <= within_distance_tol;
	    if (!recovery_projected) {
		recovered[index] = ON_2dPoint::UnsetPoint;
		recovered_distance = DBL_MAX;
		pullback_closest_point(*data, target, recovered[index], lift,
		    recovered_distance, 0, same_point_tol, same_point_tol);
		recovery_projected = recovered[index].IsValid() &&
		    std::isfinite(recovered_distance) &&
		    recovered_distance <= within_distance_tol;
	    }
	    recovery_valid[index] = recovery_projected;
	    return static_cast<bool>(recovery_valid[index]);
	};
	bool recovered_all = true;
	for (size_t i = recovery_anchor + 1; i < sample_parameters.size(); ++i) {
	    if (!recover(i, recovered[i - 1])) {
		recovered_all = false;
		break;
	    }
	}
	for (size_t i = recovery_anchor; recovered_all && i > 0; --i) {
	    if (!recover(i - 1, recovered[i])) {
		recovered_all = false;
		break;
	    }
	}
	if (recovered_all) {
	    for (size_t i = 1; i < recovered.size(); ++i) {
		for (int direction = 0; direction < 2; ++direction) {
		    if (!surf->IsClosed(direction)) continue;
		    const double period = surf->Domain(direction).Length();
		    if (period > ON_ZERO_TOLERANCE)
			recovered[i][direction] += round((recovered[i - 1][direction] -
			    recovered[i][direction]) / period) * period;
		}
	    }
	    samples->Empty();
	    for (std::vector<ON_2dPoint>::const_iterator point = recovered.begin();
		 point != recovered.end(); ++point)
		samples->Append(*point);
	} else if (!brlcad::PullbackWorkCancelled()) {
	    /* Low-level numerical routines must not bypass the converter's
	     * structured diagnostic stream.  The caller supplies entity context
	     * and aggregates this exact count. */
	    ++data->failed_seam_crossing_searches;
	}
    }
    delete [] knots;

    if (samples != NULL) {
	data->segments->push_back(samples);

	size_t numsegs = data->segments->size();

	if (numsegs > 1) {
	    if (curve->IsClosed()) {
		ON_2dPointArray *reordered_samples= new ON_2dPointArray();
		// must have walked over seam but have closed curve so reorder stitching
		size_t seg = 0;
		for (std::list<ON_2dPointArray *>::reverse_iterator rit=data->segments->rbegin(); rit!=data->segments->rend(); ++seg) {
		    samples = *rit;
		    if (seg < numsegs-1) { // since end points should be repeated
			reordered_samples->Append(samples->Count()-1, (const ON_2dPoint *)samples->Array());
		    } else {
			reordered_samples->Append(samples->Count(), (const ON_2dPoint *)samples->Array());
		    }
		    data->segments->erase((++rit).base());
		    rit = data->segments->rbegin();
		    delete samples;
		}
		data->segments->clear();
		data->segments->push_back(reordered_samples);

	    } else {
		//punt for now
	    }
	}
    }

    return;
}


PBCData *
pullback_samples(const ON_Surface* surf,
		 const ON_Curve* curve,
		 double tolerance,
		 double flatness,
		 double same_point_tol,
		 double within_distance_tol,
		 const std::shared_ptr<brlcad::PullbackContext> &context)
{
    if (!surf)
	return NULL;

    PBCData *data = new PBCData;
    data->tolerance = tolerance;
    data->flatness = flatness;
    data->curve = curve;
    data->surf = surf;
    data->surface_closed[0] = surf->IsClosed(0);
    data->surface_closed[1] = surf->IsClosed(1);
    data->surface_domain[0] = surf->Domain(0);
    data->surface_domain[1] = surf->Domain(1);
    data->surface_parameterization_cached = true;
    data->context = context;
    data->surftree = NULL;
    data->segments = new std::list<ON_2dPointArray *>();
    data->failure_reason = PullbackFailureReason::None;
    data->projection_samples = 0;
    data->rejected_projection_samples = 0;
    data->failed_projection_samples = 0;
    data->maximum_projection_distance = 0.0;
    data->tolerance_adjusted = false;
    data->declared_tolerance = tolerance;
    data->maximum_recovery_tolerance = tolerance;
    data->maximum_recovery_distance = 0.0;

    double tmin, tmax;
    data->curve->GetDomain(&tmin, &tmax);

    if (data->surface_closed[0] || data->surface_closed[1]) {
	const double parameter_scale = std::max(1.0,
	    std::max(fabs(tmin), fabs(tmax)));
	const double zero_split_tolerance = DBL_EPSILON *
	    kParameterZeroSplitEpsilonScale * parameter_scale;
	if ((tmin < -zero_split_tolerance) &&
		(tmax > zero_split_tolerance)) {
	    ON_2dPoint uv = ON_2dPoint::UnsetPoint;
	    ON_3dPoint p = curve->PointAt(0.0);
	    ON_3dPoint p3d = ON_3dPoint::UnsetPoint;
	    double distance = DBL_MAX;
	    int quadrant = 0; // optional - 0 = default, 1 from NE quadrant, 2 from NW quadrant, 3 from SW quadrant, 4 from SE quadrant
	    pullback_closest_point(*data, p, uv, p3d, distance, quadrant,
		same_point_tol, same_point_tol);
	    if (uv.IsValid() && std::isfinite(distance) &&
		distance <= within_distance_tol) {
		if (IsAtSeam(surf, uv, PBC_SEAM_TOL) > 0) {
		    ON_2dPointArray *samples1 = pullback_samples(data, tmin, 0.0, same_point_tol, within_distance_tol);
		    ON_2dPointArray *samples2 = pullback_samples(data, 0.0, tmax, same_point_tol, within_distance_tol);
		    if (samples1 != NULL) {
			data->segments->push_back(samples1);
		    }
		    if (samples2 != NULL) {
			data->segments->push_back(samples2);
		    }
		} else {
		    ON_2dPointArray *samples = pullback_samples(data, tmin, tmax, same_point_tol, within_distance_tol);
		    if (samples != NULL) {
			data->segments->push_back(samples);
		    }
		}
	    } else {
		/* Parameter zero is used only to decide whether a split belongs on a
		 * periodic seam.  Failing the strict distance test there must not
		 * discard the whole diagnostic result: sample the unsplit curve so the
		 * caller can distinguish solver failure from a bounded source
		 * curve/surface mismatch and, in non-exact mode, adjust that edge. */
		ON_2dPointArray *samples = pullback_samples(data, tmin, tmax,
		    same_point_tol, within_distance_tol);
		if (samples != NULL)
		    data->segments->push_back(samples);
	    }
	} else {
	    pullback_samples_from_closed_surface(data, tmin, tmax, same_point_tol, within_distance_tol);
	}
    } else {
	ON_2dPointArray *samples = pullback_samples(data, tmin, tmax, same_point_tol, within_distance_tol);
	if (samples != NULL) {
	    data->segments->push_back(samples);
	}
    }
    return data;
}


ON_Curve*
refit_edge(const ON_BrepEdge* edge, double UNUSED(tolerance))
{
    double edge_tolerance = 0.01;
    ON_Brep *brep = edge->Brep();
#ifdef SHOW_UNUSED
    ON_3dPoint start = edge->PointAtStart();
    ON_3dPoint end = edge->PointAtEnd();
#endif

    ON_BrepTrim& trim1 = brep->m_T[edge->m_ti[0]];
    ON_BrepTrim& trim2 = brep->m_T[edge->m_ti[1]];
    ON_BrepFace *face1 = trim1.Face();
    ON_BrepFace *face2 = trim2.Face();
    const ON_Surface *surface1 = face1->SurfaceOf();
    const ON_Surface *surface2 = face2->SurfaceOf();
    bool removeTrimmed = false;
    brlcad::SurfaceTree st1(face1, removeTrimmed);
    brlcad::SurfaceTree st2(face2, removeTrimmed);

    ON_Curve *curve = brep->m_C3[edge->m_c3i];
    double t0, t1;
    curve->GetDomain(&t0, &t1);
    ON_Plane plane;
    curve->FrameAt(t0, plane);
#ifdef SHOW_UNUSED
    ON_3dPoint origin = plane.Origin();
    ON_3dVector xaxis = plane.Xaxis();
    ON_3dVector yaxis = plane.Yaxis();
    ON_3dVector zaxis = plane.zaxis;
    ON_3dPoint px = origin + xaxis;
    ON_3dPoint py = origin + yaxis;
    ON_3dPoint pz = origin + zaxis;
#endif

    int numKnots = curve->SpanCount();
    double *knots = new double[numKnots + 1];
    curve->GetSpanVector(knots);

    int samplesperknotinterval;
    int degree = curve->Degree();

    if (degree > 1) {
	samplesperknotinterval = 3 * degree;
    } else {
	samplesperknotinterval = 18 * degree;
    }
    double t = 0.0;
    ON_3dPoint pointOnCurve;
    ON_3dPoint knudgedPointOnCurve;
    for (int i = 0; i <= numKnots; i++) {
	if (i <= numKnots / 2) {
	    if (i > 0) {
		double delta = (knots[i] - knots[i - 1]) / (double) samplesperknotinterval;
		for (int j = 1; j < samplesperknotinterval; j++) {
		    t = knots[i - 1] + j * delta;
		    pointOnCurve = curve->PointAt(t);
		    knudgedPointOnCurve = curve->PointAt(t + PBC_TOL);

		    ON_3dPoint point = pointOnCurve;
		    ON_3dPoint knudgepoint = knudgedPointOnCurve;
		    ON_3dPoint ps1;
		    ON_3dPoint ps2;
		    bool found = false;
		    double dist;
		    while (!found) {
			ON_2dPoint uv;
			if (st1.getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
			    ps1 = surface1->PointAt(uv.x, uv.y);
			    if (st2.getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
				ps2 = surface2->PointAt(uv.x, uv.y);
			    }
			}
			dist = ps1.DistanceTo(ps2);
			if (NEAR_ZERO(dist, PBC_TOL)) {
			    point = ps1;
			    found = true;
			} else {
			    ON_3dVector v1 = ps1 - point;
			    ON_3dVector v2 = ps2 - point;
			    knudgepoint = point;
			    ON_3dVector deltav = v1 + v2;
			    if (NEAR_ZERO(deltav.Length(), PBC_TOL)) {
				found = true; // as close as we are going to get
			    } else {
				point = point + v1 + v2;
			    }
			}
		    }
		}
	    }
	    t = knots[i];
	    pointOnCurve = curve->PointAt(t);
	    knudgedPointOnCurve = curve->PointAt(t + PBC_TOL);
	    ON_3dPoint point = pointOnCurve;
	    ON_3dPoint knudgepoint = knudgedPointOnCurve;
	    ON_3dPoint ps1;
	    ON_3dPoint ps2;
	    bool found = false;
	    double dist;

	    while (!found) {
		ON_2dPoint uv;
		if (st1.getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
		    ps1 = surface1->PointAt(uv.x, uv.y);
		    if (st2.getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
			ps2 = surface2->PointAt(uv.x, uv.y);
		    }
		}
		dist = ps1.DistanceTo(ps2);
		if (NEAR_ZERO(dist, PBC_TOL)) {
		    point = ps1;
		    found = true;
		} else {
		    ON_3dVector v1 = ps1 - point;
		    ON_3dVector v2 = ps2 - point;
		    knudgepoint = point;
		    ON_3dVector deltav = v1 + v2;
		    if (NEAR_ZERO(deltav.Length(), PBC_TOL)) {
			found = true; // as close as we are going to get
		    } else {
			point = point + v1 + v2;
		    }
		}
	    }
	} else {
	    if (i > 0) {
		double delta = (knots[i] - knots[i - 1]) / (double) samplesperknotinterval;
		for (int j = 1; j < samplesperknotinterval; j++) {
		    t = knots[i - 1] + j * delta;
		    pointOnCurve = curve->PointAt(t);
		    knudgedPointOnCurve = curve->PointAt(t - PBC_TOL);

		    ON_3dPoint point = pointOnCurve;
		    ON_3dPoint knudgepoint = knudgedPointOnCurve;
		    ON_3dPoint ps1;
		    ON_3dPoint ps2;
		    bool found = false;
		    double dist;

		    while (!found) {
			ON_2dPoint uv;
			if (st1.getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
			    ps1 = surface1->PointAt(uv.x, uv.y);
			    if (st2.getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
				ps2 = surface2->PointAt(uv.x, uv.y);
			    }
			}
			dist = ps1.DistanceTo(ps2);
			if (NEAR_ZERO(dist, PBC_TOL)) {
			    point = ps1;
			    found = true;
			} else {
			    ON_3dVector v1 = ps1 - point;
			    ON_3dVector v2 = ps2 - point;
			    knudgepoint = point;
			    ON_3dVector deltav = v1 + v2;
			    if (NEAR_ZERO(deltav.Length(), PBC_TOL)) {
				found = true; // as close as we are going to get
			    } else {
				point = point + v1 + v2;
			    }
			}
		    }
		}
		t = knots[i];
		pointOnCurve = curve->PointAt(t);
		knudgedPointOnCurve = curve->PointAt(t - PBC_TOL);
		ON_3dPoint point = pointOnCurve;
		ON_3dPoint knudgepoint = knudgedPointOnCurve;
		ON_3dPoint ps1;
		ON_3dPoint ps2;
		bool found = false;
		double dist;

		while (!found) {
		    ON_2dPoint uv;
		    if (st1.getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
			ps1 = surface1->PointAt(uv.x, uv.y);
			if (st2.getSurfacePoint((const ON_3dPoint&) point, uv, (const ON_3dPoint&) knudgepoint, edge_tolerance) > 0) {
			    ps2 = surface2->PointAt(uv.x, uv.y);
			}
		    }
		    dist = ps1.DistanceTo(ps2);
		    if (NEAR_ZERO(dist, PBC_TOL)) {
			point = ps1;
			found = true;
		    } else {
			ON_3dVector v1 = ps1 - point;
			ON_3dVector v2 = ps2 - point;
			knudgepoint = point;
			ON_3dVector deltav = v1 + v2;
			if (NEAR_ZERO(deltav.Length(), PBC_TOL)) {
			    found = true; // as close as we are going to get
			} else {
			    point = point + v1 + v2;
			}
		    }
		}
	    }
	}
    }
    delete [] knots;


    return NULL;
}


bool
has_singularity(const ON_Surface *surf)
{
    bool ret = false;
    if (UNLIKELY(!surf)) return ret;
    // 0 = south, 1 = east, 2 = north, 3 = west
    for (int i = 0; i < 4; i++) {
	if (surf->IsSingular(i)) {
	    /*
	      switch (i) {
	      case 0:
	      std::cout << "Singular South" << std::endl;
	      break;
	      case 1:
	      std::cout << "Singular East" << std::endl;
	      break;
	      case 2:
	      std::cout << "Singular North" << std::endl;
	      break;
	      case 3:
	      std::cout << "Singular West" << std::endl;
	      }
	    */
	    ret = true;
	}
    }
    return ret;
}


bool is_closed(const ON_Surface *surf)
{
    bool ret = false;
    if (UNLIKELY(!surf)) return ret;
    // dir 0 = "s", 1 = "t"
    for (int i = 0; i < 2; i++) {
	if (surf->IsClosed(i)) {
//			switch (i) {
//			case 0:
//				std::cout << "Closed in U" << std::endl;
//				break;
//			case 1:
//				std::cout << "Closed in V" << std::endl;
//			}
	    ret = true;
	}
    }
    return ret;
}


bool
check_pullback_closed(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator d = pbcs.begin();
    if ((*d) == NULL || (*d)->surf == NULL)
	return false;

    const ON_Surface *surf = (*d)->surf;

    //TODO:
    // 0 = U, 1 = V
    if (surf->IsClosed(0) && surf->IsClosed(1)) {
	//TODO: need to check how torus UV looks to determine checks
	std::cerr << "Is this some kind of torus????" << std::endl;
    } else if (surf->IsClosed(0)) {
	//check_pullback_closed_U(pbcs);
	std::cout << "check closed in U" << std::endl;
    } else if (surf->IsClosed(1)) {
	//check_pullback_closed_V(pbcs);
	std::cout << "check closed in V" << std::endl;
    }
    return true;
}


bool
check_pullback_singular_east(std::list<PBCData*> &pbcs)
{
    std::list<PBCData *>::iterator cs = pbcs.begin();
    if ((*cs) == NULL || (*cs)->surf == NULL)
	return false;

    const ON_Surface *surf = (*cs)->surf;
    double umin, umax;
    ON_2dPoint *prev = NULL;

    surf->GetDomain(0, &umin, &umax);
    std::cout << "Umax: " << umax << std::endl;
    while (cs != pbcs.end()) {
	PBCData *data = (*cs);
	std::list<ON_2dPointArray *>::iterator si = data->segments->begin();
	int segcnt = 0;
	while (si != data->segments->end()) {
	    ON_2dPointArray *samples = (*si);
	    std::cerr << std::endl << "Segment:" << ++segcnt << std::endl;
	    if (true) {
		int ilast = samples->Count() - 1;
		std::cerr << std::endl << 0 << "- " << (*samples)[0].x << ", " << (*samples)[0].y << std::endl;
		std::cerr << ilast << "- " << (*samples)[ilast].x << ", " << (*samples)[ilast].y << std::endl;
	    } else {
		for (int i = 0; i < samples->Count(); i++) {
		    if (NEAR_EQUAL((*samples)[i].x, umax, PBC_TOL)) {
			if (prev != NULL) {
			    std::cerr << "prev - " << prev->x << ", " << prev->y << std::endl;
			}
			std::cerr << i << "- " << (*samples)[i].x << ", " << (*samples)[i].y << std::endl << std::endl;
		    }
		    prev = &(*samples)[i];
		}
	    }
	    si ++;
	}
	cs++;
    }
    return true;
}


bool
check_pullback_singular(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator d = pbcs.begin();
    if ((*d) == NULL || (*d)->surf == NULL)
	return false;

    const ON_Surface *surf = (*d)->surf;
    int cnt = 0;

    for (int i = 0; i < 4; i++) {
	if (surf->IsSingular(i)) {
	    cnt++;
	}
    }
    if (cnt > 2) {
	//TODO: I don't think this makes sense but check out
	std::cerr << "Is this some kind of sickness????" << std::endl;
	return false;
    } else if (cnt == 2) {
	if (surf->IsSingular(0) && surf->IsSingular(2)) {
	    std::cout << "check singular North-South" << std::endl;
	} else if (surf->IsSingular(1) && surf->IsSingular(2)) {
	    std::cout << "check singular East-West" << std::endl;
	} else {
	    //TODO: I don't think this makes sense but check out
	    std::cerr << "Is this some kind of sickness????" << std::endl;
	    return false;
	}
    } else {
	if (surf->IsSingular(0)) {
	    std::cout << "check singular South" << std::endl;
	} else if (surf->IsSingular(1)) {
	    std::cout << "check singular East" << std::endl;
	    if (check_pullback_singular_east(pbcs)) {
		return true;
	    }
	} else if (surf->IsSingular(2)) {
	    std::cout << "check singular North" << std::endl;
	} else if (surf->IsSingular(3)) {
	    std::cout << "check singular West" << std::endl;
	}
    }
    return true;
}


#ifdef _DEBUG_TESTING_
void
print_pullback_data(std::string str, std::list<PBCData*> &pbcs, bool justendpoints)
{
    std::list<PBCData*>::iterator cs = pbcs.begin();
    int trimcnt = 0;
    if (justendpoints) {
	// print out endpoints before
	std::cerr << "EndPoints " << str << ":" << std::endl;
	while (cs != pbcs.end()) {
	    PBCData *data = (*cs);
	    if (!data || !data->surf)
		continue;

	    const ON_Surface *surf = data->surf;
	    std::list<ON_2dPointArray *>::iterator si = data->segments->begin();
	    int segcnt = 0;
	    while (si != data->segments->end()) {
		ON_2dPointArray *samples = (*si);
		std::cerr << std::endl << "  Segment:" << ++segcnt << std::endl;
		int ilast = samples->Count() - 1;
		std::cerr << "    T:" << ++trimcnt << std::endl;
		int i = 0;
		int singularity = IsAtSingularity(surf, (*samples)[i], PBC_SEAM_TOL);
		int seam = IsAtSeam(surf, (*samples)[i], PBC_SEAM_TOL);
		std::cerr << "--------";
		if ((seam > 0) && (singularity >= 0)) {
		    std::cerr << " S/S  " << (*samples)[i].x << ", " << (*samples)[i].y;
		} else if (seam > 0) {
		    std::cerr << " Seam " << (*samples)[i].x << ", " << (*samples)[i].y;
		} else if (singularity >= 0) {
		    std::cerr << " Sing " << (*samples)[i].x << ", " << (*samples)[i].y;
		} else {
		    std::cerr << "      " << (*samples)[i].x << ", " << (*samples)[i].y;
		}
		ON_3dPoint p = surf->PointAt((*samples)[i].x, (*samples)[i].y);
		std::cerr << "  (" << p.x << ", " << p.y << ", " << p.z << ") " << std::endl;

		i = ilast;
		singularity = IsAtSingularity(surf, (*samples)[i], PBC_SEAM_TOL);
		seam = IsAtSeam(surf, (*samples)[i], PBC_SEAM_TOL);
		std::cerr << "        ";
		if ((seam > 0) && (singularity >= 0)) {
		    std::cerr << " S/S  " << (*samples)[i].x << ", " << (*samples)[i].y << std::endl;
		} else if (seam > 0) {
		    std::cerr << " Seam " << (*samples)[i].x << ", " << (*samples)[i].y << std::endl;
		} else if (singularity >= 0) {
		    std::cerr << " Sing " << (*samples)[i].x << ", " << (*samples)[i].y << std::endl;
		} else {
		    std::cerr << "      " << (*samples)[i].x << ", " << (*samples)[i].y << std::endl;
		}
		p = surf->PointAt((*samples)[i].x, (*samples)[i].y);
		std::cerr << "  (" << p.x << ", " << p.y << ", " << p.z << ") " << std::endl;
		si++;
	    }
	    cs++;
	}
    } else {
	// print out all points
	trimcnt = 0;
	cs = pbcs.begin();
	std::cerr << str << ":" << std::endl;
	while (cs != pbcs.end()) {
	    PBCData *data = (*cs);
	    if (!data || !data->surf)
		continue;

	    const ON_Surface *surf = data->surf;
	    std::list<ON_2dPointArray *>::iterator si = data->segments->begin();
	    int segcnt = 0;
	    std::cerr << "2d surface domain: " << std::endl;
	    std::cerr << "in rpp rpp" << surf->Domain(0).m_t[0] << " " << surf->Domain(0).m_t[1] << " " << surf->Domain(1).m_t[0] << " " << surf->Domain(1).m_t[1] << " 0.0 0.01"  << std::endl;
	    while (si != data->segments->end()) {
		ON_2dPointArray *samples = (*si);
		std::cerr << std::endl << "  Segment:" << ++segcnt << std::endl;
		std::cerr << "    T:" << ++trimcnt << std::endl;
		for (int i = 0; i < samples->Count(); i++) {
		    int singularity = IsAtSingularity(surf, (*samples)[i], PBC_SEAM_TOL);
		    int seam = IsAtSeam(surf, (*samples)[i], PBC_SEAM_TOL);
		    if (_debug_print_mged2d_points_) {
			std::cerr << "in pt_" << _debug_point_count_++ << " sph " << (*samples)[i].x << " " << (*samples)[i].y << " 0.0 0.1000"  << std::endl;
		    } else if (_debug_print_mged3d_points_) {
			ON_3dPoint p = surf->PointAt((*samples)[i].x, (*samples)[i].y);
			std::cerr << "in pt_" << _debug_point_count_++ << " sph " << p.x << " " << p.y << " " << p.z << " 0.1000"  << std::endl;
		    } else {
			if (i == 0) {
			    std::cerr << "--------";
			} else {
			    std::cerr << "        ";
			}
			if ((seam > 0) && (singularity >= 0)) {
			    std::cerr << " S/S  " << (*samples)[i].x << ", " << (*samples)[i].y;
			} else if (seam > 0) {
			    std::cerr << " Seam " << (*samples)[i].x << ", " << (*samples)[i].y;
			} else if (singularity >= 0) {
			    std::cerr << " Sing " << (*samples)[i].x << ", " << (*samples)[i].y;
			} else {
			    std::cerr << "      " << (*samples)[i].x << ", " << (*samples)[i].y;
			}
			ON_3dPoint p = surf->PointAt((*samples)[i].x, (*samples)[i].y);
			std::cerr << "  (" << p.x << ", " << p.y << ", " << p.z << ") " << std::endl;
		    }
		}
		si++;
	    }
	    cs++;
	}
    }
    /////
}
#endif


/* At a collapsed surface boundary the parameter in every closed transverse
 * direction is geometrically arbitrary.  Preserve the neighboring branch so
 * seam resolution can propagate through the pole instead of discarding its
 * only anchor.  The caller still validates the completed lift against the
 * exact edge. */
static bool
resolve_singular_closed_branch(const ON_Surface *surface,
    ON_2dPoint &sample, const ON_2dPoint *neighbor)
{
    if (!surface || !neighbor) return false;
    bool resolved = false;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction)) continue;
	sample[direction] = (*neighbor)[direction];
	resolved = true;
    }
    return resolved;
}


bool
resolve_seam_segment_from_prev(const ON_Surface *surface, ON_2dPointArray &segment, ON_2dPoint *prev = NULL)
{
    bool complete = prev != NULL;
    double umin, umax, umid;
    double vmin, vmax, vmid;

    surface->GetDomain(0, &umin, &umax);
    surface->GetDomain(1, &vmin, &vmax);
    umid = (umin + umax) / 2.0;
    vmid = (vmin + vmax) / 2.0;

    for (int i = 0; i < segment.Count(); i++) {
	if (brlcad::PullbackWorkCancelled()) return false;
	int singularity = IsAtSingularity(surface, segment[i], PBC_SEAM_TOL);
	int seam = IsAtSeam(surface, segment[i], PBC_SEAM_TOL);
	if ((seam > 0)) {
	    if (prev != NULL) {
		//std::cerr << " at seam " << seam << " but has prev" << std::endl;
		//std::cerr << "    prev: " << prev->x << ", " << prev->y << std::endl;
		//std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
		switch (seam) {
		    case 1: //east/west
			if (prev->x < umid) {
			    segment[i].x = umin;
			} else {
			    segment[i].x = umax;
			}
			break;
		    case 2: //north/south
			if (prev->y < vmid) {
			    segment[i].y = vmin;
			} else {
			    segment[i].y = vmax;
			}
			break;
		    case 3: //both
			if (prev->x < umid) {
			    segment[i].x = umin;
			} else {
			    segment[i].x = umax;
			}
			if (prev->y < vmid) {
			    segment[i].y = vmin;
			} else {
			    segment[i].y = vmax;
			}
		}
		prev = &segment[i];
	    } else {
		//std::cerr << " at seam and no prev" << std::endl;
		complete = false;
	    }
	} else {
	    if (singularity < 0) {
		prev = &segment[i];
	    } else if (resolve_singular_closed_branch(surface, segment[i], prev)) {
		prev = &segment[i];
	    } else {
		prev = NULL;
	    }
	}
    }
    return complete;
}


bool
resolve_seam_segment_from_next(const ON_Surface *surface, ON_2dPointArray &segment, ON_2dPoint *next = NULL)
{
    bool complete = false;
    double umin, umax, umid;
    double vmin, vmax, vmid;

    surface->GetDomain(0, &umin, &umax);
    surface->GetDomain(1, &vmin, &vmax);
    umid = (umin + umax) / 2.0;
    vmid = (vmin + vmax) / 2.0;

    if (next != NULL) {
	complete = true;
	for (int i = segment.Count() - 1; i >= 0; i--) {
	    if (brlcad::PullbackWorkCancelled()) return false;
	    int singularity = IsAtSingularity(surface, segment[i], PBC_SEAM_TOL);
	    int seam = IsAtSeam(surface, segment[i], PBC_SEAM_TOL);

	    if ((seam > 0)) {
		if (next != NULL) {
		    switch (seam) {
			case 1: //east/west
			    if (next->x < umid) {
				segment[i].x = umin;
			    } else {
				segment[i].x = umax;
			    }
			    break;
			case 2: //north/south
			    if (next->y < vmid) {
				segment[i].y = vmin;
			    } else {
				segment[i].y = vmax;
			    }
			    break;
			case 3: //both
			    if (next->x < umid) {
				segment[i].x = umin;
			    } else {
				segment[i].x = umax;
			    }
			    if (next->y < vmid) {
				segment[i].y = vmin;
			    } else {
				segment[i].y = vmax;
			    }
		    }
		    next = &segment[i];
		} else {
		    //std::cerr << " at seam and no prev" << std::endl;
		    complete = false;
		}
	    } else {
		if (singularity < 0) {
		    next = &segment[i];
		} else if (resolve_singular_closed_branch(surface, segment[i], next)) {
		    next = &segment[i];
		} else {
		    next = NULL;
		}
	    }
	}
    }
    return complete;
}


bool
resolve_seam_segment(const ON_Surface *surface, ON_2dPointArray &segment, bool &UNUSED(u_resolved), bool &UNUSED(v_resolved))
{
    ON_2dPoint *prev = NULL;
    bool complete = false;
    double umin, umax, umid;
    double vmin, vmax, vmid;

    surface->GetDomain(0, &umin, &umax);
    surface->GetDomain(1, &vmin, &vmax);
    umid = (umin + umax) / 2.0;
    vmid = (vmin + vmax) / 2.0;

    for (int i = 0; i < segment.Count(); i++) {
	if (brlcad::PullbackWorkCancelled()) return false;
	int singularity = IsAtSingularity(surface, segment[i], PBC_SEAM_TOL);
	int seam = IsAtSeam(surface, segment[i], PBC_SEAM_TOL);

	if ((seam > 0)) {
	    if (prev != NULL) {
		//std::cerr << " at seam " << seam << " but has prev" << std::endl;
		//std::cerr << "    prev: " << prev->x << ", " << prev->y << std::endl;
		//std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
		switch (seam) {
		    case 1: //east/west
			if (prev->x < umid) {
			    segment[i].x = umin;
			} else {
			    segment[i].x = umax;
			}
			break;
		    case 2: //north/south
			if (prev->y < vmid) {
			    segment[i].y = vmin;
			} else {
			    segment[i].y = vmax;
			}
			break;
		    case 3: //both
			if (prev->x < umid) {
			    segment[i].x = umin;
			} else {
			    segment[i].x = umax;
			}
			if (prev->y < vmid) {
			    segment[i].y = vmin;
			} else {
			    segment[i].y = vmax;
			}
		}
		prev = &segment[i];
	    } else {
		//std::cerr << " at seam and no prev" << std::endl;
		complete = false;
	    }
	} else {
	    if (singularity < 0) {
		prev = &segment[i];
	    } else if (resolve_singular_closed_branch(surface, segment[i], prev)) {
		prev = &segment[i];
	    } else {
		prev = NULL;
	    }
	}
    }
    if ((!complete) && (prev != NULL)) {
	complete = true;
	for (int i = segment.Count() - 2; i >= 0; i--) {
	    if (brlcad::PullbackWorkCancelled()) return false;
	    int singularity = IsAtSingularity(surface, segment[i], PBC_SEAM_TOL);
	    int seam = IsAtSeam(surface, segment[i], PBC_SEAM_TOL);
	    if ((seam > 0)) {
		if (prev != NULL) {
		    //std::cerr << " at seam " << seam << " but has prev" << std::endl;
		    //std::cerr << "    prev: " << prev->x << ", " << prev->y << std::endl;
		    //std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
		    switch (seam) {
			case 1: //east/west
			    if (prev->x < umid) {
				segment[i].x = umin;
			    } else {
				segment[i].x = umax;
			    }
			    break;
			case 2: //north/south
			    if (prev->y < vmid) {
				segment[i].y = vmin;
			    } else {
				segment[i].y = vmax;
			    }
			    break;
			case 3: //both
			    if (prev->x < umid) {
				segment[i].x = umin;
			    } else {
				segment[i].x = umax;
			    }
			    if (prev->y < vmid) {
				segment[i].y = vmin;
			    } else {
				segment[i].y = vmax;
			    }
		    }
		    prev = &segment[i];
		} else {
		    //std::cerr << " at seam and no prev" << std::endl;
		    complete = false;
		}
	    } else {
		if (singularity < 0) {
		    prev = &segment[i];
		} else if (resolve_singular_closed_branch(surface, segment[i], prev)) {
		    prev = &segment[i];
		} else {
		    prev = NULL;
		}
	    }
	}
    }
    return complete;
}


/*
 * number_of_seam_crossings
 */
int
number_of_seam_crossings(std::list<PBCData*> &pbcs)
{
    int rc = 0;
    std::list<PBCData*>::iterator cs;

    cs = pbcs.begin();
    while (cs != pbcs.end()) {
	PBCData *data = (*cs);
	if (!data || !data->surf) {
	    ++cs;
	    continue;
	}
	if (brlcad::PullbackWorkCancelled())
	    return rc;

	const ON_Surface *surf = data->surf;
	std::list<ON_2dPointArray *>::iterator si = data->segments->begin();
	ON_2dPoint *pt = NULL;
	ON_2dPoint *prev_pt = NULL;
	ON_2dPoint *next_pt = NULL;
	while (si != data->segments->end()) {
	    ON_2dPointArray *samples = (*si);
	    for (int i = 0; i < samples->Count(); i++) {
		pt = &(*samples)[i];
		if (!IsAtSeam(surf, *pt, PBC_SEAM_TOL)) {
		    if (prev_pt == NULL) {
			prev_pt = pt;
		    } else {
			next_pt = pt;
		    }
		    int udir=0;
		    int vdir=0;
		    if (next_pt != NULL) {
			if (ConsecutivePointsCrossClosedSeam(surf, *prev_pt, *next_pt, udir, vdir, PBC_SEAM_TOL)) {
			    rc++;
			}
			prev_pt = next_pt;
			next_pt = NULL;
		    }
		}
	    }
	    if (si != data->segments->end())
		si++;
	}

	if (cs != pbcs.end())
	    cs++;
    }

    return rc;
}


/*
 * if current and previous point on seam make sure they are on same seam
 */
bool
check_for_points_on_same_seam(std::list<PBCData*> &pbcs)
{

    std::list<PBCData*>::iterator cs = pbcs.begin();
    ON_2dPoint *prev_pt = NULL;
    int prev_seam = 0;
    while (cs != pbcs.end()) {
	PBCData *data = (*cs);
	const ON_Surface *surf = data->surf;
	std::list<ON_2dPointArray *>::iterator seg = data->segments->begin();
	while (seg != data->segments->end()) {
	    ON_2dPointArray *points = (*seg);
	    for (int i=0; i < points->Count(); i++) {
		ON_2dPoint *pt = points->At(i);
		int seam = IsAtSeam(surf, *pt, PBC_SEAM_TOL);
		if (seam > 0) {
		    if (prev_seam > 0) {
			if ((seam == 1) && ((prev_seam % 2) == 1)) {
			    pt->x = prev_pt->x;
			} else if ((seam == 2) && (prev_seam > 1)) {
			    pt->y = prev_pt->y;
			} else if (seam == 3) {
			    if ((prev_seam % 2) == 1) {
				pt->x = prev_pt->x;
			    }
			    if (prev_seam > 1) {
				pt->y = prev_pt->y;
			    }
			}
		    }
		    prev_seam = seam;
		    prev_pt = pt;
		}
	    }
	    seg++;
	}
	cs++;
    }
    return true;
}


/*
 * extend_pullback_at_shared_3D_curve_seam
 */
bool
extend_pullback_at_shared_3D_curve_seam(std::list<PBCData*> &pbcs)
{
    const ON_Curve *next_curve = NULL;
    std::list<PBCData*>::iterator cs = pbcs.begin();

    while (cs != pbcs.end()) {
	PBCData *data = (*cs++);
	const ON_Curve *curve = data->curve;
	const ON_Surface *surf = data->surf;

	if (cs != pbcs.end()) {
	    PBCData *nextdata = (*cs);
	    next_curve = nextdata->curve;
	}

	if (curve == next_curve) {
	    //find which direction we need to extend
	    if (surf->IsClosed(0) && !surf->IsClosed(1)) {
		double length = surf->Domain(0).Length();
		std::list<ON_2dPointArray *>::iterator seg = data->segments->begin();
		while (seg != data->segments->end()) {
		    ON_2dPointArray *points = (*seg);
		    for (int i=0; i < points->Count(); i++) {
			points->At(i)->x = points->At(i)->x + length;
		    }
		    seg++;
		}
	    } else if (!surf->IsClosed(0) && surf->IsClosed(1)) {
		double length = surf->Domain(1).Length();
		std::list<ON_2dPointArray *>::iterator seg = data->segments->begin();
		while (seg != data->segments->end()) {
		    ON_2dPointArray *points = (*seg);
		    for (int i=0; i < points->Count(); i++) {
			points->At(i)->y = points->At(i)->y + length;
		    }
		    seg++;
		}
	    } else {
		/* A surface closed in both directions is left unchanged here; the
		 * bounded seam resolver selects its periodic image later. */
	    }
	}
	next_curve = NULL;
    }
    return true;
}


/*
 * shift_closed_curve_split_over_seam
 */
bool
shift_single_curve_loop_straddled_over_seam(std::list<PBCData*> &pbcs)
{
    if (pbcs.size() == 1) { // single curve for this loop

	PBCData *data = pbcs.front();
	if (!data || !data->surf)
	    return false;

	const ON_Surface *surf = data->surf;
	std::list<ON_2dPointArray *>::iterator si = data->segments->begin();
	ON_2dPoint pt;
	ON_2dPoint prev_pt;
	if (data->curve->IsClosed()) {
	    int numseamcrossings = number_of_seam_crossings(pbcs);
	    if (numseamcrossings == 1) {
		ON_2dPointArray part1, part2;
		ON_2dPointArray* curr_point_array = &part2;
		while (si != data->segments->end()) {
		    ON_2dPointArray *samples = (*si);
		    for (int i = 0; i < samples->Count(); i++) {
			pt = (*samples)[i];
			if (i == 0) {
			    prev_pt = pt;
			    curr_point_array->Append(pt);
			    continue;
			}
			int udir= 0;
			int vdir= 0;
			if (ConsecutivePointsCrossClosedSeam(surf, pt, prev_pt, udir, vdir, PBC_SEAM_TOL)) {
			    if (surf->IsAtSeam(pt.x, pt.y) > 0) {
				SwapUVSeamPoint(surf, pt);
				curr_point_array->Append(pt);
				curr_point_array = &part1;
				SwapUVSeamPoint(surf, pt);
			    } else if (surf->IsAtSeam(prev_pt.x, prev_pt.y) > 0) {
				SwapUVSeamPoint(surf, prev_pt);
				curr_point_array->Append(prev_pt);
			    } else {
				std::cerr << "shift_single_curve_loop_straddled_over_seam(): Error expecting to see seam in sample points" << std::endl;
			    }
			}
			curr_point_array->Append(pt);
			prev_pt = pt;
		    }
		    samples->Empty();
		    samples->Append(part1.Count(), part1.Array());
		    samples->Append(part2.Count(), part2.Array());
		    if (si != data->segments->end())
			si++;
		}
	    }
	}
    }
    return true;
}


/*
 * extend_over_seam_crossings - all incoming points assumed to be within UV bounds
 */
bool
extend_over_seam_crossings(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator cs;
    ON_2dPoint *pt = NULL;
    ON_2dPoint *prev_pt = NULL;

    ///// Loop through and fix any seam ambiguities
    cs = pbcs.begin();
    while (cs != pbcs.end()) {
	PBCData *data = (*cs);
	if (!data || !data->surf) {
	    ++cs;
	    continue;
	}
	if (brlcad::PullbackWorkCancelled())
	    return false;

	std::list<ON_2dPointArray *>::iterator si = data->segments->begin();
	while (si != data->segments->end()) {
	    ON_2dPointArray *samples = (*si);
	    for (int i = 0; i < samples->Count(); i++) {
		pt = &(*samples)[i];

		if (prev_pt != NULL) {
		    GetClosestExtendedPoint(data->surf, *pt, *prev_pt, PBC_SEAM_TOL);
		}
		prev_pt = pt;
	    }
	    if (si != data->segments->end())
		si++;
	}
	if (cs != pbcs.end())
	    cs++;
    }

    return true;
}


/*
 * run through curve loop to determine correct start/end
 * points resolving ambiguities when point lies on a seam or
 * singularity
 */
bool
resolve_pullback_seams(std::list<PBCData*> &pbcs)
{
    struct SeamSegment {
	PBCData *data;
	ON_2dPointArray *samples;
    };
    std::vector<SeamSegment> segments;
    pullback_seam_failure.clear();
    bool u_resolved = false;
    bool v_resolved = false;

    /* Flatten the circular loop first.  The historical nested iterator walk
     * did not advance if one segment consisted entirely of ambiguous seam
     * points, which made a legal cylinder boundary spin forever.  A segment
     * with an unambiguous point anchors the periodic branch; neighboring
     * segments are then resolved from their exact predecessor/successor.
     * This changes only the UV image, not the sampled 3-D curve or tolerance. */
    for (std::list<PBCData *>::iterator data_it = pbcs.begin();
	 data_it != pbcs.end(); ++data_it) {
	if (brlcad::PullbackWorkCancelled()) {
	    pullback_seam_failure = "seam resolution was cancelled";
	    return false;
	}
	PBCData *data = *data_it;
	if (!data || !data->surf || !data->segments) {
	    pullback_seam_failure = "seam resolution received incomplete pullback data";
	    return false;
	}
	for (std::list<ON_2dPointArray *>::iterator segment_it =
		data->segments->begin(); segment_it != data->segments->end();
		++segment_it) {
	    if (!*segment_it) {
		pullback_seam_failure = "seam resolution received a null UV segment";
		return false;
	    }
	    /* An empty seam-split fragment carries no points to disambiguate.
	     * Leave it in its PBCData: the caller's bounded absent-pcurve repair
	     * uses the adjacent exact endpoints to regenerate that trim. */
	    if ((*segment_it)->Count() == 0)
		continue;
	    SeamSegment segment = {data, *segment_it};
	    segments.push_back(segment);
	}
    }
    if (segments.empty()) {
	pullback_seam_failure = "seam resolution received no UV segments";
	return false;
    }

    size_t anchor = segments.size();
    for (size_t i = 0; i < segments.size(); ++i) {
	if (brlcad::PullbackWorkCancelled()) {
	    pullback_seam_failure = "seam resolution was cancelled while finding an anchor";
	    return false;
	}
	if (resolve_seam_segment(segments[i].data->surf, *segments[i].samples,
		u_resolved, v_resolved)) {
	    anchor = i;
	    break;
	}
    }
    if (anchor == segments.size()) {
	/* A periodic boundary may lie wholly on a seam (notably an
	 * inner loop on a torus), leaving no off-seam point from which to choose
	 * a periodic image.  Seed the choice from the first supplied UV point and
	 * propagate that side around the loop.  Domain endpoints on a periodic
	 * surface have identical 3-D lift, so this is an exact reparameterization.
	 * Singular endpoints do not make this ambiguous: every parameter value on
	 * a collapsed boundary lifts to the same 3-D pole, while the noncollapsed
	 * seam samples retain the selected side.  The caller's dense edge-lift and
	 * loop validation still has to prove the resulting pcurve before output. */
	if (brlcad::PullbackWorkCancelled()) {
	    pullback_seam_failure = "seam resolution was cancelled before periodic anchoring";
	    return false;
	}
	ON_2dPoint *seed = segments.front().samples->First();
	if (!seed || !resolve_seam_segment_from_prev(
		segments.front().data->surf, *segments.front().samples, seed)) {
	    pullback_seam_failure = "the supplied periodic seam seed did not resolve its UV segment";
	    return false;
	}
	anchor = 0;
    }

    ON_2dPoint *next = segments[anchor].samples->First();
    for (size_t i = anchor; i > 0; --i) {
	if (brlcad::PullbackWorkCancelled()) {
	    pullback_seam_failure = "seam resolution was cancelled during reverse propagation";
	    return false;
	}
	SeamSegment &segment = segments[i - 1];
	if (!resolve_seam_segment(segment.data->surf, *segment.samples,
		u_resolved, v_resolved) &&
		!resolve_seam_segment_from_next(segment.data->surf,
		    *segment.samples, next)) {
	    pullback_seam_failure = "a UV seam segment could not be resolved from its successor";
	    return false;
	}
	next = segment.samples->First();
    }

    ON_2dPoint *prev = &(*segments[anchor].samples)[
	segments[anchor].samples->Count() - 1];
    for (size_t offset = 1; offset < segments.size(); ++offset) {
	if (brlcad::PullbackWorkCancelled()) {
	    pullback_seam_failure = "seam resolution was cancelled during forward propagation";
	    return false;
	}
	SeamSegment &segment = segments[(anchor + offset) % segments.size()];
	if (!resolve_seam_segment(segment.data->surf, *segment.samples,
		u_resolved, v_resolved) &&
		!resolve_seam_segment_from_prev(segment.data->surf,
		    *segment.samples, prev)) {
	    pullback_seam_failure = "a UV seam segment could not be resolved from its predecessor";
	    return false;
	}
	prev = &(*segment.samples)[segment.samples->Count() - 1];
    }
    return true;
}


/*
 * run through curve loop to determine correct start/end
 * points resolving ambiguities when point lies on a seam or
 * singularity
 */
bool
resolve_pullback_singularities(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator cs = pbcs.begin();

    ///// Loop through and fix any seam ambiguities
    ON_2dPoint *prev = NULL;
    bool complete = false;
    int checkcnt = 0;

    prev = NULL;
    complete = false;
    checkcnt = 0;
    while (!complete && (checkcnt < 2)) {
	cs = pbcs.begin();
	complete = true;
	checkcnt++;
	//std::cerr << "Checkcnt - " << checkcnt << std::endl;
	while (cs != pbcs.end()) {
	    if (brlcad::PullbackWorkCancelled())
		return false;
	    int singularity;
	    prev = NULL;
	    PBCData *data = (*cs);
	    if (!data || !data->surf) {
		++cs;
		continue;
	    }

	    const ON_Surface *surf = data->surf;
	    std::list<ON_2dPointArray *>::iterator si = data->segments->begin();
	    while (si != data->segments->end()) {
		ON_2dPointArray *samples = (*si);
		for (int i = 0; i < samples->Count(); i++) {
		    // 0 = south, 1 = east, 2 = north, 3 = west
		    if ((singularity = IsAtSingularity(surf, (*samples)[i], PBC_SEAM_TOL)) >= 0) {
			if (prev != NULL) {
			    //std::cerr << " at singularity " << singularity << " but has prev" << std::endl;
			    //std::cerr << "    prev: " << prev->x << ", " << prev->y << std::endl;
			    //std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
			    switch (singularity) {
				case 0: //south
				    (*samples)[i].x = prev->x;
				    (*samples)[i].y = surf->Domain(1)[0];
				    break;
				case 1: //east
				    (*samples)[i].y = prev->y;
				    (*samples)[i].x = surf->Domain(0)[1];
				    break;
				case 2: //north
				    (*samples)[i].x = prev->x;
				    (*samples)[i].y = surf->Domain(1)[1];
				    break;
				case 3: //west
				    (*samples)[i].y = prev->y;
				    (*samples)[i].x = surf->Domain(0)[0];
			    }
			    prev = NULL;
			    //std::cerr << "    curr now: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
			} else {
			    //std::cerr << " at singularity " << singularity << " and no prev" << std::endl;
			    //std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
			    complete = false;
			}
		    } else {
			prev = &(*samples)[i];
		    }
		}
		if (!complete) {
		    //std::cerr << "Lets work backward:" << std::endl;
		    for (int i = samples->Count() - 2; i >= 0; i--) {
			// 0 = south, 1 = east, 2 = north, 3 = west
			if ((singularity = IsAtSingularity(surf, (*samples)[i], PBC_SEAM_TOL)) >= 0) {
			    if (prev != NULL) {
				//std::cerr << " at singularity " << singularity << " but has prev" << std::endl;
				//std::cerr << "    prev: " << prev->x << ", " << prev->y << std::endl;
				//std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
				switch (singularity) {
				    case 0: //south
					(*samples)[i].x = prev->x;
					(*samples)[i].y = surf->Domain(1)[0];
					break;
				    case 1: //east
					(*samples)[i].y = prev->y;
					(*samples)[i].x = surf->Domain(0)[1];
					break;
				    case 2: //north
					(*samples)[i].x = prev->x;
					(*samples)[i].y = surf->Domain(1)[1];
					break;
				    case 3: //west
					(*samples)[i].y = prev->y;
					(*samples)[i].x = surf->Domain(0)[0];
				}
				prev = NULL;
				//std::cerr << "    curr now: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
			    } else {
				//std::cerr << " at singularity " << singularity << " and no prev" << std::endl;
				//std::cerr << "    curr: " << data->samples[i].x << ", " << data->samples[i].y << std::endl;
				complete = false;
			    }
			} else {
			    prev = &(*samples)[i];
			}
		    }
		}
		si++;
	    }
	    cs++;
	}
    }

    return true;
}


static bool
recover_collapsed_periodic_pullback(PBCData *data)
{
    if (!data || !data->surf || !data->curve || !data->segments ||
	(!data->surf->IsClosed(0) && !data->surf->IsClosed(1)) ||
	!(data->tolerance > 0.0))
	return false;

    ON_2dPoint seed = ON_2dPoint::UnsetPoint;
    size_t sample_count = 0;
    for (std::list<ON_2dPointArray *>::const_iterator segment =
	    data->segments->begin(); segment != data->segments->end(); ++segment) {
	if (!*segment) return false;
	for (int point = 0; point < (*segment)->Count(); ++point) {
	    seed = (**segment)[point];
	    ++sample_count;
	}
    }
    if (sample_count != 1 || !seed.IsValid())
	return false;

    const double recovery_tolerance = std::max(data->tolerance,
	data->maximum_recovery_tolerance);
    if (!(recovery_tolerance > 0.0))
	return false;

    const ON_Interval curve_domain = data->curve->Domain();
    const ON_3dPoint seed_lift = data->surf->PointAt(seed.x, seed.y);
    if (!curve_domain.IsIncreasing() || !seed_lift.IsValid())
	return false;

    size_t anchor = 0;
    double anchor_distance = DBL_MAX;
    for (size_t sample = 0; sample <= kPeriodicRecoverySegments; ++sample) {
	if (brlcad::PullbackWorkCancelled()) return false;
	const double fraction = static_cast<double>(sample) /
	    kPeriodicRecoverySegments;
	const double distance = data->curve->PointAt(
	    curve_domain.ParameterAt(fraction)).DistanceTo(seed_lift);
	if (distance < anchor_distance) {
	    anchor = sample;
	    anchor_distance = distance;
	}
    }

    std::vector<ON_2dPoint> recovered(kPeriodicRecoverySegments + 1,
	ON_2dPoint::UnsetPoint);
    recovered[anchor] = seed;
	double failed_distance = DBL_MAX;
    const auto recover = [data, &recovered, &curve_domain,
	recovery_tolerance, &failed_distance](size_t sample,
	    const ON_2dPoint &previous) {
	const double fraction = static_cast<double>(sample) /
	    kPeriodicRecoverySegments;
	const ON_3dPoint target = data->curve->PointAt(
	    curve_domain.ParameterAt(fraction));
	ON_3dPoint lift = ON_3dPoint::UnsetPoint;
	double distance = DBL_MAX;
	bool projected = pullback_closest_point_seeded(*data, target, previous,
	    recovered[sample], lift, distance, recovery_tolerance);
	if (!projected)
	    projected = pullback_closest_point(*data, target, recovered[sample],
		lift, distance, 0, recovery_tolerance, recovery_tolerance);
	failed_distance = distance;
	if (projected && distance <= recovery_tolerance)
	    data->maximum_recovery_distance = std::max(
		data->maximum_recovery_distance, distance);
	return projected && distance <= recovery_tolerance;
    };
    for (size_t sample = anchor + 1; sample <= kPeriodicRecoverySegments;
	 ++sample) {
	if (!recover(sample, recovered[sample - 1])) {
	    bu_log("Collapsed periodic pullback forward recovery failed at %zu/%zu "
		"(closest distance %.17g, tolerance %.17g, anchor %zu, anchor distance %.17g)\n",
		sample, kPeriodicRecoverySegments, failed_distance, recovery_tolerance,
		anchor, anchor_distance);
	    return false;
	}
    }
    for (size_t sample = anchor; sample > 0; --sample) {
	if (!recover(sample - 1, recovered[sample])) {
	    bu_log("Collapsed periodic pullback reverse recovery failed at %zu/%zu "
		"(closest distance %.17g, tolerance %.17g, anchor %zu, anchor distance %.17g)\n",
		sample - 1, kPeriodicRecoverySegments, failed_distance, recovery_tolerance,
		anchor, anchor_distance);
	    return false;
	}
    }

    for (size_t sample = 1; sample < recovered.size(); ++sample) {
	for (int direction = 0; direction < 2; ++direction) {
	    if (!data->surf->IsClosed(direction)) continue;
	    const double period = data->surf->Domain(direction).Length();
	    if (period > ON_ZERO_TOLERANCE)
		recovered[sample][direction] += round((recovered[sample - 1][direction] -
		    recovered[sample][direction]) / period) * period;
	}
    }

    while (!data->segments->empty()) {
	delete data->segments->front();
	data->segments->pop_front();
    }
    ON_2dPointArray *samples = new ON_2dPointArray();
    samples->Reserve(static_cast<int>(recovered.size()));
    for (std::vector<ON_2dPoint>::const_iterator point = recovered.begin();
	 point != recovered.end(); ++point)
	samples->Append(*point);
    data->segments->push_back(samples);
    return true;
}


void
remove_consecutive_intersegment_duplicates(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator cs = pbcs.begin();
    while (cs != pbcs.end()) {
	PBCData *data = (*cs);
	std::list<ON_2dPointArray *>::iterator si = data->segments->begin();
	while (si != data->segments->end()) {
	    ON_2dPointArray *samples = (*si);
	    if (samples->Count() == 0) {
		delete samples;
		si = data->segments->erase(si);
	    } else {
		for (int i = 0; i < samples->Count() - 1; i++) {
		    while ((i < (samples->Count() - 1)) && (*samples)[i].DistanceTo((*samples)[i + 1]) < 1e-9) {
			samples->Remove(i + 1);
		    }
		}
		si++;
	    }
	}
	if (data->segments->empty()) {
	    cs = pbcs.erase(cs);
	} else {
	    cs++;
	}
    }
}


bool
check_pullback_data(std::list<PBCData*> &pbcs)
{
    std::list<PBCData*>::iterator d = pbcs.begin();

    if (d == pbcs.end() || (*d) == NULL || (*d)->surf == NULL)
	return false;

    const ON_Surface *surf = (*d)->surf;
    bool singular = has_singularity(surf);
    bool closed = is_closed(surf);

    if (singular) {
	/* The caller owns entity-aware diagnostics.  Preserve the historical
	 * best-effort continuation here without emitting an untraceable stderr
	 * message that lacks the STEP loop and edge identifiers. */
	resolve_pullback_singularities(pbcs);
    }

    if (closed) {
	// check for same 3D curve use
	if (!check_for_points_on_same_seam(pbcs)) {
	    return false;
	}
	// check for same 3D curve use
	if (!extend_pullback_at_shared_3D_curve_seam(pbcs)) {
	    return false;
	}
	if (!shift_single_curve_loop_straddled_over_seam(pbcs)) {
	    return false;
	}
	if (!resolve_pullback_seams(pbcs)) {
	    return false;
	}
	if (!extend_over_seam_crossings(pbcs)) {
	    return false;
	}
    }

    // consecutive duplicates within segment will cause problems in curve fit
    remove_consecutive_intersegment_duplicates(pbcs);
	for (std::list<PBCData *>::iterator data = pbcs.begin();
	     data != pbcs.end(); ++data) {
	    if (brlcad::PullbackWorkCancelled()) return false;
	    recover_collapsed_periodic_pullback(*data);
	}

    return true;
}


int
check_pullback_singularity_bridge(const ON_Surface *surf, const ON_2dPoint &p1, const ON_2dPoint &p2)
{
    if (has_singularity(surf)) {
	int is, js;
	if (((is = IsAtSingularity(surf, p1, PBC_SEAM_TOL)) >= 0) && ((js = IsAtSingularity(surf, p2, PBC_SEAM_TOL)) >= 0)) {
	    //create new singular trim
	    if (is == js) {
		return is;
	    }
	}
    }
    return -1;
}


int
check_pullback_seam_bridge(const ON_Surface *surf, const ON_2dPoint &p1, const ON_2dPoint &p2)
{
    if (is_closed(surf)) {
	int is, js;
	if (((is = IsAtSeam(surf, p1, PBC_SEAM_TOL)) > 0) && ((js = IsAtSeam(surf, p2, PBC_SEAM_TOL)) > 0)) {
	    //create new seam trim
	    if (is == js) {
		// need to check if seam 3d points are equal
		double endpoint_distance = p1.DistanceTo(p2);
		double t0, t1;

		int dir = is - 1;
		surf->GetDomain(dir, &t0, &t1);
		if (endpoint_distance > 0.5 * (t1 - t0)) {
		    return is;
		}
	    }
	}
    }
    return -1;
}


ON_Curve*
pullback_curve(const brlcad::SurfaceTree* surfacetree,
	       const ON_Surface* surf,
	       const ON_Curve* curve,
	       double tolerance,
	       double flatness)
{
    PBCData data;
    data.tolerance = tolerance;
    data.flatness = flatness;
    data.curve = curve;
    data.surf = surf;
    data.surftree = (brlcad::SurfaceTree*)surfacetree;
    ON_2dPointArray samples;
    data.segments = new std::list<ON_2dPointArray *>();
    data.segments->push_back(&samples);

    // Step 1 - adaptively sample the curve
    double tmin, tmax;
    data.curve->GetDomain(&tmin, &tmax);
    ON_2dPoint& start = samples.AppendNew(); // new point is added to samples and returned
    if (!toUV(data, start, tmin, 0.001)) {
	return NULL;    // fails if first point is out of tolerance!
    }

    ON_2dPoint uv;
    ON_3dPoint p = curve->PointAt(tmin);
    ON_3dPoint from = curve->PointAt(tmin + 0.0001);
    brlcad::SurfaceTree *st = (brlcad::SurfaceTree *)surfacetree;
    if (st->getSurfacePoint((const ON_3dPoint&)p, uv, (const ON_3dPoint&)from) < 0) {
	std::cerr << "Error: Can not get surface point." << std::endl;
    }

    ON_2dPoint p1, p2;

#ifdef SHOW_UNUSED
    if (!data.surf)
	return NULL;

    const ON_Surface *surf = data.surf;
#endif

    if (toUV(data, p1, tmin, PBC_TOL) && toUV(data, p2, tmax, -PBC_TOL)) {
#ifdef SHOW_UNUSED
	ON_3dPoint a = surf->PointAt(p1.x, p1.y);
	ON_3dPoint b = surf->PointAt(p2.x, p2.y);
#endif

	p = curve->PointAt(tmax);
	from = curve->PointAt(tmax - 0.0001);
	if (st->getSurfacePoint((const ON_3dPoint&)p, uv, (const ON_3dPoint&)from) < 0) {
	    std::cerr << "Error: Can not get surface point." << std::endl;
	}

	if (!sample(data, tmin, tmax, p1, p2)) {
	    return NULL;
	}

	for (int i = 0; i < samples.Count(); i++) {
	    std::cerr << samples[i].x << ", " << samples[i].y << std::endl;
	}
    } else {
	return NULL;
    }

    return interpolateCurve(samples);
}


ON_Curve*
pullback_seam_curve(enum seam_direction seam_dir,
		    const brlcad::SurfaceTree* surfacetree,
		    const ON_Surface* surf,
		    const ON_Curve* curve,
		    double tolerance,
		    double flatness)
{
    PBCData data;
    data.tolerance = tolerance;
    data.flatness = flatness;
    data.curve = curve;
    data.surf = surf;
    data.surftree = (brlcad::SurfaceTree*)surfacetree;
    ON_2dPointArray samples;
    data.segments = new std::list<ON_2dPointArray *>();
    data.segments->push_back(&samples);

    // Step 1 - adaptively sample the curve
    double tmin, tmax;
    data.curve->GetDomain(&tmin, &tmax);
    ON_2dPoint& start = samples.AppendNew(); // new point is added to samples and returned
    if (!toUV(data, start, tmin, 0.001)) {
	return NULL;    // fails if first point is out of tolerance!
    }

    ON_2dPoint p1, p2;

    if (toUV(data, p1, tmin, PBC_TOL) && toUV(data, p2, tmax, -PBC_TOL)) {
	if (!sample(data, tmin, tmax, p1, p2)) {
	    return NULL;
	}

	for (int i = 0; i < samples.Count(); i++) {
	    if (seam_dir == NORTH_SEAM) {
		samples[i].y = 1.0;
	    } else if (seam_dir == EAST_SEAM) {
		samples[i].x = 1.0;
	    } else if (seam_dir == SOUTH_SEAM) {
		samples[i].y = 0.0;
	    } else if (seam_dir == WEST_SEAM) {
		samples[i].x = 0.0;
	    }
	    std::cerr << samples[i].x << ", " << samples[i].y << std::endl;
	}
    } else {
	return NULL;
    }

    return interpolateCurve(samples);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
