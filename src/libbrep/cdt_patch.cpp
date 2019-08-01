/*                 C D T _ P A T C H . C P P
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
/** @file cdt_patch.cpp
 *
 * Local remeshing of portions of a previously triangulated mesh.
 *
 */

#include "common.h"
#include "bg/chull.h"
#include "./cdt.h"

#define CDT_DEBUG_PLOTS 1

#if CDT_DEBUG_PLOTS
static void
plot_edge_set(std::vector<std::pair<trimesh::index_t, trimesh::index_t>> &es, std::vector<ON_3dPoint *> &pnts_3d, const char *filename)
{
    bu_file_delete(filename);
    FILE* plot_file = fopen(filename, "w");
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    pl_color(plot_file, r, g, b);

    for (size_t i = 0; i < es.size(); i++) {
	ON_3dPoint *p1 = pnts_3d[es[i].first];
	ON_3dPoint *p2 = pnts_3d[es[i].second];
	point_t bnp1, bnp2;
	VSET(bnp1, p1->x, p1->y, p1->z);
	VSET(bnp2, p2->x, p2->y, p2->z);

	// Arrowhead
	vect_t vrev, vperp, varrow;
	VSUB2(vrev, bnp2, bnp1);
	VSCALE(vrev, vrev, 0.1);
	bn_vec_perp(vperp, vrev);
	VSCALE(vperp, vperp, 0.5);
	VADD2(varrow, vperp, vrev);
	VADD2(varrow, varrow, bnp2);

	pdv_3move(plot_file, bnp1);
	pdv_3cont(plot_file, bnp2);
	pdv_3cont(plot_file, varrow);
    }

    fclose(plot_file);
}

static void
plot_edge_loop(std::vector<trimesh::index_t> &el, std::vector<ON_3dPoint *> &pnts_3d, const char *filename)
{
    bu_file_delete(filename);
    FILE* plot_file = fopen(filename, "w");
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    pl_color(plot_file, r, g, b);

    point_t bnp1, bnp2;
    ON_3dPoint *p1, *p2;
    p2 = pnts_3d[el[0]];
    VSET(bnp2, p2->x, p2->y, p2->z);
    pdv_3move(plot_file, bnp2);

    for (size_t i = 1; i < el.size(); i++) {
	p1 = p2;
	p2 = pnts_3d[el[i]];
	VSET(bnp1, p1->x, p1->y, p1->z);
	VSET(bnp2, p2->x, p2->y, p2->z);

	// Arrowhead
	vect_t vrev, vperp, varrow;
	VSUB2(vrev, bnp2, bnp1);
	VSCALE(vrev, vrev, 0.1);
	bn_vec_perp(vperp, vrev);
	VSCALE(vperp, vperp, 0.5);
	VADD2(varrow, vperp, vrev);
	VADD2(varrow, varrow, bnp2);

	pdv_3cont(plot_file, bnp2);

	pdv_3cont(plot_file, varrow);
	pdv_3cont(plot_file, bnp2);
    }

    fclose(plot_file);
}

static void
plot_edge_loop_2d(std::vector<p2t::Point *> &el , const char *filename)
{
    bu_file_delete(filename);
    FILE* plot_file = fopen(filename, "w");
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    pl_color(plot_file, r, g, b);

    point_t bnp;
    VSET(bnp, el[0]->x, el[0]->y, 0);
    pdv_3move(plot_file, bnp);

    for (size_t i = 1; i < el.size(); i++) {
	VSET(bnp, el[i]->x, el[i]->y, 0);
	pdv_3cont(plot_file, bnp);
    }

    fclose(plot_file);
}

void
plot_2d_bg_tri(int *ecfaces, int num_faces, point2d_t *pnts, const char *filename)
{
    bu_file_delete(filename);
    FILE* plot_file = fopen(filename, "w");
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    pl_color(plot_file, r, g, b);

    for (int k = 0; k < num_faces; k++) {
	point_t p1, p2, p3;
	VSET(p1, pnts[ecfaces[3*k]][X], pnts[ecfaces[3*k]][Y], 0);
	VSET(p2, pnts[ecfaces[3*k+1]][X], pnts[ecfaces[3*k+1]][Y], 0);
	VSET(p3, pnts[ecfaces[3*k+2]][X], pnts[ecfaces[3*k+2]][Y], 0);

	pdv_3move(plot_file, p1);
	pdv_3cont(plot_file, p2);
	pdv_3move(plot_file, p1);
	pdv_3cont(plot_file, p3);
	pdv_3move(plot_file, p2);
	pdv_3cont(plot_file, p3);
    }
    fclose(plot_file);
}

