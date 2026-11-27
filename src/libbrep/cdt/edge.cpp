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
#include "bu/str.h"
#include "./chart.h"
#include "./cdt.h"
#include "./surface.h"

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
    auto &tree = s_cdt->face_rtrees_2d[face_index];
    tree.RemoveAll();
    const auto incomplete = [&tree]() {
	/* A broken polygon walk cannot supply a complete spatial index. */
	tree.RemoveAll();
	return false;
    };

    for (int li = 0; li < face.LoopCount(); li++) {
	const ON_BrepLoop *loop = face.Loop(li);
	const bool is_outer = face.OuterLoop()->m_loop_index ==
	    loop->m_loop_index;
	cpolygon_t *cpoly = is_outer ? &fmesh->outer_loop :
	    fmesh->inner_loops[li];
	if (!cpoly || cpoly->poly.empty())
	    return incomplete();

	size_t ecnt = 1;
	cpolyedge_t *first = *cpoly->poly.begin();
	cpolyedge_t *next = first->next;
	rtree_bbox_2d(s_cdt, first, tight);
	while (first != next) {
	    ecnt++;
	    if (!next)
		return incomplete();
	    rtree_bbox_2d(s_cdt, next, tight);
	    next = next->next;
	    if (ecnt > cpoly->poly.size())
		return incomplete();
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
    ON_3dPoint trim_3d = cdt_surface_point(s, trim_2d);

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
	    cdt_surface_normal(s, cp, tmp1, norm);
	}
	if (trim->Face()->m_bRev) {
	    norm = -1 * norm;
	}
	//std::cout << "Face " << trim->Face()->m_face_index << ", Loop " << trim->Loop()->m_loop_index << " norm: " << norm.x << "," << norm.y << "," << norm.z << "\n";
    }
    return norm;
}

int
cdt_test_periodic_edge_normals(void)
{
    ON_Brep brep;
    ON_Torus torus(ON_Circle(ON_xy_plane, 9.0), 2.5);
    std::unique_ptr<ON_NurbsSurface> surface(new ON_NurbsSurface());
    if (!torus.GetNurbForm(*surface))
	return 1;
    ON_BrepFace &face = brep.NewFace(brep.AddSurface(surface.release()));
    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
    const int curve_index = brep.AddTrimCurve(new ON_LineCurve(
	ON_2dPoint(0.0, 0.0), ON_2dPoint(1.0, 1.0)));
    ON_BrepTrim &trim = brep.NewTrim(false, loop, curve_index);
    trim.m_type = ON_BrepTrim::boundary;
    const ON_Surface *source = face.SurfaceOf();
    const ON_2dPoint native(source->Domain(0).ParameterAt(0.375),
	source->Domain(1).ParameterAt(0.125));
    ON_3dPoint expected_point;
    ON_3dVector expected_normal;
    if (!surface_EvNormal(source, native.x, native.y,
	    expected_point, expected_normal))
	return 2;
    for (bool reverse : {false, true}) {
	face.m_bRev = reverse;
	for (int ucopy : {-3, 0, 2}) {
	    for (int vcopy : {-2, 0, 3}) {
		ON_2dPoint uv(native.x + ucopy * source->Domain(0).Length(),
		    native.y + vcopy * source->Domain(1).Length());
		const ON_2dPoint original = uv;
		const ON_3dVector normal = trim_normal(&trim, uv);
		const ON_3dVector expected = reverse ? -expected_normal : expected_normal;
		if (!normal.IsValid() || (normal - expected).Length() > ON_SQRT_EPSILON ||
		    uv != original || cdt_surface_point(source, uv).DistanceTo(expected_point) > BN_TOL_DIST)
		    return 3;
		/* Initial vertex normals use the p-curve endpoints, whereas
		 * splitting evaluates an interior trim parameter.  Cover both. */
		ON_Curve *curve = brep.m_C2[curve_index];
		const ON_2dVector shift = uv - ON_2dPoint(curve->PointAtStart());
		if (!curve->Translate(ON_3dVector(shift.x, shift.y, 0.0)))
		    return 6;
		ON_BrepVertex vertex;
		vertex.SetPoint(expected_point);
		const ON_3dVector vertex_normal = calc_trim_vnorm(vertex, &trim);
		if (!vertex_normal.IsValid() ||
		    (vertex_normal - expected).Length() > ON_SQRT_EPSILON)
		    return 7;
	    }
	}
    }
    ON_3dPoint point;
    ON_3dVector normal;
    if (cdt_surface_normal(NULL, native, point, normal) ||
	cdt_surface_point(source, ON_2dPoint::UnsetPoint).IsValid())
	return 4;
    /* A finite extreme copy must not overflow an integer turn counter. */
    /* OpenNURBS reserves values near DBL_MAX as invalid coordinates. */
    const ON_2dPoint distant(std::numeric_limits<double>::max() / 2.0, native.y);
    const ON_2dPoint evaluated = cdt_surface_uv(source, distant);
    if (!evaluated.IsValid() || !source->Domain(0).Includes(evaluated.x) ||
	!source->Domain(0).Includes(cdt_surface_parameter(
	    std::numeric_limits<double>::max(), source->Domain(0))))
	return 5;
    return 0;
}


