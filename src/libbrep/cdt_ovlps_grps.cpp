/*            C D T _ O V L P S _ G R P S . C P P
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
/** @file cdt_ovlps.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 * This file is logic specifically for refining meshes to clear overlaps
 * between sets of triangles in different meshes that need to share a common
 * tessellation.
 *
 */

#include "common.h"
#include <queue>
#include <numeric>
#include <random>
#include "bu/str.h"
#include "bg/chull.h"
#include "bg/tri_pt.h"
#include "bg/tri_tri.h"
#include "./cdt.h"
#include "./cdt_ovlps.h"

void
ovlp_grp::list_tris()
{
    std::set<size_t>::iterator t_it;
    std::cout << "      " << om1->fmesh->name << " " << om1->fmesh->f_id << ": " << tris1.size() << " tris\n";
    for (t_it = tris1.begin(); t_it != tris1.end(); t_it++) {
	std::cout << "      " << *t_it << "\n";
    }
    std::cout << "      " << om2->fmesh->name << " " << om2->fmesh->f_id << ": " << tris2.size() << " tris\n";
    for (t_it = tris2.begin(); t_it != tris2.end(); t_it++) {
	std::cout << "      " << *t_it << "\n";
    }
}

void
ovlp_grp::list_overts()
{
    std::set<overt_t *>::iterator o_it;
    std::cout << "      " << om1->fmesh->name << " " << om1->fmesh->f_id << ": " << overts1.size() << " verts\n";
    for (o_it = overts1.begin(); o_it != overts1.end(); o_it++) {
	std::cout << "      " << (*o_it)->p_id << "\n";
    }
    std::cout << "      " << om2->fmesh->name << " " << om2->fmesh->f_id << ": " << overts2.size() << " verts\n";
    for (o_it = overts2.begin(); o_it != overts2.end(); o_it++) {
	std::cout << "      " << (*o_it)->p_id << "\n";
    }
}

static bool
is_edge_vert(omesh_t *omesh2, ON_3dPoint &sp)
{
    cdt_mesh::uedge_t closest_edge = omesh2->closest_uedge(sp);
    if (omesh2->fmesh->brep_edges.find(closest_edge) != omesh2->fmesh->brep_edges.end()) {
	cdt_mesh::bedge_seg_t *bseg = omesh2->fmesh->ue2b_map[closest_edge];
	if (!bseg) {
	    std::cout << "edge point, but couldn't find bseg pointer??\n";
	    return false;
	} else {
	    return true;
	}
    }
    return false;
}

void
ovlp_grp::characterize_verts(int ind)
{
    omesh_t *other_m = (!ind) ? om2 : om1;
    std::set<overt_t *> &ov1 = (!ind) ? overts1 : overts2;

    std::map<overt_t *, overt_t *> &ovm = (!ind) ? om1_om2_verts : om2_om1_verts;
    std::map<overt_t *, overt_t *> &ovmo = (!ind) ? om2_om1_verts : om1_om2_verts;
    std::set<overt_t *> &ovum = (!ind) ? om1_unmappable_rverts : om2_unmappable_rverts;

    std::set<overt_t *>::iterator ov_it;
    for (ov_it = ov1.begin(); ov_it != ov1.end(); ov_it++) {
	overt_t *ov = *ov_it;
	overt_t *nv = other_m->vert_closest(NULL, ov);

	if (ovmo.find(nv) != ovmo.end()) {
	    // We already have a mapping on this mesh for nv - closer one wins,
	    // further is added to unmappable_verts for later consideration
	    overt_t *ov_orig = ovmo[nv];
	    double d_orig = ov_orig->vpnt().DistanceTo(nv->vpnt());
	    double d_new = ov->vpnt().DistanceTo(nv->vpnt());

	    if (d_orig < d_new) {
		ovum.insert(ov);
	    } else {
		ovum.insert(ov_orig);
		ovmo[nv] = ov;
	    }
	    std::cout << "already mapped\n";
	    continue;
	}

	// Find any points whose matching closest surface point isn't a vertex
	// in the other mesh per the vtree.  That point is a refinement point.
	ON_3dPoint target_point = ov->vpnt();
	double pdist = ov->bb.Diagonal().Length() * 10;
	ON_3dPoint s_p;
	ON_3dVector s_n;
	bool feval = closest_surf_pnt(s_p, s_n, *other_m->fmesh, &target_point, 2*pdist);
	if (!feval) {
	    std::cout << "Error - couldn't find closest point for unpaired vert\n";
	    continue;
	}
	ON_BoundingBox spbb(s_p, s_p);
	std::cout << "ov " << ov->p_id << " closest vert " << nv->p_id << ", dist " << s_p.DistanceTo(nv->vpnt()) << "\n";

	// If the opposite vertex is close to the current vertex per their bounding boxes and is close
	// to the closet surface point, go with it
	std::set<overt_t *> &ov2 = (!ind) ? overts2 : overts1;
	std::set<long> &v2 = (!ind) ? verts2 : verts1;
	if (!nv->bb.IsDisjoint(spbb) && (s_p.DistanceTo(nv->vpnt()) < 2*ON_ZERO_TOLERANCE)) {
	    ovm[ov] = nv;
	    ovmo[nv] = ov;
	    // Make sure both vert sets store all the required vertices
	    ov2.insert(nv);
	    v2.insert(nv->p_id);
	    continue;
	}

	std::cout << "Need new vert paring(" << s_p.DistanceTo(nv->vpnt()) << ": " << target_point.x << "," << target_point.y << "," << target_point.z << "\n";

	if (is_edge_vert(other_m, s_p)) {
	    std::set<overt_t *> &ovev = (!ind) ? om2_everts_from_om1 : om1_everts_from_om2;
	    ovev.insert(ov);
	} else {
	    // Unmapped non-edge point
	    std::cout << "Non edge point needs a new refinement point inserted??\n";
	    std::set<overt_t *> &ovrv = (!ind) ? om2_rverts_from_om1 : om1_rverts_from_om2;
	    ovrv.insert(ov);
	}

	// Make sure both vert sets store all the required vertices - the
	// polygons need to encompass all active verts mapped in by both
	// meshes, not just those from the overlapping triangles original to
	// that mesh.
	ov2.insert(nv);
	v2.insert(nv->p_id);
    }

}

