/*              T E S T _ B O T 2 N U R B S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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
/** @file test_bot2nurbs.cpp
 *
 * This file contains the logic that takes an input BoT, breaks that
 * BoT down into patches, fits surfaces to those patches, builds
 * trimming loops for the surfaces based on NURBS curves fitted to
 * patch edge segments, and assembles the result into a closed BREP.
 *
 */

/*  TODO - Can't keep shifting the fitting plane for a triangle patch
 *  every time a triangle is added - too computationally expensive.
 *  Instead, need to make sure that the starting plane used for the
 *  NURBS fit is the same plane being used for the triangle overlap
 *  testing.  Hopefully, that will preclude any "vertical or worse"
 *  surface fit issues while still allowing for a good surface fit,
 *  and avoid this business of triangles that overlap in one fit
 *  plane but not in another. Will probably need a change or two
 *  in the PCL surface fitting code.*/


#include "common.h"

#include <map>
#include <set>
#include <queue>
#include <list>
#include <vector>
#include <iostream>
#include <fstream>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "plot3.h"
#include "opennurbs.h"
#include "opennurbs_fit.h"
#include <Eigen/SVD>
#include "brep.h"

typedef std::pair<size_t, size_t> Edge;
typedef std::map< size_t, std::set<Edge> > VertToEdge;
typedef std::map< size_t, std::set<size_t> > VertToPatch;
typedef std::map< Edge, std::set<size_t> > EdgeToPatch;
typedef std::map< Edge, std::set<size_t> > EdgeToFace;
typedef std::map< size_t, size_t > FaceToPatch;
typedef std::map< size_t, size_t > PatchToPlane;
typedef std::set<Edge> EdgeList;
typedef std::set<size_t> FaceList;

struct Manifold_Info {
    ON_Brep *brep;
    ON_Brep *brep2;
    struct rt_bot_internal *bot;

    std::map< size_t, std::set<size_t> > patches;
    std::map< size_t, std::set<size_t> > patch_edges;
    std::map< size_t, std::vector<size_t> > polycurves;

    VertToEdge vert_to_edge;
    VertToPatch vert_to_patch;
    EdgeToFace edge_to_face;
    EdgeToPatch edge_to_patch;
    FaceToPatch face_to_patch;
    std::map< size_t, size_t > face_to_plane;
    PatchToPlane patch_to_plane;
    ON_3dVectorArray face_normals;
    ON_3dVectorArray vectors;
    std::map< std::pair<size_t, size_t>, fastf_t > norm_results;
    size_t patch_size_threshold;
    double face_plane_parallel_threshold;
    double neighbor_angle_threshold;
    int patch_cnt;
};


/**********************************************************************************
 *   Utility functions used throughout the process of fitting
 **********************************************************************************/

// Make an edge using consistent vertex ordering
static Edge
mk_edge(size_t pt_A, size_t pt_B)
{
    if (pt_A <= pt_B) {
	return std::make_pair(pt_A, pt_B);
    } else {
	return std::make_pair(pt_B, pt_A);
    }
}


// Calculate area of a face
static double
face_area(struct rt_bot_internal *bot, size_t face_num)
{
    point_t ptA, ptB, ptC;
    double a, b, c, p;
    double area;
    VMOVE(ptA, &bot->vertices[bot->faces[face_num*3+0]*3]);
    VMOVE(ptB, &bot->vertices[bot->faces[face_num*3+1]*3]);
    VMOVE(ptC, &bot->vertices[bot->faces[face_num*3+2]*3]);
    a = DIST_PT_PT(ptA, ptB);
    b = DIST_PT_PT(ptB, ptC);
    c = DIST_PT_PT(ptC, ptA);
    p = (a + b + c)/2;
    area = sqrt(p*(p-a)*(p-b)*(p-c));
    return area;
}


// Given a patch consisting of a set of faces find the
// set of edges that forms the outer edge of the patch
static void
find_edge_segments(std::set<size_t> *faces, EdgeList *patch_edges, struct Manifold_Info *info)
{
    size_t pt_A, pt_B, pt_C;
    std::set<size_t>::iterator it;
    std::map<std::pair<size_t, size_t>, size_t> edge_face_cnt;
    std::map<std::pair<size_t, size_t>, size_t>::iterator efc_it;
    patch_edges->clear();
    for (it = faces->begin(); it != faces->end(); it++) {
	pt_A = info->bot->faces[(*it)*3+0]*3;
	pt_B = info->bot->faces[(*it)*3+1]*3;
	pt_C = info->bot->faces[(*it)*3+2]*3;
	edge_face_cnt[mk_edge(pt_A, pt_B)] += 1;
	edge_face_cnt[mk_edge(pt_B, pt_C)] += 1;
	edge_face_cnt[mk_edge(pt_C, pt_A)] += 1;
    }

    for (efc_it = edge_face_cnt.begin(); efc_it!=edge_face_cnt.end(); efc_it++) {
	if ((*efc_it).second == 1) {
	    info->vert_to_edge[(*efc_it).first.first].insert((*efc_it).first);
	    info->vert_to_edge[(*efc_it).first.second].insert((*efc_it).first);
	    patch_edges->insert((*efc_it).first);
	}
    }
}


// Given a BoT face, return the three edges associated with that face
static void
get_face_edges(struct rt_bot_internal *bot, size_t face_num, std::set<Edge> *face_edges)
{
    size_t pt_A, pt_B, pt_C;
    pt_A = bot->faces[face_num*3+0]*3;
    pt_B = bot->faces[face_num*3+1]*3;
    pt_C = bot->faces[face_num*3+2]*3;
    face_edges->insert(mk_edge(pt_A, pt_B));
    face_edges->insert(mk_edge(pt_B, pt_C));
    face_edges->insert(mk_edge(pt_C, pt_A));
}


// Given a BoT face, return the three other faces that share an edge with that face
static void
get_connected_faces(struct rt_bot_internal *bot, size_t face_num, EdgeToFace *edge_to_face, std::set<size_t> *faces)
{
    std::set<Edge> face_edges;
    std::set<Edge>::iterator face_edges_it;
    get_face_edges(bot, face_num, &face_edges);
    for (face_edges_it = face_edges.begin(); face_edges_it != face_edges.end(); face_edges_it++) {
	std::set<size_t> faces_from_edge = (*edge_to_face)[(*face_edges_it)];
	std::set<size_t>::iterator ffe_it;
	for (ffe_it = faces_from_edge.begin(); ffe_it != faces_from_edge.end() ; ffe_it++) {
	    if ((*ffe_it) != face_num) {
		faces->insert((*ffe_it));
	    }
	}
    }
}


// Use SVD algorithm from Soderkvist to fit a plane to vertex points
// http://www.math.ltu.se/~jove/courses/mam208/svd.pdf
static void
fit_plane(size_t UNUSED(patch_id), std::set<size_t> *faces, struct Manifold_Info *info, ON_Plane *plane)
{
    if (faces->size() > 0) {
	ON_3dPoint center(0.0, 0.0, 0.0);
	std::set<size_t> verts;
	std::set<size_t>::iterator f_it, v_it;
	for(f_it = faces->begin(); f_it != faces->end(); f_it++) {
	    verts.insert(info->bot->faces[(*f_it)*3+0]*3);
	    verts.insert(info->bot->faces[(*f_it)*3+1]*3);
	    verts.insert(info->bot->faces[(*f_it)*3+2]*3);
	}
	point_t pt;
	for(v_it = verts.begin(); v_it != verts.end(); v_it++) {
	    VMOVE(pt, &info->bot->vertices[*v_it]);
	    center.x += pt[0]/verts.size();
	    center.y += pt[1]/verts.size();
	    center.z += pt[2]/verts.size();
	}
        Eigen::MatrixXd A(3, verts.size());
	int vert_cnt = 0;
	for(v_it = verts.begin(); v_it != verts.end(); v_it++) {
	    VMOVE(pt, &info->bot->vertices[*v_it]);
	    A(0,vert_cnt) = pt[0] - center.x;
	    A(1,vert_cnt) = pt[1] - center.y;
	    A(2,vert_cnt) = pt[2] - center.z;
	    vert_cnt++;
	}

	Eigen::JacobiSVD<Eigen::MatrixXd> svd(A, Eigen::ComputeThinU);

	// 4.  Normal is in column 3 of U matrix
	ON_3dVector normal(svd.matrixU()(0,2), svd.matrixU()(1,2), svd.matrixU()(2,2));

	// 5.  Construct plane
	ON_Plane new_plane(center, normal);
        (*plane) = new_plane;
/* Uncomment to plot fitting planes */
#if 0
	struct bu_vls name;
	bu_vls_init(&name);
	bu_vls_printf(&name, "fit_plane_%d.pl", patch_id);
	FILE* plot_file = fopen(bu_vls_addr(&name), "w");
	int r = int(256*drand48() + 1.0);
	int g = int(256*drand48() + 1.0);
	int b = int(256*drand48() + 1.0);
        point_t pc;
	for(f_it = faces->begin(); f_it != faces->end(); f_it++) {
            point_t p1, p2, p3;
            VMOVE(p1, &info->bot->vertices[info->bot->faces[(*f_it)*3+0]*3]);
            VMOVE(p2, &info->bot->vertices[info->bot->faces[(*f_it)*3+1]*3]);
            VMOVE(p3, &info->bot->vertices[info->bot->faces[(*f_it)*3+2]*3]);
            VSUB2(p1, p1, pc);
            VSUB2(p2, p2, pc);
            VSUB2(p3, p3, pc);
	    pdv_3move(plot_file, p1);
	    pdv_3cont(plot_file, p2);
	    pdv_3move(plot_file, p1);
	    pdv_3cont(plot_file, p3);
	    pdv_3move(plot_file, p2);
	    pdv_3cont(plot_file, p3);
	}
	pl_color(plot_file, 255, 255, 255);
        point_t pn;
        VSET(pc, center.x,center.y,center.z);
	pdv_3move(plot_file, pc);
        VSET(pn, normal.x,normal.y,normal.z);
        VSCALE(pn, pn, 200);
        VADD2(pn,pn,pc);
	pdv_3cont(plot_file, pn);

	fclose(plot_file);
#endif
    }
}