/* Callers choose the search coordinates, bracket and evaluation budget. */
template <typename Distance>
static double
golden_section_minimum(double low, double high, int iterations,
	const Distance &distance_squared)
{
    const double golden = 0.5 * (std::sqrt(5.0) - 1.0);
    double left = high - golden * (high - low);
    double right = low + golden * (high - low);
    double left_distance = distance_squared(left);
    double right_distance = distance_squared(right);
    for (int iteration = 0; iteration < iterations; ++iteration) {
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
    return 0.5 * (low + high);
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
	const ON_3dPoint point = cdt_surface_point(surface, uv);
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
	    const int trim_search_iterations = 48;
	    const double refined_fraction = golden_section_minimum(low, high,
		trim_search_iterations, [&](double fraction) {
		    return distance_squared(domain.ParameterAt(fraction));
		});
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
	    const int curve_search_iterations = 64;
	    const double refined = golden_section_minimum(low, high,
		curve_search_iterations, distance_squared);
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

/* Consecutive UVs can range over several turns. Test the lifted path
 * against the shared edge in trim order, allowing independently
 * parameterized curves and rejecting a different periodic branch. */
static bool
trim_curve_matches_edge(const ON_Curve &trim, const ON_Surface &surface,
	const ON_BrepEdge &edge, bool reversed)
{
    for (int end = 0; end < 2; ++end) {
	const ON_BrepVertex *vertex = edge.Vertex(reversed ? 1 - end : end);
	const double t = trim.Domain().ParameterAt(end);
	const ON_3dPoint point = cdt_surface_point(&surface, trim.PointAt(t));
	if (!vertex || !point.IsValid() ||
		point.DistanceTo(vertex->Point()) > BN_TOL_DIST)
	    return false;
    }
    const std::unique_ptr<ON_NurbsCurve> curve(edge.NurbsCurve());
    if (!curve)
	return false;
    const int spans = trim.SpanCount();
    const int max_trim_spans = 4096;
    if (spans < 1 || spans > max_trim_spans)
	return false;
    std::vector<double> breaks((size_t)spans + 1);
    if (!trim.GetSpanVector(breaks.data()))
	return false;
    const ON_Interval trim_domain = trim.Domain();
    const ON_Interval domain = curve->Domain();
    ON_Interval remaining = domain;
    const int samples_per_span = 4;
    /* Dense imported p-curves can have hundreds of spans on a single
     * edge, even when their edge parameters already agree. Allow those
     * direct checks without multiplying the closest-point search budget. */
    const int max_closest_point_searches = 1024;
    int closest_point_searches = 0;
    for (int span = 0; span < spans; ++span) {
	const ON_Interval interval(breaks[span], breaks[span + 1]);
	if (!interval.IsIncreasing())
	    return false;
	for (int sample = 1; sample <= samples_per_span; ++sample) {
	    if (span == spans - 1 && sample == samples_per_span)
		break;
	    const double t = interval.ParameterAt(
		(double)sample / samples_per_span);
	    const ON_3dPoint point = cdt_surface_point(&surface, trim.PointAt(t));
	    if (!point.IsValid())
		return false;
	    const double fraction = trim_domain.NormalizedParameterAt(t);
	    double parameter = domain.ParameterAt(reversed ?
		1.0 - fraction : fraction);
	    const ON_3dPoint guess = curve->PointAt(parameter);
	    const bool matches = guess.IsValid() &&
		split_parameter_interior(remaining[0], remaining[1], parameter) &&
		guess.DistanceTo(point) <= BN_TOL_DIST;
	    if (!matches && (++closest_point_searches > max_closest_point_searches ||
		    !curve_interior_point_parameter_impl(&parameter, curve.get(),
			remaining, point, BN_TOL_DIST, false)))
		return false;
	    remaining[reversed ? 1 : 0] = parameter;
	}
    }
    return true;
}

int
cdt_test_many_trim_spans(void)
{
    const double edge_length = 10.0;
    ON_Brep fixture;
    const ON_3dPoint first(0.0, 0.0, 0.0);
    const ON_3dPoint last(edge_length, 0.0, 0.0);
    fixture.NewVertex(first, 0.0);
    fixture.NewVertex(last, 0.0);
    const ON_Interval domain(0.0, edge_length);
    const int curve = fixture.AddEdgeCurve(new ON_LineCurve(ON_Line(first, last),
	domain.Min(), domain.Max()));
    const ON_BrepEdge &edge = fixture.NewEdge(fixture.m_V[0], fixture.m_V[1],
	curve, nullptr, 0.0);
    const ON_PlaneSurface surface(ON_xy_plane);
    const int spans = 704;
    ON_PolylineCurve trim;
    for (int i = 0; i <= spans; ++i) {
	const double parameter = domain.ParameterAt((double)i / spans);
	trim.m_pline.Append(ON_3dPoint(parameter, 0.0, 0.0));
	trim.m_t.Append(parameter);
    }
    if (!trim.ChangeDimension(2) ||
	    !trim_curve_matches_edge(trim, surface, edge, false))
	return 1;

    /* A bad span late in the trim still invalidates the correspondence. */
    trim.m_pline[spans - 2].y = 2.0 * BN_TOL_DIST;
    return trim_curve_matches_edge(trim, surface, edge, false) ? 2 : 0;
}

/* A straight two-row control net with equal paired weights preserves the
 * source axial parameter exactly. Other cylinders may twist or alter
 * their axial speed and cannot use a line with this parameter domain. */
static bool
cylinder_axial_displacement(ON_3dVector &displacement,
	const ON_NurbsSurface &surface, int axial, const ON_3dVector &axis)
{
    for (int i = 0; i < surface.CVCount(1 - axial); ++i) {
	ON_3dPoint lower, upper;
	ON_4dPoint lower_weight, upper_weight;
	const int low_u = axial ? i : 0;
	const int low_v = axial ? 0 : i;
	const int high_u = axial ? i : 1;
	const int high_v = axial ? 1 : i;
	if (!surface.GetCV(low_u, low_v, lower) ||
		!surface.GetCV(high_u, high_v, upper) ||
		!surface.GetCV(low_u, low_v, lower_weight) ||
		!surface.GetCV(high_u, high_v, upper_weight) ||
		!NEAR_EQUAL(lower_weight.w, upper_weight.w, ON_ZERO_TOLERANCE))
	    return false;
	const ON_3dVector delta = upper - lower;
	if (!i)
	    displacement = (delta * axis) * axis;
	if ((delta - displacement).Length() > ON_ZERO_TOLERANCE)
	    return false;
    }
    return displacement.Length() > ON_ZERO_TOLERANCE;
}

static bool
trim_endpoints_match_surface(const ON_BrepTrim &trim, const ON_Surface *surface)
{
    for (int end = 0; end < 2; ++end) {
	const double t = trim.Domain().ParameterAt(end);
	const ON_3dPoint point = cdt_surface_point(surface, trim.PointAt(t));
	if (!point.IsValid() ||
		point.DistanceTo(trim.Vertex(end)->Point()) > BN_TOL_DIST)
	    return false;
    }
    return true;
}

static std::unique_ptr<ON_RevSurface>
radian_cylinder_surface(const ON_BrepFace &face)
{
    const ON_NurbsSurface *source = ON_NurbsSurface::Cast(face.SurfaceOf());
    if (!source)
	return nullptr;
    const bool u_closed = source->IsClosed(0);
    if (u_closed == source->IsClosed(1))
	return nullptr;
    const int angular = u_closed ? 0 : 1;
    const int axial = 1 - angular;
    const ON_Interval u = source->Domain(angular);
    const ON_Interval v = source->Domain(axial);
    const double period = 2.0 * ON_PI;
    if (!u.IsIncreasing() || !v.IsIncreasing() ||
	    u.Length() <= period + cdt_surface_parameter_tolerance(u) ||
	    source->Degree(axial) != 1 || source->CVCount(axial) != 2 ||
	    cdt_face_has_seam(face))
	return nullptr;

    std::vector<const ON_BrepTrim *> trims;
    bool source_matches = true;
    const double axial_roundoff = cdt_surface_parameter_tolerance(v);
    for (int li = 0; li < face.LoopCount(); ++li) {
	const ON_BrepLoop *loop = face.Loop(li);
	if (!loop)
	    return nullptr;
	for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	    const ON_BrepTrim *trim = loop->Trim(ti);
	    if (!trim || !trim->Edge() || !trim->Vertex(0) || !trim->Vertex(1))
		return nullptr;
	    ON_BoundingBox bounds;
	    if (!trim->GetBoundingBox(bounds) || !bounds.IsValid() ||
		    bounds.Min()[axial] < v.Min() - axial_roundoff ||
		    bounds.Max()[axial] > v.Max() + axial_roundoff)
		return nullptr;
	    trims.push_back(trim);
	    source_matches = source_matches &&
		trim_endpoints_match_surface(*trim, source);
	}
    }
    if (source_matches)
	return nullptr;

    ON_Cylinder cylinder;
    if (!source->IsCylinder(&cylinder, ON_ZERO_TOLERANCE) ||
	    !cylinder.IsValid())
	return nullptr;
    ON_3dVector axis = cylinder.circle.plane.zaxis;
    const ON_3dPoint center = cylinder.circle.Center();
    ON_3dPoint point;
    ON_3dVector du, dv;
    const double parameters[2] = {angular ? v.Min() : u.Mid(),
	angular ? u.Mid() : v.Min()};
    if (!source->Ev1Der(parameters[0], parameters[1], point, du, dv))
	return nullptr;
    const ON_3dVector derivative = angular ? dv : du;
    if (derivative * ON_CrossProduct(axis, point - center) < 0.0)
	axis = -axis;
    ON_3dVector displacement;
    if (!cylinder_axial_displacement(displacement, *source, axial, axis))
	return nullptr;

    /* Recover the phase from one topological endpoint. The remaining
     * endpoints and the shared edge shapes must independently agree. */
    const ON_BrepTrim &anchor = *trims.front();
    ON_3dVector radial = anchor.Vertex(0)->Point() - center;
    radial -= (radial * axis) * axis;
    if (!radial.Unitize())
	return nullptr;
    const double phase = std::remainder(u.Min() - anchor.PointAtStart()[angular],
	period);
    const ON_3dPoint start = center + ((point - center) * axis) * axis +
	cylinder.circle.radius * (std::cos(phase) * radial +
	    std::sin(phase) * ON_CrossProduct(axis, radial));
    /* Use one evaluation period. The original trim loops retain their
     * lifted coordinates, including multiple revolutions. */
    auto result = std::make_unique<ON_RevSurface>();
    result->m_curve = new ON_LineCurve(start, start + displacement);
    result->m_axis = ON_Line(center, center + axis);
    result->m_bTransposed = angular != 0;
    if (!result->m_curve->SetDomain(v.Min(), v.Max()) ||
	    !result->SetAngleRadians(0.0, period) ||
	    !result->SetDomain(angular, u.Min(), u.Min() + period) ||
	    !result->IsValid())
	return nullptr;

    for (const ON_BrepTrim *trim : trims) {
	if (!trim_curve_matches_edge(*trim, *result, *trim->Edge(),
		trim->m_bRev3d))
	    return nullptr;
    }
    return result;
}

bool
restore_cylinder_trim_parameters(ON_Brep &brep, ON_BrepFace &face)
{
    std::unique_ptr<ON_RevSurface> replacement = radian_cylinder_surface(face);
    if (!replacement)
	return false;
    const int surface_index = brep.AddSurface(replacement.get());
    if (surface_index < 0)
	return false;
    face.m_si = surface_index;
    face.SetProxySurface(replacement.release());
    return true;
}

/* A collapsed full-turn carrier can still have a cylindrical neighbor
 * across its complete circular boundary. That neighbor fixes the support;
 * the two uses of the straight seam fix the axial range and trim phase. */
static bool
collapsed_to_seam(const ON_NurbsSurface &source, int angular,
	const ON_3dPoint (&endpoints)[2])
{
    const int axial = 1 - angular;
    if (source.Degree(axial) != 1 || source.CVCount(axial) != 2)
	return false;
    for (int i = 0; i < source.CVCount(angular); ++i) {
	ON_4dPoint row[2];
	for (int end = 0; end < 2; ++end) {
	    if (!source.GetCV(angular ? end : i, angular ? i : end, row[end]) ||
		    !(row[end].w > 0.0))
		return false;
	    const ON_3dPoint p(row[end]);
	    if (!p.IsValid() || p.DistanceTo(endpoints[end]) > BN_TOL_DIST)
		return false;
	}
	if (!NEAR_EQUAL(row[0].w, row[1].w, ON_ZERO_TOLERANCE))
	    return false;
    }
    return true;
}

static bool
seam_spans_domain(const ON_BrepTrim &trim, int angular,
	double boundary, const ON_Interval &axial)
{
    const auto first = trim.PointAtStart();
    const auto last = trim.PointAtEnd();
    /* Importers can extend the carrier slightly past the seam's endpoint.
     * The unchanged 3D edge and all replacement lifts are checked below. */
    const double axial_roundoff =
	std::sqrt(std::numeric_limits<double>::epsilon()) * axial.Length();
    return NEAR_EQUAL(first[angular], boundary, ON_ZERO_TOLERANCE) &&
	NEAR_EQUAL(last[angular], boundary, ON_ZERO_TOLERANCE) &&
	NEAR_EQUAL(std::min(first[1 - angular], last[1 - angular]),
	    axial.Min(), axial_roundoff) &&
	NEAR_EQUAL(std::max(first[1 - angular], last[1 - angular]),
	    axial.Max(), axial_roundoff) &&
	trim.IsLinear(ON_ZERO_TOLERANCE);
}

static std::unique_ptr<ON_RevSurface>
continued_cylinder(const ON_BrepFace &face, const ON_BrepTrim &rim,
	int angular, const ON_3dPoint (&ends)[2])
{
    const auto &source = *face.SurfaceOf();
    const auto u = source.Domain(angular);
    const auto v = source.Domain(1 - angular);
    const auto first = rim.PointAtStart();
    const auto last = rim.PointAtEnd();
    const auto *edge = rim.Edge();
    const double tol = cdt_surface_parameter_tolerance(u);
    if (!edge || !edge->IsClosed() || edge->TrimCount() != 2 ||
	    !rim.IsLinear(ON_ZERO_TOLERANCE) ||
	    !NEAR_EQUAL(std::min(first[angular], last[angular]), u.Min(), tol) ||
	    !NEAR_EQUAL(std::max(first[angular], last[angular]), u.Max(), tol) ||
	    !NEAR_EQUAL(first[1 - angular], last[1 - angular], ON_ZERO_TOLERANCE) ||
	    !(NEAR_EQUAL(first[1 - angular], v.Min(), ON_ZERO_TOLERANCE) ||
	    NEAR_EQUAL(first[1 - angular], v.Max(), ON_ZERO_TOLERANCE)))
	return nullptr;
    const ON_BrepTrim *other = edge->Trim(edge->Trim(0) == &rim ? 1 : 0);
    const ON_BrepFace *neighbor = other ? other->Face() : nullptr;
    const ON_Surface *mate = neighbor && neighbor != &face ?
	neighbor->SurfaceOf() : nullptr;
    ON_Cylinder cylinder;
    ON_Arc circle;
    const double period = 2.0 * ON_PI;
    if (!mate || !mate->IsCylinder(&cylinder, ON_ZERO_TOLERANCE) ||
	    !cylinder.IsValid() || !edge->IsArc(nullptr, &circle, BN_TOL_DIST) ||
	    !circle.IsCircle() ||
	    !NEAR_EQUAL(circle.Radius(), cylinder.circle.Radius(), BN_TOL_DIST) ||
	    std::fabs(u.Length() - period) * cylinder.circle.Radius() > BN_TOL_DIST)
	return nullptr;

    const auto center = cylinder.circle.Center();
    const auto direction = cylinder.circle.plane.zaxis;
    const ON_Line axis(center, center + direction);
    const double radius = cylinder.circle.Radius();
    const auto axis_start = axis.ClosestPointTo(ends[0]);
    const auto axis_end = axis.ClosestPointTo(ends[1]);
    auto radial = ends[0] - axis_start;
    if (axis.ClosestPointTo(circle.Center()).DistanceTo(circle.Center()) >
	    BN_TOL_DIST ||
	    ON_CrossProduct(direction, circle.plane.zaxis).Length() * radius >
	    BN_TOL_DIST ||
	    !NEAR_EQUAL(radial.Length(), radius, BN_TOL_DIST) ||
	    (ends[1] - axis_end - radial).Length() > BN_TOL_DIST ||
	    !radial.Unitize())
	return nullptr;

    for (int orientation : {1, -1}) {
	auto candidate = std::make_unique<ON_RevSurface>();
	candidate->m_curve = new ON_LineCurve(axis_start + radius * radial,
	    axis_end + radius * radial);
	candidate->m_axis = ON_Line(center, center + orientation * direction);
	candidate->m_bTransposed = angular != 0;
	if (!candidate->m_curve->SetDomain(v.Min(), v.Max()) ||
		!candidate->SetAngleRadians(0.0, period) ||
		!candidate->SetDomain(angular, u.Min(), u.Max()) ||
		!candidate->IsValid())
	    continue;
	bool matches = true;
	for (int li = 0; li < face.LoopCount() && matches; ++li) {
	    const ON_BrepLoop *loop = face.Loop(li);
	    if (!loop) {
		matches = false;
		break;
	    }
	    for (int ti = 0; ti < loop->TrimCount(); ++ti) {
		const ON_BrepTrim *trim = loop->Trim(ti);
		if (!trim || !trim->Edge() || !trim_curve_matches_edge(*trim,
			*candidate, *trim->Edge(), trim->m_bRev3d)) {
		    matches = false;
		    break;
		}
	    }
	}
	if (matches)
	    return candidate;
    }
    return nullptr;
}

bool
restore_collapsed_cylinder_support(ON_Brep &brep, ON_BrepFace &face)
{
    const auto *source = ON_NurbsSurface::Cast(face.SurfaceOf());
    const auto *loop = face.OuterLoop();
    if (!source || !loop ||
	    source->IsClosed(0) || source->IsClosed(1))
	return false;
    for (int i = 0; i < loop->TrimCount(); ++i) {
	const auto *primary = loop->Trim(i);
	const auto *edge = primary ? primary->Edge() : nullptr;
	if (!edge || primary->m_type != ON_BrepTrim::seam ||
		edge->TrimCount() != 2 || !edge->IsLinear(BN_TOL_DIST) ||
		!primary->Vertex(0) || !primary->Vertex(1))
	    continue;
	const auto *other = edge->Trim(edge->Trim(0) == primary ? 1 : 0);
	if (!other || other->Face() != &face)
	    continue;
	for (int angular = 0; angular < 2; ++angular) {
	    const auto u = source->Domain(angular);
	    const auto v = source->Domain(1 - angular);
	    if (!seam_spans_domain(*primary, angular, u.Min(), v) ||
		    !seam_spans_domain(*other, angular, u.Max(), v))
		continue;
	    const bool forward = primary->PointAtStart()[1 - angular] <
		primary->PointAtEnd()[1 - angular];
	    const ON_3dPoint ends[2] = {primary->Vertex(forward ? 0 : 1)->Point(),
		primary->Vertex(forward ? 1 : 0)->Point()};
	    if (!collapsed_to_seam(*source, angular, ends))
		continue;
	    for (int j = 0; j < loop->TrimCount(); ++j) {
		const auto *rim = loop->Trim(j);
		if (!rim)
		    continue;
		auto support = continued_cylinder(face, *rim, angular, ends);
		if (!support)
		    continue;
		const int index = brep.AddSurface(support.get());
		if (index < 0)
		    return false;
		face.m_si = index;
		face.SetProxySurface(support.release());
		return true;
	    }
	}
    }
    return false;
}

int
cdt_test_collapsed_cylinder_support(void)
{
    const double radius = 3.0;
    const double height = 2.0;
    const ON_Cylinder cylinder(ON_Circle(ON_xy_plane, radius), height);
    std::unique_ptr<ON_RevSurface> lower(cylinder.RevSurfaceForm());
    std::unique_ptr<ON_RevSurface> upper(cylinder.RevSurfaceForm());
    ON_Brep fixture;
    if (!lower || !upper ||
	    !upper->Transform(ON_Xform::TranslationTransformation(0.0, 0.0,
		height)) || !fixture.NewFace(*lower))
	return 1;

    const auto *ring = fixture.m_F[0].OuterLoop()->Trim(2);
    if (!ring || !ring->Edge() || !ring->Edge()->IsClosed())
	return 2;
    const int vertex = ring->m_vi[0];
    int vertices[] = {vertex, vertex, -1, -1};
    int edges[] = {ring->m_ei, -1, -1, -1};
    bool reverse[] = {!ring->m_bRev3d, false, false, false};
    ON_BrepFace *damaged = fixture.NewFace(upper.get(), vertices, edges,
	reverse);
    if (!damaged)
	return 3;
    upper.release();
    const auto *correct = damaged->SurfaceOf();
    const ON_Interval angular = correct->Domain(0);
    const ON_Interval axial = correct->Domain(1);
    const ON_3dPoint low = correct->PointAt(angular.Min(), axial.Min());
    const ON_3dPoint high = correct->PointAt(angular.Min(), axial.Max());
    const double test_angle = angular.ParameterAt(0.25);
    const auto expected = correct->PointAt(test_angle, axial.Mid());
    if (restore_collapsed_cylinder_support(fixture, *damaged))
	return 4;

    auto collapsed = std::make_unique<ON_NurbsSurface>(3, false, 2, 2, 2, 2);
    const ON_3dVector offset(0.0, BN_TOL_DIST / 32.0, 0.0);
    if (!collapsed->MakeClampedUniformKnotVector(0) ||
	    !collapsed->MakeClampedUniformKnotVector(1) ||
	    !collapsed->SetDomain(0, angular.Min(), angular.Max()) ||
	    !collapsed->SetDomain(1, axial.Min(), axial.Max()) ||
	    !collapsed->SetCV(0, 0, low) || !collapsed->SetCV(0, 1, high) ||
	    !collapsed->SetCV(1, 0, low + offset) ||
	    !collapsed->SetCV(1, 1, high + offset))
	return 4;
    const int broken_surface = fixture.AddSurface(collapsed.get());
    if (broken_surface < 0)
	return 5;
    damaged->m_si = broken_surface;
    damaged->SetProxySurface(collapsed.release());
    fixture.SetTolerancesBoxesAndFlags();
    for (int i = 0; i < fixture.m_E.Count(); ++i)
	fixture.m_E[i].m_tolerance = BN_TOL_DIST;
    for (int i = 0; i < fixture.m_V.Count(); ++i)
	fixture.m_V[i].m_tolerance = BN_TOL_DIST;
    if (!fixture.IsValid())
	return 6;
    const ON__UINT32 crc = fixture.DataCRC(0);
    ON_Brep working(fixture);
    if (!restore_collapsed_cylinder_support(working, working.m_F[1]) ||
	    fixture.DataCRC(0) != crc)
	return 6;
    const auto *recovered = working.m_F[1].SurfaceOf();
    if (!recovered->IsCylinder(nullptr, ON_ZERO_TOLERANCE) ||
	    recovered->PointAt(test_angle, axial.Mid()).DistanceTo(expected) >
	    BN_TOL_DIST)
	return 7;
    return 0;
}

struct rim_trim_update {
    ON_BrepTrim *trim;
    std::unique_ptr<ON_LineCurve> curve;
};

/* Sliding junctions along a complete, straight UV rim preserves the
 * trimming region. Require every revised subarc to agree with its own
 * shared edge before installing any new curve. */
static std::vector<rim_trim_update>
periodic_rim_updates(ON_BrepLoop &loop, const ON_Surface &surface,
	int start_trim, int angular, const ON_Interval &domain)
{
    const int axial = 1 - angular;
    const auto first_uv = loop.Trim(start_trim)->PointAtStart();
    const double start = first_uv[angular];
    const double height = first_uv[axial];
    const double parameter_tol = cdt_surface_parameter_tolerance(domain);
    const auto height_domain = surface.Domain(axial);
    const double height_tol = cdt_surface_parameter_tolerance(height_domain);
    const bool forward = NEAR_EQUAL(start, domain.Min(), parameter_tol);
    if ((!forward && !NEAR_EQUAL(start, domain.Max(), parameter_tol)) ||
	    (!NEAR_EQUAL(height, height_domain.Min(), height_tol) &&
	    !NEAR_EQUAL(height, height_domain.Max(), height_tol)))
	return {};
    const double finish = forward ? domain.Max() : domain.Min();

    std::vector<ON_BrepTrim *> trims;
    std::vector<double> division{start};
    for (int step = 0; step < loop.TrimCount(); ++step) {
	ON_BrepTrim *trim = loop.Trim((start_trim + step) % loop.TrimCount());
	if (!trim || !trim->Edge() || !trim->Vertex(0) || !trim->Vertex(1) ||
		!trim->IsLinear(ON_ZERO_TOLERANCE))
	    return {};
	const auto first = trim->PointAtStart();
	const auto last = trim->PointAtEnd();
	if (!NEAR_EQUAL(first[axial], height, height_tol) ||
		!NEAR_EQUAL(last[axial], height, height_tol) ||
		!NEAR_EQUAL(first[angular], division.back(), parameter_tol) ||
		(forward ? !(last[angular] > first[angular]) :
		    !(last[angular] < first[angular])))
	    return {};
	trims.push_back(trim);
	division.push_back(last[angular]);
	if (NEAR_EQUAL(last[angular], finish, parameter_tol))
	    break;
    }
    if (trims.size() < 2 || !NEAR_EQUAL(division.back(), finish, parameter_tol) ||
	    trims.front()->Vertex(0) != trims.back()->Vertex(1))
	return {};

    if (std::all_of(trims.begin(), trims.end(), [&](const ON_BrepTrim *trim) {
		return trim_endpoints_match_surface(*trim, &surface);
	    }))
	return {};

    const std::unique_ptr<ON_Curve> iso(surface.IsoCurve(angular, height));
    const auto *rim = iso ? ON_NurbsCurve::Cast(iso.get()) : nullptr;
    if (!rim || !NEAR_EQUAL(rim->Domain().Min(), domain.Min(), parameter_tol) ||
	    !NEAR_EQUAL(rim->Domain().Max(), domain.Max(), parameter_tol))
	return {};
    /* Disjoint search intervals keep neighboring junctions ordered. */
    const auto original = division;
    for (size_t i = 1; i + 1 < division.size(); ++i) {
	const ON_BrepVertex *vertex = trims[i]->Vertex(0);
	if (vertex != trims[i - 1]->Vertex(1))
	    return {};
	const ON_3dPoint point = vertex->Point();
	if (rim->PointAt(original[i]).DistanceTo(point) <= BN_TOL_DIST)
	    continue;
	const double left = 0.5 * (original[i - 1] + original[i]);
	const double right = 0.5 * (original[i] + original[i + 1]);
	const ON_Interval interval(std::min(left, right), std::max(left, right));
	if (!curve_interior_point_parameter_impl(&division[i], rim, interval,
		point, BN_TOL_DIST, false))
	    return {};
    }

    std::vector<rim_trim_update> updates;
    for (size_t i = 0; i < trims.size(); ++i) {
	ON_2dPoint first, last;
	first[axial] = last[axial] = height;
	first[angular] = division[i];
	last[angular] = division[i + 1];
	auto replacement = std::make_unique<ON_LineCurve>(first, last);
	const ON_BrepTrim &trim = *trims[i];
	const ON_Interval tr = trim.Domain();
	/* Both adjacent segments must agree with their own master edge.
	 * Moving the shared subdivision never changes the rim as a set. */
	if (!replacement->SetDomain(tr.Min(), tr.Max()) ||
		!trim_curve_matches_edge(*replacement, surface, *trim.Edge(),
		    trim.m_bRev3d))
	    return {};
	updates.push_back({trims[i], std::move(replacement)});
    }
    return updates;
}

bool
restore_periodic_rim_trims(ON_Brep &brep, ON_BrepFace &face)
{
    const ON_NurbsSurface *surface = ON_NurbsSurface::Cast(face.SurfaceOf());
    if (!surface)
	return true;
    const bool u_closed = surface->IsClosed(0);
    if (u_closed == surface->IsClosed(1) || !cdt_face_has_seam(face))
	return true;
    const int angular = u_closed ? 0 : 1;
    const ON_Interval domain = surface->Domain(angular);
    for (int li = 0; li < face.LoopCount(); ++li) {
	ON_BrepLoop *loop = face.Loop(li);
	if (!loop)
	    continue;
	bool changed = false;
	for (int ti = 0; ti < loop->TrimCount(); ++ti) {
	    if (!loop->Trim(ti))
		continue;
	    auto updates = periodic_rim_updates(*loop, *surface, ti,
		angular, domain);
	    for (auto &update : updates) {
		const int curve = brep.AddTrimCurve(update.curve.get());
		if (curve < 0)
		    return false;
		update.curve.release();
		if (!update.trim->ChangeTrimCurve(curve))
		    return false;
		changed = true;
	    }
	}
	if (changed && !brep.SetTrimBoundingBoxes(*loop))
	    return false;
    }
    return true;
}

int
cdt_test_periodic_rim_conditioning(void)
{
    const double radius = 3.0;
    const double height = 2.0;
    const ON_Cylinder cylinder(ON_Circle(ON_xy_plane, radius), height);
    auto surface = std::make_unique<ON_NurbsSurface>();
    if (cylinder.GetNurbForm(*surface) != 2)
	return 1;

    ON_Brep source;
    ON_NurbsSurface *carrier = surface.get();
    const int surface_index = source.AddSurface(carrier);
    if (surface_index < 0)
	return 1;
    surface.release();
    ON_BrepFace &face = source.NewFace(surface_index);
    ON_BrepLoop &loop = source.NewLoop(ON_BrepLoop::outer, face);
    const ON_Interval u = carrier->Domain(0);
    const ON_Interval v = carrier->Domain(1);
    const double division = u.ParameterAt(0.36);
    const double misplaced = u.ParameterAt(0.39);
    const ON_2dPoint corner[] = {
	ON_2dPoint(u.Min(), v.Min()), ON_2dPoint(division, v.Min()),
	ON_2dPoint(u.Max(), v.Min()), ON_2dPoint(u.Max(), v.Max()),
	ON_2dPoint(u.Min(), v.Max())
    };
    int vertex[] = {
	source.NewVertex(carrier->PointAt(corner[0].x, corner[0].y), 0.0).m_vertex_index,
	source.NewVertex(carrier->PointAt(corner[1].x, corner[1].y), 0.0).m_vertex_index,
	-1,
	source.NewVertex(carrier->PointAt(corner[3].x, corner[3].y), 0.0).m_vertex_index,
	-1
    };
    vertex[2] = vertex[0];
    vertex[4] = vertex[3];

    int seam = -1;
    for (int i = 0; i < 5; ++i) {
	const int next = (i + 1) % 5;
	const bool along_rim = i != 2 && i != 4;
	if (i != 4) {
	    std::unique_ptr<ON_Curve> edge(carrier->IsoCurve(along_rim ? 0 : 1,
		along_rim ? corner[i].y : corner[i].x));
	    const int direction = along_rim ? 0 : 1;
	    const double first = corner[i][direction];
	    const double last = corner[next][direction];
	    if (!edge || !edge->Trim(ON_Interval(std::min(first, last),
		    std::max(first, last))) || (first > last && !edge->Reverse()))
		return 2;
	    source.NewEdge(source.m_V[vertex[i]], source.m_V[vertex[next]],
		source.AddEdgeCurve(edge.release()), nullptr, 2.0 * radius);
	}
	if (i == 2)
	    seam = source.m_E.Count() - 1;
	const int edge_index = i == 4 ? seam : source.m_E.Count() - 1;
	ON_2dPoint start = corner[i];
	ON_2dPoint end = corner[next];
	if (i == 0)
	    end.x = misplaced;
	if (i == 1)
	    start.x = misplaced;
	ON_BrepTrim &trim = source.NewTrim(source.m_E[edge_index], i == 4,
	    loop, source.AddTrimCurve(new ON_LineCurve(start, end)));
	trim.m_type = i == 2 || i == 4 ? ON_BrepTrim::seam :
	    ON_BrepTrim::boundary;
	trim.m_tolerance[0] = trim.m_tolerance[1] = 0.0;
    }
    source.SetTrimIsoFlags();
    source.SetTrimBoundingBoxes(loop);
    if (!source.IsValid())
	return 3;

    const ON__UINT32 crc = source.DataCRC(0);
    ON_Brep working(source);
    if (!restore_periodic_rim_trims(working, working.m_F[0]) ||
	    !working.IsValid() || source.DataCRC(0) != crc)
	return 4;
    const auto &first = working.m_T[0];
    const auto &second = working.m_T[1];
    if (!trim_endpoints_match_surface(first, carrier) ||
	    !trim_endpoints_match_surface(second, carrier) ||
	    !NEAR_EQUAL(first.PointAtEnd().x, second.PointAtStart().x,
		ON_ZERO_TOLERANCE))
	return 5;
    if (first.PointAtStart().DistanceTo(corner[0]) > ON_ZERO_TOLERANCE ||
	    second.PointAtEnd().DistanceTo(corner[2]) > ON_ZERO_TOLERANCE)
	return 6;
    return 0;
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
	    max_allowed = 5.0 * s_len;
	    min_allowed = 0.2 * s_len;
	    if (s_cdt->tol.absmax <= ON_ZERO_TOLERANCE) {
		/* The neighboring trim sets a triangle sizing preference.
		 * Bound how far that preference can subdivide a straight
		 * edge. Adjacent normals still control geometric refinement. */
		double sizing_floor = 0.0;
		if (shape_refinement_spacing(&sizing_floor, s_cdt->absmin,
			bseg->cp_len))
		    max_allowed = std::max(max_allowed, sizing_floor);
	    }
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
	    /* Only segments incident to an original endpoint have a
	     * neighbor-derived spacing. Applying its floor to an interior
	     * segment would turn that floor into a new subdivision target. */
	    if (s_len > 0.0) {
		if (!edge_spacing_floor(&s_len, s_cdt->absmin, local_min))
		    return false;
		max_allowed = 2 * s_len;
		min_allowed = 0.5 * s_len;
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

struct edge_split_candidate {
    double edge_parameter = 0.0;
    ON_3dPoint point = ON_3dPoint::UnsetPoint;
    ON_3dVector tangent = ON_3dVector::UnsetVector;
    fastf_t trim_parameters[2];
    ON_2dPoint trim_points[2];
    double approximation_miss = -1.0;
    double quarantine_misses[2] = {0.0, 0.0};
};


static bool
select_edge_split(edge_split_candidate &candidate,
	struct ON_Brep_CDT_State *s_cdt, bedge_seg_t *bseg, int force,
	const double *t, const ON_3dPoint *shared_point,
	bool required_closed_split)
{
    ON_BrepEdge& edge = s_cdt->brep->m_E[bseg->edge_ind];
    ON_BrepTrim *trim1 = &s_cdt->brep->m_T[bseg->tseg1->trim_ind];
    ON_BrepTrim *trim2 = &s_cdt->brep->m_T[bseg->tseg2->trim_ind];

    ON_BrepFace *face1 = trim1->Face();
    ON_BrepFace *face2 = trim2->Face();


    // Get the 3D midpoint (and tangent, if we can) from the edge curve
    double midpoint = 0.0;
    if (t) {
	midpoint = *t;
	const double lower = std::min(bseg->edge_start, bseg->edge_end);
	const double upper = std::max(bseg->edge_start, bseg->edge_end);
	if (!std::isfinite(midpoint) || !(midpoint > lower && midpoint < upper))
	    return false;
    } else if (!edge_split_midpoint(bseg->edge_start, bseg->edge_end,
	    &midpoint)) {
	return false;
    }
    candidate.edge_parameter = midpoint;
    bool evtangent_status = bseg->nc->EvTangent(candidate.edge_parameter,
	candidate.point, candidate.tangent);
    if (!evtangent_status) {
	// EvTangent call failed, get 3d point
	candidate.point = bseg->nc->PointAt(candidate.edge_parameter);
	candidate.tangent = ON_3dVector::UnsetVector;
    }
    if (!split_point_progress(*bseg->e_start, candidate.point,
	    *bseg->e_end))
	return false;

    // Unless we're forcing a split this is the point at which we do tolerance
    // based testing to determine whether to proceed with the split or halt.
    if (!force && !tol_need_split(s_cdt, bseg, candidate.point)) {
	return false;
    }

    // Find the 2D points
    double elen1 = bseg->nc->PointAt(bseg->edge_start).DistanceTo(
	bseg->nc->PointAt(candidate.edge_parameter));
    double elen2 = bseg->nc->PointAt(candidate.edge_parameter).DistanceTo(
	bseg->nc->PointAt(bseg->edge_end));
    double elen = (elen1 + elen2) * 0.5;
    const ON_3dPoint &trim_target_3d = shared_point ? *shared_point :
	candidate.point;
    candidate.trim_points[0] = get_trim_midpt(&candidate.trim_parameters[0],
	s_cdt, bseg->tseg1, trim_target_3d, elen, edge.m_tolerance);
    candidate.trim_points[1] = get_trim_midpt(&candidate.trim_parameters[1],
	s_cdt, bseg->tseg2, trim_target_3d, elen, edge.m_tolerance);

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
	    bseg->tseg1->trim_end, candidate.trim_parameters[0]) &&
	    split_parameter_interior(bseg->tseg2->trim_start,
	    bseg->tseg2->trim_end, candidate.trim_parameters[1]) &&
	    split_point_progress(trim1_start, candidate.trim_points[0], trim1_end) &&
	    split_point_progress(trim2_start, candidate.trim_points[1], trim2_end);

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
	    cdt_surface_point(surface1, fallback_uv1) :
	    ON_3dPoint::UnsetPoint;
	const ON_3dPoint fallback_point2 = surface2 && fallback_uv2.IsValid() ?
	    cdt_surface_point(surface2, fallback_uv2) :
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
	    fallback_point1.DistanceTo(candidate.point) : DBL_MAX;
	const double fallback_edge_miss2 = fallback_point2.IsValid() ?
	    fallback_point2.DistanceTo(candidate.point) : DBL_MAX;
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
		fallback_point1, fallback_point2, candidate.point,
		relaxed_tolerance);
	const bool closed_edge_has_authoritative_side =
	    required_closed_split && fallback_trim1_parameter &&
	    fallback_trim2_parameter && candidate.point.IsValid() &&
	    (fallback_edge_miss1 <= agreement_tolerance ||
	    fallback_edge_miss2 <= agreement_tolerance);
	if (fallback_trim1_parameter && fallback_trim2_parameter &&
		(fallback_agreement <= agreement_tolerance ||
		relaxed_edge_split) &&
		fallback_edge_progress) {
	    candidate.trim_parameters[0] = fallback_t1;
	    candidate.trim_parameters[1] = fallback_t2;
	    candidate.trim_points[0] = fallback_uv1;
	    candidate.trim_points[1] = fallback_uv2;
	    candidate.point = fallback_point;
	    candidate.tangent = ON_3dVector::UnsetVector;
	    trim_progress = true;
	    if (relaxed_edge_split)
		candidate.approximation_miss = bounded_edge_miss;
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
	    candidate.trim_parameters[0] = fallback_t1;
	    candidate.trim_parameters[1] = fallback_t2;
	    candidate.trim_points[0] = fallback_uv1;
	    candidate.trim_points[1] = fallback_uv2;
	    candidate.tangent = ON_3dVector::UnsetVector;
	    trim_progress = true;
	    if (fallback_edge_miss1 > agreement_tolerance &&
		    std::isfinite(fallback_edge_miss1))
		candidate.quarantine_misses[0] = fallback_edge_miss1;
	    if (fallback_edge_miss2 > agreement_tolerance &&
		    std::isfinite(fallback_edge_miss2))
		candidate.quarantine_misses[1] = fallback_edge_miss2;
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
		fallback_point1.DistanceTo(candidate.point) : DBL_MAX,
		fallback_point2.IsValid() ?
		fallback_point2.DistanceTo(candidate.point) : DBL_MAX,
		fallback_edge_progress ? 1 : 0);
	}
    }
    return trim_progress;
}


