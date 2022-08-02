/*                      P L A N A R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file planar.cpp
 *
 * Brief description
 *
 */

#include "common.h"

#include <vector>
#include <set>
#include <map>
#include <algorithm>

#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "bg/polygon.h"
#include "bg/tri_ray.h"
#include "brep/util.h"
#include "shape_recognition.h"


#define MAX_CNT_CONVEX_PLANE_TEST 15

// Check for plane intersection points that are inside/outside the
// arb.  If a set of three planes defines a point that is inside, those
// planes are part of the final arb.  Based on the arbn prep test.
void
convex_plane_usage(ON_SimpleArray<ON_Plane> *planes, int **pu)
{
    int *planes_used = (*pu);

    for (int i = 0; i < planes->Count()-2; i++) {
	for (int j = i + 1; j < planes->Count()-1; j++) {
	    // If normals are parallel, no intersection
	    if ((*planes)[i].Normal().IsParallelTo((*planes)[j].Normal(), 0.0005)) continue;
	    ON_Line l_plane;
	    ON_Intersect((*planes)[i], (*planes)[j], l_plane);
	    for (int k = j + 1; k < planes->Count(); k++) {
		if ((*planes)[k].Normal().IsPerpendicularTo(l_plane.Direction(), 0.0005)) continue;
		int not_used = 0;
		double l_param;
		ON_Plane p3 = (*planes)[k];
		ON_Intersect(l_plane, (*planes)[k], &l_param);
		ON_3dPoint p3d = l_plane.PointAt(l_param);
		/* See if point is outside arb */
		for (int m = 0; m < planes->Count(); m++) {
		    if (m == i || m == j || m == k) continue;
		    if (ON_DotProduct(p3d - (*planes)[m].origin, (*planes)[m].Normal()) > 0) {
			not_used = 1;
			break;
		    }
		}
		if (not_used) continue;
		planes_used[i]++;
		planes_used[j]++;
		planes_used[k]++;
	    }
	}
    }
}


int
triangulate_array(ON_2dPointArray &on2dpts, int *verts_map, int **ffaces, int loop_dir, int *rev_status)
{
    int num_faces = 0;

    int *pt_ind = (int *)bu_calloc(on2dpts.Count(), sizeof(int), "pt_ind");
    point2d_t *verts2d = (point2d_t *)bu_calloc(on2dpts.Count(), sizeof(point2d_t), "bot verts");
    for (int i = 0; i < on2dpts.Count(); i++) {
	V2MOVE(verts2d[i], on2dpts[i]);
	pt_ind[i] = i;
    }

    int ccw = bg_polygon_direction(on2dpts.Count(), verts2d, pt_ind);

    if (loop_dir && ccw == loop_dir) {
	//bu_log("loop direction flipped - has consequences.\n");
	(*rev_status) = 1;
    }

    if (ccw == 0) {
	//bu_log("degenerate loop - skip\n");
	bu_free(verts2d, "free verts2d");
	bu_free(pt_ind, "free verts2d");
	return 0;
    }
    if (ccw == BG_CW) {
	for (int i = on2dpts.Count() - 1; i >= 0; i--) {
	    int d = on2dpts.Count() - 1 - i;
	    V2MOVE(verts2d[d], on2dpts[i]);
	}
	//bu_log("flip loop\n");
	std::vector<int> vert_map;
	for (int i = 0; i < on2dpts.Count(); i++) vert_map.push_back(verts_map[i]);
	std::reverse(vert_map.begin(), vert_map.end());
	for (int i = 0; i < on2dpts.Count(); i++) verts_map[i] = vert_map[i];
    }

    if (bg_polygon_triangulate(ffaces, &num_faces, NULL, NULL, NULL, 0, (const point2d_t *)verts2d, on2dpts.Count(), TRI_EAR_CLIPPING)) {
	bu_free(verts2d, "free verts2d");
	bu_free(pt_ind, "free verts2d");
	return 0;
    }

    // fix up vertex indices
    for (int i = 0; i < num_faces*3; i++) {
	int old_ind = (*ffaces)[i];
	(*ffaces)[i] = verts_map[old_ind];
    }

    bu_free(verts2d, "free verts2d");
    bu_free(pt_ind, "free verts2d");

    return num_faces;
}


