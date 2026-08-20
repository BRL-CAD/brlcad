/*                        C D T . C P P
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
/** @file cdt_edge.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"

#include "cdt/test_api.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <queue>
#include <numeric>
#include <iterator>
#include <memory>
#include "bg/chull.h"
#include "./chart.h"
#include "./cdt.h"

static bool
edge_has_singular_trim(const ON_BrepTrim *trim1, const ON_BrepTrim *trim2)
{
    return trim1 && trim2 &&
	(trim1->m_type == ON_BrepTrim::singular ||
	 trim2->m_type == ON_BrepTrim::singular);
}


static bool
edge_needs_curved_seed(const ON_Curve *curve, bool closed_trim)
{
    return curve && (closed_trim || !curve->IsLinear(BN_TOL_DIST));
}

#define BREP_PLANAR_TOL 0.05
#define MAX_TRIANGULATION_ATTEMPTS 5


#if 0
// If we have an associated 3D edge, we need a surface point that will
// result in a sensible triangle near that edge.  Construct and evaluate
// the proposed 3D triangle, and use that to evaluate the suitability of
// the proposed surface point 
bool
candidate_surf_pnt(struct ON_Brep_CDT_State *s_cdt, cpolyedge_t *pe)
{
    if (!pe->eseg) return false;

    // TODO - find the smallest angle between this segment and the prev/next
    // segs - that may (will?) constrain how far out into the surface we can
    // safely go.  If we're unconstrained try for equilateral, else bisect the
    // angle and intersect that vector with the vector off the midpoint.  (i.e.
    // use trig to get the length to which to scale ndir)
    //
    // If we're unconstrained, we'll still need to check the candidate point
    // against the rtree to see if we're closer to another edge elsewhere in
    // the loop than we our to pe.  if we are, shorten the distance until we
    // either make the triangle too narrow or we get an unusable triangle in
    // 3D.  if either of those happen, this edge can't supply a trim point.
    //
    // This has the potential to allow for sparser minimal splitting than the
    // box overlap test, since as long as we can produce a usable associated
    // surface point we are OK to use a trim without further splitting.
    //
    // Note the candidate must also be closer to the midpoint than either of
    // the neighboring segment points - if that distance is shorter than the
    // equilateral distance, it's our start just as an angle constraint limits
    // our start...


    // TODO - do we ever need to flip these?
    ON_3dPoint *p1 = pe->eseg->e_start;
    ON_3dPoint *p2 = pe->eseg->e_end;

    ON_BrepTrim& trim = s_cdt->brep->m_T[pe->trim_ind];
    ON_2dPoint p2d1(pe->polygon->pnts_2d[pe->v[0]].first, pe->polygon->pnts_2d[pe->v[0]].second);
    ON_2dPoint p2d2(pe->polygon->pnts_2d[pe->v[1]].first, pe->polygon->pnts_2d[pe->v[1]].second);

    ON_3dPoint p13d(p2d1.x, p2d1.y, 0);
    ON_3dPoint p23d(p2d2.x, p2d2.y, 0);
    ON_3dPoint p1norm(p2d1.x, p2d1.y, 1);
    ON_3dVector vtrim = p23d - p13d;
    ON_3dVector vnorm = p1norm - p13d;
    vtrim.Unitize();
    vnorm.Unitize();
    ON_3dVector ndir = ON_CrossProduct(vnorm, vtrim);
    ndir.Unitize();


    double dist = p2d1.DistanceTo(p2d2);
    double bdist = 0.5*dist;
    ndir = ndir * bdist;
    ON_2dVector ndir2d(ndir.x, ndir.y);
    ON_2dPoint p2mid = (p2d1 + p2d2) * 0.5;
    pe->spnt = p2mid + ndir2d;
    pe->defines_spnt = true;


}
#endif



// TODO - can we base the width of this box on how far the 3D edge curve midpoint is from
// the midpoint of the 3D line segment?  That might relate the 2D box size to how far away
// we need to be to avoid problematic surface points...
//
// Probably still want some minimum distance to avoid extremely slim triangles, even if they
// are "valid"...
static void
rtree_bbox_2d(struct ON_Brep_CDT_State *s_cdt, cpolyedge_t *pe, int tight)
{
    ON_BrepTrim& trim = s_cdt->brep->m_T[pe->trim_ind];
    ON_2dPoint p2d1(pe->polygon->pnts_2d[pe->v2d[0]].first, pe->polygon->pnts_2d[pe->v2d[0]].second);
    ON_2dPoint p2d2(pe->polygon->pnts_2d[pe->v2d[1]].first, pe->polygon->pnts_2d[pe->v2d[1]].second);
    double dist = p2d1.DistanceTo(p2d2);
    double bdist = 0.5*dist;

    // If we have an associated 3D edge, we need a surface point that will
    // result in a sensible triangle near that edge.
    if (!tight && pe->eseg) {
	ON_3dPoint p13d(p2d1.x, p2d1.y, 0);
	ON_3dPoint p23d(p2d2.x, p2d2.y, 0);
	ON_3dPoint p1norm(p2d1.x, p2d1.y, 1);
	ON_3dVector vtrim = p23d - p13d;
	ON_3dVector vnorm = p1norm - p13d;
	vtrim.Unitize();
	vnorm.Unitize();
	ON_3dVector ndir = ON_CrossProduct(vnorm, vtrim);
	ndir.Unitize();
	ndir = ndir * bdist;
	ON_2dVector ndir2d(ndir.x, ndir.y);
	ON_2dPoint p2mid = (p2d1 + p2d2) * 0.5;
	pe->spnt = p2mid + ndir2d;
	pe->defines_spnt = true;
    }

    ON_Line line(p2d1, p2d2);
    pe->bb = line.BoundingBox();
    pe->bb.m_max.x = pe->bb.m_max.x + ON_ZERO_TOLERANCE;
    pe->bb.m_max.y = pe->bb.m_max.y + ON_ZERO_TOLERANCE;
    pe->bb.m_min.x = pe->bb.m_min.x - ON_ZERO_TOLERANCE;
    pe->bb.m_min.y = pe->bb.m_min.y - ON_ZERO_TOLERANCE;

    if (!tight) {
	double xdist = pe->bb.m_max.x - pe->bb.m_min.x;
	double ydist = pe->bb.m_max.y - pe->bb.m_min.y;
	// If we're close to the edge, we want to know - the Search callback will
	// check the precise distance and make a decision on what to do.
	if (xdist < bdist) {
	    pe->bb.m_min.x = pe->bb.m_min.x - 0.5*bdist;
	    pe->bb.m_max.x = pe->bb.m_max.x + 0.5*bdist;
	}
	if (ydist < bdist) {
	    pe->bb.m_min.y = pe->bb.m_min.y - 0.5*bdist;
	    pe->bb.m_max.y = pe->bb.m_max.y + 0.5*bdist;
	}

	if (pe->eseg) {
	    pe->bb.Set(pe->spnt, true);
	}
    }

    double p1[2];
    p1[0] = pe->bb.Min().x;
    p1[1] = pe->bb.Min().y;
    double p2[2];
    p2[0] = pe->bb.Max().x;
    p2[1] = pe->bb.Max().y;
    s_cdt->face_rtrees_2d[trim.Face()->m_face_index].Insert(p1, p2, (void *)pe);
}

static void
rtree_bbox_2d_remove(struct ON_Brep_CDT_State *s_cdt, cpolyedge_t *pe)
{
    ON_BrepTrim& trim = s_cdt->brep->m_T[pe->trim_ind];
    ON_2dPoint p2d1(pe->polygon->pnts_2d[pe->v2d[0]].first, pe->polygon->pnts_2d[pe->v2d[0]].second);
    ON_2dPoint p2d2(pe->polygon->pnts_2d[pe->v2d[1]].first, pe->polygon->pnts_2d[pe->v2d[1]].second);
    ON_Line line(p2d1, p2d2);
    ON_BoundingBox bb = line.BoundingBox();
    bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
    bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
    bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
    bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;

    double dist = p2d1.DistanceTo(p2d2);
    double bdist = 0.5*dist;
    double xdist = bb.m_max.x - bb.m_min.x;
    double ydist = bb.m_max.y - bb.m_min.y;
   // Be slightly more aggressive in the size of this bbox than when adding,
   // since we want to avoid floating point weirdness when it comes to the
   // RTree Remove routine looking for this box
    if (xdist < bdist) {
	bb.m_min.x = bb.m_min.x - 0.51*bdist;
	bb.m_max.x = bb.m_max.x + 0.51*bdist;
    }
    if (ydist < bdist) {
	bb.m_min.y = bb.m_min.y - 0.51*bdist;
	bb.m_max.y = bb.m_max.y + 0.51*bdist;
    }

    double p1[2];
    p1[0] = bb.Min().x;
    p1[1] = bb.Min().y;
    double p2[2];
    p2[0] = bb.Max().x;
    p2[1] = bb.Max().y;

    s_cdt->face_rtrees_2d[trim.Face()->m_face_index].Remove(p1, p2,
	(void *)pe);
}

static bool
rebuild_face_rtree_2d(struct ON_Brep_CDT_State *s_cdt, int face_index,
	int tight)
{
    ON_BrepFace &face = s_cdt->brep->m_F[face_index];
    cdt_mesh_t *fmesh = &s_cdt->fmeshes[face_index];
    s_cdt->face_rtrees_2d[face_index].RemoveAll();

    for (int li = 0; li < face.LoopCount(); li++) {
	const ON_BrepLoop *loop = face.Loop(li);
	const bool is_outer = face.OuterLoop()->m_loop_index ==
	    loop->m_loop_index;
	cpolygon_t *cpoly = is_outer ? &fmesh->outer_loop :
	    fmesh->inner_loops[li];
	if (!cpoly || cpoly->poly.empty())
	    return false;

	size_t ecnt = 1;
	cpolyedge_t *first = *cpoly->poly.begin();
	cpolyedge_t *next = first->next;
	rtree_bbox_2d(s_cdt, first, tight);
	while (first != next) {
	    ecnt++;
	    if (!next)
		return false;
	    rtree_bbox_2d(s_cdt, next, tight);
	    next = next->next;
	    if (ecnt > cpoly->poly.size())
		return false;
	}
    }

    return true;
}

static void
rtree_bbox_3d(struct ON_Brep_CDT_State *s_cdt, cpolyedge_t *pe)
{
    if (!pe->eseg) return;
    ON_BrepTrim& trim = s_cdt->brep->m_T[pe->trim_ind];
    double tcparam = (pe->trim_start + pe->trim_end) / 2.0;
    ON_3dPoint trim_2d = trim.PointAt(tcparam);
    const ON_Surface *s = trim.SurfaceOf();
    ON_3dPoint trim_3d = s->PointAt(trim_2d.x, trim_2d.y);

    ON_3dPoint *p3d1 = pe->eseg->e_start;
    ON_3dPoint *p3d2 = pe->eseg->e_end;
    ON_Line line(*p3d1, *p3d2);

    double arc_dist = 2*trim_3d.DistanceTo(line.ClosestPointTo(trim_3d));

    ON_BoundingBox bb = line.BoundingBox();
    bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
    bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
    bb.m_max.z = bb.m_max.z + ON_ZERO_TOLERANCE;
    bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
    bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;
    bb.m_min.z = bb.m_min.z - ON_ZERO_TOLERANCE;

    double dist = p3d1->DistanceTo(*p3d2);
    double bdist = (0.5*dist > arc_dist) ? 0.5*dist : arc_dist;
    double xdist = bb.m_max.x - bb.m_min.x;
    double ydist = bb.m_max.y - bb.m_min.y;
    double zdist = bb.m_max.z - bb.m_min.z;
    // If we're close to the edge, we want to know - the Search callback will
    // check the precise distance and make a decision on what to do.
    if (xdist < bdist) {
	bb.m_min.x = bb.m_min.x - 0.5*bdist;
	bb.m_max.x = bb.m_max.x + 0.5*bdist;
    }
    if (ydist < bdist) {
	bb.m_min.y = bb.m_min.y - 0.5*bdist;
	bb.m_max.y = bb.m_max.y + 0.5*bdist;
    }
    if (zdist < bdist) {
	bb.m_min.z = bb.m_min.z - 0.5*bdist;
	bb.m_max.z = bb.m_max.z + 0.5*bdist;
    }

    double p1[3];
    p1[0] = bb.Min().x;
    p1[1] = bb.Min().y;
    p1[2] = bb.Min().z;
    double p2[3];
    p2[0] = bb.Max().x;
    p2[1] = bb.Max().y;
    p2[2] = bb.Max().z;

    s_cdt->face_rtrees_3d[trim.Face()->m_face_index].Insert(p1, p2, (void *)pe);

    // Also put a box around the start point - otherwise there are occasionally
    // 'holes' in a curved loop's bbox coverage where sampling can get very
    // close to the loop right near the edge points without getting rejected.
    p1[0] = p3d1->x - 0.5*bdist;
    p1[1] = p3d1->y - 0.5*bdist;
    p1[2] = p3d1->z - 0.5*bdist;
    p2[0] = p3d1->x + 0.5*bdist;
    p2[1] = p3d1->y + 0.5*bdist;
    p2[2] = p3d1->z + 0.5*bdist;

    s_cdt->face_rtrees_3d[trim.Face()->m_face_index].Insert(p1, p2, (void *)pe);
}

struct rtree_minsplit_context {
    struct ON_Brep_CDT_State *s_cdt;
    cpolyedge_t *cseg;
};

static bool MinSplit2dCallback(void *data, void *a_context) {
    cpolyedge_t *tseg = (cpolyedge_t *)data;
    struct rtree_minsplit_context *context= (struct rtree_minsplit_context *)a_context;


    //plot_ce_bbox(context->s_cdt, tseg, "l.p3");

    // Intersecting with oneself isn't cause for splitting
    if (tseg == context->cseg || tseg == context->cseg->prev || tseg == context->cseg->next) return true;

    // Someone needs to split - figure out if it's us
    ON_2dPoint cp2d1(context->cseg->polygon->pnts_2d[context->cseg->v2d[0]].first, context->cseg->polygon->pnts_2d[context->cseg->v2d[0]].second);
    ON_2dPoint cp2d2(context->cseg->polygon->pnts_2d[context->cseg->v2d[1]].first, context->cseg->polygon->pnts_2d[context->cseg->v2d[1]].second);
    ON_2dPoint tp2d1(tseg->polygon->pnts_2d[tseg->v2d[0]].first, tseg->polygon->pnts_2d[tseg->v2d[0]].second);
    ON_2dPoint tp2d2(tseg->polygon->pnts_2d[tseg->v2d[1]].first, tseg->polygon->pnts_2d[tseg->v2d[1]].second);
    double cdist = cp2d1.DistanceTo(cp2d2);
    double tdist = tp2d1.DistanceTo(tp2d2);

    // If tseg is longer, it should be the one to split, but we'll need
    // to re-check this trim in the next pass
    if (cdist < tdist && !NEAR_EQUAL(cdist, tdist, ON_ZERO_TOLERANCE)) {
	context->cseg->split_status = 1;
	return true;
    }

    // Mark this segment down as a segment to split
    context->cseg->split_status = 2;

    // No need to keep checking if we already know we're going to split
    return false;
}

static double
median_seg_len(std::vector<double> &lsegs)
{
    // Get the median segment length (https://stackoverflow.com/a/42791986)
    double median, e1, e2;
    std::vector<double>::iterator v1, v2;

    if (!lsegs.size()) return -DBL_MAX;
    if (lsegs.size() % 2 == 0) {
	v1 = lsegs.begin() + lsegs.size() / 2 - 1;
	v2 = lsegs.begin() + lsegs.size() / 2;
	std::nth_element(lsegs.begin(), v1, lsegs.end());
	e1 = *v1;
	std::nth_element(lsegs.begin(), v2, lsegs.end());
	e2 = *v2;
	median = (e1+e2)*0.5;
    } else {
	v2 = lsegs.begin() + lsegs.size() / 2;
	std::nth_element(lsegs.begin(), v2, lsegs.end());
	median = *v2;
    }

    return median;
}

static double
edge_median_seg_len(struct ON_Brep_CDT_State *s_cdt, int m_edge_index)
{
    std::vector<double> lsegs;
    std::set<bedge_seg_t *> &epsegs = s_cdt->e2polysegs[m_edge_index];
    std::set<bedge_seg_t *>::iterator e_it;
    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
	bedge_seg_t *b = *e_it;
	double seg_dist = b->e_start->DistanceTo(*b->e_end);
	lsegs.push_back(seg_dist);
    }
    return median_seg_len(lsegs);
}

static ON_3dVector
trim_normal(ON_BrepTrim *trim, ON_2dPoint &cp)
{
    ON_3dVector norm = ON_3dVector::UnsetVector;
    if (trim->m_type != ON_BrepTrim::singular) {
	// 3D points are globally unique, but normals are not - the same edge point may
	// have different normals from two faces at a sharp edge.  Calculate the
	// face normal for this point on this surface.
	ON_Plane fplane;
	const ON_Surface *s = trim->SurfaceOf();
	double ptol = s->BoundingBox().Diagonal().Length()*0.001;
	ptol = (ptol < BREP_PLANAR_TOL) ? ptol : BREP_PLANAR_TOL;
	if (s->IsPlanar(&fplane, ptol)) {
	    norm = fplane.Normal();
	} else {
	    ON_3dPoint tmp1;
	    surface_EvNormal(trim->SurfaceOf(), cp.x, cp.y, tmp1, norm);
	}
	if (trim->Face()->m_bRev) {
	    norm = -1 * norm;
	}
	//std::cout << "Face " << trim->Face()->m_face_index << ", Loop " << trim->Loop()->m_loop_index << " norm: " << norm.x << "," << norm.y << "," << norm.z << "\n";
    }
    return norm;
}

static double
periodic_surface_parameter(double parameter, const ON_Interval &domain)
{
    const double period = domain.Length();
    if (!(period > 0.0) || !std::isfinite(parameter))
	return parameter;
    const double magnitude = std::max(std::fabs(domain.Min()),
	std::fabs(domain.Max()));
    const double tolerance = 256.0 *
	std::numeric_limits<double>::epsilon() *
	std::max(magnitude, period);
    if (parameter >= domain.Min() - tolerance &&
	    parameter <= domain.Max() + tolerance)
	return std::max(domain.Min(), std::min(domain.Max(), parameter));
    double remainder = std::fmod(parameter - domain.Min(), period);
    if (remainder < 0.0)
	remainder += period;
    if (std::fabs(remainder) <= tolerance && parameter > domain.Min())
	return domain.Max();
    return domain.Min() + remainder;
}

static ON_2dPoint
get_trim_midpt(fastf_t *t, struct ON_Brep_CDT_State *s_cdt,
	cpolyedge_t *pe, const ON_3dPoint &edge_mid_3d, double elen,
	double brep_edge_tol)
{
    ON_BrepTrim& trim = s_cdt->brep->m_T[pe->trim_ind];
    ON_Interval domain(pe->trim_start, pe->trim_end);
    double tparam;
    bool cpoint = ON_TrimCurve_GetClosestPoint(&tparam, &trim, edge_mid_3d, 0, &domain);
    if (!cpoint) {
	tparam = (pe->trim_start + pe->trim_end) / 2.0;
    }

    const ON_Surface *surface = trim.SurfaceOf();
    auto distance_squared = [&](double parameter) {
	ON_2dPoint uv = trim.PointAt(parameter);
	if (!uv.IsValid() || !surface)
	    return std::numeric_limits<double>::infinity();
	for (int direction = 0; direction < 2; ++direction) {
	    if (surface->IsClosed(direction))
		uv[direction] = periodic_surface_parameter(uv[direction],
		    surface->Domain(direction));
	}
	const ON_3dPoint point = surface->PointAt(uv.x, uv.y);
	if (!point.IsValid())
	    return std::numeric_limits<double>::infinity();
	const double distance = point.DistanceTo(edge_mid_3d);
	return distance * distance;
    };
    double best_parameter = tparam;
    double best_distance_squared = distance_squared(tparam);
    double coordinate_scale = std::max(1.0, std::max(
	std::max(std::fabs(edge_mid_3d.x), std::fabs(edge_mid_3d.y)),
	std::fabs(edge_mid_3d.z)));
    const double numerical_tolerance = 1024.0 *
	std::numeric_limits<double>::epsilon() *
	std::max(coordinate_scale, elen);
    double correction_tolerance = std::max(numerical_tolerance,
	0.01 * std::max(elen, numerical_tolerance));
    if (std::isfinite(brep_edge_tol) && brep_edge_tol > 0.0 &&
	    !NEAR_EQUAL(brep_edge_tol, ON_UNSET_VALUE,
	    ON_ZERO_TOLERANCE))
	correction_tolerance = std::max(numerical_tolerance,
	    std::min(correction_tolerance, 0.01 * brep_edge_tol));

    /* The legacy recursive search can report a local stationary point as a
     * success even when it maps far from the requested shared edge point.
     * Only pay for a bounded global search when that postcondition fails. */
    if (!std::isfinite(best_distance_squared) ||
	    best_distance_squared > correction_tolerance *
	    correction_tolerance) {
	const int sample_count = 32;
	for (int sample = 0; sample <= sample_count; ++sample) {
	    const double fraction = (double)sample / sample_count;
	    const double parameter = domain.ParameterAt(fraction);
	    const double candidate = distance_squared(parameter);
	    if (candidate < best_distance_squared) {
		best_distance_squared = candidate;
		best_parameter = parameter;
	    }
	}
	if (std::isfinite(best_distance_squared)) {
	    const double best_fraction =
		domain.NormalizedParameterAt(best_parameter);
	    double low = std::max(0.0,
		best_fraction - 1.0 / sample_count);
	    double high = std::min(1.0,
		best_fraction + 1.0 / sample_count);
	    const double golden = 0.5 * (std::sqrt(5.0) - 1.0);
	    double left = high - golden * (high - low);
	    double right = low + golden * (high - low);
	    double left_distance = distance_squared(domain.ParameterAt(left));
	    double right_distance = distance_squared(domain.ParameterAt(right));
	    for (int iteration = 0; iteration < 48; ++iteration) {
		if (left_distance < right_distance) {
		    high = right;
		    right = left;
		    right_distance = left_distance;
		    left = high - golden * (high - low);
		    left_distance = distance_squared(
			domain.ParameterAt(left));
		} else {
		    low = left;
		    left = right;
		    left_distance = right_distance;
		    right = low + golden * (high - low);
		    right_distance = distance_squared(
			domain.ParameterAt(right));
		}
	    }
	    const double refined_fraction = 0.5 * (low + high);
	    const double refined_parameter =
		domain.ParameterAt(refined_fraction);
	    const double refined_distance =
		distance_squared(refined_parameter);
	    if (refined_distance < best_distance_squared) {
		best_distance_squared = refined_distance;
		best_parameter = refined_parameter;
	    }
	}
    }

    (*t) = best_parameter;
    return trim.PointAt(best_parameter);
}

