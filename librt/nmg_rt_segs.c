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
#include <math.h>
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

static CONST 
struct structparse rt_ef_parsetab[] = {
	{"%f", 1, "fdotr", offsetof(struct ef_data, fdotr), FUNC_NULL},
	{"%f", 1, "fdotl", offsetof(struct ef_data, fdotl), FUNC_NULL},
	{"%f", 1, "ndotr", offsetof(struct ef_data, ndotr), FUNC_NULL},
	{"%x", 1, "eu",   offsetof(struct ef_data, eu),   FUNC_NULL},
	{"", 0, (char *)NULL,	  0,			  FUNC_NULL}
};

static CONST
struct structparse rt_hit_parsetab[] = {
{"%f", 1, "hit_dist", offsetof(struct hit, hit_dist), FUNC_NULL},
{"%f", 3, "hit_point", offsetofarray(struct hit, hit_point), FUNC_NULL},
{"%f", 4, "hit_normal", offsetofarray(struct hit, hit_normal), FUNC_NULL},
{"%f", 3, "hit_vpriv", offsetofarray(struct hit, hit_vpriv), FUNC_NULL},
{"%x", 1, "hit_private", offsetof(struct hit, hit_private), FUNC_NULL},
{"%d", 1, "hit_surfno", offsetof(struct hit, hit_surfno), FUNC_NULL},
{"", 0, (char *)NULL,	  0,			  FUNC_NULL}
};


#define CK_SEGP(_p) if ( !(_p) || !(*(_p)) ) {\
	rt_log("%s[line:%d]: Bad seg_p pointer\n", __FILE__, __LINE__); \
	rt_bomb("Goodbye"); }


