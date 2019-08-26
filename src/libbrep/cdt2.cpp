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
#include "bg/chull.h"
#include "./cdt.h"

// EXPERIMENT - see if we can generate polygons from the loops
// For all faces, and each face loop in those faces, build the
// initial polygons strictly based on trim start/end points

ON_3dVector
bseg_tangent(struct ON_Brep_CDT_State *s_cdt, cdt_mesh::bedge_seg_t *bseg, double eparam, double t1param, double t2param)
{
    ON_3dPoint tmp;
    ON_3dVector tangent = ON_3dVector::UnsetVector;
    if (!bseg->nc->EvTangent(eparam, tmp, tangent)) {
	if (t1param < DBL_MAX && t2param < DBL_MAX) {
	    // If the edge curve failed, average tangents from trims
	    ON_BrepEdge& edge = s_cdt->brep->m_E[bseg->edge_ind];
	    ON_BrepTrim *trim1 = edge.Trim(0);
	    ON_BrepTrim *trim2 = edge.Trim(1);
	    int evals = 0;
	    ON_3dPoint tmp1, tmp2;
	    ON_3dVector trim1_tangent, trim2_tangent;
	    evals += (trim1->EvTangent(t1param, tmp1, trim1_tangent)) ? 1 : 0;
	    evals += (trim2->EvTangent(t2param, tmp2, trim2_tangent)) ? 1 : 0;
	    if (evals == 2) {
		tangent = (trim1_tangent + trim2_tangent) / 2;
	    } else {
		tangent = ON_3dVector::UnsetVector;
	    }
	}
    }

    return tangent;
}

ON_3dVector
trim_normal(ON_BrepTrim *trim, ON_2dPoint &cp)
{
    ON_3dVector norm = ON_3dVector::UnsetVector;
    if (trim->m_type != ON_BrepTrim::singular) {
	// 3D points are globally unique, but normals are not - the same edge point may
	// have different normals from two faces at a sharp edge.  Calculate the
	// face normal for this point on this surface.
	ON_3dPoint tmp1;
	surface_EvNormal(trim->SurfaceOf(), cp.x, cp.y, tmp1, norm);
    }
    return norm;
}