/* Deferring chart cleanup is useful only if the shared edge can be
 * subdivided without substituting geometry from inconsistent pullbacks. */
bool
shared_edge_midpoint_matches_trims(struct ON_Brep_CDT_State *state,
	bedge_seg_t *segment, const ON_3dPoint &midpoint)
{
    if (!state || !segment->tseg1 || !segment->tseg2)
	return false;
    edge_split_candidate candidate;
    if (!select_edge_split(candidate, state, segment, 1, nullptr,
	    &midpoint, false))
	return false;
    const cpolyedge_t *segments[] = {segment->tseg1, segment->tseg2};
    for (int side = 0; side < 2; ++side) {
	const auto &trim = state->brep->m_T[segments[side]->trim_ind];
	const ON_3dPoint mapped = cdt_surface_point(trim.SurfaceOf(),
	    candidate.trim_points[side]);
	if (!mapped.IsValid() || mapped.DistanceTo(midpoint) > BN_TOL_DIST)
	    return false;
    }
    return true;
}

static long
add_split_point(cdt_mesh_t &mesh, ON_2dPoint &uv, ON_3dPoint *point)
{
    const long native_point = mesh.add_point(uv);
    const long vertex = mesh.add_point(point);
    if (mesh.p2d3d.find(native_point) != mesh.p2d3d.end())
	bu_log("%d: 2d->3d mapping already exists for %ld\n",
	    mesh.f_id, native_point);
    mesh.p2d3d[native_point] = vertex;
    return native_point;
}