// Check if a mesh is planar
static int
planar_patch_test(size_t patch_id, std::set<size_t> *faces, struct Manifold_Info *info, fastf_t tol)
{
    if (faces->size() == 1) return 1;
    if (faces->size() > 0) {
	std::set<size_t> verts;
	std::set<size_t>::iterator f_it, v_it;
        fastf_t max_dist = 0.0;
        ON_Plane best_fit_plane;
        fit_plane(patch_id, faces, info, &best_fit_plane);
	for(f_it = faces->begin(); f_it != faces->end(); f_it++) {
	    verts.insert(info->bot->faces[(*f_it)*3+0]*3);
	    verts.insert(info->bot->faces[(*f_it)*3+1]*3);
	    verts.insert(info->bot->faces[(*f_it)*3+2]*3);
	}
	for(v_it = verts.begin(); v_it != verts.end(); v_it++) {
	    point_t pt;
	    VMOVE(pt, &info->bot->vertices[*v_it]);
            fastf_t curr_dist = best_fit_plane.DistanceTo(ON_3dPoint(pt[0], pt[1], pt[2]));
	    if (curr_dist > max_dist) max_dist = curr_dist;
	}
        if (max_dist < tol) return 1;
        return 0;
    } else {
	return 0;
    }
}


/**********************************************************************************
 *   Debugging code for plotting structures commonly generated during fitting
 **********************************************************************************/

// To plot specific groups of curves in MGED, do the following:
// set glob_compat_mode 0
// set pl_list [glob 27_curve*.pl]
// foreach plfile $pl_list {overlay $plfile}
static void
plot_curve(struct rt_bot_internal *bot, std::vector<size_t> *pts, int r, int g, int b, FILE *c_plot)
{
    if (c_plot == NULL) {
	FILE* plot = fopen("plot_curve.pl", "w");
	pl_color(plot, int(256*drand48() + 1.0), int(256*drand48() + 1.0), int(256*drand48() + 1.0));
	std::vector<size_t>::iterator v_it;
	for (v_it = pts->begin(); v_it != pts->end()-1; v_it++) {
	    pdv_3move(plot, &bot->vertices[(*v_it)]);
	    pdv_3cont(plot, &bot->vertices[(*(v_it+1))]);
	}
	fclose(plot);
    } else {
	pl_color(c_plot, r, g, b);
	std::vector<size_t>::iterator v_it;
	for (v_it = pts->begin(); v_it != pts->end()-1; v_it++) {
	    pdv_3move(c_plot, &bot->vertices[(*v_it)]);
	    pdv_3cont(c_plot, &bot->vertices[(*(v_it+1))]);
	}
    }
}


static void
plot_face(point_t p1, point_t p2, point_t p3, int r, int g, int b, FILE *c_plot)
{
    pl_color(c_plot, r, g, b);
    pdv_3move(c_plot, p1);
    pdv_3cont(c_plot, p2);
    pdv_3move(c_plot, p1);
    pdv_3cont(c_plot, p3);
    pdv_3move(c_plot, p2);
    pdv_3cont(c_plot, p3);
}


static void
plot_faces(std::map< size_t, std::set<size_t> > *patches, struct Manifold_Info *info, const char *filename)
{
    std::map< size_t, std::set<size_t> >::iterator p_it;
    FILE* plot_file = fopen(filename, "w");
    for (p_it = patches->begin(); p_it != patches->end(); p_it++) {
	int r = int(256*drand48() + 1.0);
	int g = int(256*drand48() + 1.0);
	int b = int(256*drand48() + 1.0);
	std::set<size_t> *faces = &((*p_it).second);
	std::set<size_t>::iterator f_it;
	for (f_it = faces->begin(); f_it != faces->end(); f_it++) {
	    plot_face(&info->bot->vertices[info->bot->faces[(*f_it)*3+0]*3], &info->bot->vertices[info->bot->faces[(*f_it)*3+1]*3], &info->bot->vertices[info->bot->faces[(*f_it)*3+2]*3], r, g ,b, plot_file);
	}
    }
    fclose(plot_file);
}


static void
plot_patch_borders(std::map< size_t, std::set<size_t> > *patches, struct Manifold_Info *info, const char *filename)
{
    std::map< size_t, std::set<size_t> >::iterator p_it;
    FILE* edge_plot = fopen(filename, "w");
    pl_color(edge_plot, 255, 255, 255);
    for (p_it = patches->begin(); p_it != patches->end(); p_it++) {
	EdgeList patch_edges;
	EdgeList::iterator e_it;
	find_edge_segments(&((*p_it).second), &patch_edges, info);
	for (e_it = patch_edges.begin(); e_it != patch_edges.end(); e_it++) {
	    pdv_3move(edge_plot, &info->bot->vertices[(*e_it).first]);
	    pdv_3cont(edge_plot, &info->bot->vertices[(*e_it).second]);
	}
    }
    fclose(edge_plot);
}


static void
plot_ncurve(ON_Curve &curve, FILE *c_plot)
{
    double pt1[3], pt2[3];
    ON_2dPoint from, to;
    int plotres = 100;
    if (curve.IsLinear()) {
	int knotcnt = curve.SpanCount();
	double *knots = new double[knotcnt + 1];

	curve.GetSpanVector(knots);
	for (int i = 1; i <= knotcnt; i++) {
	    ON_3dPoint p = curve.PointAt(knots[i - 1]);
	    VMOVE(pt1, p);
	    p = curve.PointAt(knots[i]);
	    VMOVE(pt2, p);
	    pdv_3move(c_plot, pt1);
	    pdv_3cont(c_plot, pt2);
	}

    } else {
	ON_Interval dom = curve.Domain();
	// XXX todo: dynamically sample the curve
	for (int i = 1; i <= plotres; i++) {
	    ON_3dPoint p = curve.PointAt(dom.ParameterAt((double)(i - 1)
							 / (double)plotres));
	    VMOVE(pt1, p);
	    p = curve.PointAt(dom.ParameterAt((double) i / (double)plotres));
	    VMOVE(pt2, p);
	    pdv_3move(c_plot, pt1);
	    pdv_3cont(c_plot, pt2);
	}
    }
}


// plot loop
static void
plot_loop(std::vector<size_t> *edges, struct Manifold_Info *info, FILE *l_plot)
{
    std::vector<size_t>::iterator v_it;
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    for (v_it = edges->begin(); v_it != edges->end(); v_it++) {
	pl_color(l_plot, r, g, b);
        ON_BrepEdge& edge = info->brep->m_E[(int)(*v_it)];
	const ON_Curve* edge_curve = edge.EdgeCurveOf();
	ON_Interval dom = edge_curve->Domain();
	// XXX todo: dynamically sample the curve
	int plotres = 50;
	for (int i = 1; i <= plotres; i++) {
	    double pt1[3], pt2[3];
	    ON_3dPoint p = edge_curve->PointAt(dom.ParameterAt((double)(i - 1)
							       / (double)plotres));
	    VMOVE(pt1, p);
	    p = edge_curve->PointAt(dom.ParameterAt((double) i / (double)plotres));
	    VMOVE(pt2, p);
	    pdv_3move(l_plot, pt1);
	    pdv_3cont(l_plot, pt2);
	}
    }
}


/**********************************************************************************
 *   Code for partitioning a mesh into patches suitable for NURBS surface fitting
 **********************************************************************************/

// Clean out empty patches
static void
remove_empty_patches(struct Manifold_Info *info)
{
    for (int i = 0; i < (int)info->patches.size(); i++) {
	if (info->patches[i].size() == 0) {
	    info->patches.erase(i);
	}
    }
}


static void
sync_structure_maps(struct Manifold_Info *info)
{
    std::map< size_t, std::set<size_t> >::iterator p_it;
    info->edge_to_patch.clear();
    info->vert_to_patch.clear();
    info->face_to_patch.clear();
    for (p_it = info->patches.begin(); p_it != info->patches.end(); p_it++) {
	std::set<size_t>::iterator f_it;
	std::set<size_t> *faces = &((*p_it).second);
	for (f_it = faces->begin(); f_it != faces->end(); f_it++) {
	    info->face_to_patch[(*f_it)] = (*p_it).first;
	    std::set<Edge> face_edges;
	    std::set<Edge>::iterator face_edges_it;
	    get_face_edges(info->bot, (*f_it), &face_edges);
	    for (face_edges_it = face_edges.begin(); face_edges_it != face_edges.end(); face_edges_it++) {
		info->edge_to_patch[(*face_edges_it)].insert((*p_it).first);
		info->vert_to_patch[(*face_edges_it).first].insert((*p_it).first);
		info->vert_to_patch[(*face_edges_it).second].insert((*p_it).first);
	    }
	}
	info->patch_to_plane[(*p_it).first] = info->face_to_plane[*(faces->begin())];
    }
}


