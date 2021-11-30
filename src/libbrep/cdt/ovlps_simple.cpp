/*                   C D T _ O V L P S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2021 United States Government as represented by
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
 * This file is logic specifically for refining meshes to clear
 * overlaps between sets of objects.
 *
 */

#include "common.h"
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include "bu/time.h"
#include "bg/chull.h"
#include "bg/tri_pt.h"
#include "bg/tri_tri.h"
#include "./cdt.h"


// Functions to build sets of active meshes from the active pairs set

std::set<cdt_mesh_t *>
active_meshes(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs)
{
    std::set<cdt_mesh_t *> a_mesh;
    std::set<cdt_mesh_t *>::iterator a_it;
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cdt_mesh_t *cdt_mesh1 = cp_it->first;
	cdt_mesh_t *cdt_mesh2 = cp_it->second;
	a_mesh.insert(cdt_mesh1);
	a_mesh.insert(cdt_mesh2);
    }

    return a_mesh;
}

/******************************************************************************
 * As a first step, use the face bboxes to narrow down where we have potential
 * interactions between breps
 ******************************************************************************/

struct nf_info {
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> *check_pairs;
    cdt_mesh_t *cmesh;
};

static bool NearFacesPairsCallback(void *data, void *a_context) {
    struct nf_info *nf = (struct nf_info *)a_context;
    cdt_mesh_t *fmesh = (cdt_mesh_t *)data;
    std::pair<cdt_mesh_t *, cdt_mesh_t *> p1(nf->cmesh, fmesh);
    std::pair<cdt_mesh_t *, cdt_mesh_t *> p2(fmesh, nf->cmesh);
    if ((nf->check_pairs->find(p1) == nf->check_pairs->end()) && (nf->check_pairs->find(p1) == nf->check_pairs->end())) {
	nf->check_pairs->insert(p1);
    }
    return true;
}

std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>
possibly_interfering_face_pairs(struct ON_Brep_CDT_State **s_a, int s_cnt)
{
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> check_pairs;
    RTree<void *, double, 3> rtree_fmeshes;
    for (int i = 0; i < s_cnt; i++) {
	struct ON_Brep_CDT_State *s_i = s_a[i];
	for (int i_fi = 0; i_fi < s_i->brep->m_F.Count(); i_fi++) {
	    const ON_BrepFace *i_face = s_i->brep->Face(i_fi);
	    ON_BoundingBox bb = i_face->BoundingBox();
	    cdt_mesh_t *fmesh = &s_i->fmeshes[i_fi];
	    struct nf_info nf;
	    nf.cmesh = fmesh;
	    nf.check_pairs = &check_pairs;
	    double fMin[3];
	    fMin[0] = bb.Min().x;
	    fMin[1] = bb.Min().y;
	    fMin[2] = bb.Min().z;
	    double fMax[3];
	    fMax[0] = bb.Max().x;
	    fMax[1] = bb.Max().y;
	    fMax[2] = bb.Max().z;
	    // Check the new box against the existing tree, and add any new
	    // interaction pairs to check_pairs
	    rtree_fmeshes.Search(fMin, fMax, NearFacesPairsCallback, (void *)&nf);
	    // Add the new box to the tree so any additional boxes can check
	    // against it as well
	    rtree_fmeshes.Insert(fMin, fMax, (void *)fmesh);
	}
    }

    return check_pairs;
}

void
triangle_uedges(std::set<uedge_t> &uedges, std::set<uedge_t> &uskip, triangle_t &t, double lthresh)
{
    double lel = t.longest_edge_len();
    if (lel < lthresh) {
	return;
    }
    for (int i = 0; i < 3; i++) {
	if (t.uedge_len(i) > lthresh) {
	    uedge_t ue = t.uedge(i);
	    uedges.insert(ue);
	}
    }
    // Remove the shortest edge from splitting (if present) even if it is
    // longer than the threshold length.  If we do need to split this edge,
    // we'll do so after getting the longer edges.
    uedge_t ue = t.shortest_edge();
    uskip.insert(ue);
}