static bool
edge_spacing_floor(double *spacing, double absmin, double local_min)
{
    if (!spacing || !std::isfinite(absmin) || absmin <= 0.0)
	return false;

    /* A zero chord is valid for a closed curved segment, but it is not a
     * useful length scale to propagate onto another edge.  The globally
     * digested absmin is the caller's smallest requested mesh dimension.  An
     * edge-local floor also bounds subdivision when a pathological B-Rep
     * bounding box makes that global scale unreliable. */
    double floor = absmin;
    if (std::isfinite(local_min) && local_min > floor)
	floor = local_min;
    if (!std::isfinite(*spacing) || *spacing < floor)
	*spacing = floor;
    return true;
}

static bool
shape_refinement_spacing(double *spacing, double absmin, double cp_len)
{
    const double local_min = std::isfinite(cp_len) && cp_len > 0.0 ?
	cp_len / 256.0 : 0.0;
    return edge_spacing_floor(spacing, absmin, local_min);
}

static bool
split_parameter_interior(double start, double end, double candidate)
{
    if (!std::isfinite(start) || !std::isfinite(end) ||
	    !std::isfinite(candidate))
	return false;
    const double lower = std::min(start, end);
    const double upper = std::max(start, end);
    return candidate > lower && candidate < upper;
}

static bool
edge_split_midpoint(double start, double end, double *midpoint)
{
    if (!midpoint || !std::isfinite(start) || !std::isfinite(end))
	return false;
    const double candidate = start + 0.5 * (end - start);
    if (!split_parameter_interior(start, end, candidate))
	return false;
    *midpoint = candidate;
    return true;
}

static bool
split_point_progress(const ON_3dPoint &start, const ON_3dPoint &midpoint,
	const ON_3dPoint &end)
{
    if (!start.IsValid() || !midpoint.IsValid() || !end.IsValid())
	return false;
    const double scale = std::max(1.0, std::max({
	std::fabs(start.x), std::fabs(start.y), std::fabs(start.z),
	std::fabs(midpoint.x), std::fabs(midpoint.y),
	std::fabs(midpoint.z), std::fabs(end.x), std::fabs(end.y),
	std::fabs(end.z)}));
    const double tolerance = 64.0 *
	std::numeric_limits<double>::epsilon() * scale;
    return midpoint.DistanceTo(start) > tolerance &&
	midpoint.DistanceTo(end) > tolerance;
}

static bool
split_point_progress(const ON_2dPoint &start, const ON_2dPoint &midpoint,
	const ON_2dPoint &end)
{
    if (!start.IsValid() || !midpoint.IsValid() || !end.IsValid())
	return false;
    const double scale = std::max(1.0, std::max({
	std::fabs(start.x), std::fabs(start.y), std::fabs(midpoint.x),
	std::fabs(midpoint.y), std::fabs(end.x), std::fabs(end.y)}));
    const double tolerance = 64.0 *
	std::numeric_limits<double>::epsilon() * scale;
    return midpoint.DistanceTo(start) > tolerance &&
	midpoint.DistanceTo(end) > tolerance;
}

static bool
curve_interior_point_parameter_impl(double *parameter,
	const ON_NurbsCurve *curve, const ON_Interval &domain,
	const ON_3dPoint &point, double tolerance, bool reject_near_endpoint)
{
    if (!parameter || !curve || !point.IsValid() ||
	    !(domain.Length() > 0.0) || !std::isfinite(tolerance) ||
	    tolerance < 0.0)
	return false;
    const ON_3dPoint start = curve->PointAt(domain.Min());
    const ON_3dPoint end = curve->PointAt(domain.Max());
    if (!start.IsValid() || !end.IsValid() ||
	    (reject_near_endpoint &&
	    (start.DistanceTo(point) <= tolerance ||
	    end.DistanceTo(point) <= tolerance)))
	return false;
    const auto distance_squared = [&](double candidate_parameter) {
	const ON_3dPoint curve_point = curve->PointAt(candidate_parameter);
	if (!curve_point.IsValid())
	    return std::numeric_limits<double>::infinity();
	const double distance = curve_point.DistanceTo(point);
	return distance * distance;
    };
    double candidate = DBL_MAX;
    double candidate_distance = std::numeric_limits<double>::infinity();
    if (ON_NurbsCurve_GetClosestPoint(&candidate, curve, point,
	    tolerance, &domain))
	candidate_distance = distance_squared(candidate);
    const double parameter_tolerance = 4096.0 *
	std::numeric_limits<double>::epsilon() * std::max(1.0,
	std::max(std::fabs(domain.Min()), std::fabs(domain.Max())));

    /* The OpenNURBS closest-point search occasionally fails on a short
     * interior interval even when a curve passes through the target.  Search
     * each NURBS span with a bounded set of seeds, then refine the best
     * bracket.  This is only used for the few shared edges incident to a
     * singular surface, so the additional evaluations are tightly scoped. */
    if (!std::isfinite(candidate_distance) ||
	    candidate_distance > tolerance * tolerance ||
	    candidate <= domain.Min() + parameter_tolerance ||
	    candidate >= domain.Max() - parameter_tolerance) {
	std::vector<double> samples;
	const int span_count = curve->SpanCount();
	if (span_count > 0 && span_count <= 4096) {
	    std::vector<double> spans((size_t)span_count + 1);
	    if (curve->GetSpanVector(spans.data())) {
		const int samples_per_span = std::max(2,
		    std::min(64, 8192 / span_count));
		for (int span = 0; span < span_count; ++span) {
		    const double low = std::max(domain.Min(), spans[span]);
		    const double high = std::min(domain.Max(),
			spans[span + 1]);
		    if (!(high > low))
			continue;
		    for (int sample = 0; sample <= samples_per_span; ++sample) {
			const double value = low + (high - low) *
			    (double)sample / samples_per_span;
			if (samples.empty() || value > samples.back())
			    samples.push_back(value);
		    }
		}
	    }
	}
	if (samples.size() < 3) {
	    samples.clear();
	    for (int sample = 0; sample <= 64; ++sample)
		samples.push_back(domain.ParameterAt((double)sample / 64.0));
	}
	size_t best_sample = 0;
	double best_distance = std::numeric_limits<double>::infinity();
	for (size_t sample = 0; sample < samples.size(); ++sample) {
	    const double distance = distance_squared(samples[sample]);
	    if (distance < best_distance) {
		best_distance = distance;
		best_sample = sample;
	    }
	}
	if (best_sample > 0 && best_sample + 1 < samples.size()) {
	    double low = samples[best_sample - 1];
	    double high = samples[best_sample + 1];
	    const double golden = 0.5 * (std::sqrt(5.0) - 1.0);
	    double left = high - golden * (high - low);
	    double right = low + golden * (high - low);
	    double left_distance = distance_squared(left);
	    double right_distance = distance_squared(right);
	    for (int iteration = 0; iteration < 64; ++iteration) {
		if (left_distance < right_distance) {
		    high = right;
		    right = left;
		    right_distance = left_distance;
		    left = high - golden * (high - low);
		    left_distance = distance_squared(left);
		} else {
		    low = left;
		    left = right;
		    left_distance = right_distance;
		    right = low + golden * (high - low);
		    right_distance = distance_squared(right);
		}
	    }
	    const double refined = 0.5 * (low + high);
	    const double refined_distance = distance_squared(refined);
	    if (refined_distance < candidate_distance) {
		candidate = refined;
		candidate_distance = refined_distance;
	    }
	}
    }
    if (candidate <= domain.Min() + parameter_tolerance ||
	    candidate >= domain.Max() - parameter_tolerance)
	return false;
    if (!std::isfinite(candidate_distance) ||
	    candidate_distance > tolerance * tolerance)
	return false;
    *parameter = candidate;
    return true;
}

static bool
curve_interior_point_parameter(double *parameter, const ON_NurbsCurve *curve,
	const ON_Interval &domain, const ON_3dPoint &point, double tolerance)
{
    return curve_interior_point_parameter_impl(parameter, curve, domain,
	point, tolerance, true);
}

static bool
close_edge_split_worthwhile(const struct ON_Brep_CDT_State *s_cdt,
	const bedge_seg_t *bseg)
{
    if (!s_cdt || !bseg || !bseg->nc || !bseg->e_start || !bseg->e_end ||
	!std::isfinite(s_cdt->absmin) || s_cdt->absmin <= 0.0)
	return true;

    double midpoint = 0.0;
    if (!edge_split_midpoint(bseg->edge_start, bseg->edge_end, &midpoint))
	return false;

    const ON_3dPoint mid = bseg->nc->PointAt(midpoint);
    if (!mid.IsValid())
	return true;
    const double span = bseg->e_start->DistanceTo(mid) +
	mid.DistanceTo(*bseg->e_end);
    double min_span = 0.0;
    if (!shape_refinement_spacing(&min_span, s_cdt->absmin, bseg->cp_len))
	return true;
    return !std::isfinite(span) || span > min_span;
}

/* Return 1 when a representable split exists, 0 when the unsplittable
 * residual is covered by the edge tolerance, and -1 when subdivision has
 * stalled with a geometrically significant residual. */
static int
edge_split_progress(double start, double end, double chord,
	double start_miss, double end_miss, double edge_tolerance,
	double *midpoint, double *residual)
{
    if (edge_split_midpoint(start, end, midpoint))
	return 1;
    if (!std::isfinite(start) || !std::isfinite(end))
	return -1;

    const double remaining = std::max(chord,
	std::max(start_miss, end_miss));
    if (residual)
	*residual = remaining;
    if (std::isfinite(remaining) && std::isfinite(edge_tolerance) &&
	edge_tolerance >= 0.0 && remaining <= edge_tolerance)
	return 0;
    return -1;
}

/* Choose one shared point only when the midpoint between the two face
 * pullbacks remains close to both pullbacks and the native edge curve. */
