/*                   C D T _ O V L P S . C P P
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
/** @file cdt_ovlps.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 * overlap vertex implementation.
 *
 */

#include "common.h"
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include "bu/str.h"
#include "./mesh.h"
#include "./cdt.h"

ON_3dPoint
overt_t::vpnt() {
    ON_3dPoint vp = *(omesh->fmesh->pnts[p_id]);
    return vp;
}

bool
overt_t::edge_vert() {
    return (omesh->fmesh->ep.find(p_id) != omesh->fmesh->ep.end());
}

void
overt_t::update() {

    // Get pnt's associated edges.
    std::set<edge_t> edges = omesh->fmesh->v2edges[p_id];

    // If we don't have any edge information, this method of
    // updating the vertex isn't viable - leave it as is.
    if (!edges.size()) {
	return;
    }

    double fMin[3], fMax[3];
    if (init) {
	// Previously updated - remove old instance from tree
	fMin[0] = bb.Min().x-ON_ZERO_TOLERANCE;
	fMin[1] = bb.Min().y-ON_ZERO_TOLERANCE;
	fMin[2] = bb.Min().z-ON_ZERO_TOLERANCE;
	fMax[0] = bb.Max().x+ON_ZERO_TOLERANCE;
	fMax[1] = bb.Max().y+ON_ZERO_TOLERANCE;
	fMax[2] = bb.Max().z+ON_ZERO_TOLERANCE;
	omesh->vtree.Remove(fMin, fMax, p_id);
    }

    // find the shortest edge associated with pnt
    std::set<edge_t>::iterator e_it;
    double elen = DBL_MAX;
    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
	ON_3dPoint *p1 = omesh->fmesh->pnts[(*e_it).v[0]];
	ON_3dPoint *p2 = omesh->fmesh->pnts[(*e_it).v[1]];
	double dist = p1->DistanceTo(*p2);
	elen = (dist < elen) ? dist : elen;
    }
    v_min_edge_len = elen;

    // We also need to check if the vertex is close to
    // the opposite edge of one of its triangles - that
    // will constrain how we can move it without flipping
    // a triangle.
    std::set<size_t> vtri_inds = omesh->fmesh->v2tris[p_id];
    std::set<size_t>::iterator tv_it;
    for (tv_it = vtri_inds.begin(); tv_it != vtri_inds.end(); tv_it++) {
	triangle_t tri = omesh->fmesh->tris_vect[*tv_it];
	double odist = tri.opp_edge_dist(p_id);
	elen = (odist < elen) ? odist : elen;
    }

    // create a bbox around pnt using length ~20% of the shortest edge length.
    ON_3dPoint v3dpnt = *omesh->fmesh->pnts[p_id];

    ON_BoundingBox init_bb(v3dpnt, v3dpnt);
    bb = init_bb;
    ON_3dPoint npnt = v3dpnt;
    double lfactor = 0.2;
    npnt.x = npnt.x + lfactor*elen;
    bb.Set(npnt, true);
    npnt = v3dpnt;
    npnt.x = npnt.x - lfactor*elen;
    bb.Set(npnt, true);
    npnt = v3dpnt;
    npnt.y = npnt.y + lfactor*elen;
    bb.Set(npnt, true);
    npnt = v3dpnt;
    npnt.y = npnt.y - lfactor*elen;
    bb.Set(npnt, true);
    npnt = v3dpnt;
    npnt.z = npnt.z + lfactor*elen;
    bb.Set(npnt, true);
    npnt = v3dpnt;
    npnt.z = npnt.z - lfactor*elen;
    bb.Set(npnt, true);

    // (re)insert the vertex into the parent omesh's vtree, once we're done
    // updating
    fMin[0] = bb.Min().x;
    fMin[1] = bb.Min().y;
    fMin[2] = bb.Min().z;
    fMax[0] = bb.Max().x;
    fMax[1] = bb.Max().y;
    fMax[2] = bb.Max().z;
    if (fMin[0] < -0.4*DBL_MAX || fMax[0] > 0.4*DBL_MAX) {
	std::cout << "Bad vert box!\n";
    }
    omesh->vtree.Insert(fMin, fMax, p_id);

    // Any closet point calculations and alignments are now invalid - update accordingly
    std::map<omesh_t *, overt_t *>::iterator a_it;
    for (a_it = aligned.begin(); a_it != aligned.end(); a_it++) {
	overt_t *other_vert = a_it->second;
	other_vert->aligned.erase(omesh);
	other_vert->cpoints.erase(this);
	other_vert->cnorms.erase(this);
    }
    aligned.clear();
    cpoints.clear();
    cnorms.clear();

    // Once we've gotten here, the vertex is fully initialized
    init = true;
}

