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
 * Routines for processing 3D edges
 */

#include "common.h"
#include "./cdt.h"

double
midpt_binary_search(fastf_t *tmid, const ON_BrepTrim *trim, double tstart, double tend, ON_3dPoint &edge_mid_3d, double tol, int verbose)
{
    double tcmid = (tstart + tend) / 2.0;
    ON_3dPoint trim_mid_2d = trim->PointAt(tcmid);
    const ON_Surface *s = trim->SurfaceOf();
    ON_3dPoint trim_mid_3d = s->PointAt(trim_mid_2d.x, trim_mid_2d.y);
    double dist = edge_mid_3d.DistanceTo(trim_mid_3d);

    if (dist > tol) {
	ON_3dPoint trim_start_2d = trim->PointAt(tstart);
	ON_3dPoint trim_end_2d = trim->PointAt(tend);
	ON_3dPoint trim_start_3d = s->PointAt(trim_start_2d.x, trim_start_2d.y);
	ON_3dPoint trim_end_3d = s->PointAt(trim_end_2d.x, trim_end_2d.y);

	ON_3dVector v1 = edge_mid_3d - trim_start_3d;
	ON_3dVector v2 = edge_mid_3d - trim_end_3d;

	if (verbose) {
	    bu_log("start point (%f %f %f) and end point (%f %f %f)\n", trim_start_3d.x, trim_start_3d.y, trim_start_3d.z, trim_end_3d.x, trim_end_3d.y, trim_end_3d.z);
	    bu_log("Note (%f:%f)%f - edge point (%f %f %f) and trim point (%f %f %f): %f, %f\n", tstart, tend, ON_DotProduct(v1,v2), edge_mid_3d.x, edge_mid_3d.y, edge_mid_3d.z, trim_mid_3d.x, trim_mid_3d.y, trim_mid_3d.z, dist, tol);
	}

	if (ON_DotProduct(v1,v2) < 0) {
	    if (verbose)
		bu_log("(%f - %f - %f (%f): searching left and right subspans\n", tstart, tcmid, tend, ON_DotProduct(v1,v2));
	    double tlmid, trmid;
	    double fldist = midpt_binary_search(&tlmid, trim, tstart, tcmid, edge_mid_3d, tol, 0);
	    double frdist = midpt_binary_search(&trmid, trim, tcmid, tend, edge_mid_3d, tol, 0);
	    if (fldist >= 0 && frdist < -1) {
		if (verbose)
		    bu_log("(%f - %f - %f: going with fldist: %f\n", tstart, tcmid, tend, fldist);
		(*tmid) = tlmid;
		return fldist;
	    }
	    if (frdist >= 0 && fldist < -1) {
		if (verbose)
		    bu_log("(%f - %f - %f: going with frdist: %f\n", tstart, tcmid, tend, frdist);
		(*tmid) = trmid;
		return frdist;
	    }
	    if (fldist < -1 && frdist < -1) {
		if (verbose)
		    bu_log("(%f - %f: point not in either subspan (%f)\n", tstart, tend, ON_DotProduct(v1,v2));
		return -2;
	    }
	} else {
	    // Not in this span
	    if (verbose)
		bu_log("(%f - %f: point not in span (dot-product: %f)\n", tstart, tend, ON_DotProduct(v1,v2));
	    return -2;
	}
    }

    // close enough - this works
    if (verbose)
	bu_log("Workable (%f:%f) - edge point (%f %f %f) and trim point (%f %f %f): %f, %f\n", tstart, tend, edge_mid_3d.x, edge_mid_3d.y, edge_mid_3d.z, trim_mid_3d.x, trim_mid_3d.y, trim_mid_3d.z, dist, tol);

    (*tmid) = tcmid;
    return dist;
}

