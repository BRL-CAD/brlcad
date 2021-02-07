/*                      C D T _ M E S H . H
 * BRL-CAD
 *
 * Copyright (c) 2019-2021 United States Government as represented by
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
/** @file cdt_mesh.h
 *
 * Mesh routines in support of Constrained Delaunay Triangulation of NURBS
 * B-Rep objects
 *
 */

// This evolved from the original trimesh halfedge data structure code:

// Author: Yotam Gingold <yotam (strudel) yotamgingold.com>
// License: Public Domain.  (I, Yotam Gingold, the author, release this code into the public domain.)
// GitHub: https://github.com/yig/halfedge


#ifndef __cdt_mesh_h__
#define __cdt_mesh_h__

#include "common.h"

#include <algorithm>
#include <vector>
#include <set>
#include <map>
#include "RTree.h"
#include "opennurbs.h"
#include "bu/color.h"
#include "bg/polygon.h"
#include "bg/tri_tri.h"

extern "C" {
    struct ctriangle_t {
	long v[3];
	size_t ind;
	bool all_bedge;
	bool isect_edge;
	bool uses_uncontained;
	bool contains_uncontained;
	double angle_to_nearest_uncontained;
    };
}

//class omesh_t;

struct edge2d_t {
    long v2d[2];

    long& start() { return v2d[0]; }
    const long& start() const {	return v2d[0]; }
    long& end() { return v2d[1]; }
    const long& end() const { return v2d[1]; }

    edge2d_t(long i, long j)
    {
	v2d[0] = i;
	v2d[1] = j;
    }

    edge2d_t()
    {
	v2d[0] = v2d[1] = -1;
    }

    void set(long i, long j)
    {
	v2d[0] = i;
	v2d[1] = j;
    }

    bool operator<(edge2d_t other) const
    {
	bool c1 = (v2d[0] < other.v2d[0]);
	bool c1e = (v2d[0] == other.v2d[0]);
	bool c2 = (v2d[1] < other.v2d[1]);
	return (c1 || (c1e && c2));
    }
    bool operator==(edge2d_t other) const
    {
	bool c1 = (v2d[0] == other.v2d[0]);
	bool c2 = (v2d[1] == other.v2d[1]);
	return (c1 && c2);
    }
    bool operator!=(edge2d_t other) const
    {
	bool c1 = (v2d[0] != other.v2d[0]);
	bool c2 = (v2d[1] != other.v2d[1]);
	return (c1 || c2);
    }
};


struct edge_t {
    long v[2];

    long& start() { return v[0]; }
    const long& start() const {	return v[0]; }
    long& end() { return v[1]; }
    const long& end() const { return v[1]; }

    edge_t(long i, long j)
    {
	v[0] = i;
	v[1] = j;
    }

    edge_t()
    {
	v[0] = v[1] = -1;
    }

    void set(long i, long j)
    {
	v[0] = i;
	v[1] = j;
    }

    bool operator<(edge_t other) const
    {
	bool c1 = (v[0] < other.v[0]);
	bool c1e = (v[0] == other.v[0]);
	bool c2 = (v[1] < other.v[1]);
	return (c1 || (c1e && c2));
    }
    bool operator==(edge_t other) const
    {
	bool c1 = (v[0] == other.v[0]);
	bool c2 = (v[1] == other.v[1]);
	return (c1 && c2);
    }
    bool operator!=(edge_t other) const
    {
	bool c1 = (v[0] != other.v[0]);
	bool c2 = (v[1] != other.v[1]);
	return (c1 || c2);
    }
};

struct uedge_t {
    long v[2];

    uedge_t()
    {
	v[0] = v[1] = -1;
    }

    uedge_t(struct edge_t e)
    {
	v[0] = (e.v[0] <= e.v[1]) ? e.v[0] : e.v[1];
	v[1] = (e.v[0] > e.v[1]) ? e.v[0] : e.v[1];
    }

    uedge_t(long i, long j)
    {
	v[0] = (i <= j) ? i : j;
	v[1] = (i > j) ? i : j;
    }

    void set(long i, long j)
    {
	v[0] = (i <= j) ? i : j;
	v[1] = (i > j) ? i : j;
    }

