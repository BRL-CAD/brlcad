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
cpolygon_t::closed()
{
    if (poly.size() < 3) {
	return false;
    }

    if (flipped_face.size()) {
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
  
    if (v == pe->v[0]) {
	bu_free(polypnts, "polyline");
	return false;
    }

    V2MOVE(polypnts[pind], pnts_2d[pe->v[0]]);
   
    // Walk the loop
    while (first != next) {
	pind++;
	if (v == next->v[0]) {
	    bu_free(polypnts, "polyline");
	    return false;
	}

	V2MOVE(polypnts[pind], pnts_2d[next->v[0]]);
	next = next->next;
    }

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

    int *opoly = (int *)bu_calloc(poly.size(), sizeof(int), "polygon points");

    size_t vcnt = 0;
    cpolyedge_t *pe = (*poly.begin());
    cpolyedge_t *first = pe;
    cpolyedge_t *next = pe->next;

    opoly[vcnt] = pe->v[0];

    // Walk the loop - an infinite loop is not closed
    while (first != next) {
	vcnt++;
	opoly[vcnt] = next->v[0];
	next = next->next;
    }

    bool result = (bool)!bg_nested_polygon_triangulate( &faces, &num_faces,
	    NULL, NULL, opoly, vcnt, NULL, NULL, 0, steiner,
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
    if (!closed()) {
	std::cerr << "Polygon not closed\n";
	return;
    }

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

    while (ecurr != efirst) {
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

void
csweep_t::cull_problem_tris(std::set<triangle_t> *tcandidates)
{
    std::set<triangle_t> gtris;
    if (!tcandidates) {
	return;
    }

    std::set<triangle_t>::iterator tc_it;
    for (tc_it = tcandidates->begin(); tc_it != tcandidates->end(); tc_it++) {
	if (cmesh->problem_triangles.find(*tc_it) == cmesh->problem_triangles.end()) {
	    gtris.insert(*tc_it);
	}
    }

    tcandidates->clear();

    (*tcandidates) = gtris;
}

std::set<triangle_t>
csweep_t::non_visited_tris(std::set<long> &verts)
{
    std::set<triangle_t> result;
    if (!verts.size()) {
	return result;
    }
    std::set<long>::iterator v_it;

    for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	std::vector<triangle_t> faces = cmesh->vertex_face_neighbors(*v_it);

	// TODO - we need to constrain this set - we're looking for triangles
	// near the original triangle, but a vertex triangle set may potentially
	// pull in a large number of other triangles.  Maybe the subset that
	// either share an edge with the poly or failing that the closest to
	// the original seed triangle?  Or, maybe check the directions of the
	// edges and only accept those that are least far from the center of the
	// seed?  Maybe prefer triangles with more points on the polyline or
	// (pre-polyline) more interior points?

	for (size_t i = 0; i < faces.size(); i++) {
	    if (visited_triangles.find(faces[i]) == visited_triangles.end()) {
		result.insert(faces[i]);
	    }
	}
    }

    return result;
}

std::set<triangle_t>
csweep_t::polygon_tris(double angle)
{
    std::set<triangle_t> result;

    std::set<cpolyedge_t *>::iterator p_it;
    for (p_it = polygon.poly.begin(); p_it != polygon.poly.end(); p_it++) {
	cpolyedge_t *pe = *p_it;
	struct uedge_t ue(pe->v[0], pe->v[1]);
	std::set<triangle_t> petris = cmesh->uedges2tris[ue];
	std::set<triangle_t>::iterator t_it;
	for (t_it = petris.begin(); t_it != petris.end(); t_it++) {
	    ON_3dVector tn = cmesh->bnorm(*t_it);
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
    // TODO - do an initial pass to check for uncontained points that are actually interior.


    if (stop_on_contained && !polygon.uncontained.size()) {
	return 0;
    }

    if (deg < 0 || deg > 170) {
	return -1;
    }

    double angle = deg * ON_PI/180.0;

    // First step - collect all the unvisited triangles from the polyline edges.

    std::queue<triangle_t> tq;

    std::set<edge_t> flipped_edges;

    std::set<triangle_t> ptris = polygon_tris(angle);



    std::set<triangle_t>::iterator f_it;
    for (f_it = ptris.begin(); f_it != ptris.end(); f_it++) {
	tq.push(*f_it);
    }

    while (!tq.empty()) {

	triangle_t ct = tq.front();
	tq.pop();

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

	if (new_edge_cnt == -2) {
	    // Vert from bad edges - added to uncontained.  Start over with another triangle - we
	    // need to walk out until this point is swept up by the polygon.
	    visited_triangles.insert(ct);
	    continue;
	}

	if (new_edge_cnt == -1) {
	    // If the triangle shares one edge but all three vertices are on the
	    // polygon, we can't use this triangle at this time - it would produce
	    // a self-intersecting polygon.  Don't mark it as visited however, as
	    // it may be pulled back in successfully in later processing.
	    continue;
	}

	if (new_edge_cnt == 2 && flipped_tri) {
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

	if (new_edge_cnt <= 0 || new_edge_cnt > 2) {
	    std::cerr << "fatal shared triangle filter error!\n";
	    return -1;
	}

	polygon.replace_edges(new_edges, shared_edges);
	visited_triangles.insert(ct);


	if (!polygon.have_uncontained()) {
	    std::cout << "In principle, we now have a workable subset\n";
	    polygon.polygon_plot("test.plot3");

	    polygon.cdt();

	    exit(0);
	}

	if (tq.empty()) {
	    // That's all the triangles from this ring - if we haven't
	    // terminated yet, pull the next triangle set.

	    // TODO - we may want to be more selective about what triangles
	    // we queue up - i.e. if we have known uncontained points we're
	    // trying to contain, skip triangles that are moving away from
	    // the uncontained points (don't want to pull in additional
	    // problem regions unnecessarily - it is possible to grow a subset
	    // to a point where it can't be projected.)

	    std::set<triangle_t> ntris = polygon_tris(angle);
	    for (f_it = ntris.begin(); f_it != ntris.end(); f_it++) {
		tq.push(*f_it);
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
csweep_t::build_initial_loop(triangle_t &seed)
{
    std::set<uedge_t>::iterator u_it;

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

    // TODO - do we need this?
    std::set<uedge_t> tedges = cmesh->uedges(seed);
    for (u_it = tedges.begin(); u_it != tedges.end(); u_it++) {
	// The exception to this is Brep boundary edges - they are never
	// interior.  Note that one point on the edge isn't enough - the
	// whole edge must be on the brep edge.
	if (cmesh->brep_edge_pnt((*u_it).v[0]) && cmesh->brep_edge_pnt((*u_it).v[1])) {
	    continue;
	}
	interior_uedges.insert(*u_it);
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