int
mesh_ovlps(
	std::map<cdt_mesh_t *, std::set<uedge_t>> *iedges,
	std::set<bedge_seg_t *> *bsegs,
	std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> check_pairs, int mode, double lthresh)
{
    std::map<cdt_mesh_t *, std::set<uedge_t>>::iterator ie_it;
    std::map<cdt_mesh_t *, std::set<uedge_t>> iedges_init;
    std::map<cdt_mesh_t *, std::set<uedge_t>> iedges_skip;
    iedges->clear();
    bsegs->clear();

    // Assemble the set of mesh faces that (per the check_pairs sets) we
    // may need to process.
    std::set<cdt_mesh_t *> a_mesh = active_meshes(check_pairs);
    std::set<cdt_mesh_t *>::iterator a_it;

    // Intersect first the triangle RTrees, then any potentially
    // overlapping triangles found within the leaves.
    size_t tri_isects = 0;
    std::map<cdt_mesh_t *, std::set<long>> itris;
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cdt_mesh_t *fmesh1 = cp_it->first;
	cdt_mesh_t *fmesh2 = cp_it->second;
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)fmesh1->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)fmesh2->p_cdt;
	//fmesh1->tris_rtree_plot("tree1.plot3");
	//fmesh2->tris_rtree_plot("tree2.plot3");
	if (s_cdt1 != s_cdt2) {
	    std::set<std::pair<size_t, size_t>> tris_prelim;
	    size_t ovlp_cnt = fmesh1->tris_tree.Overlaps(fmesh2->tris_tree, &tris_prelim);
	    if (ovlp_cnt) {
		std::set<std::pair<size_t, size_t>>::iterator tb_it;
#if 0
		std::set<size_t> tris_p1, tris_p2;
		for (tb_it = tris_prelim.begin(); tb_it != tris_prelim.end(); tb_it++) {
		    tris_p1.insert(tb_it->first);
		    tris_p2.insert(tb_it->second);
		}
		fmesh1->tris_set_plot(tris_p1,"tp1.plot3");
		fmesh2->tris_set_plot(tris_p2,"tp2.plot3");
		std::set<size_t> tris_i1, tris_i2;
#endif
		for (tb_it = tris_prelim.begin(); tb_it != tris_prelim.end(); tb_it++) {
		    triangle_t t1 = fmesh1->tris_vect[tb_it->first];
		    triangle_t t2 = fmesh2->tris_vect[tb_it->second];
		    int isect = tri_isect(t1, t2, mode);
		    if (isect) {
			itris[t1.m].insert(t1.ind);
			itris[t2.m].insert(t2.ind);
			//tris_i1.insert(t1.ind);
			//tris_i2.insert(t2.ind);
			tri_isects++;
		    }
		}
		//fmesh1->tris_set_plot(tris_i1,"ti1.plot3");
		//fmesh2->tris_set_plot(tris_i2,"ti2.plot3");
	    }
	}
    }

    // Build up the initial edge sets
    std::map<cdt_mesh_t *, std::set<long>>::iterator i_it;
    for (i_it = itris.begin(); i_it != itris.end(); i_it++) {
	std::set<long>::iterator l_it;
	for (l_it = i_it->second.begin(); l_it != i_it->second.end(); l_it++) {
	    triangle_t t = i_it->first->tris_vect[*l_it];
	    triangle_uedges(iedges_init[t.m], iedges_skip[t.m], t, lthresh);
	}
    }

#if 0
    for (ie_it = iedges_init.begin(); ie_it != iedges_init.end(); ie_it++) {
	cdt_mesh_t *fmesh = ie_it->first;
	std::string pname = std::string(fmesh->name) + std::string("_long_") + std::to_string(fmesh->f_id) + std::string(".plot3");
	fmesh->edge_set_plot(ie_it->second, pname.c_str(), 160, 160, 160);
    }
    for (ie_it = iedges_skip.begin(); ie_it != iedges_skip.end(); ie_it++) {
	cdt_mesh_t *fmesh = ie_it->first;
	std::string pname = std::string(fmesh->name) + std::string("_short_") + std::to_string(fmesh->f_id) + std::string(".plot3");
	fmesh->edge_set_plot(ie_it->second, pname.c_str(), 255, 0, 0);
    }
