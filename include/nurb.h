/*                          N U R B . H
 * BRL-CAD
 *
 * Copyright (c) 1991-2014 United States Government as represented by
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

#ifndef NURB_H
#define NURB_H

#include "common.h"
#include "bu.h"
#include "bn.h"
#include "vmath.h"

struct resource;

#ifndef NURB_EXPORT
#  if defined(NURB_DLL_EXPORTS) && defined(NURB_DLL_IMPORTS)
#    error "Only NURB_DLL_EXPORTS or NURB_DLL_IMPORTS can be defined, not both."
#  elif defined(NURB_DLL_EXPORTS)
#    define NURB_EXPORT __declspec(dllexport)
#  elif defined(NURB_DLL_IMPORTS)
#    define NURB_EXPORT __declspec(dllimport)
#  else
#    define NURB_EXPORT
#  endif
#endif

/**
 * controls the libnurb debug level
 */
NURB_EXPORT extern uint32_t nurb_debug;

#define DEBUG_NURB_ISECT   0x00000001	/**< @brief 1 mged: animated evaluation */
#define DEBUG_NURB_SPLINE   0x00000002	/**< @brief 2 mged: add delays to animation */

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
#define RT_NURB_PT_XY 	1			/**< @brief x, y coordinates */
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
#define NMG_CK_CNURB(_p)	BU_CKMAG(_p, NMG_EDGE_G_CNURB_MAGIC, "cnurb")
#define NMG_CK_SNURB(_p)	BU_CKMAG(_p, NMG_FACE_G_SNURB_MAGIC, "snurb")

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

#define NURB_CK_RESOURCE(_p) BU_CKMAG(_p, RESOURCE_MAGIC, "struct resource")

/* ----- Internal structures ----- */

/**
 * Information about curvature of the surface at a hit point.  The
 * principal direction pdir has unit length and principal curvature
 * c1.  |c1| <= |c2|, i.e. c1 is the most nearly flat principle
 * curvature.  A POSITIVE curvature indicates that the surface bends
 * TOWARD the (outward pointing) normal vector at that point.  c1 and
 * c2 are the inverse radii of curvature.  The other principle
 * direction is implied: pdir2 = normal x pdir1.
 */
struct curvature {
    vect_t	crv_pdir;	/**< @brief Principle direction */
    fastf_t	crv_c1;		/**< @brief curvature in principle dir */
    fastf_t	crv_c2;		/**< @brief curvature in other direction */
};
#define CURVE_NULL	((struct curvature *)0)
#define RT_CURVATURE_INIT_ZERO { VINIT_ZERO, 0.0, 0.0 }

/**
 * Use this macro after having computed the normal, to compute the
 * curvature at a hit point.
 *
 * In Release 4.4 and earlier, this was called RT_CURVE().  When the
 * extra argument was added the name was changed.
 */
#define RT_CURVATURE(_curvp, _hitp, _flipflag, _stp) { \
	RT_CK_HIT(_hitp); \
	RT_CK_SOLTAB(_stp); \
	RT_CK_FUNCTAB((_stp)->st_meth); \
	if ((_stp)->st_meth->ft_curve) { \
	    (_stp)->st_meth->ft_curve(_curvp, _hitp, _stp); \
	} \
	if (_flipflag) { \
	    (_curvp)->crv_c1 = - (_curvp)->crv_c1; \
	    (_curvp)->crv_c2 = - (_curvp)->crv_c2; \
	} \
    }

/* A more powerful interface would be: */
/* RT_GET_CURVATURE(_curvp, _partition, inhit/outhit flag, ap) */

/**
 * @brief
 * Definition of a knot vector.
 *
 * Not found independently, but used in the cnurb and snurb
 * structures.  (Exactly the same as the definition in nurb.h)
 */
struct knot_vector {
    uint32_t magic;
    int k_size;		/**< @brief knot vector size */
    fastf_t * knots;	/**< @brief pointer to knot vector */
};

struct face_g_plane {
    uint32_t magic;
    struct bu_list f_hd;	/**< @brief list of faces sharing this surface */
    plane_t N;			/**< @brief Plane equation (incl normal) */
    long index;			/**< @brief struct # in this model */
};

