/*                   C D T _ O V L P S . C P P
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
 * This file is logic specifically for refining meshes to clear
 * overlaps between sets of objects.
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

/*******************************************
 * Temporary debugging utilities
 ******************************************/
void
ovbbp(ON_BoundingBox &bb) {
    FILE *plot = fopen ("bb.plot3", "w");
    ON_BoundingBox_Plot(plot, bb);
    fclose(plot);
}


#define PPOINT 3.05501831768742305,7.50007628741969601,23.99999799973181069
bool
PPCHECK(ON_3dPoint &p)
{
    ON_3dPoint problem(PPOINT);
    if (problem.DistanceTo(p) < 0.01) {
	std::cout << "Saw problem point\n";
	return true;
    }
    return false;
}

bool
VPCHECK(overt_t *v, const char *fname)
{
    if (fname && !BU_STR_EQUAL(v->omesh->fmesh->name, fname)) {
	return false;
    }
    ON_3dPoint p = v->vpnt();
    if (PPCHECK(p)) {
	std::cout << "Saw problem vert: " << v->omesh->fmesh->name << "-" << v->omesh->fmesh->f_id << "\n";
	return true;
    }
    return false;
}

/******************************************/
/*          overt implementation          */
/******************************************/

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

void
omesh_t::plot_vtree(const char *fname)
{
    FILE *plot = fopen(fname, "w");
    RTree<long, double, 3>::Iterator tree_it;
    long v_ind;
    vtree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	v_ind = *tree_it;
	pl_color(plot, 255, 0, 0);
	overts[v_ind]->plot(plot);
	++tree_it;
    }
    fclose(plot);
}

/******************************************/
/*          omesh implementation          */
/******************************************/

std::string
omesh_t::sname()
{
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh->p_cdt;
    return std::string(s_cdt->name);
}

bool
omesh_t::validate_vtree()
{
    RTree<long, double, 3>::Iterator tree_it;
    long v_ind;
    vtree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	v_ind = *tree_it;
	overt_t *ov = overts[v_ind];
	ON_3dPoint vp = ov->vpnt();
	std::set<overt_t *> search_vert_bb = vert_search(ov->bb);
	if (!search_vert_bb.size()) {
	    std::cout << "Error: no nearby verts for vert bb " << v_ind << "??\n";
	    return false;
	}
	if (search_vert_bb.find(ov) == search_vert_bb.end()) {
	    std::cout << "Error: vert in tree, but bb search couldn't find: " << v_ind << "\n";
	    std::set<overt_t *> s2 = vert_search(ov->bb);
	    if (s2.find(ov) == s2.end()) {
		std::cout << "Second try didn't work: " << v_ind << "\n";
	    }
	    return false;
	}

	ON_BoundingBox pbb(vp, vp);
	std::set<overt_t *> search_vert = vert_search(pbb);
	if (!search_vert.size()) {
	    std::cout << "Error: vert point not contained by tree box?? " << v_ind << "??\n";
	    std::set<overt_t *> sv2 = vert_search(pbb);
	    if (sv2.find(ov) == sv2.end()) {
		std::cout << "Second try didn't work: " << v_ind << "\n";
	    }
	    return false;
	}

	++tree_it;
    }
    return true;
}

std::set<std::pair<long, long>>
omesh_t::vert_ovlps(omesh_t *other)
{
    std::set<std::pair<long, long>> vert_pairs;
    vert_pairs.clear();
    vtree.Overlaps(other->vtree, &vert_pairs);
    return vert_pairs;
}

void
omesh_t::plot(const char *fname,
       	std::map<bedge_seg_t *, std::set<overt_t *>> *ev
	)
{
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

    pl_color(plot, 0, 0, 255);
    fmesh->tris_tree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	t_ind = *tree_it;
	tri = fmesh->tris_vect[t_ind];
	fmesh->plot_tri(tri, &c, plot, 255, 0, 0);
	++tree_it;
    }

    double tri_r = 0;

    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot, &c);
    std::set<size_t>::iterator ts_it;
    for (ts_it = intruding_tris.begin(); ts_it != intruding_tris.end(); ts_it++) {
	tri = fmesh->tris_vect[*ts_it];
	double tr = fmesh->tri_pnt_r(tri.ind);
	tri_r = (tr > tri_r) ? tr : tri_r;
	fmesh->plot_tri(tri, &c, plot, 255, 0, 0);
    }

    std::map<overt_t *, std::set<long>>::iterator iv_it;


    pl_color(plot, 255, 0, 0);
    for (iv_it = refinement_overts.begin(); iv_it != refinement_overts.end(); iv_it++) {
	overt_t *iv = iv_it->first;
	ON_3dPoint vp = iv->vpnt();
	plot_pnt_3d(plot, &vp, tri_r, 0);
    }

    if (ev) {
	std::map<bedge_seg_t *, std::set<overt_t *>>::iterator ev_it;
	pl_color(plot, 128, 0, 255);
	for (ev_it = ev->begin(); ev_it != ev->end(); ev_it++) {
	    bedge_seg_t *eseg = ev_it->first;
	    struct ON_Brep_CDT_State *s_cdt_edge = (struct ON_Brep_CDT_State *)eseg->p_cdt;
	    int f_id1 = s_cdt_edge->brep->m_T[eseg->tseg1->trim_ind].Face()->m_face_index;
	    int f_id2 = s_cdt_edge->brep->m_T[eseg->tseg2->trim_ind].Face()->m_face_index;
	    cdt_mesh_t &fmesh_f1 = s_cdt_edge->fmeshes[f_id1];
	    cdt_mesh_t &fmesh_f2 = s_cdt_edge->fmeshes[f_id2];
	    omesh_t *o1 = fmesh_f1.omesh;
	    omesh_t *o2 = fmesh_f2.omesh;
	    if (o1 == this || o2 == this) {
		std::set<overt_t *>::iterator v_it;
		for (v_it = ev_it->second.begin(); v_it != ev_it->second.end(); v_it++) {
		    overt_t *iv = *v_it;
		    if (iv->omesh == this) continue;
		    ON_3dPoint vp = iv->vpnt();
		    plot_pnt_3d(plot, &vp, tri_r, 0);
		}
	    }
	}
    }


    fclose(plot);
}

void
omesh_t::plot(std::map<bedge_seg_t *, std::set<overt_t *>> *ev)
{
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh->p_cdt;
    bu_vls_sprintf(&fname, "%s-%d_ovlps.plot3", s_cdt->name, fmesh->f_id);
    plot(bu_vls_cstr(&fname), ev);
    bu_vls_free(&fname);
}

void
omesh_t::plot()
{
    plot(NULL);
}

overt_t *
omesh_t::vert_closest(double *vdist, overt_t *v)
{
    ON_BoundingBox fbbox = v->omesh->fmesh->bbox();
    ON_3dPoint opnt = v->bb.Center();

    // Find close verts, iterate through them and
    // find the closest
    ON_BoundingBox s_bb = v->bb;
    ON_3dPoint p = v->vpnt();
    std::set<overt_t *> nverts = vert_search(s_bb);
    bool last_try = false;
    while (!nverts.size() && !last_try) {
	ON_3dVector vmin = s_bb.Min() - s_bb.Center();
	ON_3dVector vmax = s_bb.Max() - s_bb.Center();
	vmin.Unitize();
	vmax.Unitize();
	vmin = vmin * s_bb.Diagonal().Length() * 2;
	vmax = vmax * s_bb.Diagonal().Length() * 2;
	s_bb.m_min = opnt + vmin;
	s_bb.m_max = opnt + vmax;
	if (s_bb.Includes(fbbox, true)) {
	    last_try = true;
	}
	nverts = vert_search(s_bb);
    }

    double dist = DBL_MAX;
    overt_t *nv = NULL;
    std::set<overt_t *>::iterator v_it;
    for (v_it = nverts.begin(); v_it != nverts.end(); v_it++) {
	overt_t *ov = *v_it;
	ON_3dPoint np = ov->vpnt();
	if (np.DistanceTo(p) < dist) {
	    nv = ov;
	    dist = np.DistanceTo(p);
	}
    }
    if (vdist) {
	(*vdist) = dist;
    }
    return nv;
}

overt_t *
omesh_t::vert_closest(double *vdist, ON_3dPoint &opnt)
{
    ON_BoundingBox fbbox = fmesh->bbox();

    ON_3dPoint ptol(ON_ZERO_TOLERANCE, ON_ZERO_TOLERANCE, ON_ZERO_TOLERANCE);
    ON_BoundingBox s_bb(opnt, opnt);
    ON_3dPoint npnt = opnt + ptol;
    s_bb.Set(npnt, true);
    npnt = opnt - ptol;
    s_bb.Set(npnt, true);

    std::set<overt_t *> nverts = vert_search(s_bb);
    bool last_try = false;
    while (!nverts.size() && !last_try) {
	ON_3dVector vmin = s_bb.Min() - s_bb.Center();
	ON_3dVector vmax = s_bb.Max() - s_bb.Center();
	vmin.Unitize();
	vmax.Unitize();
	vmin = vmin * s_bb.Diagonal().Length() * 2;
	vmax = vmax * s_bb.Diagonal().Length() * 2;
	s_bb.m_min = opnt + vmin;
	s_bb.m_max = opnt + vmax;
	if (s_bb.Includes(fbbox, true)) {
	    last_try = true;
	}
	nverts = vert_search(s_bb);
    }

    double dist = DBL_MAX;
    overt_t *nv = NULL;
    std::set<overt_t *>::iterator v_it;
    for (v_it = nverts.begin(); v_it != nverts.end(); v_it++) {
	overt_t *ov = *v_it;
	ON_3dPoint np = ov->vpnt();
	if (np.DistanceTo(opnt) < dist) {
	    nv = ov;
	    dist = np.DistanceTo(opnt);
	}
    }
    if (vdist) {
	(*vdist) = dist;
    }
    return nv;
}

