/*                          N M G . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2015 United States Government as represented by
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
/** @file rt/nmg.h
 *
 */

#ifndef RT_NMG_H
#define RT_NMG_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "bu/ptbl.h"
#include "bn/tol.h"
#include "rt/hit.h"
#include "rt/application.h"
#include "rt/soltab.h"
#include "rt/seg.h"
#include "nmg.h"

__BEGIN_DECLS

/*********************************************************************************
 *      The following section is an exact copy of what was previously "nmg_rt.h" *
 *      (with minor changes to NMG_GET_HITMISS and NMG_FREE_HITLIST              *
 *      moved here to use RTG.rtg_nmgfree freelist for hitmiss structs.         *
 ******************************************************************************* */

#define NMG_HIT_LIST    0
#define NMG_MISS_LIST   1


/* These values are for the hitmiss "in_out" variable and indicate the
 * nature of the hit when known
 */
#define HMG_INBOUND_STATE(_hm) (((_hm)->in_out & 0x0f0) >> 4)
#define HMG_OUTBOUND_STATE(_hm) ((_hm)->in_out & 0x0f)


#define NMG_RAY_STATE_INSIDE    1
#define NMG_RAY_STATE_ON        2
#define NMG_RAY_STATE_OUTSIDE   4
#define NMG_RAY_STATE_ANY       8

#define HMG_HIT_IN_IN   0x11    /**< @brief  hit internal structure */
#define HMG_HIT_IN_OUT  0x14    /**< @brief  breaking out */
#define HMG_HIT_OUT_IN  0x41    /**< @brief  breaking in */
#define HMG_HIT_OUT_OUT 0x44    /**< @brief  edge/vertex graze */
#define HMG_HIT_IN_ON   0x12
#define HMG_HIT_ON_IN   0x21
#define HMG_HIT_ON_ON   0x22
#define HMG_HIT_OUT_ON  0x42
#define HMG_HIT_ON_OUT  0x24
#define HMG_HIT_ANY_ANY 0x88    /**< @brief  hit on non-3-manifold */

#define NMG_VERT_ENTER 1
#define NMG_VERT_ENTER_LEAVE 0
#define NMG_VERT_LEAVE -1
#define NMG_VERT_UNKNOWN -2

#define NMG_HITMISS_SEG_IN 0x696e00     /**< @brief  "in" */
#define NMG_HITMISS_SEG_OUT 0x6f757400  /**< @brief  "out" */

struct hitmiss {
    struct bu_list      l;
    struct hit          hit;
    fastf_t             dist_in_plane;  /**< @brief  distance from plane intersect */
    int                 in_out;         /**< @brief  status of ray as it transitions
                                         * this hit point.
                                         */
    long                *inbound_use;
    vect_t              inbound_norm;
    long                *outbound_use;
    vect_t              outbound_norm;
    int                 start_stop;     /**< @brief  is this a seg_in or seg_out */
    struct hitmiss      *other;         /**< @brief  for keeping track of the other
                                         * end of the segment when we know
                                         * it
                                         */
};


#ifdef NO_BOMBING_MACROS
#  define NMG_CK_HITMISS(hm) BU_IGNORE((hm))
#else
#  define NMG_CK_HITMISS(hm) \
    {\
        switch (hm->l.magic) { \
            case NMG_RT_HIT_MAGIC: \
            case NMG_RT_HIT_SUB_MAGIC: \
            case NMG_RT_MISS_MAGIC: \
                break; \
            case NMG_MISS_LIST: \
                bu_log(BU_FLSTR ": struct hitmiss has NMG_MISS_LIST magic #\n"); \
                bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
            case NMG_HIT_LIST: \
                bu_log(BU_FLSTR ": struct hitmiss has NMG_MISS_LIST magic #\n"); \
                bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
            default: \
                bu_log(BU_FLSTR ": bad struct hitmiss magic: %u:(0x%08x)\n", \
                       hm->l.magic, hm->l.magic); \
                bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
        }\
        if (!hm->hit.hit_private) { \
            bu_log(BU_FLSTR ": NULL hit_private in hitmiss struct\n"); \
            bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
        } \
    }
#endif

#ifdef NO_BOMBING_MACROS
#  define NMG_CK_HITMISS_LISTS(rd) BU_IGNORE((rd))
#else
#  define NMG_CK_HITMISS_LISTS(rd) \
    { \
        struct hitmiss *_a_hit; \
        for (BU_LIST_FOR(_a_hit, hitmiss, &rd->rd_hit)) {NMG_CK_HITMISS(_a_hit);} \
        for (BU_LIST_FOR(_a_hit, hitmiss, &rd->rd_miss)) {NMG_CK_HITMISS(_a_hit);} \
    }
#endif

/**
 * Ray Data structure
 *
 * A) the hitmiss table has one element for each nmg structure in the
 * nmgmodel.  The table keeps track of which elements have been
 * processed before and which haven't.  Elements in this table will
 * either be: (NULL) item not previously processed hitmiss ptr item
 * previously processed
 *
 * the 0th item in the array is a pointer to the head of the "hit"
 * list.  The 1th item in the array is a pointer to the head of the
 * "miss" list.
 *
 * B) If plane_pt is non-null then we are currently processing a face
 * intersection.  The plane_dist and ray_dist_to_plane are valid.  The
 * ray/edge intersector should check the distance from the plane
 * intercept to the edge and update "plane_closest" if the current
 * edge is closer to the intercept than the previous closest object.
 */