struct face_g_snurb {
    /* NOTICE: l.forw & l.back *not* stored in database.  They are for
     * bspline primitive internal use only.
     */
    struct bu_list l;
    struct bu_list f_hd;	/**< @brief list of faces sharing this surface */
    int order[2];		/**< @brief surface order [0] = u, [1] = v */
    struct knot_vector u;	/**< @brief surface knot vectors */
    struct knot_vector v;	/**< @brief surface knot vectors */
    /* surface control points */
    int s_size[2];		/**< @brief mesh size, u, v */
    int pt_type;		/**< @brief surface point type */
    fastf_t *ctl_points;	/**< @brief array [size[0]*size[1]] */
    /* START OF ITEMS VALID IN-MEMORY ONLY -- NOT STORED ON DISK */
    int dir;			/**< @brief direction of last refinement */
    point_t min_pt;		/**< @brief min corner of bounding box */
    point_t max_pt;		/**< @brief max corner of bounding box */
    /* END OF ITEMS VALID IN-MEMORY ONLY -- NOT STORED ON DISK */
    long index;			/**< @brief struct # in this model */
};

struct loop_g {
    uint32_t magic;
    point_t min_pt;		/**< @brief minimums of bounding box */
    point_t max_pt;		/**< @brief maximums of bounding box */
    long index;			/**< @brief struct # in this model */
};

/**
 * IMPORTANT: First two items in edge_g_lseg and edge_g_cnurb must be
 * identical structure, so pointers are puns for both.  eu_hd2 list
 * must be in same place for both.
 */
struct edge_g_lseg {
    struct bu_list l;		/**< @brief NOTICE:  l.forw & l.back *not* stored in database.  For alignment only. */
    struct bu_list eu_hd2;	/**< @brief heads l2 list of edgeuses on this line */
    point_t e_pt;		/**< @brief parametric equation of the line */
    vect_t e_dir;
    long index;			/**< @brief struct # in this model */
};

/**
 * The ctl_points on this curve are (u, v) values on the face's
 * surface.  As a storage and performance efficiency measure, if order
 * <= 0, then the cnurb is a straight line segment in parameter space,
 * and the k.knots and ctl_points pointers will be NULL.  In this
 * case, the vertexuse_a_cnurb's at both ends of the edgeuse define
 * the path through parameter space.
 */
struct edge_g_cnurb {
    struct bu_list l;		/**< @brief NOTICE: l.forw & l.back are NOT stored in database.  For bspline primitive internal use only. */
    struct bu_list eu_hd2;	/**< @brief heads l2 list of edgeuses on this curve */
    int order;			/**< @brief Curve Order */
    struct knot_vector k;	/**< @brief curve knot vector */
    /* curve control polygon */
    int c_size;			/**< @brief number of ctl points */
    int pt_type;		/**< @brief curve point type */
    fastf_t *ctl_points;	/**< @brief array [c_size] */
    long index;			/**< @brief struct # in this model */
};

struct vertex_g {
    uint32_t magic;
    point_t coord;		/**< @brief coordinates of vertex in space */
    long index;			/**< @brief struct # in this model */
};

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

/* --- new way */

struct bezier_2d_list {
    struct bu_list	l;
    point2d_t	*ctl;
};

__BEGIN_DECLS

/* basis.c */
NURB_EXPORT extern fastf_t rt_nurb_basis_eval(struct knot_vector *knts, int interval, int order, fastf_t mu);

/* bezier.c */
NURB_EXPORT extern int rt_nurb_bezier(struct bu_list *bezier_hd, const struct face_g_snurb * srf, struct resource *res);
NURB_EXPORT extern int rt_bez_check(const struct face_g_snurb * srf);
NURB_EXPORT extern int nurb_crv_is_bezier(const struct edge_g_cnurb *crv);
NURB_EXPORT extern void nurb_c_to_bezier(struct bu_list *clist, struct edge_g_cnurb *crv);

/* bound.c */
NURB_EXPORT extern int rt_nurb_s_bound(struct face_g_snurb *srf, point_t bmin, point_t bmax);
NURB_EXPORT extern int rt_nurb_c_bound(struct edge_g_cnurb *crv, point_t bmin, point_t bmax);
NURB_EXPORT extern int rt_nurb_s_check(struct face_g_snurb *srf);
NURB_EXPORT extern int rt_nurb_c_check(struct edge_g_cnurb *crv);

/* c2.c */
NURB_EXPORT extern void rt_nurb_curvature(struct curvature *cvp,
					  const struct face_g_snurb *srf,
					  fastf_t u, fastf_t v);

/* copy.c */
NURB_EXPORT extern struct face_g_snurb *rt_nurb_scopy(const struct face_g_snurb *srf, struct resource *res);
NURB_EXPORT extern struct edge_g_cnurb *rt_nurb_crv_copy(const struct edge_g_cnurb * crv);

