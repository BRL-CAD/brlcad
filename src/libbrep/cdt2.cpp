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
    } else {
	tangent = ON_3dVector::UnsetVector;
    }

    return tangent;
}


double
midpnt_binary_search(fastf_t *tmid, const ON_BrepTrim &trim, double tstart, double tend, ON_3dPoint &edge_mid_3d, double tol, int verbose)
{
    double tcmid = (tstart + tend) / 2.0;
    ON_3dPoint trim_mid_2d = trim.PointAt(tcmid);
    const ON_Surface *s = trim.SurfaceOf();
    ON_3dPoint trim_mid_3d = s->PointAt(trim_mid_2d.x, trim_mid_2d.y);
    double dist = edge_mid_3d.DistanceTo(trim_mid_3d);

    if (dist > tol) {
	ON_3dPoint trim_start_2d = trim.PointAt(tstart);
	ON_3dPoint trim_end_2d = trim.PointAt(tend);
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
	    double fldist = midpnt_binary_search(&tlmid, trim, tstart, tcmid, edge_mid_3d, tol, 0);
	    double frdist = midpnt_binary_search(&trmid, trim, tcmid, tend, edge_mid_3d, tol, 0);
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

ON_2dPoint
get_trim_midpt(fastf_t *t, struct ON_Brep_CDT_State *s_cdt, cdt_mesh::cpolyedge_t *pe, ON_3dPoint &edge_mid_3d, double elen)
{
    int verbose = 1;
    double tol = (elen < BN_TOL_DIST) ? 0.01*elen : 0.1*BN_TOL_DIST;
    ON_BrepTrim& trim = s_cdt->brep->m_T[pe->trim_ind];
    double tmid;
    double dist = midpnt_binary_search(&tmid, trim, pe->trim_start, pe->trim_end, edge_mid_3d, tol, 1);
    if (dist < 0) {
        if (verbose) {
            bu_log("Warning - could not find suitable trim point\n");
	}
        tmid = (pe->trim_start + pe->trim_end) / 2.0;
    } else {
	if (verbose && (dist > tol)) {
	    bu_log("going with distance %f greater than desired tolerance %f\n", dist, tol);
	}
    }
    ON_2dPoint trim_mid_2d = trim.PointAt(tmid);
    (*t) = tmid;
    return trim_mid_2d;
}

std::set<cdt_mesh::bedge_seg_t *>
split_edge_seg(struct ON_Brep_CDT_State *s_cdt, cdt_mesh::bedge_seg_t *e)
{
    std::set<cdt_mesh::bedge_seg_t *> nedges;

    if (!e->tseg1 || !e->tseg2 || !e->nc) return nedges;

    ON_BrepEdge& edge = s_cdt->brep->m_E[e->edge_ind];

    ON_BrepTrim *trim1 = edge.Trim(0);
    ON_BrepTrim *trim2 = edge.Trim(1);

    if (!trim1 || !trim2) return nedges;

    ON_3dPoint edge_mid_3d = ON_3dPoint::UnsetPoint;
    ON_3dVector edge_mid_tang = ON_3dVector::UnsetVector;

    // Get the 3D midpoint (and tangent, if we can) from the edge curve
    fastf_t emid = (e->edge_start + e->edge_end) / 2.0;
    bool evtangent_status = e->nc->EvTangent(emid, edge_mid_3d, edge_mid_tang);
    if (!evtangent_status) {
        // EvTangent call failed, get 3d point
        edge_mid_3d = e->nc->PointAt(emid);
    }

#if 0
    // edge_mid_3d is a new point in the cdt and the fmesh - add it
    ON_3dPoint *mid_3d = new ON_3dPoint(edge_mid_3d);
    long p_ind = s_cdt->fmesh->add_point(mid_3d

    double elen = (e->nc->PointAt(e->edge_start)).DistanceTo(e->nc->PointAt(e->edge_end));

    fastf_t t1mid, t2mid;
    ON_3dPoint trim1_mid_2d, trim2_mid_2d;
    trim1_mid_2d = get_trim_midpt(&t1mid, s_cdt, e->tseg1, edge_mid_3d, elen);
    trim2_mid_2d = get_trim_midpt(&t2mid, s_cdt, e->tseg2, edge_mid_3d, elen);

    // add new points to the cpolygons in preparation for replacing polygon edges


    // replace bseg with new bsegs and cpolygon edge with new edges in both faces
    cdt_mesh::bedge_seg_t *bseg1 = new cdt_mesh::bedge_seg_t;
    cdt_mesh::bedge_seg_t *bseg2 = new cdt_mesh::bedge_seg_t;
#endif

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

    // 1.  Any edges with at least 1 closed trim are split.
    if (trim1->IsClosed() || trim2->IsClosed()) {
	esegs_closed = split_edge_seg(s_cdt, e);
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
	    std::set<cdt_mesh::bedge_seg_t *> etmp = split_edge_seg(s_cdt, *e_it);
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

    // First, set up the edge containers that will manage the edge subdivision.  Loop
    // ordering is not the job of these containers - that's handled by the trim loop
    // polygons.  These containers maintain the association between trims in different
    // faces and the 3D edge curve information used to drive shared points.
    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	cdt_mesh::bedge_seg_t *bseg = new cdt_mesh::bedge_seg_t;
	bseg->edge_ind = edge.m_edge_index;

	// Provide a normalize edge NURBS curve
	const ON_Curve* crv = edge.EdgeCurveOf();
	bseg->nc = crv->NurbsCurve();
	double cplen = bseg->nc->ControlPolygonLength();
	bseg->nc->SetDomain(0.0, cplen);

	// Set the initial edge curve t parameter values
	bseg->edge_start = 0.0;
	bseg->edge_end = cplen;

	// Get the trims and normalize their domains as well.
	// TODO - another point where this won't work if we don't have a 1->2 edge to trims relationship
	ON_BrepTrim *trim1 = edge.Trim(0);
	ON_BrepTrim *trim2 = edge.Trim(1);
	s_cdt->brep->m_T[trim1->TrimCurveIndexOf()].SetDomain(bseg->edge_start, bseg->edge_end);
	s_cdt->brep->m_T[trim2->TrimCurveIndexOf()].SetDomain(bseg->edge_start, bseg->edge_end);

	ON_3dPoint tmp;
	// The 3D start and endpoints will be vertex points (they are shared with other edges).
	bseg->e_start = (*s_cdt->vert_pnts)[edge.Vertex(0)->m_vertex_index];
	bseg->e_end = (*s_cdt->vert_pnts)[edge.Vertex(1)->m_vertex_index];

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

	for (int li = 0; li < loop_cnt; li++) {
	    const ON_BrepLoop *loop = face.Loop(li);
	    bool is_outer = (face.OuterLoop()->m_loop_index == loop->m_loop_index) ? true : false;
	    if (is_outer) {
		cpoly = &fmesh->outer_loop;
	    } else {
		cpoly = new cdt_mesh::cpolygon_t;
		fmesh->inner_loops[li] = cpoly;
	    }
	    cpoly->cdt_mesh = fmesh;
	    int trim_count = loop->TrimCount();

	    ON_2dPoint cp(0,0);

	    long cv, pv, fv = -1;
	    for (int lti = 0; lti < trim_count; lti++) {
		ON_BrepTrim *trim = loop->Trim(lti);
		ON_Interval range = trim->Domain();
		if (lti == 0) {
		    // Polygon first
		    cp = trim->PointAt(range.m_t[0]);
		    pv = cpoly->add_point(cp);
		    long find = fmesh->add_point(cp);
		    cpoly->p2f[pv] = find;
		    fv = pv;

		    // Let cdt_mesh know about new information
		    ON_3dVector norm = ON_3dVector::UnsetVector;
		    if (trim->m_type != ON_BrepTrim::singular) {
			ON_3dPoint tmp1;
			surface_EvNormal(trim->SurfaceOf(), cp.x, cp.y, tmp1, norm);
		    }
		    ON_3dPoint *op3d = (*s_cdt->vert_pnts)[trim->Vertex(0)->m_vertex_index];
		    fmesh->add_point(op3d);
		    fmesh->add_normal(new ON_3dPoint(norm));

		} else {
		    pv = cv;
		}

		// NOTE: Singularities have a segment in 2D but not 3D - we're adding extra copies of pointers to
		// points in the arrays to deal with this non-uniqueness to keep a 1-1 relationship
		// between the two array indices in the polygon.  For the 3D p2ind mapping, this will mean that the
		// ON_3dPoint pointer will always point to the highest index value in the vector
		// to be assigned that particular pointer.  For tests which are concerned with 3D point
		// uniqueness, a 2d->ind->3d->ind lookup will be needed to "canonicalize"
		// the 3D index value.  (TODO In particular, this will be needed for triangle
		// comparisons.)
		//
		//
		cp = trim->PointAt(range.m_t[1]);
		cv = cpoly->add_point(cp);
		long find = fmesh->add_point(cp);
		cpoly->p2f[cv] = find;

		// Let cdt_mesh know about the 3D information as well
		ON_3dVector norm = ON_3dVector::UnsetVector;
		if (trim->m_type != ON_BrepTrim::singular) {
		    // 3D points are globally unique, but normals are not - the same edge point may
		    // have different normals from two faces at a sharp edge.  Calculate the
		    // face normal for this point on this surface.
		    ON_3dPoint tmp1;
		    surface_EvNormal(trim->SurfaceOf(), cp.x, cp.y, tmp1, norm);
		}
		ON_3dPoint *cp3d = (*s_cdt->vert_pnts)[trim->Vertex(1)->m_vertex_index];
		fmesh->add_point(cp3d);
		fmesh->add_normal(new ON_3dPoint(norm));


		struct cdt_mesh::edge_t lseg(pv, cv);
		cdt_mesh::cpolyedge_t *ne = cpoly->add_edge(lseg);
		ne->trim_ind = trim->m_trim_index;
		ne->trim_start = range.m_t[0];
		ne->trim_end = range.m_t[1];
		if (trim->m_ei >= 0) {
		    cdt_mesh::bedge_seg_t *eseg = *s_cdt->e2polysegs[trim->m_ei].begin();
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

#if 0
    // Initialize the tangents.
    std::map<int, std::set<cdt_mesh::bedge_seg_t *>>::iterator epoly_it;
    for (epoly_it = s_cdt->e2polysegs.begin(); epoly_it != s_cdt->e2polysegs.end(); epoly_it++) {
	std::set<cdt_mesh::bedge_seg_t *>::iterator seg_it;
	for (seg_it = epoly_it->second.begin(); seg_it != epoly_it->second.end(); seg_it++) {
	    cdt_mesh::bedge_seg_t *bseg = *seg_it;
	    ON_BrepTrim &trim1 = s_cdt->brep->m_T[bseg->tseg1->trim_ind];
	    ON_BrepTrim &trim2 = s_cdt->brep->m_T[bseg->tseg2->trim_ind];
	    double ts1 = (trim1.m_bRev3d) ? bseg->tseg1->trim_end : bseg->tseg1->trim_start;
	    double ts2 = (trim2.m_bRev3d) ? bseg->tseg2->trim_end : bseg->tseg2->trim_start;
	    bseg->tan_start = bseg_tangent(s_cdt, bseg, bseg->edge_start, ts1, ts2);

	    double te1 = (trim1.m_bRev3d) ? bseg->tseg1->trim_start : bseg->tseg1->trim_end;
	    double te2 = (trim2.m_bRev3d) ? bseg->tseg2->trim_start : bseg->tseg2->trim_end;
	    bseg->tan_end = bseg_tangent(s_cdt, bseg, bseg->edge_end, te1, te2);
	}
    }

    // Do the non-tolerance based initializations
    for (epoly_it = s_cdt->e2polysegs.begin(); epoly_it != s_cdt->e2polysegs.end(); epoly_it++) {
	std::set<cdt_mesh::bedge_seg_t *>::iterator seg_it;
	for (seg_it = epoly_it->second.begin(); seg_it != epoly_it->second.end(); seg_it++) {
	    cdt_mesh::bedge_seg_t *bseg = *seg_it;
	    if (!initialize_edge_segs(s_cdt, bseg)) {
		std::cout << "Initialization failed for edge " << epoly_it->first << "\n";
	    }
	}
    }
#endif

    // Process the non-linear edges first - we will need information
    // from them to handle the linear edges
    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];

	const ON_Curve* crv = edge.EdgeCurveOf();
	if (!crv->IsLinear(BN_TOL_DIST)) {
	    std::set<cdt_mesh::bedge_seg_t *> &epsegs = s_cdt->e2polysegs[edge.m_edge_index];
	    std::set<cdt_mesh::bedge_seg_t *>::iterator e_it;
	    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
		cdt_mesh::bedge_seg_t *b = *e_it;
		if (!b->tseg1 || !b->tseg2) {
		    std::cout << "don't have trims\n";
		}
	    }
	}
    }

    for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	ON_BrepFace &face = s_cdt->brep->m_F[face_index];
	int loop_cnt = face.LoopCount();
	cdt_mesh::cdt_mesh_t *fmesh = &s_cdt->fmeshes[face_index];
	cdt_mesh::cpolygon_t *cpoly = NULL;

	//if (face_index != 27) continue;

	for (int li = 0; li < loop_cnt; li++) {
	    const ON_BrepLoop *loop = face.Loop(li);
	    bool is_outer = (face.OuterLoop()->m_loop_index == loop->m_loop_index) ? true : false;
	    if (is_outer) {
		cpoly = &fmesh->outer_loop;
	    } else {
		cpoly = fmesh->inner_loops[li];
	    }
	    std::cout << "Face: " << face_index << ", Loop: " << li << "\n";
	    cpoly->print();

	    struct bu_vls fname = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&fname, "%d-%d-poly3d.plot3", face_index, li);
	    cpoly->polygon_plot_3d(bu_vls_cstr(&fname));
	    cpoly->cdt();
	    bu_vls_sprintf(&fname, "%d-%d-cdt.plot3", face_index, li);
	    cpoly->cdt_mesh->tris_set_plot(cpoly->tris, bu_vls_cstr(&fname));
	    bu_vls_free(&fname);

	}

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