// For a given face, determine which patch shares the majority of its edges.  If no one patch
// has a majority, return -1
static int
find_major_patch(size_t face_num, struct Manifold_Info *info, size_t curr_patch, size_t patch_count)
{
    std::map<size_t, size_t> patch_cnt;
    std::map<size_t, size_t>::iterator patch_cnt_itr;
    std::set<size_t> connected_faces;
    std::set<size_t>::iterator cf_it;
    size_t max_patch = 0;
    size_t max_patch_edge_cnt = 0;
    get_connected_faces(info->bot, face_num, &(info->edge_to_face), &connected_faces);
    for (cf_it = connected_faces.begin(); cf_it != connected_faces.end() ; cf_it++) {
	patch_cnt[info->face_to_patch[*cf_it]] += 1;
    }
    for (patch_cnt_itr = patch_cnt.begin(); patch_cnt_itr != patch_cnt.end() ; patch_cnt_itr++) {
	if ((*patch_cnt_itr).second > max_patch_edge_cnt) {
	    max_patch_edge_cnt = (*patch_cnt_itr).second;
	    max_patch = (*patch_cnt_itr).first;
	}
    }
    if (max_patch_edge_cnt == 1) {
	if (patch_count > 3) {
	    return -1;
	} else {
	    // A patch with three or fewer triangles is considered a small patch - if one of the shared
	    // faces is a viable candidate, return it.
	    int candidate_patch = -1;
	    double candidate_patch_norm = 0;
	    for (cf_it = connected_faces.begin(); cf_it != connected_faces.end() ; cf_it++) {
		if (info->face_to_patch[*cf_it] != curr_patch) {
		    double current_norm = info->norm_results[std::make_pair(face_num, info->patch_to_plane[info->face_to_patch[(*cf_it)]])];
		    if (current_norm > candidate_patch_norm) {
			candidate_patch = info->face_to_patch[(*cf_it)];
			candidate_patch_norm = current_norm;
		    }
		}
	    }
	    return candidate_patch;
	}
    } else {
	return (int)max_patch;
    }
}


// Given a list of edges and the "current" patch, find triangles that share more edges with
// a different patch.  If the corresponding normals allow, move the triangles in question to
// the patch with which they share the most edges.  Return the number of triangles shifted.
// This won't fully "smooth" an edge in a given pass, but an iterative approach should converge
// to edges with triangles in their correct "major" patches
static size_t
shift_edge_triangles(std::map< size_t, std::set<size_t> > *patches, size_t curr_patch, struct Manifold_Info *info)
{
    std::set<size_t> patch_edgefaces;
    std::map<size_t, size_t> faces_to_shift;
    std::map<size_t, size_t>::iterator f_it;
    EdgeList edges;
    EdgeList::iterator e_it;
    std::set<size_t>::iterator sizet_it;

    // 0. Build edge set;
    find_edge_segments(&((*patches)[curr_patch]), &edges, info);

    // 1. build triangle set of edge triangles in patch.
    for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
	std::set<size_t> faces_from_edge = info->edge_to_face[(*e_it)];
	for (sizet_it = faces_from_edge.begin(); sizet_it != faces_from_edge.end() ; sizet_it++) {
	    if (info->face_to_patch[(*sizet_it)] == curr_patch) {
		patch_edgefaces.insert((*sizet_it));
	    }
	}

    }
    // 2. build triangle set of edge triangles with major patch other than current patch
    //    and a normal that permits movement to the major patch
    for (sizet_it = patch_edgefaces.begin(); sizet_it != patch_edgefaces.end() ; sizet_it++) {
	int major_patch = find_major_patch((*sizet_it), info, curr_patch, (*patches)[curr_patch].size());
	if (major_patch != (int)curr_patch && major_patch != -1) {
	    if (info->norm_results[std::make_pair((*sizet_it), info->patch_to_plane[major_patch])] >= 0) {
		faces_to_shift[(*sizet_it)] = major_patch;
	    }
	}
    }

    // 3. move triangles to their major patch.
    for (f_it = faces_to_shift.begin(); f_it != faces_to_shift.end(); f_it++) {
	(*patches)[info->face_to_patch[(*f_it).first]].erase((*f_it).first);
	(*patches)[(*f_it).second].insert((*f_it).first);
	info->face_to_patch[(*f_it).first] = (*f_it).second;
    }

    return faces_to_shift.size();
}


static void
construct_patches(std::set<size_t> *faces, std::map< size_t, std::set<size_t> > *patches, struct Manifold_Info *info)
{
    if (faces->size() > 0){
	while(faces->size() > 0) {
	    info->patch_cnt++;
	    size_t new_patch = info->patch_cnt;
	    std::queue<size_t> face_queue;
	    face_queue.push((*faces->begin()));
	    faces->erase(face_queue.front());
	    size_t start_face_num = face_queue.front();
	    info->patch_to_plane[new_patch] = info->face_to_plane[face_queue.front()];
	    while (!face_queue.empty()) {
		size_t face_num = face_queue.front();
		face_queue.pop();
		(*patches)[new_patch].insert(face_num);
		info->face_to_patch[face_num] = new_patch;
		std::set<size_t> connected_faces;
		std::set<size_t>::iterator cf_it;
		get_connected_faces(info->bot, face_num, &(info->edge_to_face), &connected_faces);
		for (cf_it = connected_faces.begin(); cf_it != connected_faces.end() ; cf_it++) {
		    if(faces->find((*cf_it)) != faces->end()) {
			if(ON_DotProduct( *(*info).face_normals.At((int)(start_face_num)),*(*info).face_normals.At((int)(*cf_it)) ) > 0.5) {
			    face_queue.push((*cf_it));
			    faces->erase((*cf_it));
			}
		    }
		}
	    }
	}
    }
}


static void
split_overlapping_patch(size_t face1, size_t face2, size_t orig_patch, std::map< size_t, std::set<size_t> > *patches, struct Manifold_Info *info)
{
    std::set<size_t> *faces = &((*patches)[orig_patch]);
    std::queue<size_t> face_queue_1;
    std::queue<size_t> face_queue_2;
    face_queue_1.push(*faces->find(face1));
    faces->erase(face_queue_1.front());
    info->patch_cnt++;
    size_t new_patch_1 = info->patch_cnt;
    size_t start_face_num_1 = face_queue_1.front();
    info->patch_to_plane[new_patch_1] = info->face_to_plane[face_queue_1.front()];
    face_queue_2.push(*faces->find(face2));
    faces->erase(face_queue_2.front());
    info->patch_cnt++;
    size_t new_patch_2 = info->patch_cnt;
    size_t start_face_num_2 = face_queue_2.front();
    info->patch_to_plane[new_patch_2] = info->face_to_plane[face_queue_2.front()];
    while (!face_queue_1.empty() || !face_queue_2.empty()) {
        if (!face_queue_1.empty()) {
	    size_t face_num_1 = face_queue_1.front();
	    face_queue_1.pop();
	    (*patches)[new_patch_1].insert(face_num_1);
	    info->face_to_patch[face_num_1] = new_patch_1;
	    std::set<size_t> connected_faces;
	    std::set<size_t>::iterator cf_it;
	    get_connected_faces(info->bot, face_num_1, &(info->edge_to_face), &connected_faces);
	    for (cf_it = connected_faces.begin(); cf_it != connected_faces.end() ; cf_it++) {
		if(faces->find((*cf_it)) != faces->end() && (*patches)[new_patch_2].find(*cf_it) == (*patches)[new_patch_2].end()) {
		    if(ON_DotProduct( *(*info).face_normals.At((int)(start_face_num_1)),*(*info).face_normals.At((int)(*cf_it)) ) > 0.5) {
			face_queue_1.push((*cf_it));
			faces->erase((*cf_it));
		    }
		}
	    }
	}
        if (!face_queue_2.empty()) {
	    size_t face_num_2 = face_queue_2.front();
	    face_queue_2.pop();
	    (*patches)[new_patch_2].insert(face_num_2);
	    info->face_to_patch[face_num_2] = new_patch_2;
	    std::set<size_t> connected_faces;
	    std::set<size_t>::iterator cf_it;
	    get_connected_faces(info->bot, face_num_2, &(info->edge_to_face), &connected_faces);
	    for (cf_it = connected_faces.begin(); cf_it != connected_faces.end(); cf_it++) {
		if(faces->find((*cf_it)) != faces->end() && (*patches)[new_patch_1].find(*cf_it) == (*patches)[new_patch_1].end()) {
		    if(ON_DotProduct( *(*info).face_normals.At(((int)start_face_num_2)),*(*info).face_normals.At((int)(*cf_it)) ) > 0.5) {
			face_queue_2.push((*cf_it));
			faces->erase((*cf_it));
		    }
		}
	    }
	}
    }
    if (faces->size() > 0) construct_patches(faces, patches, info);
}


