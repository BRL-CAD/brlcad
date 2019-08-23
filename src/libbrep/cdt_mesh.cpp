// This evolved from the original trimesh halfedge data structure code:

// Author: Yotam Gingold <yotam (strudel) yotamgingold.com>
// License: Public Domain.  (I, Yotam Gingold, the author, release this code into the public domain.)
// GitHub: https://github.com/yig/halfedge
//
// It ended up rather different, as we are concerned with somewhat different
// mesh properties for CDT and the halfedge data structure didn't end up being
// a good fit, but as it's derived from that source the public domain license
// is maintained for the changes as well.

#include "common.h"

#include "bu/color.h"
#include "bu/log.h"
#include "bu/sort.h"
#include "bu/malloc.h"
#include "bn/mat.h" /* bn_vec_perp */
#include "bn/plot3.h"
#include "bn/plane.h" /* bn_fit_plane */
#include "bg/polygon.h"
#include "brep.h"
#include "./cdt_mesh.h"

// needed for implementation
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <limits>
#include <stack>


namespace cdt_mesh
{

void
plot_pnt_3d(FILE *plot_file, ON_3dPoint *p, double r, int dir)
{
    point_t origin, bnp;
    VSET(origin, p->x, p->y, p->z);
    pdv_3move(plot_file, origin);

    if (dir == 0) {
	VSET(bnp, p->x+r, p->y, p->z);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x-r, p->y, p->z);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x, p->y+r, p->z);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x, p->y-r, p->z);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x, p->y, p->z+r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x, p->y, p->z-r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
    }
    if (dir == 1) {
	VSET(bnp, p->x+r, p->y+r, p->z+r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x+r, p->y-r, p->z+r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x-r, p->y+r, p->z+r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x-r, p->y-r, p->z+r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);

	VSET(bnp, p->x+r, p->y+r, p->z-r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x+r, p->y-r, p->z-r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x-r, p->y+r, p->z-r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, p->x-r, p->y-r, p->z-r);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
    }
}

void
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


void
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
    if (polygon->pnts_type == 1) {
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

	return;
    }

    // Polygon indicates we're using 3D point indices - go with what the indices and mappings define
    ON_3dPoint *t_s = polygon->cdt_mesh->pnts[polygon->p2o[v[0]]];
    ON_3dPoint *t_e = polygon->cdt_mesh->pnts[polygon->p2o[v[1]]];
    ON_3dPoint *t_sn = polygon->cdt_mesh->n3d(polygon->cdt_mesh->nmap[polygon->p2o[v[0]]]);
    ON_3dPoint *t_en = polygon->cdt_mesh->n3d(polygon->cdt_mesh->nmap[polygon->p2o[v[1]]]);
    double slen = t_s->DistanceTo(*t_e);
    ON_3dVector t_sv(*t_sn);
    ON_3dVector t_ev(*t_en);

    FILE* plot_file = fopen(fname, "w");
    pl_color(plot_file, 0, 0, 254);
    plot_seg_3d(plot_file, t_s, t_e);

    pl_color(plot_file, 254, 0, 0);
    plot_pnt_3d(plot_file, t_s, 0.05*slen, 1);
    plot_vec_3d(plot_file, t_s, &t_sv, 0.2*slen);
    pl_color(plot_file, 0, 254, 0);
    plot_pnt_3d(plot_file, t_e, 0.05*slen, 1);
    plot_vec_3d(plot_file, t_e, &t_ev, 0.2*slen);

    fclose(plot_file);
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

/***************************/
/* CPolygon implementation */
/***************************/

long
cpolygon_t::add_point(ON_2dPoint &on_2dp)
{
    std::pair<double, double> proj_2d;
    proj_2d.first = on_2dp.x;
    proj_2d.second = on_2dp.y;
    pnts_2d.push_back(proj_2d);
    p2ind[proj_2d] = pnts_2d.size() - 1;
    return (long)(pnts_2d.size() - 1);
}