void
plot_2d_cdt_tri(p2t::CDT *ncdt, const char *filename)
{
    bu_file_delete(filename);
    FILE* plot_file = fopen(filename, "w");
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    pl_color(plot_file, r, g, b);

    std::vector<p2t::Triangle*> ntris = ncdt->GetTriangles();
    for (size_t i = 0; i < ntris.size(); i++) {
	point_t pp1, pp2, pp3;
	p2t::Triangle *t = ntris[i];
	p2t::Point *p1 = t->GetPoint(0);
	p2t::Point *p2 = t->GetPoint(1);
	p2t::Point *p3 = t->GetPoint(2);
	VSET(pp1, p1->x, p1->y, 0);
	VSET(pp2, p2->x, p2->y, 0);
	VSET(pp3, p3->x, p3->y, 0);

	pdv_3move(plot_file, pp1);
	pdv_3cont(plot_file, pp2);
	pdv_3move(plot_file, pp1);
	pdv_3cont(plot_file, pp3);
	pdv_3move(plot_file, pp2);
	pdv_3cont(plot_file, pp3);

    }
    fclose(plot_file);
}



static void
plot_best_fit_plane(point_t *center, vect_t *norm, const char *filename)
{
    FILE* plot_file = fopen(filename, "w");
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

static void
plot_trimesh_tris_3d_each_tri(std::set<trimesh::index_t> *faces, std::vector<trimesh::triangle_t> &farray, std::map<p2t::Point *, ON_3dPoint *> *pointmap, const char *filename)
{
    std::set<trimesh::index_t>::iterator f_it;
    struct bu_vls fname = BU_VLS_INIT_ZERO;
    int cnt = 0;
    for (f_it = faces->begin(); f_it != faces->end(); f_it++) {
	bu_vls_sprintf(&fname, "%s-%d", filename, cnt);
	FILE* plot_file = fopen(bu_vls_cstr(&fname), "w");
	int r = int(256*drand48() + 1.0);
	int g = int(256*drand48() + 1.0);
	int b = int(256*drand48() + 1.0);
	p2t::Triangle *t = farray[*f_it].t;
	plot_tri_3d(t, pointmap, r, g ,b, plot_file);
	fclose(plot_file);
	cnt++;
    }
    bu_vls_free(&fname);
}

static void
plot_trimesh_tris_3d(std::set<trimesh::index_t> *faces, std::vector<trimesh::triangle_t> &farray, std::map<p2t::Point *, ON_3dPoint *> *pointmap, const char *filename)
{
    std::set<trimesh::index_t>::iterator f_it;
    FILE* plot_file = fopen(filename, "w");
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    for (f_it = faces->begin(); f_it != faces->end(); f_it++) {
	p2t::Triangle *t = farray[*f_it].t;
	plot_tri_3d(t, pointmap, r, g ,b, plot_file);
    }
    fclose(plot_file);
}

static void
plot_trimesh(std::vector<trimesh::triangle_t> &farray, std::map<p2t::Point *, ON_3dPoint *> *pointmap, const char *filename)
{
    std::set<trimesh::index_t>::iterator f_it;
    FILE* plot_file = fopen(filename, "w");
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    for (size_t i = 0; i < farray.size(); i++) {
	p2t::Triangle *t = farray[i].t;
	plot_tri_3d(t, pointmap, r, g ,b, plot_file);
    }
    fclose(plot_file);
}

static void
plot_3d_cdt_tri(std::set<p2t::Triangle *> *faces, std::map<p2t::Point *, ON_3dPoint *> *pointmap, const char *filename)
{
    std::set<p2t::Triangle *>::iterator f_it;
    FILE* plot_file = fopen(filename, "w");
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    for (f_it = faces->begin(); f_it != faces->end(); f_it++) {
	p2t::Triangle *t = *f_it;
	plot_tri_3d(t, pointmap, r, g ,b, plot_file);
    }
    fclose(plot_file);
}
#endif


void
best_fit_plane(point_t *orig, vect_t *norm, struct trimesh_info *tm, std::set<trimesh::index_t> *remesh_triangles,
    struct ON_Brep_CDT_Face_State *f)
{
    // Find the best fit plane for the Brep normals of the vertices of the triangles involved with
    // the remeshing
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::set<ON_3dPoint *> active_3d_pnts;
    ON_3dVector avgtnorm(0.0,0.0,0.0);
    std::set<trimesh::index_t>::iterator f_it;
    for (f_it = remesh_triangles->begin(); f_it != remesh_triangles->end(); f_it++) {
	p2t::Triangle *t = tm->triangles[*f_it].t;
	ON_3dVector ltdir = p2tTri_Brep_Normal(f, t);
	avgtnorm += ltdir;
	for (size_t j = 0; j < 3; j++) {
	    active_3d_pnts.insert((*pointmap)[t->GetPoint(j)]);
	}
    }
    avgtnorm = avgtnorm * 1.0/(double)remesh_triangles->size();

    point_t pcenter;
    vect_t pnorm;
    {
	point_t *pnts = (point_t *)bu_calloc(active_3d_pnts.size()+1, sizeof(point_t), "fitting points");
	int pnts_ind = 0;
	std::set<ON_3dPoint *>::iterator a_it;
	for (a_it = active_3d_pnts.begin(); a_it != active_3d_pnts.end(); a_it++) {
	    ON_3dPoint *p = *a_it;
	    pnts[pnts_ind][X] = p->x;
	    pnts[pnts_ind][Y] = p->y;
	    pnts[pnts_ind][Z] = p->z;
	    pnts_ind++;
	}
	if (bn_fit_plane(&pcenter, &pnorm, pnts_ind, pnts)) {
	    bu_log("Failed to get best fit plane!\n");
	}
	bu_free(pnts, "fitting points");

	ON_3dVector on_norm(pnorm[X], pnorm[Y], pnorm[Z]);
	if (ON_DotProduct(on_norm, avgtnorm) < 0) {
	    VSCALE(pnorm, pnorm, -1);
	}
    }

    VMOVE(*orig, pcenter);
    VMOVE(*norm, pnorm);
}


static int
remove_butterfly_vertices(std::vector<trimesh::triangle_t> &triangles, size_t pnt_size, std::set<trimesh::index_t> *remesh_triangles, trimesh::index_t seed_id, std::map<p2t::Point *, ON_3dPoint *> *pointmap, int face_index, struct bu_vls *pname_root)
{
    int butterfly_verts = 1;
    int pass = 1;

    while (butterfly_verts) {

	std::set<trimesh::index_t> culls;
	std::vector<trimesh::triangle_t> submesh_triangles;
	std::map<trimesh::index_t, trimesh::index_t> n2o;
	std::set<trimesh::index_t>::iterator tm_it;
	for (tm_it = remesh_triangles->begin(); tm_it != remesh_triangles->end(); tm_it++) {
	    trimesh::triangle_t tmt = triangles[*tm_it];
	    submesh_triangles.push_back(tmt);
	    n2o[submesh_triangles.size()-1] = *tm_it;
	}
	bu_log("initial submesh triangle cnt: %zd\n", submesh_triangles.size());

	trimesh::trimesh_t smesh;
	std::vector<trimesh::edge_t> sedges;
	trimesh::unordered_edges_from_triangles(submesh_triangles.size(), &submesh_triangles[0], sedges);

	smesh.build(pnt_size, submesh_triangles.size(), &submesh_triangles[0], sedges.size(), &sedges[0]);

	bu_log("butterfly vertex cnt: %zd\n", smesh.m_butterfly_vertices.size());
	if (smesh.m_butterfly_vertices.size()) {
	    bu_log("butterfly verts!\n");
	}

	// For each butterfly vertex, pull the set of associated faces.
	// For each triangle, if not the seed triangle, yank it.
	std::set<trimesh::index_t>::iterator bf_it;
	for (bf_it = smesh.m_butterfly_vertices.begin(); bf_it != smesh.m_butterfly_vertices.end(); bf_it++) {
	    std::vector<trimesh::index_t> faces = smesh.vertex_face_neighbors(*bf_it);
	    bu_log("bf faces cnt: %zd\n", faces.size());
	    for (size_t i = 0; i < faces.size(); i++) {
		if (n2o[faces[i]] == seed_id) continue;

		// Get the trimesh index from the original mesh, not the working submesh
		culls.insert(n2o[faces[i]]);
		bu_log("cull %ld -> %ld\n", faces[i], n2o[faces[i]]);
	    }
	}

	// Thin down the remesh triangle set based on the edge analysis
	std::set<trimesh::index_t>::iterator c_it;
	for (c_it = culls.begin(); c_it != culls.end(); c_it++) {
	    remesh_triangles->erase(*c_it);
	    bu_log("culling %ld\n", *c_it);
	}

	// If we didn't have to pull anything, we should be done (may still have to deal with holes,
	// but that involves a generalization of boundary_loop in trimesh...)
	if (!culls.size()) {
	    butterfly_verts = 0;
	}

#if CDT_DEBUG_PLOTS
	struct bu_vls pname = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&pname, "%s-%d-04-butterfly-pass-%d-3D.plot3", bu_vls_cstr(pname_root), face_index, pass);
	plot_trimesh_tris_3d(remesh_triangles, triangles, pointmap, bu_vls_cstr(&pname));

	// TODO - need to see 2D projection mesh as well, since that's actually the one poly2tri needs
	// to work on
#endif


    }

    return 0;
}