// Given a patch, find triangles that overlap when projected into the patch domain, remove them
// from the current patch and set them up in their own patch.  Returns the number of faces moved
// from the current patch to their own patch.
//
// TODO - saw at least one case where triangle removal resulted one patch breaking into two
// topological patches.  Need to handle this, otherwise Bad Things will happen when trying
// to generate BREPs.
// Also - need to check how the nurbs fitting code is calculating their initial plane, and use
// that for this test rather than the original faces - it is projection into *THAT* plane that
// must be clean, and a projection clean in the "standard" direction may not be clean there.
static size_t
overlapping_edge_triangles(std::map< size_t, std::set<size_t> > *patches, struct Manifold_Info *info)
{
    size_t total_overlapping = 0;
    std::map< size_t, std::set<size_t> >::iterator p_it;
    for (p_it = patches->begin(); p_it != patches->end(); p_it++) {
	if ((*p_it).second.size() > 0) {
	    ON_Plane plane;
	    fit_plane((*p_it).first, &((*p_it).second), info, &plane);
	    ON_Xform xf;
	    xf.PlanarProjection(plane);
	    size_t overlap_cnt = 1;
	    size_t patch_overlapping = 0;
	    while (overlap_cnt != 0) {
		std::set<size_t> edge_triangles;
		EdgeList edges;
		find_edge_segments(&((*p_it).second), &edges, info);
		EdgeList::iterator e_it;
		std::set<size_t>::iterator sizet_it;
		// 1. build triangle set of edge triangles in patch.
		for (e_it = edges.begin(); e_it != edges.end(); e_it++) {
		    std::set<size_t> faces_from_edge = info->edge_to_face[(*e_it)];
		    for (sizet_it = faces_from_edge.begin(); sizet_it != faces_from_edge.end() ; sizet_it++) {
			if (info->face_to_patch[(*sizet_it)] == (*p_it).first) {
			    edge_triangles.insert((*sizet_it));
			}
		    }
		}
		// 2. Find an overlapping triangle, remove it to its own patch, fix the edges, continue
		overlap_cnt = 0;
		for (sizet_it = edge_triangles.begin(); sizet_it != edge_triangles.end() ; sizet_it++) {
		    point_t v0, v1, v2;
		    ON_3dPoint onpt_v0(&info->bot->vertices[info->bot->faces[(*sizet_it)*3+0]*3]);
		    ON_3dPoint onpt_v1(&info->bot->vertices[info->bot->faces[(*sizet_it)*3+1]*3]);
		    ON_3dPoint onpt_v2(&info->bot->vertices[info->bot->faces[(*sizet_it)*3+2]*3]);
		    onpt_v0.Transform(xf);
		    onpt_v1.Transform(xf);
		    onpt_v2.Transform(xf);
		    VSET(v0, onpt_v0.x, onpt_v0.y, onpt_v0.z);
		    VSET(v1, onpt_v1.x, onpt_v1.y, onpt_v1.z);
		    VSET(v2, onpt_v2.x, onpt_v2.y, onpt_v2.z);
		    edge_triangles.erase(*sizet_it);
		    std::set<size_t>::iterator ef_it2 = edge_triangles.begin();
		    for (ef_it2 = edge_triangles.begin(); ef_it2!=edge_triangles.end(); ef_it2++) {
			if ((*sizet_it) != (*ef_it2)) {
			    point_t u0, u1, u2;
			    ON_3dPoint onpt_u0(&info->bot->vertices[info->bot->faces[(*ef_it2)*3+0]*3]);
			    ON_3dPoint onpt_u1(&info->bot->vertices[info->bot->faces[(*ef_it2)*3+1]*3]);
			    ON_3dPoint onpt_u2(&info->bot->vertices[info->bot->faces[(*ef_it2)*3+2]*3]);
			    onpt_u0.Transform(xf);
			    onpt_u1.Transform(xf);
			    onpt_u2.Transform(xf);
			    VSET(u0, onpt_u0.x, onpt_u0.y, onpt_u0.z);
			    VSET(u1, onpt_u1.x, onpt_u1.y, onpt_u1.z);
			    VSET(u2, onpt_u2.x, onpt_u2.y, onpt_u2.z);
			    int overlap = bn_tri_tri_isect_coplanar(v0, v1, v2, u0, u1, u2, 1);
			    if (overlap) {
				std::cout << "Patch " << (*p_it).first << " overlap: (" << *sizet_it << "," << *ef_it2 << ")\n";
				split_overlapping_patch(*sizet_it, *ef_it2, (*p_it).first, patches, info);
#if 0
				info->patch_cnt++;
				size_t new_patch = info->patch_cnt;
				(*patches)[new_patch].insert(*sizet_it);
				(*patches)[(*p_it).first].erase(*sizet_it);
				info->patch_to_plane[new_patch] = info->face_to_plane[*sizet_it];
				info->face_to_patch[*sizet_it] = new_patch;
				find_edge_segments(&((*p_it).second), &edges, info);
#endif
				overlap_cnt++;
				patch_overlapping++;
				total_overlapping++;
			    }
			}
			if (overlap_cnt > 0) break;
		    }
		    if (overlap_cnt > 0) break;
		}
	    }
//	    verify_patch_integrity((*p_it).first, &((*p_it).second), patches, info);
	}
    }
    return total_overlapping;
}


static void
bot_partition(struct Manifold_Info *info)
{
    std::map< size_t, std::set<size_t> > face_groups;
    std::map< size_t, std::set<size_t> > patches;
    // Calculate face normals dot product with bounding rpp planes
    for (size_t i=0; i < info->bot->num_faces; ++i) {
	size_t pt_A, pt_B, pt_C;
	size_t result_max;
	double vdot = 0.0;
	double result = 0.0;
	vect_t a, b, norm_dir;
	// Add vert -> edge and edge -> face mappings to global map.
	pt_A = info->bot->faces[i*3+0]*3;
	pt_B = info->bot->faces[i*3+1]*3;
	pt_C = info->bot->faces[i*3+2]*3;
	info->edge_to_face[mk_edge(pt_A, pt_B)].insert(i);
	info->edge_to_face[mk_edge(pt_B, pt_C)].insert(i);
	info->edge_to_face[mk_edge(pt_C, pt_A)].insert(i);
	// Categorize face
	VSUB2(a, &info->bot->vertices[info->bot->faces[i*3+1]*3], &info->bot->vertices[info->bot->faces[i*3]*3]);
	VSUB2(b, &info->bot->vertices[info->bot->faces[i*3+2]*3], &info->bot->vertices[info->bot->faces[i*3]*3]);
	VCROSS(norm_dir, a, b);
	VUNITIZE(norm_dir);
	info->face_normals.Append(ON_3dVector(norm_dir[0], norm_dir[1], norm_dir[2]));
	(*info).face_normals.At((int)i)->Unitize();
	for (size_t j=0; j < (size_t)(*info).vectors.Count(); j++) {
	    vdot = ON_DotProduct(*(*info).vectors.At((int)j), *(*info).face_normals.At((int)i));
	    info->norm_results[std::make_pair(i, j)] = vdot;
	    if (vdot > result) {
		result_max = j;
		result = vdot;
	    }
	}
	face_groups[result_max].insert(i);
	info->face_to_plane[i]=result_max;
    }
    // Sort the groups by their face count - we want to start with the group containing
    // either the least or the most faces - try most.
    std::map< size_t, std::set<size_t> >::iterator fg_it;
    std::vector<size_t> ordered_vects((*info).vectors.Count());
    std::set<int> face_group_set;
    for (int i = 0; i < (*info).vectors.Count(); i++) {
	face_group_set.insert(i);
    }
    size_t array_pos = 0;
    while (!face_group_set.empty()) {
	size_t curr_max = 0;
	size_t curr_vect = 0;
	for (fg_it = face_groups.begin(); fg_it != face_groups.end(); fg_it++) {
	    if ((*fg_it).second.size() > curr_max) {
		if (face_group_set.find((*fg_it).first) != face_group_set.end()) {
		    curr_max = (*fg_it).second.size();
		    curr_vect = (*fg_it).first;
		}
	    }
	}
	ordered_vects.at(array_pos) = curr_vect;
	face_group_set.erase(curr_vect);
	array_pos++;
    }

    // All faces must belong to some patch - continue until all faces are processed
    for (int i = 0; i < 6; i++) {
	fg_it = face_groups.find(ordered_vects.at(i));
	std::set<size_t> *faces = &((*fg_it).second);
	while (!faces->empty()) {
	    info->patch_cnt++;
	    // Find largest remaining face in group
	    double face_size_criteria = 0.0;
	    size_t smallest_face = 0;
	    for (std::set<size_t>::iterator fgl_it = faces->begin(); fgl_it != faces->end(); fgl_it++) {
		double fa = face_area(info->bot, *fgl_it);
		if (fa > face_size_criteria) {
		    smallest_face = (*fgl_it);
		    face_size_criteria = fa;
		}
	    }
	    std::queue<size_t> face_queue;
	    FaceList::iterator f_it;
	    face_queue.push(smallest_face);
	    face_groups[info->face_to_plane[face_queue.front()]].erase(face_queue.front());
	    size_t current_plane = info->face_to_plane[face_queue.front()];
	    //size_t start_face_num = face_queue.front();
	    while (!face_queue.empty()) {
		size_t face_num = face_queue.front();
		face_queue.pop();
		patches[info->patch_cnt].insert(face_num);
		info->patch_to_plane[info->patch_cnt] = current_plane;
		info->face_to_patch[face_num] = info->patch_cnt;

		std::set<size_t> connected_faces;
		std::set<size_t>::iterator cf_it;
		get_connected_faces(info->bot, face_num, &(info->edge_to_face), &connected_faces);
		for (cf_it = connected_faces.begin(); cf_it != connected_faces.end() ; cf_it++) {
		    double curr_face_area = face_area(info->bot, (*cf_it));
		    if (curr_face_area < (face_size_criteria*10) && curr_face_area > (face_size_criteria*0.1)) {
			if (face_groups[info->face_to_plane[(*cf_it)]].find((*cf_it)) != face_groups[info->face_to_plane[(*cf_it)]].end()) {
			    if (info->face_to_plane[(*cf_it)] == current_plane) {
				//	if(ON_DotProduct( *(*info).face_normals.At((start_face_num)),*(*info).face_normals.At((*cf_it)) ) > 0.5) {
				// Large patches pose a problem for feature preservation - make an attempt to ensure "large"
				// patches are flat.
				if (patches[info->patch_cnt].size() > info->patch_size_threshold) {
				    vect_t origin;
				    size_t ok = 1;
				    VMOVE(origin, &info->bot->vertices[info->bot->faces[(face_num)*3]*3]);
				    ON_Plane plane(ON_3dPoint(origin[0], origin[1], origin[2]), *(*info).face_normals.At((int)(face_num)));
				    for (int pt = 0; pt < 3; pt++) {
					point_t cpt;
					VMOVE(cpt, &info->bot->vertices[info->bot->faces[((*cf_it))*3+pt]*3]);
					double dist_to_plane = plane.DistanceTo(ON_3dPoint(cpt[0], cpt[1], cpt[2]));
					//std::cout << "Distance[" << face_num << "," << (*cf_it) << "](" <<  pt << "): " << dist_to_plane << "\n";
					if (dist_to_plane > 0) {
					    double dist = DIST_PT_PT(origin, cpt);
					    double angle = atan(dist_to_plane/dist);
					    if (angle > info->neighbor_angle_threshold) ok = 0;
					}
				    }
				    if (ok) {
					face_queue.push((*cf_it));
					face_groups[info->face_to_plane[(*cf_it)]].erase((*cf_it));
				    }
				} else {
				    face_queue.push((*cf_it));
				    face_groups[info->face_to_plane[(*cf_it)]].erase((*cf_it));
				}
				//	}
			    }
			}
		    }
		}
	    }
	}
    }

    // Find "interior" patches and see if they can be merged with the parent patch


    // "Shave" sharp triangles off of the edges by shifting them from one patch to
    // another - doesn't avoid all sharp corners, but does produce some cleanup.

    std::map< size_t, std::set<size_t> >::iterator p_it;
    size_t shaved_cnt = 1;
    size_t edge_smoothing_count = 0;
    while (shaved_cnt != 0 && edge_smoothing_count <= 20) {
	shaved_cnt = 0;
	edge_smoothing_count++;
	for (p_it = patches.begin(); p_it != patches.end(); p_it++) {
	    shaved_cnt += shift_edge_triangles(&patches, (*p_it).first, info);
	}
	std::cout << "Triangle shifts in smoothing pass " << edge_smoothing_count << ": " << shaved_cnt << "\n";
    }

    // Identify triangles that overlap in the patches associated high-level planar projection, and
    // assign them to their own patches - they pose a problem for surface fitting if left in the
    // original overlapping patches.
    size_t overlapping_face_cnt = 0;
    overlapping_face_cnt = overlapping_edge_triangles(&patches, info);
    std::cout << "Found " << overlapping_face_cnt << " overlapping faces in projection\n";

    // now, populate &(info->patches) with the non-empty patches and fix *_to_patch (maybe make that
    // part of sync_structure_maps.
    std::ofstream patch_map;
    patch_map.open("patch_map.txt");
    for (p_it = patches.begin(); p_it != patches.end(); p_it++) {
	if (patches[(*p_it).first].size() > 0) {
            patch_map << (*p_it).first << " -> " << info->patches.size() << "\n";
	    info->patches[info->patches.size()].insert(patches[(*p_it).first].begin(), patches[(*p_it).first].end());
	}
    }
    patch_map.close();

    std::cout << "Patch count: " << info->patches.size() << "\n";
    plot_faces(&(info->patches), info, "patches.pl");
    plot_patch_borders(&(info->patches), info, "patch_borders.pl");

    // Now that the mesh is fully partitioned, build the maps needed for curve discovery
    sync_structure_maps(info);
}