double
midpnt_binary_search(fastf_t *tmid, const ON_BrepTrim &trim, double tstart, double tend, ON_3dPoint &edge_mid_3d, double tol, int verbose, int depth, int force)
{
    double tcmid = (tstart + tend) / 2.0;
    ON_3dPoint trim_mid_2d = trim.PointAt(tcmid);
    const ON_Surface *s = trim.SurfaceOf();
    ON_3dPoint trim_mid_3d = s->PointAt(trim_mid_2d.x, trim_mid_2d.y);
    double dist = edge_mid_3d.DistanceTo(trim_mid_3d);

    if (dist > tol && !force) {
	ON_3dPoint trim_start_2d = trim.PointAt(tstart);
	ON_3dPoint trim_end_2d = trim.PointAt(tend);
	ON_3dPoint trim_start_3d = s->PointAt(trim_start_2d.x, trim_start_2d.y);
	ON_3dPoint trim_end_3d = s->PointAt(trim_end_2d.x, trim_end_2d.y);

	ON_3dVector v1 = edge_mid_3d - trim_start_3d;
	ON_3dVector v2 = edge_mid_3d - trim_end_3d;
	double sedist = trim_start_3d.DistanceTo(trim_end_3d);

	if (verbose) {
	    //bu_log("start point (%f %f %f) and end point (%f %f %f)\n", trim_start_3d.x, trim_start_3d.y, trim_start_3d.z, trim_end_3d.x, trim_end_3d.y, trim_end_3d.z);
	}

	double vdot = ON_DotProduct(v1,v2);

	if (vdot < 0 && dist > ON_ZERO_TOLERANCE) {
	    //if (verbose)
	    //	bu_log("(%f - %f - %f (%f): searching left and right subspans\n", tstart, tcmid, tend, ON_DotProduct(v1,v2));
	    double tlmid, trmid;
	    double fldist = midpnt_binary_search(&tlmid, trim, tstart, tcmid, edge_mid_3d, tol, verbose, depth+1, 0);
	    double frdist = midpnt_binary_search(&trmid, trim, tcmid, tend, edge_mid_3d, tol, verbose, depth+1, 0);
	    if (fldist >= 0 && frdist < -1) {
		//	if (verbose)
		//	    bu_log("(%f - %f - %f: going with fldist: %f\n", tstart, tcmid, tend, fldist);
		(*tmid) = tlmid;
		return fldist;
	    }
	    if (frdist >= 0 && fldist < -1) {
		//	if (verbose)
		//	    bu_log("(%f - %f - %f: going with frdist: %f\n", tstart, tcmid, tend, frdist);
		(*tmid) = trmid;
		return frdist;
	    }
	    if (fldist < -1 && frdist < -1) {
		fldist = midpnt_binary_search(&tlmid, trim, tstart, tcmid, edge_mid_3d, tol, verbose, depth+1, 1);
		frdist = midpnt_binary_search(&trmid, trim, tcmid, tend, edge_mid_3d, tol, verbose, depth+1, 1);
		if (verbose) {
		    bu_log("Trim %d: point not in either subspan according to dot product (distances are %f and %f, distance between sampling segment ends is %f), forcing the issue\n", trim.m_trim_index, fldist, frdist, sedist);
		}

		if ((fldist < frdist) && (fldist < dist)) {
		    (*tmid) = tlmid;
		    return fldist;
		}
		if ((frdist < fldist) && (frdist < dist)) {
		    (*tmid) = trmid;
		    return frdist;
		}
		(*tmid) = tcmid;
		return dist;

	    }
	} else if (NEAR_ZERO(vdot, ON_ZERO_TOLERANCE)) {
	    (*tmid) = tcmid;
	    return dist;
	} else {
	    // Not in this span
	    if (verbose && depth < 2) {
		//bu_log("Trim %d: (%f:%f)%f - edge point (%f %f %f) and trim point (%f %f %f): distance between them is %f, tol is %f, search seg length: %f\n", trim.m_trim_index, tstart, tend, ON_DotProduct(v1,v2), edge_mid_3d.x, edge_mid_3d.y, edge_mid_3d.z, trim_mid_3d.x, trim_mid_3d.y, trim_mid_3d.z, dist, tol, sedist);
	    }
	    if (depth == 0) {
		(*tmid) = tcmid;
		return dist;
	    } else {
		return -2;
	    }
	}
    }

    // close enough - this works
    //if (verbose)
    //	bu_log("Workable (%f:%f) - edge point (%f %f %f) and trim point (%f %f %f): %f, %f\n", tstart, tend, edge_mid_3d.x, edge_mid_3d.y, edge_mid_3d.z, trim_mid_3d.x, trim_mid_3d.y, trim_mid_3d.z, dist, tol);

    (*tmid) = tcmid;
    return dist;
}

ON_2dPoint
get_trim_midpt(fastf_t *t, struct ON_Brep_CDT_State *s_cdt, cdt_mesh::cpolyedge_t *pe, ON_3dPoint &edge_mid_3d, double elen, double brep_edge_tol)
{
    int verbose = 0;
    double tol;
    if (!NEAR_EQUAL(brep_edge_tol, ON_UNSET_VALUE, ON_ZERO_TOLERANCE)) {
	tol = brep_edge_tol;
    } else {
	tol = (elen < BN_TOL_DIST) ? 0.01*elen : 0.1*BN_TOL_DIST;
    }
    ON_BrepTrim& trim = s_cdt->brep->m_T[pe->trim_ind];
    double tmid;
    double dist = midpnt_binary_search(&tmid, trim, pe->trim_start, pe->trim_end, edge_mid_3d, tol, 0, 0, 0);
    if (dist < 0) {
        if (verbose) {
            bu_log("Warning - could not find suitable trim point\n");
	}
        tmid = (pe->trim_start + pe->trim_end) / 2.0;
    } else {
	if (verbose && (dist > tol)) {
	    bu_log("going with distance %f greater than desired tolerance %f\n", dist, tol);
	    if (dist > 10*tol) {
		dist = midpnt_binary_search(&tmid, trim, pe->trim_start, pe->trim_end, edge_mid_3d, tol, 0, 0, 0);
	    }
	}
    }
    ON_2dPoint trim_mid_2d = trim.PointAt(tmid);
    (*t) = tmid;
    return trim_mid_2d;
}

struct edge_split_tols {
    fastf_t edge_max_allowed_dist;
    fastf_t edge_min_allowed_dist;
    fastf_t max_edgept_dist_from_edge;
    fastf_t within_angle_cos;
};

