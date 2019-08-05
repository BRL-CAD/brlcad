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

    return -1;
}

bool
cpolygon_t::closed()
{
    return false;
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

    return result;
}

bool
csweep_t::interior_points_contained()
{
    return false;
}

void
csweep_t::cull_problem_tris(std::set<triangle_t> *tcandidates)
{
    if (!tcandidates) {
	return;
    }
}

std::set<triangle_t>
csweep_t::non_visited_tris(std::set<long> &verts)
{
    std::set<triangle_t> result;
    if (!verts.size()) {
	return result;
    }

    return result;
}

long
csweep_t::grow_loop(double deg, bool stop_on_contained)
{
    if (stop_on_contained && !interior_points.size()) {
	return 0;
    }

    if (deg < 0 || deg > 170) {
	return -1;
    }

#if 0
    double angle = deg * ON_PI/180.0;
    ON_3dVector sn = bnorm(seed);

    visited_triangles.insert(seed);

    std::queue<triangle_t> q1, q2;
    std::queue<triangle_t> *tq, *nq;
    tq = &q1;
    nq = &q2;

    {
	std::set<triangle_t> fvneigh;
	std::set<triangle_t>::iterator f_it;
	for (int i = 0; i < 3; i++) {
	    std::vector<triangle_t> fneigh = vertex_face_neighbors(seed.v[i]);
	    fvneigh.insert(fneigh.begin(), fneigh.end());
	}
	for (f_it = fvneigh.begin(); f_it != fvneigh.end(); f_it++) {
	    q1.push(*f_it);
	}
    }
    while (!tq->empty()) {
	triangle_t ct = tq->front();
	tq->pop();
	// Check normal
	ON_3dVector tn = bnorm(ct);
	double dprd = ON_DotProduct(sn, tn);
	double dang = (NEAR_EQUAL(dprd, 1.0, ON_ZERO_TOLERANCE)) ? 0 : acos(dprd);

	if (dang > angle) {
	    std::cerr << "Angle rejection (" << dang << "," << angle << ")\n";
	    continue;
	}

	// TODO - if triangle's non active vertex is inside the current
	// boundary loop, it needs to be stored as an interior point - no
	// triangle is added to the submesh, as the submesh's only purpose is
	// to provide the outer bounding polygon for p2t.  However, the
	// triangles associated with the interior point MUST be added to the
	// mesh, or problems will result
	//
	// In principle this might happen with a triangle flipped relative to
	// its brep normals...

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

	if (tq->empty()) {
	    std::queue<triangle_t> *tmpq = tq;
	    tq = nq;
	    nq = tmpq;
	}
    }

    return submesh->tris.size();
#endif

    if (stop_on_contained && !interior_points_contained()) {
	return -1;
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



long
csweep_t::build_initial_loop(triangle_t &seed)
{
    std::set<uedge_t>::iterator u_it;

    visited_triangles.insert(seed);

    // None of the edges or vertices from any of the problem triangles can be
    // in a polygon edge.  By definition, the seed is a problem triangle.
    std::set<long> seed_verts;
    for (int i = 0; i < 3; i++) {
	seed_verts.insert(seed.v[i]);
	// The exception to interior categorization is Brep boundary points -
	// they are never interior
	if (cmesh->brep_edge_pnt(seed.v[i])) {
	    continue;
	}
	interior_points.insert(seed.v[i]);
    }

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

    // The seed's neighbors are the first candidates for polygon edges
    std::set<triangle_t> candidates = non_visited_tris(seed_verts);
    if (!candidates.size()) {
	std::cerr << "Seed has NO non-visited neighbors???\n";
	return -1;
    }

    while (!polygon.closed()) {

	// If we're exhausted our candidates, get some more.
	if (!candidates.size()) {
	    std::set<long> dv = polygon.dangling_vertices();
	    candidates = non_visited_tris(dv);
	}
	cull_problem_tris(&candidates);

	// If ALL the candidates were problems, get all unvisited neighbors
	// from the interior points - need SOMETHING to work on...
	while (!candidates.size()) {
	    candidates = non_visited_tris(interior_points);
	    if (!candidates.size()) {
		std::cerr << "NO candidates found from interior points???\n";
		return -1;
	    }
	    cull_problem_tris(&candidates);
	}

	// OK, we have candidate triangles.  Get some exterior edges.
	std::set<edge_t> ee = exterior_edges(candidates);
	std::set<edge_t>::iterator e_it;
	for (e_it = ee.begin(); e_it != ee.end(); e_it++) {
	    polygon.add_edge(*e_it);
	}

    }



    return -1;
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