size_t
collect_neighbor_triangles(std::set<trimesh::index_t> *remesh_triangles, double deg, struct ON_Brep_CDT_Face_State *f, struct trimesh_info *tm, p2t::Triangle *seed_tri)
{
    double angle = deg*ON_PI/180.0;
    
    std::set<trimesh::index_t> visited_triangles;

    remesh_triangles->clear();

    trimesh::index_t seed_id = tm->t2ind[seed_tri];
    ON_3dVector tdir = p2tTri_Brep_Normal(f, seed_tri);
    tdir.Unitize();

    // Walk out along the mesh, adding triangles whose triangle normal is < deg
    // degrees away from that of the original triangle.  These form the initial
    // candidate set of remeshing triangles, although they are not yet the
    // final set.
    std::queue<trimesh::index_t> q1, q2;
    std::queue<trimesh::index_t> *tq, *nq;
    tq = &q1;
    nq = &q2;
    q1.push(seed_id);
    while (!tq->empty()) {
	trimesh::index_t t_he = tq->front();
	tq->pop();
	p2t::Triangle *ct = tm->triangles[t_he].t;
	// Check normal
	ON_3dVector ctdir = p2tTri_Brep_Normal(f, ct);
	ctdir.Unitize();

	double dprd = ON_DotProduct(ctdir, tdir);
	double dang = (NEAR_EQUAL(dprd, 1.0, ON_ZERO_TOLERANCE)) ? 0 : acos(dprd); 

	if (dang > angle) {
	    bu_log("reject: %f > %f\n", dang, angle);
	    continue;
	} else {
	    (*remesh_triangles).insert(t_he);
	}

	// Queue up the connected faces we don't already have in the set
	std::vector<trimesh::index_t> faces = tm->mesh.face_neighbors(t_he);
	for (size_t j = 0; j < faces.size(); j++) {
	    if (visited_triangles.find(faces[j]) == visited_triangles.end()) {
		// We haven't seen this one yet
		nq->push(faces[j]);
		visited_triangles.insert(faces[j]);
	    }
	}

	if (tq->empty()) {
	    std::queue<trimesh::index_t> *tmpq = tq;
	    tq = nq;
	    nq = tmpq;
	}
    }

    return remesh_triangles->size();
}

