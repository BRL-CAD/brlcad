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
#include "bu/malloc.h"
//#include "bn/mat.h" /* bn_vec_perp */
#include "bn/plot3.h"
#include "bg/polygon.h"
#include "./cmesh.h"

// needed for implementation
#include <iostream>
#include <queue>

namespace cmesh
{

long
cpolygon_t::add_edge(const struct edge_t &e)
{
    if (e.v[0] == -1) return -1;

    int v1 = -1;
    int v2 = -1;

    std::set<cpolyedge_t *>::iterator cp_it;
    for (cp_it = poly.begin(); cp_it != poly.end(); cp_it++) {
	cpolyedge_t *pe = *cp_it;

	if (pe->v[1] == e.v[0]) {
	    v1 = e.v[0];
	}

	if (pe->v[1] == e.v[1]) {
	    v1 = e.v[1];
	}

	if (pe->v[0] == e.v[0]) {
	    v2 = e.v[0];
	}

	if (pe->v[0] == e.v[1]) {
	    v2 = e.v[1];
	}
    }

    if (v1 == -1) {
	v1 = (e.v[0] == v2) ? e.v[1] : e.v[0];
    }

    if (v2 == -1) {
	v2 = (e.v[0] == v1) ? e.v[1] : e.v[0];
    }

    struct edge_t le(v1, v2);
    cpolyedge_t *nedge = new cpolyedge_t(le);
    poly.insert(nedge);

    v2pe[v1].insert(nedge);
    v2pe[v2].insert(nedge);

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


    return 0;
}

void
cpolygon_t::remove_edge(const struct edge_t &e)
{
    struct uedge_t ue(e);
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

    v2pe[e.v[0]].erase(cull);
    v2pe[e.v[1]].erase(cull);

    // An edge removal may produce a new interior point candidate - check
    // Will need to verify eventually with point_in_polygon, but topologically
    // it may be cut loose
    for (int i = 0; i < 2; i++) {
	if (!v2pe[e.v[i]].size()) {
	    if (flipped_face.find(e.v[i]) != flipped_face.end()) {
		flipped_face.erase(e.v[i]);
	    }
	    uncontained.insert(e.v[i]);
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

long
cpolygon_t::replace_edges(std::set<edge_t> &new_edges, std::set<edge_t> &old_edges)
{

    std::set<edge_t>::iterator e_it;
    for (e_it = old_edges.begin(); e_it != old_edges.end(); e_it++) {
	remove_edge(*e_it);
    }
    for (e_it = new_edges.begin(); e_it != new_edges.end(); e_it++) {
	add_edge(*e_it);
    }

    return 0;
}

bool
cpolygon_t::self_intersecting()
{
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
	    uncontained.insert(v_it->second);
	}
    }

    // Check the projected segments against each other as well
    std::vector<cpolyedge_t *> pv(poly.begin(), poly.end());
    for (size_t i = 0; i < pv.size(); i++) {
	cpolyedge_t *pe1 = pv[i];
	ON_2dPoint p1_1(pnts_2d[pe1->v[0]][X], pnts_2d[pe1->v[0]][Y]);
	ON_2dPoint p1_2(pnts_2d[pe1->v[1]][X], pnts_2d[pe1->v[1]][Y]);
	ON_Line e1(p1_1, p1_2);
	for (size_t j = i+1; j < pv.size(); j++) {
	    cpolyedge_t *pe2 = pv[j];
	    ON_2dPoint p2_1(pnts_2d[pe2->v[0]][X], pnts_2d[pe2->v[0]][Y]);
	    ON_2dPoint p2_2(pnts_2d[pe2->v[1]][X], pnts_2d[pe2->v[1]][Y]);
	    ON_Line e2(p2_1, p2_2);

	    double a, b = 0;
	    if (!ON_IntersectLineLine(e1, e2, &a, &b, 0.0, false)) {
		continue;
	    }

	    if ((a < 0 || NEAR_ZERO(a, ON_ZERO_TOLERANCE) || a > 1 || NEAR_ZERO(1-a, ON_ZERO_TOLERANCE)) ||
		    (b < 0 || NEAR_ZERO(b, ON_ZERO_TOLERANCE) || b > 1 || NEAR_ZERO(1-b, ON_ZERO_TOLERANCE))) {
		continue;
	    } else {
		std::cout << "a: " << a << ", b: " << b << "\n";
	    }

	    self_isect = true;
	}
    }
#if 0
    if (self_isect) {
	std::cout << "Polygon reports self-intersecting\n";
	polygon_plot("self_isect.plot3");
    }
#endif

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
cpolygon_t::point_in_polygon(long v)
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

    V2MOVE(polypnts[pind], pnts_2d[pe->v[0]]);
    pind++;
    V2MOVE(polypnts[pind], pnts_2d[pe->v[1]]);

    // Walk the loop
    while (first != next) {
	pind++;
	if (v == next->v[0] || v == next->v[1]) {
	    bu_free(polypnts, "polyline");
	    return false;
	}
	V2MOVE(polypnts[pind], pnts_2d[next->v[1]]);
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

    polygon_plot("poly_2d.plot3");
    bg_polygon_plot_2d("pnt_in_poly_loop.plot3", polypnts, pind, 255, 0, 0);

    point2d_t test_pnt;
    V2MOVE(test_pnt, pnts_2d[v]);

    bool result = (bool)bg_pt_in_polygon(pind, (const point2d_t *)polypnts, (const point2d_t *)&test_pnt);

    bu_free(polypnts, "polyline");

    return result;
}

bool
cpolygon_t::cdt()
{
    if (!closed()) return false;
    int *faces = NULL;
    int num_faces = 0;
    int *steiner = NULL;
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
    }

    bool result = (bool)!bg_nested_polygon_triangulate( &faces, &num_faces,
	    NULL, NULL, opoly, poly.size()+1, NULL, NULL, 0, steiner,
	    interior_points.size(), pnts_2d, cmesh->pnts.size(),
	    TRI_CONSTRAINED_DELAUNAY);

    if (result) {
	for (int i = 0; i < num_faces; i++) {
	    triangle_t t;
	    t.v[0] = faces[3*i+0];
	    t.v[1] = faces[3*i+1];
	    t.v[2] = faces[3*i+2];

	    ON_3dVector tdir = cmesh->tnorm(t);
	    ON_3dVector bdir = cmesh->bnorm(t);
	    bool flipped_tri = (ON_DotProduct(tdir, bdir) < 0);
	    if (flipped_tri) {
		t.v[2] = faces[3*i+1];
		t.v[1] = faces[3*i+2];
	    }

	    tris.insert(t);
	}

	bu_free(faces, "faces array");
    }

    bu_free(opoly, "polygon points");

    if (steiner) {
	bu_free(steiner, "faces array");
    }

    return result;
}

// TODO - there is another case we need to handle here - when the candidate
// triangle OVERLAPS an existing visited triangle in the projection, but
// its normal indicates it should not (i.e. it's not overlapping in the
// projection by "wrapping around" the mesh on the other side of the object.)
// In that case we mark it as visited and either a. keep going if all the
// triangle verts are on the polygon already or b. mark it's non-polygon
// vert as uncontained - that vertex is something we need to contain before
// the polygon is viable.
long
cpolygon_t::tri_process(std::set<edge_t> *ne, std::set<edge_t> *se, long *nv, triangle_t &t)
{
    std::set<cpolyedge_t *>::iterator pe_it;

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

    long shared_cnt = 0;
    for (int i = 0; i < 3; i++) {
	if (e_shared[i]) {
	    shared_cnt++;
	    se->insert(e[i]);
	} else {
	    ne->insert(e[i]);
	}
    }

    if (shared_cnt == 1) {
	bool v_shared[3];
	for (int i = 0; i < 3; i++) {
	    v_shared[i] = false;
	}
	// If we've got only one shared edge, there should be a vertex not currently
	// involved with the loop - verify that, and if it's true report it.
	int vshared_cnt = 0;
	for (int i = 0; i < 3; i++) {
	    for (pe_it = poly.begin(); pe_it != poly.end(); pe_it++) {
		cpolyedge_t *pe = *pe_it;
		if (pe->v[0] == t.v[i] || pe->v[1] == t.v[i]) {
		    v_shared[i] = true;
		    vshared_cnt++;
		    break;
		}
	    }
	}
	if (vshared_cnt == 2) {
	    for (int i = 0; i < 3; i++) {
		if (v_shared[i] == false) {
		    (*nv) = t.v[i];
		    break;
		}
	    }

	    // If the uninvolved point is associated with bad edges, we can't use
	    // any of this to build the loop - it gets added to the uncontained
	    // points set, and we move on.
	    int vert_problem_cnt = 0;
	    std::set<uedge_t>::iterator p_it;
	    for (p_it = problem_edges.begin(); p_it != problem_edges.end(); p_it++) {
		if (((*p_it).v[0] == *nv) || ((*p_it).v[1] == *nv)) {
		    vert_problem_cnt++;
		}
	    }

	    if (vert_problem_cnt > 1) {
		uncontained.insert(*nv);
		(*nv) = -1;
		se->clear();
		ne->clear();
		return -2;
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
	std::set<edge_t>::iterator se_it;
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
    VSET(bnp, pnts_2d[efirst->v[0]][X], pnts_2d[efirst->v[0]][Y], 0);
    pdv_3move(plot_file, bnp);
    VSET(bnp, pnts_2d[efirst->v[1]][X], pnts_2d[efirst->v[1]][Y], 0);
    pdv_3cont(plot_file, bnp);

    V2MINMAX(pmin, pmax, pnts_2d[efirst->v[0]]);
    V2MINMAX(pmin, pmax, pnts_2d[efirst->v[1]]);

    size_t ecnt = 1;
    while (ecurr != efirst && ecnt < poly.size()+1) {
	ecurr = (!ecurr) ? efirst->next : ecurr->next;
	VSET(bnp, pnts_2d[ecurr->v[1]][X], pnts_2d[ecurr->v[1]][Y], 0);
	pdv_3cont(plot_file, bnp);
	V2MINMAX(pmin, pmax, pnts_2d[efirst->v[1]]);
    }

    // Plot interior and uncontained points as well
    double r = DIST_PT2_PT2(pmin, pmax) * 0.01;
    std::set<long>::iterator p_it;

    // Interior
    pl_color(plot_file, 0, 255, 0);
    for (p_it = interior_points.begin(); p_it != interior_points.end(); p_it++) {
	point_t origin;
	VSET(origin, pnts_2d[*p_it][X], pnts_2d[*p_it][Y], 0);
	pdv_3move(plot_file, origin);
	VSET(bnp, pnts_2d[*p_it][X]+r, pnts_2d[*p_it][Y]+r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, pnts_2d[*p_it][X]+r, pnts_2d[*p_it][Y]-r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, pnts_2d[*p_it][X]-r, pnts_2d[*p_it][Y]-r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, pnts_2d[*p_it][X]-r, pnts_2d[*p_it][Y]+r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
    }

    // Uncontained
    pl_color(plot_file, 255, 0, 0);
    for (p_it = uncontained.begin(); p_it != uncontained.end(); p_it++) {
	point_t origin;
	VSET(origin, pnts_2d[*p_it][X], pnts_2d[*p_it][Y], 0);
	pdv_3move(plot_file, origin);
	VSET(bnp, pnts_2d[*p_it][X]+r, pnts_2d[*p_it][Y]+r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, pnts_2d[*p_it][X]+r, pnts_2d[*p_it][Y]-r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, pnts_2d[*p_it][X]-r, pnts_2d[*p_it][Y]-r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
	VSET(bnp, pnts_2d[*p_it][X]-r, pnts_2d[*p_it][Y]+r, 0);
	pdv_3cont(plot_file, bnp);
	pdv_3cont(plot_file, origin);
    }

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
	std::cout << "->" << next->v[0];
	visited.insert(next);
	next = next->next;
	if (ecnt > poly.size()) {
	    std::cout << " ERROR infinite loop\n";
	    return;
	}
    }

    std::cout << "\n";

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
    if (!uncontained.size()) return true;

    std::set<long> mvpnts;

    for (u_it = uncontained.begin(); u_it != uncontained.end(); u_it++) {
	if (point_in_polygon(*u_it)) {
	    mvpnts.insert(*u_it);
	}
    }
    for (u_it = mvpnts.begin(); u_it != mvpnts.end(); u_it++) {
	uncontained.erase(*u_it);
	interior_points.insert(*u_it);
    }

    return (uncontained.size() > 0) ? true : false;
}

std::set<triangle_t>
csweep_t::polygon_tris(double angle, bool brep_norm)
{
    std::set<triangle_t> result;

    std::set<cpolyedge_t *>::iterator p_it;
    for (p_it = polygon.poly.begin(); p_it != polygon.poly.end(); p_it++) {
	cpolyedge_t *pe = *p_it;
	struct uedge_t ue(pe->v[0], pe->v[1]);
	std::set<triangle_t> petris = cmesh->uedges2tris[ue];
	std::set<triangle_t>::iterator t_it;
	for (t_it = petris.begin(); t_it != petris.end(); t_it++) {
	    ON_3dVector tn = (brep_norm) ? cmesh->bnorm(*t_it) : cmesh->tnorm(*t_it);
	    double dprd = ON_DotProduct(pdir, tn);
	    double dang = (NEAR_EQUAL(dprd, 1.0, ON_ZERO_TOLERANCE)) ? 0 : acos(dprd);
	    if (dang > angle) {
		std::cerr << "Angle rejection (" << dang << "," << angle << ")\n";
		continue;
	    }
	    if (visited_triangles.find(*t_it) != visited_triangles.end()) {
	       	continue;
	    }
	    result.insert(*t_it);
	}
    }

    return result;
}

long
csweep_t::grow_loop(double deg, bool stop_on_contained)
{
    double angle = deg * ON_PI/180.0;

    if (stop_on_contained && !polygon.uncontained.size()) {
	return 0;
    }

    if (deg < 0 || deg > 170) {
	return -1;
    }

    // First step - collect all the unvisited triangles from the polyline edges.

    std::queue<triangle_t> tq;

    std::set<edge_t> flipped_edges;

    std::set<triangle_t> ptris = polygon_tris(angle, stop_on_contained);

    if (!ptris.size()) {
	std::cout << "No triangles available??\n";
	return -1;
    }

    std::set<triangle_t>::iterator f_it;

    for (f_it = ptris.begin(); f_it != ptris.end(); f_it++) {
	tq.push(*f_it);
    }

    while (!tq.empty()) {

	triangle_t ct = tq.front();
	tq.pop();
	polygon.polygon_plot("poly_2d.plot3");

	// A triangle will introduce at most one new point into the loop.  If
	// the triangle is bad, it will define uncontained interior points and
	// potentially (if it has unmated edges) won't grow the polygon at all.

	// The first thing to do is find out of the triangle shares one or two
	// edges with the loop.  (0 or 3 would indicate something Very Wrong...)
	std::set<edge_t> new_edges;
	std::set<edge_t> shared_edges;
	std::set<edge_t>::iterator ne_it;
	long vert = -1;
	long new_edge_cnt = polygon.tri_process(&new_edges, &shared_edges, &vert, ct);

	ON_3dVector tdir = cmesh->tnorm(ct);
	ON_3dVector bdir = cmesh->bnorm(ct);
	bool flipped_tri = (ON_DotProduct(tdir, bdir) < 0);

	bool process = true;

	if (new_edge_cnt == -2) {
	    // Vert from bad edges - added to uncontained.  Start over with another triangle - we
	    // need to walk out until this point is swept up by the polygon.
	    visited_triangles.insert(ct);
	    process = false;
	}

	if (new_edge_cnt == -1) {
	    // If the triangle shares one edge but all three vertices are on the
	    // polygon, we can't use this triangle at this time - it would produce
	    // a self-intersecting polygon.  Don't mark it as visited however, as
	    // it may be pulled back in successfully in later processing.
	    process = false;
	}

	if (process) {
	    if (stop_on_contained && new_edge_cnt == 2 && flipped_tri) {
		// It is possible that a flipped face adding two new edges will
		// make a mess out of the polygon (i.e. make it self intersecting.)
		// Tag it so we know we can't trust point_in_polygon until we've grown
		// the vertex out of flipped_face (remove_edge will handle that.)

		// If we have a flipped triangle vert involving a brep edge vertex... um...
		if (cmesh->brep_edge_pnt(vert)) {
		    std::cerr << "URK! flipped face vertex on edge!\n";
		    return -1;
		} else {
		    polygon.flipped_face.insert(vert);
		}
	    }

	    if (!(polygon.poly.size() == 3 && polygon.interior_points.size())) {
		if (stop_on_contained && new_edge_cnt == 2 && !flipped_tri) {
		    // If this is a good triangle and we're in repair mode, don't add it unless
		    // it uses or points in the direction of at least one uncontained point.
		    int use_tri = 0;
		    for (int i = 0; i < 3; i++) {
			if (polygon.uncontained.find(ct.v[i]) != polygon.uncontained.end()) {
			    use_tri = 1;
			    break;
			}
		    }
		    if (!use_tri) {
			struct edge_t e = (*shared_edges.begin());
			ON_2dPoint p2d = (ON_2dPoint(polygon.pnts_2d[e.v[0]][X], polygon.pnts_2d[e.v[0]][Y]) + ON_2dPoint(polygon.pnts_2d[e.v[1]][X], polygon.pnts_2d[e.v[1]][Y])) * 0.5;
			ON_2dPoint np2d(polygon.pnts_2d[vert][X], polygon.pnts_2d[vert][Y]);
			ON_3dVector vt = np2d - p2d;
			std::set<long>::iterator u_it;
			for (u_it = polygon.uncontained.begin(); u_it != polygon.uncontained.end(); u_it++) {
			    ON_2dPoint up2d(polygon.pnts_2d[*u_it][X], polygon.pnts_2d[*u_it][Y]);
			    ON_3dVector vu = up2d - p2d;
			    if (ON_DotProduct(vu, vt) >= 0) {
				use_tri = 1;
				break;
			    }
			}
		    }

		    if (!use_tri) {
			continue;
		    }
		}
	    }

	    if (new_edge_cnt <= 0 || new_edge_cnt > 2) {
		std::cerr << "fatal loop growth error!\n";
		return -1;
	    }

	    polygon.replace_edges(new_edges, shared_edges);
	    visited_triangles.insert(ct);
	}

	bool h_uc = polygon.have_uncontained();

	if (stop_on_contained && !h_uc && polygon.poly.size() > 3) {
	    std::cout << "In principle, we now have a workable subset\n";

	    //polygon.polygon_plot("poly_2d.plot3");
	    //
	    polygon.cdt();

	    //cmesh->tris_set_plot(polygon.tris, "tri_2d.plot3");

	    return (long)cmesh->tris.size();
	}

	//polygon.polygon_plot("poly_2d.plot3");

	if (tq.empty()) {
	    // That's all the triangles from this ring - if we haven't
	    // terminated yet, pull the next triangle set.

	    std::set<triangle_t> ntris = polygon_tris(angle, stop_on_contained);
	    for (f_it = ntris.begin(); f_it != ntris.end(); f_it++) {
		if (visited_triangles.find(*f_it) == visited_triangles.end()) {
		    tq.push(*f_it);
		}
	    }

	    if (!stop_on_contained && tq.empty()) {
		// per the current angle criteria we've got everything, and we're
		// not concerned with contained points so this isn't an indication
		// of an error condition.  Generate triangles.
		std::cout << "In principle, we now have a workable patch\n";
		polygon.polygon_plot("poly_2d.plot3");

		polygon.cdt();

		cmesh->tris_set_plot(polygon.tris, "patch.plot3");

		return (long)cmesh->tris.size();
	    }

	}
    }

    return -1;
}

void
csweep_t::build_2d_pnts(ON_3dPoint &c, ON_3dVector &n)
{
    pdir = n;
    ON_Plane tri_plane(c, n);
    ON_Xform to_plane;
    to_plane.PlanarProjection(tplane);
    polygon.pnts_2d = (point2d_t *)bu_calloc(cmesh->pnts.size() + 1, sizeof(point2d_t), "2D points array");
    for (size_t i = 0; i < cmesh->pnts.size(); i++) {
	ON_3dPoint op3d = (*cmesh->pnts[i]);
	op3d.Transform(to_plane);
	polygon.pnts_2d[i][X] = op3d.x;
	polygon.pnts_2d[i][Y] = op3d.y;
    }
    tplane = tri_plane;

    polygon.cmesh = cmesh;
}


bool
csweep_t::build_initial_loop(triangle_t &seed, bool repair)
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
	    if (cmesh->brep_edge_pnt(seed.v[i])) {
		continue;
	    }
	    polygon.uncontained.insert(seed.v[i]);
	}

	// We need a initial valid polygon loop to grow.  Poll the neighbor faces - if one
	// of them is valid, it will be used to build an initial loop
	std::vector<triangle_t> faces = cmesh->face_neighbors(seed);
	for (size_t i = 0; i < faces.size(); i++) {
	    triangle_t t = faces[i];
	    if (cmesh->problem_triangles.find(t) == cmesh->problem_triangles.end()) {
		struct edge_t e1(t.v[0], t.v[1]);
		struct edge_t e2(t.v[1], t.v[2]);
		struct edge_t e3(t.v[2], t.v[0]);
		polygon.add_edge(e1);
		polygon.add_edge(e2);
		polygon.add_edge(e3);
		visited_triangles.insert(t);
		return polygon.closed();
	    }
	}

	// If we didn't find a valid mated edge triangle (urk?) try the vertices
	for (int i = 0; i < 3; i++) {
	    std::vector<triangle_t> vfaces = cmesh->vertex_face_neighbors(seed.v[i]);
	    for (size_t j = 0; j < vfaces.size(); j++) {
		triangle_t t = vfaces[j];
		if (cmesh->problem_triangles.find(t) == cmesh->problem_triangles.end()) {
		    struct edge_t e1(t.v[0], t.v[1]);
		    struct edge_t e2(t.v[1], t.v[2]);
		    struct edge_t e3(t.v[2], t.v[0]);
		    polygon.add_edge(e1);
		    polygon.add_edge(e2);
		    polygon.add_edge(e3);
		    visited_triangles.insert(t);
		    return polygon.closed();
		}
	    }
	}

	// NONE of the triangles in the neighborhood are valid?  We'll have to hope that
	// subsequent processing of other seeds will put a proper mesh in contact with
	// this face...
	return false;

    }

    // We're not repairing - start with the seed itself
    struct edge_t e1(seed.v[0], seed.v[1]);
    struct edge_t e2(seed.v[1], seed.v[2]);
    struct edge_t e3(seed.v[2], seed.v[0]);
    polygon.add_edge(e1);
    polygon.add_edge(e2);
    polygon.add_edge(e3);
    visited_triangles.insert(seed);
    return polygon.closed();
}

void csweep_t::plot_uedge(struct uedge_t &ue, FILE* plot_file)
{
    point_t bnp1, bnp2;
    VSET(bnp1, polygon.pnts_2d[ue.v[0]][X], polygon.pnts_2d[ue.v[0]][Y], 0);
    VSET(bnp2, polygon.pnts_2d[ue.v[1]][X], polygon.pnts_2d[ue.v[1]][Y], 0);
    pdv_3move(plot_file, bnp1);
    pdv_3cont(plot_file, bnp2);
}


void csweep_t::plot_tri(const triangle_t &t, struct bu_color *buc, FILE *plot, int r, int g, int b)
{
    point_t p[3];
    point_t porig;
    point_t c = VINIT_ZERO;

    for (int i = 0; i < 3; i++) {
	VSET(p[i], polygon.pnts_2d[t.v[i]][X], polygon.pnts_2d[t.v[i]][Y], 0);
	c[X] += p[i][X];
	c[Y] += p[i][Y];
    }
    c[X] = c[X]/3.0;
    c[Y] = c[Y]/3.0;
    c[Z] = 0;

    for (size_t i = 0; i < 3; i++) {
	if (i == 0) {
	    VMOVE(porig, p[i]);
	    pdv_3move(plot, p[i]);
	}
	pdv_3cont(plot, p[i]);
    }
    pdv_3cont(plot, porig);

    /* fill in the "interior" using the rgb color*/
    pl_color(plot, r, g, b);
    for (size_t i = 0; i < 3; i++) {
	pdv_3move(plot, p[i]);
	pdv_3cont(plot, c);
    }


    /* Plot the triangle normal */
    pl_color(plot, 0, 255, 255);
    {
	ON_3dVector tn = cmesh->tnorm(t);
	vect_t tnt;
	VSET(tnt, tn.x, tn.y, tn.z);
	point_t npnt;
	VADD2(npnt, tnt, c);
	pdv_3move(plot, c);
	pdv_3cont(plot, npnt);
    }

    /* Plot the brep normal */
    pl_color(plot, 0, 100, 0);
    {
	ON_3dVector tn = cmesh->bnorm(t);
	tn = tn * 0.5;
	vect_t tnt;
	VSET(tnt, tn.x, tn.y, tn.z);
	point_t npnt;
	VADD2(npnt, tnt, c);
	pdv_3move(plot, c);
	pdv_3cont(plot, npnt);
    }

    /* restore previous color */
    pl_color_buc(plot, buc);
}

void csweep_t::face_neighbors_plot(const triangle_t &f, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    // Origin triangle has red interior
    std::vector<triangle_t> faces = cmesh->face_neighbors(f);
    plot_tri(f, &c, plot_file, 255, 0, 0);

    // Neighbor triangles have blue interior
    for (size_t i = 0; i < faces.size(); i++) {
	triangle_t tri = faces[i];
	plot_tri(tri, &c, plot_file, 0, 0, 255);
    }

    fclose(plot_file);
}

void csweep_t::vertex_face_neighbors_plot(long vind, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");

    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    std::vector<triangle_t> faces = cmesh->vertex_face_neighbors(vind);

    for (size_t i = 0; i < faces.size(); i++) {
	triangle_t tri = faces[i];
	plot_tri(tri, &c, plot_file, 0, 0, 255);
    }

    // Plot the vind point that is the source of the triangles
    pl_color(plot_file, 0, 255,0);
    point2d_t p;
    V2MOVE(p, polygon.pnts_2d[vind]);
    pd_point(plot_file, p[X], p[Y]);
    fclose(plot_file);
}



}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