static
print_seg_list(seghead, seg_count, s)
struct seg *seghead;
int seg_count;
char *s;
{
	struct seg *seg_p;

	rt_log("Segment List (%d segnemts) (%s):\n", seg_count, s);
	/* print debugging data before returning */
	rt_log("Seghead:\n0x%08x magic: 0x%0x(%d) forw:0x%08x back:0x%08x\n\n",
			seghead,
			seghead->l.magic,
			seghead->l.forw,
			seghead->l.back);

	for (RT_LIST_FOR(seg_p, seg, &seghead->l) ) {
		rt_log("0x%08x magic: 0x%0x(%d) forw:0x%08x back:0x%08x\n",
			seg_p,
			seg_p->l.magic,
			seg_p->l.forw,
			seg_p->l.back);
		rt_log("dist %g  pt(%g,%g,%g)  N(%g,%g,%g)  =>\n",
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
 *	Input	|  0   1     2    3    4    5    6
 *	-------------------------------------------
 *	O_N  =	| i1   1   Ci1    5    1    5    5
 *	O_N !=	| i1  1|E  Ci1   C1  Ci1  Ci1  Ci1
 *	N_N  =	|  E   1     E    3    E    1    1
 *	N_N !=	|  E   1     E    E    E    1    1   
 *	N_O  =	|  E  o2    o2  ?o2    E   o2   o2
 *	N_O !=	|  E  o2    o2    E    E   o2   o2
 *	O_O  =	|io3  o2    o2    3    3    6    6
 *	O_O !=	|io3 o2|E Cio3 Cio3 Cio3 Cio3 Cio3
 *	A_A  =	|io4   1     2    3    4    5    6
 *	A_A !=	|io4   1  Cio4 Cio4 Cio4 Cio4 Cio4
 *	EOI     |  N   E    CN   CN   CN   CN   CN   
 *
 *	=	-> ray dist to point within tol (along ray) of last hit point
 *	!=	-> ray dist to point outside tol (along ray) of last hit point
 *
 *	State Prefix
 *	C	segment now completed, add to list & alloc new segment
 *	i	set inpoint for current segment
 *	o	set outpoint for current segment
 *	
 *	State
 *	E	Error termination
 *	N	Normal termination
 *	1	"Entering" solid
 *	2	"Leaving" solid
 *	3	O_O from outside state
 *	4	A_A from outside state
 *	5	O_O=O_N		Fuzzy state 1 or state 2 from state 3
 *	6	0_0=O_N=O_O
 */


static void
set_inpoint(seg_p, a_hit, stp, ap)
struct seg	**seg_p;	/* The segment we're building */
struct hitmiss	*a_hit;		/* The input hit point */
struct soltab	*stp;
struct application	*ap;
{
	if ( !seg_p ) {
		rt_log("%s[line:%d]: Null pointer to segment pointer\n",
			__FILE__, __LINE__);
		rt_bomb("Goodbye");
	}
		
	/* if we don't have a seg struct yet, get one */
	if ( *seg_p == (struct seg *)NULL ) {
		RT_GET_SEG(*seg_p, ap->a_resource);
		(*seg_p)->seg_stp = stp;
	}

	/* copy the "in" hit */
	bcopy(&a_hit->hit, &(*seg_p)->seg_in, sizeof(struct hit));

	/* copy the normal */
	VMOVE((*seg_p)->seg_in.hit_normal, a_hit->inbound_norm);

	if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
		rt_log("Set seg_in:\n\tdist %g  pt(%g,%g,%g)  N(%g,%g,%g)\n",
		(*seg_p)->seg_in.hit_dist,
		(*seg_p)->seg_in.hit_point[0],
		(*seg_p)->seg_in.hit_point[1],
		(*seg_p)->seg_in.hit_point[2],
		(*seg_p)->seg_in.hit_normal[0],
		(*seg_p)->seg_in.hit_normal[1],
		(*seg_p)->seg_in.hit_normal[2]);
	}
}

static void
set_outpoint(seg_p, a_hit)
struct seg	**seg_p;	/* The segment we're building */
struct hitmiss	*a_hit;		/* The input hit point */
{
	if ( !seg_p ) {
		rt_log("%s[line:%d]: Null pointer to segment pointer\n",
			__FILE__, __LINE__);
		rt_bomb("Goodbye");
	}
		
	/* if we don't have a seg struct yet, get one */
	if ( *seg_p == (struct seg *)NULL )
		rt_bomb("bad seg pointer\n");

	/* copy the "out" hit */
	bcopy(&a_hit->hit, &(*seg_p)->seg_out, sizeof(struct hit));

	/* copy the normal */
	VMOVE((*seg_p)->seg_out.hit_normal, a_hit->outbound_norm);

	if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
		rt_log("Set seg_out:\n\tdist %g  pt(%g,%g,%g)  N(%g,%g,%g)  =>\n",
		(*seg_p)->seg_in.hit_dist,
		(*seg_p)->seg_in.hit_point[0],
		(*seg_p)->seg_in.hit_point[1],
		(*seg_p)->seg_in.hit_point[2],
		(*seg_p)->seg_in.hit_normal[0],
		(*seg_p)->seg_in.hit_normal[1],
		(*seg_p)->seg_in.hit_normal[2]);
		rt_log("\tdist %g  pt(%g,%g,%g)  N(%g,%g,%g)\n",
		(*seg_p)->seg_out.hit_dist,
		(*seg_p)->seg_out.hit_point[0],
		(*seg_p)->seg_out.hit_point[1],
		(*seg_p)->seg_out.hit_point[2],
		(*seg_p)->seg_out.hit_normal[0],
		(*seg_p)->seg_out.hit_normal[1],
		(*seg_p)->seg_out.hit_normal[2]);
	}
}



static int
state0(seghead, seg_p, seg_count, a_hit, stp, ap, tol)
struct seg	*seghead;	/* intersection w/ ray */
struct seg	**seg_p;	/* The segment we're building */
int		*seg_count;	/* The number of valid segments built */
struct hitmiss	*a_hit;		/* The input hit point */
struct soltab	*stp;
struct application *ap;
struct rt_tol	*tol;
{
	int ret_val = -1;

	switch (a_hit->in_out) {
	case HMG_HIT_OUT_IN:
	case HMG_HIT_OUT_ON:
		/* save the in-hit point */
		set_inpoint(seg_p, a_hit, stp, ap);
		ret_val = 1;
		break;
	case HMG_HIT_ON_ON:
	case HMG_HIT_IN_IN:
	case HMG_HIT_ON_IN:
	case HMG_HIT_IN_ON:
	case HMG_HIT_IN_OUT:
	case HMG_HIT_ON_OUT:
		/* error */
		rt_log("%s[line:%d]: State transition error: exit without entry.\n",
			__FILE__, __LINE__);
		ret_val = -2;
		break;
	case HMG_HIT_OUT_OUT:
		/* Save the in/out points */
		set_inpoint(seg_p, a_hit, stp, ap);
		set_outpoint(seg_p, a_hit);
		ret_val = 3;
		break;
	case HMG_HIT_ANY_ANY:
		/* Save the in/out points */
		set_inpoint(seg_p, a_hit, stp, ap);
		set_outpoint(seg_p, a_hit);
		ret_val = 4;
		break;
	default:
		rt_log("%s[line:%d]: bogus hit in/out status\n",
			__FILE__, __LINE__);
		rt_bomb("Goodbye\n");
		break;
	}

	return ret_val;
}

static int
state1(seghead, seg_p, seg_count, a_hit, stp, ap, tol)
struct seg	*seghead;	/* intersection w/ ray */
struct seg	**seg_p;	/* The segment we're building */
int		*seg_count;	/* The number of valid segments built */
struct hitmiss	*a_hit;		/* The input hit point */
struct soltab	*stp;
struct application *ap;
struct rt_tol	*tol;
{
	int ret_val = -1;

	switch (a_hit->in_out) {
	case HMG_HIT_OUT_ON:
	case HMG_HIT_OUT_IN:
	case HMG_HIT_IN_IN:
	case HMG_HIT_ON_IN:
	case HMG_HIT_IN_ON:
	case HMG_HIT_ON_ON:
		ret_val = 1;
		break;
	case HMG_HIT_ON_OUT:
	case HMG_HIT_IN_OUT:
		set_outpoint(seg_p, a_hit);
		ret_val = 2;
		break;
	case HMG_HIT_OUT_OUT:
		/* XXX possibly an error condition if not within tol */
		set_outpoint(seg_p, a_hit);
		ret_val = 2;
		break;
	case HMG_HIT_ANY_ANY:
		ret_val = 1;
		break;
	default:
		rt_log("%s[line:%d]: bogus hit in/out status\n",
			__FILE__, __LINE__);
		rt_bomb("Goodbye\n");
		break;
	}

	return ret_val;
}

static int
state2(seghead, seg_p, seg_count, a_hit, stp, ap, tol)
struct seg	*seghead;	/* intersection w/ ray */
struct seg	**seg_p;	/* The segment we're building */
int		*seg_count;	/* The number of valid segments built */
struct hitmiss	*a_hit;		/* The input hit point */
struct soltab	*stp;
struct application *ap;
struct rt_tol	*tol;
{
	int ret_val = -1;
	double delta;

	switch (a_hit->in_out) {
	case HMG_HIT_OUT_ON:
	case HMG_HIT_OUT_IN:
		/* Segment completed.  Insert into segment list */
		RT_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
		RT_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
		(*seg_count)++;

		/* start new segment */
		(*seg_p) = (struct seg *)NULL;
		set_inpoint(seg_p, a_hit, stp, ap);

		ret_val = 1;
		break;
	case HMG_HIT_IN_IN:
	case HMG_HIT_ON_ON:
	case HMG_HIT_ON_IN:
	case HMG_HIT_IN_ON:
		/* Error */
		rt_log("%s[line:%d]: State transition error.\n",
			__FILE__, __LINE__);
		ret_val = -2;
		break;
	case HMG_HIT_ON_OUT:
	case HMG_HIT_IN_OUT:
		/* progress the out-point */
		set_outpoint(seg_p, a_hit);
		ret_val = 2;
		break;
	case HMG_HIT_OUT_OUT:
		CK_SEGP(seg_p);
		RT_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if ( delta < tol->dist) {
			set_outpoint(seg_p, a_hit);
			ret_val = 2;
			break;
		}
		/* complete the segment */
		RT_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
		RT_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
		(*seg_count)++;

		/* start new segment */
		(*seg_p) = (struct seg *)NULL;
		set_inpoint(seg_p, a_hit, stp, ap);
		set_outpoint(seg_p, a_hit);

		ret_val = 3;
		break;
	case HMG_HIT_ANY_ANY:
		CK_SEGP(seg_p);
		RT_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if ( delta < tol->dist) {
			set_outpoint(seg_p, a_hit);
			ret_val = 2;
			break;
		}
		/* complete the segment */
		RT_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
		RT_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
		(*seg_count)++;

		/* start new segment */
		(*seg_p) = (struct seg *)NULL;
		set_inpoint(seg_p, a_hit, stp, ap);
		set_outpoint(seg_p, a_hit);

		ret_val = 4;
		break;
	default:
		rt_log("%s[line:%d]: bogus hit in/out status\n",
			__FILE__, __LINE__);
		rt_bomb("Goodbye\n");
		break;
	}

	return ret_val;
}

static int
state3(seghead, seg_p, seg_count, a_hit, stp, ap, tol)
struct seg	*seghead;	/* intersection w/ ray */
struct seg	**seg_p;	/* The segment we're building */
int		*seg_count;	/* The number of valid segments built */
struct hitmiss	*a_hit;		/* The input hit point */
struct soltab	*stp;
struct application *ap;
struct rt_tol	*tol;
{
	int ret_val = -1;
	double delta;

	CK_SEGP(seg_p);
	RT_CK_TOL(tol);
	delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);

	switch (a_hit->in_out) {
	case HMG_HIT_OUT_ON:
	case HMG_HIT_OUT_IN:
		if ( delta < tol->dist) {
			ret_val = 5;
		} else {
			/* complete the segment */
			RT_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			RT_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
			(*seg_count)++;

			/* start new segment */
			(*seg_p) = (struct seg *)NULL;
			set_inpoint(seg_p, a_hit, stp, ap);

			ret_val = 1;
		}
		break;
	case HMG_HIT_IN_IN:
	case HMG_HIT_ON_IN:
	case HMG_HIT_IN_ON:
	case HMG_HIT_ON_ON:
		if ( delta < tol->dist) {
			ret_val = 3;
		} else {
			/* Error */
			rt_log("%s[line:%d]: State transition error.\n",
				__FILE__, __LINE__);
			ret_val = -2;
		}
		break;
	case HMG_HIT_ON_OUT:
	case HMG_HIT_IN_OUT:
		/*
		 * This can happen when the ray hits an edge/vertex and
		 * (due to floating point fuzz) also appears to have a 
		 * hit point in the area of a face:
		 *
		 * ------->o
		 *	  / \------------>
		 *	 /   \
		 *
		 */
		set_outpoint(seg_p, a_hit);
		ret_val = 2;
		break;
	case HMG_HIT_OUT_OUT:
		if (delta > tol->dist) {
			/* complete the segment */
			RT_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			RT_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
			(*seg_count)++;

			/* start new segment */
			(*seg_p) = (struct seg *)NULL;
			set_inpoint(seg_p, a_hit, stp, ap);
			set_outpoint(seg_p, a_hit);
		}
		ret_val = 3;
		break;
	case HMG_HIT_ANY_ANY:
		if (delta < tol->dist) {
			ret_val = 3;
		} else {
			/* complete the segment */
			RT_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			RT_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
			(*seg_count)++;

			/* start new segment */
			(*seg_p) = (struct seg *)NULL;
			set_inpoint(seg_p, a_hit, stp, ap);
			ret_val = 4;
		}
		break;
	default:
		rt_log("%s[line:%d]: bogus hit in/out status\n",
			__FILE__, __LINE__);
		rt_bomb("Goodbye\n");
		break;
	}

	return ret_val;
}

