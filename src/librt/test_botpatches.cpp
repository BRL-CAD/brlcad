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
#include "nurbs.h"

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
    std::map< size_t, std::set<size_t> > patches;
    std::map< size_t, EdgeList > patch_edges;
    std::map< size_t, std::vector<size_t> > polycurves;
    std::map< size_t, std::vector<size_t> > loops;
    std::map< size_t, std::set<size_t> > patch_polycurves;
    std::map< size_t, std::set<size_t> > polycurves_patch;
    std::map< size_t, size_t> outer_loops;
    std::map< size_t , std::set<size_t> > inner_loops;
    ON_SimpleArray<ON_NurbsSurface*> surface_array;
    std::map< size_t, size_t> patch_to_surface;
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
 *
 *   Debugging code for plotting structures commonly generated during fitting
 *
 **********************************************************************************/

// To plot specific groups of curves in MGED, do the following:
// set glob_compat_mode 0
// set pl_list [glob 27_curve*.pl]
// foreach plfile $pl_list {overlay $plfile}
void plot_curve(struct rt_bot_internal *bot, std::vector<size_t> *pts, int r, int g, int b, FILE *c_plot)
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

// plot loop
void plot_loop(struct rt_bot_internal *bot, size_t loop_id, struct Manifold_Info *info, FILE *l_plot)
{
    std::vector<size_t>::iterator v_it;
    int r = int(256*drand48() + 1.0);
    int g = int(256*drand48() + 1.0);
    int b = int(256*drand48() + 1.0);
    for (v_it = info->loops[loop_id].begin(); v_it != info->loops[loop_id].end(); v_it++) {
        plot_curve(bot, &(info->polycurves[*v_it]), r, g, b, l_plot);
    }
}

void plot_face(point_t p1, point_t p2, point_t p3, int r, int g, int b, FILE *c_plot)
{
    pl_color(c_plot, r, g, b);
    pdv_3move(c_plot, p1);
    pdv_3cont(c_plot, p2);
    pdv_3move(c_plot, p1);
    pdv_3cont(c_plot, p3);
    pdv_3move(c_plot, p2);
    pdv_3cont(c_plot, p3);
}

void plot_faces(struct rt_bot_internal *bot_ip, struct Manifold_Info *info, const char *filename) {
    std::map< size_t, std::set<size_t> >::iterator p_it;
    FILE* plot_file = fopen(filename, "w");
    for (p_it = info->patches.begin(); p_it != info->patches.end(); p_it++) {
	int r = int(256*drand48() + 1.0);
	int g = int(256*drand48() + 1.0);
	int b = int(256*drand48() + 1.0);
        std::set<size_t> *faces = &((*p_it).second);
        std::set<size_t>::iterator f_it;
	for (f_it = faces->begin(); f_it != faces->end(); f_it++) {
	    plot_face(&bot_ip->vertices[bot_ip->faces[(*f_it)*3+0]*3], &bot_ip->vertices[bot_ip->faces[(*f_it)*3+1]*3], &bot_ip->vertices[bot_ip->faces[(*f_it)*3+2]*3], r, g ,b, plot_file);
        }
    }
    fclose(plot_file);
}

void plot_patch_borders(struct rt_bot_internal *bot_ip, struct Manifold_Info *info, const char *filename) {
    std::map< size_t, std::set<size_t> >::iterator p_it;
    FILE* edge_plot = fopen(filename, "w");
    pl_color(edge_plot, 255, 255, 255);
    for (p_it = info->patches.begin(); p_it != info->patches.end(); p_it++) {
	EdgeList::iterator e_it;
	for (e_it = info->patch_edges[(*p_it).first].begin(); e_it != info->patch_edges[(*p_it).first].end(); e_it++) {
	    pdv_3move(edge_plot, &bot_ip->vertices[(*e_it).first]);
	    pdv_3cont(edge_plot, &bot_ip->vertices[(*e_it).second]);
	}
    }
    fclose(edge_plot);
}

void plot_ncurve(ON_Curve &curve, FILE *c_plot)
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
            ON_3dPoint p = curve.PointAt(dom.ParameterAt((double) (i - 1)
                                                         / (double)plotres));
            VMOVE(pt1, p);
            p = curve.PointAt(dom.ParameterAt((double) i / (double)plotres));
            VMOVE(pt2, p);
	    pdv_3move(c_plot, pt1);
	    pdv_3cont(c_plot, pt2);
        }
    }
}



/**********************************************************************************
 *
 *   Code for partitioning a mesh into patches suitable for NURBS surface fitting
 *
 **********************************************************************************/