static bool
bounded_edge_midpoint(ON_3dPoint *midpoint, double *maximum_miss,
	const ON_3dPoint &point1, const ON_3dPoint &point2,
	const ON_3dPoint &edge_point, double tolerance)
{
    if (!midpoint || !maximum_miss || !point1.IsValid() ||
	    !point2.IsValid() || !edge_point.IsValid() ||
	    !(tolerance > 0.0) || !std::isfinite(tolerance))
	return false;

    const ON_3dPoint candidate = 0.5 * (point1 + point2);
    if (!candidate.IsValid())
	return false;
    const double miss = std::max(candidate.DistanceTo(edge_point),
	std::max(candidate.DistanceTo(point1), candidate.DistanceTo(point2)));
    if (!std::isfinite(miss) || miss > tolerance)
	return false;
    *midpoint = candidate;
    *maximum_miss = miss;
    return true;
}

int
cdt_test_bounded_edge_midpoint(void)
{
    ON_3dPoint midpoint = ON_3dPoint::UnsetPoint;
    double miss = 0.0;
    if (!bounded_edge_midpoint(&midpoint, &miss, ON_3dPoint(0.0, 0.0, 0.0),
	    ON_3dPoint(2.0, 0.0, 0.0), ON_3dPoint(1.0, 1.0, 0.0), 1.0))
	return 1;
    if (midpoint.DistanceTo(ON_3dPoint(1.0, 0.0, 0.0)) >
	    ON_ZERO_TOLERANCE || std::fabs(miss - 1.0) > ON_ZERO_TOLERANCE)
	return 2;
    if (bounded_edge_midpoint(&midpoint, &miss,
	    ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(2.0, 0.0, 0.0),
	    ON_3dPoint(1.0, 1.0 + ON_ZERO_TOLERANCE, 0.0), 1.0))
	return 3;
    if (bounded_edge_midpoint(&midpoint, &miss,
	    ON_3dPoint(0.0, 0.0, 0.0), ON_3dPoint(2.0, 0.0, 0.0),
	    ON_3dPoint(1.0, 0.0, 0.0), 0.0))
	return 4;
    if (bounded_edge_midpoint(&midpoint, &miss,
	    ON_3dPoint::UnsetPoint, ON_3dPoint(2.0, 0.0, 0.0),
	    ON_3dPoint(1.0, 0.0, 0.0), 1.0))
	return 5;
    return 0;
}

int
cdt_test_linear_edge_spacing(void)
{
    const double floor = 0.25;
    double spacing = 0.0;
    if (!edge_spacing_floor(&spacing, floor, 0.0) ||
	    std::fabs(spacing - floor) > ON_ZERO_TOLERANCE)
	return 1;

    spacing = -1.0;
    if (!edge_spacing_floor(&spacing, floor, 0.0) ||
	    std::fabs(spacing - floor) > ON_ZERO_TOLERANCE)
	return 2;

    spacing = std::numeric_limits<double>::quiet_NaN();
    if (!edge_spacing_floor(&spacing, floor, 0.0) ||
	    std::fabs(spacing - floor) > ON_ZERO_TOLERANCE)
	return 3;

    spacing = 0.5;
    if (!edge_spacing_floor(&spacing, floor, 0.0) ||
	    std::fabs(spacing - 0.5) > ON_ZERO_TOLERANCE)
	return 4;

    spacing = 0.5;
    if (!edge_spacing_floor(&spacing, floor, 0.75) ||
	    std::fabs(spacing - 0.75) > ON_ZERO_TOLERANCE)
	return 5;

    double midpoint = 0.0;
    if (!edge_split_midpoint(0.0, 10.0, &midpoint) ||
	    std::fabs(midpoint - 5.0) > ON_ZERO_TOLERANCE)
	return 6;
    const double adjacent = std::nextafter(10.0, 11.0);
    if (edge_split_midpoint(10.0, adjacent, &midpoint))
	return 7;

    double residual = 0.0;
    if (edge_split_progress(10.0, adjacent, 3.2, 0.0, 3.2, 3.4,
	    &midpoint, &residual) != 0 ||
	    std::fabs(residual - 3.2) > ON_ZERO_TOLERANCE)
	return 8;
    if (edge_split_progress(10.0, adjacent, 3.2, 0.0, 3.2, 3.0,
	    &midpoint, &residual) != -1)
	return 9;

    spacing = 0.0;
    if (!shape_refinement_spacing(&spacing, floor, 256.0) ||
	std::fabs(spacing - 1.0) > ON_ZERO_TOLERANCE)
	return 10;

    if (edge_spacing_floor(&spacing, -1.0, 0.0))
	return 11;

    if (!split_parameter_interior(0.0, 10.0, 5.0) ||
	    !split_parameter_interior(10.0, 0.0, 5.0) ||
	    split_parameter_interior(0.0, 10.0, 0.0) ||
	    split_parameter_interior(0.0, 10.0, 10.0))
	return 12;

    const ON_3dPoint start_3d(1.0, 2.0, 3.0);
    const ON_3dPoint middle_3d(2.0, 2.0, 3.0);
    const ON_3dPoint end_3d(3.0, 2.0, 3.0);
    if (!split_point_progress(start_3d, middle_3d, end_3d) ||
	    split_point_progress(start_3d, start_3d, end_3d))
	return 13;

    const ON_2dPoint start_2d(1.0, 2.0);
    const ON_2dPoint middle_2d(2.0, 2.0);
    const ON_2dPoint end_2d(3.0, 2.0);
    if (!split_point_progress(start_2d, middle_2d, end_2d) ||
	    split_point_progress(start_2d, end_2d, end_2d))
	return 14;

    ON_LineCurve line(ON_3dPoint(-1.0, 0.0, 0.0),
	ON_3dPoint(1.0, 0.0, 0.0));
    std::unique_ptr<ON_NurbsCurve> curve(line.NurbsCurve());
    if (!curve)
	return 15;
    double parameter = DBL_MAX;
    if (!curve_interior_point_parameter(&parameter, curve.get(),
	    curve->Domain(), ON_3dPoint::Origin, BN_TOL_DIST) ||
	    !split_parameter_interior(curve->Domain().Min(),
	    curve->Domain().Max(), parameter))
	return 16;
    if (curve_interior_point_parameter(&parameter, curve.get(),
	    curve->Domain(), curve->PointAt(curve->Domain().Min()),
	    BN_TOL_DIST))
	return 17;
    if (curve_interior_point_parameter(&parameter, curve.get(),
	    curve->Domain(), ON_3dPoint(0.0, 0.01, 0.0), BN_TOL_DIST))
	return 18;

    return 0;
}

static bool
tol_need_split(struct ON_Brep_CDT_State *s_cdt, bedge_seg_t *bseg, ON_3dPoint &edge_mid_3d)
{
    ON_Line line3d(*(bseg->e_start), *(bseg->e_end));
    double seg_len = line3d.Length();

    double max_allowed = (s_cdt->tol.absmax > ON_ZERO_TOLERANCE) ? s_cdt->tol.absmax : 1.1*bseg->cp_len;
    double min_allowed = (s_cdt->tol.rel > ON_ZERO_TOLERANCE) ? s_cdt->tol.rel * bseg->cp_len : 0.0;
    double max_edgept_dist_from_edge = seg_len;
    if (s_cdt->tol.abs > ON_ZERO_TOLERANCE)
	max_edgept_dist_from_edge = s_cdt->tol.abs;
    if (s_cdt->tol.rel > ON_ZERO_TOLERANCE) {
	const double relative_tolerance = s_cdt->tol.rel * bseg->cp_len;
	max_edgept_dist_from_edge = s_cdt->tol.abs > ON_ZERO_TOLERANCE ?
	    std::min(max_edgept_dist_from_edge, relative_tolerance) :
	    relative_tolerance;
    }
    ON_BrepLoop *l1 = s_cdt->brep->m_T[bseg->tseg1->trim_ind].Loop();
    ON_BrepLoop *l2 = s_cdt->brep->m_T[bseg->tseg2->trim_ind].Loop();
    const ON_Surface *s1= l1->SurfaceOf();
    const ON_Surface *s2= l2->SurfaceOf();
    double len_1 = -1;
    double len_2 = -1;
    double s_len;
    const double local_min = s_cdt->tol.rel > ON_ZERO_TOLERANCE ?
	s_cdt->tol.rel * bseg->cp_len * 0.01 : 0.0;

    switch (bseg->edge_type) {
	case 0:
	    // singularity splitting is handled in a separate step, since it isn't based
	    // on 3D information
	    return false;
	case 1:
	    // Curved edge - default assigned values are correct.
	    break;
	case 2:
	    // Linear edge on non-planar surface - use the median segment lengths
	    // from the trims from non-planar faces associated with this edge
	    len_1 = (!s1->IsPlanar(NULL, BN_TOL_DIST)) ? s_cdt->l_median_len[l1->m_loop_index] : -1;
	    len_2 = (!s2->IsPlanar(NULL, BN_TOL_DIST)) ? s_cdt->l_median_len[l2->m_loop_index] : -1;
	    if (len_1 < 0 && len_2 < 0) {
		bu_log("Error - both loops report invalid median lengths\n");
		return false;
	    }
	    s_len = (len_1 > 0) ? len_1 : len_2;
	    s_len = (len_2 > 0 && len_2 < s_len) ? len_2 : s_len;
	    if (!edge_spacing_floor(&s_len, s_cdt->absmin, local_min))
		return false;
	    max_allowed = 5*s_len;
	    min_allowed = 0.2*s_len;
	    break;
	case 3:
	    // Linear edge connected to one or more non-linear edges.  If the start or end points
	    // are the same as the root start or end points, use the median edge length of the
	    // connected edge per the vert lookup.
	    if (bseg->e_start == bseg->e_root_start || bseg->e_end == bseg->e_root_start) {
		len_1 = s_cdt->v_min_seg_len[bseg->e_root_start];
	    }
	    if (bseg->e_start == bseg->e_root_end || bseg->e_end == bseg->e_root_end) {
		len_2 = s_cdt->v_min_seg_len[bseg->e_root_end];
	    }
	    if (bseg->e_start == bseg->e_root_start || bseg->e_end == bseg->e_root_start) {
		if (len_1 < 0 && len_2 < 0) {
		    bu_log("Error - verts report invalid lengths on type 3 line segment\n");
		    return false;
		}
	    }
	    s_len = (len_1 > 0) ? len_1 : len_2;
	    s_len = (len_2 > 0 && len_2 < s_len) ? len_2 : s_len;
	    if (!edge_spacing_floor(&s_len, s_cdt->absmin, local_min))
		return false;
	    if (s_len > 0) {
		max_allowed = 2*s_len;
		min_allowed = 0.5*s_len;
	    }
	    break;
	case 4:
	    // Linear segment, no curves involved
	    break;
	default:
	    bu_log("Error - invalid edge type: %d\n", bseg->edge_type);
	    return false;
    }


    if (seg_len > max_allowed) return true;

    if (seg_len < min_allowed) return false;

    /* A straight B-Rep edge may bound a strongly curved surface.  Its curve
     * chord and tangent need no refinement, but the shared edge sequence must
     * still resolve the adjacent face normals.  Otherwise no face-interior
     * insertion can repair triangles folded over the fixed boundary chord. */
    if (bseg->edge_type <= 1) {
	double dist3d =
	    edge_mid_3d.DistanceTo(line3d.ClosestPointTo(edge_mid_3d));
	if (dist3d > max_edgept_dist_from_edge)
	    return true;
	if ((bseg->tan_start * bseg->tan_end) < s_cdt->cos_within_ang)
	    return true;
    }

    /* Even when the caller does not request a normal tolerance, constrain a
     * shared segment to at most 22.5 degrees of endpoint-normal change.  This
     * is an orientation-safety bound; retain any stricter caller limit. */
    const double orientation_cos = cos(ON_PI / 8.0);
    const double normal_cos = (s_cdt->cos_within_ang > -1.0 +
	ON_ZERO_TOLERANCE) ?
	std::max(s_cdt->cos_within_ang, orientation_cos) : orientation_cos;
    cpolyedge_t *trim_segments[2] = {bseg->tseg1, bseg->tseg2};
    for (int i = 0; i < 2; ++i) {
	ON_BrepTrim *trim =
	    &s_cdt->brep->m_T[trim_segments[i]->trim_ind];
	ON_2dPoint uv_start = trim->PointAt(trim_segments[i]->trim_start);
	ON_2dPoint uv_end = trim->PointAt(trim_segments[i]->trim_end);
	ON_3dVector normal_start = trim_normal(trim, uv_start);
	ON_3dVector normal_end = trim_normal(trim, uv_end);
	if (normal_start.Unitize() && normal_end.Unitize() &&
		(normal_start * normal_end) < normal_cos - VUNITIZE_TOL)
	    return true;
    }

    return false;
}

