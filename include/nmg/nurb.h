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
/** @addtogroup nmg
 * @brief
 * NMG nurbs definitions 
 */
/** @{ */
/** @file nmg/nurb.h */

#ifndef NMG_NURB_H
#define NMG_NURB_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"

__BEGIN_DECLS

#define NMG_CK_EDGE_G_CNURB(_p)       NMG_CKMAG(_p, NMG_EDGE_G_CNURB_MAGIC, "edge_g_cnurb")
#define NMG_CK_EDGE_G_EITHER(_p)      NMG_CK2MAG(_p, NMG_EDGE_G_LSEG_MAGIC, NMG_EDGE_G_CNURB_MAGIC, "edge_g_lseg|edge_g_cnurb")
#define NMG_CK_VERTEXUSE_A_CNURB(_p)  NMG_CKMAG(_p, NMG_VERTEXUSE_A_CNURB_MAGIC, "vertexuse_a_cnurb")
#define NMG_CK_VERTEXUSE_A_EITHER(_p) NMG_CK2MAG(_p, NMG_VERTEXUSE_A_PLANE_MAGIC, NMG_VERTEXUSE_A_CNURB_MAGIC, "vertexuse_a_plane|vertexuse_a_cnurb")

/**
 * @brief
 * Definition of a knot vector.
 *
 * Not found independently, but used in the cnurb and snurb
 * structures.  (Exactly the same as the definition in nurb.h)
 */
struct knot_vector {
    uint32_t magic;
    int k_size;         /**< @brief knot vector size */
    fastf_t * knots;    /**< @brief pointer to knot vector */
};


struct face_g_snurb {
    /* NOTICE: l.forw & l.back *not* stored in database.  They are for
     * bspline primitive internal use only.
     */
    struct bu_list l;
    struct bu_list f_hd;        /**< @brief list of faces sharing this surface */
    int order[2];               /**< @brief surface order [0] = u, [1] = v */
    struct knot_vector u;       /**< @brief surface knot vectors */
    struct knot_vector v;       /**< @brief surface knot vectors */
    /* surface control points */
    int s_size[2];              /**< @brief mesh size, u, v */
    int pt_type;                /**< @brief surface point type */
    fastf_t *ctl_points;        /**< @brief array [size[0]*size[1]] */
    /* START OF ITEMS VALID IN-MEMORY ONLY -- NOT STORED ON DISK */
    int dir;                    /**< @brief direction of last refinement */
    point_t min_pt;             /**< @brief min corner of bounding box */
    point_t max_pt;             /**< @brief max corner of bounding box */
    /* END OF ITEMS VALID IN-MEMORY ONLY -- NOT STORED ON DISK */
    long index;                 /**< @brief struct # in this model */
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
    struct bu_list l;           /**< @brief NOTICE: l.forw & l.back are NOT stored in database.  For bspline primitive   internal use only. */
    struct bu_list eu_hd2;      /**< @brief heads l2 list of edgeuses on this curve */
    int order;                  /**< @brief Curve Order */
    struct knot_vector k;       /**< @brief curve knot vector */
    /* curve control polygon */
    int c_size;                 /**< @brief number of ctl points */
    int pt_type;                /**< @brief curve point type */
    fastf_t *ctl_points;        /**< @brief array [c_size] */
    long index;                 /**< @brief struct # in this model */
};

struct vertexuse_a_plane {
    uint32_t magic;
    vect_t N;                   /**< @brief (opt) surface Normal at vertexuse */
    long index;                 /**< @brief struct # in this model */
};

struct vertexuse_a_cnurb {
    uint32_t magic;
    fastf_t param[3];           /**< @brief (u, v, w) of vu on eu's cnurb */
    long index;                 /**< @brief struct # in this model */
};


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
