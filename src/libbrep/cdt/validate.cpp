/*                C D T _ V A L I D A T E . C P P
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
/** @file cdt_validate.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

/* TODO:
 *
 * Need a process for how to modify edges and faces, and in what order.  Need to
 * come up with heuristics for applying different "fixing" strategies:
 *
 * - remove problematic surface point from face tessellation
 * - insert new point(s) at midpoints of edges of problem face
 * - insert new point at the center of the problem face
 * - split edge involved with problem face and retessellate both faces on that edge
 *
 * Different strategies may be appropriate depending on the characteristics of the
 * "bad" triangle - for example, a triangle with all three brep normals nearly perpendicular
 * to the triangle normal and having only one non-edge point we should probably try to
 * handle by removing the surface point.  On the other hand, if the angle is not as
 * perpendicular and the surface area is significant, we might want to add a new point
 * on the longest edge of the triangle (which might be a shared edge with another face.)
 *
 * Also, consider whether to retessellate whole face or try to assemble "local" set of
 * faces involved, build bounding polygon and do new local tessellation.  Latter
 * is a lot more work but might avoid re-introducing new problems elsewhere in a
 * mesh during a full re-CDT.
 *
 * Need to be able to introduce new edge points in specific subranges of an edge.
 *
 */

#include "common.h"
#include "./cdt.h"

#if 0
typedef std::pair<ON_3dPoint *, ON_3dPoint *> Edge;
typedef std::map<Edge, std::set<p2t::Triangle*> > EdgeToTri;


static Edge
mk_edge(ON_3dPoint *pt_A, ON_3dPoint *pt_B)
{
    if (pt_A <= pt_B) {
	return std::make_pair(pt_A, pt_B);
    } else {
	return std::make_pair(pt_B, pt_A);
    }
}

#define IsEdgePt(_a) (f->s_cdt->edge_pnts->find(_a) != f->s_cdt->edge_pnts->end())

void
add_tri_edges(EdgeToTri &e2f, std::map<Edge, int> &ecnt, struct ON_Brep_CDT_Face_State *f, p2t::Triangle *t,
	std::map<p2t::Point *, ON_3dPoint *> *pointmap, std::map<ON_3dPoint *, p2t::Point *> *on3_to_p2t)
{
    ON_3dPoint *pt_A, *pt_B, *pt_C;

    p2t::Point *p2_A = t->GetPoint(0);
    p2t::Point *p2_B = t->GetPoint(1);
    p2t::Point *p2_C = t->GetPoint(2);
    pt_A = (*pointmap)[p2_A];
    pt_B = (*pointmap)[p2_B];
    pt_C = (*pointmap)[p2_C];
    (*on3_to_p2t)[pt_A] = p2_A;
    (*on3_to_p2t)[pt_B] = p2_B;
    (*on3_to_p2t)[pt_C] = p2_C;
    e2f[(std::make_pair(pt_A, pt_B))].insert(t);
    e2f[(std::make_pair(pt_B, pt_C))].insert(t);
    e2f[(std::make_pair(pt_C, pt_A))].insert(t);

    // We need to count edge uses.
    ecnt[(mk_edge(pt_A, pt_B))]++;
    ecnt[(mk_edge(pt_B, pt_C))]++;
    ecnt[(mk_edge(pt_C, pt_A))]++;

    // Because we are working on a single isolated face, only closed surface
    // directions will result in closed meshes.  Count edge segments twice, so
    // the < 2 uses test will always work even if the surface isn't closed
    if (IsEdgePt(pt_A) && IsEdgePt(pt_B)) {
	ecnt[(mk_edge(pt_A, pt_B))]++;
    }
    if (IsEdgePt(pt_B) && IsEdgePt(pt_C)) {
	ecnt[(mk_edge(pt_B, pt_C))]++;
    }
    if (IsEdgePt(pt_C) && IsEdgePt(pt_A)) {
	ecnt[(mk_edge(pt_C, pt_A))]++;
    }
}

