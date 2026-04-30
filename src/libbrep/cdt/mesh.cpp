/*                    C D T _ M E S H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2026 United States Government as represented by
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
/** @file cdt_mesh.cpp
 *
 * Mesh routines in support of Constrained Delaunay Triangulation of NURBS
 * B-Rep objects
 *
 */

// This evolved from the original trimesh halfedge data structure code:

// Author: Yotam Gingold <yotam (strudel) yotamgingold.com>
// License: Public Domain.  (I, Yotam Gingold, the author, release this code into the public domain.)
// GitHub: https://github.com/yig/halfedge

#include "common.h"

#include "bu/color.h"
#include "bu/log.h"
#include "bu/sort.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bn/mat.h" /* bn_vec_perp */
#include "bv/plot3.h"
#include "bg/plane.h" /* bg_fit_plane */
#include "bg/polygon.h"
#include "bg/tri_pt.h"
#include "bg/trimesh.h"
#include "brep.h"
#include "./cdt.h"
#include "./mesh.h"

/* GTE mean-value parameterization for the lscm_reproject path */
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wfloat-equal"
#  pragma GCC diagnostic ignored "-Wshadow"
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wshadow"
#endif
#include <Mathematics/LSCMParameterization.h>
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif

// needed for implementation
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <limits>
#include <stack>