uedge_t
omesh_t::closest_uedge(ON_3dPoint &p)
{
    uedge_t uedge;

    if (!fmesh->pnts.size()) return uedge;

    ON_BoundingBox fbbox = fmesh->bbox();

    ON_BoundingBox bb(p, p);
    bb.m_min.x = bb.Min().x - ON_ZERO_TOLERANCE;
    bb.m_min.y = bb.Min().y - ON_ZERO_TOLERANCE;
    bb.m_min.z = bb.Min().z - ON_ZERO_TOLERANCE;
    bb.m_max.x = bb.Max().x + ON_ZERO_TOLERANCE;
    bb.m_max.y = bb.Max().y + ON_ZERO_TOLERANCE;
    bb.m_max.z = bb.Max().z + ON_ZERO_TOLERANCE;

    // Find close triangles, iterate through them and
    // find the closest interior edge
    ON_BoundingBox s_bb = bb;
    std::set<size_t> ntris = tris_search(s_bb);
    bool last_try = false;
    while (!ntris.size() && !last_try) {
	ON_3dVector vmin = s_bb.Min() - s_bb.Center();
	ON_3dVector vmax = s_bb.Max() - s_bb.Center();
	vmin.Unitize();
	vmax.Unitize();
	vmin = vmin * s_bb.Diagonal().Length() * 2;
	vmax = vmax * s_bb.Diagonal().Length() * 2;
	s_bb.m_min = p + vmin;
	s_bb.m_max = p + vmax;
	if (s_bb.Includes(fbbox, true)) {
	    last_try = true;
	}
	ntris = tris_search(s_bb);
    }

    double uedist = DBL_MAX;
    std::set<size_t>::iterator tr_it;
    for (tr_it = ntris.begin(); tr_it != ntris.end(); tr_it++) {
	triangle_t t = fmesh->tris_vect[*tr_it];
	ON_3dPoint tp = bb.Center();
	uedge_t ue = fmesh->closest_uedge(t, tp);
	double ue_cdist = fmesh->uedge_dist(ue, p);
	if (ue_cdist < uedist) {
	    uedge = ue;
	    uedist = ue_cdist;
	}
    }

    return uedge;
}


std::set<uedge_t>
omesh_t::interior_uedges_search(ON_BoundingBox &bb)
{
    std::set<uedge_t> uedges;
    uedges.clear();

    ON_3dPoint opnt = bb.Center();

    if (!fmesh->pnts.size()) return uedges;

    ON_BoundingBox fbbox = fmesh->bbox();

    // Find close triangles, iterate through them and
    // find the closest interior edge
    ON_BoundingBox s_bb = bb;
    std::set<size_t> ntris = tris_search(s_bb);
    bool last_try = false;
    while (!ntris.size() && !last_try) {
	ON_3dVector vmin = s_bb.Min() - s_bb.Center();
	ON_3dVector vmax = s_bb.Max() - s_bb.Center();
	vmin.Unitize();
	vmax.Unitize();
	vmin = vmin * s_bb.Diagonal().Length() * 2;
	vmax = vmax * s_bb.Diagonal().Length() * 2;
	s_bb.m_min = opnt + vmin;
	s_bb.m_max = opnt + vmax;
	if (s_bb.Includes(fbbox, true)) {
	    last_try = true;
	}
	ntris = tris_search(s_bb);
    }

    std::set<size_t>::iterator tr_it;
    for (tr_it = ntris.begin(); tr_it != ntris.end(); tr_it++) {
	triangle_t t = fmesh->tris_vect[*tr_it];
	ON_3dPoint tp = bb.Center();
	uedge_t ue = fmesh->closest_interior_uedge(t, tp);
	uedges.insert(ue);
    }

    return uedges;
}

static bool NearTrisCallback(size_t data, void *a_context) {
    std::set<size_t> *ntris = (std::set<size_t> *)a_context;
    ntris->insert(data);
    return true;
}
std::set<size_t>
omesh_t::tris_search(ON_BoundingBox &bb)
{
    double fMin[3], fMax[3];
    fMin[0] = bb.Min().x;
    fMin[1] = bb.Min().y;
    fMin[2] = bb.Min().z;
    fMax[0] = bb.Max().x;
    fMax[1] = bb.Max().y;
    fMax[2] = bb.Max().z;
    std::set<size_t> near_tris;
    size_t nhits = fmesh->tris_tree.Search(fMin, fMax, NearTrisCallback, (void *)&near_tris);

    if (!nhits) {
	return std::set<size_t>();
    }

    return near_tris;
}

static bool NearVertsCallback(long data, void *a_context) {
    std::set<long> *nverts = (std::set<long> *)a_context;
    nverts->insert(data);
    return true;
}
std::set<overt_t *>
omesh_t::vert_search(ON_BoundingBox &bb)
{
    double fMin[3], fMax[3];
    fMin[0] = bb.Min().x - ON_ZERO_TOLERANCE;
    fMin[1] = bb.Min().y - ON_ZERO_TOLERANCE;
    fMin[2] = bb.Min().z - ON_ZERO_TOLERANCE;
    fMax[0] = bb.Max().x + ON_ZERO_TOLERANCE;
    fMax[1] = bb.Max().y + ON_ZERO_TOLERANCE;
    fMax[2] = bb.Max().z + ON_ZERO_TOLERANCE;
    std::set<long> near_verts;
    size_t nhits = vtree.Search(fMin, fMax, NearVertsCallback, (void *)&near_verts);

    if (!nhits) {
	return std::set<overt_t *>();
    }

    std::set<overt_t *> near_overts;
    std::set<long>::iterator n_it;
    for (n_it = near_verts.begin(); n_it != near_verts.end(); n_it++) {
	near_overts.insert(overts[*n_it]);    
    }

    return near_overts;
}

overt_t *
omesh_t::vert_add(long f3ind, ON_BoundingBox *bb)
{
    overt_t *nv = new overt_t(this, f3ind);
    if (bb) {
	nv->update(bb);
    } else {
	nv->update();
    }
    overts[f3ind] = nv;
    return nv;
}

void
omesh_t::refinement_clear()
{
    refinement_overts.clear();
    intruding_tris.clear();
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
    cdt_mesh_t *omesh = (cdt_mesh_t *)data;
    std::pair<cdt_mesh_t *, cdt_mesh_t *> p1(nf->cmesh, omesh);
    std::pair<cdt_mesh_t *, cdt_mesh_t *> p2(omesh, nf->cmesh);
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
plot_active_omeshes(
	std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> check_pairs,
	std::map<bedge_seg_t *, std::set<overt_t *>> *ev
	)
{
    std::set<omesh_t *> a_omesh;
    std::set<omesh_t *>::iterator a_it;
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;

    // Find the meshes with actual intersecting triangles
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first->omesh;
	omesh_t *omesh2 = cp_it->second->omesh;
	if (omesh1->itris.size()) {
	    a_omesh.insert(omesh1);
	}
	if (omesh2->itris.size()) {
	    a_omesh.insert(omesh2);
	}
    }

    // Plot overlaps with refinement points
    for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	omesh_t *am = *a_it;
	am->plot(ev);
    }

}

