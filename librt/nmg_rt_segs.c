/*
 *			N M G _ R T _ S E G S. C
 *
 *	Support routines for raytracing an NMG.
 *
 *  Author -
 *	Lee A. Butler
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066  USA
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1993 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "./nmg_rt.h"

/*	EDGE-FACE correlation data
 * 	used in edge_hit() for 3manifold case
 */
struct ef_data {
    	fastf_t		fdotr;	/* face vector VDOT with ray */
    	fastf_t		fdotl;	/* face vector VDOT with ray-left */
    	fastf_t		ndotr;	/* face normal VDOT with ray */
    	struct edgeuse *eu;
};

static
struct structparse ef_parsetab[] = {
	{"%f", 1, "fdotr", offsetof(struct ef_data, fdotr), FUNC_NULL},
	{"%f", 1, "fdotl", offsetof(struct ef_data, fdotl), FUNC_NULL},
	{"%f", 1, "ndotr", offsetof(struct ef_data, ndotr), FUNC_NULL},
	{"%x", 1, "eu",   offsetof(struct ef_data, eu),   FUNC_NULL},
	{"", 0, (char *)NULL,	  0,			  FUNC_NULL}
};

static
struct structparse hit_parsetab[] = {
{"%f", 1, "hit_dist", offsetof(struct hit, hit_dist), FUNC_NULL},
{"%f", 3, "hit_point", offsetofarray(struct hit, hit_point), FUNC_NULL},
{"%f", 4, "hit_normal", offsetofarray(struct hit, hit_normal), FUNC_NULL},
{"%f", 3, "hit_vpriv", offsetofarray(struct hit, hit_vpriv), FUNC_NULL},
{"%x", 1, "hit_private", offsetof(struct hit, hit_private), FUNC_NULL},
{"%d", 1, "hit_surfno", offsetof(struct hit, hit_surfno), FUNC_NULL},
{"", 0, (char *)NULL,	  0,			  FUNC_NULL}
};

static
print_seg_list(seghead)
struct seg *seghead;
{
	struct seg *seg_p;

	rt_log("Segment List:\n");
	/* print debugging data before returning */
	for (RT_LIST_FOR(seg_p, seg, &seghead->l) ) {
		rt_log("dist %g  pt(%g,%g,%g)  N(%g,%g,%g)  =>  ",
		seg_p->seg_in.hit_dist,
		seg_p->seg_in.hit_point[0],
		seg_p->seg_in.hit_point[1],
		seg_p->seg_in.hit_point[2],
		seg_p->seg_in.hit_normal[0],
		seg_p->seg_in.hit_normal[1],
		seg_p->seg_in.hit_normal[2]);
		rt_log("dist %g  pt(%g,%g,%g)  N(%g,%g,%g)\n",
		seg_p->seg_out.hit_dist,
		seg_p->seg_out.hit_point[0],
		seg_p->seg_out.hit_point[1],
		seg_p->seg_out.hit_point[2],
		seg_p->seg_out.hit_normal[0],
		seg_p->seg_out.hit_normal[1],
		seg_p->seg_out.hit_normal[2]);
	}
}


/*
 *		N E X T _ S T A T E _ T A B L E
 *
 *			Current_State
 *	Input	| 0   1   2   3   4   5   6
 *	-------------------------------------
 *	O_N  =	| 1   1  C1   5   1   5   5
 *	O_N !=	| 1  1|E C1  C1  C1  C1  C1
 *	N_N  =	| E   1   E   3   E   1   6    
 *	N_N !=	| E   1   E   E   E   1   1   
 *	N_O  =	| E   2   2  ?2   E   2   2
 *	N_O !=	| E   2   2   E   E   2   2
 *	O_O  =	| 3   2   2   3   3   6   6
 *	O_O !=	| 3  2|E C3  C3  C3  C3  C3
 *	A_A  =	| 4   1   2   3   4   8   6
 *	A_A !=	| 4   1  C4  C4  C4  C4  C4
 *
 *
 *	=	-> ray dist to point within tol (along ray) of last hit point
 *	!=	-> ray dist to point outside tol (along ray) of last hit point
 *
 *
 *	State Prefix
 *	C	current segment now completed
 *	C_	Previous segment now completed
 *
 *	State
 *	E	Error termination
 *	1	"Entering" solid
 *	2	"Leaving" solid
 *	3	O_O from outside state
 *	4	A_A from outside state
 *	5	O_O=O_N		Fuzzy state 1 or state 2 from state 3
 *	6	0_0=O_N=O_O
 */










#define MAKE_SEG(in_hit, out_hit) \
	RT_GET_SEG(seg_p, ap->a_resource); \
	RT_CK_SEG(seg_p); \
	seg_p->seg_stp = stp; \
	bcopy(&in_hit->hit, &seg_p->seg_in, sizeof(struct hit)); \
	bcopy(&out_hit->hit, &seg_p->seg_out, sizeof(struct hit)); \
	seg_count++; \
	RT_LIST_INSERT(&(seghead->l), &(seg_p->l) )



static int
nmg_bsegs(rd, ap, seghead, stp)
struct ray_data		*rd;
struct application	*ap;
struct seg		*seghead;	/* intersection w/ ray */
struct soltab		*stp;
{
	int ray_state = NMG_RAY_STATE_OUTSIDE;
	struct hitmiss *in_hit = (struct hitmiss *)NULL;
	struct hitmiss *a_hit;
	int seg_count = 0;
	struct seg *seg_p;

