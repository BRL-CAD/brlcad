#include "common.h"

#include <map>
#include <set>
#include <queue>
#include <list>
#include <iostream>

#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "plot3.h"
//#include "opennurbs_fit.h"

typedef std::pair<size_t, size_t> Edge;
typedef std::map< size_t, std::set<Edge> > VertToEdge;
typedef std::map< size_t, std::set<size_t> > VertToPatch;
typedef std::map< Edge, std::set<size_t> > EdgeToPatch;
typedef std::map< Edge, std::set<size_t> > EdgeToFace;
typedef std::set<Edge> EdgeList;
typedef std::set<size_t> FaceList;

class ThreeDPoint
{
public:
    fastf_t x, y, z;
    ThreeDPoint(fastf_t, fastf_t, fastf_t);
    bool operator< (const ThreeDPoint &other) const {
	return (100*x + 10*y + z) < (100*other.x + 10*other.y + other.z);
    }
};

ThreeDPoint::ThreeDPoint(fastf_t a, fastf_t b, fastf_t c)
{
    x = a;
    y = b;
    z = c;
}

typedef std::map< ThreeDPoint, std::set<size_t> > FaceCategorization;

class Curve
{
public:
    size_t id;
    std::pair<size_t, size_t> start_and_end;
    size_t inner_patch;
    size_t outer_patch;
    EdgeList edges;
    bool operator< (const Curve &other) const {
	return id < other.id;
    }
};

typedef std::set<Curve> CurveList;
typedef std::set<CurveList> LoopList;

class Patch
{
public:
    size_t id;
    ThreeDPoint *category_plane;
    EdgeList edges;
    LoopList loops;
    FaceList faces;
    CurveList outer_loop;
    bool operator< (const Patch &other) const {
	return id < other.id;
    }
};


typedef std::set<ThreeDPoint> VectList;
#if 0
void PatchToVector3d(struct rt_bot_internal *bot, const Patch *patch, on_fit::vector_vec3d &data)
{
    FaceList::iterator f_it;
    std::set<size_t> verts;
    std::set<size_t>::iterator v_it;
    int i = 0;
    for (i = 0; i < bot->num_vertices; i++) {
	printf("v(%d): %f %f %f\n", i, V3ARGS(&bot->vertices[3*i]));
    }
    for (f_it = patch->faces.begin(); f_it != patch->faces.end(); f_it++) {
	verts.insert(bot->faces[(*f_it)*3+0]);
	verts.insert(bot->faces[(*f_it)*3+1]);
	verts.insert(bot->faces[(*f_it)*3+2]);
    }
    for (v_it = verts.begin(); v_it != verts.end(); v_it++) {
	printf("vert %d\n", (int)(*v_it));
	printf("vert(%d): %f %f %f\n", (int)(*v_it), V3ARGS(&bot->vertices[(*v_it)*3]));
	data.push_back(Eigen::Vector3d(V3ARGS(&bot->vertices[(*v_it)*3])));
    }
}
#endif


// To plot specific groups of curves in MGED, do the following:
// set glob_compat_mode 0
// set pl_list [glob 27_curve*.pl]
// foreach plfile $pl_list {overlay $plfile}
void plot_curve(struct rt_bot_internal *bot, size_t patch_id, const Curve *curve, int r, int g, int b, FILE *c_plot)
{
    if (c_plot == NULL) {
	struct bu_vls name;
	static FILE* plot = NULL;
	bu_vls_init(&name);
	bu_vls_printf(&name, "%d_curve_%d.pl", (int) patch_id, (int) curve->id);
	plot = fopen(bu_vls_addr(&name), "w");
	pl_color(plot, int(256*drand48() + 1.0), int(256*drand48() + 1.0), int(256*drand48() + 1.0));
	EdgeList::iterator e_it;
	for (e_it = curve->edges.begin(); e_it != curve->edges.end(); e_it++) {
	    pdv_3move(plot, &bot->vertices[(*e_it).first]);
	    pdv_3cont(plot, &bot->vertices[(*e_it).second]);
	}
	fclose(plot);
	bu_vls_free(&name);
    } else {
	pl_color(c_plot, r, g, b);
	EdgeList::iterator e_it;
	for (e_it = curve->edges.begin(); e_it != curve->edges.end(); e_it++) {
	    pdv_3move(c_plot, &bot->vertices[(*e_it).first]);
	    pdv_3cont(c_plot, &bot->vertices[(*e_it).second]);
	}
    }
}

