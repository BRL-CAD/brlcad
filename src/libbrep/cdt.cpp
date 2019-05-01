/*                        C D T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2019 United States Government as represented by
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
/** @file cdt.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"

#include <vector>
#include <list>
#include <map>
#include <stack>
#include <iostream>
#include <algorithm>
#include <set>
#include <utility>

#include "poly2tri/poly2tri.h"

#include "assert.h"

#include "vmath.h"

#include "bu/color.h"
#include "bu/cv.h"
#include "bu/opt.h"
#include "bu/time.h"
#include "bn/tol.h"
#include "bn/vlist.h"
#include "bg/trimesh.h"
#include "brep/defines.h"
#include "brep/cdt.h"
#include "brep/pullback.h"
#include "brep/util.h"


typedef std::pair<ON_3dPoint *, ON_3dPoint *> Edge;
typedef std::map< Edge, std::set<p2t::Triangle*> > EdgeToTri;

static Edge
mk_edge(ON_3dPoint *pt_A, ON_3dPoint *pt_B)
{
    if (pt_A <= pt_B) {
	return std::make_pair(pt_A, pt_B);
    } else {
	return std::make_pair(pt_B, pt_A);
    }
}

static void
add_tri_edges(EdgeToTri *e2f, p2t::Triangle *t,
    std::map<p2t::Point *, ON_3dPoint *> *pointmap)
{
    ON_3dPoint *pt_A, *pt_B, *pt_C;

    p2t::Point *p2_A = t->GetPoint(0);
    p2t::Point *p2_B = t->GetPoint(1);
    p2t::Point *p2_C = t->GetPoint(2);
    pt_A = (*pointmap)[p2_A];
    pt_B = (*pointmap)[p2_B];
    pt_C = (*pointmap)[p2_C];
    (*e2f)[(mk_edge(pt_A, pt_B))].insert(t);
    (*e2f)[(mk_edge(pt_B, pt_C))].insert(t);
    (*e2f)[(mk_edge(pt_C, pt_A))].insert(t);
}

#define BREP_CDT_FAILED -3
#define BREP_CDT_NON_SOLID -2
#define BREP_CDT_UNTESSELLATED -1
#define BREP_CDT_SOLID 0

/* Note - these tolerance values are based on the default
 * values from the GED 'tol' command */
#define BREP_CDT_DEFAULT_TOL_ABS 0.0
#define BREP_CDT_DEFAULT_TOL_REL 0.01
#define BREP_CDT_DEFAULT_TOL_NORM 0.0
#define BREP_CDT_DEFAULT_TOL_DIST BN_TOL_DIST

/* this is a debugging structure - it holds origin information for
 * a point added to the CDT state */
struct cdt_audit_info {
    int face_index;
    int vert_index;
    int trim_index;
    int edge_index;
    ON_2dPoint surf_uv;
};

struct cdt_audit_info *
cdt_ainfo(int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d)
{
    struct cdt_audit_info *a = new struct cdt_audit_info;
    a->face_index = fid;
    a->vert_index = vid;
    a->trim_index = tid;
    a->edge_index = eid;
    a->surf_uv = ON_2dPoint(x2d, y2d);
    return a;
}

struct ON_Brep_CDT_State {

    int status;
    ON_Brep *brep;

    /* Tolerances */
    fastf_t abs;
    fastf_t rel;
    fastf_t norm;
    fastf_t dist;

    /* 3D data */
    std::vector<ON_3dPoint *> *w3dpnts;
    std::map<int, ON_3dPoint *> *vert_pnts;
    std::vector<ON_3dPoint *> *w3dnorms;
    std::map<int, ON_3dPoint *> *vert_norms;

    /* Poly2Tri data */
    p2t::CDT **p2t_faces;
    std::vector<p2t::Triangle *> **p2t_extra_faces;
    std::map<p2t::Point *, ON_3dPoint *> **p2t_maps;
    std::map<p2t::Point *, ON_3dPoint *> **p2t_nmaps;

    /* Audit data */
    std::map<ON_3dPoint *, struct cdt_audit_info *> pnt_audit_info;

    /* BoT -> ON mappings */
    std::map<int, ON_3dPoint *> *vert_to_on;
    std::set<ON_3dPoint *> *edge_pnts;
};

void
CDT_Add3DPnt(struct ON_Brep_CDT_State *s, ON_3dPoint *p, int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d)
{
    s->w3dpnts->push_back(p);
    s->pnt_audit_info[p] = cdt_ainfo(fid, vid, tid, eid, x2d, y2d);
}


struct brep_cdt_tol {
    fastf_t min_dist;
    fastf_t max_dist;
    fastf_t within_dist;
    fastf_t cos_within_ang;
};

#define BREP_CDT_TOL_ZERO {0.0, 0.0, 0.0, 0.0}

// Digest tessellation tolerances...
static void
CDT_Tol_Set(struct brep_cdt_tol *cdt, double dist, fastf_t md, double t_abs, double t_rel, double t_norm, double t_dist)
{
    fastf_t min_dist, max_dist, within_dist, cos_within_ang;

    max_dist = md;

    if (t_abs < t_dist + ON_ZERO_TOLERANCE) {
	min_dist = t_dist;
    } else {
	min_dist = t_abs;
    }

    double rel = 0.0;
    if (t_rel > 0.0 + ON_ZERO_TOLERANCE) {
	rel = t_rel * dist;
	// TODO - think about what we want maximum distance to mean and how it
	// relates to other tessellation settings...
	if (max_dist < rel * 10.0) {
	    max_dist = rel * 10.0;
	}
	within_dist = rel < min_dist ? min_dist : rel;
    } else if (t_abs > 0.0 + ON_ZERO_TOLERANCE) {
	within_dist = min_dist;
    } else {
	within_dist = 0.01 * dist; // default to 1% of dist
    }

    if (t_norm > 0.0 + ON_ZERO_TOLERANCE) {
	cos_within_ang = cos(t_norm);
    } else {
	cos_within_ang = cos(ON_PI / 2.0);
    }

    cdt->min_dist = min_dist;
    cdt->max_dist = max_dist;
    cdt->within_dist = within_dist;
    cdt->cos_within_ang = cos_within_ang;
}


static void
getEdgePoints(
	struct ON_Brep_CDT_State *s_cdt,
	ON_BrepEdge *edge,
	ON_NurbsCurve *nc,
	const ON_BrepTrim &trim,
	BrepTrimPoint *sbtp1,
	BrepTrimPoint *ebtp1,
	BrepTrimPoint *sbtp2,
	BrepTrimPoint *ebtp2,
	const struct brep_cdt_tol *cdt_tol,
	std::map<double, BrepTrimPoint *> *trim1_param_points,
	std::map<double, BrepTrimPoint *> *trim2_param_points
	)
{
    ON_3dPoint tmp1, tmp2;
    ON_BrepTrim *trim2 = (edge->Trim(0)->m_trim_index == trim.m_trim_index) ? edge->Trim(1) : edge->Trim(0);
    const ON_Surface *s1 = trim.SurfaceOf();
    const ON_Surface *s2 = trim2->SurfaceOf();
    fastf_t t1 = (sbtp1->t + ebtp1->t) / 2.0;
    fastf_t t2 = (sbtp2->t + ebtp2->t) / 2.0;
    ON_3dPoint trim1_mid_2d = trim.PointAt(t1);
    ON_3dPoint trim2_mid_2d = trim2->PointAt(t2);
    fastf_t emid = (sbtp1->e + ebtp1->e) / 2.0;

    ON_3dVector trim1_mid_norm = ON_3dVector::UnsetVector;
    ON_3dVector trim2_mid_norm = ON_3dVector::UnsetVector;
    ON_3dPoint edge_mid_3d = ON_3dPoint::UnsetPoint;
    ON_3dVector edge_mid_tang = ON_3dVector::UnsetVector;

    if (!(nc->EvTangent(emid, edge_mid_3d, edge_mid_tang))) {
	// EvTangent call failed, get 3d point
	edge_mid_3d = nc->PointAt(emid);
	// If the edge curve failed, try to average tangents from trims
	ON_3dVector trim1_mid_tang(0.0, 0.0, 0.0);
	ON_3dVector trim2_mid_tang(0.0, 0.0, 0.0);
	int evals = 0;
	evals += (trim.EvTangent(t1, tmp1, trim1_mid_tang)) ? 1 : 0;
	evals += (trim2->EvTangent(t2, tmp2, trim2_mid_tang)) ? 1 : 0;
	if (evals == 2) {
	    edge_mid_tang = (trim1_mid_tang + trim2_mid_tang) / 2;
	} else {
	    edge_mid_tang = ON_3dVector::UnsetVector;
	}
    }

    // bu_log("Edge %d, Trim %d(%d): %f %f %f\n", edge->m_edge_index, trim.m_trim_index, trim.m_bRev3d, edge_mid_3d.x, edge_mid_3d.y, edge_mid_3d.z);

    int dosplit = 0;

    ON_Line line3d(*(sbtp1->p3d), *(ebtp1->p3d));
    double dist3d = edge_mid_3d.DistanceTo(line3d.ClosestPointTo(edge_mid_3d));
    dosplit += (line3d.Length() > cdt_tol->max_dist) ? 1 : 0;
    dosplit += (dist3d > (cdt_tol->within_dist + ON_ZERO_TOLERANCE)) ? 1 : 0;

    if ((dist3d > cdt_tol->min_dist + ON_ZERO_TOLERANCE)) {
	if (!dosplit) {
	    dosplit += ((sbtp1->tangent * ebtp1->tangent) < cdt_tol->cos_within_ang - ON_ZERO_TOLERANCE) ? 1 : 0;
	}

	if (!dosplit && sbtp1->normal != ON_3dVector::UnsetVector && ebtp1->normal != ON_3dVector::UnsetVector) {
	    dosplit += ((sbtp1->normal * ebtp1->normal) < cdt_tol->cos_within_ang - ON_ZERO_TOLERANCE) ? 1 : 0;
	}

	if (!dosplit && sbtp2->normal != ON_3dVector::UnsetVector && ebtp2->normal != ON_3dVector::UnsetVector) {
	    dosplit += ((sbtp2->normal * ebtp2->normal) < cdt_tol->cos_within_ang - ON_ZERO_TOLERANCE) ? 1 : 0;
	}
    }

    if (dosplit) {

	if (!surface_EvNormal(s1, trim1_mid_2d.x, trim1_mid_2d.y, tmp1, trim1_mid_norm)) {
	    trim1_mid_norm = ON_3dVector::UnsetVector;
	} else {
	    if (trim.Face()->m_bRev) {
		trim1_mid_norm = trim1_mid_norm  * -1.0;
	    }
	}

	if (!surface_EvNormal(s2, trim2_mid_2d.x, trim2_mid_2d.y, tmp2, trim2_mid_norm)) {
	    trim2_mid_norm = ON_3dVector::UnsetVector;
	} else {
	    if (trim2->Face()->m_bRev) {
		trim2_mid_norm = trim2_mid_norm  * -1.0;
	    }
	}

	ON_3dPoint *npt = new ON_3dPoint(edge_mid_3d);
	CDT_Add3DPnt(s_cdt, npt, -1, -1, -1, edge->m_edge_index, emid, 0);
	s_cdt->edge_pnts->insert(npt);

	BrepTrimPoint *nbtp1 = new BrepTrimPoint;
	nbtp1->p3d = npt;
	nbtp1->n3d = NULL;
	nbtp1->p2d = trim1_mid_2d;
	nbtp1->normal = trim1_mid_norm;
	nbtp1->tangent = edge_mid_tang;
	nbtp1->t = t1;
	nbtp1->e = emid;
	(*trim1_param_points)[nbtp1->t] = nbtp1;

	BrepTrimPoint *nbtp2 = new BrepTrimPoint;
	nbtp2->p3d = npt;
	nbtp2->n3d = NULL;
	nbtp2->p2d = trim2_mid_2d;
	nbtp2->normal = trim2_mid_norm;
	nbtp2->tangent = edge_mid_tang;
	nbtp2->t = t2;
	nbtp2->e = emid;
	(*trim2_param_points)[nbtp2->t] = nbtp2;

	getEdgePoints(s_cdt, edge, nc, trim, sbtp1, nbtp1, sbtp2, nbtp2, cdt_tol, trim1_param_points, trim2_param_points);
	getEdgePoints(s_cdt, edge, nc, trim, nbtp1, ebtp1, nbtp2, ebtp2, cdt_tol, trim1_param_points, trim2_param_points);
	return;
    }

    int udir = 0;
    int vdir = 0;
    double trim1_seam_t = 0.0;
    double trim2_seam_t = 0.0;
    ON_2dPoint trim1_start = sbtp1->p2d;
    ON_2dPoint trim1_end = ebtp1->p2d;
    ON_2dPoint trim2_start = sbtp2->p2d;
    ON_2dPoint trim2_end = ebtp2->p2d;
    ON_2dPoint trim1_seam_2d, trim2_seam_2d;
    ON_3dPoint trim1_seam_3d, trim2_seam_3d;
    int t1_dosplit = 0;
    int t2_dosplit = 0;

    if (ConsecutivePointsCrossClosedSeam(s1, trim1_start, trim1_end, udir, vdir, BREP_SAME_POINT_TOLERANCE)) {
	ON_2dPoint from = ON_2dPoint::UnsetPoint;
	ON_2dPoint to = ON_2dPoint::UnsetPoint;
	if (FindTrimSeamCrossing(trim, sbtp1->t, ebtp1->t, trim1_seam_t, from, to, BREP_SAME_POINT_TOLERANCE)) {
	    trim1_seam_2d = trim.PointAt(trim1_seam_t);
	    trim1_seam_3d = s1->PointAt(trim1_seam_2d.x, trim1_seam_2d.y);
	    if (trim1_param_points->find(trim1_seam_t) == trim1_param_points->end()) {
		t1_dosplit = 1;
	    }
	}
    }

    if (ConsecutivePointsCrossClosedSeam(s2, trim2_start, trim2_end, udir, vdir, BREP_SAME_POINT_TOLERANCE)) {
	ON_2dPoint from = ON_2dPoint::UnsetPoint;
	ON_2dPoint to = ON_2dPoint::UnsetPoint;
	if (FindTrimSeamCrossing(trim, sbtp2->t, ebtp2->t, trim2_seam_t, from, to, BREP_SAME_POINT_TOLERANCE)) {
	    trim2_seam_2d = trim2->PointAt(trim2_seam_t);
	    trim2_seam_3d = s2->PointAt(trim2_seam_2d.x, trim2_seam_2d.y);
	    if (trim2_param_points->find(trim2_seam_t) == trim2_param_points->end()) {
		t2_dosplit = 1;
	    }
	}
    }

    if (t1_dosplit || t2_dosplit) {
	ON_3dPoint *nsptp = NULL;
	if (t1_dosplit && t2_dosplit) {
	    ON_3dPoint nspt = (trim1_seam_3d + trim2_seam_3d)/2;
	    nsptp = new ON_3dPoint(nspt);
	    CDT_Add3DPnt(s_cdt, nsptp, trim.Face()->m_face_index, -1, trim.m_trim_index, trim.Edge()->m_edge_index, trim1_seam_2d.x, trim1_seam_2d.y);
	    s_cdt->edge_pnts->insert(nsptp);
	} else {
	    // Since the above if test got the both-true case, only one of these at
	    // a time will ever be true.  TODO - could this be a source of degenerate
	    // faces in 3D if we're only splitting one trim?
	    if (!t1_dosplit) {
		trim1_seam_t = (sbtp1->t + ebtp1->t)/2;
		trim1_seam_2d = trim.PointAt(trim1_seam_t);
		nsptp = new ON_3dPoint(trim2_seam_3d);
		CDT_Add3DPnt(s_cdt, nsptp, trim2->Face()->m_face_index, -1, trim2->m_trim_index, trim2->Edge()->m_edge_index, trim1_seam_2d.x, trim1_seam_2d.y);
		s_cdt->edge_pnts->insert(nsptp);
	    }
	    if (!t2_dosplit) {
		trim2_seam_t = (sbtp2->t + ebtp2->t)/2;
		trim2_seam_2d = trim2->PointAt(trim2_seam_t);
		nsptp = new ON_3dPoint(trim1_seam_3d);
		CDT_Add3DPnt(s_cdt, nsptp, trim.Face()->m_face_index, -1, trim.m_trim_index, trim.Edge()->m_edge_index, trim1_seam_2d.x, trim1_seam_2d.y);
		s_cdt->edge_pnts->insert(nsptp);
	    }
	}

	// Note - by this point we shouldn't need tangents and normals...
	BrepTrimPoint *nbtp1 = new BrepTrimPoint;
	nbtp1->p3d = nsptp;
	nbtp1->p2d = trim1_seam_2d;
	nbtp1->t = trim1_seam_t;
	nbtp1->e = ON_UNSET_VALUE;
	(*trim1_param_points)[nbtp1->t] = nbtp1;

	BrepTrimPoint *nbtp2 = new BrepTrimPoint;
	nbtp2->p3d = nsptp;
	nbtp2->p2d = trim2_seam_2d;
	nbtp2->t = trim2_seam_t;
	nbtp2->e = ON_UNSET_VALUE;
	(*trim2_param_points)[nbtp2->t] = nbtp2;
    }

}

