/*                        C D T . H
 * BRL-CAD
 *
 * Copyright (c) 2007-2026 United States Government as represented by
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

#ifndef LIBBREP_CDT_H
#define LIBBREP_CDT_H

#include "common.h"

#include <vector>
#include <list>
#include <map>
#include <stack>
#include <iostream>
#include <algorithm>
#include <set>
#include <unordered_map>
#include <utility>

#include "../../libbg/RTree.h"

#include "assert.h"

#include "vmath.h"

#include "bu/color.h"
#include "bu/cv.h"
#include "bu/opt.h"
#include "bu/datetime.h"
#include "bn/mat.h"
#include "bg/plane.h"
#include "bv/plot3.h"
#include "bn/tol.h"
#include "bv/vlist.h"
#include "bg/polygon.h"
#include "bg/trimesh.h"
#include "brep/defines.h"
#include "brep/cdt.h"
#include "brep/pullback.h"
#include "brep/util.h"

#include "./mesh.h"

#define BREP_PLANAR_TOL 0.05

/***************************************************/

#define BREP_CDT_FAILED -3
#define BREP_CDT_NON_SOLID -2
#define BREP_CDT_UNTESSELLATED -1
#define BREP_CDT_SOLID 0

/* Note - this tolerance value is based on the default
 * value from the GED 'tol' command */
#define BREP_CDT_DEFAULT_TOL_REL 0.01

/* Origin information used for tolerance checks and failure diagnostics. */
struct cdt_audit_info {
    int face_index;
    int vert_index;
    int trim_index;
    int edge_index;
    ON_2dPoint surf_uv;
};

struct brep_cdt_tol {
    fastf_t min_dist;
    fastf_t max_dist;
    fastf_t within_dist;
    fastf_t cos_within_ang;
};
#define BREP_CDT_TOL_ZERO {0.0, 0.0, 0.0, 0.0}

struct ON_Brep_CDT_State {

    int status;
    struct brep_cdt_diagnostic diagnostic;
    std::vector<int> failed_face_indices;
    std::map<int, struct brep_cdt_diagnostic> failed_face_diagnostics;
    bool repair_source_valid;
    struct brep_cdt_diagnostic repair_source_diagnostic;
    bool tolerance_changed;
    ON_Brep *orig_brep;
    ON_Brep *brep;
    const char *name;

    /* Tolerances */
    struct bg_tess_tol tol;
    fastf_t absmax;
    fastf_t absmin;
    fastf_t cos_within_ang;
    fastf_t ovlp_max_len;

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
    std::map<int, std::set<bedge_seg_t *>> e2polysegs;
    /* Derived mesh-only welding for explicit B-Rep edges whose complete
     * curve is smaller than the modeling tolerance.  Source B-Rep topology
     * and geometry are never changed. */
    std::map<ON_3dPoint *, ON_3dPoint *> collapsed_edge_pnts;
    std::set<int> collapsed_edges;
    /* A repair-only retry may use the midpoint between disagreeing paired
     * p-curves when both misses stay within this explicit fidelity bound. */
    bool allow_bounded_edge_approximation;
    fastf_t bounded_edge_approximation_tolerance;
    std::map<int, fastf_t> approximated_edges;
    std::map<ON_3dPoint *, double> v_min_seg_len;
    std::map<int, double> l_median_len;
    std::set<cpolyedge_t *> unsplit_singular_edges;

    /* Audit data */
    std::vector<ON_3dPoint *> *bot_pnt_to_on_pnt;
    std::vector<int> bot_face_to_brep_face;
    std::vector<size_t> bot_face_to_cdt_triangle;
    std::unordered_map<ON_3dPoint *, struct cdt_audit_info> *pnt_audit_info;

    /* A successful full tessellation has already paid for assembly,
     * orientation synchronization, and solid validation.  Retain that
     * certified export so callers do not repeat those full-mesh operations
     * while all face-local state is still resident. */
    int *certified_faces;
    int certified_face_count;
    fastf_t *certified_vertices;
    int certified_vertex_count;
    int *certified_face_normals;
    int certified_face_normal_count;
    fastf_t *certified_normals;
    int certified_normal_count;
    bool certified_repaired;

