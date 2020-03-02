/*                        C D T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2020 United States Government as represented by
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
#include "bu/malloc.h"
#include "bu/vls.h"
#include "brep/pullback.h"
#include "./cdt.h"

mesh_uedge_t *
brep_cdt_state::get_uedge()
{
    size_t n_ind;
    if (b_uequeue.size()) {
	n_ind = b_uequeue.front();
	b_uequeue.pop();
    } else {
	mesh_uedge_t nedge;
	b_uedges_vect.push_back(nedge);
	n_ind = b_uedges_vect.size() - 1;
    }
    mesh_uedge_t *nue = &(b_uedges_vect[n_ind]);
    nue->vect_ind = n_ind;
    return nue;
}
void
brep_cdt_state::put_uedge(mesh_uedge_t *ue)
{
    if (!ue) return;
    size_t n_ind = ue->vect_ind;

    // If we no longer have any ordered edges associated with the uedge, we can
    // reuse it.  Otherwise, it's still active.
    if (!ue->e[0] && !ue->e[1]) {
	ue->reset();
	b_uequeue.push(n_ind);
    }
}

void
brep_cdt_state::delete_uedge(mesh_uedge_t &ue)
{
    if (ue.vect_ind == -1) {
	return;
    }
    // Remove from RTree
    size_t ind = ue.vect_ind;
    double p1[3];
    p1[0] = ue.bb.Min().x - 2*ON_ZERO_TOLERANCE;
    p1[1] = ue.bb.Min().y - 2*ON_ZERO_TOLERANCE;
    p1[2] = ue.bb.Min().z - 2*ON_ZERO_TOLERANCE;
    double p2[3];
    p2[0] = ue.bb.Max().x + 2*ON_ZERO_TOLERANCE;
    p2[1] = ue.bb.Max().y + 2*ON_ZERO_TOLERANCE;
    p2[2] = ue.bb.Max().z + 2*ON_ZERO_TOLERANCE;
    b_uedges_tree.Remove(p1, p2, ind);

    put_uedge(&ue);
}

static bool Nearby_VectAdd(size_t data, void *a_context) {
    std::vector<size_t> *nearby = (std::vector<size_t> *)a_context;
    nearby->push_back(data);
    return true;
}

long
brep_cdt_state::find_uedge(mesh_edge_t &e)
{
    ON_3dPoint on_p1 = b_pnts[e.v[0]].p;
    ON_3dPoint on_p2 = b_pnts[e.v[1]].p;
    ON_BoundingBox bb(on_p1, on_p2);
    double p1[3];
    p1[0] = bb.Min().x - 2*ON_ZERO_TOLERANCE;
    p1[1] = bb.Min().y - 2*ON_ZERO_TOLERANCE;
    p1[2] = bb.Min().z - 2*ON_ZERO_TOLERANCE;
    double p2[3];
    p2[0] = bb.Max().x + 2*ON_ZERO_TOLERANCE;
    p2[1] = bb.Max().y + 2*ON_ZERO_TOLERANCE;
    p2[2] = bb.Max().z + 2*ON_ZERO_TOLERANCE;

    std::vector<size_t> near_uedges;
    b_uedges_tree.Search(p1, p2, Nearby_VectAdd, &near_uedges);

    bool c1, c2;
    for (size_t i = 0; i < near_uedges.size(); i++) {
	mesh_uedge_t &ue = b_uedges_vect[near_uedges[i]];
	c1 = (ue.v[0] == e.v[0] || ue.v[1] == e.v[0]);
	c2 = (ue.v[0] == e.v[1] || ue.v[1] == e.v[1]);
	if (c1 && c2) {
	    // Found a matching uedge
	    return ue.vect_ind;
	}    
    }

    return -1;
}



void
brep_cdt_state::brep_cpy_init()
{
    // We will probably be changing the ON_Brep data, so work on a copy
    // rather than the original object
    brep = new ON_Brep(*orig_brep);

    // Attempt to minimize situations where 2D and 3D distances get out of sync
    // by shrinking the surfaces down to the active area of the face
    brep->ShrinkSurfaces();

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
}

void
brep_cdt_state::verts_init()
{
    // Populate b_pnts with all the vertex points.
    for (int vert_index = 0; vert_index < brep->m_V.Count(); vert_index++) {
	mesh_point_t np;
	np.cdt = cdt;
	np.vert_index = vert_index;
	np.p = brep->m_V[vert_index].Point();
	np.n = ON_3dVector::UnsetVector;
	np.type = B_VERT;
	np.update();
	np.vect_ind = b_pnts.size();
	b_pnts.push_back(np);
    }

    // Flag singular vertices.
    for (int index = 0; index < brep->m_T.Count(); index++) {
	ON_BrepTrim &trim = brep->m_T[index];
	if (trim.m_type == ON_BrepTrim::singular) {
	    b_pnts[trim.Vertex(0)->m_vertex_index].singular = true;
	    b_pnts[trim.Vertex(1)->m_vertex_index].singular = true;
	}
    }

    // Calculate normals for singular vertices - the surface normals at those
    // aren't well defined, so other nearby information must be used.
    for (size_t index = 0; index < b_pnts.size(); index++) {
	mesh_point_t &p = b_pnts[index];
	if (p.singular) {
	    p.n = singular_vert_norm(brep, p.vert_index);
	}
    }
}

static ON_3dVector
edge_tangent(ON_BrepEdge& edge, ON_NurbsCurve *nc, double eparam, double t1param, double t2param)
{
    ON_3dPoint tmp;
    ON_3dVector tangent = ON_3dVector::UnsetVector;
    if (!nc->EvTangent(eparam, tmp, tangent)) {
	if (t1param < DBL_MAX && t2param < DBL_MAX) {
	    // If the edge curve failed, average tangents from trims
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

void
brep_cdt_state::uedges_init()
{
    // Build the initial set of unordered edges.  Unordered edges on BRep face
    // edges are independent of faces - ordered edges are specific to
    // individual faces.  As yet, we're not concerned with specific faces, so
    // only create the unordered edges.
    //
    // NOTE: At this stage need the index of each b_uedge to match the
    // ON_BrepEdge index value, so we can populate the loop structures without
    // having to build a std::map.  If for any reason that should not prove
    // viable, need to build and use the map.
    for (int index = 0; index < brep->m_E.Count(); index++) {
	ON_BrepEdge& edge = brep->m_E[index];
	long v0 = edge.Vertex(0)->m_vertex_index;
	long v1 = edge.Vertex(1)->m_vertex_index;
	mesh_uedge_t *ue = get_uedge();
	ue->cdt = cdt;
	ue->edge = brep->Edge(index);
	ue->edge_pnts[0] = v0;
	ue->edge_pnts[1] = v1;

	// This sorting is what makes a mesh_uedge_t unordered - otherwise the edge
	// curve will determine how data is stored.
	ue->v[0] = (v1 > v0) ? v0 : v1;
	ue->v[1] = (v1 > v0) ? v1 : v0;

	// Characterize the edge type
	ue->type = (edge.EdgeCurveOf() == NULL) ? B_SINGULAR : B_BOUNDARY;

	// Set up the NURBS curve to be used for splitting
	const ON_Curve* crv = edge.EdgeCurveOf();
	ue->nc = crv->NurbsCurve();

	// Curved edges will need some minimal splitting regardless, so
	// identify them up front
	double c_len_tol = 0.1 * ue->nc->ControlPolygonLength();
	double tol = (c_len_tol < BN_TOL_DIST) ? c_len_tol : BN_TOL_DIST;
	ue->linear = crv->IsLinear(tol);

	// Match the parameterization to the control polygon length, so
	// distances in t paramater space will have some relationship to 3D
	// distances.
	ue->cp_len = ue->nc->ControlPolygonLength();
	ue->nc->SetDomain(0.0, ue->cp_len);
	ue->t_start = 0.0;
	ue->t_end = ue->cp_len;

	// Set trim curve domains to match the updated edge curve domains.
	for (int i = 0; i < edge.TrimCount(); i++) {
	    ON_BrepTrim *trim = edge.Trim(i);
	    brep->m_T[trim->TrimCurveIndexOf()].SetDomain(ue->t_start, ue->t_end);
	}

	// Calculate the edge length once up front
	ue->len = b_pnts[ue->v[0]].p.DistanceTo(b_pnts[ue->v[1]].p);

	// Set an initial edge bbox.  Because this is the first round,
	// update will need something well defined to operate on - just
	// define a minimal box around the point.  Subsequent update calls
	// will have an initialized box and won't need this.
	ON_3dPoint pztol(ON_ZERO_TOLERANCE, ON_ZERO_TOLERANCE, ON_ZERO_TOLERANCE);
	ue->bb = ON_BoundingBox(b_pnts[v0].p,b_pnts[v1].p);
	ue->bb.m_max = ue->bb.m_max + pztol;
	ue->bb.m_min = ue->bb.m_min - pztol;
	ue->update();

	// Establish the vertex to edge connectivity
	b_pnts[ue->v[0]].uedges.insert(ue);
	b_pnts[ue->v[1]].uedges.insert(ue);

	// Initialize the tangent values.  For the specific case of the starting
	// values when we need to evaluate at vertex points, back up the curve
	// evaluation with trim parameter values in case the evaluation at the
	// vertex point should fail to produce a tangent.
	for (int i = 0; i < 2; i++) {
	    double t1param, t2param;
	    double ue_param = (i == 0) ? ue->t_start : ue->t_end;
	    ON_BrepTrim *trim1 = edge.Trim(0);
	    ON_BrepTrim *trim2 = edge.Trim(1);
	    int ind1 = (i == 0) ? 0 : 1;
	    int ind2 = (i == 0) ? 1 : 0;
	    t1param = (trim1->m_bRev3d) ? trim1->Domain().m_t[ind2] : trim1->Domain().m_t[ind1];
	    t2param = (trim2->m_bRev3d) ? trim2->Domain().m_t[ind2] : trim2->Domain().m_t[ind1];
	    ue->tangents[i] = edge_tangent(edge, ue->nc, ue_param, t1param, t2param);
	    ue->t_verts[i] = (i == 0) ? v0 : v1;
	}
    }

    // Now that we have edge associations, update vert bboxes.
    for (size_t index = 0; index < b_pnts.size(); index++) {
	b_pnts[index].update();
    }
}

// THOUGHT: for edge splitting, we don't actually need to split the trim curves
// at all - what we need are the 2D coordinates for the polygon point to insert,
// and the identification of the start/end points in the polygon between which
// the new point will be inserted.  The trim curve parameter isn't actually important,
// so instead of trying to split the trim curve we can just use the closest point
// on surface routine to go directly to the u,v coordinates for the point.
bool
brep_cdt_state::faces_init()
{
    for (int index = 0; index < brep->m_F.Count(); index++) {
	mesh_t m_new;
	b_faces_vect.push_back(m_new);
	mesh_t &m = b_faces_vect[b_faces_vect.size() - 1];
	if (a_faces.size() && a_faces.find(index) == a_faces.end()) {
	    // If we're not processing this face, go no further
	    continue;
	}
	m.cdt = cdt;
	m.f = brep->Face(index);
	m.bb = m.f->BoundingBox();
	m.vect_ind = b_faces_vect.size() - 1;
	for (int li = 0; li < brep->m_L.Count(); li++) {
	    const ON_BrepLoop *loop = m.f->Loop(li);
	    polygon_t *l = m.get_poly(li);
	    if (m.f->OuterLoop()->m_loop_index == loop->m_loop_index) {
		m.outer_loop = l->vect_ind;
	    }
	    // Initialize polygon points and loop edges
	    for (int lti = 0; lti < loop->TrimCount(); lti++) {
		ON_BrepTrim *trim = loop->Trim(lti);
		ON_Interval range = trim->Domain();
		long cp1, cp2;
		if (lti == 0) {
		    ON_2dPoint cp = trim->PointAt(range.m_t[0]);
		    cp1 = l->add_point(&b_pnts[trim->Vertex(0)->m_vertex_index], cp);
		} else {
		    cp1 = l->p_pnts_vect.size()-1;
		}
		if (lti < loop->TrimCount() - 1) {
		    ON_2dPoint cp = trim->PointAt(range.m_t[1]);
		    cp2 = l->add_point(&b_pnts[trim->Vertex(1)->m_vertex_index], cp);
		} else {
		    cp2 = 0;
		}
		poly_edge_t *pe = l->add_ordered_edge(cp1, cp2, trim->m_trim_index);
		pe->m_bRev3d = trim->m_bRev3d;

		// If we're not singular, define an ordered 3D edge and
		// associate it with an unordered 3D edge (creating at need).
		ON_BrepEdge *edge = trim->Edge();
		if (edge && edge->EdgeCurveOf()) {
		    pe->type = B_BOUNDARY;
		    mesh_edge_t *e3d = m.edge(*pe);
		    if (!m.uedge(*e3d)) {
			bu_vls_printf(m.cdt->msgs, "fatal error - couldn't associate an ordered 3D edge with an unordered 3D edge\n");
			return false;
		    }
		} else {
		    pe->type = B_SINGULAR;
		}
	    }
	}

	// Insert face bounding box into the faces RTree - unlike more active
	// trees this is a one time operation.  The face bbox is based on the
	// BRep bounding box and does not change as a consequence of subsequent
	// processing.
	double p1[3];
	p1[0] = m.bb.Min().x;
	p1[1] = m.bb.Min().y;
	p1[2] = m.bb.Min().z;
	double p2[3];
	p2[0] = m.bb.Max().x;
	p2[1] = m.bb.Max().y;
	p2[2] = m.bb.Max().z;
	m.cdt->i->s.b_faces_tree.Insert(p1, p2, (size_t)m.f->m_face_index);
    }

    return true;
}

static int
brep_cdt_init(struct brep_cdt *s, void *bv, const char *objname)
{
    if (!s || !bv || !objname) return -1;
    s->i = new brep_cdt_impl;
    if (!s->i) return -1;
    s->i->s.cdt = s;
    s->i->s.orig_brep = (ON_Brep *)bv;
    s->i->s.name = std::string(objname);
    s->msgs = (struct bu_vls *)bu_calloc(1, sizeof(struct bu_vls), "message buffer");
    bu_vls_init(s->msgs);
    return 0;
}

extern "C" int
brep_cdt_create(struct brep_cdt **cdt, void *bv, const char *objname)
{
    if (!cdt || !bv) return -1;
    (*cdt) = new brep_cdt;
    return brep_cdt_init(*cdt, bv, objname);
}

static void
brep_cdt_clear(struct brep_cdt *s)
{
    delete s->i;
    bu_vls_free(s->msgs);
    bu_free(s->msgs, "message buffer free");
}

extern "C" void
brep_cdt_destroy(struct brep_cdt *s)
{
    if (!s) return;
    brep_cdt_clear(s);
    delete s;
}


int
brep_cdt_triangulate(struct brep_cdt *s_cdt, int face_cnt, int *faces, long flags)
{
    if (!s_cdt) return -1;

    // Check validity and solidity
    s_cdt->i->s.is_valid = s_cdt->i->s.orig_brep->IsValid();
    s_cdt->i->s.is_solid = s_cdt->i->s.orig_brep->IsSolid();

    // Check for any conditions that are show-stoppers
    if (!s_cdt->i->s.is_valid) {
	bu_vls_printf(s_cdt->msgs, "NOTE: brep is NOT valid, cannot produce watertight mesh\n");
    }

    if (!(flags & BG_CDT_NO_WATERTIGHT) && !s_cdt->i->s.is_solid) {
	bu_vls_printf(s_cdt->msgs, "NOTE: watertight meshing requested but brep is not closed.  Cannot produce watertight mesh.\n");
    }

    if (face_cnt) {
	for (int i = 0; i < face_cnt; i++) {
	    s_cdt->i->s.a_faces.insert(faces[i]);
	}
    }

    // brep init
    s_cdt->i->s.brep_cpy_init();

    // m_V init
    s_cdt->i->s.verts_init();

    // m_E init
    s_cdt->i->s.uedges_init();

    // m_F/m_T init
    s_cdt->i->s.faces_init();

    // Vertices and edges are the only two elements that are not unique to
    // faces.  However, to build up unordered edges we also want the polygon
    // edges in place for the faces.  Build initial polygons now.
    // At this point, with no splitting done, the ON_Brep indicies and brep_cdt
    // indices should still agree.

    // Build up the initial m_edges set and tree, associating ordered edges
    // and polygon edges with the uedges.  polygon edges and ordered edges have
    // an unambiguous association, so the only difficult mapping is between the
    // ordered edge and the unordered edge - we need to translate the two
    // vertex indices into an unordered edge array index (if the associated
    // unordered edge has already been created by the other edge in the
    // pairing.)  Use an RTree lookup on the uedge tree for this, since by
    // definition the two ordered edges will have the same bounding box in
    // space.

    // When we get to splitting: the maximum normal tolerance allowed should
    // probably be 45 degrees - that's splitting a half circle twice, which is
    // pretty much the minimum needed to get anything remotely represenative of
    // the curve (one split per half produces a square for a circle, which is
    // deceptive.)  Need to apply that to surface sampling as well - might help
    // with the "define a minimum valid surface point sampling density"
    // question...

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