static void
plot_pnt_2d(FILE *plot_file, ON_2dPoint *p, double r, int dir)
{
    point_t origin, bnp;
    VSET(origin, p->x, p->y, 0);
    pdv_3move(plot_file, origin);

    if (dir == 0) {
	VSET(bnp, p->x+r, p->y, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x-r, p->y, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x, p->y+r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x, p->y-r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x, p->y, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x, p->y, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
    }
    if (dir == 1) {
	VSET(bnp, p->x+r, p->y+r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x+r, p->y-r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x-r, p->y+r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x-r, p->y-r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);

	VSET(bnp, p->x+r, p->y+r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x+r, p->y-r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x-r, p->y+r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x-r, p->y-r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
    }
}

static void
plot_vec_3d(FILE *plot_file, ON_3dPoint *p, ON_3dVector *v, double elen)
{
    point_t origin, bnp;
    VSET(origin, p->x, p->y, p->z);
    pdv_3move(plot_file, origin);

    ON_3dVector vp = *v;
    vp.Unitize();
    vp = vp * elen;
    ON_3dPoint np = *p + vp;

    VSET(bnp, np.x, np.y, np.z);
    pdv_3cont(plot_file, bnp);
}


static void
plot_seg_3d(FILE *plot_file, ON_3dPoint *p1, ON_3dPoint *p2)
{
    point_t origin, bnp;
    VSET(origin, p1->x, p1->y, p1->z);
    pdv_3move(plot_file, origin);
    VSET(bnp, p2->x, p2->y, p2->z);
    pdv_3cont(plot_file, bnp);
}

void
cpolyedge_t::plot3d(const char *fname)
{
    // If our polygon is in 2D, use the Brep to calculate 3D info
    if (eseg) {
	ON_Brep *brep = eseg->brep;
	ON_BrepTrim& trim = brep->m_T[trim_ind];
	ON_3dPoint trim_s_2d = trim.PointAt(trim_start);
	ON_3dPoint trim_e_2d = trim.PointAt(trim_end);
	ON_3dPoint trim_s_3d, trim_e_3d;
	ON_3dVector trim_s_norm, trim_e_norm;
	surface_EvNormal(trim.SurfaceOf(), trim_s_2d.x, trim_s_2d.y, trim_s_3d, trim_s_norm);
	surface_EvNormal(trim.SurfaceOf(), trim_e_2d.x, trim_e_2d.y, trim_e_3d, trim_e_norm);
	double slen = trim_s_3d.DistanceTo(trim_e_3d);

	FILE* plot_file = fopen(fname, "w");

	pl_color(plot_file, 0, 0, 255);
	plot_seg_3d(plot_file, &trim_s_3d, &trim_e_3d);

	pl_color(plot_file, 255, 0, 0);
	plot_pnt_3d(plot_file, &trim_s_3d, 0.05*slen, 0);
	plot_vec_3d(plot_file, &trim_s_3d, &trim_s_norm, 0.2*slen);
	pl_color(plot_file, 0, 255, 0);
	plot_pnt_3d(plot_file, &trim_e_3d, 0.05*slen, 0);
	plot_vec_3d(plot_file, &trim_e_3d, &trim_e_norm, 0.2*slen);

	fclose(plot_file);
    } else {
	std::cout << "no brep information available on trim segment\n";
    }

}

void
bedge_seg_t::plot(const char *fname)
{
    FILE* plot_file = fopen(fname, "w");

    double slen = e_start->DistanceTo(*e_end);
    double plen = 0.05*slen;

    pl_color(plot_file, 0, 0, 255);
    plot_seg_3d(plot_file, e_start, e_end);
    pl_color(plot_file, 255, 0, 0);
    plot_pnt_3d(plot_file, e_start, plen, 0);
    plot_vec_3d(plot_file, e_start, &tan_start, 0.2*slen);
    pl_color(plot_file, 0, 255, 0);
    plot_pnt_3d(plot_file, e_end, plen, 0);
    plot_vec_3d(plot_file, e_end, &tan_end, 0.2*slen);

    fclose(plot_file);
}

std::vector<std::pair<cdt_mesh_t *,uedge_t>>
bedge_seg_t::uedges()
{
    std::vector<std::pair<cdt_mesh_t *,uedge_t>> edges;
    struct ON_Brep_CDT_State *s_cdt_edge = (struct ON_Brep_CDT_State *)p_cdt;
    int f_id1 = s_cdt_edge->brep->m_T[tseg1->trim_ind].Face()->m_face_index;
    int f_id2 = s_cdt_edge->brep->m_T[tseg2->trim_ind].Face()->m_face_index;
    cdt_mesh_t &fmesh_f1 = s_cdt_edge->fmeshes[f_id1];
    cdt_mesh_t &fmesh_f2 = s_cdt_edge->fmeshes[f_id2];

    cpolyedge_t *pe1 = tseg1;
    cpolyedge_t *pe2 = tseg2;
    cpolygon_t *poly1 = pe1->polygon;
    cpolygon_t *poly2 = pe2->polygon;
    long ue1_1 = fmesh_f1.p2ind[fmesh_f1.pnts[fmesh_f1.p2d3d[poly1->p2o[tseg1->v2d[0]]]]];
    long ue1_2 = fmesh_f1.p2ind[fmesh_f1.pnts[fmesh_f1.p2d3d[poly1->p2o[tseg1->v2d[1]]]]];
    uedge_t ue1(ue1_1, ue1_2);

    edges.push_back(std::make_pair(&fmesh_f1, ue1));

    long ue2_1 = fmesh_f2.p2ind[fmesh_f2.pnts[fmesh_f2.p2d3d[poly2->p2o[tseg2->v2d[0]]]]];
    long ue2_2 = fmesh_f2.p2ind[fmesh_f2.pnts[fmesh_f2.p2d3d[poly2->p2o[tseg2->v2d[1]]]]];
    uedge_t ue2(ue2_1, ue2_2);

    edges.push_back(std::make_pair(&fmesh_f2, ue2));

    return edges;
}

/*****************************/
/* triangle_t implementation */
/*****************************/

ON_3dPoint *
triangle_t::vpnt(int idx)
{
    return m->pnts[v[idx]];
}

double
triangle_t::opp_edge_dist(int vind)
{
    std::vector<int> everts;
    for (int idx = 0; idx < 3; idx++) {
	if (v[idx] != vind) {
	    everts.push_back(v[idx]);
	}
    }
    if (everts.size() != 2) {
	// Degenerate triangle
	return 0;
    }

    uedge_t ue(everts[0], everts[1]);

    ON_3dPoint p = *m->pnts[vind];

    double edist = m->uedge_dist(ue, p);
    return edist;
}

char *
triangle_t::ppnt(int idx)
{
    ON_3dPoint *p = vpnt(idx);
    struct bu_vls pp = BU_VLS_INIT_ZERO;
    char *rstr = NULL;
    bu_vls_sprintf(&pp, "%.17f %.17f %.17f", p->x, p->y, p->z);
    rstr = bu_strdup(bu_vls_cstr(&pp));
    bu_vls_free(&pp);
    return rstr;
}

void
triangle_t::plot(const char *fname)
{
    m->tri_plot(ind, fname);
}

double
triangle_t::shortest_edge_len()
{
    double len = DBL_MAX;
    for (int idx = 0; idx < 3; idx++) {
	long v0 = v[idx];
	long v1 = (idx < 2) ? v[idx + 1] : v[0];
	ON_3dPoint *p1 = m->pnts[v0];
	ON_3dPoint *p2 = m->pnts[v1];
	double d = p1->DistanceTo(*p2);
	len = (d < len) ? d : len;
    }

    return len;
}

uedge_t
triangle_t::shortest_edge()
{
    uedge_t ue;
    double len = DBL_MAX;
    for (int idx = 0; idx < 3; idx++) {
	long v0 = v[idx];
	long v1 = (idx < 2) ? v[idx + 1] : v[0];
	ON_3dPoint *p1 = m->pnts[v0];
	ON_3dPoint *p2 = m->pnts[v1];
	double d = p1->DistanceTo(*p2);
	if (d < len) {
	    len = d;
	    ue = uedge_t(v0, v1);
	}
    }

    return ue;
}

std::set<triangle_t>
triangle_t::split(uedge_t &ue, long split_pnt, bool flip)
{
    long A = -1;
    long B = -1;
    long C = -1;
    long E =split_pnt;
    for (int idx = 0; idx < 3; idx++) {
	long v0 = v[idx];
	long v1 = (idx < 2) ? v[idx + 1] : v[0];
	if ((v0 == ue.v[0] && v1 == ue.v[1]) || (v0 == ue.v[1] && v1 == ue.v[0])) {
	    A = v1;
	    C = v0;
	    break;
	}
    }
    for (int idx = 0; idx < 3; idx++) {
	if (v[idx] != A && v[idx] != C) {
	    B = v[idx];
	    break;
	}
    }
    if (flip) {
	long tmp = C;
	C = A;
	A = tmp;
    }

    triangle_t t1, t2;
    t1.v[0] = A;
    t1.v[1] = B;
    t1.v[2] = E;
    t1.m = m;

    t2.v[0] = B;
    t2.v[1] = C;
    t2.v[2] = E;
    t1.m = m;

    std::set<triangle_t> stris;
    stris.insert(t1);
    stris.insert(t2);

    return stris;
}

double
triangle_t::uedge_len(int idx)
{
    long v0 = v[idx];
    long v1 = (idx < 2) ? v[idx + 1] : v[0];
    ON_3dPoint *p1 = m->pnts[v0];
    ON_3dPoint *p2 = m->pnts[v1];
    return p1->DistanceTo(*p2);
}

uedge_t
triangle_t::uedge(int idx)
{
    long v0 = v[idx];
    long v1 = (idx < 2) ? v[idx + 1] : v[0];
    return uedge_t(v0, v1);
}

double
triangle_t::longest_edge_len()
{
    double len = -DBL_MAX;
    for (int idx = 0; idx < 3; idx++) {
	long v0 = v[idx];
	long v1 = (idx < 2) ? v[idx + 1] : v[0];
	ON_3dPoint *p1 = m->pnts[v0];
	ON_3dPoint *p2 = m->pnts[v1];
	double d = p1->DistanceTo(*p2);
	len = (d > len) ? d : len;
    }

    return len;
}

uedge_t
triangle_t::longest_edge()
{
    uedge_t ue;
    double len = -DBL_MAX;
    for (int idx = 0; idx < 3; idx++) {
	long v0 = v[idx];
	long v1 = (idx < 2) ? v[idx + 1] : v[0];
	ON_3dPoint *p1 = m->pnts[v0];
	ON_3dPoint *p2 = m->pnts[v1];
	double d = p1->DistanceTo(*p2);
	if (d > len) {
	    len = d;
	    ue = uedge_t(v0, v1);
	}
    }
    return ue;
}


/***************************/
/* CPolygon implementation */
/***************************/

long
cpolygon_t::add_point(ON_2dPoint &on_2dp, long orig_index)
{
    std::pair<double, double> proj_2d;
    proj_2d.first = on_2dp.x;
    proj_2d.second = on_2dp.y;
    pnts_2d.push_back(proj_2d);
    p2ind[proj_2d] = pnts_2d.size() - 1;
    p2o[pnts_2d.size() - 1] = orig_index;
    o2p[orig_index] = pnts_2d.size() - 1;
    return (long)(pnts_2d.size() - 1);
}


cpolyedge_t *
cpolygon_t::add_ordered_edge(const struct edge2d_t &e)
{
    if (e.v2d[0] == -1) return NULL;

    struct edge2d_t ne(e);
    cpolyedge_t *nedge = new cpolyedge_t(ne);
    poly.insert(nedge);

    nedge->polygon = this;

    v2pe[e.v2d[0]].insert(nedge);
    v2pe[e.v2d[1]].insert(nedge);

    cpolyedge_t *prev = NULL;
    cpolyedge_t *next = NULL;

    std::set<cpolyedge_t *>::iterator cp_it;
    for (cp_it = poly.begin(); cp_it != poly.end(); cp_it++) {
	cpolyedge_t *pe = *cp_it;

	if (pe == nedge) continue;

	if (pe->v2d[1] == nedge->v2d[0]) {
	    prev = pe;
	}

	if (pe->v2d[0] == nedge->v2d[1]) {
	    next = pe;
	}
    }

    if (prev) {
	prev->next = nedge;
	nedge->prev = prev;
    }

    if (next) {
	next->prev = nedge;
	nedge->next = next;
    }

    return nedge;
}

void
cpolygon_t::remove_ordered_edge(const struct edge2d_t &e)
{
    cpolyedge_t *cull = NULL;
    std::set<cpolyedge_t *>::iterator cp_it;
    for (cp_it = poly.begin(); cp_it != poly.end(); cp_it++) {
	cpolyedge_t *pe = *cp_it;
	struct edge2d_t oe(pe->v2d[0], pe->v2d[1]);
	if (e == oe) {
	    // Existing segment with this ending vertex exists
	    cull = pe;
	    break;
	}
    }

    if (!cull) return;

    v2pe[e.v2d[0]].erase(cull);
    v2pe[e.v2d[1]].erase(cull);

    for (cp_it = poly.begin(); cp_it != poly.end(); cp_it++) {
	cpolyedge_t *pe = *cp_it;
	if (pe->prev == cull) {
	    pe->prev = NULL;
	}
	if (pe->next == cull) {
	    pe->next = NULL;
	}
    }
    poly.erase(cull);
    delete cull;
}

cpolyedge_t *
cpolygon_t::add_edge(const struct uedge2d_t &ue)
{
    if (ue.v2d[0] == -1) return NULL;

    int v1 = -1;
    int v2 = -1;

    std::set<cpolyedge_t *>::iterator cp_it;
    for (cp_it = poly.begin(); cp_it != poly.end(); cp_it++) {
	cpolyedge_t *pe = *cp_it;

	if (pe->v2d[1] == ue.v2d[0]) {
	    v1 = ue.v2d[0];
	}

	if (pe->v2d[1] == ue.v2d[1]) {
	    v1 = ue.v2d[1];
	}

	if (pe->v2d[0] == ue.v2d[0]) {
	    v2 = ue.v2d[0];
	}

	if (pe->v2d[0] == ue.v2d[1]) {
	    v2 = ue.v2d[1];
	}
    }

    if (v1 == -1) {
	v1 = (ue.v2d[0] == v2) ? ue.v2d[1] : ue.v2d[0];
    }

    if (v2 == -1) {
	v2 = (ue.v2d[0] == v1) ? ue.v2d[1] : ue.v2d[0];
    }

    struct edge2d_t le(v1, v2);
    cpolyedge_t *nedge = new cpolyedge_t(le);
    poly.insert(nedge);

    nedge->polygon = this;

    v2pe[v1].insert(nedge);
    v2pe[v2].insert(nedge);
    active_edges.insert(uedge2d_t(v1, v2));
    used_verts.insert(v1);
    used_verts.insert(v2);

    cpolyedge_t *prev = NULL;
    cpolyedge_t *next = NULL;

    for (cp_it = poly.begin(); cp_it != poly.end(); cp_it++) {
	cpolyedge_t *pe = *cp_it;

	if (pe == nedge) continue;

	if (pe->v2d[1] == nedge->v2d[0]) {
	    prev = pe;
	}

	if (pe->v2d[0] == nedge->v2d[1]) {
	    next = pe;
	}
    }

    if (prev) {
	prev->next = nedge;
	nedge->prev = prev;
    }

    if (next) {
	next->prev = nedge;
	nedge->next = next;
    }


    return nedge;
}

void
cpolygon_t::remove_edge(const struct uedge2d_t &ue)
{
    cpolyedge_t *cull = NULL;
    std::set<cpolyedge_t *>::iterator cp_it;
    for (cp_it = poly.begin(); cp_it != poly.end(); cp_it++) {
	cpolyedge_t *pe = *cp_it;
	struct uedge2d_t pue(pe->v2d[0], pe->v2d[1]);
	if (ue == pue) {
	    // Existing segment with this ending vertex exists
	    cull = pe;
	    break;
	}
    }

    if (!cull) return;

    v2pe[ue.v2d[0]].erase(cull);
    v2pe[ue.v2d[1]].erase(cull);
    active_edges.erase(uedge2d_t(cull->v2d[0], cull->v2d[1]));

    // An edge removal may produce a new interior point candidate - check
    // Will need to verify eventually with point_in_polygon, but topologically
    // it may be cut loose
    for (int i = 0; i < 2; i++) {
	if (!v2pe[ue.v2d[i]].size()) {
	    if (flipped_face.find(ue.v2d[i]) != flipped_face.end()) {
		flipped_face.erase(ue.v2d[i]);
	    }
	    uncontained.insert(ue.v2d[i]);
	}
    }

    for (cp_it = poly.begin(); cp_it != poly.end(); cp_it++) {
	cpolyedge_t *pe = *cp_it;
	if (pe->prev == cull) {
	    pe->prev = NULL;
	}
	if (pe->next == cull) {
	    pe->next = NULL;
	}
    }
    poly.erase(cull);
    delete cull;
}

std::set<cpolyedge_t *>
cpolygon_t::replace_edges(std::set<uedge_t> &new_edges, std::set<uedge_t> &old_edges)
{
    std::set<cpolyedge_t *> nedges;

    std::set<uedge_t>::iterator e_it;
    for (e_it = old_edges.begin(); e_it != old_edges.end(); e_it++) {
	uedge_t e3d = *e_it;
	uedge2d_t ue2d(o2p[e3d.v[0]], o2p[e3d.v[1]]);
	remove_edge(ue2d);
    }
    for (e_it = new_edges.begin(); e_it != new_edges.end(); e_it++) {
	uedge_t e3d = *e_it;
	uedge2d_t ue2d(o2p[e3d.v[0]], o2p[e3d.v[1]]);
	cpolyedge_t *ne = add_edge(ue2d);
	nedges.insert(ne);
    }

    return nedges;
}

long
cpolygon_t::shared_edge_cnt(triangle_t &t)
{
    struct uedge2d_t ue[3];
    ue[0].set(o2p[t.v[0]], o2p[t.v[1]]);
    ue[1].set(o2p[t.v[1]], o2p[t.v[2]]);
    ue[2].set(o2p[t.v[2]], o2p[t.v[0]]);
    long shared_cnt = 0;
    for (int i = 0; i < 3; i++) {
	if (active_edges.find(ue[i]) != active_edges.end()) {
	    shared_cnt++;
	}
    }
    return shared_cnt;
}

long
cpolygon_t::unshared_vertex(triangle_t &t)
{
    if (shared_edge_cnt(t) != 1) return -1;

    for (int i = 0; i < 3; i++) {
	if (v2pe.find(o2p[t.v[i]]) == v2pe.end()) {
	    return t.v[i];
	}
	// TODO - need to check C++ map container behavior - if the set
	// a key points to is fully cleared, will the above test work?
	// (if it finds the empty set successfully it doesn't do what
	// we need...)
	if (v2pe[o2p[t.v[i]]].size() == 0) {
	    return t.v[i];
	}
    }

    return -1;
}

std::pair<long,long>
cpolygon_t::shared_vertices(triangle_t &t)
{
    if (shared_edge_cnt(t) != 1) {
	return std::make_pair<long,long>(-1,-1);
    }

    std::pair<long, long> ret;

    int vcnt = 0;
    for (int i = 0; i < 3; i++) {
	if (v2pe.find(o2p[t.v[i]]) != v2pe.end()) {
	    if (!vcnt) {
		ret.first = t.v[i];
		vcnt++;
	    } else {
		ret.second = t.v[i];
	    }
	}
    }

    return ret;
}

double
cpolygon_t::ucv_angle(triangle_t &t)
{
    double r_ang = DBL_MAX;
    std::set<long>::iterator u_it;
    long nv = unshared_vertex(t);
    if (nv == -1) return -1;
    nv = o2p[nv]; // Need 2D point index, but unshared_vertex returns tri vert (3D).
    std::pair<long, long> s_vert = shared_vertices(t);
    if (s_vert.first == -1 || s_vert.second == -1) return -1;

    ON_3dPoint ep1 = ON_3dPoint(pnts_2d[s_vert.first].first, pnts_2d[s_vert.first].second, 0);
    ON_3dPoint ep2 = ON_3dPoint(pnts_2d[s_vert.second].first, pnts_2d[s_vert.second].second, 0);
    ON_3dPoint pnew = ON_3dPoint(pnts_2d[nv].first, pnts_2d[nv].second, 0);
    ON_Line l2d(ep1,ep2);
    ON_3dPoint pline = l2d.ClosestPointTo(pnew);
    ON_3dVector vu = pnew - pline;
    vu.Unitize();

    for (u_it = uncontained.begin(); u_it != uncontained.end(); u_it++) {
	if (point_in_polygon(*u_it, true)) {
	    ON_2dPoint op = ON_2dPoint(pnts_2d[*u_it].first, pnts_2d[*u_it].second);
	    ON_3dVector vt = op - pline;

	    // If vt is almost on l2d, we want this triangle - there's an excellent chance
	    // this triangle will contain it, but the unitized vector direction may be unreliable
	    // if our starting vector is vanishingly short...
	    if (vt.Length() < 0.01 * l2d.Length()) return ON_ZERO_TOLERANCE;

	    // Not almost on the edge, but if it's still heading in the direction we need to go
	    // we want to know.  Unitize and proceed.
	    vt.Unitize();
	    double vangle = ON_DotProduct(vu, vt);
	    if (vangle > 0 && r_ang > vangle) {
		r_ang = vangle;
	    }
	}
    }
    for (u_it = flipped_face.begin(); u_it != flipped_face.end(); u_it++) {
	if (point_in_polygon(*u_it, true)) {
	    ON_2dPoint op = ON_2dPoint(pnts_2d[*u_it].first, pnts_2d[*u_it].second);
	    ON_3dVector vt = op - pline;

	    // If vt is almost on l2d, we want this triangle - there's an excellent chance
	    // this triangle will contain it, but the unitized vector direction may be unreliable
	    // if our starting vector is vanishingly short...
	    if (vt.Length() < 0.01 * l2d.Length()) return ON_ZERO_TOLERANCE;

	    // Not almost on the edge, but if it's still heading in the direction we need to go
	    // we want to know.  Unitize and proceed.
	    vt.Unitize();
	    double vangle = ON_DotProduct(vu, vt);
	    if (vangle > 0 && r_ang > vangle) {
		r_ang = vangle;
	    }
	}
    }

    for (u_it = target_verts.begin(); u_it != target_verts.end(); u_it++) {
	if (point_in_polygon(*u_it, true)) {
	    ON_2dPoint op = ON_2dPoint(pnts_2d[*u_it].first, pnts_2d[*u_it].second);
	    ON_3dVector vt = op - pline;

	    // If vt is almost on l2d, we want this triangle - there's an excellent chance
	    // this triangle will contain it, but the unitized vector direction may be unreliable
	    // if our starting vector is vanishingly short...
	    if (vt.Length() < 0.01 * l2d.Length()) return ON_ZERO_TOLERANCE;

	    // Not almost on the edge, but if it's still heading in the direction we need to go
	    // we want to know.  Unitize and proceed.
	    vt.Unitize();
	    double vangle = ON_DotProduct(vu, vt);
	    if (vangle > 0 && r_ang > vangle) {
		r_ang = vangle;
	    }
	}
    }

    return r_ang;
}


bool
cpolygon_t::self_intersecting()
{
    self_isect_edges.clear();
    bool self_isect = false;
    std::map<long, int> vecnt;
    std::set<cpolyedge_t *>::iterator pe_it;
    for (pe_it = poly.begin(); pe_it != poly.end(); pe_it++) {
	cpolyedge_t *pe = *pe_it;
	vecnt[pe->v2d[0]]++;
	vecnt[pe->v2d[1]]++;
    }
    std::map<long, int>::iterator v_it;
    for (v_it = vecnt.begin(); v_it != vecnt.end(); v_it++) {
	if (v_it->second > 2) {
	    self_isect = true;
	    if (brep_edge_pnts.find(v_it->second) == brep_edge_pnts.end()) {
		uncontained.insert(v_it->second);
	    }
	}
    }

    // Check the projected segments against each other as well.  Store any
    // self-intersecting edges for use in later repair attempts.
    std::vector<cpolyedge_t *> pv(poly.begin(), poly.end());
    for (size_t i = 0; i < pv.size(); i++) {
	cpolyedge_t *pe1 = pv[i];
	ON_2dPoint p1_1(pnts_2d[pe1->v2d[0]].first, pnts_2d[pe1->v2d[0]].second);
	ON_2dPoint p1_2(pnts_2d[pe1->v2d[1]].first, pnts_2d[pe1->v2d[1]].second);
	struct uedge2d_t ue1(pe1->v2d[0], pe1->v2d[1]);
	// if we already know this segment intersects at least one other segment, we
	// don't need to re-test it - it's already "active"
	if (self_isect_edges.find(ue1) != self_isect_edges.end()) continue;
	ON_BoundingBox e1b(p1_1, p1_2);
	ON_Line e1(p1_1, p1_2);
	for (size_t j = i+1; j < pv.size(); j++) {
	    cpolyedge_t *pe2 = pv[j];
	    ON_2dPoint p2_1(pnts_2d[pe2->v2d[0]].first, pnts_2d[pe2->v2d[0]].second);
	    ON_2dPoint p2_2(pnts_2d[pe2->v2d[1]].first, pnts_2d[pe2->v2d[1]].second);
	    struct uedge_t ue2(pe2->v2d[0], pe2->v2d[1]);
	    ON_BoundingBox e2b(p2_1, p2_2);
	    ON_Line e2(p2_1, p2_2);

	    if (e1b.IsDisjoint(e2b)) {
		continue;
	    }

	    double a, b = 0;
	    if (!ON_IntersectLineLine(e1, e2, &a, &b, 0.0, false)) {
		continue;
	    }

	    if ((a < 0 || NEAR_ZERO(a, ON_ZERO_TOLERANCE) || a > 1 || NEAR_ZERO(1-a, ON_ZERO_TOLERANCE)) ||
		(b < 0 || NEAR_ZERO(b, ON_ZERO_TOLERANCE) || b > 1 || NEAR_ZERO(1-b, ON_ZERO_TOLERANCE))) {
		continue;
	    }
	    self_isect = true;
	}
    }

    return self_isect;
}

bool
cpolygon_t::closed()
{
    if (poly.size() < 3) {
	return false;
    }

    if (flipped_face.size()) {
	return false;
    }

    if (self_intersecting()) {
	return false;
    }

    size_t ecnt = 1;
    cpolyedge_t *pe = (*poly.begin());
    cpolyedge_t *first = pe;
    cpolyedge_t *next = pe->next;

    // Walk the loop - an infinite loop is not closed
    while (first != next) {
	ecnt++;
	next = next->next;
	if (ecnt > poly.size()) {
	    return false;
	}
    }

    // If we're not using all the poly edges in the loop, we're not closed
    if (ecnt < poly.size()) {
	return false;
    }

    // Prev and next need to be set for all poly edges, or we're not closed
    std::set<cpolyedge_t *>::iterator cp_it;
    for (cp_it = poly.begin(); cp_it != poly.end(); cp_it++) {
	cpolyedge_t *pec = *cp_it;
	if (!pec->prev || !pec->next) {
	    return false;
	}
    }

    return true;
}

long
cpolygon_t::bg_polygon(point2d_t **ppnts)
{
    if (!closed()) return -1;

    point2d_t *polypnts = (point2d_t *)bu_calloc(poly.size()+1, sizeof(point2d_t), "polyline");

    long pind = 0;

    cpolyedge_t *pe = (*poly.begin());
    cpolyedge_t *first = pe;
    cpolyedge_t *next = pe->next;

    V2SET(polypnts[pind], pnts_2d[pe->v2d[0]].first, pnts_2d[pe->v2d[0]].second);
    pind++;
    V2SET(polypnts[pind], pnts_2d[pe->v2d[1]].first, pnts_2d[pe->v2d[1]].second);

    // Walk the loop
    while (first != next) {
	pind++;
	V2SET(polypnts[pind], pnts_2d[next->v2d[1]].first, pnts_2d[next->v2d[1]].second);
	next = next->next;
	if ((size_t)pind > poly.size()+1) {
	    std::cout << "\nERROR infinite loop\n";
	    bu_free(polypnts, "free polypnts");
	    return -1;
	}
    }

    (*ppnts) = polypnts;

    return pind;
}

bool
cpolygon_t::point_in_polygon(long v, bool flip)
{
    if (v == -1) return false;
    if (!closed()) return false;

    cpolyedge_t *pe = (*poly.begin());
    if (v == pe->v2d[0] || v == pe->v2d[1]) {
	return true;
    }

    point2d_t *polypnts = NULL;
    long pind = bg_polygon(&polypnts);
    if (pind < 0) return false;

#if 0
    if (bg_polygon_direction(pind+1, pnts_2d, NULL) == BG_CCW) {
	point2d_t *rpolypnts = (point2d_t *)bu_calloc(poly.size()+1, sizeof(point2d_t), "polyline");
	for (long p = (long)pind; p >= 0; p--) {
	    V2MOVE(rpolypnts[pind - p], polypnts[p]);
	}
	bu_free(polypnts, "free original loop");
	polypnts = rpolypnts;
    }
#endif

    //bg_polygon_plot_2d("bg_pnt_in_poly_loop.plot3", polypnts, pind, 255, 0, 0);

    point2d_t test_pnt;
    V2SET(test_pnt, pnts_2d[v].first, pnts_2d[v].second);

    bool result = (bool)bg_pnt_in_polygon(pind, (const point2d_t *)polypnts, (const point2d_t *)&test_pnt);

    if (flip) {
	result = (result) ? false : true;
    }

    bu_free(polypnts, "polyline");

    return result;
}

void
cpolygon_t::rm_points_in_polygon(std::set<ON_2dPoint *> *pnts, bool flip)
{
    if (!closed() || !pnts || !pnts->size()) return;

    point2d_t *polypnts = (point2d_t *)bu_calloc(poly.size()+1, sizeof(point2d_t), "polyline");

    size_t pind = 0;

    cpolyedge_t *pe = (*poly.begin());
    cpolyedge_t *first = pe;
    cpolyedge_t *next = pe->next;

    V2SET(polypnts[pind], pnts_2d[pe->v2d[0]].first, pnts_2d[pe->v2d[0]].second);
    pind++;
    V2SET(polypnts[pind], pnts_2d[pe->v2d[1]].first, pnts_2d[pe->v2d[1]].second);

    // Walk the loop
    while (first != next) {
	pind++;
	V2SET(polypnts[pind], pnts_2d[next->v2d[1]].first, pnts_2d[next->v2d[1]].second);
	next = next->next;
    }

    std::set<ON_2dPoint *> rm_pnts;
    std::set<ON_2dPoint *>::iterator p_it;

    for (p_it = pnts->begin(); p_it != pnts->end(); p_it++) {
	ON_2dPoint *p2d = *p_it;
	point2d_t test_pnt;
	V2SET(test_pnt, p2d->x, p2d->y);

	bool result = (bool)bg_pnt_in_polygon(pind, (const point2d_t *)polypnts, (const point2d_t *)&test_pnt);

	if (flip) {
	    result = (result) ? false : true;
	}

	if (result) {
	    rm_pnts.insert(p2d);
	}
    }
    for (p_it = rm_pnts.begin(); p_it != rm_pnts.end(); p_it++) {
	pnts->erase(*p_it);
    }

    bu_free(polypnts, "polyline");
}

void cpolygon_t::cdt_inputs_print(const char *filename)
{
    std::ofstream sfile(filename);

    if (!sfile.is_open()) {
	std::cerr << "Could not open file " << filename << " for writing\n";
	return;
    }

    sfile << "#include <stdio.h>\n";
    sfile << "#include \"bu/malloc.h\"\n";
    sfile << "#include \"bg/polygon.h\"\n";
    sfile << "int main() {\n";
    sfile << "point2d_t *pnts_2d = (point2d_t *)bu_calloc(" << pnts_2d.size()+1 << ", sizeof(point2d_t), \"2D points array\");\n";

    for (size_t i = 0; i < pnts_2d.size(); i++) {
	sfile << "pnts_2d[" << i << "][X] = ";
	sfile << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << pnts_2d[i].first << ";\n";
	sfile << "pnts_2d[" << i << "][Y] = ";
	sfile << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << pnts_2d[i].second << ";\n";
    }

    sfile << "int *faces = NULL;\nint num_faces = 0;int *steiner = NULL;\n";

    std::set<long> oloop_pnts;

    sfile << "int *opoly = (int *)bu_calloc(" << poly.size()+1 << ", sizeof(int), \"polygon points\");\n";

    size_t vcnt = 1;
    cpolyedge_t *pe = (*poly.begin());
    cpolyedge_t *first = pe;
    cpolyedge_t *next = pe->next;

    sfile << "opoly[" << vcnt-1 << "] = " << pe->v2d[0] << ";\n";
    sfile << "opoly[" << vcnt << "] = " << pe->v2d[1] << ";\n";
    oloop_pnts.insert(pe->v2d[0]);
    oloop_pnts.insert(pe->v2d[1]);

    // Walk the loop
    while (first != next) {
	vcnt++;
	sfile << "opoly[" << vcnt << "] = " << next->v2d[1] << ";\n";
	oloop_pnts.insert(next->v2d[1]);
	next = next->next;
	if (vcnt > poly.size()) {
	    return;
	}
    }

    if (interior_points.size()) {
	std::set<long> erase;
	std::set<long>::iterator p_it;
	for (p_it = interior_points.begin(); p_it != interior_points.end(); p_it++) {
	    if (oloop_pnts.find(*p_it) != oloop_pnts.end()) {
		erase.insert(*p_it);
	    }
	}
	for (p_it = erase.begin(); p_it != erase.end(); p_it++) {
	    interior_points.erase(*p_it);
	}
    }

    size_t steiner_cnt = interior_points.size();
    if (steiner_cnt) {
	sfile << "steiner = (int *)bu_calloc(" << interior_points.size() << ", sizeof(int), \"interior points\");\n";
	std::set<long>::iterator p_it;
	int vind = 0;
	for (p_it = interior_points.begin(); p_it != interior_points.end(); p_it++) {
	    sfile << "steiner[" << vind << "] = " << *p_it << ";\n";
	    vind++;
	}
    }


    sfile << "int result = !bg_nested_poly_triangulate(&faces, &num_faces,\n";
    sfile << "             NULL, NULL, opoly, " << poly.size()+1 << ", NULL, NULL, 0,\n";
    sfile << "             steiner, " << interior_points.size() << ", pnts_2d, " << pnts_2d.size() << ", TRI_CONSTRAINED_DELAUNAY);\n";
    sfile << "if (result) printf(\"success\\n\");\n";
    sfile << "if (!result) printf(\"FAIL\\n\");\n";
    sfile << "return !result;\n";
    sfile << "};\n";

    sfile.close();
}

bool
cpolygon_t::cdt(triangulation_t ttype)
{
    if (!closed()) return false;

    point2d_t *bgp_2d = (point2d_t *)bu_calloc(pnts_2d.size() + 1, sizeof(point2d_t), "2D points array");
    for (size_t i = 0; i < pnts_2d.size(); i++) {
	V2SET(bgp_2d[i], pnts_2d[i].first, pnts_2d[i].second);
    }

    int *faces = NULL;
    int num_faces = 0;
    int *steiner = NULL;
    std::set<long> oloop_pnts;

    int *opoly = (int *)bu_calloc(poly.size()+1, sizeof(int), "polygon points");

    size_t vcnt = 1;
    cpolyedge_t *pe = (*poly.begin());
    cpolyedge_t *first = pe;
    cpolyedge_t *next = pe->next;

    opoly[vcnt-1] = pe->v2d[0];
    opoly[vcnt] = pe->v2d[1];
    oloop_pnts.insert(pe->v2d[0]);
    oloop_pnts.insert(pe->v2d[1]);

    // Walk the loop
    while (first != next) {
	vcnt++;
	opoly[vcnt] = next->v2d[1];
	oloop_pnts.insert(next->v2d[1]);
	next = next->next;
	if (vcnt > poly.size()) {
	    bu_free(bgp_2d, "free libbg 2d points array)");
	    std::cerr << "cdt attempt on infinite loop (shouldn't be possible - closed() test failed to detect this somehow...)\n";
	    return false;
	}
    }

   if (interior_points.size()) {
       std::set<long> erase;
       std::set<long>::iterator p_it;
       for (p_it = interior_points.begin(); p_it != interior_points.end(); p_it++) {
	  if (oloop_pnts.find(*p_it) != oloop_pnts.end()) {
	      erase.insert(*p_it);
	  }
       }
       for (p_it = erase.begin(); p_it != erase.end(); p_it++) {
	   interior_points.erase(*p_it);
      }
   }

   size_t steiner_cnt = interior_points.size();
   if (steiner_cnt) {
       steiner = (int *)bu_calloc(steiner_cnt, sizeof(int), "interior points");
       std::set<long>::iterator p_it;
       int vind = 0;
       for (p_it = interior_points.begin(); p_it != interior_points.end(); p_it++) {
	   steiner[vind] = (int)*p_it;
	   vind++;
       }
   }

    bool result = (bool)!bg_nested_poly_triangulate(&faces, &num_faces,
		  NULL, NULL, opoly, poly.size()+1, NULL, NULL, 0, steiner,
		  steiner_cnt, bgp_2d, pnts_2d.size(),
		  ttype);

    if (!result) {
	// Dump a stand-alone C test file so the failure can be reproduced
	// independently of the full CDT pipeline.
	static int patch_fail_cnt = 0;
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fname, "cdt_patch_fail_%03d.c", patch_fail_cnt++);
	cdt_inputs_print(bu_vls_cstr(&fname));
	bu_log("patch CDT failure inputs written to %s\n", bu_vls_cstr(&fname));
	bu_vls_free(&fname);
    }

    if (result) {
	for (int i = 0; i < num_faces; i++) {
	    triangle_t t;
	    t.v[0] = faces[3*i+0];
	    t.v[1] = faces[3*i+1];
	    t.v[2] = faces[3*i+2];

	    bool inside = true;
	    if (ttype == TRI_DELAUNAY) {
		double u = 0;
		double v = 0;
		for (int j = 0; j < 3; j++) {
		    u = u + pnts_2d[t.v[j]].first;
		    v = v + pnts_2d[t.v[j]].second;
		}
		u = u / 3.0;
		v = v / 3.0;
		std::pair<double, double> center_2d;
		center_2d.first = u; 
		center_2d.second = v;
		pnts_2d.push_back(center_2d);
		inside = point_in_polygon(pnts_2d.size() - 1, false);
		pnts_2d.pop_back();
	    }

	    if (inside) {
		ltris.insert(t);
	    }
	}

	bu_free(faces, "faces array");
    }

    bu_free(bgp_2d, "free libbg 2d points array)");
    bu_free(opoly, "polygon points");

    if (steiner) {
	bu_free(steiner, "faces array");
    }

    // Map the local polygon triangles to triangles described using the
    // original point indices (may be an identify operation if the polygon
    // expresses all the original points in the same order).
    std::set<triangle_t>::iterator tr_it;
    for (tr_it = ltris.begin(); tr_it != ltris.end(); tr_it++) {
	triangle_t otri = *tr_it;
	long nv1 = p2o[otri.v[0]];
	long nv2 = p2o[otri.v[1]];
	long nv3 = p2o[otri.v[2]];
	triangle_t ntri(nv1, nv2, nv3);
	tris.insert(ntri);
    }

    return result;
}

void cpolygon_t::polygon_plot(const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    point2d_t pmin, pmax;
    V2SET(pmin, DBL_MAX, DBL_MAX);
    V2SET(pmax, -DBL_MAX, -DBL_MAX);

    cpolyedge_t *efirst = *(poly.begin());
    cpolyedge_t *ecurr = NULL;

    point_t bnp;
    VSET(bnp, pnts_2d[efirst->v2d[0]].first, pnts_2d[efirst->v2d[0]].second, 0);
    pdv_3move(plot_file, bnp);
    VSET(bnp, pnts_2d[efirst->v2d[1]].first, pnts_2d[efirst->v2d[1]].second, 0);
    pdv_3cont(plot_file, bnp);

    point2d_t wpnt;
    V2SET(wpnt, pnts_2d[efirst->v2d[0]].first, pnts_2d[efirst->v2d[0]].second);
    V2MINMAX(pmin, pmax, wpnt);
    V2SET(wpnt, pnts_2d[efirst->v2d[1]].first, pnts_2d[efirst->v2d[1]].second);
    V2MINMAX(pmin, pmax, wpnt);

    size_t ecnt = 1;
    while (ecurr != efirst && ecnt < poly.size()+1) {
	ecnt++;
	ecurr = (!ecurr) ? efirst->next : ecurr->next;
	VSET(bnp, pnts_2d[ecurr->v2d[1]].first, pnts_2d[ecurr->v2d[1]].second, 0);
	pdv_3cont(plot_file, bnp);
	V2SET(wpnt, pnts_2d[ecurr->v2d[1]].first, pnts_2d[ecurr->v2d[1]].second);
	V2MINMAX(pmin, pmax, wpnt);
	if (ecnt > poly.size()) {
	    break;
	}
    }

    // Plot interior and uncontained points as well
    double r = DIST_PNT2_PNT2(pmin, pmax) * 0.01;
    std::set<long>::iterator p_it;

    // Interior
    pl_color(plot_file, 0, 255, 0);
    for (p_it = interior_points.begin(); p_it != interior_points.end(); p_it++) {
	point_t origin;
	VSET(origin, pnts_2d[*p_it].first, pnts_2d[*p_it].second, 0);
	pdv_3move(plot_file, origin);
	VSET(bnp, pnts_2d[*p_it].first+r, pnts_2d[*p_it].second+r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, pnts_2d[*p_it].first+r, pnts_2d[*p_it].second-r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, pnts_2d[*p_it].first-r, pnts_2d[*p_it].second-r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, pnts_2d[*p_it].first-r, pnts_2d[*p_it].second+r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
    }

    // Uncontained
    pl_color(plot_file, 255, 0, 0);
    for (p_it = uncontained.begin(); p_it != uncontained.end(); p_it++) {
	point_t origin;
	VSET(origin, pnts_2d[*p_it].first, pnts_2d[*p_it].second, 0);
	pdv_3move(plot_file, origin);
	VSET(bnp, pnts_2d[*p_it].first+r, pnts_2d[*p_it].second+r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, pnts_2d[*p_it].first+r, pnts_2d[*p_it].second-r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, pnts_2d[*p_it].first-r, pnts_2d[*p_it].second-r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, pnts_2d[*p_it].first-r, pnts_2d[*p_it].second+r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
    }

    fclose(plot_file);
}


void cpolygon_t::polygon_plot_in_plane(const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    ON_3dPoint ppnt;
    point_t pmin, pmax;
    point_t bnp;
    VSET(pmin, DBL_MAX, DBL_MAX, DBL_MAX);
    VSET(pmax, -DBL_MAX, -DBL_MAX, -DBL_MAX);

    cpolyedge_t *efirst = *(poly.begin());
    cpolyedge_t *ecurr = NULL;

    ppnt = tplane.PointAt(pnts_2d[efirst->v2d[0]].first, pnts_2d[efirst->v2d[0]].second);
    VSET(bnp, ppnt.x, ppnt.y, ppnt.z);
    pdv_3move(plot_file, bnp);
    VMINMAX(pmin, pmax, bnp);
    ppnt = tplane.PointAt(pnts_2d[efirst->v2d[1]].first, pnts_2d[efirst->v2d[1]].second);
    VSET(bnp, ppnt.x, ppnt.y, ppnt.z);
    pdv_3cont(plot_file, bnp);
    VMINMAX(pmin, pmax, bnp);

    size_t ecnt = 1;
    while (ecurr != efirst && ecnt < poly.size()+1) {
	ecnt++;
	ecurr = (!ecurr) ? efirst->next : ecurr->next;
	ppnt = tplane.PointAt(pnts_2d[ecurr->v2d[1]].first, pnts_2d[ecurr->v2d[1]].second);
	VSET(bnp, ppnt.x, ppnt.y, ppnt.z);
	pdv_3cont(plot_file, bnp);
	VMINMAX(pmin, pmax, bnp);
	if (ecnt > poly.size()) {
	    break;
	}
    }

    // Plot interior and uncontained points as well
    double r = DIST_PNT_PNT(pmin, pmax) * 0.01;
    std::set<long>::iterator p_it;

    // Interior
    pl_color(plot_file, 0, 255, 0);
    for (p_it = interior_points.begin(); p_it != interior_points.end(); p_it++) {
	point_t origin;
	ppnt = tplane.PointAt(pnts_2d[*p_it].first, pnts_2d[*p_it].second);
	VSET(origin, ppnt.x, ppnt.y, ppnt.z);
	pdv_3move(plot_file, origin);
	ppnt = tplane.PointAt(pnts_2d[*p_it].first+r, pnts_2d[*p_it].second+r);
	VSET(bnp, ppnt.x, ppnt.y, ppnt.z);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	ppnt = tplane.PointAt(pnts_2d[*p_it].first+r, pnts_2d[*p_it].second-r);
	VSET(bnp, ppnt.x, ppnt.y, ppnt.z);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	ppnt = tplane.PointAt(pnts_2d[*p_it].first-r, pnts_2d[*p_it].second-r);
	VSET(bnp, ppnt.x, ppnt.y, ppnt.z);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	ppnt = tplane.PointAt(pnts_2d[*p_it].first-r, pnts_2d[*p_it].second+r);
	VSET(bnp, ppnt.x, ppnt.y, ppnt.z);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
    }

    // Uncontained
    pl_color(plot_file, 255, 0, 0);
    for (p_it = uncontained.begin(); p_it != uncontained.end(); p_it++) {
	point_t origin;
	ppnt = tplane.PointAt(pnts_2d[*p_it].first, pnts_2d[*p_it].second);
	VSET(origin, ppnt.x, ppnt.y, ppnt.z);
	pdv_3move(plot_file, origin);
	ppnt = tplane.PointAt(pnts_2d[*p_it].first+r, pnts_2d[*p_it].second+r);
	VSET(bnp, ppnt.x, ppnt.y, ppnt.z);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	ppnt = tplane.PointAt(pnts_2d[*p_it].first+r, pnts_2d[*p_it].second-r);
	VSET(bnp, ppnt.x, ppnt.y, ppnt.z);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	ppnt = tplane.PointAt(pnts_2d[*p_it].first-r, pnts_2d[*p_it].second-r);
	VSET(bnp, ppnt.x, ppnt.y, ppnt.z);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	ppnt = tplane.PointAt(pnts_2d[*p_it].first-r, pnts_2d[*p_it].second+r);
	VSET(bnp, ppnt.x, ppnt.y, ppnt.z);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
    }

    fclose(plot_file);
}

void cpolygon_t::print()
{

    size_t ecnt = 1;
    if (!poly.size()) return;
    cpolyedge_t *pe = (*poly.begin());
    cpolyedge_t *first = pe;
    cpolyedge_t *next = pe->next;

    std::set<cpolyedge_t *> visited;
    visited.insert(first);

    std::cout << first->v2d[0];

    // Walk the loop - an infinite loop is not closed
    while (first != next) {
	ecnt++;
	if (!next) {
	    break;
	}
	std::cout << "->" << next->v2d[0];
	visited.insert(next);
	next = next->next;
	if (ecnt > poly.size()) {
	    std::cout << "\nERROR infinite loop\n";
	    break;
	}
    }

    std::cout << "\n";

    visited.clear();

    pe = (*poly.begin());
    first = pe;
    next = pe->next;
    visited.insert(first);
    ecnt = 1;
    std::cout << p2o[first->v2d[0]];
    // Walk the loop - an infinite loop is not closed
    while (first != next) {
	ecnt++;
	if (!next) {
	    break;
	}
	std::cout << "->" << p2o[next->v2d[0]];
	visited.insert(next);
	next = next->next;
	if (ecnt > poly.size()) {
	    std::cout << "\nERROR infinite loop\n";
	    break;
	}
    }

    std::cout << "\n";

    visited.clear();



    pe = (*poly.begin());
    first = pe;
    next = pe->next;
    visited.insert(first);
    std::cout << "(" << pnts_2d[first->v2d[0]].first << "," << pnts_2d[first->v2d[0]].second << ")" ;
    ecnt = 1;
    // Walk the loop - an infinite loop is not closed
    while (first != next) {
	ecnt++;
	if (!next) {
	    break;
	}
	std::cout << "->";
	std::cout << "(" << pnts_2d[next->v2d[0]].first << "," << pnts_2d[next->v2d[0]].second << ")" ;
	visited.insert(next);
	next = next->next;
	if (ecnt > poly.size()) {
	    std::cout << "\nERROR infinite loop\n";
	    break;
	}
    }
    std::cout << "\n";


    if (visited.size() != poly.size()) {
	std::cout << "Missing edges:\n";
	std::set<cpolyedge_t *>::iterator p_it;
	for (p_it = poly.begin(); p_it != poly.end(); p_it++) {
	    if (visited.find(*p_it) == visited.end()) {
		std::cout << "  " << (*p_it)->v2d[0] << "->" << (*p_it)->v2d[1] << "\n";
	    }
	}
    }
}

bool
cpolygon_t::update_uncontained()
{
    std::set<long>::iterator u_it;

    if (!closed()) return true;

    if (!uncontained.size()) return false;

    std::set<long> mvpnts;

    for (u_it = uncontained.begin(); u_it != uncontained.end(); u_it++) {
	if (point_in_polygon(*u_it, false)) {
	    mvpnts.insert(*u_it);
	}
    }
    for (u_it = mvpnts.begin(); u_it != mvpnts.end(); u_it++) {
	uncontained.erase(*u_it);
	interior_points.insert(*u_it);
	used_verts.insert(*u_it);
    }

    return (uncontained.size() > 0) ? true : false;
}


extern "C" {
    static int
    ctriangle_cmp(const void *p1, const void *p2, void *UNUSED(arg))
    {
	struct ctriangle_t *t1 = *(struct ctriangle_t **)p1;
	struct ctriangle_t *t2 = *(struct ctriangle_t **)p2;
	if (t1->isect_edge && !t2->isect_edge) return 1;
	if (!t1->isect_edge && t2->isect_edge) return 0;
	if (t1->uses_uncontained && !t2->uses_uncontained) return 1;
	if (!t1->uses_uncontained && t2->uses_uncontained) return 0;
	if (t1->contains_uncontained && !t2->contains_uncontained) return 1;
	if (!t1->contains_uncontained && t2->contains_uncontained) return 0;
	if (t1->all_bedge && !t2->all_bedge) return 1;
	if (!t1->all_bedge && t2->all_bedge) return 0;
	if (t1->angle_to_nearest_uncontained > t2->angle_to_nearest_uncontained) return 1;
	if (t1->angle_to_nearest_uncontained < t2->angle_to_nearest_uncontained) return 0;
	bool c1 = (t1->v[0] < t2->v[0]);
	bool c1e = (t1->v[0] == t2->v[0]);
	bool c2 = (t1->v[1] < t2->v[1]);
	bool c2e = (t1->v[1] == t2->v[1]);
	bool c3 = (t1->v[2] < t2->v[2]);
	return (c1 || (c1e && c2) || (c1e && c2e && c3));
    }
}

bool
ctriangle_vect_cmp(std::vector<ctriangle_t> &v1, std::vector<ctriangle_t> &v2)
{
    if (v1.size() != v2.size()) return false;

    for (size_t i = 0; i < v1.size(); i++) {
	for (int j = 0; j < 3; j++) {
	    if (v1[i].v[j] != v2[i].v[j]) return false;
	}
    }

    return true;
}

/***************************/
/* CDT_Mesh implementation */
/***************************/

long
cdt_mesh_t::add_point(ON_2dPoint &on_2dp)
{
    std::pair<double, double> proj_2d;
    proj_2d.first = on_2dp.x;
    proj_2d.second = on_2dp.y;
    m_pnts_2d.push_back(proj_2d);
    return (long)(m_pnts_2d.size() - 1);
}

long
cdt_mesh_t::add_point(ON_3dPoint *on_3dp)
{
    pnts.push_back(on_3dp);
    p2ind[on_3dp] = pnts.size() - 1;
    return (long)(pnts.size() - 1);
}

long
cdt_mesh_t::add_normal(ON_3dPoint *on_3dn)
{
    normals.push_back(on_3dn);
    n2ind[on_3dn] = normals.size() - 1;
    return (long)(normals.size() - 1);
}

static bool NearTrisCallbackTriAdd(size_t data, void *a_context) {
    std::vector<size_t> *near_tris = (std::vector<size_t> *)a_context;
    near_tris->push_back(data);
    return true;
}

// TODO - perf reports a huge percentage of time is spend dealing with
// tri_add - we should only need the uniqueness guarantee for triangles
// associated with singularities, so it might be worth trying to make this
// more nuanced and see if we can avoid the need for a general set container
// for all triangles.
bool
cdt_mesh_t::tri_add(triangle_t &tri)
{

    // Skip degenerate triangles, but report true since this was never
    // a valid triangle in the first place
    if (tri.v[0] == tri.v[1] || tri.v[1] == tri.v[2] || tri.v[2] == tri.v[0]) {
	return true;
    }

    // Tell the triangle what mesh it is part of
    tri.m = this;


    ON_3dPoint *p3d = pnts[tri.v[0]];
    ON_BoundingBox bb(*p3d, *p3d);
    for (int i = 1; i < 3; i++) {
	p3d = pnts[tri.v[i]];
	bb.Set(*p3d, true);
    }

    double fMin[3];
    fMin[0] = bb.Min().x;
    fMin[1] = bb.Min().y;
    fMin[2] = bb.Min().z;
    double fMax[3];
    fMax[0] = bb.Max().x;
    fMax[1] = bb.Max().y;
    fMax[2] = bb.Max().z;

    std::vector<size_t> near_tris;
    size_t nhits = tris_tree.Search(fMin, fMax, NearTrisCallbackTriAdd, &near_tris);

    if (nhits) {
	// We've got something nearby, see if any of them are duplicates
	std::vector<size_t>::iterator t_it;
	for (t_it = near_tris.begin(); t_it != near_tris.end(); t_it++) {
	    triangle_t orig = tris_vect[*t_it];
	    if (tri == orig) {
		// Duplicate.  Check the normals of the original and the
		// candidate.  If the original is flipped and the new one
		// isn't, swap them out - this will help with subsequent
		// processing.
		std::cout << "Dup: orig: " << orig.v[0] << "," << orig.v[1] << "," << orig.v[2] << "\n";
		std::cout << "Dup:  new: " << tri.v[0] << "," << tri.v[1] << "," << tri.v[2] << "\n";
		ON_3dVector torig_dir = tnorm(orig);
		ON_3dVector tnew_dir = tnorm(tri);
		ON_3dVector bdir = tnorm(orig);
		bool f1 = (ON_DotProduct(torig_dir, bdir) < 0);
		bool f2 = (ON_DotProduct(tnew_dir, bdir) < 0);
		if (f1 && !f2) {
		    tri_remove(orig);
		    std::cout << "remove dup\n";
		} else {
		    std::cout << "skip dup\n";
		    return true;
		}
		break;
	    }
	}
    }

    // Add the triangle
    tri.ind = tris_vect.size();
    tris_vect.push_back(tri);
    tris_tree.Insert(fMin, fMax, tri.ind);

#if 0
    int pcnt = 0;
    ON_3dPoint problem1(2.5989674496614921,7.8208160252273462,26.533125750337135);
    ON_3dPoint problem2(2.5989674496614921,7.8208160252273462,25.033125750337135);
    for (int ind = 0; ind < 3; ind++) {
	ON_3dPoint p = *pnts[tri.v[ind]];
	if (problem1.DistanceTo(p) < 0.01) {
	    pcnt++;
	    continue;
	}
	if (problem2.DistanceTo(p) < 0.01) {
	    pcnt++;
	    continue;
	}
    }
    if (pcnt > 1) {
	std::cout << "Adding problem tri to " << name << "\n";
    }
#endif

    // Populate maps
    long i = tri.v[0];
    long j = tri.v[1];
    long k = tri.v[2];
    struct edge_t e[3];
    struct uedge_t ue[3];
    e[0].set(i, j);
    e[1].set(j, k);
    e[2].set(k, i);
    for (int ind = 0; ind < 3; ind++) {
	ue[ind].set(e[ind].v[0], e[ind].v[1]);
	edges2tris[e[ind]] = tri.ind;
	uedges2tris[ue[ind]].insert(tri.ind);
#if 0
	if (pcnt > 1) {
	    std::cout << "uedge[" << ind << "]: " << ue[ind].v[0] << "," << ue[ind].v[1] << "\n";
	}
#endif
	this->v2edges[e[ind].v[0]].insert(e[ind]);
	v2tris[tri.v[ind]].insert(tri.ind);
    }

    // The addition of a triangle may change the boundary edge set.  Set update
    // flag.
    boundary_edges_stale = true;

    // Ditto bounding box
    bounding_box_stale = true;

    return true;
}

void cdt_mesh_t::tri_remove(triangle_t &tri)
{
    // Update edge maps
    long i = tri.v[0];
    long j = tri.v[1];
    long k = tri.v[2];
    struct edge_t e[3];
    struct uedge_t ue[3];
    e[0].set(i, j);
    e[1].set(j, k);
    e[2].set(k, i);
    for (int ind = 0; ind < 3; ind++) {
	ue[ind].set(e[ind].v[0], e[ind].v[1]);
	edges2tris.erase(e[ind]);
	uedges2tris[uedge_t(e[ind])].erase(tri.ind);
	this->v2edges[e[ind].v[0]].erase(e[ind]);
	v2tris[tri.v[ind]].erase(tri.ind);
    }

    // Remove the triangle from the tree
    ON_3dPoint *p3d = pnts[tri.v[0]];
    ON_BoundingBox bb(*p3d, *p3d);
    for (int ind = 1; ind < 3; ind++) {
	p3d = pnts[tri.v[ind]];
	bb.Set(*p3d, true);
    }

    double fMin[3];
    fMin[0] = bb.Min().x;
    fMin[1] = bb.Min().y;
    fMin[2] = bb.Min().z;
    double fMax[3];
    fMax[0] = bb.Max().x;
    fMax[1] = bb.Max().y;
    fMax[2] = bb.Max().z;
    tris_tree.Remove(fMin, fMax, tri.ind);

    // flag boundary edge information for updating
    boundary_edges_stale = true;
    bounding_box_stale = true;
}

std::vector<triangle_t>
cdt_mesh_t::face_neighbors(const triangle_t &f)
{
    std::vector<triangle_t> result;
    long i = f.v[0];
    long j = f.v[1];
    long k = f.v[2];
    struct uedge_t e[3];
    e[0].set(i, j);
    e[1].set(j, k);
    e[2].set(k, i);
    for (int ind = 0; ind < 3; ind++) {
	std::set<size_t> faces = uedges2tris[e[ind]];
	std::set<size_t>::iterator f_it;
	for (f_it = faces.begin(); f_it != faces.end(); f_it++) {
	    if (tris_vect[*f_it] != f) {
		result.push_back(tris_vect[*f_it]);
	    }
	}
    }

    return result;
}


std::vector<triangle_t>
cdt_mesh_t::vertex_face_neighbors(long vind)
{
    std::vector<triangle_t> result;
    std::set<size_t> faces = v2tris[vind];
    std::set<size_t>::iterator f_it;
    for (f_it = faces.begin(); f_it != faces.end(); f_it++) {
	result.push_back(tris_vect[*f_it]);
    }
    return result;
}

std::set<uedge_t>
cdt_mesh_t::get_boundary_edges()
{
    if (boundary_edges_stale) {
	boundary_edges_update();
    }

    return boundary_edges;
}

// Note: misoriented edge pairs (two triangles sharing a directed edge in the
// same direction) were a known issue affecting NIST models.  They are now
// detected by boundary_edges_update() (size==2 case) and repaired by the
// second pass in repair().  The pattern was originally reported as:
//
// 3 misoriented edges
// 13337->84234: 504.806023 290.490191 81.197878->505.275113 290.582257 81.189823
// eface 151366: 84236 13337 84234 : ...
// point 13337: Face(-1) Vert(-1) Trim(-1) Edge(647) UV(0.000000,0.000000)

void
cdt_mesh_t::update_problem_edges()
{
    if (boundary_edges_stale) {
	boundary_edges_update();
    }
}

void
cdt_mesh_t::boundary_edges_update()
{
    if (!boundary_edges_stale) return;
    boundary_edges_stale = false;

    boundary_edges.clear();
    problem_edges.clear();

    std::map<uedge_t, std::set<size_t>>::iterator ue_it;
    for (ue_it = uedges2tris.begin(); ue_it != uedges2tris.end(); ue_it++) {
	if ((*ue_it).second.size() != 1) {
	    continue;
	}
	struct uedge_t ue((*ue_it).first);
	int problem_edge = 0;

	// Only brep boundary edges are "real" boundary edges - anything else is problematic
	if (brep_edges.find(ue) == brep_edges.end()) {
	    problem_edge = 1;
	}

	if (!problem_edge) {
	    boundary_edges.insert((*ue_it).first);
	} else {
	    // Track these edges, as they represent places where subsequent
	    // mesh operations will require extra care
	    problem_edges.insert((*ue_it).first);
	}
    }
}

edge_t
cdt_mesh_t::find_boundary_oriented_edge(uedge_t &ue)
{
    // For the unordered boundary edge, there is exactly one
    // directional edge - find it
    std::set<edge_t>::iterator e_it;
    for (int i = 0; i < 2; i++) {
	std::set<edge_t> vedges = this->v2edges[ue.v[i]];
	for (e_it = vedges.begin(); e_it != vedges.end(); e_it++) {
	    uedge_t vue(*e_it);
	    if (vue == ue) {
		edge_t de(*e_it);
		return de;
	    }
	}
    }

    // This shouldn't happen
    edge_t empty;
    return empty;
}

std::vector<triangle_t>
cdt_mesh_t::interior_incorrect_normals()
{

    RTree<size_t, double, 3>::Iterator tree_it;
    size_t t_ind;
    triangle_t tri;
    std::set<size_t> flip_tris;

    // TODO - just flipping these edge triangles may introduce edge problems - now that
    // we're keeping points away from the edges, see if we can just handle all flipped
    // faces through the repair logic.
#if 0
    tris_tree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	t_ind = *tree_it;
	tri = tris_vect[t_ind];

	ON_3dVector tdir = tnorm(tri);
	ON_3dVector bdir = bnorm(tri);
	if (tdir.Length() > 0 && bdir.Length() > 0 && ON_DotProduct(tdir, bdir) < 0.1) {
	    int epnt_cnt = 0;
	    for (int i = 0; i < 3; i++) {
		epnt_cnt = (ep.find((tri).v[i]) == ep.end()) ? epnt_cnt : epnt_cnt + 1;
	    }
	    if (epnt_cnt == 2) {
		// We're on the edge of the face - just flip this
		flip_tris.insert(tri.ind);
	    }
	}
	++tree_it;
    }

    std::set<size_t>::iterator tr_it;
    for (tr_it = flip_tris.begin(); tr_it != flip_tris.end(); tr_it++) {
	long tmp = tris_vect[*tr_it].v[1];
	tris_vect[*tr_it].v[1] = tris_vect[*tr_it].v[2];
	tris_vect[*tr_it].v[2] = tmp;
	//std::cerr << "Repairing flipped edge triangle\n";
    }
    if (flip_tris.size()) {
	update_problem_edges();
    }
#endif

    std::vector<triangle_t> results;
    tris_tree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	t_ind = *tree_it;
	tri = tris_vect[t_ind];
	ON_3dVector tdir = tnorm(tri);
	ON_3dVector bdir = bnorm(tri);
	if (tdir.Length() > 0 && bdir.Length() > 0 && ON_DotProduct(tdir, bdir) < 0.1) {
	    results.push_back(tri);
	}
	++tree_it;
    }

    return results;
}


std::vector<triangle_t>
cdt_mesh_t::singularity_triangles()
{
    std::vector<triangle_t> results;
    std::set<triangle_t> uniq_tris;
    std::set<long>::iterator s_it;

    for (s_it = sv.begin(); s_it != sv.end(); s_it++) {
	std::vector<triangle_t> faces = this->vertex_face_neighbors(*s_it);
	uniq_tris.insert(faces.begin(), faces.end());
    }

    results.assign(uniq_tris.begin(), uniq_tris.end());
    return results;
}

std::set<uedge_t>
cdt_mesh_t::uedges(const triangle_t &t)
{
    struct uedge_t ue[3];
    ue[0].set(t.v[0], t.v[1]);
    ue[1].set(t.v[1], t.v[2]);
    ue[2].set(t.v[2], t.v[0]);

    std::set<uedge_t> edges;
    for (int idx = 0; idx < 3; idx++) {
	edges.insert(ue[idx]);
    }

    return edges;
}

std::set<uedge_t>
cdt_mesh_t::b_uedges(const triangle_t &t)
{
    std::set<uedge_t> b_ue;
    std::set<uedge_t> ue = uedges(t);
    std::set<uedge_t>::iterator u_it;
    for (u_it = ue.begin(); u_it != ue.end(); u_it++) {
	if (brep_edges.find(*u_it) != brep_edges.end()) {
	    b_ue.insert(*u_it);
	}
    }

    return b_ue;
}

bool
cdt_mesh_t::face_edge_tri(const triangle_t &t)
{
    int edge_pts = 0;
    for (int i = 0; i < 3; i++) {
	if (ep.find(t.v[i]) != ep.end()) {
	    edge_pts++;
	}
    }

    return (edge_pts > 1);
}


static bool NearTrisCallback(size_t data, void *a_context) {
    std::set<size_t> *ntris = (std::set<size_t> *)a_context;
    ntris->insert(data);
    return true;
}
std::set<size_t>
cdt_mesh_t::tris_search(ON_BoundingBox &bb)
{
    double fMin[3], fMax[3];
    fMin[0] = bb.Min().x;
    fMin[1] = bb.Min().y;
    fMin[2] = bb.Min().z;
    fMax[0] = bb.Max().x;
    fMax[1] = bb.Max().y;
    fMax[2] = bb.Max().z;
    std::set<size_t> near_tris;
    size_t nhits = tris_tree.Search(fMin, fMax, NearTrisCallback, (void *)&near_tris);

    if (!nhits) {
	return std::set<size_t>();
    }

    return near_tris;
}

bool
cdt_mesh_t::tri_active(size_t ind)
{

    if (ind >= tris_vect.size()) {
	return false;
    }
    ON_BoundingBox bb = tri_bbox(ind);

    double fMin[3], fMax[3];
    fMin[0] = bb.Min().x;
    fMin[1] = bb.Min().y;
    fMin[2] = bb.Min().z;
    fMax[0] = bb.Max().x;
    fMax[1] = bb.Max().y;
    fMax[2] = bb.Max().z;
    std::set<size_t> near_tris;
    size_t nhits = tris_tree.Search(fMin, fMax, NearTrisCallback, (void *)&near_tris);

    if (!nhits) {
	return false;
    }

    std::set<size_t>::iterator n_it;
    for (n_it = near_tris.begin(); n_it != near_tris.end(); n_it++) {
	if (*n_it == ind) {
	    return true;
	}
    }

    return false;
}


ON_BoundingBox &
cdt_mesh_t::bbox()
{
    if (!bounding_box_stale) {
	return mbb;
    }

    RTree<size_t, double, 3>::Iterator tree_it;
    tris_tree.GetFirst(tree_it);

    if (tree_it.IsNull()) {
	mbb = ON_BoundingBox();
	return mbb;
    }

    ON_3dPoint *p3d = pnts[tris_vect[*tree_it].v[0]];
    ON_BoundingBox cbb(*p3d, *p3d);

    while (!tree_it.IsNull()) {
	p3d = pnts[tris_vect[*tree_it].v[0]];
	cbb.Set(*p3d, true);
	p3d = pnts[tris_vect[*tree_it].v[1]];
	cbb.Set(*p3d, true);
	p3d = pnts[tris_vect[*tree_it].v[2]];
	cbb.Set(*p3d, true);
	++tree_it;
    }
    mbb = cbb;

    bounding_box_stale = false;

    return mbb;
}


ON_BoundingBox
cdt_mesh_t::tri_bbox(size_t ind)
{
    triangle_t tri = tris_vect[ind];
    ON_3dPoint p0 = *pnts[tri.v[0]];
    ON_3dPoint p1 = *pnts[tri.v[1]];
    ON_3dPoint p2 = *pnts[tri.v[2]];
    ON_BoundingBox bb(p0,p0);
    bb.Set(p1, true);
    bb.Set(p2, true);
    return bb;
}

uedge_t
cdt_mesh_t::closest_uedge(const triangle_t &t, ON_3dPoint &p)
{
    uedge_t result;
    std::set<uedge_t> ue_s = uedges(t);
    std::set<uedge_t>::iterator u_it;
    double mdist = DBL_MAX;

    for (u_it = ue_s.begin(); u_it != ue_s.end(); u_it++) {
	uedge_t ue = *u_it;
	double dedge = uedge_dist(ue, p);
	if (dedge < mdist) {
	    mdist = dedge;
	    result = ue;
	}
    }
    return result;
}

uedge_t
cdt_mesh_t::closest_interior_uedge(const triangle_t &t, ON_3dPoint &p)
{
    uedge_t result;
    std::set<uedge_t> ue_s = uedges(t);
    std::set<uedge_t>::iterator u_it;
    double mdist = DBL_MAX;

    for (u_it = ue_s.begin(); u_it != ue_s.end(); u_it++) {
	uedge_t ue = *u_it;
	if (brep_edges.find(ue) != brep_edges.end()) continue;
	double dedge = uedge_dist(ue, p);
	if (dedge < mdist) {
	    mdist = dedge;
	    result = ue;
	}
    }
    return result;
}

double
cdt_mesh_t::uedge_dist(uedge_t &ue, ON_3dPoint &p)
{
    ON_3dPoint p1, p2;
    p1 = *pnts[ue.v[0]];
    p2 = *pnts[ue.v[1]];
    ON_Line l(p1, p2);
    double t;
    l.ClosestPointTo(p, &t);
    if ((t < 0 || NEAR_ZERO(t, ON_ZERO_TOLERANCE))) {
	return p.DistanceTo(p1);
    }
    if ((t > 1 || NEAR_EQUAL(t, 1, ON_ZERO_TOLERANCE))) {
	return p.DistanceTo(p2);
    }

    return p.DistanceTo(l.PointAt(t));
}

class uedge_dist_t
{
    public:
	uedge_t ue;
	double dist;

	bool operator < (const uedge_dist_t& other) const
	{
	    return (dist < other.dist);
	}
};

std::vector<uedge_t>
cdt_mesh_t::sorted_uedges_l_to_s(std::set<uedge_t> &edges)
{
    std::vector<uedge_dist_t> ued_vect;
    std::set<uedge_t>::iterator ue_it;
    for (ue_it = edges.begin(); ue_it != edges.end(); ue_it++) {
	uedge_dist_t ued;
	ued.ue = *ue_it;
	ON_3dPoint p1, p2;
	p1 = *pnts[ued.ue.v[0]];
	p2 = *pnts[ued.ue.v[1]];
	ued.dist = p1.DistanceTo(p2);
	ued_vect.push_back(ued);
    }

    std::sort(ued_vect.begin(), ued_vect.end());
    std::reverse(ued_vect.begin(), ued_vect.end());

    std::vector<uedge_t> ue_sorted;
    for (size_t i = 0; i < ued_vect.size(); i++) {
	uedge_t ue = ued_vect[i].ue;
	ue_sorted.push_back(ue);
    }

    return ue_sorted;
}

cpolygon_t *
cdt_mesh_t::uedge_polygon(uedge_t &ue)
{
    std::set<size_t> ue_tris = uedges2tris[ue];

    if (ue_tris.size() != 2) {
	std::cout << "Error - found " << ue_tris.size() << " triangles, not building polygon\n";
	return NULL;
    }

    std::set<long> t_pts;
    std::set<size_t>::iterator r_it;
    for (r_it = ue_tris.begin(); r_it != ue_tris.end(); r_it++) {
	triangle_t tri = tris_vect[*r_it];
	for (int i = 0; i < 3; i++) {
	    t_pts.insert(tri.v[i]);
	}
    }
    if (t_pts.size() != 4) {
	std::cout << "Error - found " << t_pts.size() << " triangle points??\n";
	return NULL;
    }

    point_t pcenter;
    vect_t pnorm;
    point_t *fpnts = (point_t *)bu_calloc(t_pts.size()+1, sizeof(point_t), "fitting points");
    std::set<long>::iterator p_it;
    int tpind = 0;
    for (p_it = t_pts.begin(); p_it != t_pts.end(); p_it++) {
	ON_3dPoint *p = pnts[*p_it];
	fpnts[tpind][X] = p->x;
	fpnts[tpind][Y] = p->y;
	fpnts[tpind][Z] = p->z;
	tpind++;
    }
    if (bg_fit_plane(&pcenter, &pnorm, t_pts.size(), fpnts)) {
	std::cout << "fitting plane failed!\n";
	bu_free(fpnts, "fitting points");
	return NULL;
    }
    bu_free(fpnts, "fitting points");

    ON_3dPoint opc(pcenter);
    ON_3dPoint opn(pnorm);
    ON_Plane fit_plane(opc, opn);

    // Build our polygon out of the two triangles.

    // First step, add the 2D projection of the triangle vertices to the
    // polygon data structure.
    cpolygon_t *polygon = new cpolygon_t;
    polygon->pdir = fit_plane.Normal();
    polygon->tplane = fit_plane;
    for (p_it = t_pts.begin(); p_it != t_pts.end(); p_it++) {
	double u, v;
	long pind = *p_it;
	ON_3dPoint *p = pnts[pind];
	fit_plane.ClosestPointTo(*p, &u, &v);
	std::pair<double, double> proj_2d;
	proj_2d.first = u;
	proj_2d.second = v;
	polygon->pnts_2d.push_back(proj_2d);
	polygon->p2o[polygon->pnts_2d.size() - 1] = pind;
	polygon->o2p[pind] = polygon->pnts_2d.size() - 1;
    }
    // Initialize the polygon edges with one of the triangles.
    triangle_t tri1 = tris_vect[*(ue_tris.begin())];
    ue_tris.erase(ue_tris.begin());
    struct edge2d_t e1(polygon->o2p[tri1.v[0]], polygon->o2p[tri1.v[1]]);
    struct edge2d_t e2(polygon->o2p[tri1.v[1]], polygon->o2p[tri1.v[2]]);
    struct edge2d_t e3(polygon->o2p[tri1.v[2]], polygon->o2p[tri1.v[0]]);
    polygon->add_edge(e1);
    polygon->add_edge(e2);
    polygon->add_edge(e3);

    // Grow the polygon with the other triangle.
    std::set<uedge_t> new_edges;
    std::set<uedge_t> shared_edges;
    triangle_t tri2 = tris_vect[*(ue_tris.begin())];
    ue_tris.erase(ue_tris.begin());
    for (int i = 0; i < 3; i++) {
	int v1 = i;
	int v2 = (i < 2) ? i + 1 : 0;
	uedge_t ue1(tri2.v[v1], tri2.v[v2]);
	if (ue1 != ue) {
	    new_edges.insert(ue1);
	} else {
	    shared_edges.insert(ue1);
	}
    }
    polygon->replace_edges(new_edges, shared_edges);

    return polygon;
}


bool
cdt_mesh_t::closest_surf_pnt(ON_3dPoint &s_p, ON_3dVector &s_norm, ON_3dPoint *p, double tol)
{
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)p_cdt;
    ON_2dPoint surf_p2d;
    ON_3dPoint surf_p3d = ON_3dPoint::UnsetPoint;
    s_p = ON_3dPoint::UnsetPoint;
    s_norm = ON_3dVector::UnsetVector;
    double cdist;
    if (tol <= 0) {
	surface_GetClosestPoint3dFirstOrder(s_cdt->brep->m_F[f_id].SurfaceOf(), *p, surf_p2d, surf_p3d, cdist);
    } else {
	surface_GetClosestPoint3dFirstOrder(s_cdt->brep->m_F[f_id].SurfaceOf(), *p, surf_p2d, surf_p3d, cdist, 0, ON_ZERO_TOLERANCE, tol);
    }
    if (NEAR_EQUAL(cdist, DBL_MAX, ON_ZERO_TOLERANCE)) return false;
    bool seval = surface_EvNormal(s_cdt->brep->m_F[f_id].SurfaceOf(), surf_p2d.x, surf_p2d.y, s_p, s_norm);
    if (!seval) return false;

    if (m_bRev) {
	s_norm = s_norm * -1;
    }

    return true;
}

