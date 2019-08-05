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
//#include "bn/mat.h" /* bn_vec_perp */
#include "bn/plot3.h"
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
    cpolyedge_t *cull = NULL;
    std::set<cpolyedge_t *>::iterator cp_it;
    for (cp_it = poly.begin(); cp_it != poly.end(); cp_it++) {
	cpolyedge_t *pe = *cp_it;
	if (pe->v[0] == e.v[0] && pe->v[1] == e.v[1]) {
	    // Existing segment with this ending vertex exists
	    cull = pe;
	}
    }

    if (cull) {
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
}

long
cpolygon_t::add_edges(std::set<edge_t> &new_edges, std::set<edge_t> &shared_edges)
{

    std::set<edge_t>::iterator e_it;
    for (e_it = shared_edges.begin(); e_it != shared_edges.end(); e_it++) {
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

    std::set<cpolyedge_t *>::iterator cp_it;
    for (cp_it = poly.begin(); cp_it != poly.end(); cp_it++) {
	cpolyedge_t *pe = *cp_it;
	if (!pe->prev || !pe->next) {
	    return false;
	}
    }

    return true;
}

bool
cpolygon_t::point_in_polygon(long v)
{
    if (v == -1) return false;

    return false;
}

std::vector<long>
cpolygon_t::polyvect()
{
    std::vector<long> result;
    return result;
}

std::set<long>
cpolygon_t::dangling_vertices()
{
    std::set<long> result;
    return result;
}

std::set<long>
csweep_t::non_interior_verts(const triangle_t &t)
{
    std::set<long> result;
    if (t.v[0] == -1) {
	return result;
    }

    return result;

}

std::set<edge_t>
csweep_t::tri_ext_edges(const triangle_t &t)
{
    std::set<edge_t> result;
    if (t.v[0] == -1) {
	return result;
    }

    return result;
}

std::set<edge_t>
csweep_t::exterior_edges(std::set<triangle_t> &tris)
{
    std::set<edge_t> result;
    if (!tris.size()) {
	return result;
    }

    std::set<triangle_t>::iterator t_it;
    for (t_it = tris.begin(); t_it != tris.end(); t_it++) {
	triangle_t t = (*t_it);
	struct edge_t e[3];
	e[0].set(t.v[0], t.v[1]);
	e[1].set(t.v[1], t.v[2]);
	e[2].set(t.v[2], t.v[0]);
	struct uedge_t ue[3];
	for (int i = 0; i < 3; i++) {
	    ue[i].set(e[i].v[0], e[i].v[1]);
	}

	for (int i = 0; i < 3; i++) {
	    bool v1int = (interior_points.find(e[i].v[0]) != interior_points.end());
	    bool v2int = (interior_points.find(e[i].v[1]) != interior_points.end());
	    if (v1int || v2int) {
		interior_uedges.insert(ue[i]);
	    } else {
		result.insert(e[i]);
	    }
	}

    }

    return result;
}

long
cpolygon_t::tri_shared_filter(std::set<edge_t> *ne, std::set<edge_t> *se, long *nv, triangle_t &t)
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
	} else {
	    (*nv) = -1;
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

bool
csweep_t::interior_points_contained()
{
    return false;
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
    if (stop_on_contained && !uncontained.size()) {
	return 0;
    }

    if (deg < 0 || deg > 170) {
	return -1;
    }

    double angle = deg * ON_PI/180.0;

    // First step - collect all the unvisited triangles from the polyline edges.

    std::queue<triangle_t> q1, q2;
    std::queue<triangle_t> *tq, *nq;
    tq = &q1;
    nq = &q2;

    std::set<triangle_t> ptris = polygon_tris(angle);
    std::set<triangle_t>::iterator f_it;
    for (f_it = ptris.begin(); f_it != ptris.end(); f_it++) {
	q1.push(*f_it);
    }

    while (!tq->empty()) {

	triangle_t ct = tq->front();
	tq->pop();

	// A triangle will introduce at most one new point into the loop.  If
	// the triangle is bad, it will define uncontained interior points and
	// potentially (if it has unmated edges) won't grow the polygon at all.

	// The first thing to do is find out of the triangle shares one or two
	// edges with the loop.  (0 or 3 would indicate something Very Wrong...)
	std::set<edge_t> new_edges;
	std::set<edge_t> shared_edges;
	std::set<edge_t>::iterator ne_it;
	long vert = -1;
	long new_edge_cnt = polygon.tri_shared_filter(&new_edges, &shared_edges, &vert, ct);

	if (new_edge_cnt <= 0 || new_edge_cnt > 2) {
	    std::cerr << "fatal shared triangle filter error!\n";
	    return -1;
	}

	polygon.add_edges(new_edges, shared_edges);

	// If the triangle shares one edge but all three vertices are on the
	// polygon, we can't use this triangle at this time - it would produce
	// a self-intersecting polygon.  Don't mark it as visited however, as
	// it may be pulled back in successfully in later processing.


	// If triangle has a vertex not on the loop that is *inside* the current
	// boundary loop, flag it as an uncontained interior point.  Will need
	// to double check if any interior points are newly uncontained after
	// processing such a face.

#if 0

	// TODO - if triangle has problem edges, it cannot be added as a
	// triangle - its non-active vertex needs to be stored as an interior
	// point, and the mesh must grow until that 2D point is inside the 2D
	// loop.  This will usually happen quickly, but isn't guaranteed to...
	//
	// One option might be to do a post-repair re-assessment, and if any
	// new problem edges have appeared re-seed with those triangles...
	//
	// Another might be an "all or nothing" policy for uedges with more than
	// two triangles...


	// TODO -reject triangle if tri_add indicates it would self-intersect
	// the boundary loop.  Remove it from visited if that's why we're
	// rejecting it, since subsequent additions might make it viable again.
	submesh->tri_add(ct, 1);


	// TODO grow this not by vertex or face neighbors, but by getting the
	// triangle sets from the current boundary_loop's uedge segments.
	// In essence, march the boundary loop out in steps rather than 
	// once face at a time.
	{
	    std::set<triangle_t> fvneigh;
	    std::set<triangle_t>::iterator f_it;
	    for (int i = 0; i < 3; i++) {
		std::vector<triangle_t> fneigh = vertex_face_neighbors(seed.v[i]);
		fvneigh.insert(fneigh.begin(), fneigh.end());
	    }
	    for (f_it = fvneigh.begin(); f_it != fvneigh.end(); f_it++) {
		if (submesh->visited_triangles.find(*f_it) == submesh->visited_triangles.end()) {
		    nq->push(*f_it);
		    submesh->visited_triangles.insert(*f_it);
		}
	    }
	}
#endif

	if (tq->empty()) {
	    std::queue<triangle_t> *tmpq = tq;
	    tq = nq;
	    nq = tmpq;
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
    for (size_t i = 0; i < cmesh->pnts.size(); i++) {
	ON_3dPoint op3d = (*cmesh->pnts[i]);
	op3d.Transform(to_plane);
	ON_2dPoint *n2d = new ON_2dPoint(op3d.x, op3d.y);
	pnts_2d.push_back(n2d);
    }

    tplane = tri_plane;
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
	uncontained.insert(seed.v[i]);
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
    ON_2dPoint *p1 = this->pnts_2d[ue.v[0]];
    ON_2dPoint *p2 = this->pnts_2d[ue.v[1]];
    point_t bnp1, bnp2;
    VSET(bnp1, p1->x, p1->y, 0);
    VSET(bnp2, p2->x, p2->y, 0);
    pdv_3move(plot_file, bnp1);
    pdv_3cont(plot_file, bnp2);
}

void csweep_t::polygon_plot(const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
    struct bu_color c = BU_COLOR_INIT_ZERO;
    bu_color_rand(&c, BU_COLOR_RANDOM_LIGHTENED);
    pl_color_buc(plot_file, &c);

    cpolyedge_t *efirst = *(polygon.poly.begin());
    cpolyedge_t *ecurr = NULL;

    point_t bnp;
    ON_2dPoint *p = pnts_2d[efirst->v[0]];
    VSET(bnp, p->x, p->y, 0);
    pdv_3move(plot_file, bnp);
    p = pnts_2d[ecurr->v[1]];
    VSET(bnp, p->x, p->y, 0);
    pdv_3cont(plot_file, bnp);

    while (ecurr != efirst) {
	ecurr = (!ecurr) ? efirst->next : ecurr->next;
	p = pnts_2d[ecurr->v[1]];
	VSET(bnp, p->x, p->y, 0);
	pdv_3cont(plot_file, bnp);
    }
    fclose(plot_file);
}

void csweep_t::plot_tri(const triangle_t &t, struct bu_color *buc, FILE *plot, int r, int g, int b)
{
    point_t p[3];
    point_t porig;
    point_t c = VINIT_ZERO;

    for (int i = 0; i < 3; i++) {
	ON_2dPoint *p2d = this->pnts_2d[t.v[i]];
	VSET(p[i], p2d->x, p2d->y, 0);
	c[X] += p2d->x;
	c[Y] += p2d->y;
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
    ON_2dPoint *p = this->pnts_2d[vind];
    pd_point(plot_file, p->x, p->y);
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
