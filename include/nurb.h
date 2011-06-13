/*                          N U R B . H
 * BRL-CAD
 *
 * Copyright (c) 1991-2011 United States Government as represented by
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
/** @addtogroup nurb */
/** @{ */
/** @file nurb.h
 *
 * @brief
 *	Define surface and curve structures for
 * 	Non Rational Uniform B-Spline (NURB)
 *	curves and surfaces.
 *
 */

#ifndef __NURB_H__
#define __NURB_H__

#include "common.h"

#ifndef VMATH_H
# include "vmath.h"
#endif

#include "nmg.h"

#ifndef RAYTRACE_H
# include "raytrace.h"
#endif

/* Define parametric directions for splitting. */

#define RT_NURB_SPLIT_ROW 0
#define RT_NURB_SPLIT_COL 1
#define RT_NURB_SPLIT_FLAT 2

/* Definition of NURB point types and coordinates
 * Bit:	  8765 4321 0
 *       |nnnn|tttt|h
 *			h     - 1 if Homogeneous
 *			tttt  - point type
 *				1 = XY
 *				2 = XYZ
 *				3 = UV
 *				4 = Random data
 *				5 = Projected surface
 *			nnnnn - number of coordinates per point
 *				includes the rational coordinate
 */

/* point types */
#define RT_NURB_PT_XY 	1			/**< @brief x, y coordintes */
#define RT_NURB_PT_XYZ	2			/**< @brief x, y, z coordinates */
#define RT_NURB_PT_UV	3			/**< @brief trim u, v parameter space */
#define RT_NURB_PT_DATA 4			/**< @brief random data */
#define RT_NURB_PT_PROJ	5			/**< @brief Projected Surface */

#define RT_NURB_PT_RATIONAL	1
#define RT_NURB_PT_NONRAT 	0

#define RT_NURB_MAKE_PT_TYPE(n, t, h)	((n<<5) | (t<<1) | h)
#define RT_NURB_EXTRACT_COORDS(pt)	(pt>>5)
#define RT_NURB_EXTRACT_PT_TYPE(pt)		((pt>>1) & 0x0f)
#define RT_NURB_IS_PT_RATIONAL(pt)		(pt & 0x1)
#define RT_NURB_STRIDE(pt)		(RT_NURB_EXTRACT_COORDS(pt) * sizeof( fastf_t))

/* macros to check/validate a structure pointer
 */
#define NMG_CK_KNOT(_p)		BU_CKMAG(_p, RT_KNOT_VECTOR_MAGIC, "knot_vector")
#define NMG_CK_CNURB(_p)	BU_CKMAG(_p, RT_CNURB_MAGIC, "cnurb")
#define NMG_CK_SNURB(_p)	BU_CKMAG(_p, RT_SNURB_MAGIC, "snurb")

#define GET_CNURB(p/*, m*/) 		{BU_GETSTRUCT(p, edge_g_cnurb); \
	/* NMG_INCR_INDEX(p, m); */ \
	BU_LIST_INIT( &(p)->l ); (p)->l.magic = NMG_EDGE_G_CNURB_MAGIC; }
#define GET_SNURB(p/*, m*/) 		{BU_GETSTRUCT(p, face_g_snurb); \
	/* NMG_INCR_INDEX(p, m); */ \
	BU_LIST_INIT( &(p)->l ); (p)->l.magic = NMG_FACE_G_SNURB_MAGIC; }


/* ----- Internal structures ----- */

struct rt_nurb_poly {
    struct rt_nurb_poly * next;
    point_t		ply[3];		/**< @brief Vertices */
    fastf_t		uv[3][2];	/**< @brief U, V parametric values */
};

struct rt_nurb_uv_hit {
    struct rt_nurb_uv_hit * next;
    int		sub;
    fastf_t		u;
    fastf_t		v;
};


struct oslo_mat {
    struct oslo_mat * next;
    int		offset;
    int		osize;
    fastf_t		* o_vec;
};