// Identify refinement points that are close to face edges - they present a
// particular problem 
std::map<bedge_seg_t *, std::set<overt_t *>>
find_edge_verts(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> check_pairs)
{
    std::set<omesh_t *> a_omesh;
    std::set<omesh_t *>::iterator a_it;
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;

    // Find the meshes with actual refinement points
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first->omesh;
	omesh_t *omesh2 = cp_it->second->omesh;
	if (omesh1->refinement_overts.size()) {
	    a_omesh.insert(omesh1);
	}
	if (omesh2->refinement_overts.size()) {
	    a_omesh.insert(omesh2);
	}
    }

    std::map<bedge_seg_t *, std::set<overt_t *>> edge_verts;
    edge_verts.clear();
    for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	omesh_t *am = *a_it;
	std::set<overt_t *> everts;
	std::set<overt_t *>::iterator e_it;
	std::map<overt_t *, std::set<long>>::iterator r_it;
	for (r_it = am->refinement_overts.begin(); r_it != am->refinement_overts.end(); r_it++) {
	    overt_t *v = r_it->first;
	    ON_3dPoint p = v->vpnt();
	    uedge_t closest_edge = am->closest_uedge(p);
	    bool pprint = false;
#if 0
	    //if (am->sname() == std::string("c.s") && am->fmesh->f_id == 0 && PPCHECK(p)) {
	    if (PPCHECK(p)) {
		std::cout << am->sname() << ":\n";
		std::cout << "center " << p.x << " " << p.y << " " << p.z << "\n";
		ON_3dPoint pe1 = *am->fmesh->pnts[closest_edge.v[0]];
		std::cout << "center " << pe1.x << " " << pe1.y << " " << pe1.z << "\n";
		ON_3dPoint pe2 = *am->fmesh->pnts[closest_edge.v[1]];
		std::cout << "center " << pe2.x << " " << pe2.y << " " << pe2.z << "\n";
		pprint = true;
	    }
#endif
	    if (am->fmesh->brep_edges.find(closest_edge) != am->fmesh->brep_edges.end()) {
		bedge_seg_t *bseg = am->fmesh->ue2b_map[closest_edge];
		if (!bseg) {
		    std::cout << "couldn't find bseg pointer??\n";
		} else {

		    // TODO - be aware of distances - if v is far from the
		    // closest_edge line segment relative to that segment's
		    // length and that closest segment is a brep edge segment,
		    // don't split the edge at this point - we'll probably be
		    // introducing other, more useful points

		    VPCHECK(v, NULL);
		    edge_verts[bseg].insert(v);

		    // Erase regardless - if we're here, we won't be using this point for
		    // interiors on the other mesh
		    everts.insert(v);
		}

		if (pprint) {
		    std::cout << "PROBLEM point INSERTED from " << am->sname() << "!\n";
		}
	    }
	}
	// Remove any edge vertices from the refinement_overts container - they'll be handled
	// separately
	for (e_it = everts.begin(); e_it != everts.end(); e_it++) {
	    am->refinement_overts.erase(*e_it);
	}
    }

    plot_active_omeshes(check_pairs, &edge_verts);

    return edge_verts;
}


/* TODO - There are three levels of aggressiveness we can use when assigning refinement points:
 *
 * 1.  Only use vertices that share at least two triangles which overlap with the other mesh
 * 2.  Use all vertices from triangles that overlap
 * 3.  Use all vertices from the other mesh that are contained by the bounding box of the
 *     overlapping triangle from this mesh.
 */
size_t
omesh_refinement_pnts(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> check_pairs, int level)
{
    std::set<omesh_t *> a_omesh;
    std::set<omesh_t *>::iterator a_it;
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;

    // Find the meshes with actual intersecting triangles
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first->omesh;
	omesh_t *omesh2 = cp_it->second->omesh;
	if (omesh1->itris.size()) {
	    a_omesh.insert(omesh1);
	    omesh1->refinement_overts.clear();
	}
	if (omesh2->itris.size()) {
	    a_omesh.insert(omesh2);
	    omesh2->refinement_overts.clear();
	}
    }

    // TODO - incorporate tri_nearedge_refine in here somewhere...

    if (level == 0) {
	// Count triangles to determine which verts need attention.  If a vertex is associated
	// with two or more triangles that intersect another face, it is a refinement point
	// candidate.
	for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	    omesh_t *am = *a_it;
	    std::map<size_t, std::set<std::pair<omesh_t *, size_t>> >::iterator p_it;
	    for (p_it = am->itris.begin(); p_it != am->itris.end(); p_it++) {
		std::set<std::pair<omesh_t *, size_t>>::iterator s_it;
		for (s_it = p_it->second.begin(); s_it != p_it->second.end(); s_it++) {
		    omesh_t *im = s_it->first;
		    size_t otri_ind = s_it->second;
		    triangle_t tri = im->fmesh->tris_vect[otri_ind];
		    for (int i = 0; i < 3; i++) {
			overt_t *v = im->overts[tri.v[i]];
			am->refinement_overts[v].insert(otri_ind);
		    }
		}
	    }
	}
	for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	    std::set<overt_t *> ev;
	    std::set<overt_t *>::iterator ev_it;
	    std::map<overt_t *, std::set<long>>::iterator r_it;
	    omesh_t *am = *a_it;
	    for (r_it = am->refinement_overts.begin(); r_it != am->refinement_overts.end(); r_it++) {
		VPCHECK(r_it->first, NULL);
		// Not enough activity hits, not a refinement vertex
		if (r_it->second.size() == 1) {
		    ev.insert(r_it->first);
		}
	    }
	    for (ev_it = ev.begin(); ev_it != ev.end(); ev_it++) {
		am->refinement_overts.erase(*ev_it);
	    }
	}

	plot_active_omeshes(check_pairs, NULL);

	// Add triangle intersection vertices that are close to the edge of the opposite
	// triangle, whether or not they satisfy the count criteria - these are a source
	// of potential trouble.
	for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	    omesh_t *am = *a_it;
	    std::map<size_t, std::set<std::pair<omesh_t *, size_t>> >::iterator p_it;
	    for (p_it = am->itris.begin(); p_it != am->itris.end(); p_it++) {
		triangle_t t1 = am->fmesh->tris_vect[p_it->first];
		std::set<std::pair<omesh_t *, size_t>>::iterator s_it;
		for (s_it = p_it->second.begin(); s_it != p_it->second.end(); s_it++) {
		    omesh_t *im = s_it->first;
		    size_t otri_ind = s_it->second;
		    triangle_t t2 = im->fmesh->tris_vect[otri_ind];
		    tri_nearedge_refine(am, t1, im, t2);
		}
	    }
	}
    }

    if (level == 1) {
	// Add all vertices associated with triangles that intersect another
	// face.
	for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	    omesh_t *am = *a_it;
	    std::map<size_t, std::set<std::pair<omesh_t *, size_t>> >::iterator p_it;
	    for (p_it = am->itris.begin(); p_it != am->itris.end(); p_it++) {
		std::set<std::pair<omesh_t *, size_t>>::iterator s_it;
		for (s_it = p_it->second.begin(); s_it != p_it->second.end(); s_it++) {
		    omesh_t *im = s_it->first;
		    size_t otri_ind = s_it->second;
		    triangle_t tri = im->fmesh->tris_vect[otri_ind];
		    for (int i = 0; i < 3; i++) {
			overt_t *v = im->overts[tri.v[i]];
			am->refinement_overts[v].insert(otri_ind);
		    }
		}
	    }
	}
    }

    // This case is a bit different - we're altering the intruding mesh's
    // containers based on the triangles recorded in this container, instead
    // of the other way around.
    if (level > 1) {
    	for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	    omesh_t *am = *a_it;
	    std::map<size_t, std::set<std::pair<omesh_t *, size_t>> >::iterator p_it;
	    for (p_it = am->itris.begin(); p_it != am->itris.end(); p_it++) {
		std::set<std::pair<omesh_t *, size_t>>::iterator s_it;
		for (s_it = p_it->second.begin(); s_it != p_it->second.end(); s_it++) {
		    omesh_t *im = s_it->first;
		    size_t otri_ind = s_it->second;

		    triangle_t tri = im->fmesh->tris_vect[otri_ind];
		    ON_BoundingBox t_bb = im->fmesh->tri_bbox(tri.ind);
		    std::set<size_t> otris = am->tris_search(t_bb);
		    std::set<size_t>::iterator o_it;
		    for (o_it = otris.begin(); o_it != otris.end(); o_it++) {
			triangle_t otri = am->fmesh->tris_vect[*o_it];
			for (int i = 0; i < 3; i++) {
			    overt_t *v = am->overts[otri.v[i]];
			    im->refinement_overts[v].insert(otri.ind);
			}
		    }
		}
	    }
	}
    }

    size_t rcnt = 0;
    for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	omesh_t *am = *a_it;
	rcnt += am->refinement_overts.size();
    }

    plot_active_omeshes(check_pairs, NULL);

    return rcnt;
}

size_t
omesh_ovlps(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> check_pairs, int mode)
{
    size_t tri_isects = 0;

    // Assemble the set of mesh faces that (per the check_pairs sets) we
    // may need to process.
    std::set<omesh_t *> a_omesh;
    std::set<omesh_t *>::iterator a_it;
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	cp_it->first->omesh->refinement_clear();
	cp_it->second->omesh->refinement_clear();
	a_omesh.insert(cp_it->first->omesh);
	a_omesh.insert(cp_it->second->omesh);
    }

    // Scrub the old data in active mesh containers (if any)
    for (a_it = a_omesh.begin(); a_it != a_omesh.end(); a_it++) {
	omesh_t *am = *a_it;
	am->itris.clear();
	am->intruding_tris.clear();
    }
    a_omesh.clear();

    // Intersect first the triangle RTrees, then any potentially
    // overlapping triangles found within the leaves.
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first->omesh;
	omesh_t *omesh2 = cp_it->second->omesh;
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)omesh1->fmesh->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)omesh2->fmesh->p_cdt;
	if (s_cdt1 != s_cdt2) {
	    std::set<std::pair<size_t, size_t>> tris_prelim;
	    size_t ovlp_cnt = omesh1->fmesh->tris_tree.Overlaps(omesh2->fmesh->tris_tree, &tris_prelim);
	    if (ovlp_cnt) {
		std::set<std::pair<size_t, size_t>>::iterator tb_it;
		for (tb_it = tris_prelim.begin(); tb_it != tris_prelim.end(); tb_it++) {
		    triangle_t t1 = omesh1->fmesh->tris_vect[tb_it->first];
		    triangle_t t2 = omesh2->fmesh->tris_vect[tb_it->second];
		    int isect = tri_isect(omesh1, t1, omesh2, t2, mode);
		    if (isect) {
			omesh1->itris[t1.ind].insert(std::make_pair(omesh2, t2.ind));
			omesh2->itris[t2.ind].insert(std::make_pair(omesh1, t1.ind));
			omesh1->intruding_tris.insert(t1.ind);
			omesh2->intruding_tris.insert(t2.ind);
			a_omesh.insert(omesh1);
			a_omesh.insert(omesh2);
			tri_isects++;
		    }
		}
	    }
	}
    }

    plot_active_omeshes(check_pairs, NULL);

    return tri_isects;
}


