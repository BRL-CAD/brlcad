/*                        D E F I N E S . H
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
