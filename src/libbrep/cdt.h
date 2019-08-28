/*                        C D T . H
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
/** @file cdt.h
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"

#include <vector>
#include <list>
#include <map>
#include <stack>
#include <iostream>
#include <algorithm>
#include <set>
#include <utility>

#include "poly2tri/poly2tri.h"
#include "RTree.h"

#include "assert.h"

#include "vmath.h"

#include "bu/color.h"
#include "bu/cv.h"
#include "bu/opt.h"
#include "bu/time.h"
#include "bn/mat.h"
#include "bn/plane.h"
#include "bn/plot3.h"
#include "bn/tol.h"
#include "bn/vlist.h"
#include "bg/polygon.h"
#include "bg/trimesh.h"
#include "brep/defines.h"
#include "brep/cdt.h"
#include "brep/pullback.h"
#include "brep/util.h"

#include "./trimesh.h"
#include "./cdt_mesh.h"

#define BREP_PLANAR_TOL 0.05

/***************************************************/

#define BREP_CDT_FAILED -3
#define BREP_CDT_NON_SOLID -2
#define BREP_CDT_UNTESSELLATED -1
#define BREP_CDT_SOLID 0

/* Note - this tolerance value is based on the default
 * value from the GED 'tol' command */
#define BREP_CDT_DEFAULT_TOL_REL 0.01

/* this is a debugging structure - it holds origin information for
 * a point added to the CDT state */
struct cdt_audit_info {
    int face_index;
    int vert_index;
    int trim_index;
    int edge_index;
    ON_2dPoint surf_uv;
    ON_3dPoint vert_pnt; // For auditing normals
};

/* Halfedge info for local mesh subset building */
struct trimesh_info {
    std::vector<trimesh::edge_t> edges;
    trimesh::trimesh_t mesh;
    std::set<p2t::Point *> uniq_p2d;
    std::map<p2t::Point *, trimesh::index_t> p2dind;
    std::map<trimesh::index_t, p2t::Point *> ind2p2d;
    std::map<p2t::Triangle *, trimesh::index_t> t2ind;
    std::vector<trimesh::triangle_t> triangles;
};

struct brep_cdt_tol {
    fastf_t min_dist;
    fastf_t max_dist;
    fastf_t within_dist;
};
#define BREP_CDT_TOL_ZERO {0.0, 0.0, 0.0}

struct BrepEdgeSegment;
struct BrepEdgeSegment {
    struct ON_Brep_CDT_State *s_cdt;
    ON_BrepEdge *edge;
    ON_NurbsCurve *nc;
    const ON_BrepTrim *trim1;
    const ON_BrepTrim *trim2;
    BrepTrimPoint *sbtp1;
    BrepTrimPoint *ebtp1;
    BrepTrimPoint *sbtp2;
    BrepTrimPoint *ebtp2;
    std::map<double, BrepTrimPoint *> *trim1_param_points;
    std::map<double, BrepTrimPoint *> *trim2_param_points;
    std::set<struct BrepEdgeSegment *> children;
    struct BrepEdgeSegment *parent;
    struct brep_cdt_tol cdt_tol;
    /* Average length of the child segments below the current segment */
    double avg_seg_len;
    /* Distance used when deciding to refine edges */
    double loop_min_dist;
};

struct ON_Brep_CDT_State;

struct ON_Brep_CDT_Face_State {
    struct ON_Brep_CDT_State *s_cdt;

    struct bu_vls face_root;

    cdt_mesh::cdt_mesh_t fmesh;
    std::map<ON_3dPoint *, ON_3dPoint *> *on3_to_norm_map;

    int has_singular_trims;
    std::set<ON_3dPoint *> *singularities;

    int ind;
    std::map<ON_2dPoint *, struct cdt_audit_info *> *pnt2d_audit_info;

    /* 3D data specific to this face (i.e. not shared at an edge) */
    std::vector<ON_2dPoint *> *w2dpnts;
    std::vector<ON_3dPoint *> *w3dpnts;
    std::vector<ON_3dPoint *> *w3dnorms;

    /* loop points */
    ON_SimpleArray<BrepTrimPoint> *face_loop_points;
    std::map<p2t::Point *, int> *p2t_trim_ind;
    std::set<ON_2dPoint *> *on_surf_points;
    ON_RTree *rt_trims;
    ON_RTree *rt_trims_3d;

