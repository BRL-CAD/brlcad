/*                        N U R B . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/** @addtogroup libnmg
 * @brief
 * NMG NURBS definitions
 *
 * Note that some of the NURBS specific data structures currently live in
 * topology.h - this is less than ideal, but the original implementation
 * does not hide them from the topology containers...
 */
/** @{ */
/** @file nmg/nurb.h */

#ifndef NMG_NURB_H
#define NMG_NURB_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/topology.h"

__BEGIN_DECLS

#define NMG_CK_EDGE_G_CNURB(_p)       NMG_CKMAG(_p, NMG_EDGE_G_CNURB_MAGIC, "edge_g_cnurb")
#define NMG_CK_EDGE_G_EITHER(_p)      NMG_CK2MAG(_p, NMG_EDGE_G_LSEG_MAGIC, NMG_EDGE_G_CNURB_MAGIC, "edge_g_lseg|edge_g_cnurb")
#define NMG_CK_VERTEXUSE_A_CNURB(_p)  NMG_CKMAG(_p, NMG_VERTEXUSE_A_CNURB_MAGIC, "vertexuse_a_cnurb")
#define NMG_CK_VERTEXUSE_A_EITHER(_p) NMG_CK2MAG(_p, NMG_VERTEXUSE_A_PLANE_MAGIC, NMG_VERTEXUSE_A_CNURB_MAGIC, "vertexuse_a_plane|vertexuse_a_cnurb")

#define RT_NURB_SPLIT_ROW 0
#define RT_NURB_SPLIT_COL 1
#define RT_NURB_SPLIT_FLAT 2

/* Definition of NURB point types and coordinates
 * Bit:   8765 4321 0
 *       |nnnn|tttt|h
 *                      h     - 1 if Homogeneous
 *                      tttt  - point type
 *                              1 = XY
 *                              2 = XYZ
 *                              3 = UV
 *                              4 = Random data
 *                              5 = Projected surface
 *                      nnnnn - number of coordinates per point
 *                              includes the rational coordinate
 */

/* point types */
#define RT_NURB_PT_XY   1                       /**< @brief x, y coordinates */
#define RT_NURB_PT_XYZ  2                       /**< @brief x, y, z coordinates */
#define RT_NURB_PT_UV   3                       /**< @brief trim u, v parameter space */
#define RT_NURB_PT_DATA 4                       /**< @brief random data */
#define RT_NURB_PT_PROJ 5                       /**< @brief Projected Surface */

#define RT_NURB_PT_RATIONAL     1
#define RT_NURB_PT_NONRAT       0

#define RT_NURB_MAKE_PT_TYPE(n, t, h)   ((n<<5) | (t<<1) | h)
#define RT_NURB_EXTRACT_COORDS(pt)      (pt>>5)
#define RT_NURB_EXTRACT_PT_TYPE(pt)             ((pt>>1) & 0x0f)
#define RT_NURB_IS_PT_RATIONAL(pt)              (pt & 0x1)
#define RT_NURB_STRIDE(pt)              (RT_NURB_EXTRACT_COORDS(pt) * sizeof( fastf_t))

#define DEBUG_NMG_SPLINE        0x00000100      /**< @brief 9 Splines */

/* macros to check/validate a structure pointer
 */
#define NMG_CK_KNOT(_p)         BU_CKMAG(_p, RT_KNOT_VECTOR_MAGIC, "knot_vector")
#define NMG_CK_CNURB(_p)        BU_CKMAG(_p, NMG_EDGE_G_CNURB_MAGIC, "cnurb")
#define NMG_CK_SNURB(_p)        BU_CKMAG(_p, NMG_FACE_G_SNURB_MAGIC, "snurb")

/* DEPRECATED */
#define GET_CNURB(p/*, m*/) { \
        BU_ALLOC((p), struct edge_g_cnurb); \
        /* NMG_INCR_INDEX(p, m); */ \
        BU_LIST_INIT( &(p)->l ); (p)->l.magic = NMG_EDGE_G_CNURB_MAGIC; \
    }

/* DEPRECATED */
#define GET_SNURB(p/*, m*/) { \
        BU_ALLOC((p), struct face_g_snurb); \
        /* NMG_INCR_INDEX(p, m); */ \
        BU_LIST_INIT( &(p)->l ); (p)->l.magic = NMG_FACE_G_SNURB_MAGIC; \
    }