#if !defined(MAX)
# define MAX(i, j)    ( (i) > (j) ? (i) : (j) )
#endif
#if !defined(MIN)
# define MIN(i, j)    ( (i) < (j) ? (i) : (j) )
#endif

/* --- new way */

struct bezier_2d_list {
    struct bu_list	l;
    point2d_t	*ctl;
};

#ifdef __cplusplus
extern "C" {
#endif

/* nurb_basis.c */
RT_EXPORT extern fastf_t rt_nurb_basis_eval(struct knot_vector *knts, int interval,
						 int order, fastf_t mu);

/* nurb_bezier.c */
RT_EXPORT extern int rt_nurb_bezier(struct bu_list *bezier_hd, const struct face_g_snurb * srf, struct resource *res);
RT_EXPORT extern int rt_bez_check(const struct face_g_snurb * srf);
RT_EXPORT extern int nurb_crv_is_bezier(const struct edge_g_cnurb *crv);
RT_EXPORT extern void nurb_c_to_bezier(struct bu_list *clist, struct edge_g_cnurb *crv);

/* nurb_bound.c */
RT_EXPORT extern int rt_nurb_s_bound(struct face_g_snurb *srf, point_t bmin, point_t bmax);
RT_EXPORT extern int rt_nurb_c_bound(struct edge_g_cnurb *crv, point_t bmin, point_t bmax);
RT_EXPORT extern int rt_nurb_s_check(struct face_g_snurb *srf);
RT_EXPORT extern int rt_nurb_c_check(struct edge_g_cnurb *crv);

/* nurb_copy.c */
RT_EXPORT extern struct face_g_snurb *rt_nurb_scopy(const struct face_g_snurb *srf, struct resource *res);
RT_EXPORT extern struct edge_g_cnurb *rt_nurb_crv_copy(const struct edge_g_cnurb * crv);

/* nurb_diff.c */
RT_EXPORT extern struct face_g_snurb *rt_nurb_s_diff(const struct face_g_snurb *srf, int dir);
RT_EXPORT extern struct edge_g_cnurb *rt_nurb_c_diff(const struct edge_g_cnurb *crv);
RT_EXPORT extern void rt_nurb_mesh_diff(int order, const fastf_t *o_pts,
					     fastf_t *n_pts,
					     const fastf_t *knots, int o_stride, int n_stride,
					     int o_size, int pt_type);

/* nurb_eval.c */
RT_EXPORT extern void rt_nurb_s_eval(const struct face_g_snurb *srf, fastf_t u, fastf_t v, fastf_t * final_value);
RT_EXPORT extern void rt_nurb_c_eval(const struct edge_g_cnurb *crv, fastf_t param, fastf_t * final_value);
RT_EXPORT extern fastf_t *rt_nurb_eval_crv(fastf_t *crv, int order,
						fastf_t param,
						const struct knot_vector *k_vec, int k_index, int coords);
RT_EXPORT extern void rt_nurb_pr_crv(fastf_t *crv, int c_size, int coords);

/* nurb_flat.c */
RT_EXPORT extern int rt_nurb_s_flat(struct face_g_snurb *srf, fastf_t epsilon);
RT_EXPORT extern fastf_t rt_nurb_crv_flat(fastf_t *crv, int	size, int pt_type);

/* nurb_knot.c */
RT_EXPORT extern void rt_nurb_kvknot(struct knot_vector *new_knots, int order,
					  fastf_t lower, fastf_t upper, int num, struct resource *res);
RT_EXPORT extern void rt_nurb_kvmult(struct knot_vector *new_kv,
					  const struct knot_vector *kv,
					  int num, fastf_t val, struct resource *res);
RT_EXPORT extern void rt_nurb_kvgen(struct knot_vector *kv,
					 fastf_t lower, fastf_t upper, int num, struct resource *res);
RT_EXPORT extern void rt_nurb_kvmerge(struct knot_vector *new_knots,
					   const struct knot_vector *kv1,
					   const struct knot_vector *kv2, struct resource *res);
RT_EXPORT extern int rt_nurb_kvcheck(fastf_t val, const struct knot_vector *kv);
RT_EXPORT extern void rt_nurb_kvextract(struct knot_vector *new_kv,
					     const struct knot_vector *kv,
					     int lower, int upper, struct resource *res);
RT_EXPORT extern void rt_nurb_kvcopy(struct knot_vector *new_kv,
					  const struct knot_vector *old_kv, struct resource *res);
RT_EXPORT extern void rt_nurb_kvnorm(struct knot_vector *kv);
RT_EXPORT extern int rt_nurb_knot_index(const struct knot_vector *kv, fastf_t k_value, int order);
RT_EXPORT extern void rt_nurb_gen_knot_vector(struct knot_vector *new_knots,
						   int order, fastf_t lower, fastf_t upper, struct resource *res);

/* nurb_norm.c */
RT_EXPORT extern void rt_nurb_s_norm(struct face_g_snurb *srf, fastf_t u, fastf_t v, fastf_t * norm);

/* nurb_c2.c */
RT_EXPORT extern void rt_nurb_curvature(struct curvature *cvp,
					     const struct face_g_snurb *srf, fastf_t u, fastf_t v);

/* nurb_plot.c */
RT_EXPORT extern void rt_nurb_plot_snurb(FILE *fp, const struct face_g_snurb *srf);
RT_EXPORT extern void rt_nurb_plot_cnurb(FILE *fp, const struct edge_g_cnurb *crv);
RT_EXPORT extern void rt_nurb_s_plot(const struct face_g_snurb *srf);

/* nurb_interp.c */
RT_EXPORT extern void rt_nurb_cinterp(struct edge_g_cnurb *crv, int order,
					   const fastf_t *data, int n);
RT_EXPORT extern void rt_nurb_sinterp(struct face_g_snurb *srf, int order,
					   const fastf_t *data, int ymax, int xmax);

/* nurb_poly.c */
RT_EXPORT extern struct rt_nurb_poly *rt_nurb_to_poly(struct face_g_snurb *srf);
RT_EXPORT extern struct rt_nurb_poly *rt_nurb_mk_poly(fastf_t *v1, fastf_t *v2, fastf_t *v3,
		     fastf_t uv1[2], fastf_t uv2[2], fastf_t uv3[2]);

/* nurb_ray.c */
RT_EXPORT extern struct face_g_snurb *rt_nurb_project_srf(const struct face_g_snurb *srf,
							       plane_t plane1, plane_t plane2, struct resource *res);
RT_EXPORT extern void rt_nurb_clip_srf(const struct face_g_snurb *srf,
					    int dir, fastf_t *min, fastf_t *max);
RT_EXPORT extern struct face_g_snurb *rt_nurb_region_from_srf(const struct face_g_snurb *srf,
								   int dir, fastf_t param1, fastf_t param2, struct resource *res);
RT_EXPORT extern struct rt_nurb_uv_hit *rt_nurb_intersect(const struct face_g_snurb * srf,
							       plane_t plane1, plane_t plane2, double uv_tol, struct resource *res, struct bu_list *plist);

/* nurb_refine.c */
RT_EXPORT extern struct face_g_snurb *rt_nurb_s_refine(const struct face_g_snurb *srf,
							    int dir, struct knot_vector *kv, struct resource *res);
RT_EXPORT extern struct edge_g_cnurb *rt_nurb_c_refine(const struct edge_g_cnurb * crv,
							    struct knot_vector *kv);

/* nurb_solve.c */
RT_EXPORT extern void rt_nurb_solve(fastf_t *mat_1, fastf_t *mat_2,
					 fastf_t *solution, int dim, int coords);
RT_EXPORT extern void rt_nurb_doolittle(fastf_t *mat_1, fastf_t *mat_2,
					     int row, int coords);
RT_EXPORT extern void rt_nurb_forw_solve(const fastf_t *lu, const fastf_t *b,
					      fastf_t *y, int n);
RT_EXPORT extern void rt_nurb_back_solve(const fastf_t *lu, const fastf_t *y,
					      fastf_t *x, int n);
RT_EXPORT extern void rt_nurb_p_mat(const fastf_t * mat, int dim);

/* nurb_split.c */
RT_EXPORT extern void rt_nurb_s_split(struct bu_list *split_hd, const struct face_g_snurb *srf,
					   int dir, struct resource *res);
RT_EXPORT extern void rt_nurb_c_split(struct bu_list *split_hd, const struct edge_g_cnurb *crv);

/* nurb_trim.c */
RT_EXPORT extern int nmg_uv_in_lu(const fastf_t u, const fastf_t v, const struct loopuse *lu);

/* nurb_util.c */
RT_EXPORT extern struct face_g_snurb *rt_nurb_new_snurb(int u_order, int v_order,
							     int n_u_knots, int n_v_knots,
							     int n_rows, int n_cols, int pt_type, struct resource *res);
RT_EXPORT extern struct edge_g_cnurb *rt_nurb_new_cnurb(int order, int n_knots,
							     int n_pts, int pt_type);
RT_EXPORT extern void rt_nurb_free_snurb(struct face_g_snurb *srf, struct resource *res);
RT_EXPORT extern void rt_nurb_free_cnurb(struct edge_g_cnurb * crv);
RT_EXPORT extern void rt_nurb_c_print(const struct edge_g_cnurb *crv);
RT_EXPORT extern void rt_nurb_s_print(char *c, const struct face_g_snurb *srf);
RT_EXPORT extern void rt_nurb_pr_kv(const struct knot_vector *kv);
RT_EXPORT extern void rt_nurb_pr_mesh(const struct face_g_snurb *m);
RT_EXPORT extern void rt_nurb_print_pt_type(int c);
RT_EXPORT extern void rt_nurb_clean_cnurb(struct edge_g_cnurb *crv);

/* nurb_xsplit.c */
RT_EXPORT extern struct face_g_snurb *rt_nurb_s_xsplit(struct face_g_snurb *srf,
							    fastf_t param, int dir);
RT_EXPORT extern struct edge_g_cnurb *rt_nurb_c_xsplit(struct edge_g_cnurb *crv, fastf_t param);

/* oslo_calc.c */
RT_EXPORT extern struct oslo_mat *rt_nurb_calc_oslo(int order,
							 const struct knot_vector *tau_kv,
							 struct knot_vector *t_kv, struct resource *res);
RT_EXPORT extern void rt_nurb_pr_oslo(struct oslo_mat *om);
RT_EXPORT extern void rt_nurb_free_oslo(struct oslo_mat *om, struct resource *res);

/* oslo_map.c */
RT_EXPORT extern void rt_nurb_map_oslo(struct oslo_mat *oslo,
					    fastf_t *old_pts, fastf_t *new_pts,
					    int o_stride, int n_stride,
					    int lower, int upper, int pt_type);

/* bezier_2d_isect.c */
RT_EXPORT extern int CrossingCount(point2d_t *V, int degree, point2d_t ray_start, point2d_t ray_perp );
RT_EXPORT extern int ControlPolygonFlatEnough(point2d_t *V, int degree, fastf_t epsilon );
RT_EXPORT extern void Bezier(point2d_t *V, int degree, double t,
				   point2d_t *Left, point2d_t *Right, point2d_t eval_pt, point2d_t normal );
RT_EXPORT extern int FindRoots(point2d_t *w, int degree, point2d_t **intercept, point2d_t **normal,
				     point2d_t ray_start, point2d_t ray_dir, point2d_t ray_perp,
				     int depth, fastf_t epsilon);
RT_EXPORT extern struct bezier_2d_list *subdivide_bezier(struct bezier_2d_list *bezier_hd, int degree,
							       fastf_t epsilon, int depth);
#ifdef __cplusplus
}
#endif

#endif /* __NURB_H__ */

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