	NMG_CK_HITMISS_LISTS(a_hit, rd);


	while (RT_LIST_NON_EMPTY(&rd->rd_hit) ) {

		a_hit = RT_LIST_FIRST(hitmiss, &rd->rd_hit);
		NMG_CK_HITMISS(a_hit);
		RT_LIST_DEQUEUE( &a_hit->l );

		if (ray_state == NMG_RAY_STATE_OUTSIDE) {
			switch (a_hit->in_out) {
			case HMG_HIT_OUT_IN:
			case HMG_HIT_OUT_ON:
				in_hit = a_hit;
				break;
			case HMG_HIT_ANY_ANY:
				/* Hit a Non-3-Manifold */
			case HMG_HIT_OUT_OUT:
				if (in_hit)
					rt_bomb("What should I have done?\n");

				/* build the seg */
				MAKE_SEG(a_hit, a_hit);
				VMOVE(seg_p->seg_in.hit_normal,
					a_hit->inbound_norm);
				VMOVE(seg_p->seg_out.hit_normal,
					a_hit->outbound_norm);

				ray_state = NMG_RAY_STATE_INSIDE;
				break;
			case HMG_HIT_IN_IN:
			case HMG_HIT_IN_ON:
			case HMG_HIT_ON_IN:
			case HMG_HIT_ON_ON:
				if (!in_hit)
					rt_bomb("How did I get in?\n");
				/* if we're already inside, just eat this */
				break;
			case HMG_HIT_IN_OUT:
			case HMG_HIT_ON_OUT:
				if (!in_hit)
					rt_bomb("How can I be leaving when I haven't entered?\n");
				/* build the seg */
				MAKE_SEG(in_hit, a_hit);
				VMOVE(seg_p->seg_in.hit_normal,
					in_hit->inbound_norm);
				VMOVE(seg_p->seg_out.hit_normal,
					a_hit->outbound_norm);

				in_hit = (struct hitmiss *)NULL;
				ray_state = NMG_RAY_STATE_INSIDE;
				break;
			default:
				rt_bomb("Bogus ray state while leaving\n");
				break;
			}
		} else { /* ray_state == NMG_RAY_STATE_INSIDE */
			switch (a_hit->in_out) {
			case HMG_HIT_IN_OUT:
			case HMG_HIT_ON_OUT:
				/* "eat" the hit point */
				break;
			case HMG_HIT_IN_IN:
			case HMG_HIT_IN_ON:
			case HMG_HIT_ON_IN:
			case HMG_HIT_ON_ON:
				rt_bomb("I've already left the NMG and this guy wants to stay inside\n");
				break;
			case HMG_HIT_OUT_IN:
			case HMG_HIT_OUT_ON:
				ray_state = NMG_RAY_STATE_OUTSIDE;
				in_hit = a_hit;
				break;

			case HMG_HIT_ANY_ANY:
			case HMG_HIT_OUT_OUT:
				ray_state = NMG_RAY_STATE_OUTSIDE;
				/* build the seg */
				MAKE_SEG(a_hit, a_hit);
				VMOVE(seg_p->seg_in.hit_normal,
					in_hit->inbound_norm);
				VMOVE(seg_p->seg_out.hit_normal,
					a_hit->outbound_norm);
				break;
			default:
				rt_bomb("Bogus ray state while leaving\n");
				break;
			}
		}
	}


	if (rt_g.NMG_debug & DEBUG_RT_SEGS && RT_LIST_NON_EMPTY(&rd->rd_miss))
		rt_log("freeing miss point(s)\n");

	while ( RT_LIST_NON_EMPTY(&rd->rd_miss) ) {
		a_hit = RT_LIST_FIRST(hitmiss, &rd->rd_miss);
		NMG_CK_HITMISS(a_hit);
		RT_LIST_DEQUEUE( &a_hit->l );
		rt_free((char *)a_hit, "freeing miss point");
	}

	return seg_count;
}



/*	N M G _ R A Y _ S E G S
 *
 *	Obtain the list of ray segments which intersect with the nmg.
 *	This routine does all of the "work" for rt_nmg_shot()
 */
int
nmg_ray_segs(rd)
struct ray_data	*rd;
{
	struct hitmiss *a_hit;
	int seg_count=0;
	static int last_miss=0;

	NMG_CK_HITMISS_LISTS(a_hit, rd);

	if (RT_LIST_IS_EMPTY(&rd->rd_hit)) {
		if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
			if (last_miss)	rt_log(".");
			else		rt_log("ray missed NMG\n");
		}
		last_miss = 1;
		return(0);			/* MISS */
	} else if (rt_g.NMG_debug & DEBUG_RT_SEGS) {

		rt_log("\n\nnmg_ray_segs(rd)\nsorted nmg/ray hit list\n");

		for (RT_LIST_FOR(a_hit, hitmiss, &rd->rd_hit))
			nmg_rt_print_hitmiss(a_hit);
	}

	last_miss = 0;

	seg_count = nmg_bsegs(rd, rd->ap, rd->seghead, rd->stp);

	if (!(rt_g.NMG_debug & DEBUG_RT_SEGS))
		return(seg_count);

	/* print debugging data before returning */
	print_seg_list(rd->seghead);

	return(seg_count);
}