    bool operator<(uedge_t other) const
    {
	bool c1 = (v[0] < other.v[0]);
	bool c1e = (v[0] == other.v[0]);
	bool c2 = (v[1] < other.v[1]);
	return (c1 || (c1e && c2));
    }
    bool operator==(uedge_t other) const
    {
	bool c1 = ((v[0] == other.v[0]) || (v[0] == other.v[1]));
	bool c2 = ((v[1] == other.v[0]) || (v[1] == other.v[1]));
	return (c1 && c2);
    }
    bool operator!=(uedge_t other) const
    {
	bool c1 = ((v[0] != other.v[0]) && (v[0] != other.v[1]));
	bool c2 = ((v[1] != other.v[0]) && (v[1] != other.v[1]));
	return (c1 || c2);
    }
};


struct uedge2d_t {
    long v2d[2];

    uedge2d_t()
    {
	v2d[0] = v2d[1] = -1;
    }

    uedge2d_t(struct edge2d_t e)
    {
	v2d[0] = (e.v2d[0] <= e.v2d[1]) ? e.v2d[0] : e.v2d[1];
	v2d[1] = (e.v2d[0] > e.v2d[1]) ? e.v2d[0] : e.v2d[1];
    }

    uedge2d_t(long i, long j)
    {
	v2d[0] = (i <= j) ? i : j;
	v2d[1] = (i > j) ? i : j;
    }

    void set(long i, long j)
    {
	v2d[0] = (i <= j) ? i : j;
	v2d[1] = (i > j) ? i : j;
    }

    bool operator<(uedge2d_t other) const
    {
	bool c1 = (v2d[0] < other.v2d[0]);
	bool c1e = (v2d[0] == other.v2d[0]);
	bool c2 = (v2d[1] < other.v2d[1]);
	return (c1 || (c1e && c2));
    }
    bool operator==(uedge2d_t other) const
    {
	bool c1 = ((v2d[0] == other.v2d[0]) || (v2d[0] == other.v2d[1]));
	bool c2 = ((v2d[1] == other.v2d[0]) || (v2d[1] == other.v2d[1]));
	return (c1 && c2);
    }
    bool operator!=(uedge2d_t other) const
    {
	bool c1 = ((v2d[0] != other.v2d[0]) && (v2d[0] != other.v2d[1]));
	bool c2 = ((v2d[1] != other.v2d[0]) && (v2d[1] != other.v2d[1]));
	return (c1 || c2);
    }
};

class cdt_mesh_t;

struct triangle_t {
    long v[3];
    size_t ind;
    cdt_mesh_t *m;

    long& i() { return v[0]; }
    const long& i() const { return v[0]; }
    long& j() {	return v[1]; }
    const long& j() const { return v[1]; }
    long& k() { return v[2]; }
    const long& k() const { return v[2]; }

    triangle_t(long i, long j, long k)
    {
	v[0] = i;
	v[1] = j;
	v[2] = k;
	m = NULL;
    }

    triangle_t(long i, long j, long k, size_t pind)
    {
	v[0] = i;
	v[1] = j;
	v[2] = k;
	ind = pind;
	m = NULL;
    }

    triangle_t()
    {
	v[0] = v[1] = v[2] = -1;
	ind = LONG_MAX;
	m = NULL;
    }

    triangle_t(const triangle_t &other)
    {
	v[0] = other.v[0];
	v[1] = other.v[1];
	v[2] = other.v[2];
	ind = other.ind;
	m = other.m;
    }

    triangle_t& operator=(const triangle_t &other) = default;

    std::set<uedge_t> uedges()
    {
	std::set<uedge_t> ue;
	ue.insert(uedge_t(v[0], v[1]));
	ue.insert(uedge_t(v[1], v[2]));
	ue.insert(uedge_t(v[2], v[0]));
	return ue;
    }

    bool operator<(triangle_t other) const
    {
	std::vector<long> vca, voa;
	for (int i = 0; i < 3; i++) {
	    vca.push_back(v[i]);
	    voa.push_back(other.v[i]);
	}
	std::sort(vca.begin(), vca.end());
	std::sort(voa.begin(), voa.end());
	bool c1 = (vca[0] < voa[0]);
	bool c1e = (vca[0] == voa[0]);
	bool c2 = (vca[1] < voa[1]);
	bool c2e = (vca[1] == voa[1]);
	bool c3 = (vca[2] < voa[2]);
	return (c1 || (c1e && c2) || (c1e && c2e && c3));
    }
    bool operator==(triangle_t other) const
    {
	if (m != other.m) {
	    return false;
	}
	bool c1 = ((v[0] == other.v[0]) || (v[0] == other.v[1]) || (v[0] == other.v[2]));
	bool c2 = ((v[1] == other.v[0]) || (v[1] == other.v[1]) || (v[1] == other.v[2]));
	bool c3 = ((v[2] == other.v[0]) || (v[2] == other.v[1]) || (v[2] == other.v[2]));
	return (c1 && c2 && c3);
    }
    bool operator!=(triangle_t other) const
    {
	if (m != other.m) {
	    return true;
	}
	bool c1 = ((v[0] != other.v[0]) && (v[0] != other.v[1]) && (v[0] != other.v[2]));
	bool c2 = ((v[1] != other.v[0]) && (v[1] != other.v[1]) && (v[1] != other.v[2]));
	bool c3 = ((v[2] != other.v[0]) && (v[2] != other.v[1]) && (v[2] != other.v[2]));
	return (c1 || c2 || c3);
    }

