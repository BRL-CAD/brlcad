/*                        C D T . C P P
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
/** @file cdt.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"
#include "bg/chull.h"
#include "./cdt.h"

#define BREP_PLANAR_TOL 0.05
#define MAX_TRIANGULATION_ATTEMPTS 5

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

static void
plot_concave_hull_2d(point2d_t *pnts, int npnts, const char *filename)
{
    bu_file_delete(filename);
    FILE* plot_file = fopen(filename, "w");
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    pl_color(plot_file, r, g, b);

    point_t bnp;
    VSET(bnp, pnts[0][X], pnts[0][Y], 0);
    pdv_3move(plot_file, bnp);

    for (int i = 1; i < npnts; i++) {
	VSET(bnp, pnts[i][X], pnts[i][Y], 0);
	pdv_3cont(plot_file, bnp);
    }

    VSET(bnp, pnts[0][X], pnts[0][Y], 0);
    pdv_3cont(plot_file, bnp);

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

static bool
flipped_face(p2t::Triangle *t, std::map<p2t::Point *, ON_3dPoint *> *pointmap, std::map<p2t::Point *, ON_3dPoint *> *normalmap, bool m_bRev)
{
    ON_3dVector tdir = p2tTri_Normal(t, pointmap);
    int invalid_face_normal = 0;

    for (size_t j = 0; j < 3; j++) {
	ON_3dPoint onorm = *(*normalmap)[t->GetPoint(j)];
	if (m_bRev) {
	    onorm = onorm * -1;
	}
	if (tdir.Length() > 0 && ON_DotProduct(onorm, tdir) < 0.1) {
	    invalid_face_normal++;
	}
    }

    return (invalid_face_normal == 3);
}

void
Plot_Singular_Connected(struct ON_Brep_CDT_Face_State *f, struct trimesh_info *tm, ON_3dPoint *p3d)
{
    if (bu_file_exists("singularity_triangles.plot3", NULL)) return;
    std::vector<p2t::Triangle*> tris = f->cdt->GetTriangles();
    std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
    std::map<p2t::Point *, ON_3dPoint *> *normalmap = f->p2t_to_on3_norm_map;
    std::set<trimesh::index_t> singularity_triangles;
    std::set<trimesh::index_t>::iterator f_it;

    // Find all the triangles using p3d as a vertex - that's our
    // starting triangle set.
    for (size_t i = 0; i < tris.size(); i++) {
	p2t::Triangle *t = tris[i];
	if (f->tris_degen->find(t) != f->tris_degen->end()) {
	    continue;
	}
	for (size_t j = 0; j < 3; j++) {
	    if ((*pointmap)[t->GetPoint(j)] == p3d) {
		trimesh::index_t ind = tm->p2dind[t->GetPoint(j)];
		std::vector<trimesh::index_t> faces = tm->mesh.vertex_face_neighbors(ind);
		for (size_t k = 0; k < faces.size(); k++) {
		    singularity_triangles.insert(faces[k]);
		}
	    }
	}
    }

    bu_file_delete("singularity_triangles.plot3");
    plot_trimesh_tris_3d(&singularity_triangles, tm->triangles, pointmap, "singularity_triangles.plot3");

    // Find the furthest distance of any active triangle vertex to the singularity point - this
    // will define our 'local' region in which to add triangles
    double max_connected_dist = 0.0;
    for (f_it = singularity_triangles.begin(); f_it != singularity_triangles.end(); f_it++) {
	p2t::Triangle *t = tm->triangles[*f_it].t;
	for (size_t j = 0; j < 3; j++) {
	    ON_3dPoint *vpnt = (*pointmap)[t->GetPoint(j)];
	    double vpntdist = vpnt->DistanceTo(*p3d);
	    max_connected_dist = (max_connected_dist < vpntdist) ? vpntdist : max_connected_dist;
	}
    }

    // Find the best fit plane for the vertices of the triangles involved with
    // the singular point
    std::set<ON_3dPoint *> active_3d_pnts;
    ON_3dVector avgtnorm(0.0,0.0,0.0);
    for (f_it = singularity_triangles.begin(); f_it != singularity_triangles.end(); f_it++) {
	p2t::Triangle *t = tm->triangles[*f_it].t;
	ON_3dVector tdir = p2tTri_Normal(t, pointmap);
	avgtnorm += tdir;
	for (size_t j = 0; j < 3; j++) {
	    active_3d_pnts.insert((*pointmap)[t->GetPoint(j)]);
	}
    }
    avgtnorm = avgtnorm * 1.0/(double)singularity_triangles.size();

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

    bu_file_delete("best_fit_plane_1.plot3");
    plot_best_fit_plane(&pcenter, &pnorm, "best_fit_plane_1.plot3");

    // Walk out along the mesh, adding triangles with at least 1 vert within max_connected_dist and
    // whose triangle normal is < 45 degrees away from that of the original best fit plane
    std::set<trimesh::index_t> flipped_faces;
    std::queue<trimesh::index_t> q1, q2;
    std::queue<trimesh::index_t> *tq, *nq;
    tq = &q1;
    nq = &q2;
    for (f_it = singularity_triangles.begin(); f_it != singularity_triangles.end(); f_it++) {
	q1.push(*f_it);
    }
    while (!tq->empty()) {
	trimesh::index_t t_he = tq->front();
	tq->pop();
	p2t::Triangle *t = tm->triangles[t_he].t;
	// Check normal
	ON_3dVector tdir = p2tTri_Normal(t, pointmap);
	ON_3dVector on_pnorm(pnorm[X], pnorm[Y], pnorm[Z]);

	// If this triangle is "flipped" or nearly so relative to the NURBS surface
	// add it.  If an edge from this triangle ends up in the outer boundary,
	// we're going to have to fix the outer boundary in the projection...
	bool ff = flipped_face(t, pointmap, normalmap, f->s_cdt->brep->m_F[f->ind].m_bRev);

	if (!ff && ON_DotProduct(tdir, on_pnorm) < 0.707) {
	    continue;
	}
	for (size_t i = 0; i < 3; i++) {
	    trimesh::index_t ind = tm->p2dind[t->GetPoint(i)];
	    std::vector<trimesh::index_t> faces = tm->mesh.vertex_face_neighbors(ind);
	    for (size_t j = 0; j < faces.size(); j++) {
		if (singularity_triangles.find(faces[j]) != singularity_triangles.end()) {
		    // We've already got this one, keep going
		    continue;
		}
		p2t::Triangle *tn = tm->triangles[faces[j]].t;

		int is_close = 0;
		for (size_t k = 0; k < 3; k++) {
		    // If at least one vertex is within max_connected_dist, queue up
		    // the triangle
		    ON_3dPoint *vnpt = (*pointmap)[tn->GetPoint(k)];
		    if (p3d->DistanceTo(*vnpt) < max_connected_dist) {
			is_close = 1;
			break;
		    }
		}
		if (is_close) {
		    if (ff) {
			flipped_faces.insert(faces[j]);
			bu_log("adding flipped face\n");
		    } else {
			bu_log("adding face\n");
		    }
		    nq->push(faces[j]);
		    singularity_triangles.insert(faces[j]);
		}
	    }
	}

	if (tq->empty()) {
	    std::queue<trimesh::index_t> *tmpq = tq;
	    tq = nq;
	    nq = tmpq;
	}
    }

    bu_file_delete("singularity_triangles_2.plot3");
    plot_trimesh_tris_3d(&singularity_triangles, tm->triangles, pointmap, "singularity_triangles_2.plot3");

    // Project all 3D points in the subset into the plane, getting XY coordinates
    // for poly2Tri.
    std::set<ON_3dPoint *> sub_3d;
    std::set<trimesh::index_t>::iterator tr_it;
    for (tr_it = singularity_triangles.begin(); tr_it != singularity_triangles.end(); tr_it++) {
	p2t::Triangle *t = tm->triangles[*tr_it].t;
	for (size_t j = 0; j < 3; j++) {
	    sub_3d.insert((*pointmap)[t->GetPoint(j)]);
	}
    }
    std::vector<ON_3dPoint *> pnts_3d(sub_3d.begin(), sub_3d.end());
    std::vector<ON_2dPoint *> pnts_2d;
    std::map<ON_3dPoint *, size_t> pnts_3d_ind;
    bu_log("singularity triangle cnt: %zd\n", singularity_triangles.size());
    
    for (size_t i = 0; i < pnts_3d.size(); i++) {
	pnts_3d_ind[pnts_3d[i]] = i;
    }
    std::vector<trimesh::triangle_t> submesh_triangles;
    std::set<trimesh::index_t> smtri;
    for (tr_it = singularity_triangles.begin(); tr_it != singularity_triangles.end(); tr_it++) {
	p2t::Triangle *t = tm->triangles[*tr_it].t;
	trimesh::triangle_t tmt;
	for (size_t j = 0; j < 3; j++) {
	    tmt.v[j] = pnts_3d_ind[(*pointmap)[t->GetPoint(j)]];
	}
	tmt.t = t;
	if (tmt.v[0] != tmt.v[1] && tmt.v[0] != tmt.v[2] && tmt.v[1] != tmt.v[2]) {
	    smtri.insert(submesh_triangles.size());
	    submesh_triangles.push_back(tmt);
	} else {
	    bu_log("Skipping: %ld -> %ld -> %ld\n", tmt.v[0], tmt.v[1], tmt.v[2]);
	}

    }
    bu_log("submesh triangle cnt: %zd\n", submesh_triangles.size());

    ON_Plane bfplane(ON_3dPoint(pcenter[X],pcenter[Y],pcenter[Z]),ON_3dVector(pnorm[X],pnorm[Y],pnorm[Z]));
    ON_Xform to_plane;
    to_plane.PlanarProjection(bfplane);
    for (size_t i = 0; i < pnts_3d.size(); i++) {
	ON_3dPoint op3d = (*pnts_3d[i]);
	op3d.Transform(to_plane);
	pnts_2d.push_back(new ON_2dPoint(op3d.x, op3d.y)); 
    }

    std::vector<trimesh::edge_t> sedges;
    trimesh::trimesh_t smesh;
    trimesh::unordered_edges_from_triangles(submesh_triangles.size(), &submesh_triangles[0], sedges);
    smesh.build(pnts_2d.size(), submesh_triangles.size(), &submesh_triangles[0], sedges.size(), &sedges[0]);

    // Build the new outer boundary
    std::vector<std::pair<trimesh::index_t, trimesh::index_t>> bedges = smesh.boundary_edges();
    bu_log("boundary edge segment cnt: %zd\n", bedges.size());

    plot_edge_set(bedges, pnts_3d, "outer_edge.plot3");
    //plot_trimesh_tris_3d(&smtri, submesh_triangles, pointmap, "submesh_triangles.plot3");

    // Given the set of unordered boundary edge segments, construct the outer loop
    std::vector<trimesh::index_t> sloop = smesh.boundary_loop();

    // TODO - may do better to just use https://github.com/sadaszewski/concaveman-cpp to
    // build the loop we want from the outer points, rather than the (questionable) behavior
    // of the projected 3D loop...
    plot_edge_loop(sloop, pnts_3d, "outer_loop.plot3");

    int ccnt = 0;
    int hull_dir = 0;
    point2d_t *hull;
    {
	point2d_t *bpnts_2d = (point2d_t *)bu_calloc(sloop.size()+2, sizeof(point_t), "concave hull 2D points");
	for (size_t i = 0; i < sloop.size(); i++) {
	    ON_2dPoint *p = pnts_2d[sloop[i]];
	    bpnts_2d[i][X] = p->x;
	    bpnts_2d[i][Y] = p->y;
	}

	bu_log("sloop.size: %zd\n", sloop.size());

	ccnt = bg_2d_concave_hull(&hull, bpnts_2d, (int)sloop.size());
	if (!ccnt) {
	    bu_log("concave hull build failed\n");
	} else {
	    int *pt_ind = (int *)bu_calloc(ccnt, sizeof(int), "pt_ind");
	    for (int i = 0; i < ccnt; i++) {
		pt_ind[i] = i;
	    }
	    hull_dir = bg_polygon_direction(ccnt, hull, pt_ind);
	    bu_free(pt_ind, "point indices");
	    bu_log("ccnt: %d\n", ccnt);
	    if (!hull_dir) {
		bu_log("could not determine hull direction!\n");
	    }
	    if (hull_dir == BG_CCW) bu_log("CCW\n");
	    if (hull_dir == BG_CW) bu_log("CW\n");
	    plot_concave_hull_2d(hull, ccnt, "concave_hull.plot3");

	    for (int i = 0; i < ccnt; i++) {
		bu_log("%d: %f %f 0\n", i, hull[i][X], hull[i][Y]);
	    }

	    int *ecfaces;
	    int num_faces;
	    if (bg_polygon_triangulate(&ecfaces, &num_faces, NULL, NULL, hull, ccnt, TRI_EAR_CLIPPING)) {
		bu_log("ear clipping failed\n");
	    } else {
		plot_2d_bg_tri(ecfaces, num_faces, hull, "earclip.plot3");
	    }
	}
    }

    if (flipped_faces.size() > 0) {
	bu_log("WARNING: incorporated flipped faces - need to do outer boundary ordering sanity checking!!\n");
    }

    std::set<ON_2dPoint *> handled;
    std::set<ON_2dPoint *>::iterator u_it;
    std::vector<p2t::Point *> polyline;
    p2t::Point *fp = NULL;
    if (!ccnt) {
	for (size_t i = 0; i < sloop.size(); i++) {
	    ON_2dPoint *onp2d = pnts_2d[sloop[i]];
	    p2t::Point *np = new p2t::Point(onp2d->x, onp2d->y);
	    if (!fp) fp = np;
	    (*pointmap)[np] = pnts_3d[i];
	    polyline.push_back(np);
	    handled.insert(onp2d);
	}
    } else {
	for (int i = 0; i < ccnt; i++) {
	    p2t::Point *np = new p2t::Point(hull[i][X], hull[i][Y]);
	    if (!fp) fp = np;
	    (*pointmap)[np] = pnts_3d[i];
	    polyline.push_back(np);
	}
	//polyline.push_back(fp);
    }
    plot_edge_loop_2d(polyline, "polyline.plot3");
    // Perform the new triangulation
    p2t::CDT *ncdt = new p2t::CDT(polyline);
#if 0
    for (size_t i = 0; i < pnts_2d.size(); i++) {
	ON_2dPoint *onp2d = pnts_2d[i];
	if (handled.find(onp2d) != handled.end()) continue;
	p2t::Point *np = new p2t::Point(onp2d->x, onp2d->y);
	(*pointmap)[np] = pnts_3d[i];
	ncdt->AddPoint(np);
    }
#endif
    ncdt->Triangulate(true, -1);
    plot_2d_cdt_tri(ncdt, "poly2tri_2d.plot3");

    // assemble the new p2t Triangle set using
    // the mappings, and check the new 3D mesh thus created for issues.  If it
    // is clean, replace the original subset of the parent face with the new
    // triangle set, else fail.  Note that a consequence of this will be that
    // only one of the original singularity trim points will end up being used.



}

static int
do_triangulation(struct ON_Brep_CDT_Face_State *f, int full_surface_sample, int cnt)
{
    int ret = 0;

    if (cnt > MAX_TRIANGULATION_ATTEMPTS) {
	std::cerr << "Error: even after " << MAX_TRIANGULATION_ATTEMPTS << " iterations could not successfully triangulate face " << f->ind << "\n";
	return 0;
    }
    // TODO - for now, don't reset before returning the error - we want to see the
    // failed mesh for diagnostics
    ON_Brep_CDT_Face_Reset(f, full_surface_sample);

    bool outer = build_poly2tri_polylines(f, full_surface_sample);
    if (outer) {
	std::cerr << "Error: Face(" << f->ind << ") cannot evaluate its outer loop and will not be facetized." << std::endl;
	return -1;
    }

    if (full_surface_sample) {
	// Sample the surface, independent of the trimming curves, to get points that
	// will tie the mesh to the interior surface.
	getSurfacePoints(f);
    }

    std::set<ON_2dPoint *>::iterator p_it;
    for (p_it = f->on_surf_points->begin(); p_it != f->on_surf_points->end(); p_it++) {
	ON_2dPoint *p = *p_it;
	p2t::Point *tp = new p2t::Point(p->x, p->y);
	(*f->p2t_to_on2_map)[tp] = p;
	f->cdt->AddPoint(tp);
    }

    // All preliminary steps are complete, perform the triangulation using
    // Poly2Tri's triangulation.  NOTE: it is important that the inputs to
    // Poly2Tri satisfy its constraints - failure here could cause a crash.
    f->cdt->Triangulate(true, -1);

    /* Calculate any 3D points we don't already have */
    populate_3d_pnts(f);


    // TODO - this needs to be considerably more sophisticated - only fall back
    // on the singular surface logic if the problem points are from faces that
    // involve a singularity, not just flagging on the surface.  Ideally, pass
    // in the singularity point that is the particular problem so the routine
    // knows where it's working
    if (f->has_singular_trims) {
	std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
	std::vector<p2t::Triangle*> tv = f->cdt->GetTriangles();
	std::set<p2t::Triangle *> triset(tv.begin(), tv.end());
	struct trimesh_info *tm = CDT_Face_Build_Halfedge(&triset, f->tris_degen);

	// Identify any singular points
	std::set<ON_3dPoint *> active_singular_pnts;
	for (size_t i = 0; i < tv.size(); i++) {
	    p2t::Triangle *t = tv[i];
	    if (f->tris_degen->find(t) != f->tris_degen->end()) {
		continue;
	    }
	    for (size_t j = 0; j < 3; j++) {
		ON_3dPoint *p3d = (*pointmap)[t->GetPoint(j)];
		if (f->s_cdt->singular_vert_to_norms->find(p3d) != f->s_cdt->singular_vert_to_norms->end()) {
		    active_singular_pnts.insert(p3d);
		}
	    }
	}
	std::set<ON_3dPoint *>::iterator a_it;
	for (a_it = active_singular_pnts.begin(); a_it != active_singular_pnts.end(); a_it++) {
	    Plot_Singular_Connected(f, tm, *a_it);
	}

	delete tm;
    }


    // Trivially degenerate pass
    triangles_degenerate_trivial(f);

    // Zero area triangles
    triangles_degenerate_area(f);

    // Validate based on edges.  If we get a return > 0, adjustments were
    // made to the surface points and we need to redo the triangulation
    int eret = triangles_check_edges(f);
    if (eret < 0) {
	bu_log("Fatal failure on edge checking\n");
	return -1;
    }
    ret += eret;

    // Incorrect normals.  If we get a return > 0, adjustments were mde
    // to the surface points and we need to redo the triangulation
    int nret = triangles_incorrect_normals(f);
    if (nret < 0) {
	bu_log("Fatal failure on normals checking\n");
	return -1;
    }
    ret += nret;

    if (ret > 0) {
	return do_triangulation(f, 0, cnt+1);
    }

    if (!ret) {
	bu_log("Face %d: successful triangulation after %d passes\n", f->ind, cnt);
    }
    return ret;
}