int
Remesh_Near_Tri(struct ON_Brep_CDT_Face_State *f, p2t::Triangle *seed_tri, std::set<p2t::Triangle *> *wq)
{

#if CDT_DEBUG_PLOTS
    struct bu_vls pname_root = BU_VLS_INIT_ZERO;
    struct bu_vls pname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&pname_root, "remesh_triangles-0");
    bu_vls_sprintf(&pname, "%s-%d.plot3", bu_vls_cstr(&pname_root), f->ind);
    while (bu_file_exists(bu_vls_cstr(&pname), NULL)) {
	bu_vls_incr(&pname_root, NULL, "2:0:0:0", NULL, NULL);
	bu_vls_sprintf(&pname, "%s-%d.plot3", bu_vls_cstr(&pname_root), f->ind);
    }
#endif

    struct trimesh_info *tm = CDT_Face_Build_Halfedge(f->tris);

    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = f->p2t_to_on3_norm_map;
    std::set<trimesh::index_t> remesh_triangles;
    std::set<trimesh::index_t>::iterator f_it;
    trimesh::index_t seed_id = tm->t2ind[seed_tri];

#if CDT_DEBUG_PLOTS
    bu_vls_sprintf(&pname, "%s-%d-00-initial_tmesh.plot3", bu_vls_cstr(&pname_root), f->ind);
    plot_trimesh(tm->triangles, pointmap, bu_vls_cstr(&pname));
    bu_vls_sprintf(&pname, "%s-%d-00-initial_tmesh_2d.plot3", bu_vls_cstr(&pname_root), f->ind);
    plot_trimesh_2d(tm->triangles, bu_vls_cstr(&pname));