#endif


    // Build up the final edge sets
    int ecnt = 0;
    for (ie_it = iedges_init.begin(); ie_it != iedges_init.end(); ie_it++) {
	cdt_mesh_t *fmesh = ie_it->first;
	std::set<uedge_t>::iterator u_it;
	for (u_it = ie_it->second.begin(); u_it != ie_it->second.end(); u_it++) {
	    if (iedges_skip[fmesh].find(*u_it) == iedges_skip[fmesh].end()) {
		if (fmesh->brep_edges.find(*u_it) == fmesh->brep_edges.end()) {
		    (*iedges)[fmesh].insert(*u_it);
		} else {
		    bedge_seg_t *bseg = fmesh->ue2b_map[*u_it];
		    bsegs->insert(bseg);
		}
		ecnt++;
	    }
	}
    }

    //iedges_init.begin()->first->edge_set_plot(*bsegs, "bsegs.plot3");

    return ecnt;
}

static void
split_tri(cdt_mesh_t &fmesh, triangle_t &t, long np_id, uedge_t &split_edge)
{
    std::set<triangle_t> ntris = t.split(split_edge, np_id, false);

    fmesh.tri_remove(t);
    std::set<triangle_t>::iterator t_it;
    for (t_it = ntris.begin(); t_it != ntris.end(); t_it++) {
	triangle_t nt = *t_it;
	fmesh.tri_add(nt);
    }
}

int
internal_split_edge(
	cdt_mesh_t *fmesh,
	uedge_t &ue
	)
{
    // Find the two triangles that we will be using to form the outer polygon
    std::set<size_t> rtris = fmesh->uedges2tris[ue];
    if (rtris.size() != 2) {
        std::cout << "Error - could not associate uedge with two triangles??\n";
        return -1;
    }

    std::set<size_t> crtris = rtris;
    triangle_t tri1 = fmesh->tris_vect[*crtris.begin()];
    crtris.erase(crtris.begin());
    triangle_t tri2 = fmesh->tris_vect[*crtris.begin()];
    crtris.erase(crtris.begin());

    // Add interior point.
    ON_3dPoint epnt = (*fmesh->pnts[ue.v[0]] + *fmesh->pnts[ue.v[1]]) * 0.5;
    double spdist = fmesh->pnts[ue.v[0]]->DistanceTo(*fmesh->pnts[ue.v[1]]);
    ON_3dPoint spnt;
    ON_3dVector sn;
    if (!fmesh->closest_surf_pnt(spnt, sn, &epnt, spdist*10)) {
        std::cerr << "Error - couldn't find closest point\n";
        return -1;
    }

    // We're going to use it - add new 3D point
    long f3ind = fmesh->add_point(new ON_3dPoint(spnt));
    long fnind = fmesh->add_normal(new ON_3dPoint(sn));
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh->p_cdt;
    CDT_Add3DPnt(s_cdt, fmesh->pnts[f3ind], fmesh->f_id, -1, -1, -1, 0, 0);
    CDT_Add3DNorm(s_cdt, fmesh->normals[fnind], fmesh->pnts[f3ind], fmesh->f_id, -1, -1, -1, 0, 0);
    fmesh->nmap[f3ind] = fnind;

    // Split the old triangles, perform the removals and add the new triangles
    split_tri(*fmesh, tri1, f3ind, ue);
    split_tri(*fmesh, tri2, f3ind, ue);
   
    return 2;
}