bool
cdt_mesh_t::planar(double ptol)
{
    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)p_cdt;
    return s_cdt->brep->m_F[f_id].SurfaceOf()->IsPlanar(NULL, ptol);
}

ON_3dVector
cdt_mesh_t::tnorm(const triangle_t &t)
{
    ON_3dPoint *p1 = this->pnts[t.v[0]];
    ON_3dPoint *p2 = this->pnts[t.v[1]];
    ON_3dPoint *p3 = this->pnts[t.v[2]];

    ON_3dVector e1 = *p2 - *p1;
    ON_3dVector e2 = *p3 - *p1;
    ON_3dVector tdir = ON_CrossProduct(e1, e2);
    tdir.Unitize();
    if (m_bRev) {
	tdir = -1 * tdir;
    }
    return tdir;
}

ON_3dPoint
cdt_mesh_t::tcenter(const triangle_t &t)
{
    ON_3dPoint avgpnt(0,0,0);

    for (size_t i = 0; i < 3; i++) {
	ON_3dPoint *p3d = this->pnts[t.v[i]];
	avgpnt = avgpnt + *p3d;
    }

    ON_3dPoint cpnt = avgpnt/3.0;
    return cpnt;
}

ON_Plane
cdt_mesh_t::tplane(const triangle_t &t)
{
    ON_3dPoint tc = tcenter(t);
    ON_3dVector tn = tnorm(t);
    return ON_Plane(tc, tn);
}

ON_3dVector
cdt_mesh_t::bnorm(const triangle_t &t)
{
    ON_3dPoint avgnorm(0,0,0);

    // Can't calculate this without some key Brep data
    if (!nmap.size() && !sv.size()) return avgnorm;

    double norm_cnt = 0.0;

    // First pass: average normals from non-singularity vertices, as they
    // provide the most reliable orientation signal for the triangle.
    for (size_t i = 0; i < 3; i++) {
	if (sv.find(t.v[i]) != sv.end()) {
	    // singular vert norms are a product of multiple faces
	    continue;
	}
	ON_3dPoint onrm = *normals[nmap[t.v[i]]];
	if (this->m_bRev) {
	    onrm = onrm * -1;
	}
	avgnorm = avgnorm + onrm;
	norm_cnt = norm_cnt + 1.0;
    }
    if (norm_cnt > 0) {
	ON_3dVector anrm = avgnorm/norm_cnt;
	anrm.Unitize();
	return anrm;
    }

    // All three vertices are singularity vertices.  The old code would
    // divide by zero here; use the pre-averaged singularity normals instead.
    // Deduplicate by 3D pointer so that multiple UV-space instances of the
    // same singularity pole are counted only once.
    std::set<ON_3dPoint *> seen_pts;
    for (size_t i = 0; i < 3; i++) {
	ON_3dPoint *p3d = pnts[(size_t)t.v[i]];
	if (!seen_pts.insert(p3d).second)
	    continue;
	ON_3dVector vn = vert_norm(t.v[i]);
	if (vn.Length() > ON_ZERO_TOLERANCE) {
	    avgnorm = avgnorm + ON_3dPoint(vn);
	    norm_cnt++;
	}
    }

    if (norm_cnt < 1) return avgnorm;
    ON_3dVector anrm = avgnorm / norm_cnt;
    anrm.Unitize();
    return anrm;
}

ON_Plane
cdt_mesh_t::bplane(const triangle_t &t)
{
    ON_3dPoint tc = tcenter(t);
    ON_3dVector tn = bnorm(t);
    return ON_Plane(tc, tn);
}

bool
cdt_mesh_t::brep_edge_pnt(long v)
{
    return (ep.find(v) != ep.end());
}

void cdt_mesh_t::reset()
{
    this->tris_vect.clear();
    this->tris_tree.RemoveAll();
    this->v2edges.clear();
    this->v2tris.clear();
    this->edges2tris.clear();
    this->uedges2tris.clear();
}

bool
cdt_mesh_t::tri_problem_edges(triangle_t &t)
{
    if (boundary_edges_stale) {
	boundary_edges_update();
    }

    if (!problem_edges.size()) return false;

    uedge_t ue1(t.v[0], t.v[1]);
    uedge_t ue2(t.v[1], t.v[2]);
    uedge_t ue3(t.v[2], t.v[0]);

    if (problem_edges.find(ue1) != problem_edges.end()) return true;
    if (problem_edges.find(ue2) != problem_edges.end()) return true;
    if (problem_edges.find(ue3) != problem_edges.end()) return true;
    return false;
}

// TODO If a triangle has only one bad edge, we need to figure out if we can yank it...
void
cdt_mesh_t::remove_dangling_tris()
{
    if (boundary_edges_stale) {
	boundary_edges_update();
    }

    if (!problem_edges.size()) return;

    std::set<uedge_t>::iterator u_it;
    std::set<size_t>::iterator t_it;
    for (u_it = problem_edges.begin(); u_it != problem_edges.end(); u_it++) {
	std::set<size_t> ptris = uedges2tris[(*u_it)];
	for (t_it = ptris.begin(); t_it != ptris.end(); t_it++) {
	    triangle_t t = tris_vect[*t_it];
	    std::set<uedge_t> ue = t.uedges();
	    std::set<uedge_t>::iterator ue_it;
	    int bedge_cnt = 0;
	    for (ue_it = ue.begin(); ue_it != ue.end(); ue_it++) {
		if (problem_edges.find(*ue_it) != problem_edges.end()) {
		    bedge_cnt++;
		}
	    }

	    if (bedge_cnt == 1) {
		tri_remove(t);
	    }
	}
    }
}

std::vector<triangle_t>
cdt_mesh_t::problem_edge_tris()
{
    std::vector<triangle_t> eresults;

    if (boundary_edges_stale) {
	boundary_edges_update();
    }

    if (!problem_edges.size()) return eresults;

    std::set<triangle_t> uresults;
    std::set<uedge_t>::iterator u_it;
    std::set<size_t>::iterator t_it;
    for (u_it = problem_edges.begin(); u_it != problem_edges.end(); u_it++) {
	std::set<size_t> ptris = uedges2tris[(*u_it)];
	for (t_it = uedges2tris[(*u_it)].begin(); t_it != uedges2tris[(*u_it)].end(); t_it++) {
	    triangle_t t = tris_vect[*t_it];
	    uresults.insert(t);
	}
    }

    std::vector<triangle_t> results(uresults.begin(), uresults.end());
    return results;
}