ON_3dPoint
get_trim_midpt(fastf_t *t, const ON_BrepTrim *trim, double tstart, double tend, ON_3dPoint &edge_mid_3d, double tol, int verbose) {
    double tmid;
    double dist = midpt_binary_search(&tmid, trim, tstart, tend, edge_mid_3d, tol, verbose);
    if (dist < 0) {
	if (verbose)
	    bu_log("Warning - could not find suitable trim point\n");
	tmid = (tstart + tend) / 2.0;
    } else {
	if (verbose) {
	    if (dist > tol)
		bu_log("going with distance %f greater than desired tolerance %f\n", dist, tol);
	}
    }
    ON_3dPoint trim_mid_2d = trim->PointAt(tmid);
    (*t) = tmid;
    return trim_mid_2d;
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
	std::map<double, BrepTrimPoint *> *trim2_param_points,
	double loop_min_dist
	)
{
    ON_3dPoint tmp1, tmp2;
    ON_BrepTrim *trim2 = (edge->Trim(0)->m_trim_index == trim.m_trim_index) ? edge->Trim(1) : edge->Trim(0);
    const ON_Surface *s1 = trim.SurfaceOf();
    const ON_Surface *s2 = trim2->SurfaceOf();

    ON_3dVector trim1_mid_norm = ON_3dVector::UnsetVector;
    ON_3dVector trim2_mid_norm = ON_3dVector::UnsetVector;
    ON_3dPoint edge_mid_3d = ON_3dPoint::UnsetPoint;
    ON_3dVector edge_mid_tang = ON_3dVector::UnsetVector;
    
    fastf_t emid = (sbtp1->e + ebtp1->e) / 2.0;
    bool evtangent_status = nc->EvTangent(emid, edge_mid_3d, edge_mid_tang);
    if (!evtangent_status) {
	// EvTangent call failed, get 3d point
	edge_mid_3d = nc->PointAt(emid);
    }

    // We need the trim points to be pretty close to the edge point, or
    // we get distortions in the mesh.
    
    if (edge->m_edge_index == 790) {
	bu_log("trim fun commencing\n");
    }
    fastf_t t1, t2;
    fastf_t emindist = (cdt_tol->min_dist < 0.5*loop_min_dist) ? cdt_tol->min_dist : 0.5 * loop_min_dist;
    ON_3dPoint trim1_mid_2d, trim2_mid_2d;
    if (edge->m_edge_index == 790) {
	trim1_mid_2d = get_trim_midpt(&t1, &trim, sbtp1->t, ebtp1->t, edge_mid_3d, emindist, 0);
	trim2_mid_2d = get_trim_midpt(&t2, trim2, sbtp2->t, ebtp2->t, edge_mid_3d, emindist, 0);
    } else {
	trim1_mid_2d = get_trim_midpt(&t1, &trim, sbtp1->t, ebtp1->t, edge_mid_3d, emindist, 0);
	trim2_mid_2d = get_trim_midpt(&t2, trim2, sbtp2->t, ebtp2->t, edge_mid_3d, emindist, 0);
    }

    if (!evtangent_status) {
	// If the edge curve evaluation failed, try to average tangents from trims
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

    int dosplit = 0;

    ON_Line line3d(*(sbtp1->p3d), *(ebtp1->p3d));
    double dist3d = edge_mid_3d.DistanceTo(line3d.ClosestPointTo(edge_mid_3d));
    dosplit += (line3d.Length() > cdt_tol->max_dist) ? 1 : 0;
    dosplit += (dist3d > (cdt_tol->within_dist + ON_ZERO_TOLERANCE)) ? 1 : 0;
    dosplit += (dist3d > 2*loop_min_dist) ? 1 : 0;

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

    if (edge->m_edge_index == 790) {
	bu_log("Edge %d: %f %f %f\n", edge->m_edge_index, edge_mid_3d.x, edge_mid_3d.y, edge_mid_3d.z);
	bu_log("Trim %d, Face %d, (IsRev: %d): %f %f %f\n", trim.m_trim_index, trim.Face()->m_face_index, trim.m_bRev3d, tmp1.x, tmp1.y, tmp1.z);
	bu_log("Trim %d, Face %d, (IsRev: %d): %f %f %f\n", trim2->m_trim_index, trim2->Face()->m_face_index, trim2->m_bRev3d, tmp2.x, tmp2.y, tmp2.z);
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
	nbtp1->trim_ind = trim.m_trim_index;
	(*trim1_param_points)[nbtp1->t] = nbtp1;
	(*s_cdt->on_brep_edge_pnts)[npt].insert(nbtp1);

	BrepTrimPoint *nbtp2 = new BrepTrimPoint;
	nbtp2->p3d = npt;
	nbtp2->n3d = NULL;
	nbtp2->p2d = trim2_mid_2d;
	nbtp2->normal = trim2_mid_norm;
	nbtp2->tangent = edge_mid_tang;
	nbtp2->t = t2;
	nbtp2->e = emid;
	nbtp2->trim_ind = trim2->m_trim_index;
	(*trim2_param_points)[nbtp2->t] = nbtp2;
	(*s_cdt->on_brep_edge_pnts)[npt].insert(nbtp2);

	getEdgePoints(s_cdt, edge, nc, trim, sbtp1, nbtp1, sbtp2, nbtp2, cdt_tol, trim1_param_points, trim2_param_points, loop_min_dist);
	getEdgePoints(s_cdt, edge, nc, trim, nbtp1, ebtp1, nbtp2, ebtp2, cdt_tol, trim1_param_points, trim2_param_points, loop_min_dist);
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
	nbtp1->trim_ind = trim.m_trim_index;
	nbtp1->e = ON_UNSET_VALUE;
	(*trim1_param_points)[nbtp1->t] = nbtp1;
	(*s_cdt->on_brep_edge_pnts)[nsptp].insert(nbtp1);

	BrepTrimPoint *nbtp2 = new BrepTrimPoint;
	nbtp2->p3d = nsptp;
	nbtp2->p2d = trim2_seam_2d;
	nbtp2->t = trim2_seam_t;
	nbtp2->trim_ind = trim2->m_trim_index;
	nbtp2->e = ON_UNSET_VALUE;
	(*trim2_param_points)[nbtp2->t] = nbtp2;
	(*s_cdt->on_brep_edge_pnts)[nsptp].insert(nbtp2);
    }

}

std::map<double, BrepTrimPoint *> *
getEdgePoints(
	struct ON_Brep_CDT_State *s_cdt,
	ON_BrepEdge *edge,
	ON_BrepTrim &trim,
	fastf_t max_dist,
	fastf_t loop_min_dist
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

    if (edge->m_edge_index == 790) {
	bu_log("Starting 790\n");
    }

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


    /* Normalize the domain of the curve to the ControlPolygonLength() of the
     * NURBS form of the curve to attempt to minimize distortion in 3D to
     * mirror what we do for the surfaces.  (GetSurfaceSize uses this under the
     * hood for its estimates.)  */
    const ON_Curve* crv = edge->EdgeCurveOf();
    ON_NurbsCurve *nc = crv->NurbsCurve();
    double cplen = nc->ControlPolygonLength();
    nc->SetDomain(0.0, cplen);
    s_cdt->brep->m_T[trim.TrimCurveIndexOf()].SetDomain(0.0, cplen);
    s_cdt->brep->m_T[trim2->TrimCurveIndexOf()].SetDomain(0.0, cplen);
    ON_Interval erange = nc->Domain();


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
    edge_start_3d = (*s_cdt->vert_pnts)[edge->Vertex(0)->m_vertex_index];
    edge_end_3d = (*s_cdt->vert_pnts)[edge->Vertex(1)->m_vertex_index];

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
    ON_3dPoint *edge_start_3dnorm = (*s_cdt->vert_avg_norms)[edge->Vertex(0)->m_vertex_index];
    ON_3dPoint *edge_end_3dnorm = (*s_cdt->vert_avg_norms)[edge->Vertex(1)->m_vertex_index];

    /* trim 1 */
    if (!surface_EvNormal(s1, trim1_start_2d.x, trim1_start_2d.y, tmpp, trim1_start_normal)) {
	t1_sn = edge_start_3dnorm;
    } else {
	if (trim.Face()->m_bRev) {
	    trim1_start_normal = trim1_start_normal  * -1.0;
	}
	if (edge_start_3dnorm && ON_DotProduct(trim1_start_normal, *edge_start_3dnorm) < -0.5) {
	    t1_sn = edge_start_3dnorm;
	} else {
	    t1_sn = new ON_3dPoint(trim1_start_normal);
	    *t1_sn = trim1_start_normal;
	    s_cdt->w3dnorms->push_back(t1_sn);
	}
    }
    if (!surface_EvNormal(s1, trim1_end_2d.x, trim1_end_2d.y, tmp1, trim1_end_normal)) {
	t1_en = edge_end_3dnorm;
    } else {
	if (trim.Face()->m_bRev) {
	    trim1_end_normal = trim1_end_normal  * -1.0;
	}
	if (edge_end_3dnorm && ON_DotProduct(trim1_end_normal, *edge_end_3dnorm) < -0.5) {
	    t1_en = edge_end_3dnorm;
	} else {
	    t1_en = new ON_3dPoint(trim1_end_normal);
	    *t1_en = trim1_end_normal;
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
	if (edge_start_3dnorm && ON_DotProduct(trim2_start_normal, *edge_start_3dnorm) < -0.5) {
	    t2_sn = edge_start_3dnorm;
	} else {
	    t2_sn = new ON_3dPoint(trim2_start_normal);
	    *t2_sn = trim2_start_normal;
	    s_cdt->w3dnorms->push_back(t2_sn);
	}
    }
    if (!surface_EvNormal(s2, trim2_end_2d.x, trim2_end_2d.y, tmpp, trim2_end_normal)) {
	t2_en = edge_end_3dnorm;
    } else {
	if (trim2->Face()->m_bRev) {
	    trim2_end_normal = trim2_end_normal  * -1.0;
	}
	if (edge_end_3dnorm && ON_DotProduct(trim2_end_normal, *edge_end_3dnorm) < -0.5) {
	    t2_en = edge_end_3dnorm;
	} else {
	    t2_en = new ON_3dPoint(trim2_end_normal);
	    *t2_en = trim2_end_normal;
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
    sbtp1->trim_ind = trim.m_trim_index;
    (*trim1_param_points)[sbtp1->t] = sbtp1;
    (*s_cdt->on_brep_edge_pnts)[edge_start_3d].insert(sbtp1);

    BrepTrimPoint *ebtp1 = new BrepTrimPoint;
    ebtp1->p3d = edge_end_3d;
    ebtp1->n3d = t1_en;
    ebtp1->tangent = edge_end_tang;
    ebtp1->e = erange.m_t[1];
    ebtp1->p2d = trim1_end_2d;
    ebtp1->normal = trim1_end_normal;
    ebtp1->t = et1;
    ebtp1->trim_ind = trim.m_trim_index;
    (*trim1_param_points)[ebtp1->t] = ebtp1;
    (*s_cdt->on_brep_edge_pnts)[edge_end_3d].insert(ebtp1);

    BrepTrimPoint *sbtp2 = new BrepTrimPoint;
    sbtp2->p3d = edge_start_3d;
    sbtp2->n3d = t2_sn;
    sbtp2->tangent = edge_start_tang;
    sbtp2->e = erange.m_t[0];
    sbtp2->p2d = trim2_start_2d;
    sbtp2->normal = trim2_start_normal;
    sbtp2->t = st2;
    sbtp2->trim_ind = trim2->m_trim_index;
    (*trim2_param_points)[sbtp2->t] = sbtp2;
    (*s_cdt->on_brep_edge_pnts)[edge_start_3d].insert(sbtp2);

    BrepTrimPoint *ebtp2 = new BrepTrimPoint;
    ebtp2->p3d = edge_end_3d;
    ebtp2->n3d = t2_en;
    ebtp2->tangent = edge_end_tang;
    ebtp2->e = erange.m_t[1];
    ebtp2->p2d = trim2_end_2d;
    ebtp2->normal = trim2_end_normal;
    ebtp2->t = et2;
    ebtp2->trim_ind = trim2->m_trim_index;
    (*trim2_param_points)[ebtp2->t] = ebtp2;
    (*s_cdt->on_brep_edge_pnts)[edge_end_3d].insert(ebtp2);


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
	mbtp1->trim_ind = trim.m_trim_index;
	(*trim1_param_points)[mbtp1->t] = mbtp1;
	(*s_cdt->on_brep_edge_pnts)[nmp].insert(mbtp1);

	BrepTrimPoint *mbtp2 = new BrepTrimPoint;
	mbtp2->p3d = nmp;
	mbtp2->n3d = NULL;
	mbtp2->p2d = trim2_mid_2d;
	mbtp2->tangent = edge_mid_tang;
	mbtp2->normal = trim2_mid_norm;
	mbtp2->t = trim2_mid_range;
	mbtp1->e = edge_mid_range;
	mbtp2->trim_ind = trim2->m_trim_index;
	(*trim2_param_points)[mbtp2->t] = mbtp2;
	(*s_cdt->on_brep_edge_pnts)[nmp].insert(mbtp2);

	getEdgePoints(s_cdt, edge, nc, trim, sbtp1, mbtp1, sbtp2, mbtp2, &cdt_tol, trim1_param_points, trim2_param_points, loop_min_dist);
	getEdgePoints(s_cdt, edge, nc, trim, mbtp1, ebtp1, mbtp2, ebtp2, &cdt_tol, trim1_param_points, trim2_param_points, loop_min_dist);

    } else {

	getEdgePoints(s_cdt, edge, nc, trim, sbtp1, ebtp1, sbtp2, ebtp2, &cdt_tol, trim1_param_points, trim2_param_points, loop_min_dist);

    }

    return trim1_param_points;
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