#endif

    double deg = 10;
    size_t ntricnt = collect_neighbor_triangles(&remesh_triangles, deg, f, tm, seed_tri);
    while (ntricnt < 10 && deg < 45) {
	bu_log("too few triangles: %zd\n", ntricnt);
	deg = deg + 5;
	ntricnt = collect_neighbor_triangles(&remesh_triangles, deg, f, tm, seed_tri);
    }

    if (remesh_triangles.size() < 3) {
	bu_log("Could not get enough triangles, even at degree %f: %zd\n", deg, remesh_triangles.size());
	plot_trimesh_tris_3d_each_tri(&remesh_triangles, tm->triangles, pointmap, "bad_news.plot3");
	deg = 10;
	ntricnt = collect_neighbor_triangles(&remesh_triangles, deg, f, tm, seed_tri);
	while (ntricnt < 10 && deg < 45) {
	    bu_log("too few triangles: %zd\n", ntricnt);
	    deg = deg + 5;
	    ntricnt = collect_neighbor_triangles(&remesh_triangles, deg, f, tm, seed_tri);
	}
	bu_exit(1, "fail");
    }

    bu_log("remesh triangle stage 1 cnt: %zd\n", remesh_triangles.size());

#if CDT_DEBUG_PLOTS
    bu_vls_sprintf(&pname, "%s-%d-01.plot3", bu_vls_cstr(&pname_root), f->ind);
    plot_trimesh_tris_3d(&remesh_triangles, tm->triangles, pointmap, bu_vls_cstr(&pname));
#endif


    // Weed out any trivially degenerate triangles (TODO - shouldn't we have
    // already culled these by now?)
    std::set<trimesh::index_t>::iterator tm_it;
    std::set<trimesh::index_t> tm_degen;
    for (tm_it = remesh_triangles.begin(); tm_it != remesh_triangles.end(); tm_it++) {
	p2t::Triangle *t = tm->triangles[*tm_it].t;
	ON_3dPoint *v[3];
	for (size_t j = 0; j < 3; j++) {
	    v[j] = (*pointmap)[t->GetPoint(j)];
	}
	if (!(v[0] != v[1] && v[0] != v[2] && v[1] != v[2])) {
	    tm_degen.insert(*tm_it);
	}
    }
    for (tm_it = tm_degen.begin(); tm_it != tm_degen.end(); tm_it++) {
	remesh_triangles.erase(*tm_it);
    }
    bu_log("remesh triangle stage 2 cnt: %zd\n", remesh_triangles.size());


#if CDT_DEBUG_PLOTS
    bu_vls_sprintf(&pname, "%s-%d-02.plot3", bu_vls_cstr(&pname_root), f->ind);
    plot_trimesh_tris_3d(&remesh_triangles, tm->triangles, pointmap, bu_vls_cstr(&pname));
#endif

    // Find the best fit plane for the vertices of the triangles involved with
    // the remeshing
    point_t pcenter;
    vect_t pnorm;
    best_fit_plane(&pcenter, &pnorm, tm, &remesh_triangles, f);