    double uedge_len(int ind);
    uedge_t uedge(int ind);

    double longest_edge_len();
    double shortest_edge_len();
    uedge_t longest_edge();
    uedge_t shortest_edge();

    std::set<triangle_t> split(uedge_t &ue, long split_pnt, bool flip);

    ON_3dPoint * vpnt(int i);
    char*ppnt(int i);
    void plot(const char *fname);
    double opp_edge_dist(int vind);
};


class cpolyedge_t;

class bedge_seg_t {
    public:
	bedge_seg_t() {
	    edge_ind = -1;
	    edge_type = -1;
	    cp_len = -1;
	    nc = NULL;
	    brep = NULL;
	    p_cdt = NULL;
	    tseg1 = NULL;
	    tseg2 = NULL;
	    edge_start = DBL_MAX;
	    edge_end = DBL_MAX;
	    e_start = NULL;
	    e_end = NULL;
	    e_root_start = NULL;
	    e_root_end = NULL;
	    tan_start = ON_3dVector::UnsetVector;
	    tan_end = ON_3dVector::UnsetVector;
	};

	bedge_seg_t(bedge_seg_t *other) {
	    edge_ind = other->edge_ind;
	    edge_type = other->edge_type;
	    cp_len = other->cp_len;
	    brep = other->brep;
	    p_cdt = other->p_cdt;
	    nc = other->nc;
	    tseg1 = NULL;
	    tseg2 = NULL;
	    edge_start = DBL_MAX;
	    edge_end = DBL_MAX;
	    e_start = NULL;
	    e_end = NULL;
	    e_root_start = NULL;
	    e_root_end = NULL;
	    tan_start = ON_3dVector::UnsetVector;
	    tan_end = ON_3dVector::UnsetVector;
	};

	void plot(const char *fname);

	int edge_ind;
	int edge_type;
	double cp_len;
	ON_NurbsCurve *nc;
	ON_Brep *brep;
	void *p_cdt;

	cpolyedge_t *tseg1;
	cpolyedge_t *tseg2;
	double edge_start;
	double edge_end;
	ON_3dPoint *e_start;
	ON_3dPoint *e_end;
	ON_3dPoint *e_root_start;
	ON_3dPoint *e_root_end;
	ON_3dVector tan_start;
	ON_3dVector tan_end;

	std::vector<std::pair<cdt_mesh_t *,uedge_t>> uedges();

};

class cpolygon_t;

class cpolyedge_t
{
    public:
	cpolygon_t *polygon;
	cpolyedge_t *prev;
	cpolyedge_t *next;

	cpolyedge_t() {
	    v2d[0] = -1;
	    v2d[1] = -1;
	    polygon = NULL;
	    prev = NULL;
	    next = NULL;
	    defines_spnt = false;
	    eseg = NULL;
	};

	long v2d[2];

	cpolyedge_t(edge2d_t &e){
	    v2d[0] = e.v2d[0];
	    v2d[1] = e.v2d[1];
	    polygon = NULL;
	    prev = NULL;
	    next = NULL;
	    defines_spnt = false;
	    eseg = NULL;
	};

	void plot3d(const char *fname);

	/* For those instance when we're working
	 * Brep edge polygons */
	int trim_ind;
	int loop_type; /* 0 == N/A, 1 == outer, 2 == inner */
	bool defines_spnt;
	ON_2dPoint spnt;
	ON_BoundingBox bb;
	double trim_start;
	double trim_end;
	int split_status;
	double v1_dist;
	double v2_dist;
	bedge_seg_t *eseg;

};

class cpolygon_t
{
    public:

	/* Perform a triangulation (populates ltris and tris) */
	bool cdt(triangulation_t ttype = TRI_CONSTRAINED_DELAUNAY);
	void cdt_inputs_print(const char *filename);

	/* Output triangles defined using the indexing from the p2o map (mapping polygon
	 * point indexing back to a caller-defined source array's indexing */
	std::set<triangle_t> tris;