bool
tol_need_split(struct ON_Brep_CDT_State *s_cdt, struct edge_split_tols *etol, cdt_mesh::bedge_seg_t *bseg, ON_3dPoint &edge_mid_3d)
{
    ON_Line line3d(*(bseg->e_start), *(bseg->e_end));
    double dist3d = edge_mid_3d.DistanceTo(line3d.ClosestPointTo(edge_mid_3d));

    if (line3d.Length() > etol->edge_max_allowed_dist) return true;

    if (line3d.Length() < etol->edge_min_allowed_dist) return false;

    if (dist3d > etol->max_edgept_dist_from_edge) return true;

    if ((bseg->tan_start * bseg->tan_end) < etol->within_angle_cos) return true;

    ON_3dPoint *n1, *n2;

    ON_BrepEdge& edge = s_cdt->brep->m_E[bseg->edge_ind];
    ON_BrepTrim *trim1 = edge.Trim(0);
    ON_BrepFace *face1 = trim1->Face();
    cdt_mesh::cdt_mesh_t *fmesh1 = &s_cdt->fmeshes[face1->m_face_index];
    n1 = fmesh1->normals[fmesh1->nmap[fmesh1->p2ind[bseg->e_start]]];
    n2 = fmesh1->normals[fmesh1->nmap[fmesh1->p2ind[bseg->e_end]]];

    if (ON_3dVector(*n1) != ON_3dVector::UnsetVector && ON_3dVector(*n2) != ON_3dVector::UnsetVector) {
	if ((ON_3dVector(*n1) * ON_3dVector(*n2)) < etol->within_angle_cos - ON_ZERO_TOLERANCE) return true;
    }

    ON_BrepTrim *trim2 = edge.Trim(1);
    ON_BrepFace *face2 = trim2->Face();
    cdt_mesh::cdt_mesh_t *fmesh2 = &s_cdt->fmeshes[face2->m_face_index];
    n1 = fmesh2->normals[fmesh2->nmap[fmesh2->p2ind[bseg->e_start]]];
    n2 = fmesh2->normals[fmesh2->nmap[fmesh2->p2ind[bseg->e_end]]];

    if (ON_3dVector(*n1) != ON_3dVector::UnsetVector && ON_3dVector(*n2) != ON_3dVector::UnsetVector) {
	if ((ON_3dVector(*n1) * ON_3dVector(*n2)) < etol->within_angle_cos - ON_ZERO_TOLERANCE) return true;
    }

    return false;
}