// plot loop
void plot_loop(struct rt_bot_internal *bot, size_t patch_id, int loop_num, int r, int g, int b, const CurveList *curves, FILE *l_plot)
{
    CurveList::iterator c_it;
    if (l_plot == NULL) {
	struct bu_vls name;
	static FILE* lplot = NULL;
	bu_vls_init(&name);
	bu_vls_printf(&name, "%d_loop_%d.pl", (int) patch_id, loop_num);
	lplot = fopen(bu_vls_addr(&name), "w");
	for (c_it = curves->begin(); c_it != curves->end(); c_it++) {
	    plot_curve(bot, patch_id, &(*c_it), r, g, b, lplot);
	}
	fclose(lplot);
    } else {
	for (c_it = curves->begin(); c_it != curves->end(); c_it++) {
	    plot_curve(bot, patch_id, &(*c_it), r, g, b, l_plot);
	}
    }
}

void find_edge_segments(struct rt_bot_internal *bot, const Patch *patch, EdgeList *patch_edges, VertToEdge *vert_to_edge)
{
    size_t pt_A, pt_B, pt_C;
    std::set<size_t>::iterator it;
    std::map<std::pair<size_t, size_t>, size_t> edge_face_cnt;
    std::map<std::pair<size_t, size_t>, size_t>::iterator efc_it;
    for (it = patch->faces.begin(); it != patch->faces.end(); it++) {
	pt_A = bot->faces[(*it)*3+0]*3;
	pt_B = bot->faces[(*it)*3+1]*3;
	pt_C = bot->faces[(*it)*3+2]*3;
	if (pt_A <= pt_B) {
	    edge_face_cnt[std::make_pair(pt_A, pt_B)] += 1;
	} else {
	    edge_face_cnt[std::make_pair(pt_B, pt_A)] += 1;
	}
	if (pt_B <= pt_C) {
	    edge_face_cnt[std::make_pair(pt_B, pt_C)] += 1;
	} else {
	    edge_face_cnt[std::make_pair(pt_C, pt_B)] += 1;
	}
	if (pt_C <= pt_A) {
	    edge_face_cnt[std::make_pair(pt_C, pt_A)] += 1;
	} else {
	    edge_face_cnt[std::make_pair(pt_A, pt_C)] += 1;
	}
    }

    for (efc_it = edge_face_cnt.begin(); efc_it!=edge_face_cnt.end(); efc_it++) {
	if ((*efc_it).second == 1) {
	    (*vert_to_edge)[(*efc_it).first.first].insert((*efc_it).first);
	    (*vert_to_edge)[(*efc_it).first.second].insert((*efc_it).first);
	    patch_edges->insert((*efc_it).first);
	}
    }
}