/* ----- Internal structures ----- */

struct nmg_nurb_poly {
    struct nmg_nurb_poly * next;
    point_t             ply[3];         /**< @brief Vertices */
    fastf_t             uv[3][2];       /**< @brief U, V parametric values */
};

struct nmg_nurb_uv_hit {
    struct nmg_nurb_uv_hit * next;
    int         sub;
    fastf_t             u;
    fastf_t             v;
};


struct oslo_mat {
    struct oslo_mat * next;
    int         offset;
    int         osize;
    fastf_t             * o_vec;
};

/* --- new way */

struct bezier_2d_list {
    struct bu_list      l;
    point2d_t   *ctl;
};

/**
 * Evaluate a Bezier curve at a particular parameter value. Fill in
 * control points for resulting sub-curves if "Left" and "Right" are
 * non-null.
 */
NMG_EXPORT extern void bezier(point2d_t *V, int degree, double t, point2d_t *Left, point2d_t *Right, point2d_t eval_pt, point2d_t normal );

/**
 * Given an equation in Bernstein-Bezier form, find all of the roots
 * in the interval [0, 1].  Return the number of roots found.
 */
NMG_EXPORT extern int bezier_roots(point2d_t *w, int degree, point2d_t **intercept, point2d_t **normal, point2d_t ray_start, point2d_t ray_dir, point2d_t ray_perp, int depth, fastf_t epsilon);

/**
 * subdivide a 2D bezier curve at t=0.5
 */
NMG_EXPORT extern struct bezier_2d_list *bezier_subdivide(struct bezier_2d_list *bezier_hd, int degree, fastf_t epsilon, int depth);


/* TODO - this is another one of those data concepts common to librt and libnmg */
struct nmg_curvature {
    vect_t      crv_pdir;       /**< @brief Principle direction */
    fastf_t     crv_c1;         /**< @brief curvature in principle dir */
    fastf_t     crv_c2;         /**< @brief curvature in other direction */
};

/* nurb_basis.c */
NMG_EXPORT extern fastf_t nmg_nurb_basis_eval(struct knot_vector *knts, int interval,
                                              int order, fastf_t mu);

/* nurb_bezier.c */
NMG_EXPORT extern int nmg_nurb_bezier(struct bu_list *bezier_hd, const struct face_g_snurb * srf);
NMG_EXPORT extern int nmg_bez_check(const struct face_g_snurb * srf);
NMG_EXPORT extern int nurb_crv_is_bezier(const struct edge_g_cnurb *crv);
NMG_EXPORT extern void nurb_c_to_bezier(struct bu_list *clist, struct edge_g_cnurb *crv);

/* nurb_bound.c */
NMG_EXPORT extern int nmg_nurb_s_bound(struct face_g_snurb *srf, point_t bmin, point_t bmax);
NMG_EXPORT extern int nmg_nurb_c_bound(struct edge_g_cnurb *crv, point_t bmin, point_t bmax);
NMG_EXPORT extern int nmg_nurb_s_check(struct face_g_snurb *srf);
NMG_EXPORT extern int nmg_nurb_c_check(struct edge_g_cnurb *crv);

/* nurb_copy.c */
NMG_EXPORT extern struct face_g_snurb *nmg_nurb_scopy(const struct face_g_snurb *srf);
NMG_EXPORT extern struct edge_g_cnurb *nmg_nurb_crv_copy(const struct edge_g_cnurb * crv);

/* nurb_diff.c */
NMG_EXPORT extern struct face_g_snurb *nmg_nurb_s_diff(const struct face_g_snurb *srf, int dir);
NMG_EXPORT extern struct edge_g_cnurb *nmg_nurb_c_diff(const struct edge_g_cnurb *crv);
NMG_EXPORT extern void nmg_nurb_mesh_diff(int order, const fastf_t *o_pts,
                                          fastf_t *n_pts,
                                          const fastf_t *knots, int o_stride, int n_stride,
                                          int o_size, int pt_type);

