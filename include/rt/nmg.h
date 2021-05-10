/*                          N M G . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
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
/** @file rt/nmg.h */

#ifndef RT_NMG_H
#define RT_NMG_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "bu/ptbl.h"
#include "bn/tol.h"
#include "bview/vlist.h"
#include "bg/defines.h"
#include "rt/geom.h"
#include "rt/hit.h"
#include "rt/seg.h"
#include "rt/application.h"
#include "rt/soltab.h"
#include "nmg.h"

__BEGIN_DECLS

struct rt_db_internal; /*forward declaration*/

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

/**
 * global nmg animation plot callback
 */
RT_EXPORT extern void (*nmg_plot_anim_upcall)(void);


/* From nmg_rt_isect.c */
RT_EXPORT extern void rt_nmg_print_hitlist(struct bu_list *hd);
RT_EXPORT extern void rt_nmg_print_hitmiss(struct hitmiss *a_hit);
RT_EXPORT extern void rt_isect_ray_model(struct ray_data *rd, struct bu_list *vlfree);

/************************************************************************
 *                                                                      *
 *                      NMG Support Function Declarations               *
 *                                                                      *
 ************************************************************************/
#if defined(NMG_H)

/* nmg.c */
RT_EXPORT extern int nmg_ray_segs(struct ray_data       *rd, struct bu_list *vlfree);

RT_EXPORT extern int nmg_to_arb(const struct model *m,
				struct rt_arb_internal *arb_int);
RT_EXPORT extern int nmg_to_tgc(const struct model *m,
				struct rt_tgc_internal *tgc_int,
				const struct bn_tol *tol);
RT_EXPORT extern int nmg_to_poly(const struct model *m,
				 struct rt_pg_internal *poly_int,
				 struct bu_list *vlfree,
				 const struct bn_tol *tol);
RT_EXPORT extern struct rt_bot_internal *nmg_bot(struct shell *s,
						 struct bu_list *vlfree,
						 const struct bn_tol *tol);
RT_EXPORT extern struct rt_bot_internal *nmg_mdl_to_bot(struct model *m,
							struct bu_list *vlfree,
							const struct bn_tol *tol);
struct db_tree_state; /* forward declaration */
RT_EXPORT extern union tree *nmg_booltree_leaf_tess(struct db_tree_state *tsp,
						    const struct db_full_path *pathp,
						    struct rt_db_internal *ip,
						    void *client_data);
RT_EXPORT extern union tree *nmg_booltree_leaf_tnurb(struct db_tree_state *tsp,
						     const struct db_full_path *pathp,
						     struct rt_db_internal *ip,
						     void *client_data);
RT_EXPORT extern int nmg_bool_eval_silent;      /* quell output from nmg_booltree_evaluate */
RT_EXPORT extern union tree *nmg_booltree_evaluate(union tree *tp,
						   struct bu_list *vlfree,
						   const struct bn_tol *tol,
						   struct resource *resp);
RT_EXPORT extern int nmg_boolean(union tree *tp,
				 struct model *m,
				 struct bu_list *vlfree,
				 const struct bn_tol *tol,
				 struct resource *resp);




/*  nmg_tri_mc.c */
RT_EXPORT extern void nmg_triangulate_model_mc(struct model *m,
					       const struct bn_tol *tol);
RT_EXPORT extern int nmg_mc_realize_cube(struct shell *s,
					 int pv,
					 point_t *edges,
					 const struct bn_tol *tol);
RT_EXPORT extern int nmg_mc_evaluate(struct shell *s,
				     struct rt_i *rtip,
				     const struct db_full_path *pathp,
				     const struct bg_tess_tol *ttol,
				     const struct bn_tol *tol);


/* nmg.c */
RT_EXPORT extern void nmg_stash_model_to_file(const char *filename,
					      const struct model *m,
					      const char *title);


#endif

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