	/* Triangles using the local polygon indexing */
	std::set<triangle_t> ltris;

	/* Map from points in polygon to the same points in the source data. If points
	 * are added directly without using add_point, the caller must manually ensure
	 * that this map has the correct information to go from indexing in the parent's
	 * original point array to the polygon pnts_2d point array.*/
	std::map<long, long> p2o;

	/* Map from points in the source date to the same points in the polygon.*/
	std::map<long, long> o2p;

	/* Polygon edge manipulation */
	cpolyedge_t *add_ordered_edge(const struct edge2d_t &e);
	void remove_ordered_edge(const struct edge2d_t &e);

	cpolyedge_t *add_edge(const struct uedge2d_t &e);
	void remove_edge(const struct uedge2d_t &e);
	std::set<cpolyedge_t *> replace_edges(std::set<uedge_t> &new_edges, std::set<uedge_t> &old_edges);

	/* Means to update the point array if we're incrementally building. orig_index should
	 * identify the same point in the parent's index, so cdt() knows what triangles to
	 * write into tris */
	long add_point(ON_2dPoint &on_2dp, long orig_index);

	/* Storage container for polygon data */
	std::set<cpolyedge_t *> poly;
	std::vector<std::pair<double, double> > pnts_2d;
	std::map<std::pair<double, double>, long> p2ind;

	/* Validity tests (closed also checks self_intersecting) */
	bool closed();
	bool self_intersecting();

	/* Return a libbg style polygon container */
	long bg_polygon(point2d_t **ppnts);

	/* Test if a point is inside the polygon.  Supplying flip=true will instead
	 * report if the point is outside the polygon.
	 * TODO - document what it does if it's ON the polygon...*/
	bool point_in_polygon(long v, bool flip);

	/* Process a set of points and filter out those in (or out, if flip is set) of the polygon */
	void rm_points_in_polygon(std::set<ON_2dPoint *> *pnts, bool flip);

	// Debugging routines
	void polygon_plot_in_plane(const char *filename);
	void polygon_plot(const char *filename);
	void print();

	/**********************************************************************/
	/* Internal state information and functionality related to loop growth,
	 * which is driven by cdt_mesh routines */
	/**********************************************************************/

	// Apply the point-in-polygon test to all uncontained points using the currently
	// defined polygon, moving any inside the loop into interior_points.  Returns true
	// if there are active uncontained points reported by the current polygon.
	bool update_uncontained();
	double ucv_angle(triangle_t &t);
	long shared_edge_cnt(triangle_t &t);
	long unshared_vertex(triangle_t &t);
	std::map<long, std::set<cpolyedge_t *>> v2pe;
	std::pair<long,long> shared_vertices(triangle_t &t);
	std::set<long> brep_edge_pnts;
	std::set<long> flipped_face;
	std::set<long> interior_points;
	std::set<long> target_verts;
	std::set<long> uncontained;
	std::set<long> used_verts; /* both interior and active points - for a quick check if a point is active */
	std::set<triangle_t> unusable_triangles;
	std::set<triangle_t> visited_triangles;
	std::set<uedge2d_t> active_edges;
	std::set<uedge2d_t> self_isect_edges;
	ON_Plane tplane;
	ON_Plane fit_plane;
	ON_3dVector pdir;
};


class cdt_mesh_t
{
public:

    cdt_mesh_t() {
	pnts.clear();
	p2ind.clear();
	normals.clear();
	n2ind.clear();
	nmap.clear();
	tris_tree.RemoveAll();

	uedges2tris.clear();
	seed_tris.clear();
	new_tris.clear();

	v2edges.clear();
	v2tris.clear();
	edges2tris.clear();

	omesh = NULL;
    };

    /* The 3D triangle set (defined by index values) and it associated points array.
     *
     * TODO - need to get perf on this - it's not trivially clear the RTree is
     * actually faster than the set yet just adding individual triangle boxes
     * to it - might need to nest RTrees with some upper limit on how many
     * triangles are in each cell before subdividing, to keep each local RTree
     * easier to handle? */
    //std::set<triangle_t> tris;
    std::vector<triangle_t> tris_vect;
    RTree<size_t, double, 3> tris_tree;
    std::vector<ON_3dPoint *> pnts;

    /* Setup / Repair */
    long add_point(ON_2dPoint &on_2dp);
    long add_point(ON_3dPoint *on_3dp);
    long add_normal(ON_3dPoint *on_3dn);
    bool repair();
    bool optimize(double deg = 10);
    bool optimize(std::set<triangle_t> &seeds);
    bool optimize(std::set<triangle_t> &seeds, ON_Plane &pplane);
    void reset();
    bool valid(int verbose);
    bool serialize(const char *fname);
    bool deserialize(const char *fname);