// Clean out empty patches
void remove_empty_patches(struct Manifold_Info *info) {
    for (int i = 0; i < (int)info->patches.size(); i++) {
	if (info->patches[i].size() == 0) {
	    info->patches.erase(i);
	    info->patch_edges.erase(i);
	}
    }
}

// Make an edge using consistent vertex ordering
Edge mk_edge(size_t pt_A, size_t pt_B) {
    if (pt_A <= pt_B) {
	return std::make_pair(pt_A, pt_B);
    } else {
	return std::make_pair(pt_B, pt_A);
    }
}

void get_face_edges(struct rt_bot_internal *bot, size_t face_num, std::set<Edge> *face_edges) {
    size_t pt_A, pt_B, pt_C;
    pt_A = bot->faces[face_num*3+0]*3;
    pt_B = bot->faces[face_num*3+1]*3;
    pt_C = bot->faces[face_num*3+2]*3;
    face_edges->insert(mk_edge(pt_A, pt_B));
    face_edges->insert(mk_edge(pt_B, pt_C));
    face_edges->insert(mk_edge(pt_C, pt_A));
}

void get_connected_faces(struct rt_bot_internal *bot, size_t face_num, EdgeToFace *edge_to_face, std::set<size_t> *faces) {
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

// face_to_patch is built and maintained during the initial partitioning, but the
// edge and vertex mappings are not tracked during the intial process.  Call this
// function to populate them, after the initial partitioning is complete.
void sync_structure_maps(struct rt_bot_internal *bot, struct Manifold_Info *info) {
    std::map< size_t, std::set<size_t> >::iterator p_it;
    info->edge_to_patch.clear();
    info->vert_to_patch.clear();
    for (p_it = info->patches.begin(); p_it != info->patches.end(); p_it++) {
        std::set<size_t>::iterator f_it;
        std::set<size_t> *faces = &((*p_it).second);
	for (f_it = faces->begin(); f_it != faces->end(); f_it++) {
	    std::set<Edge> face_edges;
	    std::set<Edge>::iterator face_edges_it;
	    get_face_edges(bot, (*f_it), &face_edges);
	    for (face_edges_it = face_edges.begin(); face_edges_it != face_edges.end(); face_edges_it++) {
		info->edge_to_patch[(*face_edges_it)].insert((*p_it).first);
		info->vert_to_patch[(*face_edges_it).first].insert((*p_it).first);
		info->vert_to_patch[(*face_edges_it).second].insert((*p_it).first);
	    }
	}
    }
}

// For a given face, determine which patch shares the majority of its edges.  If no one patch
// has a majority, return -1
int find_major_patch(struct rt_bot_internal *bot, size_t face_num, struct Manifold_Info *info, size_t curr_patch, size_t patch_count) {
    std::map<size_t, size_t> patch_cnt;
    std::map<size_t, size_t>::iterator patch_cnt_itr;
    std::set<size_t> connected_faces;
    std::set<size_t>::iterator cf_it;
    size_t max_patch = 0;
    size_t max_patch_edge_cnt = 0;
    get_connected_faces(bot, face_num, &(info->edge_to_face), &connected_faces);
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
                    if(current_norm > candidate_patch_norm) {
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
size_t shift_edge_triangles(struct rt_bot_internal *bot, std::map< size_t, std::set<size_t> > *patches, size_t curr_patch, EdgeList *edges, struct Manifold_Info *info) {
    std::set<size_t> patch_edgefaces;
    std::map<size_t, size_t> faces_to_shift;
    std::map<size_t, size_t>::iterator f_it;
    EdgeList::iterator e_it;
    std::set<size_t>::iterator sizet_it;
    // 1. build triangle set of edge triangles in patch.
    for (e_it = edges->begin(); e_it != edges->end(); e_it++) {
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
        int major_patch = find_major_patch(bot, (*sizet_it), info, curr_patch, (*patches)[curr_patch].size());
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

// Calculate area of a face
double face_area (struct rt_bot_internal *bot, size_t face_num) {
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

void pnt_project(point_t orig_pt, point_t *new_pt, point_t normal) {
    point_t p1, norm_scale;
    fastf_t dotP;
    VMOVE(p1, orig_pt);
    dotP = VDOT(p1, normal);
    VSCALE(norm_scale, normal, dotP);
    VSUB2(*new_pt, p1, norm_scale);
}

// Given a patch consisting of a set of faces find the
// set of edges that forms the outer edge of the patch
void find_edge_segments(struct rt_bot_internal *bot, std::set<size_t> *faces, EdgeList *patch_edges, struct Manifold_Info *info)
{
    size_t pt_A, pt_B, pt_C;
    std::set<size_t>::iterator it;
    std::map<std::pair<size_t, size_t>, size_t> edge_face_cnt;
    std::map<std::pair<size_t, size_t>, size_t>::iterator efc_it;
    patch_edges->clear();
    for (it = faces->begin(); it != faces->end(); it++) {
	pt_A = bot->faces[(*it)*3+0]*3;
	pt_B = bot->faces[(*it)*3+1]*3;
	pt_C = bot->faces[(*it)*3+2]*3;
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

// Given a list of edges and the "current" patch, find triangles that overlap when projected into
// the patch domain.  Returns the number of the face with the largest number of overlapping triangles
// (or the first such triangle, if there are multiple such) or 0 if no overlaps are found.
size_t overlapping_edge_triangles(struct rt_bot_internal *bot, size_t curr_patch, struct Manifold_Info *info) {
    ON_3dVector *vect = info->vectors.At(info->patch_to_plane[curr_patch]);
    size_t overlap_cnt = 1;
    size_t total_overlapping = 0;
    while (overlap_cnt != 0) {
	std::set<size_t> edge_triangles;
	EdgeList *edges = &(info->patch_edges[curr_patch]);
	EdgeList::iterator e_it;
	std::set<size_t>::iterator sizet_it;
	// 1. build triangle set of edge triangles in patch.
	for (e_it = edges->begin(); e_it != edges->end(); e_it++) {
	    std::set<size_t> faces_from_edge = info->edge_to_face[(*e_it)];
	    for (sizet_it = faces_from_edge.begin(); sizet_it != faces_from_edge.end() ; sizet_it++) {
		if (info->face_to_patch[(*sizet_it)] == curr_patch) {
		    edge_triangles.insert((*sizet_it));
		}
	    }
	}
	// 2. Find an overlapping triangle, remove it to its own patch, fix the edges, continue 
	overlap_cnt = 0;
	for (sizet_it = edge_triangles.begin(); sizet_it != edge_triangles.end() ; sizet_it++) {
	    point_t normal;
	    point_t v0, v1, v2;
	    VSET(normal, vect->x, vect->y, vect->z);
	    pnt_project(&bot->vertices[bot->faces[(*sizet_it)*3+0]*3], &v0, normal);
	    pnt_project(&bot->vertices[bot->faces[(*sizet_it)*3+1]*3], &v1, normal);
	    pnt_project(&bot->vertices[bot->faces[(*sizet_it)*3+2]*3], &v2, normal);
	    edge_triangles.erase(*sizet_it);
	    std::set<size_t>::iterator ef_it2 = edge_triangles.begin();
	    for (ef_it2 = edge_triangles.begin(); ef_it2!=edge_triangles.end(); ef_it2++) {
		if ((*sizet_it) != (*ef_it2)) {
		    point_t u0, u1, u2;
		    pnt_project(&bot->vertices[bot->faces[(*ef_it2)*3+0]*3], &u0, normal);
		    pnt_project(&bot->vertices[bot->faces[(*ef_it2)*3+1]*3], &u1, normal);
		    pnt_project(&bot->vertices[bot->faces[(*ef_it2)*3+2]*3], &u2, normal);

		    int overlap = bn_coplanar_tri_tri_isect(v0, v1, v2, u0, u1, u2, 1);
		    if(overlap) {
                        //std::cout << "Overlap: (" << *sizet_it << "," << *ef_it2 << ")\n";
                        info->patch_cnt++;
                        size_t new_patch = info->patch_cnt; 
			info->patches[new_patch].insert(*sizet_it);
			info->patches[curr_patch].erase(*sizet_it);
			info->patch_to_plane[new_patch] = info->face_to_plane[*sizet_it];
			info->face_to_patch[*sizet_it] = new_patch;
			find_edge_segments(bot, &(info->patches[curr_patch]), &(info->patch_edges[curr_patch]), info);
			find_edge_segments(bot, &(info->patches[new_patch]), &(info->patch_edges[new_patch]), info);
                        overlap_cnt++;
                        total_overlapping++;
		    }
		}
                if (overlap_cnt > 0) break;
	    }
	    if (overlap_cnt > 0) break;
	}
    }
    if (total_overlapping > 0)
	std::cout << "Patch " << curr_patch  << " overlaps: " << total_overlapping << "\n";
    return total_overlapping;
}

void bot_partition(struct rt_bot_internal *bot, std::map< size_t, std::set<size_t> > *patches, struct Manifold_Info *info)
{
    std::map< size_t, std::set<size_t> > face_groups;
    // Calculate face normals dot product with bounding rpp planes
    for (size_t i=0; i < bot->num_faces; ++i) {
	size_t pt_A, pt_B, pt_C;
	size_t result_max;
	double vdot = 0.0;
	double result = 0.0;
	vect_t a, b, norm_dir;
	// Add vert -> edge and edge -> face mappings to global map.
	pt_A = bot->faces[i*3+0]*3;
	pt_B = bot->faces[i*3+1]*3;
	pt_C = bot->faces[i*3+2]*3;
	info->edge_to_face[mk_edge(pt_A, pt_B)].insert(i);
	info->edge_to_face[mk_edge(pt_B, pt_C)].insert(i);
	info->edge_to_face[mk_edge(pt_C, pt_A)].insert(i);
	// Categorize face
	VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
	VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
	VCROSS(norm_dir, a, b);
	VUNITIZE(norm_dir);
        info->face_normals.Append(ON_3dVector(norm_dir[0], norm_dir[1], norm_dir[2]));
	(*info).face_normals.At(i)->Unitize();
	for (size_t j=0; j < (size_t)(*info).vectors.Count(); j++) {
            vdot = ON_DotProduct(*(*info).vectors.At(j), *(*info).face_normals.At(i));
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
    for (int i = 0; i < (*info).vectors.Count(); i++) {face_group_set.insert(i);}
    size_t array_pos = 0;
    while (!face_group_set.empty()) {
        size_t curr_max = 0;
        size_t curr_vect = 0;
	for (fg_it = face_groups.begin(); fg_it != face_groups.end(); fg_it++) {
	    if((*fg_it).second.size() > curr_max) {
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
	    for(std::set<size_t>::iterator fg_it = faces->begin(); fg_it != faces->end(); fg_it++) {
		double fa = face_area(bot, *fg_it);
		if (fa > face_size_criteria) {
		   smallest_face = (*fg_it);
		   face_size_criteria = fa;
		}
	    }
	    std::queue<size_t> face_queue;
	    FaceList::iterator f_it;
	    face_queue.push(smallest_face);
	    face_groups[info->face_to_plane[face_queue.front()]].erase(face_queue.front());
	    size_t current_plane = info->face_to_plane[face_queue.front()];
	    while (!face_queue.empty()) {
		size_t face_num = face_queue.front();
		face_queue.pop();
		(*patches)[info->patch_cnt].insert(face_num);
                info->patch_to_plane[info->patch_cnt] = current_plane;
                info->face_to_patch[face_num] = info->patch_cnt;

                std::set<size_t> connected_faces;
		std::set<size_t>::iterator cf_it;
                get_connected_faces(bot, face_num, &(info->edge_to_face), &connected_faces);
		for (cf_it = connected_faces.begin(); cf_it != connected_faces.end() ; cf_it++) {
		    double curr_face_area = face_area(bot, (*cf_it));
		    if (curr_face_area < (face_size_criteria*10) && curr_face_area > (face_size_criteria*0.1)) {
			if (face_groups[info->face_to_plane[(*cf_it)]].find((*cf_it)) != face_groups[info->face_to_plane[(*cf_it)]].end()) {
			    if (info->face_to_plane[(*cf_it)] == current_plane) {
				// Large patches pose a problem for feature preservation - make an attempt to ensure "large"
				// patches are flat.
				if((*patches)[info->patch_cnt].size() > info->patch_size_threshold) {
				    vect_t origin;
				    size_t ok = 1;
				    VMOVE(origin, &bot->vertices[bot->faces[(face_num)*3]*3]);
				    ON_Plane plane(ON_3dPoint(origin[0], origin[1], origin[2]), *(*info).face_normals.At((face_num)));
				    for(int pt = 0; pt < 3; pt++) {
					point_t cpt;
					VMOVE(cpt, &bot->vertices[bot->faces[((*cf_it))*3+pt]*3]);
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
			    }
			}
		    }
		}
	    }
	}
    }
    for (std::map< size_t, std::set<size_t> >::iterator np_it = info->patches.begin(); np_it != info->patches.end(); np_it++) {
	struct bu_vls name;
	bu_vls_init(&name);
	bu_vls_printf(&name, "patch_%d.pl", (int)(*np_it).first);
	FILE* plot_file = fopen(bu_vls_addr(&name), "w");
	int r = int(256*drand48() + 1.0);
	int g = int(256*drand48() + 1.0);
	int b = int(256*drand48() + 1.0);
        std::set<size_t> *nfaces = &((*np_it).second);
	std::set<size_t>::iterator nf_it;
	for (nf_it = nfaces->begin(); nf_it != nfaces->end(); nf_it++) {
	    plot_face(&bot->vertices[bot->faces[(*nf_it)*3+0]*3], &bot->vertices[bot->faces[(*nf_it)*3+1]*3], &bot->vertices[bot->faces[(*nf_it)*3+2]*3], r, g ,b, plot_file);
	}
	fclose(plot_file);
    }
}

/**********************************************************************************
 *
 *   Code for moving from patches with edge segments to a structured group of
 *   fitted 3D NURBS curve segments and loops.
 *
 **********************************************************************************/

/*
void find_outer_loop(struct rt_bot_internal *bot, const Patch *patch, LoopList *loops, CurveList *outer_loop)
{
    // If we have more than one loop, we need to identify the outer loop.  OpenNURBS handles outer
    // and inner loops separately, so we need to know which loop is our outer surface boundary.
    if (loops->size() > 0) {
	if (loops->size() > 1) {
	    std::cout << "Loop count: " << loops->size() << "\n";
	    LoopList::iterator l_it;
	    fastf_t diag_max = 0.0;
	    CurveList oloop;
	    for (l_it = loops->begin(); l_it != loops->end(); l_it++) {
		vect_t min, max, diag;
		VSETALL(min, MAX_FASTF);
		VSETALL(max, -MAX_FASTF);
		CurveList::iterator c_it;
		for (c_it = (*l_it).begin(); c_it != (*l_it).end(); c_it++) {
		    EdgeList::iterator e_it;
		    for (e_it = (*c_it).edges.begin(); e_it != (*c_it).edges.end(); e_it++) {
			vect_t eproj, normal;
			VSET(normal, patch->category_plane->x, patch->category_plane->y, patch->category_plane->z);
			pnt_project(&bot->vertices[(*e_it).first], &eproj, normal);
			VMINMAX(min, max, eproj);
			pnt_project(&bot->vertices[(*e_it).second], &eproj, normal);
			VMINMAX(min, max, eproj);
		    }
		}
		VSUB2(diag, max, min);

		if (MAGNITUDE(diag) > diag_max) {
		    diag_max = MAGNITUDE(diag);
		    oloop = (*l_it);
		}
	    }
	    *outer_loop = oloop;
	    std::cout << "Outer loop diagonal: " << diag_max << "\n";
	} else {
	    *outer_loop = (*(loops->begin()));
	}
    }
}
*/

void find_polycurves(struct rt_bot_internal *bot, struct Manifold_Info *info)
{
    std::map< size_t, std::set<size_t> >::iterator p_it;
    for (p_it = info->patches.begin(); p_it != info->patches.end(); p_it++) {
	if (!info->patches[(*p_it).first].empty()) {
	    struct bu_vls name;
	    bu_vls_init(&name);
	    bu_vls_printf(&name, "polycurves_patch_%d.pl", (int)(*p_it).first);
	    FILE* pcurveplot = fopen(bu_vls_addr(&name), "w");

	    EdgeList *patch_edges = &(info->patch_edges[(*p_it).first]);
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
		info->patch_edges[(*p_it).first].erase(edge_queue.front());
		info->patch_edges[other_patch_id].erase(edge_queue.front());
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
			if(polycurve_edges.find((*e_it)) != polycurve_edges.end()) {
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
			    info->patch_edges[(*p_it).first].erase(*e_it);
			    info->patch_edges[other_patch_id].erase(*e_it);
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
		    if(next_pt == start_end.second) start_end_cnt++;
		    size_t old_pt = next_pt;
		    info->polycurves[curve_id].push_back(old_pt);
		    next_pt = *(vert_to_verts[old_pt].begin());
		    vert_to_verts[next_pt].erase(old_pt);
		}

		// Plot curve
		int r = int(256*drand48() + 1.0);
		int g = int(256*drand48() + 1.0);
		int b = int(256*drand48() + 1.0);
		plot_curve(bot, &(info->polycurves[curve_id]), r, g, b, pcurveplot);
		
		// Let the patches know they have a curve associated with them.
		info->patch_polycurves[(*p_it).first].insert(curve_id);
		info->patch_polycurves[other_patch_id].insert(curve_id);
		info->polycurves_patch[curve_id].insert((*p_it).first);
		info->polycurves_patch[curve_id].insert(other_patch_id);
	    }
	fclose(pcurveplot);
	}
    }
    // restore edge sets
    for (p_it = info->patches.begin(); p_it != info->patches.end(); p_it++) {
	find_edge_segments(bot, &((*p_it).second), &(info->patch_edges[(*p_it).first]), info);
    }
}

void find_loops(struct rt_bot_internal *bot, struct Manifold_Info *info) {
    std::map< size_t, std::set<size_t> >::iterator p_it;
    for (p_it = info->patches.begin(); p_it != info->patches.end(); p_it++) {
	if (!info->patches[(*p_it).first].empty()) {
	    struct bu_vls name;
	    bu_vls_init(&name);
	    bu_vls_printf(&name, "loops_patch_%d.pl", (int)(*p_it).first);
	    FILE* ploopplot = fopen(bu_vls_addr(&name), "w");
/*
	    struct bu_vls name2;
	    bu_vls_init(&name2);
	    bu_vls_printf(&name2, "polycurves_patch_%d.pl", (int)(*p_it).first);
	    FILE* pcurveplot = fopen(bu_vls_addr(&name2), "w");
*/
	    std::set<size_t> curr_polycurves = info->patch_polycurves[(*p_it).first];
            std::map<size_t, std::set<size_t> > vert_to_curves;
	    for(std::set<size_t>::iterator poly_it = curr_polycurves.begin(); poly_it != curr_polycurves.end(); poly_it++) {
	//	int r = int(256*drand48() + 1.0);
	//	int g = int(256*drand48() + 1.0);
	//	int b = int(256*drand48() + 1.0);
//		plot_curve(bot, &(info->polycurves[*poly_it]), r, g, b, pcurveplot);


                size_t curve_start = info->polycurves[*poly_it].front();
                size_t curve_end = info->polycurves[*poly_it].back();
                if (curve_start == curve_end) {
		    size_t curr_loop = info->loops.size();
		    info->loops[curr_loop].push_back((*poly_it));
		    plot_loop(bot, curr_loop, info, ploopplot);
		    curr_polycurves.erase(*poly_it);
		    //std::cout << "Patch " << (*p_it).first << " closed loop formed by curve " << (*poly_it) << " Start/End pts: (" << info->polycurves[*poly_it].front() << "," << info->polycurves[*poly_it].back() << ")\n";
		} else {
		    vert_to_curves[info->polycurves[*poly_it].front()].insert(*poly_it);	 
		    vert_to_curves[info->polycurves[*poly_it].back()].insert(*poly_it);	 
		}
	    }
 //           fclose(pcurveplot);
            
            while (curr_polycurves.size() > 0) {
                  size_t curr_loop = info->loops.size();
                  std::queue<size_t> curve_queue;
                  curve_queue.push(*(curr_polycurves.begin()));
                  curr_polycurves.erase(curve_queue.front());
                  while (!curve_queue.empty()) {
                       size_t curr_curve = curve_queue.front();
                       //std::cout << "Patch " << (*p_it).first << " loop: " << curr_loop << " adding curve: " << curr_curve << "\n";
                       curve_queue.pop();
		       info->loops[curr_loop].push_back(curr_curve);
                       size_t v1 = info->polycurves[curr_curve].front();
                       size_t v2 = info->polycurves[curr_curve].back();
                       // use vert_to_curves to assemble other curves
                       std::set<size_t> candidate_curves;
                       candidate_curves.insert(vert_to_curves[v1].begin(), vert_to_curves[v1].end());
                       candidate_curves.insert(vert_to_curves[v2].begin(), vert_to_curves[v2].end());
                       candidate_curves.erase(curr_curve);
                       for (std::set<size_t>::iterator c_it = candidate_curves.begin(); c_it != candidate_curves.end(); c_it++) {
                           if (curr_polycurves.find(*c_it) != curr_polycurves.end()) {
                           curve_queue.push(*c_it); 
			   curr_polycurves.erase(*c_it);
                           }
		       }
		  }
                  plot_loop(bot, curr_loop, info, ploopplot);
            }
           fclose(ploopplot); 
	}
    }
}



/**********************************************************************************
 *
 *              Code for fitted 3D NURBS surfaces to patches.
 *
 **********************************************************************************/

void PatchToVector3d(struct rt_bot_internal *bot, std::set<size_t> *faces, EdgeList *edges, on_fit::vector_vec3d &data)
{
    std::set<size_t>::iterator f_it;
    std::set<size_t> verts;
    unsigned int i = 0;
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
    }

    // Edges are important for patch merging - tighten them by adding more edge
    // points than just the vertex points.  Probably the right thing to do here is
    // to use points from the 3D NURBS edge curves instead of the bot edges, but
    // this is a first step and it does seem to improve the surface mapping at the
    // edges, at least from what I can tell in the wireframe.
    EdgeList::iterator e_it;
    for (e_it = edges->begin(); e_it != edges->end(); e_it++) {
        point_t p1, p2, p3, p4, p5;
        VMOVE(p1, &bot->vertices[(*e_it).first])
        VMOVE(p2, &bot->vertices[(*e_it).second])
        p3[0] = (p1[0] + p2[0])/2; 
        p3[1] = (p1[1] + p2[1])/2; 
        p3[2] = (p1[2] + p2[2])/2;
        // add edge midpoint 
	data.push_back(ON_3dVector(V3ARGS(p3)));
    }

}

void fit_nurbs_edge_curves(struct rt_bot_internal *bot, struct Manifold_Info *info) {
   FILE *nurbs_curves = fopen("nurbs_curves.pl", "w");
   std::map< size_t, std::vector<size_t> >::iterator pc_it;
   for (pc_it = info->polycurves.begin(); pc_it != info->polycurves.end(); pc_it++) {
       int r = int(256*drand48() + 1.0);
       int g = int(256*drand48() + 1.0);
       int b = int(256*drand48() + 1.0);
       pl_color(nurbs_curves, r, g, b);

       ON_3dPointArray curve_pnts;
       std::vector<size_t>::iterator v_it;
       for (v_it = (*pc_it).second.begin(); v_it != (*pc_it).second.end(); v_it++) {
	   curve_pnts.Append(ON_3dPoint(&bot->vertices[(*v_it)]));
       }
       ON_BezierCurve curve_bez((const ON_3dPointArray)curve_pnts);
       ON_NurbsCurve *curve_nurb = ON_NurbsCurve::New();
       curve_bez.GetNurbForm(*curve_nurb);
       curve_nurb->SetDomain(0.0, 1.0);
       double pt1[3], pt2[3];
       int plotres = 50;
       ON_Interval dom = curve_nurb->Domain();
       for (int i = 1; i <= plotres; i++) {
	   ON_3dPoint p = curve_nurb->PointAt(dom.ParameterAt((double)(i - 1)
		       / (double)plotres));
	   VMOVE(pt1, p);
	   p = curve_nurb->PointAt(dom.ParameterAt((double) i / (double)plotres));
	   VMOVE(pt2, p);
	   pdv_3move(nurbs_curves, pt1);
	   pdv_3cont(nurbs_curves, pt2);
       }
   }
   fclose(nurbs_curves);
}

int main(int argc, char *argv[])
{
    struct db_i *dbip;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot_ip = NULL;
    struct rt_wdb *wdbp;
    struct bu_vls name;
    char *bname;

    std::map< size_t, std::set<size_t> >::iterator p_it;
    std::map< size_t, EdgeList >::iterator pe_it;

    ON_Brep *brep = ON_Brep::New();
    FaceList::iterator f_it;

    ON_SimpleArray<ON_NurbsCurve> edges;

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

    static FILE* plot = NULL;
    point_t min, max;
    plot = fopen("edges.pl", "w");
    VSET(min, -2048, -2048, -2048);
    VSET(max, 2048, 2048, 2048);
    pdv_3space(plot, min, max);

    if (argc != 3) {
	bu_exit(1, "Usage: %s file.g object", argv[0]);
    }

    dbip = db_open(argv[1], "r+w");
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

    // Do the initial partitioning of the mesh into patches
    bu_log("Performing initial partitioning of %s\n", dp->d_namep);
    bot_partition(bot_ip, &(info.patches), &info);

    std::cout << "Patch count, first pass: " << info.patches.size() << "\n";
    plot_faces(bot_ip, &info, "patches_pass_1.pl");

    // For debugging, find edges in initial state - this is not necessary for
    // the logic of the fit.
    for (p_it = info.patches.begin(); p_it != info.patches.end(); p_it++) {
	find_edge_segments(bot_ip, &((*p_it).second), &(info.patch_edges[(*p_it).first]), &info);
    }
    plot_patch_borders(bot_ip, &info, "patch_borders_pass_1.pl");

    // "Shave" sharp triangles off of the edges by shifting them from one patch to
    // another - doesn't avoid all sharp corners, but does produce some cleanup.
    size_t shaved_cnt = 1;
    size_t edge_smoothing_count = 0;
    while (shaved_cnt != 0 && edge_smoothing_count <= 20) {
	shaved_cnt = 0;
	edge_smoothing_count++;
	for (p_it = info.patches.begin(); p_it != info.patches.end(); p_it++) {
	    find_edge_segments(bot_ip, &((*p_it).second), &(info.patch_edges[(*p_it).first]), &info);
	}

	for (p_it = info.patches.begin(); p_it != info.patches.end(); p_it++) {
	    shaved_cnt += shift_edge_triangles(bot_ip, &(info.patches), (*p_it).first, &(info.patch_edges[(*p_it).first]), &info);
	}
	std::cout << "Triangle shifts in smoothing pass " << edge_smoothing_count << ": " << shaved_cnt << "\n";
    }

    plot_faces(bot_ip, &info, "patches_pass_2.pl");
    plot_patch_borders(bot_ip, &info, "patch_borders_pass_2.pl");
    remove_empty_patches(&info);
    std::cout << "Patch count, second pass: " << info.patches.size() << "\n";

    // Identify triangles that overlap in the patches associated high-level planar projection, and
    // remove them - they pose a potential problem for surface fitting.
    size_t overlapping_face_cnt = 0;
    for (pe_it = info.patch_edges.begin(); pe_it != info.patch_edges.end(); pe_it++) {
	overlapping_face_cnt += overlapping_edge_triangles(bot_ip, (*pe_it).first, &info);
    }
    std::cout << "Found " << overlapping_face_cnt << " overlapping faces in projection\n";

    // Make sure all our edge sets are up to date
    for (p_it = info.patches.begin(); p_it != info.patches.end(); p_it++) {
	find_edge_segments(bot_ip, &((*p_it).second), &(info.patch_edges[(*p_it).first]), &info);
    }

    std::cout << "Patch count, third pass: " << info.patches.size() << "\n";
    plot_faces(bot_ip, &info, "patches_pass_3.pl");
    plot_patch_borders(bot_ip, &info, "patch_borders_pass_3.pl");

    // Now that the mesh is fully partitioned, build the maps needed for curve discovery
    sync_structure_maps(bot_ip, &info);

    // Now, using the patch edge sets, construct polycurves
    find_polycurves(bot_ip, &info);

    // Build the loops
    find_loops(bot_ip, &info);

    fit_nurbs_edge_curves(bot_ip, &info);

    // Actually fit the NURBS surfaces
    size_t nurbs_surface_count = -1;
    std::ofstream patch_to_surface;
    std::ofstream surface_to_patch;
    patch_to_surface.open("patch_to_surface.txt");
    surface_to_patch.open("surface_to_patch.txt");
    for (int p = 0; p < (int)info.patches.size(); p++) {
	if (info.patches[p].size() != 0) {
	    std::cout << "Found patch " << p << " with " << info.patches[p].size() << " faces\n";
	    unsigned order(3);
	    on_fit::NurbsDataSurface data;
	    PatchToVector3d(bot_ip, &(info.patches[p]), &(info.patch_edges[p]), data.interior);
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
	    brep->NewFace(fit.m_nurbs);
            info.surface_array.Append(new ON_NurbsSurface(fit.m_nurbs));
            nurbs_surface_count++;
            info.patch_to_surface[p] = nurbs_surface_count;
            patch_to_surface << p << " -> " << nurbs_surface_count << "\n";
            surface_to_patch << nurbs_surface_count << " -> " << p << "\n";
	}
    }
    patch_to_surface.close();
    surface_to_patch.close();

    // Generate BRL-CAD brep object
    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    bname = (char*)bu_malloc(strlen(argv[2])+6, "char");
    bu_strlcpy(bname, argv[2], strlen(argv[2])+1);
    bu_strlcat(bname, "_brep", strlen(bname)+6);
    if (mk_brep(wdbp, bname, brep) == 0) {
	bu_log("Generated brep object %s\n", bname);
    }
    bu_free(bname, "char");

#if 0
    // SSI intersection.  May need edge-based polycurves anyway to guide selection of "correct" intersection segment in the cases where
    // fitted surfaces intersect multiple times - will want curves with at least the start and endpoints close to those
    // of the polycurves, and the total absence of a candidate SSI curve for a given polycurve would give a local indication
    // of a surface fitting problem.
    FILE* curve_plot = fopen("curve_plot.pl", "w");
    pl_color(curve_plot, 255, 255, 255);
    std::set<std::pair<size_t, size_t> > patch_interactions;
    std::set<std::pair<size_t, size_t> >::iterator o_it;
    for (p_it = info.patches.begin(); p_it != info.patches.end(); p_it++) {
	EdgeList::iterator e_it;
	for (e_it = info.patch_edges[(*p_it).first].begin(); e_it != info.patch_edges[(*p_it).first].end(); e_it++) {
	    std::set<size_t> ep = info.edge_to_patch[(*e_it)];
	    ep.erase((*p_it).first);
	    if((*p_it).first < (*ep.begin())) {
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