static int
state4(seghead, seg_p, seg_count, a_hit, stp, ap, tol)
struct seg	*seghead;	/* intersection w/ ray */
struct seg	**seg_p;	/* The segment we're building */
int		*seg_count;	/* The number of valid segments built */
struct hitmiss	*a_hit;		/* The input hit point */
struct soltab	*stp;
struct application *ap;
struct rt_tol	*tol;
{
	int ret_val = -1;
	double delta;

	switch (a_hit->in_out) {
	case HMG_HIT_OUT_ON:
	case HMG_HIT_OUT_IN:
		CK_SEGP(seg_p);
		RT_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta > tol->dist) {
			/* complete the segment */
			RT_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			RT_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
			(*seg_count)++;

			/* start new segment */
			(*seg_p) = (struct seg *)NULL;
			set_inpoint(seg_p, a_hit, stp, ap);
		}
		ret_val = 1;
		break;
	case HMG_HIT_IN_IN:
	case HMG_HIT_ON_IN:
	case HMG_HIT_IN_ON:
	case HMG_HIT_ON_ON:
	case HMG_HIT_ON_OUT:
	case HMG_HIT_IN_OUT:
		/* Error */
		rt_log("%s[line:%d]: State transition error.\n",
			__FILE__, __LINE__);
		ret_val = -2;
		break;
	case HMG_HIT_OUT_OUT:
		CK_SEGP(seg_p);
		RT_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta > tol->dist) {
			/* complete the segment */
			RT_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			RT_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
			(*seg_count)++;

			/* start new segment */
			(*seg_p) = (struct seg *)NULL;
			set_inpoint(seg_p, a_hit, stp, ap);
			set_outpoint(seg_p, a_hit);
		}
		ret_val = 3;
		break;
	case HMG_HIT_ANY_ANY:
		CK_SEGP(seg_p);
		RT_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta > tol->dist) {
			/* complete the segment */
			RT_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			RT_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
			(*seg_count)++;

			/* start new segment */
			(*seg_p) = (struct seg *)NULL;
			set_inpoint(seg_p, a_hit, stp, ap);
			set_outpoint(seg_p, a_hit);
		}
		ret_val = 4;
		break;
	default:
		rt_log("%s[line:%d]: bogus hit in/out status\n",
			__FILE__, __LINE__);
		rt_bomb("Goodbye\n");
		break;
	}

	return ret_val;
}

