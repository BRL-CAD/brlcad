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

static void
isect_process_edge_vert2(
	overt_t *v, omesh_t *omesh2, ON_3dPoint &sp,
	std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *edge_verts)
{
    cdt_mesh::uedge_t closest_edge = omesh2->closest_uedge(sp);
    if (omesh2->fmesh->brep_edges.find(closest_edge) != omesh2->fmesh->brep_edges.end()) {
	cdt_mesh::bedge_seg_t *bseg = omesh2->fmesh->ue2b_map[closest_edge];
	if (!bseg) {
	    std::cout << "couldn't find bseg pointer??\n";
	} else {
	    (*edge_verts)[bseg].insert(v);
	}
    }
}

bool
ovlp_grp::ovlp_vert_validate(int ind, std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *edge_verts)
{
    omesh_t *other_m = (!ind) ? om2 : om1;
    std::set<overt_t *> &ov1 = (!ind) ? overts1 : overts2;
    std::set<overt_t *> &ov2 = (!ind) ? overts2 : overts1;
    std::set<long> &v2 = (!ind) ? verts2 : verts1;

    bool have_refine_pnts = false;
    std::set<overt_t *>::iterator ov_it;
    for (ov_it = ov1.begin(); ov_it != ov1.end(); ov_it++) {
	// Find any points whose matching closest surface point isn't a vertex
	// in the other mesh per the vtree.  That point is a refinement point.
	overt_t *ov = *ov_it;
	overt_t *nv = other_m->vert_closest(NULL, ov);
	ON_3dPoint target_point = ov->vpnt();
	double pdist = ov->bb.Diagonal().Length() * 10;
	ON_3dPoint s_p;
	ON_3dVector s_n;
	bool feval = closest_surf_pnt(s_p, s_n, *other_m->fmesh, &target_point, 2*pdist);
	if (!feval) {
	    std::cout << "Error - couldn't find closest point for unpaired vert\n";
	}
	ON_BoundingBox spbb(s_p, s_p);
	std::cout << "ov " << ov->p_id << " closest vert " << nv->p_id << ", dist " << s_p.DistanceTo(nv->vpnt()) << "\n";
	if (nv->bb.IsDisjoint(spbb) || (s_p.DistanceTo(nv->vpnt()) > ON_ZERO_TOLERANCE)) {
	    std::cout << "Need new vert paring(" << s_p.DistanceTo(nv->vpnt()) << ": " << target_point.x << "," << target_point.y << "," << target_point.z << "\n";
	    isect_process_edge_vert2(ov, other_m, s_p, edge_verts);
	    // Ensure the count for this vert is above the processing threshold
	    other_m->refinement_overts[ov].insert(-1);
	    other_m->refinement_overts[ov].insert(-2);
	    have_refine_pnts = true;
	}
	// Make sure both vert sets store all the required vertices
	ov2.insert(nv);
	v2.insert(nv->p_id);
    }

    return have_refine_pnts;
}

bool
ovlp_grp::refinement_pnts(std::map<cdt_mesh::bedge_seg_t *, std::set<overt_t *>> *edge_verts)
{
    size_t v1_prev_size = verts1.size();
    size_t v2_prev_size = verts2.size();
    bool r1 = ovlp_vert_validate(0, edge_verts);
    bool r2 = ovlp_vert_validate(1, edge_verts);
    while ((v1_prev_size != verts1.size()) || (v2_prev_size != verts2.size())) {
	v1_prev_size = verts1.size();
	v2_prev_size = verts2.size();
	r1 = ovlp_vert_validate(0, edge_verts);
	r2 = ovlp_vert_validate(1, edge_verts);
    }
    return (r1 || r2);
}

bool
ovlp_grp::validate()
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