// Assemble edges into curves.  A curve is shared between two
// and only two patches, and all vertex points except the endpoints
// are used by only two edges (edges in this case referring to face
// edges that are also part of some edge curve).
void find_curves(struct rt_bot_internal *bot, const Patch *patch, std::set<Curve> *loop_curves, EdgeList *patch_edges, VertToEdge *vert_to_edge, VertToPatch *vert_to_patch, EdgeToPatch *edge_to_patch)
{
    size_t curve_cnt = 1;
    while (!patch_edges->empty()) {
	Curve curve;
	curve.start_and_end.first = INT_MAX;
	curve.start_and_end.second = INT_MAX;
	curve.inner_patch = patch->id;
	curve.id = curve_cnt;
	curve_cnt++;
	// Need to get the other patch for this curve
	std::set<size_t> edge_patches = (*edge_to_patch)[(*patch_edges->begin())];
	edge_patches.erase(patch->id);
	curve.outer_patch = (*edge_patches.begin());

	// Initialize queue
	std::queue<Edge> edge_queue;
	edge_queue.push(*patch_edges->begin());
	patch_edges->erase(*patch_edges->begin());

	while (!edge_queue.empty()) {
	    VertToEdge::iterator v_it;
	    std::set<Edge> eset1, eset2;

	    // Add the first edge in the queue
	    Edge curr_edge = edge_queue.front();
	    edge_queue.pop();
	    curve.edges.insert(curr_edge);

	    // Check both vertices of the current edge to see how many patches use them.
	    // If three patches use a vertex, that vertex terminates the current curve.
	    // The current edge is the only edge associated with the current vertex that
	    // can be a part of this curve.  Otherwise, pull the edge set for this vertex
	    // for further processing.
	    if ((*vert_to_patch)[curr_edge.first].size() > 2) {
		if (curve.start_and_end.first == INT_MAX) {
		    curve.start_and_end.first = curr_edge.first;
		} else {
		    curve.start_and_end.second= curr_edge.first;
		}
	    } else {
		eset1 = (*vert_to_edge)[curr_edge.first];
		eset1.erase(curr_edge);
	    }
	    // use edgetopatch - if the edge shares the current outer patch, add it to the queue
	    if (!eset1.empty()) {
		Edge es1 = *eset1.begin();
		if ((*edge_to_patch)[es1].find(curve.outer_patch) != (*edge_to_patch)[es1].end() &&
		    curve.edges.find(es1) == curve.edges.end()) {
		    edge_queue.push(es1);
		    patch_edges->erase(es1);
		}
	    }

	    // Also check the other vertex of the current edge.  The termination criteria
	    // are the same as for the first vertex
	    if ((*vert_to_patch)[curr_edge.second].size() > 2) {
		if (curve.start_and_end.first == INT_MAX) {
		    curve.start_and_end.first = curr_edge.second;
		} else {
		    curve.start_and_end.second= curr_edge.second;
		}
	    } else {
		eset2 = (*vert_to_edge)[curr_edge.second];
		eset2.erase(curr_edge);
	    }

	    if (!eset2.empty()) {
		Edge es2 = *eset2.begin();
		if ((*edge_to_patch)[es2].find(curve.outer_patch) != (*edge_to_patch)[es2].end() &&
		    curve.edges.find(es2) == curve.edges.end()) {
		    edge_queue.push(es2);
		    patch_edges->erase(es2);
		}
	    }
	}
	EdgeList::iterator e_it;
	std::set<size_t> single_verts;
	std::map<size_t, size_t> vertex_use;
	std::map<size_t, size_t>::iterator vu_it;
	for (e_it = curve.edges.begin(); e_it != curve.edges.end(); e_it++) {
	    vertex_use[(*e_it).first]++;
	    vertex_use[(*e_it).second]++;
	}
	for (vu_it = vertex_use.begin(); vu_it != vertex_use.end(); vu_it++) {
	    if ((*vu_it).second == 1) single_verts.insert((*vu_it).first);
	}
	if (single_verts.size() != 2 && single_verts.size() != 0) {
	    printf("Error - curve returned with edge count other than two or closed!\n");
	} else {
	    if (single_verts.size() == 2) {
		curve.start_and_end.first = (*single_verts.begin());
		single_verts.erase(single_verts.begin());
		curve.start_and_end.second = (*single_verts.begin());
	    } else {
		curve.start_and_end.first = (*curve.edges.begin()).first;
		curve.start_and_end.second = (*curve.edges.begin()).first;
	    }
	}
	std::cout << "      " << patch->id << " curve " << curve.id << " edge count: " << curve.edges.size() << "\n";
	plot_curve(bot, patch->id, &curve, 0, 0, 0, NULL);
	loop_curves->insert(curve);
    }
}

int loop_is_closed(std::set<Curve> *curves)
{
    std::set<Curve>::iterator cv_it;
    EdgeList::iterator ev_it;
    std::map<size_t, size_t> vert_use_cnt;
    std::map<size_t, size_t>::iterator vv_it;
    int closed = 1;
    for (cv_it = curves->begin(); cv_it != curves->end(); cv_it++) {
	for (ev_it = (*cv_it).edges.begin(); ev_it != (*cv_it).edges.end(); ev_it++) {
	    vert_use_cnt[(*ev_it).first]++;
	    vert_use_cnt[(*ev_it).second]++;
	}
    }

    for (vv_it = vert_use_cnt.begin(); vv_it!=vert_use_cnt.end(); vv_it++) {
	if ((*vv_it).second == 1) {
	    closed = 0;
	}
    }
    return closed;
}