static int
state5(seghead, seg_p, seg_count, a_hit, stp, ap, tol)
struct seg	*seghead;	/* intersection w/ ray */
struct seg	**seg_p;	/* The segment we're building */
int		*seg_count;	/* The number of valid segments built */
struct hitmiss	*a_hit;		/* The input hit point */
struct soltab	*stp;
struct application *ap;
struct rt_tol	*tol;
{
	int ret_val = -1;
	double delta;

	switch (a_hit->in_out) {
	case HMG_HIT_OUT_ON:
	case HMG_HIT_OUT_IN:
		CK_SEGP(seg_p);
		RT_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta < tol->dist) {
			ret_val = 5;
		} else {
			/* complete the segment */
			RT_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			RT_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
			(*seg_count)++;

			/* start new segment */
			(*seg_p) = (struct seg *)NULL;
			set_inpoint(seg_p, a_hit, stp, ap);
			ret_val = 1;
		}
		break;
	case HMG_HIT_IN_IN:
	case HMG_HIT_ON_IN:
	case HMG_HIT_IN_ON:
	case HMG_HIT_ON_ON:
		ret_val = 1;
		break;
	case HMG_HIT_ON_OUT:
	case HMG_HIT_IN_OUT:
		set_outpoint(seg_p, a_hit);
		ret_val = 2;
		break;
	case HMG_HIT_OUT_OUT:
		CK_SEGP(seg_p);
		RT_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta < tol->dist) {
			ret_val = 6;
		} else {
			/* complete the segment */
			RT_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			RT_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
			(*seg_count)++;

			/* start new segment */
			(*seg_p) = (struct seg *)NULL;
			set_inpoint(seg_p, a_hit, stp, ap);
			set_outpoint(seg_p, a_hit);
			ret_val = 3;
		}
		break;
	case HMG_HIT_ANY_ANY:
		CK_SEGP(seg_p);
		RT_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta < tol->dist) {
			ret_val = 5;
		} else {
			/* complete the segment */
			RT_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			RT_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
			(*seg_count)++;

			/* start new segment */
			(*seg_p) = (struct seg *)NULL;
			set_inpoint(seg_p, a_hit, stp, ap);
			set_outpoint(seg_p, a_hit);
			ret_val = 4;
		}
		break;
	default:
		rt_log("%s[line:%d]: bogus hit in/out status\n",
			__FILE__, __LINE__);
		rt_bomb("Goodbye\n");
		break;
	}

	return ret_val;
}

