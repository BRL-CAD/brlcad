/*            C D T _ O V L P S _ G R P S . C P P
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
 * This file is logic specifically for refining meshes to clear overlaps
 * between sets of triangles in different meshes that need to share a common
 * tessellation.
 *
 */

#include "common.h"
#include <iostream>
#include <numeric>
#include <queue>
#include <random>
#include <string>
#include "bu/str.h"
#include "bg/chull.h"
#include "bg/tri_pt.h"
#include "bg/tri_tri.h"

#include "./mesh.h"
#include "./cdt.h"

void
ovlp_grp::plot(const char *fname, int ind)
{

    omesh_t *om = (!ind) ? om1 : om2;
    std::set<triangle_t> &tris = (!ind) ? tris1 : tris2;

    struct bu_color c = BU_COLOR_INIT_ZERO;
    unsigned char rgb[3] = {0,0,255};

    // Randomize the blue so (usually) subsequent
    // draws of different meshes won't erase the
    // previous wireframe
    std::random_device rd;
    std::mt19937 reng(rd());
    std::uniform_int_distribution<> rrange(100,250);
    int brand = rrange(reng);
    rgb[2] = (unsigned char)brand;

    FILE *plot = fopen(fname, "w");

    RTree<size_t, double, 3>::Iterator tree_it;
    size_t t_ind;
    triangle_t tri;

    bu_color_from_rgb_chars(&c, (const unsigned char *)rgb);

    om->fmesh->tris_tree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	t_ind = *tree_it;
	tri = om->fmesh->tris_vect[t_ind];
	om->fmesh->plot_tri(tri, &c, plot, 255, 0, 0);
	++tree_it;
    }

    double tri_r = 0;

    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot, &c);
    std::set<triangle_t>::iterator ts_it;
    for (ts_it = tris.begin(); ts_it != tris.end(); ts_it++) {
	tri = *ts_it;
	double tr = om->fmesh->tri_pnt_r(tri.ind);
	tri_r = (tr > tri_r) ? tr : tri_r;
	om->fmesh->plot_tri(tri, &c, plot, 255, 0, 0);
    }

    fclose(plot);
}

void
ovlp_grp::plot(const char *fname)
{
    struct bu_vls omname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&omname, "%s-0.plot3", fname);
    plot(bu_vls_cstr(&omname), 0);
    bu_vls_sprintf(&omname, "%s-1.plot3", fname);
    plot(bu_vls_cstr(&omname), 1);
    bu_vls_free(&omname);
}



// Validate that all triangles in tris are still in the fmesh - if
// not, remove them so we don't try to process them.
bool
ovlp_grp::validate()
{
    // Once we're invalid, no point in doing the work again
    if (state == -1) {
	return false;
    }

    // Assume valid until proven otherwise...
    state = 1;

    std::set<triangle_t> etris;
    std::set<triangle_t>::iterator tr_it;
    for (tr_it = tris1.begin(); tr_it != tris1.end(); tr_it++) {
	if (!om1->fmesh->tri_active(tr_it->ind)) {
	    etris.insert(*tr_it);
	}
    }
    for (tr_it = etris.begin(); tr_it != etris.end(); tr_it++) {
	tris1.erase(*tr_it);
    }

    etris.clear();
    for (tr_it = tris2.begin(); tr_it != tris2.end(); tr_it++) {
	if (!om2->fmesh->tri_active(tr_it->ind)) {
	    etris.insert(*tr_it);
	}
    }
    for (tr_it = etris.begin(); tr_it != etris.end(); tr_it++) {
	tris2.erase(*tr_it);
    }

    if (!tris1.size() || !tris2.size()) {
	state = -1;
	return false;
    }

    return true;
}