// This shouldn't actually be needed if we don't have self intersecting islands...
int
triangulate_array_with_holes(ON_2dPointArray &on2dpts, int *verts_map, int loop_cnt, int *loop_starts, int **ffaces, const ON_Brep *UNUSED(brep))
{
    int ccw;
    int num_faces;

    // Only use this to triangulate loops with holes.
    if (loop_cnt <= 1) return -1;

    point2d_t *verts2d = (point2d_t *)bu_calloc(on2dpts.Count(), sizeof(point2d_t), "bot verts");
    for (int i = 0; i < on2dpts.Count(); i++) {
	V2MOVE(verts2d[i], on2dpts[i]);
    }

    // Outer loop first
    size_t outer_npts = loop_starts[1] - loop_starts[0];
    int *outer_pt_ind = (int *)bu_calloc(outer_npts, sizeof(int), "pt_ind");
    for (unsigned int i = 0; i < outer_npts; i++) { outer_pt_ind[i] = i; }
    ccw = bg_polygon_direction(outer_npts, verts2d, outer_pt_ind);
    if (ccw == 0) {
	//bu_log("outer loop is degenerate - skip\n");
	bu_free(verts2d, "free verts2d");
	bu_free(outer_pt_ind, "free verts2d");
	return 0;
    }
    if (ccw == BG_CW) {
	for (int i = outer_npts - 1; i >= 0; i--) {
	    int d = outer_npts - 1 - i;
	    V2MOVE(verts2d[d], on2dpts[i]);
	}
	ccw = bg_polygon_direction(outer_npts, verts2d, outer_pt_ind);
	if (ccw == BG_CW) bu_log("huh?? loop ccw after flip??\n");
	std::vector<int> vert_map;
	for (unsigned int i = 0; i < outer_npts; i++) vert_map.push_back(verts_map[i]);
	std::reverse(vert_map.begin(), vert_map.end());
	for (unsigned int i = 0; i < outer_npts; i++) verts_map[i] = vert_map[i];
	//bu_log("flip outer loop\n");
    }

    // Now, holes
    std::vector<int *> holes_arrays;
    std::vector<int> holes_npts;
    size_t nholes = loop_cnt - 1;
    for (int i = 1; i < loop_cnt; i++) {
	size_t array_start = loop_starts[i];
	size_t array_end = (i == loop_cnt - 1) ? on2dpts.Count() : loop_starts[i + 1];
	size_t nhole_pts = array_end - array_start;
	int *holes_array = (int *)bu_calloc(nhole_pts, sizeof(int), "hole array");
	for (unsigned int j = 0; j < nhole_pts; j++) { holes_array[j] = array_start + j; }
	ccw = bg_polygon_direction(nhole_pts, verts2d, holes_array);
	if (ccw == 0) {
	    //bu_log("inner loop is degenerate - skip\n");
	    bu_free(holes_array, "free holes array");
	    nholes--;
	    continue;
	}
	if (ccw == BG_CCW) {
	    for (unsigned int j = array_end - 1; j >= array_start; j--) {
		int d = array_end - 1 - j;
		V2MOVE(verts2d[d], on2dpts[j]);
	    }
	    std::vector<int> vert_map;
	    for (unsigned int j = array_start; j < array_end ; j++) vert_map.push_back(verts_map[j]);
	    std::reverse(vert_map.begin(), vert_map.end());
	    for (unsigned int j = 0; j <= array_end - array_start; j++) verts_map[array_start + j] = vert_map[j];

	    //bu_log("flip inner loop\n");
	}
	holes_arrays.push_back(holes_array);
	holes_npts.push_back(nhole_pts);
    }

    int **h_arrays = (int **)bu_calloc(holes_arrays.size(), sizeof(int *), "holes array");
    size_t *h_npts = (size_t *)bu_calloc(holes_npts.size(), sizeof(size_t), "hole size array");
    for (unsigned int i = 0; i < holes_arrays.size(); i++) {
	h_arrays[i] = holes_arrays[i];
	h_npts[i] = holes_npts[i];
    }

    bg_nested_polygon_triangulate(ffaces, &num_faces, NULL, NULL, outer_pt_ind, outer_npts, (const int **)h_arrays, h_npts, nholes, NULL, 0, (const point2d_t *)verts2d, on2dpts.Count(), TRI_EAR_CLIPPING);

    // fix up vertex indices
    for (int i = 0; i < num_faces*3; i++) {
	int old_ind = (*ffaces)[i];
	(*ffaces)[i] = verts_map[old_ind];
    }

    bu_free(verts2d, "free verts2d");

    return num_faces;
}


