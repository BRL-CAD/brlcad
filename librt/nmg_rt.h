/*
 *			N M G _ R T . H
 *
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 *
 *  $Header$
 */

/* defining the following flag will improve NMG raytrace speed by eliminating some checking
 * Use with CAUTION!!! */
#define FAST_NMG	1

#define NMG_HIT_LIST	0
#define NMG_MISS_LIST	1
#define NMG_RT_HIT_MAGIC 0x48697400	/* "Hit" */
#define NMG_RT_HIT_SUB_MAGIC 0x48696d00	/* "Him" */
#define NMG_RT_MISS_MAGIC 0x4d697300	/* "Mis" */


/* These values are for the hitmiss "in_out" variable and indicate the
 * nature of the hit when known
 */
#define HMG_INBOUND_STATE(_hm) (((_hm)->in_out & 0x0f0) >> 4)
#define HMG_OUTBOUND_STATE(_hm) ((_hm)->in_out & 0x0f)


#define NMG_RAY_STATE_INSIDE	1
#define NMG_RAY_STATE_ON	2
#define NMG_RAY_STATE_OUTSIDE	4

#define HMG_HIT_IN_IN	0x11	/* hit internal structure */
#define HMG_HIT_IN_OUT	0x14	/* breaking out */
#define HMG_HIT_OUT_IN	0x41	/* breaking in */
#define HMG_HIT_OUT_OUT 0x44	/* edge/vertex graze */
#define HMG_HIT_IN_ON	0x12
#define HMG_HIT_ON_IN	0x21
#define HMG_HIT_ON_ON	0x22
#define HMG_HIT_OUT_ON	0x42
#define HMG_HIT_ON_OUT	0x24
#define HMG_HIT_ANY_ANY	0x88	/* hit on non-3-mainifold */

#define	NMG_VERT_ENTER 1
#define NMG_VERT_ENTER_LEAVE 0
#define NMG_VERT_LEAVE -1
#define NMG_VERT_UNKNOWN -2

#define NMG_HITMISS_SEG_IN 0x696e00	/* "in" */
#define NMG_HITMISS_SEG_OUT 0x6f757400	/* "out" */

struct hitmiss {
	struct rt_list	l;
	struct hit	hit;
	fastf_t		dist_in_plane;	/* distance from plane intersect */
	int		in_out;		/* status of ray as it transitions
					 * this hit point.
					 */
	long		*inbound_use;
	vect_t		inbound_norm;
	long		*outbound_use;
	vect_t		outbound_norm;
	int		start_stop;	/* is this a seg_in or seg_out */
	struct hitmiss	*other;		/* for keeping track of the other
					 * end of the segment when we know
					 * it
					 */
};

#define NMG_CK_HITMISS(hm) {\
	switch (hm->l.magic) { \
	case NMG_RT_HIT_MAGIC: \
	case NMG_RT_HIT_SUB_MAGIC: \
	case NMG_RT_MISS_MAGIC: \
		break; \
	case NMG_MISS_LIST: \
		rt_log("%s[%d]: struct hitmiss has  NMG_MISS_LIST magic #\n",\
			__FILE__, __LINE__); \
		rt_bomb("going down in flames\n"); \
	case NMG_HIT_LIST: \
		rt_log("%s[%d]: struct hitmiss has  NMG_MISS_LIST magic #\n",\
			__FILE__, __LINE__); \
		rt_bomb("going down in flames\n"); \
	default: \
		rt_log("%s[%d]: bad struct hitmiss magic: %d:(0x%08x)\n", \
			__FILE__, __LINE__, hm->l.magic, hm->l.magic); \
		rt_bomb("going down in flames\n"); \
	}\
	if (!hm->hit.hit_private) { \
		rt_log("%s[%d]: NULL hit_private in hitmiss struct\n", \
			__FILE__, __LINE__); \
		rt_bomb("going down in flames\n"); \
	} \
}

#define NMG_CK_HITMISS_LISTS(a_hit, rd) { \
    for (RT_LIST_FOR(a_hit, hitmiss, &rd->rd_hit)){NMG_CK_HITMISS(a_hit);} \
    for (RT_LIST_FOR(a_hit, hitmiss, &rd->rd_miss)){NMG_CK_HITMISS(a_hit);} }