    /* Triangulation related functions and data. */
    bool cdt();
    std::vector<triangle_t> tris_2d;
    std::vector<std::pair<double, double> > m_pnts_2d;
    std::map<long, long> p2d3d;
    cpolygon_t outer_loop;
    std::map<int, cpolygon_t*> inner_loops;
    std::set<long> m_interior_pnts;
    bool initialize_interior_pnts(std::set<ON_2dPoint *>);

    /* Mesh data set accessors */
    std::set<uedge_t> get_boundary_edges();
    void boundary_edges_update();
    void update_problem_edges();
    std::vector<triangle_t> face_neighbors(const triangle_t &f);
    std::vector<triangle_t> vertex_face_neighbors(long vind);

    /* Tests */
    bool self_intersecting_mesh();
    bool brep_edge_pnt(long v);

    // Triangle geometry information
    ON_3dPoint tcenter(const triangle_t &t);
    ON_3dVector bnorm(const triangle_t &t);
    ON_3dVector tnorm(const triangle_t &t);
    ON_Plane tplane(const triangle_t &t);
    ON_Plane bplane(const triangle_t &t);
    std::set<uedge_t> uedges(const triangle_t &t);
    std::set<uedge_t> b_uedges(const triangle_t &t);

    bool face_edge_tri(const triangle_t &t);

    // Find close triangles
    std::set<size_t> tris_search(ON_BoundingBox &bb);

    bool tri_active(size_t tind);

    ON_BoundingBox& bbox();
    ON_BoundingBox mbb;
    ON_BoundingBox tri_bbox(size_t tind);

    // Find the edge of the triangle that is closest to the
    // specified point
    uedge_t closest_uedge(const triangle_t &t, ON_3dPoint &p);

    // Find the edge of the triangle that is closest to the
    // specified point and not a brep face edge
    uedge_t closest_interior_uedge(const triangle_t &t, ON_3dPoint &p);

    // Find the distance to the closest point on a uedge
    double uedge_dist(uedge_t &ue, ON_3dPoint &p);

    // Length sorted (longest to shortest) array of uedges in set.
    std::vector<uedge_t> sorted_uedges_l_to_s(std::set<uedge_t> &uedges);

    // Given an unordered edge, return a polygon defined by the
    // two triangles associated with that edge
    cpolygon_t * uedge_polygon(uedge_t &ue);

    // Given a test point, calculate the closest point on the
    // Brep face's surface to that point
    bool closest_surf_pnt(ON_3dPoint &s_p, ON_3dVector &s_norm, ON_3dPoint *p, double tol = -1);

    // Report if the brep surface associated with this face is planar
    bool planar(double ptol = ON_ZERO_TOLERANCE);

    // Plot3 generation routines for debugging
    void boundary_edges_plot(const char *filename);
    void face_neighbors_plot(const triangle_t &f, const char *filename);
    void vertex_face_neighbors_plot(long vind, const char *filename);
    void interior_incorrect_normals_plot(const char *filename);
    void tris_set_plot(std::set<triangle_t> &tset, const char *filename);
    void tris_set_plot(std::set<size_t> &tset, const char *filename);
    void tris_vect_plot(std::vector<triangle_t> &tvect, const char *filename);
    void ctris_vect_plot(std::vector<struct ctriangle_t> &tvect, const char *filename);
    void tris_plot(const char *filename);
    void tri_plot(const triangle_t &tri, const char *filename);
    void tri_plot(long ind, const char *filename);
    void plot_tri(const triangle_t &t, struct bu_color *buc, FILE *plot, int r, int g, int b);
    void tris_rtree_plot(const char *fname);

    void edge_set_plot(std::set<uedge_t> &eset, const char *filename);
    void edge_set_plot(std::set<bedge_seg_t *> &eset, const char *filename);

    void edge_set_plot(std::set<uedge_t> &eset, const char *filename, int r, int g, int b);
    void edge_set_plot(std::set<bedge_seg_t *> &eset, const char *filename, int r, int g, int b);

    double tri_pnt_r(long tri_ind);

    void tris_vect_plot_2d(std::vector<triangle_t> &tset, const char *filename);
    void tris_plot_2d(const char *filename);

    void polygon_plot_2d(cpolygon_t *polygon, const char *filename);
    void polygon_plot_3d(cpolygon_t *polygon, const char *filename);
    void polygon_print_3d(cpolygon_t *polygon);