static int
state6(seghead, seg_p, seg_count, a_hit, stp, ap, tol)
struct seg	*seghead;	/* intersection w/ ray */
struct seg	**seg_p;	/* The segment we're building */
int		*seg_count;	/* The number of valid segments built */
struct hitmiss	*a_hit;		/* The input hit point */
struct soltab	*stp;
struct application *ap;
struct rt_tol	*tol;
{
	int ret_val = -1;
	double delta;

	switch (a_hit->in_out) {
	case HMG_HIT_OUT_ON:
	case HMG_HIT_OUT_IN:
		CK_SEGP(seg_p);
		RT_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta < tol->dist) {
			ret_val = 5;
		} else {
			/* complete the segment */
			RT_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			RT_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
			(*seg_count)++;

			/* start new segment */
			(*seg_p) = (struct seg *)NULL;
			set_inpoint(seg_p, a_hit, stp, ap);
			ret_val = 1;
		}
		break;
	case HMG_HIT_IN_IN:
	case HMG_HIT_ON_IN:
	case HMG_HIT_IN_ON:
	case HMG_HIT_ON_ON:
		ret_val = 1;
		break;
	case HMG_HIT_ON_OUT:
	case HMG_HIT_IN_OUT:
		set_outpoint(seg_p, a_hit);
		ret_val = 2;
		break;
	case HMG_HIT_OUT_OUT:
		CK_SEGP(seg_p);
		RT_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta < tol->dist) {
			ret_val = 6;
		} else {
			/* complete the segment */
			RT_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			RT_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
			(*seg_count)++;

			/* start new segment */
			(*seg_p) = (struct seg *)NULL;
			set_inpoint(seg_p, a_hit, stp, ap);
			set_outpoint(seg_p, a_hit);
			ret_val = 3;
		}
		break;
	case HMG_HIT_ANY_ANY:
		CK_SEGP(seg_p);
		RT_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta < tol->dist) {
			ret_val = 6;
		} else {
			/* complete the segment */
			RT_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			RT_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
			(*seg_count)++;

			/* start new segment */
			(*seg_p) = (struct seg *)NULL;
			set_inpoint(seg_p, a_hit, stp, ap);
			set_outpoint(seg_p, a_hit);
			ret_val = 4;
		}
		break;
	default:
		rt_log("%s[line:%d]: bogus hit in/out status\n",
			__FILE__, __LINE__);
		rt_bomb("Goodbye\n");
		break;
	}

	return ret_val;
}