std::set<cdt_mesh::bedge_seg_t *>
split_edge_seg(struct ON_Brep_CDT_State *s_cdt, struct edge_split_tols *etol, cdt_mesh::bedge_seg_t *bseg)
{
    std::set<cdt_mesh::bedge_seg_t *> nedges;

    if (!bseg->tseg1 || !bseg->tseg2 || !bseg->nc) return nedges;

    //std::cout << "splitting edge " << bseg->edge_ind << "\n";

    ON_BrepEdge& edge = s_cdt->brep->m_E[bseg->edge_ind];

    ON_BrepTrim *trim1 = edge.Trim(0);
    ON_BrepTrim *trim2 = edge.Trim(1);

    if (!trim1 || !trim2) return nedges;

    ON_3dPoint edge_mid_3d = ON_3dPoint::UnsetPoint;
    ON_3dVector edge_mid_tan = ON_3dVector::UnsetVector;

    // Get the 3D midpoint (and tangent, if we can) from the edge curve
    fastf_t emid = (bseg->edge_start + bseg->edge_end) / 2.0;
    bool evtangent_status = bseg->nc->EvTangent(emid, edge_mid_3d, edge_mid_tan);
    if (!evtangent_status) {
        // EvTangent call failed, get 3d point
        edge_mid_3d = bseg->nc->PointAt(emid);
	edge_mid_tan = ON_3dVector::UnsetVector;
    }

    // Unless we're forcing a split this is the point at which we do tolerance
    // based testing to determine whether to proceed with the split or halt.
    if (etol && !tol_need_split(s_cdt, etol, bseg, edge_mid_3d)) {
	return nedges;
    }

    // edge_mid_3d is a new point in the cdt and the fmesh, as well as a new
    // edge point - add it to the appropriate containers
    ON_3dPoint *mid_3d = new ON_3dPoint(edge_mid_3d);
    CDT_Add3DPnt(s_cdt, mid_3d, -1, -1, -1, edge.m_edge_index, 0, 0);
    s_cdt->edge_pnts->insert(mid_3d);

    // Find the 2D points
    double elen1 = (bseg->nc->PointAt(bseg->edge_start)).DistanceTo(bseg->nc->PointAt(emid));
    double elen2 = (bseg->nc->PointAt(emid)).DistanceTo(bseg->nc->PointAt(bseg->edge_end));
    double elen = (elen1 + elen2) * 0.5;
    fastf_t t1mid, t2mid;
    ON_2dPoint trim1_mid_2d, trim2_mid_2d;
    trim1_mid_2d = get_trim_midpt(&t1mid, s_cdt, bseg->tseg1, edge_mid_3d, elen, edge.m_tolerance);
    trim2_mid_2d = get_trim_midpt(&t2mid, s_cdt, bseg->tseg2, edge_mid_3d, elen, edge.m_tolerance);

    // Let the fmeshes know about the new information, and find the normals for the
    // new point on each face.
    ON_BrepFace *face1 = trim1->Face();
    ON_BrepFace *face2 = trim2->Face();
    cdt_mesh::cdt_mesh_t *fmesh1 = &s_cdt->fmeshes[face1->m_face_index];
    cdt_mesh::cdt_mesh_t *fmesh2 = &s_cdt->fmeshes[face2->m_face_index];

    // Update the 2D and 2D->3D info in the fmeshes
    long f1_ind2d = fmesh1->add_point(trim1_mid_2d);
    long f1_ind3d = fmesh1->add_point(mid_3d);
    fmesh1->p2d3d[f1_ind2d] = f1_ind3d;
    long f2_ind2d = fmesh2->add_point(trim2_mid_2d);
    long f2_ind3d = fmesh2->add_point(mid_3d);
    fmesh2->p2d3d[f2_ind2d] = f2_ind3d;

    ON_3dVector norm1 = trim_normal(trim1, trim1_mid_2d);
    fmesh1->normals.push_back(new ON_3dPoint(norm1));
    long f1_nind = fmesh1->normals.size() - 1;
    fmesh1->nmap[f1_ind3d] = f1_nind;

    ON_3dVector norm2 = trim_normal(trim2, trim2_mid_2d);
    fmesh2->normals.push_back(new ON_3dPoint(norm2));
    long f2_nind = fmesh2->normals.size() - 1;
    fmesh2->nmap[f2_ind3d] = f2_nind;

    // From the existing polyedge, make the two new polyedges that will replace the old one
    cdt_mesh::bedge_seg_t *bseg1 = new cdt_mesh::bedge_seg_t(bseg);
    bseg1->edge_start = bseg->edge_start;
    bseg1->edge_end = emid;
    bseg1->e_start = bseg->e_start;
    bseg1->e_end = mid_3d;
    bseg1->tan_start = bseg->tan_start;
    bseg1->tan_end = edge_mid_tan;

    cdt_mesh::bedge_seg_t *bseg2 = new cdt_mesh::bedge_seg_t(bseg);
    bseg2->edge_start = emid;
    bseg2->edge_end = bseg->edge_end;
    bseg2->e_start = mid_3d;
    bseg2->e_end = bseg->e_end;
    bseg2->tan_start = edge_mid_tan;
    bseg2->tan_end = bseg->tan_end;

    // Using the 2d mid points, update the polygons associated with tseg1 and tseg2.
    cdt_mesh::cpolyedge_t *poly1_ne1, *poly1_ne2, *poly2_ne1, *poly2_ne2;
    {
	cdt_mesh::cpolygon_t *poly1 = bseg->tseg1->polygon;
	int v[2];
	v[0] = bseg->tseg1->v[0];
	v[1] = bseg->tseg1->v[1];
	int trim_ind = bseg->tseg1->trim_ind;
	double old_trim_start = bseg->tseg1->trim_start;
	double old_trim_end = bseg->tseg1->trim_end;
	poly1->remove_edge(cdt_mesh::edge_t(v[0], v[1]));
	long poly1_2dind = poly1->add_point(trim1_mid_2d, f1_ind2d);
	struct cdt_mesh::edge_t poly1_edge1(v[0], poly1_2dind);
	poly1_ne1 = poly1->add_edge(poly1_edge1);
	poly1_ne1->trim_ind = trim_ind;
	poly1_ne1->trim_start = old_trim_start;
	poly1_ne1->trim_end = t1mid;
	struct cdt_mesh::edge_t poly1_edge2(poly1_2dind, v[1]);
	poly1_ne2 = poly1->add_edge(poly1_edge2);
    	poly1_ne2->trim_ind = trim_ind;
	poly1_ne2->trim_start = t1mid;
	poly1_ne2->trim_end = old_trim_end;
    }
    {
	cdt_mesh::cpolygon_t *poly2 = bseg->tseg2->polygon;
	int v[2];
	v[0] = bseg->tseg2->v[0];
	v[1] = bseg->tseg2->v[1];
	int trim_ind = bseg->tseg2->trim_ind;
	double old_trim_start = bseg->tseg2->trim_start;
	double old_trim_end = bseg->tseg2->trim_end;
	poly2->remove_edge(cdt_mesh::edge_t(v[0], v[1]));
	long poly2_2dind = poly2->add_point(trim2_mid_2d, f2_ind2d);
	struct cdt_mesh::edge_t poly2_edge1(v[0], poly2_2dind);
	poly2_ne1 = poly2->add_edge(poly2_edge1);
	poly2_ne1->trim_ind = trim_ind;
	poly2_ne1->trim_start = old_trim_start;
	poly2_ne1->trim_end = t2mid;
	struct cdt_mesh::edge_t poly2_edge2(poly2_2dind, v[1]);
	poly2_ne2 = poly2->add_edge(poly2_edge2);
   	poly2_ne2->trim_ind = trim_ind;
	poly2_ne2->trim_start = t2mid;
	poly2_ne2->trim_end = old_trim_end;
    }
    // The new trim segments are then associated with the new bounding edge segments
    bseg1->tseg1 = poly1_ne1;
    bseg1->tseg2 = poly2_ne1;
    bseg2->tseg1 = poly1_ne2;
    bseg2->tseg2 = poly2_ne2;

    nedges.insert(bseg1);
    nedges.insert(bseg2);
    delete bseg;
    return nedges;
}