/**********************************************************************************
 *   Code for moving from patches with edge segments to a structured topology
 *   of curve segments and loops.
 **********************************************************************************/
static void
find_edges(struct Manifold_Info *info)
{
    std::map< size_t, size_t> vert_to_m_V;  // Keep mapping between vertex index being used in Brep and BoT index
    std::map< size_t, std::set<size_t> >::iterator p_it;
    std::map< size_t, EdgeList > all_patch_edges;
    for (p_it = info->patches.begin(); p_it != info->patches.end(); p_it++) {
	if (!info->patches[(*p_it).first].empty()) {
	    find_edge_segments(&((*p_it).second), &(all_patch_edges[(*p_it).first]), info);
	}
    }
    FILE* pcurveplot = fopen("polycurves.pl", "w");
    FILE *nurbs_curves = fopen("nurbs_curves.pl", "w");
    FILE *curves_plot = fopen("curves.pl", "w");
    for (p_it = info->patches.begin(); p_it != info->patches.end(); p_it++) {
	if (!info->patches[(*p_it).first].empty()) {
	    EdgeList *patch_edges = &(all_patch_edges[(*p_it).first]);
	    while (!patch_edges->empty()) {
		// We know the current patch - find out what the other one is for
		// this edge.
		const Edge *first_edge = &(*(patch_edges->begin()));
		std::set<size_t> edge_patches = (*(info->edge_to_patch.find(*first_edge))).second;
		edge_patches.erase((*p_it).first);
		size_t other_patch_id = *(edge_patches.begin());
		// Now we know our patches - we need to start with the first line
		// segment, and build up a set of edges until one of the halting
		// criteria is met.
		std::set<Edge> polycurve_edges;
		std::set<Edge>::iterator pc_it;
		std::queue<Edge> edge_queue;
		edge_queue.push(*(patch_edges->begin()));
		all_patch_edges[(*p_it).first].erase(edge_queue.front());
		all_patch_edges[other_patch_id].erase(edge_queue.front());
		while (!edge_queue.empty()) {
		    // Add the first edge in the queue
		    Edge curr_edge = edge_queue.front();
		    edge_queue.pop();
		    polycurve_edges.insert(curr_edge);
		    //std::cout << "Current edge: (" << curr_edge.first << "," << curr_edge.second << ")\n";

		    // Using the current edge, identify candidate edges and evaluate them.
		    std::set<Edge> eset;
		    std::set<Edge>::iterator e_it;
		    // If a vertex is used by three patches, it is a stopping point for curve
		    // buildup.  Otherwise, consider its edges.
		    if (info->vert_to_patch[curr_edge.first].size() <= 2) {
			eset.insert(info->vert_to_edge[curr_edge.first].begin(), info->vert_to_edge[curr_edge.first].end());
		    }
		    if (info->vert_to_patch[curr_edge.second].size() <= 2) {
			eset.insert(info->vert_to_edge[curr_edge.second].begin(), info->vert_to_edge[curr_edge.second].end());
		    }
		    // We don't need to re-consider any edge we've already considered.
		    for (e_it = eset.begin(); e_it != eset.end(); e_it++) {
			if (polycurve_edges.find((*e_it)) != polycurve_edges.end()) {
			    eset.erase((*e_it));
			}
		    }
		    // For the new candidate edges, pull their patches and see if
		    // they match the current patches.  If they do, and the edge has not
		    // already been removed from one of the patches, this edge is part of
		    // the current polycurve.
		    for (e_it = eset.begin(); e_it != eset.end(); e_it++) {
			std::set<size_t> *ep = &(info->edge_to_patch[(*e_it)]);
			if (ep->find(other_patch_id) != ep->end() && ep->find((*p_it).first) != ep->end() && polycurve_edges.find(*e_it) == polycurve_edges.end()) {
			    edge_queue.push(*e_it);
			    all_patch_edges[(*p_it).first].erase(*e_it);
			    all_patch_edges[other_patch_id].erase(*e_it);
			}
		    }
		}
		// populate polycurve
		std::map<size_t, std::set<size_t> > vert_to_verts;
		std::map<size_t, std::set<size_t> >::iterator vv_it;
		for (pc_it = polycurve_edges.begin(); pc_it != polycurve_edges.end(); pc_it++) {
		    vert_to_verts[(*pc_it).first].insert((*pc_it).second);
		    vert_to_verts[(*pc_it).second].insert((*pc_it).first);
		}
		// Find start and end points, if any
		std::pair<size_t, size_t> start_end;
		start_end.first = INT_MAX;
		start_end.second = INT_MAX;
		for (vv_it = vert_to_verts.begin(); vv_it != vert_to_verts.end(); vv_it++) {
		    if ((*vv_it).second.size() == 1) {
			if (start_end.first == INT_MAX) {
			    start_end.first = (*vv_it).first;
			} else {
			    if (start_end.second == INT_MAX) {
				start_end.second = (*vv_it).first;
			    }
			}
		    }
		}
		// Get curve ID number
		size_t curve_id = info->polycurves.size();

		// If we have a loop, need different halting conditions for
		// curve assembly.
		size_t threshold = 0;
		if (start_end.first == INT_MAX && start_end.second == INT_MAX) {
		    threshold = 1;
		    start_end.first = *(*vert_to_verts.begin()).second.begin();
		    start_end.second = *(*vert_to_verts.begin()).second.begin();
		}

		// Using the starting point, build a vector with the points
		// in curve order.
		size_t next_pt = start_end.first;
		size_t start_end_cnt = 0;
		while (start_end_cnt <= threshold) {
		    if (next_pt == start_end.second) start_end_cnt++;
		    size_t old_pt = next_pt;
		    info->polycurves[curve_id].push_back(old_pt);
		    next_pt = *(vert_to_verts[old_pt].begin());
		    vert_to_verts[next_pt].erase(old_pt);
		}

		// Make m_V vertices out of start and end, m_C3 curve of fit, and BrepEdge using
		// start/end pts and curve.  When new edge is added, brep->m_E.Count() is the index
		// of the new edge. - get it with ON_BrepEdge* edge = brep.Edge(ei);  verts are m_vi[0] or m_vi[1],
		// or (possibly) edge.Vertex(0)
		int vs_id, ve_id;
		if (vert_to_m_V.find(start_end.first) == vert_to_m_V.end()) {
		    ON_BrepVertex& newV = info->brep->NewVertex(ON_3dPoint(&info->bot->vertices[start_end.first]), 0.0001);
		    vs_id = newV.m_vertex_index;
		    vert_to_m_V[start_end.first] = vs_id;
		} else {
		    vs_id = vert_to_m_V[start_end.first];
		}
		if (vert_to_m_V.find(start_end.second) == vert_to_m_V.end()) {
		    ON_BrepVertex& newV = info->brep->NewVertex(ON_3dPoint(&info->bot->vertices[start_end.second]), 0.0001);
		    ve_id = newV.m_vertex_index;
		    vert_to_m_V[start_end.second] = ve_id;
		} else {
		    ve_id = vert_to_m_V[start_end.second];
		}
		ON_3dPointArray curve_pnts;
		std::vector<size_t>::iterator v_it;
                // TODO - for curves from patches that had to have triangles removed, need to add
                // additional points - a loose fit runs the risk of overlapping NURBS curves in
                // projections.  May even need a different fit for those situations - worst case,
                // use linear edges instead of smooth approximations for all curves that initially failed the
                // overlapping triangles test.
		for (v_it = info->polycurves[curve_id].begin(); v_it != info->polycurves[curve_id].end(); v_it++) {
		    ON_3dPoint pt_3d(&info->bot->vertices[(*v_it)]);
		    curve_pnts.Append(pt_3d);
		}
		ON_NurbsCurve *curve_nurb;
                int c3i;
		if (curve_pnts.Count() == 2) {
		    c3i = info->brep->AddEdgeCurve(new ON_LineCurve(*(curve_pnts.First()), *(curve_pnts.Last())));
		} else {
		    curve_nurb = interpolateLocalCubicCurve(curve_pnts);
		    c3i = info->brep->AddEdgeCurve(curve_nurb);
		}
		ON_BrepVertex& StartV = info->brep->m_V[vs_id];
		StartV.m_tolerance = 1e-3;
		ON_BrepVertex& EndV= info->brep->m_V[ve_id];
		EndV.m_tolerance = 1e-3;
		ON_BrepEdge& edge = info->brep->NewEdge(StartV, EndV, c3i, NULL ,0);
		edge.m_tolerance = 1e-3;

		// Let the patches know they have a curve associated with them.
		info->patch_edges[(*p_it).first].insert(edge.m_edge_index);
		info->patch_edges[other_patch_id].insert(edge.m_edge_index);

		// Plot curve
		int r = int(256*drand48() + 1.0);
		int g = int(256*drand48() + 1.0);
		int b = int(256*drand48() + 1.0);
		plot_curve(info->bot, &(info->polycurves[curve_id]), r, g, b, pcurveplot);
		plot_curve(info->bot, &(info->polycurves[curve_id]), r, g, b, curves_plot);

		pl_color(nurbs_curves, r, g, b);
		double pt1[3], pt2[3];
		int plotres = 50;
		const ON_Curve* edge_curve = edge.EdgeCurveOf();
		ON_Interval dom = edge_curve->Domain();
		for (int i = 1; i <= 50; i++) {
		    ON_3dPoint p = edge_curve->PointAt(dom.ParameterAt((double)(i - 1)
								       / (double)plotres));
		    VMOVE(pt1, p);
		    p = edge_curve->PointAt(dom.ParameterAt((double) i / (double)plotres));
		    VMOVE(pt2, p);
		    pdv_3move(nurbs_curves, pt1);
		    pdv_3cont(nurbs_curves, pt2);
		    pdv_3move(curves_plot, pt1);
		    pdv_3cont(curves_plot, pt2);
		}

	    }
	}
    }
    fclose(curves_plot);
    fclose(nurbs_curves);
    fclose(pcurveplot);
}