bool
ovlp_grp::fit_plane()
{
    std::set<long> verts1;
    std::set<long> verts2;

    // Collect the vertices
    std::set<triangle_t>::iterator tr_it;
    for (tr_it = tris1.begin(); tr_it != tris1.end(); tr_it++) {
	for (int i = 0; i < 3; i++) {
	    verts1.insert(tr_it->v[i]);
	}
    }
    for (tr_it = tris2.begin(); tr_it != tris2.end(); tr_it++) {
	for (int i = 0; i < 3; i++) {
	    verts2.insert(tr_it->v[i]);
	}
    }

    if (!verts1.size() || !verts2.size()) {
	return false;
    }

    std::set<ON_3dPoint> plane1_pnts;
    std::set<ON_3dPoint> plane2_pnts;
    std::set<ON_3dPoint> plane_pnts;
    std::set<long>::iterator vp_it;
    for (vp_it = verts1.begin(); vp_it != verts1.end(); vp_it++) {
	ON_3dPoint p = *om1->fmesh->pnts[*vp_it];
	plane1_pnts.insert(p);
	plane_pnts.insert(p);
    }
    for (vp_it = verts2.begin(); vp_it != verts2.end(); vp_it++) {
	ON_3dPoint p = *om2->fmesh->pnts[*vp_it];
	plane2_pnts.insert(p);
	plane_pnts.insert(p);
    }

    ON_Plane fplane1;
    {
	point_t pcenter;
	vect_t pnorm;
	point_t *fpnts = (point_t *)bu_calloc(plane1_pnts.size(), sizeof(point_t), "fitting points");
	std::set<ON_3dPoint>::iterator p_it;
	int pcnt = 0;
	for (p_it = plane1_pnts.begin(); p_it != plane1_pnts.end(); p_it++) {
	    fpnts[pcnt][X] = p_it->x;
	    fpnts[pcnt][Y] = p_it->y;
	    fpnts[pcnt][Z] = p_it->z;
	    pcnt++;
	}
	if (bn_fit_plane(&pcenter, &pnorm, plane1_pnts.size(), fpnts)) {
	    std::cout << "fitting plane failed!\n";
	}
	bu_free(fpnts, "fitting points");
	fplane1 = ON_Plane(pcenter, pnorm);
    }
    fp1 = fplane1;

    ON_Plane fplane2;
    {
	point_t pcenter;
	vect_t pnorm;
	point_t *fpnts = (point_t *)bu_calloc(plane2_pnts.size(), sizeof(point_t), "fitting points");
	std::set<ON_3dPoint>::iterator p_it;
	int pcnt = 0;
	for (p_it = plane2_pnts.begin(); p_it != plane2_pnts.end(); p_it++) {
	    fpnts[pcnt][X] = p_it->x;
	    fpnts[pcnt][Y] = p_it->y;
	    fpnts[pcnt][Z] = p_it->z;
	    pcnt++;
	}
	if (bn_fit_plane(&pcenter, &pnorm, plane2_pnts.size(), fpnts)) {
	    std::cout << "fitting plane failed!\n";
	}
	bu_free(fpnts, "fitting points");
	fplane2 = ON_Plane(pcenter, pnorm);
    }
    fp2 = fplane2;

    double pangle = ang_deg(fplane1.Normal(), fplane2.Normal());
    if (pangle > 30) {
	// If the planes are too far from parallel, we're done - there is no
	// sensible common plane for projections.
	separate_planes = true;
	return true;
    }
    separate_planes = false;

    ON_Plane fplane;
    {
	point_t pcenter;
	vect_t pnorm;
	point_t *fpnts = (point_t *)bu_calloc(plane_pnts.size(), sizeof(point_t), "fitting points");
	std::set<ON_3dPoint>::iterator p_it;
	int pcnt = 0;
	for (p_it = plane_pnts.begin(); p_it != plane_pnts.end(); p_it++) {
	    fpnts[pcnt][X] = p_it->x;
	    fpnts[pcnt][Y] = p_it->y;
	    fpnts[pcnt][Z] = p_it->z;
	    pcnt++;
	}
	if (bn_fit_plane(&pcenter, &pnorm, plane_pnts.size(), fpnts)) {
	    std::cout << "fitting plane failed!\n";
	}
	bu_free(fpnts, "fitting points");
	fplane = ON_Plane(pcenter, pnorm);
    }

    fp1 = fplane;
    fp2 = fplane;

    return true;
}