static void
add_split_normal(struct ON_Brep_CDT_State *state, cdt_mesh_t &mesh,
	ON_BrepTrim &trim, ON_2dPoint &uv, ON_3dPoint *point,
	long native_point, int edge_index)
{
    const ON_3dVector normal = trim_normal(&trim, uv);
    mesh.normals.push_back(new ON_3dPoint(normal));
    CDT_Add3DNorm(state, mesh.normals.back(), point, mesh.f_id, -1,
	trim.m_trim_index, edge_index, uv.x, uv.y);
    mesh.nmap[mesh.p2d3d.at(native_point)] = mesh.normals.size() - 1;
}


static std::pair<cpolyedge_t *, cpolyedge_t *>
split_trim_polygon(cpolyedge_t *segment, ON_2dPoint &uv,
	long native_point, double parameter)
{
    cpolygon_t *polygon = segment->polygon;
    const int start = segment->v2d[0], end = segment->v2d[1];
    const int trim_index = segment->trim_ind;
    const double trim_start = segment->trim_start;
    const double trim_end = segment->trim_end;
    polygon->remove_ordered_edge(edge2d_t(start, end));
    const long midpoint = polygon->add_point(uv, native_point);
    cpolyedge_t *first = polygon->add_ordered_edge(edge2d_t(start, midpoint));
    first->trim_ind = trim_index;
    first->trim_start = trim_start;
    first->trim_end = parameter;
    cpolyedge_t *second = polygon->add_ordered_edge(edge2d_t(midpoint, end));
    second->trim_ind = trim_index;
    second->trim_start = parameter;
    second->trim_end = trim_end;
    return std::make_pair(first, second);
}