static void
find_outer_loop(std::map<size_t, std::vector<size_t> > *loops, size_t *outer_loop, std::set<size_t> *inner_loops, struct Manifold_Info *info)
{
    // If we have more than one loop, we need to identify the outer loop.  OpenNURBS handles outer
    // and inner loops separately, so we need to know which loop is our outer surface boundary.
    if ((*loops).size() == 1) {
	*outer_loop = 0;
    } else {
	std::map<size_t, std::vector<size_t> >::iterator l_it;
	fastf_t diag_max = 0.0;
	for(l_it = loops->begin(); l_it != loops->end(); l_it++) {
	    double boxmin[3] = {0.0, 0.0, 0.0};
	    double boxmax[3] = {0.0, 0.0, 0.0};
	    std::vector<size_t>::iterator v_it;
	    for(v_it = (*l_it).second.begin(); v_it != (*l_it).second.end(); v_it++) {
		ON_BrepEdge& edge = info->brep->m_E[(int)(*v_it)];
		const ON_Curve* edge_curve = edge.EdgeCurveOf();
		edge_curve->GetBBox((double *)&boxmin, (double *)&boxmax, 1);
	    }
	    ON_3dPoint pmin(boxmin[0], boxmin[1], boxmin[2]);
	    ON_3dPoint pmax(boxmax[0], boxmax[1], boxmax[2]);
	    if (pmin.DistanceTo(pmax) > diag_max) {
		*outer_loop = (*l_it).first;
		diag_max = pmin.DistanceTo(pmax);
            }
	}
	for(l_it = loops->begin(); l_it != loops->end(); l_it++) {
	    if((*l_it).first != *outer_loop) {
		(*inner_loops).insert((*l_it).first);
	    }
	}
    }
}


// Do pullbacks to generate trimming curves in 2D, use them to create BrepTrim and BrepLoop structures for
// outer and inner trimming loops
static void
build_loop(size_t patch_id, size_t loop_index, ON_BrepLoop::TYPE loop_type, std::map<size_t, std::vector<size_t> > *loops, struct Manifold_Info *info)
{
    ON_BrepFace& face = info->brep->m_F[(int)patch_id];
    const ON_Surface *surface = face.SurfaceOf();
    ON_Interval xdom = surface->Domain(0);
    ON_Interval ydom = surface->Domain(1);
    // Start with outer loop
    ON_BrepLoop& loop = info->brep->NewLoop(loop_type, face);
    // build surface tree
    brlcad::SurfaceTree* st = new brlcad::SurfaceTree(&face, false);

    std::vector<size_t>::iterator loop_it;
    std::vector<size_t> *loop_edges = &((*loops)[loop_index]);
    int vert_prev = -1;
    //std::cout << "Patch " << patch_id << " loop edges: \n";
    bool trim_rev = false;
    for(loop_it = loop_edges->begin(); loop_it != loop_edges->end(); loop_it++) {
	size_t curr_edge = (*loop_it);
	// Will we need to flip the trim?
	ON_BrepEdge& edge = info->brep->m_E[(int)curr_edge];
	if(vert_prev != -1) {
	    if (vert_prev == edge.m_vi[0]) {
		trim_rev = false;
	    } else {
		trim_rev = true;
	    }
	}
	if(trim_rev) {
	    vert_prev = edge.m_vi[0];
	} else {
	    vert_prev = edge.m_vi[1];
	}
	//std::cout << "edge verts: " << edge.m_vi[0] << "," << edge.m_vi[1] << " flip=" << trim_rev << "\n";

	ON_2dPointArray curve_pnts_2d;
	const ON_Curve* edge_curve = edge.EdgeCurveOf();
	ON_Interval dom = edge_curve->Domain();

	ON_3dPoint pt_3d = edge_curve->PointAt(dom.ParameterAt(0));
	ON_2dPoint pt_2d;
	ON_2dPoint loop_anchor;
	ON_2dPoint pt_2d_prev;
        int prev_trim_rev = 0;
        int istart = 1;

	size_t pullback_failures;
	if(loop_it == loop_edges->begin()) {
	    pullback_failures = 0;
	    prev_trim_rev = trim_rev;
	    int found_first_pt = 0;
	    while(!found_first_pt && istart < 50) {
		pt_3d = edge_curve->PointAt(dom.ParameterAt((double)(istart-1)/(double)50));
		if(get_closest_point(pt_2d, &face, pt_3d, st)) {
		    if (xdom.Includes(pt_2d.x) && ydom.Includes(pt_2d.y)) {
			curve_pnts_2d.Append(pt_2d);
			loop_anchor = pt_2d;
			found_first_pt = 1;
		    } else {
			istart++;
                    }
		} else {
		    std::cout << "Pullback failure on first pt (" << patch_id << "," << curr_edge << "," << (double)(istart-1)/(double)50 << "): " << pt_3d.x << "," << pt_3d.y << "," << pt_3d.z << "\n";
		    pullback_failures++;
		    istart++;
		}
	    }
	} else {
	    curve_pnts_2d.Append(pt_2d_prev);
	}
	// XXX todo: dynamically sample the curve - must use consistent method for all sampling, else
	// surface may not contain points sought by curve
	if(!trim_rev) {
	    for (int i = istart; i < 50; i++) {
		pt_3d = edge_curve->PointAt(dom.ParameterAt((double)(i)/(double)50));
		if(get_closest_point(pt_2d, &face, pt_3d, st) && pt_2d != pt_2d_prev) {
		    if (xdom.Includes(pt_2d.x) && ydom.Includes(pt_2d.y)) {
			curve_pnts_2d.Append(pt_2d);
			pt_2d_prev = pt_2d;
		    }
		} else {
		    std::cout << "Pullback failure (" << patch_id <<  "," << curr_edge << "," << (double)(i)/(double)50 << "): " << pt_3d.x << "," << pt_3d.y << "," << pt_3d.z << "\n";
		    pullback_failures++;
		}
	    }
	} else {
	    for (int i = 50; i > istart; i--) {
		pt_3d = edge_curve->PointAt(dom.ParameterAt((double)(i)/(double)50));
		if(get_closest_point(pt_2d, &face, pt_3d, st) && pt_2d != pt_2d_prev) {
		    if (xdom.Includes(pt_2d.x) && ydom.Includes(pt_2d.y)) {
			curve_pnts_2d.Append(pt_2d);
			pt_2d_prev = pt_2d;
		    }
		} else {
		    std::cout << "Pullback failure (" << patch_id <<  "," << curr_edge << "," << (double)(i)/(double)50 << "): " << pt_3d.x << "," << pt_3d.y << "," << pt_3d.z << "\n";
		    pullback_failures++;
                }
	    }
	}
        // For final curve, doesn't matter what last pullback is - we MUST force the loop to close.
	if(loop_it+1 == loop_edges->end()) {
	    curve_pnts_2d.Append(loop_anchor);
	}
	//if(trim_rev) {curve_pnts_2d.Reverse();}
	ON_Curve *trim_curve = interpolateCurve(curve_pnts_2d);
	int c2i = info->brep->AddTrimCurve(trim_curve);
	ON_BrepTrim& trim = info->brep->NewTrim(edge, trim_rev, loop, c2i);
	trim.m_type = ON_BrepTrim::mated;
	trim.m_tolerance[0] = 1e-3;
	trim.m_tolerance[1] = 1e-3;
    }
    if (info->brep->LoopDirection(loop) != 1 && loop_type == ON_BrepLoop::outer) {
	info->brep->FlipLoop(loop);
    }
    if (info->brep->LoopDirection(loop) != -1 && loop_type == ON_BrepLoop::inner) {
	info->brep->FlipLoop(loop);
    }
    delete st;
}