/* nurb_eval.c */
NMG_EXPORT extern void nmg_nurb_s_eval(const struct face_g_snurb *srf, fastf_t u, fastf_t v, fastf_t * final_value);
NMG_EXPORT extern void nmg_nurb_c_eval(const struct edge_g_cnurb *crv, fastf_t param, fastf_t * final_value);
NMG_EXPORT extern fastf_t *nmg_nurb_eval_crv(fastf_t *crv, int order,
                                             fastf_t param,
                                             const struct knot_vector *k_vec, int k_index, int coords);
NMG_EXPORT extern void nmg_nurb_pr_crv(fastf_t *crv, int c_size, int coords);

/* nurb_flat.c */
NMG_EXPORT extern int nmg_nurb_s_flat(struct face_g_snurb *srf, fastf_t epsilon);
NMG_EXPORT extern fastf_t nmg_nurb_crv_flat(fastf_t *crv, int   size, int pt_type);

/* nurb_knot.c */
NMG_EXPORT extern void nmg_nurb_kvknot(struct knot_vector *new_knots, int order,
                                       fastf_t lower, fastf_t upper, int num);
NMG_EXPORT extern void nmg_nurb_kvmult(struct knot_vector *new_kv,
                                       const struct knot_vector *kv,
                                       int num, fastf_t val);
NMG_EXPORT extern void nmg_nurb_kvgen(struct knot_vector *kv,
                                      fastf_t lower, fastf_t upper, int num);
NMG_EXPORT extern void nmg_nurb_kvmerge(struct knot_vector *new_knots,
                                        const struct knot_vector *kv1,
                                        const struct knot_vector *kv2);
NMG_EXPORT extern int nmg_nurb_kvcheck(fastf_t val, const struct knot_vector *kv);
NMG_EXPORT extern void nmg_nurb_kvextract(struct knot_vector *new_kv,
                                          const struct knot_vector *kv,
                                          int lower, int upper);
NMG_EXPORT extern void nmg_nurb_kvcopy(struct knot_vector *new_kv,
                                       const struct knot_vector *old_kv);
NMG_EXPORT extern void nmg_nurb_kvnorm(struct knot_vector *kv);
NMG_EXPORT extern int nmg_nurb_knot_index(const struct knot_vector *kv, fastf_t k_value, int order);
NMG_EXPORT extern void nmg_nurb_gen_knot_vector(struct knot_vector *new_knots,
                                                int order, fastf_t lower, fastf_t upper);

/* nurb_norm.c */
NMG_EXPORT extern void nmg_nurb_s_norm(struct face_g_snurb *srf, fastf_t u, fastf_t v, fastf_t * norm);

/* nurb_c2.c */
NMG_EXPORT extern void nmg_nurb_curvature(struct nmg_curvature *cvp,
                                          const struct face_g_snurb *srf, fastf_t u, fastf_t v);

/* nurb_plot.c */
NMG_EXPORT extern void nmg_nurb_plot_snurb(FILE *fp, const struct face_g_snurb *srf);
NMG_EXPORT extern void nmg_nurb_plot_cnurb(FILE *fp, const struct edge_g_cnurb *crv);
NMG_EXPORT extern void nmg_nurb_s_plot(const struct face_g_snurb *srf);

/* nurb_interp.c */
NMG_EXPORT extern void nmg_nurb_cinterp(struct edge_g_cnurb *crv, int order,
                                        const fastf_t *data, int n);
NMG_EXPORT extern void nmg_nurb_sinterp(struct face_g_snurb *srf, int order,
                                        const fastf_t *data, int ymax, int xmax);

/* nurb_poly.c */
NMG_EXPORT extern struct nmg_nurb_poly *nmg_nurb_to_poly(struct face_g_snurb *srf);
NMG_EXPORT extern struct nmg_nurb_poly *nmg_nurb_mk_poly(fastf_t *v1, fastf_t *v2, fastf_t *v3,
                                                         fastf_t uv1[2], fastf_t uv2[2], fastf_t uv3[2]);

/* nurb_ray.c */
NMG_EXPORT extern struct face_g_snurb *nmg_nurb_project_srf(const struct face_g_snurb *srf,
                                                            plane_t plane1, plane_t plane2);
NMG_EXPORT extern void nmg_nurb_clip_srf(const struct face_g_snurb *srf,
                                         int dir, fastf_t *min, fastf_t *max);