std::set<bedge_seg_t *>
split_edge_seg(struct ON_Brep_CDT_State *s_cdt, bedge_seg_t *bseg,
	int force, double *t, int update_rtrees, ON_3dPoint *shared_point,
	bool required_closed_split)
{
    std::set<bedge_seg_t *> nedges;
    if (!bseg->tseg1 || !bseg->tseg2 || !bseg->nc)
	return nedges;
    edge_split_candidate candidate;
    if (!select_edge_split(candidate, s_cdt, bseg, force, t, shared_point,
	    required_closed_split))
	return nedges;

    ON_BrepEdge &edge = s_cdt->brep->m_E[bseg->edge_ind];
    ON_BrepTrim *trim1 = &s_cdt->brep->m_T[bseg->tseg1->trim_ind];
    ON_BrepTrim *trim2 = &s_cdt->brep->m_T[bseg->tseg2->trim_ind];
    cdt_mesh_t *fmesh1 = &s_cdt->fmeshes[trim1->Face()->m_face_index];
    cdt_mesh_t *fmesh2 = &s_cdt->fmeshes[trim2->Face()->m_face_index];
    if (candidate.approximation_miss >= 0.0) {
	fastf_t &recorded = s_cdt->approximated_edges[edge.m_edge_index];
	recorded = std::max(recorded, (fastf_t)candidate.approximation_miss);
    }
    const int face_indices[2] = {trim1->Face()->m_face_index,
	trim2->Face()->m_face_index};
    for (int side = 0; side < 2; ++side) {
	const double miss = candidate.quarantine_misses[side];
	if (!(miss > 0.0))
	    continue;
	const auto current = s_cdt->inconsistent_edge_faces.find(face_indices[side]);
	if (current == s_cdt->inconsistent_edge_faces.end() ||
		miss > current->second.second)
	    s_cdt->inconsistent_edge_faces[face_indices[side]] =
		std::make_pair(edge.m_edge_index, (fastf_t)miss);
    }

    /* UV samples must remain on their trims so adjacent faces retain exactly
     * the same boundary subdivision.  The shared master-edge point remains
     * the watertight 3-D authority even when a face pullback differs within
     * the B-Rep edge tolerance. */
    ON_3dPoint *mid_3d = shared_point ? shared_point :
	new ON_3dPoint(candidate.point);
    if (!shared_point) {
	CDT_Add3DPnt(s_cdt, mid_3d, -1, -1, -1, edge.m_edge_index,
	    0, 0);
	s_cdt->edge_pnts->insert(mid_3d);
    }

    const long f1_ind2d = add_split_point(*fmesh1,
	candidate.trim_points[0], mid_3d);
    const long f2_ind2d = add_split_point(*fmesh2,
	candidate.trim_points[1], mid_3d);
    add_split_normal(s_cdt, *fmesh1, *trim1, candidate.trim_points[0],
	mid_3d, f1_ind2d, bseg->edge_ind);
    add_split_normal(s_cdt, *fmesh2, *trim2, candidate.trim_points[1],
	mid_3d, f2_ind2d, bseg->edge_ind);

    // From the existing polyedge, make the two new polyedges that will replace the old one
    bedge_seg_t *bseg1 = new bedge_seg_t(bseg);
    bseg1->edge_start = bseg->edge_start;
    bseg1->edge_end = candidate.edge_parameter;
    bseg1->e_start = bseg->e_start;
    bseg1->e_end = mid_3d;
    bseg1->tan_start = bseg->tan_start;
    bseg1->tan_end = candidate.tangent;

    bedge_seg_t *bseg2 = new bedge_seg_t(bseg);
    bseg2->edge_start = candidate.edge_parameter;
    bseg2->edge_end = bseg->edge_end;
    bseg2->e_start = mid_3d;
    bseg2->e_end = bseg->e_end;
    bseg2->tan_start = candidate.tangent;
    bseg2->tan_end = bseg->tan_end;

    // Remove the old segments from their respective rtrees
    if (update_rtrees) {
	rtree_bbox_2d_remove(s_cdt, bseg->tseg1);
	rtree_bbox_2d_remove(s_cdt, bseg->tseg2);
    }

    const auto side1 = split_trim_polygon(bseg->tseg1,
	candidate.trim_points[0], f1_ind2d, candidate.trim_parameters[0]);
    const auto side2 = split_trim_polygon(bseg->tseg2,
	candidate.trim_points[1], f2_ind2d, candidate.trim_parameters[1]);
    cpolyedge_t *poly1_ne1 = side1.first, *poly1_ne2 = side1.second;
    cpolyedge_t *poly2_ne1 = side2.first, *poly2_ne2 = side2.second;

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
	const auto miss = [&](cpolyedge_t *trim_segment) {
	    fastf_t trim_parameter = 0.0;
	    ON_2dPoint uv = get_trim_midpt(&trim_parameter, s_cdt,
		trim_segment, edge_point, edge_length,
		edge.m_tolerance);
	    const ON_Surface *surface = trim->SurfaceOf();
	    if (!surface || !uv.IsValid())
		return DBL_MAX;
	    const ON_3dPoint surface_point = cdt_surface_point(surface, uv);
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
	    const ON_Curve *crv = edge.TrimCount() ? edge.EdgeCurveOf() : NULL;
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
	const ON_Curve *crv = edge.TrimCount() ? edge.EdgeCurveOf() : NULL;

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
	if (!edge.TrimCount())
	    continue;

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
	const ON_Curve *crv = edge.TrimCount() ? edge.EdgeCurveOf() : NULL;
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
		const ON_BrepEdge &neighbor = brep->m_E[brep->m_V[vert_ind].m_ei[j]];
		const ON_Curve *crv2 = neighbor.TrimCount() ? neighbor.EdgeCurveOf() : NULL;
		if (crv2 && !crv2->IsLinear(BN_TOL_DIST)) {
		    double emed = edge_median_seg_len(s_cdt, neighbor.m_edge_index);
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
	const int closed_direction = cdt_face_closed_direction(face);
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
		bool complete_rtrees = true;
		for (int dirty_face : dirty_rtrees) {
		    if (!rebuild_face_rtree_2d(s_cdt, dirty_face, 0)) {
			bu_log("Unable to rebuild face %d close-edge RTree\n",
			    dirty_face);
			complete_rtrees = false;
		    }
		}
		if (!complete_rtrees)
		    return;
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

bool
finalize_face_rtrees(struct ON_Brep_CDT_State *s_cdt, int face_index)
{
    s_cdt->face_rtrees_3d[face_index].RemoveAll();
    if (!rebuild_face_rtree_2d(s_cdt, face_index, 1))
	return false;
    for (cpolyedge_t *trim : cdt_face_polyedges(s_cdt, face_index))
	rtree_bbox_3d(s_cdt, trim);
    return true;
}

void
finalize_rtrees(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;
    /* Shared-edge refinement deletes segments on multiple faces.  Refresh
     * every index even when one polygon walk is incomplete, so later faces
     * cannot query deleted segments through their previous 2-D or 3-D trees. */
    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	if (!rebuild_face_rtree_2d(s_cdt, face_index, 1)) {
	    bu_log("Unable to finalize face %d 2D RTree\n", face_index);
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

int
cdt_test_rtree_finalization(void)
{
    const ON_2dPoint corners[] = {
	ON_2dPoint(0.0, 0.0), ON_2dPoint(2.0, 0.0), ON_2dPoint(0.0, 2.0)
    };
    const size_t corner_count = sizeof(corners) / sizeof(corners[0]);
    const int face_count = 2;
    /* Both a complete rebuild and failures before or after a healthy face
     * must discard prior indexes, including entries for deleted segments. */
    for (int broken_face : {-1, 0, 1}) {
	ON_Brep brep;
	ON_Brep_CDT_State state = {};
	state.brep = &brep;
	std::vector<std::unique_ptr<cpolyedge_t>> owned_edges;
	int stale_entry = 0;
	const double minimum_2d[] = {0.0, 0.0};
	const double maximum_2d[] = {2.0, 2.0};
	const double minimum_3d[] = {0.0, 0.0, 0.0};
	const double maximum_3d[] = {2.0, 2.0, 0.0};
	for (int face_index = 0; face_index < face_count; ++face_index) {
	    ON_BrepFace &face = brep.NewFace(brep.AddSurface(
		new ON_PlaneSurface(ON_xy_plane)));
	    ON_BrepLoop &loop = brep.NewLoop(ON_BrepLoop::outer, face);
	    cpolygon_t &polygon = state.fmeshes[face_index].outer_loop;
	    for (size_t corner = 0; corner < corner_count; ++corner) {
		ON_2dPoint point = corners[corner];
		polygon.add_point(point, (long)corner);
	    }
	    for (size_t corner = 0; corner < corner_count; ++corner) {
		const size_t next = (corner + 1) % corner_count;
		ON_BrepTrim &trim = brep.NewTrim(false, loop,
		    brep.AddTrimCurve(new ON_LineCurve(corners[corner],
			corners[next])));
		cpolyedge_t *segment = polygon.add_ordered_edge(
		    edge2d_t((long)corner, (long)next));
		segment->trim_ind = trim.m_trim_index;
		owned_edges.emplace_back(segment);
	    }
	    if (face_index == broken_face)
		(*polygon.poly.begin())->next = NULL;
	    state.face_rtrees_2d[face_index].Insert(minimum_2d, maximum_2d,
		&stale_entry);
	    state.face_rtrees_3d[face_index].Insert(minimum_3d, maximum_3d,
		&stale_entry);
	}
	finalize_rtrees(&state);
	for (int face_index = 0; face_index < face_count; ++face_index) {
	    const int expected_edges = face_index == broken_face ? 0 :
		(int)corner_count;
	    if (state.face_rtrees_2d[face_index].Count() != expected_edges ||
		state.face_rtrees_3d[face_index].Count() != 0) {
		bu_log("RTree finalization retained an incomplete or stale index "
		    "for face %d after broken face %d\n", face_index,
		    broken_face);
		return 1;
	    }
	}
    }
    return 0;
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