int
triangles_check_edges(struct ON_Brep_CDT_Face_State *f)
{
    EdgeToTri e2f;
    std::map<Edge, int> ecnt;
    int have_problem_edge = 0;
    int ret = 0;
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::map<ON_3dPoint *, p2t::Point *> on3_to_p2t;
    std::set<p2t::Triangle *>::iterator tr_it;
    std::set<p2t::Triangle *> *tris = f->tris;
    for (tr_it = tris->begin(); tr_it != tris->end(); tr_it++) {
	p2t::Triangle *t = *tr_it;
	add_tri_edges(e2f, ecnt, f, t, pointmap, &on3_to_p2t);
    }

    std::map<ON_3dPoint *, std::set<Edge>> point_to_edges;
    std::map<ON_3dPoint *, int> point_cnts;
    std::map<Edge, int>::iterator ec_it;

    for (ec_it = ecnt.begin(); ec_it != ecnt.end(); ec_it++) {
	if (ec_it->second < 2) {
	    ON_3dPoint *p1 = ec_it->first.first;
	    ON_3dPoint *p2 = ec_it->first.second;
	    bu_log("Face %d: single use edge found! %f %f %f -> %f %f %f\n", f->ind, p1->x, p1->y, p1->z, p2->x, p2->y, p2->z);
	    have_problem_edge = 1;
	    if (!IsEdgePt(p1)) {
		point_cnts[p1]++;
		point_to_edges[p1].insert(ec_it->first);
	    }
	    if (!IsEdgePt(p2)) {
		point_cnts[p2]++;
		point_to_edges[p2].insert(ec_it->first);
	    }
	}
    }

    std::map<ON_3dPoint *, int>::iterator pc_it;
    for (pc_it = point_cnts.begin(); pc_it != point_cnts.end(); pc_it++) {
	if (pc_it->second > 1) {
	    ON_3dPoint *p = pc_it->first;
	    bu_log("Face %d problem point with %d hits: %f %f %f\n", f->ind, pc_it->second, p->x, p->y, p->z);
	    p2t::Point *tpnt = on3_to_p2t[p];
	    ON_2dPoint *spnt = (*f->p2t_to_on2_map)[tpnt];
	    f->on_surf_points->erase(spnt);
	    ret = 1;
	    std::set<Edge> edges = point_to_edges[p];
	    std::set<Edge>::iterator e_it;
	    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
		Edge e = *e_it;
		std::set<p2t::Triangle*> tset;
		if (e2f.find(e) == e2f.end()) {
		    Edge ef = std::make_pair(e.second, e.first);
		    tset = e2f[ef];
		} else {
		    tset = e2f[e];
		}
		std::set<p2t::Triangle*>::iterator ptr_it;
		for (ptr_it = tset.begin(); ptr_it != tset.end(); ptr_it++) {
		    p2t::Triangle *t = *ptr_it;
		    ON_3dPoint *tpnts[3] = {NULL, NULL, NULL};
		    for (size_t j = 0; j < 3; j++) {
			tpnts[j] = (*pointmap)[t->GetPoint(j)];
		    }
		    bu_log("Triangle (%f %f %f) -> (%f %f %f) -> (%f %f %f)\n", tpnts[0]->x, tpnts[0]->y, tpnts[0]->z, tpnts[1]->x, tpnts[1]->y, tpnts[1]->z ,tpnts[2]->x, tpnts[2]->y, tpnts[2]->z);
		}
	    }
	}
    }

    if (have_problem_edge && !ret) {
	bu_log("Problem edge with no way to resolve it\n");
	ret = -1;
    }

    return ret;
}


void
triangles_degenerate_trivial(struct ON_Brep_CDT_Face_State *f)
{
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::set<p2t::Triangle *>::iterator tr_it;
    std::set<p2t::Triangle *> *tris = f->tris;
    for (tr_it = tris->begin(); tr_it != tris->end(); tr_it++) {
	p2t::Triangle *t = *tr_it;
	(*f->s_cdt->tri_brep_face)[t] = f->ind;
	ON_3dPoint *tpnts[3] = {NULL, NULL, NULL};
	for (size_t j = 0; j < 3; j++) {
	    tpnts[j] = (*pointmap)[t->GetPoint(j)];
	}

	/* See if this triangle is trivially degenerate (this can happen with
	 * singular trims) */
	if (tpnts[0] == tpnts[1] || tpnts[1] == tpnts[2] || tpnts[2] == tpnts[0]) {
	    /* degenerate */
	    f->degen_tris->insert(t);
	}
    }
}

int
triangle_is_zero_area(p2t::Triangle *t, std::map<p2t::Point *, ON_3dPoint *> *pointmap, fastf_t dist)
{
    int is_zero_area = 0;

    ON_3dPoint *tpnts[3] = {NULL, NULL, NULL};
    for (size_t j = 0; j < 3; j++) {
	p2t::Point *p = t->GetPoint(j);
	ON_3dPoint *op = (*pointmap)[p];
	tpnts[j] = op;
    }

    ON_Line l(*tpnts[0], *tpnts[2]);
    if (l.Length() < dist) {
	ON_Line l2(*tpnts[0], *tpnts[1]);
	if (l2.Length() < dist) {
	    is_zero_area = 1;
	} else {
	    if (l2.DistanceTo(*tpnts[2]) < dist) {
		is_zero_area = 1;
	    }
	}
    } else {
	if (l.DistanceTo(*tpnts[1]) < dist) {
	    is_zero_area = 1;
	}
    }

    return is_zero_area;
}

void
triangles_degenerate_area_notify(struct ON_Brep_CDT_Face_State *f)
{
    /* If we have degeneracies along an edge, the impact is not
     * local to this face but will also impact the other face.
     * Find them and notify them. */
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::set<p2t::Point *>::iterator p2it;
    for (p2it = f->degen_pnts->begin(); p2it != f->degen_pnts->end(); p2it++) {
	ON_3dPoint *pt3D = (*pointmap)[*p2it];
	std::set<BrepTrimPoint *> &pt = (*f->s_cdt->on_brep_edge_pnts)[pt3D];
	std::set<BrepTrimPoint *>::iterator bit;
	for (bit = pt.begin(); bit != pt.end(); bit++) {
	    BrepTrimPoint *tpt = *bit;
	    int f2ind = f->s_cdt->brep->m_T[tpt->trim_ind].Face()->m_face_index;
	    if (f2ind != f->ind) {
		//bu_log("Pulls in face %d\n", f2ind);
		std::set<p2t::Point *> tri_pnts = (*f->on3_to_tri_map)[pt3D];
		std::set<p2t::Point *>::iterator tp_it;
		for (tp_it = tri_pnts.begin(); tp_it != tri_pnts.end(); tp_it++) {
		    f->ext_degen_pnts->insert(*tp_it);
		}
	    }
	}
    }
}