// There are a couple of edge splitting operations that have to happen in the
// beginning regardless of tolerance settings.  Do them up front so the subsequent
// working set has consistent properties.
bool
initialize_edge_segs(struct ON_Brep_CDT_State *s_cdt, cdt_mesh::bedge_seg_t *e)
{
    ON_BrepEdge& edge = s_cdt->brep->m_E[e->edge_ind];
    // TODO - another point where this won't work if we don't have a 1->2 edge to trims relationship
    ON_BrepTrim *trim1 = edge.Trim(0);
    ON_BrepTrim *trim2 = edge.Trim(1);
    std::set<cdt_mesh::bedge_seg_t *> esegs_closed;

    if (!trim1 || !trim2) return false;

    if (trim1->m_type == ON_BrepTrim::singular || trim1->m_type == ON_BrepTrim::singular) return false;

    // 1.  Any edges with at least 1 closed trim are split.
    if (trim1->IsClosed() || trim2->IsClosed()) {
	esegs_closed = split_edge_seg(s_cdt, NULL, e);
	if (!esegs_closed.size()) {
	    // split failed??
	    return false;
	}
    } else {
	esegs_closed.insert(e);
    }

    // 2.  Any edges with a non-linear edge curve are split.  (If non-linear
    // and closed, split again - a curved, closed curve must be split twice
    // to have chance of producing a non-degenerate polygon.)
    std::set<cdt_mesh::bedge_seg_t *> esegs_csplit;
    const ON_Curve* crv = edge.EdgeCurveOf();
    if (!crv->IsLinear(BN_TOL_DIST)) {
	std::set<cdt_mesh::bedge_seg_t *>::iterator e_it;
	for (e_it = esegs_closed.begin(); e_it != esegs_closed.end(); e_it++) {
	    std::set<cdt_mesh::bedge_seg_t *> etmp = split_edge_seg(s_cdt, NULL, *e_it);
	    if (!etmp.size()) {
		// split failed??
		return false;
	    }
	    esegs_csplit.insert(etmp.begin(), etmp.end());
	}
    } else {
	esegs_csplit = esegs_closed;
    }

    s_cdt->e2polysegs[edge.m_edge_index].clear();
    s_cdt->e2polysegs[edge.m_edge_index].insert(esegs_csplit.begin(), esegs_csplit.end());

    return true;
}