bool
cdt_mesh_t::self_intersecting_mesh()
{
    std::set<triangle_t> pedge_tris;
    std::set<uedge_t>::iterator u_it;
    std::set<size_t>::iterator t_it;

    if (boundary_edges_stale) {
	boundary_edges_update();
    }

    for (u_it = problem_edges.begin(); u_it != problem_edges.end(); u_it++) {
	std::set<size_t> ptris = uedges2tris[(*u_it)];
	for (t_it = uedges2tris[(*u_it)].begin(); t_it != uedges2tris[(*u_it)].end(); t_it++) {
	    pedge_tris.insert(tris_vect[*t_it]);
	}
    }

    std::map<uedge_t, std::set<size_t>>::iterator et_it;
    for (et_it = uedges2tris.begin(); et_it != uedges2tris.end(); et_it++) {
	std::set<size_t> uetris = et_it->second;
	if (uetris.size() > 2) {
	    size_t valid_cnt = 0;
	    for (t_it = uetris.begin(); t_it != uetris.end(); t_it++) {
		triangle_t t = tris_vect[*t_it];
		if (pedge_tris.find(t) == pedge_tris.end()) {
		    valid_cnt++;
		}
	    }
	    if (valid_cnt > 2) {
		std::cout << "Self intersection in mesh found\n";
		struct uedge_t pue = et_it->first;
		FILE* plot_file = fopen("self_intersecting_edge.plot3", "w");
		struct bu_color c = BU_COLOR_INIT_ZERO;
		bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
		pl_color_buc(plot_file, &c);
		plot_edge(pue, plot_file);
		fclose(plot_file);
		return true;
	    }
	}
    }

    return false;
}

double
cdt_mesh_t::max_angle_delta(triangle_t &seed, std::vector<triangle_t> &s_tris)
{
    double dmax = 0;
    ON_3dVector sn = tnorm(seed);

    for (size_t i = 0; i < s_tris.size(); i++) {
	ON_3dVector tn = tnorm(s_tris[i]);
	double d_ang = ang_deg(sn, tn);
	dmax = (d_ang > dmax) ? d_ang : dmax;
    }

    dmax = (dmax < 10) ? 10 : dmax;
    return (dmax < 170) ? dmax : 170;
}

std::vector<struct ctriangle_t>
cdt_mesh_t::polygon_tris(cpolygon_t *polygon, double angle, bool brep_norm, int initial)
{
    std::set<triangle_t> initial_set;

    std::set<cpolyedge_t *>::iterator p_it;
    for (p_it = polygon->poly.begin(); p_it != polygon->poly.end(); p_it++) {
	cpolyedge_t *pe = *p_it;
	struct uedge2d_t ue2d(pe->v2d[0], pe->v2d[1]);
	bool edge_isect = (polygon->self_isect_edges.find(ue2d) != polygon->self_isect_edges.end());
	struct uedge_t ue(polygon->p2o[pe->v2d[0]], polygon->p2o[pe->v2d[1]]);
	std::set<size_t> petris = uedges2tris[ue];
	std::set<size_t>::iterator t_it;
	for (t_it = petris.begin(); t_it != petris.end(); t_it++) {

	    if (polygon->visited_triangles.find(tris_vect[*t_it]) != polygon->visited_triangles.end()) {
		continue;
	    }

	    // If the triangle is involved with a self intersecting edge in the
	    // polygon and we haven't already see it, we have to try incorporating it
	    if (edge_isect) {
		initial_set.insert(tris_vect[*t_it]);
		continue;
	    }

	    // If all three verts are edge vertices and this is our first run
	    // through, we need to try incorporating it.  We may be seeding
	    // next to a "vertical" edge triangle, and it may be that none of
	    // the relevant points qualified to be "uncontained" points.  If
	    // so, our growth criteria will not result in a new polygon, but we
	    // need to try and correct the vertical triangle.
	    if (initial) {
		if (brep_edge_pnt(tris_vect[*t_it].v[0]) && brep_edge_pnt(tris_vect[*t_it].v[1]) && brep_edge_pnt(tris_vect[*t_it].v[2])) {
		    initial_set.insert(tris_vect[*t_it]);
		    continue;
		}
	    }

	    ON_3dVector tn = (brep_norm) ? bnorm(tris_vect[*t_it]) : tnorm(tris_vect[*t_it]);
	    double d_ang = ang_deg(polygon->pdir, tn);
	    if (d_ang > angle && !NEAR_EQUAL(d_ang, angle, ON_ZERO_TOLERANCE)) {
		continue;
	    }
	    initial_set.insert(tris_vect[*t_it]);
	}
    }

    // Now that we have the triangles, characterize them.
    struct ctriangle_t **ctris = (struct ctriangle_t **)bu_calloc(initial_set.size()+1, sizeof(ctriangle_t *), "sortable ctris");
    std::set<triangle_t>::iterator f_it;
    int ctris_cnt = 0;
    for (f_it = initial_set.begin(); f_it != initial_set.end(); f_it++) {

	struct ctriangle_t *nct = (struct ctriangle_t *)bu_calloc(1, sizeof(ctriangle_t), "ctriangle");
	ctris[ctris_cnt] = nct;
	ctris_cnt++;

	triangle_t t = *f_it;
	nct->v[0] = t.v[0];
	nct->v[1] = t.v[1];
	nct->v[2] = t.v[2];
	nct->ind = t.ind;
	struct edge2d_t e1(polygon->o2p[t.v[0]], polygon->o2p[t.v[1]]);
	struct edge2d_t e2(polygon->o2p[t.v[1]], polygon->o2p[t.v[2]]);
	struct edge2d_t e3(polygon->o2p[t.v[2]], polygon->o2p[t.v[0]]);
	struct uedge2d_t ue[3];
	ue[0].set(polygon->o2p[t.v[0]], polygon->o2p[t.v[1]]);
	ue[1].set(polygon->o2p[t.v[1]], polygon->o2p[t.v[2]]);
	ue[2].set(polygon->o2p[t.v[2]], polygon->o2p[t.v[0]]);

	nct->all_bedge = false;
	nct->isect_edge = false;
	nct->uses_uncontained = false;
	nct->contains_uncontained = false;
	nct->angle_to_nearest_uncontained = DBL_MAX;

	for (int i = 0; i < 3; i++) {
	    if (polygon->self_isect_edges.find(ue[i]) != polygon->self_isect_edges.end()) {
		nct->isect_edge = true;
		polygon->unusable_triangles.erase(*f_it);
	    }
	    if (nct->isect_edge) break;
	}
	if (nct->isect_edge) continue;


	// If we're not on a self-intersecting edge, check for use of uncontained points
	for (int i = 0; i < 3; i++) {
	    if (polygon->uncontained.find((t).v[i]) != polygon->uncontained.end()) {
		nct->uses_uncontained = true;
		polygon->unusable_triangles.erase(*f_it);
	    }
	    if (nct->uses_uncontained) break;
	    if (polygon->flipped_face.find((t).v[i]) != polygon->flipped_face.end()) {
		nct->uses_uncontained = true;
		polygon->unusable_triangles.erase(*f_it);
	    }
	    if (nct->uses_uncontained) break;
	}
	if (nct->uses_uncontained) continue;

	// If we aren't directly using an uncontained point, see if one is inside
	// the triangle projection
	cpolygon_t tpoly;
	tpoly.pnts_2d = polygon->pnts_2d;
	tpoly.add_edge(e1);
	tpoly.add_edge(e2);
	tpoly.add_edge(e3);
	std::set<long>::iterator u_it;
	for (u_it = polygon->uncontained.begin(); u_it != polygon->uncontained.end(); u_it++) {
	    if (tpoly.point_in_polygon(*u_it, false)) {
		nct->contains_uncontained = true;
		polygon->unusable_triangles.erase(*f_it);
	    }
	}
	if (!nct->contains_uncontained) {
	    for (u_it = polygon->flipped_face.begin(); u_it != polygon->flipped_face.end(); u_it++) {
		if (tpoly.point_in_polygon(*u_it, false)) {
		    nct->contains_uncontained = true;
		    polygon->unusable_triangles.erase(*f_it);
		}
	    }
	}
	if (nct->contains_uncontained) continue;


	// If we've pulled in a face that is all edge vertices, go with it
	if (brep_edge_pnt(t.v[0]) && brep_edge_pnt(t.v[1]) && brep_edge_pnt(t.v[2])) {
	    nct->all_bedge = true;
	    polygon->unusable_triangles.erase(*f_it);
	}
	if (nct->all_bedge) continue;

	// If we aren't directly involved with an uncontained point and we only
	// share 1 edge with the polygon, see how much it points at one of the
	// points of interest (if any) definitely outside the current polygon
	std::set<cpolyedge_t *>::iterator pe_it;
	long shared_cnt = polygon->shared_edge_cnt(t);
	if (shared_cnt != 1) continue;
	double vangle = polygon->ucv_angle(t);
	if (vangle > 0 && nct->angle_to_nearest_uncontained > vangle) {
	    nct->angle_to_nearest_uncontained = vangle;
	    polygon->unusable_triangles.erase(*f_it);
	}
    }

    // Now that we have the characterized triangles, sort them.
    bu_sort(ctris, ctris_cnt, sizeof(struct ctriangle_t *), ctriangle_cmp, NULL);


    // Push the final sorted results into the vector, free the ctris entries and array
    std::vector<ctriangle_t> result;
    for (long i = 0; i < ctris_cnt; i++) {
	result.push_back(*ctris[i]);
    }
    for (long i = 0; i < ctris_cnt; i++) {
	bu_free(ctris[i], "ctri");
    }
    bu_free(ctris, "ctris array");
    return result;
}

// TODO - this should be all flip or all non-flip - selective flipping will break edge
// connectivity
bool
cdt_mesh_t::oriented_polycdt(cpolygon_t *polygon, bool reproject)
{
    std::set<triangle_t> otris;

    // Save the original 2D coordinates before any reprojection so that if LSCM
    // introduces a fold-over (mixed-orientation triangles) we can fall back to
    // the best-fit-plane approach with unmodified input coordinates.
    bool tried_lscm = false;
    std::vector<std::pair<double, double>> pnts_2d_orig;

    if (reproject) {
	pnts_2d_orig = polygon->pnts_2d;
	// Try LSCM parameterization first: it maps the boundary to a unit circle
	// guaranteeing a non-self-intersecting 2D domain even for highly curved
	// patches where the best-fit plane projection would fold on itself,
	// causing CDT (bg_nested_poly_triangulate) to fail.
	// Fall back to the plane-based approach if LSCM fails.
	if (lscm_reproject(polygon)) {
	    tried_lscm = true;
	} else {
	    best_fit_plane_reproject(polygon);
	}
    }

    if (!polygon->cdt()) {
	// If LSCM was used and CDT failed despite the unit-circle validation
	// passing, fall back to best_fit_plane as a safety net.  This handles
	// the rare case where the CG solver produces coordinates that pass the
	// 1% unit-circle tolerance but still cause CDT to fail (e.g. a Steiner
	// point in a large arc segment outside the inscribed polygon).
	if (tried_lscm) {
	    bu_log("lscm CDT failed on f_id=%d, retrying with best_fit_plane\n", f_id);
	    polygon->pnts_2d = pnts_2d_orig;
	    polygon->ltris.clear();
	    polygon->tris.clear();
	    tried_lscm = false;
	    best_fit_plane_reproject(polygon);
	    if (!polygon->cdt()) return false;
	} else {
	    return false;
	}
    }

    // Count flipped triangles, but exclude any triangle that has at least one
    // singularity vertex (sv member).  Near a singularity the 3D triangle can
    // be nearly degenerate (two or three vertices at the same 3D pole), making
    // the cross-product in tnorm() numerically unreliable.  Including those
    // triangles in the flip count causes false fold-over detections that
    // trigger spurious best-fit-plane retries.  The orientation of sv-touching
    // triangles is better trusted from the parameterization (LSCM preserves
    // global CCW), so we do not count them here.
    auto count_flips = [&](const std::set<triangle_t> &tris,
			   size_t *out_flip, size_t *out_eligible) {
	*out_flip = 0;
	*out_eligible = 0;
	for (auto tit = tris.begin(); tit != tris.end(); tit++) {
	    triangle_t t = *tit;
	    t.m = this;
	    bool has_sv = false;
	    for (int i = 0; i < 3; i++) {
		if (sv.find(t.v[i]) != sv.end()) { has_sv = true; break; }
	    }
	    if (has_sv) continue;
	    (*out_eligible)++;
	    ON_3dVector tdir = tnorm(t);
	    ON_3dVector bdir = bnorm(t);
	    if (ON_DotProduct(tdir, bdir) < 0)
		(*out_flip)++;
	}
    };

    size_t flip_cnt = 0;
    size_t eligible_cnt = 0;
    count_flips(polygon->tris, &flip_cnt, &eligible_cnt);

    // If LSCM was used and produced a mix of flipped and correctly-oriented
    // non-sv triangles, that signals a fold-over in the conformal mapping.
    // Retry with best-fit-plane.  If best-fit-plane also fails (e.g. a
    // self-intersecting polygon from duplicate singularity boundary vertices),
    // fall back to the LSCM result.
    if (tried_lscm && flip_cnt > 0 && flip_cnt <= eligible_cnt / 2) {
	bu_log("lscm fold-over on f_id=%d (flip=%zu/%zu eligible/%zu total), retrying with best_fit_plane\n",
	    f_id, flip_cnt, eligible_cnt, polygon->tris.size());
	// Save the LSCM triangulation before overwriting it.
	std::vector<std::pair<double, double>> pnts_2d_lscm = polygon->pnts_2d;
	std::set<triangle_t> tris_lscm = polygon->tris;
	size_t flip_cnt_lscm = flip_cnt;
	polygon->pnts_2d = pnts_2d_orig;
	polygon->ltris.clear();
	polygon->tris.clear();
	best_fit_plane_reproject(polygon);
	if (!polygon->cdt()) {
	    // best_fit_plane CDT failed (e.g. self-intersecting polygon from
	    // duplicate singularity boundary vertices).  Fall back to the LSCM
	    // result and accept its minor fold-over; the global majority-vote
	    // flip below will handle overall orientation.
	    bu_log("best_fit_plane CDT also failed on f_id=%d, using LSCM result\n", f_id);
	    polygon->pnts_2d = pnts_2d_lscm;
	    polygon->tris = tris_lscm;
	    flip_cnt = flip_cnt_lscm;
	    eligible_cnt = 0;  // recalculated below
	    count_flips(polygon->tris, &flip_cnt, &eligible_cnt);
	} else {
	    // Recount flips for the new triangulation (same sv-exclusion rule).
	    count_flips(polygon->tris, &flip_cnt, &eligible_cnt);
	}
    }

    std::set<triangle_t>::iterator tr_it;
    if (flip_cnt > eligible_cnt / 2 && eligible_cnt > 0) {
	for (tr_it = polygon->tris.begin(); tr_it != polygon->tris.end(); tr_it++) {
	    triangle_t t = *tr_it;
	    t.m = this;
	    triangle_t nt(t);
	    nt.v[2] = t.v[1];
	    nt.v[1] = t.v[2];
	    otris.insert(nt);
	}
	polygon->tris.clear();
	polygon->tris.insert(otris.begin(), otris.end());
	bu_log("flipping tris\n");
    } else {
	if (flip_cnt && flip_cnt != polygon->tris.size()) {
	    bu_log("NOT flipping tris, adding %zd flipped tris\n", flip_cnt);
	} else {
	    bu_log("NOT flipping tris, OK\n");
	}
    }

    // ── Boundary-neighbor orientation check ─────────────────────────────────
    // The majority-vote flip above uses tnorm vs bnorm to orient the patch.
    // When different patches use different parameterization paths (LSCM vs
    // best_fit_plane after a fold-over retry), their flip decisions can be
    // inconsistent at shared boundary edges, creating "naked edges" (problem
    // edges) in the final mesh.
    //
    // This second pass checks the patch's orientation against already-committed
    // mesh triangles that share its boundary edges.  For a manifold mesh, each
    // shared interior edge must be traversed in OPPOSITE directions by the two
    // triangles that share it.  If a majority of boundary edges conflict with
    // committed neighbors, flip all patch triangles.
    //
    // NOTE: visited_triangles have NOT been removed from tris_vect yet when
    // oriented_polycdt runs (process_seed_tri does that after grow_loop
    // returns).  We filter them out to find the true committed neighbor on each
    // boundary edge.
    {
	int bnd_consistent   = 0;
	int bnd_inconsistent = 0;

	std::set<cpolyedge_t *>::iterator pe_it;
	for (pe_it = polygon->poly.begin(); pe_it != polygon->poly.end(); pe_it++) {
	    cpolyedge_t *pe = *pe_it;
	    // p2o is identity (2D polygon index == 3D mesh index)
	    long va = polygon->p2o[pe->v2d[0]];
	    long vb = polygon->p2o[pe->v2d[1]];
	    uedge_t ue(va, vb);

	    auto ue_it = uedges2tris.find(ue);
	    if (ue_it == uedges2tris.end()) continue;

	    // Find a committed neighbor: a triangle that uses this undirected
	    // edge and is NOT in the visited set (i.e. not being replaced).
	    const triangle_t *committed = NULL;
	    for (size_t ti : ue_it->second) {
		if (polygon->visited_triangles.find(tris_vect[ti]) ==
		    polygon->visited_triangles.end()) {
		    committed = &tris_vect[ti];
		    break;
		}
	    }
	    if (!committed) continue;

	    // Determine which directed sense the committed neighbor uses for
	    // this undirected edge.  committed_fwd == true means it uses va→vb.
	    bool committed_fwd = false;
	    for (int i = 0; i < 3; i++) {
		if (committed->v[i] == va && committed->v[(i+1)%3] == vb) {
		    committed_fwd = true;
		    break;
		}
	    }

	    // Find the patch triangle that shares this edge and determine its
	    // directed sense.
	    bool patch_found       = false;
	    bool patch_consistent  = false;
	    for (auto const& pt : polygon->tris) {
		bool has_fwd = false, has_rev = false;
		for (int i = 0; i < 3; i++) {
		    if (pt.v[i] == va && pt.v[(i+1)%3] == vb) has_fwd = true;
		    if (pt.v[i] == vb && pt.v[(i+1)%3] == va) has_rev = true;
		}
		if (!has_fwd && !has_rev) continue;
		patch_found = true;
		// Manifold consistency: committed(fwd)→patch(rev) or vice-versa.
		patch_consistent = (committed_fwd && has_rev) ||
				   (!committed_fwd && has_fwd);
		break;
	    }

	    if (!patch_found) continue;
	    if (patch_consistent) bnd_consistent++;
	    else                  bnd_inconsistent++;
	}

	if (bnd_inconsistent > bnd_consistent && bnd_inconsistent > 0) {
	    std::set<triangle_t> flipped;
	    for (auto const& pt : polygon->tris) {
		triangle_t ft = pt;
		long tmp = ft.v[1]; ft.v[1] = ft.v[2]; ft.v[2] = tmp;
		flipped.insert(ft);
	    }
	    polygon->tris.clear();
	    polygon->tris.insert(flipped.begin(), flipped.end());
	    bu_log("boundary-neighbor flip on f_id=%d (inconsistent=%d consistent=%d)\n",
		f_id, bnd_inconsistent, bnd_consistent);
	}
    }

    return true;
}

int
cdt_mesh_t::grow_loop(cpolygon_t *polygon, double deg, bool stop_on_contained, triangle_t &target, bool reproject)
{
    double angle = deg;

    if (stop_on_contained && !polygon->uncontained.size() && polygon->visited_triangles.size() > 1) {
	return 0;
    }

    if (deg < 0 || deg > 170) {
	std::cerr << "Degree error: " << deg << "\n";
	return -1;
    }

    if (polygon->visited_triangles.find(target) == polygon->visited_triangles.end()) {
	for (int i = 0; i < 3; i++) {
	    if (polygon->used_verts.find(target.v[i]) == polygon->used_verts.end()) {
		polygon->target_verts.insert(target.v[i]);
	    }
	}
    }

    polygon->unusable_triangles.clear();

    // First step - collect all the unvisited triangles from the polyline edges.

    std::stack<ctriangle_t> ts;

    std::set<edge_t> flipped_edges;

    std::vector<ctriangle_t> ptris = polygon_tris(polygon, angle, stop_on_contained, 1);

    if (!ptris.size() && stop_on_contained) {
	if (!grow_loop_failure_ok) {
	    std::cerr << "No triangles available??\n";
	}
	return -1;
    }
    if (!ptris.size() && !stop_on_contained) {
	return 0;
    }


    for (size_t i = 0; i < ptris.size(); i++) {
	ts.push(ptris[i]);
    }

    while (!ts.empty()) {

	ctriangle_t cct = ts.top();
	ts.pop();
	triangle_t ct(cct.v[0], cct.v[1], cct.v[2], cct.ind);

	// A triangle will introduce at most one new point into the loop.  If
	// the triangle is bad, it will define uncontained interior points and
	// potentially (if it has unmated edges) won't grow the polygon at all.

	// The first thing to do is find out of the triangle shares one or two
	// edges with the loop.  (0 or 3 would indicate something Very Wrong...)
	std::set<uedge_t> new_edges;
	std::set<uedge_t> shared_edges;
	long vert = -1;
	long new_edge_cnt = tri_process(polygon, &new_edges, &shared_edges, &vert, ct);

	bool process = true;

	if (new_edge_cnt == -2) {
	    // Vert from bad edges - added to uncontained.  Start over with another triangle - we
	    // need to walk out until this point is swept up by the polygon.
	    polygon->visited_triangles.insert(ct);
	    process = false;
	}

	if (new_edge_cnt == -1) {
	    // If the triangle shares one edge but all three vertices are on the
	    // polygon, we can't use this triangle in this context - it would produce
	    // a self-intersecting polygon.
	    polygon->unusable_triangles.insert(ct);
	    process = false;
	}

	if (process) {

	    ON_3dVector tdir = tnorm(ct);
	    ON_3dVector bdir = bnorm(ct);
	    bool flipped_tri = (ON_DotProduct(tdir, bdir) < 0);

	    if (stop_on_contained && new_edge_cnt == 2 && flipped_tri) {
		// It is possible that a flipped face adding two new edges will
		// make a mess out of the polygon (i.e. make it self intersecting.)
		// Tag it so we know we can't trust point_in_polygon until we've grown
		// the vertex out of flipped_face (remove_edge will handle that.)
		if (brep_edge_pnt(vert)) {
		    // We can't flag brep edge points as uncontained, so if we hit this case
		    // flag all the verts except the edge verts as flipped face problem cases.
		    for (int i = 0; i < 3; i++) {
			if (!brep_edge_pnt(ct.v[i])) {
			    polygon->flipped_face.insert(ct.v[i]);
			}
		    }
		} else {
		    polygon->flipped_face.insert(vert);
		}
	    }

	    int use_tri = 1;
	    if (!(polygon->poly.size() == 3 && polygon->interior_points.size())) {
		if (stop_on_contained && new_edge_cnt == 2 && !flipped_tri) {
		    // If this is a good triangle and we're in repair mode, don't add it unless
		    // it uses or points in the direction of at least one uncontained point.
		    if (!cct.isect_edge && !cct.uses_uncontained && !cct.contains_uncontained &&
			!cct.all_bedge &&
			(cct.angle_to_nearest_uncontained > 2*ON_PI || cct.angle_to_nearest_uncontained < 0)) {
			use_tri = 0;
		    }
		}
	    }

	    if (new_edge_cnt <= 0 || new_edge_cnt > 2) {
		struct bu_vls fname = BU_VLS_INIT_ZERO;
		std::cerr << "fatal loop growth error!\n";
		bu_vls_sprintf(&fname, "%d-fatal_loop_growth_poly_3d.plot3", f_id);
		polygon->polygon_plot_in_plane(bu_vls_cstr(&fname));
		bu_vls_sprintf(&fname, "%d-fatal_loop_growth_visited_tris.plot3", f_id);
		tris_set_plot(polygon->visited_triangles, bu_vls_cstr(&fname));
		bu_vls_sprintf(&fname, "%d-fatal_loop_growth_unusable_tris.plot3", f_id);
		tris_set_plot(polygon->unusable_triangles, bu_vls_cstr(&fname));
		bu_vls_sprintf(&fname, "%d-fatal_loop_growth_bad_tri.plot3", f_id);
		tri_plot(ct, bu_vls_cstr(&fname));
		bu_vls_free(&fname);
		return -1;
	    }

	    if (use_tri) {
		polygon->replace_edges(new_edges, shared_edges);
		polygon->visited_triangles.insert(ct);
	    }
	}

	bool h_uc = polygon->update_uncontained();

	if (polygon->visited_triangles.find(target) != polygon->visited_triangles.end() && stop_on_contained && !h_uc &&
	    (polygon->interior_points.size() > 1 || polygon->poly.size() > 3)) {
	    //polygon->print();
	    //polygon->cdt_inputs_print("cdt_poly.c");
	    //polygon->polygon_plot("cdt_poly.plot3");
	    bool cdt_status = oriented_polycdt(polygon, reproject);
	    if (cdt_status) {
		//tris_set_plot(tris, "patch.plot3");
		return (long)polygon->tris.size();
	    } else {
		struct bu_vls fname = BU_VLS_INIT_ZERO;
		//std::cerr << "cdt() failure\n";
		bu_vls_sprintf(&fname, "%d-cdt_failure_poly_3d.plot3", f_id);
		polygon->polygon_plot_in_plane(bu_vls_cstr(&fname));
		bu_vls_free(&fname);
		return -1;
	    }
	}

	if (ts.empty()) {
	    // That's all the triangles from this ring - if we haven't
	    // terminated yet, pull the next triangle set.

	    if (!stop_on_contained && reproject) {
		angle = 0.75 * angle;
	    }

	    // We queue these up in a specific order - we want any triangles
	    // actually using flipped or uncontained vertices to be at the top
	    // of the stack (i.e. the first ones tried.  polygon_tris is responsible
	    // for sorting in priority order.
	    std::vector<struct ctriangle_t> ntris = polygon_tris(polygon, angle, stop_on_contained, 0);

	    if (ctriangle_vect_cmp(ptris, ntris)) {
		if (h_uc || (stop_on_contained && polygon->poly.size() <= 3)) {
		    if (!grow_loop_failure_ok) {
			struct bu_vls fname = BU_VLS_INIT_ZERO;
			std::cerr << "Error - new triangle set from polygon edge is the same as the previous triangle set.  Infinite loop, aborting\n";
			std::vector<struct ctriangle_t> infinite_loop_tris = polygon_tris(polygon, angle, stop_on_contained, 0);
			bu_vls_sprintf(&fname, "%d-infinite_loop_poly_2d.plot3", f_id);
			polygon->polygon_plot(bu_vls_cstr(&fname));
			bu_vls_sprintf(&fname, "%d-infinite_loop_poly_3d.plot3", f_id);
			polygon->polygon_plot_in_plane(bu_vls_cstr(&fname));
			bu_vls_sprintf(&fname, "%d-infinite_loop_tris.plot3", f_id);
			ctris_vect_plot(infinite_loop_tris, bu_vls_cstr(&fname));
			bu_vls_sprintf(&fname, "%d-infinite_loop.cdtmesh", f_id);
			serialize(bu_vls_cstr(&fname));
			bu_vls_free(&fname);
		    }
		    return -1;
		} else {
		    // We're not in a repair situation, and we've already tried
		    // the current triangle candidate set with no polygon
		    // change.  Generate triangles.
		    bool cdt_status = oriented_polycdt(polygon, reproject);
		    if (cdt_status) {
			//tris_set_plot(tris, "patch.plot3");
			return 1;
		    } else {
			struct bu_vls fname = BU_VLS_INIT_ZERO;
			std::cerr << "cdt() failure\n";
			bu_vls_sprintf(&fname, "%d-cdt_failure_poly_3d.plot3", f_id);
			polygon->polygon_plot_in_plane(bu_vls_cstr(&fname));
			bu_vls_free(&fname);
			return -1;
		    }
		}
	    }
	    ptris.clear();
	    ptris = ntris;

	    for (size_t i = 0; i < ntris.size(); i++) {
		ts.push(ntris[i]);
	    }

	    if (!stop_on_contained && ts.empty()) {
		// per the current angle criteria we've got everything, and we're
		// not concerned with contained points so this isn't an indication
		// of an error condition.  Generate triangles.
		// TODO - this produces polygon tris, which will need to be checked for flippping
		bool cdt_status = oriented_polycdt(polygon, reproject);
		if (cdt_status) {
		    //tris_set_plot(tris, "patch.plot3");
		    return 1;
		} else {
		    struct bu_vls fname = BU_VLS_INIT_ZERO;
		    std::cerr << "cdt() failure\n";
		    bu_vls_sprintf(&fname, "%d-cdt_failure_poly_3d.plot3", f_id);
		    polygon->polygon_plot_in_plane(bu_vls_cstr(&fname));
		    bu_vls_free(&fname);
		    return -1;
		}
	    }
	}
    }

    if (!grow_loop_failure_ok) {
	std::cout << "Error - loop growth terminated but conditions for triangulation were never satisfied!\n";
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fname, "%d-failed_patch_poly_2d.plot3", f_id);
	polygon->polygon_plot(bu_vls_cstr(&fname));
	bu_vls_sprintf(&fname, "%d-failed_patch_poly_3d.plot3", f_id);
	polygon->polygon_plot_in_plane(bu_vls_cstr(&fname));
    }
    return -1;
}