/* TODO -rename to planar_polygon_tri */
int
subbrep_polygon_tri(struct bu_vls *UNUSED(msgs), struct subbrep_island_data *data, int *loops, int loop_cnt, int *rev_status, int **ffaces)
{
    const ON_Brep *brep = data->brep;
    int num_faces;

    // Get ignored vertices reported from shoal building - this is where they'll matter
    std::set<int> ignored_verts;
    array_to_set(&ignored_verts, data->null_verts, data->null_vert_cnt);

    /* Sanity check */
    if (loop_cnt < 1 || !data || !loops || !ffaces) return 0;

    /* Only one loop is easier */
    if (loop_cnt == 1) {
	ON_2dPointArray on2dpts;
	const ON_BrepLoop *b_loop = &(brep->m_L[loops[0]]);
	int loop_dir = 0;

	/* We need to build a 2D vertex list for the triangulation, and as long
	 * as we make sure the vertex mapping accounts for flipped trims in 3D,
	 * the point indices returned for the 2D triangulation will correspond
	 * to the correct 3D points without any additional work.  In essence,
	 * we use the fact that the BRep gives us a ready-made 2D point
	 * parameterization to save some work.*/
	std::vector<int> vert_array;
	for (int ti = 0; ti < b_loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim *trim = &(brep->m_T[b_loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
	    int vert_ind;
	    if (trim->m_bRev3d) {
		vert_ind = edge->Vertex(0)->m_vertex_index;
	    } else {
		vert_ind = edge->Vertex(1)->m_vertex_index;
	    }
	    if (ignored_verts.find(vert_ind) == ignored_verts.end()) {
		const ON_Curve *trim_curve = trim->TrimCurveOf();
		ON_2dPoint cp = trim_curve->PointAt(trim_curve->Domain().Max());
		on2dpts.Append(cp);
		vert_array.push_back(vert_ind);
		//bu_log("%d vertex: %d\n", b_loop->m_loop_index, vert_ind);
	    } else {
		//bu_log("%d vertex %d ignored - need to check if loop dir changed\n", b_loop->m_loop_index, vert_ind);
		loop_dir = brep->LoopDirection(brep->m_L[b_loop->m_loop_index]);
	    }
	}

	// Degenerate - no triangulation
	if (vert_array.empty()) return 0;

	int *vert_map = (int *)bu_calloc(vert_array.size(), sizeof(int), "vertex map");
	for (unsigned int i = 0; i < vert_array.size(); i++) {
	    vert_map[i] = vert_array[i];
	}

	num_faces = triangulate_array(on2dpts, vert_map, ffaces, loop_dir, rev_status);

    } else {

	/* We've got multiple loops - more complex setup since we need to define a polygon
	 * with holes.  This shouldn't happen if we're restricting ourselves to non-self-intersecting
	 * islands. */
	ON_2dPointArray on2dpts;
	std::vector<int> vert_array;
	int *loop_starts = (int *)bu_calloc(loop_cnt, sizeof(int), "start of loop indices");
	// Ensure the outer loop is first
	std::vector<int> ordered_loops;
	int outer_loop = brep->m_L[loops[0]].Face()->OuterLoop()->m_loop_index;
	ordered_loops.push_back(outer_loop);
	for (int i = 0; i < loop_cnt; i++) {
	    if (brep->m_L[loops[i]].m_loop_index != outer_loop)
		ordered_loops.push_back(brep->m_L[loops[i]].m_loop_index);
	}
	for (int i = 0; i < loop_cnt; i++) {
	    const ON_BrepLoop *b_loop = &(brep->m_L[ordered_loops[i]]);
	    loop_starts[i] = on2dpts.Count();
	    for (int ti = 0; ti < b_loop->m_ti.Count(); ti++) {
		const ON_BrepTrim *trim = &(brep->m_T[b_loop->m_ti[ti]]);
		const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
		int vert_ind;
		if (trim->m_bRev3d) {
		    vert_ind = edge->Vertex(0)->m_vertex_index;
		} else {
		    vert_ind = edge->Vertex(1)->m_vertex_index;
		}
		if (ignored_verts.find(vert_ind) == ignored_verts.end()) {
		    const ON_Curve *trim_curve = trim->TrimCurveOf();
		    ON_2dPoint cp = trim_curve->PointAt(trim_curve->Domain().Max());
		    on2dpts.Append(cp);
		    vert_array.push_back(vert_ind);
		    //bu_log("%d vertex: %d\n", b_loop->m_loop_index, vert_ind);
		} else {
		    //bu_log("%d vertex %d ignored\n", b_loop->m_loop_index, vert_ind);
		}
	    }
	}

	// Degenerate - no triangulation
	if (vert_array.empty()) return 0;

	int *vert_map = (int *)bu_calloc(vert_array.size(), sizeof(int), "vertex map");
	for (unsigned int i = 0; i < vert_array.size(); i++) {
	    vert_map[i] = vert_array[i];
	}

	num_faces = triangulate_array_with_holes(on2dpts, vert_map, loop_cnt, loop_starts, ffaces, brep);
    }
    return num_faces;
}


// A shoal, unlike a planar face, may define it's bounding planar loop using
// data from multiple face loops.  For this situation we walk the edges, and if
// we hit a null vertex we need to use that vertex to find the next edge that
// is not null.  The opposite vertex is then checked - if it is also null, continue
// to the next non-null edge.  Otherwise, resume the loop building with that vertex.
//
// This will generate a loop using 3d points.  Next, project those points into the
// 2D implicit plane from the shoal. Check the new 2D loop to determine if it is
// counterclockwise - if not, reverse it.
int
shoal_polygon_tri(struct bu_vls *UNUSED(msgs), struct subbrep_shoal_data *data, int **ffaces)
{
    int num_faces;
    const ON_Brep *brep = data->i->brep;

    // polygon verts
    std::vector<int> polygon_verts;

    // Get the set of ignored edges
    std::set<int> ignored_edges;
    array_to_set(&ignored_edges, data->i->null_edges, data->i->null_edge_cnt);

    // Get the set of ignored verts
    std::set<int> ignored_verts;
    array_to_set(&ignored_verts, data->i->null_verts, data->i->null_vert_cnt);

    // Get the set of edges associated with the shoal's loops.
    std::set<int> faces;
    std::set<int> shoal_edges;
    std::set<int>::iterator se_it;
    for (int i = 0; i < data->shoal_loops_cnt; i++) {
	const ON_BrepLoop *loop = &(data->i->brep->m_L[data->shoal_loops[i]]);
	faces.insert(loop->Face()->m_face_index);
	for (int ti = 0; ti < loop->m_ti.Count(); ti++) {
	    const ON_BrepTrim *trim = &(brep->m_T[loop->m_ti[ti]]);
	    const ON_BrepEdge *edge = &(brep->m_E[trim->m_ei]);
	    if (ignored_edges.find(edge->m_edge_index) == ignored_edges.end()) {
		shoal_edges.insert(edge->m_edge_index);
	    }
	}
    }

    // Find a suitable seed vert
    int seed_vert = -1;
    const ON_BrepEdge *first_edge = NULL;
    for (se_it = shoal_edges.begin(); se_it != shoal_edges.end(); se_it++) {
	first_edge = &(brep->m_E[(int)(*se_it)]);
	if (ignored_verts.find(first_edge->Vertex(0)->m_vertex_index) == ignored_verts.end()) {
	    seed_vert = first_edge->Vertex(0)->m_vertex_index;
	    break;
	}
	if (ignored_verts.find(first_edge->Vertex(1)->m_vertex_index) == ignored_verts.end()) {
	    seed_vert = first_edge->Vertex(1)->m_vertex_index;
	    break;
	}
    }

    // If we have the degenerate case, there is no triangulation.
    if (seed_vert == -1) return 0;

    polygon_verts.push_back(seed_vert);

    // Walk the edges collecting non-ignored verts.
    int curr_vert = -1;
    int curr_edge = -1;
    int prev_edge = -1;
    //bu_log("first edge: %d\n", first_edge->m_edge_index);
    while (first_edge && curr_edge != first_edge->m_edge_index && curr_vert != seed_vert) {
	// Once we're past the first edge, initialize our terminating condition if not already set.
	if (curr_edge == -1) curr_edge = first_edge->m_edge_index;
	if (curr_vert == -1) curr_vert = seed_vert;

	for (int i = 0; i < brep->m_V[curr_vert].m_ei.Count(); i++) {
	    const ON_BrepEdge *e = &(brep->m_E[brep->m_V[curr_vert].m_ei[i]]);
	    int ce = e->m_edge_index;
	    //bu_log("considering : %d\n", ce);
	    if (ce != curr_edge && ignored_edges.find(ce) == ignored_edges.end()) {
		int has_face_in_shoal = 0;
		for (int j = 0; j < e->m_ti.Count(); j++) {
		    const ON_BrepTrim *trim = &(brep->m_T[e->m_ti[j]]);
		    const ON_BrepFace *f = trim->Face();
		    if (faces.find(f->m_face_index) != faces.end()) {
			has_face_in_shoal = 1;
			break;
		    } else {
			//bu_log("face %d not in shoal\n", f->m_face_index);
		    }
		}
		if (has_face_in_shoal) {
		    // Have a viable edge - find our new vert.
		    //bu_log("  passed: %d\n", ce);
		    int nv = (e->Vertex(0)->m_vertex_index == curr_vert) ? e->Vertex(1)->m_vertex_index : e->Vertex(0)->m_vertex_index;
		    if (ignored_verts.find(nv) == ignored_verts.end()) {
			//bu_log("  next vert: %d\n", curr_vert);
			polygon_verts.push_back(nv);
		    }
		    curr_vert = nv;
		    prev_edge = curr_edge;
		    curr_edge = ce;
		    break;
		    //bu_log("  next edge: %d\n", curr_edge);
		}
	    }
	}
	if (curr_edge == prev_edge) {
	    bu_log("infinite loop?? - couldn't find next edge, fatal error\n");
	    return 0;
	}
    }
    // Don't double count the start/end vertex
    polygon_verts.pop_back();

    std::vector<int>::iterator v_it;
    for(v_it = polygon_verts.begin(); v_it != polygon_verts.end(); v_it++) {
	//bu_log("shoal vert found: %d\n", (int)*v_it);
    }

    // Have the verts - now we need 2d coordinates in the implicit plane
    ON_3dPoint imp_origin;
    ON_3dVector imp_normal;
    ON_VMOVE(imp_origin, data->params->implicit_plane_origin);
    ON_VMOVE(imp_normal, data->params->implicit_plane_normal);
    ON_Plane p(imp_origin, imp_normal);
    ON_3dVector xaxis = p.Xaxis();
    ON_3dVector yaxis = p.Yaxis();
    //bu_log("implicit origin: %f, %f, %f\n", p.Origin().x, p.Origin().y, p.Origin().z);
    //bu_log("implicit normal: %f, %f, %f\n", p.Normal().x, p.Normal().y, p.Normal().z);
    ON_2dPointArray on2dpts;
    for(v_it = polygon_verts.begin(); v_it != polygon_verts.end(); v_it++) {
	const ON_BrepVertex *v = &(brep->m_V[(int)*v_it]);
	ON_3dVector v3d = v->Point() - p.Origin();
	double xcoord = ON_DotProduct(v3d, xaxis);
	double ycoord = ON_DotProduct(v3d, yaxis);
	on2dpts.Append(ON_2dPoint(xcoord, ycoord));
	//bu_log("x, y: %f, %f\n", xcoord, ycoord);
    }

    if (polygon_verts.empty()) return 0;

    int *vert_map = (int *)bu_calloc(polygon_verts.size(), sizeof(int), "vertex map");
    for (unsigned int i = 0; i < polygon_verts.size(); i++) {
	vert_map[i] = polygon_verts[i];
    }

    num_faces = triangulate_array(on2dpts, vert_map, ffaces, 0, NULL);


    return num_faces;
}


/*
 * To determine if a polyhedron is inside or outside, we do a ray-based
 * test.  We will assume that determining the positive/negative status of one
 * face is sufficient - i.e., we will not check for flipped normals on faces.
 *
 1.  Construct a ray with a point outside the bounding box and
 *     a point on one of the faces (try to make sure the face normal is not
 *     close to perpendicular relative to the constructed ray.) Intersect this
 *     ray with all triangles in the BoT (unless BoTs regularly appear that are
 *     *far* larger than expected here, it's not worth building acceleration
 *     structures for a one time, one ray conversion process.
 * 2.  Count the intersections, and determine whether to test for an angle
 *     between the face normal and the ray of >ON_PI or <ON_PI.
 * 3.  Do the test as determined by #2.
 */

// How close to paralle will we tolerate before moving to another corner?
#define NPOLY_DOTP_TOL 0.01

int
negative_polygon(struct bu_vls *UNUSED(msgs), struct csg_object_params *data)
{

    /* Get bounding box from the vertices */
    ON_BoundingBox vert_bbox;
    ON_MinMaxInit(&vert_bbox.m_min, &vert_bbox.m_max);
    for (int i = 0; i < data->csg_vert_cnt; i++) {
	ON_3dPoint p;
	ON_VMOVE(p, data->csg_verts[i]);
	vert_bbox.Set(p, true);
    }

    /* Get normal from first triangle in array - TODO - fiddle with this to get the correct normal... */
    ON_3dPoint tp1, tp2, tp3;
    ON_VMOVE(tp1, data->csg_verts[data->csg_faces[0]]);
    ON_VMOVE(tp2, data->csg_verts[data->csg_faces[1]]);
    ON_VMOVE(tp3, data->csg_verts[data->csg_faces[2]]);
    ON_3dVector v1 = tp2 - tp1;
    ON_3dVector v2 = tp3 - tp1;
    ON_3dVector triangle_normal = ON_CrossProduct(v1, v2);
    ON_3dPoint origin_pnt = (tp1 + tp2 + tp3) / 3;

    // Scale bounding box to make sure corners are away from the volume
    vert_bbox.m_min = vert_bbox.m_min * 1.1;
    vert_bbox.m_max = vert_bbox.m_max * 1.1;

    // Pick a ray direction
    ON_3dVector rdir;
    ON_3dPoint box_corners[8];
    vert_bbox.GetCorners(box_corners);
    int have_dir = 0;
    int corner = 0;
    double dotp;
    while (!have_dir && corner < 8) {
	rdir = box_corners[corner] - origin_pnt;
	dotp = ON_DotProduct(triangle_normal, rdir);
	(NEAR_ZERO(dotp, NPOLY_DOTP_TOL)) ? corner++ : have_dir = 1;
    }
    if (!have_dir) {
	bu_log("Error: NONE of the corners worked??\n");
	return 0;
    }
    point_t origin, dir;
    BN_VMOVE(origin, origin_pnt);
    BN_VMOVE(dir, rdir);

    // Test the ray against the triangle set
    int hit_cnt = 0;
    point_t p1, p2, p3, isect;
    ON_3dPointArray hit_pnts;
    for (int i = 0; i < data->csg_face_cnt; i++) {
	ON_3dPoint onp1, onp2, onp3, hit_pnt;
	VMOVE(p1, data->csg_verts[data->csg_faces[i*3+0]]);
	VMOVE(p2, data->csg_verts[data->csg_faces[i*3+1]]);
	VMOVE(p3, data->csg_verts[data->csg_faces[i*3+2]]);
	ON_VMOVE(onp1, p1);
	ON_VMOVE(onp2, p2);
	ON_VMOVE(onp3, p3);
	ON_Plane fplane(onp1, onp2, onp3);
	int is_hit = bg_isect_tri_ray(origin, dir, p1, p2, p3, &isect);
	ON_VMOVE(hit_pnt, isect);
	// Don't count the point on the ray origin
	if (hit_pnt.DistanceTo(origin_pnt) < 0.0001) is_hit = 0;
	if (is_hit) {
	    // No double-counting
	    for (int j = 0; j < hit_pnts.Count(); j++) {
		if (hit_pnts[j].DistanceTo(hit_pnt) < 0.001) is_hit = 0;
	    }
	    if (is_hit) {
		//bu_log("in hit_cnt%d.s sph %f %f %f 0.1\n", hit_pnts.Count()+1, isect[0], isect[1], isect[2]);
		hit_pnts.Append(hit_pnt);
	    }
	}
    }
    hit_cnt = hit_pnts.Count();
    //bu_log("hit count: %d\n", hit_cnt);
    //bu_log("dotp : %f\n", dotp);

    int io_state;
    // Final inside/outside determination
    if (hit_cnt % 2) {
	io_state = (dotp > 0) ? -1 : 1;
    } else {
	io_state = (dotp < 0) ? -1 : 1;
    }

    //bu_log("inside out state: %d\n", io_state);

    return io_state;
}


// Check for a hard-to-spot degenerate nucleus case that can crop up if we
// have an island with one child.
//
// Return 1 if degenerate, 0 otherwise
int
arbn_nucleus_degeneracy(struct subbrep_island_data *data, std::set<int> *planar_faces)
{
    const ON_Brep *brep = data->brep;
    if (BU_PTBL_LEN(data->island_children) == 1) {

	// Build the set of all arbn shoal planes
	ON_SimpleArray<ON_Plane> arbn_planes;
	for (size_t i = 0; i < BU_PTBL_LEN(data->island_children); i++) {
	    struct subbrep_shoal_data *d = (struct subbrep_shoal_data *)BU_PTBL_GET(data->island_children, i);
	    if (d->shoal_children) {
		for (size_t j = 0; j < BU_PTBL_LEN(d->shoal_children); j++) {
		    struct csg_object_params *c = (struct csg_object_params *)BU_PTBL_GET(d->shoal_children, j);
		    if (c->plane_cnt > 0) {
			for (size_t k = 0; k < c->plane_cnt; k++) {
			    ON_3dVector n;
			    n.x = c->planes[k][0];
			    n.y = c->planes[k][1];
			    n.z = c->planes[k][2];
			    ON_3dPoint o = n * c->planes[k][3];
			    ON_Plane ap(o, n);
			    arbn_planes.Append(ap);
			}
		    }
		}
	    }
	}
	if (arbn_planes.Count() > 0) {
	    // Check if the island planar faces are coplanar with one of the shoal
	    // arbn planes (if any).  If *all* the island planes are coplanar in
	    // that fashion, the nucleus is degenerate.
	    std::set<int> arbg_coplanar_faces;
	    std::set<int>::iterator p_it;
	    for (p_it = planar_faces->begin(); p_it != planar_faces->end(); p_it++) {
		const ON_BrepFace *face = &(brep->m_F[(int)*p_it]);
		ON_Plane p;
		face->SurfaceOf()->IsPlanar(&p, BREP_PLANAR_TOL);
		if (face->m_bRev) p.Flip();
		// Check for face coplanarity with shoal arbns.
		for (int i = 0; i < arbn_planes.Count(); i++) {
		    if (arbn_planes[i].Normal().IsParallelTo(p.Normal(), BREP_PLANAR_TOL)) {
			if (fabs(arbn_planes[i].DistanceTo(p.origin)) < BREP_PLANAR_TOL) {
			    arbg_coplanar_faces.insert(face->m_face_index);
			    //bu_log("found coplanar face: %d\n", face->m_face_index);
			    break;
			}
		    }
		}
	    }
	    if (arbg_coplanar_faces.size() == planar_faces->size()) return 1;
	}
    }

    return 0;
}


int
island_nucleus(struct bu_vls *msgs, struct subbrep_island_data *data)
{
    int negative_nucleus = 0;
    int convex_polyhedron = 1;
    int parallel_planes = 0;
    int curr_vert = 0;
    ON_Plane seed_plane;
    std::map<int, int> vert_map;
    std::vector<int> all_faces;
    std::set<int> all_used_verts;
    std::set<int>::iterator auv_it;

    const ON_Brep *brep = data->brep;
    std::set<int> island_loops;
    array_to_set(&island_loops, data->island_loops, data->island_loops_cnt);

    // Accumulate per face triangle information
    std::vector< int * > face_arrays;
    std::vector< int > face_cnts;

    // Collect the set of all planes for convexity testing - if convex, we'll use an arbn
    // instead of a triangle mesh
    ON_SimpleArray<ON_Plane> planes;

    // For this step, loops alone are not enough.  It's quite possible to have
    // an island containing a face with both inner and outer loops, so we need
    // the set of planar faces and for each face the set of loops in the
    // island.
    std::set<int> planar_faces;
    std::set<int>::iterator p_it;
    for (int i = 0; i < data->island_loops_cnt; i++) {
	const ON_BrepLoop *loop = &(brep->m_L[data->island_loops[i]]);
	const ON_BrepFace *face = loop->Face();
	surface_t surface_type = ((surface_t *)data->face_surface_types)[face->m_face_index];
	if (surface_type == SURFACE_PLANE) {
	    planar_faces.insert(face->m_face_index);
	}
    }

    // Start out by checking for a single shoal island with all planar faces
    // accounted for by the shoal - in that situation, no polyhedron nucleus is
    // needed.
    if (arbn_nucleus_degeneracy(data, &planar_faces)) goto degenerate;

    // First, deal with planar faces
    for (p_it = planar_faces.begin(); p_it != planar_faces.end(); p_it++) {
	int f_lcnt;
	int loop_rev = 0; // Check if loop flips when ignored verts are removed
	int *f_loops = NULL;
	std::set<int> active_loops;
	const ON_BrepFace *face = &(brep->m_F[(int)*p_it]);
	ON_Plane p;
	face->SurfaceOf()->IsPlanar(&p, BREP_PLANAR_TOL);
	//bu_log("face(%d): %f, %f, %f %f, %f, %f\n", face->m_face_index,  p.origin.x, p.origin.y, p.origin.z, p.Normal().x, p.Normal().y, p.Normal().z);

	// Determine if we have 1 or a set of loops.
	for (int i = 0; i < face->LoopCount(); i++) {
	    if (island_loops.find(face->m_li[i]) != island_loops.end()) active_loops.insert(face->m_li[i]);
	}
	set_to_array(&f_loops, &f_lcnt, &active_loops);
	int *faces;
	int face_cnt = subbrep_polygon_tri(msgs, data, f_loops, f_lcnt, &loop_rev, &faces);
	if (loop_rev) negative_nucleus++;
	// If the face is flipped, flip the triangles and the plane so their normals are correct
	if ((face->m_bRev && !loop_rev) || (!face->m_bRev && loop_rev)) {
	    //bu_log("face %d is reversed - flip the plane.\n", face->m_face_index);
	    for (int i = 0; i < face_cnt; i++) {
		int tmp = faces[i*3+1];
		faces[i*3+1] = faces[i*3+2];
		faces[i*3+2] = tmp;
	    }
	    p.Flip();
	}

	// One more possible flip - if this face comes to the island via a
	// fil connection, flip it.  Don't flip a face with multiple loops
	// active, since that face is not a mating face.
	std::set<int> fil;
	array_to_set(&fil, data->fil, data->fil_cnt);
	if (active_loops.size() == 1 && fil.find(face->m_face_index) != fil.end()) {
	    //bu_log("face %d came to us via inner loop - flip the plane.\n", face->m_face_index);
	    for (int i = 0; i < face_cnt; i++) {
		int tmp = faces[i*3+1];
		faces[i*3+1] = faces[i*3+2];
		faces[i*3+2] = tmp;
	    }
	    p.Flip();
	}

	// If the face is not degenerate in the nucleus, record it.
	if (face_cnt > 0) {
	    face_cnts.push_back(face_cnt);
	    face_arrays.push_back(faces);
	    planes.Append(p);
	}
    }

    // Second, deal with non-planar shoals.
    for (size_t i = 0; i < BU_PTBL_LEN(data->island_children); i++) {
	struct subbrep_shoal_data *d = (struct subbrep_shoal_data *)BU_PTBL_GET(data->island_children, i);
	if (d->params->have_implicit_plane) {
	    int *faces;
	    int face_cnt = shoal_polygon_tri(msgs, d, &faces);
	    // If the face is not degenerate in the nucleus, record it.
	    if (face_cnt > 0) {
		face_cnts.push_back(face_cnt);
		face_arrays.push_back(faces);
		ON_3dPoint o;
		ON_3dVector n;
		ON_VMOVE(o, d->params->implicit_plane_origin);
		ON_VMOVE(n, d->params->implicit_plane_normal);
		ON_Plane p(o, n);

		if (d->params->bool_op == '-') p.Flip();

		planes.Append(p);
	    }
	}
    }
    if (planes.Count() < 4) goto degenerate;

    // Third, generate compact vertex list for csg params and create the final
    // vertex array for the w
    for (unsigned int i = 0; i < face_arrays.size(); i++) {
	int *fa = face_arrays[i];
	int fc = face_cnts[i];
	for (int j = 0; j < fc*3; j++) all_used_verts.insert(fa[j]);
	for (int j = 0; j < fc*3; j++) all_faces.push_back(fa[j]);
    }

    // Allocate and initialize nucleus
    BU_GET(data->nucleus, struct subbrep_shoal_data);
    subbrep_shoal_init(data->nucleus, data);
    data->nucleus->params->csg_id = (*(data->obj_cnt))++;
    data->nucleus->params->csg_type = PLANAR_VOLUME;
    data->nucleus->shoal_id = data->nucleus->params->csg_id;
    data->nucleus->shoal_type = PLANAR_VOLUME;

    // The nucleus shoal in this configuration will have the same
    // number of loops in play as the island itself.
    data->nucleus->shoal_loops_cnt = data->island_loops_cnt;
    data->nucleus->shoal_loops = (int *)bu_calloc(data->island_loops_cnt, sizeof(int), "shoal loops");
    for (int m = 0; m != data->island_loops_cnt; m++) {
	data->nucleus->shoal_loops[m] = data->island_loops[m];
    }

    // Assign the verts and faces to the csg parameters structure
    data->nucleus->params->csg_verts = (point_t *)bu_calloc(all_used_verts.size(), sizeof(point_t), "final verts array");
    data->nucleus->params->csg_vert_cnt = all_used_verts.size();
    data->nucleus->params->csg_faces = (int *)bu_calloc(all_faces.size(), sizeof(int), "final faces array");
    data->nucleus->params->csg_face_cnt = all_faces.size() / 3;
    curr_vert = 0;
    for (auv_it = all_used_verts.begin(); auv_it != all_used_verts.end(); auv_it++) {
	ON_3dPoint vp = brep->m_V[(int)(*auv_it)].Point();
	BN_VMOVE(data->nucleus->params->csg_verts[curr_vert], vp);
	vert_map.insert(std::pair<int, int>((int)*auv_it, curr_vert));
	curr_vert++;
    }
    for (unsigned int i = 0; i < all_faces.size(); i++) {
	data->nucleus->params->csg_faces[i] = vert_map.find(all_faces[i])->second;
    }


    // Check whether the polyhedron nucleus is degenerate
    seed_plane = planes[0];
    parallel_planes++;
    for (int i = 1; i < planes.Count(); i++) {
	if (planes[i].Normal().IsParallelTo(seed_plane.Normal(), VUNITIZE_TOL)) parallel_planes++;
    }
    if (parallel_planes == planes.Count()) goto degenerate;


    // Test for a negative polyhedron - if negative, handle accordingly
    if (negative_polygon(msgs, data->nucleus->params) == -1) {
	data->nucleus->params->negative = -1;
	data->nucleus->params->bool_op = '-';
	// Flip triangles and planes so the resulting BoT/arbn is a positive
	// shape.  All csg params need to describe positive shapes for
	// primitive creation - their "negative" flag preserves their original
	// role per their original B-Rep face normals
	for (int i = 0; i < data->nucleus->params->csg_face_cnt; i++) {
	    int tmp = data->nucleus->params->csg_faces[i*3+1];
	    data->nucleus->params->csg_faces[i*3+1] = data->nucleus->params->csg_faces[i*3+2];
	    data->nucleus->params->csg_faces[i*3+2] = tmp;
	}
	for (int i = 0; i < planes.Count(); i++) {
	    planes[i].Flip();
	}
    } else {
	data->nucleus->params->negative = 1;
	data->nucleus->params->bool_op = 'u';
    }


    // Test for a convex polyhedron. This can be an expensive test, so only do
    // it for a small number of planes
    if (planes.Count() < MAX_CNT_CONVEX_PLANE_TEST) {
	int *planes_used = (int *)bu_calloc(planes.Count(), sizeof(int), "usage flags");
	convex_plane_usage(&planes, &planes_used);
	for (int i = 0; i < planes.Count(); i++) {
	    if (planes_used[i] == 0) {
		convex_polyhedron = 0;
		break;
	    }
	}
	bu_free(planes_used, "free used array");
    } else {
	convex_polyhedron = 0;
    }

    // If the planes can form an arbn, we still need to make sure that none of the known vertices
    // from the planar faces are trimmed away by any of the arbn planes
    if (convex_polyhedron) {
	for (int v = 0; v < data->nucleus->params->csg_vert_cnt; v++) {
	    ON_3dPoint p3d;
	    ON_VMOVE(p3d, data->nucleus->params->csg_verts[v]);
	    /* See if point is outside arb */
	    for (int m = 0; m < planes.Count(); m++) {
		double dotp = ON_DotProduct(p3d - planes[m].origin, planes[m].Normal());
		if (dotp > 0 && !NEAR_ZERO(dotp, VUNITIZE_TOL)) {
		    convex_polyhedron = 0;
		    break;
		}
	    }
	    if (!convex_polyhedron) break;
	}
    }

    if (convex_polyhedron) {
	// If we do in fact have a convex polyhedron, we can create an arbn
	// instead of a BoT for this nucleus shape
	data->nucleus->params->csg_type = ARBN;
	data->nucleus->shoal_type = ARBN;
	data->nucleus->params->plane_cnt = planes.Count();
	data->nucleus->params->planes = (plane_t *)bu_calloc(planes.Count(), sizeof(plane_t), "planes");
	for (int i = 0; i < planes.Count(); i++) {
	    ON_Plane p = planes[i];
	    double d = p.DistanceTo(ON_3dPoint(0, 0, 0));
	    data->nucleus->params->planes[i][0] = p.Normal().x;
	    data->nucleus->params->planes[i][1] = p.Normal().y;
	    data->nucleus->params->planes[i][2] = p.Normal().z;
	    data->nucleus->params->planes[i][3] = -1 * d;
	}
    } else {
	// Otherwise, confirm as BoT
	data->nucleus->params->csg_type = PLANAR_VOLUME;
    }

    // There is one final wrinkle.  It is possible for a polyhedron nucleus to
    // be volumetrically inside a shoal (shape + arbn).  In this situation, the
    // nucleus is subtracted from the shoal shape.
    if (negative_nucleus > 0) {
	//bu_log("flip nucleus and child\n");
	if (BU_PTBL_LEN(data->island_children) != 1) {
	    bu_log("huh? flipped loops and child count != 1?\n");
	} else {
	    struct subbrep_shoal_data *tmp_n = data->nucleus;
	    struct subbrep_shoal_data *d = (struct subbrep_shoal_data *)BU_PTBL_GET(data->island_children, 0);
	    data->nucleus = d;
	    bu_ptbl_rm(data->island_children, (long *)d);
	    bu_ptbl_ins(data->island_children, (long *)tmp_n);
	}
    }

    return 1;

degenerate:
    // If the polyhedron nucleus is degenerate, one of the shoals is the nucleus.
    //
    // First, check whether the shoal negative/positive flags are the same.
    int shoal_same_status = 1;
    struct subbrep_shoal_data *cn = (struct subbrep_shoal_data *)BU_PTBL_GET(data->island_children, 0);
    for (size_t i = 1; i < BU_PTBL_LEN(data->island_children); i++) {
	struct subbrep_shoal_data *d = (struct subbrep_shoal_data *)BU_PTBL_GET(data->island_children, i);
	if (cn->params->negative != d->params->negative) shoal_same_status = 0;
    }
    if (shoal_same_status) {
	// If they all are the same, then any of them may serve as the
	// nucleus and whether the nucleus is negative be based on the
	// negative shape status of the shoal.  The island's children
	// will be unioned.
	//bu_log("shoal negative status is uniform\n");
	subbrep_shoal_free(data->nucleus);
	data->nucleus = cn;
	bu_ptbl_rm(data->island_children, (long *)cn);
    } else {
	// If we get here and we've got more than two shoals, we don't currently
	// handle it
	if (BU_PTBL_LEN(data->island_children) != 2) return 0;
	struct subbrep_shoal_data *s1 = (struct subbrep_shoal_data *)BU_PTBL_GET(data->island_children, 0);
	struct subbrep_shoal_data *s2 = (struct subbrep_shoal_data *)BU_PTBL_GET(data->island_children, 1);
	volume_t s1t = (volume_t)s1->params->csg_type;
	volume_t s2t = (volume_t)s2->params->csg_type;

	// If we've got shoals with different boolean status, then we have
	// to decide which shape is "inside" the other to make a call.  If
	// two cylinders the smaller radius is the new nucleus, for
	// example. cyl/cone is also possible - any others? sph/cyl with
	// sph as end cap will be degenerate...  Need to give this some
	// thought, but suspect a general solution isn't possible unless
	// based on raytracing.  For the time being, use type
	// specific rules.  Probably need to break this bit out into
	// separate functions/routines.

	if ((s1t == CYLINDER && s2t == CYLINDER)) {
	    double r1 = s1->params->radius;
	    double r2 = s2->params->radius;
	    struct subbrep_shoal_data *smaller = (r1 < r2) ? s1 : s2;
	    struct subbrep_shoal_data *larger = (r1 > r2) ? s1 : s2;
	    cn = (larger->params->half_cyl != 1) ? larger : smaller;
	    subbrep_shoal_free(data->nucleus);
	    data->nucleus = cn;
	    bu_ptbl_rm(data->island_children, (long *)cn);
	    return 1;
	}

	//TODO with sphere and cylinder/cone, might be able to check the normal of the
	//implicit face of the sphere against the normal of the cylinder's face that's
	//connected with the sphere...

	// Other option is to flag this island's shoals as all top-level
	// objects.  The subtractions and then the unions will be made
	// after all other csg logic has been built up.  Bad for locality
	// but may serve as a fall-back if nucleus resolution doesn't resolve...
	bu_log("\n\n\nshoals have different negative status - currently unhandled!\n\n\n");
    }


    return 1;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