void
triangles_degenerate_area(struct ON_Brep_CDT_Face_State *f)
{
    f->degen_pnts->clear();
    std::set<p2t::Triangle*> tris_degen;
    // Use a distance three orders of magnitude smaller than the smallest
    // edge segment length of the face to decide if a face is degenerate
    // based on area.
    fastf_t dist = 0.001 * (*f->s_cdt->min_edge_seg_len)[f->ind];
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::set<p2t::Triangle *>::iterator tr_it;
    std::set<p2t::Triangle *> *tris = f->tris;
    for (tr_it = tris->begin(); tr_it != tris->end(); tr_it++) {
	p2t::Triangle *t = *tr_it;

	if (triangle_is_zero_area(t, pointmap, dist)) {
	    // The edges from this face are degenerate edges
	    p2t::Point *p2_A = t->GetPoint(0);
	    p2t::Point *p2_B = t->GetPoint(1);
	    p2t::Point *p2_C = t->GetPoint(2);

	    f->degen_pnts->insert(p2_A);
	    f->degen_pnts->insert(p2_B);
	    f->degen_pnts->insert(p2_C);

	    tris_degen.insert(t);
	}
    }

    for (tr_it = tris_degen.begin(); tr_it != tris_degen.end(); tr_it++) {
	p2t::Triangle *t = *tr_it;
	f->tris->erase(t);
	delete t;
    }
}

// Check boundary triangles for distortion.
// If problem, yank the non-edge point - local impact only, and we already try
// to avoid sampling too close to edges.
int
triangles_slim_edge(struct ON_Brep_CDT_Face_State *f)
{
    int pntset_altered = 0;
    struct trimesh_info *tm = CDT_Face_Build_Halfedge(f->tris);
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;

    std::vector<std::pair<trimesh::index_t, trimesh::index_t>> bedges = tm->mesh.boundary_edges();
    std::vector<std::pair<trimesh::index_t, trimesh::index_t>>::iterator b_it;
    std::set<trimesh::index_t> etris;
    for (b_it = bedges.begin(); b_it != bedges.end(); b_it++) {
	// trimesh boundary edges won't map to a triangle - get the flipped
	// form of the edge that will
	std::pair<trimesh::index_t, trimesh::index_t> fedge = std::pair<trimesh::index_t, trimesh::index_t>((*b_it).second, (*b_it).first);
	trimesh::index_t tind = tm->mesh.m_de2fi[fedge];
	etris.insert(tind);
	p2t::Triangle *t = tm->triangles[tind].t;

	p2t::Point *p1 = tm->ind2p2d[(*b_it).first];  // edge point
	p2t::Point *p2 = tm->ind2p2d[(*b_it).second]; // edge point
	p2t::Point *p3 = NULL; // surface point
	for (size_t j = 0; j < 3; j++) {
	    if (t->GetPoint(j) != p1 && t->GetPoint(j) != p2) {
		p3 = t->GetPoint(j);
		break;
	    }
	}

	if (!p1 || !p2 || !p3) {
	    bu_log("Triangle point failure!\n");
	}

	ON_3dPoint *e1 = (*pointmap)[p1];
	ON_3dPoint *e2 = (*pointmap)[p2];
	ON_3dPoint *pf3d = (*pointmap)[p3];

	if (!e1 || !e2 || !pf3d) {
	    bu_log("3D Triangle point failure!\n");
	}

	if (e1 == e2 || e1 == pf3d || e2 == pf3d) {
	    // Don't make any decisions based on trivially degenerate triangles
	    continue;
	}

	ON_Line eline(*e1, *e2);
	double elen = e1->DistanceTo(*e2);
	double dist = eline.DistanceTo(*pf3d);
	if (elen > 100*dist) {
	    ON_2dPoint *spnt = (*f->p2t_to_on2_map)[p3];
	    ON_3dPoint *pt3D = (*pointmap)[p3];
	    // Don't yank if pt3D is also an edge point
	    if (f->s_cdt->on_brep_edge_pnts->find(pt3D) == f->s_cdt->on_brep_edge_pnts->end()) {
		f->on_surf_points->erase(spnt);
		pntset_altered = 1;
		bu_log("%f %f %f elen: %f, dist: %f\n", pf3d->x, pf3d->y, pf3d->z, elen, dist);
	    }
	}
    }

    delete tm;
    return pntset_altered;
}

