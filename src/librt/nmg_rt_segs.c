/*                   N M G _ R T _ S E G S . C
 * BRL-CAD
 *
 * Copyright (c) 1993-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup nmg */
/*@{*/
/** @file nmg_rt_segs.c
 *	Support routines for raytracing an NMG.
 *
 *  Author -
 *	Lee A. Butler
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066  USA
 *
 */
/*@}*/
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "machine.h"
#include "vmath.h"
#include "nmg.h"
#include "raytrace.h"
#include "plot3.h"


/*	EDGE-FACE correlation data
 * 	used in edge_hit() for 3manifold case
 */
struct ef_data {
    	fastf_t		fdotr;	/* face vector VDOT with ray */
    	fastf_t		fdotl;	/* face vector VDOT with ray-left */
    	fastf_t		ndotr;	/* face normal VDOT with ray */
    	struct edgeuse *eu;
};

#if 0
static const
struct bu_structparse rt_ef_parsetab[] = {
	{"%f", 1, "fdotr", bu_offsetof(struct ef_data, fdotr), BU_STRUCTPARSE_FUNC_NULL},
	{"%f", 1, "fdotl", bu_offsetof(struct ef_data, fdotl), BU_STRUCTPARSE_FUNC_NULL},
	{"%f", 1, "ndotr", bu_offsetof(struct ef_data, ndotr), BU_STRUCTPARSE_FUNC_NULL},
	{"%x", 1, "eu",   bu_offsetof(struct ef_data, eu),   BU_STRUCTPARSE_FUNC_NULL},
	{"", 0, (char *)NULL,	  0,			  BU_STRUCTPARSE_FUNC_NULL}
};

static const
struct bu_structparse rt_hit_parsetab[] = {
{"%f", 1, "hit_dist", bu_offsetof(struct hit, hit_dist), BU_STRUCTPARSE_FUNC_NULL},
{"%f", 3, "hit_point", bu_offsetofarray(struct hit, hit_point), BU_STRUCTPARSE_FUNC_NULL},
{"%f", 4, "hit_normal", bu_offsetofarray(struct hit, hit_normal), BU_STRUCTPARSE_FUNC_NULL},
{"%f", 3, "hit_vpriv", bu_offsetofarray(struct hit, hit_vpriv), BU_STRUCTPARSE_FUNC_NULL},
{"%x", 1, "hit_private", bu_offsetof(struct hit, hit_private), BU_STRUCTPARSE_FUNC_NULL},
{"%d", 1, "hit_surfno", bu_offsetof(struct hit, hit_surfno), BU_STRUCTPARSE_FUNC_NULL},
{"", 0, (char *)NULL,	  0,			  BU_STRUCTPARSE_FUNC_NULL}
};
#endif

#define CK_SEGP(_p) if ( !(_p) || !(*(_p)) ) {\
	bu_log("%s[line:%d]: Bad seg_p pointer\n", __FILE__, __LINE__); \
	nmg_rt_segs_exit("Goodbye"); }
#define DO_LONGJMP
#ifdef DO_LONGJMP
jmp_buf nmg_longjump_env;
#define nmg_rt_segs_exit(_s) {bu_log("%s\n",_s);longjmp(nmg_longjump_env, -1);}
#else
#define nmg_rt_segs_exit(_s) rt_bomb(_s)
#endif



static void
print_seg_list(struct seg *seghead, int seg_count, char *s)
{
	struct seg *seg_p;

	bu_log("Segment List (%d segnemts) (%s):\n", seg_count, s);
	/* print debugging data before returning */
	bu_log("Seghead:\n0x%08x magic: 0x%0x(%d) forw:0x%08x back:0x%08x\n\n",
			seghead,
			seghead->l.magic,
			seghead->l.forw,
			seghead->l.back);

	for (BU_LIST_FOR(seg_p, seg, &seghead->l) ) {
		bu_log("0x%08x magic: 0x%0x(%d) forw:0x%08x back:0x%08x\n",
			seg_p,
			seg_p->l.magic,
			seg_p->l.forw,
			seg_p->l.back);
		bu_log("dist %g  pt(%g,%g,%g)  N(%g,%g,%g)  =>\n",
		seg_p->seg_in.hit_dist,
		seg_p->seg_in.hit_point[0],
		seg_p->seg_in.hit_point[1],
		seg_p->seg_in.hit_point[2],
		seg_p->seg_in.hit_normal[0],
		seg_p->seg_in.hit_normal[1],
		seg_p->seg_in.hit_normal[2]);
		bu_log("dist %g  pt(%g,%g,%g)  N(%g,%g,%g)\n",
		seg_p->seg_out.hit_dist,
		seg_p->seg_out.hit_point[0],
		seg_p->seg_out.hit_point[1],
		seg_p->seg_out.hit_point[2],
		seg_p->seg_out.hit_normal[0],
		seg_p->seg_out.hit_normal[1],
		seg_p->seg_out.hit_normal[2]);
	}
}