    /* singular trim info */
    std::map<int,ON_3dPoint *> *strim_pnts;
    std::map<int,ON_3dPoint *> *strim_norms;

    /* mappings */
    std::map<p2t::Point *, ON_2dPoint *> *p2t_to_on2_map;
    std::map<p2t::Point *, ON_3dPoint *> *p2t_to_on3_map;
    std::map<p2t::Point *, ON_3dPoint *> *p2t_to_on3_norm_map;
    std::map<ON_3dPoint *, std::set<p2t::Point *>> *on3_to_tri_map;

    /* Poly2Tri information */
    std::set<p2t::Triangle *> *tris;
    std::set<p2t::Triangle *> *degen_tris;
    std::set<p2t::Point *> *degen_pnts;
    std::set<p2t::Point *> *ext_degen_pnts;
};


struct ON_Brep_CDT_State {

    int status;
    ON_Brep *orig_brep;
    ON_Brep *brep;

    struct bu_vls brep_root;

    /* Tolerances */
    struct bg_tess_tol tol;
    fastf_t absmax;
    fastf_t absmin;
    fastf_t cos_within_ang;

    /* 3D data */
    std::vector<ON_3dPoint *> *w3dpnts;
    std::vector<ON_3dPoint *> *w3dnorms;

    /* Vertices */
    std::map<int, ON_3dPoint *> *vert_pnts;
    std::map<int, ON_3dPoint *> *vert_avg_norms;
    std::map<ON_3dPoint *, ON_3dPoint *> *singular_vert_to_norms;

    /* Edges */
    std::set<ON_3dPoint *> *edge_pnts;
    std::set<std::pair<ON_3dPoint *, ON_3dPoint *>> fedges;
    std::map<int, double> *min_edge_seg_len;
    std::map<int, double> *max_edge_seg_len;
    std::map<ON_3dPoint *, std::set<BrepTrimPoint *>> *on_brep_edge_pnts;
    std::map<int, struct BrepEdgeSegment *> *etrees;
    std::map<int, RTree<void *, double, 3>> edge_segs_3d;
    std::map<int, RTree<void *, double, 2>> trim_segs;
    std::map<int, std::set<cdt_mesh::bedge_seg_t *>> e2polysegs;
    std::map<ON_3dPoint *, double> v_min_seg_len;
    std::map<int, double> l_median_len;

    /* Audit data */
    std::map<int, ON_3dPoint *> *bot_pnt_to_on_pnt;
    std::map<ON_3dPoint *, struct cdt_audit_info *> *pnt_audit_info;

    /* Face specific data */
    std::map<int, struct ON_Brep_CDT_Face_State *> *faces;

    std::map<int, cdt_mesh::cdt_mesh_t> fmeshes;


    /* Mesh data */
    std::map<p2t::Triangle*, int> *tri_brep_face;
};

struct cdt_surf_info {
    struct ON_Brep_CDT_State *s_cdt;
    const ON_Surface *s;
    const ON_BrepFace *f;
    ON_RTree *rt_trims;
    ON_RTree *rt_trims_3d;
    std::map<int,ON_3dPoint *> *strim_pnts;
    std::map<int,ON_3dPoint *> *strim_norms;
    double u1, u2, v1, v2;
    fastf_t ulen;
    fastf_t u_lower_3dlen;
    fastf_t u_mid_3dlen;
    fastf_t u_upper_3dlen;
    fastf_t vlen;
    fastf_t v_lower_3dlen;
    fastf_t v_mid_3dlen;
    fastf_t v_upper_3dlen;
    fastf_t min_edge;
    fastf_t max_edge;
    fastf_t min_dist;
    fastf_t within_dist;
    fastf_t cos_within_ang;
    std::set<ON_BoundingBox *> leaf_bboxes;
};

void
PerformClosedSurfaceChecks(
	struct ON_Brep_CDT_State *s_cdt,
	const ON_Surface *s,
	const ON_BrepFace &face,
	ON_SimpleArray<BrepTrimPoint> *brep_loop_points,
	double same_point_tolerance,
	bool verbose);

void
Get_Edge_Points(struct ON_Brep_CDT_State *s_cdt);

void
Refine_Edges(struct ON_Brep_CDT_State *s_cdt);