void
orient_tri(cdt_mesh_t &fmesh, triangle_t &t)
{
    ON_3dVector tdir = fmesh.tnorm(t);
    ON_3dVector bdir = fmesh.bnorm(t);
    bool flipped_tri = (ON_DotProduct(tdir, bdir) < 0);
    if (flipped_tri) {
	long tmp = t.v[2];
	t.v[2] = t.v[1];
	t.v[1] = tmp;
    }
}

class revt_pt_t {
    public:
	ON_3dPoint spnt;
	ON_3dVector sn;
	overt_t *ov;
};


int
refine_edge_vert_sets (
	omesh_t *omesh,
	std::map<uedge_t, std::vector<revt_pt_t>> *edge_sets
)
{
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)omesh->fmesh->p_cdt;

    std::cout << "Processing " << s_cdt->name << " face " << omesh->fmesh->f_id << ":\n";

    std::map<uedge_t, std::vector<revt_pt_t>>::iterator es_it;

    int new_tris = 0; 

    while (edge_sets->size()) {
	std::map<uedge_t, std::vector<revt_pt_t>> updated_esets;

	uedge_t ue = edge_sets->begin()->first;
	std::vector<revt_pt_t> epnts = edge_sets->begin()->second;
	edge_sets->erase(edge_sets->begin());

	// Find the two triangles that we will be using to form the outer polygon
	std::set<size_t> rtris = omesh->fmesh->uedges2tris[ue];
	if (rtris.size() != 2) {
	    std::cout << "Error - could not associate uedge with two triangles??\n";
	}
	std::set<size_t> crtris = rtris;
	triangle_t tri1 = omesh->fmesh->tris_vect[*crtris.begin()];
	crtris.erase(crtris.begin());
	triangle_t tri2 = omesh->fmesh->tris_vect[*crtris.begin()];
	crtris.erase(crtris.begin());

	// For the involved triangle edges that are not the edge to be removed, we will
	// need to reassess their closest edge assignment after new edges are added.
	// Build up the set of those uedges for later processing.  While we are looping,
	// get initial data for the polygon build.
	std::set<uedge_t> pnt_reassignment_edges;
	std::set<size_t>::iterator r_it;
	for (r_it = rtris.begin(); r_it != rtris.end(); r_it++) {
	    triangle_t tri = omesh->fmesh->tris_vect[*r_it];
	    std::set<uedge_t> tuedges = omesh->fmesh->uedges(tri);
	    pnt_reassignment_edges.insert(tuedges.begin(), tuedges.end());
	}
	pnt_reassignment_edges.erase(ue);


	cpolygon_t *polygon = omesh->fmesh->uedge_polygon(ue);

#if 0
	{
	FILE *t1plot = fopen("t1.plot", "w");
	struct bu_color c = BU_COLOR_INIT_ZERO;
	unsigned char rgb[3] = {0,0,255};
	bu_color_from_rgb_chars(&c, (const unsigned char *)rgb);
	omesh->fmesh->plot_tri(tri1, &c, t1plot, 255, 0, 0);
	fclose(t1plot);
	}
	{
	FILE *t2plot = fopen("t2.plot", "w");
	struct bu_color c = BU_COLOR_INIT_ZERO;
	unsigned char rgb[3] = {0,0,255};
	bu_color_from_rgb_chars(&c, (const unsigned char *)rgb);
	omesh->fmesh->plot_tri(tri2, &c, t2plot, 255, 0, 0);
	fclose(t2plot);
	}
#endif

	// Add any interior points to the polygon before cdt.
	std::set<overt_t *> new_overts;
	bool have_inside = false;
	for (size_t i = 0; i < epnts.size(); i++) {
	    bool skip_epnt = false;

	    VPCHECK(epnts[i].ov, NULL);

	    ON_BoundingBox sbb(epnts[i].spnt,epnts[i].spnt);
	    ON_3dVector vmin = epnts[i].ov->bb.Min() - epnts[i].ov->bb.Center();
	    ON_3dVector vmax = epnts[i].ov->bb.Max() - epnts[i].ov->bb.Center();
	    vmin.Unitize();
	    vmax.Unitize();
	    vmin = vmin * epnts[i].ov->bb.Diagonal().Length() * 0.5;
	    vmax = vmax * epnts[i].ov->bb.Diagonal().Length() * 0.5;
	    sbb.m_min = epnts[i].spnt + vmin;
	    sbb.m_max = epnts[i].spnt + vmax;

	    std::set <overt_t *> nv = omesh->vert_search(sbb);
	    std::set<overt_t *>::iterator v_it;
	    for (v_it = nv.begin(); v_it != nv.end(); v_it++) {

		ON_3dPoint cvpnt = (*v_it)->vpnt();
		double cvbbdiag = (*v_it)->bb.Diagonal().Length() * 0.1;
		if (cvpnt.DistanceTo(epnts[i].spnt) < cvbbdiag) {
		    // Too close to a vertex in the current mesh, skip
		    std::cout << "skip epnt, too close to vert\n";
		    skip_epnt = true;
		    break;
		}
	    }
	    if (skip_epnt) {
		continue;
	    }
	    // If the point is too close to a brep face edge,
	    // we also need to reject it to avoid creating degenerate
	    // triangles
	    std::set<uedge_t>::iterator ue_it;
	    std::set<uedge_t> b_ue_1 = omesh->fmesh->b_uedges(tri1);
	    for (ue_it = b_ue_1.begin(); ue_it != b_ue_1.end(); ue_it++) {
		uedge_t lue = *ue_it;
		double epdist = omesh->fmesh->uedge_dist(lue, epnts[i].spnt);
		ON_Line lline(*omesh->fmesh->pnts[lue.v[0]], *omesh->fmesh->pnts[lue.v[1]]);
		if (epdist < 0.001 * lline.Length()) {
		    std::cout << "Close to brep face edge\n";
		    skip_epnt = true;
		    break;
		}
	    }
	    if (skip_epnt) {
		continue;
	    }
	    std::set<uedge_t> b_ue_2 = omesh->fmesh->b_uedges(tri2);
	    for (ue_it = b_ue_2.begin(); ue_it != b_ue_2.end(); ue_it++) {
		uedge_t lue = *ue_it;
		double epdist = omesh->fmesh->uedge_dist(lue, epnts[i].spnt);
		ON_Line lline(*omesh->fmesh->pnts[lue.v[0]], *omesh->fmesh->pnts[lue.v[1]]);
		if (epdist < 0.001 * lline.Length()) {
		    std::cout << "Close to brep face edge\n";
		    skip_epnt = true;
		    break;
		}
	    }
	    if (skip_epnt) {
		continue;
	    }

	    bool inside = false;
	    {
	        // Use the overt point for this - the closest surface point will
	        // always pass, so it's not usable for rejection.
		ON_3dPoint ovpnt = epnts[i].ov->vpnt();
		double u, v;
		polygon->tplane.ClosestPointTo(ovpnt, &u, &v);
		std::pair<double, double> proj_2d;
		proj_2d.first = u;
		proj_2d.second = v;
		polygon->pnts_2d.push_back(proj_2d);
		inside = polygon->point_in_polygon(polygon->pnts_2d.size() - 1, false);
		polygon->pnts_2d.pop_back();
	    }
	    if (inside) {
		ON_3dPoint p = epnts[i].spnt;
		double u, v;
		polygon->tplane.ClosestPointTo(p, &u, &v);
		std::pair<double, double> proj_2d;
		proj_2d.first = u;
		proj_2d.second = v;
		polygon->pnts_2d.push_back(proj_2d);
		size_t p2dind = polygon->pnts_2d.size() - 1;

		// We're going to use it - add new 3D point to CDT
		long f3ind = omesh->fmesh->add_point(new ON_3dPoint(epnts[i].spnt));
		long fnind = omesh->fmesh->add_normal(new ON_3dPoint(epnts[i].sn));
		CDT_Add3DPnt(s_cdt, omesh->fmesh->pnts[f3ind], omesh->fmesh->f_id, -1, -1, -1, 0, 0);
		CDT_Add3DNorm(s_cdt, omesh->fmesh->normals[fnind], omesh->fmesh->pnts[f3ind], omesh->fmesh->f_id, -1, -1, -1, 0, 0);
		omesh->fmesh->nmap[f3ind] = fnind;
		overt_t *nvrt = omesh->vert_add(f3ind, &sbb);
		new_overts.insert(nvrt);
		polygon->p2o[p2dind] = f3ind;
		polygon->interior_points.insert(p2dind);

		have_inside = true;
	    }

	}

	if (!have_inside) {
	    // No point in doing this if none of the points are interior
	    delete polygon;
	    continue;
	}

	polygon->cdt();
	//omesh->fmesh->boundary_edges_plot("bedges.plot3");

	if (polygon->tris.size()) {
	    unsigned char rgb[3] = {0,255,255};
	    struct bu_color c = BU_COLOR_INIT_ZERO;
	    bu_color_from_rgb_chars(&c, (const unsigned char *)rgb);
	    omesh->fmesh->tri_remove(tri1);
	    omesh->fmesh->tri_remove(tri2);
	    std::set<triangle_t>::iterator v_it;
	    for (v_it = polygon->tris.begin(); v_it != polygon->tris.end(); v_it++) {
		triangle_t vt = *v_it;
		orient_tri(*omesh->fmesh, vt);
		omesh->fmesh->tri_add(vt);
	    }

	    new_tris += polygon->tris.size();

	    //omesh->fmesh->tris_set_plot(polygon->tris, "poly_tris.plot3");
	    //polygon->polygon_plot_in_plane("poly.plot3");
	} else {
	    std::cout << "polygon cdt failed!\n";
	    bu_file_delete("tri_replace_pair_fail.plot3");
	    polygon->polygon_plot_in_plane("tri_replace_pair_fail.plot3");
	}

	if (!omesh->fmesh->valid(1)) {
	    std::cout << "CDT broke mesh!\n";
	    polygon->polygon_plot_in_plane("poly.plot3");
	    bu_exit(1, "Fatal cdt error\n");
	}
	delete polygon;

	// Have new triangles, update overts
	std::set<overt_t *>::iterator n_it;
	for (n_it = new_overts.begin(); n_it != new_overts.end(); n_it++) {
	    (*n_it)->update();
	}

	// Reassign points to their new closest edge (may be the same edge, but we need
	// to check)
	std::set<uedge_t>::iterator pre_it;
	for (pre_it = pnt_reassignment_edges.begin(); pre_it != pnt_reassignment_edges.end(); pre_it++) {
	    uedge_t rue = *pre_it;
	    std::vector<revt_pt_t> old_epnts = (*edge_sets)[rue];
	    std::map<uedge_t, std::vector<revt_pt_t>>::iterator er_it = edge_sets->find(rue);
	    edge_sets->erase(er_it);
	    for (size_t i = 0; i < old_epnts.size(); i++) {
		overt_t *ov = old_epnts[i].ov;
		std::set<uedge_t> close_edges = omesh->interior_uedges_search(ov->bb);
		double mindist = DBL_MAX; 
		uedge_t closest_uedge;
		std::set<uedge_t>::iterator c_it;
		for (c_it = close_edges.begin(); c_it != close_edges.end(); c_it++) {
		    uedge_t cue = *c_it;
		    double dline = omesh->fmesh->uedge_dist(cue, old_epnts[i].spnt);
		    if (mindist > dline) {
			closest_uedge = cue;
			mindist = dline;
		    }
		}

		(*edge_sets)[closest_uedge].push_back(old_epnts[i]);
	    }
	}
    }

    return new_tris;
}