cpolyedge_t *
cpolygon_t::add_edge(const struct uedge_t &ue)
{
    if (ue.v[0] == -1) return NULL;


    int v1 = -1;
    int v2 = -1;

    std::set<cpolyedge_t *>::iterator cp_it;
    for (cp_it = poly.begin(); cp_it != poly.end(); cp_it++) {
	cpolyedge_t *pe = *cp_it;

	if (pe->v[1] == ue.v[0]) {
	    v1 = ue.v[0];
	}

	if (pe->v[1] == ue.v[1]) {
	    v1 = ue.v[1];
	}

	if (pe->v[0] == ue.v[0]) {
	    v2 = ue.v[0];
	}

	if (pe->v[0] == ue.v[1]) {
	    v2 = ue.v[1];
	}
    }

    if (v1 == -1) {
	v1 = (ue.v[0] == v2) ? ue.v[1] : ue.v[0];
    }

    if (v2 == -1) {
	v2 = (ue.v[0] == v1) ? ue.v[1] : ue.v[0];
    }

    struct edge_t le(v1, v2);
    cpolyedge_t *nedge = new cpolyedge_t(le);
    poly.insert(nedge);

    nedge->polygon = this;

    v2pe[v1].insert(nedge);
    v2pe[v2].insert(nedge);
    active_edges.insert(uedge_t(le));
    used_verts.insert(v1);
    used_verts.insert(v2);

    cpolyedge_t *prev = NULL;
    cpolyedge_t *next = NULL;

    for (cp_it = poly.begin(); cp_it != poly.end(); cp_it++) {
	cpolyedge_t *pe = *cp_it;

	if (pe == nedge) continue;

	if (pe->v[1] == nedge->v[0]) {
	    prev = pe;
	}

	if (pe->v[0] == nedge->v[1]) {
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
cpolygon_t::remove_edge(const struct uedge_t &ue)
{
    cpolyedge_t *cull = NULL;
    std::set<cpolyedge_t *>::iterator cp_it;
    for (cp_it = poly.begin(); cp_it != poly.end(); cp_it++) {
	cpolyedge_t *pe = *cp_it;
	struct uedge_t pue(pe->v[0], pe->v[1]);
	if (ue == pue) {
	    // Existing segment with this ending vertex exists
	    cull = pe;
	    break;
	}
    }

    if (!cull) return;

    v2pe[ue.v[0]].erase(cull);
    v2pe[ue.v[1]].erase(cull);
    active_edges.erase(ue);

    // An edge removal may produce a new interior point candidate - check
    // Will need to verify eventually with point_in_polygon, but topologically
    // it may be cut loose
    for (int i = 0; i < 2; i++) {
	if (!v2pe[ue.v[i]].size()) {
	    if (flipped_face.find(ue.v[i]) != flipped_face.end()) {
		flipped_face.erase(ue.v[i]);
	    }
	    uncontained.insert(ue.v[i]);
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
	remove_edge(*e_it);
    }
    for (e_it = new_edges.begin(); e_it != new_edges.end(); e_it++) {
	cpolyedge_t *ne = add_edge(*e_it);
	nedges.insert(ne);
    }

    return nedges;
}

long
cpolygon_t::shared_edge_cnt(triangle_t &t)
{
    struct uedge_t ue[3];
    ue[0].set(t.v[0], t.v[1]);
    ue[1].set(t.v[1], t.v[2]);
    ue[2].set(t.v[2], t.v[0]);
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
	if (v2pe.find(t.v[i]) == v2pe.end()) {
	    return t.v[i];
	}
	// TODO - need to check C++ map container behavior - if the set
	// a key points to is fully cleared, will the above test work?
	// (if it finds the empty set successfully it doesn't do what
	// we need...)
	if (v2pe[t.v[i]].size() == 0) {
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
	if (v2pe.find(t.v[i]) != v2pe.end()) {
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
	vecnt[pe->v[0]]++;
	vecnt[pe->v[1]]++;
    }
    std::map<long, int>::iterator v_it;
    for (v_it = vecnt.begin(); v_it != vecnt.end(); v_it++) {
	if (v_it->second > 2) {
	    self_isect = true;
	    if (!cdt_mesh->brep_edge_pnt(v_it->second)) {
		uncontained.insert(v_it->second);
	    }
	}
    }

    // Check the projected segments against each other as well.  Store any
    // self-intersecting edges for use in later repair attempts.
    std::vector<cpolyedge_t *> pv(poly.begin(), poly.end());
    for (size_t i = 0; i < pv.size(); i++) {
	cpolyedge_t *pe1 = pv[i];
	ON_2dPoint p1_1(pnts_2d[pe1->v[0]].first, pnts_2d[pe1->v[0]].second);
	ON_2dPoint p1_2(pnts_2d[pe1->v[1]].first, pnts_2d[pe1->v[1]].second);
	struct uedge_t ue1(pe1->v[0], pe1->v[1]);
	// if we already know this segment intersects at least one other segment, we
	// don't need to re-test it - it's already "active"
	if (self_isect_edges.find(ue1) != self_isect_edges.end()) continue;
	ON_BoundingBox e1b(p1_1, p1_2);
	ON_Line e1(p1_1, p1_2);
	for (size_t j = i+1; j < pv.size(); j++) {
	    cpolyedge_t *pe2 = pv[j];
	    ON_2dPoint p2_1(pnts_2d[pe2->v[0]].first, pnts_2d[pe2->v[0]].second);
	    ON_2dPoint p2_2(pnts_2d[pe2->v[1]].first, pnts_2d[pe2->v[1]].second);
	    struct uedge_t ue2(pe2->v[0], pe2->v[1]);
	    ON_BoundingBox e2b(p2_1, p2_2);
	    ON_Line e2(p2_1, p2_2);

	    if (e1b.IsDisjoint(e2b)) {
		continue;
	    }

	    double a, b = 0;
	    if (!ON_IntersectLineLine(e1, e2, &a, &b, 0.0, false)) {
		continue;
	    }

#if 0
	    // Weed out nearly parallel lines
	    double angle_tolerance_radians = 0.5*ON_PI/180.0;
	    double parallel_tol = cos(angle_tolerance_radians);
	    if ( fabs(e1.Tangent()*e2.Tangent()) >= parallel_tol ) {
		continue;
	    }
#endif

	    if ((a < 0 || NEAR_ZERO(a, ON_ZERO_TOLERANCE) || a > 1 || NEAR_ZERO(1-a, ON_ZERO_TOLERANCE)) ||
		    (b < 0 || NEAR_ZERO(b, ON_ZERO_TOLERANCE) || b > 1 || NEAR_ZERO(1-b, ON_ZERO_TOLERANCE))) {
		continue;
	    }
#if 0
	    else {
		std::cout << "Isect: a = " << a << ", b = " << b << ":\n";
		ON_2dPoint p2d;
		self_isect_edges.insert(ue1);
		p2d = ON_2dPoint(pnts_2d[ue1.v[0]].first, pnts_2d[ue1.v[0]].second);
		std::cout << ue1.v[0] << "(" << p2d.x << "," << p2d.y << ")<->";
		p2d = ON_2dPoint(pnts_2d[ue1.v[1]].first, pnts_2d[ue1.v[1]].second);
		std::cout << ue1.v[1] << "(" << p2d.x << "," << p2d.y << ") isects with ";
		self_isect_edges.insert(ue2);
		p2d = ON_2dPoint(pnts_2d[ue2.v[0]].first, pnts_2d[ue2.v[0]].second);
		std::cout << ue2.v[0] << "(" << p2d.x << "," << p2d.y << ")<->";
		p2d = ON_2dPoint(pnts_2d[ue2.v[1]].first, pnts_2d[ue2.v[1]].second);
		std::cout << ue2.v[1] << "(" << p2d.x << "," << p2d.y << ")\n";
	    }
#endif
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

bool
cpolygon_t::point_in_polygon(long v, bool flip)
{
    if (v == -1) return false;
    if (!closed()) return false;

    point2d_t *polypnts = (point2d_t *)bu_calloc(poly.size()+1, sizeof(point2d_t), "polyline");

    size_t pind = 0;

    cpolyedge_t *pe = (*poly.begin());
    cpolyedge_t *first = pe;
    cpolyedge_t *next = pe->next;

    if (v == pe->v[0] || v == pe->v[1]) {
	bu_free(polypnts, "polyline");
	return false;
    }

    V2SET(polypnts[pind], pnts_2d[pe->v[0]].first, pnts_2d[pe->v[0]].second);
    pind++;
    V2SET(polypnts[pind], pnts_2d[pe->v[1]].first, pnts_2d[pe->v[1]].second);

    // Walk the loop
    while (first != next) {
	pind++;
	if (v == next->v[0] || v == next->v[1]) {
	    bu_free(polypnts, "polyline");
	    return false;
	}
	V2SET(polypnts[pind], pnts_2d[next->v[1]].first, pnts_2d[next->v[1]].second);
	next = next->next;
    }

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

    bool result = (bool)bg_pt_in_polygon(pind, (const point2d_t *)polypnts, (const point2d_t *)&test_pnt);

    if (flip) {
	result = (result) ? false : true;
    }

    bu_free(polypnts, "polyline");

    return result;
}

bool
cpolygon_t::best_fit_plane()
{
    // Find the best fit plane for the Brep vertices of the triangles involved with
    // the polygon
    std::set<long> averts;
    int ncnt = 0;

    if (pnts_type == 1) return false;

    std::set<cpolyedge_t *>::iterator cp_it;
    for (cp_it = poly.begin(); cp_it != poly.end(); cp_it++) {
	cpolyedge_t *pe = *cp_it;
	averts.insert(pe->v[0]);
	averts.insert(pe->v[1]);
    }
    std::set<long>::iterator a_it;
    for (a_it = interior_points.begin(); a_it != interior_points.end(); a_it++) {
	averts.insert(*a_it);
    }

    ON_3dVector avgtnorm(0.0,0.0,0.0);
    for (a_it = averts.begin(); a_it != averts.end(); a_it++) {
	ON_3dPoint *vn = cdt_mesh->normals[cdt_mesh->nmap[p2o[*a_it]]];
	if (vn) {
	    avgtnorm += *vn;
	    ncnt++;
	}
    }
    avgtnorm = avgtnorm * 1.0/(double)ncnt;

    point_t pcenter;
    vect_t pnorm;
    {
        point_t *vpnts = (point_t *)bu_calloc(averts.size()+1, sizeof(point_t), "fitting points");
        int pnts_ind = 0;
	for (a_it = averts.begin(); a_it != averts.end(); a_it++) {
            ON_3dPoint *p = cdt_mesh->pnts[p2o[*a_it]];
            vpnts[pnts_ind][X] = p->x;
            vpnts[pnts_ind][Y] = p->y;
            vpnts[pnts_ind][Z] = p->z;
            pnts_ind++;
        }
        if (bn_fit_plane(&pcenter, &pnorm, pnts_ind, vpnts)) {
	    return false;
	}
        bu_free(vpnts, "fitting points");

        ON_3dVector on_norm(pnorm[X], pnorm[Y], pnorm[Z]);
        if (ON_DotProduct(on_norm, avgtnorm) < 0) {
            VSCALE(pnorm, pnorm, -1);
        }
    }

    ON_Plane rplane(pcenter, pnorm);

    fit_plane = rplane;
    return true;
}

bool
cpolygon_t::cdt()
{
    if (!closed()) return false;

    int *faces = NULL;
    int num_faces = 0;
    int *steiner = NULL;
    point2d_t *bgp_2d;

    if (pnts_type == 0) {
	// In 3D we may have faces perpendicular to the original triangle face included,
	// so calculate a best fit plane and re-project the original points.  The
	// new plane should be close, but not exactly the same plane as the
	// starting plane.  It may happen that the reprojection invalids the
	// inside/outside categorization of points - in that case, abandon the
	// re-fit and attempt the cdt with the original plane.
	if (!best_fit_plane()) {
	    std::cerr << "cdt(): finding the best fit plane failed.\n";
	    return false;
	}

	bgp_2d = (point2d_t *)bu_calloc(cdt_mesh->pnts.size() + 1, sizeof(point2d_t), "2D points array");
	for (size_t i = 0; i < cdt_mesh->pnts.size(); i++) {
	    double u, v;
	    ON_3dPoint op3d = (*cdt_mesh->pnts[i]);
	    fit_plane.ClosestPointTo(op3d, &u, &v);
	    bgp_2d[i][X] = u;
	    bgp_2d[i][Y] = v;
	}

	// Make sure the new points still form a close polygon and all the interior points
	// are still interior points - if not, put them back
	int valid_reprojection = 1;
	if (!closed()) {
	    valid_reprojection = 0;
	} else {
	    std::set<long>::iterator u_it;
	    for (u_it = interior_points.begin(); u_it != interior_points.end(); u_it++) {
		if (point_in_polygon(*u_it, false)) {
		    valid_reprojection = 0;
		    break;
		}
	    }
	}
	if (!valid_reprojection) {
	    for (size_t i = 0; i < cdt_mesh->pnts.size(); i++) {
		V2SET(bgp_2d[i], pnts_2d[i].first, pnts_2d[i].second);
	    }
	}

    } else {
	// 2D polygons w
	bgp_2d = (point2d_t *)bu_calloc(cdt_mesh->pnts.size() + 1, sizeof(point2d_t), "2D points array");
	for (size_t i = 0; i < pnts_2d.size(); i++) {
	    bgp_2d[i][X] = pnts_2d[i].first;
	    bgp_2d[i][Y] = pnts_2d[i].second;
	}
    }

    if (interior_points.size()) {
	steiner = (int *)bu_calloc(interior_points.size(), sizeof(int), "interior points");
	std::set<long>::iterator p_it;
	int vind = 0;
	for (p_it = interior_points.begin(); p_it != interior_points.end(); p_it++) {
	    steiner[vind] = (int)*p_it;
	    vind++;
	}
    }

    int *opoly = (int *)bu_calloc(poly.size()+1, sizeof(int), "polygon points");

    size_t vcnt = 1;
    cpolyedge_t *pe = (*poly.begin());
    cpolyedge_t *first = pe;
    cpolyedge_t *next = pe->next;

    opoly[vcnt-1] = pe->v[0];
    opoly[vcnt] = pe->v[1];

    // Walk the loop
    while (first != next) {
	vcnt++;
	opoly[vcnt] = next->v[1];
	next = next->next;
    	if (vcnt > poly.size()) {
	    bu_free(bgp_2d, "free libbg 2d points array)");
	    std::cerr << "cdt attempt on infinite loop (shouldn't be possible - closed() test failed to detect this somehow...)\n";
	    return false;
	}
    }

    bool result = (bool)!bg_nested_polygon_triangulate( &faces, &num_faces,
	    NULL, NULL, opoly, poly.size()+1, NULL, NULL, 0, steiner,
	    interior_points.size(), bgp_2d, cdt_mesh->pnts.size(),
	    TRI_CONSTRAINED_DELAUNAY);

    if (result) {
	for (int i = 0; i < num_faces; i++) {
	    triangle_t t;
	    t.v[0] = faces[3*i+0];
	    t.v[1] = faces[3*i+1];
	    t.v[2] = faces[3*i+2];

	    if (pnts_type == 0) {
		ON_3dVector tdir = cdt_mesh->tnorm(t);
		ON_3dVector bdir = cdt_mesh->bnorm(t);
		bool flipped_tri = (ON_DotProduct(tdir, bdir) < 0);
		if (flipped_tri) {
		    t.v[2] = faces[3*i+1];
		    t.v[1] = faces[3*i+2];
		}
	    }

	    tris.insert(t);
	}

	bu_free(faces, "faces array");
    }

    bu_free(bgp_2d, "free libbg 2d points array)");
    bu_free(opoly, "polygon points");

    if (steiner) {
	bu_free(steiner, "faces array");
    }

    return result;
}

long
cpolygon_t::tri_process(std::set<uedge_t> *ne, std::set<uedge_t> *se, long *nv, triangle_t &t)
{
    std::set<cpolyedge_t *>::iterator pe_it;

    std::set<uedge_t> problem_edges = cdt_mesh->get_problem_edges();

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
	for (pe_it = poly.begin(); pe_it != poly.end(); pe_it++) {
	    cpolyedge_t *pe = *pe_it;
	    struct uedge_t pue(pe->v[0], pe->v[1]);
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
	    if (!cdt_mesh->brep_edge_pnt(ue[i].v[0])) {
		v2ue[ue[i].v[0]].insert(ue[i]);
	    }
	    if (!cdt_mesh->brep_edge_pnt(ue[i].v[1])) {
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
		    uncontained.insert(v_it->first);
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
	long unshared_vert = unshared_vertex(t);

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
			uncontained.insert(*nv);
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
    VSET(bnp, pnts_2d[efirst->v[0]].first, pnts_2d[efirst->v[0]].second, 0);
    pdv_3move(plot_file, bnp);
    VSET(bnp, pnts_2d[efirst->v[1]].first, pnts_2d[efirst->v[1]].second, 0);
    pdv_3cont(plot_file, bnp);

    point2d_t wpnt;
    V2SET(wpnt, pnts_2d[efirst->v[0]].first, pnts_2d[efirst->v[0]].second);
    V2MINMAX(pmin, pmax, wpnt);
    V2SET(wpnt, pnts_2d[efirst->v[1]].first, pnts_2d[efirst->v[1]].second);
    V2MINMAX(pmin, pmax, wpnt);

    size_t ecnt = 1;
    while (ecurr != efirst && ecnt < poly.size()+1) {
	ecnt++;
        ecurr = (!ecurr) ? efirst->next : ecurr->next;
        VSET(bnp, pnts_2d[ecurr->v[1]].first, pnts_2d[ecurr->v[1]].second, 0);
	pdv_3cont(plot_file, bnp);
	V2SET(wpnt, pnts_2d[ecurr->v[1]].first, pnts_2d[ecurr->v[1]].second);
	V2MINMAX(pmin, pmax, wpnt);
    	if (ecnt > poly.size()) {
	    break;
	}
    }

    // Plot interior and uncontained points as well
    double r = DIST_PT2_PT2(pmin, pmax) * 0.01;
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

    ppnt = tplane.PointAt(pnts_2d[efirst->v[0]].first, pnts_2d[efirst->v[0]].second);
    VSET(bnp, ppnt.x, ppnt.y, ppnt.z);
    pdv_3move(plot_file, bnp);
    VMINMAX(pmin, pmax, bnp);
    ppnt = tplane.PointAt(pnts_2d[efirst->v[1]].first, pnts_2d[efirst->v[1]].second);
    VSET(bnp, ppnt.x, ppnt.y, ppnt.z);
    pdv_3cont(plot_file, bnp);
    VMINMAX(pmin, pmax, bnp);

    size_t ecnt = 1;
    while (ecurr != efirst && ecnt < poly.size()+1) {
	ecnt++;
	ecurr = (!ecurr) ? efirst->next : ecurr->next;
	ppnt = tplane.PointAt(pnts_2d[ecurr->v[1]].first, pnts_2d[ecurr->v[1]].second);
	VSET(bnp, ppnt.x, ppnt.y, ppnt.z);
	pdv_3cont(plot_file, bnp);
	VMINMAX(pmin, pmax, bnp);
       	if (ecnt > poly.size()) {
	    break;
	}
    }

    // Plot interior and uncontained points as well
    double r = DIST_PT_PT(pmin, pmax) * 0.01;
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

void
cpolygon_t::plot_best_fit_plane(const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    pl_color(plot_file, r, g, b);
    ON_3dPoint ocenter = fit_plane.Origin();
    ON_3dVector onorm = fit_plane.Normal();
    point_t center, norm;

    VSET(center, ocenter.x, ocenter.y, ocenter.z);
    VSET(norm, onorm.x, onorm.y, onorm.z);

    vect_t xbase, ybase, tip;
    vect_t x_1, x_2, y_1, y_2;
    bn_vec_perp(xbase, norm);
    VCROSS(ybase, xbase, norm);
    VUNITIZE(xbase);
    VUNITIZE(ybase);
    VSCALE(xbase, xbase, 10);
    VSCALE(ybase, ybase, 10);
    VADD2(x_1, center, xbase);
    VSUB2(x_2, center, xbase);
    VADD2(y_1, center, ybase);
    VSUB2(y_2, center, ybase);

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

    VSCALE(tip, norm, 5);
    VADD2(tip, center, tip);
    pdv_3move(plot_file, center);
    pdv_3cont(plot_file, tip);

    fclose(plot_file);
}

void cpolygon_t::print()
{

    size_t ecnt = 1;
    cpolyedge_t *pe = (*poly.begin());
    cpolyedge_t *first = pe;
    cpolyedge_t *next = pe->next;

    std::set<cpolyedge_t *> visited;
    visited.insert(first);

    std::cout << first->v[0];

    // Walk the loop - an infinite loop is not closed
    while (first != next) {
	ecnt++;
	if (!next) {
	    break;
	}
	std::cout << "->" << next->v[0];
	visited.insert(next);
	next = next->next;
	if (ecnt > poly.size()) {
	    std::cout << "\nERROR infinite loop\n";
	    break;
	}
    }

    std::cout << "\n";


    std::set<cpolyedge_t *>::iterator pd_it;
    std::cout << "poly data:\n";
    for (pd_it = poly.begin(); pd_it != poly.end(); pd_it++) {
	std::cout << "entry:  ";
	if ((*pd_it)->prev) {
	    std::cout << "(" << (*pd_it)->prev->v[0] << "->" << (*pd_it)->prev->v[1] << ")";
	} else {
	    std::cout << "NULL";
	}
	std::cout << "<-(" << (*pd_it)->v[0] << "->" << (*pd_it)->v[1] << ")->";
	if ((*pd_it)->next) {
	    std::cout << "(" << (*pd_it)->next->v[0] << "->" << (*pd_it)->next->v[1] << ")";
	} else {
	    std::cout << "NULL";
	}
	std::cout << "\n";
    }

    ON_3dPoint *p;

    self_intersecting();
    std::set<uedge_t>::iterator sie_it;
    if (self_isect_edges.size()) {
	std::cout << "Self-intersecting edges from polygon:\n";
	for (sie_it = self_isect_edges.begin(); sie_it != self_isect_edges.end(); sie_it++) {
	    p = cdt_mesh->pnts[p2o[sie_it->v[0]]];
	    std::cout << sie_it->v[0] << "(" << p->x << "," << p->y << "," << p->z << ")<->";
	    p = cdt_mesh->pnts[p2o[sie_it->v[1]]];
	    std::cout << sie_it->v[1] << "(" << p->x << "," << p->y << "," << p->z << ")\n";
	}
    }

    // Print the 3D points
    if (pnts_type == 0) {
	next = first->next;
	p = cdt_mesh->pnts[p2o[first->v[0]]];
	std::cout << "(" << p->x << "," << p->y << "," << p->z << ")->";
	ecnt = 1;
	while (first != next) {
	    ecnt++;
	    if (!next) {
		break;
	    }
	    p = cdt_mesh->pnts[p2o[next->v[0]]];
	    std::cout << "(" << p->x << "," << p->y << "," << p->z << ")->";
	    visited.insert(next);
	    next = next->next;
	    if (ecnt > poly.size()) {
		std::cout << "\nERROR infinite loop\n";
		break;
	    }
	}
	p = cdt_mesh->pnts[p2o[first->v[0]]];
	std::cout << "(" << p->x << "," << p->y << "," << p->z << ")\n";
    }

    if (visited.size() != poly.size()) {
	std::cout << "Missing edges:\n";
	std::set<cpolyedge_t *>::iterator p_it;
	for (p_it = poly.begin(); p_it != poly.end(); p_it++) {
	    if (visited.find(*p_it) == visited.end()) {
		std::cout << "  " << (*p_it)->v[0] << "->" << (*p_it)->v[1] << "\n";
	    }
	}
    }
}

bool
cpolygon_t::have_uncontained()
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
    HIDDEN int
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

std::vector<struct ctriangle_t>
cpolygon_t::polygon_tris(double angle, bool brep_norm, int initial)
{
    std::set<triangle_t> initial_set;

    std::set<cpolyedge_t *>::iterator p_it;
    for (p_it = poly.begin(); p_it != poly.end(); p_it++) {
	cpolyedge_t *pe = *p_it;
	struct uedge_t ue(pe->v[0], pe->v[1]);
	bool edge_isect = (self_isect_edges.find(ue) != self_isect_edges.end());
	std::set<triangle_t> petris = cdt_mesh->uedges2tris[ue];
	std::set<triangle_t>::iterator t_it;
	for (t_it = petris.begin(); t_it != petris.end(); t_it++) {

	    if (visited_triangles.find(*t_it) != visited_triangles.end()) {
	       	continue;
	    }

	    // If the triangle is involved with a self intersecting edge in the
	    // polygon and we haven't already see it, we have to try incorporating it
	    if (edge_isect) {
		initial_set.insert(*t_it);
		continue;
	    }

	    // If all three verts are edge vertices and this is our first run
	    // through, we need to try incorporating it.  We may be seeding
	    // next to a "vertical" edge triangle, and it may be that none of
	    // the relevant points qualified to be "uncontained" points.  If
	    // so, our growth criteria will not result in a new polygon, but we
	    // need to try and correct the vertical triangle.
	    if (initial) {
		if (cdt_mesh->brep_edge_pnt((*t_it).v[0]) && cdt_mesh->brep_edge_pnt((*t_it).v[1]) && cdt_mesh->brep_edge_pnt((*t_it).v[2])) {
		    initial_set.insert(*t_it);
		    continue;
		}
	    }

	    ON_3dVector tn = (brep_norm) ? cdt_mesh->bnorm(*t_it) : cdt_mesh->tnorm(*t_it);
	    double dprd = ON_DotProduct(pdir, tn);
	    double dang = (NEAR_EQUAL(dprd, 1.0, ON_ZERO_TOLERANCE)) ? 0 : acos(dprd);
	    if (dang > angle) {
		continue;
	    }
	    initial_set.insert(*t_it);
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
	struct edge_t e1(t.v[0], t.v[1]);
	struct edge_t e2(t.v[1], t.v[2]);
	struct edge_t e3(t.v[2], t.v[0]);
	struct uedge_t ue[3];
	ue[0].set(t.v[0], t.v[1]);
	ue[1].set(t.v[1], t.v[2]);
	ue[2].set(t.v[2], t.v[0]);

	nct->all_bedge = false;
	nct->isect_edge = false;
	nct->uses_uncontained = false;
	nct->contains_uncontained = false;
	nct->angle_to_nearest_uncontained = DBL_MAX;

	for (int i = 0; i < 3; i++) {
	    if (self_isect_edges.find(ue[i]) != self_isect_edges.end()) {
		nct->isect_edge = true;
		unusable_triangles.erase(*f_it);
	    }
	    if (nct->isect_edge) break;
	}
	if (nct->isect_edge) continue;


	// If we're not on a self-intersecting edge, check for use of uncontained points
	for (int i = 0; i < 3; i++) {
	    if (uncontained.find((t).v[i]) != uncontained.end()) {
		nct->uses_uncontained = true;
		unusable_triangles.erase(*f_it);
	    }
	    if (nct->uses_uncontained) break;
	    if (flipped_face.find((t).v[i]) != flipped_face.end()) {
		nct->uses_uncontained = true;
		unusable_triangles.erase(*f_it);
	    }
	    if (nct->uses_uncontained) break;
	}
	if (nct->uses_uncontained) continue;

	// If we aren't directly using an uncontained point, see if one is inside
	// the triangle projection
	cpolygon_t tpoly;
	tpoly.pnts_2d = pnts_2d;
	tpoly.add_edge(e1);
	tpoly.add_edge(e2);
	tpoly.add_edge(e3);
	std::set<long>::iterator u_it;
	for (u_it = uncontained.begin(); u_it != uncontained.end(); u_it++) {
	    if (tpoly.point_in_polygon(*u_it, false)) {
		nct->contains_uncontained = true;
		unusable_triangles.erase(*f_it);
	    }
	}
	if (!nct->contains_uncontained) {
	    for (u_it = flipped_face.begin(); u_it != flipped_face.end(); u_it++) {
		if (tpoly.point_in_polygon(*u_it, false)) {
		    nct->contains_uncontained = true;
		    unusable_triangles.erase(*f_it);
		}
	    }
	}
	if (nct->contains_uncontained) continue;


	// If we've pulled in a face that is all edge vertices, go with it
	if (cdt_mesh->brep_edge_pnt(t.v[0]) && cdt_mesh->brep_edge_pnt(t.v[1]) && cdt_mesh->brep_edge_pnt(t.v[2])) {
	    nct->all_bedge = true;
	    unusable_triangles.erase(*f_it);
	}
	if (nct->all_bedge) continue;

	// If we aren't directly involved with an uncontained point and we only
	// share 1 edge with the polygon, see how much it points at one of the
	// points of interest (if any) definitely outside the current polygon
	std::set<cpolyedge_t *>::iterator pe_it;
	long shared_cnt = shared_edge_cnt(t);
	if (shared_cnt != 1) continue;
	double vangle = ucv_angle(t);
	if (vangle > 0 && nct->angle_to_nearest_uncontained > vangle) {
	    nct->angle_to_nearest_uncontained = vangle;
	    unusable_triangles.erase(*f_it);
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

long
cpolygon_t::grow_loop(double deg, bool stop_on_contained, triangle_t &target)
{
    double angle = deg * ON_PI/180.0;

    if (stop_on_contained && !uncontained.size() && visited_triangles.size() > 1) {
	return 0;
    }

    if (deg < 0 || deg > 170) {
	std::cerr << "Degree error: " << deg << "\n";
	return -1;
    }

    if (visited_triangles.find(target) == visited_triangles.end()) {
	for (int i = 0; i < 3; i++) {
	    if (used_verts.find(target.v[i]) == used_verts.end()) {
		target_verts.insert(target.v[i]);
	    }
	}
    }

    unusable_triangles.clear();

    // First step - collect all the unvisited triangles from the polyline edges.

    std::stack<ctriangle_t> ts;

    std::set<edge_t> flipped_edges;

    std::vector<ctriangle_t> ptris = polygon_tris(angle, stop_on_contained, 1);

    if (!ptris.size() && stop_on_contained) {
	std::cerr << "No triangles available??\n";
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
	triangle_t ct(cct.v[0], cct.v[1], cct.v[2]);

	// A triangle will introduce at most one new point into the loop.  If
	// the triangle is bad, it will define uncontained interior points and
	// potentially (if it has unmated edges) won't grow the polygon at all.

	// The first thing to do is find out of the triangle shares one or two
	// edges with the loop.  (0 or 3 would indicate something Very Wrong...)
	std::set<uedge_t> new_edges;
	std::set<uedge_t> shared_edges;
	long vert = -1;
	long new_edge_cnt = tri_process(&new_edges, &shared_edges, &vert, ct);

	bool process = true;

	if (new_edge_cnt == -2) {
	    // Vert from bad edges - added to uncontained.  Start over with another triangle - we
	    // need to walk out until this point is swept up by the polygon.
	    visited_triangles.insert(ct);
	    process = false;
	}

	if (new_edge_cnt == -1) {
	    // If the triangle shares one edge but all three vertices are on the
	    // polygon, we can't use this triangle in this context - it would produce
	    // a self-intersecting polygon.
	    unusable_triangles.insert(ct);
	    process = false;
	}

	if (process) {

	    ON_3dVector tdir = cdt_mesh->tnorm(ct);
	    ON_3dVector bdir = cdt_mesh->bnorm(ct);
	    bool flipped_tri = (ON_DotProduct(tdir, bdir) < 0);

	    if (stop_on_contained && new_edge_cnt == 2 && flipped_tri) {
		// It is possible that a flipped face adding two new edges will
		// make a mess out of the polygon (i.e. make it self intersecting.)
		// Tag it so we know we can't trust point_in_polygon until we've grown
		// the vertex out of flipped_face (remove_edge will handle that.)
		if (cdt_mesh->brep_edge_pnt(vert)) {
		    // We can't flag brep edge points as uncontained, so if we hit this case
		    // flag all the verts except the edge verts as flipped face problem cases.
		    for (int i = 0; i < 3; i++) {
			if (!cdt_mesh->brep_edge_pnt(ct.v[i])) {
			    flipped_face.insert(ct.v[i]);
			}
		    }
		} else {
		    flipped_face.insert(vert);
		}
	    }

	    int use_tri = 1;
	    if (!(poly.size() == 3 && interior_points.size())) {
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
		bu_vls_sprintf(&fname, "%d-fatal_loop_growth_poly_3d.plot3", cdt_mesh->f_id);
		polygon_plot_in_plane(bu_vls_cstr(&fname));
		bu_vls_sprintf(&fname, "%d-fatal_loop_growth_visited_tris.plot3", cdt_mesh->f_id);
		cdt_mesh->tris_set_plot(visited_triangles, bu_vls_cstr(&fname));
		bu_vls_sprintf(&fname, "%d-fatal_loop_growth_unusable_tris.plot3", cdt_mesh->f_id);
		cdt_mesh->tris_set_plot(unusable_triangles, bu_vls_cstr(&fname));
		bu_vls_sprintf(&fname, "%d-fatal_loop_growth_bad_tri.plot3", cdt_mesh->f_id);
		cdt_mesh->tri_plot(ct, bu_vls_cstr(&fname));
		bu_vls_free(&fname);
		return -1;
	    }

	    if (use_tri) {
		replace_edges(new_edges, shared_edges);
		visited_triangles.insert(ct);
	    }
	}

	bool h_uc = have_uncontained();

	if (visited_triangles.find(target) != visited_triangles.end() && stop_on_contained && !h_uc && 
		(interior_points.size() > 1 || poly.size() > 3)) {
	    bool cdt_status = cdt();
	    if (cdt_status) {
		//cdt_mesh->tris_set_plot(tris, "patch.plot3");
		return (long)cdt_mesh->tris.size();
	    } else {
		struct bu_vls fname = BU_VLS_INIT_ZERO;
		//std::cerr << "cdt() failure\n";
		bu_vls_sprintf(&fname, "%d-cdt_failure_poly_3d.plot3", cdt_mesh->f_id);
		polygon_plot_in_plane(bu_vls_cstr(&fname));
		bu_vls_free(&fname);
		return -1;
	    }
	}

	if (ts.empty()) {
	    // That's all the triangles from this ring - if we haven't
	    // terminated yet, pull the next triangle set.

	    if (!stop_on_contained) {
		angle = 0.75 * angle;
	    }

	    // We queue these up in a specific order - we want any triangles
	    // actually using flipped or uncontained vertices to be at the top
	    // of the stack (i.e. the first ones tried.  polygon_tris is responsible
	    // for sorting in priority order.
	    std::vector<struct ctriangle_t> ntris = polygon_tris(angle, stop_on_contained, 0);

	    if (ctriangle_vect_cmp(ptris, ntris)) {
		if (h_uc || (stop_on_contained && poly.size() <= 3)) {
		    struct bu_vls fname = BU_VLS_INIT_ZERO;
		    std::cerr << "Error - new triangle set from polygon edge is the same as the previous triangle set.  Infinite loop, aborting\n";
		    std::vector<struct ctriangle_t> infinite_loop_tris = polygon_tris(angle, stop_on_contained, 0);
		    bu_vls_sprintf(&fname, "%d-infinite_loop_poly_2d.plot3", cdt_mesh->f_id);
		    polygon_plot(bu_vls_cstr(&fname));
		    bu_vls_sprintf(&fname, "%d-infinite_loop_poly_3d.plot3", cdt_mesh->f_id);
		    polygon_plot_in_plane(bu_vls_cstr(&fname));
		    bu_vls_sprintf(&fname, "%d-infinite_loop_tris.plot3", cdt_mesh->f_id);
		    cdt_mesh->ctris_vect_plot(infinite_loop_tris, bu_vls_cstr(&fname));
		    bu_vls_sprintf(&fname, "%d-infinite_loop.cdtmesh", cdt_mesh->f_id);
		    cdt_mesh->serialize(bu_vls_cstr(&fname));
		    bu_vls_free(&fname);
		    return -1;
		} else {
		    // We're not in a repair situation, and we've already tried
		    // the current triangle candidate set with no polygon
		    // change.  Generate triangles.
		    bool cdt_status = cdt();
		    if (cdt_status) {
			//cdt_mesh->tris_set_plot(tris, "patch.plot3");
			return (long)cdt_mesh->tris.size();
		    } else {
			struct bu_vls fname = BU_VLS_INIT_ZERO;
			std::cerr << "cdt() failure\n";
			bu_vls_sprintf(&fname, "%d-cdt_failure_poly_3d.plot3", cdt_mesh->f_id);
			polygon_plot_in_plane(bu_vls_cstr(&fname));
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
		bool cdt_status = cdt();
		if (cdt_status) {
		    //cdt_mesh->tris_set_plot(tris, "patch.plot3");
		    return (long)cdt_mesh->tris.size();
		} else {
		    struct bu_vls fname = BU_VLS_INIT_ZERO;
		    std::cerr << "cdt() failure\n";
		    bu_vls_sprintf(&fname, "%d-cdt_failure_poly_3d.plot3", cdt_mesh->f_id);
		    polygon_plot_in_plane(bu_vls_cstr(&fname));
		    bu_vls_free(&fname);
		    return -1;
		}
	    }
	}
    }

    std::cout << "Error - loop growth terminated but conditions for triangulation were never satisfied!\n";
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&fname, "%d-failed_patch_poly_2d.plot3", cdt_mesh->f_id);
    polygon_plot(bu_vls_cstr(&fname));
    bu_vls_sprintf(&fname, "%d-failed_patch_poly_3d.plot3", cdt_mesh->f_id);
    polygon_plot_in_plane(bu_vls_cstr(&fname));
    return -1;
}

void
cpolygon_t::build_2d_pnts(ON_3dPoint &c, ON_3dVector &n)
{
    pdir = n;
    ON_Plane tri_plane(c, n);
    for (size_t i = 0; i < cdt_mesh->pnts.size(); i++) {
	double u, v;
	ON_3dPoint op3d = (*cdt_mesh->pnts[i]);
	tri_plane.ClosestPointTo(op3d, &u, &v);
	std::pair<double, double> proj_2d;
	proj_2d.first = u;
	proj_2d.second = v;
	pnts_2d.push_back(proj_2d);
	p2o[i] = i;
    }
    tplane = tri_plane;
}


bool
cpolygon_t::build_initial_loop(triangle_t &seed, bool repair)
{
    std::set<uedge_t>::iterator u_it;

    if (repair) {
	// None of the edges or vertices from any of the problem triangles can be
	// in a polygon edge.  By definition, the seed is a problem triangle.
	std::set<long> seed_verts;
	for (int i = 0; i < 3; i++) {
	    seed_verts.insert(seed.v[i]);
	    // The exception to interior categorization is Brep boundary points -
	    // they are never interior or uncontained
	    if (cdt_mesh->brep_edge_pnt(seed.v[i])) {
		continue;
	    }
	    uncontained.insert(seed.v[i]);
	}

	// We need a initial valid polygon loop to grow.  Poll the neighbor faces - if one
	// of them is valid, it will be used to build an initial loop
	std::vector<triangle_t> faces = cdt_mesh->face_neighbors(seed);
	for (size_t i = 0; i < faces.size(); i++) {
	    triangle_t t = faces[i];
	    if (cdt_mesh->seed_tris.find(t) == cdt_mesh->seed_tris.end()) {
		struct edge_t e1(t.v[0], t.v[1]);
		struct edge_t e2(t.v[1], t.v[2]);
		struct edge_t e3(t.v[2], t.v[0]);
		add_edge(e1);
		add_edge(e2);
		add_edge(e3);
		visited_triangles.insert(t);
		return closed();
	    }
	}

	// If we didn't find a valid mated edge triangle (urk?) try the vertices
	for (int i = 0; i < 3; i++) {
	    std::vector<triangle_t> vfaces = cdt_mesh->vertex_face_neighbors(seed.v[i]);
	    for (size_t j = 0; j < vfaces.size(); j++) {
		triangle_t t = vfaces[j];
		if (cdt_mesh->seed_tris.find(t) == cdt_mesh->seed_tris.end()) {
		    struct edge_t e1(t.v[0], t.v[1]);
		    struct edge_t e2(t.v[1], t.v[2]);
		    struct edge_t e3(t.v[2], t.v[0]);
		    add_edge(e1);
		    add_edge(e2);
		    add_edge(e3);
		    visited_triangles.insert(t);
		    return closed();
		}
	    }
	}

	// NONE of the triangles in the neighborhood are valid?  We'll have to hope that
	// subsequent processing of other seeds will put a proper mesh in contact with
	// this face...
	return false;

    }

    // If we're not repairing, start with the seed itself
    struct edge_t e1(seed.v[0], seed.v[1]);
    struct edge_t e2(seed.v[1], seed.v[2]);
    struct edge_t e3(seed.v[2], seed.v[0]);
    add_edge(e1);
    add_edge(e2);
    add_edge(e3);
    visited_triangles.insert(seed);
    return closed();
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
    pnts_2d.push_back(proj_2d);
    return (long)(pnts_2d.size() - 1);
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

bool
cdt_mesh_t::tri_add(triangle_t &tri)
{

    // Skip degenerate triangles, but report true since this was never
    // a valid triangle in the first place
    if (tri.v[0] == tri.v[1] || tri.v[1] == tri.v[2] || tri.v[2] == tri.v[0]) {
	return true;
    }

    // Skip duplicate triangles, but report true as this triangle is in the mesh
    // already
    if (this->tris.find(tri) != this->tris.end()) {
	// Check the normal of the included duplicate and the candidate.
	// If the original is flipped and the new one isn't, swap them out - this
	// will help with subsequent processing.
	triangle_t orig = (*this->tris.find(tri));
	ON_3dVector torig_dir = tnorm(orig);
	ON_3dVector tnew_dir = tnorm(tri);
	ON_3dVector bdir = tnorm(orig);
	bool f1 = (ON_DotProduct(torig_dir, bdir) < 0);
	bool f2 = (ON_DotProduct(tnew_dir, bdir) < 0);
	if (f1 && !f2) {
	    tri_remove(orig);
	} else {
	    return true;
	}
    }


    // Add the triangle
    this->tris.insert(tri);

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
	this->edges2tris[e[ind]] = tri;
	this->uedges2tris[uedge_t(e[ind])].insert(tri);
	this->v2edges[e[ind].v[0]].insert(e[ind]);
	this->v2tris[tri.v[ind]].insert(tri);
    }

    // The addition of a triangle may change the boundary edge set.  Set update
    // flag.
    boundary_edges_stale = true;

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
	this->edges2tris[e[ind]];
	this->uedges2tris[uedge_t(e[ind])].erase(tri);
	this->v2edges[e[ind].v[0]].erase(e[ind]);
	this->v2tris[tri.v[ind]].erase(tri);
    }

    // Remove the triangle
    this->tris.erase(tri);

    // flag boundary edge information for updating
    boundary_edges_stale = true;
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
	std::set<triangle_t> faces = this->uedges2tris[e[ind]];
	std::set<triangle_t>::iterator f_it;
	for (f_it = faces.begin(); f_it != faces.end(); f_it++) {
	    if (*f_it != f) {
		result.push_back(*f_it);
	    }
	}
    }

    return result;
}


std::vector<triangle_t>
cdt_mesh_t::vertex_face_neighbors(long vind)
{
    std::vector<triangle_t> result;
    std::set<triangle_t> faces = this->v2tris[vind];
    std::set<triangle_t>::iterator f_it;
    for (f_it = faces.begin(); f_it != faces.end(); f_it++) {
	result.push_back(*f_it);
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

std::set<uedge_t>
cdt_mesh_t::get_problem_edges()
{
    if (boundary_edges_stale) {
	boundary_edges_update();
    }

    return problem_edges;
}

void
cdt_mesh_t::boundary_edges_update()
{
    if (!boundary_edges_stale) return;
    boundary_edges_stale = false;

    boundary_edges.clear();
    problem_edges.clear();

    std::map<uedge_t, std::set<triangle_t>>::iterator ue_it;
    for (ue_it = this->uedges2tris.begin(); ue_it != this->uedges2tris.end(); ue_it++) {
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
    std::vector<triangle_t> results;

    std::set<triangle_t>::iterator tr_it;
    for (tr_it = this->tris.begin(); tr_it != this->tris.end(); tr_it++) {
	ON_3dVector tdir = this->tnorm(*tr_it);
	ON_3dVector bdir = this->bnorm(*tr_it);
	if (tdir.Length() > 0 && bdir.Length() > 0 && ON_DotProduct(tdir, bdir) < 0.1) {
	    int epnt_cnt = 0;
	    for (int i = 0; i < 3; i++) {
		epnt_cnt = (ep.find((*tr_it).v[i]) == ep.end()) ? epnt_cnt : epnt_cnt + 1;
	    }
	    if (epnt_cnt == 2) {
		std::cerr << "UNCULLED problem point from surface???????:\n";
		for (int i = 0; i < 3; i++) {
		    if (ep.find((*tr_it).v[i]) == ep.end()) {
			ON_3dPoint *p = pnts[(*tr_it).v[i]];
			std::cerr << "(" << (*tr_it).v[i] << "): " << p->x << " " << p->y << " " << p->z << "\n";
		    }
		}
		results.clear();
		return results;
	    }
	    results.push_back(*tr_it);
	}
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

void
cdt_mesh_t::set_brep_data(
	bool brev,
       	std::set<ON_3dPoint *> *e,
       	std::set<std::pair<ON_3dPoint *, ON_3dPoint *>> *original_b_edges,
       	std::set<ON_3dPoint *> *s,
       	std::map<ON_3dPoint *, ON_3dPoint *> *n)
{
    this->m_bRev = brev;
    this->edge_pnts = e;
    this->b_edges = original_b_edges;
    this->singularities = s;
    this->normalmap = n;
}

std::set<uedge_t>
cdt_mesh_t::uedges(const triangle_t &t)
{
    struct uedge_t ue[3];
    ue[0].set(t.v[0], t.v[1]);
    ue[1].set(t.v[1], t.v[2]);
    ue[2].set(t.v[2], t.v[0]);

    std::set<uedge_t> uedges;
    for (int i = 0; i < 3; i++) {
	uedges.insert(ue[i]);
    }

    return uedges;
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

ON_3dVector
cdt_mesh_t::bnorm(const triangle_t &t)
{
    ON_3dPoint avgnorm(0,0,0);

    // Can't calculate this without some key Brep data
    if (!nmap.size()) return avgnorm;

    double norm_cnt = 0.0;

    for (size_t i = 0; i < 3; i++) {
	if (sv.find(t.v[i]) != sv.end()) {
	    // singular vert norms are a product of multiple faces - not useful for this
	    continue;
	}
	ON_3dPoint onrm = *normals[nmap[t.v[i]]];
	if (this->m_bRev) {
	    onrm = onrm * -1;
	}
	avgnorm = avgnorm + onrm;
	norm_cnt = norm_cnt + 1.0;
    }

    ON_3dVector anrm = avgnorm/norm_cnt;
    anrm.Unitize();
    return anrm;
}

bool
cdt_mesh_t::brep_edge_pnt(long v)
{
    return (ep.find(v) != ep.end());
}

void cdt_mesh_t::reset()
{
    this->tris.clear();
    this->v2edges.clear();
    this->v2tris.clear();
    this->edges2tris.clear();
    this->uedges2tris.clear();
}

void cdt_mesh_t::build(std::set<p2t::Triangle *> *cdttri, std::map<p2t::Point *, ON_3dPoint *> *pointmap)
{
    if (!cdttri || !pointmap) return;

    this->reset();
    this->pnts.clear();
    this->p2ind.clear();
    this->n2ind.clear();
    this->sv.clear();

    std::set<p2t::Triangle*>::iterator s_it;

    // Populate points
    std::set<ON_3dPoint *> uniq_p3d;
    for (s_it = cdttri->begin(); s_it != cdttri->end(); s_it++) {
	p2t::Triangle *t = *s_it;
	for (size_t j = 0; j < 3; j++) {
	    uniq_p3d.insert((*pointmap)[t->GetPoint(j)]);
	}
    }
    this->sv.clear();
    std::set<ON_3dPoint *>::iterator u_it;
    for (u_it = uniq_p3d.begin(); u_it != uniq_p3d.end(); u_it++) {
	this->pnts.push_back(*u_it);
	this->p2ind[*u_it] = this->pnts.size() - 1;
	if (this->singularities->find(*u_it) != this->singularities->end()) {
	    this->sv.insert(this->p2ind[*u_it]);
	}
    }

    // Populate normals
    std::set<ON_3dPoint *> uniq_n3d;
    std::map<ON_3dPoint *, ON_3dPoint *>::iterator n_it;
    for (n_it = normalmap->begin(); n_it != normalmap->end(); n_it++) {
	uniq_n3d.insert(n_it->second);
    }
    for (u_it = uniq_n3d.begin(); u_it != uniq_n3d.end(); u_it++) {
	this->normals.push_back(*u_it);
	this->n2ind[*u_it] = this->normals.size() - 1;
    }
    for (u_it = uniq_p3d.begin(); u_it != uniq_p3d.end(); u_it++) {
	nmap[p2ind[*u_it]] = n2ind[(*normalmap)[*u_it]];
    }

    // From the triangles, populate the containers
    for (s_it = cdttri->begin(); s_it != cdttri->end(); s_it++) {
	p2t::Triangle *t = *s_it;
	ON_3dPoint *pt_A = (*pointmap)[t->GetPoint(0)];
	ON_3dPoint *pt_B = (*pointmap)[t->GetPoint(1)];
	ON_3dPoint *pt_C = (*pointmap)[t->GetPoint(2)];

	// Skip degenerate triangles
	if (pt_A == pt_B || pt_B == pt_C || pt_C == pt_A) {
	    continue;
	}

	long Aind = this->p2ind[pt_A];
	long Bind = this->p2ind[pt_B];
	long Cind = this->p2ind[pt_C];
	struct triangle_t nt(Aind, Bind, Cind);
	cdt_mesh_t::tri_add(nt);
    }

    // Populate cdt_mesh edge point set
    std::set<ON_3dPoint *>::iterator ep_it;
    for (ep_it = edge_pnts->begin(); ep_it != edge_pnts->end(); ep_it++) {
	ep.insert(p2ind[*ep_it]);
    }

    // Define brep edge set in cdt_mesh terms
    std::set<std::pair<ON_3dPoint *, ON_3dPoint *>>::iterator b_it;
    for (b_it = b_edges->begin(); b_it != b_edges->end(); b_it++) {
	long p1_ind = p2ind[b_it->first];
	long p2_ind = p2ind[b_it->second];
	brep_edges.insert(uedge_t(p1_ind, p2_ind));
    }
    boundary_edges_update();
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
    std::set<triangle_t>::iterator t_it;
    for (u_it = problem_edges.begin(); u_it != problem_edges.end(); u_it++) {
	std::set<triangle_t> ptris = uedges2tris[(*u_it)];
	for (t_it = uedges2tris[(*u_it)].begin(); t_it != uedges2tris[(*u_it)].end(); t_it++) {
	    uresults.insert(*t_it);
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
    std::set<triangle_t>::iterator t_it;

    if (boundary_edges_stale) {
	boundary_edges_update();
    }

    for (u_it = problem_edges.begin(); u_it != problem_edges.end(); u_it++) {
	std::set<triangle_t> ptris = uedges2tris[(*u_it)];
	for (t_it = uedges2tris[(*u_it)].begin(); t_it != uedges2tris[(*u_it)].end(); t_it++) {
	    pedge_tris.insert(*t_it);
	}
    }

    std::map<uedge_t, std::set<triangle_t>>::iterator et_it;
    for (et_it = uedges2tris.begin(); et_it != uedges2tris.end(); et_it++) {
	std::set<triangle_t> uetris = et_it->second;
	if (uetris.size() > 2) {
	    size_t valid_cnt = 0;
	    for (t_it = uetris.begin(); t_it != uetris.end(); t_it++) {
		triangle_t t = *t_it;
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
		plot_uedge(pue, plot_file);
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
	double dprd = ON_DotProduct(sn, tn);
	double dang = (NEAR_EQUAL(dprd, 1.0, ON_ZERO_TOLERANCE)) ? 0 : acos(dprd);
	dmax = (dang > dmax) ? dang : dmax;
    }

    dmax = dmax * 180.0/ON_PI;
    dmax = (dmax < 10) ? 10 : dmax;
    return (dmax < 170) ? dmax : 170;
}

bool
cdt_mesh_t::process_seed_tri(triangle_t &seed, bool repair, double deg)
{
    // We use the Brep normal for this, since the triangles are
    // problem triangles and their normals cannot be relied upon.
    ON_3dPoint sp = this->tcenter(seed);
    ON_3dVector sn = this->bnorm(seed);

    cpolygon_t polygon;
    polygon.cdt_mesh = this;
    polygon.build_2d_pnts(sp, sn);

    // build an initial loop from a nearby valid triangle
    bool tcnt = polygon.build_initial_loop(seed, repair);

    if (!tcnt && repair) {
	std::cerr << "Could not build initial valid loop\n";
	return false;
    }


    // Grow until we contain the seed and its associated problem data
    long tri_cnt = polygon.grow_loop(deg, repair, seed);
    if (tri_cnt < 0) {
	std::cerr << "grow_loop failure\n";
	return false;
    }

    // If nothing to do at the seed, we don't change the mesh
    if (tri_cnt == 0) {
	return true;
    }

    // Remove everything the patch claimed
    std::set<triangle_t>::iterator v_it;
    for (v_it = polygon.visited_triangles.begin(); v_it != polygon.visited_triangles.end(); v_it++) {
	triangle_t vt = *v_it;
	seed_tris.erase(vt);
	tri_remove(vt);
    }

    // Add in the replacement triangles
    for (v_it = polygon.tris.begin(); v_it != polygon.tris.end(); v_it++) {
	triangle_t vt = *v_it;
	tri_add(vt);
    }

    return true;
}

// TODO - should be a function provided by cpolygon_t...
int *
loop_to_bgpoly(const cpolygon_t *loop)
{
    int *opoly = (int *)bu_calloc(loop->poly.size()+1, sizeof(int), "polygon points");

    std::map<long, long> p2f = loop->p2o;

    size_t vcnt = 1;
    cpolyedge_t *pe = (*loop->poly.begin());
    cpolyedge_t *first = pe;
    cpolyedge_t *next = pe->next;

    opoly[vcnt-1] = p2f[pe->v[0]];
    opoly[vcnt] = p2f[pe->v[1]];

    while (first != next) {
	vcnt++;
	opoly[vcnt] = p2f[next->v[1]];
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
	bu_log("outer loop reports not closed!\n");
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

    point2d_t *bgp_2d = (point2d_t *)bu_calloc(pnts_2d.size() + 1, sizeof(point2d_t), "2D points array");
    for (size_t i = 0; i < pnts_2d.size(); i++) {
	bgp_2d[i][X] = pnts_2d[i].first;
	bgp_2d[i][Y] = pnts_2d[i].second;
    }

    int *faces = NULL;
    int num_faces = 0;
    int *steiner = NULL;
    if (interior_pnts.size()) {
	steiner = (int *)bu_calloc(interior_pnts.size(), sizeof(int), "interior points");
	std::set<long>::iterator p_it;
	int vind = 0;
	for (p_it = interior_pnts.begin(); p_it != interior_pnts.end(); p_it++) {
	    steiner[vind] = (int)*p_it;
	    vind++;
	}
    }

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

    bool result = (bool)!bg_nested_polygon_triangulate( &faces, &num_faces,
	    NULL, NULL, opoly, outer_loop.poly.size()+1, holes_array, holes_npts, holes_cnt,
	    steiner, interior_pnts.size(), bgp_2d, pnts_2d.size(),
	    TRI_CONSTRAINED_DELAUNAY);

    if (result) {
	for (int i = 0; i < num_faces; i++) {
	    triangle_t t;
	    t.v[0] = faces[3*i+0];
	    t.v[1] = faces[3*i+1];
	    t.v[2] = faces[3*i+2];

	    tris_2d.insert(t);
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

    if (steiner) {
	bu_free(steiner, "faces array");
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

    // *Wrong* triangles: problem edge and/or flipped normal triangles.  Handle
    // those first, so the subsequent clean-up pass doesn't have to worry about
    // errors they might introduce.
    std::vector<triangle_t> f_tris = this->interior_incorrect_normals();
    std::vector<triangle_t> e_tris = this->problem_edge_tris();
    seed_tris.clear();
    seed_tris.insert(e_tris.begin(), e_tris.end());
    seed_tris.insert(f_tris.begin(), f_tris.end());

    size_t st_size = seed_tris.size();
    while (seed_tris.size()) {
	triangle_t seed = *seed_tris.begin();

	bool pseed = process_seed_tri(seed, true, 170.0);

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

    // Now that the out-and-out problem triangles have been handled,
    // remesh near singularities to try and produce more reasonable
    // triangles.

    std::vector<triangle_t> s_tris = this->singularity_triangles();
    seed_tris.insert(s_tris.begin(), s_tris.end());

    st_size = seed_tris.size();
    while (seed_tris.size()) {
	triangle_t seed = *seed_tris.begin();

	double deg = max_angle_delta(seed, s_tris);
	bool pseed = process_seed_tri(seed, false, deg);

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

    boundary_edges_update();
    if (problem_edges.size() > 0) {
	return false;
    }
    return true;
}

bool
cdt_mesh_t::valid()
{
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    bool nret = true;
    bool eret = true;
    std::set<triangle_t>::iterator tr_it;
    for (tr_it = tris.begin(); tr_it != tris.end(); tr_it++) {
	ON_3dVector tdir = tnorm(*tr_it);
	ON_3dVector bdir = bnorm(*tr_it);
	if (tdir.Length() > 0 && bdir.Length() > 0 && ON_DotProduct(tdir, bdir) < 0.1) {
	    std::cout << "Still have invalid normals in mesh, triangle (" << (*tr_it).v[0] << "," << (*tr_it).v[1] << "," << (*tr_it).v[2] << ")\n";
	    bu_vls_sprintf(&fname, "%d-invalid_normal_tri_%ld_%ld_%ld.plot3", f_id, (*tr_it).v[0], (*tr_it).v[1], (*tr_it).v[2]);
	    tri_plot(*tr_it, bu_vls_cstr(&fname));
	    nret = false;
	}
    }

    if (!nret) {
	bu_vls_sprintf(&fname, "%d-invalid_normals_mesh.plot3", f_id);
	tris_plot(bu_vls_cstr(&fname));
	bu_vls_sprintf(&fname, "%d-invalid_normals.cdtmesh", f_id);
	serialize(bu_vls_cstr(&fname));
    }

    boundary_edges_update();
    if (problem_edges.size() > 0) {
	std::cout << "Still have problem edges in mesh\n";
	eret = false;
    }

    if (!eret) {
	bu_vls_sprintf(&fname, "%d-invalid_normals_mesh.plot3", f_id);
	tris_plot(bu_vls_cstr(&fname));
	bu_vls_sprintf(&fname, "%d-invalid_edges.cdtmesh", f_id);
	serialize(bu_vls_cstr(&fname));
	bu_vls_sprintf(&fname, "%d-invalid_edges_boundary.plot3", f_id);
	boundary_edges_plot(bu_vls_cstr(&fname));
    }


    bu_vls_free(&fname);
    return (nret && eret);
}

void cdt_mesh_t::plot_uedge(struct uedge_t &ue, FILE* plot_file)
{
    ON_3dPoint *p1 = this->pnts[ue.v[0]];
    ON_3dPoint *p2 = this->pnts[ue.v[1]];
    point_t bnp1, bnp2;
    VSET(bnp1, p1->x, p1->y, p1->z);
    VSET(bnp2, p2->x, p2->y, p2->z);
    pdv_3move(plot_file, bnp1);
    pdv_3cont(plot_file, bnp2);
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
	plot_uedge(ue, plot_file);
    }

    if (this->problem_edges.size()) {
	pl_color(plot_file, 255, 0, 0);
	for (b_it = problem_edges.begin(); b_it != problem_edges.end(); b_it++) {
	    uedge_t ue = *b_it;
	    plot_uedge(ue, plot_file);
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
    pl_color_buc(plot, buc);
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

void cdt_mesh_t::tris_plot(const char *filename)
{
    this->tris_set_plot(this->tris, filename);
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

    sfile << "V1\n";

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


    sfile << "TRIANGLES " << tris.size() << "\n";

    std::set<triangle_t>::iterator t_it;
    for (t_it = tris.begin(); t_it != tris.end(); t_it++) {
	sfile << (*t_it).v[0] << "," << (*t_it).v[1] << "," << (*t_it).v[2] << "\n";
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
    tris.clear();

    uedges2tris.clear();
    seed_tris.clear();

    v2edges.clear();
    v2tris.clear();
    edges2tris.clear();

    // When loading a serialization, we don't have a parent Brep structure
    edge_pnts = NULL;
    b_edges = NULL;
    singularities = NULL;
    normalmap = NULL;

    brep_edges.clear();
    ep.clear();
    sv.clear();

    boundary_edges.clear();
    boundary_edges_stale = true;
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

	if (dtype == std::string("TRIANGLES")) {
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
		triangle_t t(v1, v2, v3);
		tri_add(t);
	    }
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

} // close namespace cdt_mesh

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