static std::map<double, BrepTrimPoint *> *
getEdgePoints(
	struct ON_Brep_CDT_State *s_cdt,
	ON_BrepEdge *edge,
	ON_BrepTrim &trim,
	fastf_t max_dist,
	std::map<int, ON_3dPoint *> *vert_pnts,
	std::map<int, ON_3dPoint *> *vert_norms
	)
{
    struct brep_cdt_tol cdt_tol = BREP_CDT_TOL_ZERO;
    std::map<double, BrepTrimPoint *> *trim1_param_points = NULL;
    std::map<double, BrepTrimPoint *> *trim2_param_points = NULL;

    // Get the other trim
    // TODO - this won't work if we don't have a 1->2 edge to trims relationship - in that
    // case we'll have to split up the edge and find the matching sub-trims (possibly splitting
    // those as well if they don't line up at shared 3D points.)
    ON_BrepTrim *trim2 = (edge->Trim(0)->m_trim_index == trim.m_trim_index) ? edge->Trim(1) : edge->Trim(0);

    double dist = 1000.0;

    const ON_Surface *s1 = trim.SurfaceOf();
    const ON_Surface *s2 = trim2->SurfaceOf();

    bool bGrowBox = false;
    ON_3dPoint min, max;

    /* If we're out of sync, bail - something is very very wrong */
    if (trim.m_trim_user.p != NULL && trim2->m_trim_user.p == NULL) {
	return NULL;
    }
    if (trim.m_trim_user.p == NULL && trim2->m_trim_user.p != NULL) {
	return NULL;
    }


    /* If we've already got the points, just return them */
    if (trim.m_trim_user.p != NULL) {
	trim1_param_points = (std::map<double, BrepTrimPoint *> *) trim.m_trim_user.p;
	return trim1_param_points;
    }

    /* Establish tolerances (TODO - get from edge curve...) */
    if (trim.GetBoundingBox(min, max, bGrowBox)) {
	dist = DIST_PT_PT(min, max);
    }
    CDT_Tol_Set(&cdt_tol, dist, max_dist, s_cdt->abs, s_cdt->rel, s_cdt->norm, s_cdt->dist);

    /* Begin point collection */
    ON_3dPoint tmp1, tmp2;
    int evals = 0;
    ON_3dPoint *edge_start_3d, *edge_end_3d = NULL;
    ON_3dVector edge_start_tang, edge_end_tang = ON_3dVector::UnsetVector;
    ON_3dPoint trim1_start_2d, trim1_end_2d = ON_3dPoint::UnsetPoint;
    ON_3dVector trim1_start_tang, trim1_end_tang = ON_3dVector::UnsetVector;
    ON_3dPoint trim2_start_2d, trim2_end_2d = ON_3dPoint::UnsetPoint;
    ON_3dVector trim2_start_tang, trim2_end_tang = ON_3dVector::UnsetVector;

    trim1_param_points = new std::map<double, BrepTrimPoint *>();
    trim.m_trim_user.p = (void *) trim1_param_points;
    trim2_param_points = new std::map<double, BrepTrimPoint *>();
    trim2->m_trim_user.p = (void *) trim2_param_points;

    ON_Interval range1 = trim.Domain();
    ON_Interval range2 = trim2->Domain();

    // TODO - what is this for?
    if (s1->IsClosed(0) || s1->IsClosed(1)) {
	ON_BoundingBox trim_bbox = ON_BoundingBox::EmptyBoundingBox;
	trim.GetBoundingBox(trim_bbox, false);
    }
    if (s2->IsClosed(0) || s2->IsClosed(1)) {
	ON_BoundingBox trim_bbox2 = ON_BoundingBox::EmptyBoundingBox;
	trim2->GetBoundingBox(trim_bbox2, false);
    }

    /* For the start and end points, use the vertex point */
    edge_start_3d = (*vert_pnts)[edge->Vertex(0)->m_vertex_index];
    edge_end_3d = (*vert_pnts)[edge->Vertex(1)->m_vertex_index];

    /* Normalize the domain of the curve to the ControlPolygonLength() of the
     * NURBS form of the curve to attempt to minimize distortion in 3D to
     * mirror what we do for the surfaces.  (GetSurfaceSize uses this under the
     * hood for its estimates.)  */
    const ON_Curve* crv = edge->EdgeCurveOf();
    ON_NurbsCurve *nc = crv->NurbsCurve();
    double cplen = nc->ControlPolygonLength();
    nc->SetDomain(0.0, cplen);
    ON_Interval erange = nc->Domain();

    /* Populate the 2D points */
    double st1 = (trim.m_bRev3d) ? range1.m_t[1] : range1.m_t[0];
    double et1 = (trim.m_bRev3d) ? range1.m_t[0] : range1.m_t[1];
    double st2 = (trim2->m_bRev3d) ? range2.m_t[1] : range2.m_t[0];
    double et2 = (trim2->m_bRev3d) ? range2.m_t[0] : range2.m_t[1];
    trim1_start_2d = trim.PointAt(st1);
    trim1_end_2d = trim.PointAt(et1);
    trim2_start_2d = trim2->PointAt(st2);
    trim2_end_2d = trim2->PointAt(et2);

    /* Get starting tangent from edge*/
    if (!(nc->EvTangent(erange.m_t[0], tmp1, edge_start_tang))) {
	// If the edge curve failed, average tangents from trims
	evals = 0;
	evals += (trim.EvTangent(st1, tmp1, trim1_start_tang)) ? 1 : 0;
	evals += (trim2->EvTangent(st2, tmp2, trim2_start_tang)) ? 1 : 0;
	if (evals == 2) {
	    edge_start_tang = (trim1_start_tang + trim2_start_tang) / 2;
	} else {
	    edge_start_tang = ON_3dVector::UnsetVector;
	}
    }
    /* Get ending tangent from edge*/
    if (!(nc->EvTangent(erange.m_t[1], tmp2, edge_end_tang))) {
	// If the edge curve failed, average tangents from trims
	evals = 0;
	evals += (trim.EvTangent(et1, tmp1, trim1_end_tang)) ? 1 : 0;
	evals += (trim2->EvTangent(et2, tmp2, trim2_end_tang)) ? 1 : 0;
	if (evals == 2) {
	    edge_end_tang = (trim1_end_tang + trim2_end_tang) / 2;
	} else {
	    edge_end_tang = ON_3dVector::UnsetVector;
	}
    }

    // Get the normals
    ON_3dPoint tmpp;
    ON_3dVector trim1_start_normal, trim1_end_normal = ON_3dVector::UnsetVector;
    ON_3dVector trim2_start_normal, trim2_end_normal = ON_3dVector::UnsetVector;
    ON_3dPoint *t1_sn, *t1_en, *t2_sn, *t2_en = NULL;
    ON_3dPoint *edge_start_3dnorm = (*vert_norms)[edge->Vertex(0)->m_vertex_index];
    ON_3dPoint *edge_end_3dnorm = (*vert_norms)[edge->Vertex(1)->m_vertex_index];

    /* trim 1 */
    if (!surface_EvNormal(s1, trim1_start_2d.x, trim1_start_2d.y, tmpp, trim1_start_normal)) {
	t1_sn = edge_start_3dnorm;
    } else {
	if (trim.Face()->m_bRev) {
	    trim1_start_normal = trim1_start_normal  * -1.0;
	}
	if (ON_DotProduct(trim1_start_normal, *edge_start_3dnorm) < 0) {
	    t1_sn = edge_start_3dnorm;
	} else {
	    t1_sn = new ON_3dPoint(trim1_start_normal);
	    s_cdt->w3dnorms->push_back(t1_sn);
	}
    }
    if (!surface_EvNormal(s1, trim1_end_2d.x, trim1_end_2d.y, tmp1, trim1_end_normal)) {
	t1_en = edge_end_3dnorm;
    } else {
	if (trim.Face()->m_bRev) {
	    trim1_end_normal = trim1_end_normal  * -1.0;
	}
	if (ON_DotProduct(trim1_end_normal, *edge_end_3dnorm) < 0) {
	    t1_en = edge_end_3dnorm;
	} else {
	    t1_en = new ON_3dPoint(trim1_end_normal);
	    s_cdt->w3dnorms->push_back(t1_en);
	}
    }


    /* trim 2 */
    if (!surface_EvNormal(s2, trim2_start_2d.x, trim2_start_2d.y, tmpp, trim2_start_normal)) {
	t2_sn = edge_start_3dnorm;
    } else {
	if (trim2->Face()->m_bRev) {
	    trim2_start_normal = trim2_start_normal  * -1.0;
	}
	if (ON_DotProduct(trim2_start_normal, *edge_start_3dnorm) < 0) {
	    t2_sn = edge_start_3dnorm;
	} else {
	    t2_sn = new ON_3dPoint(trim2_start_normal);
	    s_cdt->w3dnorms->push_back(t2_sn);
	}
    }
    if (!surface_EvNormal(s2, trim2_end_2d.x, trim2_end_2d.y, tmpp, trim2_end_normal)) {
	t2_en = edge_end_3dnorm;
    } else {
	if (trim2->Face()->m_bRev) {
	    trim2_end_normal = trim2_end_normal  * -1.0;
	}
	if (ON_DotProduct(trim2_end_normal, *edge_end_3dnorm) < 0) {
	    t2_en = edge_end_3dnorm;
	} else {
	    t2_en = new ON_3dPoint(trim2_end_normal);
	    s_cdt->w3dnorms->push_back(t2_en);
	}
    }

    /* Start and end points for both trims can now be defined */
    BrepTrimPoint *sbtp1 = new BrepTrimPoint;
    sbtp1->p3d = edge_start_3d;
    sbtp1->n3d = t1_sn;
    sbtp1->tangent = edge_start_tang;
    sbtp1->e = erange.m_t[0];
    sbtp1->p2d = trim1_start_2d;
    sbtp1->normal = trim1_start_normal;
    sbtp1->t = st1;
    (*trim1_param_points)[sbtp1->t] = sbtp1;

    BrepTrimPoint *ebtp1 = new BrepTrimPoint;
    ebtp1->p3d = edge_end_3d;
    ebtp1->n3d = t1_en;
    ebtp1->tangent = edge_end_tang;
    ebtp1->e = erange.m_t[1];
    ebtp1->p2d = trim1_end_2d;
    ebtp1->normal = trim1_end_normal;
    ebtp1->t = et1;
    (*trim1_param_points)[ebtp1->t] = ebtp1;

    BrepTrimPoint *sbtp2 = new BrepTrimPoint;
    sbtp2->p3d = edge_start_3d;
    sbtp2->n3d = t2_sn;
    sbtp2->tangent = edge_start_tang;
    sbtp2->e = erange.m_t[0];
    sbtp2->p2d = trim2_start_2d;
    sbtp2->normal = trim2_start_normal;
    sbtp2->t = st2;
    (*trim2_param_points)[sbtp2->t] = sbtp2;

    BrepTrimPoint *ebtp2 = new BrepTrimPoint;
    ebtp2->p3d = edge_end_3d;
    ebtp2->n3d = t2_en;
    ebtp2->tangent = edge_end_tang;
    ebtp2->e = erange.m_t[1];
    ebtp2->p2d = trim2_end_2d;
    ebtp2->normal = trim2_end_normal;
    ebtp2->t = et2;
    (*trim2_param_points)[ebtp2->t] = ebtp2;


    if (trim.IsClosed() || trim2->IsClosed()) {

	double trim1_mid_range = (st1 + et1) / 2.0;
	double trim2_mid_range = (st2 + et2) / 2.0;
	double edge_mid_range = (erange.m_t[0] + erange.m_t[1]) / 2.0;
	ON_3dVector edge_mid_tang, trim1_mid_norm, trim2_mid_norm = ON_3dVector::UnsetVector;
	ON_3dPoint edge_mid_3d = ON_3dPoint::UnsetPoint;

	if (!(nc->EvTangent(edge_mid_range, edge_mid_3d, edge_mid_tang))) {
	    // EvTangent call failed, get 3d point
	    edge_mid_3d = nc->PointAt(edge_mid_range);
	    // If the edge curve failed, try to average tangents from trims
	    ON_3dVector trim1_mid_tang(0.0, 0.0, 0.0);
	    ON_3dVector trim2_mid_tang(0.0, 0.0, 0.0);
	    evals = 0;
	    evals += (trim.EvTangent(trim1_mid_range, tmp1, trim1_mid_tang)) ? 1 : 0;
	    evals += (trim2->EvTangent(trim2_mid_range, tmp2, trim2_mid_tang)) ? 1 : 0;
	    if (evals == 2) {
		edge_mid_tang = (trim1_start_tang + trim2_start_tang) / 2;
	    } else {
		edge_mid_tang = ON_3dVector::UnsetVector;
	    }
	}

	evals = 0;
	ON_3dPoint trim1_mid_2d = trim.PointAt(trim1_mid_range);
	ON_3dPoint trim2_mid_2d = trim2->PointAt(trim2_mid_range);

	evals += (surface_EvNormal(s1, trim1_mid_2d.x, trim1_mid_2d.y, tmp1, trim1_mid_norm)) ? 1 : 0;
	if (trim.Face()->m_bRev) {
	    trim1_mid_norm = trim1_mid_norm  * -1.0;
	}

	evals += (surface_EvNormal(s2, trim2_mid_2d.x, trim2_mid_2d.y, tmp2, trim2_mid_norm)) ? 1 : 0;
	if (trim2->Face()->m_bRev) {
	    trim2_mid_norm = trim2_mid_norm  * -1.0;
	}

	if (evals != 2) {
	    bu_log("problem with mid normal evals\n");
	}

	ON_3dPoint *nmp = new ON_3dPoint(edge_mid_3d);
	CDT_Add3DPnt(s_cdt, nmp, -1, -1, -1, edge->m_edge_index, edge_mid_range, 0);
	s_cdt->edge_pnts->insert(nmp);

	BrepTrimPoint *mbtp1 = new BrepTrimPoint;
	mbtp1->p3d = nmp;
	mbtp1->n3d = NULL;
	mbtp1->p2d = trim1_mid_2d;
	mbtp1->tangent = edge_mid_tang;
	mbtp1->normal = trim1_mid_norm;
	mbtp1->t = trim1_mid_range;
	mbtp1->e = edge_mid_range;
	(*trim1_param_points)[mbtp1->t] = mbtp1;

	BrepTrimPoint *mbtp2 = new BrepTrimPoint;
	mbtp2->p3d = nmp;
	mbtp2->n3d = NULL;
	mbtp2->p2d = trim2_mid_2d;
	mbtp2->tangent = edge_mid_tang;
	mbtp2->normal = trim2_mid_norm;
	mbtp2->t = trim2_mid_range;
	mbtp1->e = edge_mid_range;
	(*trim2_param_points)[mbtp2->t] = mbtp2;

	getEdgePoints(s_cdt, edge, nc, trim, sbtp1, mbtp1, sbtp2, mbtp2, &cdt_tol, trim1_param_points, trim2_param_points);
	getEdgePoints(s_cdt, edge, nc, trim, mbtp1, ebtp1, mbtp2, ebtp2, &cdt_tol, trim1_param_points, trim2_param_points);

    } else {

	getEdgePoints(s_cdt, edge, nc, trim, sbtp1, ebtp1, sbtp2, ebtp2, &cdt_tol, trim1_param_points, trim2_param_points);

    }

    return trim1_param_points;
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

    if ((udist < min_dist + ON_ZERO_TOLERANCE)
	|| (vdist < min_dist + ON_ZERO_TOLERANCE)) {
	return;
    }

    if (udist > ldfactor * vdist) {
	int isteps = (int)(udist / vdist);
	isteps = (int)(udist / vdist / ldfactor * 2.0);
	fastf_t step = udist / (fastf_t) isteps;

	fastf_t step_u;
	for (int i = 1; i <= isteps; i++) {
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
    } else if (vdist > ldfactor * udist) {
	int isteps = (int)(vdist / udist);
	isteps = (int)(vdist / udist / ldfactor * 2.0);
	fastf_t step = vdist / (fastf_t) isteps;
	fastf_t step_v;
	for (int i = 1; i <= isteps; i++) {
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
getSurfacePoints(struct ON_Brep_CDT_State *s_cdt,
	         const ON_BrepFace &face,
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

	if ((surface_width < s_cdt->dist) || (surface_height < s_cdt->dist)) {
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

	ON_BoundingBox tight_bbox;
	if (brep->GetTightBoundingBox(tight_bbox)) {
	    dist = DIST_PT_PT(tight_bbox.m_min, tight_bbox.m_max);
	}

	if (s_cdt->abs < s_cdt->dist + ON_ZERO_TOLERANCE) {
	    min_dist = s_cdt->dist;
	} else {
	    min_dist = s_cdt->abs;
	}

	double rel = 0.0;
	if (s_cdt->rel > 0.0 + ON_ZERO_TOLERANCE) {
	    rel = s_cdt->rel * dist;
	    within_dist = rel < min_dist ? min_dist : rel;
	    //if (s_cdt->abs < s_cdt->dist + ON_ZERO_TOLERANCE) {
	    //    min_dist = within_dist;
	    //}
	} else if ((s_cdt->abs > 0.0 + ON_ZERO_TOLERANCE)
		   && (s_cdt->norm < 0.0 + ON_ZERO_TOLERANCE)) {
	    within_dist = min_dist;
	} else if ((s_cdt->abs > 0.0 + ON_ZERO_TOLERANCE)
		   || (s_cdt->norm > 0.0 + ON_ZERO_TOLERANCE)) {
	    within_dist = dist;
	} else {
	    within_dist = 0.01 * dist; // default to 1% minimum surface distance
	}

	if (s_cdt->norm > 0.0 + ON_ZERO_TOLERANCE) {
	    cos_within_ang = cos(s_cdt->norm);
	} else {
	    cos_within_ang = cos(ON_PI / 2.0);
	}
	ON_BOOL32 uclosed = s->IsClosed(0);
	ON_BOOL32 vclosed = s->IsClosed(1);
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
getUVCurveSamples(
	          const ON_Surface *s,
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
    ON_Interval range = curve->Domain();
    ON_3dPoint mid_2d(0.0, 0.0, 0.0);
    ON_3dPoint mid_3d(0.0, 0.0, 0.0);
    ON_3dVector mid_norm(0.0, 0.0, 0.0);
    ON_3dVector mid_tang(0.0, 0.0, 0.0);
    fastf_t t = (t1 + t2) / 2.0;

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


static std::map<double, ON_3dPoint *> *
getUVCurveSamples(struct ON_Brep_CDT_State *s_cdt,
	          const ON_Surface *surf,
		  const ON_Curve *curve,
		  fastf_t max_dist)
{
    fastf_t min_dist, within_dist, cos_within_ang;

    double dist = 1000.0;

    bool bGrowBox = false;
    ON_3dPoint min, max;
    if (curve->GetBoundingBox(min, max, bGrowBox)) {
	dist = DIST_PT_PT(min, max);
    }

    if (s_cdt->abs < s_cdt->dist + ON_ZERO_TOLERANCE) {
	min_dist = s_cdt->dist;
    } else {
	min_dist = s_cdt->abs;
    }

    double rel = 0.0;
    if (s_cdt->rel > 0.0 + ON_ZERO_TOLERANCE) {
	rel = s_cdt->rel * dist;
	if (max_dist < rel * 10.0) {
	    max_dist = rel * 10.0;
	}
	within_dist = rel < min_dist ? min_dist : rel;
    } else if (s_cdt->abs > 0.0 + ON_ZERO_TOLERANCE) {
	within_dist = min_dist;
    } else {
	within_dist = 0.01 * dist; // default to 1% minimum surface distance
    }

    if (s_cdt->norm > 0.0 + ON_ZERO_TOLERANCE) {
	cos_within_ang = cos(s_cdt->norm);
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
	    if ((seam = IsAtSeam(surf, btp.p2d, same_point_tolerance)) > 0) {
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
    for (int i = 1; i < num_points; i++) {
	if (surf->IsClosed(0)) {
	    double delta = brep_loop_points[i].p2d.x - brep_loop_points[i - 1].p2d.x;
	    while (fabs(delta) > ulength / 2.0) {
		if (delta < 0.0) {
		    brep_loop_points[i].p2d.x += ulength; // east bound
		} else {
		    brep_loop_points[i].p2d.x -= ulength;; // west bound
		}
		delta = brep_loop_points[i].p2d.x - brep_loop_points[i - 1].p2d.x;
	    }
	}
	if (surf->IsClosed(1)) {
	    double delta = brep_loop_points[i].p2d.y - brep_loop_points[i - 1].p2d.y;
	    while (fabs(delta) > vlength / 2.0) {
		if (delta < 0.0) {
		    brep_loop_points[i].p2d.y += vlength; // north bound
		} else {
		    brep_loop_points[i].p2d.y -= vlength;; // south bound
		}
		delta = brep_loop_points[i].p2d.y - brep_loop_points[i - 1].p2d.y;
	    }
	}
    }

    return true;
}

static void
get_loop_sample_points(
	struct ON_Brep_CDT_State *s_cdt,
	ON_SimpleArray<BrepTrimPoint> *points,
	const ON_BrepFace &face,
	const ON_BrepLoop *loop,
	fastf_t max_dist,
	std::map<int, ON_3dPoint *> *vert_pnts,
	std::map<int, ON_3dPoint *> *vert_norms
	)
{
    int trim_count = loop->TrimCount();

    for (int lti = 0; lti < trim_count; lti++) {
	ON_BrepTrim *trim = loop->Trim(lti);
	ON_BrepEdge *edge = trim->Edge();

	/* Provide 2D points for p2d, but we need to be aware that this will result
	 * in degenerate 3D faces that we need to filter out when assembling a mesh */
	if (trim->m_type == ON_BrepTrim::singular) {
	    BrepTrimPoint btp;
	    const ON_BrepVertex& v1 = face.Brep()->m_V[trim->m_vi[0]];
	    ON_3dPoint *p3d = (*s_cdt->vert_pnts)[v1.m_vertex_index];
	    double delta =  trim->Domain().Length() / 10.0;
	    ON_Interval trim_dom = trim->Domain();

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

	if (!trim->m_trim_user.p) {
	    (void)getEdgePoints(s_cdt, edge, *trim, max_dist, vert_pnts, vert_norms);
	    //bu_log("Initialized trim->m_trim_user.p: Trim %d (associated with Edge %d) point count: %zd\n", trim->m_trim_index, trim->Edge()->m_edge_index, m->size());
	}
	if (trim->m_trim_user.p) {
	    std::map<double, BrepTrimPoint *> *param_points3d = (std::map<double, BrepTrimPoint *> *) trim->m_trim_user.p;
	    //bu_log("Trim %d (associated with Edge %d) point count: %zd\n", trim->m_trim_index, trim->Edge()->m_edge_index, param_points3d->size());

	    ON_3dPoint boxmin;
	    ON_3dPoint boxmax;

	    if (trim->GetBoundingBox(boxmin, boxmax, false)) {
		double t0, t1;

		std::map<double, BrepTrimPoint*>::const_iterator i;
		std::map<double, BrepTrimPoint*>::const_iterator ni;

		trim->GetDomain(&t0, &t1);
		for (i = param_points3d->begin(); i != param_points3d->end();) {
		    BrepTrimPoint *btp = (*i).second;
		    ni = ++i;
		    // skip last point of trim if not last trim
		    if (ni == param_points3d->end()) {
			if (lti < trim_count - 1) {
			    continue;
			}
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
	ON_SimpleArray<BrepTrimPoint> *brep_loop_points,
	double same_point_tolerance)
{
    int loop_cnt = face.LoopCount();
    for (int li = 0; li < loop_cnt; li++) {
	int num_loop_points = brep_loop_points[li].Count();
	if (num_loop_points > 1) {
	    for (int i = 0; i < num_loop_points; i++) {
		ON_2dPoint &p = brep_loop_points[li][i].p2d;
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
	ON_SimpleArray<BrepTrimPoint> *brep_loop_points)
{
    int loop_cnt = face.LoopCount();
    // extend loop points over seam if needed.
    for (int li = 0; li < loop_cnt; li++) {
	int num_loop_points = brep_loop_points[li].Count();
	if (num_loop_points > 1) {
	    if (!extend_over_seam_crossings(s, brep_loop_points[li])) {
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
	ON_SimpleArray<BrepTrimPoint> *brep_loop_points,
	double same_point_tolerance)
{
    int loop_cnt = face.LoopCount();
    for (int li = 0; li < loop_cnt; li++) {
	int num_loop_points = brep_loop_points[li].Count();
	if (num_loop_points > 1) {
	    ON_2dPoint brep_loop_begin = brep_loop_points[li][0].p2d;
	    ON_2dPoint brep_loop_end = brep_loop_points[li][num_loop_points - 1].p2d;

	    if (!V2NEAR_EQUAL(brep_loop_begin, brep_loop_end, same_point_tolerance)) {
		if (LoopStraddlesDomain(s, brep_loop_points[li])) {
		    // reorder loop points
		    shift_loop_straddled_over_seam(s, brep_loop_points[li], same_point_tolerance);
		}
	    }
	}
    }
}


// process through closing open loops that begin and end on closed seam
static void
CloseOpenLoops(
	struct ON_Brep_CDT_State *s_cdt,
	const ON_Surface *s,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> *brep_loop_points,
	double same_point_tolerance)
{
    std::list<std::map<double, ON_3dPoint *> *> bridgePoints;
    int loop_cnt = face.LoopCount();
    for (int li = 0; li < loop_cnt; li++) {
	int num_loop_points = brep_loop_points[li].Count();
	if (num_loop_points > 1) {
	    ON_2dPoint brep_loop_begin = brep_loop_points[li][0].p2d;
	    ON_2dPoint brep_loop_end = brep_loop_points[li][num_loop_points - 1].p2d;
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
			    int rnum_loop_points = brep_loop_points[rli].Count();
			    ON_2dPoint rbrep_loop_begin = brep_loop_points[rli][0].p2d;
			    ON_2dPoint rbrep_loop_end = brep_loop_points[rli][rnum_loop_points - 1].p2d;
			    if (!V2NEAR_EQUAL(rbrep_loop_begin, rbrep_loop_end, same_point_tolerance)) {
				if (IsAtSeam(s, rbrep_loop_begin, same_point_tolerance) && IsAtSeam(s, rbrep_loop_end, same_point_tolerance)) {
				    double t0, t1;
				    ON_LineCurve line1(brep_loop_end, rbrep_loop_begin);
				    std::map<double, ON_3dPoint *> *linepoints3d = getUVCurveSamples(s_cdt, s, &line1, 1000.0);
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
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li].Append(btp);
				    }
				    //brep_loop_points[li].Append(brep_loop_points[rli].Count(),brep_loop_points[rli].Array());
				    for (int j = 1; j < rnum_loop_points; j++) {
					brep_loop_points[li].Append(brep_loop_points[rli][j]);
				    }
				    ON_LineCurve line2(rbrep_loop_end, brep_loop_begin);
				    linepoints3d = getUVCurveSamples(s_cdt, s, &line2, 1000.0);
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
					btp.p2d = line2.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li].Append(btp);
				    }
				    brep_loop_points[rli].Empty();
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
				    std::map<double, ON_3dPoint *> *linepoints3d = getUVCurveSamples(s_cdt, s, &line1, 1000.0);
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
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li].Append(btp);
				    }
				    line1.SetStartPoint(p);
				    p.x = u.m_t[1];
				    line1.SetEndPoint(p);
				    linepoints3d = getUVCurveSamples(s_cdt, s, &line1, 1000.0);
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
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li].Append(btp);
				    }
				    line1.SetStartPoint(p);
				    line1.SetEndPoint(brep_loop_begin);
				    linepoints3d = getUVCurveSamples(s_cdt, s, &line1, 1000.0);
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
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li].Append(btp);
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
				    std::map<double, ON_3dPoint *> *linepoints3d = getUVCurveSamples(s_cdt, s, &line1, 1000.0);
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
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li].Append(btp);
				    }
				    line1.SetStartPoint(p);
				    p.x = u.m_t[0];
				    line1.SetEndPoint(p);
				    linepoints3d = getUVCurveSamples(s_cdt, s, &line1, 1000.0);
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
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li].Append(btp);
				    }
				    line1.SetStartPoint(p);
				    line1.SetEndPoint(brep_loop_begin);
				    linepoints3d = getUVCurveSamples(s_cdt, s, &line1, 1000.0);
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
					btp.p2d = line1.PointAt(t0 + (t1 - t0) * btp.t);
					btp.e = ON_UNSET_VALUE;
					brep_loop_points[li].Append(btp);
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
	ON_SimpleArray<BrepTrimPoint> *brep_loop_points)
{
    int loop_cnt = face.LoopCount();
    if (loop_cnt > 1) { // has inner loops or holes
	for( int li = 1; li < loop_cnt; li++) {
	    ON_2dPoint p2d((brep_loop_points[li])[0].p2d.x, (brep_loop_points[li])[0].p2d.y);
	    if (!PointInPolygon(p2d, brep_loop_points[0])) {
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
			if (PointInPolygon(sftd_pt, brep_loop_points[0])) {
			    // shift all U accordingly
			    ShiftPoints(brep_loop_points[li], ushift, 0.0);
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
			if (PointInPolygon(sftd_pt, brep_loop_points[0])) {
			    // shift all V accordingly
			    ShiftPoints(brep_loop_points[li], 0.0, vshift);
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
			    if (PointInPolygon(sftd_pt, brep_loop_points[0])) {
				// shift all U & V accordingly
				ShiftPoints(brep_loop_points[li], ushift, vshift);
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
			if (PointInPolygon(sftd_pt, brep_loop_points[0])) {
			    // shift all U accordingly
			    ShiftPoints(brep_loop_points[li], ushift, 0.0);
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
			if (PointInPolygon(sftd_pt, brep_loop_points[0])) {
			    // shift all V accordingly
			    ShiftPoints(brep_loop_points[li], 0.0, vshift);
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
	struct ON_Brep_CDT_State *s_cdt,
	const ON_Surface *s,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> *brep_loop_points,
	double same_point_tolerance)
{
    // force near seam points to seam.
    ForceNearSeamPointsToSeam(s, face, brep_loop_points, same_point_tolerance);

    // extend loop points over closed seam if needed.
    ExtendPointsOverClosedSeam(s, face, brep_loop_points);

    // shift open loops that straddle a closed seam with the intent of closure at the surface boundary.
    ShiftLoopsThatStraddleSeam(s, face, brep_loop_points, same_point_tolerance);

    // process through closing open loops that begin and end on closed seam
    CloseOpenLoops(s_cdt, s, face, brep_loop_points, same_point_tolerance);

    // process through to make sure inner hole loops are actually inside of outer polygon
    // need to make sure that any hole polygons are properly shifted over correct closed seams
    ShiftInnerLoops(s, face, brep_loop_points);
}



struct ON_Brep_CDT_State *
ON_Brep_CDT_Create(ON_Brep *brep)
{
    struct ON_Brep_CDT_State *cdt = new struct ON_Brep_CDT_State;
    cdt->brep = brep;

    cdt->w3dpnts = new std::vector<ON_3dPoint *>;
    cdt->vert_pnts = new std::map<int, ON_3dPoint *>;
    cdt->w3dnorms = new std::vector<ON_3dPoint *>;
    cdt->vert_norms = new std::map<int, ON_3dPoint *>;
    cdt->vert_to_on = new std::map<int, ON_3dPoint *>;
    cdt->edge_pnts = new std::set<ON_3dPoint *>;

    cdt->p2t_faces = (p2t::CDT **)bu_calloc(brep->m_F.Count(), sizeof(p2t::CDT *), "poly2tri triangulations");
    cdt->p2t_extra_faces = (std::vector<p2t::Triangle *> **)bu_calloc(brep->m_F.Count(), sizeof(std::vector<p2t::Triangle *> *), "extra p2t faces");
    cdt->p2t_maps = (std::map<p2t::Point *, ON_3dPoint *> **)bu_calloc(brep->m_F.Count(), sizeof(std::map<p2t::Point *, ON_3dPoint *> *), "poly2tri point to ON_3dPoint maps");
    cdt->p2t_nmaps = (std::map<p2t::Point *, ON_3dPoint *> **)bu_calloc(brep->m_F.Count(), sizeof(std::map<p2t::Point *, ON_3dPoint *> *), "poly2tri point to ON_3dVector normal maps");

    /* Set status to "never evaluated" */
    cdt->status = BREP_CDT_UNTESSELLATED;

    /* Set sane default tolerances.  May want to do
     * something better (perhaps brep dimension based...) */
    cdt->abs = BREP_CDT_DEFAULT_TOL_ABS;
    cdt->rel = BREP_CDT_DEFAULT_TOL_REL ;
    cdt->norm = BREP_CDT_DEFAULT_TOL_NORM ;
    cdt->dist = BREP_CDT_DEFAULT_TOL_DIST ;

    return cdt;
}

void
ON_Brep_CDT_Destroy(struct ON_Brep_CDT_State *s_cdt)
{
    for (size_t i = 0; i < s_cdt->w3dpnts->size(); i++) {
	delete (*(s_cdt->w3dpnts))[i];
    }
    for (size_t i = 0; i < s_cdt->w3dnorms->size(); i++) {
	delete (*(s_cdt->w3dnorms))[i];
    }

    for (int i = 0; i < s_cdt->brep->m_F.Count(); i++) {
	std::vector<p2t::Triangle *> *ef = s_cdt->p2t_extra_faces[i];
	if (ef) {
	    std::vector<p2t::Triangle *>::iterator trit;
	    for (trit = ef->begin(); trit != ef->end(); trit++) {
		p2t::Triangle *t = *trit;
		delete t;
	    }
	}
    }

    delete s_cdt->w3dpnts;
    delete s_cdt->vert_pnts;
    delete s_cdt->w3dnorms;
    delete s_cdt->vert_norms;
    delete s_cdt->vert_to_on;
    delete s_cdt->edge_pnts;
    delete s_cdt->p2t_extra_faces;

    // TODO - delete p2t data

    delete s_cdt;
}

int
ON_Brep_CDT_Status(struct ON_Brep_CDT_State *s_cdt)
{
    return s_cdt->status;
}


ON_Brep *
ON_Brep_CDT_Brep(struct ON_Brep_CDT_State *s_cdt)
{
    return s_cdt->brep;
}

void
ON_Brep_CDT_Tol_Set(struct ON_Brep_CDT_State *s, const struct ON_Brep_CDT_Tols *t)
{
    if (!s) {
	return;
    }

    if (!t) {
	/* reset to defaults */
	s->abs = BREP_CDT_DEFAULT_TOL_ABS;
	s->rel = BREP_CDT_DEFAULT_TOL_REL ;
	s->norm = BREP_CDT_DEFAULT_TOL_NORM ;
	s->dist = BREP_CDT_DEFAULT_TOL_DIST ;
    }

    s->abs = t->abs;
    s->rel = t->rel;
    s->norm = t->norm;
    s->dist = t->dist;
}

void
ON_Brep_CDT_Tol_Get(struct ON_Brep_CDT_Tols *t, const struct ON_Brep_CDT_State *s)
{
    if (!t) {
	return;
    }

    if (!s) {
	/* set to defaults */
	t->abs = BREP_CDT_DEFAULT_TOL_ABS;
	t->rel = BREP_CDT_DEFAULT_TOL_REL ;
	t->norm = BREP_CDT_DEFAULT_TOL_NORM ;
	t->dist = BREP_CDT_DEFAULT_TOL_DIST ;
    }

    t->abs = s->abs;
    t->rel = s->rel;
    t->norm = s->norm;
    t->dist = s->dist;
}

static int
ON_Brep_CDT_Face(struct ON_Brep_CDT_State *s_cdt, std::map<const ON_Surface *, double> *s_to_maxdist, ON_BrepFace &face)
{
    ON_RTree rt_trims;
    ON_2dPointArray on_surf_points;
    const ON_Surface *s = face.SurfaceOf();
    int loop_cnt = face.LoopCount();
    ON_2dPointArray on_loop_points;
    ON_SimpleArray<BrepTrimPoint> *brep_loop_points = new ON_SimpleArray<BrepTrimPoint>[loop_cnt];
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = new std::map<p2t::Point *, ON_3dPoint *>();
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = new std::map<p2t::Point *, ON_3dPoint *>();
    std::vector<p2t::Point*> polyline;
    p2t::CDT* cdt = NULL;

    // first simply load loop point samples
    for (int li = 0; li < loop_cnt; li++) {
	double max_dist = 0.0;
	const ON_BrepLoop *loop = face.Loop(li);
	if (s_to_maxdist->find(face.SurfaceOf()) != s_to_maxdist->end()) {
	    max_dist = (*s_to_maxdist)[face.SurfaceOf()];
	}
	get_loop_sample_points(s_cdt, &brep_loop_points[li], face, loop, max_dist, s_cdt->vert_pnts, s_cdt->vert_norms);
    }

    std::list<std::map<double, ON_3dPoint *> *> bridgePoints;
    if (s->IsClosed(0) || s->IsClosed(1)) {
	PerformClosedSurfaceChecks(s_cdt, s, face, brep_loop_points, BREP_SAME_POINT_TOLERANCE);

    }
    // process through loops building polygons.
    bool outer = true;
    for (int li = 0; li < loop_cnt; li++) {
	int num_loop_points = brep_loop_points[li].Count();
	if (num_loop_points > 2) {
	    for (int i = 1; i < num_loop_points; i++) {
		// map point to last entry to 3d point
		p2t::Point *p = new p2t::Point((brep_loop_points[li])[i].p2d.x, (brep_loop_points[li])[i].p2d.y);
		polyline.push_back(p);
		(*pointmap)[p] = (brep_loop_points[li])[i].p3d;
		(*normalmap)[p] = (brep_loop_points[li])[i].n3d;
	    }
	    for (int i = 1; i < brep_loop_points[li].Count(); i++) {
		// map point to last entry to 3d point
		ON_Line *line = new ON_Line((brep_loop_points[li])[i - 1].p2d, (brep_loop_points[li])[i].p2d);
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
		cdt = new p2t::CDT(polyline);
		outer = false;
	    } else {
		cdt->AddHole(polyline);
	    }
	    polyline.clear();
	}
    }
    delete [] brep_loop_points;

    // TODO - we may need to add 2D points on trims that the edges didn't know
    // about.  Since 3D points must be shared along edges and we're using
    // synchronized numbers of parametric domain ordered 2D and 3D points to
    // make that work, we will need to track new 2D points and update the
    // corresponding 3D edge based data structures.  More than that - we must
    // also update all 2D structures in all other faces associated with the
    // edge in question to keep things in overall sync.

    // TODO - if we are going to apply clipper boolean resolution to sets of
    // face loops, that would come here - once we have assembled the loop
    // polygons for the faces. That also has the potential to generate "new" 3D
    // points on edges that are driven by 2D boolean intersections between
    // trimming loops, and may require another update pass as above.

    if (outer) {
	std::cerr << "Error: Face(" << face.m_face_index << ") cannot evaluate its outer loop and will not be facetized." << std::endl;
	return -1;
    }

    getSurfacePoints(s_cdt, face, on_surf_points);

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
		if (NEAR_ZERO(dist, s_cdt->dist)) {
		    on_edge = true;
		    break;
		}
	    }
	    if (!on_edge) {
		cdt->AddPoint(new p2t::Point(p->x, p->y));
	    }
	} else {
	    cdt->AddPoint(new p2t::Point(p->x, p->y));
	}
    }

    ON_SimpleArray<void*> results;
    ON_BoundingBox bb = rt_trims.BoundingBox();

    rt_trims.Search2d((const double *) bb.m_min, (const double *) bb.m_max,
	    results);

    if (results.Count() > 0) {
	for (int ri = 0; ri < results.Count(); ri++) {
	    const ON_Line *l = (const ON_Line *)*results.At(ri);
	    delete l;
	}
    }
    rt_trims.RemoveAll();

    cdt->Triangulate(true, -1);

    /* Calculate any 3D points we don't already have */
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	for (size_t j = 0; j < 3; j++) {
	    p2t::Point *p = t->GetPoint(j);
	    if (p) {
		ON_3dPoint *op = (*pointmap)[p];
		ON_3dPoint *onorm = (*normalmap)[p];
		if (!op || !onorm) {
		    /* We've got some calculating to do... */
		    const ON_Surface *surf = face.SurfaceOf();
		    ON_3dPoint pnt;
		    ON_3dVector norm;
		    if (surface_EvNormal(surf, p->x, p->y, pnt, norm)) {
			if (face.m_bRev) {
			    norm = norm * -1.0;
			}
			if (!op) {
			    op = new ON_3dPoint(pnt);
			    CDT_Add3DPnt(s_cdt, op, face.m_face_index, -1, -1, -1, p->x, p->y);
			    (*pointmap)[p] = op;
			}
			if (!onorm) {
			    onorm = new ON_3dPoint(norm);
			    s_cdt->w3dnorms->push_back(onorm);
			    (*normalmap)[p] = onorm;
			}
		    } else {
			if (!op) {
			    pnt = s->PointAt(p->x, p->y);
			    op = new ON_3dPoint(pnt);
			    CDT_Add3DPnt(s_cdt, op, face.m_face_index, -1, -1, -1, p->x, p->y);
			}
			(*pointmap)[p] = op;
		    }
		}
	    } else {
		bu_log("Face %d: p2t face without proper point info...\n", face.m_face_index);
	    }
	}
    }

    /* Stash mappings for BoT reassembly */
    int face_index = face.m_face_index;
    s_cdt->p2t_faces[face_index] = cdt;
    s_cdt->p2t_maps[face_index] = pointmap;
    s_cdt->p2t_nmaps[face_index] = normalmap;

    return 0;
}

int
ON_Brep_CDT_Tessellate(struct ON_Brep_CDT_State *s_cdt, std::vector<int> *faces)
{

    ON_wString wstr;
    ON_TextLog tl(wstr);

    ON_Brep* brep = s_cdt->brep;

    // Check for any conditions that are show-stoppers
    ON_wString wonstr;
    ON_TextLog vout(wonstr);
    if (!brep->IsValid(&vout)) {
	bu_log("brep is NOT valid, cannot produce watertight mesh\n");
	//return -1;
    }

    for (int index = 0; index < brep->m_E.Count(); index++) {
        ON_BrepEdge& edge = brep->m_E[index];
        if (edge.TrimCount() != 2) {
            bu_log("Edge %d trim count: %d - can't (yet) do watertight meshing\n", edge.m_edge_index, edge.TrimCount());
            return -1;
        }
    }

    // Reparameterize the face's surface and transform the "u" and "v"
    // coordinates of all the face's parameter space trimming curves to
    // minimize distortion in the map from parameter space to 3d..
    if (!s_cdt->w3dpnts->size()) {
	for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	    ON_BrepFace *face = brep->Face(face_index);
	    const ON_Surface *s = face->SurfaceOf();
	    double surface_width, surface_height;
	    if (s->GetSurfaceSize(&surface_width, &surface_height)) {
		face->SetDomain(0, 0.0, surface_width);
		face->SetDomain(1, 0.0, surface_height);
	    }
	}
    }

    /* We want to use ON_3dPoint pointers and BrepVertex points, but
     * vert->Point() produces a temporary address.  If this is our first time
     * through, make stable copies of the Vertex points. */
    if (!s_cdt->w3dpnts->size()) {
	for (int index = 0; index < brep->m_V.Count(); index++) {
	    ON_BrepVertex& v = brep->m_V[index];
	    (*s_cdt->vert_pnts)[index] = new ON_3dPoint(v.Point());
	    CDT_Add3DPnt(s_cdt, (*s_cdt->vert_pnts)[index], -1, v.m_vertex_index, -1, -1, -1, -1);
	    // topologically, any vertex point will be on edges
	    s_cdt->edge_pnts->insert((*s_cdt->vert_pnts)[index]);
	}
    }

    /* If this is the first time through, get vertex normals that are the
     * average of the surface points at the junction.  Use these to catch
     * and correct surface normals that are in the "wrong" direction for
     * the vertex.
     */
    if (!s_cdt->w3dnorms->size()) {
	for (int index = 0; index < brep->m_V.Count(); index++) {
	    ON_BrepVertex& v = brep->m_V[index];
	    ON_3dVector vnrml(0.0, 0.0, 0.0);

	    for (int eind = 0; eind != v.EdgeCount(); eind++) {
		ON_3dPoint tmp1, tmp2;
		ON_3dVector trim1_norm = ON_3dVector::UnsetVector;
		ON_3dVector trim2_norm = ON_3dVector::UnsetVector;
		ON_BrepEdge& edge = brep->m_E[v.m_ei[eind]];
		ON_BrepTrim *trim1 = edge.Trim(0);
		ON_BrepTrim *trim2 = edge.Trim(1);
		ON_Interval t1range = trim1->Domain();
		ON_Interval t2range = trim2->Domain();
		double t1 = ((eind && trim1->m_bRev3d) || (!eind && !trim1->m_bRev3d)) ? t1range[0] : t1range[1];
		double t2 = ((eind && trim2->m_bRev3d) || (!eind && !trim2->m_bRev3d)) ? t2range[0] : t2range[1];
		ON_3dPoint t1_2d = trim1->PointAt(t1);
		ON_3dPoint t2_2d = trim2->PointAt(t2);
		const ON_Surface *s1 = trim1->SurfaceOf();
		const ON_Surface *s2 = trim2->SurfaceOf();
		if (surface_EvNormal(s1, t1_2d.x, t1_2d.y, tmp1, trim1_norm)) {
		    if (trim1->Face()->m_bRev) {
			trim1_norm = trim1_norm * -1;
		    }
		    vnrml += trim1_norm;
		}
		if (surface_EvNormal(s2, t2_2d.x, t2_2d.y, tmp2, trim2_norm)) {
		    if (trim2->Face()->m_bRev) {
			trim2_norm = trim2_norm * -1;
		    }
		    vnrml += trim2_norm;
		}
	    }
	    vnrml.Unitize();

	    // We store this as a point to keep C++ happy...  If we try to
	    // propagate the ON_3dVector type through all the CDT logic it
	    // triggers issues with the compile.
	    (*s_cdt->vert_norms)[index] = new ON_3dPoint(vnrml);
	    s_cdt->w3dnorms->push_back((*s_cdt->vert_norms)[index]);
	}
    }

    /* To generate watertight meshes, the faces must share 3D edge points.  To ensure
     * a uniform set of edge points, we first sample all the edges and build their
     * point sets */
    std::map<const ON_Surface *, double> s_to_maxdist;
    for (int index = 0; index < brep->m_E.Count(); index++) {
        ON_BrepEdge& edge = brep->m_E[index];
        ON_BrepTrim& trim1 = brep->m_T[edge.Trim(0)->m_trim_index];
        ON_BrepTrim& trim2 = brep->m_T[edge.Trim(1)->m_trim_index];

        // Get distance tolerances from the surface sizes
        fastf_t max_dist = 0.0;
        fastf_t md1, md2 = 0.0;
        double sw1, sh1, sw2, sh2;
        const ON_Surface *s1 = trim1.Face()->SurfaceOf();
        const ON_Surface *s2 = trim2.Face()->SurfaceOf();
        if (s1->GetSurfaceSize(&sw1, &sh1) && s2->GetSurfaceSize(&sw2, &sh2)) {
            if ((sw1 < s_cdt->dist) || (sh1 < s_cdt->dist) || sw2 < s_cdt->dist || sh2 < s_cdt->dist) {
                return -1;
            }
            md1 = sqrt(sw1 * sh1 + sh1 * sw1) / 10.0;
            md2 = sqrt(sw2 * sh2 + sh2 * sw2) / 10.0;
            max_dist = (md1 < md2) ? md1 : md2;
            s_to_maxdist[s1] = max_dist;
            s_to_maxdist[s2] = max_dist;
        }

        // Generate the BrepTrimPoint arrays for both trims associated with this edge
	//
	// TODO - need to make this robust to changed tolerances on refinement
	// runs - if pre-existing solutions for "high level" splits exist,
	// reuse them and dig down to find where we need further refinement to
	// create new points.
        (void)getEdgePoints(s_cdt, &edge, trim1, max_dist, s_cdt->vert_pnts, s_cdt->vert_norms);

    }


    int face_failures = 0;
    int face_successes = 0;

    if (!faces) {

	for (int face_index = 0; face_index < s_cdt->brep->m_F.Count(); face_index++) {
	    ON_BrepFace &face = brep->m_F[face_index];

	    if (ON_Brep_CDT_Face(s_cdt, &s_to_maxdist, face)) {
		face_failures++;
	    } else {
		face_successes++;
	    }

	}
    } else {
	for (unsigned int i = 0; i < faces->size(); i++) {
	    if ((*faces)[i] < s_cdt->brep->m_F.Count()) {
		ON_BrepFace &face = brep->m_F[(*faces)[i]];
		if (ON_Brep_CDT_Face(s_cdt, &s_to_maxdist, face)) {
		    face_failures++;
		} else {
		    face_successes++;
		}
	    }
	}
    }

    // If we only tessellated some of the faces, we don't have the
    // full solid mesh yet (by definition).  Return accordingly.
    if (face_failures || !face_successes || face_successes < s_cdt->brep->m_F.Count()) {
	return (face_successes) ? 1 : -1;
    }

    /* We've got face meshes and no reported failures - check to see if we have a
     * solid mesh */
    int valid_fcnt, valid_vcnt;
    int *valid_faces = NULL;
    fastf_t *valid_vertices = NULL;
    struct bg_trimesh_solid_errors se = BG_TRIMESH_SOLID_ERRORS_INIT_NULL;

    if (ON_Brep_CDT_Mesh(&valid_faces, &valid_fcnt, &valid_vertices, &valid_vcnt, NULL, NULL, NULL, NULL, s_cdt) < 0) {
	return -1;
    }

    int invalid = bg_trimesh_solid2(valid_vcnt, valid_fcnt, valid_vertices, valid_faces, &se);

    if (invalid) {
	if (se.degenerate.count > 0) {
	    std::set<int> problem_pnts;
	    std::set<int>::iterator pp_it;
	    bu_log("%d degenerate faces\n", se.degenerate.count);
	    for (int i = 0; i < se.degenerate.count; i++) {
		int face = se.degenerate.faces[i];
		bu_log("dface %d: %d %d %d :  %f %f %f->%f %f %f->%f %f %f \n", face, valid_faces[face*3], valid_faces[face*3+1], valid_faces[face*3+2],
		       	valid_vertices[valid_faces[face*3]*3], valid_vertices[valid_faces[face*3]*3+1],valid_vertices[valid_faces[face*3]*3+2],
		       	valid_vertices[valid_faces[face*3+1]*3], valid_vertices[valid_faces[face*3+1]*3+1],valid_vertices[valid_faces[face*3+1]*3+2],
		       	valid_vertices[valid_faces[face*3+2]*3], valid_vertices[valid_faces[face*3+2]*3+1],valid_vertices[valid_faces[face*3+2]*3+2]);
		problem_pnts.insert(valid_faces[face*3]);
		problem_pnts.insert(valid_faces[face*3+1]);
		problem_pnts.insert(valid_faces[face*3+2]);
	    }
	    for (pp_it = problem_pnts.begin(); pp_it != problem_pnts.end(); pp_it++) {
		int pind = (*pp_it);
		ON_3dPoint *p = (*s_cdt->vert_to_on)[pind];
		if (!p) {
		    bu_log("unmapped point??? %d\n", pind);
		} else {
		    struct cdt_audit_info *paudit = s_cdt->pnt_audit_info[p];
		    if (!paudit) {
			bu_log("point with no audit info??? %d\n", pind);
		    } else {
			bu_log("point %d: Face(%d) Vert(%d) Trim(%d) Edge(%d) UV(%f,%f)\n", pind, paudit->face_index, paudit->vert_index, paudit->trim_index, paudit->edge_index, paudit->surf_uv.x, paudit->surf_uv.y);
		    }
		}
	    }
	}
	if (se.excess.count > 0) {
	    bu_log("extra edges???\n");
	}
	if (se.unmatched.count > 0) {
	    std::set<int> problem_pnts;
	    std::set<int>::iterator pp_it;

	    bu_log("%d unmatched edges\n", se.unmatched.count);
	    for (int i = 0; i < se.unmatched.count; i++) {
		int v1 = se.unmatched.edges[i*2];
		int v2 = se.unmatched.edges[i*2+1];
		bu_log("%d->%d: %f %f %f->%f %f %f\n", v1, v2,
			valid_vertices[v1*3], valid_vertices[v1*3+1], valid_vertices[v1*3+2],
			valid_vertices[v2*3], valid_vertices[v2*3+1], valid_vertices[v2*3+2]
			);
		for (int j = 0; j < valid_fcnt; j++) {
		    std::set<std::pair<int,int>> fedges;
		    fedges.insert(std::pair<int,int>(valid_faces[j*3],valid_faces[j*3+1]));
		    fedges.insert(std::pair<int,int>(valid_faces[j*3+1],valid_faces[j*3+2]));
		    fedges.insert(std::pair<int,int>(valid_faces[j*3+2],valid_faces[j*3]));
		    int has_edge = (fedges.find(std::pair<int,int>(v1,v2)) != fedges.end()) ? 1 : 0;
		    if (has_edge) {
			int face = j;
			bu_log("eface %d: %d %d %d :  %f %f %f->%f %f %f->%f %f %f \n", face, valid_faces[face*3], valid_faces[face*3+1], valid_faces[face*3+2],
				valid_vertices[valid_faces[face*3]*3], valid_vertices[valid_faces[face*3]*3+1],valid_vertices[valid_faces[face*3]*3+2],
				valid_vertices[valid_faces[face*3+1]*3], valid_vertices[valid_faces[face*3+1]*3+1],valid_vertices[valid_faces[face*3+1]*3+2],
				valid_vertices[valid_faces[face*3+2]*3], valid_vertices[valid_faces[face*3+2]*3+1],valid_vertices[valid_faces[face*3+2]*3+2]);
			problem_pnts.insert(valid_faces[j*3]);
			problem_pnts.insert(valid_faces[j*3+1]);
			problem_pnts.insert(valid_faces[j*3+2]);
		    }
		}
	    }
	    for (pp_it = problem_pnts.begin(); pp_it != problem_pnts.end(); pp_it++) {
		int pind = (*pp_it);
		ON_3dPoint *p = (*s_cdt->vert_to_on)[pind];
		if (!p) {
		    bu_log("unmapped point??? %d\n", pind);
		} else {
		    struct cdt_audit_info *paudit = s_cdt->pnt_audit_info[p];
		    if (!paudit) {
			bu_log("point with no audit info??? %d\n", pind);
		    } else {
			bu_log("point %d: Face(%d) Vert(%d) Trim(%d) Edge(%d) UV(%f,%f)\n", pind, paudit->face_index, paudit->vert_index, paudit->trim_index, paudit->edge_index, paudit->surf_uv.x, paudit->surf_uv.y);
		    }
		}
	    }
	}
	if (se.misoriented.count > 0) {
	    std::set<int> problem_pnts;
	    std::set<int>::iterator pp_it;

	    bu_log("%d misoriented edges\n", se.misoriented.count);
	    for (int i = 0; i < se.misoriented.count; i++) {
		int v1 = se.misoriented.edges[i*2];
		int v2 = se.misoriented.edges[i*2+1];
		bu_log("%d->%d: %f %f %f->%f %f %f\n", v1, v2,
			valid_vertices[v1*3], valid_vertices[v1*3+1], valid_vertices[v1*3+2],
			valid_vertices[v2*3], valid_vertices[v2*3+1], valid_vertices[v2*3+2]
			);
		for (int j = 0; j < valid_fcnt; j++) {
		    std::set<std::pair<int,int>> fedges;
		    fedges.insert(std::pair<int,int>(valid_faces[j*3],valid_faces[j*3+1]));
		    fedges.insert(std::pair<int,int>(valid_faces[j*3+1],valid_faces[j*3+2]));
		    fedges.insert(std::pair<int,int>(valid_faces[j*3+2],valid_faces[j*3]));
		    int has_edge = (fedges.find(std::pair<int,int>(v1,v2)) != fedges.end()) ? 1 : 0;
		    if (has_edge) {
			int face = j;
			bu_log("eface %d: %d %d %d :  %f %f %f->%f %f %f->%f %f %f \n", face, valid_faces[face*3], valid_faces[face*3+1], valid_faces[face*3+2],
				valid_vertices[valid_faces[face*3]*3], valid_vertices[valid_faces[face*3]*3+1],valid_vertices[valid_faces[face*3]*3+2],
				valid_vertices[valid_faces[face*3+1]*3], valid_vertices[valid_faces[face*3+1]*3+1],valid_vertices[valid_faces[face*3+1]*3+2],
				valid_vertices[valid_faces[face*3+2]*3], valid_vertices[valid_faces[face*3+2]*3+1],valid_vertices[valid_faces[face*3+2]*3+2]);
			problem_pnts.insert(valid_faces[j*3]);
			problem_pnts.insert(valid_faces[j*3+1]);
			problem_pnts.insert(valid_faces[j*3+2]);
		    }
		}
	    }
	    for (pp_it = problem_pnts.begin(); pp_it != problem_pnts.end(); pp_it++) {
		int pind = (*pp_it);
		ON_3dPoint *p = (*s_cdt->vert_to_on)[pind];
		if (!p) {
		    bu_log("unmapped point??? %d\n", pind);
		} else {
		    struct cdt_audit_info *paudit = s_cdt->pnt_audit_info[p];
		    if (!paudit) {
			bu_log("point with no audit info??? %d\n", pind);
		    } else {
			bu_log("point %d: Face(%d) Vert(%d) Trim(%d) Edge(%d) UV(%f,%f)\n", pind, paudit->face_index, paudit->vert_index, paudit->trim_index, paudit->edge_index, paudit->surf_uv.x, paudit->surf_uv.y);
		    }
		}
	    }
	}

    }

    bu_free(valid_faces, "faces");
    bu_free(valid_vertices, "vertices");

    if (invalid) {
	return 1;
    }

    return 0;

}

static int
ON_Brep_CDT_VList_Face(
	struct bu_list *vhead,
	struct bu_list *vlfree,
	int face_index,
	int mode,
	const struct ON_Brep_CDT_State *s)
{
    point_t pt[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    vect_t nv[3] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    point_t pt1 = VINIT_ZERO;
    point_t pt2 = VINIT_ZERO;

    p2t::CDT *cdt = s->p2t_faces[face_index];
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = s->p2t_maps[face_index];
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = s->p2t_nmaps[face_index];
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();

    switch (mode) {
	case 0:
	    // 3D shaded triangles
	    for (size_t i = 0; i < tris.size(); i++) {
		p2t::Triangle *t = tris[i];
		p2t::Point *p = NULL;
		for (size_t j = 0; j < 3; j++) {
		    p = t->GetPoint(j);
		    ON_3dPoint *op = (*pointmap)[p];
		    ON_3dPoint *onorm = (*normalmap)[p];
		    VSET(pt[j], op->x, op->y, op->z);
		    VSET(nv[j], onorm->x, onorm->y, onorm->z);
		}
		//tri one
		BN_ADD_VLIST(vlfree, vhead, nv[0], BN_VLIST_TRI_START);
		BN_ADD_VLIST(vlfree, vhead, nv[0], BN_VLIST_TRI_VERTNORM);
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_TRI_MOVE);
		BN_ADD_VLIST(vlfree, vhead, nv[1], BN_VLIST_TRI_VERTNORM);
		BN_ADD_VLIST(vlfree, vhead, pt[1], BN_VLIST_TRI_DRAW);
		BN_ADD_VLIST(vlfree, vhead, nv[2], BN_VLIST_TRI_VERTNORM);
		BN_ADD_VLIST(vlfree, vhead, pt[2], BN_VLIST_TRI_DRAW);
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_TRI_END);
		//bu_log("Face %d, Tri %zd: %f/%f/%f-%f/%f/%f -> %f/%f/%f-%f/%f/%f -> %f/%f/%f-%f/%f/%f\n", face_index, i, V3ARGS(pt[0]), V3ARGS(nv[0]), V3ARGS(pt[1]), V3ARGS(nv[1]), V3ARGS(pt[2]), V3ARGS(nv[2]));
	    }
	    break;
	case 1:
	    // 3D wireframe
	    for (size_t i = 0; i < tris.size(); i++) {
		p2t::Triangle *t = tris[i];
		p2t::Point *p = NULL;
		for (size_t j = 0; j < 3; j++) {
		    p = t->GetPoint(j);
		    ON_3dPoint *op = (*pointmap)[p];
		    VSET(pt[j], op->x, op->y, op->z);
		}
		//tri one
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_LINE_MOVE);
		BN_ADD_VLIST(vlfree, vhead, pt[1], BN_VLIST_LINE_DRAW);
		BN_ADD_VLIST(vlfree, vhead, pt[2], BN_VLIST_LINE_DRAW);
		BN_ADD_VLIST(vlfree, vhead, pt[0], BN_VLIST_LINE_DRAW);
	    }
	    break;
	case 2:
	    // 2D wireframe
	    for (size_t i = 0; i < tris.size(); i++) {
		p2t::Triangle *t = tris[i];
		p2t::Point *p = NULL;

		for (size_t j = 0; j < 3; j++) {
		    if (j == 0) {
			p = t->GetPoint(2);
		    } else {
			p = t->GetPoint(j - 1);
		    }
		    pt1[0] = p->x;
		    pt1[1] = p->y;
		    pt1[2] = 0.0;
		    p = t->GetPoint(j);
		    pt2[0] = p->x;
		    pt2[1] = p->y;
		    pt2[2] = 0.0;
		    BN_ADD_VLIST(vlfree, vhead, pt1, BN_VLIST_LINE_MOVE);
		    BN_ADD_VLIST(vlfree, vhead, pt2, BN_VLIST_LINE_DRAW);
		}
	    }
	    break;
	default:
	    return -1;
    }

    return 0;
}

int ON_Brep_CDT_VList(
	struct bn_vlblock *vbp,
	struct bu_list *vlfree,
	struct bu_color *c,
	int mode,
	const struct ON_Brep_CDT_State *s)
{
    int r, g, b;
    struct bu_list *vhead = NULL;
    int have_color = 0;

   if (UNLIKELY(!c) || mode < 0) {
       return -1;
   }

   have_color = bu_color_to_rgb_ints(c, &r, &g, &b);

   if (UNLIKELY(!have_color)) {
       return -1;
   }

   vhead = bn_vlblock_find(vbp, r, g, b);

   if (UNLIKELY(!vhead)) {
       return -1;
   }

   for (int i = 0; i < s->brep->m_F.Count(); i++) {
       if (s->p2t_faces[i]) {
	   (void)ON_Brep_CDT_VList_Face(vhead, vlfree, i, mode, s);
       }
   }

   return 0;
}

// Generate a BoT with normals.
// TODO - apply the bot validity testing routines.
int
ON_Brep_CDT_Mesh(
	int **faces, int *fcnt,
	fastf_t **vertices, int *vcnt,
	int **face_normals, int *fn_cnt,
	fastf_t **normals, int *ncnt,
	struct ON_Brep_CDT_State *s_cdt)
{
     if (!faces || !fcnt || !vertices || !vcnt || !s_cdt) {
	 return -1;
     }

     /* We can ignore the face normals if we want, but if some of the
      * return variables are non-NULL they all need to be non-NULL */
     if (face_normals || fn_cnt || normals || ncnt) {
	 if (!face_normals || !fn_cnt || !normals || !ncnt) {
	     return -1;
	 }
     }

    std::vector<ON_3dPoint *> vfpnts;
    std::vector<ON_3dPoint *> vfnormals;
    std::map<ON_3dPoint *, int> on_pnt_to_bot_pnt;
    std::map<ON_3dPoint *, int> on_pnt_to_bot_norm;
    std::set<p2t::Triangle*> tris_degen;
    std::set<p2t::Triangle*> tris_zero_3D_area;
    std::map<p2t::Triangle*, int> tri_brep_face;
    size_t triangle_cnt = 0;
    EdgeToTri *e2f = new EdgeToTri;

    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	p2t::CDT *cdt = s_cdt->p2t_faces[face_index];
	std::map<p2t::Point *, ON_3dPoint *> *pointmap = s_cdt->p2t_maps[face_index];
	std::map<p2t::Point *, ON_3dPoint *> *normalmap = s_cdt->p2t_nmaps[face_index];
	std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
	triangle_cnt += tris.size();
	for (size_t i = 0; i < tris.size(); i++) {
	    p2t::Triangle *t = tris[i];
	    tri_brep_face[t] = face_index;
	    ON_3dPoint *tpnts[3] = {NULL, NULL, NULL};
	    for (size_t j = 0; j < 3; j++) {
		p2t::Point *p = t->GetPoint(j);
		if (p) {
		    ON_3dPoint *op = (*pointmap)[p];
		    ON_3dPoint *onorm = (*normalmap)[p];
		    if (!op || !onorm) {
			/* We've got some calculating to do... */
			const ON_Surface *s = face.SurfaceOf();
			ON_3dPoint pnt;
			ON_3dVector norm;
			if (surface_EvNormal(s, p->x, p->y, pnt, norm)) {
			    if (face.m_bRev) {
				norm = norm * -1.0;
			    }
			    if (!op) {
				op = new ON_3dPoint(pnt);
				CDT_Add3DPnt(s_cdt, op, face.m_face_index, -1, -1, -1, p->x, p->y);
				vfpnts.push_back(op);
				(*pointmap)[p] = op;
				on_pnt_to_bot_pnt[op] = vfpnts.size() - 1;
				(*s_cdt->vert_to_on)[vfpnts.size() - 1] = op;
			    }
			    if (!onorm) {
				onorm = new ON_3dPoint(norm);
				s_cdt->w3dnorms->push_back(onorm);
				vfnormals.push_back(onorm);
				(*normalmap)[p] = onorm;
				on_pnt_to_bot_norm[op] = vfnormals.size() - 1;
			    }
			} else {
			    bu_log("Erm... eval failed, no normal info?\n");
			    if (!op) {
				pnt = s->PointAt(p->x, p->y);
				op = new ON_3dPoint(pnt);
				CDT_Add3DPnt(s_cdt, op, face.m_face_index, -1, -1, -1, p->x, p->y);
				vfpnts.push_back(op);
				(*pointmap)[p] = op;
				on_pnt_to_bot_pnt[op] = vfpnts.size() - 1;
				(*s_cdt->vert_to_on)[vfpnts.size() - 1] = op;
			    } else {
				if (on_pnt_to_bot_pnt.find(op) == on_pnt_to_bot_pnt.end()) {
				    vfpnts.push_back(op);
				    (*pointmap)[p] = op;
				    on_pnt_to_bot_pnt[op] = vfpnts.size() - 1;
				    (*s_cdt->vert_to_on)[vfpnts.size() - 1] = op;
				}
			    }
			}
		    } else {
			/* We've got them already, just add them */ 
			if (on_pnt_to_bot_pnt.find(op) == on_pnt_to_bot_pnt.end()) {
			    vfpnts.push_back(op);
			    vfnormals.push_back(onorm);
			    on_pnt_to_bot_pnt[op] = vfpnts.size() - 1;
			    (*s_cdt->vert_to_on)[vfpnts.size() - 1] = op;
			    on_pnt_to_bot_norm[op] = vfnormals.size() - 1;
			}
		    }
		    tpnts[j] = op;
		} else {
		    bu_log("Face %d: p2t face without proper point info...\n", face.m_face_index);
		}
	    }

	    /* Now that all 3D points are mapped, make sure this face isn't
	     * trivially degenerate (this can happen with singular trims) */
	    if (tpnts[0] == tpnts[1] || tpnts[1] == tpnts[2] || tpnts[2] == tpnts[0]) {
		/* degenerate */
		triangle_cnt--;
		tris_degen.insert(t);
		continue;
	    }

	    add_tri_edges(e2f, tris[i], pointmap);

	}

    }

    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	p2t::CDT *cdt = s_cdt->p2t_faces[face_index];
	std::map<p2t::Point *, ON_3dPoint *> *pointmap = s_cdt->p2t_maps[face_index];
	std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
	for (size_t i = 0; i < tris.size(); i++) {
	    p2t::Triangle *t = tris[i];

	    if (tris_degen.find(t) != tris_degen.end()) {
		continue;
	    }

	    ON_3dPoint *tpnts[3] = {NULL, NULL, NULL};
	    for (size_t j = 0; j < 3; j++) {
		p2t::Point *p = t->GetPoint(j);
		ON_3dPoint *op = (*pointmap)[p];
		tpnts[j] = op;
	    }

	    /* If we have a face with 3 shared or co-linear points, it's not
	     * trivially degenerate and we need to do more work.  (This can
	     * arise when the 2D triangulation has a non-degenerate triangle
	     * that maps degenerately into 3D). For now, just build up the set
	     * of degenerate triangles. */
	    ON_Line l(*tpnts[0], *tpnts[2]);
	    int is_zero_area = 0;
	    if (l.Length() < s_cdt->dist) {
		ON_Line l2(*tpnts[0], *tpnts[1]);
		if (l2.Length() < s_cdt->dist) {
		    bu_log("completely degenerate triangle\n");
		    triangle_cnt--;
		    tris_degen.insert(t);
		    continue;
		} else {
		    if (l2.DistanceTo(*tpnts[2]) < s_cdt->dist) {
			is_zero_area = 1;
		    }
		}
	    } else {
		if (l.DistanceTo(*tpnts[1]) < s_cdt->dist) {
		    is_zero_area = 1;
		}
	    }
	    if (is_zero_area) {
		// Remove this face from the edge to face maps
		ON_3dPoint *pt_A, *pt_B, *pt_C;
		p2t::Point *p2_A = t->GetPoint(0);
		p2t::Point *p2_B = t->GetPoint(1);
		p2t::Point *p2_C = t->GetPoint(2);
		pt_A = (*pointmap)[p2_A];
		pt_B = (*pointmap)[p2_B];
		pt_C = (*pointmap)[p2_C];
		(*e2f)[(mk_edge(pt_A, pt_B))].erase(t);
		(*e2f)[(mk_edge(pt_B, pt_C))].erase(t);
		(*e2f)[(mk_edge(pt_C, pt_A))].erase(t);
		triangle_cnt--;
		tris_degen.insert(t);
		tris_zero_3D_area.insert(t);
	    }
	}
    }

    /* For the zero-area triangles, we need to take each degenerate triangle
     * (1) and find the p2t triangle(s) that shares the longest edge of the
     * degenerate triangle (i.e. the edge that uses the two furthest endpoints
     * of the colinear polyline) (2).  Any non-degenerate faces using that edge
     * need to be split to incorporate the vertex that will end up otherwise
     * unused once the degenerate faces are removed.
     */
    std::map<p2t::Triangle *, p2t::Point *> midmap;
    std::map<p2t::Triangle *, Edge> lemap;
    if (tris_zero_3D_area.size()) {
	//bu_log("Found %zd near-zero area triangles\n", tris_zero_3D_area.size());

	// Step 1: For each zero area triangle, find the longest edge.
	std::set<p2t::Triangle*>::iterator tz_it;
	for (tz_it = tris_zero_3D_area.begin(); tz_it != tris_zero_3D_area.end(); tz_it++) {

	    p2t::Triangle *t = *tz_it;
	    std::map<p2t::Point *, ON_3dPoint *> *pointmap = s_cdt->p2t_maps[tri_brep_face[t]];
	    p2t::Point *p2_A = t->GetPoint(0);
	    p2t::Point *p2_B = t->GetPoint(1);
	    p2t::Point *p2_C = t->GetPoint(2);
	    ON_3dPoint *pA = (*pointmap)[p2_A];
	    ON_3dPoint *pB = (*pointmap)[p2_B];
	    ON_3dPoint *pC = (*pointmap)[p2_C];
	    fastf_t d1 = pA->DistanceTo(*pB);
	    fastf_t d2 = pB->DistanceTo(*pC);
	    fastf_t d3 = pC->DistanceTo(*pA);
	    if (d1 > d2 && d1 > d3) {
		lemap[t] = mk_edge(pA, pB);
		midmap[t] = p2_C;
	    }
	    if (d2 > d1 && d2 > d3) {
		lemap[t] = mk_edge(pB, pC);
		midmap[t] = p2_A;
	    }
	    if (d3 > d1 && d3 > d2) {
		lemap[t] = mk_edge(pC, pA);
		midmap[t] = p2_B;
	    }
	}

	// Step 2: For each zero area triangle, use the maps to look up the
	// mating non-degenerate triangle in the same face sharing the longest
	// edge .  Those triangles must be split, removed, and replaced by new
	// triangles.
	int rtriangle = 0;
	for (tz_it = tris_zero_3D_area.begin(); tz_it != tris_zero_3D_area.end(); tz_it++) {
	    p2t::Triangle *t = *tz_it;
	    int t_face_index = tri_brep_face[t];
	    Edge le = lemap[t];
	    EdgeToTri::iterator m_it = e2f->find(le);
	    int t_replace = 0;
	    if (m_it != e2f->end()) {
		std::set<p2t::Triangle*> &f = (*m_it).second;
		if (f.size()) {
		    std::set<p2t::Triangle*>::iterator f_it;
		    std::set<p2t::Triangle*> fdone;
		    for (f_it = f.begin(); f_it != f.end(); f_it++) {
			p2t::Triangle *m = *f_it;
			int face_index = tri_brep_face[m];
			if (t_face_index != face_index) {
			    continue;
			}
			t_replace++;
			//bu_log("found triangle to split(%d,%d) %d\n", rtriangle, face_index, t_replace);
			rtriangle++;
			std::map<p2t::Point *, ON_3dPoint *> *pointmap = s_cdt->p2t_maps[face_index];
			std::vector<p2t::Triangle *> *tri_add;
			if (!s_cdt->p2t_extra_faces[face_index]) {
			    tri_add = new std::vector<p2t::Triangle *>;
			    s_cdt->p2t_extra_faces[face_index] = tri_add;
			} else {
			    tri_add = s_cdt->p2t_extra_faces[face_index];
			}

			// TODO - the logic below only works if one extra point on the edge must be
			// handled for this triangle.  That's not going to cut it - NIST 2 has
			// an example of a case where we need to split triangles more than once.
			// For each non-zero-area triangle with two points on this edge, we
			// need to build the polygon with that triangle and all extra points on the
			// Brep edge contained by the triangle's edge.  Then, use poly2tri to
			// triangulate in the triangle's 2D plane (rather than the NURBS parametric
			// space) so we get a triangulation that doesn't have degenerate faces and
			// accounts for all points.  The 3D faces corresponding to the new triangulation
			// are the ones that will replace the old face.


			p2t::Point *p2_A = m->GetPoint(0);
			p2t::Point *p2_B = m->GetPoint(1);
			p2t::Point *p2_C = m->GetPoint(2);
			ON_3dPoint *pA = (*pointmap)[p2_A];
			ON_3dPoint *pB = (*pointmap)[p2_B];
			ON_3dPoint *pC = (*pointmap)[p2_C];
			p2t::Point *dmid = midmap[t];

			Edge se = lemap[t];
			Edge ne;
			p2t::Point *p2_1= NULL;
			p2t::Point *p2_2= NULL;
			p2t::Point *cmid = NULL;
			if ((se.first == pA && se.second == pB) || (se.first == pB && se.second == pA)) {
			    cmid = p2_C;
			    p2_1 = p2_A;
			    p2_2 = p2_B;
			}
			if ((se.first == pB && se.second == pC) || (se.first == pC && se.second == pB)) {
			    cmid = p2_A;
			    p2_1 = p2_B;
			    p2_2 = p2_C;
			}
			if ((se.first == pC && se.second == pA) || (se.first == pA && se.second == pC)) {
			    cmid = p2_B;
			    p2_1 = p2_C;
			    p2_2 = p2_A;
			}
			if (dmid && cmid) {
			    p2t::Triangle *t1 = new p2t::Triangle(*cmid, *p2_1, *dmid);
			    p2t::Triangle *t2 = new p2t::Triangle(*cmid, *dmid, *p2_2);
			    tris_degen.insert(m);
			    triangle_cnt--;
			    tri_add->push_back(t1);
			    tri_add->push_back(t2);
			} else {
			    bu_log("Error - failed to make new triangles\n");
			}
		    }
		}
	    }
	}
    }

    //bu_log("tri_cnt_init: %zd\n", triangle_cnt);
    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	std::vector<p2t::Triangle *> *tri_add = s_cdt->p2t_extra_faces[face_index];
	if (!tri_add) {
	    continue;
	}
	triangle_cnt += tri_add->size();
    }
    //bu_log("tri_cnt_init+: %zd\n", triangle_cnt);


    // Know how many faces and points now - initialize BoT container.
    *fcnt = (int)triangle_cnt;
    *faces = (int *)bu_calloc(triangle_cnt*3, sizeof(int), "new faces array");
    *vcnt = (int)vfpnts.size();
    *vertices = (fastf_t *)bu_calloc(vfpnts.size()*3, sizeof(fastf_t), "new vert array");
    if (normals) {
	*ncnt = (int)vfnormals.size();
	*normals = (fastf_t *)bu_calloc(vfnormals.size()*3, sizeof(fastf_t), "new normals array");
	*fn_cnt = (int)triangle_cnt;
	*face_normals = (int *)bu_calloc(triangle_cnt*3, sizeof(int), "new face_normals array");
    }

    for (size_t i = 0; i < vfpnts.size(); i++) {
	(*vertices)[i*3] = vfpnts[i]->x;
	(*vertices)[i*3+1] = vfpnts[i]->y;
	(*vertices)[i*3+2] = vfpnts[i]->z;
    }

    if (normals) {
	for (size_t i = 0; i < vfnormals.size(); i++) {
	    (*normals)[i*3] = vfnormals[i]->x;
	    (*normals)[i*3+1] = vfnormals[i]->y;
	    (*normals)[i*3+2] = vfnormals[i]->z;
	}
    }

    // Iterate over faces, adding points and faces to BoT container.  Note: all
    // 3D points should be geometrically unique in this final container.
    int face_cnt = 0;
    triangle_cnt = 0;
    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	p2t::CDT *cdt = s_cdt->p2t_faces[face_index];
	std::map<p2t::Point *, ON_3dPoint *> *pointmap = s_cdt->p2t_maps[face_index];
	std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
	triangle_cnt += tris.size();
	for (size_t i = 0; i < tris.size(); i++) {
	    p2t::Triangle *t = tris[i];
	    if (tris_degen.size() > 0 && tris_degen.find(t) != tris_degen.end()) {
		triangle_cnt--;
		continue;
	    }
	    if (tris_zero_3D_area.size() > 0 && tris_zero_3D_area.find(t) != tris_zero_3D_area.end()) {
		continue;
	    }
	    for (size_t j = 0; j < 3; j++) {
		p2t::Point *p = t->GetPoint(j);
		ON_3dPoint *op = (*pointmap)[p];
		int ind = on_pnt_to_bot_pnt[op];
		(*faces)[face_cnt*3 + j] = ind;
		if (normals) {
		    int nind = on_pnt_to_bot_norm[op];
		    (*face_normals)[face_cnt*3 + j] = nind;
		}
	    }
	    if (s_cdt->brep->m_F[face_index].m_bRev) {
		int ftmp = (*faces)[face_cnt*3 + 1];
		(*faces)[face_cnt*3 + 1] = (*faces)[face_cnt*3 + 2];
		(*faces)[face_cnt*3 + 2] = ftmp;
		if (normals) {
		    int fntmp = (*face_normals)[face_cnt*3 + 1];
		    (*face_normals)[face_cnt*3 + 1] = (*face_normals)[face_cnt*3 + 2];
		    (*face_normals)[face_cnt*3 + 2] = fntmp;
		}
	    }

	    face_cnt++;
	}
    }
    //bu_log("tri_cnt_1: %zd\n", triangle_cnt);
    //bu_log("face_cnt: %d\n", face_cnt);

    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	std::vector<p2t::Triangle *> *tri_add = s_cdt->p2t_extra_faces[face_index];
	std::map<p2t::Point *, ON_3dPoint *> *pointmap = s_cdt->p2t_maps[face_index];
	if (!tri_add) {
	    continue;
	}
	triangle_cnt += tri_add->size();
	for (size_t i = 0; i < tri_add->size(); i++) {
	    p2t::Triangle *t = (*tri_add)[i];
	    for (size_t j = 0; j < 3; j++) {
		p2t::Point *p = t->GetPoint(j);
		ON_3dPoint *op = (*pointmap)[p];
		int ind = on_pnt_to_bot_pnt[op];
		//bu_log("tri(%d)(%zu): %f %f %f -> %d\n", face_cnt, j, op->x, op->y, op->z, ind);
		(*faces)[face_cnt*3 + j] = ind;
		if (normals) {
		    int nind = on_pnt_to_bot_norm[op];
		    (*face_normals)[face_cnt*3 + j] = nind;
		}
	    }
	    if (s_cdt->brep->m_F[face_index].m_bRev) {
		int ftmp = (*faces)[face_cnt*3 + 1];
		(*faces)[face_cnt*3 + 1] = (*faces)[face_cnt*3 + 2];
		(*faces)[face_cnt*3 + 2] = ftmp;
		if (normals) {
		    int fntmp = (*face_normals)[face_cnt*3 + 1];
		    (*face_normals)[face_cnt*3 + 1] = (*face_normals)[face_cnt*3 + 2];
		    (*face_normals)[face_cnt*3 + 2] = fntmp;
		}
	    }

	    face_cnt++;
	}
	//bu_log("tri_cnt_2: %zd\n", triangle_cnt);
	//bu_log("face_cnt_2: %d\n", face_cnt);


    }


    /* Clear out extra faces so they don't pollute another pass */
    for (int i = 0; i < s_cdt->brep->m_F.Count(); i++) {
	std::vector<p2t::Triangle *> *ef = s_cdt->p2t_extra_faces[i];
	if (ef) {
	    std::vector<p2t::Triangle *>::iterator trit;
	    for (trit = ef->begin(); trit != ef->end(); trit++) {
		p2t::Triangle *t = *trit;
		delete t;
	    }
	    ef->clear();
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