    void cdt_inputs_plot(const char *filename);
    void cdt_inputs_print(const char *filename);


    int f_id;
    bool has_singularities;

    /* Data containers */
    std::map<ON_3dPoint *, long> p2ind;
    std::vector<ON_3dPoint *> normals;
    std::map<ON_3dPoint *, long> n2ind;
    std::map<long, long> nmap;
    std::map<uedge_t, std::set<size_t>> uedges2tris;
    std::map<long, std::set<edge_t>> v2edges;
    std::map<long, std::set<size_t>> v2tris;

    // cdt_mesh index versions of Brep data
    std::set<uedge_t> brep_edges;
    std::map<uedge_t, bedge_seg_t *> ue2b_map;
    std::set<long> ep; // Brep edge point vertex indices
    std::set<long> sv; // Singularity vertex indices
    bool m_bRev;

    // Mesh manipulation functions
    bool tri_add(triangle_t &atris);
    void tri_remove(triangle_t &etris);

    ON_Brep *brep;
    const char *name;
    void *p_cdt;
    //omesh_t *omesh;
    void *omesh;

    // Characterize triangle's relationship to polygon.  Returned
    // edges and vertices are in triangle (3D) point indices rather than
    // polygon (2D) vertex indices - use o2p mapping to get polygon indices.
    long tri_process(cpolygon_t *polygon, std::set<uedge_t> *ne, std::set<uedge_t> *se, long *nv, triangle_t &t);

    bool process_seed_tri(triangle_t &seed, bool repair, double deg, ON_Plane *pplane);

    /* Working polygon sweeping seed triangle set */
    std::set<triangle_t> seed_tris;

    /* New triangles introduced into the mesh by a repair or optimization pass */
    std::set<triangle_t> new_tris;

    ON_Plane best_fit_plane(std::set<triangle_t> &ts);
    double max_tri_angle(ON_Plane &plane, std::set<triangle_t> &ts);

private:
    /* Data containers */
    std::map<edge_t, size_t> edges2tris;

    // For situations where we need to process using Brep data
    std::set<ON_3dPoint *> *edge_pnts;
    std::set<std::pair<ON_3dPoint *, ON_3dPoint *>> *b_edges;
    std::set<ON_3dPoint *> *singularities;

    // Boundary edge information
    std::set<uedge_t> boundary_edges;
    bool boundary_edges_stale;
    std::set<uedge_t> problem_edges;
    edge_t find_boundary_oriented_edge(uedge_t &ue);


    bool bounding_box_stale;

    // Submesh building
    std::vector<triangle_t> singularity_triangles();
    void remove_dangling_tris();
    std::vector<triangle_t> problem_edge_tris();
    bool tri_problem_edges(triangle_t &t);
    std::vector<triangle_t> interior_incorrect_normals();
    double max_angle_delta(triangle_t &seed, std::vector<triangle_t> &s_tris);

    // Plotting utility functions
    void plot_edge(const uedge_t &ue, FILE* plot);
    void plot_edge(const bedge_seg_t *s, FILE* plot);
 
    void plot_tri_2d(const triangle_t &t, struct bu_color *buc, FILE *plot);
    void polyplot_2d(cpolygon_t *polygon, FILE* plot_file);


    // Repair functionality using cpolygon

    // Use the seed triangle and build an initial loop.  If repair is set, find a nearby triangle that is
    // valid and use that instead.
    cpolygon_t *build_initial_loop(triangle_t &seed, bool repair, ON_Plane *pplane);

    std::vector<struct ctriangle_t> polygon_tris(cpolygon_t *polygon, double angle, bool brep_norm, int initial);

    // To grow only until all interior points are within the polygon, supply true
    // for stop_on_contained.  Otherwise, grow_loop will follow the triangles out
    // until the Brep normals of the triangles are beyond the deg limit.  Note
    // that triangles which would cause a self-intersecting polygon will be
    // rejected, even if they satisfy deg.
    int grow_loop(cpolygon_t *polygon, double deg, bool stop_on_contained, triangle_t &target, bool reproject);
    bool grow_loop_failure_ok;

    bool best_fit_plane_reproject(cpolygon_t *polygon);
    void best_fit_plane_plot(point_t *center, vect_t *norm, const char *fname);


    bool oriented_polycdt(cpolygon_t *polygon, bool reproject);

    bool optimize_process(double deg, ON_Plane *pplane);

};

#if 0
class overt_t {
    public:

