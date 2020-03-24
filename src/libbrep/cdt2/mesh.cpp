/*                       M E S H . C P P
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
/** @file mesh.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "brep/pullback.h"
#include "./cdt.h"

mesh_edge_t *
mesh_t::get_edge()
{
    size_t n_ind;
    if (m_equeue.size()) {
	n_ind = m_equeue.front();
	m_equeue.pop();
    } else {
	mesh_edge_t nedge;
	m_edges_vect.push_back(nedge);
	n_ind = m_edges_vect.size() - 1;
    }
    mesh_edge_t *ne = &(m_edges_vect[n_ind]);
    ne->m = this;
    ne->vect_ind = n_ind;
    return ne;
}

void
mesh_t::put_edge(mesh_edge_t *e)
{
    if (!e) return;
    size_t n_ind = e->vect_ind;

    mesh_uedge_t *rmu3d = NULL;
    poly_edge_t *rmp3d = NULL;
    mesh_tri_t *rmtri = NULL;

    // We'll need to clean up the other edges associated with this edge (if any).
    // Because this routine may be called from other cleanup routines and vice
    // versa, we only call the other cleanup routine if the various pointers
    // haven't been NULLed out by other deletes first.  If they have, just do
    // our bit - someone else is managing the other cleanup.  If not, we need
    // to take care of calling the relevant routines from here.
    if (e->pe) {
        rmp3d = e->pe;
        e->pe = NULL;
        rmp3d->e3d = NULL;
    }
    if (e->uedge) {
        rmu3d = e->uedge;
        e->uedge = NULL;
	for (int i = 0; i < 2; i++) {
	    if (rmu3d->e[i] == e) {
		rmu3d->e[i] = NULL;
	    }
	}
    }
    if (e->tri) {
	rmtri = e->tri;
	e->tri = NULL;
	for (int i = 0; i < 3; i++) {
	    if (rmtri->e[i] == e) {
		// For the triangle edge that is the current edge, just NULL it
		rmtri->e[i] = NULL;
	    } else {
		// For other edges, we'll need to remove them but we don't need
		// to call the tri remove routine again.
		if (rmtri->e[i]) {
		    rmtri->e[i]->tri = NULL;
		}
	    }
	}
    }

    e->reset();
    m_equeue.push(n_ind);

    // Call other cleanup routines, if needed
    if (rmp3d) {
	rmp3d->polygon->put_pedge(rmp3d);
    }
    if (rmu3d) {
	cdt->i->s.put_uedge(rmu3d);
    }
    if (rmtri) {
	put_tri(rmtri);
    }
}

mesh_tri_t *
mesh_t::get_tri()
{
    size_t n_ind;
    if (m_tqueue.size()) {
	n_ind = m_tqueue.front();
	m_tqueue.pop();
    } else {
	mesh_tri_t ntri;
	m_tris_vect.push_back(ntri);
	n_ind = m_tris_vect.size() - 1;
    }
    mesh_tri_t *nt = &(m_tris_vect[n_ind]);
    nt->m = this;
    nt->vect_ind = n_ind;
    return nt;
}

void
mesh_t::put_tri(mesh_tri_t *t)
{
    if (!t) return;
    size_t n_ind = t->vect_ind;
    t->reset();
    m_tqueue.push(n_ind);
}

polygon_t *
mesh_t::get_poly(int l_id = -1)
{
    size_t n_ind;
    if (m_pqueue.size()) {
	n_ind = m_pqueue.front();
	m_pqueue.pop();
    } else {
	polygon_t npoly;
	m_polys_vect.push_back(npoly);
	n_ind = m_polys_vect.size() - 1;
    }
    polygon_t *np = &(m_polys_vect[n_ind]);
    np->m = this;
    np->vect_ind = n_ind;
    np->loop_id = l_id;
    return np;
}

polygon_t *
mesh_t::get_poly(ON_Plane &polyp)
{
    size_t n_ind;
    if (m_pqueue.size()) {
	n_ind = m_pqueue.front();
	m_pqueue.pop();
    } else {
	polygon_t npoly;
	m_polys_vect.push_back(npoly);
	n_ind = m_polys_vect.size() - 1;
    }
    polygon_t *np = &(m_polys_vect[n_ind]);
    np->m = this;
    np->vect_ind = n_ind;
    np->loop_id = -1;
    np->p_plane = polyp;
    return np;
}
void
mesh_t::put_poly(polygon_t *p)
{
    if (!p) return;
    size_t n_ind = p->vect_ind;
    p->reset();
    m_pqueue.push(n_ind);
}

mesh_edge_t *
mesh_t::edge(poly_edge_t &pe)
{
    mesh_edge_t *ne = NULL;

    if (pe.type == B_SINGULAR) {
	return NULL;
    }

    if (pe.e3d) {
	ne = &(m_edges_vect[pe.e3d->vect_ind]);
	return ne;
    }

    size_t n_ind;
    if (m_equeue.size()) {
	n_ind = m_equeue.front();
	m_equeue.pop();
    } else {
	mesh_edge_t nedge;
	nedge.vect_ind = m_edges_vect.size();
	m_edges_vect.push_back(nedge);
	n_ind = nedge.vect_ind;
    }

    // Assign ordered 3D points
    ne = &(m_edges_vect[n_ind]);
    ne->v[0] = (pe.m_bRev3d) ? pe.polygon->p_pnts_vect[pe.v[1]].mp->vect_ind : pe.polygon->p_pnts_vect[pe.v[0]].mp->vect_ind;
    ne->v[1] = (pe.m_bRev3d) ? pe.polygon->p_pnts_vect[pe.v[0]].mp->vect_ind : pe.polygon->p_pnts_vect[pe.v[1]].mp->vect_ind;
    ne->m = this;
    ne->m = pe.polygon->m;
    ne->pe = &pe.polygon->p_pedges_vect[pe.vect_ind];

    // Tell the original polyedge which edge it got assigned
    pe.e3d = &m_edges_vect[ne->vect_ind];

    // Find the unordered edge that has both vertices matching this edge
    std::set<mesh_uedge_t *>::iterator ue_it;
    for (ue_it = ne->m->cdt->i->s.b_pnts[ne->v[0]].uedges.begin(); ue_it != ne->m->cdt->i->s.b_pnts[ne->v[0]].uedges.end(); ue_it++) {
	mesh_uedge_t *ue = *ue_it;
	if ((ue->v[0] == ne->v[0] || ue->v[0] == ne->v[1]) && (ue->v[1] == ne->v[0] || ue->v[1] == ne->v[1])) {
	    ne->uedge = ue;
	    // TODO - do we need to do this type assignment?
	    ne->uedge->type = pe.type;
	    if (ue->e[0]) {
		ue->e[1] = ne;
	    } else {
		ue->e[0] = ne;
	    }
	    ne->uedge->update();
	    break;
	}
    }

    return ne;
}


mesh_uedge_t *
mesh_t::uedge(mesh_edge_t &e)
{
    mesh_uedge_t *ue = e.uedge;

    // If we don't already have an associated uedge, see if one of the
    // points knows about a compatible uedge - another mesh_edge_t might
    // already have created the edge we need.
    if (!ue) {
	for (int i = 0; i < 2; i++) {
	    mesh_point_t &p = cdt->i->s.b_pnts[e.v[i]];
	    std::set<mesh_uedge_t *>::iterator ue_it;
	    for (ue_it = p.uedges.begin(); ue_it != p.uedges.end(); ue_it++) {
		mesh_uedge_t *pue = *ue_it;
		if ((ue->v[0] == pue->v[0] || ue->v[0] == pue->v[1]) && (ue->v[1] == pue->v[0] || ue->v[1] == pue->v[1])) {
		    ue = pue;
		    break;
		}
	    }
	}
    }

    // IFF we've got nothing at this point we need a new uedge - else,
    // we're just making the additional data connections needed in the
    // existing uedge.
    bool new_uedge = false;
    if (!ue) {
	ue = cdt->i->s.get_uedge();
	new_uedge = true;
    }

    // Shouldn't happen...
    if (!ue) return NULL;

    // Check that the uedge container holds the correct info about this ordered edge
    if (!ue->e[0]) {
	ue->e[0] = &m_edges_vect[e.vect_ind];
	// First edge assignment, - assign ordered vert points
	ue->v[0] = (e.v[0] < e.v[1]) ? e.v[0] : e.v[1];
	ue->v[1] = (e.v[0] < e.v[1]) ? e.v[1] : e.v[0];
    } else if (!ue->e[1]) {
	ue->e[1] = &m_edges_vect[e.vect_ind];
    } else {
	// Already have two edges associated with uedge??  This is an error condition
    }

    // Link the uedge back into the ordered edge
    e.uedge = ue;

    if (new_uedge) {
	ON_3dPoint pztol(ON_ZERO_TOLERANCE, ON_ZERO_TOLERANCE, ON_ZERO_TOLERANCE);
	mesh_point_t &p1 = cdt->i->s.b_pnts[ue->v[0]];
	mesh_point_t &p2 = cdt->i->s.b_pnts[ue->v[2]];
	ue->bb = ON_BoundingBox(p1.p, p2.p);
	ue->bb.m_max = ue->bb.m_max + pztol;
	ue->bb.m_min = ue->bb.m_min - pztol;
	ue->update();
	// Let the points know they've got new uedges
	cdt->i->s.b_pnts[ue->v[0]].uedges.insert(ue);
	cdt->i->s.b_pnts[ue->v[1]].uedges.insert(ue);
    }
    return ue;
}

bool
mesh_t::closest_surf_pt(ON_3dVector *sn, ON_3dPoint &s3d, ON_2dPoint &s2d, ON_3dPoint *p, double tol)
{
    s2d = ON_2dPoint::UnsetPoint;
    s3d = ON_3dPoint::UnsetPoint;
    if (sn) {
	(*sn) = ON_3dVector::UnsetVector;
    }

    double cdist;
    if (tol <= 0) {
	surface_GetClosestPoint3dFirstOrder(f->SurfaceOf(), *p, s2d, s3d, cdist);
    } else {
	surface_GetClosestPoint3dFirstOrder(f->SurfaceOf(), *p, s2d, s3d, cdist, 0, ON_ZERO_TOLERANCE, tol);
    }

    if (NEAR_EQUAL(cdist, DBL_MAX, ON_ZERO_TOLERANCE)) {
       	return false;
    }

    if (sn) {
	bool seval = surface_EvNormal(f->SurfaceOf(), s2d.x, s2d.y, s3d, (*sn));
	if (!seval) return false;

	if (f->m_bRev) {
	    (*sn) = (*sn) * -1;
	}
    }

    return true;
}

bool
mesh_t::split_close_edges()
{
    return false;
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