// TODO - the optimize calls here are calling the mesh repair subroutines, which don't
// (yet) have an option for allowing the specification of an explicit plane when
// building the initial polygon loop - it's calculating that plane from the triangle.
// That won't give us our shared plane needed for this step, so going to have to
// generalize that routine to allow an optional externally supplied projection plane.
bool
ovlp_grp::optimize()
{
    // Clear out anything stale
    if (!validate()) return false;

    double ang1 = 1.0;
    double ang2 = 1.0;

    if (tris1.size() > 1 || tris2.size() > 2) {
	fit_plane();
    }

    if (tris1.size() > 1) {
	ang1 = om1->fmesh->max_tri_angle(fp1, tris1);
	std::cout << "ang1: " << ang1 << "\n";
    }
    om1->fmesh->optimize(tris1, fp1);

    if (om1->fmesh->new_tris.size()) {
	tris1 = om1->fmesh->new_tris;
	// Need to update vertex bboxes after changing triangles!
	std::set<triangle_t>::iterator n_it;
	std::set<overt_t *> tri_verts;
	for (n_it = om1->fmesh->new_tris.begin(); n_it != om1->fmesh->new_tris.end(); n_it++) {
	    for (int i = 0; i < 3; i++) {
		tri_verts.insert(om1->overts[(*n_it).v[i]]);
	    }
	}
	std::set<overt_t *>::iterator o_it;
	for (o_it = tri_verts.begin(); o_it != tri_verts.end(); o_it++) {
	    (*o_it)->update();
	}
    }

    if (tris2.size() > 1) {
	ang2 = om2->fmesh->max_tri_angle(fp2, tris2);
	std::cout << "ang2: " << ang2 << "\n";
    }
    om2->fmesh->optimize(tris2, fp2);

    if (om2->fmesh->new_tris.size()) {
	tris2 = om2->fmesh->new_tris;
	// Need to update vertex bboxes after changing triangles!
	std::set<triangle_t>::iterator n_it;
	std::set<overt_t *> tri_verts;
	for (n_it = om2->fmesh->new_tris.begin(); n_it != om2->fmesh->new_tris.end(); n_it++) {
	    for (int i = 0; i < 3; i++) {
		tri_verts.insert(om2->overts[(*n_it).v[i]]);
	    }
	}
	std::set<overt_t *>::iterator o_it;
	for (o_it = tri_verts.begin(); o_it != tri_verts.end(); o_it++) {
	    (*o_it)->update();
	}
    }

    if (!overlapping()) {
	state = 2;
    }

    return true;
}

// TODO - If we have edges that have non-aligned vertices, should probably
// split those as well - interiors by itself doesn't look like it will be
// enough.
std::set<uedge_t>
ovlp_grp::interior_uedges(int ind)
{
    std::set<triangle_t> &tris = (!ind) ? tris1 : tris2;

    std::map<uedge_t, int> ecnts;
    std::set<triangle_t>::iterator t_it;
    for (t_it = tris.begin(); t_it != tris.end(); t_it++) {
	triangle_t tri = *t_it;
	std::set<uedge_t> tedges = tri.uedges();
	std::set<uedge_t>::iterator u_it;
	for (u_it = tedges.begin(); u_it != tedges.end(); u_it++) {
	    ecnts[*u_it]++;
	}
    }

    std::set<uedge_t> iedges;
    std::map<uedge_t, int>::iterator m_it;
    for (m_it = ecnts.begin(); m_it != ecnts.end(); m_it++) {
	if (m_it->second > 1) {
	    // Two triangles used this edge in the group - it is interior
	    uedge_t iedge = m_it->first;
	    iedges.insert(iedge);
	}
    }

    return iedges;
}

bool
ovlp_grp::split_interior_uedges()
{
    std::set<uedge_t> u0 = interior_uedges(0);
    std::set<uedge_t> u1 = interior_uedges(1);
    std::set<uedge_t> u = (u0.size() > u1.size()) ? u0 : u1;

    std::cout << "have " << u.size() << " interior edges\n";

    return true;
}


/* A grouping of overlapping triangles is defined as the set of triangles from
 * two meshes that overlap and are surrounded in both meshes by either
 * non-involved triangles or face boundary edges.  Visually, these are "clusters"
 * of overlapping triangles on the mesh surfaces. */