int
ON_Brep_CDT_Tessellate2(struct ON_Brep_CDT_State *s_cdt)
{
    ON_Brep* brep = s_cdt->brep;

#if 0
    // TODO - get the ON_BoundingBox of the brep and use for tolerance guidance (maybe?)
    ON_BoundingBox bbox = brep->BoundingBox();
    double b_len = bbox.Diagonal().Length();
#endif

    // Characterize the vertices
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
    // Charcterize the edges.  Five possibilities:
    //
    // 0.  Singularity
    // 1.  Curved edge
    // 2.  Linear edge, associated with at least 1 non-planar surface
    // 3.  Linear edge, associated with planar surfaces but sharing one or more vertices with
    //     curved edges.
    // 4.  Linear edge, associated only with planar faces and linear edges.
    std::vector<int> edge_type;
    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	const ON_Curve* crv = edge.EdgeCurveOf();

	if (!crv) {
	    edge_type.push_back(0);
	    continue;
	}

	if (!crv->IsLinear(BN_TOL_DIST)) {
	    edge_type.push_back(1);
	    continue;
	}

	const ON_Surface *s1= edge.Trim(0)->SurfaceOf();
	const ON_Surface *s2= edge.Trim(1)->SurfaceOf();
	if (!s1->IsPlanar(NULL, BN_TOL_DIST) || !s2->IsPlanar(NULL, BN_TOL_DIST)) {
	    edge_type.push_back(2);
	}

	if (vert_type[edge.Vertex(0)->m_vertex_index] || vert_type[edge.Vertex(1)->m_vertex_index]) {
	    edge_type.push_back(3);
	}

	edge_type.push_back(4);
    }

    // Set up the edge containers that will manage the edge subdivision.  Loop
    // ordering is not the job of these containers - that's handled by the trim loop
    // polygons.  These containers maintain the association between trims in different
    // faces and the 3D edge curve information used to drive shared points.
    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	cdt_mesh::bedge_seg_t *bseg = new cdt_mesh::bedge_seg_t;
	bseg->edge_ind = edge.m_edge_index;
	bseg->brep = s_cdt->brep;

	// Provide a normalize edge NURBS curve
	const ON_Curve* crv = edge.EdgeCurveOf();
	bseg->nc = crv->NurbsCurve();
	bseg->cp_len = bseg->nc->ControlPolygonLength();
	bseg->nc->SetDomain(0.0, bseg->cp_len);

	// Set the initial edge curve t parameter values
	bseg->edge_start = 0.0;
	bseg->edge_end = bseg->cp_len;

	// Get the trims and normalize their domains as well.
	// NOTE - another point where this won't work if we don't have a 1->2 edge to trims relationship
	ON_BrepTrim *trim1 = edge.Trim(0);
	ON_BrepTrim *trim2 = edge.Trim(1);
	s_cdt->brep->m_T[trim1->TrimCurveIndexOf()].SetDomain(bseg->edge_start, bseg->edge_end);
	s_cdt->brep->m_T[trim2->TrimCurveIndexOf()].SetDomain(bseg->edge_start, bseg->edge_end);

	// The 3D start and endpoints will be vertex points (they are shared with other edges).
	bseg->e_start = (*s_cdt->vert_pnts)[edge.Vertex(0)->m_vertex_index];
	bseg->e_end = (*s_cdt->vert_pnts)[edge.Vertex(1)->m_vertex_index];

	// Do the linearity test up front
	if (!crv->IsLinear(BN_TOL_DIST)) {
	    bseg->linear_edge = 0;
	} else {
	    bseg->linear_edge = 1;
	}

	s_cdt->e2polysegs[edge.m_edge_index].insert(bseg);
    }

    // Next, for each face and each loop in each face define the initial
    // loop polygons.  Note there is no splitting of edges at this point -
    // we are simply establishing the initial closed polygons.
    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	int loop_cnt = face.LoopCount();
	cdt_mesh::cdt_mesh_t *fmesh = &s_cdt->fmeshes[face_index];
	cdt_mesh::cpolygon_t *cpoly = NULL;

