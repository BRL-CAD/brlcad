/*                      C D T _ M E S H . H
 * BRL-CAD
 *
 * Copyright (c) 2019 United States Government as represented by
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
#include "poly2tri/poly2tri.h"
#include "opennurbs.h"
#include "bu/color.h"

extern "C" {
    struct ctriangle_t {
	long v[3];
	bool all_bedge;
	bool isect_edge;
	bool uses_uncontained;
	bool contains_uncontained;
	double angle_to_nearest_uncontained;
    };
}

namespace cdt_mesh
{

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

struct triangle_t {
    long v[3];

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
    }

    triangle_t()
    {
	v[0] = v[1] = v[2] = -1;
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
	bool c1 = ((v[0] == other.v[0]) || (v[0] == other.v[1]) || (v[0] == other.v[2]));
	bool c2 = ((v[1] == other.v[0]) || (v[1] == other.v[1]) || (v[1] == other.v[2]));
	bool c3 = ((v[2] == other.v[0]) || (v[2] == other.v[1]) || (v[2] == other.v[2]));
	return (c1 && c2 && c3);
    }
    bool operator!=(triangle_t other) const
    {
	bool c1 = ((v[0] != other.v[0]) && (v[0] != other.v[1]) && (v[0] != other.v[2]));
	bool c2 = ((v[1] != other.v[0]) && (v[1] != other.v[1]) && (v[1] != other.v[2]));
	bool c3 = ((v[2] != other.v[0]) && (v[2] != other.v[1]) && (v[2] != other.v[2]));
	return (c1 || c2 || c3);
    }

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
	    nc = other->nc;
	    tseg1 = NULL;
	    tseg2 = NULL;
	    edge_start = DBL_MAX;
	    edge_end = DBL_MAX;
	    e_start = NULL;
	    e_end = NULL;
	    e_root_start = NULL;
	    e_root_end = NULL;
	    // TODO - see if it makes sense to initialize these from parents and override or not
	    nlc_start = NULL;
	    nlc_end = NULL;
	    tan_start = ON_3dVector::UnsetVector;
	    tan_end = ON_3dVector::UnsetVector;
	};

	void plot(const char *fname);

	int edge_ind;
	int edge_type;
	double cp_len;
	ON_NurbsCurve *nc;
	ON_Brep *brep;

	/* These are pointers to points (if any) shared beween this edge and
	 * connected non-linear edges. Splitting decisions that involve one
	 * of these points will also need to consider the dimensions of the
	 * mated edge. */
	ON_3dPoint *nlc_start;
	ON_3dPoint *nlc_end;

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
};

class cpolygon_t;

class cpolyedge_t
{
    public:
	cpolygon_t *polygon;
	cpolyedge_t *prev;
	cpolyedge_t *next;

	cpolyedge_t() {
	    v[0] = -1;
	    v[1] = -1;
	    polygon = NULL;
	    prev = NULL;
	    next = NULL;
	};

	long v[2];

	cpolyedge_t(edge_t &e){
	    v[0] = e.v[0];
	    v[1] = e.v[1];
	    polygon = NULL;
	    prev = NULL;
	    next = NULL;
	};

	void plot3d(const char *fname);

	/* For those instance when we're working
	 * Brep edge polygons */
	int trim_ind;
	double trim_start;
	double trim_end;
	bedge_seg_t *eseg;

};

class cdt_mesh_t;

class cpolygon_t
{
    public:

	/* Perform a triangulation (populates ltris and tris) */
	bool cdt();

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

	/* Polygon edge manipulation */
	cpolyedge_t *add_ordered_edge(const struct edge_t &e);
	void remove_ordered_edge(const struct edge_t &e);

	cpolyedge_t *add_edge(const struct uedge_t &e);
	void remove_edge(const struct uedge_t &e);
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
	std::set<uedge_t> active_edges;
	std::set<uedge_t> self_isect_edges;
	ON_Plane tplane;
	ON_Plane fit_plane;
	ON_3dVector pdir;
};


class cdt_mesh_t
{
public:

    /* The 3D triangle set (defined by index values) and it associated points array */
    std::set<triangle_t> tris;
    std::vector<ON_3dPoint *> pnts;

    /* Setup / Repair */
    long add_point(ON_2dPoint &on_2dp);
    long add_point(ON_3dPoint *on_3dp);
    long add_normal(ON_3dPoint *on_3dn);
    void build(std::set<p2t::Triangle *> *cdttri, std::map<p2t::Point *, ON_3dPoint *> *pointmap);
    void set_brep_data(
	    bool brev,
	    std::set<ON_3dPoint *> *e,
	    std::set<std::pair<ON_3dPoint *,ON_3dPoint *>> *original_b_edges,
	    std::set<ON_3dPoint *> *s,
	    std::map<ON_3dPoint *, ON_3dPoint *> *n
	    );
    bool repair();
    void reset();
    bool valid(int verbose);
    bool serialize(const char *fname);
    bool deserialize(const char *fname);