overt_t *
get_largest_overt(std::set<overt_t *> &verts)
{
    double elen = 0;
    overt_t *l = NULL;
    std::set<overt_t *>::iterator v_it;
    for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	overt_t *v = *v_it;
	if (v->min_len() > elen) {
	    elen = v->min_len();
	    l = v;
	}
    }
    return l;
}

overt_t *
closest_overt(std::set<overt_t *> &verts, overt_t *v)
{
    overt_t *closest = NULL;
    double dist = DBL_MAX;
    ON_3dPoint p1 = *v->omesh->fmesh->pnts[v->p_id];
    std::set<overt_t *>::iterator v_it;
    for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	overt_t *c = *v_it;
	ON_3dPoint p2 = *c->omesh->fmesh->pnts[c->p_id];
	double d = p1.DistanceTo(p2);
	if (dist > d) {
	    closest = c;
	    dist = d;
	}
    }
    return closest;
}

// TODO - right now, we've locked brep face edge points.  This
// isn't ideal, as we could in principle move those points locally
// along the edge curve to align with other points.  This way we
// end up with small triangles to handle fixed edge points.  However,
// adjusting the brep edge points will involved local knowledge
// on the part of each edge vertex about the distances to the next
// edge vertices on either side, as well as updating the corresponding
// vert values throughout the data structures.
//
// Return how many vertices were actually moved a significant distance
// per their bounding box dimensions
int
adjust_overt_pair(overt_t *v1, overt_t *v2)
{
    v1->omesh->refinement_overts.erase(v2);
    v2->omesh->refinement_overts.erase(v1);

    // If we've got two edge vertices, no dice
    if (v1->edge_vert() && v2->edge_vert()) return 0;

    ON_3dPoint p1 = v1->vpnt();
    ON_3dPoint p2 = v2->vpnt();
    double pdist = p1.DistanceTo(p2);
    ON_3dPoint s1_p, s2_p;
    ON_3dVector s1_n, s2_n;

    if (v1->edge_vert()) {
	// It's all up to v2 - if we're too far away, skip it
	if (pdist > v2->min_len()*0.5) {
	    return 0;
	}
	return v2->adjust(p1, pdist);
    }

    if (v2->edge_vert()) {
	// It's all up to v1 - if we're too far away, skip it
	if (pdist > v1->min_len()*0.5) {
	    return 0;
	}
	return v1->adjust(p2, pdist);
    }

    ON_Line l(p1,p2);
    // Weight the t parameter on the line so we are closer to the vertex
    // with the shorter edge length (i.e. less freedom to move without
    // introducing locally severe mesh distortions.)
    double t = 1 - (v2->min_len() / (v1->min_len() + v2->min_len()));
    ON_3dPoint p_wavg = l.PointAt(t);
    if ((p1.DistanceTo(p_wavg) > v1->min_len()*0.5) || (p2.DistanceTo(p_wavg) > v2->min_len()*0.5)) {
	std::cout << "WARNING: large point shift compared to triangle edge length.\n";
    }

    int adj_cnt = 0;
    adj_cnt += v1->adjust(p_wavg, pdist);
    adj_cnt += v2->adjust(p_wavg, pdist);

    return adj_cnt;
}

size_t
adjust_close_verts(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs)
{
    std::map<overt_t *, std::set<overt_t*>> vert_ovlps;
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first->omesh;
	omesh_t *omesh2 = cp_it->second->omesh;
	struct ON_Brep_CDT_State *s_cdt1 = (struct ON_Brep_CDT_State *)omesh1->fmesh->p_cdt;
	struct ON_Brep_CDT_State *s_cdt2 = (struct ON_Brep_CDT_State *)omesh2->fmesh->p_cdt;
	if (s_cdt1 == s_cdt2) continue;
#if 0
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fname, "%s-%d-vtree.plot3", s_cdt1->name, omesh1->fmesh->f_id);
	omesh1->plot_vtree(bu_vls_cstr(&fname));
	bu_vls_sprintf(&fname, "%s-%d-vtree.plot3", s_cdt2->name, omesh2->fmesh->f_id);
	omesh2->plot_vtree(bu_vls_cstr(&fname));
	bu_vls_free(&fname);
	std::cout << "omesh1 vtree cnt: " << omesh1->vtree.Count() << "\n";
	std::cout << "omesh2 vtree cnt: " << omesh2->vtree.Count() << "\n";
#endif
	std::set<std::pair<long, long>> vert_pairs = omesh1->vert_ovlps(omesh2);
	//std::cout << "(" << s_cdt1->name << "," << omesh1->fmesh->f_id << ")+(" << s_cdt2->name << "," << omesh2->fmesh->f_id << "): " << vert_pairs.size() << " vert box overlaps\n";
	std::set<std::pair<long, long>>::iterator v_it;
	for (v_it = vert_pairs.begin(); v_it != vert_pairs.end(); v_it++) {
	    long v_first = (long)v_it->first;
	    long v_second = (long)v_it->second;
	    overt_t *v1 = omesh1->overts[v_first];
	    overt_t *v2 = omesh2->overts[v_second];
	    vert_ovlps[v1].insert(v2);
	    vert_ovlps[v2].insert(v1);
	}
    }
    //std::cout << "Found " << vert_ovlps.size() << " vertices with box overlaps\n";


    std::queue<std::pair<overt_t *, overt_t *>> vq;
    std::set<overt_t *> vq_multi;
    std::map<overt_t *, std::set<overt_t*>>::iterator vo_it;
    for (vo_it = vert_ovlps.begin(); vo_it != vert_ovlps.end(); vo_it++) {
	overt_t *v = vo_it->first;
	if (vo_it->second.size() > 1) {
	    vq_multi.insert(v);
	    continue;
	}
	overt_t *v_other = *vo_it->second.begin();
	if (vert_ovlps[v_other].size() > 1) {
	    // The other point has multiple overlapping points
	    vq_multi.insert(v);
	    continue;
	}
	// Both v and it's companion only have one overlapping point
	vq.push(std::make_pair(v,v_other));
	v->aligned[v_other->omesh] = v_other;
	v_other->aligned[v->omesh] = v;
    }

    //std::cout << "Have " << vq.size() << " simple interactions\n";
    //std::cout << "Have " << vq_multi.size() << " complex interactions\n";
    std::set<overt_t *> adjusted;

    int adjusted_overts = 0;
    while (!vq.empty()) {
	std::pair<overt_t *, overt_t *> vpair = vq.front();
	vq.pop();

	if (adjusted.find(vpair.first) == adjusted.end() && adjusted.find(vpair.second) == adjusted.end()) {
	    adjusted_overts += adjust_overt_pair(vpair.first, vpair.second);
	    adjusted.insert(vpair.first);
	    adjusted.insert(vpair.second);
	}
    }

    // If the box structure is more complicated, we need to be a bit selective
    while (vq_multi.size()) {
	overt_t *l = get_largest_overt(vq_multi);
	if (!l) {
	    vq_multi.erase(l);
	    continue;
	}
	overt_t *c = closest_overt(vert_ovlps[l], l);
	vq_multi.erase(l);
	vq_multi.erase(c);
	vert_ovlps[l].erase(c);
	vert_ovlps[c].erase(l);
	//std::cout << "COMPLEX - adjusting 1 pair only:\n";
	if (adjusted.find(l) == adjusted.end() && adjusted.find(c) == adjusted.end()) {
	    adjusted_overts += adjust_overt_pair(l, c);
	    adjusted.insert(l);
	    adjusted.insert(c);
	    l->aligned[c->omesh] = c;
	    c->aligned[l->omesh] = l;
	}
    }

    return adjusted_overts;
}