#if 0
	// Get the ON_BoundingBox diagonal length of the outer face loop
	// - this length will feed the evaluation of the local relmax and
	// relmin tolerance decisions (maybe?).
	//
	// Another possible interpretation of the rel tolerance for edges is relative
	// to the ControlPolygonLength of the individual edge, which has the result of
	// refining each curve locally as it makes sense (in essense, each edge curve
	// becomes a "feature".)  An abs_min would be need to prevent getting too fine
	// for very small curves, but if we go that route the overall loop dimensions
	// become less important.  To avoid disparate triangle sizes at edge mating
	// points, we track when an edge segment (linear or otherwise) meets up with other
	// edges that are non-linear via shared 3D points.  The first pass over the non
	// linear curves will just split locally, but a second pass will check for mated
	// segments that are close and refine the splitting accordingly.  (For linear
	// edges, the latter is in fact the only pass - the two things that will matter
	// for linear splitting are refining in the local neighborhood of a curve and
	// respecting a maximum edge length filter.
	//
	// Note another piece of necessary information for linear edge splitting is
	// whether either of the adjoining surfaces is non-planar.  If that's the case,
	// the linear edges will need to be split close to the segment size of the
	// non-linear edges in the outer surface loop, since the interior triangles
	// will be broken down to a much finer size.  To avoid mating small interior
	// triangle edges to long edge line segments, we need to split those types
	// of linear edges further.  Since we don't yet know the surface interior information,
	// the best available information will be the splits in the non-linear loop
	// curves.
	ON_BoundingBox lbbox;
	face.OuterLoop()->GetBoundingBox(lbbox);
	double l_dlen = bbox.Diagonal().Length();
#endif

	for (int li = 0; li < loop_cnt; li++) {
	    const ON_BrepLoop *loop = face.Loop(li);
	    bool is_outer = (face.OuterLoop()->m_loop_index == loop->m_loop_index) ? true : false;
	    if (is_outer) {
		cpoly = &fmesh->outer_loop;
	    } else {
		cpoly = new cdt_mesh::cpolygon_t;
		fmesh->inner_loops[li] = cpoly;
	    }
	    int trim_count = loop->TrimCount();

	    ON_2dPoint cp(0,0);

	    long cv = -1;
	    long pv = -1;
	    long fv = -1;
	    for (int lti = 0; lti < trim_count; lti++) {
		ON_BrepTrim *trim = loop->Trim(lti);
		int sind = (trim->m_bRev3d) ? 1 : 0;
		int eind = (trim->m_bRev3d) ? 0 : 1;
		ON_Interval range = trim->Domain();
		if (lti == 0) {
		    // Get the 2D point, add it to the mesh and current polygon
		    cp = trim->PointAt(range.m_t[sind]);
		    long find = fmesh->add_point(cp);
		    pv = cpoly->add_point(cp, find);
		    fv = pv;

		    // Let cdt_mesh know about new 3D information
		    ON_3dVector norm = ON_3dVector::UnsetVector;
		    if (trim->m_type != ON_BrepTrim::singular) {
			// 3D points are globally unique, but normals are not - the same edge point may
			// have different normals from two faces at a sharp edge.  Calculate the
			// face normal for this point on this surface.
			ON_3dPoint tmp1;
			surface_EvNormal(trim->SurfaceOf(), cp.x, cp.y, tmp1, norm);
		    }
		    ON_3dPoint *op3d = (*s_cdt->vert_pnts)[trim->Vertex(0)->m_vertex_index];
		    long f3ind = fmesh->add_point(op3d);
		    long fnind = fmesh->add_normal(new ON_3dPoint(norm));
		    fmesh->p2d3d[find] = f3ind;
		    fmesh->nmap[f3ind] = fnind;

		} else {
		    pv = cv;
		}

		// Get the 2D point, add it to the mesh and current polygon
		cp = trim->PointAt(range.m_t[eind]);
		long find = fmesh->add_point(cp);
		cv = cpoly->add_point(cp, find);

		// Let cdt_mesh know about the 3D information
		ON_3dVector norm = ON_3dVector::UnsetVector;
		if (trim->m_type != ON_BrepTrim::singular) {
		    // 3D points are globally unique, but normals are not - the same edge point may
		    // have different normals from two faces at a sharp edge.  Calculate the
		    // face normal for this point on this surface.
		    ON_3dPoint tmp1;
		    surface_EvNormal(trim->SurfaceOf(), cp.x, cp.y, tmp1, norm);
		}
		ON_3dPoint *cp3d = (*s_cdt->vert_pnts)[trim->Vertex(1)->m_vertex_index];
		long f3ind = fmesh->add_point(cp3d);
		long fnind = fmesh->add_normal(new ON_3dPoint(norm));
		fmesh->p2d3d[find] = f3ind;
		fmesh->nmap[f3ind] = fnind;

		struct cdt_mesh::edge_t lseg(pv, cv);
		cdt_mesh::cpolyedge_t *ne = cpoly->add_edge(lseg);
		ne->trim_ind = trim->m_trim_index;
		ne->trim_start = range.m_t[sind];
		ne->trim_end = range.m_t[eind];
		if (trim->m_ei >= 0) {
		    cdt_mesh::bedge_seg_t *eseg = *s_cdt->e2polysegs[trim->m_ei].begin();
		    // Associate the edge segment with the trim segment and vice versa
		    ne->eseg = eseg;
		    if (eseg->tseg1 && eseg->tseg2) {
			bu_log("error - more than two trims associated with an edge\n");
			return -1;
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
		}
	    }
	    struct cdt_mesh::edge_t last_seg(cv, fv);
	    cpoly->add_edge(last_seg);
	}
    }

    std::map<int, std::set<cdt_mesh::bedge_seg_t *>>::iterator epoly_it;

    // Initialize the tangents.
    for (epoly_it = s_cdt->e2polysegs.begin(); epoly_it != s_cdt->e2polysegs.end(); epoly_it++) {
	std::set<cdt_mesh::bedge_seg_t *>::iterator seg_it;
	for (seg_it = epoly_it->second.begin(); seg_it != epoly_it->second.end(); seg_it++) {
	    cdt_mesh::bedge_seg_t *bseg = *seg_it;
	    double ts1 = bseg->tseg1->trim_start;
	    double ts2 = bseg->tseg2->trim_start;
	    bseg->tan_start = bseg_tangent(s_cdt, bseg, bseg->edge_start, ts1, ts2);

	    double te1 = bseg->tseg1->trim_end;
	    double te2 = bseg->tseg2->trim_end;
	    bseg->tan_end = bseg_tangent(s_cdt, bseg, bseg->edge_end, te1, te2);
	}
    }

    // Do the non-tolerance based initializations
    for (epoly_it = s_cdt->e2polysegs.begin(); epoly_it != s_cdt->e2polysegs.end(); epoly_it++) {
	std::set<cdt_mesh::bedge_seg_t *>::iterator seg_it;
	std::set<cdt_mesh::bedge_seg_t *> wsegs = epoly_it->second;
	for (seg_it = wsegs.begin(); seg_it != wsegs.end(); seg_it++) {
	    cdt_mesh::bedge_seg_t *bseg = *seg_it;
	    if (!initialize_edge_segs(s_cdt, bseg)) {
		std::cout << "Initialization failed for edge " << epoly_it->first << "\n";
	    }
	}
    }

