/*                        C D T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2022 United States Government as represented by
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
#include <iostream>
#include <numeric>
#include <queue>
#include <string>
#include "bg/chull.h"
#include "bg/tri_tri.h"
#include "./cdt.h"

#define BREP_PLANAR_TOL 0.05
#define MAX_TRIANGULATION_ATTEMPTS 5

// TODO - get rid of all BN_TOL_DIST-only tolerances - if the object is
// very small, that distance is too big (e.g. for linearity testing).

static ON_3dVector
bseg_tangent(struct ON_Brep_CDT_State *s_cdt, bedge_seg_t *bseg, double eparam, double t1param, double t2param)
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



static bool
refine_triangulation(struct ON_Brep_CDT_State *s_cdt, cdt_mesh_t *fmesh, int cnt, int rebuild)
{
    if (!s_cdt || !fmesh) return false;

    if (cnt > MAX_TRIANGULATION_ATTEMPTS) {
	std::cerr << "Error: even after " << MAX_TRIANGULATION_ATTEMPTS << " iterations could not successfully refine triangulate face " << fmesh->f_id << " for solidity criteria\n";
	return false;
    }

    // If a previous pass has made changes in which points are active in the
    // surface set, we need to rebuild the whole triangulation.

    if (rebuild && !fmesh->cdt()) {
	bu_log("Fatal failure attempting full retriangulation of face\n");
	return false;
    }

    // Now, the hard part - create local subsets, remesh them, and replace the original
    // triangles with the new ones.
    if (!fmesh->repair()) {
	bu_log("Face %d: repair FAILED!\n", fmesh->f_id);
	return false;
    }

    return true;
}

static void
loop_edges(cdt_mesh_t *fmesh, cpolygon_t *loop)
{
    size_t vcnt = 1;
    cpolyedge_t *pe = (*loop->poly.begin());
    cpolyedge_t *first = pe;
    cpolyedge_t *next = pe->next;

    long p1_ind = fmesh->p2ind[fmesh->pnts[fmesh->p2d3d[loop->p2o[pe->v2d[0]]]]];
    long p2_ind = fmesh->p2ind[fmesh->pnts[fmesh->p2d3d[loop->p2o[pe->v2d[1]]]]];
    fmesh->ep.insert(p1_ind);
    fmesh->ep.insert(p2_ind);
    fmesh->brep_edges.insert(uedge_t(p1_ind, p2_ind));
    fmesh->ue2b_map[uedge_t(p1_ind, p2_ind)] = pe->eseg;

    while (first != next) {
	vcnt++;
	p1_ind = fmesh->p2ind[fmesh->pnts[fmesh->p2d3d[loop->p2o[next->v2d[0]]]]];
	p2_ind = fmesh->p2ind[fmesh->pnts[fmesh->p2d3d[loop->p2o[next->v2d[1]]]]];
	fmesh->ep.insert(p1_ind);
	fmesh->ep.insert(p2_ind);
	fmesh->brep_edges.insert(uedge_t(p1_ind, p2_ind));
	fmesh->ue2b_map[uedge_t(p1_ind, p2_ind)] = next->eseg;
	next = next->next;
	if (vcnt > loop->poly.size()) {
	    std::cerr << "infinite loop when reading loop edges\n";
	    return;
	}
    }
}

static bool
do_triangulation(struct ON_Brep_CDT_State *s_cdt, int fi)
{
    ON_BrepFace &face = s_cdt->brep->m_F[fi];

    // Document the min and max segment lengths - used to guide surface sampling
    int loop_cnt = face.LoopCount();
    double min_edge_seg_len = DBL_MAX;
    double max_edge_seg_len = 0;
    for (int li = 0; li < loop_cnt; li++) {
	const ON_BrepLoop *loop = face.Loop(li);
	for (int lti = 0; lti < loop->TrimCount(); lti++) {
	    ON_BrepTrim *trim = loop->Trim(lti);
	    ON_BrepEdge *edge = trim->Edge();
	    if (!edge) continue;
	    const ON_Curve* crv = edge->EdgeCurveOf();
	    if (!crv) continue;
	    std::set<bedge_seg_t *> &epsegs = s_cdt->e2polysegs[edge->m_edge_index];
	    if (!epsegs.size()) continue;
	    std::set<bedge_seg_t *>::iterator e_it;
	    for (e_it = epsegs.begin(); e_it != epsegs.end(); e_it++) {
		bedge_seg_t *b = *e_it;
		double seg_dist = b->e_start->DistanceTo(*b->e_end);
		min_edge_seg_len = (min_edge_seg_len > seg_dist) ? seg_dist : min_edge_seg_len;
		max_edge_seg_len = (max_edge_seg_len < seg_dist) ? seg_dist : max_edge_seg_len;
	    }
	}
    }
    (*s_cdt->min_edge_seg_len)[face.m_face_index] = min_edge_seg_len;
    (*s_cdt->max_edge_seg_len)[face.m_face_index] = max_edge_seg_len;

    // Sample the surface, independent of the trimming curves, to get points that
    // will tie the mesh to the interior surface.
    GetInteriorPoints(s_cdt, face.m_face_index);

    cdt_mesh_t *fmesh = &s_cdt->fmeshes[face.m_face_index];
    fmesh->brep = s_cdt->brep;
    fmesh->p_cdt = (void *)s_cdt;
    fmesh->name = s_cdt->name;
    fmesh->f_id = face.m_face_index;
    fmesh->m_bRev = face.m_bRev;

    if (!fmesh->cdt()) {
	return false;
    }

    // List singularities
    for (size_t i = 0; i < fmesh->pnts.size(); i++) {
	ON_3dPoint *p3d = fmesh->pnts[i];
	if (s_cdt->singular_vert_to_norms->find(p3d) != s_cdt->singular_vert_to_norms->end()) {
	    fmesh->sv.insert(fmesh->p2ind[p3d]);
	}
    }

    // List edges
    fmesh->brep_edges.clear();
    fmesh->ue2b_map.clear();
    loop_edges(fmesh, &fmesh->outer_loop);
    std::map<int, cpolygon_t*>::iterator i_it;
    for (i_it = fmesh->inner_loops.begin(); i_it != fmesh->inner_loops.end(); i_it++) {
	loop_edges(fmesh, i_it->second);
    }
    fmesh->boundary_edges_update();

    /* The libbg triangulation is not guaranteed to have all the properties
     * we want out of the box - trigger a series of checks */
    return refine_triangulation(s_cdt, fmesh, 0, 0);
}