void find_single_curve_loops(std::set<Curve> *loop_curves, LoopList *loops)
{
    CurveList::iterator cl_it;
    for (cl_it = loop_curves->begin(); cl_it != loop_curves->end(); cl_it++) {
	if ((*cl_it).start_and_end.first == (*cl_it).start_and_end.second) {
	    //std::cout << "Curve " << (*cl_it).id << " is a loop\n";
	    CurveList curve_loop;
	    curve_loop.insert((*cl_it));
	    loops->insert(curve_loop);
	    loop_curves->erase(cl_it);
	}
    }
}

void find_multicurve_loops(std::set<Curve> *loop_curves, LoopList *loops)
{
    // build a map of endpoints to curves
    std::map<size_t, std::set<Curve> > endpts_to_curves;
    CurveList::iterator cl_it;
    for (cl_it = loop_curves->begin(); cl_it != loop_curves->end(); cl_it++) {
	endpts_to_curves[(*cl_it).start_and_end.first].insert((*cl_it));
	endpts_to_curves[(*cl_it).start_and_end.second].insert((*cl_it));
    }

    while (!loop_curves->empty()) {
	std::queue<Curve> curve_queue;
	CurveList curr_loop;
	CurveList::iterator c_it;
	curve_queue.push(*loop_curves->begin());
	loop_curves->erase(*loop_curves->begin());
	while (!curve_queue.empty()) {
	    int is_matched = 0;
	    // Add the first edge in the queue
	    Curve curr_curve = curve_queue.front();
	    curve_queue.pop();
	    curr_loop.insert(curr_curve);
	    std::set<Curve> curves1, curves2;
	    std::set<Curve>::iterator curves_it;

	    // Pull curve id list.
	    curves1 = endpts_to_curves[curr_curve.start_and_end.first];
	    curves2 = endpts_to_curves[curr_curve.start_and_end.second];
	    curves1.erase(curr_curve);
	    curves2.erase(curr_curve);
	    // use the curve info - if the curve shares the current patches, add it to the queue
	    for (curves_it = curves1.begin(); curves_it != curves1.end(); curves_it++) {
		if ((*curves_it).inner_patch == curr_curve.inner_patch &&
		    curr_loop.find((*curves_it)) == curr_loop.end()) {
		    curve_queue.push((*curves_it));
		    loop_curves->erase((*curves_it));
		    is_matched = 1;
		}
	    }
	    for (curves_it = curves2.begin(); curves_it != curves2.end(); curves_it++) {
		if ((*curves_it).inner_patch == curr_curve.inner_patch &&
		    curr_loop.find((*curves_it)) == curr_loop.end()) {
		    curve_queue.push((*curves_it));
		    loop_curves->erase((*curves_it));
		    is_matched = 1;
		}
	    }
	}
	if (curr_loop.size() > 0) {
	    std::cout << "Inserting loop:" << curr_loop.size() << "\n";
	    loops->insert(curr_loop);
	}
	if (!loop_is_closed(&curr_loop)) std::cout << "Waaaittt!  Loop is not closed!!!\n";
    }
}

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
			vect_t ep1, ep2, eproj, eproj2, norm_scale, normal;
			fastf_t dotP;
			VMOVE(ep1, &bot->vertices[(*e_it).first]);
			VMOVE(ep2, &bot->vertices[(*e_it).second]);
			VSET(normal, patch->category_plane->x, patch->category_plane->y, patch->category_plane->z);
			dotP = VDOT(ep1, normal);
			VSCALE(norm_scale, normal, dotP);
			VSUB2(eproj, ep1, norm_scale);
			VSCALE(eproj2, eproj, 1.1);
			VMINMAX(min, max, eproj);

			dotP = VDOT(ep2, normal);
			VSCALE(norm_scale, normal, dotP);
			VSUB2(eproj, ep2, norm_scale);
			VSCALE(eproj2, eproj, 1.1);
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