void
replace_edge_split_tri(cdt_mesh_t &fmesh, size_t t_id, long np_id, uedge_t &split_edge)
{
    triangle_t &t = fmesh.tris_vect[t_id];

    fmesh.tri_plot(t, "replacing.plot3");
    // Find the two triangle edges that weren't split - these are the starting points for the
    // new triangles
    edge_t e1, e2;
    int ecnt = 0;
    for (int i = 0; i < 3; i++) {
	long v0 = t.v[i];
	long v1 = (i < 2) ? t.v[i + 1] : t.v[0];
	edge_t ec(v0, v1);
	uedge_t uec(ec);
	if (uec != split_edge) {
	    if (!ecnt) {
		e1 = ec;
	    } else {
		e2 = ec;
	    }
	    ecnt++;
	}
    }

    triangle_t ntri1, ntri2;
    ntri1.v[0] = e1.v[0];
    ntri1.v[1] = np_id;
    ntri1.v[2] = e1.v[1];
    orient_tri(fmesh, ntri1);

    ntri2.v[0] = e2.v[0];
    ntri2.v[1] = np_id;
    ntri2.v[2] = e2.v[1];
    orient_tri(fmesh, ntri2);

    fmesh.tri_remove(t);
    fmesh.tri_add(ntri1);
    fmesh.tri_add(ntri2);

    std::set<uedge_t> nedges;
    std::set<uedge_t> n1 = fmesh.uedges(ntri1);
    std::set<uedge_t> n2 = fmesh.uedges(ntri2);
    nedges.insert(n1.begin(), n1.end());
    nedges.insert(n2.begin(), n2.end());
    std::set<uedge_t>::iterator n_it;

    fmesh.tri_plot(ntri1, "nt1.plot3");
    fmesh.tri_plot(ntri2, "nt2.plot3");

}

int
ovlp_split_edge(
	overt_t **nv,
	std::set<bedge_seg_t *> *nsegs,
	bedge_seg_t *eseg, double t
	)
{
    int replaced_tris = 0;

    std::vector<std::pair<cdt_mesh_t *,uedge_t>> uedges = eseg->uedges();

    cdt_mesh_t *fmesh_f1 = uedges[0].first;
    cdt_mesh_t *fmesh_f2 = uedges[1].first;

    int f_id1 = fmesh_f1->f_id;
    int f_id2 = fmesh_f2->f_id;

    uedge_t ue1 = uedges[0].second;
    uedge_t ue2 = uedges[1].second;

    std::set<size_t> f1_tris = fmesh_f1->uedges2tris[ue1];
    std::set<size_t> f2_tris = fmesh_f2->uedges2tris[ue2];

    size_t tris_cnt = 0;
    if (fmesh_f1->f_id == fmesh_f2->f_id) {
	std::set<size_t> f_alltris;
	f_alltris.insert(f1_tris.begin(), f1_tris.end());
	f_alltris.insert(f2_tris.begin(), f2_tris.end());
	tris_cnt = f_alltris.size();
    } else {
	tris_cnt = f1_tris.size() + f2_tris.size();
    }
    if (tris_cnt != 2) {
	if (f1_tris.size() != 1 || ((fmesh_f1->f_id == fmesh_f2->f_id) && (f1_tris.size() != 2))) {
	    std::cerr << "FATAL: could not find expected triangle in mesh " << fmesh_f1->name << "," << f_id1 << " using uedge " << ue1.v[0] << "," << ue1.v[1] << "\n";
	    ON_3dPoint v1 = *fmesh_f1->pnts[ue1.v[0]];
	    ON_3dPoint v2 = *fmesh_f1->pnts[ue1.v[1]];
	    std::cerr << "ue1.v[0]: " << v1.x << "," << v1.y << "," << v1.z << "\n";
	    std::cerr << "ue1.v[1]: " << v2.x << "," << v2.y << "," << v2.z << "\n";
	    CDT_Audit((struct ON_Brep_CDT_State *)eseg->p_cdt);
	    CDT_Audit((struct ON_Brep_CDT_State *)fmesh_f1->p_cdt);
	    exit(1);
	}
	if (f2_tris.size() != 1 || ((fmesh_f1->f_id == fmesh_f2->f_id) && (f2_tris.size() != 2))) {
	    std::cerr << "FATAL: could not find expected triangle in mesh " << fmesh_f2->name << "," << f_id2 << " using uedge " << ue2.v[0] << "," << ue2.v[1] << "\n";
	    ON_3dPoint v1 = *fmesh_f2->pnts[ue2.v[0]];
	    ON_3dPoint v2 = *fmesh_f2->pnts[ue2.v[1]];
	    std::cerr << "ue2.v[0]: " << v1.x << "," << v1.y << "," << v1.z << "\n";
	    std::cerr << "ue2.v[1]: " << v2.x << "," << v2.y << "," << v2.z << "\n";
	    CDT_Audit((struct ON_Brep_CDT_State *)eseg->p_cdt);
	    CDT_Audit((struct ON_Brep_CDT_State *)fmesh_f2->p_cdt);
	    exit(1);
	}
	ON_3dPoint ue1_p1 = *fmesh_f1->pnts[ue1.v[0]];
	ON_3dPoint ue1_p2 = *fmesh_f1->pnts[ue1.v[1]];
	std::cout << fmesh_f1->name << "," << f_id1 << " ue1: " << ue1.v[0] << "," << ue1.v[1] << ": " << ue1_p1.x << "," << ue1_p1.y << "," << ue1_p1.z << " -> " << ue1_p2.x << "," << ue1_p2.y << "," << ue1_p2.z << "\n";
	ON_3dPoint ue2_p1 = *fmesh_f2->pnts[ue2.v[0]];
	ON_3dPoint ue2_p2 = *fmesh_f2->pnts[ue2.v[1]];
	std::cout << fmesh_f2->name << "," << f_id2 << " ue2: " << ue2.v[0] << "," << ue2.v[1] << ": " << ue2_p1.x << "," << ue2_p1.y << "," << ue2_p1.z << " -> " << ue2_p2.x << "," << ue2_p2.y << "," << ue2_p2.z << "\n";
	return -1;
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

    if (nsegs) {
	(*nsegs).insert(esegs_split.begin(), esegs_split.end());
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

    // Need to return one of the inserted verts from this process - we need to
    // check if that point is going to require any splitting on any other
    // objects' faces.  Any triangle from another object "close" to the new
    // point should have a vertex that can be adjusted, or if not one needs to
    // be introduced.
    long np_id;
    if (f_id1 == f_id2) {
	std::set<size_t> ftris;
	std::set<size_t>::iterator tr_it;
	uedge_t ue;
	ue = (f1_tris.size()) ? ue1 : ue2;
	ftris.insert(f1_tris.begin(), f1_tris.end());
	ftris.insert(f2_tris.begin(), f2_tris.end());
	np_id = fmesh_f1->pnts.size() - 1;
	fmesh_f1->ep.insert(np_id);
	for (tr_it = ftris.begin(); tr_it != ftris.end(); tr_it++) {
	    replace_edge_split_tri(*fmesh_f1, *tr_it, np_id, ue);
	    replaced_tris++;
	}
	omesh_t *om = fmesh_f1->omesh;
	overt_t *nvert = om->vert_add(np_id);
	(*nv) = nvert;
    } else {
	np_id = fmesh_f1->pnts.size() - 1;
	fmesh_f1->ep.insert(np_id);
	replace_edge_split_tri(*fmesh_f1, *f1_tris.begin(), np_id, ue1);
	replaced_tris++;
	omesh_t *om1 = fmesh_f1->omesh;
	om1->vert_add(np_id);

	np_id = fmesh_f2->pnts.size() - 1;
	fmesh_f2->ep.insert(np_id);
	replace_edge_split_tri(*fmesh_f2, *f2_tris.begin(), np_id, ue2);
	replaced_tris++;
    	omesh_t *om2 = fmesh_f2->omesh;
	overt_t *nvert = om2->vert_add(np_id);
	(*nv) = nvert;
    }

    return replaced_tris;
}

// Find the point on the edge nearest to the point, and split the edge at that point.
//
// TODO - probably need to get more aggressive about splitting near verts as the
// iteration count increases - if there's no other way to resolve the mesh, that's
// what we need to do, small triangles or not...
int
bedge_split_at_t(
	overt_t **nv,
	bedge_seg_t *eseg, double t,
	std::set<bedge_seg_t *> *nsegs
	)
{
    int replaced_tris = 0;
    ON_NurbsCurve *nc = eseg->nc;
    ON_3dPoint cep = nc->PointAt(t);
    double epdist1 = eseg->e_start->DistanceTo(cep);
    double epdist2 = eseg->e_end->DistanceTo(cep);
    double lseg_check = 0.1 * eseg->e_start->DistanceTo(*eseg->e_end);
    if (epdist1 > lseg_check && epdist2 > lseg_check) {
	// If the point is not close to a start/end point on the edge then split the edge.
	int rtris = ovlp_split_edge(nv, nsegs, eseg, t);
	if (rtris >= 0) {
	    replaced_tris += rtris;
	} else {
	    std::cout << "split failed\n";
	}
    } else {
	std::cout << "split point too close to vertices - skip\n";
    }
    return replaced_tris;
}

// Find the point on the edge nearest to the point, and split the edge at that point.
int
bedge_split_near_pnt(
	overt_t **nv,
	bedge_seg_t *eseg, ON_3dPoint &p,
	std::set<bedge_seg_t *> *nsegs
	)
{
    ON_NurbsCurve *nc = eseg->nc;
    ON_Interval domain(eseg->edge_start, eseg->edge_end);
    double t;
    ON_NurbsCurve_GetClosestPoint(&t, nc, p, 0.0, &domain);
    return bedge_split_at_t(nv, eseg, t, nsegs);
}

// Find the point on the edge nearest to the vert point.
int
bedge_split_near_vert(
	overt_t **nv,
	std::map<bedge_seg_t *, overt_t *> &edge_vert)
{
    int replaced_tris = 0;
    std::map<bedge_seg_t *, overt_t *>::iterator ev_it;
    for (ev_it = edge_vert.begin(); ev_it != edge_vert.end(); ev_it++) {
	bedge_seg_t *eseg = ev_it->first;
	overt_t *v = ev_it->second;
	ON_3dPoint p = v->vpnt();
	replaced_tris += bedge_split_near_pnt(nv, eseg, p, NULL);
    }
    return replaced_tris;
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
	    valid = false;
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
	for (f_it = fmeshes.begin(); f_it != fmeshes.end(); f_it++) {
	    cdt_mesh_t *fmesh = *f_it;
	    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)fmesh->p_cdt;
	    std::string fpname = std::string(s_cdt->name) + std::string("_face_") + std::to_string(fmesh->f_id) + std::string(".plot3");
	    fmesh->tris_plot(fpname.c_str());
	}
	bu_exit(1, "fatal mesh damage");
    }

    return valid;
}