int
brep_split_edge(
	bedge_seg_t *eseg, double t
	)
{
    std::vector<std::pair<cdt_mesh_t *,uedge_t>> uedges = eseg->uedges();

    if (uedges.size() != 2) {
	std::cout << "Unexpected edge count for bedge segment\n";
	return -1;
    }

    cdt_mesh_t *fmesh_f1 = uedges[0].first;
    cdt_mesh_t *fmesh_f2 = uedges[1].first;

    int f_id1 = fmesh_f1->f_id;
    int f_id2 = fmesh_f2->f_id;

    uedge_t ue1 = uedges[0].second;
    uedge_t ue2 = uedges[1].second;

    std::set<size_t> f1_tris = fmesh_f1->uedges2tris[ue1];
    std::set<size_t> f2_tris = fmesh_f2->uedges2tris[ue2];
    triangle_t tri1, tri2;
    if (f_id1 == f_id2) {
	if ((f1_tris.size() != 2 && f1_tris.size() != 0) || (f2_tris.size() != 2 && f2_tris.size() != 0)) {
	    std::cout << "Unexpected triangle count for edge\n";
	    std::cout << "f1_tris: " << f1_tris.size() << "\n";
	    std::cout << "f2_tris: " << f2_tris.size() << "\n";
	    return -1;
	}
	if ((!f1_tris.size() && !f2_tris.size())) {
	    std::cout << "Unexpected triangle count for edge\n";
	    std::cout << "f1_tris: " << f1_tris.size() << "\n";
	    std::cout << "f2_tris: " << f2_tris.size() << "\n";
	    return -1;
	}
	if (f1_tris.size()) {
	    tri1 = fmesh_f1->tris_vect[*f1_tris.begin()];
	    f1_tris.erase(f1_tris.begin());
	    tri2 = fmesh_f1->tris_vect[*f1_tris.begin()];
	    f1_tris.erase(f1_tris.begin());
	} else {
	    tri1 = fmesh_f2->tris_vect[*f2_tris.begin()];
	    f1_tris.erase(f1_tris.begin());
	    tri2 = fmesh_f2->tris_vect[*f2_tris.begin()];
	    f1_tris.erase(f1_tris.begin());
	}
    } else {
	if (f1_tris.size() != 1 || f2_tris.size() != 1) {
	    std::cout << "Unexpected triangle count for edge\n";
	    std::cout << "f1_tris: " << f1_tris.size() << "\n";
	    std::cout << "f2_tris: " << f2_tris.size() << "\n";
	    return -1;
	}
	tri1 = fmesh_f1->tris_vect[*f1_tris.begin()];
	tri2 = fmesh_f2->tris_vect[*f2_tris.begin()];
    }

    // We're replacing the edge and its triangles - clear old data
    // from the containers
    fmesh_f1->brep_edges.erase(ue1);
    fmesh_f2->brep_edges.erase(ue2);
    fmesh_f1->ue2b_map.erase(ue1);
    fmesh_f2->ue2b_map.erase(ue2);

    int eind = eseg->edge_ind;
    struct ON_Brep_CDT_State *s_cdt_edge = (struct ON_Brep_CDT_State *)eseg->p_cdt;
    std::set<bedge_seg_t *> epsegs = s_cdt_edge->e2polysegs[eind];
    epsegs.erase(eseg);
    std::set<bedge_seg_t *> esegs_split = split_edge_seg(s_cdt_edge, eseg, 1, &t, 1);
    if (!esegs_split.size()) {
	std::cerr << "SPLIT FAILED!\n";
	return -1;
    }

    epsegs.insert(esegs_split.begin(), esegs_split.end());
    s_cdt_edge->e2polysegs[eind].clear();
    s_cdt_edge->e2polysegs[eind] = epsegs;
    std::set<bedge_seg_t *>::iterator es_it;
    for (es_it = esegs_split.begin(); es_it != esegs_split.end(); es_it++) {
	bedge_seg_t *es = *es_it;

	std::vector<std::pair<cdt_mesh_t *,uedge_t>> nuedges = es->uedges();
	cdt_mesh_t *f1 = nuedges[0].first;
	cdt_mesh_t *f2 = nuedges[1].first;
	uedge_t ue_1 = nuedges[0].second;
	uedge_t ue_2 = nuedges[1].second;

	f1->brep_edges.insert(ue_1);
	f2->brep_edges.insert(ue_2);
	f1->ue2b_map[ue_1] = es;
	f2->ue2b_map[ue_2] = es;
    }

    long np_id_1 = tri1.m->pnts.size() - 1;
    tri1.m->ep.insert(np_id_1);
    split_tri(*tri1.m, tri1, np_id_1, ue1);

    long np_id_2 = tri2.m->pnts.size() - 1;
    tri2.m->ep.insert(np_id_2);
    split_tri(*tri2.m, tri2, np_id_2, ue2);

    return 2;
}