    /* Face specific data */
    std::map<int, cdt_mesh_t> fmeshes;
    std::map<int, RTree<void *, double, 2>> face_rtrees_2d;
    std::map<int, RTree<void *, double, 3>> face_rtrees_3d;
    std::map<int, std::map<int,ON_3dPoint *>> strim_pnts;
    std::map<int, std::map<int,ON_3dPoint *>> strim_norms;

    std::map<int, std::map<size_t, std::set<struct brep_face_ovlp_instance *>>> face_tri_ovlps;
    std::map<int, std::set<size_t>> face_ovlp_tris;
    std::map<int, std::vector<struct brep_face_ovlp_instance *>> face_ovlps;
    std::set<int> faces_to_update;
};

ON_3dVector calc_trim_vnorm(ON_BrepVertex& v, ON_BrepTrim *trim);
bool initialize_edge_segs(struct ON_Brep_CDT_State *s_cdt, char *message,
	size_t message_size);
bool initialize_loop_polygons(struct ON_Brep_CDT_State *s_cdt);
bool split_edges_at_surface_poles(struct ON_Brep_CDT_State *s_cdt,
    char *failure_message, size_t failure_message_size);
size_t synchronize_coincident_edge_samples(
    struct ON_Brep_CDT_State *s_cdt,
    std::map<ON_3dPoint *, ON_3dPoint *> &welds);
double ang_deg(const ON_3dVector &v1, const ON_3dVector &v2);
std::set<bedge_seg_t *> split_edge_seg(struct ON_Brep_CDT_State *s_cdt,
    bedge_seg_t *bseg, int force, double *t, int update_rtrees,
    ON_3dPoint *shared_point = NULL);
std::set<cpolyedge_t *> split_singular_seg(struct ON_Brep_CDT_State *s_cdt, cpolyedge_t *ce, int update_rtree);
std::vector<cpolyedge_t *> cdt_face_polyedges(struct ON_Brep_CDT_State *s_cdt, int face_index);
void CDT_Add3DNorm(struct ON_Brep_CDT_State *s, ON_3dPoint *norm, ON_3dPoint *vert, int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d);
void CDT_Add3DPnt(struct ON_Brep_CDT_State *s, ON_3dPoint *p, int fid, int vid, int tid, int eid, fastf_t x2d, fastf_t y2d);
void CDT_Tol_Set(struct brep_cdt_tol *cdt, double dist, fastf_t md, double t_abs, double t_rel, double t_dist);
bool GetInteriorPoints(struct ON_Brep_CDT_State *s_cdt, int face_index);
void cdt_tol_global_calc(struct ON_Brep_CDT_State *s);
void curved_edges_refine(struct ON_Brep_CDT_State *s_cdt);
void finalize_rtrees(struct ON_Brep_CDT_State *s_cdt);
void initialize_edge_containers(struct ON_Brep_CDT_State *s_cdt);
void refine_close_edges(struct ON_Brep_CDT_State *s_cdt);
void tol_curved_edges_split(struct ON_Brep_CDT_State *s_cdt);
bool tol_linear_edges_split(struct ON_Brep_CDT_State *s_cdt,
	char *failure_message, size_t failure_message_size);
void cdt_state_reset(struct ON_Brep_CDT_State *s_cdt);
void cdt_diagnostic_set(struct ON_Brep_CDT_State *s_cdt, int result,
	int stage, int face_index, int completed_faces, int failed_faces,
	const char *message);
bool cdt_tri_tri_intersection(const point_t first[3],
	const point_t second[3]);
bool cdt_trim_pcurves_retrace(const ON_BrepTrim *first,
	const ON_BrepTrim *second);

/* debug */
void trimesh_error_report(struct ON_Brep_CDT_State *s_cdt, int valid_fcnt, int valid_vcnt, int *valid_faces, fastf_t *valid_vertices, struct bg_trimesh_solid_errors *se);
bool CDT_Audit(struct ON_Brep_CDT_State *s_cdt);
void on_bb_plot(const char *fname, ON_BoundingBox &bb);
void plot_pnt_3d(FILE *plot_file, ON_3dPoint *p, double r, int dir);
bool PPCHECK(ON_3dPoint &p);
bool TRICHECK(triangle_t &tri);

/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

#endif /* LIBBREP_CDT_H */