void find_edges(struct rt_bot_internal *bot, const Patch *patch, FILE *plot, ON_SimpleArray<ON_NurbsCurve> *UNUSED(bedges), EdgeToPatch *edge_to_patch, VertToPatch *vert_to_patch)
{
    VertToEdge vert_to_edge;
    EdgeList patch_edges;
    LoopList patch_loops;
    LoopList patch_loops_1curve;
    LoopList patch_loops_all;

    // Find the edges of the patch
    find_edge_segments(bot, (const Patch *)patch, &patch_edges, &vert_to_edge);

    std::cout << "Patch " << patch->id << " edge total: " << patch_edges.size() << "\n";

    // Assemble the edge segments into curves
    std::set<Curve> loop_curves;
    find_curves(bot, patch, &loop_curves, &patch_edges, &vert_to_edge, vert_to_patch, edge_to_patch);

    // We have the curves of the patch - build the loops
    //
    // First, pull any curves that are already loops
    find_single_curve_loops(&loop_curves, &patch_loops_1curve);

    // assemble multicurve loops - use end points to find candidate curves,
    // then check patch ids to confirm
    find_multicurve_loops(&loop_curves, &patch_loops);
    LoopList::iterator l_it;
    int lp_cnt = 1;
    struct bu_vls name;
    static FILE* laplot = NULL;
    bu_vls_init(&name);
    bu_vls_printf(&name, "%d_loop.pl", (int) patch->id);
    laplot = fopen(bu_vls_addr(&name), "w");
    for (l_it = patch_loops.begin(); l_it != patch_loops.end(); l_it++) {
	int r = int(256*drand48() + 1.0);
	int g = int(256*drand48() + 1.0);
	int b = int(256*drand48() + 1.0);
	plot_loop(bot, patch->id, lp_cnt, r, g, b, &(*l_it), laplot);
	lp_cnt++;
    }
    fclose(laplot);


    // Find the outer loop - this will be the outer trimming loop for the NURBS patch
    CurveList outer_loop;
    LoopList::iterator all_it;
    for (all_it = patch_loops_1curve.begin(); all_it != patch_loops_1curve.end(); all_it++) {
	patch_loops_all.insert((*all_it));
    }
    for (all_it = patch_loops.begin(); all_it != patch_loops.end(); all_it++) {
	patch_loops_all.insert((*all_it));
    }
    find_outer_loop(bot, (const Patch *)patch, &patch_loops_all, &outer_loop);
    plot_loop(bot, patch->id, 0, 255, 0, 0, &(outer_loop), NULL);

    // Make some edge curves
    for (l_it = patch_loops.begin(); l_it != patch_loops.end(); l_it++) {
	CurveList::iterator c_it;
	for (c_it = (*l_it).begin(); c_it != (*l_it).end(); c_it++) {
	    pl_color(plot, int(256*drand48() + 1.0), int(256*drand48() + 1.0), int(256*drand48() + 1.0));
	    if ((*c_it).edges.size() > 1) {
		EdgeList::iterator e_it;
		std::multimap<size_t, size_t> vert_map;
		ON_3dPointArray curve_pnts;
		for (e_it = (*c_it).edges.begin(); e_it != (*c_it).edges.end(); e_it++) {
		    vert_map.insert(std::make_pair((*e_it).first, (*e_it).second));
		    vert_map.insert(std::make_pair((*e_it).second, (*e_it).first));
		}
		size_t current_pt = (*c_it).start_and_end.first;
		std::set<size_t> visited;

		while (current_pt != (*c_it).start_and_end.second) {
		    visited.insert(current_pt);
		    curve_pnts.Append(ON_3dPoint(&bot->vertices[current_pt]));
		    std::multimap<size_t, size_t>::iterator v_it;
		    std::pair<std::multimap<size_t, size_t>::iterator, std::multimap<size_t, size_t>::iterator> v_range;
		    v_range = vert_map.equal_range(current_pt);
		    for (v_it = v_range.first; v_it != v_range.second; v_it++) {
			if ((*v_it).second != current_pt && visited.find((*v_it).second) == visited.end()) {
			    current_pt = (*v_it).second;
			}
		    }
		}
		curve_pnts.Append(ON_3dPoint(&bot->vertices[(*c_it).start_and_end.second]));

		ON_BezierCurve curve_bez((const ON_3dPointArray)curve_pnts);
		ON_NurbsCurve *curve_nurb = ON_NurbsCurve::New();
		curve_bez.GetNurbForm(*curve_nurb);
		curve_nurb->SetDomain(0.0, 1.0);
		//bedges->Append(*curve_nurb);

		double pt1[3], pt2[3];
		int plotres = 50;
		ON_Interval dom = curve_nurb->Domain();
		for (int i = 1; i <= plotres; i++) {
		    ON_3dPoint p = curve_nurb->PointAt(dom.ParameterAt((double)(i - 1)
								       / (double)plotres));
		    VMOVE(pt1, p);
		    p = curve_nurb->PointAt(dom.ParameterAt((double) i / (double)plotres));
		    VMOVE(pt2, p);
		    pdv_3move(plot, pt1);
		    pdv_3cont(plot, pt2);
		}
	    } else {
		pdv_3move(plot, &bot->vertices[(*(*c_it).edges.begin()).first]);
		pdv_3cont(plot, &bot->vertices[(*(*c_it).edges.begin()).second]);
	    }
	}
    }

}