static void
find_loops(struct Manifold_Info *info)
{
    std::map< size_t, std::set<size_t> >::iterator p_it;
    for (p_it = info->patch_edges.begin(); p_it != info->patch_edges.end(); p_it++) {
	std::map<size_t, std::vector<size_t> > loops;
	std::map<size_t, std::set<size_t> > vert_to_edges;
	struct bu_vls name;
	bu_vls_init(&name);
	bu_vls_printf(&name, "loops_patch_%d.pl", (int)(*p_it).first);
	FILE* ploopplot = fopen(bu_vls_addr(&name), "w");
	std::set<size_t> curr_edges = (*p_it).second;
	for (std::set<size_t>::iterator edge_it = curr_edges.begin(); edge_it != curr_edges.end(); edge_it++) {
	    ON_BrepEdge& edge = info->brep->m_E[(int)(*edge_it)];
	    if (edge.m_vi[0] == edge.m_vi[1]) {
		size_t curr_loop = loops.size();
		loops[curr_loop].push_back((*edge_it));
		plot_loop(&loops[curr_loop], info, ploopplot);
		curr_edges.erase(*edge_it);
	    } else {
		vert_to_edges[edge.m_vi[0]].insert(*edge_it);
		vert_to_edges[edge.m_vi[1]].insert(*edge_it);
	    }
	}

	while (curr_edges.size() > 0) {
	    size_t curr_loop = loops.size();
	    std::queue<size_t> edge_queue;
	    edge_queue.push(*(curr_edges.begin()));
	    curr_edges.erase(edge_queue.front());
	    int vert_to_match = info->brep->m_E[(int)edge_queue.front()].m_vi[0];
	    while (!edge_queue.empty()) {
		size_t curr_edge = edge_queue.front();
		edge_queue.pop();
		loops[curr_loop].push_back(curr_edge);
		ON_BrepEdge& edge = info->brep->m_E[(int)curr_edge];
                if(vert_to_match == edge.m_vi[0]) {
		    vert_to_match = edge.m_vi[1];
                } else {
		    vert_to_match = edge.m_vi[0];
                }
		// use vert_to_edges to assemble other curves
		std::set<size_t> candidate_edges;
		candidate_edges.insert(vert_to_edges[vert_to_match].begin(), vert_to_edges[vert_to_match].end());
		candidate_edges.erase(curr_edge);
		for (std::set<size_t>::iterator c_it = candidate_edges.begin(); c_it != candidate_edges.end(); c_it++) {
		    if (curr_edges.find(*c_it) != curr_edges.end()) {
			edge_queue.push(*c_it);
			curr_edges.erase(*c_it);
		    }
		}
	    }
#ifdef BOT_TO_NURBS_DEBUG
	    std::cout << "Patch " << (*p_it).first << " loop " << curr_loop << ": ";
	    for (size_t i = 0; i < loops[curr_loop].size(); i++) {
		std::cout << loops[curr_loop][i] << ",";
	    }
	    std::cout << "\n";
#endif
	    plot_loop(&loops[curr_loop], info, ploopplot);
	}
	fclose(ploopplot);
	// Now that we have loops, determine which is the outer loop
	size_t outer_loop;
        std::set<size_t> inner_loops;
	find_outer_loop(&loops, &outer_loop, &inner_loops, info);
	if(loops.size() > 1) {
	    std::cout << "Patch " << (*p_it).first << " outer loop: " << outer_loop << "\n";
	}

	build_loop((*p_it).first, outer_loop, ON_BrepLoop::outer, &loops, info);
        // If there are any inner loops, build them too
	for (std::set<size_t>::iterator il_it = inner_loops.begin(); il_it != inner_loops.end(); il_it++) {
	    build_loop((*p_it).first, (*il_it), ON_BrepLoop::inner, &loops, info);
	}
    }
}


/**********************************************************************************
 *              Code for fitted 3D NURBS surfaces to patches.
 **********************************************************************************/

static void
PatchToVector3d(struct rt_bot_internal *bot, size_t curr_patch, struct Manifold_Info *info, on_fit::vector_vec3d &data)
{
    std::set<size_t> *faces = &(info->patches[curr_patch]);
    std::set<size_t>::iterator f_it;
    std::set<size_t> verts;
    unsigned int i = 0;
    //ON_3dPointArray pnts;
    for (i = 0; i < bot->num_vertices; i++) {
	//printf("v(%d): %f %f %f\n", i, V3ARGS(&bot->vertices[3*i]));
    }
    for (f_it = faces->begin(); f_it != faces->end(); f_it++) {
	verts.insert(bot->faces[(*f_it)*3+0]);
	verts.insert(bot->faces[(*f_it)*3+1]);
	verts.insert(bot->faces[(*f_it)*3+2]);
    }
    for (f_it = verts.begin(); f_it != verts.end(); f_it++) {
	//printf("vert %d\n", (int)(*f_it));
	//printf("vert(%d): %f %f %f\n", (int)(*f_it), V3ARGS(&bot->vertices[(*f_it)*3]));
	data.push_back(ON_3dVector(V3ARGS(&bot->vertices[(*f_it)*3])));
        //pnts.Append(ON_3dPoint(V3ARGS(&bot->vertices[(*f_it)*3])));
    }

    // Points on the edges of surfaces make problems for our current 2D uv pullback routine. Uncomment
    // the following to try to force the surface edges away from the actual patch boundaries by adding
    // "extension" points to the fit.
#if 0
    ON_BoundingBox bbox;
    pnts.GetTightBoundingBox(bbox);
    fastf_t diagonal = bbox.Diagonal().Length();
    ON_Plane best_fit_plane;
    fit_plane(curr_patch, faces, info, &best_fit_plane);

    fastf_t greatest_distance_to_plane = 0.0;
    for (int j = 0; j < pnts.Count(); ++j) {
	fastf_t dist = best_fit_plane.DistanceTo(pnts[j]);
	if (dist > greatest_distance_to_plane) greatest_distance_to_plane = dist;
    }
    ON_3dVector move = best_fit_plane.Normal();
    double vdot = ON_DotProduct(move, *info->face_normals.At((int)*(info->patches[curr_patch].begin())));
    if (vdot > 0.05) move.Reverse();
    ON_3dPoint newcenter = best_fit_plane.Origin() + move * greatest_distance_to_plane;
    best_fit_plane.SetOrigin(newcenter);

    struct bu_vls filename;
    bu_vls_init(&filename);
    bu_vls_sprintf(&filename, "extensions_%d.pl", curr_patch);
    FILE* edge_plot = fopen(bu_vls_addr(&filename), "w");
    EdgeList patch_edges_extend;
    EdgeList::iterator e_it;
    point_t pt1, pt2;
    VSET(pt1, best_fit_plane.Origin().x, best_fit_plane.Origin().y, best_fit_plane.Origin().z)
	ON_Circle bounding_circle(best_fit_plane, diagonal);
    for (int t = 0; t < 10; t++) {
	data.push_back(ON_3dVector(bounding_circle.PointAt(t*36)));
	VSET(pt2, bounding_circle.PointAt(t*36).x, bounding_circle.PointAt(t*36).y, bounding_circle.PointAt(t*36).z);

	pdv_3move(edge_plot, pt1);
	pdv_3cont(edge_plot, pt2);
    }
    fclose(edge_plot);
#endif

    // Edges are important for patch merging - tighten them by adding more edge
    // points than just the vertex points. Use points from the 3D NURBS edge curves
    // instead of the bot edges to ensure the surface includes the volume needed
    // for curve pullback.
    std::set<size_t> *patch_edges = &(info->patch_edges[curr_patch]);
    std::set<size_t>::iterator pe_it;
    for (pe_it = patch_edges->begin(); pe_it != patch_edges->end(); pe_it++) {
	ON_BrepEdge& edge = info->brep->m_E[(int)(*pe_it)];
	const ON_Curve* edge_curve = edge.EdgeCurveOf();
	ON_Interval dom = edge_curve->Domain();
	// XXX todo: dynamically sample the curve
	ON_3dPoint pt_3d = edge_curve->PointAt(dom.ParameterAt(0));
	data.push_back(ON_3dVector(pt_3d));
	for (int ipts = 1; ipts < 50; ipts++) {
	    pt_3d = edge_curve->PointAt(dom.ParameterAt((double)(ipts)/(double)50));
	    data.push_back(ON_3dVector(pt_3d));
	}
	pt_3d = edge_curve->PointAt(dom.ParameterAt(1.0));
	data.push_back(ON_3dVector(pt_3d));
    }
}