    /* Triangulation related functions and data. */
    bool cdt();
    std::set<triangle_t> tris_2d;
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
    std::set<uedge_t> uedges(const triangle_t &t);

    // Plot3 generation routines for debugging
    void boundary_edges_plot(const char *filename);
    void face_neighbors_plot(const triangle_t &f, const char *filename);
    void vertex_face_neighbors_plot(long vind, const char *filename);
    void interior_incorrect_normals_plot(const char *filename);
    void tris_set_plot(std::set<triangle_t> &tset, const char *filename);
    void tris_vect_plot(std::vector<triangle_t> &tvect, const char *filename);
    void ctris_vect_plot(std::vector<struct ctriangle_t> &tvect, const char *filename);
    void tris_plot(const char *filename);
    void tri_plot(const triangle_t &tri, const char *filename);

    void tris_set_plot_2d(std::set<triangle_t> &tset, const char *filename);
    void tris_plot_2d(const char *filename);

    void polygon_plot_2d(cpolygon_t *polygon, const char *filename);
    void polygon_plot_3d(cpolygon_t *polygon, const char *filename);
    void polygon_print_3d(cpolygon_t *polygon);


    int f_id;

    /* Data containers */
    std::map<ON_3dPoint *, long> p2ind;
    std::vector<ON_3dPoint *> normals;
    std::map<ON_3dPoint *, long> n2ind;
    std::map<long, long> nmap;
    std::map<uedge_t, std::set<triangle_t>> uedges2tris;

    // cdt_mesh index versions of Brep data
    std::set<uedge_t> brep_edges;
    std::set<long> ep; // Brep edge point vertex indices
    std::set<long> sv; // Singularity vertex indices
    bool m_bRev;

private:
    /* Data containers */
    std::map<long, std::set<edge_t>> v2edges;
    std::map<long, std::set<triangle_t>> v2tris;
    std::map<edge_t, triangle_t> edges2tris;

    // For situations where we need to process using Brep data
    std::set<ON_3dPoint *> *edge_pnts;
    std::set<std::pair<ON_3dPoint *, ON_3dPoint *>> *b_edges;
    std::set<ON_3dPoint *> *singularities;

    // Boundary edge information
    std::set<uedge_t> boundary_edges;
    bool boundary_edges_stale;
    std::set<uedge_t> problem_edges;
    edge_t find_boundary_oriented_edge(uedge_t &ue);

    // Submesh building
    std::vector<triangle_t> singularity_triangles();
    std::vector<triangle_t> problem_edge_tris();
    bool tri_problem_edges(triangle_t &t);
    std::vector<triangle_t> interior_incorrect_normals();
    double max_angle_delta(triangle_t &seed, std::vector<triangle_t> &s_tris);
    bool process_seed_tri(triangle_t &seed, bool repair, double deg);

    // Mesh manipulation functions
    bool tri_add(triangle_t &atris);
    void tri_remove(triangle_t &etris);

    // Plotting utility functions
    void plot_tri(const triangle_t &t, struct bu_color *buc, FILE *plot, int r, int g, int b);
    void plot_uedge(struct uedge_t &ue, FILE* plot_file);
    void plot_tri_2d(const triangle_t &t, struct bu_color *buc, FILE *plot);


    // Repair functionality using cpolygon

    // Use the seed triangle and build an initial loop.  If repair is set, find a nearby triangle that is
    // valid and use that instead.
    cpolygon_t *build_initial_loop(triangle_t &seed, bool repair);

    std::vector<struct ctriangle_t> polygon_tris(cpolygon_t *polygon, double angle, bool brep_norm, int initial);

    // To grow only until all interior points are within the polygon, supply true
    // for stop_on_contained.  Otherwise, grow_loop will follow the triangles out
    // until the Brep normals of the triangles are beyond the deg limit.  Note
    // that triangles which would cause a self-intersecting polygon will be
    // rejected, even if they satisfy deg.
    long grow_loop(cpolygon_t *polygon, double deg, bool stop_on_contained, triangle_t &target);

    bool best_fit_plane_reproject(cpolygon_t *polygon);
    void best_fit_plane_plot(point_t *center, vect_t *norm, const char *fname);

    long tri_process(cpolygon_t *polygon, std::set<uedge_t> *ne, std::set<uedge_t> *se, long *nv, triangle_t &t);

    bool oriented_polycdt(cpolygon_t *polygon);

    /* Working polygon sweeping seed triangle set */
    std::set<triangle_t> seed_tris;

};

}

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