static int (*state_table[7])() = {
	state0, state1, state2, state3,
	state4, state5, state6
};


static int
nmg_bsegs(rd, ap, seghead, stp)
struct ray_data		*rd;
struct application	*ap;
struct seg		*seghead;	/* intersection w/ ray */
struct soltab		*stp;
{
	int ray_state = 0;
	int new_state;
	struct hitmiss *a_hit = (struct hitmiss *)NULL;
	struct hitmiss *hm = (struct hitmiss *)NULL;
	struct seg *seg_p = (struct seg *)NULL;
	int seg_count = 0;

	NMG_CK_HITMISS_LISTS(a_hit, rd);

	for (RT_LIST_FOR(a_hit, hitmiss, &rd->rd_hit)) {
		NMG_CK_HITMISS(a_hit);

		new_state = state_table[ray_state](seghead, &seg_p,
							&seg_count, a_hit,
							stp, ap, rd->tol);
		if (new_state < 0) {
			/* state transition error.  Print out the hit list
			 * and indicate where we were in processing it.
			 */
			for (RT_LIST_FOR(hm, hitmiss, &rd->rd_hit)) {
				if (hm == a_hit) {
					rt_log("======= State %d ======\n",
						ray_state);
					nmg_rt_print_hitmiss(hm);
					rt_log("================\n");
				} else 
					nmg_rt_print_hitmiss(hm);
			}

			/* Now bomb off */
			rt_bomb("Goodbye\n");
		}

		ray_state = new_state;
	}

	/* Check to make sure the input ran out in the right place 
	 * in the state table.
	 */
	if (ray_state == 1) {
		rt_log("%s[line:%d]: Input ended at non-terminal FSM state\n",
			__FILE__, __LINE__);
		rt_bomb("Goodbye");
	}

	/* Insert the last segment if appropriate */
	if (ray_state > 1) {
		/* complete the segment */
		RT_LIST_MAGIC_SET( &( seg_p->l ), RT_SEG_MAGIC);
		RT_LIST_INSERT(&(seghead->l), &(seg_p->l) );
		seg_count++;
	}

	return seg_count;
}

/*
 *	If a_tbl and next_tbl have an element in common, return it.
 *	Otherwise return a NULL pointer.
 */
static long *
common_topo(a_tbl, next_tbl)
struct nmg_ptbl *a_tbl;
struct nmg_ptbl *next_tbl;
{
	long **p;

	for (p = &a_tbl->buffer[a_tbl->end] ; p >= a_tbl->buffer ; p--) {
		if (nmg_tbl(next_tbl, TBL_LOC, *p) >= 0)
			return *p;
	}

	return (long *)NULL;
}


static void
visitor(l_p, tbl, after)
long *l_p;
genptr_t tbl;
int after;
{
	(void)nmg_tbl( (struct nmg_ptbl *)tbl, TBL_INS_UNIQUE, l_p);
}

/*
 *	Add an element provided by nmg_visit to a nmg_ptbl struct.
 */
static void
build_topo_list(l_p, tbl)
long *l_p;
struct nmg_ptbl *tbl;
{
	struct nmg_visit_handlers htab;
	struct loopuse *lu;
	struct edgeuse *eu;
	struct edgeuse *eu_p;
	struct vertexuse *vu;
	struct vertexuse *vu_p;
	int radial_not_mate=0;