static void
pl_ray(struct ray_data *rd)
{
	FILE *fd;
	char name[80];
	static int plot_file_number=0;
	struct hitmiss *a_hit;
	int old_state = NMG_RAY_STATE_OUTSIDE;
	int in_state;
	int out_state;
	point_t old_point;
	point_t end_point;
	int old_cond = 0;

	sprintf(name, "nmg_ray%02d.pl", plot_file_number++);
	if ((fd=fopen(name, "w")) == (FILE *)NULL) {
		perror(name);
		bu_bomb("unable to open file for writing");
	} else {
		bu_log("overlay %s\n", name);
	}

	VMOVE(old_point, rd->rp->r_pt);

	for (BU_LIST_FOR(a_hit, hitmiss, &rd->rd_hit)) {
#ifndef FAST_NMG
		NMG_CK_HITMISS(a_hit);
#endif

		in_state = HMG_INBOUND_STATE(a_hit);
		out_state = HMG_OUTBOUND_STATE(a_hit);

		if (in_state == old_state) {
			switch(in_state) {
			case NMG_RAY_STATE_INSIDE:
				pl_color(fd, 55, 255, 55);
				pdv_3line(fd, old_point, a_hit->hit.hit_point);
				break;
			case NMG_RAY_STATE_ON:
				pl_color(fd, 155, 155, 255);
				pdv_3line(fd, old_point, a_hit->hit.hit_point);
				break;
			case NMG_RAY_STATE_OUTSIDE:
				pl_color(fd, 255, 255, 255);
				pdv_3line(fd, old_point, a_hit->hit.hit_point);
				break;
			}
			old_cond = 0;
		} else {
			if (old_cond) {
				pl_color(fd, 255, 155, 255);
				old_cond = 0;
			} else {
				pl_color(fd, 255, 55, 255);
				old_cond = 1;
			}
			pdv_3line(fd, old_point, a_hit->hit.hit_point);
		}
		VMOVE(old_point, a_hit->hit.hit_point);
		old_state = out_state;
	}

	if (old_state == NMG_RAY_STATE_OUTSIDE)
		pl_color(fd, 255, 255, 255);
	else
		pl_color(fd, 255, 55, 255);

	VADD2(end_point, old_point, rd->rp->r_dir);
	pdv_3line(fd, old_point, end_point);

	fclose(fd);
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
set_inpoint(struct seg **seg_p, struct hitmiss *a_hit, struct soltab *stp, struct application *ap)
          	        	/* The segment we're building */
              	       		/* The input hit point */


{
	if ( !seg_p ) {
		bu_log("%s[line:%d]: Null pointer to segment pointer\n",
			__FILE__, __LINE__);
		nmg_rt_segs_exit("Goodbye");
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
		bu_log("Set seg_in:\n\tdist %g  pt(%g,%g,%g)  N(%g,%g,%g)\n",
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
set_outpoint(struct seg **seg_p, struct hitmiss *a_hit)
          	        	/* The segment we're building */
              	       		/* The input hit point */
{
	if ( !seg_p ) {
		bu_log("%s[line:%d]: Null pointer to segment pointer\n",
			__FILE__, __LINE__);
		nmg_rt_segs_exit("Goodbye");
	}

	/* if we don't have a seg struct yet, get one */
	if ( *seg_p == (struct seg *)NULL )
		nmg_rt_segs_exit("bad seg pointer\n");

	/* copy the "out" hit */
	bcopy(&a_hit->hit, &(*seg_p)->seg_out, sizeof(struct hit));

	/* copy the normal */
	VMOVE((*seg_p)->seg_out.hit_normal, a_hit->outbound_norm);

	if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
		bu_log("Set seg_out:\n\tdist %g  pt(%g,%g,%g)  N(%g,%g,%g)  =>\n",
		(*seg_p)->seg_in.hit_dist,
		(*seg_p)->seg_in.hit_point[0],
		(*seg_p)->seg_in.hit_point[1],
		(*seg_p)->seg_in.hit_point[2],
		(*seg_p)->seg_in.hit_normal[0],
		(*seg_p)->seg_in.hit_normal[1],
		(*seg_p)->seg_in.hit_normal[2]);
		bu_log("\tdist %g  pt(%g,%g,%g)  N(%g,%g,%g)\n",
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
state0(struct seg *seghead, struct seg **seg_p, int *seg_count, struct hitmiss *a_hit, struct soltab *stp, struct application *ap, struct bn_tol *tol)
          	         	/* intersection w/ ray */
          	        	/* The segment we're building */
   		           	/* The number of valid segments built */
              	       		/* The input hit point */



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
		bu_log("%s[line:%d]: State transition error: exit without entry.\n",
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
		bu_log("%s[line:%d]: bogus hit in/out status\n",
			__FILE__, __LINE__);
		nmg_rt_segs_exit("Goodbye\n");
		break;
	}

	return ret_val;
}

static int
state1(struct seg *seghead, struct seg **seg_p, int *seg_count, struct hitmiss *a_hit, struct soltab *stp, struct application *ap, struct bn_tol *tol)
          	         	/* intersection w/ ray */
          	        	/* The segment we're building */
   		           	/* The number of valid segments built */
              	       		/* The input hit point */



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
		bu_log("%s[line:%d]: bogus hit in/out status\n",
			__FILE__, __LINE__);
		nmg_rt_segs_exit("Goodbye\n");
		break;
	}

	return ret_val;
}

static int
state2(struct seg *seghead, struct seg **seg_p, int *seg_count, struct hitmiss *a_hit, struct soltab *stp, struct application *ap, struct bn_tol *tol)
          	         	/* intersection w/ ray */
          	        	/* The segment we're building */
   		           	/* The number of valid segments built */
              	       		/* The input hit point */



{
	int ret_val = -1;
	double delta;

	switch (a_hit->in_out) {
	case HMG_HIT_OUT_ON:
	case HMG_HIT_OUT_IN:
		/* Segment completed.  Insert into segment list */
		BU_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
		BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
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
		bu_log("%s[line:%d]: State transition error.\n",
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
		BN_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if ( delta < tol->dist) {
			set_outpoint(seg_p, a_hit);
			ret_val = 2;
			break;
		}
		/* complete the segment */
		BU_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
		BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
		(*seg_count)++;

		/* start new segment */
		(*seg_p) = (struct seg *)NULL;
		set_inpoint(seg_p, a_hit, stp, ap);
		set_outpoint(seg_p, a_hit);

		ret_val = 3;
		break;
	case HMG_HIT_ANY_ANY:
		CK_SEGP(seg_p);
		BN_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if ( delta < tol->dist) {
			set_outpoint(seg_p, a_hit);
			ret_val = 2;
			break;
		}
		/* complete the segment */
		BU_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
		BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
		(*seg_count)++;

		/* start new segment */
		(*seg_p) = (struct seg *)NULL;
		set_inpoint(seg_p, a_hit, stp, ap);
		set_outpoint(seg_p, a_hit);

		ret_val = 4;
		break;
	default:
		bu_log("%s[line:%d]: bogus hit in/out status\n",
			__FILE__, __LINE__);
		nmg_rt_segs_exit("Goodbye\n");
		break;
	}

	return ret_val;
}

static int
state3(struct seg *seghead, struct seg **seg_p, int *seg_count, struct hitmiss *a_hit, struct soltab *stp, struct application *ap, struct bn_tol *tol)
          	         	/* intersection w/ ray */
          	        	/* The segment we're building */
   		           	/* The number of valid segments built */
              	       		/* The input hit point */



{
	int ret_val = -1;
	double delta;

	CK_SEGP(seg_p);
	BN_CK_TOL(tol);
	delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);

	switch (a_hit->in_out) {
	case HMG_HIT_OUT_ON:
	case HMG_HIT_OUT_IN:
		if ( delta < tol->dist) {
			ret_val = 5;
		} else {
			/* complete the segment */
			BU_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
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
			bu_log("%s[line:%d]: State transition error.\n",
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
			BU_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
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
			BU_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
			(*seg_count)++;

			/* start new segment */
			(*seg_p) = (struct seg *)NULL;
			set_inpoint(seg_p, a_hit, stp, ap);
			ret_val = 4;
		}
		break;
	default:
		bu_log("%s[line:%d]: bogus hit in/out status\n",
			__FILE__, __LINE__);
		nmg_rt_segs_exit("Goodbye\n");
		break;
	}

	return ret_val;
}

static int
state4(struct seg *seghead, struct seg **seg_p, int *seg_count, struct hitmiss *a_hit, struct soltab *stp, struct application *ap, struct bn_tol *tol)
          	         	/* intersection w/ ray */
          	        	/* The segment we're building */
   		           	/* The number of valid segments built */
              	       		/* The input hit point */



{
	int ret_val = -1;
	double delta;

	switch (a_hit->in_out) {
	case HMG_HIT_OUT_ON:
	case HMG_HIT_OUT_IN:
		CK_SEGP(seg_p);
		BN_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta > tol->dist) {
			/* complete the segment */
			BU_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
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
		bu_log("%s[line:%d]: State transition error.\n",
			__FILE__, __LINE__);
		ret_val = -2;
		break;
	case HMG_HIT_OUT_OUT:
		CK_SEGP(seg_p);
		BN_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta > tol->dist) {
			/* complete the segment */
			BU_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
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
		BN_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta > tol->dist) {
			/* complete the segment */
			BU_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
			(*seg_count)++;

			/* start new segment */
			(*seg_p) = (struct seg *)NULL;
			set_inpoint(seg_p, a_hit, stp, ap);
			set_outpoint(seg_p, a_hit);
		}
		ret_val = 4;
		break;
	default:
		bu_log("%s[line:%d]: bogus hit in/out status\n",
			__FILE__, __LINE__);
		nmg_rt_segs_exit("Goodbye\n");
		break;
	}

	return ret_val;
}

static int
state5(struct seg *seghead, struct seg **seg_p, int *seg_count, struct hitmiss *a_hit, struct soltab *stp, struct application *ap, struct bn_tol *tol)
          	         	/* intersection w/ ray */
          	        	/* The segment we're building */
   		           	/* The number of valid segments built */
              	       		/* The input hit point */



{
	int ret_val = -1;
	double delta;

	switch (a_hit->in_out) {
	case HMG_HIT_OUT_ON:
	case HMG_HIT_OUT_IN:
		CK_SEGP(seg_p);
		BN_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta < tol->dist) {
			ret_val = 5;
		} else {
			/* complete the segment */
			BU_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
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
		BN_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta < tol->dist) {
			ret_val = 6;
		} else {
			/* complete the segment */
			BU_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
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
		BN_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta < tol->dist) {
			ret_val = 5;
		} else {
			/* complete the segment */
			BU_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
			(*seg_count)++;

			/* start new segment */
			(*seg_p) = (struct seg *)NULL;
			set_inpoint(seg_p, a_hit, stp, ap);
			set_outpoint(seg_p, a_hit);
			ret_val = 4;
		}
		break;
	default:
		bu_log("%s[line:%d]: bogus hit in/out status\n",
			__FILE__, __LINE__);
		nmg_rt_segs_exit("Goodbye\n");
		break;
	}

	return ret_val;
}

static int
state6(struct seg *seghead, struct seg **seg_p, int *seg_count, struct hitmiss *a_hit, struct soltab *stp, struct application *ap, struct bn_tol *tol)
          	         	/* intersection w/ ray */
          	        	/* The segment we're building */
   		           	/* The number of valid segments built */
              	       		/* The input hit point */



{
	int ret_val = -1;
	double delta;

	switch (a_hit->in_out) {
	case HMG_HIT_OUT_ON:
	case HMG_HIT_OUT_IN:
		CK_SEGP(seg_p);
		BN_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta < tol->dist) {
			ret_val = 5;
		} else {
			/* complete the segment */
			BU_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
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
		BN_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta < tol->dist) {
			ret_val = 6;
		} else {
			/* complete the segment */
			BU_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
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
		BN_CK_TOL(tol);
		delta = fabs((*seg_p)->seg_in.hit_dist - a_hit->hit.hit_dist);
		if (delta < tol->dist) {
			ret_val = 6;
		} else {
			/* complete the segment */
			BU_LIST_MAGIC_SET( &( (*seg_p)->l ), RT_SEG_MAGIC);
			BU_LIST_INSERT(&(seghead->l), &((*seg_p)->l) );
			(*seg_count)++;

			/* start new segment */
			(*seg_p) = (struct seg *)NULL;
			set_inpoint(seg_p, a_hit, stp, ap);
			set_outpoint(seg_p, a_hit);
			ret_val = 4;
		}
		break;
	default:
		bu_log("%s[line:%d]: bogus hit in/out status\n",
			__FILE__, __LINE__);
		nmg_rt_segs_exit("Goodbye\n");
		break;
	}

	return ret_val;
}


static int (*state_table[7])() = {
	state0, state1, state2, state3,
	state4, state5, state6
};


static int
nmg_bsegs(struct ray_data *rd, struct application *ap, struct seg *seghead, struct soltab *stp)


          		         	/* intersection w/ ray */

{
	int ray_state = 0;
	int new_state;
	struct hitmiss *a_hit = (struct hitmiss *)NULL;
	struct hitmiss *hm = (struct hitmiss *)NULL;
	struct seg *seg_p = (struct seg *)NULL;
	int seg_count = 0;

#ifndef FAST_NMG
	NMG_CK_HITMISS_LISTS(a_hit, rd);
#endif

	for (BU_LIST_FOR(a_hit, hitmiss, &rd->rd_hit)) {
#ifndef FAST_NMG
		NMG_CK_HITMISS(a_hit);
#endif

		new_state = state_table[ray_state](seghead, &seg_p,
							&seg_count, a_hit,
							stp, ap, rd->tol);
		if (new_state < 0) {
			/* state transition error.  Print out the hit list
			 * and indicate where we were in processing it.
			 */
			for (BU_LIST_FOR(hm, hitmiss, &rd->rd_hit)) {
				if (hm == a_hit) {
					bu_log("======= State %d ======\n",
						ray_state);
					nmg_rt_print_hitmiss(hm);
					bu_log("================\n");
				} else
					nmg_rt_print_hitmiss(hm);
			}

			/* Now bomb off */
			bu_log("Primitive: %s, pixel=%d %d, lvl=%d %s\n",
				rd->stp->st_dp->d_namep,
				rd->ap->a_x, rd->ap->a_y, rd->ap->a_level,
				rd->ap->a_purpose );
		    	bu_log("Ray: pt:(%g %g %g) dir:(%g %g %g)\n",
		    		V3ARGS(rd->rp->r_pt), V3ARGS(rd->rp->r_dir) );
			nmg_rt_segs_exit("Goodbye\n");
		}

		ray_state = new_state;
	}

	/* Check to make sure the input ran out in the right place
	 * in the state table.
	 */
	if (ray_state == 1) {
		bu_log("%s[line:%d]: Input ended at non-terminal FSM state\n",
			__FILE__, __LINE__);

	    	bu_log("Ray: pt:(%g %g %g) dir:(%g %g %g)\n",
	    		V3ARGS(rd->rp->r_pt), V3ARGS(rd->rp->r_dir) );

		bu_log("Primitive: %s, pixel=%d %d, lvl=%d %s\n",
			stp->st_dp->d_namep,
			ap->a_x, ap->a_y, ap->a_level,
			ap->a_purpose );
		nmg_rt_segs_exit("Goodbye");
	}

	/* Insert the last segment if appropriate */
	if (ray_state > 1) {
		/* complete the segment */
		BU_LIST_MAGIC_SET( &( seg_p->l ), RT_SEG_MAGIC);
		BU_LIST_INSERT(&(seghead->l), &(seg_p->l) );
		seg_count++;
	}

	return seg_count;
}

/**
 *	If a_tbl and next_tbl have an element in common, return it.
 *	Otherwise return a NULL pointer.
 */
static long *
common_topo(struct bu_ptbl *a_tbl, struct bu_ptbl *next_tbl)
{
	long **p;

	for (p = &a_tbl->buffer[a_tbl->end] ; p >= a_tbl->buffer ; p--) {
		if (bu_ptbl_locate(next_tbl, *p) >= 0)
			return *p;
	}

	return (long *)NULL;
}


static void
visitor(long int *l_p, genptr_t tbl, int after)
{
	(void)bu_ptbl_ins_unique( (struct bu_ptbl *)tbl, l_p);
}

/**
 *	Add an element provided by nmg_visit to a bu_ptbl struct.
 */
static void
build_topo_list(long int *l_p, struct bu_ptbl *tbl)
{
	struct loopuse *lu;
	struct edgeuse *eu;
	struct edgeuse *eu_p;
	struct vertexuse *vu;
	struct vertexuse *vu_p;
	int radial_not_mate=0;
	static const struct nmg_visit_handlers htab = {NULL, NULL, NULL, NULL, NULL,
						       NULL, NULL, NULL, NULL, NULL,
						       visitor, NULL, NULL, NULL, NULL,
						       NULL, NULL, NULL, visitor, NULL,
						       NULL, NULL, NULL, visitor, NULL};
	/* htab.vis_face = htab.vis_edge = htab.vis_vertex = visitor; */

	if (!l_p) {
		bu_log("%s:%d NULL l_p\n", __FILE__, __LINE__);
		nmg_rt_segs_exit("");
	}

	switch (*l_p) {
	case NMG_FACEUSE_MAGIC:
		nmg_visit(l_p, &htab, (genptr_t)tbl);
		break;
	case NMG_EDGEUSE_MAGIC:
		eu = eu_p = (struct edgeuse *)l_p;
		do {
			/* if the parent of this edgeuse is a face loopuse
			 * add the face to the list of shared topology
			 */
			if (*eu->up.magic_p == NMG_LOOPUSE_MAGIC &&
			    *eu->up.lu_p->up.magic_p == NMG_FACEUSE_MAGIC)
				bu_ptbl_ins_unique(tbl,
					(long *)eu->up.lu_p->up.fu_p->f_p);

			if (radial_not_mate)	eu = eu->radial_p;
			else			eu = eu->eumate_p;
			radial_not_mate = ! radial_not_mate;
		} while (eu != eu_p);

		bu_ptbl_ins_unique(tbl, (long *)eu->e_p);
		bu_ptbl_ins_unique(tbl, (long *)eu->vu_p->v_p);
		bu_ptbl_ins_unique(tbl, (long *)eu->eumate_p->vu_p->v_p);

		break;
	case NMG_VERTEXUSE_MAGIC:
		vu_p = (struct vertexuse *)l_p;
		bu_ptbl_ins_unique(tbl, (long *)vu_p->v_p);

		for (BU_LIST_FOR(vu, vertexuse, &vu_p->v_p->vu_hd)) {
			lu = (struct loopuse *)NULL;
			switch (*vu->up.magic_p) {
			case NMG_EDGEUSE_MAGIC:
				eu = vu->up.eu_p;
				bu_ptbl_ins_unique(tbl, (long *)eu->e_p);
				if (*eu->up.magic_p !=  NMG_LOOPUSE_MAGIC)
					break;

				lu = eu->up.lu_p;
				/* fallthrough */

			case NMG_LOOPUSE_MAGIC:
				if ( ! lu ) lu = vu->up.lu_p;

				if (*lu->up.magic_p == NMG_FACEUSE_MAGIC)
					bu_ptbl_ins_unique(tbl,
						(long *)lu->up.fu_p->f_p);
				break;
			case NMG_SHELL_MAGIC:
				break;
			default:
				bu_log("%s[%d]: Bogus vertexuse parent magic:%s.",
					bu_identify_magic( *vu->up.magic_p ));
				nmg_rt_segs_exit("goodbye");
			}
		}
		break;
	default:
		bu_log("%s[%d]: Bogus magic number pointer:%s",
			bu_identify_magic( *l_p ) );
		nmg_rt_segs_exit("goodbye");
	}
}

static void
unresolved(struct hitmiss *a_hit, struct hitmiss *next_hit, struct bu_ptbl *a_tbl, struct bu_ptbl *next_tbl, struct hitmiss *hd, struct ray_data *rd)
{

	struct hitmiss *hm;
	register long **l_p;
	register long **b;

	bu_log("Unable to fix state transition--->\n");
	bu_log( "\tray start = (%f %f %f) dir = (%f %f %f)\n",
		V3ARGS( rd->rp->r_pt ), V3ARGS( rd->rp->r_dir ) );
	for (BU_LIST_FOR(hm, hitmiss, &hd->l)) {
		if (hm == next_hit) {
			bu_log("======= ======\n");
			nmg_rt_print_hitmiss(hm);
			bu_log("================\n");
		} else
			nmg_rt_print_hitmiss(hm);
	}

	bu_log("topo table A\n");
	b = &a_tbl->buffer[a_tbl->end];
	l_p = &a_tbl->buffer[0];
	for ( ; l_p < b ; l_p ++)
		bu_log("\t0x%08x %s\n",**l_p, bu_identify_magic( **l_p));

	bu_log("topo table NEXT\n");
	b = &next_tbl->buffer[next_tbl->end];
	l_p = &next_tbl->buffer[0];
	for ( ; l_p < b ; l_p ++)
		bu_log("\t0x%08x %s\n",**l_p, bu_identify_magic( **l_p));

	bu_log("<---Unable to fix state transition\n");
	pl_ray(rd);
	bu_log("Primitive: %s, pixel=%d %d,  lvl=%d %s\n",
		rd->stp->st_dp->d_namep,
		rd->ap->a_x, rd->ap->a_y, rd->ap->a_level,
		rd->ap->a_purpose );
}


static int
check_hitstate(struct hitmiss *hd, struct ray_data *rd)
{
	struct hitmiss *a_hit;
	struct hitmiss *next_hit;
	int ibs;
	int obs;
	struct bu_ptbl *a_tbl = (struct bu_ptbl *)NULL;
	struct bu_ptbl *next_tbl = (struct bu_ptbl *)NULL;
	struct bu_ptbl *tbl_p = (struct bu_ptbl *)NULL;
	long *long_ptr;

	BU_CK_LIST_HEAD(&hd->l);

#ifndef FAST_NMG
	NMG_CK_HITMISS_LISTS(a_hit, rd);
#endif

	/* find that first "OUTSIDE" point */
	a_hit = BU_LIST_FIRST(hitmiss, &hd->l);
#ifndef FAST_NMG
	NMG_CK_HITMISS(a_hit);
#endif
	if (((a_hit->in_out & 0x0f0) >> 4) != NMG_RAY_STATE_OUTSIDE ||
	    rt_g.NMG_debug & DEBUG_RT_SEGS) {
		bu_log("check_hitstate()\n");
	     	nmg_rt_print_hitlist(hd);

	    	bu_log("Ray: pt:(%g %g %g) dir:(%g %g %g)\n",
	    		V3ARGS(rd->rp->r_pt), V3ARGS(rd->rp->r_dir) );
	}

	while( a_hit != hd &&
		((a_hit->in_out & 0x0f0) >> 4) != NMG_RAY_STATE_OUTSIDE) {


#ifndef FAST_NMG
		NMG_CK_HITMISS(a_hit);
#endif
		/* this better be a 2-manifold face */
		bu_log("%s[%d]: This better be a 2-manifold face\n",
			__FILE__, __LINE__);
		bu_log("Primitive: %s, pixel=%d %d,  lvl=%d %s\n",
				rd->stp->st_dp->d_namep,
				rd->ap->a_x, rd->ap->a_y, rd->ap->a_level,
				rd->ap->a_purpose );
		a_hit = BU_LIST_PNEXT(hitmiss, a_hit);
		if (a_hit != hd) {
#ifndef FAST_NMG
			NMG_CK_HITMISS(a_hit);
#endif
		}
	}
	if (a_hit == hd) return 1;

	a_tbl = (struct bu_ptbl *)
		bu_calloc(1, sizeof(struct bu_ptbl), "a_tbl");
	bu_ptbl_init(a_tbl, 64, "a_tbl");


	next_tbl = (struct bu_ptbl *)
		bu_calloc(1, sizeof(struct bu_ptbl), "next_tbl");
	bu_ptbl_init(next_tbl, 64, "next_tbl");

	/* check the state transition on the rest of the hit points */
	while ((next_hit = BU_LIST_PNEXT(hitmiss, &a_hit->l)) != hd) {
#ifndef FAST_NMG
		NMG_CK_HITMISS(next_hit);
#endif
		ibs = HMG_INBOUND_STATE(next_hit);
		obs = HMG_OUTBOUND_STATE(a_hit);
		if (ibs != obs) {
			/* if these two hits share some common topological
			 * element, then we can fix things.
			 *
			 * First we build the table of elements associated
			 * with each hit.
			 */

			bu_ptbl_reset(a_tbl);

#ifndef FAST_NMG
			NMG_CK_HITMISS(a_hit);
#endif
			build_topo_list(a_hit->outbound_use, a_tbl);

			bu_ptbl_reset(next_tbl);
#ifndef FAST_NMG
			NMG_CK_HITMISS(next_hit);
#endif
			build_topo_list(next_hit->outbound_use, next_tbl);


			/* If the tables have elements in common,
			 *   then resolve the conflict by
			 *	morphing the two hits to match.
			 *   else
			 *	This is a real conflict.
			 */
			if ( (long_ptr = common_topo(a_tbl, next_tbl)) ) {
				/* morf the two hit points */
				a_hit->in_out = (a_hit->in_out & 0x0f0) +
					NMG_RAY_STATE_ON;
				a_hit->outbound_use = long_ptr;

				next_hit->in_out = (next_hit->in_out & 0x0f) +
					(NMG_RAY_STATE_ON << 4);
				a_hit->inbound_use = long_ptr;

			} else
				unresolved(a_hit, next_hit,
						a_tbl, next_tbl, hd, rd);

		}

		/* save next_tbl as a_tbl for next iteration */
		tbl_p = a_tbl;
		a_tbl = next_tbl;
		next_tbl = tbl_p;

		a_hit = next_hit;
	}

	bu_ptbl_free(next_tbl);
	bu_ptbl_free(a_tbl);
	(void)bu_free( (char *)a_tbl, "a_tbl");
	(void)bu_free( (char *)next_tbl, "next_tbl");

	return 0;
}

/**	N M G _ R A Y _ S E G S
 *
 *	Obtain the list of ray segments which intersect with the nmg.
 *	This routine does all of the "work" for rt_nmg_shot()
 *
 *	Return:
 *		# of segments added to list.
 */
int
nmg_ray_segs(struct ray_data *rd)
{
	struct hitmiss *a_hit;
	static int last_miss=0;

#ifdef DO_LONGJMP
	if (setjmp(nmg_longjump_env) != 0) {
		return 0;
	}
#endif

#ifndef FAST_NMG
	NMG_CK_HITMISS_LISTS(a_hit, rd);
#endif

	if (BU_LIST_IS_EMPTY(&rd->rd_hit)) {

		NMG_FREE_HITLIST( &rd->rd_miss, rd->ap );

		if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
			if (last_miss)	bu_log(".");
			else		bu_log("ray missed NMG\n");
		}
		last_miss = 1;
		return(0);			/* MISS */
	} else if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
		int seg_count=0;

		print_seg_list(rd->seghead, seg_count, "before");

		bu_log("\n\nnmg_ray_segs(rd)\nsorted nmg/ray hit list\n");

		for (BU_LIST_FOR(a_hit, hitmiss, &rd->rd_hit))
			nmg_rt_print_hitmiss(a_hit);
	}

	last_miss = 0;

	if (check_hitstate((struct hitmiss *)&rd->rd_hit, rd)) {
		NMG_FREE_HITLIST( &rd->rd_hit, rd->ap );
		NMG_FREE_HITLIST( &rd->rd_miss, rd->ap );
		return 0;
	}

	if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
		bu_log("----------morphed nmg/ray hit list---------\n");
		for (BU_LIST_FOR(a_hit, hitmiss, &rd->rd_hit))
			nmg_rt_print_hitmiss(a_hit);

		pl_ray(rd);
	}

	{
		int seg_count = nmg_bsegs(rd, rd->ap, rd->seghead, rd->stp);


		NMG_FREE_HITLIST( &rd->rd_hit, rd->ap );
		NMG_FREE_HITLIST( &rd->rd_miss, rd->ap );


		if (rt_g.NMG_debug & DEBUG_RT_SEGS) {
			/* print debugging data before returning */
			print_seg_list(rd->seghead, seg_count, "after");
		}
		return(seg_count);
	}
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