bool
cdt_mesh_t::process_seed_tri(triangle_t &seed, bool repairit, double deg, ON_Plane *pplane)
{
    // build an initial loop from a nearby valid triangle
    cpolygon_t *polygon = build_initial_loop(seed, repairit, pplane);

    if (!polygon) {
	std::cerr << "Could not build initial valid loop\n";
	return false;
    }

    // Grow until we contain the seed and its associated problem data
    int tri_cnt = grow_loop(polygon, deg, repairit, seed, (pplane == NULL));
    if (tri_cnt < 0) {
	if (!grow_loop_failure_ok) {
	    std::cerr << "grow_loop failure\n";
	}
	delete polygon;
	return false;
    }

    // If nothing to do at the seed, we don't change the mesh
    if (tri_cnt == 0) {
	delete polygon;
	return true;
    }

    // Remove everything the patch claimed
    std::set<triangle_t>::iterator v_it;
    for (v_it = polygon->visited_triangles.begin(); v_it != polygon->visited_triangles.end(); v_it++) {
	triangle_t vt = *v_it;
	seed_tris.erase(vt);
	tri_remove(vt);
    }

    // Add in the replacement triangles
    for (v_it = polygon->tris.begin(); v_it != polygon->tris.end(); v_it++) {
	triangle_t vt = *v_it;
	vt.m = this;
	new_tris.insert(vt);
	tri_add(vt);
    }

    delete polygon;

    return true;
}

// TODO - should be a function provided by cpolygon_t, or even better maintained
// internally - we need essentially the same thing for point_in_poly...
int *
loop_to_bgpoly(cpolygon_t *loop)
{
    int *opoly = (int *)bu_calloc(loop->poly.size()+1, sizeof(int), "polygon points");

    size_t vcnt = 1;
    cpolyedge_t *pe = (*loop->poly.begin());
    cpolyedge_t *first = pe;
    cpolyedge_t *next = pe->next;

    opoly[vcnt-1] = loop->p2o[pe->v2d[0]];
    opoly[vcnt] = loop->p2o[pe->v2d[1]];

    while (first != next) {
	vcnt++;
	opoly[vcnt] = loop->p2o[next->v2d[1]];
	next = next->next;
	if (vcnt > loop->poly.size()) {
	    bu_free(opoly, "free libbg 2d points array)");
	    std::cerr << "cdt attempt on infinite loop in outer loop (shouldn't be possible - closed() test failed to detect this somehow...)\n";
	    return NULL;
	}
    }

    return opoly;
}

bool
cdt_mesh_t::cdt()
{
    if (!outer_loop.closed()) {
	bu_log("%d: outer loop reports not closed!\n", f_id);
	return false;
    }
    std::map<int, cpolygon_t*>::iterator il_it;
    for (il_it = inner_loops.begin(); il_it != inner_loops.end(); il_it++) {
	cpolygon_t *il = il_it->second;
	if (!il->closed()) {
	    bu_log("inner loop reports not closed!\n");
	    return false;
	}
    }

    //cdt_inputs_print("cdt_inputs.c");
    //cdt_inputs_plot("cdt_inputs.plot3");

    // Aspect-ratio normalization: if the face UV bounding box has a large
    // aspect ratio (e.g. a long cylindrical face), scale the shorter axis in
    // bgp_2d so the CDT triangulator sees a near-square domain.  This avoids
    // numerically degenerate initial triangles for faces like NIST Face 35
    // whose UV extents span ~93:1.  The scale is applied only to the local
    // bgp_2d array; m_pnts_2d is unchanged, so all upstream UV coordinates
    // remain correct.  bg_nested_poly_triangulate is invariant to uniform
    // axis scaling (it only determines topology, not UV values).
    double umin = std::numeric_limits<double>::max();
    double umax = -std::numeric_limits<double>::max();
    double vmin = std::numeric_limits<double>::max();
    double vmax = -std::numeric_limits<double>::max();
    for (size_t i = 0; i < m_pnts_2d.size(); i++) {
	double u = m_pnts_2d[i].first;
	double v = m_pnts_2d[i].second;
	if (u < umin) umin = u;
	if (u > umax) umax = u;
	if (v < vmin) vmin = v;
	if (v > vmax) vmax = v;
    }
    double uscale = 1.0, vscale = 1.0;
    {
	double urng = umax - umin;
	double vrng = vmax - vmin;
	if (urng > 0.0 && vrng > 0.0) {
	    double ratio = (urng > vrng) ? urng / vrng : vrng / urng;
	    if (ratio > 10.0) {
		if (urng < vrng)
		    uscale = vrng / urng;
		else
		    vscale = urng / vrng;
		bu_log("Face %d: UV aspect ratio %.1f:1, normalizing (uscale=%.4g vscale=%.4g)\n",
		    f_id, ratio, uscale, vscale);
	    }
	}
    }

    point2d_t *bgp_2d = (point2d_t *)bu_calloc(m_pnts_2d.size() + 1, sizeof(point2d_t), "2D points array");
    for (size_t i = 0; i < m_pnts_2d.size(); i++) {
	bgp_2d[i][X] = m_pnts_2d[i].first * uscale;
	bgp_2d[i][Y] = m_pnts_2d[i].second * vscale;
    }

    int *faces = NULL;
    int num_faces = 0;

    // Walk the outer loop and build the libbg polygon
    int *opoly = loop_to_bgpoly(&outer_loop);
    if (!opoly) {
	return false;
    }

    const int **holes_array = NULL;
    size_t *holes_npts = NULL;
    int holes_cnt = inner_loops.size();
    if (holes_cnt) {
	holes_array = (const int **)bu_calloc(holes_cnt+1, sizeof(int *), "holes array");
	holes_npts = (size_t *)bu_calloc(holes_cnt+1, sizeof(size_t), "hole pntcnt array");
	int loop_cnt = 0;
	for (il_it = inner_loops.begin(); il_it != inner_loops.end(); il_it++) {
	    cpolygon_t *inl = il_it->second;
	    holes_array[loop_cnt] = loop_to_bgpoly(inl);
	    holes_npts[loop_cnt] = inl->poly.size()+1;
	    loop_cnt++;
	}
    }

    // Build Steiner array, filtering out any points that fall inside a hole.
    // Sampled interior points inside trimmed-away hole regions confuse detria.
    // Pre-build per-hole 2D polygon arrays once (reused for each Steiner point test).
    std::vector<std::vector<double>> hole_polys_flat; // pairs of (x,y) stored flat
    std::vector<size_t> hole_polys_npts;
    if (holes_cnt) {
	hole_polys_flat.resize(holes_cnt);
	hole_polys_npts.resize(holes_cnt);
	for (int hi = 0; hi < holes_cnt; hi++) {
	    hole_polys_npts[hi] = holes_npts[hi];
	    hole_polys_flat[hi].resize(holes_npts[hi] * 2);
	    for (size_t hj = 0; hj < holes_npts[hi]; hj++) {
		hole_polys_flat[hi][hj*2+0] = bgp_2d[holes_array[hi][hj]][X];
		hole_polys_flat[hi][hj*2+1] = bgp_2d[holes_array[hi][hj]][Y];
	    }
	}
    }

    std::vector<int> steiner_vec;
    steiner_vec.reserve(m_interior_pnts.size());
    for (auto p_it = m_interior_pnts.begin(); p_it != m_interior_pnts.end(); p_it++) {
	int idx = (int)*p_it;
	bool in_hole = false;
	for (int hi = 0; hi < holes_cnt && !in_hole; hi++) {
	    point2d_t test_pnt;
	    V2SET(test_pnt, bgp_2d[idx][X], bgp_2d[idx][Y]);
	    const point2d_t *hpoly = (const point2d_t *)hole_polys_flat[hi].data();
	    if (bg_pnt_in_polygon(hole_polys_npts[hi], hpoly, (const point2d_t *)&test_pnt))
		in_hole = true;
	}
	if (!in_hole)
	    steiner_vec.push_back(idx);
    }
    int *steiner = steiner_vec.empty() ? NULL : steiner_vec.data();
    size_t steiner_cnt = steiner_vec.size();

    // Sanity check: every polygon array must be closed (first index == last index).
    // Detria uses front()==back() to detect the closed-polyline format; warn if
    // that invariant is ever violated.
    if (holes_cnt) {
	size_t opoly_n = outer_loop.poly.size()+1;
	if (opoly[0] != opoly[opoly_n-1])
	    bu_log("Face %d CDT: outer polygon NOT CLOSED (first=%d last=%d)\n",
		   f_id, opoly[0], opoly[opoly_n-1]);
	for (int hi = 0; hi < holes_cnt; hi++) {
	    size_t hn = holes_npts[hi];
	    if (holes_array[hi][0] != holes_array[hi][hn-1])
		bu_log("Face %d CDT: hole[%d] NOT CLOSED (first=%d last=%d)\n",
		       f_id, hi, holes_array[hi][0], holes_array[hi][hn-1]);
	}
    }

    bool result = (bool)!bg_nested_poly_triangulate(&faces, &num_faces,
		  NULL, NULL, opoly, outer_loop.poly.size()+1, holes_array, holes_npts, holes_cnt,
		  steiner, steiner_cnt, bgp_2d, m_pnts_2d.size(),
		  TRI_CONSTRAINED_DELAUNAY);

    if (!result) {
	bu_log("Face %d: bg_nested_poly_triangulate FAILED (bnd_pnts=%zu steiner=%zu/%zu holes=%d)\n",
	    f_id, outer_loop.poly.size(), steiner_cnt, m_interior_pnts.size(), holes_cnt);

	// Dump a stand-alone C test program so the failure can be reproduced
	// and scrutinised independently of the full CDT pipeline.
	struct bu_vls fname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fname, "cdt_face%d_fail.c", f_id);
	FILE *df = fopen(bu_vls_cstr(&fname), "w");
	if (df) {
	    fprintf(df, "#include <stdio.h>\n");
	    fprintf(df, "#include \"bu/malloc.h\"\n");
	    fprintf(df, "#include \"bg/polygon.h\"\n");
	    fprintf(df, "int main() {\n");
	    size_t np = m_pnts_2d.size();
	    fprintf(df, "    point2d_t *bgp_2d = (point2d_t *)bu_calloc(%zu, sizeof(point2d_t), \"2d pts\");\n", np);
	    for (size_t i = 0; i < np; i++) {
		fprintf(df, "    bgp_2d[%zu][X] = %.17g;\n", i, m_pnts_2d[i].first  * uscale);
		fprintf(df, "    bgp_2d[%zu][Y] = %.17g;\n", i, m_pnts_2d[i].second * vscale);
	    }
	    // The polygon array for bg_nested_poly_triangulate uses a closed format:
	    // the first vertex index is repeated as the last entry (size = edge_count + 1).
	    size_t on = outer_loop.poly.size() + 1;
	    fprintf(df, "    int *opoly = (int *)bu_calloc(%zu, sizeof(int), \"opoly\");\n", on);
	    for (size_t i = 0; i < on; i++)
		fprintf(df, "    opoly[%zu] = %d;\n", i, opoly[i]);
	    if (holes_cnt) {
		fprintf(df, "    const int **holes = (const int **)bu_calloc(%d+1, sizeof(int *), \"holes\");\n", holes_cnt);
		fprintf(df, "    size_t *holes_npts = (size_t *)bu_calloc(%d+1, sizeof(size_t), \"hnpts\");\n", holes_cnt);
		for (int hi = 0; hi < holes_cnt; hi++) {
		    size_t hn = holes_npts[hi];
		    fprintf(df, "    int *hole%d = (int *)bu_calloc(%zu, sizeof(int), \"h%d\");\n", hi, hn, hi);
		    for (size_t hj = 0; hj < hn; hj++)
			fprintf(df, "    hole%d[%zu] = %d;\n", hi, hj, holes_array[hi][hj]);
		    fprintf(df, "    holes[%d] = hole%d; holes_npts[%d] = %zu;\n", hi, hi, hi, hn);
		}
	    } else {
		fprintf(df, "    const int **holes = NULL;\n");
		fprintf(df, "    size_t *holes_npts = NULL;\n");
	    }
	    if (steiner_cnt) {
		fprintf(df, "    int *steiner = (int *)bu_calloc(%zu, sizeof(int), \"stei\");\n", steiner_cnt);
		for (size_t si = 0; si < steiner_cnt; si++)
		    fprintf(df, "    steiner[%zu] = %d;\n", si, steiner[si]);
	    } else {
		fprintf(df, "    int *steiner = NULL;\n");
	    }
	    fprintf(df, "    int *faces = NULL; int num_faces = 0;\n");
	    fprintf(df, "    int r = !bg_nested_poly_triangulate(&faces, &num_faces,\n");
	    fprintf(df, "        NULL, NULL, opoly, %zu, holes, holes_npts, %d,\n", on, holes_cnt);
	    fprintf(df, "        steiner, %zu, bgp_2d, %zu, TRI_CONSTRAINED_DELAUNAY);\n", steiner_cnt, np);
	    fprintf(df, "    if (r) printf(\"success\\n\"); else printf(\"FAIL\\n\");\n");
	    fprintf(df, "    return !r;\n}\n");
	    fclose(df);
	    bu_log("Face %d: CDT failure inputs written to %s\n", f_id, bu_vls_cstr(&fname));
	}
	bu_vls_free(&fname);
    }

    tris_2d.clear();
    if (result) {
	for (int i = 0; i < num_faces; i++) {
	    triangle_t t;
	    t.v[0] = faces[3*i+0];
	    t.v[1] = faces[3*i+1];
	    t.v[2] = faces[3*i+2];

	    tris_2d.push_back(t);
	}

	bu_free(faces, "faces array");
    }

    bu_free(bgp_2d, "free libbg 2d points array)");
    bu_free(opoly, "polygon points");

    if (holes_cnt) {
	for (int i = 0; i < holes_cnt; i++) {
	    bu_free((void *)holes_array[i], "hole array");
	}
	bu_free((void *)holes_array, "holes array");
	bu_free(holes_npts, "holes array");
    }

    // steiner points into steiner_vec's internal buffer (not bu_calloc'd),
    // so no explicit free is needed here; steiner_vec cleans itself up.

    // Use the 2D triangles to create the face 3D triangle mesh
    reset();
    std::vector<triangle_t>::iterator tr_it;
    for (tr_it = tris_2d.begin(); tr_it != tris_2d.end(); tr_it++) {
	triangle_t tri2d = *tr_it;
	triangle_t tri3d;

	// NOTE: There may be multiple instances of 3D points in the pnts array
	// if different 2D points map to the same 3D point.  For 3D triangle we
	// want all of them pointing to one index for the same point,
	// regardless of which copy they were originally mapped to.  The 3D
	// pointer to 3D index p2ind map is updated every time a point is
	// added, which means the map value for a specific ON_3dPoint pointer
	// key will always point to the highest index value in the vector to be
	// assigned that particular pointer. This means that if we get the
	// ON_3dPoint pointer via the p2d3d index map and the pnts vector, then
	// use that ON_3dPoint pointer and p2ind to get an index value, we will
	// always end up with the same index value.
	//
	// In essence, the multiple lookups below are used to give us the same
	// 3D index uniqueness guarantee we already have for 3D point pointer
	// values.

	tri3d.v[0] = p2ind[pnts[p2d3d[tri2d.v[0]]]];
	tri3d.v[1] = p2ind[pnts[p2d3d[tri2d.v[1]]]];
	tri3d.v[2] = p2ind[pnts[p2d3d[tri2d.v[2]]]];

	ON_3dVector tdir = tnorm(tri3d);
	ON_3dVector bdir = bnorm(tri3d);
	if (tdir.Length() > 0 && bdir.Length() > 0 && ON_DotProduct(tdir, bdir) < 0.1) {
	    long tmp = tri3d.v[1];
	    tri3d.v[1] = tri3d.v[2];
	    tri3d.v[2] = tmp;
	}

	tri_add(tri3d);
    }

    return result;
}

bool
cdt_mesh_t::repair()
{
    // If we have edges with > 2 triangles and 3 or more of those triangles are
    // not problem_edge triangles, we have what amounts to a self-intersecting
    // mesh.  I'm not sure yet what to do about it - the obvious starting point
    // is to pick one of the triangles and yank it, along with any of its edge-
    // neighbors that overlap with any of the other triangles associated with
    // the original overloaded edge, and mark all the involved vertices as
    // uncontained.  But I'm not sure yet what the subsequent implications are
    // for the mesh processing...

    if (this->self_intersecting_mesh()) {
	std::cerr << f_id << ": self intersecting mesh\n";
	tris_plot("self_intersecting_mesh.plot3");
	return false;
    }

    grow_loop_failure_ok = false;

    remove_dangling_tris();

    // *Wrong* triangles: problem edge and/or flipped normal triangles.  Handle
    // those first, so the subsequent clean-up pass doesn't have to worry about
    // errors they might introduce.
    std::vector<triangle_t> f_tris = this->interior_incorrect_normals();
    std::vector<triangle_t> e_tris = this->problem_edge_tris();
    new_tris.clear();
    seed_tris.clear();
    seed_tris.insert(e_tris.begin(), e_tris.end());
    seed_tris.insert(f_tris.begin(), f_tris.end());

    size_t st_size = seed_tris.size();
    while (seed_tris.size()) {
	triangle_t seed = *seed_tris.begin();

	bool pseed = process_seed_tri(seed, true, 170.0, NULL);

	if (!pseed || seed_tris.size() >= st_size) {
	    std::cerr << f_id << ": Error - failed to process repair seed triangle!\n";
	    struct bu_vls fname = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&fname, "%d-failed_seed.plot3", f_id);
	    tri_plot(seed, bu_vls_cstr(&fname));
	    bu_vls_sprintf(&fname, "%d-failed_seed_mesh.plot3", f_id);
	    tris_plot(bu_vls_cstr(&fname));
	    bu_vls_sprintf(&fname, "%d-failed_seed.cdtmesh", f_id);
	    serialize(bu_vls_cstr(&fname));
	    bu_vls_free(&fname);
	    return false;
	}

	st_size = seed_tris.size();

	//tris_plot("mesh_post_patch.plot3");
    }

#if 1
    // Second-pass repair: detect topology defects introduced during the first
    // pass.  Force a fresh boundary_edges_update() so newly-created problem
    // edges are discovered.
    boundary_edges_stale = true;

    // For each edge, check if it is a boundary edge.  If not, its mirror
    // edge should have an associated triangle that is different from the
    // current triangle.  If not, we need to resolve the issue...
    std::map<edge_t, size_t>::iterator e_it;
    for (e_it = edges2tris.begin(); e_it != edges2tris.end(); e_it++) {
	edge_t e_1 = e_it->first;
	uedge_t ue(e_1);
	if (boundary_edges.find(ue) != boundary_edges.end()) continue;
	size_t t1 = e_it->second;
	edge_t e_2(e_1.v[1], e_1.v[0]);
	auto fe2 = edges2tris.find(e_2);
	if (fe2 == edges2tris.end()) continue; // reverse edge absent — handled below
	size_t t2 = fe2->second;
	if (t1 == t2) {
	    // directional edges both point to the same triangle - problem
	    std::cout << "directional edge pair referencing same triangle!\n";
	    for (int i = 0; i < 3; i++) {
		// Every triangle on one of the vertices of this triangle is
		// suspect and has to be considered
		std::vector<triangle_t> faces = vertex_face_neighbors(tris_vect[t1].v[i]);
		seed_tris.insert(faces.begin(), faces.end());
	    }
	}
    }

    // Also catch misoriented pairs (two triangles sharing a directed edge in
    // the same sense): the reverse directed edge is absent from edges2tris
    // while uedges2tris has two entries for the undirected edge.  These
    // cannot be repaired by grow_loop (which cannot cross brep boundary
    // points), so fix them directly by flipping the triangle whose surface
    // normal is inconsistent with the face normal.
    {
	std::set<uedge_t> misoriented;
	for (auto const& ue_entry : uedges2tris) {
	    if (ue_entry.second.size() != 2) continue;
	    uedge_t ue = ue_entry.first;
	    auto tit = ue_entry.second.begin();
	    const triangle_t &ta = tris_vect[*tit]; ++tit;
	    const triangle_t &tb = tris_vect[*tit];
	    // Determine directed sense each triangle uses for this uedge.
	    bool ta_fwd = false;
	    for (int i = 0; i < 3; i++) {
		if (ta.v[i] == ue.v[0] && ta.v[(i+1)%3] == ue.v[1]) { ta_fwd = true;  break; }
		if (ta.v[i] == ue.v[1] && ta.v[(i+1)%3] == ue.v[0]) { ta_fwd = false; break; }
	    }
	    bool tb_fwd = false;
	    for (int i = 0; i < 3; i++) {
		if (tb.v[i] == ue.v[0] && tb.v[(i+1)%3] == ue.v[1]) { tb_fwd = true;  break; }
		if (tb.v[i] == ue.v[1] && tb.v[(i+1)%3] == ue.v[0]) { tb_fwd = false; break; }
	    }
	    if (ta_fwd == tb_fwd)
		misoriented.insert(ue);
	}
	// For each misoriented pair, flip the triangle whose normal disagrees
	// with the BREP face normal.  Do this with remove+re-add so all mesh
	// maps stay consistent.
	int flip_pass = 0;
	while (!misoriented.empty() && flip_pass++ < 10) {
	    std::set<uedge_t> still_misoriented;
	    for (auto const& ue : misoriented) {
		auto it2 = uedges2tris.find(ue);
		if (it2 == uedges2tris.end() || it2->second.size() != 2) continue;
		auto tit2 = it2->second.begin();
		triangle_t ta = tris_vect[*tit2]; ++tit2;
		triangle_t tb = tris_vect[*tit2];
		ta.m = this; tb.m = this;
		ON_3dVector ta_n = tnorm(ta);
		ON_3dVector tb_n = tnorm(tb);
		ON_3dVector bdir = bnorm(ta);
		bool ta_ok = (ON_DotProduct(ta_n, bdir) >= 0);
		bool tb_ok = (ON_DotProduct(tb_n, bdir) >= 0);
		// Flip the one that is inconsistent with the face normal.
		// If both are consistent (or neither is), flip tb as a
		// tiebreaker to try to create a manifold neighbourhood.
		triangle_t bad = (!ta_ok && tb_ok) ? ta : tb;
		tri_remove(bad);
		long tmp = bad.v[1];
		bad.v[1] = bad.v[2];
		bad.v[2] = tmp;
		tri_add(bad);
		// Check if the edge is still misoriented after the flip.
		auto it3 = uedges2tris.find(ue);
		if (it3 != uedges2tris.end() && it3->second.size() == 2) {
		    tit2 = it3->second.begin();
		    const triangle_t &na = tris_vect[*tit2]; ++tit2;
		    const triangle_t &nb = tris_vect[*tit2];
		    bool na_fwd = false;
		    for (int i = 0; i < 3; i++) {
			if (na.v[i] == ue.v[0] && na.v[(i+1)%3] == ue.v[1]) { na_fwd = true;  break; }
			if (na.v[i] == ue.v[1] && na.v[(i+1)%3] == ue.v[0]) { na_fwd = false; break; }
		    }
		    bool nb_fwd = false;
		    for (int i = 0; i < 3; i++) {
			if (nb.v[i] == ue.v[0] && nb.v[(i+1)%3] == ue.v[1]) { nb_fwd = true;  break; }
			if (nb.v[i] == ue.v[1] && nb.v[(i+1)%3] == ue.v[0]) { nb_fwd = false; break; }
		    }
		    if (na_fwd == nb_fwd)
			still_misoriented.insert(ue);
		}
	    }
	    misoriented = still_misoriented;
	}
    }

    size_t try_cnt = 0;
    std::set<triangle_t>::iterator s_it = seed_tris.begin();
    while (seed_tris.size()) {
	triangle_t seed = *s_it;

	bool pseed = process_seed_tri(seed, true, 170.0, NULL);

	if (seed_tris.size() >= st_size) {
	    s_it++;
	} else {
	    s_it = seed_tris.begin();
	}

	if (!pseed && try_cnt > seed_tris.size()) {
	    std::cerr << f_id << ": Error - failed to process repair seed triangle!\n";
	    struct bu_vls fname = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&fname, "%d-failed_seed.plot3", f_id);
	    tri_plot(seed, bu_vls_cstr(&fname));
	    bu_vls_sprintf(&fname, "%d-failed_seed_mesh.plot3", f_id);
	    tris_plot(bu_vls_cstr(&fname));
	    bu_vls_sprintf(&fname, "%d-failed_seed.cdtmesh", f_id);
	    serialize(bu_vls_cstr(&fname));
	    bu_vls_free(&fname);
	    return false;
	}

	try_cnt++;
	//tris_plot("mesh_post_patch.plot3");
    }
#endif

    // Now that the out-and-out problem triangles have been handled,
    // remesh near singularities to try and produce more reasonable
    // triangles.

    if (has_singularities) {
	std::vector<triangle_t> s_tris = this->singularity_triangles();
	if (s_tris.size() > 2) {
	    seed_tris.insert(s_tris.begin(), s_tris.end());

	    st_size = seed_tris.size();
	    while (seed_tris.size()) {
		triangle_t seed = *seed_tris.begin();

		double deg = max_angle_delta(seed, s_tris);
		bool pseed = process_seed_tri(seed, false, deg, NULL);

		if (!pseed || seed_tris.size() >= st_size) {
		    std::cerr << f_id << ":  Error - failed to process refinement seed triangle!\n";
		    struct bu_vls fname = BU_VLS_INIT_ZERO;
		    bu_vls_sprintf(&fname, "%d-failed_seed.plot3", f_id);
		    tri_plot(seed, bu_vls_cstr(&fname));
		    bu_vls_sprintf(&fname, "%d-failed_seed_mesh.plot3", f_id);
		    tris_plot(bu_vls_cstr(&fname));
		    bu_vls_sprintf(&fname, "%d-failed_seed.cdtmesh", f_id);
		    serialize(bu_vls_cstr(&fname));
		    bu_vls_free(&fname);
		    return false;
		    break;
		}

		st_size = seed_tris.size();

		//tris_plot("mesh_post_pretty.plot3");
	    }
	}
    }

    boundary_edges_update();
    if (problem_edges.size() > 0) {
	return false;
    }

    return true;
}

bool
cdt_mesh_t::optimize_process(double deg, ON_Plane *pplane)
{
    grow_loop_failure_ok = true;

    if (std::string(name) == std::string("c.s") && f_id == 0) {
	tris_plot("pre_smooth.plot3");
    }

    while (seed_tris.size()) {
	triangle_t seed = *seed_tris.begin();
	seed_tris.erase(seed);
	process_seed_tri(seed, false, deg, pplane);
    }

    if (std::string(name) == std::string("c.s") && f_id == 0) {
	tris_plot("smooth.plot3");
    }

    return true;
}