static int
ON_Brep_CDT_Face(struct ON_Brep_CDT_Face_State *f, std::map<const ON_Surface *, double> *s_to_maxdist)
{
    struct ON_Brep_CDT_State *s_cdt = f->s_cdt;
    int face_index = f->ind;
    ON_BrepFace &face = f->s_cdt->brep->m_F[face_index];
    const ON_Surface *s = face.SurfaceOf();
    int loop_cnt = face.LoopCount();
    ON_SimpleArray<BrepTrimPoint> *brep_loop_points = (f->face_loop_points) ? f->face_loop_points : new ON_SimpleArray<BrepTrimPoint>[loop_cnt];
    f->face_loop_points = brep_loop_points;

    // If this face is using at least one singular trim, set the flag
    for (int li = 0; li < loop_cnt; li++) {
	ON_BrepLoop *l = face.Loop(li);
	int trim_count = l->TrimCount();
	for (int lti = 0; lti < trim_count; lti++) {
	    ON_BrepTrim *trim = l->Trim(lti);
	    if (trim->m_type == ON_BrepTrim::singular) {
		f->has_singular_trims = 1;
		break;
	    }
	}
	if (f->has_singular_trims) {
	    break;
	}
    }


    // Use the edge curves and loops to generate an initial set of trim polygons.
    for (int li = 0; li < loop_cnt; li++) {
	double max_dist = 0.0;
	if (s_to_maxdist->find(face.SurfaceOf()) != s_to_maxdist->end()) {
	    max_dist = (*s_to_maxdist)[face.SurfaceOf()];
	}
	Process_Loop_Edges(f, li, max_dist);
    }

    // Handle a variety of situations that complicate loop handling on closed surfaces
    if (s->IsClosed(0) || s->IsClosed(1)) {
	PerformClosedSurfaceChecks(s_cdt, s, face, brep_loop_points, BREP_SAME_POINT_TOLERANCE);
    }

    // Find for this face, find the minimum and maximum edge polyline segment lengths
    (*s_cdt->max_edge_seg_len)[face.m_face_index] = 0.0;
    (*s_cdt->min_edge_seg_len)[face.m_face_index] = DBL_MAX;
    for (int li = 0; li < loop_cnt; li++) {
	int num_loop_points = brep_loop_points[li].Count();
	if (num_loop_points > 1) {
	    ON_3dPoint *p1 = (brep_loop_points[li])[0].p3d;
	    ON_3dPoint *p2 = NULL;
	    for (int i = 1; i < num_loop_points; i++) {
		p2 = p1;
		p1 = (brep_loop_points[li])[i].p3d;
		fastf_t dist = p1->DistanceTo(*p2);
		if (dist > (*s_cdt->max_edge_seg_len)[face.m_face_index]) {
		    (*s_cdt->max_edge_seg_len)[face.m_face_index] = dist;
		}
		if ((dist > SMALL_FASTF) && (dist < (*s_cdt->min_edge_seg_len)[face.m_face_index]))  {
		    (*s_cdt->min_edge_seg_len)[face.m_face_index] = dist;
		}
	    }
	}
    }


    // TODO - we may need to add 2D points on trims that the edges didn't know
    // about.  Since 3D points must be shared along edges and we're using
    // synchronized numbers of parametric domain ordered 2D and 3D points to
    // make that work, we will need to track new 2D points and update the
    // corresponding 3D edge based data structures.  More than that - we must
    // also update all 2D structures in all other faces associated with the
    // edge in question to keep things in overall sync.

    // TODO - if we are going to apply clipper boolean resolution to sets of
    // face loops, that would come here - once we have assembled the loop
    // polygons for the faces. That also has the potential to generate "new" 3D
    // points on edges that are driven by 2D boolean intersections between
    // trimming loops, and may require another update pass as above.

    return do_triangulation(f, 1, 0);
}