void
getSurfacePoints(struct ON_Brep_CDT_Face_State *f);


struct ON_Brep_CDT_Face_State *
ON_Brep_CDT_Face_Create(struct ON_Brep_CDT_State *s_cdt, int ind);
void
ON_Brep_CDT_Face_Reset(struct ON_Brep_CDT_Face_State *fcdt, int full_surface_sample);
void
ON_Brep_CDT_Face_Destroy(struct ON_Brep_CDT_Face_State *fcdt);

void plot_rtree_2d(ON_RTree *rtree, const char *filename);
void plot_rtree_2d2(RTree<void *, double, 2> &rtree, const char *filename);
void
plot_rtree_3d(RTree<void *, double, 3> &rtree, const char *filename);
void
plot_bbox(point_t m_min, point_t m_max, const char *filename);
void
plot_on_bbox(ON_BoundingBox &bb, const char *filename);
void
plot_polyline(std::vector<p2t::Point *> *pnts, const char *filename);
void
plot_tri(p2t::Triangle *t, const char *filename);
void
plot_tri_2d(p2t::Triangle *t, struct bu_color *c, FILE *plot);
void
plot_tri_3d(p2t::Triangle *t, std::map<p2t::Point *, ON_3dPoint *> *pointmap, struct bu_color *c, FILE *c_plot);
void
plot_trimesh_2d(std::vector<trimesh::triangle_t> &farray, const char *filename);


struct cdt_audit_info *
cdt_ainfo(int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d, double px, double py, double pz);

void
CDT_Add2DPnt(struct ON_Brep_CDT_Face_State *f, ON_2dPoint *p, int fid, int vid, int tid, int eid, fastf_t tparam);
void
CDT_Add3DPnt(struct ON_Brep_CDT_State *s, ON_3dPoint *p, int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d);
void
CDT_Add3DNorm(struct ON_Brep_CDT_State *s, ON_3dPoint *norm, ON_3dPoint *vert, int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d);

ON_3dVector p2tTri_Normal(p2t::Triangle *t, std::map<p2t::Point *, ON_3dPoint *> *pointmap);
ON_3dVector p2tTri_Brep_Normal(struct ON_Brep_CDT_Face_State *f, p2t::Triangle *t);

void cdt_tol_global_calc(struct ON_Brep_CDT_State *s);
void
CDT_Tol_Set(struct brep_cdt_tol *cdt, double dist, fastf_t md, double t_abs, double t_rel, double t_dist);

void populate_3d_pnts(struct ON_Brep_CDT_Face_State *f);

void triangles_degenerate_trivial(struct ON_Brep_CDT_Face_State *f);
void triangles_degenerate_area(struct ON_Brep_CDT_Face_State *f);
void triangles_degenerate_area_notify(struct ON_Brep_CDT_Face_State *f);
int triangles_check_edge_tris(struct ON_Brep_CDT_Face_State *f);
int triangles_slim_edge(struct ON_Brep_CDT_Face_State *f);
int triangles_incorrect_normals(struct ON_Brep_CDT_Face_State *f, std::set<p2t::Triangle *> *atris);


int triangles_check_edges(struct ON_Brep_CDT_Face_State *f);

void triangles_rebuild_involved(struct ON_Brep_CDT_Face_State *f);

void
trimesh_error_report(struct ON_Brep_CDT_State *s_cdt, int valid_fcnt, int valid_vcnt, int *valid_faces, fastf_t *valid_vertices, struct bg_trimesh_solid_errors *se);

bool build_poly2tri_polylines(struct ON_Brep_CDT_Face_State *f, p2t::CDT **cdt, int init_rtree);

void
Process_Loop_Edges(struct ON_Brep_CDT_Face_State *f, int li);

struct trimesh_info *CDT_Face_Build_Halfedge(std::set<p2t::Triangle *> *triangles);

int Remesh_Near_Tri(struct ON_Brep_CDT_Face_State *f, p2t::Triangle *t, std::set<p2t::Triangle *> *wq);
int Remesh_Near_cTri(struct ON_Brep_CDT_Face_State *f, cdt_mesh::triangle_t t, std::set<cdt_mesh::triangle_t> *wq, int rcnt);

int
ON_Brep_CDT_Tessellate2(struct ON_Brep_CDT_State *s_cdt);


/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