bool
cdt_mesh_t::optimize(double deg)
{
    seed_tris.clear();
    new_tris.clear();

    // If we're using this method for picking seed triangles
    // and the surface is planar, it's a no-op.
    if (planar()) return false;

    RTree<size_t, double, 3>::Iterator tree_it;
    tris_tree.GetFirst(tree_it);
    size_t t_ind;
    triangle_t tri;
    while (!tree_it.IsNull()) {
	t_ind = *tree_it;
	tri = tris_vect[t_ind];
	ON_3dVector tdir = tnorm(tri);
	ON_3dVector bdir = bnorm(tri);
	// larger angle between brep and triangle = seed candidate
	double d_ang = ang_deg(tdir, bdir);
	if (d_ang > deg) {
	    seed_tris.insert(tri);
	}
	++tree_it;
    }

    optimize_process(deg, NULL);

    return true;
}

bool
cdt_mesh_t::optimize(std::set<triangle_t> &seeds)
{
    seed_tris.clear();
    new_tris.clear();

    seed_tris = seeds;

    if (planar()) return false;

    // Calculate best fit plane and find the maximum angle of any seed tri from
    // that plane - that's our angle limit for the optimization build.
    ON_Plane fp = best_fit_plane(seeds);
    double deg = max_tri_angle(fp, seeds);
    optimize_process(deg, NULL);

    return true;
}

bool
cdt_mesh_t::optimize(std::set<triangle_t> &seeds, ON_Plane &pplane)
{
    seed_tris.clear();
    new_tris.clear();

    seed_tris = seeds;

    if (planar()) return false;

    // Calculate best fit plane and find the maximum angle of any seed tri from
    // that plane - that's our angle limit for the optimization build.
    double deg = max_tri_angle(pplane, seeds);

    ON_Plane poly_plane = pplane;
    optimize_process(deg, &poly_plane);

    return true;
}

bool
cdt_mesh_t::valid(int verbose)
{
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bool nret = true;
    bool eret = true;
    bool tret = true;
    bool topret = true;

    bool fplanar = planar();

    boundary_edges_update();

    RTree<size_t, double, 3>::Iterator tree_it;
    tris_tree.GetFirst(tree_it);
    size_t t_ind;
    triangle_t tri;
    while (!tree_it.IsNull()) {
	t_ind = *tree_it;
	tri = tris_vect[t_ind];
	ON_3dVector tdir = tnorm(tri);
	ON_3dVector bdir = bnorm(tri);
	if (tdir.Length() > 0 && bdir.Length() > 0 && ON_DotProduct(tdir, bdir) < 0.1) {
	    if (verbose > 0) {
		std::cout << name << " face " << f_id << ": invalid normals in mesh, triangle " << tri.ind << " (" << tri.v[0] << "," << tri.v[1] << "," << tri.v[2] << ")\n";
	    }
	    if (verbose > 1) {
		bu_vls_sprintf(&fname, "%d-invalid_normal_tri_%ld_%ld_%ld.plot3", f_id, tri.v[0], tri.v[1], tri.v[2]);
		tri_plot(tri, bu_vls_cstr(&fname));
	    }
	    nret = false;
	}
	++tree_it;
    }

    if (!nret && verbose > 1) {
	bu_vls_sprintf(&fname, "%d-invalid_normals_mesh.plot3", f_id);
	tris_plot(bu_vls_cstr(&fname));
	bu_vls_sprintf(&fname, "%d-invalid_normals.cdtmesh", f_id);
	serialize(bu_vls_cstr(&fname));
    }

    tris_tree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	t_ind = *tree_it;
	tri = tris_vect[t_ind];

	int epnt_cnt = 0;
	int bedge_cnt = 0;
	for (int i = 0; i < 3; i++) {
	    epnt_cnt = (ep.find(tri.v[i]) == ep.end()) ? epnt_cnt : epnt_cnt + 1;
	}
	std::set<uedge_t> ue = tri.uedges();
	std::set<uedge_t>::iterator ue_it;
	for (ue_it = ue.begin(); ue_it != ue.end(); ue_it++) {
	    if (boundary_edges.find(*ue_it) != boundary_edges.end()) {
		bedge_cnt++;
	    }
	}

	if (!fplanar && epnt_cnt == 3 && bedge_cnt < 2) {
	    std::cerr << "tri has three edge points, but only " << bedge_cnt << "  boundary edges??\n";
	    tret = false;
	}

	++tree_it;
    }

    if (!tret && verbose > 1) {
	bu_vls_sprintf(&fname, "%d-bad_edge_tri.plot3", f_id);
	tris_plot(bu_vls_cstr(&fname));
	bu_vls_sprintf(&fname, "%d-bad_edge_tri.cdtmesh", f_id);
	serialize(bu_vls_cstr(&fname));
    }

    if (problem_edges.size() > 0) {
	if (verbose > 0) {
	    std::cout << name << " face " << f_id << ": problem edges in mesh\n";
	}
	eret = false;
    }

    if (!eret && verbose > 1) {
	bu_vls_sprintf(&fname, "%d-invalid_normals_mesh.plot3", f_id);
	tris_plot(bu_vls_cstr(&fname));
	bu_vls_sprintf(&fname, "%d-invalid_edges.cdtmesh", f_id);
	serialize(bu_vls_cstr(&fname));
	bu_vls_sprintf(&fname, "%d-invalid_edges_boundary.plot3", f_id);
	boundary_edges_plot(bu_vls_cstr(&fname));
    }

    bu_vls_free(&fname);

    // Check all map lookups to make sure we can find what we expect to
    // find
    tris_tree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	t_ind = *tree_it;
	tri = tris_vect[t_ind];

	long i = tri.v[0];
	long j = tri.v[1];
	long k = tri.v[2];
	struct edge_t e[3];
	struct uedge_t ue[3];
	e[0].set(i, j);
	e[1].set(j, k);
	e[2].set(k, i);
	for (int ind = 0; ind < 3; ind++) {
	    ue[ind].set(e[ind].v[0], e[ind].v[1]);
	}
	for (int ind = 0; ind < 3; ind++) {
	    if (edges2tris[e[ind]] != tri.ind) {
		topret = false;
	    }
	    if (uedges2tris[ue[ind]].find(tri.ind) == uedges2tris[ue[ind]].end()) {
		topret = false;
	    }
	    if (v2tris[tri.v[ind]].find(tri.ind) == v2tris[tri.v[ind]].end()) {
		topret = false;
	    }
	    if (uedges2tris[ue[ind]].size() != 2 && brep_edges.find(ue[ind]) == brep_edges.end()) {
		std::cout << "not enough triangles for edge?\n";
		topret = false;
	    }
	}
	++tree_it;
    }

#if 0
    if ((nret && eret && tret && topret) && verbose > 0) {
	std::cout << name << " face " << f_id << ": valid\n";
    }
#endif

    return (nret && eret && tret && topret);
}

void cdt_mesh_t::boundary_edges_plot(const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    std::set<uedge_t> bedges = get_boundary_edges();
    std::set<uedge_t>::iterator b_it;
    for (b_it = bedges.begin(); b_it != bedges.end(); b_it++) {
	uedge_t ue = *b_it;
	plot_edge(ue, plot_file);
    }

    if (this->problem_edges.size()) {
	pl_color(plot_file, 255, 0, 0);
	for (b_it = problem_edges.begin(); b_it != problem_edges.end(); b_it++) {
	    uedge_t ue = *b_it;
	    plot_edge(ue, plot_file);
	}
    }

    fclose(plot_file);
}

#if 0
void cdt_mesh_t::plot_tri(const triangle_t &t, struct bu_color *buc, FILE *plot, int r, int g, int b)
#else
void cdt_mesh_t::plot_tri(const triangle_t &t, struct bu_color *buc, FILE *plot, int UNUSED(r), int UNUSED(g), int UNUSED(b))
#endif
{
    point_t p[3];
    point_t porig;
    point_t c = VINIT_ZERO;
    for (int i = 0; i < 3; i++) {
	ON_3dPoint *p3d = this->pnts[t.v[i]];
	VSET(p[i], p3d->x, p3d->y, p3d->z);
	c[X] += p3d->x;
	c[Y] += p3d->y;
	c[Z] += p3d->z;
    }
    c[X] = c[X]/3.0;
    c[Y] = c[Y]/3.0;
    c[Z] = c[Z]/3.0;

    for (size_t i = 0; i < 3; i++) {
	if (i == 0) {
	    VMOVE(porig, p[i]);
	    pdv_3move(plot, p[i]);
	}
	pdv_3cont(plot, p[i]);
    }
    pdv_3cont(plot, porig);
#if 0
    /* fill in the "interior" using the rgb color*/
    pl_color(plot, r, g, b);
    for (size_t i = 0; i < 3; i++) {
	pdv_3move(plot, p[i]);
	pdv_3cont(plot, c);
    }
#endif

#if 0
    /* Plot the triangle normal */
    pl_color(plot, 0, 255, 255);
    {
	ON_3dVector tn = this->tnorm(t);
	vect_t tnt;
	VSET(tnt, tn.x, tn.y, tn.z);
	point_t npnt;
	VADD2(npnt, tnt, c);
	pdv_3move(plot, c);
	pdv_3cont(plot, npnt);
    }
#endif

#if 0
    /* Plot the brep normal */
    pl_color(plot, 0, 100, 0);
    {
	ON_3dVector tn = this->bnorm(t);
	tn = tn * 0.5;
	vect_t tnt;
	VSET(tnt, tn.x, tn.y, tn.z);
	point_t npnt;
	VADD2(npnt, tnt, c);
	pdv_3move(plot, c);
	pdv_3cont(plot, npnt);
    }
#endif
    /* restore previous color */
    if (buc) {
	pl_color_buc(plot, buc);
    }
}

double
cdt_mesh_t::tri_pnt_r(long tri_ind)
{
    triangle_t tri = tris_vect[tri_ind];
    ON_3dPoint *p3d = pnts[tri.v[0]];
    ON_BoundingBox bb(*p3d, *p3d);
    for (int i = 1; i < 3; i++) {
	p3d = pnts[tri.v[i]];
	bb.Set(*p3d, true);
    }
    double bbd = bb.Diagonal().Length();
    return bbd * 0.01;
}

void cdt_mesh_t::face_neighbors_plot(const triangle_t &f, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    // Origin triangle has red interior
    std::vector<triangle_t> faces = this->face_neighbors(f);
    this->plot_tri(f, &c, plot_file, 255, 0, 0);

    // Neighbor triangles have blue interior
    for (size_t i = 0; i < faces.size(); i++) {
	triangle_t tri = faces[i];
	this->plot_tri(tri, &c, plot_file, 0, 0, 255);
    }

    fclose(plot_file);
}

void cdt_mesh_t::vertex_face_neighbors_plot(long vind, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    std::vector<triangle_t> faces = this->vertex_face_neighbors(vind);

    for (size_t i = 0; i < faces.size(); i++) {
	triangle_t tri = faces[i];
	this->plot_tri(tri, &c, plot_file, 0, 0, 255);
    }

    // Plot the vind point that is the source of the triangles
    pl_color(plot_file, 0, 255,0);
    ON_3dPoint *p = this->pnts[vind];
    vect_t pt;
    VSET(pt, p->x, p->y, p->z);
    pdv_3point(plot_file, pt);

    fclose(plot_file);
}

void cdt_mesh_t::interior_incorrect_normals_plot(const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    std::vector<triangle_t> faces = this->interior_incorrect_normals();
    for (size_t i = 0; i < faces.size(); i++) {
	this->plot_tri(faces[i], &c, plot_file, 0, 255, 0);
    }
    fclose(plot_file);
}

void cdt_mesh_t::tri_plot(const triangle_t &tri, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    this->plot_tri(tri, &c, plot_file, 255, 0, 0);

    fclose(plot_file);
}

void cdt_mesh_t::tri_plot(long ind, const char *filename)
{
    triangle_t tri = tris_vect[ind];
    tri_plot(tri, filename);
}

void cdt_mesh_t::ctris_vect_plot(std::vector<struct ctriangle_t> &tvect, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    for (size_t i = 0; i < tvect.size(); i++) {
	triangle_t tri(tvect[i].v[0], tvect[i].v[1], tvect[i].v[2]);
	this->plot_tri(tri, &c, plot_file, 255, 0, 0);
    }
    fclose(plot_file);
}

void cdt_mesh_t::tris_vect_plot(std::vector<triangle_t> &tvect, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    for (size_t i = 0; i < tvect.size(); i++) {
	triangle_t tri = tvect[i];
	this->plot_tri(tri, &c, plot_file, 255, 0, 0);
    }
    fclose(plot_file);
}

void cdt_mesh_t::tris_set_plot(std::set<triangle_t> &tset, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    std::set<triangle_t>::iterator s_it;

    for (s_it = tset.begin(); s_it != tset.end(); s_it++) {
	triangle_t tri = (*s_it);
	this->plot_tri(tri, &c, plot_file, 255, 0, 0);
    }
    fclose(plot_file);
}

void cdt_mesh_t::tris_set_plot(std::set<size_t> &tset, const char *filename)
{
    std::vector<triangle_t> ts_vect;

    std::set<size_t>::iterator ts_it;
    for (ts_it = tset.begin(); ts_it != tset.end(); ts_it++) {
	ts_vect.push_back(tris_vect[*ts_it]);
    }

    tris_vect_plot(ts_vect, filename);
}

void cdt_mesh_t::tris_plot(const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    RTree<size_t, double, 3>::Iterator tree_it;
    size_t t_ind;
    triangle_t tri;
    std::set<size_t> flip_tris;

    tris_tree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	t_ind = *tree_it;
	tri = tris_vect[t_ind];
	plot_tri(tri, &c, plot_file, 255, 0, 0);
	++tree_it;
    }
    fclose(plot_file);
}

#define BB_PLOT_2D(min, max) {         \
        fastf_t pt[4][3];                  \
        VSET(pt[0], max[X], min[Y], 0);    \
        VSET(pt[1], max[X], max[Y], 0);    \
        VSET(pt[2], min[X], max[Y], 0);    \
        VSET(pt[3], min[X], min[Y], 0);    \
        pdv_3move(plot_file, pt[0]); \
        pdv_3cont(plot_file, pt[1]); \
        pdv_3cont(plot_file, pt[2]); \
        pdv_3cont(plot_file, pt[3]); \
        pdv_3cont(plot_file, pt[0]); \
}

#define TREE_LEAF_FACE_3D(valp, a, b, c, d)  \
        pdv_3move(plot_file, pt[a]); \
    pdv_3cont(plot_file, pt[b]); \
    pdv_3cont(plot_file, pt[c]); \
    pdv_3cont(plot_file, pt[d]); \
    pdv_3cont(plot_file, pt[a]); \

#define BB_PLOT(min, max) {                 \
        fastf_t pt[8][3];                       \
        VSET(pt[0], max[X], min[Y], min[Z]);    \
        VSET(pt[1], max[X], max[Y], min[Z]);    \
        VSET(pt[2], max[X], max[Y], max[Z]);    \
        VSET(pt[3], max[X], min[Y], max[Z]);    \
        VSET(pt[4], min[X], min[Y], min[Z]);    \
        VSET(pt[5], min[X], max[Y], min[Z]);    \
        VSET(pt[6], min[X], max[Y], max[Z]);    \
        VSET(pt[7], min[X], min[Y], max[Z]);    \
        TREE_LEAF_FACE_3D(pt, 0, 1, 2, 3);      \
        TREE_LEAF_FACE_3D(pt, 4, 0, 3, 7);      \
        TREE_LEAF_FACE_3D(pt, 5, 4, 7, 6);      \
        TREE_LEAF_FACE_3D(pt, 1, 5, 6, 2);      \
}

void cdt_mesh_t::tris_rtree_plot(const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    RTree<size_t, double, 3>::Iterator tree_it;
    tris_tree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	double m_min[3];
	double m_max[3];
	tree_it.GetBounds(m_min, m_max);
	BB_PLOT(m_min, m_max);
	++tree_it;
    }
    fclose(plot_file);
}

void cdt_mesh_t::plot_edge(const uedge_t &ue, FILE *plot)
{
    point_t p1 = VINIT_ZERO;
    point_t p2 = VINIT_ZERO;
    ON_3dPoint *p3d1 = pnts[ue.v[0]];
    ON_3dPoint *p3d2 = pnts[ue.v[1]];
    VSET(p1, p3d1->x, p3d1->y, p3d1->z);
    VSET(p2, p3d2->x, p3d2->y, p3d2->z);
    pdv_3move(plot, p1);
    pdv_3cont(plot, p2);
}

void cdt_mesh_t::plot_edge(const bedge_seg_t *s, FILE *plot)
{
    point_t p1 = VINIT_ZERO;
    point_t p2 = VINIT_ZERO;
    ON_3dPoint *p3d1 = s->e_start;
    ON_3dPoint *p3d2 = s->e_end;
    VSET(p1, p3d1->x, p3d1->y, p3d1->z);
    VSET(p2, p3d2->x, p3d2->y, p3d2->z);
    pdv_3move(plot, p1);
    pdv_3cont(plot, p2);
}

void cdt_mesh_t::edge_set_plot(std::set<uedge_t> &eset, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    std::set<uedge_t>::iterator e_it;

    for (e_it = eset.begin(); e_it != eset.end(); e_it++) {
	plot_edge(*e_it, plot_file);
    }
    fclose(plot_file);
}

void cdt_mesh_t::edge_set_plot(std::set<uedge_t> &eset, const char *filename, int r, int g, int b)
{
    FILE* plot_file = fopen(filename, "w");

    pl_color(plot_file, r, g, b);

    std::set<uedge_t>::iterator e_it;

    for (e_it = eset.begin(); e_it != eset.end(); e_it++) {
	plot_edge(*e_it, plot_file);
    }
    fclose(plot_file);
}


void cdt_mesh_t::edge_set_plot(std::set<bedge_seg_t *> &eset, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    std::set<bedge_seg_t *>::iterator e_it;

    for (e_it = eset.begin(); e_it != eset.end(); e_it++) {
	plot_edge(*e_it, plot_file);
    }
    fclose(plot_file);
}

void cdt_mesh_t::edge_set_plot(std::set<bedge_seg_t *> &eset, const char *filename, int r, int g, int b)
{
    FILE* plot_file = fopen(filename, "w");

    pl_color(plot_file, r, g, b);

    std::set<bedge_seg_t *>::iterator e_it;

    for (e_it = eset.begin(); e_it != eset.end(); e_it++) {
	plot_edge(*e_it, plot_file);
    }
    fclose(plot_file);
}


void cdt_mesh_t::plot_tri_2d(const triangle_t &t, struct bu_color *buc, FILE *plot)
{
    point_t p[3];
    point_t porig;

    for (int i = 0; i < 3; i++) {
	VSET(p[i], m_pnts_2d[t.v[i]].first, m_pnts_2d[t.v[i]].second, 0);
    }

    for (size_t i = 0; i < 3; i++) {
	if (i == 0) {
	    VMOVE(porig, p[i]);
	    pdv_3move(plot, p[i]);
	}
	pdv_3cont(plot, p[i]);
    }
    pdv_3cont(plot, porig);

    /* restore previous color */
    pl_color_buc(plot, buc);
}

void cdt_mesh_t::tris_vect_plot_2d(std::vector<triangle_t> &tset, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    std::vector<triangle_t>::iterator s_it;

    for (s_it = tset.begin(); s_it != tset.end(); s_it++) {
	triangle_t tri = (*s_it);
	this->plot_tri_2d(tri, &c, plot_file);
    }
    fclose(plot_file);
}

void cdt_mesh_t::tris_plot_2d(const char *filename)
{
    tris_vect_plot_2d(tris_2d, filename);
}


/* Very simple dump of the cdt_mesh state */
bool
cdt_mesh_t::serialize(const char *fname)
{
    std::ofstream sfile(fname);

    if (!sfile.is_open()) {
	std::cerr << "Could not open file " << fname << " for writing, serialization failed\n";
	return false;
    }

    sfile << "V2\n";

    sfile << "POINTS " << pnts.size() << "\n";

    for (size_t i = 0; i < pnts.size(); i++) {
	sfile << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << pnts[i]->x << " ";
	sfile << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << pnts[i]->y << " ";
	sfile << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << pnts[i]->z;
	sfile << "\n";
    }


    sfile << "NORMALS " << normals.size() << "\n";

    for (size_t i = 0; i < normals.size(); i++) {
	sfile << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << normals[i]->x << " ";
	sfile << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << normals[i]->y << " ";
	sfile << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << normals[i]->z;
	sfile << "\n";
    }


    sfile << "NORMALMAP " << nmap.size() << "\n";

    std::map<long, long>::iterator m_it;
    for (m_it = nmap.begin(); m_it != nmap.end(); m_it++) {
	sfile << m_it->first << "," << m_it->second << "\n";
    }

    sfile << "TRIANGLES_VECT" << tris_vect.size() << "\n";
    std::vector<triangle_t>::iterator t_it;
    for (t_it = tris_vect.begin(); t_it != tris_vect.end(); t_it++) {
	sfile << (*t_it).v[0] << "," << (*t_it).v[1] << "," << (*t_it).v[2] << "," << (*t_it).ind << "\n";
    }

    sfile << "TRIANGLES_TREE" << tris_tree.Count() << "\n";
    RTree<size_t, double, 3>::Iterator tree_it;
    size_t t_ind;
    triangle_t tri;
    std::set<size_t> flip_tris;

    tris_tree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	double m_min[3];
	double m_max[3];
	tree_it.GetBounds(m_min, m_max);
	sfile << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << m_min[0] << " ";
	sfile << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << m_min[1] << " ";
	sfile << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << m_min[2] << " ";
	sfile << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << m_max[0] << " ";
	sfile << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << m_max[1] << " ";
	sfile << std::fixed << std::setprecision(std::numeric_limits<double>::max_digits10) << m_max[2] << " ";
	t_ind = *tree_it;
	tri = tris_vect[t_ind];
	sfile << tri.ind << "\n";
	++tree_it;
    }

    int m_bRev_digit = (m_bRev) ? 1 : 0;
    sfile << "m_bRev " << m_bRev_digit << "\n";

    sfile << "FACE_ID " << f_id << "\n";

    sfile << "SINGULARITIES " << sv.size() << "\n";
    std::set<long>::iterator v_it;
    for (v_it = sv.begin(); v_it != sv.end(); v_it++) {
	sfile << *v_it << "\n";
    }

    sfile << "BREP_EDGE_POINTS " << ep.size() << "\n";
    for (v_it = ep.begin(); v_it != ep.end(); v_it++) {
	sfile << *v_it << "\n";
    }


    sfile << "BREP_EDGES " << brep_edges.size() << "\n";
    std::set<uedge_t>::iterator b_it;
    for (b_it = brep_edges.begin(); b_it != brep_edges.end(); b_it++) {
	sfile << (*b_it).v[0] << "," << (*b_it).v[1] << "\n";
    }

    sfile.close();
    return true;
}

static double
str2d(std::string s)
{
    double d;
    std::stringstream ss(s);
    ss >> std::setprecision(std::numeric_limits<double>::max_digits10) >> std::fixed >> d;
    return d;
}

bool
cdt_mesh_t::deserialize(const char *fname)
{
    std::ifstream sfile(fname);

    if (!sfile.is_open()) {
	std::cerr << "Could not open file " << fname << " for reading, deserialization failed\n";
	return false;
    }

    int version = -1;
    std::string switch_line;

    // First line has to be serialization format version string
    if (std::getline(sfile,switch_line)) {
	if (switch_line == std::string("V1")) {
	    version = 1;
	}
    }
    if (version < 1 || version > 1) {
	std::cerr << "Invalid deserialization file - format version " << switch_line << "\n";
	return false;
    }

    pnts.clear();
    p2ind.clear();
    normals.clear();
    n2ind.clear();
    nmap.clear();
    tris_vect.clear();
    tris_tree.RemoveAll();

    uedges2tris.clear();
    seed_tris.clear();

    v2edges.clear();
    v2tris.clear();
    edges2tris.clear();

    // When loading a serialization, we don't have a parent Brep structure
    edge_pnts = NULL;
    b_edges = NULL;
    singularities = NULL;

    brep_edges.clear();
    ep.clear();
    sv.clear();

    boundary_edges.clear();
    boundary_edges_stale = true;
    bounding_box_stale = true;
    problem_edges.clear();

    while (std::getline(sfile,switch_line)) {
	std::cout << switch_line << "\n";
	size_t spos = switch_line.find_first_of(' ');
	std::string dtype = switch_line.substr(0, spos);
	switch_line.erase(0, spos+1);
	long lcnt = std::stol(switch_line);

	if (dtype == std::string("POINTS")) {
	    for (long i = 0; i < lcnt; i++) {
		std::string pline;
		std::getline(sfile,pline);
		spos = pline.find_first_of(' ');
		std::string xstr = pline.substr(0, spos);
		pline.erase(0, spos+1);
		spos = pline.find_first_of(' ');
		std::string ystr = pline.substr(0, spos);
		pline.erase(0, spos+1);
		std::string zstr = pline;
		double xval, yval, zval;
		xval = str2d(xstr);
		yval = str2d(ystr);
		zval = str2d(zstr);
		ON_3dPoint *p3d = new ON_3dPoint(xval, yval, zval);
		pnts.push_back(p3d);
		p2ind[p3d] = pnts.size() - 1;
	    }
	    continue;
	}

	if (dtype == std::string("NORMALS")) {
	    for (long i = 0; i < lcnt; i++) {
		std::string nline;
		std::getline(sfile,nline);
		spos = nline.find_first_of(' ');
		std::string xstr = nline.substr(0, spos);
		nline.erase(0, spos+1);
		spos = nline.find_first_of(' ');
		std::string ystr = nline.substr(0, spos);
		nline.erase(0, spos+1);
		std::string zstr = nline;
		double xval, yval, zval;
		xval = str2d(xstr);
		yval = str2d(ystr);
		zval = str2d(zstr);
		ON_3dPoint *n3d = new ON_3dPoint(xval, yval, zval);
		normals.push_back(n3d);
		n2ind[n3d] = normals.size() - 1;
	    }
	    continue;
	}

	if (dtype == std::string("NORMALMAP")) {
	    for (long i = 0; i < lcnt; i++) {
		std::string nmline;
		std::getline(sfile,nmline);
		spos = nmline.find_first_of(',');
		std::string kstr = nmline.substr(0, spos);
		nmline.erase(0, spos+1);
		std::string vstr = nmline;
		long key = std::stol(kstr);
		long val = std::stol(vstr);
		nmap[key] = val;
	    }
	    continue;
	}

	if (dtype == std::string("TRIANGLES_VECT")) {
	    for (long i = 0; i < lcnt; i++) {
		std::string tline;
		std::getline(sfile,tline);
		spos = tline.find_first_of(',');
		std::string v1str = tline.substr(0, spos);
		tline.erase(0, spos+1);
		spos = tline.find_first_of(',');
		std::string v2str = tline.substr(0, spos);
		tline.erase(0, spos+1);
		std::string v3str = tline;
		long v1 = std::stol(v1str);
		long v2 = std::stol(v2str);
		long v3 = std::stol(v3str);
		triangle_t tri(v1, v2, v3);
		// The tree is loaded separately - just do the basic population
		tri.ind = tris_vect.size();
		tris_vect.push_back(tri);
	    }
	    continue;
	}

	if (dtype == std::string("TRIANGLES_TREE")) {
	    for (long i = 0; i < lcnt; i++) {

		std::string pline;
		std::getline(sfile,pline);
		spos = pline.find_first_of(' ');
		std::string m_min0_str = pline.substr(0, spos);
		pline.erase(0, spos+1);
		spos = pline.find_first_of(' ');
		std::string m_min1_str = pline.substr(0, spos);
		pline.erase(0, spos+1);
		spos = pline.find_first_of(' ');
		std::string m_min2_str = pline.substr(0, spos);
		pline.erase(0, spos+1);
		spos = pline.find_first_of(' ');
		std::string m_max0_str = pline.substr(0, spos);
		pline.erase(0, spos+1);
		spos = pline.find_first_of(' ');
		std::string m_max1_str = pline.substr(0, spos);
		pline.erase(0, spos+1);
		spos = pline.find_first_of(' ');
		std::string m_max2_str = pline.substr(0, spos);
		pline.erase(0, spos+1);
		std::string tindstr = pline;
		double m_min[3], m_max[3];
		m_min[0] = str2d(m_min0_str);
		m_min[1] = str2d(m_min1_str);
		m_min[2] = str2d(m_min2_str);
		m_max[0] = str2d(m_max0_str);
		m_max[1] = str2d(m_max1_str);
		m_max[2] = str2d(m_max2_str);
		size_t tind = (size_t)std::stol(tindstr);
		tris_tree.Insert(m_min, m_max, tind);

		// Populate maps - this triangle is in the tree and thus active
		triangle_t tri = tris_vect[tind];
		long ti = tri.v[0];
		long tj = tri.v[1];
		long tk = tri.v[2];
		struct edge_t e[3];
		struct uedge_t ue[3];
		e[0].set(ti, tj);
		e[1].set(tj, tk);
		e[2].set(tk, ti);
		for (int ind = 0; ind < 3; ind++) {
		    ue[ind].set(e[ind].v[0], e[ind].v[1]);
		    edges2tris[e[ind]] = tind;
		    uedges2tris[uedge_t(e[ind])].insert(tind);
		    v2edges[e[ind].v[0]].insert(e[ind]);
		    v2tris[tri.v[ind]].insert(tind);
		}

	    }
	    boundary_edges_stale = true;
	    bounding_box_stale = true;
	    continue;
	}

	if (dtype == std::string("m_bRev")) {
	    m_bRev = (lcnt) ? true : false;
	    continue;
	}

	if (dtype == std::string("FACE_ID")) {
	    f_id = lcnt;
	    continue;
	}

	if (dtype == std::string("SINGULARITIES")) {
	    for (long i = 0; i < lcnt; i++) {
		std::string sline;
		std::getline(sfile,sline);
		long sval = std::stol(sline);
		sv.insert(sval);
	    }
	    continue;
	}

	if (dtype == std::string("BREP_EDGE_POINTS")) {
	    for (long i = 0; i < lcnt; i++) {
		std::string bepline;
		std::getline(sfile,bepline);
		long epval = std::stol(bepline);
		ep.insert(epval);
	    }
	    continue;
	}

	if (dtype == std::string("BREP_EDGES")) {
	    for (long i = 0; i < lcnt; i++) {
		std::string beline;
		std::getline(sfile,beline);
		spos = beline.find_first_of(',');
		std::string kstr = beline.substr(0, spos);
		beline.erase(0, spos+1);
		std::string vstr = beline;
		long v1 = std::stol(kstr);
		long v2 = std::stol(vstr);
		brep_edges.insert(uedge_t(v1, v2));
	    }
	    continue;
	}

	std::cerr << "Unexpected line:\n" << switch_line << "\nSerialization import failed.\n";
	return false;
    }

    boundary_edges_update();

    return true;
}