void
overt_t::update(ON_BoundingBox *bbox) {
    double fMin[3], fMax[3];

    if (!bbox) {
	std::cerr << "Cannot update vertex with NULL bounding box!\n";
	return;
    }

    if (init) {
	// Previously updated - remove old instance from tree
	fMin[0] = bb.Min().x-ON_ZERO_TOLERANCE;
	fMin[1] = bb.Min().y-ON_ZERO_TOLERANCE;
	fMin[2] = bb.Min().z-ON_ZERO_TOLERANCE;
	fMax[0] = bb.Max().x+ON_ZERO_TOLERANCE;
	fMax[1] = bb.Max().y+ON_ZERO_TOLERANCE;
	fMax[2] = bb.Max().z+ON_ZERO_TOLERANCE;
	omesh->vtree.Remove(fMin, fMax, p_id);
    }

    // Use the new bbox to set bb
    bb = *bbox;

    // Get pnt's associated edges.
    std::set<edge_t> edges = omesh->fmesh->v2edges[p_id];

    // find the shortest edge associated with pnt
    std::set<edge_t>::iterator e_it;
    double elen = DBL_MAX;
    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
	ON_3dPoint *p1 = omesh->fmesh->pnts[(*e_it).v[0]];
	ON_3dPoint *p2 = omesh->fmesh->pnts[(*e_it).v[1]];
	double dist = p1->DistanceTo(*p2);
	elen = (dist < elen) ? dist : elen;
    }
    v_min_edge_len = elen;

    // (re)insert the vertex into the parent omesh's vtree, once we're done
    // updating
    fMin[0] = bb.Min().x;
    fMin[1] = bb.Min().y;
    fMin[2] = bb.Min().z;
    fMax[0] = bb.Max().x;
    fMax[1] = bb.Max().y;
    fMax[2] = bb.Max().z;
    if (fMin[0] < -0.4*DBL_MAX || fMax[0] > 0.4*DBL_MAX) {
	std::cout << "Bad vert box!\n";
    }
    omesh->vtree.Insert(fMin, fMax, p_id);

    // Once we've gotten here, the vertex is fully initialized
    init = true;
}

void
overt_t::update_ring()
{
    std::set<long> mod_verts;

    // 1.  Get pnt's associated edges.
    std::set<edge_t> edges = omesh->fmesh->v2edges[p_id];

    // 2.  Collect all the vertices associated with all the edges connected to
    // the original point - these are the vertices that will have a new associated
    // edge length after the change, and need to check if they have a new minimum
    // associated edge length (with its implications for bounding box size).
    std::set<edge_t>::iterator e_it;
    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
	mod_verts.insert((*e_it).v[0]);
	mod_verts.insert((*e_it).v[1]);
    }

    // 3.  Update each vertex
    std::set<long>::iterator v_it;
    for (v_it = mod_verts.begin(); v_it != mod_verts.end(); v_it++) {
	omesh->overts[*v_it]->update();
    }
}

int
overt_t::adjust(ON_3dPoint &target_point, double dtol)
{
    ON_3dPoint s_p;
    ON_3dVector s_n;
    bool feval = omesh->fmesh->closest_surf_pnt(s_p, s_n, &target_point, dtol);
    if (feval) {

	// See if s_p is an actual move - if not, this is a no-op
	ON_3dPoint p = vpnt();
	if (p.DistanceTo(s_p) < ON_ZERO_TOLERANCE) {
	    return 0;
	}

	(*omesh->fmesh->pnts[p_id]) = s_p;
	(*omesh->fmesh->normals[omesh->fmesh->nmap[p_id]]) = s_n;
	update();
	// We just changed the vertex point values - need to update all
	// this vertex and all vertices connected to the updated vertex
	// by an edge.
	update_ring();

	return 1;
    }

    return 0;
}


void
overt_t::plot(FILE *plot)
{
    ON_3dPoint *i_p = omesh->fmesh->pnts[p_id];
    ON_BoundingBox_Plot(plot, bb);
    double r = 0.05*bb.Diagonal().Length();
    pl_color(plot, 0, 255, 0);
    plot_pnt_3d(plot, i_p, r, 0);
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