#if CDT_DEBUG_PLOTS
    bu_vls_sprintf(&pname, "%s-%d-03-best_fit_plane.plot3", bu_vls_cstr(&pname_root), f->ind);
    plot_best_fit_plane(&pcenter, &pnorm, bu_vls_cstr(&pname));
#endif

    // Project all 3D points in the subset into the best fit plane. We need XY coordinates
    // for poly2Tri.
    std::set<ON_3dPoint *> sub_3d;
    std::map<ON_3dPoint *, ON_3dPoint *> sub_3d_norm;
    for (tm_it = remesh_triangles.begin(); tm_it != remesh_triangles.end(); tm_it++) {
	p2t::Triangle *t = tm->triangles[*tm_it].t;
	for (size_t j = 0; j < 3; j++) {
	    sub_3d.insert((*pointmap)[t->GetPoint(j)]);
	    sub_3d_norm[(*pointmap)[t->GetPoint(j)]] = (*normalmap)[t->GetPoint(j)];
	}
    }
    std::vector<ON_3dPoint *> pnts_3d(sub_3d.begin(), sub_3d.end());
    std::vector<ON_2dPoint *> pnts_2d;
    std::map<ON_3dPoint *, size_t> pnts_3d_ind;

    ON_Plane bfplane(ON_3dPoint(pcenter[X],pcenter[Y],pcenter[Z]),ON_3dVector(pnorm[X],pnorm[Y],pnorm[Z]));
    ON_Xform to_plane;
    to_plane.PlanarProjection(bfplane);
    std::map<ON_2dPoint *, ON_3dPoint *> on2_3;
    std::map<ON_2dPoint *, ON_3dPoint *> on2_3n;
    for (size_t i = 0; i < pnts_3d.size(); i++) {
	pnts_3d_ind[pnts_3d[i]] = i;
	ON_3dPoint op3d = (*pnts_3d[i]);
	op3d.Transform(to_plane);
	ON_2dPoint *n2d = new ON_2dPoint(op3d.x, op3d.y);
	pnts_2d.push_back(n2d);
	on2_3[n2d] = pnts_3d[i];
	on2_3n[n2d] = sub_3d_norm[pnts_3d[i]];
    }

    // Translate the poly2tri triangle into trimesh
    std::map<trimesh::index_t, trimesh::index_t> sp2o;
    std::vector<trimesh::triangle_t> submesh_triangles_prelim;
    std::set<trimesh::index_t> smtri;
    for (tm_it = remesh_triangles.begin(); tm_it != remesh_triangles.end(); tm_it++) {
	p2t::Triangle *t = tm->triangles[*tm_it].t;
	trimesh::triangle_t tmt;
	for (size_t j = 0; j < 3; j++) {
	    tmt.v[j] = pnts_3d_ind[(*pointmap)[t->GetPoint(j)]];
	}
	tmt.t = t;
	if (tmt.v[0] != tmt.v[1] && tmt.v[0] != tmt.v[2] && tmt.v[1] != tmt.v[2]) {
	    submesh_triangles_prelim.push_back(tmt);
	    sp2o[submesh_triangles_prelim.size()-1] = *tm_it;
	    smtri.insert(submesh_triangles_prelim.size()-1);
	} else {
	    bu_log("Skipping: %ld -> %ld -> %ld\n", tmt.v[0], tmt.v[1], tmt.v[2]);
	}

    }
    bu_log("submesh triangle prelim cnt: %zd\n", submesh_triangles_prelim.size());

    // The above selection isn't guaranteed to produce something poly2tri can
    // handle - do a filtering pass to deactivate triangles causing butterfly
    // vertices in the mesh
    remove_butterfly_vertices(submesh_triangles_prelim, pnts_2d.size(), &smtri, seed_id, pointmap, f->ind, &pname_root);

    std::vector<trimesh::triangle_t> submesh_triangles;
    std::map<trimesh::index_t, trimesh::index_t> s2sp;
    for (tm_it = smtri.begin(); tm_it != smtri.end(); tm_it++) {
	trimesh::triangle_t tmt = submesh_triangles_prelim[*tm_it];
	submesh_triangles.push_back(tmt);
	s2sp[submesh_triangles.size()-1] = *tm_it;
    }
    bu_log("submesh triangle cnt: %zd\n", submesh_triangles.size());

    for (size_t i = 0; i < submesh_triangles.size(); i++) {
	wq->erase(tm->triangles[sp2o[s2sp[i]]].t);
    }

    std::vector<trimesh::edge_t> sedges;
    trimesh::trimesh_t smesh;
    trimesh::unordered_edges_from_triangles(submesh_triangles.size(), &submesh_triangles[0], sedges);
    smesh.build(pnts_2d.size(), submesh_triangles.size(), &submesh_triangles[0], sedges.size(), &sedges[0]);

    // Build the new outer boundary
    std::vector<std::pair<trimesh::index_t, trimesh::index_t>> bedges = smesh.boundary_edges();
    bu_log("boundary edge segment cnt: %zd\n", bedges.size());