void make_structure(struct rt_bot_internal *bot, VectList *vects, std::set<Patch> *patches, ON_SimpleArray<ON_NurbsCurve> *bedges, FILE *plot)
{
    FaceCategorization face_groups;
    FaceCategorization::iterator fg_it;
    EdgeToFace edge_to_face;
    VertToPatch vert_to_patch;
    EdgeToPatch edge_to_patch;
    int patch_cnt = 1;

    VectList::iterator vect_it;
    // Break apart the bot based on angles between faces and bounding rpp planes
    for (size_t i=0; i < bot->num_faces; ++i) {
	size_t pt_A, pt_B, pt_C;
	size_t count = 0;
	const ThreeDPoint *result_max;
	fastf_t vdot = 0.0;
	fastf_t result = 0.0;
	vect_t a, b, dir, norm_dir;
	// Add vert -> edge and edge -> face mappings to global map.
	pt_A = bot->faces[i*3+0]*3;
	pt_B = bot->faces[i*3+1]*3;
	pt_C = bot->faces[i*3+2]*3;
	if (pt_A <= pt_B) {
	    edge_to_face[std::make_pair(pt_A, pt_B)].insert(i);
	} else {
	    edge_to_face[std::make_pair(pt_B, pt_A)].insert(i);
	}
	if (pt_B <= pt_C) {
	    edge_to_face[std::make_pair(pt_B, pt_C)].insert(i);
	} else {
	    edge_to_face[std::make_pair(pt_C, pt_B)].insert(i);
	}
	if (pt_C <= pt_A) {
	    edge_to_face[std::make_pair(pt_C, pt_A)].insert(i);
	} else {
	    edge_to_face[std::make_pair(pt_A, pt_C)].insert(i);
	}
	// Categorize face
	VSUB2(a, &bot->vertices[bot->faces[i*3+1]*3], &bot->vertices[bot->faces[i*3]*3]);
	VSUB2(b, &bot->vertices[bot->faces[i*3+2]*3], &bot->vertices[bot->faces[i*3]*3]);
	VCROSS(norm_dir, a, b);
	VUNITIZE(norm_dir);
	for (vect_it = vects->begin(); vect_it != vects->end(); vect_it++) {
	    count++;
	    VSET(dir, (*vect_it).x, (*vect_it).y, (*vect_it).z);
	    VUNITIZE(dir);
	    vdot = VDOT(dir, norm_dir);
	    if (vdot > result) {
		result_max = &(*vect_it);
		result = vdot;
	    }
	}
	face_groups[*result_max].insert(i);
    }
    // For each "sub-bot", identify contiguous patches.
    for (fg_it = face_groups.begin(); fg_it != face_groups.end(); fg_it++) {
	// Make a list of all faces associated with the current category
	std::set<size_t> group_faces = (*fg_it).second;

	// All faces must belong to some patch - continue until all faces are processed
	while (!group_faces.empty()) {
	    size_t pt_A, pt_B, pt_C;
	    Edge e1, e2, e3;
	    Patch curr_patch;
	    curr_patch.id = patch_cnt;
	    curr_patch.category_plane = new ThreeDPoint((*fg_it).first.x, (*fg_it).first.y, (*fg_it).first.z);
	    patch_cnt++;
	    std::queue<size_t> face_queue;
	    FaceList::iterator f_it;
	    face_queue.push(*(group_faces.begin()));
	    while (!face_queue.empty()) {
		std::set<Edge> face_edges;
		std::set<Edge>::iterator face_edges_it;
		size_t face_num = face_queue.front();
		face_queue.pop();
		curr_patch.faces.insert(face_num);
		group_faces.erase(face_num);
		pt_A = bot->faces[face_num*3+0]*3;
		pt_B = bot->faces[face_num*3+1]*3;
		pt_C = bot->faces[face_num*3+2]*3;
		if (pt_A <= pt_B) {
		    e1.first = pt_A;
		    e1.second= pt_B;
		} else {
		    e1.first = pt_B;
		    e1.second= pt_A;
		}
		face_edges.insert(e1);
		edge_to_patch[e1].insert(curr_patch.id);
		vert_to_patch[e1.first].insert(curr_patch.id);
		vert_to_patch[e1.second].insert(curr_patch.id);
		if (pt_B <= pt_C) {
		    e2.first = pt_B;
		    e2.second= pt_C;
		} else {
		    e2.first = pt_C;
		    e2.second= pt_B;
		}
		face_edges.insert(e2);
		edge_to_patch[e2].insert(curr_patch.id);
		vert_to_patch[e2.first].insert(curr_patch.id);
		vert_to_patch[e2.second].insert(curr_patch.id);

		if (pt_C <= pt_A) {
		    e3.first = pt_C;
		    e3.second= pt_A;
		} else {
		    e3.first = pt_A;
		    e3.second= pt_C;
		}
		face_edges.insert(e3);
		edge_to_patch[e3].insert(curr_patch.id);
		vert_to_patch[e3.first].insert(curr_patch.id);
		vert_to_patch[e3.second].insert(curr_patch.id);

		for (face_edges_it = face_edges.begin(); face_edges_it != face_edges.end(); face_edges_it++) {
		    std::set<size_t> faces_from_edge = edge_to_face[(*face_edges_it)];
		    std::set<size_t>::iterator ffe_it;
		    for (ffe_it = faces_from_edge.begin(); ffe_it != faces_from_edge.end() ; ffe_it++) {
			FaceList::iterator this_it = group_faces.find((*ffe_it));
			if (this_it != group_faces.end()) {
			    face_queue.push((*ffe_it));
			    group_faces.erase((*ffe_it));
			}
		    }
		}

	    }
	    //std::cout << "Patch: " << curr_patch.id << "\n";
	    patches->insert(curr_patch);
	}
    }

    std::cout << "Patch count: " << patches->size() << "\n";

    // Build edges
    std::set<Patch>::iterator patch_it;
    for (patch_it = (*patches).begin(); patch_it != (*patches).end(); patch_it++) {
	find_edges(bot, &(*patch_it), plot, bedges, &edge_to_patch, &vert_to_patch);
    }
}


