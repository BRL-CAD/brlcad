/*                   P O L Y G O N . C P P
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
/** @file polygon.cpp
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
poly_edge_t::reset()
{
    polygon = NULL;
    vect_ind = -1;
    v[0] = -1;
    v[1] = -1;
    e3d = NULL;
    type = B_UNSET;
    m_trim_index = -1;
    m_bRev3d = false;
    split_status = 0;
    bb = ON_BoundingBox();
}

void
polygon_t::reset()
{
    loop_id = -1;
    vect_ind = -1;
    m = NULL;
    o2p.clear();
    p_edges_tree.RemoveAll();
    p_pedges_vect.clear();
    p_pnts_vect.clear();
    while (!p_pequeue.empty()) p_pequeue.pop();
}

long
polygon_t::add_point(mesh_point_t *meshp)
{
    if (!meshp) {
	return -1;
    }

    poly_point_t pp;

    if (loop_id == -1) {
	p_plane.ClosestPointTo(meshp->p, &pp.u, &pp.v);
    } else {
	ON_2dPoint p2d;
	ON_3dPoint p3d;
	if (!m->closest_surf_pt(NULL, p3d, p2d, &(meshp->p), -1)) {
	    bu_vls_printf(m->cdt->msgs, "closest point evaluation failed for polygon 2D\n");
	    return -1;
	}
	pp.u = p2d.x;
	pp.v = p2d.y;
    }

    // The current index of p_pnts (before we add the new point)
    // is the vector index that will be assigned to this point.
    pp.vect_ind = p_pnts_vect.size();

    // Teach the polygon point where it came from
    pp.mp = meshp;

    // Add the new polygon point
    p_pnts_vect.push_back(pp);

    // Update the original-to-polygon point map
    o2p[pp.mp->vect_ind] = pp.vect_ind;

    return pp.vect_ind;
}

long
polygon_t::add_point(mesh_point_t *meshp, ON_2dPoint &p2d)
{
    if (!meshp) {
	return -1;
    }
    poly_point_t pp;
    pp.u = p2d.x;
    pp.v = p2d.y;

    // The current index of p_pnts (before we add the new point)
    // is the vector index that will be assigned to this point.
    pp.vect_ind = p_pnts_vect.size();

    // Teach the polygon point where it came from
    pp.mp = meshp;

    // Add the new polygon point
    p_pnts_vect.push_back(pp);

    // Update the original-to-polygon point map.  NOTE: for singular
    // points this mapping is not unique, so the result will be a
    // last-insertion-wins in the o2p map.  Assemblies that need
    // to handle singular points will need to manage the process
    // using information beyond that stored in o2p
    o2p[pp.mp->vect_ind] = pp.vect_ind;

    return pp.vect_ind;
}

poly_edge_t *
polygon_t::get_pedge()
{
    size_t n_ind;
    if (p_pequeue.size()) {
	n_ind = p_pequeue.front();
	p_pequeue.pop();
    } else {
	poly_edge_t nedge;
	p_pedges_vect.push_back(nedge);
	n_ind = p_pedges_vect.size() - 1;
    }
    poly_edge_t *npe = &(p_pedges_vect[n_ind]);
    npe->polygon = this;
    npe->vect_ind = n_ind;
    return npe;
}

void
polygon_t::put_pedge(poly_edge_t *pe)
{
    if (!pe) return;
    size_t n_ind = pe->vect_ind;
    pe->reset();
    p_pequeue.push(n_ind);
}

poly_edge_t *
polygon_t::add_ordered_edge(long p1, long p2, int trim_ind = -1)
{
    if (p1 < 0 || p2 < 0) return NULL;
    if (p1 >= (long)p_pnts_vect.size() || p2 >= (long)p_pnts_vect.size()) return NULL;

    // Look up the polygon points associated with the indices
    poly_point_t &pp1 = p_pnts_vect[p1];
    poly_point_t &pp2 = p_pnts_vect[p2];

    // If either point already has two associated edges, we have a problem.
    if (pp1.pedges.size() > 1 || pp2.pedges.size() > 1) {
	return NULL;
    }

    // Look for prev/next settings
    long prev = -1;
    long next = -1;
    std::set<size_t>::iterator p_it;
    for (p_it = pp1.pedges.begin(); p_it != pp1.pedges.end(); p_it++) {
	poly_edge_t &ce = p_pedges_vect[*p_it];
	if (ce.v[1] == p1) {
	    prev = ce.vect_ind;
	}
	if (ce.v[0] == p2) {
	    next = ce.vect_ind;
	}
    }
    for (p_it = pp2.pedges.begin(); p_it != pp2.pedges.end(); p_it++) {
	poly_edge_t &ce = p_pedges_vect[*p_it];
	if (ce.v[1] == p1) {
	    prev = ce.vect_ind;
	}
	if (ce.v[0] == p2) {
	    next = ce.vect_ind;
	}
    }



    poly_edge_t *pe = get_pedge();
    pe->v[0] = p1;
    pe->v[1] = p2;

    // Determine edge type
    if (trim_ind != -1) {
	pe->m_trim_index = trim_ind;
	pe->type = B_BOUNDARY;
    }
    if (pe->type == B_BOUNDARY && (pp1.mp->singular || pp2.mp->singular)) {
	pe->type = B_SINGULAR;
    }

    // Update connectivity
    if (prev != -1) {
	p_pedges_vect[prev].next = pe->vect_ind;
    }
    pe->prev = prev;

    if (next != -1) {
	p_pedges_vect[next].prev = pe->vect_ind;
    }
    pe->next = next;

    pp1.pedges.insert(pe->vect_ind);
    pp2.pedges.insert(pe->vect_ind);

    // IFF the edge is not degenerate (which can happen if we're initializing
    // closed loops) add it to the RTrees.  Note that singularity edges are
    // not necessarily degenerate in 2D - degenerate in this case refers
    // specifically to an edge whose start and end indices are the same.
    if (p1 != p2) {
	ON_Line line(ON_2dPoint(pp1.u, pp1.v), ON_2dPoint(pp2.u, pp2.v));
	pe->bb = line.BoundingBox();
	pe->bb.m_max.x = pe->bb.m_max.x + ON_ZERO_TOLERANCE;
	pe->bb.m_max.y = pe->bb.m_max.y + ON_ZERO_TOLERANCE;
	pe->bb.m_min.x = pe->bb.m_min.x - ON_ZERO_TOLERANCE;
	pe->bb.m_min.y = pe->bb.m_min.y - ON_ZERO_TOLERANCE;
	double bp1[2];
	bp1[0] = pe->bb.Min().x;
	bp1[1] = pe->bb.Min().y;
	double bp2[2];
	bp2[0] = pe->bb.Max().x;
	bp2[1] = pe->bb.Max().y;
	// For the polygon tree, we want the tight box
	p_edges_tree.Insert(bp1, bp2, pe->vect_ind);

	// The parent mesh keeps track of all boundary 2D polyedges present for higher
	// level processing as well.
	if (pe->type == B_BOUNDARY || pe->type == B_SINGULAR) {
	    // For this tree and edge type, we want a more aggressive bounding box
	    ON_BoundingBox lbb = pe->bb;
	    double xdist = lbb.m_max.x - lbb.m_min.x;
	    double ydist = lbb.m_max.y - lbb.m_min.y;
	    double bdist = (xdist < ydist) ? ydist : xdist;
	    if (xdist < bdist) { 
		lbb.m_min.x = lbb.m_min.x - 0.5*bdist;
		lbb.m_max.x = lbb.m_max.x + 0.5*bdist;
	    }
	    if (ydist < bdist) { 
		lbb.m_min.y = lbb.m_min.y - 0.5*bdist;
		lbb.m_max.y = lbb.m_max.y + 0.5*bdist;
	    }
	    bp1[0] = lbb.Min().x;
	    bp1[1] = lbb.Min().y;
	    bp2[0] = lbb.Max().x;
	    bp2[1] = lbb.Max().y;

	    m->loops_tree.Insert(bp1, bp2, (void *)(&p_pedges_vect[pe->vect_ind]));
	}
    }

    return pe;
}

void
polygon_t::remove_ordered_edge(poly_edge_t &pe)
{
    mesh_edge_t *rme3d = NULL;

    // We'll need to clean up the 3D edge associated with this edge (if any).
    // Because this routine may be called from other cleanup routines and vice
    // versa, we only call the other cleanup routine if the various pointers
    // haven't been NULLed out by other deletes first.  If they have, just do
    // our bit - someone else is managing the other cleanup.  If not, we need
    // to take care of calling the relevant routines from here.
    if (pe.e3d) {
	rme3d = pe.e3d;
	pe.e3d = NULL;
	rme3d->pe = NULL;
    }

    // Update other edges in the polygon that are referencing this edge
    std::set<size_t>::iterator p_it;
    for (int i = 0; i < 2; i++) {
	std::set<size_t> &pedges = p_pnts_vect[pe.v[i]].pedges;
	for (p_it = pedges.begin(); p_it != pedges.end(); p_it++) {
	    poly_edge_t &pev = p_pedges_vect[*p_it];
	    if (pev.prev == pe.vect_ind) {
		pev.prev = -1;
	    }
	    if (pev.next == pe.vect_ind) {
		pev.next = -1;
	    }
	    for (int j = 0; j < 2; j++) {
		std::set<size_t> &vedges = p_pnts_vect[pev.v[j]].pedges;
		vedges.erase(pe.vect_ind);
	    } 
	}
    }

    // IFF the edge is not degenerate remove it from the RTrees.  (If it is
    // degenerate it should never have been inserted.)
    // Note that singularity edges are not necessarily degenerate in 2D -
    // degenerate in this case refers specifically to an edge whose start and
    // end indices are the same.
    if (pe.v[0] != pe.v[1]) {
	poly_point_t &pp1 = p_pnts_vect[pe.v[0]];
	poly_point_t &pp2 = p_pnts_vect[pe.v[1]];
	ON_Line line(ON_2dPoint(pp1.u, pp1.v), ON_2dPoint(pp2.u, pp2.v));
	pe.bb = line.BoundingBox();
	pe.bb.m_max.x = pe.bb.m_max.x + 2*ON_ZERO_TOLERANCE;
	pe.bb.m_max.y = pe.bb.m_max.y + 2*ON_ZERO_TOLERANCE;
	pe.bb.m_min.x = pe.bb.m_min.x - 2*ON_ZERO_TOLERANCE;
	pe.bb.m_min.y = pe.bb.m_min.y - 2*ON_ZERO_TOLERANCE;
	double bp1[2];
	bp1[0] = pe.bb.Min().x;
	bp1[1] = pe.bb.Min().y;
	double bp2[2];
	bp2[0] = pe.bb.Max().x;
	bp2[1] = pe.bb.Max().y;
	p_edges_tree.Remove(bp1, bp2, pe.vect_ind);

	// The parent mesh keeps track of all boundary 2D polyedges present for higher
	// level processing as well.
	if (pe.type == B_BOUNDARY || pe.type == B_SINGULAR) {
	    m->loops_tree.Remove(bp1, bp2, (void *)(&p_pedges_vect[pe.vect_ind]));
	}

    }

    put_pedge(&pe);

    // It will be the 3D edge's job to update uedge information and (IFF both
    // associated 3D ordered edges are gone) clean out the uedge
    if (rme3d) {
	m->put_edge(rme3d);
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