int
bedge_split_near_verts(
	std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs,
	std::set<overt_t *> *nverts,
	std::map<bedge_seg_t *, std::set<overt_t *>> &edge_verts
	)
{
    int replaced_tris = 0;
    std::map<bedge_seg_t *, std::set<overt_t *>>::iterator ev_it;
    int evcnt = 0;
    for (ev_it = edge_verts.begin(); ev_it != edge_verts.end(); ev_it++) {
	std::cout << "ev (" << evcnt+1 << " of " << edge_verts.size() <<")\n";
	std::set<overt_t *> verts = ev_it->second;
	std::set<bedge_seg_t *> segs;
	segs.insert(ev_it->first);

	while (verts.size()) {
	    overt_t *v = *verts.begin();
	    ON_3dPoint p = v->vpnt();
	    verts.erase(v);

#if 0
	    if (PPCHECK(p)) {
		std::cout << v->omesh->sname() << ":\n";
		std::cout << "center " << p.x << " " << p.y << " " << p.z << "\n";
	    }
#endif

	    bedge_seg_t *eseg_split = NULL;
	    double split_t = -1.0;
	    double closest_dist = DBL_MAX;
	    std::set<bedge_seg_t *>::iterator e_it;
	    //std::cout << "segs size: " << segs.size() << "\n";
	    for (e_it = segs.begin(); e_it != segs.end(); e_it++) {
		bedge_seg_t *eseg = *e_it;
		ON_NurbsCurve *nc = eseg->nc;
		ON_Interval domain(eseg->edge_start, eseg->edge_end);
		double t;
		ON_NurbsCurve_GetClosestPoint(&t, nc, p, 0.0, &domain);
		ON_3dPoint cep = nc->PointAt(t);
		double ecdist = cep.DistanceTo(p);
		if (closest_dist > ecdist) {
		    //std::cout << "closest_dist: " << closest_dist << "\n";
		    //std::cout << "ecdist: " << ecdist << "\n";
		    closest_dist = ecdist;
		    eseg_split = eseg;
		    split_t = t;
		}
	    }

	    std::set<bedge_seg_t *> nsegs;
	    overt_t *nv = NULL;
	    int ntri_cnt = bedge_split_at_t(&nv, eseg_split, split_t, &nsegs);
	    if (ntri_cnt) {
		segs.erase(eseg_split);
		replaced_tris += ntri_cnt;
		segs.insert(nsegs.begin(), nsegs.end());
		nverts->insert(nv);

		// Ouch - we're getting a validity failure after the near_verts split
		check_faces_validity(check_pairs);
	    }
	}
	evcnt++;
    }

    return replaced_tris;
}

int
vert_nearby_closest_point_check(
	overt_t *nv,
	std::map<bedge_seg_t *, std::set<overt_t *>> &edge_verts,
	std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs)
{
    int retcnt = 0;
    ON_3dPoint vpnt = nv->vpnt();

    // For the given vertex, check any mesh from any of the pairs that
    // is potentially overlapping.
    std::set<omesh_t *> check_meshes;
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator o_it;
    for (o_it = check_pairs.begin(); o_it != check_pairs.end(); o_it++) {
	if (o_it->first->omesh == nv->omesh && o_it->second->omesh != nv->omesh) {
	    check_meshes.insert(o_it->second->omesh);
	}
	if (o_it->second->omesh == nv->omesh && o_it->first->omesh != nv->omesh) {
	    check_meshes.insert(o_it->first->omesh);
	}
    }


    std::set<omesh_t *>::iterator m_it;
    for (m_it = check_meshes.begin(); m_it != check_meshes.end(); m_it++) {
	// If there are close triangles to this vertex but the closest surface
	// point is not near one of the verts of those triangles, we need to
	// introduce a new point onto the opposite mesh.  Also will have to
	// check if the point we need to introduce is an edge point.
	omesh_t *m = *m_it;

	ON_3dPoint spnt;
	ON_3dVector sn;
	double dist = nv->bb.Diagonal().Length() * 10;
	if (!m->fmesh->closest_surf_pnt(spnt, sn, &vpnt, 2*dist)) {
	    // If there's no surface point with dist, we're not close
	    continue;
	}
	ON_BoundingBox spbb(spnt, spnt);

	// Check the vtree and see if we can eliminate the closest surf point
	// as being already close to an existing vert in the other mesh
	double vdist;
	overt_t *existing_v = m->vert_closest(&vdist, nv);

	// The key point here is whether the other vertex is too close according
	// to the point needing a match, not according to the other point (which
	// may have an arbitrarily sized bbox depending on its associated edge
	// lengths.  Construct a new bbox using the existing_v point and the box
	// size of nv.
	ON_BoundingBox evnbb(existing_v->vpnt(), existing_v->vpnt());
	ON_3dPoint c1 = existing_v->vpnt() + nv->bb.Diagonal()*0.5;
	ON_3dPoint c2 = existing_v->vpnt() - nv->bb.Diagonal()*0.5;
	evnbb.Set(c1, true);
	evnbb.Set(c2, true);

	if (evnbb.IsDisjoint(spbb)) {
	    // Closest surface point isn't inside the closest vert bbox - add
	    // a new refinement vert

	    // If we're close to a brep face edge, needs to go in edge_verts.
	    // Else, new interior vert for m
	    uedge_t closest_edge = m->closest_uedge(vpnt);
	    if (m->fmesh->brep_edges.find(closest_edge) != m->fmesh->brep_edges.end()) {
		bedge_seg_t *bseg = m->fmesh->ue2b_map[closest_edge];
		if (!bseg) {
		    std::cout << "couldn't find bseg pointer??\n";
		} else {
		    edge_verts[bseg].insert(nv);
		    retcnt++;
		}
	    } else {
		std::set<size_t> uet = m->fmesh->uedges2tris[closest_edge];
		std::set<size_t>::iterator u_it;
		for (u_it = uet.begin(); u_it != uet.end(); u_it++) {
		    m->refinement_overts[nv].insert(*u_it);
		}
		retcnt++;
	    }
	}
    }

    return retcnt;
}




