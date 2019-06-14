/*                C D T _ S I N G U L A R _ S U R F . C P P
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
/** @file cdt_singular_surf.cpp
 *
 * Special handling of faces with singular trims.
 *
 */

#include "common.h"
#include "./cdt.h"

#define p2tEdge std::pair<p2t::Point *, p2t::Point *>

#define p2dIsSingular()

struct ON_Mesh_Singular_Face {
    struct ON_Brep_CDT_Face_State *f;
    std::map<p2t::Point *, std::set<p2t::Triangle *>> p2t;
    std::map<p2t::Point *, std::set<p2tEdge>> p2e_ordered;
    std::map<p2t::Point *, std::set<p2tEdge>> p2e_unordered;
    std::map<p2tEdge, std::set<p2t::Triangle *>> e2t_ordered;
    std::map<p2tEdge, std::set<p2t::Triangle *>> e2t_unordered;
    std::map<p2t::Triangle *, std::set<p2tEdge>> t2e_ordered;
    std::map<p2t::Triangle *, std::set<p2tEdge>> t2e_unordered;
    std::map<p2tEdge, int> euse;
    std::map<ON_3dPoint *,std::set<p2t::Point *>> p2t_singular_pnts;
};

static p2tEdge
mk_unordered_edge(p2t::Point *pt_A, p2t::Point *pt_B)
{
    if (pt_A <= pt_B) {
	return std::make_pair(pt_A, pt_B);
    } else {
	return std::make_pair(pt_B, pt_A);
    }
}

#define IsEdgePt(_a) (f->s_cdt->edge_pnts->find(_a) != f->s_cdt->edge_pnts->end())

static void
add_p2t_tri_edges(struct ON_Mesh_Singular_Face *sf, p2t::Triangle *t)
{
    p2t::Point *p2_A = t->GetPoint(0);
    p2t::Point *p2_B = t->GetPoint(1);
    p2t::Point *p2_C = t->GetPoint(2);

    sf->e2t_ordered[(std::make_pair(p2_A, p2_B))].insert(t);
    sf->e2t_ordered[(std::make_pair(p2_B, p2_C))].insert(t);
    sf->e2t_ordered[(std::make_pair(p2_C, p2_A))].insert(t);
    sf->t2e_ordered[t].insert(std::make_pair(p2_A, p2_B));
    sf->t2e_ordered[t].insert(std::make_pair(p2_B, p2_C));
    sf->t2e_ordered[t].insert(std::make_pair(p2_C, p2_A));

    sf->e2t_unordered[(mk_unordered_edge(p2_A, p2_B))].insert(t);
    sf->e2t_unordered[(mk_unordered_edge(p2_B, p2_C))].insert(t);
    sf->e2t_unordered[(mk_unordered_edge(p2_C, p2_A))].insert(t);
    sf->t2e_unordered[t].insert(mk_unordered_edge(p2_A, p2_B));
    sf->t2e_unordered[t].insert(mk_unordered_edge(p2_B, p2_C));
    sf->t2e_unordered[t].insert(mk_unordered_edge(p2_C, p2_A));


    sf->p2e_ordered[p2_A].insert(std::make_pair(p2_A, p2_B));
    sf->p2e_ordered[p2_A].insert(std::make_pair(p2_C, p2_A));
    sf->p2e_ordered[p2_B].insert(std::make_pair(p2_A, p2_B));
    sf->p2e_ordered[p2_B].insert(std::make_pair(p2_B, p2_C));
    sf->p2e_ordered[p2_C].insert(std::make_pair(p2_B, p2_C));
    sf->p2e_ordered[p2_C].insert(std::make_pair(p2_A, p2_C));

    sf->p2e_unordered[p2_A].insert(mk_unordered_edge(p2_A, p2_B));
    sf->p2e_unordered[p2_A].insert(mk_unordered_edge(p2_C, p2_A));
    sf->p2e_unordered[p2_B].insert(mk_unordered_edge(p2_A, p2_B));
    sf->p2e_unordered[p2_B].insert(mk_unordered_edge(p2_B, p2_C));
    sf->p2e_unordered[p2_C].insert(mk_unordered_edge(p2_B, p2_C));
    sf->p2e_unordered[p2_C].insert(mk_unordered_edge(p2_A, p2_C));

    // We need to count unordered edge uses to identify boundary edges.
    sf->euse[(mk_unordered_edge(p2_A, p2_B))]++;
    sf->euse[(mk_unordered_edge(p2_B, p2_C))]++;
    sf->euse[(mk_unordered_edge(p2_C, p2_A))]++;
}