static ON_3dVector
calc_trim_vnorm(ON_BrepVertex& v, ON_BrepTrim *trim)
{
    ON_3dPoint t1, t2;
    ON_3dVector v1 = ON_3dVector::UnsetVector;
    ON_3dVector v2 = ON_3dVector::UnsetVector;
    ON_3dVector trim_norm = ON_3dVector::UnsetVector;

    ON_Interval trange = trim->Domain();
    ON_3dPoint t_2d1 = trim->PointAt(trange[0]);
    ON_3dPoint t_2d2 = trim->PointAt(trange[1]);

    ON_Plane fplane;
    const ON_Surface *s = trim->SurfaceOf();
    if (s->IsPlanar(&fplane, BREP_PLANAR_TOL)) {
	trim_norm = fplane.Normal();
	if (trim->Face()->m_bRev) {
	    trim_norm = trim_norm * -1;
	}
    } else {
	int ev1 = 0;
	int ev2 = 0;
	if (surface_EvNormal(s, t_2d1.x, t_2d1.y, t1, v1)) {
	    if (trim->Face()->m_bRev) {
		v1 = v1 * -1;
	    }
	    if (v.Point().DistanceTo(t1) < ON_ZERO_TOLERANCE) {
		ev1 = 1;
		trim_norm = v1;
	    }
	}
	if (surface_EvNormal(s, t_2d2.x, t_2d2.y, t2, v2)) {
	    if (trim->Face()->m_bRev) {
		v2 = v2 * -1;
	    }
	    if (v.Point().DistanceTo(t2) < ON_ZERO_TOLERANCE) {
		ev2 = 1;
		trim_norm = v2;
	    }
	}
	// If we got both of them, go with the closest one
	if (ev1 && ev2) {
#if 0
	    if (ON_DotProduct(v1, v2) < 0) {
		bu_log("Vertex %d(%f %f %f), trim %d: got both normals\n", v.m_vertex_index, v.Point().x, v.Point().y, v.Point().z, trim->m_trim_index);
		bu_log("v1(%f)(%f %f)(%f %f %f): %f %f %f\n", v.Point().DistanceTo(t1), t_2d1.x, t_2d1.y, t1.x, t1.y, t1.z, v1.x, v1.y, v1.z);
		bu_log("v2(%f)(%f %f)(%f %f %f): %f %f %f\n", v.Point().DistanceTo(t2), t_2d2.x, t_2d2.y, t2.x, t2.y, t2.z, v2.x, v2.y, v2.z);
	    }
#endif
	    trim_norm = (v.Point().DistanceTo(t1) < v.Point().DistanceTo(t2)) ? v1 : v2;
	}
    }

    return trim_norm;
}