cpolygon_t *
cdt_mesh_t::build_initial_loop(triangle_t &seed, bool repairit, ON_Plane *pplane)
{
    std::set<uedge_t>::iterator u_it;

    cpolygon_t *polygon = new cpolygon_t;

    // Set up the 2D points.
    // We use the Brep normal for this, since the triangles are
    // problem triangles and their normals cannot be relied upon.
    ON_3dPoint sp = tcenter(seed);
    ON_3dVector sn = bnorm(seed);

    if (!pplane) {
	ON_Plane tri_plane(sp, sn);
	polygon->tplane = tri_plane;
	polygon->pdir = sn;
    } else {
	polygon->tplane = *pplane;
	polygon->pdir = pplane->Normal();
    }
    for (size_t i = 0; i < pnts.size(); i++) {
	double u, v;
	ON_3dPoint op3d = (*pnts[i]);
	polygon->tplane.ClosestPointTo(op3d, &u, &v);
	std::pair<double, double> proj_2d;
	proj_2d.first = u;
	proj_2d.second = v;
	polygon->pnts_2d.push_back(proj_2d);
	if (brep_edge_pnt(i)) {
	    polygon->brep_edge_pnts.insert(i);
	}
	polygon->p2o[i] = i;
	polygon->o2p[i] = i;
    }

    if (repairit) {
	// None of the edges or vertices from any of the problem triangles can be
	// in a polygon edge.  By definition, the seed is a problem triangle.
	std::set<long> seed_verts;
	for (int i = 0; i < 3; i++) {
	    seed_verts.insert(seed.v[i]);
	    // The exception to interior categorization is Brep boundary points -
	    // they are never interior or uncontained
	    if (brep_edge_pnt(seed.v[i])) {
		continue;
	    }
	    polygon->uncontained.insert(seed.v[i]);
	}

	int have_valid = 0;

	// We need a initial valid polygon loop to grow.  Poll the neighbor faces - if one
	// of them is valid, it will be used to build an initial loop
	std::vector<triangle_t> faces = face_neighbors(seed);
	for (size_t i = 0; i < faces.size(); i++) {
	    triangle_t t = faces[i];
	    if (seed_tris.find(t) == seed_tris.end()) {
		struct edge2d_t e1(polygon->o2p[t.v[0]], polygon->o2p[t.v[1]]);
		struct edge2d_t e2(polygon->o2p[t.v[1]], polygon->o2p[t.v[2]]);
		struct edge2d_t e3(polygon->o2p[t.v[2]], polygon->o2p[t.v[0]]);
		polygon->add_edge(e1);
		polygon->add_edge(e2);
		polygon->add_edge(e3);
		polygon->visited_triangles.insert(t);
		have_valid = 1;
		break;
	    }
	}

	// If we didn't find a valid mated edge triangle (urk?) try the vertices
	if (!have_valid) {
	    for (int i = 0; i < 3; i++) {
		std::vector<triangle_t> vfaces = vertex_face_neighbors(seed.v[i]);
		for (size_t j = 0; j < vfaces.size(); j++) {
		    triangle_t t = vfaces[j];
		    if (seed_tris.find(t) == seed_tris.end()) {
			struct edge2d_t e1(polygon->o2p[t.v[0]], polygon->o2p[t.v[1]]);
			struct edge2d_t e2(polygon->o2p[t.v[1]], polygon->o2p[t.v[2]]);
			struct edge2d_t e3(polygon->o2p[t.v[2]], polygon->o2p[t.v[0]]);
			polygon->add_edge(e1);
			polygon->add_edge(e2);
			polygon->add_edge(e3);
			polygon->visited_triangles.insert(t);
			have_valid = 1;
			break;
		    }
		}
		if (have_valid) {
		    break;
		}
	    }
	}

	// NONE of the triangles in the neighborhood are valid?  We'll have to hope that
	// subsequent processing of other seeds will put a proper mesh in contact with
	// this face...
	if (!have_valid) {
	    delete polygon;
	    return NULL;
	}

    } else {
	// If we're not repairing, start with the seed itself
	struct edge2d_t e1(polygon->o2p[seed.v[0]], polygon->o2p[seed.v[1]]);
	struct edge2d_t e2(polygon->o2p[seed.v[1]], polygon->o2p[seed.v[2]]);
	struct edge2d_t e3(polygon->o2p[seed.v[2]], polygon->o2p[seed.v[0]]);
	polygon->add_edge(e1);
	polygon->add_edge(e2);
	polygon->add_edge(e3);
	polygon->visited_triangles.insert(seed);
    }

    if (polygon->closed()) {
	return polygon;
    }

    delete polygon;
    return NULL;
}

void
cdt_mesh_t::best_fit_plane_plot(point_t *center, vect_t *norm, const char *fname)
{
    FILE* plot_file = fopen(fname, "w");
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    pl_color(plot_file, r, g, b);

    vect_t xbase, ybase, tip;
    vect_t x_1, x_2, y_1, y_2;
    bn_vec_perp(xbase, *norm);
    VCROSS(ybase, xbase, *norm);
    VUNITIZE(xbase);
    VUNITIZE(ybase);
    VSCALE(xbase, xbase, 10);
    VSCALE(ybase, ybase, 10);
    VADD2(x_1, *center, xbase);
    VSUB2(x_2, *center, xbase);
    VADD2(y_1, *center, ybase);
    VSUB2(y_2, *center, ybase);

    pdv_3move(plot_file, x_1);
    pdv_3cont(plot_file, x_2);
    pdv_3move(plot_file, y_1);
    pdv_3cont(plot_file, y_2);

    pdv_3move(plot_file, x_1);
    pdv_3cont(plot_file, y_1);
    pdv_3move(plot_file, x_2);
    pdv_3cont(plot_file, y_2);

    pdv_3move(plot_file, x_2);
    pdv_3cont(plot_file, y_1);
    pdv_3move(plot_file, x_1);
    pdv_3cont(plot_file, y_2);

    VSCALE(tip, *norm, 5);
    VADD2(tip, *center, tip);
    pdv_3move(plot_file, *center);
    pdv_3cont(plot_file, tip);

    fclose(plot_file);
}

/* TODO - best fit plane is good/preferable when the brep area we are
 * approximating is locally semi-planar, but if the repair area is not one
 * that can be projected into a plane without introducing self intersections,
 * we will need something more sophisticated.  Take a look at
 * Geogram's uv projection capabilities to see if they can help here.
 *
 * Also possibilities:
 * https://github.com/educelab/OpenABF
 * https://github.com/jpcy/xatlas
 */

/* Return the best available surface normal for mesh vertex vi.
 *
 * At NURBS singularities (poles) the directly-evaluated surface normal is
 * undefined, so we pre-compute an averaged normal from the surrounding
 * well-behaved surfaces and store it in s_cdt->singular_vert_to_norms.
 * This function returns that averaged normal for any vertex in sv, and the
 * ordinary normals[nmap[vi]] for all other vertices.  m_bRev is applied so
 * callers need not worry about face orientation.
 */
ON_3dVector
cdt_mesh_t::vert_norm(long vi)
{
    ON_3dPoint *norm_pt = NULL;

    if (sv.find(vi) != sv.end()) {
	// Singularity vertex: prefer the pre-averaged normal from the CDT state.
	if (p_cdt) {
	    struct ON_Brep_CDT_State *s_cdt = (struct ON_Brep_CDT_State *)p_cdt;
	    auto it = s_cdt->singular_vert_to_norms->find(pnts[(size_t)vi]);
	    if (it != s_cdt->singular_vert_to_norms->end())
		norm_pt = it->second;
	}
    } else {
	auto nit = nmap.find(vi);
	if (nit != nmap.end())
	    norm_pt = normals[nit->second];
    }

    if (!norm_pt)
	return ON_3dVector(0.0, 0.0, 0.0);

    ON_3dVector v(*norm_pt);
    if (m_bRev)
	v = -v;
    return v;
}

bool
cdt_mesh_t::best_fit_plane_reproject(cpolygon_t *polygon)
{
    // We may have faces perpendicular to the original triangle face included,
    // so calculate a best fit plane and re-project the original points.  The
    // new plane should be close, but not exactly the same plane as the
    // starting plane.  It may happen that the reprojection invalidates the
    // inside/outside categorization of points - in that case, abandon the
    // re-fit and restore the original points.

    std::set<long> averts;
    int ncnt = 0;

    std::set<cpolyedge_t *>::iterator cp_it;
    for (cp_it = polygon->poly.begin(); cp_it != polygon->poly.end(); cp_it++) {
	cpolyedge_t *pe = *cp_it;
	averts.insert(polygon->p2o[pe->v2d[0]]);
	averts.insert(polygon->p2o[pe->v2d[1]]);
    }
    std::set<long>::iterator a_it;
    for (a_it = polygon->interior_points.begin(); a_it != polygon->interior_points.end(); a_it++) {
	averts.insert(polygon->p2o[*a_it]);
    }

    ON_3dVector avgtnorm(0.0,0.0,0.0);
    {
	// Deduplicate by 3D point pointer: singularity points can have multiple
	// UV-space indices all mapping to the same 3D location.  Counting each
	// UV copy once would distort the average toward that singularity.
	std::set<ON_3dPoint *> seen_pts;
	for (a_it = averts.begin(); a_it != averts.end(); a_it++) {
	    ON_3dPoint *p3d = pnts[(size_t)*a_it];
	    if (!seen_pts.insert(p3d).second)
		continue; // already counted this 3D point
	    ON_3dVector vn = vert_norm(*a_it);
	    if (vn.Length() > ON_ZERO_TOLERANCE) {
		avgtnorm += vn;
		ncnt++;
	    }
	}
    }
    if (ncnt > 0) {
	avgtnorm = avgtnorm * (1.0/(double)ncnt);
    } else {
	// No vertex normals available: fall back to the polygon's existing
	// plane normal as the orientation reference.
	avgtnorm = polygon->tplane.zaxis;
    }

    point_t pcenter;
    vect_t pnorm;
    {
	// Deduplicate by 3D pointer so repeated singularity UV points don't
	// skew bg_fit_plane toward the singularity location.
	// First pass: count unique 3D points.
	std::set<ON_3dPoint *> seen_fit;
	for (a_it = averts.begin(); a_it != averts.end(); a_it++)
	    seen_fit.insert(pnts[(size_t)*a_it]);
	// Second pass: fill the array.
	point_t *vpnts = (point_t *)bu_calloc(seen_fit.size() + 1, sizeof(point_t), "fitting points");
	int pnts_ind = 0;
	std::set<ON_3dPoint *> seen_fit2;
	for (a_it = averts.begin(); a_it != averts.end(); a_it++) {
	    ON_3dPoint *p = pnts[(size_t)*a_it];
	    if (!seen_fit2.insert(p).second)
		continue;
	    vpnts[pnts_ind][X] = p->x;
	    vpnts[pnts_ind][Y] = p->y;
	    vpnts[pnts_ind][Z] = p->z;
	    pnts_ind++;
	}
	bool fit_failed = bg_fit_plane(&pcenter, &pnorm, pnts_ind, vpnts);
	bu_free(vpnts, "fitting points");
	if (fit_failed)
	    return false;

	ON_3dVector on_norm(pnorm[X], pnorm[Y], pnorm[Z]);
	if (ON_DotProduct(on_norm, avgtnorm) < 0) {
	    VSCALE(pnorm, pnorm, -1);
	}
    }

    ON_3dPoint opc(pcenter);
    ON_3dPoint opn(pnorm);
    ON_Plane fit_plane(opc, opn);

    std::vector<std::pair<double, double> > pnts_2d_cached = polygon->pnts_2d;

    for (size_t i = 0; i < pnts.size(); i++) {
	double u, v;
	ON_3dPoint op3d = (*pnts[i]);
	fit_plane.ClosestPointTo(op3d, &u, &v);
	polygon->pnts_2d[i].first = u;
	polygon->pnts_2d[i].second = v;
    }

    // Make sure the new points still form a close polygon and all the interior points
    // are still interior points - if not, put them back
    int valid_reprojection = 1;
    if (!polygon->closed()) {
	valid_reprojection = 0;
    } else {
	std::set<long>::iterator u_it;
	for (u_it = polygon->interior_points.begin(); u_it != polygon->interior_points.end(); u_it++) {
	    if (!polygon->point_in_polygon(*u_it, false)) {
		valid_reprojection = 0;
		break;
	    }
	}
    }
    if (!valid_reprojection) {
	polygon->pnts_2d.clear();
	polygon->pnts_2d = pnts_2d_cached;
	return false;
    } else {
	polygon->fit_plane = fit_plane;
    }

    return true;
}

/* Mean-value parameterization reprojection for oriented_polycdt.
 *
 * Instead of projecting the polygon vertices onto a best-fit plane (which can
 * produce self-intersections for highly curved patches), we use GTE's
 * LSCMParameterization to map the boundary loop to a unit circle and solve a
 * mean-value Laplacian (Floater 2003) for interior vertices.  The mean-value
 * weights are always positive, so by Tutte's theorem the resulting UV map is
 * injective (fold-over-free) for any convex boundary, giving
 * bg_nested_poly_triangulate a valid domain to work with.
 *
 * Algorithm:
 *  1. Walk the polygon boundary loop (same ordering as cdt()) to get ordered
 *     2D (== 3D, since o2p is identity) vertex indices.
 *  2. Collect all vertices: boundary loop + all vertices in visited_triangles
 *     + any extra interior_points.
 *  3. Classify as boundary (on bnd_loop), true-interior (in visited_triangles
 *     but not on boundary), or extra-interior (in interior_points only).
 *  4. Run LSCMParameterization::Parameterize() when we have visited_triangles
 *     with interior vertices; fall back to MapBoundaryToCircle otherwise.
 *  5. Write the resulting UV coordinates back to polygon->pnts_2d.
 *  6. Validate: polygon must still be closed() and all interior_points must
 *     still pass point_in_polygon().  Restore pnts_2d on failure.
 *
 * Returns true on success.  On failure the caller should try best_fit_plane_reproject.
 */
bool
cdt_mesh_t::lscm_reproject(cpolygon_t *polygon)
{
    if (polygon->poly.size() < 3)
	return false;

    // ── Step 1: Walk the boundary loop ───────────────────────────────────────
    // Collect exactly poly.size() unique boundary vertices (one per edge,
    // using each edge's start vertex) so LSCMParameterization receives N
    // distinct vertices without a closing repeat.
    std::vector<int32_t> bnd_loop;
    {
	cpolyedge_t *pe  = *polygon->poly.begin();
	cpolyedge_t *cur = pe;
	do {
	    bnd_loop.push_back((int32_t)cur->v2d[0]);
	    cur = cur->next;
	    if (bnd_loop.size() > polygon->poly.size())
		return false; // defensive guard against broken loop linkage
	} while (cur != pe);
    }
    if ((int)bnd_loop.size() < 3)
	return false;

    // Build a lookup set for quick boundary membership tests.
    std::set<int32_t> bnd_set(bnd_loop.begin(), bnd_loop.end());

    // ── Step 2: Build compact vertex set ────────────────────────────────────
    // Order: boundary vertices first (in loop order), then true-interior
    // (appear in visited_triangles but not on boundary), then extra-interior
    // (in interior_points only, not in any visited triangle).
    std::map<int32_t, int32_t> g2c; // original mesh idx  →  compact idx
    std::vector<int32_t>       c2g; // compact idx        →  original mesh idx

    auto add_vert = [&](int32_t vi) {
	if (g2c.find(vi) == g2c.end()) {
	    g2c[vi] = (int32_t)c2g.size();
	    c2g.push_back(vi);
	}
    };

    // Boundary first (preserves bnd_loop order in compact array).
    for (int32_t vi : bnd_loop)
	add_vert(vi);

    // All vertices from visited_triangles.
    std::set<int32_t> tri_vert_set;
    for (auto const& t : polygon->visited_triangles) {
	for (int k = 0; k < 3; k++) {
	    tri_vert_set.insert((int32_t)t.v[k]);
	    add_vert((int32_t)t.v[k]);
	}
    }

    // Any remaining interior_points not yet added.
    for (long ip : polygon->interior_points)
	add_vert((int32_t)ip);

    int32_t ncompact = (int32_t)c2g.size();

    // ── Step 3: Classify interior vertices ──────────────────────────────────
    // true_interior: in visited_triangles, not on boundary → valid for Laplacian
    // extra_interior: in interior_points but not in any triangle
    std::vector<int32_t> true_interior_cpt;
    for (int32_t gi : tri_vert_set) {
	if (bnd_set.find(gi) == bnd_set.end())
	    true_interior_cpt.push_back(g2c.at(gi));
    }

    // If any interior_point is not a boundary vertex and not covered by a
    // visited_triangle (i.e. it is "extra-interior"), we cannot assign it a
    // meaningful LSCM UV coordinate.  Such vertices will be pinned to (0,0)
    // by the Parameterize / MapBoundaryToCircle path, which makes multiple
    // coincident Steiner points in the CDT and produces degenerate or
    // misoriented triangles.  Fall back to best_fit_plane_reproject instead.
    for (long ip : polygon->interior_points) {
	int32_t gi = (int32_t)ip;
	if (bnd_set.find(gi) == bnd_set.end() &&
	    tri_vert_set.find(gi) == tri_vert_set.end()) {
	    return false;
	}
    }

    // ── Step 4: Build per-compact 3D positions ───────────────────────────────
    std::vector<gte::Vector3<double>> v3d;
    v3d.reserve((size_t)ncompact);
    for (int32_t gi : c2g) {
	ON_3dPoint *op = pnts[(size_t)gi];
	gte::Vector3<double> p;
	p[0] = op->x; p[1] = op->y; p[2] = op->z;
	v3d.push_back(p);
    }

    // ── Step 5: Boundary loop in compact indices ─────────────────────────────
    std::vector<int32_t> bnd_cpt;
    bnd_cpt.reserve(bnd_loop.size());
    for (int32_t vi : bnd_loop)
	bnd_cpt.push_back(g2c.at(vi));

    // ── Step 6: Build visited_triangles in compact indices ───────────────────
    std::vector<std::array<int32_t, 3>> tris_cpt;
    tris_cpt.reserve(polygon->visited_triangles.size());
    for (auto const& t : polygon->visited_triangles) {
	std::array<int32_t, 3> ct;
	bool all_in = true;
	for (int k = 0; k < 3; k++) {
	    auto it = g2c.find((int32_t)t.v[k]);
	    if (it == g2c.end()) { all_in = false; break; }
	    ct[k] = it->second;
	}
	if (all_in)
	    tris_cpt.push_back(ct);
    }

    // ── Step 7: Compute ellipse semi-axes from boundary 3D bounding box ──────
    // The unit-circle domain works for isotropic patches, but for highly
    // elongated surfaces (e.g. a thin fillet wrapping around a large cylinder)
    // the Laplacian interior solution produces severe distortion: interior
    // vertices end up compressed against the boundary on the short sides.
    // Remedy: scale the final UV domain to an axis-aligned ellipse whose
    // aspect ratio matches the physical aspect ratio of the boundary loop.
    //
    // We measure the 3D bounding-box spans of the boundary vertices, sort
    // them, and derive semi-axes a >= b so that a/b approximates the ratio of
    // the two largest physical extents.  Since Tutte's theorem only requires
    // the boundary to be convex (an ellipse is convex), the fold-over
    // guarantee still holds after this uniform scaling.
    //
    // Caps: a is normalised to 1 (so coordinates stay order-of-unity);
    //       b is clamped to [0.2, 1] to avoid near-degenerate domains.
    //       The 5:1 maximum prevents extreme aspect ratios that would produce
    //       very thin CDT triangles with unreliable 3D orientations.
    double ellipse_a = 1.0;
    double ellipse_b = 1.0;
    {
	double xmin = 1e300, xmax = -1e300;
	double ymin = 1e300, ymax = -1e300;
	double zmin = 1e300, zmax = -1e300;
	for (int32_t vi : bnd_loop) {
	    ON_3dPoint *op = pnts[(size_t)vi];
	    if (op->x < xmin) xmin = op->x;
	    if (op->x > xmax) xmax = op->x;
	    if (op->y < ymin) ymin = op->y;
	    if (op->y > ymax) ymax = op->y;
	    if (op->z < zmin) zmin = op->z;
	    if (op->z > zmax) zmax = op->z;
	}
	double spans[3] = { xmax - xmin, ymax - ymin, zmax - zmin };
	// Sort descending.
	if (spans[0] < spans[1]) { double t = spans[0]; spans[0] = spans[1]; spans[1] = t; }
	if (spans[0] < spans[2]) { double t = spans[0]; spans[0] = spans[2]; spans[2] = t; }
	if (spans[1] < spans[2]) { double t = spans[1]; spans[1] = spans[2]; spans[2] = t; }
	// spans[0] >= spans[1] >= spans[2].
	if (spans[0] > ON_ZERO_TOLERANCE) {
	    // Use the two largest spans as the ellipse axes, with a=1, b=ratio.
	    double ratio = spans[1] / spans[0];
	    // Clamp b to [0.2, 1.0].  The lower bound avoids the extreme aspect
	    // ratios that would produce very thin CDT domains where triangles
	    // tend to have unreliable 3D orientations.  An upper bound of 1
	    // keeps the domain convex and order-of-unity.
	    if (ratio < 0.2) ratio = 0.2;
	    ellipse_a = 1.0;
	    ellipse_b = ratio;
	}
    }

    // ── Step 8: LSCM parameterization ────────────────────────────────────────
    std::vector<gte::Vector2<double>> uv;
    bool lscm_ok = false;

    if (!true_interior_cpt.empty() && !tris_cpt.empty()) {
	// Full LSCM: boundary pinned to circle, interior solved via mean-value
	// Laplacian using the visited_triangles as the mesh connectivity.
	lscm_ok = gte::LSCMParameterization<double>::Parameterize(
		v3d, bnd_cpt, true_interior_cpt, tris_cpt, uv);
    }

    if (!lscm_ok) {
	// Boundary-only fallback: map boundary to circle, leave interior at 0.
	// Interior Steiner points at UV=(0,0) land at the centroid of the
	// ellipse, which is geometrically inside the ellipse boundary.
	std::vector<gte::Vector2<double>> bnd_uv;
	lscm_ok = gte::LSCMParameterization<double>::MapBoundaryToCircle(
		v3d, bnd_cpt, bnd_uv);
	if (lscm_ok) {
	    uv.assign((size_t)ncompact, gte::Vector2<double>{ 0.0, 0.0 });
	    for (int i = 0; i < (int)bnd_cpt.size(); i++)
		uv[(size_t)bnd_cpt[i]] = bnd_uv[i];
	}
    }

    if (!lscm_ok || uv.empty()) {
	bu_log("lscm_reproject: f_id=%d mean-value parameterization failed (int_verts=%zu tris=%zu bnd=%zu)\n",
	    f_id, true_interior_cpt.size(), tris_cpt.size(), bnd_loop.size());
	return false;
    }

    // ── Step 9: Scale unit-circle UV to ellipse ───────────────────────────────
    // Both the Parameterize and MapBoundaryToCircle paths pin the boundary to
    // the unit circle.  Rescale the entire UV domain to the ellipse computed
    // above.  This is a uniform anisotropic scaling: u *= a, v *= b.
    if (ellipse_b < 0.999) {
	for (auto& p : uv) {
	    p[0] *= ellipse_a;
	    p[1] *= ellipse_b;
	}
    }

    // ── Step 10: Write UV back to polygon->pnts_2d ───────────────────────────
    // pnts_2d is indexed identically to pnts (o2p is identity), so we can
    // write directly using the original mesh index.
    std::vector<std::pair<double, double>> pnts_2d_cached = polygon->pnts_2d;

    for (int32_t ci = 0; ci < ncompact; ci++) {
	int32_t gi = c2g[(size_t)ci];
	if ((size_t)gi < polygon->pnts_2d.size()) {
	    polygon->pnts_2d[(size_t)gi].first  = uv[(size_t)ci][0];
	    polygon->pnts_2d[(size_t)gi].second = uv[(size_t)ci][1];
	}
    }

    // ── Step 11: Validate the reprojection ───────────────────────────────────
    // After LSCM + ellipse scaling, the boundary loop lies on the ellipse
    // (u/a)^2 + (v/b)^2 = 1.  Tutte's theorem (mean-value weights are always
    // positive) guarantees that all true interior vertices are strictly inside
    // the ellipse.  We validate with an ellipse-distance test rather than
    // Franklin's ray-cast point_in_polygon, because the ray-cast algorithm is
    // undefined (can return 0) for points exactly on or very near the polygon
    // boundary — which is exactly the situation here.
    int valid = 1;
    if (!polygon->closed()) {
	valid = 0;
    } else {
	// Tolerance: 1% headroom for CG-solver numerical imprecision.
	// A genuine LSCM failure (CG divergence) would place vertices far
	// outside the ellipse, not merely 1% beyond it.
	static const double kEllipseTolSq = 1.02 * 1.02;
	double inv_a2 = 1.0 / (ellipse_a * ellipse_a);
	double inv_b2 = 1.0 / (ellipse_b * ellipse_b);
	for (long ip : polygon->interior_points) {
	    int32_t gi = (int32_t)ip;
	    // Vertices that are also on the polygon boundary loop are placed
	    // exactly on the ellipse by the LSCM mapping; they are trivially
	    // "inside" the polygon boundary.
	    if (bnd_set.find(gi) != bnd_set.end())
		continue;
	    if ((size_t)gi >= polygon->pnts_2d.size()) {
		valid = 0;
		break;
	    }
	    double u = polygon->pnts_2d[(size_t)gi].first;
	    double v = polygon->pnts_2d[(size_t)gi].second;
	    if (u * u * inv_a2 + v * v * inv_b2 > kEllipseTolSq) {
		valid = 0;
		break;
	    }
	}
    }

    if (!valid) {
	bu_log("lscm_reproject: f_id=%d validation failed (closed=%d ellipse_b=%.3f)\n",
	    f_id, polygon->closed() ? 1 : 0, ellipse_b);
	polygon->pnts_2d = pnts_2d_cached;
	return false;
    }

    return true;
}