std::vector<ovlp_grp>
find_ovlp_grps(
	std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs
	)
{
    omesh_ovlps(check_pairs, 1);

    std::vector<ovlp_grp> bins;

    std::map<std::pair<omesh_t *, size_t>, size_t> bin_map;

    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first->omesh;
	omesh_t *omesh2 = cp_it->second->omesh;
	if (!omesh1->intruding_tris.size() || !omesh2->intruding_tris.size()) {
	    continue;
	}
	std::set<size_t> om1_tris = omesh1->intruding_tris;
	while (om1_tris.size()) {
	    size_t t1 = *om1_tris.begin();
triangle_t tri1 = omesh1->fmesh->tris_vect[t1];
	    std::pair<omesh_t *, size_t> key1(omesh1, t1);
	    om1_tris.erase(t1);
	    ON_BoundingBox tri_bb = omesh1->fmesh->tri_bbox(t1);
	    std::set<size_t> ntris = omesh2->tris_search(tri_bb);
	    std::set<size_t>::iterator nt_it;
	    for (nt_it = ntris.begin(); nt_it != ntris.end(); nt_it++) {
		triangle_t tri2 = omesh2->fmesh->tris_vect[*nt_it];

		int real_ovlp = tri_isect(tri1, tri2, 1);
		if (!real_ovlp) continue;

		std::pair<omesh_t *, size_t> key2(omesh2, *nt_it);
		size_t key_id;
		if (bin_map.find(key1) == bin_map.end() && bin_map.find(key2) == bin_map.end()) {
		    // New group
		    ovlp_grp ngrp(key1.first, key2.first);
		    ngrp.add_tri(tri1);
		    ngrp.add_tri(tri2);
		    bins.push_back(ngrp);
		    key_id = bins.size() - 1;
		    bin_map[key1] = key_id;
		    bin_map[key2] = key_id;
		} else {
		    if (bin_map.find(key1) != bin_map.end()) {
			key_id = bin_map[key1];
			bins[key_id].add_tri(tri2);
			bin_map[key2] = key_id;
		    } else {
			key_id = bin_map[key2];
			bins[key_id].add_tri(tri1);
			bin_map[key1] = key_id;
		    }
		}
	    }
	}
    }

    return bins;
}

bool
ovlp_grp::overlapping()
{
    // For now, just do the naive comparison without acceleration
    // structures - if these sets get big it might be worth building
    // rtrees specific to the group tris.

    std::set<triangle_t>::iterator i_it, j_it;
    for (i_it = tris1.begin(); i_it != tris1.end(); i_it++) {
	triangle_t tri1 = *i_it;
	for (j_it = tris2.begin(); j_it != tris2.end(); j_it++) {
	    triangle_t tri2 = *j_it;
	    if (tri_isect(tri1, tri2, 1)) {
		return true;
	    }
	}
    }

    return false;
}

bool
ovlp_grp::aligned_verts_check()
{
    std::set<overt_t *> overts1;
    std::set<overt_t *> overts2;

    std::set<triangle_t>::iterator t_it;
    for (t_it = tris1.begin(); t_it != tris1.end(); t_it++) {
	for (int i = 0; i < 3; i++) {
	    overt_t *cv = om1->overts[t_it->v[i]];
	    overts1.insert(cv);
	}
    }

    for (t_it = tris2.begin(); t_it != tris2.end(); t_it++) {
	for (int i = 0; i < 3; i++) {
	    overt_t *cv = om2->overts[t_it->v[i]];
	    overts2.insert(cv);
	}
    }

    if (overts1.size() != 4 || overts2.size() != 4) {
	return false;
    }

    std::set<overt_t *> unmapped1 = overts1;
    std::set<overt_t *> unmapped2 = overts2;
    std::set<overt_t *>::iterator v_it;
    for (v_it = overts1.begin(); v_it != overts1.end(); v_it++) {
	overt_t *ov = *v_it;
	overt_t *ovc = om2->vert_closest(NULL, ov);
	if (overts2.find(ovc) != overts2.end()) {
	    p2p[ov->p_id] = ovc->p_id;
	    unmapped1.erase(ov);
	    unmapped2.erase(ovc);
	}
    }

    if (unmapped1.size() || unmapped2.size()) {
	return false;
    }

    return true;
}