ON_3dVector
calc_trim_vnorm(ON_BrepVertex& v, ON_BrepTrim *trim)
{
    ON_3dPoint t1, t2;
    ON_3dVector v1 = ON_3dVector::UnsetVector;
    ON_3dVector v2 = ON_3dVector::UnsetVector;
    ON_3dVector trim_norm = ON_3dVector::UnsetVector;

    ON_Interval trange = trim->Domain();
    ON_3dPoint t_2d1 = trim->PointAt(trange[0]);
    ON_3dPoint t_2d2 = trim->PointAt(trange[1]);

    ON_Plane fplane;
    const ON_Surface *s = trim->SurfaceOf();
    double ptol = s->BoundingBox().Diagonal().Length()*0.001;
    ptol = (ptol < BREP_PLANAR_TOL) ? ptol : BREP_PLANAR_TOL;
    if (s->IsPlanar(&fplane, ptol)) {
	trim_norm = fplane.Normal();
	if (trim->Face()->m_bRev) {
	    trim_norm = trim_norm * -1;
	}
    } else {
	int ev1 = 0;
	int ev2 = 0;
	if (surface_EvNormal(s, t_2d1.x, t_2d1.y, t1, v1)) {
	    if (trim->Face()->m_bRev) {
		v1 = v1 * -1;
	    }
	    ev1 = 1;
	}
	if (surface_EvNormal(s, t_2d2.x, t_2d2.y, t2, v2)) {
	    if (trim->Face()->m_bRev) {
		v2 = v2 * -1;
	    }
	    ev2 = 1;
	}
	// If we got both of them, go with the closest one
	if (ev1 && ev2) {
	    trim_norm = (v.Point().DistanceTo(t1) < v.Point().DistanceTo(t2)) ? v1 : v2;
	}

	if (ev1 && !ev2) {
	    trim_norm = v1;
	}

	if (!ev1 && ev2) {
	    trim_norm = v2;
	}
    }

    return trim_norm;
}