struct ray_data {
    uint32_t            magic;
    struct model        *rd_m;
    char                *manifolds; /**< @brief   structure 1-3manifold table */
    vect_t              rd_invdir;
    struct xray         *rp;
    struct application  *ap;
    struct seg          *seghead;
    struct soltab       *stp;
    const struct bn_tol *tol;
    struct hitmiss      **hitmiss;      /**< @brief  1 struct hitmiss ptr per elem. */
    struct bu_list      rd_hit;         /**< @brief  list of hit elements */
    struct bu_list      rd_miss;        /**< @brief  list of missed/sub-hit elements */

/* The following are to support isect_ray_face() */

    /**
     * plane_pt is the intercept point of the ray with the plane of
     * the face.
     */
    point_t             plane_pt;       /**< @brief  ray/plane(face) intercept point */

    /**
     * ray_dist_to_plane is the parametric distance along the ray from
     * the ray origin (rd->rp->r_pt) to the ray/plane intercept point
     */
    fastf_t             ray_dist_to_plane; /**< @brief  ray parametric dist to plane */

    /**
     * the "face_subhit" element is a boolean used by isect_ray_face
     * and [e|v]u_touch_func to record the fact that the
     * ray/(plane/face) intercept point was within tolerance of an
     * edge/vertex of the face.  In such instances, isect_ray_face
     * does NOT need to generate a hit point for the face, as the hit
     * point for the edge/vertex will suffice.
     */
    int                 face_subhit;

    /**
     * the "classifying_ray" flag indicates that this ray is being
     * used to classify a point, so that the "eu_touch" and "vu_touch"
     * functions should not be called.
     */
    int                 classifying_ray;
};

#define NMG_PCA_EDGE    1
#define NMG_PCA_EDGE_VERTEX 2
#define NMG_PCA_VERTEX 3
#define NMG_CK_RD(_rd) NMG_CKMAG(_rd, NMG_RAY_DATA_MAGIC, "ray data");


#define NMG_GET_HITMISS(_p, _ap) { \
        (_p) = BU_LIST_FIRST(hitmiss, &((_ap)->a_resource->re_nmgfree)); \
        if (BU_LIST_IS_HEAD((_p), &((_ap)->a_resource->re_nmgfree))) \
            BU_ALLOC((_p), struct hitmiss); \
        else \
            BU_LIST_DEQUEUE(&((_p)->l)); \
    }


#define NMG_FREE_HITLIST(_p, _ap) { \
        BU_CK_LIST_HEAD((_p)); \
        BU_LIST_APPEND_LIST(&((_ap)->a_resource->re_nmgfree), (_p)); \
    }


#define HIT 1   /**< @brief  a hit on a face */
#define MISS 0  /**< @brief  a miss on the face */


#ifdef NO_BOMBING_MACROS
#  define nmg_bu_bomb(rd, str) BU_IGNORE((rd))
#else
#  define nmg_bu_bomb(rd, str) { \
        bu_log("%s", str); \
        if (RTG.NMG_debug & DEBUG_NMGRT) bu_bomb("End of diagnostics"); \
        BU_LIST_INIT(&rd->rd_hit); \
        BU_LIST_INIT(&rd->rd_miss); \
        RTG.NMG_debug |= DEBUG_NMGRT; \
        nmg_isect_ray_model(rd); \
        (void) nmg_ray_segs(rd); \
        bu_bomb("Should have bombed before this\n"); \
    }
#endif

struct nmg_radial {
    struct bu_list      l;
    struct edgeuse      *eu;
    struct faceuse      *fu;            /**< @brief  Derived from eu */
    struct shell        *s;             /**< @brief  Derived from eu */
    int                 existing_flag;  /**< @brief  !0 if this eu exists on dest edge */
    int                 is_crack;       /**< @brief  This eu is part of a crack. */
    int                 is_outie;       /**< @brief  This crack is an "outie" */
    int                 needs_flip;     /**< @brief  Insert eumate, not eu */
    fastf_t             ang;            /**< @brief  angle, in radians.  0 to 2pi */
};
#define NMG_CK_RADIAL(_p) NMG_CKMAG(_p, NMG_RADIAL_MAGIC, "nmg_radial")

struct nmg_inter_struct {
    uint32_t            magic;
    struct bu_ptbl      *l1;            /**< @brief  vertexuses on the line of */
    struct bu_ptbl      *l2;            /**< @brief  intersection between planes */
    fastf_t             *mag1;          /**< @brief  Distances along intersection line */
    fastf_t             *mag2;          /**< @brief  for each vertexuse in l1 and l2. */
    int                 mag_len;        /**< @brief  Array size of mag1 and mag2 */
    struct shell        *s1;
    struct shell        *s2;
    struct faceuse      *fu1;           /**< @brief  null if l1 comes from a wire */
    struct faceuse      *fu2;           /**< @brief  null if l2 comes from a wire */
    struct bn_tol       tol;
    int                 coplanar;       /**< @brief  a flag */
    struct edge_g_lseg  *on_eg;         /**< @brief  edge_g for line of intersection */
    point_t             pt;             /**< @brief  3D line of intersection */
    vect_t              dir;
    point_t             pt2d;           /**< @brief  2D projection of isect line */
    vect_t              dir2d;
    fastf_t             *vert2d;        /**< @brief  Array of 2d vertex projections [index] */
    int                 maxindex;       /**< @brief  size of vert2d[] */
    mat_t               proj;           /**< @brief  Matrix to project onto XY plane */
    const uint32_t      *twod;          /**< @brief  ptr to face/edge of 2d projection */
};
#define NMG_CK_INTER_STRUCT(_p) NMG_CKMAG(_p, NMG_INTER_STRUCT_MAGIC, "nmg_inter_struct")


__END_DECLS

#endif /* RT_NMG_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
