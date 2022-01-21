/*                       R A Y . H
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
/** @addtogroup nmg_ray
 *
 * TODO - these structs and ray_in_rpp are versions of librt functionality,
 * and we need to think about how/where to merge them into a common function
 * and struct that are available to both libraries without introducing a
 * coupling dependency.
 */
/** @{ */
/** @file nmg/ray.h */

#ifndef NMG_RAY_H
#define NMG_RAY_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
//#include "nmg/model.h"

__BEGIN_DECLS

struct model;

NMG_EXPORT extern struct bu_list re_nmgfree;     /**< @brief  head of NMG hitmiss freelist */

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

#define NMG_CK_RD(_rd) NMG_CKMAG(_rd, NMG_RAY_DATA_MAGIC, "ray data");

#ifdef NO_BOMBING_MACROS
#  define NMG_CK_HITMISS(hm) (void)(hm)
#else
#  define NMG_CK_HITMISS(hm) \
    {\
        switch (hm->l.magic) { \
            case NMG_RT_HIT_MAGIC: \
            case NMG_RT_HIT_SUB_MAGIC: \
            case NMG_RT_MISS_MAGIC: \
                break; \
            case NMG_MISS_LIST: \
                bu_log(CPP_FILELINE ": struct hitmiss has NMG_MISS_LIST magic #\n"); \
                bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
                break; \
            case NMG_HIT_LIST: \
                bu_log(CPP_FILELINE ": struct hitmiss has NMG_MISS_LIST magic #\n"); \
                bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
                break; \
            default: \
                bu_log(CPP_FILELINE ": bad struct hitmiss magic: %u:(0x%08x)\n", \
                       hm->l.magic, hm->l.magic); \
                bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
        }\
        if (!hm->hit.hit_private) { \
            bu_log(CPP_FILELINE ": NULL hit_private in hitmiss struct\n"); \
            bu_bomb("NMG_CK_HITMISS: going down in flames\n"); \
        } \
    }
#endif

#ifdef NO_BOMBING_MACROS
#  define NMG_CK_HITMISS_LISTS(rd) (void)(rd)
#else
#  define NMG_CK_HITMISS_LISTS(rd) \
    { \
        struct nmg_hitmiss *_a_hit; \
        for (BU_LIST_FOR(_a_hit, nmg_hitmiss, &rd->rd_hit)) {NMG_CK_HITMISS(_a_hit);} \
        for (BU_LIST_FOR(_a_hit, nmg_hitmiss, &rd->rd_miss)) {NMG_CK_HITMISS(_a_hit);} \
    }
#endif

#define NMG_GET_HITMISS(_p) { \
        (_p) = BU_LIST_FIRST(nmg_hitmiss, &(re_nmgfree)); \
        if (BU_LIST_IS_HEAD((_p), &(re_nmgfree))) \
            BU_ALLOC((_p), struct nmg_hitmiss); \
        else \
            BU_LIST_DEQUEUE(&((_p)->l)); \
    }


#define NMG_FREE_HITLIST(_p) { \
        BU_CK_LIST_HEAD((_p)); \
        BU_LIST_APPEND_LIST(&(re_nmgfree), (_p)); \
    }

#ifdef NO_BOMBING_MACROS
#  define nmg_bu_bomb(rd, vlfree, str) (void)(rd)
#else
#  define nmg_bu_bomb(rd, vlfree, str) { \
        bu_log("%s", str); \
        if (nmg_debug & NMG_DEBUG_NMGRT) bu_bomb("End of diagnostics"); \
        BU_LIST_INIT(&rd->rd_hit); \
        BU_LIST_INIT(&rd->rd_miss); \
        nmg_debug |= NMG_DEBUG_NMGRT; \
        nmg_isect_ray_model(rd,vlfree); \
        bu_bomb("Should have bombed before this\n"); \
    }
#endif


#define HIT 1   /**< @brief  a hit on a face */
#define MISS 0  /**< @brief  a miss on the face */

struct nmg_ray {
    uint32_t            magic;
    point_t             r_pt;           /**< @brief Point at which ray starts */
    vect_t              r_dir;          /**< @brief Direction of ray (UNIT Length) */
    fastf_t             r_min;          /**< @brief entry dist to bounding sphere */
    fastf_t             r_max;          /**< @brief exit dist from bounding sphere */
};

struct nmg_hit {
    uint32_t            hit_magic;
    fastf_t             hit_dist;       /**< @brief dist from r_pt to hit_point */
    point_t             hit_point;      /**< @brief DEPRECATED: Intersection point, use VJOIN1 hit_dist */
    vect_t              hit_normal;     /**< @brief DEPRECATED: Surface Normal at hit_point, use RT_HIT_NORMAL */
    vect_t              hit_vpriv;      /**< @brief PRIVATE vector for xxx_*() */
    void *              hit_private;    /**< @brief PRIVATE handle for xxx_shot() */
    int                 hit_surfno;     /**< @brief solid-specific surface indicator */
    struct nmg_ray *    hit_rayp;       /**< @brief pointer to defining ray */
};

struct nmg_seg {
    struct bu_list      l;
    struct nmg_hit      seg_in;         /**< @brief IN information */
    struct nmg_hit      seg_out;        /**< @brief OUT information */
    void *              seg_stp;        /**< @brief pointer back to soltab */
};

struct nmg_hitmiss {
    struct bu_list      l;
    struct nmg_hit      hit;
    fastf_t             dist_in_plane;  /**< @brief  distance from plane intersect */
    int                 in_out;         /**< @brief  status of ray as it transitions
                                         * this hit point.
                                         */
    long                *inbound_use;
    vect_t              inbound_norm;
    long                *outbound_use;
    vect_t              outbound_norm;
    int                 start_stop;     /**< @brief  is this a seg_in or seg_out */
    struct nmg_hitmiss  *other;         /**< @brief  for keeping track of the other
                                         * end of the segment when we know
                                         * it
                                         */
};

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
struct nmg_ray_data {
    uint32_t            magic;
    struct model        *rd_m;
    char                *manifolds; /**< @brief   structure 1-3manifold table */
    vect_t              rd_invdir;
    struct nmg_ray      *rp;
    void *              *ap;
    struct nmg_seg      *seghead;
    void *              *stp;
    const struct bn_tol *tol;
    struct nmg_hitmiss  **hitmiss;      /**< @brief  1 struct hitmiss ptr per elem. */
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

int
ray_in_rpp(struct nmg_ray *rp,
           const fastf_t *invdir,       /* inverses of rp->r_dir[] */
           const fastf_t *min,
           const fastf_t *max);

NMG_EXPORT extern int nmg_class_ray_vs_shell(struct nmg_ray *rp,
                                             const struct shell *s,
                                             const int in_or_out_only,
                                             struct bu_list *vlfree,
                                             const struct bn_tol *tol);

NMG_EXPORT extern void nmg_isect_ray_model(struct nmg_ray_data *rd, struct bu_list *vlfree);

__END_DECLS

#endif  /* NMG_RAY_H */
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