static void
calc_singular_vert_norm(struct ON_Brep_CDT_State *s_cdt, int index)
{
    ON_BrepVertex& v = s_cdt->brep->m_V[index];
    int have_calculated = 0;
    ON_3dVector vnrml(0.0, 0.0, 0.0);

    if (s_cdt->singular_vert_to_norms->find((*s_cdt->vert_pnts)[index]) != (s_cdt->singular_vert_to_norms->end())) {
	// Already processed this one
	return;
    }

    bu_log("Processing vert %d (%f %f %f)\n", index, v.Point().x, v.Point().y, v.Point().z);

    for (int eind = 0; eind != v.EdgeCount(); eind++) {
	ON_3dVector trim1_norm = ON_3dVector::UnsetVector;
	ON_3dVector trim2_norm = ON_3dVector::UnsetVector;
	ON_BrepEdge& edge = s_cdt->brep->m_E[v.m_ei[eind]];
	if (edge.TrimCount() != 2) {
	    // Don't know what to do with this yet... skip.
	    continue;
	}
	ON_BrepTrim *trim1 = edge.Trim(0);
	ON_BrepTrim *trim2 = edge.Trim(1);

	if (trim1->m_type != ON_BrepTrim::singular) {
	    trim1_norm = calc_trim_vnorm(v, trim1);
	}
	if (trim2->m_type != ON_BrepTrim::singular) {
	    trim2_norm = calc_trim_vnorm(v, trim2);
	}

	// If one of the normals is unset and the other comes from a plane, use it
	if (trim1_norm == ON_3dVector::UnsetVector && trim2_norm != ON_3dVector::UnsetVector) {
	    const ON_Surface *s2 = trim2->SurfaceOf();
	    if (!s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		continue;
	    }
	    trim1_norm = trim2_norm;
	}
	if (trim1_norm != ON_3dVector::UnsetVector && trim2_norm == ON_3dVector::UnsetVector) {
	    const ON_Surface *s1 = trim1->SurfaceOf();
	    if (!s1->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		continue;
	    }
	    trim2_norm = trim1_norm;
	}

	// If we have disagreeing normals and one of them is from a planar surface, go
	// with that one
	if (NEAR_EQUAL(ON_DotProduct(trim1_norm, trim2_norm), -1, VUNITIZE_TOL)) {
	    const ON_Surface *s1 = trim1->SurfaceOf();
	    const ON_Surface *s2 = trim2->SurfaceOf();
	    if (!s1->IsPlanar(NULL, ON_ZERO_TOLERANCE) && !s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		// Normals severely disagree, no planar surface to fall back on - can't use this
		continue;
	    }
	    if (s1->IsPlanar(NULL, ON_ZERO_TOLERANCE) && s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		// Two disagreeing planes - can't use this
		continue;
	    }
	    if (s1->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		trim2_norm = trim1_norm;
	    }
	    if (s2->IsPlanar(NULL, ON_ZERO_TOLERANCE)) {
		trim1_norm = trim2_norm;
	    }
	}

	// Add the normals to the vnrml total
	vnrml += trim1_norm;
	vnrml += trim2_norm;
	have_calculated = 1;

    }
    if (!have_calculated) {
	return;
    }

    // Average all the successfully calculated normals into a new unit normal
    vnrml.Unitize();

    // We store this as a point to keep C++ happy...  If we try to
    // propagate the ON_3dVector type through all the CDT logic it
    // triggers issues with the compile.
    (*s_cdt->vert_avg_norms)[index] = new ON_3dPoint(vnrml);
    s_cdt->w3dnorms->push_back((*s_cdt->vert_avg_norms)[index]);

    // If we have a vertex normal, add it to the map which will allow us
    // to ascertain if a given point has such a normal.  This will allow
    // a point-based check even if we don't know a vertex index locally
    // in the code.
    (*s_cdt->singular_vert_to_norms)[(*s_cdt->vert_pnts)[index]] = (*s_cdt->vert_avg_norms)[index];

}

