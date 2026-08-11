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
#include "./chart.h"
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
#include <memory>
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

static bool
cdt_failure_dumps_enabled()
{
    const char *setting = getenv("BRLCAD_CDT_DUMP_FAILURES");
    return setting && setting[0] && !BU_STR_EQUAL(setting, "0");
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

    const std::set<cpolyedge_t *> &start_edges = v2pe[e.v2d[0]];
    for (std::set<cpolyedge_t *>::const_iterator cp_it =
	    start_edges.begin(); cp_it != start_edges.end(); ++cp_it) {
	cpolyedge_t *pe = *cp_it;
	if (pe != nedge && pe->v2d[1] == nedge->v2d[0] &&
		(!prev || pe->v2d[0] < prev->v2d[0]))
	    prev = pe;
    }
    const std::set<cpolyedge_t *> &end_edges = v2pe[e.v2d[1]];
    for (std::set<cpolyedge_t *>::const_iterator cp_it =
	    end_edges.begin(); cp_it != end_edges.end(); ++cp_it) {
	cpolyedge_t *pe = *cp_it;
	if (pe != nedge && pe->v2d[0] == nedge->v2d[1] &&
		(!next || pe->v2d[1] < next->v2d[1]))
	    next = pe;
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

/* Return a stable boundary-loop starting edge.  poly is a pointer set, so
 * dereferencing begin() makes traversal order depend on allocator layout.
 * Geometry algorithms which walk next/prev from that arbitrary entry can then
 * assign samples or exercise bounded refinements in a different order. */
cpolyedge_t *
cpolygon_t::first_edge() const
{
    cpolyedge_t *first = NULL;
    for (std::set<cpolyedge_t *>::const_iterator edge = poly.begin();
	    edge != poly.end(); ++edge) {
	cpolyedge_t *candidate = *edge;
	if (!candidate)
	    continue;
	if (!first || candidate->v2d[0] < first->v2d[0] ||
		(candidate->v2d[0] == first->v2d[0] &&
		 candidate->v2d[1] < first->v2d[1]))
	    first = candidate;
    }
    return first;
}

void
cpolygon_t::remove_ordered_edge(const struct edge2d_t &e)
{
    cpolyedge_t *cull = NULL;

    const auto vertex_entry = v2pe.find(e.v2d[0]);
    if (vertex_entry == v2pe.end())
	return;
    for (std::set<cpolyedge_t *>::const_iterator cp_it =
	    vertex_entry->second.begin(); cp_it != vertex_entry->second.end();
	    ++cp_it) {
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

    if (cull->prev && cull->prev->next == cull)
	cull->prev->next = NULL;
    if (cull->next && cull->next->prev == cull)
	cull->next->prev = NULL;
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


static bool
collect_polygon_segment(size_t segment, void *context)
{
    std::vector<size_t> *segments = (std::vector<size_t> *)context;
    segments->push_back(segment);
    return true;
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

    // Check the projected segments against each other as well.
    std::vector<cpolyedge_t *> pv(poly.begin(), poly.end());
    RTree<size_t, double, 2> edge_index;
    for (size_t i = 0; i < pv.size(); i++) {
	cpolyedge_t *pe = pv[i];
	const std::pair<double, double> &p1 = pnts_2d[pe->v2d[0]];
	const std::pair<double, double> &p2 = pnts_2d[pe->v2d[1]];
	double minimum[2] = {
	    std::min(p1.first, p2.first),
	    std::min(p1.second, p2.second)
	};
	double maximum[2] = {
	    std::max(p1.first, p2.first),
	    std::max(p1.second, p2.second)
	};
	edge_index.Insert(minimum, maximum, i);
    }
    for (size_t i = 0; i < pv.size(); i++) {
	cpolyedge_t *pe1 = pv[i];
	ON_2dPoint p1_1(pnts_2d[pe1->v2d[0]].first, pnts_2d[pe1->v2d[0]].second);
	ON_2dPoint p1_2(pnts_2d[pe1->v2d[1]].first, pnts_2d[pe1->v2d[1]].second);
	ON_BoundingBox e1b(p1_1, p1_2);
	ON_Line e1(p1_1, p1_2);
	double minimum[2] = {
	    std::min(p1_1.x, p1_2.x),
	    std::min(p1_1.y, p1_2.y)
	};
	double maximum[2] = {
	    std::max(p1_1.x, p1_2.x),
	    std::max(p1_1.y, p1_2.y)
	};
	std::vector<size_t> candidates;
	edge_index.Search(minimum, maximum, collect_polygon_segment,
	    &candidates);
	std::sort(candidates.begin(), candidates.end());
	for (size_t j : candidates) {
	    if (j <= i) {
		continue;
	    }
	    cpolyedge_t *pe2 = pv[j];
	    ON_2dPoint p2_1(pnts_2d[pe2->v2d[0]].first, pnts_2d[pe2->v2d[0]].second);
	    ON_2dPoint p2_2(pnts_2d[pe2->v2d[1]].first, pnts_2d[pe2->v2d[1]].second);
	    ON_BoundingBox e2b(p2_1, p2_2);
	    ON_Line e2(p2_1, p2_2);
	    // The RTree is only a coarse filter.  Preserve the legacy bounding
	    // box and exact intersection predicates for candidate pairs.
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
cpolygon_t::rm_points_in_polygon(std::set<ON_2dPoint *> *pnts, bool flip,
	bool delete_removed)
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
	if (delete_removed)
	    delete *p_it;
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
    /* bg_nested_poly_triangulate may select a different valid diagonal when
     * the same closed boundary is cyclically rotated.  Downstream face-mesh
     * stitching requires that choice to be independent of allocator layout,
     * so use the same stable topological start as the LSCM projection. */
    cpolyedge_t *pe = first_edge();
    last_cdt_start_vertex = pe ? pe->v2d[0] : -1;
    if (!pe) {
	bu_free(opoly, "polygon points");
	bu_free(bgp_2d, "free libbg 2d points array)");
	return false;
    }
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

    if (!result && cdt_failure_dumps_enabled()) {
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
		ON_3dVector torig_dir = tnorm(orig);
		ON_3dVector tnew_dir = tnorm(tri);
		ON_3dVector bdir = bnorm(orig);
		bool f1 = (ON_DotProduct(torig_dir, bdir) < 0);
		bool f2 = (ON_DotProduct(tnew_dir, bdir) < 0);
		if (f1 && !f2) {
		    tri_remove(orig);
		} else {
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

int
cdt_test_local_defects(void)
{
    const ON_3dVector up(0.0, 0.0, 1.0);
    const ON_3dVector down(0.0, 0.0, -1.0);
    if (!NEAR_EQUAL(ang_deg(up, down), 180.0, ON_ZERO_TOLERANCE))
	return 1;

    ON_3dPoint p0(0.0, 0.0, 0.0);
    ON_3dPoint p1(1.0, 0.0, 0.0);
    ON_3dPoint p2(0.0, 1.0, 0.0);
    ON_3dPoint n0(up);
    ON_3dPoint n1(up);
    ON_3dPoint n2(up);
    cdt_mesh_t mesh;
    mesh.m_bRev = false;
    mesh.pnts.push_back(&p0);
    mesh.pnts.push_back(&p1);
    mesh.pnts.push_back(&p2);
    mesh.p2ind[&p0] = 0;
    mesh.p2ind[&p1] = 1;
    mesh.p2ind[&p2] = 2;
    mesh.normals.push_back(&n0);
    mesh.normals.push_back(&n1);
    mesh.normals.push_back(&n2);
    mesh.nmap[0] = 0;
    mesh.nmap[1] = 1;
    mesh.nmap[2] = 2;

    triangle_t reversed;
    reversed.v[0] = 0;
    reversed.v[1] = 2;
    reversed.v[2] = 1;
    if (!mesh.tri_add(reversed))
	return 2;
    triangle_t oriented;
    oriented.v[0] = 0;
    oriented.v[1] = 1;
    oriented.v[2] = 2;
    if (!mesh.tri_add(oriented))
	return 3;

    RTree<size_t, double, 3>::Iterator triangle;
    mesh.tris_tree.GetFirst(triangle);
    if (triangle.IsNull() || *triangle != 1)
	return 4;
    ++triangle;
    if (!triangle.IsNull())
	return 5;

    const ON_Cylinder cylinder(ON_Circle(ON_xy_plane, 2.0), 5.0);
    std::unique_ptr<ON_Brep> brep(ON_BrepCylinder(cylinder, true, true));
    if (!brep || !brep->IsValid())
	return 6;
    int side_index = -1;
    for (int face_index = 0; face_index < brep->m_F.Count(); ++face_index) {
	const ON_Surface *candidate = brep->m_F[face_index].SurfaceOf();
	if (candidate && candidate->IsClosed(0)) {
	    side_index = face_index;
	    break;
	}
    }
    if (side_index < 0)
	return 7;
    const ON_Surface *surface = brep->m_F[side_index].SurfaceOf();
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    const ON_2dPoint seam_uv[3] = {
	ON_2dPoint(udom.Min(), vdom.ParameterAt(0.30)),
	ON_2dPoint(udom.ParameterAt(0.01), vdom.ParameterAt(0.31)),
	ON_2dPoint(udom.Max(), vdom.ParameterAt(0.32))
    };
    ON_3dPoint seam_points[3] = {
	surface->PointAt(seam_uv[0].x, seam_uv[0].y),
	surface->PointAt(seam_uv[1].x, seam_uv[1].y),
	surface->PointAt(seam_uv[2].x, seam_uv[2].y)
    };
    const ON_2dPoint center(udom.ParameterAt(0.01 / 3.0),
	vdom.ParameterAt(0.31));
    ON_3dPoint center_point;
    ON_3dVector expected_normal;
    if (!surface_EvNormal(surface, center.x, center.y, center_point,
	    expected_normal) || !expected_normal.Unitize())
	return 8;
    ON_3dPoint wrong_normal(-expected_normal.x, -expected_normal.y,
	-expected_normal.z);
    cdt_mesh_t periodic_mesh;
    periodic_mesh.brep = brep.get();
    periodic_mesh.f_id = side_index;
    periodic_mesh.normals.push_back(&wrong_normal);
    for (int vertex = 0; vertex < 3; ++vertex) {
	periodic_mesh.pnts.push_back(&seam_points[vertex]);
	periodic_mesh.p2ind[&seam_points[vertex]] = vertex;
	periodic_mesh.p3d2d[vertex] = vertex;
	periodic_mesh.nmap[vertex] = 0;
	periodic_mesh.m_pnts_2d.push_back(std::make_pair(
	    seam_uv[vertex].x, seam_uv[vertex].y));
    }
    periodic_mesh.ambiguous_p3d2d.insert(0);
    periodic_mesh.ambiguous_p3d2d.insert(2);
    periodic_mesh.periodic_ambiguous_p3d2d.insert(0);
    periodic_mesh.periodic_ambiguous_p3d2d.insert(2);
    triangle_t seam_triangle;
    seam_triangle.v[0] = 0;
    seam_triangle.v[1] = 1;
    seam_triangle.v[2] = 2;
    const ON_3dVector recovered_normal = periodic_mesh.bnorm(
	seam_triangle);
    if (ON_DotProduct(recovered_normal, expected_normal) < 0.999)
	return 9;
    periodic_mesh.periodic_ambiguous_p3d2d.clear();
    const ON_3dVector conservative_normal = periodic_mesh.bnorm(
	seam_triangle);
    if (ON_DotProduct(conservative_normal, ON_3dVector(wrong_normal)) <
	    0.999)
	return 10;

    const auto release_polygon_edges = [](cpolygon_t &polygon) {
	for (cpolyedge_t *edge : polygon.poly)
	    delete edge;
	polygon.poly.clear();
    };

    cpolygon_t large_polygon;
    const int large_point_count = 2048;
    for (int point = 0; point < large_point_count; ++point) {
	const double angle = 2.0 * ON_PI * point / large_point_count;
	ON_2dPoint p(cos(angle), sin(angle));
	large_polygon.add_point(p, point);
	large_polygon.add_ordered_edge(edge2d_t(point,
		(point + 1) % large_point_count));
    }
    const bool large_self_intersection = large_polygon.self_intersecting();
    release_polygon_edges(large_polygon);
    if (large_self_intersection)
	return 11;
    return 0;
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
	if (tdir.Length() > 0 && bdir.Length() > 0 &&
		ON_DotProduct(tdir, bdir) < 0.1 &&
		!toleranced_boundary_triangle(tri)) {
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
	if (tdir.Length() > 0 && bdir.Length() > 0 &&
		ON_DotProduct(tdir, bdir) < 0.1 &&
		!toleranced_boundary_triangle(tri)) {
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
    bool ambiguous_native_image = false;
    bool certified_periodic_image = true;

    // Can't calculate this without some key Brep data
    if (!nmap.size() && !sv.size()) return avgnorm;

    /* Evaluate the original surface at the triangle's local UV centroid.
     * Boundary vertex normals may be averaged across sharp B-Rep edges and
     * are not a face-local orientation oracle.  Unwrap periodic coordinates
     * first: seam copies share one model vertex, but the third vertex selects
     * the local surface image on either side of the cut. */
    if (brep && f_id >= 0 && f_id < brep->m_F.Count()) {
	const ON_Surface *surface = brep->m_F[f_id].SurfaceOf();
	if (surface) {
	    ON_2dPoint uv[3];
	    bool mapped = true;
	    for (int corner = 0; corner < 3; ++corner) {
		const long vertex = t.v[corner];
		ambiguous_native_image = ambiguous_native_image ||
		    ambiguous_p3d2d.find(vertex) != ambiguous_p3d2d.end();
		if (ambiguous_p3d2d.find(vertex) != ambiguous_p3d2d.end() &&
			periodic_ambiguous_p3d2d.find(vertex) ==
			periodic_ambiguous_p3d2d.end())
		    certified_periodic_image = false;
		const auto native = p3d2d.find(vertex);
		if (native == p3d2d.end() || native->second < 0 ||
			(size_t)native->second >= m_pnts_2d.size()) {
		    mapped = false;
		    break;
		}
		uv[corner] = ON_2dPoint(
		    m_pnts_2d[(size_t)native->second].first,
		    m_pnts_2d[(size_t)native->second].second);
	    }
	    for (int direction = 0; mapped && direction < 2; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		const double period = surface->Domain(direction).Length();
		if (!(period > 0.0) || !std::isfinite(period)) {
		    mapped = false;
		    break;
		}
		const double reference = uv[0][direction];
		for (int corner = 1; corner < 3; ++corner) {
		    while (uv[corner][direction] - reference > 0.5 * period) {
			uv[corner][direction] -= period;
		    }
		    while (uv[corner][direction] - reference < -0.5 * period) {
			uv[corner][direction] += period;
		    }
		}
	    }
	    if (mapped && (!ambiguous_native_image ||
		    certified_periodic_image)) {
		ON_2dPoint center((uv[0].x + uv[1].x + uv[2].x) /
		    3.0, (uv[0].y + uv[1].y + uv[2].y) / 3.0);
		for (int direction = 0; direction < 2; ++direction) {
		    if (!surface->IsClosed(direction))
			continue;
		    const ON_Interval domain = surface->Domain(direction);
		    const double period = domain.Length();
		    center[direction] = domain.Min() + std::fmod(
			center[direction] - domain.Min(), period);
		    if (center[direction] < domain.Min())
			center[direction] += period;
		}
		ON_3dPoint point;
		ON_3dVector normal;
		if (surface_EvNormal(surface, center.x, center.y, point,
			normal) && normal.Unitize())
		    return normal;
	    }
	}
    }

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

size_t
cdt_mesh_t::geometric_degenerate_count()
{
    size_t count = 0;
    RTree<size_t, double, 3>::Iterator tree_it;
    tris_tree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	const triangle_t &triangle = tris_vect[*tree_it];
	const ON_3dPoint &a = *pnts[(size_t)triangle.v[0]];
	const ON_3dPoint &b = *pnts[(size_t)triangle.v[1]];
	const ON_3dPoint &c = *pnts[(size_t)triangle.v[2]];
	const ON_3dVector ab = b - a;
	const ON_3dVector ac = c - a;
	const ON_3dVector bc = c - b;
	const double longest_sq = std::max(ab.LengthSquared(),
	    std::max(ac.LengthSquared(), bc.LengthSquared()));
	const double doubled_area = ON_CrossProduct(ab, ac).Length();
	if (!(longest_sq > 0.0) || doubled_area <= 64.0 *
		std::numeric_limits<double>::epsilon() * longest_sq)
	    count++;
	++tree_it;
    }
    return count;
}

size_t
cdt_mesh_t::self_intersections(std::vector<triangle_t> *problematic,
	size_t max_pairs)
{
    if (problematic)
	problematic->clear();
    if (!max_pairs)
	return 0;

    std::vector<size_t> active;
    RTree<size_t, double, 3>::Iterator tree_it;
    tris_tree.GetFirst(tree_it);
    while (!tree_it.IsNull()) {
	active.push_back(*tree_it);
	++tree_it;
    }
    std::sort(active.begin(), active.end());

    size_t intersections = 0;
    std::set<size_t> problem_ids;
    RTree<size_t, double, 3> prior_triangles;
    for (size_t triangle_id : active) {
	if (triangle_id >= tris_vect.size())
	    continue;
	const triangle_t &first = tris_vect[triangle_id];
	double minimum[3] = {
	    std::numeric_limits<double>::infinity(),
	    std::numeric_limits<double>::infinity(),
	    std::numeric_limits<double>::infinity()
	};
	double maximum[3] = {
	    -std::numeric_limits<double>::infinity(),
	    -std::numeric_limits<double>::infinity(),
	    -std::numeric_limits<double>::infinity()
	};
	point_t first_points[3];
	for (int corner = 0; corner < 3; ++corner) {
	    const ON_3dPoint &point = *pnts[(size_t)first.v[corner]];
	    VSET(first_points[corner], point.x, point.y, point.z);
	    for (int axis = 0; axis < 3; ++axis) {
		minimum[axis] = std::min(minimum[axis],
		    (double)first_points[corner][axis]);
		maximum[axis] = std::max(maximum[axis],
		    (double)first_points[corner][axis]);
	    }
	}
	std::vector<size_t> candidates;
	prior_triangles.Search(minimum, maximum,
	    [](size_t data, void *context) {
		std::vector<size_t> *found =
		    (std::vector<size_t> *)context;
		found->push_back(data);
		return true;
	    }, &candidates);
	std::sort(candidates.begin(), candidates.end());
	for (size_t candidate_id : candidates) {
	    if (candidate_id >= tris_vect.size())
		continue;
	    const triangle_t &second = tris_vect[candidate_id];
	    bool adjacent = false;
	    for (int i = 0; i < 3 && !adjacent; ++i) {
		for (int j = 0; j < 3; ++j) {
		    if (first.v[i] == second.v[j]) {
			adjacent = true;
			break;
		    }
		}
	    }
	    if (adjacent)
		continue;
	    point_t second_points[3];
	    for (int corner = 0; corner < 3; ++corner) {
		const ON_3dPoint &point = *pnts[(size_t)second.v[corner]];
		VSET(second_points[corner], point.x, point.y, point.z);
	    }
	    int intersects = cdt_tri_tri_intersection(first_points,
		second_points);
	    if (!intersects)
		continue;
	    intersections++;
	    problem_ids.insert(triangle_id);
	    problem_ids.insert(candidate_id);
	    if (intersections >= max_pairs)
		break;
	}
	if (intersections >= max_pairs)
	    break;
	prior_triangles.Insert(minimum, maximum, triangle_id);
    }
    if (problematic) {
	for (size_t triangle_id : problem_ids)
	    problematic->push_back(tris_vect[triangle_id]);
    }
    return intersections;
}

size_t
cdt_mesh_t::incorrect_normal_count()
{
    return interior_incorrect_normals().size();
}

bool
cdt_mesh_t::repair_incorrect_normal_edges()
{
    if (m_face_charts.empty() || self_intersections(NULL, 1))
	return false;
    boundary_edges_update();
    std::vector<triangle_t> folded = interior_incorrect_normals();
    if (folded.empty())
	return true;

    const std::vector<triangle_t> saved_triangles = tris_vect;
    const std::vector<triangle_t> saved_triangles_2d = tris_2d;
    const decltype(v2edges) saved_v2edges = v2edges;
    const decltype(v2tris) saved_v2tris = v2tris;
    const decltype(edges2tris) saved_edges2tris = edges2tris;
    const decltype(uedges2tris) saved_uedges2tris = uedges2tris;
    const decltype(boundary_edges) saved_boundary_edges = boundary_edges;
    const decltype(problem_edges) saved_problem_edges = problem_edges;
    std::vector<size_t> saved_active;
    RTree<size_t, double, 3>::Iterator saved_it;
    tris_tree.GetFirst(saved_it);
    while (!saved_it.IsNull()) {
	saved_active.push_back(*saved_it);
	++saved_it;
    }
    const auto restore = [&]() {
	tris_vect = saved_triangles;
	tris_2d = saved_triangles_2d;
	v2edges = saved_v2edges;
	v2tris = saved_v2tris;
	edges2tris = saved_edges2tris;
	uedges2tris = saved_uedges2tris;
	boundary_edges = saved_boundary_edges;
	problem_edges = saved_problem_edges;
	tris_tree.RemoveAll();
	for (size_t triangle_index : saved_active) {
	    if (triangle_index >= tris_vect.size())
		continue;
	    triangle_t &triangle = tris_vect[triangle_index];
	    triangle.m = this;
	    ON_BoundingBox bounds(*pnts[(size_t)triangle.v[0]],
		*pnts[(size_t)triangle.v[0]]);
	    for (int corner = 1; corner < 3; ++corner)
		bounds.Set(*pnts[(size_t)triangle.v[corner]], true);
	    const double minimum[3] = {bounds.Min().x, bounds.Min().y,
		bounds.Min().z};
	    const double maximum[3] = {bounds.Max().x, bounds.Max().y,
		bounds.Max().z};
	    tris_tree.Insert(minimum, maximum, triangle_index);
	}
	boundary_edges_stale = true;
	bounding_box_stale = true;
    };
    const auto native_triangle = [&](const triangle_t &triangle,
	    long native[3]) {
	for (int corner = 0; corner < 3; ++corner) {
	    const auto mapped = p3d2d.find(triangle.v[corner]);
	    if (mapped == p3d2d.end() || mapped->second < 0 ||
		    ambiguous_p3d2d.find(triangle.v[corner]) !=
		    ambiguous_p3d2d.end())
		return false;
	    native[corner] = mapped->second;
	}
	return true;
    };
    const auto chart_orientation = [&](const long native[3]) {
	for (const cdt_face_chart &chart : m_face_charts) {
	    const int orientation = chart.triangle_orientation(native);
	    if (orientation)
		return orientation;
	}
	return 0;
    };
    const auto acceptable_triangle = [&](triangle_t &triangle) {
	const ON_3dVector triangle_normal = tnorm(triangle);
	const ON_3dVector surface_normal = bnorm(triangle);
	return triangle_normal.Length() > 0.0 &&
	    surface_normal.Length() > 0.0 &&
	    ON_DotProduct(triangle_normal, surface_normal) >= 0.1;
    };
    const auto erase_native_triangle = [&](const long native[3]) {
	long wanted[3] = {native[0], native[1], native[2]};
	std::sort(wanted, wanted + 3);
	for (auto triangle = tris_2d.begin(); triangle != tris_2d.end();
		triangle++) {
	    long candidate[3] = {
		triangle->v[0], triangle->v[1], triangle->v[2]
	    };
	    std::sort(candidate, candidate + 3);
	    if (std::equal(candidate, candidate + 3, wanted)) {
		tris_2d.erase(triangle);
		return;
	    }
	}
    };

    const size_t flip_limit = 4 * folded.size() + 32;
    size_t flip_count = 0;
    while (!folded.empty() && flip_count < flip_limit) {
	bool changed = false;
	for (const triangle_t &folded_triangle : folded) {
	    if (!tri_active(folded_triangle.ind))
		continue;
	    triangle_t first = tris_vect[folded_triangle.ind];
	    std::vector<std::pair<double, int>> candidates;
	    for (int edge = 0; edge < 3; ++edge) {
		const ON_3dPoint &a = *pnts[(size_t)first.v[edge]];
		const ON_3dPoint &b = *pnts[(size_t)first.v[(edge + 1) % 3]];
		candidates.push_back(std::make_pair(
		    a.DistanceTo(b), edge));
	    }
	    std::sort(candidates.begin(), candidates.end(),
		[](const auto &a, const auto &b) { return a.first > b.first; });
	    for (const std::pair<double, int> &candidate : candidates) {
		const int edge = candidate.second;
		const long a = first.v[edge];
		const long b = first.v[(edge + 1) % 3];
		const long c = first.v[(edge + 2) % 3];
		const uedge_t shared(a, b);
		if (boundary_edges.find(shared) != boundary_edges.end())
		    continue;
		const auto incident = uedges2tris.find(shared);
		if (incident == uedges2tris.end() ||
			incident->second.size() != 2)
		    continue;
		size_t neighbor_index = *incident->second.begin();
		if (neighbor_index == first.ind)
		    neighbor_index = *incident->second.rbegin();
		if (neighbor_index == first.ind || !tri_active(neighbor_index))
		    continue;
		triangle_t second = tris_vect[neighbor_index];
		long d = -1;
		for (int corner = 0; corner < 3; ++corner) {
		    if (second.v[corner] != a && second.v[corner] != b) {
			d = second.v[corner];
			break;
		    }
		}
		if (d < 0 || d == c ||
			uedges2tris.find(uedge_t(c, d)) != uedges2tris.end())
		    continue;
		long first_native[3];
		long second_native[3];
		if (!native_triangle(first, first_native) ||
			!native_triangle(second, second_native))
		    continue;
		const int orientation = chart_orientation(first_native);
		if (!orientation || chart_orientation(second_native) !=
			orientation)
		    continue;
		triangle_t replacement_first;
		replacement_first.v[0] = c;
		replacement_first.v[1] = a;
		replacement_first.v[2] = d;
		triangle_t replacement_second;
		replacement_second.v[0] = c;
		replacement_second.v[1] = d;
		replacement_second.v[2] = b;
		long replacement_first_native[3];
		long replacement_second_native[3];
		if (!native_triangle(replacement_first,
			replacement_first_native) ||
			!native_triangle(replacement_second,
			replacement_second_native) ||
			chart_orientation(replacement_first_native) !=
			orientation ||
			chart_orientation(replacement_second_native) !=
			orientation ||
			!acceptable_triangle(replacement_first) ||
			!acceptable_triangle(replacement_second))
		    continue;

		tri_remove(first);
		tri_remove(second);
		tri_add(replacement_first);
		tri_add(replacement_second);
		erase_native_triangle(first_native);
		erase_native_triangle(second_native);
		triangle_t native_first;
		triangle_t native_second;
		for (int corner = 0; corner < 3; ++corner) {
		    native_first.v[corner] = replacement_first_native[corner];
		    native_second.v[corner] = replacement_second_native[corner];
		}
		tris_2d.push_back(native_first);
		tris_2d.push_back(native_second);
		flip_count++;
		changed = true;
		break;
	    }
	    if (changed)
		break;
	}
	if (!changed)
	    break;
	folded = interior_incorrect_normals();
    }
    boundary_edges_update();
    if (flip_count && problem_edges.empty() &&
	    !self_intersections(NULL, 1)) {
	if (folded.empty() && valid(0))
	    return true;
	/* The remaining folds can still be refined in the chart.  Retain
	 * successful flips: tris_2d was updated with their native identities,
	 * so subsequent point insertion remains structurally consistent. */
	return false;
    }
    restore();
    return false;
}

bool
cdt_mesh_t::toleranced_boundary_triangle(const triangle_t &triangle)
{
    if (!brep || f_id < 0 || f_id >= brep->m_F.Count())
	return false;
    const bool topology_chart =
	cdt_face_uses_topology_chart(brep->m_F[f_id]);
    bool incident_to_singularity = false;
    bool incident_to_boundary = false;
    bool all_boundary = true;
    for (int corner = 0; corner < 3; ++corner) {
	incident_to_singularity = incident_to_singularity ||
	    sv.find(triangle.v[corner]) != sv.end();
	incident_to_boundary = incident_to_boundary ||
	    ep.find(triangle.v[corner]) != ep.end();
	all_boundary = all_boundary &&
	    ep.find(triangle.v[corner]) != ep.end();
    }
    if (!incident_to_singularity &&
	    (topology_chart ? !all_boundary : !incident_to_boundary))
	return false;

    const ON_Surface *surface = brep->m_F[f_id].SurfaceOf();
    struct ON_Brep_CDT_State *state =
	(struct ON_Brep_CDT_State *)p_cdt;
    if (!surface || !state)
	return false;
    bool used_model_tolerance = false;
    for (int corner = 0; corner < 3; ++corner) {
	const long vertex = triangle.v[corner];
	if (sv.find(vertex) != sv.end())
	    continue;
	if (vertex < 0 || (size_t)vertex >= pnts.size())
	    return false;
	const ON_3dPoint *point = pnts[(size_t)vertex];
	double minimum_distance = DBL_MAX;
	for (const auto &mapping : p2d3d) {
	    if (mapping.second < 0 || (size_t)mapping.second >= pnts.size() ||
		pnts[(size_t)mapping.second] != point || mapping.first < 0 ||
		(size_t)mapping.first >= m_pnts_2d.size())
		continue;
	    const std::pair<double, double> &uv =
		m_pnts_2d[(size_t)mapping.first];
	    const ON_3dPoint surface_point = surface->PointAt(uv.first,
		uv.second);
	    if (surface_point.IsValid())
		minimum_distance = std::min(minimum_distance,
		    surface_point.DistanceTo(*point));
	}
	if (!std::isfinite(minimum_distance))
	    return false;
	const double coordinate_scale = std::max(1.0, std::max(
	    std::max(std::fabs(point->x), std::fabs(point->y)),
	    std::fabs(point->z)));
	double allowed = 1024.0 *
	    std::numeric_limits<double>::epsilon() * coordinate_scale;
	const double numerical_allowed = allowed;
	if (state->pnt_audit_info) {
	    const auto audit = state->pnt_audit_info->find(
		const_cast<ON_3dPoint *>(point));
	    if (audit != state->pnt_audit_info->end()) {
		const int edge_index = audit->second.edge_index;
		if (edge_index >= 0 && edge_index < brep->m_E.Count()) {
		    const double edge_tolerance =
			brep->m_E[edge_index].m_tolerance;
		    if (std::isfinite(edge_tolerance) &&
			    edge_tolerance > 0.0 &&
			    !NEAR_EQUAL(edge_tolerance, ON_UNSET_VALUE,
			    ON_ZERO_TOLERANCE))
			allowed = std::max(allowed, edge_tolerance);
		}
		const int vertex_index = audit->second.vert_index;
		if (vertex_index >= 0 && vertex_index < brep->m_V.Count()) {
		    const ON_BrepVertex &brep_vertex =
			brep->m_V[vertex_index];
		    const double vertex_tolerance = brep_vertex.m_tolerance;
		    if (std::isfinite(vertex_tolerance) &&
			    vertex_tolerance > 0.0 &&
			    !NEAR_EQUAL(vertex_tolerance, ON_UNSET_VALUE,
			    ON_ZERO_TOLERANCE))
			allowed = std::max(allowed, vertex_tolerance);
		    /* A shared vertex may be recorded with any one incident
		     * edge in the point audit.  Honor every incident edge's
		     * declared tolerance rather than depending on that arbitrary
		     * representative. */
		    for (int edge_offset = 0;
			    edge_offset < brep_vertex.m_ei.Count();
			    ++edge_offset) {
			const int incident_edge =
			    brep_vertex.m_ei[edge_offset];
			if (incident_edge < 0 ||
				incident_edge >= brep->m_E.Count())
			    continue;
			const double edge_tolerance =
			    brep->m_E[incident_edge].m_tolerance;
			if (std::isfinite(edge_tolerance) &&
				edge_tolerance > 0.0 &&
				!NEAR_EQUAL(edge_tolerance, ON_UNSET_VALUE,
				ON_ZERO_TOLERANCE))
			    allowed = std::max(allowed, edge_tolerance);
		    }
		}
	    }
	}
	if (minimum_distance > allowed)
	    return false;
	used_model_tolerance = used_model_tolerance ||
	    minimum_distance > numerical_allowed;
    }
    return topology_chart || used_model_tolerance;
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
    cpolyedge_t *pe = loop->first_edge();
    if (!pe) {
	bu_free(opoly, "free libbg 2d points array)");
	return NULL;
    }
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

/* Remove cyclic boundary constraints explicitly identified as belonging to
 * one accepted sub-tolerance edge component.  The component labels carry
 * the native constraint provenance; welded 3-D pointer equality by itself
 * must never collapse periodic seam copies. */
static void
simplify_subtolerance_ring(std::vector<int> &ring,
	const std::vector<const ON_3dPoint *> &points_3d,
	const std::vector<cdt_topo_vertex_id> &topology_vertices,
	const std::vector<long> &constraint_components)
{
    const auto collapsed_pair = [&](int first, int second) {
	if (first < 0 || second < 0 ||
		(size_t)first >= points_3d.size() ||
		(size_t)second >= points_3d.size() ||
		(size_t)first >= constraint_components.size() ||
		(size_t)second >= constraint_components.size() ||
		constraint_components[(size_t)first] < 0 ||
		constraint_components[(size_t)first] !=
		constraint_components[(size_t)second])
	    return false;
	const ON_3dPoint *first_point = points_3d[(size_t)first];
	const ON_3dPoint *second_point = points_3d[(size_t)second];
	return first_point && first_point == second_point;
    };
    const auto preferred_point = [&](int first, int second) {
	const cdt_topo_vertex_id first_topology = first >= 0 &&
		(size_t)first < topology_vertices.size() ?
	    topology_vertices[(size_t)first] : CDT_TOPOLOGY_ID_NONE;
	const cdt_topo_vertex_id second_topology = second >= 0 &&
		(size_t)second < topology_vertices.size() ?
	    topology_vertices[(size_t)second] : CDT_TOPOLOGY_ID_NONE;
	if (first_topology != CDT_TOPOLOGY_ID_NONE &&
		second_topology == CDT_TOPOLOGY_ID_NONE)
	    return first;
	if (second_topology != CDT_TOPOLOGY_ID_NONE &&
		first_topology == CDT_TOPOLOGY_ID_NONE)
	    return second;
	if (first_topology != second_topology)
	    return first_topology < second_topology ? first : second;
	return std::min(first, second);
    };

    if (ring.size() > 1 && ring.front() == ring.back())
	ring.pop_back();
    std::vector<int> simplified;
    simplified.reserve(ring.size());
    for (int point : ring) {
	if (!simplified.empty() && collapsed_pair(simplified.back(), point)) {
	    simplified.back() = preferred_point(simplified.back(), point);
	    continue;
	}
	simplified.push_back(point);
    }
    while (simplified.size() > 1 &&
	    collapsed_pair(simplified.back(), simplified.front())) {
	const int preferred = preferred_point(simplified.back(),
	    simplified.front());
	simplified.front() = preferred;
	simplified.pop_back();
    }
    ring.swap(simplified);
}

int
cdt_test_subtolerance_ring(void)
{
    ON_3dPoint welded(0.0, 0.0, 0.0);
    ON_3dPoint second(1.0, 0.0, 0.0);
    ON_3dPoint third(0.0, 1.0, 0.0);
    std::vector<const ON_3dPoint *> points = {
	&welded, &welded, &second, &third, &welded
    };
    std::vector<cdt_topo_vertex_id> topology = {
	146, 49, 2, 3, 49
    };
    std::vector<long> components = {7, 7, -1, -1, -1};

    std::vector<int> forward = {0, 1, 2, 3, 0};
    simplify_subtolerance_ring(forward, points, topology, components);
    const std::vector<int> expected_forward = {1, 2, 3};
    if (forward != expected_forward)
	return 1;

    /* Reversing the ring must retain the same lower topology endpoint. */
    std::vector<int> reverse = {0, 3, 2, 1, 0};
    simplify_subtolerance_ring(reverse, points, topology, components);
    const std::vector<int> expected_reverse = {1, 3, 2};
    if (reverse != expected_reverse)
	return 2;

    /* A pointer-equal periodic copy without accepted-edge constraint
     * provenance is not part of the collapse component. */
    std::vector<int> seam_adjacent = {0, 4, 2, 3, 0};
    simplify_subtolerance_ring(seam_adjacent, points, topology,
	components);
    const std::vector<int> expected_seam = {0, 4, 2, 3};
    if (seam_adjacent != expected_seam)
	return 3;

    /* Prefer a topology endpoint over an intermediate sample. */
    topology[0] = CDT_TOPOLOGY_ID_NONE;
    components[4] = 7;
    std::vector<int> sampled = {0, 4, 2, 3, 0};
    simplify_subtolerance_ring(sampled, points, topology, components);
    const std::vector<int> expected_sampled = {4, 2, 3};
    if (sampled != expected_sampled)
	return 4;

    return 0;
}

static bool
point_on_segment(const point2d_t *points, int point_index, int first,
	int second, double tolerance_sq)
{
    if (!points || point_index < 0 || first < 0 || second < 0)
	return false;
    const double px = points[point_index][X];
    const double py = points[point_index][Y];
    const double ax = points[first][X];
    const double ay = points[first][Y];
    const double dx = points[second][X] - ax;
    const double dy = points[second][Y] - ay;
    const double length_sq = dx * dx + dy * dy;
    double parameter = length_sq > DBL_EPSILON ?
	((px - ax) * dx + (py - ay) * dy) / length_sq : 0.0;
    parameter = std::max(0.0, std::min(1.0, parameter));
    const double ex = px - (ax + parameter * dx);
    const double ey = py - (ay + parameter * dy);
    return ex * ex + ey * ey <= tolerance_sq;
}

static bool
point_on_polygon_boundary(const point2d_t *points, int point_index,
	const int *polygon, size_t polygon_point_count, double tolerance_sq)
{
    if (!points || point_index < 0 || !polygon || polygon_point_count < 2)
	return false;
    for (size_t edge = 0; edge + 1 < polygon_point_count; ++edge) {
	const int first = polygon[edge];
	const int second = polygon[edge + 1];
	if (point_on_segment(points, point_index, first, second,
		tolerance_sq))
	    return true;
    }
    return false;
}

struct chart_boundary_segment {
    int first;
    int second;
};

static bool
collect_boundary_segment(size_t segment, void *context)
{
    std::vector<size_t> *segments = (std::vector<size_t> *)context;
    segments->push_back(segment);
    return true;
}

static void
index_polygon_boundary(RTree<size_t, double, 2> &index,
	std::vector<chart_boundary_segment> &segments,
	const point2d_t *points, const int *polygon,
	size_t polygon_point_count, double tolerance)
{
    for (size_t edge = 0; edge + 1 < polygon_point_count; ++edge) {
	const int first = polygon[edge];
	const int second = polygon[edge + 1];
	double minimum[2] = {
	    std::min(points[first][X], points[second][X]) - tolerance,
	    std::min(points[first][Y], points[second][Y]) - tolerance
	};
	double maximum[2] = {
	    std::max(points[first][X], points[second][X]) + tolerance,
	    std::max(points[first][Y], points[second][Y]) + tolerance
	};
	const size_t segment = segments.size();
	segments.push_back({first, second});
	index.Insert(minimum, maximum, segment);
    }
}

static bool
point_on_indexed_boundary(RTree<size_t, double, 2> &index,
	const std::vector<chart_boundary_segment> &segments,
	const point2d_t *points, int point_index, double tolerance_sq)
{
    double query[2] = {
	points[point_index][X], points[point_index][Y]
    };
    std::vector<size_t> candidates;
    index.Search(query, query, collect_boundary_segment, &candidates);
    for (size_t candidate : candidates) {
	if (candidate >= segments.size())
	    continue;
	const chart_boundary_segment &segment = segments[candidate];
	if (point_on_segment(points, point_index, segment.first,
		segment.second, tolerance_sq))
	    return true;
    }
    return false;
}

static bool
cleanable_developable_chart(const ON_BrepFace &face,
	const cdt_face_chart &chart)
{
    if (chart.closed_direction() >= 0 || !face.SurfaceOf())
	return false;
    return (chart.type() == CDT_FACE_CHART_SURFACE_METRIC &&
	face.SurfaceOf()->IsPlanar(NULL, BN_TOL_DIST)) ||
	chart.type() == CDT_FACE_CHART_CYLINDER;
}

/* Clipper can normalize weakly-simple planar loop topology, but conversion
 * quality may not invent, remove, or move a shared B-Rep boundary sample.
 * Accept its result only when every cleaned point maps back to one original
 * chart coordinate and the triangle boundary is exactly the original set of
 * nonzero constraints.  Duplicate coordinates are mergeable only when they
 * identify the same explicit B-Rep topology vertex and 3-D point. */
static bool
topology_preserving_clean_triangulation(int **faces, int *face_count,
	const cdt_face_chart &chart,
	const std::vector<const ON_3dPoint *> &source_points_3d,
	const int *outer, size_t outer_count, const int **holes,
	const size_t *hole_counts, size_t hole_count, const int *steiner,
	size_t steiner_count, const point2d_t *points)
{
    if (!faces || !face_count || !outer || outer_count < 4 || !points)
	return false;
    *faces = NULL;
    *face_count = 0;

    std::set<int> active;
    for (size_t i = 0; i < outer_count; ++i)
	active.insert(outer[i]);
    for (size_t hole = 0; hole < hole_count; ++hole) {
	for (size_t i = 0; i < hole_counts[hole]; ++i)
	    active.insert(holes[hole][i]);
    }
    for (size_t i = 0; i < steiner_count; ++i)
	active.insert(steiner[i]);
    double minimum[2] = {DBL_MAX, DBL_MAX};
    double maximum[2] = {-DBL_MAX, -DBL_MAX};
    for (int point : active) {
	if (point < 0 || (size_t)point >= chart.points.size() ||
		!std::isfinite(points[point][X]) ||
		!std::isfinite(points[point][Y]))
	    return false;
	minimum[X] = std::min(minimum[X], points[point][X]);
	minimum[Y] = std::min(minimum[Y], points[point][Y]);
	maximum[X] = std::max(maximum[X], points[point][X]);
	maximum[Y] = std::max(maximum[Y], points[point][Y]);
    }
    const double span = std::max(maximum[X] - minimum[X],
	maximum[Y] - minimum[Y]);
    if (!(span > SMALL_FASTF) || !std::isfinite(span))
	return false;
    const double origin[2] = {
	0.5 * (minimum[X] + maximum[X]),
	0.5 * (minimum[Y] + maximum[Y])
    };
    const double scale = (double)CLIPPER_MAX / span;
    typedef std::pair<int64_t, int64_t> snapped_point;
    const auto snap = [&](double x, double y) {
	return snapped_point(
	    (int64_t)std::llround((x - origin[X]) * scale),
	    (int64_t)std::llround((y - origin[Y]) * scale));
    };

    std::map<snapped_point, std::vector<int>> input_points;
    for (int point : active)
	input_points[snap(points[point][X], points[point][Y])].push_back(
	    point);
    std::map<int, const cdt_chart_vertex *> chart_vertices;
    for (const cdt_chart_vertex &vertex : chart.vertices) {
	if (vertex.id >= 0 && (size_t)vertex.id < chart.points.size())
	    chart_vertices[(int)vertex.id] = &vertex;
    }
    std::map<snapped_point, int> representative;
    for (const auto &entry : input_points) {
	int selected = -1;
	cdt_topo_vertex_id topology = CDT_TOPOLOGY_ID_NONE;
	const ON_3dPoint *point_3d = NULL;
	for (int point : entry.second) {
	    const auto vertex_entry = chart_vertices.find(point);
	    if (vertex_entry == chart_vertices.end())
		return false;
	    const cdt_chart_vertex &vertex = *vertex_entry->second;
	    const ON_3dPoint *candidate_3d = vertex.native_point >= 0 &&
		    (size_t)vertex.native_point < source_points_3d.size() ?
		source_points_3d[(size_t)vertex.native_point] : NULL;
	    if (selected < 0) {
		selected = point;
		topology = vertex.topo_vertex;
		point_3d = candidate_3d;
		continue;
	    }
	    if (entry.second.size() > 1 &&
		    (topology == CDT_TOPOLOGY_ID_NONE ||
		    vertex.topo_vertex != topology || !point_3d ||
		    candidate_3d != point_3d))
		return false;
	    if (point < selected)
		selected = point;
	}
	representative[entry.first] = selected;
    }

    int *clean_faces = NULL;
    int clean_face_count = 0;
    point2d_t *clean_points = NULL;
    int clean_point_count = 0;
    const int clean_status = bg_nested_poly_triangulate_clean(&clean_faces,
	&clean_face_count, &clean_points, &clean_point_count, outer,
	outer_count, holes, hole_counts, hole_count, steiner, steiner_count,
	points, chart.points.size());
    if (clean_status != BRLCAD_OK || !clean_faces || !clean_points ||
	    clean_face_count <= 0 || clean_point_count <= 0) {
	if (clean_faces)
	    bu_free(clean_faces, "topology-preserving clean faces");
	if (clean_points)
	    bu_free(clean_points, "topology-preserving clean points");
	return false;
    }

    bool valid = true;
    std::vector<int> clean_to_chart((size_t)clean_point_count, -1);
    std::set<snapped_point> output_points;
    for (int i = 0; i < clean_point_count; ++i) {
	const snapped_point key = snap(clean_points[i][X], clean_points[i][Y]);
	const auto source = representative.find(key);
	if (source == representative.end()) {
	    valid = false;
	    break;
	}
	clean_to_chart[(size_t)i] = source->second;
	output_points.insert(key);
    }
    for (const auto &entry : input_points) {
	if (output_points.find(entry.first) == output_points.end())
	    valid = false;
    }

    typedef std::pair<int, int> clean_edge;
    const auto edge_key = [](int first, int second) {
	return first < second ? clean_edge(first, second) :
	    clean_edge(second, first);
    };
    std::set<clean_edge> required_boundary;
    const auto require_ring = [&](const int *ring, size_t count) {
	for (size_t i = 0; i + 1 < count; ++i) {
	    const int first = representative[snap(points[ring[i]][X],
		points[ring[i]][Y])];
	    const int second = representative[snap(points[ring[i + 1]][X],
		points[ring[i + 1]][Y])];
	    if (first != second &&
		    !required_boundary.insert(edge_key(first, second)).second)
		return false;
	}
	return true;
    };
    valid = valid && require_ring(outer, outer_count);
    for (size_t hole = 0; valid && hole < hole_count; ++hole)
	valid = require_ring(holes[hole], hole_counts[hole]);

    std::map<clean_edge, int> edge_uses;
    std::vector<int> mapped_faces((size_t)clean_face_count * 3, -1);
    for (size_t i = 0; valid && i < mapped_faces.size(); i += 3) {
	for (int corner = 0; corner < 3; ++corner) {
	    const int clean_point = clean_faces[i + (size_t)corner];
	    if (clean_point < 0 || clean_point >= clean_point_count ||
		    clean_to_chart[(size_t)clean_point] < 0) {
		valid = false;
		break;
	    }
	    mapped_faces[i + (size_t)corner] =
		clean_to_chart[(size_t)clean_point];
	}
	if (!valid || mapped_faces[i] == mapped_faces[i + 1] ||
		mapped_faces[i + 1] == mapped_faces[i + 2] ||
		mapped_faces[i + 2] == mapped_faces[i]) {
	    valid = false;
	    break;
	}
	edge_uses[edge_key(mapped_faces[i], mapped_faces[i + 1])]++;
	edge_uses[edge_key(mapped_faces[i + 1], mapped_faces[i + 2])]++;
	edge_uses[edge_key(mapped_faces[i + 2], mapped_faces[i])]++;
    }
    for (const auto &edge : edge_uses) {
	const bool boundary = required_boundary.find(edge.first) !=
	    required_boundary.end();
	if ((boundary && edge.second != 1) ||
		(!boundary && edge.second != 2))
	    valid = false;
    }
    for (const clean_edge &edge : required_boundary) {
	if (edge_uses[edge] != 1)
	    valid = false;
    }

    bu_free(clean_faces, "topology-preserving clean faces");
    bu_free(clean_points, "topology-preserving clean points");
    if (!valid)
	return false;
    *faces = (int *)bu_calloc(mapped_faces.size(), sizeof(int),
	"topology-preserving chart faces");
    std::copy(mapped_faces.begin(), mapped_faces.end(), *faces);
    *face_count = clean_face_count;
    return true;
}

int
cdt_test_developable_clean(void)
{
    const ON_Cylinder cylinder(ON_Circle(ON_xy_plane, 3.0), 6.0);
    ON_NurbsSurface *surface = new ON_NurbsSurface;
    if (!cylinder.IsValid() || 2 != cylinder.GetNurbForm(*surface)) {
	delete surface;
	return 1;
    }
    const ON_Interval angular = surface->Domain(0);
    if (!surface->Trim(0, ON_Interval(angular.ParameterAt(0.1),
	    angular.ParameterAt(0.9)))) {
	delete surface;
	return 2;
    }

    ON_Brep brep;
    ON_BrepFace &face = brep.NewFace(brep.AddSurface(surface));
    const ON_Interval udom = surface->Domain(0);
    const ON_Interval vdom = surface->Domain(1);
    const double middle_u = udom.Mid();
    const double middle_v = vdom.Mid();
    const ON_2dPoint route[9] = {
	ON_2dPoint(middle_u, middle_v),
	ON_2dPoint(udom.ParameterAt(0.15), middle_v),
	ON_2dPoint(udom.ParameterAt(0.15), vdom.ParameterAt(0.15)),
	ON_2dPoint(middle_u, vdom.ParameterAt(0.15)),
	ON_2dPoint(middle_u, middle_v),
	ON_2dPoint(udom.ParameterAt(0.85), middle_v),
	ON_2dPoint(udom.ParameterAt(0.85), vdom.ParameterAt(0.85)),
	ON_2dPoint(middle_u, vdom.ParameterAt(0.85)),
	ON_2dPoint(middle_u, middle_v)
    };
    std::vector<std::pair<double, double>> native_points;
    std::vector<int> outer;
    std::vector<ON_3dPoint> point_storage(9);
    std::vector<const ON_3dPoint *> points_3d;
    std::vector<cdt_topo_vertex_id> topology_vertices;
    ON_3dPoint shared = surface->PointAt(middle_u, middle_v);
    for (int i = 0; i < 9; ++i) {
	outer.push_back(i);
	native_points.push_back(std::make_pair(route[i].x, route[i].y));
	point_storage[(size_t)i] = surface->PointAt(route[i].x, route[i].y);
	const bool shared_point = i == 0 || i == 4 || i == 8;
	points_3d.push_back(shared_point ? &shared :
	    &point_storage[(size_t)i]);
	topology_vertices.push_back(shared_point ? 12 : 20 + i);
    }

    cdt_face_chart chart;
    if (!chart.build(face, native_points, outer,
	    std::vector<std::vector<int>>(), std::vector<int>(),
	    std::vector<int>(), points_3d, topology_vertices))
	return 3;
    if (chart.type() != CDT_FACE_CHART_CYLINDER ||
	    chart.closed_direction() >= 0 ||
	    !cleanable_developable_chart(face, chart))
	return 4;

    std::vector<point2d_t> points(chart.points.size());
    for (size_t i = 0; i < chart.points.size(); ++i)
	V2SET(points[i], chart.points[i].first, chart.points[i].second);
    std::vector<int> outline(chart.outer.begin(), chart.outer.end());
    outline.push_back(chart.outer.front());
    int *faces = NULL;
    int face_count = 0;
    const bool cleaned = topology_preserving_clean_triangulation(&faces,
	&face_count, chart, points_3d, outline.data(), outline.size(), NULL,
	NULL, 0, NULL, 0, points.data());
    if (faces)
	bu_free(faces, "developable clean test faces");
    return cleaned && face_count > 0 ? 0 : 5;
}

/* Triangulate one disk in a face atlas.  The strict libbg entry point owns
 * duplicate and boundary-Steiner filtering; this wrapper retains the chart's
 * native identities.  Boundary state is committed only after every atlas
 * component has succeeded. */
static bool
triangulate_chart_component(cdt_mesh_t *mesh, const ON_BrepFace &face,
	const cdt_face_chart &chart, std::vector<triangle_t> &triangles)
{
    if (!mesh)
	return false;
    const auto diagnose = [&](int result, int stage, const char *message) {
	struct ON_Brep_CDT_State *state =
	    (struct ON_Brep_CDT_State *)mesh->p_cdt;
	if (state)
	    cdt_diagnostic_set(state, result, stage, mesh->f_id, 0, 1,
		message);
    };
    if (chart.outer.size() < 3 || chart.points.size() < 3) {
	diagnose(BREP_CDT_RESULT_CHART_FAILED,
	    BREP_CDT_STAGE_CHART_CONSTRUCTION,
	    "face atlas component has no disk boundary");
	return false;
    }
    std::vector<point2d_t> points(chart.points.size());
    for (size_t i = 0; i < chart.points.size(); ++i)
	V2SET(points[i], chart.points[i].first, chart.points[i].second);
    std::vector<int> outline(chart.outer.begin(), chart.outer.end());
    outline.push_back(chart.outer.front());
    std::vector<std::vector<int>> holes(chart.holes);
    std::vector<const int *> hole_arrays;
    std::vector<size_t> hole_counts;
    for (std::vector<int> &hole : holes) {
	hole.push_back(hole.front());
	hole_arrays.push_back(hole.data());
	hole_counts.push_back(hole.size());
    }
    std::vector<int> constraints;
    constraints.reserve(chart.constraints.size() * 2);
    for (const std::pair<int, int> &constraint : chart.constraints) {
	constraints.push_back(constraint.first);
	constraints.push_back(constraint.second);
    }
    const std::vector<int> no_steiner;
    const std::vector<int> &steiner = face.SurfaceOf()->IsPlanar(NULL,
	ON_ZERO_TOLERANCE) ? no_steiner : chart.steiner;
    int *faces = NULL;
    int face_count = 0;
    struct bg_triangulation_report report = {0, -1, {0}};
    const int status = bg_nested_poly_triangulate_constraints_strict(&faces,
	&face_count, NULL, NULL, outline.data(), outline.size(),
	hole_arrays.empty() ? NULL : hole_arrays.data(),
	hole_counts.empty() ? NULL : hole_counts.data(), hole_arrays.size(),
	steiner.empty() ? NULL : steiner.data(), steiner.size(),
	constraints.empty() ? NULL : constraints.data(),
	chart.constraints.size(), points.data(), points.size(), &report);
    if (status != BRLCAD_OK) {
	bu_log("Face %d: atlas component triangulation failed: %s\n",
	    mesh->f_id, report.message);
	struct ON_Brep_CDT_State *state =
	    (struct ON_Brep_CDT_State *)mesh->p_cdt;
	if (state) {
	    int result = BREP_CDT_RESULT_INVALID_PSLG;
	    int stage = BREP_CDT_STAGE_PSLG_VALIDATION;
	    if (report.reason == BG_TRIANGULATION_DETRIA_FAILED) {
		result = BREP_CDT_RESULT_DETRIA_FAILED;
		stage = BREP_CDT_STAGE_DETRIA;
	    } else if (report.reason ==
		    BG_TRIANGULATION_POSTCONDITION_FAILED) {
		result = BREP_CDT_RESULT_CERTIFICATION_FAILED;
		stage = BREP_CDT_STAGE_DETRIA;
	    }
	    cdt_diagnostic_set(state, result, stage, mesh->f_id, 0, 1,
		report.message);
	}
	bu_free(faces, "atlas component faces");
	return false;
    }
    const size_t original_count = triangles.size();
    for (int i = 0; i < face_count; ++i) {
	triangle_t triangle;
	for (int corner = 0; corner < 3; ++corner)
	    triangle.v[corner] = chart.native_point(faces[3 * i + corner]);
	if (triangle.v[0] < 0 || triangle.v[1] < 0 ||
		triangle.v[2] < 0) {
	    triangles.resize(original_count);
	    bu_free(faces, "atlas component faces");
	    diagnose(BREP_CDT_RESULT_CERTIFICATION_FAILED,
		BREP_CDT_STAGE_DETRIA,
		"face atlas output lost a native boundary identity");
	    return false;
	}
	triangles.push_back(triangle);
    }
    bu_free(faces, "atlas component faces");
    if (face_count > 0)
	return true;
    diagnose(BREP_CDT_RESULT_CERTIFICATION_FAILED, BREP_CDT_STAGE_DETRIA,
	"face atlas component produced no chart triangles");
    return false;
}

static bool
install_face_chart_atlas(cdt_mesh_t *mesh, const ON_BrepFace &face,
	std::vector<cdt_face_chart> &atlas, std::string &failure)
{
    if (!mesh || atlas.empty()) {
	failure = "face atlas has no chart components";
	return false;
    }
    std::set<long> atlas_edge_points;
    std::set<long> atlas_singular_points;
    std::set<uedge_t> atlas_boundary_edges;
    const auto stage_boundary = [&](const std::vector<int> &ring,
	    const cdt_face_chart &component) {
	if (ring.size() < 2)
	    return false;
	for (size_t i = 0; i < ring.size(); ++i) {
	    const long first_native = component.native_point(ring[i]);
	    const long second_native = component.native_point(
		ring[(i + 1) % ring.size()]);
	    const auto first_3d = mesh->p2d3d.find(first_native);
	    const auto second_3d = mesh->p2d3d.find(second_native);
	    if (first_3d == mesh->p2d3d.end() ||
		    second_3d == mesh->p2d3d.end() || first_3d->second < 0 ||
		    second_3d->second < 0 ||
		    (size_t)first_3d->second >= mesh->pnts.size() ||
		    (size_t)second_3d->second >= mesh->pnts.size())
		return false;
	    const auto first_mesh = mesh->p2ind.find(
		mesh->pnts[(size_t)first_3d->second]);
	    const auto second_mesh = mesh->p2ind.find(
		mesh->pnts[(size_t)second_3d->second]);
	    if (first_mesh == mesh->p2ind.end() ||
		    second_mesh == mesh->p2ind.end())
		return false;
	    atlas_edge_points.insert(first_mesh->second);
	    atlas_edge_points.insert(second_mesh->second);
	    if (first_mesh->second != second_mesh->second)
		atlas_boundary_edges.insert(uedge_t(first_mesh->second,
		    second_mesh->second));
	}
	return true;
    };
    for (const cdt_face_chart &component : atlas) {
	bool boundary_mapped = stage_boundary(component.outer, component);
	for (const std::vector<int> &hole : component.holes)
	    boundary_mapped = stage_boundary(hole, component) &&
		boundary_mapped;
	for (const cdt_chart_vertex &vertex : component.vertices) {
	    if (!vertex.singular)
		continue;
	    const auto point_3d = mesh->p2d3d.find(vertex.native_point);
	    if (vertex.native_point < 0 ||
		    point_3d == mesh->p2d3d.end() || point_3d->second < 0 ||
		    (size_t)point_3d->second >= mesh->pnts.size()) {
		boundary_mapped = false;
		continue;
	    }
	    const auto mesh_point = mesh->p2ind.find(
		mesh->pnts[(size_t)point_3d->second]);
	    if (mesh_point == mesh->p2ind.end()) {
		boundary_mapped = false;
		continue;
	    }
	    atlas_singular_points.insert(mesh_point->second);
	}
	if (!boundary_mapped) {
	    failure = "face atlas boundary did not map to mesh vertices";
	    return false;
	}
    }

    std::vector<triangle_t> atlas_triangles;
    for (const cdt_face_chart &component : atlas) {
	if (!triangulate_chart_component(mesh, face, component,
		atlas_triangles)) {
	    failure.clear();
	    return false;
	}
    }
    if (atlas_triangles.empty()) {
	failure = "face atlas produced no chart triangles";
	return false;
    }

    std::vector<triangle_t> mapped_triangles;
    size_t forward_count = 0;
    size_t reverse_count = 0;
    for (const triangle_t &triangle_2d : atlas_triangles) {
	triangle_t triangle_3d;
	for (int corner = 0; corner < 3; ++corner) {
	    const auto point_3d = mesh->p2d3d.find(
		triangle_2d.v[corner]);
	    if (point_3d == mesh->p2d3d.end() || point_3d->second < 0 ||
		    (size_t)point_3d->second >= mesh->pnts.size()) {
		failure = "face atlas triangle did not map to mesh vertices";
		return false;
	    }
	    const auto mesh_point = mesh->p2ind.find(
		mesh->pnts[(size_t)point_3d->second]);
	    if (mesh_point == mesh->p2ind.end()) {
		failure = "face atlas triangle did not map to mesh vertices";
		return false;
	    }
	    triangle_3d.v[corner] = mesh_point->second;
	}
	const ON_3dVector triangle_normal = mesh->tnorm(triangle_3d);
	const ON_3dVector surface_normal = mesh->bnorm(triangle_3d);
	if (triangle_normal.Length() > 0 && surface_normal.Length() > 0) {
	    if (ON_DotProduct(triangle_normal, surface_normal) > 0.0)
		forward_count++;
	    else
		reverse_count++;
	}
	mapped_triangles.push_back(triangle_3d);
    }

    const bool reverse_atlas = reverse_count > forward_count;
    mesh->reset();
    for (triangle_t &triangle : mapped_triangles) {
	if (reverse_atlas)
	    std::swap(triangle.v[1], triangle.v[2]);
	mesh->tri_add(triangle);
    }
    if (mesh->tris_vect.empty()) {
	failure = "face atlas produced no 3-D triangles";
	return false;
    }
    for (const uedge_t &edge : mesh->chart_boundary_edges)
	mesh->brep_edges.erase(edge);
    mesh->chart_boundary_edges = atlas_boundary_edges;
    mesh->brep_edges.insert(atlas_boundary_edges.begin(),
	atlas_boundary_edges.end());
    mesh->ep.insert(atlas_edge_points.begin(), atlas_edge_points.end());
    mesh->sv.insert(atlas_singular_points.begin(),
	atlas_singular_points.end());
    mesh->tris_2d.swap(atlas_triangles);
    mesh->m_face_charts.swap(atlas);
    return true;
}

bool
cdt_mesh_t::cdt()
{
    m_face_charts.clear();
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

    int *native_outer = loop_to_bgpoly(&outer_loop);
    if (!native_outer)
	return false;
    const size_t native_outer_count = outer_loop.poly.size() + 1;
    std::vector<int> source_outer(native_outer,
	native_outer + native_outer_count);
    bu_free(native_outer, "native chart outline");

    std::vector<std::vector<int>> source_holes;
    for (il_it = inner_loops.begin(); il_it != inner_loops.end(); ++il_it) {
	int *native_hole = loop_to_bgpoly(il_it->second);
	if (!native_hole)
	    return false;
	const size_t count = il_it->second->poly.size() + 1;
	source_holes.push_back(std::vector<int>(native_hole,
	    native_hole + count));
	bu_free(native_hole, "native chart hole");
    }
    std::vector<int> source_steiner;
    source_steiner.reserve(m_interior_pnts.size());
    for (long point : m_interior_pnts) {
	if (point < 0 || (size_t)point >= m_pnts_2d.size())
	    continue;
	source_steiner.push_back((int)point);
    }
    std::vector<int> source_refinement;
    source_refinement.reserve(m_chart_refinement_pnts.size());
    for (long point : m_chart_refinement_pnts) {
	if (point >= 0 && (size_t)point < m_pnts_2d.size())
	    source_refinement.push_back((int)point);
    }
    std::vector<const ON_3dPoint *> source_points_3d(m_pnts_2d.size(),
	NULL);
    for (size_t i = 0; i < source_points_3d.size(); ++i) {
	const auto mapped = p2d3d.find((long)i);
	if (mapped != p2d3d.end() && mapped->second >= 0 &&
		(size_t)mapped->second < pnts.size())
	    source_points_3d[i] = pnts[(size_t)mapped->second];
    }
    std::vector<cdt_topo_vertex_id> source_topology_vertices(
	m_pnts_2d.size(), CDT_TOPOLOGY_ID_NONE);
    bool source_topology_conflict = false;
    const auto assign_topology_vertex = [&](long source_point,
	    int vertex_id) {
	if (source_point < 0 || (size_t)source_point >=
		source_topology_vertices.size() || vertex_id < 0)
	    return;
	cdt_topo_vertex_id &current =
	    source_topology_vertices[(size_t)source_point];
	if (current == CDT_TOPOLOGY_ID_NONE || current == vertex_id)
	    current = vertex_id;
	else
	    source_topology_conflict = true;
    };
    const auto assign_loop_topology = [&](const cpolygon_t *loop) {
	if (!loop)
	    return;
	for (const cpolyedge_t *edge : loop->poly) {
	    if (!edge || edge->trim_ind < 0 ||
		    edge->trim_ind >= brep->m_T.Count())
		continue;
	    const ON_BrepTrim &trim = brep->m_T[edge->trim_ind];
	    const auto first_point = loop->p2o.find(edge->v2d[0]);
	    const auto second_point = loop->p2o.find(edge->v2d[1]);
	    if (first_point == loop->p2o.end() ||
		    second_point == loop->p2o.end())
		continue;
	    const long first = first_point->second;
	    const long second = second_point->second;
	    if (trim.m_type == ON_BrepTrim::singular) {
		assign_topology_vertex(first, trim.m_vi[0]);
		assign_topology_vertex(second, trim.m_vi[0]);
		continue;
	    }
	    // Segment adjacency identifies the original trim endpoints without
	    // relying on parameter values changed by edge subdivision.
	    if (!edge->prev || edge->prev->trim_ind != edge->trim_ind)
		assign_topology_vertex(first, trim.m_vi[0]);
	    if (!edge->next || edge->next->trim_ind != edge->trim_ind)
		assign_topology_vertex(second, trim.m_vi[1]);
	}
    };
    assign_loop_topology(&outer_loop);
    for (const auto &loop : inner_loops)
	assign_loop_topology(loop.second);
    if (source_topology_conflict) {
	struct ON_Brep_CDT_State *state =
	    (struct ON_Brep_CDT_State *)p_cdt;
	if (state)
	    cdt_diagnostic_set(state, BREP_CDT_RESULT_CHART_FAILED,
		BREP_CDT_STAGE_CHART_CONSTRUCTION, f_id, 0, 1,
		"chart point has conflicting B-Rep vertex identities");
	return false;
    }
    /* Remove only zero-length constraints introduced by the explicit
     * mesh-only collapse of a proven sub-tolerance B-Rep edge.  Pointer
     * equality alone is not enough: periodic seams and singularities also
     * have multiple native UV copies of one legitimate model-space point.
     *
     * Prefer an identified topology vertex over an edge sample, then the
     * lowest topology ID (and finally native point ID).  That choice is
     * stable when trim or ring orientation is reversed and agrees with the
     * global collapse representative. */
    struct ON_Brep_CDT_State *collapse_state =
	(struct ON_Brep_CDT_State *)p_cdt;
    std::vector<long> collapsed_source_parent(m_pnts_2d.size(), -1);
    const auto collapsed_source_root = [&](long point) {
	long current = point;
	while (current >= 0 && collapsed_source_parent[(size_t)current] !=
		current)
	    current = collapsed_source_parent[(size_t)current];
	return current;
    };
    const auto index_collapsed_constraints = [&](const cpolygon_t *loop) {
	if (!collapse_state || !loop)
	    return;
	for (const cpolyedge_t *edge : loop->poly) {
	    if (!edge || edge->trim_ind < 0 ||
		    edge->trim_ind >= brep->m_T.Count())
		continue;
	    const int edge_index = brep->m_T[edge->trim_ind].m_ei;
	    if (collapse_state->collapsed_edges.find(edge_index) ==
		    collapse_state->collapsed_edges.end())
		continue;
	    const auto first_native = loop->p2o.find(edge->v2d[0]);
	    const auto second_native = loop->p2o.find(edge->v2d[1]);
	    if (first_native == loop->p2o.end() ||
		    second_native == loop->p2o.end() ||
		    first_native->second < 0 || second_native->second < 0 ||
		    (size_t)first_native->second >=
		    collapsed_source_parent.size() ||
		    (size_t)second_native->second >=
		    collapsed_source_parent.size())
		continue;
	    long first = first_native->second;
	    long second = second_native->second;
	    if (collapsed_source_parent[(size_t)first] < 0)
		collapsed_source_parent[(size_t)first] = first;
	    if (collapsed_source_parent[(size_t)second] < 0)
		collapsed_source_parent[(size_t)second] = second;
	    first = collapsed_source_root(first);
	    second = collapsed_source_root(second);
	    if (first == second)
		continue;
	    const long representative = std::min(first, second);
	    const long discarded = std::max(first, second);
	    collapsed_source_parent[(size_t)discarded] = representative;
	}
    };
    index_collapsed_constraints(&outer_loop);
    for (const auto &loop : inner_loops)
	index_collapsed_constraints(loop.second);
    std::vector<long> collapsed_source_components(m_pnts_2d.size(), -1);
    for (size_t i = 0; i < collapsed_source_parent.size(); ++i) {
	if (collapsed_source_parent[i] >= 0)
	    collapsed_source_components[i] = collapsed_source_root((long)i);
    }
    simplify_subtolerance_ring(source_outer, source_points_3d,
	source_topology_vertices, collapsed_source_components);
    for (std::vector<int> &hole : source_holes)
	simplify_subtolerance_ring(hole, source_points_3d,
	    source_topology_vertices, collapsed_source_components);

    /* Build a deterministic reverse map for regular charts.  Periodic seams
     * and singularities intentionally give one 3-D vertex more than one
     * native UV identity; mark those vertices ambiguous instead of choosing
     * whichever mapping happens to be visited first. */
    p3d2d.clear();
    ambiguous_p3d2d.clear();
    periodic_ambiguous_p3d2d.clear();
    for (const auto &point_map : p2d3d) {
	const long native_point = point_map.first;
	const long point_3d = point_map.second;
	if (native_point < 0 || (size_t)native_point >= m_pnts_2d.size() ||
		point_3d < 0 || (size_t)point_3d >= pnts.size())
	    continue;
	const auto canonical = p2ind.find(pnts[(size_t)point_3d]);
	if (canonical == p2ind.end())
	    continue;
	const auto old = p3d2d.find(canonical->second);
	if (old == p3d2d.end())
	    p3d2d[canonical->second] = native_point;
	else if (old->second != native_point &&
		m_pnts_2d[(size_t)old->second] !=
		m_pnts_2d[(size_t)native_point])
	    ambiguous_p3d2d.insert(canonical->second);
    }
    std::map<long, std::vector<long>> ambiguous_native_images;
    for (const auto &point_map : p2d3d) {
	const long native_point = point_map.first;
	const long point_3d = point_map.second;
	if (native_point < 0 || (size_t)native_point >= m_pnts_2d.size() ||
		point_3d < 0 || (size_t)point_3d >= pnts.size())
	    continue;
	const auto canonical = p2ind.find(pnts[(size_t)point_3d]);
	if (canonical == p2ind.end() || ambiguous_p3d2d.find(
		canonical->second) == ambiguous_p3d2d.end())
	    continue;
	ambiguous_native_images[canonical->second].push_back(native_point);
    }

    const ON_BrepFace &face = brep->m_F[f_id];
    std::vector<std::vector<int>> atlas_outlines;
    std::vector<cdt_topo_vertex_id> atlas_poles;
    ON_Cone analytic_cone;
    const ON_Surface *face_surface = face.SurfaceOf();
    if (face_surface) {
	for (long vertex : ambiguous_p3d2d) {
	    const auto chosen = p3d2d.find(vertex);
	    if (chosen == p3d2d.end() || chosen->second < 0 ||
		    (size_t)chosen->second >= m_pnts_2d.size() || vertex < 0 ||
		    (size_t)vertex >= pnts.size())
		continue;
	    const std::pair<double, double> &reference =
		m_pnts_2d[(size_t)chosen->second];
	    const auto images = ambiguous_native_images.find(vertex);
	    if (images == ambiguous_native_images.end())
		continue;
	    bool has_periodic_alias = false;
	    bool aliases_safe = true;
	    for (long native_point : images->second) {
		if (native_point == chosen->second || native_point < 0 ||
			(size_t)native_point >= m_pnts_2d.size())
		    continue;
		const std::pair<double, double> &candidate =
		    m_pnts_2d[(size_t)native_point];
		for (int direction = 0; direction < 2; ++direction) {
		    const double first = direction ? reference.second :
			reference.first;
		    const double second = direction ? candidate.second :
			candidate.first;
		    const double difference = second - first;
		    double residual = difference;
		    double scale = std::max(1.0, std::max(std::fabs(first),
			std::fabs(second)));
		    if (face_surface->IsClosed(direction)) {
			const double period =
			    face_surface->Domain(direction).Length();
			if (!(period > 0.0) || !std::isfinite(period)) {
			    aliases_safe = false;
			    break;
			}
			residual = std::remainder(difference, period);
			scale = std::max(scale, period);
			if (std::fabs(difference) > 4096.0 *
				std::numeric_limits<double>::epsilon() * scale)
			    has_periodic_alias = true;
		    }
		    const double tolerance = 4096.0 *
			std::numeric_limits<double>::epsilon() * scale;
		    if (std::fabs(residual) > tolerance) {
			aliases_safe = false;
			break;
		    }
		}
		if (!aliases_safe)
		    break;
	    }
	    if (aliases_safe && has_periodic_alias)
		periodic_ambiguous_p3d2d.insert(vertex);
	}
    }

    /* Shared-edge initialization inserts the existing topological pole point
     * when a master curve passes through a surface pole in its interior.  The
     * resulting boundary touches that point twice and is two chart disks, not
     * one simple polygon.  Split it at the two pointer-identical occurrences;
     * the shared edge point remains watertight on the neighboring face. */
    if (source_holes.empty() && face_surface) {
	int singular_side = -1;
	int singular_count = 0;
	for (int side = 0; side < 4; ++side) {
	    if (face_surface->IsSingular(side)) {
		singular_side = side;
		singular_count++;
	    }
	}
	if (singular_count == 1) {
	    const int open_direction = (singular_side == 0 ||
		singular_side == 2) ? 1 : 0;
	    const ON_Interval open_domain =
		face_surface->Domain(open_direction);
	    const double pole_parameter = (singular_side == 0 ||
		singular_side == 3) ? open_domain.Min() : open_domain.Max();
	    const double pole_parameter_tolerance = 256.0 *
		std::numeric_limits<double>::epsilon() * std::max(1.0,
		std::max(std::fabs(open_domain.Min()),
		    std::max(std::fabs(open_domain.Max()),
		    open_domain.Length())));
	    const auto is_topological_pole = [&](int source) {
		if (source < 0 || (size_t)source >= m_pnts_2d.size() ||
			(size_t)source >= source_topology_vertices.size() ||
			source_topology_vertices[(size_t)source] ==
			CDT_TOPOLOGY_ID_NONE)
		    return false;
		const std::pair<double, double> &uv =
		    m_pnts_2d[(size_t)source];
		const double open_parameter = open_direction ? uv.second :
		    uv.first;
		return std::fabs(open_parameter - pole_parameter) <=
		    pole_parameter_tolerance;
	    };
	    int pole_source = -1;
	    const size_t source_ring_size = source_outer.size() > 1 &&
		source_outer.front() == source_outer.back() ?
		source_outer.size() - 1 : source_outer.size();
	    for (size_t i = 0; i < source_ring_size; ++i) {
		if (is_topological_pole(source_outer[i])) {
		    pole_source = source_outer[i];
		    break;
		}
	    }
	    const ON_3dPoint *pole_point = pole_source >= 0 &&
		(size_t)pole_source < source_points_3d.size() ?
		source_points_3d[(size_t)pole_source] : NULL;
	    const auto is_pole_source = [&](int source) {
		return source >= 0 && pole_point &&
		    (size_t)source < source_points_3d.size() &&
		    source_points_3d[(size_t)source] == pole_point;
	    };
	    std::vector<int> ring;
	    ring.reserve(source_outer.size());
	    for (size_t i = 0; i < source_ring_size; ++i) {
		const int source = source_outer[i];
		if (!ring.empty() && is_pole_source(source) &&
			is_pole_source(ring.back()))
		    continue;
		ring.push_back(source);
	    }
	    if (ring.size() > 1 && is_pole_source(ring.front()) &&
		    is_pole_source(ring.back()))
		ring.pop_back();
	    std::vector<size_t> pole_positions;
	    std::set<cdt_topo_vertex_id> pole_topologies;
	    for (size_t i = 0; i < ring.size(); ++i) {
		if (is_pole_source(ring[i])) {
		    pole_positions.push_back(i);
		    const int source = ring[i];
		    if (source >= 0 && (size_t)source <
			    source_topology_vertices.size() &&
			    source_topology_vertices[(size_t)source] !=
			    CDT_TOPOLOGY_ID_NONE)
			pole_topologies.insert(
			    source_topology_vertices[(size_t)source]);
		}
	    }
	    if (pole_positions.size() == 2 && pole_topologies.size() == 1) {
		const auto ring_path = [&](size_t first, size_t last) {
		    std::vector<int> path;
		    for (size_t i = first;; i = (i + 1) % ring.size()) {
			path.push_back(ring[i]);
			if (i == last)
			    break;
		    }
		    return path;
		};
		std::vector<int> components[2] = {
		    ring_path(pole_positions[0], pole_positions[1]),
		    ring_path(pole_positions[1], pole_positions[0])
		};
		if (components[0].size() >= 4 && components[1].size() >= 4) {
		    atlas_outlines.push_back(components[0]);
		    atlas_outlines.push_back(components[1]);
		    const int first_pole = ring[pole_positions[0]];
		    const int second_pole = ring[pole_positions[1]];
		    const cdt_topo_vertex_id first_topology =
			source_topology_vertices[(size_t)first_pole];
		    const cdt_topo_vertex_id second_topology =
			source_topology_vertices[(size_t)second_pole];
		    if (first_topology == CDT_TOPOLOGY_ID_NONE ||
			    second_topology == CDT_TOPOLOGY_ID_NONE) {
			atlas_outlines.clear();
		    } else {
			atlas_poles.push_back(first_topology);
			atlas_poles.push_back(second_topology);
			bu_log("Face %d: decomposed a repeated pole boundary "
			    "into two chart disks\n", f_id);
		    }
		}
	    }
	}
    }

    if (atlas_outlines.empty() && source_holes.empty() && face_surface &&
	    face_surface->IsCone(&analytic_cone, BREP_PLANAR_TOL)) {
	int singular_side = -1;
	int singular_count = 0;
	for (int side = 0; side < 4; ++side) {
	    if (face_surface->IsSingular(side)) {
		singular_side = side;
		singular_count++;
	    }
	}
	if (singular_count == 1) {
	    const int open_direction = (singular_side == 0 ||
		singular_side == 2) ? 1 : 0;
	    const ON_Interval open_domain =
		face_surface->Domain(open_direction);
	    const double pole_coordinate = (singular_side == 0 ||
		singular_side == 3) ? open_domain.Min() :
		open_domain.Max();
	    const double magnitude = std::max(std::fabs(open_domain.Min()),
		std::fabs(open_domain.Max()));
	    const double pole_tolerance = 256.0 *
		std::numeric_limits<double>::epsilon() *
		std::max(magnitude, open_domain.Length());
	    std::vector<int> ring = source_outer;
	    if (ring.size() > 1 && ring.front() == ring.back())
		ring.pop_back();
	    std::set<cdt_topo_vertex_id> pole_ids;
	    std::map<cdt_topo_vertex_id, std::vector<size_t>> occurrences;
	    for (size_t i = 0; i < ring.size(); ++i) {
		const int point = ring[i];
		if (point < 0 || (size_t)point >= m_pnts_2d.size() ||
			(size_t)point >= source_topology_vertices.size())
		    continue;
		const cdt_topo_vertex_id topology =
		    source_topology_vertices[(size_t)point];
		if (topology == CDT_TOPOLOGY_ID_NONE)
		    continue;
		const std::pair<double, double> &uv =
		    m_pnts_2d[(size_t)point];
		if (std::fabs((open_direction ? uv.second : uv.first) -
			pole_coordinate) <= pole_tolerance)
		    pole_ids.insert(topology);
		occurrences[topology].push_back(i);
	    }
	    if (pole_ids.size() > 1) {
		for (const auto &entry : occurrences) {
		    if (pole_ids.find(entry.first) != pole_ids.end() ||
			    entry.second.size() != 2)
			continue;
		    const size_t first = entry.second[0];
		    const size_t second = entry.second[1];
		    std::vector<int> components[2];
		    components[0].insert(components[0].end(),
			ring.begin() + first, ring.begin() + second + 1);
		    components[1].insert(components[1].end(),
			ring.begin() + second, ring.end());
		    components[1].insert(components[1].end(), ring.begin(),
			ring.begin() + first + 1);
		    std::set<cdt_topo_vertex_id> component_poles[2];
		    for (int component = 0; component < 2; ++component) {
			for (int point : components[component]) {
			    if (point < 0 || (size_t)point >=
				    source_topology_vertices.size())
				continue;
			    const cdt_topo_vertex_id topology =
				source_topology_vertices[(size_t)point];
			    if (pole_ids.find(topology) != pole_ids.end())
				component_poles[component].insert(topology);
			}
		    }
		    if (components[0].size() >= 3 &&
			    components[1].size() >= 3 &&
			    component_poles[0].size() == 1 &&
			    component_poles[1].size() == 1 &&
			    *component_poles[0].begin() !=
			    *component_poles[1].begin()) {
			atlas_outlines.push_back(components[0]);
			atlas_outlines.push_back(components[1]);
			atlas_poles.push_back(*component_poles[0].begin());
			atlas_poles.push_back(*component_poles[1].begin());
			break;
		    }
		}
	    }
	}
    }

    if (atlas_outlines.size() == 2 && atlas_poles.size() == 2) {
	std::vector<cdt_face_chart> atlas(2);
	bool atlas_built = true;
	std::string atlas_failure;
	for (size_t component = 0; component < atlas.size(); ++component) {
	    const bool component_built = atlas[component].build(face,
		m_pnts_2d,
		atlas_outlines[component], std::vector<std::vector<int>>(),
		source_steiner, source_refinement, source_points_3d,
		source_topology_vertices, atlas_poles[component]);
	    if (!component_built && atlas_failure.empty())
		atlas_failure = atlas[component].failure();
	    atlas_built = component_built && atlas_built;
	}
	if (atlas_built && install_face_chart_atlas(this, face, atlas,
		atlas_failure))
	    return true;
	if (atlas_failure.empty())
	    return false;
	struct ON_Brep_CDT_State *state =
	    (struct ON_Brep_CDT_State *)p_cdt;
	if (state)
	    cdt_diagnostic_set(state, BREP_CDT_RESULT_CHART_FAILED,
		BREP_CDT_STAGE_CHART_CONSTRUCTION, f_id, 0, 1,
		atlas_failure.c_str());
	return false;
    }

    cdt_face_chart chart;
    if (!chart.build(face, m_pnts_2d, source_outer, source_holes,
	    source_steiner, source_refinement, source_points_3d,
	    source_topology_vertices)) {
	bu_log("Face %d: chart construction failed: %s\n", f_id,
	    chart.failure().c_str());
	struct ON_Brep_CDT_State *state =
	    (struct ON_Brep_CDT_State *)p_cdt;
	if (state)
	    cdt_diagnostic_set(state, BREP_CDT_RESULT_CHART_FAILED,
		BREP_CDT_STAGE_CHART_CONSTRUCTION, f_id, 0, 1,
		chart.failure().c_str());
	return false;
    }
    /* A loose B-Rep edge may have distinct master-curve samples whose p-curve
     * rounds to its endpoint.  Recover a strictly ordered chart path only
     * when the collapsed residual is within the model's own tolerance. */
    const struct ON_Brep_CDT_State *chart_state =
	(const struct ON_Brep_CDT_State *)p_cdt;
    const double mesh_tolerance = chart_state &&
	std::isfinite(chart_state->absmin) && chart_state->absmin > 0.0 ?
	std::min((double)BN_TOL_DIST, chart_state->absmin) : 0.0;
    size_t repaired_endpoint_samples = 0;
    const auto repair_loop_endpoint_samples = [&](const cpolygon_t *loop) {
	if (!loop)
	    return;
	for (const cpolyedge_t *start : loop->poly) {
	    if (!start || start->trim_ind < 0 ||
		    start->trim_ind >= brep->m_T.Count() ||
		    (start->prev &&
		    start->prev->trim_ind == start->trim_ind))
		continue;
	    const ON_BrepTrim &trim = brep->m_T[start->trim_ind];
	    const ON_BrepEdge *edge = trim.Edge();
	    if (!edge || edge->IsClosed() ||
		    trim.m_type == ON_BrepTrim::singular)
		continue;
	    std::vector<int> native_path;
	    const cpolyedge_t *segment = start;
	    do {
		const auto first = loop->p2o.find(segment->v2d[0]);
		const auto second = loop->p2o.find(segment->v2d[1]);
		if (first == loop->p2o.end() || second == loop->p2o.end() ||
			(!native_path.empty() &&
			native_path.back() != first->second)) {
		    native_path.clear();
		    break;
		}
		if (native_path.empty())
		    native_path.push_back((int)first->second);
		native_path.push_back((int)second->second);
		segment = segment->next;
	    } while (segment && segment != start &&
		segment->trim_ind == start->trim_ind);
	    if (native_path.size() < 3)
		continue;
	    const double tolerance = std::max(mesh_tolerance,
		std::isfinite(edge->m_tolerance) ? edge->m_tolerance : 0.0);
	    repaired_endpoint_samples +=
		chart.repair_toleranced_edge_endpoint_samples(native_path,
		    source_points_3d, tolerance);
	}
    };
    repair_loop_endpoint_samples(&outer_loop);
    for (const auto &loop : inner_loops)
	repair_loop_endpoint_samples(loop.second);
    if (repaired_endpoint_samples)
	bu_log("Face %d: separated %zu toleranced B-Rep edge endpoint "
	    "sample%s in the chart\n", f_id, repaired_endpoint_samples,
	    repaired_endpoint_samples == 1 ? "" : "s");
    /* An iso trim from a cone pole is a straight ray in the cone chart.  A
     * valid B-Rep may let its trim pullback wander within the edge tolerance;
     * near the pole that harmless native-UV noise otherwise becomes a chart
     * zigzag and produces zero-area 3-D ears.  Straighten the chart image of
     * each such trim while preserving every native sample and its shared 3-D
     * edge point. */
    if (chart.type() == CDT_FACE_CHART_CONE_WEDGE) {
	std::map<int, std::map<double, long>> trim_samples;
	const auto collect_trim_samples = [&](const cpolygon_t *loop) {
	    if (!loop)
		return;
	    for (const cpolyedge_t *edge : loop->poly) {
		if (!edge || edge->trim_ind < 0)
		    continue;
		const auto first = loop->p2o.find(edge->v2d[0]);
		const auto second = loop->p2o.find(edge->v2d[1]);
		if (first == loop->p2o.end() || second == loop->p2o.end())
		    continue;
		trim_samples[edge->trim_ind][edge->trim_start] =
		    first->second;
		trim_samples[edge->trim_ind][edge->trim_end] =
		    second->second;
	    }
	};
	collect_trim_samples(&outer_loop);
	for (const auto &loop : inner_loops)
	    collect_trim_samples(loop.second);

	std::map<long, int> native_to_chart;
	int pole_chart_point = -1;
	for (const cdt_chart_vertex &vertex : chart.vertices) {
	    if (vertex.native_point >= 0)
		native_to_chart[vertex.native_point] = vertex.id;
	    if (vertex.singular && vertex.topo_vertex ==
		    chart.pole_topology_vertex())
		pole_chart_point = vertex.id;
	}
	const auto chart_point = [&](long native) {
	    const auto mapped = native_to_chart.find(native);
	    if (mapped != native_to_chart.end())
		return mapped->second;
	    if (native >= 0 && (size_t)native <
		    source_topology_vertices.size() &&
		    source_topology_vertices[(size_t)native] ==
		    chart.pole_topology_vertex())
		return pole_chart_point;
	    return -1;
	};
	for (const auto &trim_entry : trim_samples) {
	    if (trim_entry.first < 0 ||
		    trim_entry.first >= brep->m_T.Count())
		continue;
	    const ON_BrepTrim &trim = brep->m_T[trim_entry.first];
	    const ON_BrepEdge *brep_edge = trim.Edge();
	    const ON_Curve *edge_curve = brep_edge ?
		brep_edge->EdgeCurveOf() : NULL;
	    double linear_tolerance = BN_TOL_DIST;
	    if (brep_edge && std::isfinite(brep_edge->m_tolerance) &&
		    brep_edge->m_tolerance > 0.0 &&
		    !NEAR_EQUAL(brep_edge->m_tolerance, ON_UNSET_VALUE,
		    ON_ZERO_TOLERANCE))
		linear_tolerance = std::max(linear_tolerance,
		    brep_edge->m_tolerance);
	    const bool radial_trim = trim.m_iso != ON_Surface::not_iso ||
		(edge_curve && edge_curve->IsLinear(linear_tolerance));
	    if (trim.m_type == ON_BrepTrim::singular || !radial_trim)
		continue;
	    std::vector<std::pair<int, long>> samples;
	    for (const auto &sample : trim_entry.second) {
		const int mapped = chart_point(sample.second);
		if (mapped < 0)
		    continue;
		if (samples.empty() || samples.back().first != mapped)
		    samples.push_back(std::make_pair(mapped, sample.second));
	    }
	    if (samples.size() < 3 ||
		    (samples.front().first != pole_chart_point &&
		    samples.back().first != pole_chart_point))
		continue;
	    std::vector<double> distance(samples.size(), 0.0);
	    bool mapped_3d = true;
	    for (size_t i = 1; i < samples.size(); ++i) {
		const long first_native = samples[i - 1].second;
		const long second_native = samples[i].second;
		if (first_native < 0 || second_native < 0 ||
			(size_t)first_native >= source_points_3d.size() ||
			(size_t)second_native >= source_points_3d.size() ||
			!source_points_3d[(size_t)first_native] ||
			!source_points_3d[(size_t)second_native]) {
		    mapped_3d = false;
		    break;
		}
		distance[i] = distance[i - 1] +
		    source_points_3d[(size_t)first_native]->DistanceTo(
		    *source_points_3d[(size_t)second_native]);
	    }
	    const double total = distance.back();
	    if (!mapped_3d || !(total > 0.0))
		continue;
	    const std::pair<double, double> first =
		chart.points[(size_t)samples.front().first];
	    const std::pair<double, double> last =
		chart.points[(size_t)samples.back().first];
	    for (size_t i = 1; i + 1 < samples.size(); ++i) {
		const double fraction = distance[i] / total;
		std::pair<double, double> &point =
		    chart.points[(size_t)samples[i].first];
		point.first = first.first + fraction *
		    (last.first - first.first);
		point.second = first.second + fraction *
		    (last.second - first.second);
	    }
	}
    }

    std::vector<cdt_face_chart> chart_components;
    std::string component_failure;
    if (!chart.partition_components(chart_components,
	    &component_failure)) {
	struct ON_Brep_CDT_State *state =
	    (struct ON_Brep_CDT_State *)p_cdt;
	if (state)
	    cdt_diagnostic_set(state, BREP_CDT_RESULT_CHART_FAILED,
		BREP_CDT_STAGE_CHART_CONSTRUCTION, f_id, 0, 1,
		component_failure.c_str());
	return false;
    }
    if (chart_components.size() > 1) {
	bu_log("Face %d: partitioned disconnected chart loops into %zu "
	    "filled components\n", f_id, chart_components.size());
	if (install_face_chart_atlas(this, face, chart_components,
		component_failure))
	    return true;
	if (component_failure.empty())
	    return false;
	struct ON_Brep_CDT_State *state =
	    (struct ON_Brep_CDT_State *)p_cdt;
	if (state)
	    cdt_diagnostic_set(state, BREP_CDT_RESULT_CHART_FAILED,
		BREP_CDT_STAGE_CHART_CONSTRUCTION, f_id, 0, 1,
		component_failure.c_str());
	return false;
    }

    /* The chart boundary is the boundary the triangulator actually sees.
     * At a pole it can replace a subdivided singular trim with one edge, so
     * native loop segments alone are not a sufficient validity oracle. */
    for (const uedge_t &edge : chart_boundary_edges)
	brep_edges.erase(edge);
    chart_boundary_edges.clear();
    const auto record_chart_boundary = [&](const std::vector<int> &ring) {
	if (ring.size() < 2)
	    return false;
	for (size_t i = 0; i < ring.size(); ++i) {
	    const long first_native = chart.native_point(ring[i]);
	    const long second_native = chart.native_point(
		ring[(i + 1) % ring.size()]);
	    const auto first_3d = p2d3d.find(first_native);
	    const auto second_3d = p2d3d.find(second_native);
	    if (first_3d == p2d3d.end() || second_3d == p2d3d.end() ||
		first_3d->second < 0 || second_3d->second < 0 ||
		(size_t)first_3d->second >= pnts.size() ||
		(size_t)second_3d->second >= pnts.size())
		return false;
	    const auto first_mesh = p2ind.find(
		pnts[(size_t)first_3d->second]);
	    const auto second_mesh = p2ind.find(
		pnts[(size_t)second_3d->second]);
	    if (first_mesh == p2ind.end() || second_mesh == p2ind.end())
		return false;
	    ep.insert(first_mesh->second);
	    ep.insert(second_mesh->second);
	    if (first_mesh->second == second_mesh->second)
		continue;
	    const uedge_t edge(first_mesh->second, second_mesh->second);
	    chart_boundary_edges.insert(edge);
	    brep_edges.insert(edge);
	}
	return true;
    };
    bool chart_boundary_mapped = record_chart_boundary(chart.outer);
    for (const std::vector<int> &hole : chart.holes)
	chart_boundary_mapped = record_chart_boundary(hole) &&
	    chart_boundary_mapped;
    if (!chart_boundary_mapped) {
	struct ON_Brep_CDT_State *state =
	    (struct ON_Brep_CDT_State *)p_cdt;
	if (state)
	    cdt_diagnostic_set(state, BREP_CDT_RESULT_CHART_FAILED,
		BREP_CDT_STAGE_CHART_CONSTRUCTION, f_id, 0, 1,
		"chart boundary did not map to mesh vertices");
	return false;
    }

    // Carry explicit chart singularity identity into the 3-D mesh.  The
    // legacy global singular-normal map is incomplete when a pole has no
    // usable evaluated normal, but that must not make the pole masquerade as
    // an ordinary B-Rep edge sample during validation.
    for (const cdt_chart_vertex &vertex : chart.vertices) {
	if (!vertex.singular || vertex.native_point < 0)
	    continue;
	const auto p3d_index = p2d3d.find(vertex.native_point);
	if (p3d_index == p2d3d.end() || p3d_index->second < 0 ||
		(size_t)p3d_index->second >= pnts.size())
	    continue;
	const auto mesh_index = p2ind.find(pnts[(size_t)p3d_index->second]);
	if (mesh_index != p2ind.end())
	    sv.insert(mesh_index->second);
    }

    point2d_t *bgp_2d = (point2d_t *)bu_calloc(chart.points.size(),
	sizeof(point2d_t), "chart points array");
    for (size_t i = 0; i < chart.points.size(); ++i)
	V2SET(bgp_2d[i], chart.points[i].first, chart.points[i].second);

    const size_t opoly_count = chart.outer.size() + 1;
    int *opoly = (int *)bu_calloc(opoly_count, sizeof(int),
	"chart outline");
    for (size_t i = 0; i < chart.outer.size(); ++i)
	opoly[i] = chart.outer[i];
    opoly[chart.outer.size()] = chart.outer[0];
    std::vector<double> outer_poly_flat(opoly_count * 2);
    for (size_t pi = 0; pi < opoly_count; ++pi) {
	const int point = opoly[pi];
	outer_poly_flat[pi * 2] = bgp_2d[point][X];
	outer_poly_flat[pi * 2 + 1] = bgp_2d[point][Y];
    }

    const int holes_cnt = (int)chart.holes.size();
    const int **holes_array = NULL;
    size_t *holes_npts = NULL;
    if (holes_cnt) {
	holes_array = (const int **)bu_calloc((size_t)holes_cnt,
	    sizeof(int *), "chart holes");
	holes_npts = (size_t *)bu_calloc((size_t)holes_cnt,
	    sizeof(size_t), "chart hole counts");
	for (int hi = 0; hi < holes_cnt; ++hi) {
	    holes_npts[hi] = chart.holes[(size_t)hi].size() + 1;
	    int *hole = (int *)bu_calloc(holes_npts[hi], sizeof(int),
		"chart hole");
	    for (size_t pi = 0; pi < chart.holes[(size_t)hi].size(); ++pi)
		hole[pi] = chart.holes[(size_t)hi][pi];
	    hole[holes_npts[hi] - 1] = chart.holes[(size_t)hi][0];
	    holes_array[hi] = hole;
	}
    }

    std::vector<std::vector<double>> hole_polys_flat((size_t)holes_cnt);
    for (int hi = 0; hi < holes_cnt; ++hi) {
	hole_polys_flat[(size_t)hi].resize(holes_npts[hi] * 2);
	for (size_t pi = 0; pi < holes_npts[hi]; ++pi) {
	    const int point = holes_array[hi][pi];
	    hole_polys_flat[(size_t)hi][pi * 2] = bgp_2d[point][X];
	    hole_polys_flat[(size_t)hi][pi * 2 + 1] = bgp_2d[point][Y];
	}
    }

    double chart_min_x = DBL_MAX;
    double chart_max_x = -DBL_MAX;
    double chart_min_y = DBL_MAX;
    double chart_max_y = -DBL_MAX;
    for (const auto &point : chart.points) {
	chart_min_x = std::min(chart_min_x, point.first);
	chart_max_x = std::max(chart_max_x, point.first);
	chart_min_y = std::min(chart_min_y, point.second);
	chart_max_y = std::max(chart_max_y, point.second);
    }
    const double boundary_scale = std::max(DBL_MIN,
	hypot(chart_max_x - chart_min_x, chart_max_y - chart_min_y));
    const double boundary_tolerance = sqrt(DBL_EPSILON) * boundary_scale;
    const double boundary_tolerance_sq = boundary_tolerance *
	boundary_tolerance;
    const double coordinate_scale = std::max(1.0, std::max(
	std::max(std::fabs(chart_min_x), std::fabs(chart_max_x)),
	std::max(std::fabs(chart_min_y), std::fabs(chart_max_y))));
    const double duplicate_tolerance = 1024.0 * DBL_EPSILON *
	std::max(coordinate_scale, boundary_scale);
    RTree<size_t, double, 2> boundary_index;
    std::vector<chart_boundary_segment> boundary_segments;
    index_polygon_boundary(boundary_index, boundary_segments, bgp_2d,
	opoly, opoly_count, boundary_tolerance);
    for (int hi = 0; hi < holes_cnt; ++hi)
	index_polygon_boundary(boundary_index, boundary_segments, bgp_2d,
	    holes_array[hi], holes_npts[hi], boundary_tolerance);
    std::vector<int> steiner_vec;
    steiner_vec.reserve(chart.steiner.size());
    RTree<size_t, double, 2> steiner_index;
    const bool planar_chart = face.SurfaceOf()->IsPlanar(NULL,
	ON_ZERO_TOLERANCE);
    for (int point : chart.steiner) {
	if (planar_chart)
	    continue;
	if (point_on_indexed_boundary(boundary_index, boundary_segments,
		bgp_2d, point, boundary_tolerance_sq))
	    continue;
	point2d_t test_point;
	V2SET(test_point, bgp_2d[point][X], bgp_2d[point][Y]);
	const point2d_t *outer_polygon = (const point2d_t *)
	    outer_poly_flat.data();
	if (!bg_pnt_in_polygon(opoly_count, outer_polygon,
		(const point2d_t *)&test_point))
	    continue;
	double duplicate_minimum[2] = {
	    bgp_2d[point][X] - duplicate_tolerance,
	    bgp_2d[point][Y] - duplicate_tolerance
	};
	double duplicate_maximum[2] = {
	    bgp_2d[point][X] + duplicate_tolerance,
	    bgp_2d[point][Y] + duplicate_tolerance
	};
	if (steiner_index.Search(duplicate_minimum, duplicate_maximum,
		NULL, NULL))
	    continue;
	bool in_hole = false;
	for (int hi = 0; hi < holes_cnt && !in_hole; ++hi) {
	    const point2d_t *hole = (const point2d_t *)
		hole_polys_flat[(size_t)hi].data();
	    in_hole = bg_pnt_in_polygon(holes_npts[hi], hole,
		(const point2d_t *)&test_point);
	}
	if (!in_hole) {
	    steiner_vec.push_back(point);
	    double location[2] = {bgp_2d[point][X], bgp_2d[point][Y]};
	    steiner_index.Insert(location, location, steiner_vec.size() - 1);
	}
    }
    int *steiner = steiner_vec.empty() ? NULL : steiner_vec.data();
    const size_t steiner_cnt = steiner_vec.size();
    std::vector<int> constraint_vec;
    constraint_vec.reserve(chart.constraints.size() * 2);
    for (const std::pair<int, int> &constraint : chart.constraints) {
	constraint_vec.push_back(constraint.first);
	constraint_vec.push_back(constraint.second);
    }

    int *faces = NULL;
    int num_faces = 0;
    struct bg_triangulation_report tri_report = {0, -1, {0}};
    bool result = (bool)!bg_nested_poly_triangulate_constraints_strict(&faces,
	&num_faces, NULL, NULL, opoly, opoly_count,
	(const int **)holes_array, holes_npts, holes_cnt, steiner, steiner_cnt,
	constraint_vec.empty() ? NULL : constraint_vec.data(),
	chart.constraints.size(), bgp_2d, chart.points.size(), &tri_report);

    if (!result && constraint_vec.empty() &&
	    cleanable_developable_chart(face, chart)) {
	if (faces) {
	    bu_free(faces, "failed strict chart faces");
	    faces = NULL;
	}
	result = topology_preserving_clean_triangulation(&faces, &num_faces,
	    chart, source_points_3d, opoly, opoly_count,
	    (const int **)holes_array, holes_npts, holes_cnt, steiner,
	    steiner_cnt, bgp_2d);
	if (result)
	    bu_log("Face %d: normalized weakly-simple developable chart "
		"topology "
		"without changing its B-Rep boundary\n", f_id);
    }

    if (!result) {
	bu_log("Face %d: constrained triangulation failed: %s "
	    "(bnd_pnts=%zu steiner=%zu/%zu holes=%d)\n", f_id,
	    tri_report.message, chart.outer.size(), steiner_cnt,
	    m_interior_pnts.size(), holes_cnt);
	struct ON_Brep_CDT_State *state =
	    (struct ON_Brep_CDT_State *)p_cdt;
	if (state) {
	    int brep_result = BREP_CDT_RESULT_INVALID_PSLG;
	    int brep_stage = BREP_CDT_STAGE_PSLG_VALIDATION;
	    if (tri_report.reason == BG_TRIANGULATION_DETRIA_FAILED) {
		brep_result = BREP_CDT_RESULT_DETRIA_FAILED;
		brep_stage = BREP_CDT_STAGE_DETRIA;
	    } else if (tri_report.reason ==
		    BG_TRIANGULATION_POSTCONDITION_FAILED) {
		brep_result = BREP_CDT_RESULT_CERTIFICATION_FAILED;
		brep_stage = BREP_CDT_STAGE_DETRIA;
	    }
	    cdt_diagnostic_set(state, brep_result, brep_stage, f_id, 0, 1,
		tri_report.message);
	}

	if (cdt_failure_dumps_enabled()) {
	    // Dump a stand-alone C test program so the failure can be reproduced
	    // and scrutinised independently of the full CDT pipeline.
	    struct bu_vls fname = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&fname, "cdt_face%d_fail.c", f_id);
	    FILE *df = fopen(bu_vls_cstr(&fname), "w");
	    if (df) {
		fprintf(df, "#include <stdio.h>\n");
		fprintf(df, "#include \"bu/malloc.h\"\n");
		fprintf(df, "#include \"bg/polygon.h\"\n");
		fprintf(df, "/* chart type %d, closed direction %d */\n",
		    (int)chart.type(), chart.closed_direction());
		if (face.SurfaceOf() && chart.closed_direction() >= 0) {
		    const ON_Interval domain = face.SurfaceOf()->Domain(
			chart.closed_direction());
		    fprintf(df, "/* closed domain %.17g %.17g */\n",
			domain.Min(), domain.Max());
		}
		std::map<long, std::set<std::pair<int, int>>>
		    source_boundary_provenance;
		const auto collect_boundary_provenance =
		    [&](const cpolygon_t *loop) {
			if (!loop)
			    return;
			for (const cpolyedge_t *edge : loop->poly) {
			    if (!edge)
				continue;
			    const int brep_edge = edge->trim_ind >= 0 &&
				    edge->trim_ind < brep->m_T.Count() ?
				brep->m_T[edge->trim_ind].m_ei : -1;
			    for (int endpoint = 0; endpoint < 2; ++endpoint) {
				const auto native = loop->p2o.find(
				    edge->v2d[endpoint]);
				if (native != loop->p2o.end())
				    source_boundary_provenance[native->second].insert(
					std::make_pair(edge->trim_ind, brep_edge));
			    }
			}
		    };
		collect_boundary_provenance(&outer_loop);
		for (const auto &loop : inner_loops)
		    collect_boundary_provenance(loop.second);
		for (const cdt_chart_vertex &vertex : chart.vertices) {
		    const ON_3dPoint *point = vertex.native_point >= 0 &&
			(size_t)vertex.native_point < source_points_3d.size() ?
			source_points_3d[(size_t)vertex.native_point] : NULL;
		    fprintf(df, "/* chart %lld native %ld topo %lld edge %lld "
			"sample %lld seam %d singular %d",
			(long long)vertex.id, vertex.native_point,
			(long long)vertex.topo_vertex,
			(long long)vertex.brep_edge,
			(long long)vertex.edge_sample, vertex.seam_side,
			vertex.singular ? 1 : 0);
		    if (point)
			fprintf(df, " point %.17g %.17g %.17g", point->x,
			    point->y, point->z);
		    if (vertex.native_point >= 0 &&
			    (size_t)vertex.native_point < m_pnts_2d.size())
			fprintf(df, " uv %.17g %.17g",
			    m_pnts_2d[(size_t)vertex.native_point].first,
			    m_pnts_2d[(size_t)vertex.native_point].second);
		    const auto provenance =
			source_boundary_provenance.find(vertex.native_point);
		    if (provenance != source_boundary_provenance.end()) {
			fprintf(df, " boundary");
			for (const auto &entry : provenance->second) {
			    const double tolerance = entry.second >= 0 &&
				    entry.second < brep->m_E.Count() ?
				brep->m_E[entry.second].m_tolerance : 0.0;
			    fprintf(df, " t%d/e%d/tol=%.17g", entry.first,
				entry.second, tolerance);
			}
		    }
		    fprintf(df, " */\n");
		}
		fprintf(df, "int main() {\n");
		size_t np = chart.points.size();
		fprintf(df,
			"    point2d_t *bgp_2d = (point2d_t *)bu_calloc(%zu, "
			"sizeof(point2d_t), \"2d pts\");\n",
			np);
		for (size_t i = 0; i < np; i++) {
		    fprintf(df, "    bgp_2d[%zu][X] = %.17g;\n", i,
			    chart.points[i].first);
		    fprintf(df, "    bgp_2d[%zu][Y] = %.17g;\n", i,
			    chart.points[i].second);
		}
		// The polygon array for bg_nested_poly_triangulate uses a closed
		// format: the first vertex index is repeated as the last entry
		// (size = edge_count + 1).
		size_t on = opoly_count;
		fprintf(df,
			"    int *opoly = (int *)bu_calloc(%zu, sizeof(int), "
			"\"opoly\");\n",
			on);
		for (size_t i = 0; i < on; i++)
		    fprintf(df, "    opoly[%zu] = %d;\n", i, opoly[i]);
		if (holes_cnt) {
		    fprintf(df,
			    "    const int **holes = (const int **)bu_calloc(%d+1, "
			    "sizeof(int *), \"holes\");\n",
			    holes_cnt);
		    fprintf(df,
			    "    size_t *holes_npts = (size_t *)bu_calloc(%d+1, "
			    "sizeof(size_t), \"hnpts\");\n",
			    holes_cnt);
		    for (int hi = 0; hi < holes_cnt; hi++) {
			size_t hn = holes_npts[hi];
			fprintf(df,
				"    int *hole%d = (int *)bu_calloc(%zu, "
				"sizeof(int), \"h%d\");\n",
				hi, hn, hi);
			for (size_t hj = 0; hj < hn; hj++)
			    fprintf(df, "    hole%d[%zu] = %d;\n", hi, hj,
				    holes_array[hi][hj]);
			fprintf(df, "    holes[%d] = hole%d; holes_npts[%d] = %zu;\n",
				hi, hi, hi, hn);
		    }
		} else {
		    fprintf(df, "    const int **holes = NULL;\n");
		    fprintf(df, "    size_t *holes_npts = NULL;\n");
		}
		if (steiner_cnt) {
		    fprintf(df,
			    "    int *steiner = (int *)bu_calloc(%zu, sizeof(int), "
			    "\"stei\");\n",
			    steiner_cnt);
		    for (size_t si = 0; si < steiner_cnt; si++)
			fprintf(df, "    steiner[%zu] = %d;\n", si, steiner[si]);
		} else {
		    fprintf(df, "    int *steiner = NULL;\n");
		}
		if (!constraint_vec.empty()) {
		    fprintf(df,
			    "    int *constraints = (int *)bu_calloc(%zu, "
			    "sizeof(int), \"constraints\");\n",
			    constraint_vec.size());
		    for (size_t ci = 0; ci < constraint_vec.size(); ++ci)
			fprintf(df, "    constraints[%zu] = %d;\n", ci,
				constraint_vec[ci]);
		} else {
		    fprintf(df, "    int *constraints = NULL;\n");
		}
		fprintf(df, "    int *faces = NULL; int num_faces = 0;\n");
		fprintf(df, "    int r = "
			    "!bg_nested_poly_triangulate_constraints_strict(&"
			    "faces, &num_faces,\n");
		fprintf(
		    df,
		    "        NULL, NULL, opoly, %zu, holes, holes_npts, %d,\n",
		    on, holes_cnt);
		fprintf(df,
			"        steiner, %zu, constraints, %zu, bgp_2d, %zu, "
			"NULL);\n",
			steiner_cnt, chart.constraints.size(), np);
		fprintf(df, "    if (r) printf(\"success\\n\"); else "
			    "printf(\"FAIL\\n\");\n");
		fprintf(df, "    return !r;\n}\n");
		fclose(df);
		bu_log("Face %d: CDT failure inputs written to %s\n", f_id,
		       bu_vls_cstr(&fname));
	    }
	    bu_vls_free(&fname);
	}
    }

    tris_2d.clear();
    if (result) {
	for (int i = 0; i < num_faces; i++) {
	    triangle_t t;
	    t.v[0] = chart.native_point(faces[3*i+0]);
	    t.v[1] = chart.native_point(faces[3*i+1]);
	    t.v[2] = chart.native_point(faces[3*i+2]);
	    if (t.v[0] < 0 || t.v[1] < 0 || t.v[2] < 0) {
		result = false;
		tris_2d.clear();
		struct ON_Brep_CDT_State *state =
		    (struct ON_Brep_CDT_State *)p_cdt;
		if (state)
		    cdt_diagnostic_set(state,
			BREP_CDT_RESULT_CERTIFICATION_FAILED,
			BREP_CDT_STAGE_DETRIA, f_id, 0, 1,
			"chart output did not map to native UV");
		break;
	    }

	    tris_2d.push_back(t);
	}
    }

    bu_free(faces, "faces array");

    bu_free(bgp_2d, "free libbg 2d points array)");
    bu_free(opoly, "polygon points");

    if (holes_cnt) {
	for (int i = 0; i < holes_cnt; i++) {
	    bu_free((void *)holes_array[i], "hole array");
	}
	bu_free((void *)holes_array, "holes array");
	bu_free(holes_npts, "holes array");
    }

    // Use the 2D triangles to create the face 3D triangle mesh.  Preserve
    // chart orientation when only a subset of coarse surface chords fold in
    // 3-D.  Flipping such triangles individually destroys edge incidence;
    // the caller can instead refine their chart regions transactionally.
    reset();
    std::vector<triangle_t> mapped_tris;
    size_t forward_count = 0;
    size_t reverse_count = 0;
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
	if (tdir.Length() > 0 && bdir.Length() > 0) {
	    if (ON_DotProduct(tdir, bdir) > 0.0)
		forward_count++;
	    else
		reverse_count++;
	}
	mapped_tris.push_back(tri3d);
    }
    const bool reverse_chart = reverse_count > forward_count;
    for (triangle_t &tri3d : mapped_tris) {
	if (reverse_chart)
	    std::swap(tri3d.v[1], tri3d.v[2]);
	tri_add(tri3d);
    }

    /* Collapsing periodic seam copies can turn a valid chart triangle into a
     * zero-area 3-D triangle.  Such triangles are intentionally discarded,
     * but every distinct model-space boundary vertex must still be incident
     * to an exported triangle.  In particular, this prevents a one-pole face
     * from losing its pole through a single degenerate seam ear. */
    const auto boundary_vertex_used = [&](int chart_point) {
	const long native = chart.native_point(chart_point);
	const auto point_3d = p2d3d.find(native);
	if (native < 0 || point_3d == p2d3d.end() || point_3d->second < 0 ||
		(size_t)point_3d->second >= pnts.size())
	    return false;
	const auto mesh_point = p2ind.find(pnts[(size_t)point_3d->second]);
	if (mesh_point == p2ind.end())
	    return false;
	const auto incident = v2tris.find(mesh_point->second);
	return incident != v2tris.end() && !incident->second.empty();
    };
    bool complete_boundary = true;
    for (int point : chart.outer)
	complete_boundary = boundary_vertex_used(point) && complete_boundary;
    for (const std::vector<int> &hole : chart.holes) {
	for (int point : hole)
	    complete_boundary = boundary_vertex_used(point) &&
		complete_boundary;
    }
    if (result && !complete_boundary) {
	struct ON_Brep_CDT_State *state =
	    (struct ON_Brep_CDT_State *)p_cdt;
	if (state)
	    cdt_diagnostic_set(state,
		BREP_CDT_RESULT_CERTIFICATION_FAILED,
		BREP_CDT_STAGE_DETRIA, f_id, 0, 1,
		"3-D seam collapse left a boundary vertex unused");
	return false;
    }

    if (result)
	m_face_charts.push_back(std::move(chart));

    return result;
}

size_t
cdt_mesh_t::refine_collapsed_chart_triangles(size_t max_points)
{
    if (!max_points || !brep || f_id < 0 || f_id >= brep->m_F.Count() ||
	    m_face_charts.empty())
	return 0;
    const ON_Surface *surface = brep->m_F[f_id].SurfaceOf();
    if (!surface)
	return 0;

    /* Group nondegenerate chart triangles by their stitched model-space
     * image.  More than one chart cell in a group means a periodic quotient
     * has hidden the cells behind one coarse triangle. */
    std::map<std::array<long, 3>, std::vector<triangle_t>> images;
    for (const triangle_t &native_triangle : tris_2d) {
	std::array<long, 3> image;
	bool mapped = true;
	for (int corner = 0; corner < 3; ++corner) {
	    const auto point_3d = p2d3d.find(native_triangle.v[corner]);
	    if (point_3d == p2d3d.end() || point_3d->second < 0 ||
		    (size_t)point_3d->second >= pnts.size()) {
		mapped = false;
		break;
	    }
	    const auto mesh_point = p2ind.find(
		pnts[(size_t)point_3d->second]);
	    if (mesh_point == p2ind.end()) {
		mapped = false;
		break;
	    }
	    image[(size_t)corner] = mesh_point->second;
	}
	if (!mapped)
	    continue;
	std::sort(image.begin(), image.end());
	if (image[0] == image[1] || image[1] == image[2])
	    continue;
	images[image].push_back(native_triangle);
    }

    std::set<std::pair<double, double>> existing(m_pnts_2d.begin(),
	m_pnts_2d.end());
    struct ON_Brep_CDT_State *state =
	(struct ON_Brep_CDT_State *)p_cdt;
    size_t inserted = 0;
    for (const auto &image : images) {
	if (image.second.size() < 2)
	    continue;
	for (const triangle_t &native_triangle : image.second) {
	    if (inserted >= max_points)
		return inserted;
	    ON_2dPoint sample = ON_2dPoint::UnsetPoint;
	    bool chart_sample = false;
	    const long native_vertices[3] = {
		native_triangle.v[0], native_triangle.v[1],
		native_triangle.v[2]
	    };
	    for (const cdt_face_chart &chart : m_face_charts) {
		if (chart.triangle_interior_sample(native_vertices, sample)) {
		    chart_sample = true;
		    break;
		}
	    }
	    if (!chart_sample)
		continue;
	    for (int direction = 0; direction < 2; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		const ON_Interval domain = surface->Domain(direction);
		const double period = domain.Length();
		if (!(period > 0.0)) {
		    sample = ON_2dPoint::UnsetPoint;
		    break;
		}
		double &coordinate = direction ? sample.y : sample.x;
		coordinate = domain.Min() + std::fmod(
		    coordinate - domain.Min(), period);
		if (coordinate < domain.Min())
		    coordinate += period;
	    }
	    const std::pair<double, double> sample_key(sample.x, sample.y);
	    if (!sample.IsValid() || !existing.insert(sample_key).second)
		continue;

	    ON_3dPoint point;
	    ON_3dVector normal = ON_3dVector::UnsetVector;
	    if (!surface_EvNormal(surface, sample.x, sample.y, point, normal))
		continue;
	    if (m_bRev)
		normal = -normal;
	    const long point_2d = add_point(sample);
	    m_interior_pnts.insert(point_2d);
	    m_chart_refinement_pnts.insert(point_2d);
	    const long point_3d = add_point(new ON_3dPoint(point));
	    const long normal_3d = add_normal(new ON_3dPoint(normal));
	    p2d3d[point_2d] = point_3d;
	    nmap[point_3d] = normal_3d;
	    if (state) {
		CDT_Add3DPnt(state, pnts[(size_t)point_3d], f_id, -1, -1,
		    -1, sample.x, sample.y);
		CDT_Add3DNorm(state, normals[(size_t)normal_3d],
		    pnts[(size_t)point_3d], f_id, -1, -1, -1,
		    sample.x, sample.y);
	    }
	    inserted++;
	}
    }
    return inserted;
}

size_t
cdt_mesh_t::split_problem_triangle_edges(
	const std::vector<triangle_t> &triangles, size_t max_points,
	const ON_3dPoint *near_point)
{
    if (!max_points || !brep || f_id < 0 || f_id >= brep->m_F.Count() ||
	    m_face_charts.empty())
	return 0;
    const ON_Surface *surface = brep->m_F[f_id].SurfaceOf();
    if (!surface)
	return 0;
    boundary_edges_update();
    std::vector<triangle_t> targets = triangles;
    std::sort(targets.begin(), targets.end(), [](const triangle_t &first,
	    const triangle_t &second) { return first.ind < second.ind; });
    std::set<std::pair<double, double>> existing(m_pnts_2d.begin(),
	m_pnts_2d.end());
    struct ON_Brep_CDT_State *state =
	(struct ON_Brep_CDT_State *)p_cdt;
    size_t inserted = 0;
    const auto mesh_vertex = [&](long native_point) {
	const auto point_3d = p2d3d.find(native_point);
	if (point_3d == p2d3d.end() || point_3d->second < 0 ||
		(size_t)point_3d->second >= pnts.size())
	    return -1L;
	const auto canonical = p2ind.find(pnts[(size_t)point_3d->second]);
	return canonical == p2ind.end() ? -1L : canonical->second;
    };

    for (const triangle_t &target : targets) {
	if (inserted >= max_points || !tri_active(target.ind))
	    break;
	const triangle_t triangle = tris_vect[target.ind];
	std::vector<std::pair<double, uedge_t>> edges;
	for (int edge = 0; edge < 3; ++edge) {
	    uedge_t candidate(triangle.v[edge],
		triangle.v[(edge + 1) % 3]);
	    if (boundary_edges.find(candidate) != boundary_edges.end() ||
		    brep_edges.find(candidate) != brep_edges.end())
		continue;
	    const auto incident = uedges2tris.find(candidate);
	    if (incident == uedges2tris.end() || incident->second.size() != 2)
		continue;
	    double priority = pnts[(size_t)candidate.v[0]]->DistanceTo(
		*pnts[(size_t)candidate.v[1]]);
	    if (near_point) {
		ON_3dPoint target_point = *near_point;
		priority = uedge_dist(candidate, target_point);
	    }
	    edges.push_back(std::make_pair(priority, candidate));
	}
	std::sort(edges.begin(), edges.end(), [near_point](const auto &first,
		const auto &second) {
	    return near_point ? first.first < second.first :
		first.first > second.first;
	});
	for (const auto &edge_entry : edges) {
	    uedge_t edge = edge_entry.second;
	    long native_edge[2] = {-1, -1};
	    std::vector<long> native_candidates[2];
	    for (int endpoint = 0; endpoint < 2; ++endpoint) {
		for (const auto &mapping : p2d3d) {
		    if (mesh_vertex(mapping.first) == edge.v[endpoint])
			native_candidates[endpoint].push_back(mapping.first);
		}
	    }
	    if (native_candidates[0].empty() || native_candidates[1].empty())
		continue;
	    ON_2dPoint sample;
	    ON_2dPoint chart_sample;
	    cdt_face_chart *active_chart = NULL;
	    for (cdt_face_chart &chart : m_face_charts) {
		for (long first : native_candidates[0]) {
		    for (long second : native_candidates[1]) {
			long candidate[2] = {first, second};
			if (!chart.edge_midpoint_sample(candidate, sample,
				chart_sample))
			    continue;
			native_edge[0] = first;
			native_edge[1] = second;
			active_chart = &chart;
			break;
		    }
		    if (active_chart)
			break;
		}
		if (active_chart)
		    break;
	    }
	    if (!active_chart)
		continue;
	    for (int direction = 0; direction < 2; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		const ON_Interval domain = surface->Domain(direction);
		const double period = domain.Length();
		double &coordinate = direction ? sample.y : sample.x;
		coordinate = domain.Min() + std::fmod(
		    coordinate - domain.Min(), period);
		if (coordinate < domain.Min())
		    coordinate += period;
	    }
	    const std::pair<double, double> sample_key(sample.x, sample.y);
	    if (!sample.IsValid() || !existing.insert(sample_key).second)
		continue;

	    const auto incident = uedges2tris.find(edge);
	    if (incident == uedges2tris.end() || incident->second.size() != 2)
		continue;
	    std::vector<triangle_t> old_triangles;
	    std::vector<triangle_t> old_native_triangles;
	    std::set<size_t> used_native_triangles;
	    uedge_t native_split(native_edge[0], native_edge[1]);
	    bool complete = true;
	    for (size_t triangle_index : incident->second) {
		if (!tri_active(triangle_index)) {
		    complete = false;
		    break;
		}
		const triangle_t old_triangle = tris_vect[triangle_index];
		long sorted_wanted[3] = {old_triangle.v[0],
		    old_triangle.v[1], old_triangle.v[2]};
		std::sort(sorted_wanted, sorted_wanted + 3);
		bool found = false;
		for (size_t native_index = 0; native_index < tris_2d.size();
			native_index++) {
		    if (used_native_triangles.find(native_index) !=
			    used_native_triangles.end())
			continue;
		    const triangle_t &native_triangle = tris_2d[native_index];
		    const std::set<uedge_t> native_edges = {
			uedge_t(native_triangle.v[0], native_triangle.v[1]),
			uedge_t(native_triangle.v[1], native_triangle.v[2]),
			uedge_t(native_triangle.v[2], native_triangle.v[0])
		    };
		    if (native_edges.find(native_split) == native_edges.end())
			continue;
		    long candidate[3] = {
			mesh_vertex(native_triangle.v[0]),
			mesh_vertex(native_triangle.v[1]),
			mesh_vertex(native_triangle.v[2])
		    };
		    if (candidate[0] < 0 || candidate[1] < 0 ||
			    candidate[2] < 0)
			continue;
		    std::sort(candidate, candidate + 3);
		    if (!std::equal(candidate, candidate + 3,
			    sorted_wanted))
			continue;
		    old_triangles.push_back(old_triangle);
		    old_native_triangles.push_back(native_triangle);
		    used_native_triangles.insert(native_index);
		    found = true;
		    break;
		}
		if (!found) {
		    complete = false;
		    break;
		}
	    }
	    if (!complete || old_triangles.size() != 2)
		continue;

	    ON_3dPoint point;
	    ON_3dVector normal = ON_3dVector::UnsetVector;
	    if (!surface_EvNormal(surface, sample.x, sample.y, point, normal))
		continue;
	    if (m_bRev)
		normal = -normal;
	    const long point_2d = add_point(sample);
	    const long point_3d = add_point(new ON_3dPoint(point));
	    const long normal_3d = add_normal(new ON_3dPoint(normal));
	    p2d3d[point_2d] = point_3d;
	    p3d2d[point_3d] = point_2d;
	    nmap[point_3d] = normal_3d;
	    m_interior_pnts.insert(point_2d);
	    m_chart_refinement_pnts.insert(point_2d);
	    active_chart->add_refinement_point(point_2d, sample,
		chart_sample, native_edge);
	    if (state) {
		CDT_Add3DPnt(state, pnts[(size_t)point_3d], f_id, -1, -1,
		    -1, sample.x, sample.y);
		CDT_Add3DNorm(state, normals[(size_t)normal_3d],
		    pnts[(size_t)point_3d], f_id, -1, -1, -1,
		    sample.x, sample.y);
	    }

	    const long mesh_point = p2ind[pnts[(size_t)point_3d]];
	    for (size_t i = 0; i < old_triangles.size(); ++i) {
		tri_remove(old_triangles[i]);
		std::set<triangle_t> replacements = old_triangles[i].split(
		    edge, mesh_point, false);
		for (triangle_t replacement : replacements)
		    tri_add(replacement);

		long wanted[3] = {old_native_triangles[i].v[0],
		    old_native_triangles[i].v[1],
		    old_native_triangles[i].v[2]};
		std::sort(wanted, wanted + 3);
		for (auto old = tris_2d.begin(); old != tris_2d.end(); ++old) {
		    long candidate[3] = {old->v[0], old->v[1], old->v[2]};
		    std::sort(candidate, candidate + 3);
		    if (std::equal(candidate, candidate + 3, wanted)) {
			tris_2d.erase(old);
			break;
		    }
		}
		std::set<triangle_t> native_replacements =
		    old_native_triangles[i].split(native_split, point_2d,
			false);
		tris_2d.insert(tris_2d.end(), native_replacements.begin(),
		    native_replacements.end());
	    }
	    inserted++;
	    break;
	}
    }
    return inserted;
}

size_t
cdt_mesh_t::refine_problem_triangles(
	const std::vector<triangle_t> &triangles, size_t max_points)
{
    if (!max_points || !brep || f_id < 0 || f_id >= brep->m_F.Count())
	return 0;
    const ON_Surface *surface = brep->m_F[f_id].SurfaceOf();

    if (!surface)
	return 0;

    std::vector<triangle_t> targets = triangles;
    std::sort(targets.begin(), targets.end(), [](const triangle_t &first,
	    const triangle_t &second) { return first.ind < second.ind; });
    std::set<std::pair<double, double>> existing(m_pnts_2d.begin(),
	m_pnts_2d.end());
    struct ON_Brep_CDT_State *state =
	(struct ON_Brep_CDT_State *)p_cdt;
    double surface_size[2] = {1.0, 1.0};
    if (!surface->GetSurfaceSize(&surface_size[0], &surface_size[1])) {
	surface_size[0] = 1.0;
	surface_size[1] = 1.0;
    }
    double metric_scale[2] = {1.0, 1.0};
    for (int direction = 0; direction < 2; ++direction) {
	const double domain_length = surface->Domain(direction).Length();
	if (domain_length > 0.0 && surface_size[direction] > 0.0)
	    metric_scale[direction] = surface_size[direction] /
		domain_length;
    }
    size_t inserted = 0;

    for (const triangle_t &triangle : targets) {
	if (inserted >= max_points)
	    break;
	ON_2dPoint uv[3];
	bool mapped = true;
	triangle_t native_triangle;
	bool have_native_triangle = false;
	long target_vertices[3] = {
	    triangle.v[0], triangle.v[1], triangle.v[2]
	};
	std::sort(target_vertices, target_vertices + 3);
	for (const triangle_t &candidate : tris_2d) {
	    long candidate_vertices[3] = {-1, -1, -1};
	    int candidate_count = 0;
	    for (int corner = 0; corner < 3; ++corner) {
		const auto point_3d = p2d3d.find(candidate.v[corner]);
		if (point_3d == p2d3d.end() || point_3d->second < 0 ||
			(size_t)point_3d->second >= pnts.size())
		    break;
		const auto canonical = p2ind.find(
		    pnts[(size_t)point_3d->second]);
		if (canonical == p2ind.end())
		    break;
		candidate_vertices[candidate_count++] = canonical->second;
	    }
	    if (candidate_count != 3)
		continue;
	    std::sort(candidate_vertices, candidate_vertices + 3);
	    if (std::equal(candidate_vertices, candidate_vertices + 3,
		    target_vertices)) {
		native_triangle = candidate;
		have_native_triangle = true;
		break;
	    }
	}
	if (have_native_triangle) {
	    for (int corner = 0; corner < 3; ++corner) {
		const long native = native_triangle.v[corner];
		if (native < 0 || (size_t)native >= m_pnts_2d.size()) {
		    mapped = false;
		    continue;
		}
		uv[corner] = ON_2dPoint(
		    m_pnts_2d[(size_t)native].first,
		    m_pnts_2d[(size_t)native].second);
	    }
	} else {
	    for (int corner = 0; corner < 3; ++corner) {
		const long vertex = triangle.v[corner];
		const auto native = p3d2d.find(vertex);
		if (native == p3d2d.end() || ambiguous_p3d2d.find(vertex) !=
			ambiguous_p3d2d.end() || native->second < 0 ||
			(size_t)native->second >= m_pnts_2d.size()) {
		    mapped = false;
		    continue;
		}
		uv[corner] = ON_2dPoint(
		    m_pnts_2d[(size_t)native->second].first,
		    m_pnts_2d[(size_t)native->second].second);
	    }
	}
	if (!mapped)
	    continue;
	/* Work in a continuous local image of a periodic domain.  The native
	 * parameters on opposite sides of a seam may be almost a full period
	 * apart even though their surface points are close. */
	for (int direction = 0; direction < 2; ++direction) {
	    if (!surface->IsClosed(direction))
		continue;
	    const double period = surface->Domain(direction).Length();
	    if (!(period > 0.0) || !std::isfinite(period)) {
		mapped = false;
		break;
	    }
	    const double reference = direction ? uv[0].y : uv[0].x;
	    for (int corner = 1; corner < 3; ++corner) {
		double &coordinate = direction ? uv[corner].y : uv[corner].x;
		while (coordinate - reference > 0.5 * period)
		    coordinate -= period;
		while (coordinate - reference < -0.5 * period)
		    coordinate += period;
	    }
	}
	if (!mapped)
	    continue;
	/* Split the folded triangle from a point strictly inside it.  Use the
	 * active triangulation chart when available: a convex combination in
	 * native UV need not remain inside a nonlinear polar chart triangle. */
	ON_2dPoint sample;
	bool chart_sample = false;
	if (have_native_triangle) {
	    const long native_vertices[3] = {
		native_triangle.v[0], native_triangle.v[1],
		native_triangle.v[2]
	    };
	    for (const cdt_face_chart &chart : m_face_charts) {
		if (chart.triangle_interior_sample(native_vertices, sample)) {
		    chart_sample = true;
		    break;
		}
	    }
	}
	if (!chart_sample) {
	    double opposite_length[3] = {0.0, 0.0, 0.0};
	    double weight_sum = 0.0;
	    for (int vertex = 0; vertex < 3; ++vertex) {
		const int first = (vertex + 1) % 3;
		const int second = (vertex + 2) % 3;
		const double du = (uv[second].x - uv[first].x) *
		    metric_scale[0];
		const double dv = (uv[second].y - uv[first].y) *
		    metric_scale[1];
		opposite_length[vertex] = std::sqrt(du * du + dv * dv);
		weight_sum += opposite_length[vertex];
	    }
	    if (weight_sum > 0.0 && std::isfinite(weight_sum)) {
		sample = ON_2dPoint(
		    (opposite_length[0] * uv[0].x +
		     opposite_length[1] * uv[1].x +
		     opposite_length[2] * uv[2].x) / weight_sum,
		    (opposite_length[0] * uv[0].y +
		     opposite_length[1] * uv[1].y +
		     opposite_length[2] * uv[2].y) / weight_sum);
	    } else {
		sample = ON_2dPoint(
		    (uv[0].x + uv[1].x + uv[2].x) / 3.0,
		    (uv[0].y + uv[1].y + uv[2].y) / 3.0);
	    }
	}
	/* Surface evaluation and the CDT point set use the canonical native
	 * domain, so fold the locally unwrapped sample back into that domain. */
	for (int direction = 0; direction < 2; ++direction) {
	    if (!surface->IsClosed(direction))
		continue;
	    const ON_Interval domain = surface->Domain(direction);
	    const double period = domain.Length();
	    double &coordinate = direction ? sample.y : sample.x;
	    coordinate = domain.Min() + std::fmod(coordinate - domain.Min(),
		period);
	    if (coordinate < domain.Min())
		coordinate += period;
	}
	const std::pair<double, double> sample_key(sample.x, sample.y);
	if (!sample.IsValid() || !existing.insert(sample_key).second)
	    continue;

	ON_3dPoint point;
	ON_3dVector normal = ON_3dVector::UnsetVector;
	if (!surface_EvNormal(surface, sample.x, sample.y, point, normal))
	    continue;
	if (m_bRev)
	    normal = -normal;
	const long point_2d = add_point(sample);
	m_interior_pnts.insert(point_2d);
	m_chart_refinement_pnts.insert(point_2d);
	const long point_index = add_point(new ON_3dPoint(point));
	const long normal_index = add_normal(new ON_3dPoint(normal));
	p2d3d[point_2d] = point_index;
	nmap[point_index] = normal_index;
	if (state) {
	    CDT_Add3DPnt(state, pnts[(size_t)point_index], f_id, -1, -1,
		-1, sample.x, sample.y);
	    CDT_Add3DNorm(state, normals[(size_t)normal_index],
		pnts[(size_t)point_index], f_id, -1, -1, -1,
		sample.x, sample.y);
	}
	inserted++;
    }
    return inserted;
}

size_t
cdt_mesh_t::refine_incorrect_normals(size_t max_points)
{
    if (!brep || f_id < 0 || f_id >= brep->m_F.Count())
	return 0;
    const ON_Surface *surface = brep->m_F[f_id].SurfaceOf();
    if (!surface)
	return 0;
    return refine_problem_triangles(interior_incorrect_normals(), max_points);
}

size_t
cdt_mesh_t::refine_self_intersections(size_t max_points)
{
    std::vector<triangle_t> problematic;
    self_intersections(&problematic, std::max((size_t)1, max_points));
    return refine_problem_triangles(problematic, max_points);
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
    // triangles.  This is a quality refinement, not a topology repair.  Keep
    // it transactional: process_seed_tri may have replaced several patches
    // before a later singular seed proves unmeshable.  Returning with those
    // partial mutations used to leak isolated triangles into an otherwise
    // valid face mesh (notably the NIST MBE PMI 6 spherical cap).
    boundary_edges_stale = true;
    boundary_edges_update();
    const bool pre_singularity_mesh_valid = problem_edges.empty();
    std::vector<triangle_t> pre_singularity_triangle_store;
    std::vector<size_t> pre_singularity_active_triangles;
    decltype(v2edges) pre_singularity_v2edges;
    decltype(v2tris) pre_singularity_v2tris;
    decltype(edges2tris) pre_singularity_edges2tris;
    decltype(uedges2tris) pre_singularity_uedges2tris;
    decltype(boundary_edges) pre_singularity_boundary_edges;
    decltype(problem_edges) pre_singularity_problem_edges;
    if (pre_singularity_mesh_valid && has_singularities) {
	/* Preserve the complete indexed mesh state rather than rebuilding it
	 * through tri_add.  Coincident singularity triangles may intentionally
	 * have the same three 3-D vertex indices but different orientations;
	 * tri_add's duplicate filter would discard one and make rollback itself
	 * non-transactional. */
	pre_singularity_triangle_store = tris_vect;
	pre_singularity_v2edges = v2edges;
	pre_singularity_v2tris = v2tris;
	pre_singularity_edges2tris = edges2tris;
	pre_singularity_uedges2tris = uedges2tris;
	pre_singularity_boundary_edges = boundary_edges;
	pre_singularity_problem_edges = problem_edges;
	RTree<size_t, double, 3>::Iterator snapshot_it;
	tris_tree.GetFirst(snapshot_it);
	while (!snapshot_it.IsNull()) {
	    pre_singularity_active_triangles.push_back(*snapshot_it);
	    ++snapshot_it;
	}
    }
    const auto restore_pre_singularity_mesh = [&]() {
	if (!pre_singularity_mesh_valid ||
		pre_singularity_triangle_store.empty() ||
		pre_singularity_active_triangles.empty())
	    return false;
	tris_vect = pre_singularity_triangle_store;
	tris_tree.RemoveAll();
	for (size_t triangle_index : pre_singularity_active_triangles) {
	    if (triangle_index >= tris_vect.size()) return false;
	    triangle_t &triangle = tris_vect[triangle_index];
	    triangle.m = this;
	    ON_3dPoint *point = pnts[triangle.v[0]];
	    ON_BoundingBox bounds(*point, *point);
	    for (int vertex = 1; vertex < 3; ++vertex) {
		point = pnts[triangle.v[vertex]];
		bounds.Set(*point, true);
	    }
	    const double minimum[3] = {bounds.Min().x, bounds.Min().y,
		bounds.Min().z};
	    const double maximum[3] = {bounds.Max().x, bounds.Max().y,
		bounds.Max().z};
	    tris_tree.Insert(minimum, maximum, triangle_index);
	}
	v2edges = pre_singularity_v2edges;
	v2tris = pre_singularity_v2tris;
	edges2tris = pre_singularity_edges2tris;
	uedges2tris = pre_singularity_uedges2tris;
	boundary_edges = pre_singularity_boundary_edges;
	problem_edges = pre_singularity_problem_edges;
	seed_tris.clear();
	new_tris.clear();
	boundary_edges_stale = false;
	bounding_box_stale = true;
	return true;
    };

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
		    if (restore_pre_singularity_mesh()) {
			bu_log("Face %d: retained the valid pre-refinement mesh after singularity quality remeshing made no progress\n",
			    f_id);
			return true;
		    }
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
    bool topret = true;

    const size_t intersections = self_intersections(NULL, 1);
    const bool iret = intersections == 0;
    if (!iret && verbose > 0) {
	std::cout << name << " face " << f_id
	    << ": nonadjacent triangles intersect in mesh\n";
    }

    const bool topology_chart =
	cdt_face_uses_topology_chart(brep->m_F[f_id]);

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
	const double normal_dot = ON_DotProduct(tdir, bdir);
	const bool invalid_normal = (topology_chart ?
	    !(normal_dot > 0.0) : normal_dot < 0.1) &&
	    !toleranced_boundary_triangle(tri);
	if (tdir.Length() > 0 && bdir.Length() > 0 && invalid_normal) {
	    if (verbose > 0) {
		std::cout << name << " face " << f_id
		    << ": invalid normals in mesh, triangle " << tri.ind
		    << " (" << tri.v[0] << "," << tri.v[1] << ","
		    << tri.v[2] << "), dot=" << normal_dot << "\n";
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
		if (verbose > 0)
		    std::cout << "face " << f_id << ": unclassified mesh edge "
			<< ue[ind].v[0] << "-" << ue[ind].v[1] << " has "
			<< uedges2tris[ue[ind]].size()
			<< " incident triangles\n";
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

    return (nret && eret && topret && iret);
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

    sfile << "TRIANGLES_VECT " << tris_vect.size() << "\n";
    std::vector<triangle_t>::iterator t_it;
    for (t_it = tris_vect.begin(); t_it != tris_vect.end(); t_it++) {
	sfile << (*t_it).v[0] << "," << (*t_it).v[1] << "," << (*t_it).v[2] << "," << (*t_it).ind << "\n";
    }

    sfile << "TRIANGLES_TREE " << tris_tree.Count() << "\n";
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
	} else if (switch_line == std::string("V2")) {
	    version = 2;
	}
    }
    if (version < 1 || version > 2) {
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
	size_t spos = switch_line.find_first_of(' ');
	std::string dtype;
	std::string count_text;
	if (spos != std::string::npos) {
	    dtype = switch_line.substr(0, spos);
	    count_text = switch_line.substr(spos + 1);
	} else if (switch_line.compare(0, 14, "TRIANGLES_VECT") == 0) {
	    /* V2 snapshots written before the delimiter fix concatenated these
	     * two record names and their counts.  Accept those diagnostics so
	     * existing failure captures remain replayable. */
	    dtype = "TRIANGLES_VECT";
	    count_text = switch_line.substr(14);
	} else if (switch_line.compare(0, 14, "TRIANGLES_TREE") == 0) {
	    dtype = "TRIANGLES_TREE";
	    count_text = switch_line.substr(14);
	} else {
	    std::cerr << "Malformed serialization record: " << switch_line
		<< "\nSerialization import failed.\n";
	    return false;
	}
	long lcnt = std::stol(count_text);

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

	if (dtype == std::string("TRIANGLES") ||
		dtype == std::string("TRIANGLES_VECT")) {
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
		if (dtype == std::string("TRIANGLES")) {
		    /* V1 stored only active triangles.  Rebuild the spatial index and
		     * adjacency maps through the normal insertion path. */
		    tri.m = this;
		    tri_add(tri);
		} else {
		    /* V2 stores inactive vector entries as well; its following tree
		     * record identifies and indexes the active subset. */
		    tri.ind = tris_vect.size();
		    tris_vect.push_back(tri);
		}
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
    // distinct vertices without a closing repeat.  polygon->poly is a set of
    // pointers, so its begin() depends on heap layout.  The choice of starting
    // edge rotates the convex LSCM boundary and can change the triangulation
    // of difficult patches.  Choose the lexicographically first topological
    // edge instead, making the result independent of allocation order and
    // unrelated inputs such as the requested output object name.
    std::vector<int32_t> bnd_loop;
    {
	cpolyedge_t *pe = polygon->first_edge();
	if (!pe)
	    return false;
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

/* Exercise the allocator-independent polygon start used by the actual CDT
 * call.  A square is deliberately co-circular, so rotating its boundary input
 * can select the opposite valid diagonal unless the triangulator receives a
 * stable starting vertex.  Keep this compact guard here because cpolygon_t is
 * an internal type whose symbols are not exported from libbrep. */
int
cdt_test_boundary_start(void)
{
    const auto initialize_regular_polygon = [](cpolygon_t &polygon,
	    int point_count, int first_allocated_edge) {
	for (int point = 0; point < point_count; ++point) {
	    const double angle = 2.0 * ON_PI * point / point_count;
	    ON_2dPoint p(cos(angle), sin(angle));
	    polygon.add_point(p, point);
	}
	for (int offset = 0; offset < point_count; ++offset) {
	    const int start = (first_allocated_edge + offset) % point_count;
	    edge2d_t edge(start, (start + 1) % point_count);
	    polygon.add_ordered_edge(edge);
	}
    };
    const auto canonical_triangles = [](const cpolygon_t &polygon) {
	std::set<std::vector<long> > result;
	for (std::set<triangle_t>::const_iterator triangle =
		polygon.tris.begin(); triangle != polygon.tris.end(); ++triangle) {
	    std::vector<long> vertices;
	    vertices.push_back(triangle->v[0]);
	    vertices.push_back(triangle->v[1]);
	    vertices.push_back(triangle->v[2]);
	    std::sort(vertices.begin(), vertices.end());
	    result.insert(vertices);
	}
	return result;
    };
    const auto release_edges = [](cpolygon_t &polygon) {
	for (std::set<cpolyedge_t *>::iterator edge = polygon.poly.begin();
		edge != polygon.poly.end(); ++edge)
	    delete *edge;
	polygon.poly.clear();
    };

    for (int point_count = 4; point_count <= 12; ++point_count) {
	for (int first_allocated_edge = 1;
		first_allocated_edge < point_count; ++first_allocated_edge) {
	    cpolygon_t edge_zero_first;
	    cpolygon_t rotated_first;
	    initialize_regular_polygon(edge_zero_first, point_count, 0);
	    initialize_regular_polygon(rotated_first, point_count,
		first_allocated_edge);
	    for (cpolyedge_t *edge : edge_zero_first.poly) {
		if (!edge || edge->trim_ind != -1 || edge->loop_type != 0 ||
			edge->defines_spnt || edge->split_status != 0 ||
			edge->eseg != NULL) {
		    release_edges(edge_zero_first);
		    release_edges(rotated_first);
		    return 1;
		}
	    }
	    const bool valid = edge_zero_first.cdt() && rotated_first.cdt();
	    const std::set<std::vector<long> > first =
		canonical_triangles(edge_zero_first);
	    const std::set<std::vector<long> > rotated =
		canonical_triangles(rotated_first);
	    release_edges(edge_zero_first);
	    release_edges(rotated_first);
	    if (!valid || edge_zero_first.last_cdt_start_vertex != 0 ||
		    rotated_first.last_cdt_start_vertex != 0 ||
		    first.size() != static_cast<size_t>(point_count - 2) ||
		    first != rotated)
		return 1;
	}
    }
    return 0;
}

int
cdt_test_boundary_steiner_filter(void)
{
    point2d_t points[6] = {
	{0.0, 0.0}, {2.0, 0.0}, {2.0, 2.0}, {0.0, 2.0},
	{1.0, 0.0}, {1.0, 1.0}
    };
    const int polygon[] = {0, 1, 2, 3, 0};
    const double tolerance = sqrt(DBL_EPSILON) * sqrt(8.0);
    if (!point_on_polygon_boundary(points, 4, polygon, 5,
	    tolerance * tolerance))
	return 1;
    if (point_on_polygon_boundary(points, 5, polygon, 5,
	    tolerance * tolerance))
	return 1;
    return 0;
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
    return m->i->fmesh.deserialize(fname) ? 0 : -1;
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