bool
ovlp_grp::pairs_realign()
{
    // If we don't have exactly 2 triangles in each of the grp sets, we can't do this
    if (tris1.size() != 2 || tris2.size() != 2) {
	return false;
    }

    // If there's anything stale in the sets, it needs to wait for the next round
    if (!validate()) return false;

    // If the nearest vertex to every vertex in one triangle set isn't in the
    // other triangle vertex set, we can't do this.
    if (!aligned_verts_check()) {
	return false;
    }

    std::set<triangle_t>::iterator t_it;
    for (t_it = tris2.begin(); t_it != tris2.end(); t_it++) {
	triangle_t otri = *t_it;
	om2->fmesh->tri_remove(otri);
    }
    for (t_it = tris1.begin(); t_it != tris1.end(); t_it++) {
	triangle_t ntri;
	ntri.v[0] = p2p[(*t_it).v[0]];
	ntri.v[1] = p2p[(*t_it).v[1]];
	ntri.v[2] = p2p[(*t_it).v[2]];
	ntri.m = om2->fmesh;
	orient_tri(*om2->fmesh, ntri);
	om2->fmesh->tri_add(ntri);
	tris2.insert(ntri);
    }

    if (!overlapping()) {
	state = 2;
    }

    return true;
}

#if 0
static void
plot_bins(std::vector<ovlp_grp> &bins) {
    for (size_t i = 0; i < bins.size(); i++) {
	struct bu_vls pname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&pname, "group_%zu", i);
	bins[i].plot(bu_vls_cstr(&pname));
	bu_vls_free(&pname);
    }
}
#endif

bool
find_split_edges(std::set<uedge_t> &interior_uedges, std::set<bedge_seg_t *> &bsegs,
       	std::set<triangle_t> &tris, std::set<long> &active_verts, double lthresh)
{
    std::set<triangle_t>::iterator t_it;
    std::set<uedge_t>::iterator u_it;
    if (!tris.size()) {
	return false;
    }
    omesh_t *om = tris.begin()->m->omesh;

    std::set<uedge_t> split_uedges_init;
    std::set<uedge_t> split_uedges_skip;
    std::set<uedge_t> split_uedges;
    // Collect the set of seed uedges to split.
    for (t_it = tris.begin(); t_it != tris.end(); t_it++) {
	triangle_t t = *t_it;
	if (!om->fmesh->tri_active(t.ind)) {
	    std::cout << "Stale triangle in " << om->fmesh->f_id << ": " << t.ind << "\n";
	    continue;
	}
	for (int i = 0; i < 3; i++) {
	    if (t.uedge_len(i) > lthresh) {
		uedge_t ue = t.uedge(i);
		int acnt = 0;
		if (active_verts.find(ue.v[0]) != active_verts.end()) acnt++;
		if (!acnt && active_verts.find(ue.v[1]) != active_verts.end()) acnt++;
		if (!acnt) continue;
		split_uedges_init.insert(ue);
	    }
	}
	// In addition to raw length, if we have any triangles
	// with a longest edge much larger than the shortest edge,
	// split the longer edges.  Remove the shortest edge from
	// splitting, if present.
	double ledge = t.longest_edge_len();
	double sedge = t.shortest_edge_len();
	if (ledge > 10*sedge && sedge > .001*lthresh) {
	    for (int i = 0; i < 3; i++) {
		uedge_t ue = t.uedge(i);
		double uel = t.uedge_len(i);
		if (uel > 2*sedge) {
		    split_uedges_init.insert(ue);
		    active_verts.insert(ue.v[0]);
		    active_verts.insert(ue.v[1]);
		} else {
		    if (uel < lthresh) {
			split_uedges_skip.insert(ue);
		    }
		}
	    }
	}
    }
    std::set<uedge_t>::iterator s_ue;
    for (s_ue = split_uedges_init.begin(); s_ue != split_uedges_init.end(); s_ue++) {
	if (split_uedges_skip.find(*s_ue) == split_uedges_skip.end()) {
	    split_uedges.insert(*s_ue);
	}
    }

    std::cout << om->fmesh->name << "," << om->fmesh->f_id << ": found " << split_uedges.size() << " edges to split\n";

    // If any of these are brep face edge segments, pull them out for
    // separate processing.
    std::set<bedge_seg_t *> nbsegs;
    std::set<uedge_t> c_uedges;
    for (s_ue = split_uedges.begin(); s_ue != split_uedges.end(); s_ue++) {
	if (om->fmesh->brep_edges.find(*s_ue) != om->fmesh->brep_edges.end()) {
	    bedge_seg_t *bseg = om->fmesh->ue2b_map[*s_ue];
	    nbsegs.insert(bseg);
	} else {
	    c_uedges.insert(*s_ue);
	    std::set<size_t> rtris = om->fmesh->uedges2tris[*s_ue];
	    if (rtris.size() != 2) {
		std::cout << "Error - could not associate uedge with two triangles??\n";
	    }
	}
    }

    std::cout << om->fmesh->name << "," << om->fmesh->f_id << ": found " << c_uedges.size() << " interior edges to split\n";
    std::cout << om->fmesh->name << "," << om->fmesh->f_id << ": found " << nbsegs.size() << " brep face edges to split\n";

    size_t cnt = 0;
    for (u_it = c_uedges.begin(); u_it != c_uedges.end(); u_it++) {
	if (cnt >= c_uedges.size()) {
	    std::cout << "huh???\n";
	}
	interior_uedges.insert(*u_it);
	cnt++;
    }
    std::set<bedge_seg_t *>::iterator n_it;
    for (n_it = nbsegs.begin(); n_it != nbsegs.end(); n_it++) {
	bsegs.insert(*n_it);
    }

    return (c_uedges.size() || nbsegs.size());
}