	switch (*l_p) {
	case NMG_FACEUSE_MAGIC:
		htab = nmg_visit_handlers_null;
		htab.vis_face = htab.vis_edge = htab.vis_vertex = visitor;
		nmg_visit(l_p, &htab, (genptr_t *)tbl);
		break;
	case NMG_EDGEUSE_MAGIC:
		eu = eu_p = (struct edgeuse *)l_p;
		do {
			/* if the parent of this edgeuse is a face loopuse
			 * add the face to the list of shared topology
			 */
			if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
			    *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC)
				nmg_tbl(tbl, TBL_INS_UNIQUE,
					(long *)eu->up.lu_p->up.fu_p->f_p);

			if (radial_not_mate)	eu = eu->radial_p;
			else			eu = eu->eumate_p;
			radial_not_mate = ! radial_not_mate;
		} while (eu != eu_p);

		nmg_tbl(tbl, TBL_INS_UNIQUE, (long *)eu->e_p);
		nmg_tbl(tbl, TBL_INS_UNIQUE, (long *)eu->vu_p->v_p);
		nmg_tbl(tbl, TBL_INS_UNIQUE, (long *)eu->eumate_p->vu_p->v_p);

		break;
	case NMG_VERTEXUSE_MAGIC:
		vu_p = (struct vertexuse *)l_p;
		nmg_tbl(tbl, TBL_INS_UNIQUE, (long *)vu_p->v_p);

		for (RT_LIST_FOR(vu, vertexuse, &vu_p->v_p->vu_hd)) {
			lu = (struct loopuse *)NULL;
			switch (*vu->up.magic_p) {
			case NMG_EDGEUSE_MAGIC:
				eu = vu->up.eu_p;
				nmg_tbl(tbl, TBL_INS_UNIQUE, (long *)eu->e_p);
				if (*eu->up.magic_p !=  NMG_LOOPUSE_MAGIC)
					break;

				lu = eu->up.lu_p;
				/* fallthrough */

			case NMG_LOOPUSE_MAGIC:
				if ( ! lu ) lu = vu->up.lu_p;

				if (*lu->up.magic_p == NMG_FACEUSE_MAGIC)
					nmg_tbl(tbl, TBL_INS_UNIQUE,
						(long *)lu->up.fu_p->f_p);
				break;
			case NMG_SHELL_MAGIC:
				break;
			default:
				rt_log("%s[%d]: Bogus vertexuse parent magic:%s.",
					rt_identify_magic( *vu->up.magic_p ));
				rt_bomb("goodbye");
			}
		}
		break;
	default:
		rt_log("%s[%d]: Bogus magic number pointer:%s",
			rt_identify_magic( *l_p ) );
		rt_bomb("goodbye");
	}
}

static void
unresolved(a_hit, next_hit, a_tbl, next_tbl, hd)
struct hitmiss *a_hit;
struct hitmiss *next_hit;
struct nmg_ptbl  *a_tbl;
struct nmg_ptbl  *next_tbl;
struct hitmiss *hd;
{

	struct hitmiss *hm;
	register long **l_p;
	register long **b;

	rt_log("Unable to fix state transition--->\n");
	for (RT_LIST_FOR(hm, hitmiss, &hd->l)) {
		if (hm == next_hit) {
			rt_log("======= ======\n");
			nmg_rt_print_hitmiss(hm);
			rt_log("================\n");
		} else 
			nmg_rt_print_hitmiss(hm);
	}

	rt_log("topo table A\n");
	b = &a_tbl->buffer[a_tbl->end];
	l_p = &a_tbl->buffer[0];
	for ( ; l_p < b ; l_p ++)
		rt_log("\t0x%08x %s\n",**l_p, rt_identify_magic( **l_p));

	rt_log("topo table NEXT\n");
	b = &next_tbl->buffer[next_tbl->end];
	l_p = &next_tbl->buffer[0];
	for ( ; l_p < b ; l_p ++)
		rt_log("\t0x%08x %s\n",**l_p, rt_identify_magic( **l_p));
	
	rt_log("<---Unable to fix state transition\n");
}


static void
check_hitstate(hd)
struct hitmiss *hd;
{
	struct hitmiss *a_hit;
	struct hitmiss *next_hit;
	int ibs;
	int obs;
	struct nmg_ptbl *a_tbl = (struct nmg_ptbl *)NULL;
	struct nmg_ptbl *next_tbl = (struct nmg_ptbl *)NULL;
	struct nmg_ptbl *tbl_p = (struct nmg_ptbl *)NULL;
	long *long_ptr;