int
ON_Brep_CDT_Tessellate(struct ON_Brep_CDT_State *s_cdt, int face_cnt, int *faces)
{

    ON_wString wstr;
    ON_TextLog tl(wstr);

    // Check for any conditions that are show-stoppers
    ON_wString wonstr;
    ON_TextLog vout(wonstr);
    if (!s_cdt->orig_brep->IsValid(&vout)) {
	bu_log("brep is NOT valid, cannot produce watertight mesh\n");
	//return -1;
    }

    // For now, edges must have 2 and only 2 trims for this to work.
    for (int index = 0; index < s_cdt->orig_brep->m_E.Count(); index++) {
        ON_BrepEdge& edge = s_cdt->orig_brep->m_E[index];
        if (edge.TrimCount() != 2) {
	    bu_log("Edge %d trim count: %d - can't (yet) do watertight meshing\n", edge.m_edge_index, edge.TrimCount());
            return -1;
        }
    }

    // We may be changing the ON_Brep data, so work on a copy
    // rather than the original object
    if (!s_cdt->brep) {

	s_cdt->brep = new ON_Brep(*s_cdt->orig_brep);

	// Attempt to minimize situations where 2D and 3D distances get out of sync
	// by shrinking the surfaces down to the active area of the face
	s_cdt->brep->ShrinkSurfaces();

    }

    ON_Brep* brep = s_cdt->brep;
    std::map<const ON_Surface *, double> s_to_maxdist;

    // If this is the first time through, there are a number of once-per-conversion
    // operations to take care of.
    if (!s_cdt->w3dpnts->size()) {
	// Reparameterize the face's surface and transform the "u" and "v"
	// coordinates of all the face's parameter space trimming curves to
	// minimize distortion in the map from parameter space to 3d.
	for (int face_index = 0; face_index < brep->m_F.Count(); face_index++) {
	    ON_BrepFace *face = brep->Face(face_index);
	    const ON_Surface *s = face->SurfaceOf();
	    double surface_width, surface_height;
	    if (s->GetSurfaceSize(&surface_width, &surface_height)) {
		face->SetDomain(0, 0.0, surface_width);
		face->SetDomain(1, 0.0, surface_height);
	    }
	}

	/* We want to use ON_3dPoint pointers and BrepVertex points, but
	 * vert->Point() produces a temporary address.  If this is our first time
	 * through, make stable copies of the Vertex points. */
	for (int index = 0; index < brep->m_V.Count(); index++) {
	    ON_BrepVertex& v = brep->m_V[index];
	    (*s_cdt->vert_pnts)[index] = new ON_3dPoint(v.Point());
	    CDT_Add3DPnt(s_cdt, (*s_cdt->vert_pnts)[index], -1, v.m_vertex_index, -1, -1, -1, -1);
	    // topologically, any vertex point will be on edges
	    s_cdt->edge_pnts->insert((*s_cdt->vert_pnts)[index]);
	}

	/* If this is the first time through, check for singular trims.  For
	 * vertices associated with such a trim get vertex normals that are the
	 * average of the surface normals at the junction from faces that don't
	 * use a singular trim to reference the vertex.
	 */
	for (int index = 0; index < brep->m_T.Count(); index++) {
	    ON_BrepTrim &trim = s_cdt->brep->m_T[index];
	    if (trim.m_type == ON_BrepTrim::singular) {
		ON_BrepVertex *v1 = trim.Vertex(0);
		ON_BrepVertex *v2 = trim.Vertex(1);
		calc_singular_vert_norm(s_cdt, v1->m_vertex_index);
		calc_singular_vert_norm(s_cdt, v2->m_vertex_index);
	    }
	}

	/* To generate watertight meshes, the faces *must* share 3D edge points.  To ensure
	 * a uniform set of edge points, we first sample all the edges and build their
	 * point sets */
	for (int index = 0; index < brep->m_E.Count(); index++) {
	    ON_BrepEdge& edge = brep->m_E[index];
	    ON_BrepTrim& trim1 = brep->m_T[edge.Trim(0)->m_trim_index];
	    ON_BrepTrim& trim2 = brep->m_T[edge.Trim(1)->m_trim_index];

	    // Get distance tolerances from the surface sizes
	    fastf_t max_dist = 0.0;
	    fastf_t min_dist = DBL_MAX;
	    fastf_t mw, mh;
	    fastf_t md1, md2 = 0.0;
	    double sw1, sh1, sw2, sh2;
	    const ON_Surface *s1 = trim1.Face()->SurfaceOf();
	    const ON_Surface *s2 = trim2.Face()->SurfaceOf();
	    if (s1->GetSurfaceSize(&sw1, &sh1) && s2->GetSurfaceSize(&sw2, &sh2)) {
		if ((sw1 < s_cdt->dist) || (sh1 < s_cdt->dist) || sw2 < s_cdt->dist || sh2 < s_cdt->dist) {
		    return -1;
		}
		md1 = sqrt(sw1 * sh1 + sh1 * sw1) / 10.0;
		md2 = sqrt(sw2 * sh2 + sh2 * sw2) / 10.0;
		max_dist = (md1 < md2) ? md1 : md2;
		mw = (sw1 < sw2) ? sw1 : sw2;
		mh = (sh1 < sh2) ? sh1 : sh2;
		min_dist = (mw < mh) ? mw : mh;
		s_to_maxdist[s1] = max_dist;
		s_to_maxdist[s2] = max_dist;
	    }

	    // Generate the BrepTrimPoint arrays for both trims associated with this edge
	    //
	    // TODO - need to make this robust to changed tolerances on refinement
	    // runs - if pre-existing solutions for "high level" splits exist,
	    // reuse them and dig down to find where we need further refinement to
	    // create new points.
	    (void)getEdgePoints(s_cdt, &edge, trim1, max_dist, min_dist);

	}

    } else {
	/* Clear the mesh state, if this container was previously used */
	s_cdt->tri_brep_face->clear();
    }

    // Process all of the faces we have been instructed to process, or (default) all faces.
    // Keep track of failures and successes.
    int face_failures = 0;
    int face_successes = 0;
    int fc = ((face_cnt == 0) || !faces) ? s_cdt->brep->m_F.Count() : face_cnt;
    for (int i = 0; i < fc; i++) {
	int fi = ((face_cnt == 0) || !faces) ? i : faces[i];
	if (fi < s_cdt->brep->m_F.Count()) {
	    if (s_cdt->faces->find(fi) == s_cdt->faces->end()) {
		struct ON_Brep_CDT_Face_State *f = ON_Brep_CDT_Face_Create(s_cdt, fi);
		(*s_cdt->faces)[fi] = f;
	    }
	    if (ON_Brep_CDT_Face((*s_cdt->faces)[fi], &s_to_maxdist)) {
		face_failures++;
	    } else {
		face_successes++;
	    }
	}
    }

    // If we only tessellated some of the faces, we don't have the
    // full solid mesh yet (by definition).  Return accordingly.
    if (face_failures || !face_successes || face_successes < s_cdt->brep->m_F.Count()) {
	return (face_successes) ? 1 : -1;
    }

    /* We've got face meshes and no reported failures - check to see if we have a
     * solid mesh */
    int valid_fcnt, valid_vcnt;
    int *valid_faces = NULL;
    fastf_t *valid_vertices = NULL;

    if (ON_Brep_CDT_Mesh(&valid_faces, &valid_fcnt, &valid_vertices, &valid_vcnt, NULL, NULL, NULL, NULL, s_cdt) < 0) {
	return -1;
    }

    struct bg_trimesh_solid_errors se = BG_TRIMESH_SOLID_ERRORS_INIT_NULL;
    int invalid = bg_trimesh_solid2(valid_vcnt, valid_fcnt, valid_vertices, valid_faces, &se);

    if (invalid) {
	trimesh_error_report(s_cdt, valid_fcnt, valid_vcnt, valid_faces, valid_vertices, &se);
    }

    bu_free(valid_faces, "faces");
    bu_free(valid_vertices, "vertices");

    if (invalid) {
	return 1;
    }

    return 0;

}