// If triangles involving two edge points have incorrect normals, we yank that
// surface point from the set - the most likely scenario is it is too close to
// a relatively course approximating edge segment from the trimming curve and
// is forcing the mesh to distort in the vicinity of the edge.
int
triangles_check_edge_tris(struct ON_Brep_CDT_Face_State *f)
{
    int ret = 0;
    std::set<p2t::Triangle*> tris_degen;
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = f->p2t_to_on3_norm_map;
    std::map<p2t::Point *, int> *p2t_trim_ind = f->p2t_trim_ind;
    std::set<p2t::Point *> problem_points;

    std::set<p2t::Triangle *>::iterator tr_it;
    std::set<p2t::Triangle *> *tris = f->tris;
    for (tr_it = tris->begin(); tr_it != tris->end(); tr_it++) {
	p2t::Triangle *t = *tr_it;

	// Don't make any decisions based on trivially degenerate triangles
	if (f->degen_tris->find(t) != f->degen_tris->end()) continue;

	int invalid_face_normal = 0;
	ON_3dVector tdir = p2tTri_Normal(t, pointmap);
	p2t::Point *p[3];
	p[0] = t->GetPoint(0);
	p[1] = t->GetPoint(1);
	p[2] = t->GetPoint(2);
	int edge_pnt_cnt = 0;
	int tind[3] = {-1, -1, -1};
	int wnorm[3] = {0, 0, 0};
	int problem_surf_point = 0;
	for (size_t j = 0; j < 3; j++) {
	    ON_3dPoint *spt = (*pointmap)[p[j]];
	    ON_3dPoint onorm = *(*normalmap)[p[j]];
	    if (f->s_cdt->singular_vert_to_norms->find(spt) != f->s_cdt->singular_vert_to_norms->end()) {
		ON_3dPoint *cnrm = (*f->s_cdt->singular_vert_to_norms)[spt];
		onorm = *cnrm;
	    } else {
		if (f->s_cdt->brep->m_F[f->ind].m_bRev) {
		    onorm = onorm * -1;
		}
	    }
	    if (tdir.Length() > 0 && ON_DotProduct(onorm, tdir) < 0.1) {
		invalid_face_normal++;
		wnorm[j] = 1;
		if (p2t_trim_ind->find(p[j]) != p2t_trim_ind->end()) {
		    tind[j] = (*p2t_trim_ind)[p[j]];
		    edge_pnt_cnt++;
		} else {
		    problem_surf_point++;
		}
	    }
	}
	if (problem_surf_point && edge_pnt_cnt == 2) {
	    for (int j = 0; j < 3; j++) {
		if (wnorm[j] && tind[j] == -1) {
		    ON_3dPoint *spt = (*pointmap)[p[j]];
		    problem_points.insert(p[j]);
		    ret++;
		    bu_log("Check_Edge_Triangle_Normals: problem point from surface: %f %f %f\n", spt->x, spt->y, spt->z);
		}
	    }
	}
    }

    // If we found any problematic points, scrub them out of the on_surf_points set
    if (ret) {
	std::set<p2t::Point *>::iterator p_it;
	for (p_it = problem_points.begin(); p_it != problem_points.end(); p_it++) {
	    ON_2dPoint *p2d = (*f->p2t_to_on2_map)[*p_it];
	    f->on_surf_points->erase(p2d);
	}
    }

    return (ret > 0);
}

int
triangles_incorrect_normals(struct ON_Brep_CDT_Face_State *f, std::set<p2t::Triangle *> *atris)
{
    int ret = 0;
    std::set<p2t::Triangle*> tris_degen;
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = f->p2t_to_on3_norm_map;
    std::map<p2t::Point *, int> *p2t_trim_ind = f->p2t_trim_ind;
    std::set<p2t::Point *> problem_points;

    std::set<p2t::Triangle *>::iterator tr_it;
    std::set<p2t::Triangle *> *tris = f->tris;
    for (tr_it = tris->begin(); tr_it != tris->end(); tr_it++) {
	p2t::Triangle *t = *tr_it;

	int invalid_face_normal = 0;
	ON_3dVector tdir = p2tTri_Normal(t, pointmap);
	p2t::Point *p[3];
	p[0] = t->GetPoint(0);
	p[1] = t->GetPoint(1);
	p[2] = t->GetPoint(2);
	int wnorm[3] = {0, 0, 0};
	for (size_t j = 0; j < 3; j++) {
	    ON_3dPoint onorm = *(*normalmap)[p[j]];
	    if (f->s_cdt->brep->m_F[f->ind].m_bRev) {
		onorm = onorm * -1;
	    }
	    if (tdir.Length() > 0 && ON_DotProduct(onorm, tdir) < 0.1) {
		invalid_face_normal++;
		wnorm[j] = 1;
	    }
	}

	if (invalid_face_normal) {
	    int tind[3] = {-1, -1, -1};
	    int edge_pnt_cnt = 0;
	    for (int j = 0; j < 3; j++) {
		if (p2t_trim_ind->find(p[j]) != p2t_trim_ind->end()) {
		    tind[j] = (*p2t_trim_ind)[p[j]];
		    edge_pnt_cnt++;
		}
	    }

	    // We shouldn't have faces with incorrect normals involving
	    // boundary edges that survived previous passes - if we do, it's an
	    // indication of an error
	    if (edge_pnt_cnt == 2) {
		bu_log("UNCULLED problem point from surface:\n");
		for (int j = 0; j < 3; j++) {
		    if (wnorm[j] == 1 && tind[j] == -1) {
			ON_3dPoint *spt = (*pointmap)[p[j]];
			bu_log("%f %f %f\n", spt->x, spt->y, spt->z);
		    }
		}
		return -1;
	    }

	    atris->insert(t);
	    ret++;
	}
    }

    return (ret > 0);
}