        overt_t(omesh_t *om, long p)
        {
            omesh = om;
            p_id = p;
	    init = false;
        }

	// Update minimum edge length and bounding box information
        void update();

	// For situations where we don't yet have associated edges (i.e. a new
	// point is coming in based on an intruding mesh's vertex) we need to
	// supply external bounding box info
        void update(ON_BoundingBox *bbox);

	// Adjust this vertex to be as close as the surface allows to a target
	// point.  dtol is the allowable distance for the closest point to be
	// from the target before reporting failure.
	int adjust(ON_3dPoint &target_point, double dtol);

	// Determine if this vertex is on a brep face edge
        bool edge_vert();

	// Report the length of the shortest edge using this vertex
        double min_len() { return v_min_edge_len; }

	// Return the ON_3dPoint associated with this vertex
        ON_3dPoint vpnt();

	// Print 3D axis at the vertex point location
        void plot(FILE *plot);



	// Index of associated point in the omesh's fmesh container
	long p_id;

	// Bounding box centered on 3D vert, size based on smallest associated
	// edge length or distance to opposite triangle edge.
        ON_BoundingBox bb;

	// Associated omesh
        omesh_t *omesh;

	// If this vertex has been aligned with a vertex in another mesh, the
	// other vertex and mesh are stored here.  The idea is for this info
	// to replace the need for rev_pt_t
	std::map<omesh_t *, overt_t *> aligned;
	std::map<overt_t *, ON_3dPoint> cpoints;
	std::map<overt_t *, ON_3dVector> cnorms;
	std::map<overt_t *, int> alevel;

    private:
	// If vertices are being moved, the impact is not local to a single
	// vertex - need updating in the neighborhood
        void update_ring();

	// Smallest edge length associated with this vertex
	double v_min_edge_len;

	// Set if vertex has been previously initialized - guides what the
	// update step must do.
	bool init;
};

class revt_pt_t {
    public:
	ON_3dPoint spnt;
	ON_3dVector sn;
	overt_t *ov;
};

class omesh_t
{
    public:
        omesh_t(cdt_mesh_t *m)
        {
            fmesh = m;
	    // Walk the fmesh's rtree holding the active triangles to get all
	    // vertices active in the face
	    std::set<long> averts;
	    RTree<size_t, double, 3>::Iterator tree_it;
	    size_t t_ind;
	    triangle_t tri;
	    fmesh->tris_tree.GetFirst(tree_it);
	    while (!tree_it.IsNull()) {
		t_ind = *tree_it;
		tri = fmesh->tris_vect[t_ind];
		averts.insert(tri.v[0]);
		averts.insert(tri.v[1]);
		averts.insert(tri.v[2]);
		++tree_it;
	    }

	    std::set<long>::iterator a_it;
	    for (a_it = averts.begin(); a_it != averts.end(); a_it++) {
		overts[*a_it] = new overt_t(this, *a_it);
		overts[*a_it]->update();
	    }
        };



	// Given another omesh, find which vertex pairs have overlapping
	// bounding boxes
	std::set<std::pair<long, long>> vert_ovlps(omesh_t *other);

        // Add an fmesh vertex to the overts array and tree.
        overt_t *vert_add(long, ON_BoundingBox *bb = NULL);

        // Find the closest uedge in the mesh to a point
        uedge_t closest_uedge(ON_3dPoint &p);
        // Find closest (non-face-boundary) edges
        std::set<uedge_t> interior_uedges_search(ON_BoundingBox &bb);

        // Find close triangles
        std::set<size_t> tris_search(ON_BoundingBox &bb);

        // Find close vertices
        std::set<overt_t *> vert_search(ON_BoundingBox &bb);
        overt_t * vert_closest(double *vdist, overt_t *v);
        overt_t * vert_closest(double *vdist, ON_3dPoint &opnt);

	// Find closest point on mesh
        double closest_pt(ON_3dPoint &p, ON_3dVector &n, const ON_3dPoint &op);

	// Find closest point on any nearby mesh face in the
	// specified brep.  This isn't guaranteed to return
	// the closest point on the brep mesh - it will check
	// to see if the test point p is close to a face per
	// it's bounding box, and if it is look for the closest
	// point.  If the point isn't within any bbox, it will
	// return the closest point for the current mesh.
	//
	// This is done to save the work of searching out the
	// closest point for every face all the time, which
	// for the current application isn't what we need -
	// we just need to know if there's a very close point
	// to a nearby triangle that is closer (i.e. changes
	// the triangle status) for the local problem.
	bool closest_nearby_mesh_point(ON_3dPoint &s_p, ON_3dVector &s_n, ON_3dPoint *p, struct ON_Brep_CDT_State *s_cdt);