#if CDT_DEBUG_PLOTS
    bu_vls_sprintf(&pname, "%s-%d-05-outer_edge.plot3", bu_vls_cstr(&pname_root), f->ind);
    plot_edge_set(bedges, pnts_3d, bu_vls_cstr(&pname));
    //plot_trimesh_tris_3d(&smtri, submesh_triangles, pointmap, "submesh_triangles.plot3");
#endif

    // Given the set of unordered boundary edge segments, construct the outer loop
    // TODO - triangle selection doesn't guarantee no-hole mesh sets - need to handle
    std::vector<trimesh::index_t> sloop = smesh.boundary_loop();

    if (!sloop.size()) {
	bu_log("Error - failed to generate boundary loop!\n");
	bu_exit(1, "bail");
	return -1;
    }

#if CDT_DEBUG_PLOTS
    bu_vls_sprintf(&pname, "%s-%d-06-outer_loop.plot3", bu_vls_cstr(&pname_root), f->ind);
    plot_edge_loop(sloop, pnts_3d, bu_vls_cstr(&pname));
#endif

    // Perform the new triangulation
    std::set<ON_2dPoint *> handled;
    std::set<ON_2dPoint *>::iterator u_it;
    std::vector<p2t::Point *> polyline;
    p2t::Point *fp = NULL;
    for (size_t i = 0; i < sloop.size(); i++) {
	ON_2dPoint *onp2d = pnts_2d[sloop[i]];
	if (!onp2d) {
	    bu_log("missing onp2d!\n");
	}
	p2t::Point *np = new p2t::Point(onp2d->x, onp2d->y);
	if (!fp) fp = np;
	if (on2_3.find(onp2d) == on2_3.end()) {
	    bu_log("missing onp2d to 3D mapping!\n");
	}
	(*pointmap)[np] = on2_3[onp2d];
	//bu_log("polyline %f %f -> %f %f %f\n", np->x, np->y, (*pointmap)[np]->x, (*pointmap)[np]->y, (*pointmap)[np]->z);
	(*normalmap)[np] = on2_3n[onp2d];
	polyline.push_back(np);
	handled.insert(onp2d);
    }

    if (!polyline.size()) {
    	bu_log("Error - failed to generate polyline!\n");
	return -1;
    }

#if CDT_DEBUG_PLOTS
    bu_vls_sprintf(&pname, "%s-%d-07-polyline.plot3", bu_vls_cstr(&pname_root), f->ind);
    plot_edge_loop_2d(polyline, bu_vls_cstr(&pname));
#endif

    p2t::CDT *ncdt = new p2t::CDT(polyline);

    for (size_t i = 0; i < pnts_2d.size(); i++) {
	ON_2dPoint *onp2d = pnts_2d[i];
	if (!onp2d) {
	    bu_log("missing onp2d!\n");
	}
	if (handled.find(onp2d) != handled.end()) continue;
	p2t::Point *np = new p2t::Point(onp2d->x, onp2d->y);
	if (on2_3.find(onp2d) == on2_3.end()) {
	    bu_log("missing onp2d to 3D mapping!\n");
	}
	(*pointmap)[np] = on2_3[onp2d];
	//bu_log("AddPoint %f %f -> %f %f %f\n", np->x, np->y, (*pointmap)[np]->x, (*pointmap)[np]->y, (*pointmap)[np]->z);
	(*normalmap)[np] = on2_3n[onp2d];
	ncdt->AddPoint(np);
    }
    ncdt->Triangulate(true, -1);

    // Validate that all points have a corresponding 3D point
    std::vector<p2t::Triangle*> fntris = ncdt->GetTriangles();
    for (size_t i = 0; i < fntris.size(); i++) {
	p2t::Triangle *t = fntris[i];
	for (size_t j = 0; j < 3; j++) {
	    p2t::Point *tp = t->GetPoint(j);
	    if (f->p2t_to_on3_map->find(tp) == f->p2t_to_on3_map->end()) {
		bu_log("Error - triangle created with invalid 3D mapping!\n");
	    } else {
		//bu_log("TPnt(%zd,%zd) %f %f -> %f %f %f\n", i,j, tp->x, tp->y, (*pointmap)[tp]->x, (*pointmap)[tp]->y, (*pointmap)[tp]->z);
	    }
	}
    }