/* diff.c */
NURB_EXPORT extern struct face_g_snurb *rt_nurb_s_diff(const struct face_g_snurb *srf, int dir);
NURB_EXPORT extern struct edge_g_cnurb *rt_nurb_c_diff(const struct edge_g_cnurb *crv);
NURB_EXPORT extern void rt_nurb_mesh_diff(int order, const fastf_t *o_pts,
					  fastf_t *n_pts,
					  const fastf_t *knots, int o_stride, int n_stride,
					  int o_size, int pt_type);

/* eval.c */
NURB_EXPORT extern void rt_nurb_s_eval(const struct face_g_snurb *srf, fastf_t u, fastf_t v, fastf_t * final_value);
NURB_EXPORT extern void rt_nurb_c_eval(const struct edge_g_cnurb *crv, fastf_t param, fastf_t * final_value);
NURB_EXPORT extern fastf_t *rt_nurb_eval_crv(fastf_t *crv, int order,
					     fastf_t param,
					     const struct knot_vector *k_vec, int k_index, int coords);
NURB_EXPORT extern void rt_nurb_pr_crv(fastf_t *crv, int c_size, int coords);

/* flat.c */
NURB_EXPORT extern int rt_nurb_s_flat(struct face_g_snurb *srf, fastf_t epsilon);
NURB_EXPORT extern fastf_t rt_nurb_crv_flat(fastf_t *crv, int	size, int pt_type);

/* interp.c */
NURB_EXPORT extern void rt_nurb_cinterp(struct edge_g_cnurb *crv, int order,
					const fastf_t *data, int n);
NURB_EXPORT extern void rt_nurb_sinterp(struct face_g_snurb *srf, int order,
					const fastf_t *data, int ymax, int xmax);

/* knot.c */
NURB_EXPORT extern void rt_nurb_kvknot(struct knot_vector *new_knots, int order,
				       fastf_t lower, fastf_t upper, int num, struct resource *res);
NURB_EXPORT extern void rt_nurb_kvmult(struct knot_vector *new_kv,
				       const struct knot_vector *kv,
				       int num, fastf_t val, struct resource *res);
NURB_EXPORT extern void rt_nurb_kvgen(struct knot_vector *kv,
				      fastf_t lower, fastf_t upper, int num, struct resource *res);
NURB_EXPORT extern void rt_nurb_kvmerge(struct knot_vector *new_knots,
				        const struct knot_vector *kv1,
				        const struct knot_vector *kv2, struct resource *res);
NURB_EXPORT extern int rt_nurb_kvcheck(fastf_t val, const struct knot_vector *kv);
NURB_EXPORT extern void rt_nurb_kvextract(struct knot_vector *new_kv,
					  const struct knot_vector *kv,
					  int lower, int upper, struct resource *res);
NURB_EXPORT extern void rt_nurb_kvcopy(struct knot_vector *new_kv,
				       const struct knot_vector *old_kv, struct resource *res);
NURB_EXPORT extern void rt_nurb_kvnorm(struct knot_vector *kv);
NURB_EXPORT extern int rt_nurb_knot_index(const struct knot_vector *kv, fastf_t k_value, int order);
NURB_EXPORT extern void rt_nurb_gen_knot_vector(struct knot_vector *new_knots,
					        int order, fastf_t lower, fastf_t upper, struct resource *res);

/* norm.c */
NURB_EXPORT extern void rt_nurb_s_norm(struct face_g_snurb *srf, fastf_t u, fastf_t v, fastf_t * norm);

/* oslo_calc.c */
NURB_EXPORT extern struct oslo_mat *rt_nurb_calc_oslo(int order,
						      const struct knot_vector *tau_kv,
						      struct knot_vector *t_kv, struct resource *res);
NURB_EXPORT extern void rt_nurb_pr_oslo(struct oslo_mat *om);
NURB_EXPORT extern void rt_nurb_free_oslo(struct oslo_mat *om, struct resource *res);

/* oslo_map.c */
NURB_EXPORT extern void rt_nurb_map_oslo(struct oslo_mat *oslo,
					 fastf_t *old_pts, fastf_t *new_pts,
					 int o_stride, int n_stride,
					 int lower, int upper, int pt_type);