static void
calc_singular_vert_norm(struct ON_Brep_CDT_State *s_cdt, int index)
{
    ON_BrepVertex& v = s_cdt->brep->m_V[index];
    int have_calculated = 0;
    ON_3dVector vnrml(0.0, 0.0, 0.0);

    if (s_cdt->singular_vert_to_norms->find((*s_cdt->vert_pnts)[index]) != (s_cdt->singular_vert_to_norms->end())) {
	// Already processed this one
	return;
    }

    //bu_log("Processing vert %d (%f %f %f)\n", index, v.Point().x, v.Point().y, v.Point().z);

    for (int eind = 0; eind != v.EdgeCount(); eind++) {
	ON_3dVector trim1_norm = ON_3dVector::UnsetVector;
	ON_3dVector trim2_norm = ON_3dVector::UnsetVector;
	ON_BrepEdge& edge = s_cdt->brep->m_E[v.m_ei[eind]];
	if (edge.TrimCount() != 2) {
	    // Don't know what to do with this yet... skip.
	    continue;
	}
	ON_BrepTrim *trim1 = edge.Trim(0);
	ON_BrepTrim *trim2 = edge.Trim(1);

	if (trim1->m_type != ON_BrepTrim::singular) {
	    trim1_norm = calc_trim_vnorm(v, trim1);
	}
	if (trim2->m_type != ON_BrepTrim::singular) {
	    trim2_norm = calc_trim_vnorm(v, trim2);
	}

	// If one of the normals is unset and the other comes from a plane, use it
	if (trim1_norm == ON_3dVector::UnsetVector && trim2_norm != ON_3dVector::UnsetVector) {
	    const ON_Surface *s2 = trim2->SurfaceOf();
	    if (!s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		continue;
	    }
	    trim1_norm = trim2_norm;
	}
	if (trim1_norm != ON_3dVector::UnsetVector && trim2_norm == ON_3dVector::UnsetVector) {
	    const ON_Surface *s1 = trim1->SurfaceOf();
	    if (!s1->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		continue;
	    }
	    trim2_norm = trim1_norm;
	}

	// If we have disagreeing normals and one of them is from a planar surface, go
	// with that one
	if (NEAR_EQUAL(ON_DotProduct(trim1_norm, trim2_norm), -1, VUNITIZE_TOL)) {
	    const ON_Surface *s1 = trim1->SurfaceOf();
	    const ON_Surface *s2 = trim2->SurfaceOf();
	    if (!s1->IsPlanar(NULL, ON_ZERO_TOLERANCE) && !s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		// Normals severely disagree, no planar surface to fall back on - can't use this
		continue;
	    }
	    if (s1->IsPlanar(NULL, ON_ZERO_TOLERANCE) && s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		// Two disagreeing planes - can't use this
		continue;
	    }
	    if (s1->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		trim2_norm = trim1_norm;
	    }
	    if (s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		trim1_norm = trim2_norm;
	    }
	}

	// Add the normals to the vnrml total
	vnrml += trim1_norm;
	vnrml += trim2_norm;
	have_calculated = 1;

    }
    if (!have_calculated) {
	return;
    }

    // Average all the successfully calculated normals into a new unit normal
    vnrml.Unitize();

    // We store this as a point to keep C++ happy...  If we try to
    // propagate the ON_3dVector type through all the CDT logic it
    // triggers issues with the compile.
    (*s_cdt->vert_avg_norms)[index] = new ON_3dPoint(vnrml);
    s_cdt->w3dnorms->push_back((*s_cdt->vert_avg_norms)[index]);

    // If we have a vertex normal, add it to the map which will allow us
    // to ascertain if a given point has such a normal.  This will allow
    // a point-based check even if we don't know a vertex index locally
    // in the code.
    (*s_cdt->singular_vert_to_norms)[(*s_cdt->vert_pnts)[index]] = (*s_cdt->vert_avg_norms)[index];

}