std::set<bedge_seg_t *>
split_edge_seg(struct ON_Brep_CDT_State *s_cdt, bedge_seg_t *bseg,
	int force, double *t, int update_rtrees, ON_3dPoint *shared_point,
	bool required_closed_split)
{
    std::set<bedge_seg_t *> nedges;

    // If we don't have associated segments, we can't do anything
    if (!bseg->tseg1 || !bseg->tseg2 || !bseg->nc) return nedges;

    ON_BrepEdge& edge = s_cdt->brep->m_E[bseg->edge_ind];
    ON_BrepTrim *trim1 = &s_cdt->brep->m_T[bseg->tseg1->trim_ind];
    ON_BrepTrim *trim2 = &s_cdt->brep->m_T[bseg->tseg2->trim_ind];

    // If we don't have associated trims, we can't do anything
    if (!trim1 || !trim2) return nedges;

    ON_BrepFace *face1 = trim1->Face();
    ON_BrepFace *face2 = trim2->Face();
    cdt_mesh_t *fmesh1 = &s_cdt->fmeshes[face1->m_face_index];
    cdt_mesh_t *fmesh2 = &s_cdt->fmeshes[face2->m_face_index];


    // Get the 3D midpoint (and tangent, if we can) from the edge curve
    ON_3dPoint edge_mid_3d = ON_3dPoint::UnsetPoint;
    ON_3dVector edge_mid_tan = ON_3dVector::UnsetVector;
    double midpoint = 0.0;
    if (t) {
	midpoint = *t;
	const double lower = std::min(bseg->edge_start, bseg->edge_end);
	const double upper = std::max(bseg->edge_start, bseg->edge_end);
	if (!std::isfinite(midpoint) || !(midpoint > lower && midpoint < upper))
	    return nedges;
    } else if (!edge_split_midpoint(bseg->edge_start, bseg->edge_end,
	    &midpoint)) {
	return nedges;
    }
    fastf_t emid = midpoint;
    bool evtangent_status = bseg->nc->EvTangent(emid, edge_mid_3d, edge_mid_tan);
    if (!evtangent_status) {
	// EvTangent call failed, get 3d point
	edge_mid_3d = bseg->nc->PointAt(emid);
	edge_mid_tan = ON_3dVector::UnsetVector;
    }
    if (!split_point_progress(*bseg->e_start, edge_mid_3d,
	    *bseg->e_end))
	return nedges;

    // Unless we're forcing a split this is the point at which we do tolerance
    // based testing to determine whether to proceed with the split or halt.
    if (!force && !tol_need_split(s_cdt, bseg, edge_mid_3d)) {
	return nedges;
    }

    // Find the 2D points
    double elen1 = (bseg->nc->PointAt(bseg->edge_start)).DistanceTo(bseg->nc->PointAt(emid));
    double elen2 = (bseg->nc->PointAt(emid)).DistanceTo(bseg->nc->PointAt(bseg->edge_end));
    double elen = (elen1 + elen2) * 0.5;
    fastf_t t1mid, t2mid;
    ON_2dPoint trim1_mid_2d, trim2_mid_2d;
    const ON_3dPoint &trim_target_3d = shared_point ? *shared_point :
	edge_mid_3d;
    trim1_mid_2d = get_trim_midpt(&t1mid, s_cdt, bseg->tseg1,
	trim_target_3d, elen, edge.m_tolerance);
    trim2_mid_2d = get_trim_midpt(&t2mid, s_cdt, bseg->tseg2,
	trim_target_3d, elen, edge.m_tolerance);

    /* A closest-point correction may land on a child trim endpoint when an
     * edge and pullback disagree.  Accepting that result creates a zero-span
     * child and repeatedly inserts the same boundary coordinate.  Require
     * parameter and geometric progress on both face representations before
     * mutating either mesh. */
    const ON_2dPoint trim1_start = trim1->PointAt(bseg->tseg1->trim_start);
    const ON_2dPoint trim1_end = trim1->PointAt(bseg->tseg1->trim_end);
    const ON_2dPoint trim2_start = trim2->PointAt(bseg->tseg2->trim_start);
    const ON_2dPoint trim2_end = trim2->PointAt(bseg->tseg2->trim_end);
    bool trim_progress = split_parameter_interior(
	    bseg->tseg1->trim_start,
	    bseg->tseg1->trim_end, t1mid) &&
	    split_parameter_interior(bseg->tseg2->trim_start,
	    bseg->tseg2->trim_end, t2mid) &&
	    split_point_progress(trim1_start, trim1_mid_2d, trim1_end) &&
	    split_point_progress(trim2_start, trim2_mid_2d, trim2_end);

    /* Some imported edges retain a corrupt 3-D edge curve even though their
     * two p-curves agree geometrically.  In that case projection of the bad
     * curve midpoint lands on p-curve endpoints and cannot subdivide either
     * face.  Use paired p-curve midpoints as the shared geometric authority
     * only when their surface evaluations agree within the declared edge
     * tolerance (never more than BN_TOL_DIST).  A repair-only retry may use
     * the average of disagreeing p-curves under the separately bounded
     * tessellation tolerance; record that source-edge approximation. */
    if (!trim_progress && !shared_point) {
	const double fallback_t1 = 0.5 * (bseg->tseg1->trim_start +
	    bseg->tseg1->trim_end);
	const double fallback_t2 = 0.5 * (bseg->tseg2->trim_start +
	    bseg->tseg2->trim_end);
	const ON_2dPoint fallback_uv1 = trim1->PointAt(fallback_t1);
	const ON_2dPoint fallback_uv2 = trim2->PointAt(fallback_t2);
	const ON_Surface *surface1 = trim1->SurfaceOf();
	const ON_Surface *surface2 = trim2->SurfaceOf();
	const ON_3dPoint fallback_point1 = surface1 && fallback_uv1.IsValid() ?
	    surface1->PointAt(fallback_uv1.x, fallback_uv1.y) :
	    ON_3dPoint::UnsetPoint;
	const ON_3dPoint fallback_point2 = surface2 && fallback_uv2.IsValid() ?
	    surface2->PointAt(fallback_uv2.x, fallback_uv2.y) :
	    ON_3dPoint::UnsetPoint;
	double agreement_tolerance = BN_TOL_DIST;
	if (std::isfinite(edge.m_tolerance) && edge.m_tolerance > 0.0 &&
		!NEAR_EQUAL(edge.m_tolerance, ON_UNSET_VALUE,
		ON_ZERO_TOLERANCE))
	    agreement_tolerance = std::min(agreement_tolerance,
		edge.m_tolerance);
	ON_3dPoint fallback_point = 0.5 *
	    (fallback_point1 + fallback_point2);
	const bool fallback_trim1_parameter = split_parameter_interior(
		bseg->tseg1->trim_start,
		bseg->tseg1->trim_end, fallback_t1) &&
	    split_point_progress(trim1_start, fallback_uv1, trim1_end);
	const bool fallback_trim2_parameter = split_parameter_interior(
		bseg->tseg2->trim_start,
		bseg->tseg2->trim_end, fallback_t2) &&
	    split_point_progress(trim2_start, fallback_uv2, trim2_end);
	const double fallback_agreement = fallback_point1.IsValid() &&
	    fallback_point2.IsValid() ?
	    fallback_point1.DistanceTo(fallback_point2) : DBL_MAX;
	const double fallback_edge_miss1 = fallback_point1.IsValid() ?
	    fallback_point1.DistanceTo(edge_mid_3d) : DBL_MAX;
	const double fallback_edge_miss2 = fallback_point2.IsValid() ?
	    fallback_point2.DistanceTo(edge_mid_3d) : DBL_MAX;
	const bool fallback_edge_progress = fallback_point.IsValid() &&
	    split_point_progress(*bseg->e_start, fallback_point,
		*bseg->e_end);
	double relaxed_tolerance =
	    s_cdt->bounded_edge_approximation_tolerance;
	if (s_cdt->absmax > 0.0 && std::isfinite(s_cdt->absmax) &&
		(relaxed_tolerance <= 0.0 ||
		s_cdt->absmax < relaxed_tolerance))
	    relaxed_tolerance = s_cdt->absmax;
	double bounded_edge_miss = DBL_MAX;
	const bool relaxed_edge_split =
	    s_cdt->allow_bounded_edge_approximation &&
	    relaxed_tolerance > 0.0 && std::isfinite(relaxed_tolerance) &&
	    bounded_edge_midpoint(&fallback_point, &bounded_edge_miss,
		fallback_point1, fallback_point2, edge_mid_3d,
		relaxed_tolerance);
	const bool closed_edge_has_authoritative_side =
	    required_closed_split && fallback_trim1_parameter &&
	    fallback_trim2_parameter && edge_mid_3d.IsValid() &&
	    (fallback_edge_miss1 <= agreement_tolerance ||
	    fallback_edge_miss2 <= agreement_tolerance);
	if (fallback_trim1_parameter && fallback_trim2_parameter &&
		(fallback_agreement <= agreement_tolerance ||
		relaxed_edge_split) &&
		fallback_edge_progress) {
	    t1mid = fallback_t1;
	    t2mid = fallback_t2;
	    trim1_mid_2d = fallback_uv1;
	    trim2_mid_2d = fallback_uv2;
	    edge_mid_3d = fallback_point;
	    edge_mid_tan = ON_3dVector::UnsetVector;
	    trim_progress = true;
	    if (relaxed_edge_split) {
		fastf_t &recorded =
		    s_cdt->approximated_edges[edge.m_edge_index];
		recorded = std::max(recorded,
		    (fastf_t)bounded_edge_miss);
	    }
	    if (relaxed_edge_split && getenv("BRLCAD_CDT_DUMP_FAILURES") &&
		    getenv("BRLCAD_CDT_DUMP_FAILURES")[0] &&
		    !BU_STR_EQUAL(getenv("BRLCAD_CDT_DUMP_FAILURES"), "0"))
		bu_log("Edge %d used a shared p-curve midpoint within "
		    "tessellation tolerance %.17g (surface miss %.17g)\n",
		    edge.m_edge_index, relaxed_tolerance,
		    bounded_edge_miss);
	} else if (closed_edge_has_authoritative_side) {
	    /* A closed edge cannot remain a one-segment loop.  If its 3-D
	     * curve agrees with at least one pullback, use that curve as the
	     * shared boundary authority and quarantine only the disagreeing
	     * face.  Repair may reconstruct that face against the exact
	     * boundary, subject to its independent deviation limits. */
	    t1mid = fallback_t1;
	    t2mid = fallback_t2;
	    trim1_mid_2d = fallback_uv1;
	    trim2_mid_2d = fallback_uv2;
	    edge_mid_tan = ON_3dVector::UnsetVector;
	    trim_progress = true;
	    const auto quarantine = [&](ON_BrepFace *face, double miss) {
		if (!(miss > agreement_tolerance) || !std::isfinite(miss))
		    return;
		const auto current = s_cdt->inconsistent_edge_faces.find(
		    face->m_face_index);
		if (current == s_cdt->inconsistent_edge_faces.end() ||
			miss > current->second.second)
		    s_cdt->inconsistent_edge_faces[face->m_face_index] =
			std::make_pair(edge.m_edge_index, (fastf_t)miss);
	    };
	    quarantine(face1, fallback_edge_miss1);
	    quarantine(face2, fallback_edge_miss2);
	    if (getenv("BRLCAD_CDT_DUMP_FAILURES") &&
		    getenv("BRLCAD_CDT_DUMP_FAILURES")[0] &&
		    !BU_STR_EQUAL(getenv("BRLCAD_CDT_DUMP_FAILURES"), "0"))
		bu_log("Closed edge %d retained its 3-D midpoint and "
		    "quarantined pullback misses %.17g/%.17g\n",
		    edge.m_edge_index, fallback_edge_miss1,
		    fallback_edge_miss2);
	} else if (getenv("BRLCAD_CDT_DUMP_FAILURES") &&
		getenv("BRLCAD_CDT_DUMP_FAILURES")[0] &&
		!BU_STR_EQUAL(getenv("BRLCAD_CDT_DUMP_FAILURES"), "0")) {
	    bu_log("Edge %d (trims %d/%d, faces %d/%d) split fallback "
		"rejected: trim progress %d/%d, surface agreement %.17g "
		"(limit %.17g), edge midpoint misses %.17g/%.17g, 3-D "
		"progress %d\n", edge.m_edge_index, trim1->m_trim_index,
		trim2->m_trim_index, face1->m_face_index, face2->m_face_index,
		fallback_trim1_parameter ? 1 : 0,
		fallback_trim2_parameter ? 1 : 0, fallback_agreement,
		agreement_tolerance,
		fallback_point1.IsValid() ?
		fallback_point1.DistanceTo(edge_mid_3d) : DBL_MAX,
		fallback_point2.IsValid() ?
		fallback_point2.DistanceTo(edge_mid_3d) : DBL_MAX,
		fallback_edge_progress ? 1 : 0);
	}
    }
    if (!trim_progress)
	return nedges;

    /* UV samples must remain on their trims so adjacent faces retain exactly
     * the same boundary subdivision.  The shared master-edge point remains
     * the watertight 3-D authority even when a face pullback differs within
     * the B-Rep edge tolerance. */
    ON_3dPoint *mid_3d = shared_point ? shared_point :
	new ON_3dPoint(edge_mid_3d);
    if (!shared_point) {
	CDT_Add3DPnt(s_cdt, mid_3d, -1, -1, -1, edge.m_edge_index,
	    0, 0);
	s_cdt->edge_pnts->insert(mid_3d);
    }

    // Update the 2D and 2D->3D info in the fmeshes
    long f1_ind2d = fmesh1->add_point(trim1_mid_2d);
    long f1_ind3d = fmesh1->add_point(mid_3d);
    if (fmesh1->p2d3d.find(f1_ind2d) != fmesh1->p2d3d.end()) {
	std::cout << fmesh1->f_id << ": 2d->3d mapping already exists for " << f1_ind2d << "\n";
    }
    fmesh1->p2d3d[f1_ind2d] = f1_ind3d;
    long f2_ind2d = fmesh2->add_point(trim2_mid_2d);
    long f2_ind3d = fmesh2->add_point(mid_3d);
    if (fmesh2->p2d3d.find(f2_ind2d) != fmesh2->p2d3d.end()) {
	std::cout << fmesh2->f_id << ": 2d->3d mapping already exists for " << f2_ind2d << "\n";
    }
    fmesh2->p2d3d[f2_ind2d] = f2_ind3d;

    // Trims get their own normals
    ON_3dVector norm1 = trim_normal(trim1, trim1_mid_2d);
    fmesh1->normals.push_back(new ON_3dPoint(norm1));
    CDT_Add3DNorm(s_cdt, fmesh1->normals[fmesh1->normals.size()-1], mid_3d, fmesh1->f_id, -1, trim1->m_trim_index, bseg->edge_ind, trim1_mid_2d.x, trim1_mid_2d.y);
    long f1_nind = fmesh1->normals.size() - 1;
    fmesh1->nmap[f1_ind3d] = f1_nind;
    ON_3dVector norm2 = trim_normal(trim2, trim2_mid_2d);
    fmesh2->normals.push_back(new ON_3dPoint(norm2));
    CDT_Add3DNorm(s_cdt, fmesh2->normals[fmesh2->normals.size()-1], mid_3d, fmesh2->f_id, -1, trim2->m_trim_index, bseg->edge_ind, trim2_mid_2d.x, trim2_mid_2d.y);
    long f2_nind = fmesh2->normals.size() - 1;
    fmesh2->nmap[f2_ind3d] = f2_nind;

    // From the existing polyedge, make the two new polyedges that will replace the old one
    bedge_seg_t *bseg1 = new bedge_seg_t(bseg);
    bseg1->edge_start = bseg->edge_start;
    bseg1->edge_end = emid;
    bseg1->e_start = bseg->e_start;
    bseg1->e_end = mid_3d;
    bseg1->tan_start = bseg->tan_start;
    bseg1->tan_end = edge_mid_tan;

    bedge_seg_t *bseg2 = new bedge_seg_t(bseg);
    bseg2->edge_start = emid;
    bseg2->edge_end = bseg->edge_end;
    bseg2->e_start = mid_3d;
    bseg2->e_end = bseg->e_end;
    bseg2->tan_start = edge_mid_tan;
    bseg2->tan_end = bseg->tan_end;

    // Remove the old segments from their respective rtrees
    if (update_rtrees) {
	rtree_bbox_2d_remove(s_cdt, bseg->tseg1);
	rtree_bbox_2d_remove(s_cdt, bseg->tseg2);
    }

    // Using the 2d mid points, update the polygons associated with tseg1 and tseg2.
    cpolyedge_t *poly1_ne1, *poly1_ne2, *poly2_ne1, *poly2_ne2;
    {
	cpolygon_t *poly1 = bseg->tseg1->polygon;
	int v[2];
	v[0] = bseg->tseg1->v2d[0];
	v[1] = bseg->tseg1->v2d[1];
	int trim_ind = bseg->tseg1->trim_ind;
	double old_trim_start = bseg->tseg1->trim_start;
	double old_trim_end = bseg->tseg1->trim_end;
	poly1->remove_ordered_edge(edge2d_t(v[0], v[1]));
	long poly1_2dind = poly1->add_point(trim1_mid_2d, f1_ind2d);
	struct edge2d_t poly1_edge1(v[0], poly1_2dind);
	poly1_ne1 = poly1->add_ordered_edge(poly1_edge1);
	poly1_ne1->trim_ind = trim_ind;
	poly1_ne1->trim_start = old_trim_start;
	poly1_ne1->trim_end = t1mid;
	struct edge2d_t poly1_edge2(poly1_2dind, v[1]);
	poly1_ne2 = poly1->add_ordered_edge(poly1_edge2);
	poly1_ne2->trim_ind = trim_ind;
	poly1_ne2->trim_start = t1mid;
	poly1_ne2->trim_end = old_trim_end;
    }
    {
	cpolygon_t *poly2 = bseg->tseg2->polygon;
	int v[2];
	v[0] = bseg->tseg2->v2d[0];
	v[1] = bseg->tseg2->v2d[1];
	int trim_ind = bseg->tseg2->trim_ind;
	double old_trim_start = bseg->tseg2->trim_start;
	double old_trim_end = bseg->tseg2->trim_end;
	poly2->remove_ordered_edge(edge2d_t(v[0], v[1]));
	long poly2_2dind = poly2->add_point(trim2_mid_2d, f2_ind2d);
	struct edge2d_t poly2_edge1(v[0], poly2_2dind);
	poly2_ne1 = poly2->add_ordered_edge(poly2_edge1);
	poly2_ne1->trim_ind = trim_ind;
	poly2_ne1->trim_start = old_trim_start;
	poly2_ne1->trim_end = t2mid;
	struct edge2d_t poly2_edge2(poly2_2dind, v[1]);
	poly2_ne2 = poly2->add_ordered_edge(poly2_edge2);
	poly2_ne2->trim_ind = trim_ind;
	poly2_ne2->trim_start = t2mid;
	poly2_ne2->trim_end = old_trim_end;
    }

    // The new trim segments are then associated with the new bounding edge
    // segments.  Open edges have distinct endpoints, so m_bRev3d supplies the
    // required correspondence.  A closed edge has the same endpoint at both
    // ends and its edge curve and p-curves may choose different periodic
    // phases.  In that case m_bRev3d alone cannot identify the matching half.
    // Compare a point strictly inside the first child edge with both candidate
    // trim intervals and choose the geometrically matching interval.  Repeat
    // this at every subdivision so the established phase is preserved.
    const bool closed_root = edge.IsClosed() ||
	bseg->e_root_start == bseg->e_root_end;
    const auto first_child_uses_first = [&](ON_BrepTrim *trim,
	    cpolyedge_t *first, cpolyedge_t *second) {
	if (!closed_root)
	    return !trim->m_bRev3d;
	const double edge_parameter = 0.5 * (bseg1->edge_start +
	    bseg1->edge_end);
	ON_3dPoint edge_point = bseg1->nc->PointAt(edge_parameter);
	const double edge_length = 0.5 *
	    (edge_point.DistanceTo(*bseg1->e_start) +
	     edge_point.DistanceTo(*bseg1->e_end));
	const auto miss = [&](cpolyedge_t *candidate) {
	    fastf_t trim_parameter = 0.0;
	    ON_2dPoint uv = get_trim_midpt(&trim_parameter, s_cdt,
		candidate, edge_point, edge_length,
		edge.m_tolerance);
	    const ON_Surface *surface = trim->SurfaceOf();
	    if (!surface || !uv.IsValid())
		return DBL_MAX;
	    for (int direction = 0; direction < 2; ++direction) {
		if (surface->IsClosed(direction))
		    uv[direction] = periodic_surface_parameter(uv[direction],
			surface->Domain(direction));
	    }
	    const ON_3dPoint surface_point = surface->PointAt(uv.x, uv.y);
	    return surface_point.IsValid() ?
		surface_point.DistanceTo(edge_point) : DBL_MAX;
	};
	const double first_miss = miss(first);
	const double second_miss = miss(second);
	if (!std::isfinite(first_miss) && !std::isfinite(second_miss))
	    return !trim->m_bRev3d;
	if (NEAR_EQUAL(first_miss, second_miss, ON_ZERO_TOLERANCE))
	    return !trim->m_bRev3d;
	return first_miss < second_miss;
    };
    const bool trim1_direct = first_child_uses_first(trim1, poly1_ne1,
	poly1_ne2);
    const bool trim2_direct = first_child_uses_first(trim2, poly2_ne1,
	poly2_ne2);
    bseg1->tseg1 = trim1_direct ? poly1_ne1 : poly1_ne2;
    bseg1->tseg2 = trim2_direct ? poly2_ne1 : poly2_ne2;
    bseg2->tseg1 = trim1_direct ? poly1_ne2 : poly1_ne1;
    bseg2->tseg2 = trim2_direct ? poly2_ne2 : poly2_ne1;

    // Associated the trim segments with the edge segment they actually
    // wound up assigned to
    bseg1->tseg1->eseg = bseg1;
    bseg1->tseg2->eseg = bseg1;
    bseg2->tseg1->eseg = bseg2;
    bseg2->tseg2->eseg = bseg2;

    nedges.insert(bseg1);
    nedges.insert(bseg2);

    // Update the rtrees with the new segments
    if (update_rtrees) {
	rtree_bbox_2d(s_cdt, bseg1->tseg1, 0);
	rtree_bbox_2d(s_cdt, bseg1->tseg2, 0);
	rtree_bbox_2d(s_cdt, bseg2->tseg1, 0);
	rtree_bbox_2d(s_cdt, bseg2->tseg2, 0);
#if 0
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	int face_index = s_cdt->brep->m_T[bseg1->tseg1->trim_ind].Face()->m_face_index;
	bu_vls_sprintf(&fname, "%d-rtree_2d_split_update.plot3", face_index);
	plot_rtree_2d2(s_cdt->face_rtrees_2d[face_index], bu_vls_cstr(&fname));
	face_index = s_cdt->brep->m_T[bseg2->tseg1->trim_ind].Face()->m_face_index;
	bu_vls_sprintf(&fname, "%d-rtree_2d_split_update.plot3", face_index);
	plot_rtree_2d2(s_cdt->face_rtrees_2d[face_index], bu_vls_cstr(&fname));
	bu_vls_free(&fname);
#endif
    }

    // Let e2polysegs know about the changes
    s_cdt->e2polysegs[edge.m_edge_index].erase(bseg);
    s_cdt->e2polysegs[edge.m_edge_index].insert(bseg1);
    s_cdt->e2polysegs[edge.m_edge_index].insert(bseg2);

    delete bseg;
    return nedges;
}