void
triangles_rebuild_involved_pnts(struct ON_Brep_CDT_Face_State *f, std::set<p2t::Point *> *fdp)
{
    std::set<p2t::Triangle*> tris_degen;
    if (!f || !fdp) {
	return;
    }
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = f->p2t_to_on3_norm_map;

    std::set<p2t::Triangle *>::iterator tr_it;
    std::set<p2t::Triangle *> *tris = f->tris;
    for (tr_it = tris->begin(); tr_it != tris->end(); tr_it++) {
	p2t::Triangle *t = *tr_it;
	int involved_pnt_cnt = 0;
	p2t::Point *t2dpnts[3] = {NULL, NULL, NULL};
	for (size_t j = 0; j < 3; j++) {
	    p2t::Point *p = t->GetPoint(j);
	    t2dpnts[j] = p;
	    if (fdp->find(p) != fdp->end()) {
		involved_pnt_cnt++;
	    }
	}
	if (involved_pnt_cnt > 1) {

	    // TODO - construct the plane of this face, project 3D points
	    // into that plane to get new 2D coordinates specific to this
	    // face, and then make new p2t points from those 2D coordinates.
	    // Won't be using the NURBS 2D for this part, so existing
	    // p2t points won't help.
	    std::vector<p2t::Point *>new_2dpnts;
	    std::map<p2t::Point *, p2t::Point *> new2d_to_old2d;
	    std::map<p2t::Point *, p2t::Point *> old2d_to_new2d;
	    std::map<ON_3dPoint *, p2t::Point *> on3d_to_new2d;
	    ON_3dPoint *t3dpnts[3] = {NULL, NULL, NULL};
	    for (size_t j = 0; j < 3; j++) {
		t3dpnts[j] = (*pointmap)[t2dpnts[j]];
	    }
	    int normal_backwards = 0;
	    ON_Plane fplane(*t3dpnts[0], *t3dpnts[1], *t3dpnts[2]);

	    // To verify sanity, check fplane normal against face normals
	    for (size_t j = 0; j < 3; j++) {
		ON_3dVector pv = (*(*normalmap)[t2dpnts[j]]);
		if (ON_DotProduct(pv, fplane.Normal()) < 0) {
		    normal_backwards++;
		}
	    }
	    if (normal_backwards > 0) {
		if (normal_backwards == 3) {
		    fplane.Flip();
		    bu_log("flipped plane\n");
		} else {
		    bu_log("Only %d of the face normals agree with the plane normal??\n", 3 - normal_backwards);
		}
	    }

	    // Project the 3D face points onto the plane
	    double ucent = 0;
	    double vcent = 0;
	    for (size_t j = 0; j < 3; j++) {
		double u,v;
		fplane.ClosestPointTo(*t3dpnts[j], &u, &v);
		ucent += u;
		vcent += v;
		p2t::Point *np = new p2t::Point(u, v);
		new2d_to_old2d[np] = t2dpnts[j];
		old2d_to_new2d[t2dpnts[j]] = np;
		on3d_to_new2d[t3dpnts[j]] = np;
	    }
	    ucent = ucent/3;
	    vcent = vcent/3;
	    ON_2dPoint tcent(ucent, vcent);

	    // Any or all of the edges of the triangle may have orphan
	    // points associated with them - if both edge points are
	    // involved, we have to check.
	    std::vector<p2t::Point *> polyline;
	    for (size_t j = 0; j < 3; j++) {
		p2t::Point *p1 = t2dpnts[j];
		p2t::Point *p2 = (j < 2) ? t2dpnts[j+1] : t2dpnts[0];
		ON_3dPoint *op1 = (*pointmap)[p1];
		polyline.push_back(old2d_to_new2d[p1]);
		if (fdp->find(p1) != fdp->end() && fdp->find(p2) != fdp->end()) {

		    // Calculate a vector to use to perturb middle points off the
		    // line in 2D.  Triangulation routines don't like collinear
		    // points.  Since we're not using these 2D coordinates for
		    // anything except the tessellation itself, as long as we
		    // don't change the ordering of the polyline we should be
		    // able to nudge the points off the line without ill effect.
		    ON_2dPoint p2d1(p1->x, p1->y);
		    ON_2dPoint p2d2(p2->x, p2->y);
		    ON_2dPoint pmid = (p2d2 + p2d1)/2;
		    ON_2dVector ptb = pmid - tcent;
		    fastf_t edge_len = p2d1.DistanceTo(p2d2);
		    ptb.Unitize();
		    ptb = ptb * edge_len;

		    std::set<ON_3dPoint *> edge_3d_pnts;
		    std::map<double, ON_3dPoint *> ordered_new_pnts;
		    ON_3dPoint *op2 = (*pointmap)[p2];
		    edge_3d_pnts.insert(op1);
		    edge_3d_pnts.insert(op2);
		    ON_Line eline3d(*op1, *op2);
		    double t1, t2;
		    eline3d.ClosestPointTo(*op1, &t1);
		    eline3d.ClosestPointTo(*op2, &t2);
		    std::set<p2t::Point *>::iterator fdp_it;
		    for (fdp_it = fdp->begin(); fdp_it != fdp->end(); fdp_it++) {
			p2t::Point *p = *fdp_it;
			if (p != p1 && p != p2) {
			    ON_3dPoint *p3d = (*pointmap)[p];
			    if (edge_3d_pnts.find(p3d) == edge_3d_pnts.end()) {
				if (eline3d.DistanceTo(*p3d) < BN_TOL_DIST) {
				    double tp;
				    eline3d.ClosestPointTo(*p3d, &tp);
				    if (tp > t1 && tp < t2) {
					edge_3d_pnts.insert(p3d);
					ordered_new_pnts[tp] = p3d;
					double u,v;
					fplane.ClosestPointTo(*p3d, &u, &v);

					// Make a 2D point and nudge it off of the length
					// of the edge off the edge.  Use the line parameter
					// value to get different nudge factors for each point.
					ON_2dPoint uvp(u, v);
					uvp = uvp + (t2-tp)/(t2-t1)*0.01*ptb;

					// Define the p2t point and update maps
					p2t::Point *np = new p2t::Point(uvp.x, uvp.y);
					new2d_to_old2d[np] = p;
					old2d_to_new2d[p] = np;
					on3d_to_new2d[p3d] = np;

				    }
				}
			    }
			}
		    }

		    // Have all new points on edge, add to polyline in edge order
		    if (t1 < t2) {
			std::map<double, ON_3dPoint *>::iterator m_it;
			for (m_it = ordered_new_pnts.begin(); m_it != ordered_new_pnts.end(); m_it++) {
			    ON_3dPoint *p3d = (*m_it).second;
			    polyline.push_back(on3d_to_new2d[p3d]);
			}
		    } else {
			std::map<double, ON_3dPoint *>::reverse_iterator m_it;
			for (m_it = ordered_new_pnts.rbegin(); m_it != ordered_new_pnts.rend(); m_it++) {
			    ON_3dPoint *p3d = (*m_it).second;
			    polyline.push_back(on3d_to_new2d[p3d]);
			}
		    }
		}

		// I think(?) - need to close the loop
		if (j == 2) {
		    polyline.push_back(old2d_to_new2d[p2]);
		}
	    }

	    // Have polyline, do CDT
	    if (polyline.size() > 5) {
		bu_log("%d polyline cnt: %zd\n", f->ind, polyline.size());
	    }
	    if (polyline.size() > 4) {

		// First, try ear clipping method
		std::map<size_t, p2t::Point *> ec_to_p2t;
		point2d_t *ec_pnts = (point2d_t *)bu_calloc(polyline.size(), sizeof(point2d_t), "ear clipping point array");
		for (size_t k = 0; k < polyline.size()-1; k++) {
		    p2t::Point *p2tp = polyline[k];
		    V2SET(ec_pnts[k], p2tp->x, p2tp->y);
		    ec_to_p2t[k] = p2tp;
		}

		int *ecfaces;
		int num_faces;

		// TODO - we need to check if we're getting zero area triangles
		// back from these routines.  Unless all three triangle edges
		// somehow have extra points, we can construct a triangle set
		// that meets our needs by walking the edges in order and
		// building triangles "by hand" to meet our need.  Calling the
		// "canned" routines is an attempt to avoid that bookkeeping
		// work, but if the tricks in place to avoid collinear point
		// issues don't work reliably we'll need to just go with a
		// direct build.
		if (bg_polygon_triangulate(&ecfaces, &num_faces, NULL, NULL, NULL, 0, ec_pnts, polyline.size()-1, TRI_EAR_CLIPPING)) {

		    // Didn't work, see if poly2tri can handle it
		    //bu_log("ec triangulate failed\n");
		    p2t::CDT *fcdt = new p2t::CDT(polyline);
		    fcdt->Triangulate(true, -1);
		    std::vector<p2t::Triangle*> ftris = fcdt->GetTriangles();
		    if (ftris.size() > polyline.size()) {
#if 0
			bu_log("weird face count: %zd\n", ftris.size());
			for (size_t k = 0; k < polyline.size(); k++) {
			    bu_log("polyline[%zd]: %0.17f, %0.17f 0\n", k, polyline[k]->x, polyline[k]->y);
			}

			std::string pfile = std::string("polyline.plot3");
			plot_polyline(&polyline, pfile.c_str());
			for (size_t k = 0; k < ftris.size(); k++) {
			    std::string tfile = std::string("tri-") + std::to_string(k) + std::string(".plot3");
			    plot_tri(ftris[k], tfile.c_str());
			    p2t::Point *p0 = ftris[k]->GetPoint(0);
			    p2t::Point *p1 = ftris[k]->GetPoint(1);
			    p2t::Point *p2 = ftris[k]->GetPoint(2);
			    bu_log("tri[%zd]: %f %f -> %f %f -> %f %f\n", k, p0->x, p0->y, p1->x, p1->y, p2->x, p2->y);
			}
#endif

			bu_log("EC and Poly2Tri failed - aborting\n");
			return;
		    } else {
			bu_log("Poly2Tri: found %zd faces\n", ftris.size());
		    }

		} else {
		    for (int k = 0; k < num_faces; k++) {
			p2t::Point *p2_1 = new2d_to_old2d[ec_to_p2t[ecfaces[3*k]]];
			p2t::Point *p2_2 = new2d_to_old2d[ec_to_p2t[ecfaces[3*k+1]]];
			p2t::Point *p2_3 = new2d_to_old2d[ec_to_p2t[ecfaces[3*k+2]]];
			p2t::Triangle *nt = new p2t::Triangle(*p2_1, *p2_2, *p2_3);
			f->tris->insert(nt);
		    }
		    // We split the original triangle, so it's now replaced/degenerate in the tessellation
		    tris_degen.insert(t);
		}
	    } else {
		// Point count doesn't indicate any need to split, we should be good...
		//bu_log("Not enough points in polyline to require splitting\n");
	    }
	}
    }

    for (tr_it = tris_degen.begin(); tr_it != tris_degen.end(); tr_it++) {
	p2t::Triangle *t = *tr_it;
	f->tris->erase(t);
	delete t;
    }
}