        bool validate_vtree();


        void plot(const char *fname);
        void plot();
        void plot_vtree(const char *fname);


	// The parent cdt_mesh container holding the original mesh data
	cdt_mesh_t *fmesh;

        // The fmesh pnts array may have inactive vertices - we only want the
        // active verts for this portion of the processing, so we maintain our
	// own map of active overlap vertices.
        std::map<long, overt_t *> overts;

	// Use an rtree for fast localized lookup
        RTree<long, double, 3> vtree;

	// Triangles that intersect another mesh
	std::map<size_t, std::set<std::pair<omesh_t *, size_t>>> itris;
        std::set<size_t> intruding_tris;

        // Points from other meshes potentially needing refinement in this mesh
        std::map<overt_t *, std::set<long>> ivert_ref_cnts;
        // Points from other meshes needing corresponding vertices in this mesh
	std::map<uedge_t, std::vector<revt_pt_t>> edge_sets;

	// Set of all active fmesh pairs
	std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> *check_pairs;

	// Container that holds the current (global) set of edge vertices
	std::map<bedge_seg_t *, std::set<overt_t *>> *edge_verts;

        // Points from this mesh inducing refinement in other meshes, and
        // triangles reported by tri_isect as intersecting from this mesh
        std::map<overt_t *, std::set<long>> intruding_overts;

	std::string sname();
};

class ovlp_grp {
    public:
        ovlp_grp(omesh_t *m1, omesh_t *m2) {
            om1 = m1;
            om2 = m2;
        }
        omesh_t *om1;
	std::set<triangle_t> tris1;

	omesh_t *om2;
        std::set<triangle_t> tris2;

        void add_tri(triangle_t &t) {
            if (om1->fmesh == t.m) {
                tris1.insert(t);
		return;
	    }
            if (om2->fmesh == t.m) {
                tris2.insert(t);
                return;
            }
        }

	bool overlapping();
	bool optimize();
	bool pairs_realign();

	void plot(const char *fname, int ind);
	void plot(const char *fname);

	std::set<uedge_t> interior_uedges(int ind);
	bool split_interior_uedges();

	// -1 = invalid, 1 = valid, 2 = cleared
	int state;

    private:
	// Find the best fit plane of all 3D points from all the vertices in play from both
	// meshes or (if the individual planes are not close to parallel) the best fit
	// plane of the current mesh's inputs.
	bool fit_plane();
	ON_Plane fp1;
	ON_Plane fp2;
	bool separate_planes;

	// Confirm that all triangles in the group are still in the fmeshes, and
	// remove any that are not from the tris sets.
        bool validate();

	bool aligned_verts_check();
	std::map<long, long> p2p;
};

std::set<omesh_t *>
active_omeshes(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs);
std::set<omesh_t *>
refinement_omeshes(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs);
std::set<omesh_t *>
itris_omeshes(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs);
void
plot_active_omeshes(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs);

bool
orient_tri(cdt_mesh_t &fmesh, triangle_t &t);

int
bedge_split_near_verts(
	std::set<overt_t *> *nverts,
	std::map<bedge_seg_t *, std::set<overt_t *>> &edge_verts
	);


overt_t *
nearby_vert(overt_t *v, omesh_t *other_m, double dist);

int
add_refinement_vert(overt_t *v, omesh_t *other_m, int level);


int
omesh_interior_edge_verts(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs);

std::map<bedge_seg_t *, std::set<overt_t *>>
find_edge_verts(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> check_pairs);

bool
closest_mesh_point(ON_3dPoint &s_p, std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> *check_pairs, ON_3dPoint *p);

int
ovlp_split_edge(overt_t **nv1, overt_t **nv2,
	std::set<bedge_seg_t *> *nsegs,	bedge_seg_t *eseg, double t);

int
ovlp_split_interior_edge(overt_t **nv, std::set<long> &ntris, omesh_t *om, uedge_t &ue);


void
resolve_ovlp_grps(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs, double lthresh);

size_t
omesh_refinement_pnts(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> check_pairs, int level);

size_t
omesh_ovlps(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> check_pairs, int mode);

#endif

int tri_isect(triangle_t &t1, triangle_t &t2, int mode);
int tri_nearedge_refine(triangle_t &t1, triangle_t &t2, int level);
bool
check_faces_validity(std::set<std::pair<cdt_mesh_t *, cdt_mesh_t *>> &check_pairs);
#endif /* __cdt_mesh_h__ */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