std::set<cpolyedge_t *>
split_singular_seg(struct ON_Brep_CDT_State *s_cdt, cpolyedge_t *ce, int update_rtrees)
{
    std::set<cpolyedge_t *> nedges;
    cpolygon_t *poly = ce->polygon;
    int trim_ind = ce->trim_ind;

    ON_BrepTrim& trim = s_cdt->brep->m_T[ce->trim_ind];
    double tcparam = (ce->trim_start + ce->trim_end) / 2.0;
    ON_3dPoint trim_mid_2d_ev = trim.PointAt(tcparam);
    ON_2dPoint trim_mid_2d(trim_mid_2d_ev.x, trim_mid_2d_ev.y);

    ON_BrepFace *face = trim.Face();
    cdt_mesh_t *fmesh = &s_cdt->fmeshes[face->m_face_index];
    long f_ind2d = fmesh->add_point(trim_mid_2d);

    // Singularity - new 2D point points to the same 3D point as both of the existing
    // vertices
    if (fmesh->p2d3d.find(f_ind2d) != fmesh->p2d3d.end()) {
	std::cout << fmesh->f_id << ": 2d->3d mapping already exists for " << f_ind2d << "\n";
    }
    fmesh->p2d3d[f_ind2d] = fmesh->p2d3d[poly->p2o[ce->v2d[0]]];

    if (update_rtrees) {
	rtree_bbox_2d_remove(s_cdt, ce);
    }

    s_cdt->unsplit_singular_edges.erase(ce);

    // Using the 2d mid points, update the polygons associated with tseg1 and tseg2.
    cpolyedge_t *poly_ne1, *poly_ne2;
    int v[2];
    v[0] = ce->v2d[0];
    v[1] = ce->v2d[1];
    double old_trim_start = ce->trim_start;
    double old_trim_end = ce->trim_end;
    poly->remove_edge(uedge2d_t(v[0], v[1]));
    long poly_2dind = poly->add_point(trim_mid_2d, f_ind2d);
    struct edge2d_t poly_edge1(v[0], poly_2dind);
    poly_ne1 = poly->add_edge(poly_edge1);
    poly_ne1->trim_ind = trim_ind;
    poly_ne1->trim_start = old_trim_start;
    poly_ne1->trim_end = tcparam;
    poly_ne1->eseg = NULL;
    struct edge2d_t poly_edge2(poly_2dind, v[1]);
    poly_ne2 = poly->add_edge(poly_edge2);
    poly_ne2->trim_ind = trim_ind;
    poly_ne2->trim_start = tcparam;
    poly_ne2->trim_end = old_trim_end;
    poly_ne2->eseg = NULL;

    nedges.insert(poly_ne1);
    nedges.insert(poly_ne2);

    if (update_rtrees) {
	rtree_bbox_2d(s_cdt, poly_ne1, 0);
	rtree_bbox_2d(s_cdt, poly_ne2, 0);
    }

    return nedges;
}


// There are a couple of edge splitting operations that have to happen in the
// beginning regardless of tolerance settings.  Do them up front so the subsequent
// working set has consistent properties.
bool
initialize_edge_segs(struct ON_Brep_CDT_State *s_cdt, char *message,
	size_t message_size)
{
    std::map<int, std::set<bedge_seg_t *>>::iterator epoly_it;
    for (epoly_it = s_cdt->e2polysegs.begin(); epoly_it != s_cdt->e2polysegs.end(); epoly_it++) {
	std::set<bedge_seg_t *>::iterator seg_it;
	std::set<bedge_seg_t *> wsegs = epoly_it->second;
	for (seg_it = wsegs.begin(); seg_it != wsegs.end(); seg_it++) {
	    bedge_seg_t *e = *seg_it;


	    ON_BrepEdge& edge = s_cdt->brep->m_E[e->edge_ind];
	    ON_BrepTrim *trim1 = edge.Trim(0);
	    ON_BrepTrim *trim2 = edge.Trim(1);
	    std::set<bedge_seg_t *> esegs_closed;

	    if (!trim1 || !trim2) {
		if (message && message_size)
		    snprintf(message, message_size,
			"B-Rep edge %d is missing one of its two trims",
			edge.m_edge_index);
		return false;
	    }

	    if (edge_has_singular_trim(trim1, trim2)) {
		if (message && message_size)
		    snprintf(message, message_size,
			"B-Rep edge %d has an unexpected singular trim",
			edge.m_edge_index);
		return false;
	    }

	    const bool closed_trim = trim1->IsClosed() || trim2->IsClosed();
	    // 1.  Any edges with at least 1 closed trim are split.
	    if (closed_trim) {
		esegs_closed = split_edge_seg(s_cdt, e, 1, NULL, 1, NULL,
		    true);
		if (!esegs_closed.size()) {
		    // split failed??  On a closed edge this is fatal - we must split it
		    // to work with it at all
		    if (message && message_size)
			snprintf(message, message_size,
			    "closed B-Rep edge %d could not be split",
			    edge.m_edge_index);
		    return false;
		}
	    } else {
		esegs_closed.insert(e);
	    }

	    // 2.  Any edges with a non-linear edge curve are split.
	    std::set<bedge_seg_t *> esegs_csplit;
	    const ON_Curve* crv = edge.EdgeCurveOf();
	    /* A closed NURBS edge can report linear when its coincident
	     * endpoints make the chord test degenerate.  Two half-edge chords
	     * still form only a two-vertex loop, so always apply the curved seed
	     * subdivision to closed trims.
	     */
	    if (edge_needs_curved_seed(crv, closed_trim)) {
		std::set<bedge_seg_t *>::iterator e_it;
		for (e_it = esegs_closed.begin(); e_it != esegs_closed.end(); e_it++) {
		    std::set<bedge_seg_t *> efirst = split_edge_seg(s_cdt,
			*e_it, 1, NULL, 1, NULL, closed_trim);
		    if (!efirst.size()) {
			/* A valid topological edge can have a corrupt 3-D curve or
			 * disagreeing p-curves which make a forced midpoint split
			 * impossible.  Retain its authoritative shared chord so the
			 * affected faces can fail geometric certification locally rather
			 * than preventing every unrelated face from being triangulated.
			 * Closed roots were already split above, so retaining one of
			 * their children cannot restore a one-edge degenerate loop. */
			esegs_csplit.insert(*e_it);
		    } else {
			// To avoid representing circles with squares, split curved segments
			// one additional time
			std::set<bedge_seg_t *>::iterator s_it;
			for (s_it = efirst.begin(); s_it != efirst.end(); s_it++) {
			    std::set<bedge_seg_t *> etmp = split_edge_seg(s_cdt,
				*s_it, 1, NULL, 1, NULL, closed_trim);
			    if (!etmp.size()) {
				// split failed??  This isn't good and shouldn't
				// happen, but it's not fatal the way the previous two
				// failure cases are...
				esegs_csplit.insert(*s_it);
			    } else {
				esegs_csplit.insert(etmp.begin(), etmp.end());
			    }
			}
		    }
		}
	    } else {
		esegs_csplit = esegs_closed;
	    }

	    s_cdt->e2polysegs[edge.m_edge_index].clear();
	    s_cdt->e2polysegs[edge.m_edge_index] = esegs_csplit;
	}
    }

#if 0
    for (int face_index = 0; face_index < s_cdt->brep->m_F.Count(); face_index++) {
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fname, "%d-rtree_2d_after_initial_splits.plot3", face_index);
	plot_rtree_2d2(s_cdt->face_rtrees_2d[face_index], bu_vls_cstr(&fname));
	bu_vls_free(&fname);
    }
#endif

    return true;
}

int
cdt_test_edge_singular_pair(void)
{
    ON_BrepTrim ordinary1;
    ON_BrepTrim ordinary2;
    ON_BrepTrim singular;
    ordinary1.m_type = ON_BrepTrim::boundary;
    ordinary2.m_type = ON_BrepTrim::mated;
    singular.m_type = ON_BrepTrim::singular;
    if (edge_has_singular_trim(&ordinary1, &ordinary2))
	return 1;
    if (!edge_has_singular_trim(&ordinary1, &singular))
	return 2;
    if (!edge_has_singular_trim(&singular, &ordinary2))
	return 3;
    return 0;
}


int
cdt_test_closed_edge_seed_policy(void)
{
    ON_LineCurve line(ON_3dPoint(0.0, 0.0, 0.0),
	ON_3dPoint(1.0, 0.0, 0.0));
    if (edge_needs_curved_seed(&line, false))
	return 1;
    if (!edge_needs_curved_seed(&line, true))
	return 2;
    if (edge_needs_curved_seed(NULL, true))
	return 3;
    return 0;
}

// Charcterize the edges.  Five possibilities:
//
// 0.  Singularity
// 1.  Curved edge
// 2.  Linear edge, associated with at least 1 non-planar surface
// 3.  Linear edge, associated with planar surfaces but sharing one or more vertices with
//     curved edges.
// 4.  Linear edge, associated only with planar faces and linear edges.
static std::vector<int>
characterize_edges(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;

    // Characterize the vertices - are they used by non-linear edges?
    std::vector<int> vert_type;
    for (int i = 0; i < brep->m_V.Count(); i++) {
	int has_curved_edge = 0;
	for (int j = 0; j < brep->m_V[i].m_ei.Count(); j++) {
	    ON_BrepEdge &edge = brep->m_E[brep->m_V[i].m_ei[j]];
	    const ON_Curve* crv = edge.EdgeCurveOf();
	    if (crv && !crv->IsLinear(BN_TOL_DIST)) {
		has_curved_edge = 1;
		break;
	    }
	}
	vert_type.push_back(has_curved_edge);
    }

    std::vector<int> edge_type;
    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	const ON_Curve* crv = edge.EdgeCurveOf();

	// Singularity
	if (!crv) {
	    edge_type.push_back(0);
	    continue;
	}

	// Curved edge
	if (!crv->IsLinear(BN_TOL_DIST)) {
	    edge_type.push_back(1);
	    continue;
	}

	// Linear edge, at least one non-planar surface
	const ON_Surface *s1= edge.Trim(0)->SurfaceOf();
	const ON_Surface *s2= edge.Trim(1)->SurfaceOf();
	if (!s1->IsPlanar(NULL, BN_TOL_DIST) || !s2->IsPlanar(NULL, BN_TOL_DIST)) {
	    edge_type.push_back(2);
	    continue;
	}

	// Linear edge, at least one associated non-linear edge
	if (vert_type[edge.Vertex(0)->m_vertex_index] || vert_type[edge.Vertex(1)->m_vertex_index]) {
	    edge_type.push_back(3);
	    continue;
	}

	// Linear edge, only associated with linear edges and planar faces
	edge_type.push_back(4);
    }
    return edge_type;
}

// Set up the edge containers that will manage the edge subdivision.  Loop
// ordering is not the job of these containers - that's handled by the trim loop
// polygons.  These containers maintain the association between trims in different
// faces and the 3D edge curve information used to drive shared points.
void
initialize_edge_containers(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;

    // Charcterize the edges.
    std::vector<int> edge_type = characterize_edges(s_cdt);

    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	bedge_seg_t *bseg = new bedge_seg_t;
	bseg->edge_ind = edge.m_edge_index;
	bseg->brep = s_cdt->brep;
	bseg->p_cdt= (void *)s_cdt;

	// Provide a normalize edge NURBS curve
	const ON_Curve* crv = edge.EdgeCurveOf();
	bseg->nc = crv->NurbsCurve();
	bseg->cp_len = bseg->nc->ControlPolygonLength();
	bseg->nc->SetDomain(0.0, bseg->cp_len);

	// Set the initial edge curve t parameter values
	bseg->edge_start = 0.0;
	bseg->edge_end = bseg->cp_len;

	// Get the trims and verify that both have parameter-space curves.  Keep
	// their native domains: edge and trim segments track their parameters
	// independently and are paired geometrically when they split.  Replacing
	// a trim domain with the 3-D edge control-polygon length corrupts
	// reversed and otherwise non-identically parameterized p-curves.
	// NOTE - another point where this won't work if we don't have a 1->2 edge to trims relationship
	ON_BrepTrim *trim1 = edge.Trim(0);
	ON_BrepTrim *trim2 = edge.Trim(1);
	int t1cind = trim1->TrimCurveIndexOf();
	if (t1cind < 0) {
	    delete bseg;
	    continue;
	}
	int t2cind = trim2->TrimCurveIndexOf();
	if (t2cind < 0) {
	    delete bseg;
	    continue;
	}
	// The 3D start and endpoints will be vertex points (they are shared with other edges).
	bseg->e_start = (*s_cdt->vert_pnts)[edge.Vertex(0)->m_vertex_index];
	bseg->e_end = (*s_cdt->vert_pnts)[edge.Vertex(1)->m_vertex_index];

	// These are also the root start and end points - type 3 edges will need this information later
	bseg->e_root_start = bseg->e_start;
	bseg->e_root_end = bseg->e_end;

	// Stash the edge type - we will need it during refinement
	bseg->edge_type = edge_type[edge.m_edge_index];

	s_cdt->e2polysegs[edge.m_edge_index].insert(bseg);
    }
}