int
omesh_interior_edge_verts(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs)
{
    std::set<omesh_t *> omeshes;
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator cp_it;
    for (cp_it = check_pairs.begin(); cp_it != check_pairs.end(); cp_it++) {
	omesh_t *omesh1 = cp_it->first->omesh;
	omesh_t *omesh2 = cp_it->second->omesh;
	if (omesh1->refinement_overts.size()) {
	    omeshes.insert(omesh1);
	}
	if (omesh2->refinement_overts.size()) {
	    omeshes.insert(omesh2);
	}
    }
    std::cout << "Need to split triangles in " << omeshes.size() << " meshes\n";

    int rcnt = 0;

    std::set<omesh_t *>::iterator o_it;
    for (o_it = omeshes.begin(); o_it != omeshes.end(); o_it++) {
	omesh_t *omesh = *o_it;
	if (omesh->sname() == std::string("c.s") && omesh->fmesh->f_id == 4) {
	    std::cout << "debug\n";
	}
	std::map<uedge_t, std::vector<revt_pt_t>> edge_sets;

	// From the refinement_overts set, build up the set of vertices
	// that look like they are intruding
	std::set<overt_t *> omesh_rverts;
	std::map<overt_t *, std::set<long>>::iterator r_it;
	for (r_it = omesh->refinement_overts.begin(); r_it != omesh->refinement_overts.end(); r_it++) {
	    omesh_rverts.insert(r_it->first);
	}


	while (omesh_rverts.size()) {

	    overt_t *ov = *omesh_rverts.begin();

	    VPCHECK(ov, NULL);

	    omesh_rverts.erase(ov);
	    ON_3dPoint ovpnt = ov->vpnt();

	    ON_3dPoint spnt;
	    ON_3dVector sn;
	    double dist = ov->bb.Diagonal().Length() * 10;

	    if (!omesh->fmesh->closest_surf_pnt(spnt, sn, &ovpnt, 2*dist)) {
		std::cout << "closest point failed\n";
		continue;
	    }

	    int problem_flag = 0;

	    // Check this point against the mesh vert tree - if we're
	    // extremely close to an existing vertex, we don't want to split
	    // and create extremely tiny triangles - vertex adjustment should
	    // handle super-close points...
	    ON_BoundingBox pbb(spnt, spnt);
	    std::set<overt_t *> cverts = omesh->vert_search(pbb);
	    if (problem_flag && !cverts.size()) {
		cverts = omesh->vert_search(pbb);
	    }

	    // If we're close to a vertex, find the vertices from the other
	    // mesh that overlap with that vertex.  If there are, and if the
	    // surface point is close to them per the vert bbox dimension,
	    // skip that point as a refinement point in this step.
	    if (cverts.size()) {
		double spdist = ovpnt.DistanceTo(spnt);

		// Find the closest vertex, and use its bounding box size as a
		// gauge for how close is too close for the surface point
		double vdist = DBL_MAX;
		double bbdiag = DBL_MAX;
		bool skip = false;
		std::set<overt_t *>::iterator v_it;
		for (v_it = cverts.begin(); v_it != cverts.end(); v_it++) {
		    ON_3dPoint cvpnt = (*v_it)->vpnt();
		    double cvbbdiag = (*v_it)->bb.Diagonal().Length() * 0.1;
		    if (cvpnt.DistanceTo(spnt) < cvbbdiag) {
			// Too close to a vertex in the current mesh, skip
			skip = true;
			break;
		    }
		    double cvdist = ovpnt.DistanceTo(cvpnt);
		    if (cvdist < vdist) {
			vdist = cvdist;
			bbdiag = cvbbdiag;
		    }
		}

		if (skip || spdist < bbdiag) {
		    continue;
		}
	    }

	    // If we passed the above filter, find the closest edges
	    std::set<uedge_t> close_edges = omesh->interior_uedges_search(ov->bb);

	    double mindist = DBL_MAX; 
	    uedge_t closest_uedge;
	    std::set<uedge_t>::iterator c_it;
	    for (c_it = close_edges.begin(); c_it != close_edges.end(); c_it++) {
		uedge_t ue = *c_it;
		double dline = omesh->fmesh->uedge_dist(ue, spnt);
		if (mindist > dline) {
		    closest_uedge = ue;
		    mindist = dline;
		}
	    }

	    if (closest_uedge.v[0] == -1) {
		std::cout << "uedge problem\n";
		continue;
	    }

	    revt_pt_t rpt;
	    rpt.spnt = spnt;
	    rpt.sn = sn;
	    rpt.ov = ov;
	    edge_sets[closest_uedge].push_back(rpt);
	}

	struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)omesh->fmesh->p_cdt;
	std::cout << s_cdt->name << " face " << omesh->fmesh->f_id << " has " << edge_sets.size() << " edge/point sets:\n";
	std::map<uedge_t, std::vector<revt_pt_t>>::iterator es_it;
	for (es_it = edge_sets.begin(); es_it != edge_sets.end(); es_it++) {
	    uedge_t ue = es_it->first;
	    std::cout << "Edge: " << ue.v[0] << "<->" << ue.v[1] << ": " << es_it->second.size() << " points\n";
	}

	rcnt += refine_edge_vert_sets(omesh, &edge_sets);
    }

    return rcnt;
}



static void
make_omeshes(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> check_pairs)
{
    // Make omesh containers for all the cdt_meshes in play
    std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>>::iterator p_it;
    std::set<cdt_mesh_t *> afmeshes;
    std::vector<omesh_t *> omeshes;
    for (p_it = check_pairs.begin(); p_it != check_pairs.end(); p_it++) {
	afmeshes.insert(p_it->first);
	afmeshes.insert(p_it->second);
    }
    std::set<cdt_mesh_t *>::iterator af_it;
    for (af_it = afmeshes.begin(); af_it != afmeshes.end(); af_it++) {
	cdt_mesh_t *fmesh = *af_it;
	omeshes.push_back(new omesh_t(fmesh));
	if (fmesh->omesh) {
	    delete fmesh->omesh;
	}
	fmesh->omesh = omeshes[omeshes.size() - 1];
    }
}

int
ON_Brep_CDT_Ovlp_Resolve(struct ON_Brep_CDT_State **s_a, int s_cnt)
{
    int rpnt_level = 0;
    if (!s_a) return -1;
    if (s_cnt < 1) return 0;

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

    make_omeshes(check_pairs);

    int face_ov_cnt = omesh_ovlps(check_pairs, 0);
    if (!face_ov_cnt) {
	return 0;
    }

    // Report our starting overlap count
    std::cout << "Initial overlap cnt: " << face_ov_cnt << "\n";

    // If we're going to process, initialize refinement points
    omesh_refinement_pnts(check_pairs, rpnt_level);

    int bedge_replaced_tris = INT_MAX;

    while (bedge_replaced_tris) {
	int avcnt = 0;
	// The simplest operation is to find vertices close to each other
	// with enough freedom of movement (per triangle edge length) that
	// we can shift the two close neighbors to surface points that are
	// both the respective closest points to a center point between the
	// two originals.
	avcnt = adjust_close_verts(check_pairs);
	if (avcnt) {
	    // If we adjusted, recalculate overlapping tris and refinement points
	    std::cout << "Adjusted " << avcnt << " vertices\n";
	    face_ov_cnt = omesh_ovlps(check_pairs, 0);
	    omesh_refinement_pnts(check_pairs, rpnt_level);
	}

	// Process edge_verts
	size_t evcnt = INT_MAX;
	std::map<bedge_seg_t *, std::set<overt_t *>> edge_verts = find_edge_verts(check_pairs);
	while (evcnt > edge_verts.size()) {
	    std::set<overt_t *> nverts;
	    bedge_replaced_tris = bedge_split_near_verts(check_pairs, &nverts, edge_verts);
	    evcnt = edge_verts.size();
	    edge_verts.clear();
	    omesh_refinement_pnts(check_pairs, rpnt_level);
	    edge_verts = find_edge_verts(check_pairs);
	}

	if (bedge_replaced_tris) {
	    face_ov_cnt = omesh_ovlps(check_pairs, 0);
	    omesh_refinement_pnts(check_pairs, rpnt_level);
	}
    }

    // Once edge splits are handled, use remaining closest points and find
    // nearest interior edge curve, building sets of points near each interior
    // edge.  Then, for all interior edges, yank the two triangles associated
    // with that edge, build a polygon with interior points and tessellate.  We
    // need this to introduce no changes to the meshes as a condition of
    // termination...
    int interior_replaced_tris = INT_MAX;
    while (interior_replaced_tris) {
	find_edge_verts(check_pairs);
	interior_replaced_tris = omesh_interior_edge_verts(check_pairs);
	face_ov_cnt = omesh_ovlps(check_pairs, 0);
	omesh_refinement_pnts(check_pairs, rpnt_level);
	check_faces_validity(check_pairs);
    }

    // Refine areas of the mesh with overlapping triangles that have aligned
    // vertices but have different triangulations of those vertices.
    shared_cdts(check_pairs);

    // Make sure everything is still OK and do final overlap check
    bool final_valid = check_faces_validity(check_pairs);
    face_ov_cnt = omesh_ovlps(check_pairs, 0); // TODO - this should be the strict test...
    std::cout << "Post-processing overlap cnt: " << face_ov_cnt << "\n";

    return (face_ov_cnt > 0 || !final_valid) ? -1 : 0;
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