// TODO - need to add a check for triangles with all three vertices on the
// same brep face edge - c.s face 4 appears to have some triangles appearing
// of that sort, which are messing with the ovlp resolution...
bool
check_faces_validity(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs)
{
    int verbosity = 0;
    std::set<cdt_mesh_t *> fmeshes;
    std::set<struct ON_Brep_CDT_State *> cdt_states;
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cdt_mesh_t *fmesh1 = cp_it->first;
	cdt_mesh_t *fmesh2 = cp_it->second;
	fmeshes.insert(fmesh1);
	fmeshes.insert(fmesh2);
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)fmesh1->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)fmesh2->p_cdt;
	cdt_states.insert(s_cdt1);
	cdt_states.insert(s_cdt2);
    }
    if (verbosity > 0) {
	std::cout << "Full face validity check results:\n";
    }
    bool valid = true;

    std::set<struct ON_Brep_CDT_State *>::iterator s_it;
    for (s_it = cdt_states.begin(); s_it != cdt_states.end(); s_it++) {
	struct ON_Brep_CDT_State *s_cdt = *s_it;
	if (!CDT_Audit(s_cdt)) {
	    std::cerr << "Invalid: " << s_cdt->name << " edge data\n";
	    bu_exit(1, "urk\n");
	}
    }

    std::set<cdt_mesh_t *>::iterator f_it;
    for (f_it = fmeshes.begin(); f_it != fmeshes.end(); f_it++) {
	cdt_mesh_t *fmesh = *f_it;
	if (!fmesh->valid(verbosity)) {
	    valid = false;
#if 1
	    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh->p_cdt;
	    std::string fpname = std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh->f_id) + std::string(".plot3");
	    fmesh->tris_plot(fpname.c_str());
	    std::cerr << "Invalid: " << s_cdt->name << " face " << fmesh->f_id << "\n";
#endif
	}
    }
    if (!valid) {
#if 1
	for (f_it = fmeshes.begin(); f_it != fmeshes.end(); f_it++) {
	    cdt_mesh_t *fmesh = *f_it;
	    fmesh->valid(2);
	}
#endif
	std::cout << "fatal mesh damage\n";
	//bu_exit(1, "fatal mesh damage");
    }

    return valid;
}

int
ON_Brep_CDT_Ovlp_Resolve(struct ON_Brep_CDT_State **s_a, int s_cnt, double lthresh, int timeout)
{
    if (!s_a) return -1;
    if (s_cnt < 1) return 0;

    double timestamp = bu_gettime();

    // Get the bounding boxes of all faces of all breps in s_a, and find
    // possible interactions
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> check_pairs;
    check_pairs = possibly_interfering_face_pairs(s_a, s_cnt);

    //std::cout << "Found " << check_pairs.size() << " potentially interfering face pairs\n";
    if (!check_pairs.size()) return 0;

    // Sanity check - are we valid?
    if (!check_faces_validity(check_pairs)) {
	bu_log("ON_Brep_CDT_Ovlp_Resolve:  invalid inputs - not attempting overlap processing!\n");
	return -1;
    }

    std::map<cdt_mesh_t *, std::set<uedge_t>> otsets;
    std::set<bedge_seg_t *> bsegs; 

    int ccnt = 0;
    int ecnt = mesh_ovlps(&otsets, &bsegs, check_pairs, 1, lthresh);
    while (ecnt > 0) {
	if (((bu_gettime() - timestamp)/1e6) > (double)timeout) {
	    return ecnt;
	}
	// bsegs first - they impact more than one face
	std::set<bedge_seg_t *>::iterator b_it; 
	for (b_it = bsegs.begin(); b_it != bsegs.end(); b_it++) {
	    bedge_seg_t *bseg = *b_it;
	    double t = bseg->edge_start + ((bseg->edge_end - bseg->edge_start) * 0.5);
	    brep_split_edge(*b_it, t);
	}

	if (((bu_gettime() - timestamp)/1e6) > (double)timeout) {
	    return ecnt;
	}

	std::map<cdt_mesh_t *, std::set<uedge_t>>::iterator ot_it;
	for (ot_it = otsets.begin(); ot_it != otsets.end(); ot_it++) {
	    std::set<uedge_t>::iterator u_it;
	    for (u_it = ot_it->second.begin(); u_it != ot_it->second.end(); u_it++) {
		uedge_t ue = *u_it;
		internal_split_edge(ot_it->first, ue);
	    }
	}

	if (((bu_gettime() - timestamp)/1e6) > (double)timeout) {
	    return ecnt;
	}

	bu_log("Cycle %d: %d seconds\n", ccnt, (int)((bu_gettime() - timestamp)/1e6));
	ccnt++;

	ecnt = mesh_ovlps(&otsets, &bsegs, check_pairs, 1, lthresh);
    }

    // Make sure everything is still OK and do final overlap check
    check_faces_validity(check_pairs);

    return ecnt;
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