void
split_bins(std::vector<ovlp_grp> &bins, double lthresh)
{
    std::map<omesh_t *, std::set<long>> active_verts;

    bool do_split = true;

    std::map<omesh_t *, std::set<triangle_t>> grp_tris;
    std::map<omesh_t *, std::set<triangle_t>>::iterator g_it;

    // For each face, gather up all the remaining triangles.
    for(size_t i = 0; i < bins.size(); i++) {
	std::set<triangle_t>::iterator t_it;
	for (t_it = bins[i].tris1.begin(); t_it != bins[i].tris1.end(); t_it++) {
	    triangle_t t = *t_it;
	    if (!bins[i].om1->fmesh->tri_active(t.ind)) {
		std::cout << "Stale triangle in tris1, bin " << i << ": " << bins[i].om1->fmesh->f_id << "," << t.ind << "\n";
		continue;
	    }
	    grp_tris[bins[i].om1].insert(t);
	}
	for (t_it = bins[i].tris2.begin(); t_it != bins[i].tris2.end(); t_it++) {
	    triangle_t t = *t_it;
	    if (!bins[i].om2->fmesh->tri_active(t.ind)) {
		std::cout << "Stale triangle in tris2, bin " << i << ": " << bins[i].om2->fmesh->f_id << "," << t.ind << "\n";
		continue;
	    }
	    grp_tris[bins[i].om2].insert(t);
	}
    }

    // Collect the set of original vertices involved with these triangles.
    // If a triangle edge doesn't use either these vertices or new vertices
    // introduced by splitting, don't split it - this localize the splitting
    // behavior to the original overlapping bin triangle areas.
    for (g_it = grp_tris.begin(); g_it != grp_tris.end(); g_it++) {
	std::set<triangle_t>::iterator t_it;
	for (t_it = g_it->second.begin(); t_it != g_it->second.end(); t_it++) {
	    for (int i = 0; i < 3; i++) {
		active_verts[t_it->m->omesh].insert(t_it->v[i]);
	    }
	}
    }

    int icnt = 0;
    while (do_split == true) {
	if (icnt > 4) {
	    break;
	}
	std::map<omesh_t *, std::set<uedge_t>> iedges;
	std::set<bedge_seg_t *> bedges;
	iedges.clear();
	bedges.clear();

	do_split = false;
	for (g_it = grp_tris.begin(); g_it != grp_tris.end(); g_it++) {
	    omesh_t *om = g_it->first;
	    std::set<triangle_t> &mtris = g_it->second;
	    std::set<uedge_t> &uedges= iedges[om];
	    bool have_split_edges = find_split_edges(uedges, bedges, mtris, active_verts[om], lthresh);
	    mtris.clear();
	    if (have_split_edges) {
		// We're changing a mesh, so we'll need to come back for another round.
		do_split = true;
	    }
	}
	// Split brep edges
#if 0
	std::set<bedge_seg_t *>::iterator b_it;
	for (b_it = bedges.begin(); b_it != bedges.end(); b_it++) {
	    overt_t *nv1, *nv2;
	    std::set<bedge_seg_t *> nbsegs;
	    bedge_seg_t *eseg = *b_it;
	    double t = eseg->edge_start + (eseg->edge_end - eseg->edge_start)*0.5;
	    if (ovlp_split_edge(&nv1, &nv2, &nbsegs, eseg, t)) {
		active_verts[nv1->omesh].insert(nv1->p_id);
		active_verts[nv2->omesh].insert(nv2->p_id);
		// TODO - get triangles and add them
	    }
	}
#endif
	// Split interior edges
	std::map<omesh_t *, std::set<uedge_t>>::iterator i_it;
	for (i_it = iedges.begin(); i_it != iedges.end(); i_it++) {
	    omesh_t *om = i_it->first;
	    std::string pname = std::string(om->fmesh->name) + std::string("_") + std::to_string(icnt) + std::string(".plot3");
	    om->fmesh->tris_plot(pname.c_str());
	    std::set<uedge_t> &uedges = i_it->second;
	    std::cout << om->fmesh->name << "," << om->fmesh->f_id << ": have " << uedges.size() << " interior edges to split\n";
	    std::set<uedge_t>::iterator u_it;
	    std::set<long> ntris;
	    size_t lcnt = 0;
	    for (u_it = uedges.begin(); u_it != uedges.end(); u_it++) {
		if (om->fmesh->uedges2tris[*u_it].size() != 2) {
		    std::cout << "stale uedge?\n";
		    lcnt++;
		    if (lcnt > uedges.size()+1) {
			std::cout << "infinite loop?\n";
			break;
		    }
		    continue;
		}
		overt_t *nv;
		uedge_t ue = *u_it;
		if (ovlp_split_interior_edge(&nv, ntris, om, ue) > 0) {
		    active_verts[nv->omesh].insert(nv->p_id);
		}
	    }
	    // Multiple edge replacements may remove new triangles immediately
	    // after they are introduced into the mesh.  Now that we've processed
	    // the full edge set, we have a "stable" set of new triangles to record
	    // for the next round of processing.
	    std::set<long>::iterator n_it;
	    for (n_it = ntris.begin(); n_it != ntris.end(); n_it++) {
		grp_tris[om].insert(om->fmesh->tris_vect[*n_it]);
	    }
	}
	icnt++;
    }
}