void
triangles_rebuild_involved(struct ON_Brep_CDT_Face_State *f)
{
    if (!f) return;
    triangles_rebuild_involved_pnts(f, f->degen_pnts);
    triangles_rebuild_involved_pnts(f, f->ext_degen_pnts);
}

#endif

void
trimesh_error_report(struct ON_Brep_CDT_State *s_cdt, int valid_fcnt, int UNUSED(valid_vcnt), int *valid_faces, fastf_t *valid_vertices, struct bg_trimesh_solid_errors *se)
{
    if (se->degenerate.count > 0) {
	std::set<int> problem_pnts;
	std::set<int>::iterator pp_it;
	bu_log("%d degenerate faces\n", se->degenerate.count);
	for (int i = 0; i < se->degenerate.count; i++) {
	    int face = se->degenerate.faces[i];
	    bu_log("dface %d: %d %d %d :  %f %f %f->%f %f %f->%f %f %f \n", face, valid_faces[face*3], valid_faces[face*3+1], valid_faces[face*3+2],
		    valid_vertices[valid_faces[face*3]*3], valid_vertices[valid_faces[face*3]*3+1],valid_vertices[valid_faces[face*3]*3+2],
		    valid_vertices[valid_faces[face*3+1]*3], valid_vertices[valid_faces[face*3+1]*3+1],valid_vertices[valid_faces[face*3+1]*3+2],
		    valid_vertices[valid_faces[face*3+2]*3], valid_vertices[valid_faces[face*3+2]*3+1],valid_vertices[valid_faces[face*3+2]*3+2]);
	    problem_pnts.insert(valid_faces[face*3]);
	    problem_pnts.insert(valid_faces[face*3+1]);
	    problem_pnts.insert(valid_faces[face*3+2]);
	}
	for (pp_it = problem_pnts.begin(); pp_it != problem_pnts.end(); pp_it++) {
	    int pind = (*pp_it);
	    ON_3dPoint *p = (*s_cdt->bot_pnt_to_on_pnt)[pind];
	    if (!p) {
		bu_log("unmapped point??? %d\n", pind);
	    } else {
		struct cdt_audit_info *paudit = (*s_cdt->pnt_audit_info)[p];
		if (!paudit) {
		    bu_log("point with no audit info??? %d\n", pind);
		} else {
		    bu_log("point %d: Face(%d) Vert(%d) Trim(%d) Edge(%d) UV(%f,%f)\n", pind, paudit->face_index, paudit->vert_index, paudit->trim_index, paudit->edge_index, paudit->surf_uv.x, paudit->surf_uv.y);
		}
	    }
	}
    }
    if (se->excess.count > 0) {
	bu_log("extra edges???\n");
    }
    if (se->unmatched.count > 0) {
	std::set<int> problem_pnts;
	std::set<int>::iterator pp_it;

	bu_log("%d unmatched edges\n", se->unmatched.count);
	for (int i = 0; i < se->unmatched.count; i++) {
	    int v1 = se->unmatched.edges[i*2];
	    int v2 = se->unmatched.edges[i*2+1];
	    bu_log("%d->%d: %f %f %f->%f %f %f\n", v1, v2,
		    valid_vertices[v1*3], valid_vertices[v1*3+1], valid_vertices[v1*3+2],
		    valid_vertices[v2*3], valid_vertices[v2*3+1], valid_vertices[v2*3+2]
		  );
	    for (int j = 0; j < valid_fcnt; j++) {
		std::set<std::pair<int,int>> fedges;
		fedges.insert(std::pair<int,int>(valid_faces[j*3],valid_faces[j*3+1]));
		fedges.insert(std::pair<int,int>(valid_faces[j*3+1],valid_faces[j*3+2]));
		fedges.insert(std::pair<int,int>(valid_faces[j*3+2],valid_faces[j*3]));
		int has_edge = (fedges.find(std::pair<int,int>(v1,v2)) != fedges.end()) ? 1 : 0;
		if (has_edge) {
		    int face = j;
		    bu_log("eface %d: %d %d %d :  %f %f %f->%f %f %f->%f %f %f \n", face, valid_faces[face*3], valid_faces[face*3+1], valid_faces[face*3+2],
			    valid_vertices[valid_faces[face*3]*3], valid_vertices[valid_faces[face*3]*3+1],valid_vertices[valid_faces[face*3]*3+2],
			    valid_vertices[valid_faces[face*3+1]*3], valid_vertices[valid_faces[face*3+1]*3+1],valid_vertices[valid_faces[face*3+1]*3+2],
			    valid_vertices[valid_faces[face*3+2]*3], valid_vertices[valid_faces[face*3+2]*3+1],valid_vertices[valid_faces[face*3+2]*3+2]);
		    problem_pnts.insert(valid_faces[j*3]);
		    problem_pnts.insert(valid_faces[j*3+1]);
		    problem_pnts.insert(valid_faces[j*3+2]);
		}
	    }
	}
	for (pp_it = problem_pnts.begin(); pp_it != problem_pnts.end(); pp_it++) {
	    int pind = (*pp_it);
	    ON_3dPoint *p = (*s_cdt->bot_pnt_to_on_pnt)[pind];
	    if (!p) {
		bu_log("unmapped point??? %d\n", pind);
	    } else {
		struct cdt_audit_info *paudit = (*s_cdt->pnt_audit_info)[p];
		if (!paudit) {
		    bu_log("point with no audit info??? %d\n", pind);
		} else {
		    bu_log("point %d: Face(%d) Vert(%d) Trim(%d) Edge(%d) UV(%f,%f)\n", pind, paudit->face_index, paudit->vert_index, paudit->trim_index, paudit->edge_index, paudit->surf_uv.x, paudit->surf_uv.y);
		}
	    }
	}
    }
    if (se->misoriented.count > 0) {
	std::set<int> problem_pnts;
	std::set<int>::iterator pp_it;

	bu_log("%d misoriented edges\n", se->misoriented.count);
	for (int i = 0; i < se->misoriented.count; i++) {
	    int v1 = se->misoriented.edges[i*2];
	    int v2 = se->misoriented.edges[i*2+1];
	    bu_log("%d->%d: %f %f %f->%f %f %f\n", v1, v2,
		    valid_vertices[v1*3], valid_vertices[v1*3+1], valid_vertices[v1*3+2],
		    valid_vertices[v2*3], valid_vertices[v2*3+1], valid_vertices[v2*3+2]
		  );
	    for (int j = 0; j < valid_fcnt; j++) {
		std::set<std::pair<int,int>> fedges;
		fedges.insert(std::pair<int,int>(valid_faces[j*3],valid_faces[j*3+1]));
		fedges.insert(std::pair<int,int>(valid_faces[j*3+1],valid_faces[j*3+2]));
		fedges.insert(std::pair<int,int>(valid_faces[j*3+2],valid_faces[j*3]));
		int has_edge = (fedges.find(std::pair<int,int>(v1,v2)) != fedges.end()) ? 1 : 0;
		if (has_edge) {
		    int face = j;
		    bu_log("eface %d: %d %d %d :  %f %f %f->%f %f %f->%f %f %f \n", face, valid_faces[face*3], valid_faces[face*3+1], valid_faces[face*3+2],
			    valid_vertices[valid_faces[face*3]*3], valid_vertices[valid_faces[face*3]*3+1],valid_vertices[valid_faces[face*3]*3+2],
			    valid_vertices[valid_faces[face*3+1]*3], valid_vertices[valid_faces[face*3+1]*3+1],valid_vertices[valid_faces[face*3+1]*3+2],
			    valid_vertices[valid_faces[face*3+2]*3], valid_vertices[valid_faces[face*3+2]*3+1],valid_vertices[valid_faces[face*3+2]*3+2]);
		    problem_pnts.insert(valid_faces[j*3]);
		    problem_pnts.insert(valid_faces[j*3+1]);
		    problem_pnts.insert(valid_faces[j*3+2]);
		}
	    }
	}
	for (pp_it = problem_pnts.begin(); pp_it != problem_pnts.end(); pp_it++) {
	    int pind = (*pp_it);
	    ON_3dPoint *p = (*s_cdt->bot_pnt_to_on_pnt)[pind];
	    if (!p) {
		bu_log("unmapped point??? %d\n", pind);
	    } else {
		struct cdt_audit_info *paudit = (*s_cdt->pnt_audit_info)[p];
		if (!paudit) {
		    bu_log("point with no audit info??? %d\n", pind);
		} else {
		    bu_log("point %d: Face(%d) Vert(%d) Trim(%d) Edge(%d) UV(%f,%f)\n", pind, paudit->face_index, paudit->vert_index, paudit->trim_index, paudit->edge_index, paudit->surf_uv.x, paudit->surf_uv.y);
		}
	    }
	}
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