#if 0
    // Process the non-linear edges first - we will need information
    // from them to handle the linear edges
    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	const ON_Curve* crv = edge.EdgeCurveOf();
	if (!crv->IsLinear(BN_TOL_DIST)) {
	    struct edge_split_tols etols;
	    etols.edge_max_allowed_dist = (s_cdt->tol.absmax > 0) ? s_cdt->tol.absmax : 1.1*bseg->cp_len;
	    etols.edge_min_allowed_dist = (s_cdt->tol.rel > 0) ? s_cdt->tol.rel * bseg->cp_len : 0.0;
	    etols.max_edgept_dist_from_edge = (s_cdt->tol.abs > 0) ? s_cdt->tol.abs : DBL_MAX;
	    etols.within_angle_cos = s_cdt->cos_within_angle;

	    std::set<cdt_mesh::bedge_seg_t *> &epsegs = s_cdt->e2polysegs[edge.m_edge_index];
	    std::set<cdt_mesh::bedge_seg_t *>::iterator e_it;
	    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
		cdt_mesh::bedge_seg_t *b = *e_it;
		std::set<cdt_mesh::bedge_seg_t *> esegs_split;
		esegs_split = split_edge_seg(s_cdt, &etol, e, 0);
		if (esegs_split.size()) {
		    s_cdt->e2polysegs[e->edge_ind].clear();
		    s_cdt->e2polysegs[e->edge_ind].insert(esegs_split.begin(), esegs_split.end());
	    }
	}
	    }
	}
#endif


    // TODO - move the nonlinear and linear edge splitting based on tolerances back in.

    // TODO - build RTree of bsegs for edge aware surface point building

    // TODO - adapt surface point sampling to new setup

    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	cdt_mesh::cdt_mesh_t *fmesh = &s_cdt->fmeshes[face_index];
	fmesh->cdt();
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fname, "%d-tris.plot3", face_index);
	fmesh->tris_plot(bu_vls_cstr(&fname));
	bu_vls_free(&fname);
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