void
resolve_ovlp_grps(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs, double lthresh)
{
    std::vector<ovlp_grp> bins;

#if 0
    size_t bin_cnt = INT_MAX;
    int cycle_cnt = 0;
    while (bin_cnt && cycle_cnt < 5) {
	bins = find_ovlp_grps(check_pairs);
	bin_cnt = bins.size();

	plot_bins(bins);

	for (size_t i = 0; i < bins.size(); i++) {
	    bool v1 = bins[i].om1->fmesh->valid(1);
	    bool v2 = bins[i].om2->fmesh->valid(1);

	    if (!v1 || !v2) {
		std::cout << "Starting bin processing with invalid inputs!\n";
	    }

	    std::cout << "Check for pairs in bin " << i << "\n";
	    bins[i].pairs_realign();
	}

	for (size_t i = 0; i < bins.size(); i++) {
	    bool v1 = bins[i].om1->fmesh->valid(1);
	    bool v2 = bins[i].om2->fmesh->valid(1);

	    if (!v1 || !v2) {
		std::cout << "Starting bin processing with invalid inputs!\n";
	    }

	    if (bins[i].state == 1) {
		std::cout << "Processing bin " << i << "\n";
		bins[i].optimize();
	    }
	}

	for (size_t i = 0; i < bins.size(); i++) {
	    bool v1 = bins[i].om1->fmesh->valid(1);
	    bool v2 = bins[i].om2->fmesh->valid(1);

	    if (!v1 || !v2) {
		std::cout << "Starting bin processing with invalid inputs!\n";
	    }

	    if (bins[i].state == 1) {
		std::cout << "Interior edges: bin " << i << "\n";
		bins[i].split_interior_uedges();
	    }
	}

	bool have_invalid = false;
	bool have_unresolved = false;
	for (size_t i = 0; i < bins.size(); i++) {
	    if (bins[i].state == -1) {
		have_invalid = true;
	    }
	    if (bins[i].state == 1) {
		have_unresolved = true;
	    }
	}
	if (!have_invalid && !have_unresolved) {
	    break;
	}

	plot_active_omeshes(check_pairs);
	cycle_cnt++;
    }
#endif
    bins = find_ovlp_grps(check_pairs);
    split_bins(bins, lthresh);

    plot_active_omeshes(check_pairs);
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