// For each face and each loop in each face define the initial
// loop polygons.  Note there is no splitting of edges at this point -
// we are simply establishing the initial closed polygons.
bool
initialize_loop_polygons(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;
    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	int loop_cnt = face.LoopCount();
	cdt_mesh_t *fmesh = &s_cdt->fmeshes[face_index];
	fmesh->f_id = face_index;
	fmesh->m_bRev = face.m_bRev;
	fmesh->has_singularities = false;
	cpolygon_t *cpoly = NULL;

	for (int li = 0; li < loop_cnt; li++) {
	    const ON_BrepLoop *loop = face.Loop(li);
	    bool is_outer = (face.OuterLoop()->m_loop_index == loop->m_loop_index) ? true : false;
	    if (is_outer) {
		cpoly = &fmesh->outer_loop;
	    } else {
		cpoly = new cpolygon_t;
		fmesh->inner_loops[li] = cpoly;
	    }
	    int trim_count = loop->TrimCount();

	    ON_2dPoint cp(0,0);

	    long cv = -1;
	    long pv = -1;
	    long fv = -1;

	    for (int lti = 0; lti < trim_count; lti++) {
		ON_BrepTrim *trim = loop->Trim(lti);
		ON_Interval range = trim->Domain();
		if (lti == 0) {
		    // Get the 2D point, add it to the mesh and current polygon
		    cp = trim->PointAt(range.m_t[0]);
		    long find = fmesh->add_point(cp);
		    pv = cpoly->add_point(cp, find);
		    fv = pv;

		    // Let cdt_mesh know about new 3D information
		    ON_3dPoint *op3d = (*s_cdt->vert_pnts)[trim->Vertex(0)->m_vertex_index];
		    ON_3dVector norm = ON_3dVector::UnsetVector;
		    if (trim->m_type != ON_BrepTrim::singular) {
			// 3D points are globally unique, but normals are not - the same edge point may
			// have different normals from two faces at a sharp edge.  Calculate the
			// face normal for this point on this surface.
			norm = calc_trim_vnorm(*trim->Vertex(0), trim);
			//std::cout << "Face " << face.m_face_index << ", Loop " << loop->m_loop_index << ", Vert " << trim->Vertex(0)->m_vertex_index << " norm: " << norm.x << "," << norm.y << "," << norm.z << "\n";
		    } else {
			// Surface sampling will need some information about singularities
			s_cdt->strim_pnts[face_index][trim->m_trim_index] = op3d;
			ON_3dPoint *sn3d = (*s_cdt->vert_avg_norms)[trim->Vertex(0)->m_vertex_index];
			if (sn3d) {
			    s_cdt->strim_norms[face_index][trim->m_trim_index] = sn3d;
			}
		    }
		    long f3ind = fmesh->add_point(op3d);
		    long fnind = fmesh->add_normal(new ON_3dPoint(norm));
		    CDT_Add3DNorm(s_cdt, fmesh->normals[fmesh->normals.size()-1], op3d, face.m_face_index, trim->Vertex(0)->m_vertex_index, trim->m_trim_index, -1, cp.x, cp.y);
		    if (fmesh->p2d3d.find(find) != fmesh->p2d3d.end()) {
			std::cout << fmesh->f_id << ": 2d->3d mapping already exists for " << find << "\n";
		    }
		    fmesh->p2d3d[find] = f3ind;
		    fmesh->nmap[f3ind] = fnind;

		} else {
		    pv = cv;
		}

		// Get the 2D point, add it to the mesh and current polygon
		cp = trim->PointAt(range.m_t[1]);
		if (lti == trim_count - 1) {
		    cv = fv;
		} else {
		    long find;
		    find = fmesh->add_point(cp);
		    cv = cpoly->add_point(cp, find);

		    // Let cdt_mesh know about the 3D information
		    ON_3dPoint *cp3d = (*s_cdt->vert_pnts)[trim->Vertex(1)->m_vertex_index];
		    ON_3dVector norm = ON_3dVector::UnsetVector;
		    if (trim->m_type != ON_BrepTrim::singular) {
			// 3D points are globally unique, but normals are not - the same edge point may
			// have different normals from two faces at a sharp edge.  Calculate the
			// face normal for this point on this surface.
			norm = calc_trim_vnorm(*trim->Vertex(1), trim);
			//std::cout << "Face " << face.m_face_index << ", Loop " << loop->m_loop_index << ", Vert " << trim->Vertex(1)->m_vertex_index << " norm: " << norm.x << "," << norm.y << "," << norm.z << "\n";
		    } else {
			// Surface sampling will need some information about singularities
			s_cdt->strim_pnts[face_index][trim->m_trim_index] = cp3d;
			ON_3dPoint *sn3d = (*s_cdt->vert_avg_norms)[trim->Vertex(1)->m_vertex_index];
			if (sn3d) {
			    s_cdt->strim_norms[face_index][trim->m_trim_index] = sn3d;
			}
		    }

		    long f3ind = fmesh->add_point(cp3d);
		    long fnind = fmesh->add_normal(new ON_3dPoint(norm));
		    CDT_Add3DNorm(s_cdt, fmesh->normals[fmesh->normals.size()-1], cp3d, face.m_face_index, trim->Vertex(1)->m_vertex_index, trim->m_trim_index, -1, cp.x, cp.y);
		    if (fmesh->p2d3d.find(find) != fmesh->p2d3d.end()) {
			std::cout << fmesh->f_id << ": 2d->3d mapping already exists for " << find << "\n";
		    }
		    fmesh->p2d3d[find] = f3ind;
		    fmesh->nmap[f3ind] = fnind;
		}

		struct edge2d_t lseg(pv, cv);
		cpolyedge_t *ne = cpoly->add_ordered_edge(lseg);

		ne->trim_ind = trim->m_trim_index;
		ne->loop_type = (is_outer) ? 1 : 2;
		ne->trim_start = range.m_t[0];
		ne->trim_end = range.m_t[1];

		rtree_bbox_2d(s_cdt, ne, 0);

		if (trim->m_ei >= 0) {
		    bedge_seg_t *eseg = *s_cdt->e2polysegs[trim->m_ei].begin();
		    // Associate the edge segment with the trim segment and vice versa
		    ne->eseg = eseg;
		    if (eseg->tseg1 && eseg->tseg2) {
			bu_log("error - more than two trims associated with an edge\n");
			return false;
		    }
		    if (eseg->tseg1) {
			eseg->tseg2 = ne;
		    } else {
			eseg->tseg1 = ne;
		    }

		} else {
		    // A null eseg will indicate a singularity and a need for special case
		    // splitting of the 2D edge only
		    ne->eseg = NULL;
		    if (!cdt_face_uses_topology_chart(face))
			s_cdt->unsplit_singular_edges.insert(ne);
		    fmesh->has_singularities = true;
		}
	    }
	}

#if 0
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fname, "%d-rtree_2d_initial.plot3", face_index);
	plot_rtree_2d2(s_cdt->face_rtrees_2d[face_index], bu_vls_cstr(&fname));
	bu_vls_free(&fname);
#endif
    }
    return true;
}

bool
split_edges_at_surface_poles(struct ON_Brep_CDT_State *s_cdt,
	char *failure_message, size_t failure_message_size)
{
    if (!s_cdt || !s_cdt->brep)
	return false;
    if (failure_message && failure_message_size)
	failure_message[0] = '\0';

    for (int edge_index = 0; edge_index < s_cdt->brep->m_E.Count();
	    ++edge_index) {
	std::set<bedge_seg_t *> &segments =
	    s_cdt->e2polysegs[edge_index];
	if (segments.empty())
	    continue;
	bedge_seg_t *root = *segments.begin();
	if (!root || !root->nc || !root->tseg1 || !root->tseg2)
	    continue;
	std::set<ON_3dPoint *> pole_points;
	const cpolyedge_t *trim_segments[2] = {root->tseg1, root->tseg2};
	for (const cpolyedge_t *trim_segment : trim_segments) {
	    if (!trim_segment || trim_segment->trim_ind < 0 ||
		    trim_segment->trim_ind >= s_cdt->brep->m_T.Count())
		continue;
	    const ON_BrepFace *face = s_cdt->brep->m_T[
		trim_segment->trim_ind].Face();
	    if (!face)
		continue;
	    const auto face_poles = s_cdt->strim_pnts.find(
		face->m_face_index);
	    if (face_poles == s_cdt->strim_pnts.end())
		continue;
	    for (const auto &pole : face_poles->second) {
		if (pole.second)
		    pole_points.insert(pole.second);
	    }
	}
	if (pole_points.empty())
	    continue;

	struct pole_split {
	    double parameter;
	    ON_3dPoint *point;
	};
	std::vector<pole_split> splits;
	const ON_Interval root_domain(std::min(root->edge_start,
	    root->edge_end), std::max(root->edge_start, root->edge_end));
	for (ON_3dPoint *pole : pole_points) {
	    const double coordinate_scale = std::max(1.0, std::max(
		std::max(std::fabs(pole->x), std::fabs(pole->y)),
		std::fabs(pole->z)));
	    const double tolerance = std::max((double)BN_TOL_DIST,
		4096.0 * std::numeric_limits<double>::epsilon() *
		coordinate_scale);
	    double parameter = DBL_MAX;
	    if (curve_interior_point_parameter(&parameter, root->nc,
		    root_domain, *pole, tolerance))
		splits.push_back({parameter, pole});
	}
	std::sort(splits.begin(), splits.end(), [](const pole_split &first,
		const pole_split &second) {
	    if (first.parameter < second.parameter)
		return true;
	    if (second.parameter < first.parameter)
		return false;
	    return first.point < second.point;
	});
	for (size_t split_index = 0; split_index < splits.size(); ++split_index) {
	    if (split_index && std::fabs(splits[split_index].parameter -
		    splits[split_index - 1].parameter) <= 4096.0 *
		    std::numeric_limits<double>::epsilon() * std::max(1.0,
		    std::fabs(splits[split_index].parameter))) {
		if (splits[split_index].point !=
			splits[split_index - 1].point) {
		    if (failure_message && failure_message_size)
			std::snprintf(failure_message, failure_message_size,
			    "B-Rep edge %d crosses coincident surface poles "
			    "with distinct topology", edge_index);
		    return false;
		}
		continue;
	    }
	    bedge_seg_t *target = NULL;
	    for (bedge_seg_t *segment : segments) {
		if (split_parameter_interior(segment->edge_start,
			segment->edge_end, splits[split_index].parameter)) {
		    target = segment;
		    break;
		}
	    }
	    if (!target)
		continue;
	    double parameter = splits[split_index].parameter;
	    const std::set<bedge_seg_t *> children = split_edge_seg(s_cdt,
		target, 1, &parameter, 1, splits[split_index].point);
	    if (children.empty()) {
		if (failure_message && failure_message_size)
		    std::snprintf(failure_message, failure_message_size,
			"B-Rep edge %d could not be split at a surface pole",
			edge_index);
		return false;
	    }
	    bu_log("Split B-Rep edge %d at an interior surface pole\n",
		edge_index);
	}
    }
    return true;
}

/* Imported analytic seams are sometimes represented by two distinct B-Rep
 * edges with the same endpoints and curves which agree within modeling
 * tolerance.  Refining those edges independently produces alternating,
 * nearly coincident samples and turns an intended retrace into many tiny
 * chart crossings.  Prove curve coincidence in both directions, then insert
 * the union of both sample sets into both edges using shared 3-D pointers. */