// Generate a BoT with normals.
int
ON_Brep_CDT_Mesh(
	int **faces, int *fcnt,
	fastf_t **vertices, int *vcnt,
	int **face_normals, int *fn_cnt,
	fastf_t **normals, int *ncnt,
	struct ON_Brep_CDT_State *s_cdt)
{
    size_t triangle_cnt = 0;
    if (!faces || !fcnt || !vertices || !vcnt || !s_cdt) {
	return -1;
    }

    /* We can ignore the face normals if we want, but if some of the
     * return variables are non-NULL they all need to be non-NULL */
    if (face_normals || fn_cnt || normals || ncnt) {
	if (!face_normals || !fn_cnt || !normals || !ncnt) {
	    return -1;
	}
    }

    /* For the non-zero-area triangles sharing an edge with a non-trivially
     * degenerate zero area triangle, we need to build new polygons from each
     * triangle and the "orphaned" points along the edge(s).  We then
     * re-tessellate in the triangle's parametric space.
     *
     * An "involved" triangle is a triangle with two of its three points in the
     * face's degen_pnts set (i.e. it shares an edge with a non-trivially
     * degenerate zero-area triangle.)
     */
    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	struct ON_Brep_CDT_Face_State *f = (*s_cdt->faces)[face_index];
	triangles_rebuild_involved(f);
    }

    /* We know now the final triangle set.  We need to build up the set of
     * unique points and normals to generate a mesh containing only the
     * information actually used by the final triangle set. */
    std::set<ON_3dPoint *> vfpnts;
    std::set<ON_3dPoint *> vfnormals;
    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	struct ON_Brep_CDT_Face_State *f = (*s_cdt->faces)[face_index];
	std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
	std::map<p2t::Point *, ON_3dPoint *> *normalmap = f->p2t_to_on3_norm_map;
	std::vector<p2t::Triangle*> tris = f->cdt->GetTriangles();
	for (size_t i = 0; i < tris.size(); i++) {
	    p2t::Triangle *t = tris[i];
	    ON_3dVector tdir = p2tTri_Normal(t, pointmap);
	    if (f->tris_degen->size() > 0 && f->tris_degen->find(t) != f->tris_degen->end()) {
		continue;
	    }
	    for (size_t j = 0; j < 3; j++) {
		p2t::Point *p = t->GetPoint(j);
		ON_3dPoint *p3d = (*pointmap)[p];
		vfpnts.insert(p3d);
		ON_3dPoint *onorm = (*normalmap)[p];
		if (onorm) {
		    double onorm_dp = ON_DotProduct(*onorm, tdir);
		    if (ON_DotProduct(*onorm, tdir) < 0.1) {
			if (s_cdt->singular_vert_to_norms->find(p3d) != s_cdt->singular_vert_to_norms->end()) {
			    // Override singularity points with calculated normal, if present
			    // and a "better" normal than onorm
			    ON_3dPoint *cnrm = (*f->s_cdt->singular_vert_to_norms)[p3d];
			    double vertnorm_dp = ON_DotProduct(*cnrm, tdir);
			    if (vertnorm_dp > onorm_dp) {
				onorm = cnrm;
			    }
			}
		    }
		    vfnormals.insert(onorm);
		}
	    }
	}
	std::vector<p2t::Triangle *> *tri_add = (*s_cdt->faces)[face_index]->p2t_extra_faces;
	for (size_t i = 0; i < tri_add->size(); i++) {
	    p2t::Triangle *t = tris[i];
	    ON_3dVector tdir = p2tTri_Normal(t, pointmap);
	    for (size_t j = 0; j < 3; j++) {
		p2t::Point *p = t->GetPoint(j);
		ON_3dPoint *p3d = (*pointmap)[p];
		vfpnts.insert(p3d);
		ON_3dPoint *onorm = (*normalmap)[p];
		if (onorm) {
		    double onorm_dp = ON_DotProduct(*onorm, tdir);
		    if (ON_DotProduct(*onorm, tdir) < 0.1) {
			if (s_cdt->singular_vert_to_norms->find(p3d) != s_cdt->singular_vert_to_norms->end()) {
			    // Override singularity points with calculated normal, if present
			    // and a "better" normal than onorm
			    ON_3dPoint *cnrm = (*f->s_cdt->singular_vert_to_norms)[p3d];
			    double vertnorm_dp = ON_DotProduct(*cnrm, tdir);
			    if (vertnorm_dp > onorm_dp) {
				onorm = cnrm;
			    }
			}
		    }
		    vfnormals.insert(onorm);
		}
	    }
	}
    }

    // Get the final triangle count
    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	struct ON_Brep_CDT_Face_State *f = (*s_cdt->faces)[face_index];
	triangle_cnt += f->cdt->GetTriangles().size();
	triangle_cnt -= f->tris_degen->size();
	triangle_cnt += f->p2t_extra_faces->size();
    }

    bu_log("tri_cnt: %zd\n", triangle_cnt);

    // We know how many faces, points and normals we need now - initialize BoT containers.
    *fcnt = (int)triangle_cnt;
    *faces = (int *)bu_calloc(triangle_cnt*3, sizeof(int), "new faces array");
    *vcnt = (int)vfpnts.size();
    *vertices = (fastf_t *)bu_calloc(vfpnts.size()*3, sizeof(fastf_t), "new vert array");
    if (normals) {
	*ncnt = (int)vfnormals.size();
	*normals = (fastf_t *)bu_calloc(vfnormals.size()*3, sizeof(fastf_t), "new normals array");
	*fn_cnt = (int)triangle_cnt;
	*face_normals = (int *)bu_calloc(triangle_cnt*3, sizeof(int), "new face_normals array");
    }

    // Populate the arrays and map the ON containers to their corresponding BoT array entries
    std::set<ON_3dPoint *>::iterator p_it;

    // Index vertex points and assign them to the BoT array
    std::map<ON_3dPoint *, int> on_pnt_to_bot_pnt;
    int pnt_ind = 0;
    for (p_it = vfpnts.begin(); p_it != vfpnts.end(); p_it++) {
	ON_3dPoint *vp = *p_it;
	(*vertices)[pnt_ind*3] = vp->x;
	(*vertices)[pnt_ind*3+1] = vp->y;
	(*vertices)[pnt_ind*3+2] = vp->z;
	on_pnt_to_bot_pnt[vp] = pnt_ind;
	(*s_cdt->bot_pnt_to_on_pnt)[pnt_ind] = vp;
	pnt_ind++;
    }

    // Index vertex normal vectors and assign them to the BoT array.  Normal
    // vectors are not always uniquely mapped to vertices (consider, for
    // example, the triangles joining at a sharp edge of a box), but what we
    // are doing here is establishing unique integer identifiers for all normal
    // vectors.  In other words, this is not a unique vertex to normal mapping,
    // but a normal vector to unique index mapping.
    //
    // The mapping of 2D triangle point to its associated normal is the
    // responsibility of the  p2t_to_on3_norm_map container
    std::map<ON_3dPoint *, int> on_norm_to_bot_norm;
    size_t norm_ind = 0;
    if (normals) {
	for (p_it = vfnormals.begin(); p_it != vfnormals.end(); p_it++) {
	    ON_3dPoint *vn = *p_it;
	    (*normals)[norm_ind*3] = vn->x;
	    (*normals)[norm_ind*3+1] = vn->y;
	    (*normals)[norm_ind*3+2] = vn->z;
	    on_norm_to_bot_norm[vn] = norm_ind;
	    norm_ind++;
	}
    }

    // Iterate over faces, adding points and faces to BoT container.  Note: all
    // 3D points should be geometrically unique in this final container.
    int face_cnt = 0;
    triangle_cnt = 0;
    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	struct ON_Brep_CDT_Face_State *f = (*s_cdt->faces)[face_index];
	p2t::CDT *cdt = f->cdt;
	std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
	std::map<p2t::Point *, ON_3dPoint *> *normalmap = f->p2t_to_on3_norm_map;
	std::vector<p2t::Triangle*> tris = cdt->GetTriangles();
	triangle_cnt += tris.size();
	int active_tris = 0;
	for (size_t i = 0; i < tris.size(); i++) {
	    p2t::Triangle *t = tris[i];
	    if (f->tris_degen->size() > 0 && f->tris_degen->find(t) != f->tris_degen->end()) {
		triangle_cnt--;
		continue;
	    }
	    ON_3dVector tdir = p2tTri_Normal(t, pointmap);
	    active_tris++;
	    for (size_t j = 0; j < 3; j++) {
		p2t::Point *p = t->GetPoint(j);
		ON_3dPoint *op = (*pointmap)[p];
		(*faces)[face_cnt*3 + j] = on_pnt_to_bot_pnt[op];
		if (normals) {
		    ON_3dPoint *onorm = (*normalmap)[p];
		    double onorm_dp = ON_DotProduct(*onorm, tdir);
		    if (ON_DotProduct(*onorm, tdir) < 0.1) {
			if (s_cdt->singular_vert_to_norms->find(op) != s_cdt->singular_vert_to_norms->end()) {
			    // Override singularity points with calculated normal, if present
			    // and a "better" normal than onorm
			    ON_3dPoint *cnrm = (*f->s_cdt->singular_vert_to_norms)[op];
			    double vertnorm_dp = ON_DotProduct(*cnrm, tdir);
			    if (vertnorm_dp > onorm_dp) {
				onorm = cnrm;
			    }
			}
		    }
		    (*face_normals)[face_cnt*3 + j] = on_norm_to_bot_norm[onorm];
		}
	    }
	    // If we have a reversed face we need to adjust the triangle vertex
	    // ordering.
	    if (s_cdt->brep->m_F[face_index].m_bRev) {
		int ftmp = (*faces)[face_cnt*3 + 1];
		(*faces)[face_cnt*3 + 1] = (*faces)[face_cnt*3 + 2];
		(*faces)[face_cnt*3 + 2] = ftmp;
		if (normals) {
		    // Keep the normals in sync with the vertex points
		    int fntmp = (*face_normals)[face_cnt*3 + 1];
		    (*face_normals)[face_cnt*3 + 1] = (*face_normals)[face_cnt*3 + 2];
		    (*face_normals)[face_cnt*3 + 2] = fntmp;
		}
	    }

	    face_cnt++;
	}
    }
    //bu_log("tri_cnt_1: %zd\n", triangle_cnt);
    //bu_log("face_cnt: %d\n", face_cnt);

    for (int face_index = 0; face_index != s_cdt->brep->m_F.Count(); face_index++) {
	struct ON_Brep_CDT_Face_State *f = (*s_cdt->faces)[face_index];
	std::vector<p2t::Triangle *> *tri_add = f->p2t_extra_faces;
	std::map<p2t::Point *, ON_3dPoint *> *pointmap = f->p2t_to_on3_map;
	std::map<p2t::Point *, ON_3dPoint *> *normalmap = f->p2t_to_on3_norm_map;
	triangle_cnt += tri_add->size();
	for (size_t i = 0; i < tri_add->size(); i++) {
	    p2t::Triangle *t = (*tri_add)[i];
	    ON_3dVector tdir = p2tTri_Normal(t, pointmap);
	    for (size_t j = 0; j < 3; j++) {
		p2t::Point *p = t->GetPoint(j);
		ON_3dPoint *op = (*pointmap)[p];
		(*faces)[face_cnt*3 + j] = on_pnt_to_bot_pnt[op];
		if (normals) {
		    ON_3dPoint *onorm = (*normalmap)[p];
		    double onorm_dp = ON_DotProduct(*onorm, tdir);
		    if (ON_DotProduct(*onorm, tdir) < 0.1) {
			if (s_cdt->singular_vert_to_norms->find(op) != s_cdt->singular_vert_to_norms->end()) {
			    // Override singularity points with calculated normal, if present
			    // and a "better" normal than onorm
			    ON_3dPoint *cnrm = (*f->s_cdt->singular_vert_to_norms)[op];
			    double vertnorm_dp = ON_DotProduct(*cnrm, tdir);
			    if (vertnorm_dp > onorm_dp) {
				onorm = cnrm;
			    }
			}
		    }
		    (*face_normals)[face_cnt*3 + j] = on_norm_to_bot_norm[onorm];
		}
	    }
	    // If we have a reversed face we need to adjust the triangle vertex
	    // ordering.
	    if (s_cdt->brep->m_F[face_index].m_bRev) {
		int ftmp = (*faces)[face_cnt*3 + 1];
		(*faces)[face_cnt*3 + 1] = (*faces)[face_cnt*3 + 2];
		(*faces)[face_cnt*3 + 2] = ftmp;
		if (normals) {
		    // Keep the normals in sync with the vertex points
		    int fntmp = (*face_normals)[face_cnt*3 + 1];
		    (*face_normals)[face_cnt*3 + 1] = (*face_normals)[face_cnt*3 + 2];
		    (*face_normals)[face_cnt*3 + 2] = fntmp;
		}
	    }

	    face_cnt++;
	}
	//bu_log("added faces for %d: %zd\n", face_index, tri_add->size());
	//bu_log("tri_cnt_2: %zd\n", triangle_cnt);
	//bu_log("face_cnt_2: %d\n", face_cnt);
    }

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