bool
ovlp_grp::characterize_all_verts()
{
    characterize_verts(0);
#if 0
    characterize_verts(1);
    return (refine_edge_verts->size() > 0) ? true : false;
#endif
    return false;
}


/* A grouping of overlapping triangles is defined as the set of triangles from
 * two meshes that overlap and are surrounded in both meshes by either
 * non-involved triangles or face boundary edges.  Visually, these are "clusters"
 * of overlapping triangles on the mesh surfaces. */
std::vector<ovlp_grp>
find_ovlp_grps(
	std::map<std::pair<omesh_t *, size_t>, size_t> &bin_map,
	std::set<std::pair<omesh_t *, omesh_t *>> &check_pairs
	)
{
    std::vector<ovlp_grp> bins;

    bin_map.clear();

    std::set<std::pair<omesh_t *, omesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first;
	omesh_t *omesh2 = cp_it->second;
	if (!omesh1->intruding_tris.size() || !omesh2->intruding_tris.size()) {
	    continue;
	}
	//std::cout << "Need to check " << omesh1->fmesh->name << " " << omesh1->fmesh->f_id << " vs " << omesh2->fmesh->name << " " << omesh2->fmesh->f_id << "\n";
	std::set<size_t> om1_tris = omesh1->intruding_tris;
	while (om1_tris.size()) {
	    size_t t1 = *om1_tris.begin();
	    std::pair<omesh_t *, size_t> ckey(omesh1, t1);
	    om1_tris.erase(t1);
	    //std::cout << "Processing triangle " << t1 << "\n";
	    ON_BoundingBox tri_bb = omesh1->fmesh->tri_bbox(t1);
	    std::set<size_t> ntris = omesh2->tris_search(tri_bb);
	    std::set<size_t>::iterator nt_it;
	    for (nt_it = ntris.begin(); nt_it != ntris.end(); nt_it++) {
		int real_ovlp = tri_isect(false, omesh1, omesh1->fmesh->tris_vect[t1], omesh2, omesh2->fmesh->tris_vect[*nt_it], NULL);
		if (!real_ovlp) continue;
		//std::cout << "real overlap with " << *nt_it << "\n";
		std::pair<omesh_t *, size_t> nkey(omesh2, *nt_it);
		size_t key_id;
		if (bin_map.find(ckey) == bin_map.end() && bin_map.find(nkey) == bin_map.end()) {
		    // New group
		    ovlp_grp ngrp(ckey.first, nkey.first);
		    ngrp.add_tri(ckey.first, ckey.second);
		    ngrp.add_tri(nkey.first, nkey.second);
		    bins.push_back(ngrp);
		    key_id = bins.size() - 1;
		    bin_map[ckey] = key_id;
		    bin_map[nkey] = key_id;
		} else {
		    if (bin_map.find(ckey) != bin_map.end()) {
			key_id = bin_map[ckey];
			bins[key_id].add_tri(nkey.first, nkey.second);
			bin_map[nkey] = key_id;
		    } else {
			key_id = bin_map[nkey];
			bins[key_id].add_tri(ckey.first, ckey.second);
			bin_map[ckey] = key_id;
		    }
		}
	    }
	}
    }

    return bins;
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