// Actually fit the NURBS surfaces and build faces
static void
find_surfaces(struct Manifold_Info *info)
{
    std::map< size_t, std::set<size_t> >::iterator p_it;
    for (p_it = info->patches.begin(); p_it != info->patches.end(); p_it++) {
	std::cout << "Found patch " << (*p_it).first << " with " << (*p_it).second.size() << " faces\n";
	unsigned order(3);
	on_fit::NurbsDataSurface data;
	PatchToVector3d(info->bot, (*p_it).first, info, data.interior);
	ON_NurbsSurface nurbs = on_fit::FittingSurface::initNurbsPCABoundingBox(order, &data);
	on_fit::FittingSurface fit(&data, nurbs);
	on_fit::FittingSurface::Parameter params;
	params.interior_smoothness = 0.15;
	params.interior_weight = 1.0;
	params.boundary_smoothness = 0.15;
	params.boundary_weight = 0.0;
	// NURBS refinement
	for (unsigned i = 0; i < 5; i++) {
	    fit.refine(0);
	    fit.refine(1);
	}

	// fitting iterations
	for (unsigned i = 0; i < 2; i++) {
	    fit.assemble(params);
	    fit.solve();
	}
        ON_BrepFace *face2 = info->brep2->NewFace(fit.m_nurbs);
        info->brep2->m_S.Append(new ON_NurbsSurface(fit.m_nurbs));
        int si = info->brep->AddSurface(new ON_NurbsSurface(fit.m_nurbs));
        ON_BrepFace &face = info->brep->NewFace(si);
        // Check face normal against plane
	ON_Interval udom = fit.m_nurbs.Domain(0);
	ON_Interval vdom = fit.m_nurbs.Domain(1);
        ON_3dVector surf_norm = fit.m_nurbs.NormalAt(udom.ParameterAt(0.5), vdom.ParameterAt(0.5));
	double vdot = ON_DotProduct(surf_norm, *info->face_normals.At((int)*(info->patches[(*p_it).first].begin())));
	if (vdot < 0) {
            info->brep->FlipFace(face);
            info->brep2->FlipFace(*face2);
	}
    }
}


int
main(int argc, char *argv[])
{
    struct db_i *dbip;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot_ip = NULL;
    struct rt_wdb *wdbp;
    struct bu_vls name;
    struct bu_vls bname;

    FaceList::iterator f_it;

    Manifold_Info info;
    info.vectors.Append(ON_3dVector(-1,0,0));
    info.vectors.Append(ON_3dVector(0,-1,0));
    info.vectors.Append(ON_3dVector(0,0,-1));
    info.vectors.Append(ON_3dVector(1,0,0));
    info.vectors.Append(ON_3dVector(0,1,0));
    info.vectors.Append(ON_3dVector(0,0,1));
    // Smaller values here will do better at preserving detail but will increase patch count.
    info.patch_size_threshold = 100;
    // Smaller values mean more detail preservation, but result in higher patch counts.
    info.neighbor_angle_threshold = 0.05;
    // A "normal" value for the "parallel to projection plane" face check should be around 0.55
    info.face_plane_parallel_threshold = 0.55;
    // Initialize the patch count variable
    info.patch_cnt = -1;
    bu_vls_init(&name);

    if (argc != 3) {
	bu_exit(1, "Usage: %s file.g object", argv[0]);
    }

    dbip = db_open(argv[1], DB_OPEN_READWRITE);
    if (dbip == DBI_NULL) {
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);
    }

    if (db_dirbuild(dbip) < 0)
	bu_exit(1, "ERROR: Unable to read from %s\n", argv[1]);

    dp = db_lookup(dbip, argv[2], LOOKUP_QUIET);
    if (dp == RT_DIR_NULL) {
	bu_exit(1, "ERROR: Unable to look up object %s\n", argv[2]);
    }

    RT_DB_INTERNAL_INIT(&intern)
	if (rt_db_get_internal(&intern, dp, dbip, NULL, &rt_uniresource) < 0) {
	    bu_exit(1, "ERROR: Unable to get internal representation of %s\n", argv[2]);
	}

    if (intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_exit(1, "ERROR: object %s does not appear to be of type BoT\n", argv[2]);
    } else {
	bot_ip = (struct rt_bot_internal *)intern.idb_ptr;
    }
    RT_BOT_CK_MAGIC(bot_ip);

    info.bot = bot_ip;

    // Do the initial partitioning of the mesh into patches
    bu_log("Performing initial partitioning of %s\n", dp->d_namep);
    bot_partition(&info);

    // Create the Brep data structure to hold curves and topology information
    info.brep = ON_Brep::New();
    info.brep2 = ON_Brep::New();


    // Now, using the patch sets, construct brep edges
    find_edges(&info);

    find_surfaces(&info);

    // Build the loops
    find_loops(&info);

    // Generate BRL-CAD brep object
    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    bu_vls_init(&bname);
    bu_vls_sprintf(&bname, "%s_brep", argv[2]);
    if (mk_brep(wdbp, bu_vls_addr(&bname), info.brep) == 0) {
	bu_log("Generated brep object %s\n", bu_vls_addr(&bname));
    }

    // For debugging, generate individual face BReps
    for (int fc = 0; fc < info.brep->m_F.Count(); fc++) {
	ON_Brep *brep_face = info.brep->DuplicateFace(fc, false);
	bu_vls_sprintf(&bname, "%s_face_%d", argv[2], fc);
	if (mk_brep(wdbp, bu_vls_addr(&bname), brep_face) == 0) {
	    bu_log("Generated brep object %s\n", bu_vls_addr(&bname));
	}
    }


//#ifdef BOT_TO_NURBS_DEBUG
    for (std::map< size_t, std::set<size_t> >::iterator np_it = info.patches.begin(); np_it != info.patches.end(); np_it++) {
	struct bu_vls pname;
	bu_vls_init(&pname);
	bu_vls_printf(&pname, "patch_%d.pl", (int)(*np_it).first);
	FILE* plot_file = fopen(bu_vls_addr(&pname), "w");
	int r = int(256*drand48() + 1.0);
	int g = int(256*drand48() + 1.0);
	int b = int(256*drand48() + 1.0);
	std::set<size_t> *nfaces = &((*np_it).second);
	std::set<size_t>::iterator nf_it;
	for (nf_it = nfaces->begin(); nf_it != nfaces->end(); nf_it++) {
	    plot_face(&bot_ip->vertices[bot_ip->faces[(*nf_it)*3+0]*3], &bot_ip->vertices[bot_ip->faces[(*nf_it)*3+1]*3], &bot_ip->vertices[bot_ip->faces[(*nf_it)*3+2]*3], r, g ,b, plot_file);
	}
	fclose(plot_file);
    }
//#endif

#if 0
    FILE* curve_plot = fopen("curve_plot.pl", "w");
    pl_color(curve_plot, 255, 255, 255);
    std::set<std::pair<size_t, size_t> > patch_interactions;
    std::set<std::pair<size_t, size_t> >::iterator o_it;
    for (p_it = info.patches.begin(); p_it != info.patches.end(); p_it++) {
	EdgeList::iterator e_it;
	for (e_it = info.patch_edges[(*p_it).first].begin(); e_it != info.patch_edges[(*p_it).first].end(); e_it++) {
	    std::set<size_t> ep = info.edge_to_patch[(*e_it)];
	    ep.erase((*p_it).first);
	    if ((*p_it).first < (*ep.begin())) {
		patch_interactions.insert(std::make_pair((*p_it).first, (*ep.begin())));
	    } else {
		patch_interactions.insert(std::make_pair((*ep.begin()), (*p_it).first));
	    }
	}
    }
    std::cout << "Patch interaction count: " << patch_interactions.size() << "\n";
    for (o_it = patch_interactions.begin(); o_it != patch_interactions.end(); o_it++) {
	std::cout << "Intersecting " << (*o_it).first  << " with " << (*o_it).second << "\n";
	ON_SimpleArray<ON_NurbsCurve*> intersect3d;
	ON_SimpleArray<ON_NurbsCurve*> intersect_uv2d;
	ON_SimpleArray<ON_NurbsCurve*> intersect_st2d;
	brlcad::surface_surface_intersection((*(info.surface_array.At(info.patch_to_surface[(*o_it).first]))), (*(info.surface_array.At(info.patch_to_surface[(*o_it).second]))), intersect3d, intersect_uv2d, intersect_st2d);
	for (int k = 0; k < intersect3d.Count(); k++) {
	    plot_ncurve(*(intersect3d[k]),curve_plot);
	    delete intersect3d[k];
	}

    }
    fclose(curve_plot);
#endif
    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