/*	Ray Data structure
 *
 * A)	the hitmiss table has one element for each nmg structure in the
 *	nmgmodel.  The table keeps track of which elements have been
 *	processed before and which haven't.  Elements in this table
 *	will either be:
 *		(NULL)		item not previously processed
 *		hitmiss ptr	item previously processed
 *
 *	the 0th item in the array is a pointer to the head of the "hit"
 *	list.  The 1th item in the array is a pointer to the head of the
 *	"miss" list.
 *
 * B)	If plane_pt is non-null then we are currently processing a face
 *	intersection.  The plane_dist and ray_dist_to_plane are valid.
 *	The ray/edge intersector should check the distance from the plane
 *	intercept to the edge and update "plane_closest" if the current
 *	edge is closer to the intercept than the previous closest object.
 */
#define NMG_PCA_EDGE	1
#define NMG_PCA_EDGE_VERTEX 2
#define NMG_PCA_VERTEX 3
#define NMG_RAY_DATA_MAGIC 0x1651771
#define NMG_CK_RD(_rd) NMG_CKMAG(_rd, NMG_RAY_DATA_MAGIC, "ray data");
struct ray_data {
	long magic;
	struct model		*rd_m;
	char			*manifolds; /*  structure 1-3manifold table */
	vect_t			rd_invdir;
	struct xray		*rp;
	struct application	*ap;
	struct seg		*seghead;
	struct soltab 		*stp;
	struct rt_tol		*tol;
	struct hitmiss	**hitmiss;	/* 1 struct hitmiss ptr per elem. */
	struct rt_list	rd_hit;		/* list of hit elements */
	struct rt_list	rd_miss;	/* list of missed/sub-hit elements */

/* The following are to support isect_ray_face() */

	/* plane_pt is the intercept point of the ray with the plane of the
	 * face.
	 */
	pointp_t	plane_pt;	/* ray/plane(face) intercept point */

	/* ray_dist_to_plane is the parametric distance along the ray from
	 * the ray origin (rd->rp->r_pt) to the ray/plane intercept point
	 */
	fastf_t		ray_dist_to_plane; /* ray parametric dist to plane */

	/* the "face_subhit" element is a boolean used by isect_ray_face
	 * and [e|v]u_touch_func to record the fact that the ray/(plane/face)
	 * intercept point was within tolerance of an edge/vertex of the face.
	 * In such instances, isect_ray_face does NOT need to generate a hit 
	 * point for the face, as the hit point for the edge/vertex will 
	 * suffice.
	 */
	int		face_subhit;	

	/* the "classifying_ray" flag indicates that this ray is being used to
	 * classify a point, so that the "eu_touch" and "vu_touch" functions
	 * should not be called.
	 */
	int		classifying_ray;
};


#if FAST_NMG
#define GET_HITMISS(_p) { \
	(_p) = (struct hitmiss *)rt_calloc(1, sizeof(struct hitmiss), "hitmiss"); \
	}

#define FREE_HITMISS(_p) { \
	(void)rt_free( (char *)_p,  "hitmiss"); \
	}

#define NMG_FREE_HITLIST(_p) { \
	struct hitmiss *_hit; \
	while ( RT_LIST_WHILE(_hit, hitmiss, _p)) { \
		RT_LIST_DEQUEUE( &_hit->l ); \
		FREE_HITMISS( _hit ); \
	} }
#else
#define GET_HITMISS(_p) { \
	char str[64]; \
	(void)sprintf(str, "GET_HITMISS %s %d", __FILE__, __LINE__); \
	(_p) = (struct hitmiss *)rt_calloc(1, sizeof(struct hitmiss), str); \
	}

#define FREE_HITMISS(_p) { \
	char str[64]; \
	(void)sprintf(str, "FREE_HITMISS %s %d", __FILE__, __LINE__); \
	(void)rt_free( (char *)_p,  str); \
	}

#define NMG_FREE_HITLIST(_p) { \
	struct hitmiss *_hit; \
	while ( RT_LIST_WHILE(_hit, hitmiss, _p)) { \
		NMG_CK_HITMISS(_hit); \
		RT_LIST_DEQUEUE( &_hit->l ); \
		FREE_HITMISS( _hit ); \
	} }

#endif

#define HIT 1	/* a hit on a face */
#define MISS 0	/* a miss on the face */


#define nmg_rt_bomb(rd, str) { \
	rt_log("%s", str); \
	if (rt_g.NMG_debug & DEBUG_NMGRT) rt_bomb("End of diagnostics"); \
	RT_LIST_INIT(&rd->rd_hit); \
	RT_LIST_INIT(&rd->rd_miss); \
	rt_g.NMG_debug |= DEBUG_NMGRT; \
	nmg_isect_ray_model(rd); \
	(void) nmg_ray_segs(rd); \
	rt_bomb("Should have bombed before this\n"); }
