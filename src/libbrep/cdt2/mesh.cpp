/*                       M E S H . C P P
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


void
mesh_t::bbox_insert()
{
    // Insert vert into RTree - unlike more active trees this is a one time
    // operation since the face bbox does not change.  Consequently, we don't
    // have to remove a stale bbox from the tree.
    double p1[3];
    p1[0] = bb.Min().x;
    p1[1] = bb.Min().y;
    p1[2] = bb.Min().z;
    double p2[3];
    p2[0] = bb.Max().x;
    p2[1] = bb.Max().y;
    p2[2] = bb.Max().z;
    cdt->i->s.b_faces_tree.Insert(p1, p2, (size_t)f->m_face_index);
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
    ne->type = pe.type;
    ne->v[0] = (pe.m_bRev3d) ? pe.polygon->p_pnts_vect[pe.v[1]].mp->vect_ind : pe.polygon->p_pnts_vect[pe.v[0]].mp->vect_ind;
    ne->v[1] = (pe.m_bRev3d) ? pe.polygon->p_pnts_vect[pe.v[0]].mp->vect_ind : pe.polygon->p_pnts_vect[pe.v[1]].mp->vect_ind;
    ne->m = this;
    ne->bbox_update();
    ne->cdt = pe.polygon->cdt;
    ne->m = pe.polygon->m;
    ne->pe = &pe.polygon->p_pedges_vect[pe.vect_ind];

    // Tell the original polyedge which edge it got assigned
    pe.e3d = &m_edges_vect[ne->vect_ind];

    // Find the unordered edge that has both vertices matching this edge
    std::set<size_t>::iterator ue_it;
    for (ue_it = ne->cdt->i->s.b_pnts[ne->v[0]].uedges.begin(); ue_it != ne->cdt->i->s.b_pnts[ne->v[0]].uedges.end(); ue_it++) {
	mesh_uedge_t &ue = ne->cdt->i->s.b_uedges_vect[*ue_it];
	if ((ue.v[0] == ne->v[0] || ue.v[0] == ne->v[1]) && (ue.v[1] == ne->v[0] || ue.v[1] == ne->v[1])) {
	    ne->uedge = &(ne->cdt->i->s.b_uedges_vect[*ue_it]);
	    if (ue.e[0]) {
		ue.e[1] = ne;
	    } else {
		ue.e[0] = ne;
	    }
	    break;
	}
    }

    return ne;
}


mesh_uedge_t *
mesh_t::uedge(mesh_edge_t &e)
{
    if (e.type == B_SINGULAR) return NULL;

    mesh_uedge_t *ue = e.uedge;

    if (!ue) {
	// Check for the uedge with an RTree search.
	long r_ind = cdt->i->s.find_uedge(e);
	if (r_ind >= 0) {
	    ue = &(cdt->i->s.b_uedges_vect[r_ind]);
	}
    }

    // IFF we've got nothing post search put in a new uedge - else, we're just
    // making the data connections to the existing uedge.
    if (!ue) {
	if (cdt->i->s.b_uequeue.size()) {
	    size_t n_ind = cdt->i->s.b_uequeue.front();
	    ue = &(cdt->i->s.b_uedges_vect[n_ind]);
	    cdt->i->s.b_uequeue.pop();
	} else {
	    mesh_uedge_t nedge;
	    nedge.vect_ind = cdt->i->s.b_uedges_vect.size();
	    cdt->i->s.b_uedges_vect.push_back(nedge);
	    ue = &(cdt->i->s.b_uedges_vect[nedge.vect_ind]);
	}
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
    e.uedge = &(cdt->i->s.b_uedges_vect[ue->vect_ind]);

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