#if CDT_DEBUG_PLOTS
    bu_vls_sprintf(&pname, "%s-%d-08-poly2tri_2d.plot3", bu_vls_cstr(&pname_root), f->ind);
    plot_2d_cdt_tri(ncdt, bu_vls_cstr(&pname));
#endif

    // assemble the new p2t Triangle set using
    // the mappings, and check the new 3D mesh thus created for issues.  If it
    // is clean, replace the original subset of the parent face with the new
    // triangle set, else fail.

    std::set<p2t::Triangle*> new_faces;
    std::vector<p2t::Triangle*> ftris = ncdt->GetTriangles();
    for (size_t i = 0; i < ftris.size(); i++) {
	p2t::Triangle *t = ftris[i];
	p2t::Point *p2_1 = t->GetPoint(0);
	p2t::Point *p2_2 = t->GetPoint(1);
	p2t::Point *p2_3 = t->GetPoint(2);
	for (size_t j = 0; j < 3; j++) {
	    p2t::Point *tp = t->GetPoint(j);
	    if (pointmap->find(tp) == pointmap->end()) {
		bu_log("Error - triangle created with invalid 3D mapping!\n");
	    } else {
		//bu_log("TPnt2(%zd,%zd) %f %f -> %f %f %f\n", i,j, tp->x, tp->y, (*pointmap)[tp]->x, (*pointmap)[tp]->y, (*pointmap)[tp]->z);
	    }
	}

	//bu_log("TPnt1 %f %f -> %f %f %f\n", p2_1->x, p2_1->y, (*pointmap)[p2_1]->x, (*pointmap)[p2_1]->y, (*pointmap)[p2_1]->z);
	//bu_log("TPnt2 %f %f -> %f %f %f\n", p2_2->x, p2_2->y, (*pointmap)[p2_2]->x, (*pointmap)[p2_2]->y, (*pointmap)[p2_2]->z);
	//bu_log("TPnt3 %f %f -> %f %f %f\n", p2_3->x, p2_3->y, (*pointmap)[p2_3]->x, (*pointmap)[p2_3]->y, (*pointmap)[p2_3]->z);
	//bu_log("TPoints %f %f -> %f %f -> %f %f\n", p2_1->x, p2_1->y, p2_2->x, p2_2->y, p2_3->x, p2_3->y);
	// Check the triangle direction against the normals
	ON_3dVector ltdir = p2tTri_Normal(t, pointmap);
	ON_3dPoint *onorm = (*normalmap)[t->GetPoint(0)];
	if (ON_DotProduct(*onorm, ltdir) < 0) {
	    p2_1 = t->GetPoint(0);
	    p2_2 = t->GetPoint(2);
	    p2_3 = t->GetPoint(1);
	}
	p2t::Triangle *nt = new p2t::Triangle(*p2_1, *p2_2, *p2_3);
	f->tris->insert(nt);
	new_faces.insert(nt);
    }

#if CDT_DEBUG_PLOTS
    bu_vls_sprintf(&pname, "%s-%d.plot3", bu_vls_cstr(&pname_root), f->ind);
    plot_3d_cdt_tri(&new_faces, pointmap, bu_vls_cstr(&pname));
#endif

    for (size_t i = 0; i < submesh_triangles.size(); i++) {
	p2t::Triangle *t = submesh_triangles[i].t;
	f->tris->erase(t);
	delete t;
    }

    delete tm;

    return 0;
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