size_t
synchronize_coincident_edge_samples(struct ON_Brep_CDT_State *s_cdt,
	std::map<ON_3dPoint *, ON_3dPoint *> &welds)
{
    if (!s_cdt || !s_cdt->brep || !std::isfinite(s_cdt->absmin) ||
	    s_cdt->absmin <= 0.0)
	return 0;
    const double tolerance = std::min((double)BN_TOL_DIST,
	(double)s_cdt->absmin);
    if (!(tolerance > 0.0))
	return 0;

    typedef std::pair<ON_3dPoint *, ON_3dPoint *> endpoint_pair;
    std::map<endpoint_pair, std::vector<int>> endpoint_edges;
    for (const auto &entry : s_cdt->e2polysegs) {
	if (entry.second.empty())
	    continue;
	const bedge_seg_t *segment = *entry.second.begin();
	if (!segment || !segment->nc || !segment->e_root_start ||
		!segment->e_root_end ||
		segment->e_root_start == segment->e_root_end)
	    continue;
	ON_3dPoint *first = segment->e_root_start;
	ON_3dPoint *second = segment->e_root_end;
	if (std::less<ON_3dPoint *>()(second, first))
	    std::swap(first, second);
	endpoint_edges[endpoint_pair(first, second)].push_back(entry.first);
    }

    const auto root_segment = [&](int edge_index) {
	const auto entry = s_cdt->e2polysegs.find(edge_index);
	return entry == s_cdt->e2polysegs.end() || entry->second.empty() ?
	    (bedge_seg_t *)NULL : *entry->second.begin();
    };
    const auto root_domain = [&](int edge_index) {
	double minimum = DBL_MAX;
	double maximum = -DBL_MAX;
	for (const bedge_seg_t *segment : s_cdt->e2polysegs[edge_index]) {
	    minimum = std::min(minimum, std::min(segment->edge_start,
		segment->edge_end));
	    maximum = std::max(maximum, std::max(segment->edge_start,
		segment->edge_end));
	}
	return ON_Interval(minimum, maximum);
    };
    const auto curves_coincident = [&](int first_edge, int second_edge) {
	const bedge_seg_t *first = root_segment(first_edge);
	const bedge_seg_t *second = root_segment(second_edge);
	if (!first || !second || !first->nc || !second->nc)
	    return false;
	const ON_Interval first_domain = root_domain(first_edge);
	const ON_Interval second_domain = root_domain(second_edge);
	if (!first_domain.IsIncreasing() || !second_domain.IsIncreasing())
	    return false;
	const bool reversed = first->e_root_start == second->e_root_end &&
	    first->e_root_end == second->e_root_start;
	if (!reversed && (first->e_root_start != second->e_root_start ||
		first->e_root_end != second->e_root_end))
	    return false;
	const auto one_direction = [&](const bedge_seg_t *source,
		const ON_Interval &source_domain, const bedge_seg_t *target,
		const ON_Interval &target_domain) {
	    for (int i = 0; i <= 32; ++i) {
		const double fraction = (double)i / 32.0;
		const ON_3dPoint point = source->nc->PointAt(
		    source_domain.ParameterAt(fraction));
		if (!point.IsValid())
		    return false;
		if (i == 0 || i == 32) {
		    const double target_fraction = reversed ?
			1.0 - fraction : fraction;
		    const ON_3dPoint target_point = target->nc->PointAt(
			target_domain.ParameterAt(target_fraction));
		    if (!target_point.IsValid() ||
			    point.DistanceTo(target_point) > tolerance)
			return false;
		    continue;
		}
		const ON_3dPoint target_start = target->nc->PointAt(
		    target_domain.Min());
		const ON_3dPoint target_end = target->nc->PointAt(
		    target_domain.Max());
		if ((target_start.IsValid() &&
			point.DistanceTo(target_start) <= tolerance) ||
			(target_end.IsValid() &&
			point.DistanceTo(target_end) <= tolerance))
		    continue;
		double parameter = DBL_MAX;
		if (!curve_interior_point_parameter_impl(&parameter, target->nc,
			target_domain, point, tolerance, false))
		    return false;
	    }
	    return true;
	};
	return one_direction(first, first_domain, second, second_domain) &&
	    one_direction(second, second_domain, first, first_domain);
    };
    const auto sample_points = [&](int edge_index) {
	std::vector<std::pair<double, ON_3dPoint *>> points;
	std::vector<std::pair<double, ON_3dPoint *>> ordered;
	const ON_Interval domain = root_domain(edge_index);
	for (const bedge_seg_t *segment : s_cdt->e2polysegs[edge_index]) {
	    if (split_parameter_interior(domain.Min(), domain.Max(),
		    segment->edge_start))
		ordered.push_back(std::make_pair(segment->edge_start,
		    segment->e_start));
	    if (split_parameter_interior(domain.Min(), domain.Max(),
		    segment->edge_end))
		ordered.push_back(std::make_pair(segment->edge_end,
		    segment->e_end));
	}
	std::sort(ordered.begin(), ordered.end(),
	    [](const std::pair<double, ON_3dPoint *> &first,
		const std::pair<double, ON_3dPoint *> &second) {
		if (first.first < second.first)
		    return true;
		if (second.first < first.first)
		    return false;
		return first.second < second.second;
	    });
	std::set<ON_3dPoint *> seen;
	for (const auto &sample : ordered) {
	    if (sample.second && seen.insert(sample.second).second)
		points.push_back(std::make_pair(domain.NormalizedParameterAt(
		    sample.first), sample.second));
	}
	return points;
    };
    const auto weld_pair = [&](ON_3dPoint *first, ON_3dPoint *second) {
	if (!first || !second || first == second)
	    return;
	ON_3dPoint *representative = first;
	ON_3dPoint *removed = second;
	if (std::less<ON_3dPoint *>()(second, first)) {
	    representative = second;
	    removed = first;
	}
	welds[removed] = representative;
    };
    const auto insert_samples = [&](const std::vector<std::pair<double,
	    ON_3dPoint *>> &points, int edge_index, bool reversed) {
	bedge_seg_t *root = root_segment(edge_index);
	if (!root || !root->nc)
	    return false;
	const ON_Interval domain = root_domain(edge_index);
	/* split_edge_seg may replace and delete the root segment.  The edge's
	 * endpoint point objects remain authoritative throughout subdivision, so
	 * retain those pointers before inserting any synchronized samples. */
	ON_3dPoint *root_start = root->e_root_start;
	ON_3dPoint *root_end = root->e_root_end;
	for (const auto &sample : points) {
	    ON_3dPoint *point = sample.second;
	    bool present = false;
	    ON_3dPoint *nearest = NULL;
	    double nearest_distance = DBL_MAX;
	    for (const bedge_seg_t *segment : s_cdt->e2polysegs[edge_index]) {
		if (segment->e_start == point || segment->e_end == point) {
		    present = true;
		    break;
		}
		const double start_distance = point->DistanceTo(
		    *segment->e_start);
		const double end_distance = point->DistanceTo(*segment->e_end);
		if (start_distance < nearest_distance) {
		    nearest_distance = start_distance;
		    nearest = segment->e_start;
		}
		if (end_distance < nearest_distance) {
		    nearest_distance = end_distance;
		    nearest = segment->e_end;
		}
	    }
	    if (present)
		continue;
	    /* Independently refined coincident curves normally already have a
	     * corresponding sample within tolerance.  Weld those samples instead
	     * of interleaving two almost identical parameter sequences, which
	     * would create chart slivers and crossings. */
	    if (nearest && nearest_distance <= tolerance) {
		weld_pair(point, nearest);
		continue;
	    }
	    if (root_start && point->DistanceTo(*root_start) <= tolerance) {
		weld_pair(point, root_start);
		continue;
	    }
	    if (root_end && point->DistanceTo(*root_end) <= tolerance) {
		weld_pair(point, root_end);
		continue;
	    }
	    const double target_fraction = reversed ? 1.0 - sample.first :
		sample.first;
	    const double parameter = domain.ParameterAt(target_fraction);
	    if (!split_parameter_interior(domain.Min(), domain.Max(), parameter))
		return false;
	    bedge_seg_t *target = NULL;
	    for (bedge_seg_t *segment : s_cdt->e2polysegs[edge_index]) {
		if (split_parameter_interior(segment->edge_start,
			segment->edge_end, parameter)) {
		    target = segment;
		    break;
		}
	    }
	    if (!target) {
		nearest = NULL;
		nearest_distance = DBL_MAX;
		for (bedge_seg_t *segment : s_cdt->e2polysegs[edge_index]) {
		    const double start_distance = point->DistanceTo(
			*segment->e_start);
		    const double end_distance = point->DistanceTo(
			*segment->e_end);
		    if (start_distance < nearest_distance) {
			nearest_distance = start_distance;
			nearest = segment->e_start;
		    }
		    if (end_distance < nearest_distance) {
			nearest_distance = end_distance;
			nearest = segment->e_end;
		    }
		}
		if (!nearest || nearest_distance > tolerance)
		    return false;
		weld_pair(point, nearest);
		continue;
	    }
	    double split_parameter = parameter;
	    if (split_edge_seg(s_cdt, target, 1, &split_parameter, 1,
		    point).empty())
		return false;
	}
	return true;
    };

    size_t synchronized = 0;
    for (const auto &group : endpoint_edges) {
	if (group.second.size() != 2)
	    continue;
	const int first_edge = group.second[0];
	const int second_edge = group.second[1];
	/* Matching 3-D curves are not sufficient: distinct edges may occupy the
	 * same locus while following different paths in a singular face chart. */
	bool shared_face_retrace = false;
	const ON_BrepEdge &first_topology_edge =
	    s_cdt->brep->m_E[first_edge];
	const ON_BrepEdge &second_topology_edge =
	    s_cdt->brep->m_E[second_edge];
	for (int first_trim = 0;
		!shared_face_retrace &&
		first_trim < first_topology_edge.TrimCount(); ++first_trim) {
	    for (int second_trim = 0;
		    !shared_face_retrace &&
		    second_trim < second_topology_edge.TrimCount();
		    ++second_trim)
		shared_face_retrace = cdt_trim_pcurves_retrace(
		    first_topology_edge.Trim(first_trim),
		    second_topology_edge.Trim(second_trim));
	}
	if (!shared_face_retrace)
	    continue;
	const bool coincident = curves_coincident(first_edge, second_edge);
	if (!coincident)
	    continue;
	const bedge_seg_t *first_root = root_segment(first_edge);
	const bedge_seg_t *second_root = root_segment(second_edge);
	if (!first_root || !second_root)
	    continue;
	const bool reversed = first_root->e_root_start ==
	    second_root->e_root_end && first_root->e_root_end ==
	    second_root->e_root_start;
	const std::vector<std::pair<double, ON_3dPoint *>> first_points =
	    sample_points(first_edge);
	const std::vector<std::pair<double, ON_3dPoint *>> second_points =
	    sample_points(second_edge);
	/* Corresponding refinement already provides equal geometry on both
	 * edges.  Welding distinct topology samples would collapse a valid seam. */
	bool already_aligned = first_points.size() == second_points.size();
	for (size_t point_index = 0;
		already_aligned && point_index < first_points.size();
		++point_index) {
	    const size_t second_index = reversed ?
		second_points.size() - point_index - 1 : point_index;
	    const ON_3dPoint *first_point = first_points[point_index].second;
	    const ON_3dPoint *second_point =
		second_points[second_index].second;
	    already_aligned = first_point && second_point &&
		first_point->DistanceTo(*second_point) <= tolerance;
	}
	if (already_aligned)
	    continue;
	if (!insert_samples(first_points, second_edge, reversed) ||
		!insert_samples(second_points, first_edge, reversed))
	    continue;
	synchronized++;
	bu_log("Synchronized coincident B-Rep edges %d and %d within %.17g\n",
	    first_edge, second_edge, tolerance);
    }
    return synchronized;
}

// Split curved edges per tolerance settings
void
tol_curved_edges_split(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;
    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	const ON_Curve* crv = edge.EdgeCurveOf();
	// TODO - BN_TOL_DIST will be too large for very small trims - need to do
	// something similar to the ptol calculation for these edge curves...
	if (crv && !crv->IsLinear(BN_TOL_DIST)) {
	    std::set<bedge_seg_t *> &epsegs = s_cdt->e2polysegs[edge.m_edge_index];
	    std::set<bedge_seg_t *>::iterator e_it;
	    std::set<bedge_seg_t *> new_segs;
	    std::set<bedge_seg_t *> ws1, ws2;
	    std::set<bedge_seg_t *> *ws = &ws1;
	    std::set<bedge_seg_t *> *ns = &ws2;
	    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
		bedge_seg_t *b = *e_it;
		ws->insert(b);
	    }
	    while (ws->size()) {
		bedge_seg_t *b = *ws->begin();
		ws->erase(ws->begin());
		std::set<bedge_seg_t *> esegs_split = split_edge_seg(s_cdt, b, 0, NULL, 0);
		if (esegs_split.size()) {
		    ns->insert(esegs_split.begin(), esegs_split.end());
		} else {
		    new_segs.insert(b);
		}
		if (!ws->size() && ns->size()) {
		    std::set<bedge_seg_t *> *tmp = ws;
		    ws = ns;
		    ns = tmp;
		}
	    }
	    s_cdt->e2polysegs[edge.m_edge_index].clear();
	    s_cdt->e2polysegs[edge.m_edge_index] = new_segs;
	}
    }
}

// Calculate for each vertex involved with curved edges the minimum individual bedge_seg
// length involved.
static void
update_vert_edge_seg_lengths(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;
    for (int i = 0; i < brep->m_V.Count(); i++) {
	ON_3dPoint *p3d = (*s_cdt->vert_pnts)[i];
	double emin = DBL_MAX;
	for (int j = 0; j < brep->m_V[i].m_ei.Count(); j++) {
	    ON_BrepEdge &edge = brep->m_E[brep->m_V[i].m_ei[j]];
	    std::set<bedge_seg_t *> &epsegs = s_cdt->e2polysegs[edge.m_edge_index];
	    std::set<bedge_seg_t *>::iterator e_it;
	    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
		bedge_seg_t *b = *e_it;
		if (b->e_start == p3d || b->e_end == p3d) {
		    ON_Line line3d(*(b->e_start), *(b->e_end));
		    double seg_len = line3d.Length();
		    if (seg_len < emin) {
			emin = seg_len;
		    }
		}
	    }
	}
	s_cdt->v_min_seg_len[p3d] = emin;
	//std::cout << "Minimum vert seg length, vert " << i << ": " << s_cdt->v_min_seg_len[p3d] << "\n";
    }
}

// Calculate loop median segment lengths contributed from the curved edges
static void
update_loop_median_curved_edge_seg_lengths(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;
    for (int index = 0; index < brep->m_L.Count(); index++) {
	const ON_BrepLoop &loop = brep->m_L[index];
	std::vector<double> lsegs;
	for (int lti = 0; lti < loop.TrimCount(); lti++) {
	    ON_BrepTrim *trim = loop.Trim(lti);
	    ON_BrepEdge *edge = trim->Edge();
	    if (!edge) continue;
	    const ON_Curve* crv = edge->EdgeCurveOf();
	    if (!crv || crv->IsLinear(BN_TOL_DIST)) {
		continue;
	    }
	    std::set<bedge_seg_t *> &epsegs = s_cdt->e2polysegs[edge->m_edge_index];
	    if (!epsegs.size()) continue;
	    std::set<bedge_seg_t *>::iterator e_it;
	    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
		bedge_seg_t *b = *e_it;
		double seg_dist = b->e_start->DistanceTo(*b->e_end);
		lsegs.push_back(seg_dist);
	    }
	}
	if (!lsegs.size()) {
	    // No non-linear edges, so no segments to use
	    s_cdt->l_median_len[index] = -1;
	} else {
	    s_cdt->l_median_len[index] = median_seg_len(lsegs);
	    //std::cout << "Median loop seg length, loop " << index << ": " << s_cdt->l_median_len[index] << "\n";
	}
    }
}

// After the initial curve split, make another pass looking for curved
// edges sharing a vertex.  We want larger curves to refine close to the
// median segment length of the smaller ones, since this situation can be a
// sign that the surface will generate small triangles near large ones.
void
curved_edges_refine(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;

    // Calculate for each vertex involved with curved edges the minimum individual bedge_seg
    // length involved.
    update_vert_edge_seg_lengths(s_cdt);


    // Calculate loop median segment lengths contributed from the curved edges
    update_loop_median_curved_edge_seg_lengths(s_cdt);

    std::map<int, double> refine_targets;
    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	const ON_Curve* crv = edge.EdgeCurveOf();
	if (!crv || crv->IsLinear(BN_TOL_DIST)) continue;
	/* A closed edge has the same topological vertex at both ends.  A short
	 * curve incident at that seam does not justify imposing its segment
	 * length uniformly around the entire closed edge.  The curve and chord
	 * tolerances have already supplied the required geometric refinement. */
	if (edge.m_vi[0] == edge.m_vi[1]) continue;
	bool refine = false;
	double target_len = DBL_MAX;
	double lmed = edge_median_seg_len(s_cdt, edge.m_edge_index);
	for (int i = 0; i < 2; i++) {
	    int vert_ind = edge.Vertex(i)->m_vertex_index;
	    for (int j = 0; j < brep->m_V[vert_ind].m_ei.Count(); j++) {
		ON_BrepEdge &e2= brep->m_E[brep->m_V[vert_ind].m_ei[j]];
		const ON_Curve* crv2 = e2.EdgeCurveOf();
		if (crv2 && !crv2->IsLinear(BN_TOL_DIST)) {
		    double emed = edge_median_seg_len(s_cdt, e2.m_edge_index);
		    if (emed < lmed) {
			target_len = (2*emed < target_len) ? 2*emed : target_len;
			refine = true;
		    }
		}
	    }
	}
	if (refine) {
	    refine_targets[index] = target_len;
	}
    }
    std::map<int, double>::iterator r_it;
    for (r_it = refine_targets.begin(); r_it != refine_targets.end(); r_it++) {
	ON_BrepEdge& edge = brep->m_E[r_it->first];
	double split_tol = r_it->second;
	std::set<bedge_seg_t *> &epsegs = s_cdt->e2polysegs[r_it->first];
	/* This vertex-neighborhood pass improves element sizing after the
	 * geometric curve and chord tolerances have already been satisfied.  A
	 * tiny incident edge must not impose its scale uniformly across an
	 * unrelated long curve.  Bound the inherited spacing by both the global
	 * minimum mesh dimension and a per-source-edge subdivision limit. */
	if (epsegs.empty())
	    continue;
	if (!shape_refinement_spacing(&split_tol, s_cdt->absmin,
		(*epsegs.begin())->cp_len))
	    continue;
	std::set<bedge_seg_t *>::iterator e_it;
	std::set<bedge_seg_t *> new_segs;
	std::set<bedge_seg_t *> ws1, ws2;
	std::set<bedge_seg_t *> *ws = &ws1;
	std::set<bedge_seg_t *> *ns = &ws2;
	for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
	    bedge_seg_t *b = *e_it;
	    ws->insert(b);
	}
	while (ws->size()) {
	    bedge_seg_t *b = *ws->begin();
	    ws->erase(ws->begin());
	    bool split_edge = (b->e_start->DistanceTo(*b->e_end) > split_tol);
	    if (split_edge) {
		// If we need to split, do so
		std::set<bedge_seg_t *> esegs_split = split_edge_seg(s_cdt, b, 1, NULL, 0);
		if (esegs_split.size()) {
		    ws->insert(esegs_split.begin(), esegs_split.end());
		} else {
		    new_segs.insert(b);
		}
	    } else {
		new_segs.insert(b);
	    }
	    if (!ws->size() && ns->size()) {
		std::set<bedge_seg_t *> *tmp = ws;
		ws = ns;
		ns = tmp;
	    }
	}
	s_cdt->e2polysegs[edge.m_edge_index].clear();
	s_cdt->e2polysegs[edge.m_edge_index] = new_segs;
    }

}