int
ON_Brep_CDT_Tessellate(struct ON_Brep_CDT_State *s_cdt, int face_cnt, int *faces)
{

    ON_wString wstr;
    ON_TextLog tl(wstr);

    // Check for any conditions that are show-stoppers
    ON_wString wonstr;
    ON_TextLog vout(wonstr);
    if (!s_cdt->orig_brep->IsValid(&vout)) {
	bu_log("brep is NOT valid, cannot produce watertight mesh\n");
	//return -1;
    }

    // For now, edges must have 2 and only 2 trims for this to work.
    for (int index = 0; index < s_cdt->orig_brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = s_cdt->orig_brep->m_E[index];
	if (edge.TrimCount() != 2) {
	    bu_log("Edge %d trim count: %d - can't (yet) do watertight meshing\n", edge.m_edge_index, edge.TrimCount());
	    return -1;
	}
    }

    // We may be changing the ON_Brep data, so work on a copy
    // rather than the original object
    if (!s_cdt->brep) {

	s_cdt->brep = new ON_Brep(*s_cdt->orig_brep);

	// Attempt to minimize situations where 2D and 3D distances get out of sync
	// by shrinking the surfaces down to the active area of the face
	s_cdt->brep->ShrinkSurfaces();

    }

    ON_Brep* brep = s_cdt->brep;

    // If this is the first time through, there are a number of once-per-conversion
    // operations to take care of.
    if (!s_cdt->w3dpnts->size()) {

	// Translate global relative tolerances into physical dimensions based
	// on the BRep bounding box
	cdt_tol_global_calc(s_cdt);

	// Reparameterize the face's surface and transform the "u" and "v"
	// coordinates of all the face's parameter space trimming curves to
	// minimize distortion in the map from parameter space to 3d.
	for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	    ON_BrepFace *face = brep->Face(face_index);
	    const ON_Surface *s = face->SurfaceOf();
	    double surface_width, surface_height;
	    if (s->GetSurfaceSize(&surface_width, &surface_height)) {
		face->SetDomain(0, 0.0, surface_width);
		face->SetDomain(1, 0.0, surface_height);
	    }
	}

	/* We want to use ON_3dPoint pointers and BrepVertex points, but
	 * vert->Point() produces a temporary address.  If this is our first time
	 * through, make stable copies of the Vertex points. */
	for (int index = 0; index < brep->m_V.Count(); index++) {
	    ON_BrepVertex& v = brep->m_V[index];
	    (*s_cdt->vert_pnts)[index] = new ON_3dPoint(v.Point());
	    CDT_Add3DPnt(s_cdt, (*s_cdt->vert_pnts)[index], -1, v.m_vertex_index, -1, -1, -1, -1);
	    // topologically, any vertex point will be on edges
	    s_cdt->edge_pnts->insert((*s_cdt->vert_pnts)[index]);
	}

	/* If this is the first time through, check for singular trims.  For
	 * vertices associated with such a trim get vertex normals that are the
	 * average of the surface normals at the junction from faces that don't
	 * use a singular trim to reference the vertex.
	 */
	for (int index = 0; index < brep->m_T.Count(); index++) {
	    ON_BrepTrim &trim = s_cdt->brep->m_T[index];
	    if (trim.m_type == ON_BrepTrim::singular) {
		ON_BrepVertex *v1 = trim.Vertex(0);
		ON_BrepVertex *v2 = trim.Vertex(1);
		calc_singular_vert_norm(s_cdt, v1->m_vertex_index);
		calc_singular_vert_norm(s_cdt, v2->m_vertex_index);
	    }
	}

	// Set up the edge containers that will manage the edge subdivision.
	initialize_edge_containers(s_cdt);

	// Next, for each face and each loop in each face define the initial
	// loop polygons.  Note there is no splitting of edges at this point -
	// we are simply establishing the initial closed polygons.
	if (!initialize_loop_polygons(s_cdt)) {
	    return -1;
	}

	// Initialize the tangents.
	std::map<int, std::set<bedge_seg_t *>>::iterator epoly_it;
	for (epoly_it = s_cdt->e2polysegs.begin(); epoly_it != s_cdt->e2polysegs.end(); epoly_it++) {
	    std::set<bedge_seg_t *>::iterator seg_it;
	    for (seg_it = epoly_it->second.begin(); seg_it != epoly_it->second.end(); seg_it++) {
		bedge_seg_t *bseg = *seg_it;
		double ts1 = bseg->tseg1->trim_start;
		double ts2 = bseg->tseg2->trim_start;
		bseg->tan_start = bseg_tangent(s_cdt, bseg, bseg->edge_start, ts1, ts2);

		double te1 = bseg->tseg1->trim_end;
		double te2 = bseg->tseg2->trim_end;
		bseg->tan_end = bseg_tangent(s_cdt, bseg, bseg->edge_end, te1, te2);
	    }
	}

	// Do the non-tolerance based initialization splits.
	if (!initialize_edge_segs(s_cdt)) {
	    std::cout << "Initialization failed for edges\n";
	    return -1;
	}

	// If edge segments are too close together in 2D space compared to their
	// length, it is difficult to mesh them successfully.  Refine edges that
	// are close to other edges.
	refine_close_edges(s_cdt);

#if 1
	// On to tolerance based splitting.  Process the non-linear edges first -
	// we will need information from them to handle the linear edges
	tol_curved_edges_split(s_cdt);

	// After the initial curve split, make another pass looking for curved
	// edges sharing a vertex.  We want larger curves to refine close to the
	// median segment length of the smaller ones, since this situation can be a
	// sign that the surface will generate small triangles near large ones.
	curved_edges_refine(s_cdt);

	// Now, process the linear edges
	tol_linear_edges_split(s_cdt);
#endif

	// Split singularity trims in 2D to provide an easier input to the 2D CDT logic.  NOTE: these
	// splits will produce degenerate (zero area, two identical vertex) triangles in 3D that have
	// to be cleaned up.
	while (s_cdt->unsplit_singular_edges.size()) {
	    std::queue<cpolyedge_t *> w1, w2;
	    std::queue<cpolyedge_t *> *wq, *nq, *tmpq;
	    int cnt = 0;
	    wq = &w1;
	    nq = &w2;
	    nq->push(*(s_cdt->unsplit_singular_edges.begin()));
	    s_cdt->unsplit_singular_edges.erase(s_cdt->unsplit_singular_edges.begin());
	    while (cnt < 6) {
		cnt = 0;
		tmpq = wq;
		wq = nq;
		nq = tmpq;
		while (!wq->empty()) {
		    cpolyedge_t *ce = wq->front();
		    wq->pop();
		    std::set<cpolyedge_t *> nedges = split_singular_seg(s_cdt, ce, 0);
		    std::set<cpolyedge_t *>::iterator n_it;
		    for (n_it = nedges.begin(); n_it != nedges.end(); n_it++) {
			nq->push(*n_it);
			cnt++;
		    }
		}
	    }
	}

	// Rebuild finalized 2D RTrees for faces (needed for surface processing)
	finalize_rtrees(s_cdt);
    } else {
	/* Clear the mesh state, if this container was previously used */
    }

    // Process all of the faces we have been instructed to process, or (default) all faces.
    // Keep track of failures and successes.
    int face_failures = 0;
    int face_successes = 0;
    int fc = ((face_cnt == 0) || !faces) ? s_cdt->brep->m_F.Count() : face_cnt;
    for (int i = 0; i < fc; i++) {
	int fi = ((face_cnt == 0) || !faces) ? i : faces[i];
	if (fi < s_cdt->brep->m_F.Count()) {
	    if (do_triangulation(s_cdt, fi)) {
		face_successes++;
	    } else {
		face_failures++;
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

    if (ON_Brep_CDT_Mesh(&valid_faces, &valid_fcnt, &valid_vertices, &valid_vcnt, NULL, NULL, NULL, NULL, s_cdt, 0, NULL) < 0) {
	return -1;
    }

    struct bg_trimesh_solid_errors se = BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    int invalid = bg_trimesh_solid2(valid_vcnt, valid_fcnt, valid_vertices, valid_faces, &se);

    if (invalid) {
	trimesh_error_report(s_cdt, valid_fcnt, valid_vcnt, valid_faces, valid_vertices, &se);
    }

    bu_free(valid_faces, "faces");
    bu_free(valid_vertices, "vertices");

    if (invalid) {
	return 1;
    }

    return 0;

}

// Generate a BoT with normals.
int
ON_Brep_CDT_Mesh(
	int **faces, int *fcnt,
	fastf_t **vertices, int *vcnt,
	int **face_normals, int *fn_cnt,
	fastf_t **normals, int *ncnt,
	struct ON_Brep_CDT_State *s_cdt,
	int exp_face_cnt, int *exp_faces)
{
    size_t triangle_cnt = 0;
    if (!faces || !fcnt || !vertices || !vcnt || !s_cdt || !s_cdt->brep) {
	return -1;
    }

    /* We can ignore the face normals if we want, but if some of the
     * return variables are non-NULL they all need to be non-NULL */
    if (face_normals || fn_cnt || normals || ncnt) {
	if (!face_normals || !fn_cnt || !normals || !ncnt) {
	    return -1;
	}
    }

    std::vector<int> active_faces;
    if (!exp_face_cnt || !exp_faces) {
	for (int face_index = 0; face_index < s_cdt->brep->m_F.Count(); face_index++) {
	    active_faces.push_back(face_index);
	}
    } else {
	for (int i = 0; i < exp_face_cnt; i++) {
	    active_faces.push_back(exp_faces[i]);
	}
    }

    /* We know now the final triangle set.  We need to build up the set of
     * unique points and normals to generate a mesh containing only the
     * information actually used by the final triangle set. */
    std::set<ON_3dPoint *> vfpnts;
    std::set<ON_3dPoint *> vfnormals;
    std::set<ON_3dPoint *> flip_normals;
    for (size_t fi = 0; fi < active_faces.size(); fi++) {
	cdt_mesh_t *fmesh = &s_cdt->fmeshes[fi];
	RTree<size_t, double, 3>::Iterator tree_it;
	fmesh->tris_tree.GetFirst(tree_it);
	size_t t_ind;
	triangle_t tri;
	while (!tree_it.IsNull()) {
	    t_ind = *tree_it;
	    tri = fmesh->tris_vect[t_ind];
	    for (size_t j = 0; j < 3; j++) {
		ON_3dPoint *p3d = fmesh->pnts[tri.v[j]];
		vfpnts.insert(p3d);
		ON_3dPoint *onorm = NULL;
		if (s_cdt->singular_vert_to_norms->find(p3d) != s_cdt->singular_vert_to_norms->end()) {
		    // Use calculated normal for singularity points
		    onorm = (*s_cdt->singular_vert_to_norms)[p3d];
		} else {
		    onorm = fmesh->normals[fmesh->nmap[tri.v[j]]];
		}
		if (onorm) {
		    vfnormals.insert(onorm);
		    if (fmesh->m_bRev) {
			flip_normals.insert(onorm);
		    }
		}
	    }
	    triangle_cnt++;
	    ++tree_it;
	}
    }

    //bu_log("tri_cnt: %zd\n", triangle_cnt);

    // We know how many faces, points and normals we need now - initialize BoT containers.
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

    // Populate the arrays and map the ON containers to their corresponding BoT array entries
    std::set<ON_3dPoint *>::iterator p_it;

    // Index vertex points and assign them to the BoT array
    std::map<ON_3dPoint *, int> on_pnt_to_bot_pnt;
    int pnt_ind = 0;
    for (p_it = vfpnts.begin(); p_it != vfpnts.end(); p_it++) {
	ON_3dPoint *vp = *p_it;
	(*vertices)[pnt_ind*3] = vp->x;
	(*vertices)[pnt_ind*3+1] = vp->y;
	(*vertices)[pnt_ind*3+2] = vp->z;
	on_pnt_to_bot_pnt[vp] = pnt_ind;
	(*s_cdt->bot_pnt_to_on_pnt)[pnt_ind] = vp;
	pnt_ind++;
    }

    // Index vertex normal vectors and assign them to the BoT array.  Normal
    // vectors are not always uniquely mapped to vertices (consider, for
    // example, the triangles joining at a sharp edge of a box), but what we
    // are doing here is establishing unique integer identifiers for all normal
    // vectors.  In other words, this is not a unique vertex to normal mapping,
    // but a normal vector to unique index mapping.
    //
    // The mapping of 2D triangle point to its associated normal is the
    // responsibility of the  p2t_to_on3_norm_map container
    std::map<ON_3dPoint *, int> on_norm_to_bot_norm;
    size_t norm_ind = 0;
    if (normals) {
	for (p_it = vfnormals.begin(); p_it != vfnormals.end(); p_it++) {
	    ON_3dPoint *vn = *p_it;
	    ON_3dVector vnf(*vn);
	    if (flip_normals.find(vn) != flip_normals.end()) {
		vnf = -1 *vnf;
	    }
	    (*normals)[norm_ind*3] = vnf.x;
	    (*normals)[norm_ind*3+1] = vnf.y;
	    (*normals)[norm_ind*3+2] = vnf.z;
	    on_norm_to_bot_norm[vn] = norm_ind;
	    norm_ind++;
	}
    }

    // Iterate over faces, adding points and faces to BoT container.  Note: all
    // 3D points should be geometrically unique in this final container.
    int face_cnt = 0;
    for (size_t fi = 0; fi < active_faces.size(); fi++) {
	cdt_mesh_t *fmesh = &s_cdt->fmeshes[fi];
	RTree<size_t, double, 3>::Iterator tree_it;
	fmesh->tris_tree.GetFirst(tree_it);
	size_t t_ind;
	triangle_t tri;
	while (!tree_it.IsNull()) {
	    t_ind = *tree_it;
	    tri = fmesh->tris_vect[t_ind];
	    for (size_t j = 0; j < 3; j++) {
		ON_3dPoint *op = fmesh->pnts[tri.v[j]];
		(*faces)[face_cnt*3 + j] = on_pnt_to_bot_pnt[op];

		if (normals) {
		    ON_3dPoint *onorm;
		    if (s_cdt->singular_vert_to_norms->find(op) != s_cdt->singular_vert_to_norms->end()) {
			// Use calculated normal for singularity points
			onorm = (*s_cdt->singular_vert_to_norms)[op];
		    } else {
			onorm = fmesh->normals[fmesh->nmap[fmesh->p2ind[op]]];
		    }

		    (*face_normals)[face_cnt*3 + j] = on_norm_to_bot_norm[onorm];
		}
	    }

	    face_cnt++;
	    ++tree_it;
	}
    }

    return 0;
}

// TODO - need a function that not only checks the integrity of
// each face mesh, but also validates all the boundary edge related
// information needed to stitch them together.  This is beyond the
// scope of the face mesh validity routines, so we'll have to do it
// at the CDT state level.
bool
CDT_Audit(struct ON_Brep_CDT_State *s_cdt)
{
    bool ret = true;
    if (!s_cdt) return false;

    std::map<int, std::set<bedge_seg_t *>>::iterator ps_it;

    int bedge_cnt = 0;

    for (ps_it = s_cdt->e2polysegs.begin(); ps_it != s_cdt->e2polysegs.end(); ps_it++) {
	std::set<bedge_seg_t *>::iterator b_it;
	for (b_it = ps_it->second.begin(); b_it != ps_it->second.end(); b_it++) {
	    bedge_seg_t *eseg = *b_it;
	    std::vector<std::pair<cdt_mesh_t *,uedge_t>> uedges = eseg->uedges();
	    cdt_mesh_t *fmesh_f1 = uedges[0].first;
	    cdt_mesh_t *fmesh_f2 = uedges[1].first;
	    uedge_t ue1 = uedges[0].second;
	    uedge_t ue2 = uedges[1].second;
	    std::set<size_t> f1_tris = fmesh_f1->uedges2tris[ue1];
	    std::set<size_t> f2_tris = fmesh_f2->uedges2tris[ue2];

	    bool bad_tris = false;

	    bad_tris = (bad_tris || ((fmesh_f1->f_id != fmesh_f2->f_id) && (f1_tris.size() != 1 || f2_tris.size() != 1)));
	    bad_tris = (bad_tris || ((fmesh_f1->f_id == fmesh_f2->f_id) && ((f1_tris.size() != 0 && f1_tris.size() != 2) || (f2_tris.size() != 0 && f2_tris.size() != 2) )));
	    bad_tris = (bad_tris || (!f1_tris.size() && !f2_tris.size()));

	    if (bad_tris) {
		if (fmesh_f1->f_id == fmesh_f2->f_id) {
		    if (f1_tris.size() == 1 || (!f1_tris.size() && !f2_tris.size())) {
			std::cerr << "FATAL: could not find expected triangle in mesh " << fmesh_f1->name << "," << fmesh_f1->f_id << "\n";
			std::cerr << "uedge: " << ue1.v[0] << "," << ue1.v[1] << "\n";
			std::string fpname = std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh_f1->f_id) + std::string(".plot3");
			fmesh_f1->tris_plot(fpname.c_str());
		    }
		    if (f2_tris.size() == 1 || (!f1_tris.size() && !f2_tris.size())) {
			std::cerr << "FATAL: could not find expected triangle in mesh " << fmesh_f2->name << "," << fmesh_f2->f_id  << "\n";
			std::cerr << "uedge: " << ue2.v[0] << "," << ue2.v[1] << "\n";
			std::string fpname = std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh_f2->f_id) + std::string(".plot3");
			fmesh_f2->tris_plot(fpname.c_str());
		    }
		} else {
		    if (f1_tris.size() == 0) {
			std::cerr << "FATAL: could not find expected triangle in mesh " << fmesh_f1->name << "," << fmesh_f1->f_id << "\n";
			std::cerr << "uedge: " << ue1.v[0] << "," << ue1.v[1] << "\n";
			std::string fpname = std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh_f1->f_id) + std::string(".plot3");
			fmesh_f1->tris_plot(fpname.c_str());
		    }
		    if (f2_tris.size() == 0) {
			std::cerr << "FATAL: could not find expected triangle in mesh " << fmesh_f2->name << "," << fmesh_f2->f_id  << "\n";
			std::cerr << "uedge: " << ue2.v[0] << "," << ue2.v[1] << "\n";
			std::string fpname = std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh_f2->f_id) + std::string(".plot3");
			fmesh_f2->tris_plot(fpname.c_str());
		    }
		}
		std::string ename = std::string(s_cdt->name) + std::string("_edge_") + std::to_string(bedge_cnt) + std::string(".plot3");
		eseg->plot(ename.c_str());
		ret = false;
	    }
	    bedge_cnt++;
	}
    }

    return ret;
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