/* plot.c */
NURB_EXPORT extern void rt_nurb_plot_snurb(FILE *fp, const struct face_g_snurb *srf);
NURB_EXPORT extern void rt_nurb_plot_cnurb(FILE *fp, const struct edge_g_cnurb *crv);
NURB_EXPORT extern void rt_nurb_s_plot(const struct face_g_snurb *srf);

/* poly.c */
NURB_EXPORT extern struct rt_nurb_poly *rt_nurb_to_poly(struct face_g_snurb *srf);
NURB_EXPORT extern struct rt_nurb_poly *rt_nurb_mk_poly(fastf_t *v1, fastf_t *v2, fastf_t *v3,
							fastf_t uv1[2], fastf_t uv2[2], fastf_t uv3[2]);

/* ray.c */
NURB_EXPORT extern struct face_g_snurb *rt_nurb_project_srf(const struct face_g_snurb *srf,
							    plane_t plane1, plane_t plane2, struct resource *res);
NURB_EXPORT extern void rt_nurb_clip_srf(const struct face_g_snurb *srf,
					 int dir, fastf_t *min, fastf_t *max);
NURB_EXPORT extern struct face_g_snurb *rt_nurb_region_from_srf(const struct face_g_snurb *srf,
								int dir, fastf_t param1, fastf_t param2, struct resource *res);
NURB_EXPORT extern struct rt_nurb_uv_hit *rt_nurb_intersect(const struct face_g_snurb * srf,
							    plane_t plane1, plane_t plane2, double uv_tol, struct resource *res,
							    struct bu_list *plist);

/* refine.c */
NURB_EXPORT extern struct face_g_snurb *rt_nurb_s_refine(const struct face_g_snurb *srf,
							 int dir, struct knot_vector *kv, struct resource *res);
NURB_EXPORT extern struct edge_g_cnurb *rt_nurb_c_refine(const struct edge_g_cnurb * crv,
							 struct knot_vector *kv);

/* solve.c */
NURB_EXPORT extern void rt_nurb_solve(fastf_t *mat_1, fastf_t *mat_2,
				      fastf_t *solution, int dim, int coords);
NURB_EXPORT extern void rt_nurb_doolittle(fastf_t *mat_1, fastf_t *mat_2,
					  int row, int coords);
NURB_EXPORT extern void rt_nurb_forw_solve(const fastf_t *lu, const fastf_t *b,
					   fastf_t *y, int n);
NURB_EXPORT extern void rt_nurb_back_solve(const fastf_t *lu, const fastf_t *y,
					   fastf_t *x, int n);
NURB_EXPORT extern void rt_nurb_p_mat(const fastf_t * mat, int dim);

/* split.c */
NURB_EXPORT extern void rt_nurb_s_split(struct bu_list *split_hd, const struct face_g_snurb *srf,
					int dir, struct resource *res);
NURB_EXPORT extern void rt_nurb_c_split(struct bu_list *split_hd, const struct edge_g_cnurb *crv);

/* util.c */
NURB_EXPORT extern struct face_g_snurb *rt_nurb_new_snurb(int u_order, int v_order,
							  int n_u_knots, int n_v_knots,
							  int n_rows, int n_cols, int pt_type, struct resource *res);
NURB_EXPORT extern struct edge_g_cnurb *rt_nurb_new_cnurb(int order, int n_knots,
							  int n_pts, int pt_type);
NURB_EXPORT extern void rt_nurb_free_snurb(struct face_g_snurb *srf, struct resource *res);
NURB_EXPORT extern void rt_nurb_free_cnurb(struct edge_g_cnurb * crv);
NURB_EXPORT extern void rt_nurb_c_print(const struct edge_g_cnurb *crv);
NURB_EXPORT extern void rt_nurb_s_print(char *c, const struct face_g_snurb *srf);
NURB_EXPORT extern void rt_nurb_pr_kv(const struct knot_vector *kv);
NURB_EXPORT extern void rt_nurb_pr_mesh(const struct face_g_snurb *m);
NURB_EXPORT extern void rt_nurb_print_pt_type(int c);
NURB_EXPORT extern void rt_nurb_clean_cnurb(struct edge_g_cnurb *crv);

/* xsplit.c */
NURB_EXPORT extern struct face_g_snurb *rt_nurb_s_xsplit(struct face_g_snurb *srf, fastf_t param, int dir);
NURB_EXPORT extern struct edge_g_cnurb *rt_nurb_c_xsplit(struct edge_g_cnurb *crv, fastf_t param);

__END_DECLS

#endif /* NURB_H */

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