NMG_EXPORT extern struct face_g_snurb *nmg_nurb_region_from_srf(const struct face_g_snurb *srf,
                                                                int dir, fastf_t param1, fastf_t param2);
NMG_EXPORT extern struct nmg_nurb_uv_hit *nmg_nurb_intersect(const struct face_g_snurb * srf,
                                                             plane_t plane1, plane_t plane2, double uv_tol, struct bu_list *plist);

/* nurb_refine.c */
NMG_EXPORT extern struct face_g_snurb *nmg_nurb_s_refine(const struct face_g_snurb *srf,
                                                         int dir, struct knot_vector *kv);
NMG_EXPORT extern struct edge_g_cnurb *nmg_nurb_c_refine(const struct edge_g_cnurb * crv,
                                                         struct knot_vector *kv);

/* nurb_solve.c */
NMG_EXPORT extern void nmg_nurb_solve(fastf_t *mat_1, fastf_t *mat_2,
                                      fastf_t *solution, int dim, int coords);
NMG_EXPORT extern void nmg_nurb_doolittle(fastf_t *mat_1, fastf_t *mat_2,
                                          int row, int coords);
NMG_EXPORT extern void nmg_nurb_forw_solve(const fastf_t *lu, const fastf_t *b,
                                           fastf_t *y, int n);
NMG_EXPORT extern void nmg_nurb_back_solve(const fastf_t *lu, const fastf_t *y,
                                           fastf_t *x, int n);
NMG_EXPORT extern void nmg_nurb_p_mat(const fastf_t * mat, int dim);

/* nurb_split.c */
NMG_EXPORT extern void nmg_nurb_s_split(struct bu_list *split_hd, const struct face_g_snurb *srf,
                                        int dir);
NMG_EXPORT extern void nmg_nurb_c_split(struct bu_list *split_hd, const struct edge_g_cnurb *crv);

/* nurb_trim.c */
NMG_EXPORT extern int nmg_uv_in_lu(const fastf_t u, const fastf_t v, const struct loopuse *lu);

/* nurb_util.c */
NMG_EXPORT extern struct face_g_snurb *nmg_nurb_new_snurb(int u_order, int v_order,
                                                          int n_u_knots, int n_v_knots,
                                                          int n_rows, int n_cols, int pt_type);
NMG_EXPORT extern struct edge_g_cnurb *nmg_nurb_new_cnurb(int order, int n_knots,
                                                          int n_pts, int pt_type);
NMG_EXPORT extern void nmg_nurb_free_snurb(struct face_g_snurb *srf);
NMG_EXPORT extern void nmg_nurb_free_cnurb(struct edge_g_cnurb * crv);
NMG_EXPORT extern void nmg_nurb_c_print(const struct edge_g_cnurb *crv);
NMG_EXPORT extern void nmg_nurb_s_print(char *c, const struct face_g_snurb *srf);
NMG_EXPORT extern void nmg_nurb_pr_kv(const struct knot_vector *kv);
NMG_EXPORT extern void nmg_nurb_pr_mesh(const struct face_g_snurb *m);
NMG_EXPORT extern void nmg_nurb_print_pnt_type(int c);
NMG_EXPORT extern void nmg_nurb_clean_cnurb(struct edge_g_cnurb *crv);

/* nurb_xsplit.c */
NMG_EXPORT extern struct face_g_snurb *nmg_nurb_s_xsplit(struct face_g_snurb *srf,
                                                         fastf_t param, int dir);
NMG_EXPORT extern struct edge_g_cnurb *nmg_nurb_c_xsplit(struct edge_g_cnurb *crv, fastf_t param);

/* oslo_calc.c */
NMG_EXPORT extern struct oslo_mat *nmg_nurb_calc_oslo(int order,
                                                      const struct knot_vector *tau_kv,
                                                      struct knot_vector *t_kv);
NMG_EXPORT extern void nmg_nurb_pr_oslo(struct oslo_mat *om);
NMG_EXPORT extern void nmg_nurb_free_oslo(struct oslo_mat *om);

/* oslo_map.c */
NMG_EXPORT extern void nmg_nurb_map_oslo(struct oslo_mat *oslo,
                                         fastf_t *old_pts, fastf_t *new_pts,
                                         int o_stride, int n_stride,
                                         int lower, int upper, int pt_type);