	/* find that first "OUTSIDE" point */
	a_hit = RT_LIST_FIRST(hitmiss, &hd->l);

	while( a_hit != hd && 
	     ((a_hit->in_out & 0x0f0)>>4) != NMG_RAY_STATE_OUTSIDE) {
		/* this better be a 2-manifold face */
		rt_log("%s[%d]: This better be a 2-manifold face\n",
			__FILE__, __LINE__);
		a_hit = RT_LIST_PNEXT(hitmiss, a_hit);
	}


	a_tbl = (struct nmg_ptbl *)
		rt_calloc(1, sizeof(struct nmg_ptbl), "a_tbl");
	nmg_tbl(a_tbl, TBL_INIT, (long *)NULL);


	next_tbl = (struct nmg_ptbl *)
		rt_calloc(1, sizeof(struct nmg_ptbl), "next_tbl");
	nmg_tbl(next_tbl, TBL_INIT, (long *)NULL);

	/* check the state transition on the rest of the hit points */
	while ((next_hit = RT_LIST_PNEXT(hitmiss, &a_hit->l)) != hd) {
		ibs = HMG_INBOUND_STATE(next_hit);
		obs = HMG_OUTBOUND_STATE(a_hit);
		if (ibs != obs) {
			/* if these two hits share some common topological
			 * element, then we can fix things.
			 *
			 * First we build the table of elements associated
			 * with each hit.
			 */

			nmg_tbl(a_tbl, TBL_RST, (long *)NULL);
			build_topo_list(a_hit->outbound_use, a_tbl);

			nmg_tbl(next_tbl, TBL_RST, (long *)NULL);
			build_topo_list(next_hit->outbound_use, next_tbl);


			/* If the tables have elements in common,
			 *   then resolve the conflict by
			 *	morphing the two hits to match.
			 *   else
			 *	This is a real conflict.
			 */
			if (long_ptr = common_topo(a_tbl, next_tbl)) {
				/* morf the two hit points */
				a_hit->in_out = (a_hit->in_out & 0x0f0) + 
					NMG_RAY_STATE_ON;
				a_hit->outbound_use = long_ptr;

				next_hit->in_out = (next_hit->in_out & 0x0f) +
					(NMG_RAY_STATE_ON << 4);
				a_hit->inbound_use = long_ptr;

			} else 
				unresolved(a_hit, next_hit,
						a_tbl, next_tbl, hd);

		}

		/* save next_tbl as a_tbl for next iteration */
		tbl_p = a_tbl;
		a_tbl = next_tbl;
		next_tbl = tbl_p;

		a_hit = next_hit;
	}

	nmg_tbl(next_tbl, TBL_FREE, (long *)NULL);
	nmg_tbl(a_tbl, TBL_FREE, (long *)NULL);
	(void)rt_free( (char *)a_tbl, "a_tbl");
	(void)rt_free( (char *)next_tbl, "next_tbl");
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

		NMG_FREE_HITLIST( &rd->rd_miss );

		if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
			if (last_miss)	rt_log(".");
			else		rt_log("ray missed NMG\n");
		}
		last_miss = 1;
		return(0);			/* MISS */
	} else if (rt_g.NMG_debug & DEBUG_RT_SEGS) {

		print_seg_list(rd->seghead, seg_count, "before");

		rt_log("\n\nnmg_ray_segs(rd)\nsorted nmg/ray hit list\n");

		for (RT_LIST_FOR(a_hit, hitmiss, &rd->rd_hit))
			nmg_rt_print_hitmiss(a_hit);
	}

	last_miss = 0;

	check_hitstate((struct hitmiss *)&rd->rd_hit);

	if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
		rt_log("----------morphed nmg/ray hit list---------\n");
		for (RT_LIST_FOR(a_hit, hitmiss, &rd->rd_hit))
			nmg_rt_print_hitmiss(a_hit);
	}

	seg_count = nmg_bsegs(rd, rd->ap, rd->seghead, rd->stp);


	NMG_FREE_HITLIST( &rd->rd_hit );

	NMG_FREE_HITLIST( &rd->rd_miss );


	if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
		/* print debugging data before returning */
		print_seg_list(rd->seghead, seg_count, "after");
	}

	return(seg_count);
}