void
onsf_process(struct ON_Brep_CDT_Face_State *f)
{
    struct ON_Mesh_Singular_Face *sf = new struct ON_Mesh_Singular_Face;
    sf->f = f;

    // For all non-degenerate triangles in the face, we need to build up maps
    // so we can know where we are in relationship to other points, edges and
    // triangles.
    p2t::CDT *cdt = f->cdt;
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	if (f->tris_degen->find(t) != f->tris_degen->end()) {
	    continue;
	}

	// point map
	for (size_t j = 0; j < 3; j++) {
	    sf->p2t[t->GetPoint(j)].insert(t);
	}
	// edge map
	add_p2t_tri_edges(sf, t);
	// Identify any singular points for later reference
	for (size_t j = 0; j < 3; j++) {
	    p2t::Point *p2t = t->GetPoint(j);
	    ON_3dPoint *p3d = (*pointmap)[p2t];
	    if (f->s_cdt->singular_vert_to_norms->find(p3d) != f->s_cdt->singular_vert_to_norms->end()) {
		sf->p2t_singular_pnts[p3d].insert(p2t);
	    }
	}
    }

    // Find all the triangles using a singular point as a vertex - that's our
    // starting triangle set.
    //
    // TODO - initially, the code is going to pretty much assume one singularity per face,
    // but that's not a good assumption in general - ultimately, we should build up sets
    // of faces for each singularity for separate processing.
    if (sf->p2t_singular_pnts.size() != 1) {
	bu_log("odd singularity point count, aborting\n");
    }
    std::map<ON_3dPoint *,std::set<p2t::Point *>>::iterator ap_it;

    // In a multi-singularity version of this code this set will have to be specific to each point
    std::set<p2t::Triangle *> singularity_triangles;

    for (ap_it = sf->p2t_singular_pnts.begin(); ap_it != sf->p2t_singular_pnts.end(); ap_it++) {
	std::set<p2t::Point *>::iterator p_it;
	std::set<p2t::Point *> &p2tspnts = ap_it->second;
	for (p_it = p2tspnts.begin(); p_it != p2tspnts.end(); p_it++) {
	    std::set<p2t::Triangle *> &p2t = sf->p2t[*p_it];
	    std::set<p2t::Triangle *>::iterator p2t_it;
	    for (p2t_it = p2t.begin(); p2t_it != p2t.end(); p2t_it++) {
		singularity_triangles.insert(*p2t_it);
	    }
	}
    }

    // Find the best fit plane for the vertices of the triangles involved with
    // the singular point

    // Make sure all of the triangles can be projected to the plane
    // successfully, without flipping triangles.  If not, the mess will have be
    // handled in coordinated subsections (doable but hard!) and for now we
    // will punt in that situation.

    // Walk out along the mesh, collecting all the triangles from the face
    // that can be projected onto the plane without undo loss of area or normal
    // issues.  Once the terminating criteria are reached, we have the set
    // of triangles that will provide our "replace and remesh" subset of the
    // original face.

    // Walk around the outer triangles (edge use count internal to the subset will
    // be critical here) and construct the bounding polygon of the subset for
    // poly2Tri.

    // Project all points in the subset into the plane, getting XY coordinates
    // for poly2Tri.  Use the previously constructed loop and assemble the
    // polyline and internal point set for the new triangulation, keeping a
    // map from the new XY points to the original p2t points.

    // Perform the new triangulation, assemble the new p2t Triangle set using
    // the mappings, and check the new 3D mesh thus created for issues.  If it
    // is clean, replace the original subset of the parent face with the new
    // triangle set, else fail.  Note that a consequence of this will be that
    // only one of the original singularity trim points will end up being used.

    // Done, cleanup
    delete sf;
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