/* nurb_tess.c */
NMG_EXPORT extern fastf_t rt_cnurb_par_edge(const struct edge_g_cnurb *crv, fastf_t epsilon);


NMG_EXPORT extern void nmg_face_g_snurb(struct faceuse *fu,
                                        int u_order,
                                        int v_order,
                                        int n_u_knots,
                                        int n_v_knots,
                                        fastf_t *ukv,
                                        fastf_t *vkv,
                                        int n_rows,
                                        int n_cols,
                                        int pt_type,
                                        fastf_t *mesh);

/* From nmg_misc.c */
NMG_EXPORT extern int nmg_snurb_calc_lu_uv_orient(const struct loopuse *lu);
NMG_EXPORT extern void nmg_snurb_fu_eval(const struct faceuse *fu,
                                         const fastf_t u,
                                         const fastf_t v,
                                         point_t pt_on_srf);
NMG_EXPORT extern void nmg_snurb_fu_get_norm(const struct faceuse *fu,
                                             const fastf_t u,
                                             const fastf_t v,
                                             vect_t norm);
NMG_EXPORT extern void nmg_snurb_fu_get_norm_at_vu(const struct faceuse *fu,
                                                   const struct vertexuse *vu,
                                                   vect_t norm);


NMG_EXPORT extern void nmg_vertexuse_a_cnurb(struct vertexuse *vu,
                                             const fastf_t *uvw);
NMG_EXPORT extern void nmg_edge_g_cnurb(struct edgeuse *eu,
                                        int order,
                                        int n_knots,
                                        fastf_t *kv,
                                        int n_pts,
                                        int pt_type,
                                        fastf_t *points);
NMG_EXPORT extern void nmg_edge_g_cnurb_plinear(struct edgeuse *eu);
NMG_EXPORT extern struct edge_g_cnurb *nmg_join_cnurbs(struct bu_list *crv_head);
NMG_EXPORT extern struct edge_g_cnurb *nmg_arc2d_to_cnurb(point_t i_center,
                                                          point_t i_start,
                                                          point_t i_end,
                                                          int point_type,
                                                          const struct bn_tol *tol);
NMG_EXPORT extern int nmg_cnurb_is_linear(const struct edge_g_cnurb *cnrb);


NMG_EXPORT extern int nmg_snurb_is_planar(const struct face_g_snurb *srf,
                                          const struct bn_tol *tol);
NMG_EXPORT extern void nmg_eval_linear_trim_curve(const struct face_g_snurb *snrb,
                                                  const fastf_t uvw[3],
                                                  point_t xyz);


NMG_EXPORT extern void nmg_eval_trim_curve(const struct edge_g_cnurb *cnrb,
                                           const struct face_g_snurb *snrb,
                                           const fastf_t t, point_t xyz);
/* nmg_split_trim */
NMG_EXPORT extern void nmg_eval_trim_to_tol(const struct edge_g_cnurb *cnrb,
                                            const struct face_g_snurb *snrb,
                                            const fastf_t t0,
                                            const fastf_t t1,
                                            struct bu_list *head,
                                            const struct bn_tol *tol);
/* nmg_split_linear_trim */
NMG_EXPORT extern void nmg_eval_linear_trim_to_tol(const struct edge_g_cnurb *cnrb,
                                                   const struct face_g_snurb *snrb,
                                                   const fastf_t uvw1[3],
                                                   const fastf_t uvw2[3],
                                                   struct bu_list *head,
                                                   const struct bn_tol *tol);
NMG_EXPORT extern int nmg_cnurb_lseg_coincident(const struct edgeuse *eu1,
                                                const struct edge_g_cnurb *cnrb,
                                                const struct face_g_snurb *snrb,
                                                const point_t pt1,
                                                const point_t pt2,
                                                const struct bn_tol *tol);
NMG_EXPORT extern int   nmg_cnurb_is_on_crv(const struct edgeuse *eu,
                                            const struct edge_g_cnurb *cnrb,
                                            const struct face_g_snurb *snrb,
                                            const struct bu_list *head,
                                            const struct bn_tol *tol);


__END_DECLS

#endif  /* NMG_NURB_H */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