// Split linear edges according to tolerance information
bool
tol_linear_edges_split(struct ON_Brep_CDT_State *s_cdt,
	char *failure_message, size_t failure_message_size)
{
    /* Binary subdivision past this point is almost certainly a bad inherited
     * length scale.  It also has a disproportionate memory cost: every split
     * is represented in two face polygons, their maps, normals, and audit
     * state.  Fail closed if the absmin floor does not bound an unusual case. */
    const size_t max_segments_per_edge = 65536;
    ON_Brep* brep = s_cdt->brep;
    if (failure_message && failure_message_size)
	failure_message[0] = '\0';

    // Calculate loop median segment lengths contributed from the curved edges
    update_loop_median_curved_edge_seg_lengths(s_cdt);

    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	const ON_Curve* crv = edge.EdgeCurveOf();
	if (crv && crv->IsLinear(BN_TOL_DIST)) {
	    std::set<bedge_seg_t *> &epsegs = s_cdt->e2polysegs[edge.m_edge_index];
	    std::set<bedge_seg_t *>::iterator e_it;
	    std::set<bedge_seg_t *> new_segs;
	    std::set<bedge_seg_t *> ws1, ws2;
	    std::set<bedge_seg_t *> *ws = &ws1;
	    std::set<bedge_seg_t *> *ns = &ws2;
	    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
		bedge_seg_t *b = *e_it;
		ws->insert(b);
	    }
	    while (ws->size()) {
		bedge_seg_t *b = *ws->begin();
		ws->erase(ws->begin());
		const double parent_chord =
		    b->e_start->DistanceTo(*b->e_end);
		const double parent_start = b->edge_start;
		const double parent_end = b->edge_end;
		double midpoint = 0.0;
		double residual = DBL_MAX;
		int progress = 1;
		/* Curve endpoint evaluation is only needed in the exhausted
		 * floating-point interval case.  Keep the ordinary subdivision
		 * path free of two redundant curve evaluations per segment. */
		if (!edge_split_midpoint(parent_start, parent_end, &midpoint)) {
		    const double start_miss = b->nc->PointAt(parent_start).
			DistanceTo(*b->e_start);
		    const double end_miss = b->nc->PointAt(parent_end).
			DistanceTo(*b->e_end);
		    progress = edge_split_progress(parent_start, parent_end,
			parent_chord, start_miss, end_miss,
			edge.m_tolerance, &midpoint, &residual);
		}
		if (progress < 0) {
		    if (failure_message && failure_message_size)
			std::snprintf(failure_message, failure_message_size,
			    "linear B-Rep edge %d cannot make parameter "
			    "progress; residual %.17g exceeds tolerance %.17g",
			    edge.m_edge_index, residual, edge.m_tolerance);
		    return false;
		}
		std::set<bedge_seg_t *> esegs_split;
		if (progress)
		    esegs_split = split_edge_seg(s_cdt, b, 0, &midpoint, 0);
		if (esegs_split.size()) {
		    ns->insert(esegs_split.begin(), esegs_split.end());
		    if (s_cdt->e2polysegs[edge.m_edge_index].size() >
			    max_segments_per_edge) {
			double min_chord = DBL_MAX;
			double max_chord = 0.0;
			bedge_seg_t *max_segment = NULL;
			for (bedge_seg_t *segment :
				s_cdt->e2polysegs[edge.m_edge_index]) {
			    const double chord = segment->e_start->DistanceTo(
				*segment->e_end);
			    min_chord = std::min(min_chord, chord);
			    if (chord > max_chord) {
				max_chord = chord;
				max_segment = segment;
			    }
			}
			const double max_start_miss = max_segment ?
			    max_segment->nc->PointAt(max_segment->edge_start).
			    DistanceTo(*max_segment->e_start) : DBL_MAX;
			const double max_end_miss = max_segment ?
			    max_segment->nc->PointAt(max_segment->edge_end).
			    DistanceTo(*max_segment->e_end) : DBL_MAX;
			double child_max_chord = 0.0;
			for (bedge_seg_t *segment : esegs_split)
			    child_max_chord = std::max(child_max_chord,
				segment->e_start->DistanceTo(*segment->e_end));
			const double naive_midpoint = 0.5 *
			    (parent_start + parent_end);
			bu_log("linear edge subdivision limit: edge=%d "
			    "type=%d cp_len=%.17g absmin=%.17g segments=%zu "
			    "min_chord=%.17g max_chord=%.17g "
			    "parent_chord=%.17g child_max_chord=%.17g "
			    "max_parameter=[%.17g,%.17g] "
			    "max_endpoint_miss=[%.17g,%.17g] "
			    "parameter=[%.17g,%.17g] midpoint=%.17g "
			    "midpoint_stagnant=%d child_nonreducing=%d\n",
			    edge.m_edge_index, (*esegs_split.begin())->edge_type,
			    (*esegs_split.begin())->cp_len, s_cdt->absmin,
			    s_cdt->e2polysegs[edge.m_edge_index].size(),
			    min_chord, max_chord, parent_chord,
			    child_max_chord,
			    max_segment ? max_segment->edge_start : DBL_MAX,
			    max_segment ? max_segment->edge_end : DBL_MAX,
			    max_start_miss, max_end_miss,
			    parent_start, parent_end, naive_midpoint,
			    naive_midpoint <= parent_start ||
				naive_midpoint >= parent_end,
			    child_max_chord >= parent_chord);
			if (failure_message && failure_message_size)
			    std::snprintf(failure_message,
				failure_message_size,
				"linear B-Rep edge %d reached %zu segments "
				"(limit %zu)", edge.m_edge_index,
				s_cdt->e2polysegs[edge.m_edge_index].size(),
				max_segments_per_edge);
			return false;
		    }
		} else {
		    new_segs.insert(b);
		}
		if (!ws->size() && ns->size()) {
		    std::set<bedge_seg_t *> *tmp = ws;
		    ws = ns;
		    ns = tmp;
		}
	    }
	    s_cdt->e2polysegs[edge.m_edge_index].clear();
	    s_cdt->e2polysegs[edge.m_edge_index] = new_segs;
	}
    }

    return true;
}

static bool
periodic_trim_has_inconsistent_image(const ON_BrepFace &face,
	const ON_Surface &surface, int closed_direction)
{
    const ON_Interval domain = surface.Domain(closed_direction);
    const double period = domain.Length();
    if (!(period > 0.0))
	return false;
    const double scale = std::max(1.0, std::max(std::fabs(domain.Min()),
	std::fabs(domain.Max())));
    const double tolerance = 4096.0 *
	std::numeric_limits<double>::epsilon() * scale;
    const auto seam_side = [&](double parameter) {
	if (std::fabs(parameter - domain.Min()) <= tolerance)
	    return -1;
	if (std::fabs(parameter - domain.Max()) <= tolerance)
	    return 1;
	return 0;
    };

    for (int loop_index = 0; loop_index < face.LoopCount(); ++loop_index) {
	const ON_BrepLoop *loop = face.Loop(loop_index);
	if (!loop)
	    continue;
	for (int trim_index = 0; trim_index < loop->TrimCount();
		++trim_index) {
	    const ON_BrepTrim *trim = loop->Trim(trim_index);
	    if (!trim)
		continue;
	    const ON_Interval trim_domain = trim->Domain();
	    const double first = trim->PointAt(
		trim_domain.Min())[closed_direction];
	    const double last = trim->PointAt(
		trim_domain.Max())[closed_direction];
	    const int first_side = seam_side(first);
	    const int last_side = seam_side(last);
	    if (!first_side || !last_side || first_side == last_side)
		continue;

	    double winding = 0.0;
	    double previous = first;
	    for (int sample = 1; sample <= 16; ++sample) {
		const double current = trim->PointAt(trim_domain.ParameterAt(
		    (double)sample / 16.0))[closed_direction];
		double delta = current - previous;
		delta -= std::nearbyint(delta / period) * period;
		winding += delta;
		previous = current;
	    }
	    const double endpoint_winding = last_side > first_side ?
		period : -period;
	    if (std::fabs(winding) > 0.5 * period &&
		    winding * endpoint_winding < 0.0)
		return true;
	}
    }
    return false;
}

void
refine_close_edges(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;

    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	const bool topology_chart = cdt_face_uses_topology_chart(face);
	const bool singular_face =
	    s_cdt->fmeshes[face_index].has_singularities;
	//std::cout << "Face " << face_index << " of " << brep->m_F.Count()-1 << " close edge check...\n";

	/*
	 * Native surface parameters are not a valid planar proximity metric for
	 * a periodic face when a pcurve traverses one periodic image but stores
	 * endpoints on the opposite seam images.  Its native-UV segments then cross
	 * the rest of the loop and cause unbounded false proximity refinement.  The
	 * topology chart repairs that winding later; until this check operates in
	 * chart coordinates, rely on the curve, chord, and normal refinement.
	 */
	const ON_Surface *surface = face.SurfaceOf();
	const bool cylinder_seam = cdt_face_uses_cylinder_chart(face) &&
	    cdt_face_has_seam(face);
	const int closed_direction = surface && surface->IsClosed(0) ? 0 :
	    (surface && surface->IsClosed(1) ? 1 : -1);
	const bool inconsistent_periodic_image = !cylinder_seam && surface &&
	    closed_direction >= 0 && cdt_face_has_seam(face) &&
	    periodic_trim_has_inconsistent_image(face, *surface,
		closed_direction);
	if (cylinder_seam || inconsistent_periodic_image)
	    continue;

	std::vector<cpolyedge_t *> ws = cdt_face_polyedges(s_cdt, face_index);

	// Check all the edge segments associated with the loop to see if our bounding box overlaps with boxes
	// that aren't our neighbor boxes.  For any that do, split and check again.  Keep refining until we
	// don't have any non-neighbor overlaps.
	int split_cnt = 0;
	while (ws.size() && split_cnt < 10) {
	    std::vector<cpolyedge_t *> current_trims;
	    std::set<int> dirty_rtrees;

	    bool split_check = false;

	    // TODO - with the status determination being recorded in the cpolyedge_t structure
	    // itself, this loop should be (in principle) suitable for bu_parallel - we're not
	    // doing any splitting at this point - searching is a read only activity once
	    // the initial data containers are set up
	    std::vector<cpolyedge_t *>::iterator w_it;
	    for (w_it = ws.begin(); w_it != ws.end(); w_it++) {
		cpolyedge_t *tseg = *w_it;
		ON_2dPoint p2d1(tseg->polygon->pnts_2d[tseg->v2d[0]].first, tseg->polygon->pnts_2d[tseg->v2d[0]].second);
		ON_2dPoint p2d2(tseg->polygon->pnts_2d[tseg->v2d[1]].first, tseg->polygon->pnts_2d[tseg->v2d[1]].second);

		// Trim 2D bbox
		ON_Line line(p2d1, p2d2);
		ON_BoundingBox bb = line.BoundingBox();
		bb.m_max.x = bb.m_max.x + ON_ZERO_TOLERANCE;
		bb.m_max.y = bb.m_max.y + ON_ZERO_TOLERANCE;
		bb.m_min.x = bb.m_min.x - ON_ZERO_TOLERANCE;
		bb.m_min.y = bb.m_min.y - ON_ZERO_TOLERANCE;
		double dist = p2d1.DistanceTo(p2d2);
		double bdist = 0.5*dist;
		double xdist = bb.m_max.x - bb.m_min.x;
		double ydist = bb.m_max.y - bb.m_min.y;
		if (xdist < bdist) {
		    bb.m_min.x = bb.m_min.x - 0.51*bdist;
		    bb.m_max.x = bb.m_max.x + 0.51*bdist;
		}
		if (ydist < bdist) {
		    bb.m_min.y = bb.m_min.y - 0.51*bdist;
		    bb.m_max.y = bb.m_max.y + 0.51*bdist;
		}

		double tMin[2];
		tMin[0] = bb.Min().x;
		tMin[1] = bb.Min().y;
		double tMax[2];
		tMax[0] = bb.Max().x;
		tMax[1] = bb.Max().y;

		//plot_ce_bbox(s_cdt, tseg, "c.p3");

		// Edge context info
		struct rtree_minsplit_context a_context;
		a_context.s_cdt = s_cdt;
		a_context.cseg = tseg;

		// Do the search
		s_cdt->face_rtrees_2d[face.m_face_index].Search(tMin, tMax, MinSplit2dCallback, (void *)&a_context);
	    }

	    // If we need to split, do so.  We need to process as a set,
	    // because an edge split on a closed face may end up removing more
	    // than one cpolyedge_t in ws at the same time.
	    std::set<cpolyedge_t *> ws_s(ws.begin(), ws.end());
	    while (ws_s.size()) {
		cpolyedge_t *pe = *ws_s.begin();
		if (pe->eseg) {
		    bedge_seg_t *b = pe->eseg;
		    // Get both of them in case they're both in ws (closed face)
		    ws_s.erase(b->tseg1);
		    ws_s.erase(b->tseg2);
		    if (pe->split_status == 2) {
			/* This proximity pass improves triangle shape; it does not
			 * define the requested geometric accuracy.  Do not force a
			 * shared edge below the globally digested minimum mesh
			 * dimension, or use this heuristic alone to create more than
			 * approximately 256 spans along one source edge.  Singular
			 * charts are the exception: their pole fans need this bounded
			 * ten-round refinement to avoid collapsed chart cells. */
			if (!singular_face &&
				!close_edge_split_worthwhile(s_cdt, b)) {
			    pe->split_status = 0;
			    continue;
			}
			dirty_rtrees.insert(s_cdt->brep->m_T[
			    b->tseg1->trim_ind].Face()->m_face_index);
			dirty_rtrees.insert(s_cdt->brep->m_T[
			    b->tseg2->trim_ind].Face()->m_face_index);
			std::set<bedge_seg_t *> esegs_split = split_edge_seg(
			    s_cdt, b, 1, NULL, 0);
			if (esegs_split.size()) {
			    split_check = true;
			    // Pick up the new trim segments from the edges for the next iteration.  Only
			    // want the ones associated with the current face.
			    std::set<bedge_seg_t *>::iterator b_it;
			    for (b_it = esegs_split.begin(); b_it != esegs_split.end(); b_it++) {
				bedge_seg_t *bn = *b_it;
				cpolyedge_t *ce = (s_cdt->brep->m_T[bn->tseg1->trim_ind].Face()->m_face_index == face_index) ? bn->tseg1 : bn->tseg2;
				current_trims.push_back(ce);
			    }
			} else {
			    /* Proximity refinement is heuristic.  If either the
			     * master curve or a pullback cannot make representable
			     * progress, retain the current segment and do not request
			     * it again on a later proximity pass. */
			    b->tseg1->split_status = 0;
			    b->tseg2->split_status = 0;
			    current_trims.push_back(pe);
			}
		    } else if (pe->split_status == 1) {
			current_trims.push_back(pe);
		    }
		} else {
		    // Trim only, no edge.
		    ws_s.erase(pe);
		    if (topology_chart) {
			pe->split_status = 0;
			current_trims.push_back(pe);
			continue;
		    }
		    if (pe->split_status == 2) {
			dirty_rtrees.insert(s_cdt->brep->m_T[
			    pe->trim_ind].Face()->m_face_index);
			std::set<cpolyedge_t *> ntrims = split_singular_seg(
			    s_cdt, pe, 0);
			if (ntrims.size()) {
			    std::copy(ntrims.begin(), ntrims.end(), std::back_inserter(current_trims));
			    split_check = true;
			} else {
			    // This is probably fatal...
			    std::cerr << "Forced trim split failed???\n";
			    current_trims.push_back(pe);
			}
		    } else if (pe->split_status == 1) {
			current_trims.push_back(pe);
		    }
		}
	    }

	    ws.clear();

	    split_cnt++;

	    if (split_check) {
		for (int dirty_face : dirty_rtrees) {
		    if (!rebuild_face_rtree_2d(s_cdt, dirty_face, 0)) {
			bu_log("Unable to rebuild face %d close-edge RTree\n",
			    dirty_face);
			return;
		    }
		}
		ws = current_trims;
		for (w_it = ws.begin(); w_it != ws.end(); w_it++) {
		    // We don't want to zero this status information if this is
		    // our last iteration before bailing and we've still got
		    // unresolved inputs - we will want to know about any edges
		    // that are still overlapping with non-neighbors when doing
		    // surface points.
		    if (split_cnt < 10) {
			(*w_it)->split_status = 0;
		    }
		}
	    }
	}
    }
}

void
finalize_rtrees(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;
    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	if (!rebuild_face_rtree_2d(s_cdt, face_index, 1)) {
	    bu_log("Unable to finalize face %d 2D RTree\n", face_index);
	    return;
	}
    }

    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	s_cdt->face_rtrees_3d[face.m_face_index].RemoveAll();
    }
    for (int index = 0; index < brep->m_E.Count(); index++) {
	std::set<bedge_seg_t *> &epsegs = s_cdt->e2polysegs[index];
	std::set<bedge_seg_t *>::iterator e_it;
	for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
	    bedge_seg_t *b = *e_it;
	    rtree_bbox_3d(s_cdt, b->tseg1);
	    rtree_bbox_3d(s_cdt, b->tseg2);
	}
    }
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