ON_Plane
cdt_mesh_t::best_fit_plane(std::set<triangle_t> &ts)
{
    std::set<long> averts;
    std::set<long>::iterator a_it;

    std::set<triangle_t>::iterator t_it;
    for (t_it = ts.begin(); t_it != ts.end(); t_it++) {
	for (int i = 0; i < 3; i++) {
	    averts.insert((*t_it).v[i]);
	}
    }

    ON_3dVector avgtnorm(0.0,0.0,0.0);
    int ncnt = 0;
    {
	std::set<ON_3dPoint *> seen_nrm;
	for (a_it = averts.begin(); a_it != averts.end(); a_it++) {
	    ON_3dPoint *p3d = pnts[(size_t)*a_it];
	    if (!seen_nrm.insert(p3d).second)
		continue;
	    ON_3dVector vn = vert_norm(*a_it);
	    if (vn.Length() > ON_ZERO_TOLERANCE) {
		avgtnorm += vn;
		ncnt++;
	    }
	}
    }
    if (ncnt > 0) {
	avgtnorm = avgtnorm * (1.0/(double)ncnt);
    }
    // If ncnt==0 avgtnorm stays zero; DotProduct below returns 0 and the
    // fitted-plane normal is kept as-is (arbitrary but consistent sign).

    point_t pcenter;
    vect_t pnorm;

    {
	std::set<ON_3dPoint *> seen_fit;
	for (a_it = averts.begin(); a_it != averts.end(); a_it++)
	    seen_fit.insert(pnts[(size_t)*a_it]);
	point_t *vpnts = (point_t *)bu_calloc(seen_fit.size() + 1, sizeof(point_t), "fitting points");
	int pnts_ind = 0;
	std::set<ON_3dPoint *> seen_fit2;
	for (a_it = averts.begin(); a_it != averts.end(); a_it++) {
	    ON_3dPoint *p = pnts[(size_t)*a_it];
	    if (!seen_fit2.insert(p).second)
		continue;
	    vpnts[pnts_ind][X] = p->x;
	    vpnts[pnts_ind][Y] = p->y;
	    vpnts[pnts_ind][Z] = p->z;
	    pnts_ind++;
	}
	bool fit_failed = bg_fit_plane(&pcenter, &pnorm, pnts_ind, vpnts);
	bu_free(vpnts, "fitting points");
	if (fit_failed) {
	    ON_Plane null_fit_plane(ON_3dPoint::UnsetPoint, ON_3dVector::UnsetVector);
	    return null_fit_plane;
	}
    }

    ON_3dVector on_norm(pnorm[X], pnorm[Y], pnorm[Z]);
    if (ON_DotProduct(on_norm, avgtnorm) < 0) {
	VSCALE(pnorm, pnorm, -1);
    }

    ON_3dPoint opc(pcenter);
    ON_3dPoint opn(pnorm);
    ON_Plane fit_plane(opc, opn);

    return fit_plane;
}

double
cdt_mesh_t::max_tri_angle(ON_Plane &plane, std::set<triangle_t> &ts)
{
    double dmax = 0;
    ON_3dVector pnorm = plane.Normal();
    std::set<triangle_t>::iterator t_it;
    for (t_it = ts.begin(); t_it != ts.end(); t_it++) {
	ON_3dVector tn = tnorm(*t_it);
	double d_ang = ang_deg(tn, pnorm);
	dmax = (d_ang > dmax) ? d_ang : dmax;
    }

    return dmax;
}

long
cdt_mesh_t::tri_process(cpolygon_t *polygon, std::set<uedge_t> *ne, std::set<uedge_t> *se, long *nv, triangle_t &t)
{
    std::set<cpolyedge_t *>::iterator pe_it;

    update_problem_edges();

    bool e_shared[3];
    struct edge_t e[3];
    struct uedge_t ue[3];
    for (int i = 0; i < 3; i++) {
	e_shared[i] = false;
    }
    e[0].set(t.v[0], t.v[1]);
    e[1].set(t.v[1], t.v[2]);
    e[2].set(t.v[2], t.v[0]);
    ue[0].set(t.v[0], t.v[1]);
    ue[1].set(t.v[1], t.v[2]);
    ue[2].set(t.v[2], t.v[0]);

    // Check the polygon edges against the triangle edges
    for (int i = 0; i < 3; i++) {
	for (pe_it = polygon->poly.begin(); pe_it != polygon->poly.end(); pe_it++) {
	    cpolyedge_t *pe = *pe_it;
	    struct uedge_t pue(polygon->p2o[pe->v2d[0]], polygon->p2o[pe->v2d[1]]);
	    if (ue[i] == pue) {
		e_shared[i] = true;
		break;
	    }
	}
    }

    // Count categories and file edges in the appropriate output sets
    long shared_cnt = 0;
    for (int i = 0; i < 3; i++) {
	if (e_shared[i]) {
	    shared_cnt++;
	    se->insert(ue[i]);
	} else {
	    ne->insert(ue[i]);
	}
    }

    if (shared_cnt == 0) {
	// If we don't have any shared edges any longer (we must have at the
	// start of processing or we wouldn't be here), we've probably got a
	// "bad" triangle that is already inside the loop due to another triangle
	// from the same shared edge expanding the loop.  Find the triangle
	// vertex that is the problem and mark it as an uncontained vertex.
	std::map<long, std::set<uedge_t>> v2ue;
	for (int i = 0; i < 3; i++) {
	    if (!brep_edge_pnt(ue[i].v[0])) {
		v2ue[ue[i].v[0]].insert(ue[i]);
	    }
	    if (!brep_edge_pnt(ue[i].v[1])) {
		v2ue[ue[i].v[1]].insert(ue[i]);
	    }
	}
	std::map<long, std::set<uedge_t>>::iterator v_it;
	for (v_it = v2ue.begin(); v_it != v2ue.end(); v_it++) {
	    int bad_edge_cnt = 0;
	    std::set<uedge_t>::iterator ue_it;
	    for (ue_it = v_it->second.begin(); ue_it != v_it->second.end(); ue_it++) {
		if (problem_edges.find(*ue_it) != problem_edges.end()) {
		    bad_edge_cnt++;
		}

		if (bad_edge_cnt > 1) {
		    polygon->uncontained.insert(v_it->first);
		    (*nv) = -1;
		    se->clear();
		    ne->clear();
		    return -2;
		}
	    }
	}
    }

    if (shared_cnt == 1) {
	// If we've got only one shared edge, there should be a vertex not currently
	// involved with the loop - verify that, and if it's true report it.
	long unshared_vert = polygon->unshared_vertex(t);

	if (unshared_vert != -1) {
	    (*nv) = unshared_vert;

	    // If the uninvolved point is associated with bad edges, we can't use
	    // any of this to build the loop - it gets added to the uncontained
	    // points set, and we move on.
	    int bad_edge_cnt = 0;
	    for (int i = 0; i < 3; i++) {
		if (ue[i].v[0] == unshared_vert || ue[i].v[1] == unshared_vert) {
		    if (problem_edges.find(ue[i]) != problem_edges.end()) {
			bad_edge_cnt++;
		    }

		    if (bad_edge_cnt > 1) {
			polygon->uncontained.insert(*nv);
			(*nv) = -1;
			se->clear();
			ne->clear();
			return -2;
		    }
		}
	    }
	} else {
	    // Self intersecting
	    (*nv) = -1;
	    se->clear();
	    ne->clear();
	    return -1;
	}
    }

    if (shared_cnt == 2) {
	// We've got one vert shared by both of the shared edges - it's probably
	// about to become an interior point
	std::map<long, int> vcnt;
	std::set<uedge_t>::iterator se_it;
	for (se_it = se->begin(); se_it != se->end(); se_it++) {
	    vcnt[(*se_it).v[0]]++;
	    vcnt[(*se_it).v[1]]++;
	}
	std::map<long, int>::iterator v_it;
	for (v_it = vcnt.begin(); v_it != vcnt.end(); v_it++) {
	    if (v_it->second == 2) {
		(*nv) = v_it->first;
		break;
	    }
	}
    }

    return 3 - shared_cnt;
}

void
cdt_mesh_t::polyplot_2d(cpolygon_t *polygon, FILE* plot_file)
{
    ON_2dPoint ppnt;
    point_t pmin, pmax;
    point_t bnp;
    VSET(pmin, DBL_MAX, DBL_MAX, DBL_MAX);
    VSET(pmax, -DBL_MAX, -DBL_MAX, -DBL_MAX);

    cpolyedge_t *efirst = *(polygon->poly.begin());
    cpolyedge_t *ecurr = NULL;

    ppnt.x = m_pnts_2d[polygon->p2o[efirst->v2d[0]]].first;
    ppnt.y = m_pnts_2d[polygon->p2o[efirst->v2d[0]]].second;
    VSET(bnp, ppnt.x, ppnt.y, 0);
    pdv_3move(plot_file, bnp);
    VMINMAX(pmin, pmax, bnp);
    ppnt.x = m_pnts_2d[polygon->p2o[efirst->v2d[1]]].first;
    ppnt.y = m_pnts_2d[polygon->p2o[efirst->v2d[1]]].second;
    VSET(bnp, ppnt.x, ppnt.y, 0);
    pdv_3cont(plot_file, bnp);
    VMINMAX(pmin, pmax, bnp);

    size_t ecnt = 1;
    while (ecurr != efirst && ecnt < polygon->poly.size()+1) {
	ecnt++;
	ecurr = (!ecurr) ? efirst->next : ecurr->next;
	ppnt.x = m_pnts_2d[polygon->p2o[ecurr->v2d[1]]].first;
	ppnt.y = m_pnts_2d[polygon->p2o[ecurr->v2d[1]]].second;
	VSET(bnp, ppnt.x, ppnt.y, 0);
	pdv_3cont(plot_file, bnp);
	VMINMAX(pmin, pmax, bnp);
	if (ecnt > polygon->poly.size()) {
	    break;
	}
    }
}

void cdt_mesh_t::polygon_plot_2d(cpolygon_t *polygon, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    polyplot_2d(polygon, plot_file);

    fclose(plot_file);
}


void cdt_mesh_t::cdt_inputs_plot(const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    pl_color(plot_file, 255, 0 ,0);

    ON_BrepFace &face = brep->m_F[f_id];
    ON_3dPoint min = ON_3dPoint::UnsetPoint;
    ON_3dPoint max = ON_3dPoint::UnsetPoint;
    for (int li = 0; li < face.LoopCount(); li++) {
	for (int ti = 0; ti < face.Loop(li)->TrimCount(); ti++) {
	    ON_BrepTrim *trim = face.Loop(li)->Trim(ti);
	    trim->GetBoundingBox(min, max, true);
	}
    }
    double dist = min.DistanceTo(max) * 0.01;


    polyplot_2d(&outer_loop, plot_file);

    pl_color(plot_file, 0, 0 ,255);

    std::map<int, cpolygon_t*>::iterator il_it;
    for (il_it = inner_loops.begin(); il_it != inner_loops.end(); il_it++) {
	cpolygon_t *il = il_it->second;
	polyplot_2d(il, plot_file);
    }

    if (m_interior_pnts.size()) {
	pl_color(plot_file, 0, 255, 0);
	std::set<long>::iterator p_it;
	for (p_it = m_interior_pnts.begin(); p_it != m_interior_pnts.end(); p_it++) {
	    double x = 	m_pnts_2d[*p_it].first;
	    double y = 	m_pnts_2d[*p_it].second;
	    ON_2dPoint p(x,y);
	    plot_pnt_2d(plot_file, &p, dist, 0);
	}
    }

    fclose(plot_file);
}

void
serialize_loop(cpolygon_t *loop, std::ofstream &sfile, const char *lname)
{
    size_t vcnt = 1;
    cpolyedge_t *pe = (*loop->poly.begin());
    cpolyedge_t *first = pe;
    cpolyedge_t *next = pe->next;

    sfile << lname << "[" << vcnt-1 << "] = " << loop->p2o[pe->v2d[0]] << ";\n";
    sfile << lname << "[" << vcnt << "] = " << loop->p2o[pe->v2d[1]] << ";\n";

    while (first != next) {
	vcnt++;
	sfile << lname << "[" << vcnt << "] = " << loop->p2o[next->v2d[1]] << ";\n";
	next = next->next;
	if (vcnt > loop->poly.size()) {
	    return;
	}
    }
}

void cdt_mesh_t::cdt_inputs_print(const char *filename)
{
    std::ofstream sfile(filename);

    if (!sfile.is_open()) {
	std::cerr << "Could not open file " << filename << " for writing\n";
	return;
    }

    /* ---------- includes ---------- */
    sfile << "#include <stdio.h>\n";
    sfile << "#include <math.h>\n";
    sfile << "#include \"bu/malloc.h\"\n";
    sfile << "#include \"bg/polygon.h\"\n\n";

    /* ---------- helper: 2D triangle area ---------- */
    sfile << "static double tri_area_2d(const point2d_t *pts, int a, int b, int c) {\n";
    sfile << "    double ax=pts[a][X], ay=pts[a][Y];\n";
    sfile << "    double bx=pts[b][X], by=pts[b][Y];\n";
    sfile << "    double cx=pts[c][X], cy=pts[c][Y];\n";
    sfile << "    return fabs((bx-ax)*(cy-ay)-(cx-ax)*(by-ay))*0.5;\n";
    sfile << "}\n\n";

    sfile << "int main(void) {\n";

    /* ---------- 2D UV points ---------- */
    sfile << "    /* 2D UV points (" << m_pnts_2d.size() << " total) */\n";
    sfile << "    point2d_t *pnts_2d = (point2d_t *)bu_calloc("
	  << m_pnts_2d.size()+1 << ", sizeof(point2d_t), \"pnts_2d\");\n";
    for (size_t i = 0; i < m_pnts_2d.size(); i++) {
	sfile << "    pnts_2d[" << i << "][X] = "
	      << std::fixed
	      << std::setprecision(std::numeric_limits<double>::max_digits10)
	      << m_pnts_2d[i].first << ";\n";
	sfile << "    pnts_2d[" << i << "][Y] = "
	      << std::fixed
	      << std::setprecision(std::numeric_limits<double>::max_digits10)
	      << m_pnts_2d[i].second << ";\n";
    }
    sfile << "\n";

    /* ---------- outer polygon ---------- */
    size_t opoly_n = outer_loop.poly.size() + 1;
    sfile << "    /* Outer loop polygon: " << outer_loop.poly.size()
	  << " edges, closed (first==last) */\n";
    sfile << "    int *opoly = (int *)bu_calloc(" << opoly_n
	  << ", sizeof(int), \"opoly\");\n";
    serialize_loop(&outer_loop, sfile, "    opoly");
    sfile << "\n";

    /* ---------- hole polygons ---------- */
    sfile << "    /* Hole polygons */\n";
    sfile << "    int **holes_array = NULL;\n";
    sfile << "    size_t *holes_npts = NULL;\n";
    sfile << "    int holes_cnt = " << inner_loops.size() << ";\n";
    if (inner_loops.size()) {
	sfile << "    holes_array = (int **)bu_calloc("
	      << inner_loops.size()+1 << ", sizeof(int *), \"holes_array\");\n";
	sfile << "    holes_npts = (size_t *)bu_calloc("
	      << inner_loops.size()+1 << ", sizeof(size_t), \"holes_npts\");\n";
	int loop_cnt = 0;
	std::map<int, cpolygon_t*>::iterator il_it;
	for (il_it = inner_loops.begin(); il_it != inner_loops.end(); il_it++) {
	    cpolygon_t *inl = il_it->second;
	    size_t hn = inl->poly.size() + 1;
	    sfile << "    holes_array[" << loop_cnt << "] = (int *)bu_calloc("
		  << hn << ", sizeof(int), \"hole_" << loop_cnt << "\");\n";
	    {
		struct bu_vls lname = BU_VLS_INIT_ZERO;
		bu_vls_sprintf(&lname, "    holes_array[%d]", loop_cnt);
		serialize_loop(inl, sfile, bu_vls_cstr(&lname));
		bu_vls_free(&lname);
	    }
	    sfile << "    holes_npts[" << loop_cnt << "] = " << hn << ";\n";
	    loop_cnt++;
	}
    }
    sfile << "\n";

    /* ---------- steiner points (all interior, filtering done below) ---------- */
    size_t raw_cnt = m_interior_pnts.size();
    sfile << "    /* Interior (Steiner) points: " << raw_cnt
	  << " total; filtering performed inline */\n";
    sfile << "    int raw_steiner_cnt = " << (int)raw_cnt << ";\n";
    if (raw_cnt) {
	sfile << "    int *raw_steiner = (int *)bu_calloc(raw_steiner_cnt, sizeof(int), \"raw_st\");\n";
	std::set<long>::iterator p_it;
	int vind = 0;
	for (p_it = m_interior_pnts.begin(); p_it != m_interior_pnts.end(); p_it++)
	    sfile << "    raw_steiner[" << vind++ << "] = " << *p_it << ";\n";
    } else {
	sfile << "    int *raw_steiner = NULL;\n";
    }
    sfile << "\n";

    /* ---------- steiner filtering (mirrors runtime logic) ---------- */
    sfile << "    /* Exclude Steiner points that fall inside a hole */\n";
    sfile << "    int *steiner = raw_steiner_cnt\n";
    sfile << "        ? (int *)bu_malloc(raw_steiner_cnt * sizeof(int), \"steiner\") : NULL;\n";
    sfile << "    size_t steiner_cnt = 0;\n";
    sfile << "    {\n";
    sfile << "        int si;\n";
    sfile << "        for (si = 0; si < raw_steiner_cnt; si++) {\n";
    sfile << "            int idx = raw_steiner[si];\n";
    sfile << "            int in_hole = 0;\n";
    sfile << "            int hi;\n";
    sfile << "            for (hi = 0; hi < holes_cnt && !in_hole; hi++) {\n";
    sfile << "                point2d_t *hpoly = (point2d_t *)bu_malloc(\n";
    sfile << "                    holes_npts[hi]*sizeof(point2d_t), \"hpoly\");\n";
    sfile << "                size_t hj;\n";
    sfile << "                for (hj = 0; hj < holes_npts[hi]; hj++) {\n";
    sfile << "                    hpoly[hj][X] = pnts_2d[holes_array[hi][hj]][X];\n";
    sfile << "                    hpoly[hj][Y] = pnts_2d[holes_array[hi][hj]][Y];\n";
    sfile << "                }\n";
    sfile << "                {\n";
    sfile << "                    point2d_t tp;\n";
    sfile << "                    tp[X] = pnts_2d[idx][X];\n";
    sfile << "                    tp[Y] = pnts_2d[idx][Y];\n";
    sfile << "                    in_hole = bg_pnt_in_polygon(holes_npts[hi],\n";
    sfile << "                        (const point2d_t *)(void *)hpoly,\n";
    sfile << "                        (const point2d_t *)(void *)&tp);\n";
    sfile << "                }\n";
    sfile << "                bu_free(hpoly, \"hpoly\");\n";
    sfile << "            }\n";
    sfile << "            if (!in_hole) steiner[steiner_cnt++] = idx;\n";
    sfile << "        }\n";
    sfile << "    }\n\n";

    /* ---------- triangulation ---------- */
    sfile << "    /* Triangulation */\n";
    sfile << "    int *faces = NULL;\n";
    sfile << "    int num_faces = 0;\n";
    sfile << "    int ret = bg_nested_poly_triangulate(&faces, &num_faces,\n";
    sfile << "        NULL, NULL, opoly, " << opoly_n << ",\n";
    sfile << "        holes_cnt ? (const int **)(void *)holes_array : NULL,\n";
    sfile << "        holes_cnt ? holes_npts  : NULL,\n";
    sfile << "        (size_t)holes_cnt,\n";
    sfile << "        steiner_cnt ? steiner : NULL, steiner_cnt,\n";
    sfile << "        (const point2d_t *)(void *)pnts_2d, " << m_pnts_2d.size()
	  << ", TRI_CONSTRAINED_DELAUNAY);\n";
    sfile << "    if (ret != 0) {\n";
    sfile << "        printf(\"FAIL: bg_nested_poly_triangulate returned %d\\n\", ret);\n";
    sfile << "        return 1;\n";
    sfile << "    }\n\n";

    /* ---------- triangle-in-hole check ---------- */
    sfile << "    /* Verify: no output triangle centroid falls inside any hole polygon */\n";
    sfile << "    int bad_tris = 0;\n";
    sfile << "    double bad_area = 0.0;\n";
    sfile << "    {\n";
    sfile << "        int t;\n";
    sfile << "        for (t = 0; t < num_faces; t++) {\n";
    sfile << "            int a = faces[3*t], b = faces[3*t+1], c = faces[3*t+2];\n";
    sfile << "            point2d_t cen;\n";
    sfile << "            int hi;\n";
    sfile << "            cen[X] = (pnts_2d[a][X]+pnts_2d[b][X]+pnts_2d[c][X])/3.0;\n";
    sfile << "            cen[Y] = (pnts_2d[a][Y]+pnts_2d[b][Y]+pnts_2d[c][Y])/3.0;\n";
    sfile << "            for (hi = 0; hi < holes_cnt; hi++) {\n";
    sfile << "                point2d_t *hpoly = (point2d_t *)bu_malloc(\n";
    sfile << "                    holes_npts[hi]*sizeof(point2d_t), \"hpoly\");\n";
    sfile << "                size_t hj;\n";
    sfile << "                for (hj = 0; hj < holes_npts[hi]; hj++) {\n";
    sfile << "                    hpoly[hj][X] = pnts_2d[holes_array[hi][hj]][X];\n";
    sfile << "                    hpoly[hj][Y] = pnts_2d[holes_array[hi][hj]][Y];\n";
    sfile << "                }\n";
    sfile << "                if (bg_pnt_in_polygon(holes_npts[hi],\n";
    sfile << "                        (const point2d_t *)(void *)hpoly,\n";
    sfile << "                        (const point2d_t *)(void *)cen)) {\n";
    sfile << "                    printf(\"  PROBLEM tri %d (%d,%d,%d) cen=(%.10g,%.10g) in hole %d\\n\",\n";
    sfile << "                        t, a, b, c, (double)cen[X], (double)cen[Y], hi);\n";
    sfile << "                    bad_tris++;\n";
    sfile << "                    bad_area += tri_area_2d((const point2d_t *)(void *)pnts_2d, a, b, c);\n";
    sfile << "                }\n";
    sfile << "                bu_free(hpoly, \"hpoly\");\n";
    sfile << "            }\n";
    sfile << "        }\n";
    sfile << "    }\n\n";

    sfile << "    if (bad_tris > 0) {\n";
    sfile << "        printf(\"FAIL: %d triangles intrude into holes (bad area=%.6g)\\n\",\n";
    sfile << "               bad_tris, bad_area);\n";
    sfile << "        return 1;\n";
    sfile << "    }\n";
    sfile << "    printf(\"PASS: %d output triangles, no hole intrusions\\n\", num_faces);\n";
    sfile << "    return 0;\n";
    sfile << "}\n";

    sfile.close();
}

void cdt_mesh_t::polygon_plot_3d(cpolygon_t *polygon, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    ON_3dPoint *ppnt;
    point_t pmin, pmax;
    point_t bnp;
    VSET(pmin, DBL_MAX, DBL_MAX, DBL_MAX);
    VSET(pmax, -DBL_MAX, -DBL_MAX, -DBL_MAX);

    cpolyedge_t *efirst = *(polygon->poly.begin());
    cpolyedge_t *ecurr = NULL;

    ppnt = pnts[p2d3d[polygon->p2o[efirst->v2d[0]]]];
    VSET(bnp, ppnt->x, ppnt->y, ppnt->z);
    pdv_3move(plot_file, bnp);
    VMINMAX(pmin, pmax, bnp);
    ppnt = pnts[p2d3d[polygon->p2o[efirst->v2d[1]]]];
    VSET(bnp, ppnt->x, ppnt->y, ppnt->z);
    pdv_3cont(plot_file, bnp);
    VMINMAX(pmin, pmax, bnp);

    size_t ecnt = 1;
    while (ecurr != efirst && ecnt < polygon->poly.size()+1) {
	ecnt++;
	ecurr = (!ecurr) ? efirst->next : ecurr->next;
	ppnt = pnts[p2d3d[polygon->p2o[ecurr->v2d[1]]]];
	VSET(bnp, ppnt->x, ppnt->y, ppnt->z);
	pdv_3cont(plot_file, bnp);
	VMINMAX(pmin, pmax, bnp);
	if (ecnt > polygon->poly.size()) {
	    break;
	}
    }

    fclose(plot_file);
}


void cdt_mesh_t::polygon_print_3d(cpolygon_t *polygon)
{
    size_t ecnt = 1;
    if (!polygon->poly.size()) return;
    cpolyedge_t *pe = (*polygon->poly.begin());
    cpolyedge_t *first = pe;
    cpolyedge_t *next = pe->next;

    std::set<cpolyedge_t *> visited;
    visited.insert(first);

    std::cout << first->v2d[0];

    // Walk the loop - an infinite loop is not closed
    while (first != next) {
	ecnt++;
	if (!next) {
	    break;
	}
	std::cout << "->" << next->v2d[0];
	visited.insert(next);
	next = next->next;
	if (ecnt > polygon->poly.size()) {
	    std::cout << "\nERROR infinite loop\n";
	    break;
	}
    }

    std::cout << "\n";

    visited.clear();

    pe = (*polygon->poly.begin());
    first = pe;
    next = pe->next;
    visited.insert(first);
    ON_3dPoint *p = pnts[p2d3d[polygon->p2o[first->v2d[0]]]];
    std::cout << "(" << p->x << "," << p->y << "," << p->z << ")" ;
    ecnt = 1;
    // Walk the loop - an infinite loop is not closed
    while (first != next) {
	ecnt++;
	if (!next) {
	    break;
	}
	std::cout << "->";
	p = pnts[p2d3d[polygon->p2o[first->v2d[0]]]];
	std::cout << "(" << p->x << "," << p->y << "," << p->z << ")" ;
	visited.insert(next);
	next = next->next;
	if (ecnt > polygon->poly.size()) {
	    std::cout << "\nERROR infinite loop\n";
	    break;
	}
    }
    std::cout << "\n";
}

// PImpl exposure of some mesh operations for use in tests
struct cdt_bmesh_impl {
    cdt_mesh_t fmesh;
};

int
cdt_bmesh_create(struct cdt_bmesh **m)
{
    if (!m) return -1;
    (*m) = new cdt_bmesh;
    (*m)->i = new cdt_bmesh_impl;
    return (!(*m)->i) ? -1 : 0;
}

void
cdt_bmesh_destroy(struct cdt_bmesh *m)
{
    if (!m) return;
    delete m->i;
    delete m;
}

int
cdt_bmesh_deserialize(const char *fname, struct cdt_bmesh *m)
{
    if (!fname || !m) return -1;
    if (!bu_file_exists(fname, NULL)) return -1;
    m->i->fmesh.deserialize(fname);
    return 0;
}

int
cdt_bmesh_repair(struct cdt_bmesh *m)
{
    if (!m) return -1;
    bool rsuccess = m->i->fmesh.repair();
    return (rsuccess) ? 0 : 1;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