int
main(int argc, char *argv[])
{
    struct db_i *dbip;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot_ip = NULL;
    //struct rt_wdb *wdbp;
    struct bu_vls name;
    //char *bname;

    VectList vectors;
    std::set<Patch> patches;
    std::set<Patch>::iterator p_it;
    //ON_Brep *brep = ON_Brep::New();
    FaceList::iterator f_it;

    ON_SimpleArray<ON_NurbsCurve> edges;

    vectors.insert(*(new ThreeDPoint(-1,0,0)));
    vectors.insert(*(new ThreeDPoint(0,-1,0)));
    vectors.insert(*(new ThreeDPoint(0,0,-1)));
    vectors.insert(*(new ThreeDPoint(1,0,0)));
    vectors.insert(*(new ThreeDPoint(0,1,0)));
    vectors.insert(*(new ThreeDPoint(0,0,1)));

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
    } else {
	bu_log("Got %s\n", dp->d_namep);
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

    make_structure(bot_ip, &vectors, &patches, &edges, plot);
#if 0
    for (p_it = patches.begin(); p_it != patches.end(); p_it++) {
	std::cout << "Found patch " << (*p_it).id << "\n";
	unsigned order(3);
	on_fit::NurbsDataSurface data;
	PatchToVector3d(bot_ip, &(*p_it), data.interior);
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
	    bu_log("Solving %d - iteration %d\n", (*p_it).id, i);
	    fit.solve();
	}
	brep->NewFace(fit.m_nurbs);
    }

    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    bname = (char*)bu_malloc(strlen(argv[2])+6, "char");
    bu_strlcpy(bname, argv[2], strlen(argv[2])+1);
    bu_strlcat(bname, "_brep", strlen(bname)+6);
    if (mk_brep(wdbp, bname, brep) == 0) {
	bu_log("Generated brep object %s\n", bname);
    }
    bu_free(bname, "char");
#endif

    return 0;
}
