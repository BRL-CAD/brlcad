/*
 *			S H O O T . C
 *
 * Ray Tracing program shot coordinator.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

char CopyRight_Notice[] = "@(#) Copyright (C) 1985 by the United States Army";

#include <stdio.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "raytrace.h"
#include "debug.h"

#include "cut.h"

int debug = DEBUG_OFF;	/* non-zero for debugging, see debug.h */
long nsolids;		/* total # of solids participating */
long nregions;		/* total # of regions participating */
long nshots;		/* # of calls to ft_shot() */
long nmiss_model;	/* rays missed model RPP */
long nmiss_tree;	/* rays missed sub-tree RPP */
long nmiss_solid;	/* rays missed solid RPP */
long nmiss;		/* solid ft_shot() returned a miss */
long nhits;		/* solid ft_shot() returned a hit */
int rt_needprep = 1;	/* rt_prep() needs calling */
struct soltab *HeadSolid = SOLTAB_NULL;
struct seg *FreeSeg = SEG_NULL;		/* Head of freelist */

HIDDEN int shoot_tree();

/*
 *  			S H O O T R A Y
 *  
 *  Given a ray, shoot it at all the relevant parts of the model,
 *  (building the HeadSeg chain), and then call bool_regions()
 *  to build and evaluate the partition chain.
 *  If the ray actually hit anything, call the application's
 *  a_hit() routine with a pointer to the partition chain,
 *  otherwise, call the application's a_miss() routine.
 *
 *  It is important to note that rays extend infinitely only in the
 *  positive direction.  The ray is composed of all points P, where
 *
 *	P = r_pt + K * r_dir
 *
 *  for K ranging from 0 to +infinity.  There is no looking backwards.
 *
 *  It is also important to note that the direction vector r_dir
 *  must have unit length;  this is mandatory, and is not checked
 *  in the name of efficiency.
 *
 *  Input:  Pointer to an application structure, with these mandatory fields:
 *	a_ray.r_pt	Starting point of ray to be fired
 *	a_ray.r_dir	UNIT VECTOR with direction to fire in (dir cosines)
 *	a_hit		Routine to call when something is hit
 *	a_miss		Routine to call when ray misses everything
 *
 *  Returns: whatever the application function returns (an int).
 *
 *  NOTE:  The appliction functions may call shootray() recursively.
 *	Thus, none of the local variables may be static.
 */
int
shootray( ap )
register struct application *ap;
{
	auto struct seg *HeadSeg;
	register int ret;
	auto vect_t inv_dir;		/* inverses of ap->a_ray.r_dir */
	auto vect_t point;
	auto fastf_t	box_start, box_end, model_end;
	auto fastf_t	last_bool_start;
	auto bitv_t *solidbits;		/* bits for all solids shot so far */
	auto bitv_t *regionbits;	/* bits for all involved regions */
	auto int trybool;
	auto char *status;
	auto union cutter *lastcut;
	auto struct partition InitialPart; /* Head of Initial Partitions */
	auto struct partition FinalPart;	/* Head of Final Partitions */

	if( rt_needprep )  rt_prep();		/* so things will work */

	InitialPart.pt_forw = InitialPart.pt_back = &InitialPart;
	FinalPart.pt_forw = FinalPart.pt_back = &FinalPart;

	solidbits = (bitv_t *)0;		/* not allocated yet */

	if(debug&(DEBUG_ALLRAYS|DEBUG_SHOOT|DEBUG_PARTITION)) {
		VPRINT("\nPnt", ap->a_ray.r_pt);
		VPRINT("Dir", ap->a_ray.r_dir);
		fflush(stderr);		/* In case of instant death */
	}

	/* Verify that direction vector has unit length */
	if(debug) {
		FAST fastf_t f, diff;
		f = ap->a_ray.r_dir[X] * ap->a_ray.r_dir[X] +
			ap->a_ray.r_dir[Y] * ap->a_ray.r_dir[Y] +
			ap->a_ray.r_dir[Z] * ap->a_ray.r_dir[Z];
		if( NEAR_ZERO(f) )  {
			rtbomb("shootray:  zero length dir vector\n");
			return(0);
		}
		diff = f - 1;
		if( !NEAR_ZERO( diff ) )  {
			rtlog("shootray: non-unit dir vect (x%d y%d lvl%d)\n",
				ap->a_x, ap->a_y, ap->a_level );
			f = 1/f;
			VSCALE( ap->a_ray.r_dir, ap->a_ray.r_dir, f );
		}
	}

	inv_dir[X] = inv_dir[Y] = inv_dir[Z] = INFINITY;
	if(!NEAR_ZERO(ap->a_ray.r_dir[X])) inv_dir[X]=1.0/ap->a_ray.r_dir[X];
	if(!NEAR_ZERO(ap->a_ray.r_dir[Y])) inv_dir[Y]=1.0/ap->a_ray.r_dir[Y];
	if(!NEAR_ZERO(ap->a_ray.r_dir[Z])) inv_dir[Z]=1.0/ap->a_ray.r_dir[Z];

	/*
	 *  If ray does not enter the model RPP, skip on.
	 */
	if( !in_rpp( &ap->a_ray, inv_dir, mdl_min, mdl_max )  ||
	    ap->a_ray.r_max <= 0.0 )  {
		nmiss_model++;
		ret = ap->a_miss( ap );
		status = "MISS model";
		goto out;
	}

	/* Push ray starting point to edge of model RPP */
	box_start = ap->a_ray.r_min;
	model_end = ap->a_ray.r_max;
	if( box_start < 0.0 )
		box_start = 0.0;	/* don't look back */
	last_bool_start = box_start;

	solidbits = (bitv_t *)vmalloc(
		BITS2BYTES(nsolids) + BITS2BYTES(nregions) + 4*sizeof(bitv_t),
		"solidbits+regionbits");
	regionbits = &solidbits[2+(BITS2BYTES(nsolids)/sizeof(bitv_t))];
	BITZERO(solidbits,nsolids);
	BITZERO(regionbits,nregions);
	HeadSeg = SEG_NULL;
	lastcut = CUTTER_NULL;
	trybool = 0;

	/*
	 *  While the ray remains inside model space,
	 *  push from box to box until ray emerges from
	 *  model space again (or first hit is found, if user is impatient).
	 */
	while( box_start < model_end )  {
		register union cutter *cutp;
		struct soltab *stp;
		struct seg *waitsegs;	/* segs awaiting bool_weave() */

		waitsegs = SEG_NULL;

		/* Move point (not box_start) slightly into the new box */
		{
			FAST fastf_t f;
			f = box_start + EPSILON;
			VJOIN1( point, ap->a_ray.r_pt, f, ap->a_ray.r_dir );
		}
		cutp = &CutHead;
		while( cutp->cut_type == CUT_CUTNODE )  {
			if( point[cutp->cn.cn_axis] >= cutp->cn.cn_point )  {
				cutp=cutp->cn.cn_r;
			}  else  {
				cutp=cutp->cn.cn_l;
			}
		}
		if(cutp==CUTTER_NULL || cutp->cut_type != CUT_BOXNODE)
			rtbomb("leaf not boxnode");

		/* Make sure we don't get stuck within the same box twice */
		if( cutp==lastcut )  {
			rtlog("straight box push failed\n");
			/* Advance 0.01% of last box size */
			box_end += (box_end-box_start)*0.0001;
			box_start = box_end;
			continue;
		}
		lastcut = cutp;

		if( !in_rpp(&ap->a_ray, inv_dir,
		     cutp->bn.bn_min, cutp->bn.bn_max) )  {
			rtlog("shootray:  ray misses box?\n");
			break;
		}
		box_end = ap->a_ray.r_max;	
		ret = cutp->bn.bn_len;		/* loop count, below */

		if(debug&DEBUG_SHOOT) {
			rtlog("ray (%f, %f) %f\n", box_start, box_end, model_end);
			VPRINT( "point", point );
			pr_cut( cutp, 0 );
		}

		/* Consider all objects within the box */
		while( --ret >= 0 )  {
			register struct seg *newseg;

			stp = cutp->bn.bn_list[ret];
			if( BITTEST( solidbits, stp->st_bit ) )  {
				if(debug&DEBUG_SHOOT)rtlog("skipping %s\n", stp->st_name);
				nmiss_tree++;
				continue;	/* already shot */
			}

			/* Shoot a ray */
			if(debug&DEBUG_SHOOT)rtlog("shooting %s\n", stp->st_name);
			BITSET( solidbits, stp->st_bit );

			/* If ray does not strike the bounding RPP, skip on */
			if(
			     /*  stp->st_id != ID_ARB8 && */
			   ( !in_rpp( &ap->a_ray, inv_dir,
			      stp->st_min, stp->st_max ) ||
			      ap->a_ray.r_max <= 0.0 )
			)  {
				nmiss_solid++;
				continue;	/* MISS */
			}

			nshots++;
			if( (newseg = functab[stp->st_id].ft_shot( 
				stp, &ap->a_ray )
			     ) == SEG_NULL )  {
				nmiss++;
				continue;	/* MISS */
			}
			if(debug&DEBUG_SHOOT)  pr_seg(newseg);

			/* First, some checking */
			if( newseg->seg_in.hit_dist > newseg->seg_out.hit_dist )  {
				LOCAL struct hit temp;		/* XXX */
				rtlog("ERROR %s: in/out rev (%f,%f)\n",
					newseg->seg_stp->st_name,
					newseg->seg_in.hit_dist,
					newseg->seg_out.hit_dist );
				temp = newseg->seg_in;	/* struct copy */
				newseg->seg_in = newseg->seg_out;/* struct copy */
				newseg->seg_out = temp;	/* struct copy */
			}

			/* Discard seg entirely behind start point of ray */
			while( newseg != SEG_NULL && 
				newseg->seg_out.hit_dist < -EPSILON )  {
				register struct seg *seg2 = newseg->seg_next;
				FREE_SEG( newseg );
				newseg = seg2;
			}
			if( newseg == SEG_NULL )  {
				nmiss++;
				continue;	/* MISS */
			}
			trybool++;	/* flag to rerun bool, below */

			/* Add seg chain to list awaiting bool_weave() */
			{
				register struct seg *seg2 = newseg;
				while( seg2->seg_next != SEG_NULL )
					seg2 = seg2->seg_next;
				seg2->seg_next = waitsegs;
				waitsegs = newseg;
			}
			/* Would be even better done by per-partition bitv */
			bitv_or( regionbits, stp->st_regions, stp->st_maxreg);
			if(debug&DEBUG_PARTITION) pr_bitv( "shoot Regionbits", regionbits, nregions);
			nhits++;	/* global counter */
		}

		/*
		 *  Weave these segments into the partition list.
		 *  Done once per box.
		 */
		if( waitsegs != SEG_NULL )  {
			bool_weave( waitsegs, &InitialPart );

			/* Add segment chain to list of used segments */
			{
				register struct seg *seg2 = waitsegs;
				while( seg2->seg_next != SEG_NULL )
					seg2 = seg2->seg_next;
				seg2->seg_next = HeadSeg;
				HeadSeg = waitsegs;
				waitsegs = SEG_NULL;
			}
		}

		/* Special case for efficiency -- first hit only */
		/* Only run this every three hits, to balance cost/benefit */
		if( trybool>=3 && ap->a_onehit )  {
			/* Evaluate regions upto box_end */
			bool_final( &InitialPart, &FinalPart,
				last_bool_start, box_end, regionbits, ap );

			/* If anything was found, it's a hit! */
			if( FinalPart.pt_forw != &FinalPart )
				goto hitit;

			last_bool_start = box_end;
			trybool = 0;
		}

		/* Push ray onwards to next box */
		box_start = box_end;
	}
	/* Ray has finally left known space -- do final computations */

	/* HeadSeg chain now has all segments hit by this ray */
	if( HeadSeg == SEG_NULL )  {
		ret = ap->a_miss( ap );
		status = "MISS solids";
		goto out;
	}

	/*
	 *  All intersections of the ray with the model have
	 *  been computed.  Evaluate the boolean trees over each partition.
	 */
	bool_final( &InitialPart, &FinalPart, 0.0, model_end, regionbits, ap);

	if( FinalPart.pt_forw == &FinalPart )  {
		ret = ap->a_miss( ap );
		status = "MISS bool";
		goto freeup;
	}

	/*
	 *  A DIRECT HIT.
	 *  Last chance to compute exact hit points and surface normals.
	 *  Then hand final partitioned intersection list to the application.
	 */
hitit:
	{
		register struct partition *pp;
		for( pp=FinalPart.pt_forw; pp != &FinalPart; pp=pp->pt_forw ){
			register struct soltab *stp;

			stp = pp->pt_inseg->seg_stp;
			functab[stp->st_id].ft_norm(
				pp->pt_inhit, stp, &(ap->a_ray) );

			if( ap->a_onehit )  break;
			stp = pp->pt_outseg->seg_stp;
			functab[stp->st_id].ft_norm(
				pp->pt_outhit, stp, &(ap->a_ray) );
		}
	}
	if(debug&DEBUG_SHOOT)  pr_partitions(&FinalPart,"a_hit()");

	ret = ap->a_hit( ap, &FinalPart );
	status = "HIT";

	/*
	 * Processing of this ray is complete.  Free dynamic resources.
	 */
freeup:
	{
		register struct partition *pp;

		/* Free up initial partition list */
		for( pp = InitialPart.pt_forw; pp != &InitialPart;  )  {
			register struct partition *newpp;
			newpp = pp;
			pp = pp->pt_forw;
			FREE_PT(newpp);
		}
		/* Free up final partition list */
		for( pp = FinalPart.pt_forw; pp != &FinalPart;  )  {
			register struct partition *newpp;
			newpp = pp;
			pp = pp->pt_forw;
			FREE_PT(newpp);
		}
	}
	{
		register struct seg *segp;	/* XXX */

		while( HeadSeg != SEG_NULL )  {
			segp = HeadSeg->seg_next;
			FREE_SEG( HeadSeg );
			HeadSeg = segp;
		}
	}
out:
	if( solidbits != (bitv_t *)0)
		vfree((char *)solidbits, "solidbits");
	if(debug&(DEBUG_SHOOT|DEBUG_ALLRAYS))
		rtlog( "%s, ret%d\n", status, ret);
	if( debug )  fflush(stderr);
	return( ret );
}

/*
 *			I N _ R P P
 *
 *  Compute the intersections of a ray with a rectangular parallelpiped (RPP)
 *  that has faces parallel to the coordinate planes
 *
 *  The algorithm here was developed by Gary Kuehl for GIFT.
 *  A good description of the approach used can be found in
 *  "??" by XYZZY and Barsky,
 *  ACM Transactions on Graphics, Vol 3 No 1, January 1984.
 *
 * Note -
 *  The computation of entry and exit distance is mandatory, as the final
 *  test catches the majority of misses.
 *
 *  Returns -
 *	 0  if ray does not hit RPP,
 *	!0  if ray hits RPP.
 *
 *  Implicit return -
 *	rp->r_min = dist from start of ray to point at which ray ENTERS solid
 *	rp->r_max = dist from start of ray to point at which ray LEAVES solid
 */
in_rpp( rp, invdir, min, max )
register struct xray *rp;
register fastf_t *invdir;	/* inverses of rp->r_dir[] */
register fastf_t *min;
register fastf_t *max;
{
	register fastf_t *pt = &rp->r_pt[0];
	FAST fastf_t sv;
#define st sv			/* reuse the register */

	/* Start with infinite ray, and trim it down */
	rp->r_min = -INFINITY;
	rp->r_max = INFINITY;

	/* X axis */
	if( rp->r_dir[X] < -EPSILON )  {
		if( (sv = (*min - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > sv)
			rp->r_max = sv;
		if( rp->r_min < (st = (*max - *pt) * *invdir) )
			rp->r_min = st;
	}  else if( rp->r_dir[X] > EPSILON )  {
		if( (st = (*max - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > st)
			rp->r_max = st;
		if( rp->r_min < ((sv = (*min - *pt) * *invdir)) )
			rp->r_min = sv;
	}  else  {
		/*
		 *  Direction along this axis is NEAR 0, (ray aligned
		 *  with axis), merely check pos against the boundaries.
		 */
		if( (*min > *pt) || (*max < *pt) )
			return(0);	/* MISS */;
	}

	/* Y axis */
	pt++; invdir++; max++; min++;
	if( rp->r_dir[Y] < -EPSILON )  {
		if( (sv = (*min - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > sv)
			rp->r_max = sv;
		if( rp->r_min < (st = (*max - *pt) * *invdir) )
			rp->r_min = st;
	}  else if( rp->r_dir[Y] > EPSILON )  {
		if( (st = (*max - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > st)
			rp->r_max = st;
		if( rp->r_min < ((sv = (*min - *pt) * *invdir)) )
			rp->r_min = sv;
	}  else  {
		/*
		 *  Direction along this axis is NEAR 0, (ray aligned
		 *  with axis), merely check against the boundaries.
		 */
		if( (*min > *pt) || (*max < *pt) )
			return(0);	/* MISS */;
	}

	/* Z axis */
	pt++; invdir++; max++; min++;
	if( rp->r_dir[Z] < -EPSILON )  {
		if( (sv = (*min - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > sv)
			rp->r_max = sv;
		if( rp->r_min < (st = (*max - *pt) * *invdir) )
			rp->r_min = st;
	}  else if( rp->r_dir[Z] > EPSILON )  {
		if( (st = (*max - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > st)
			rp->r_max = st;
		if( rp->r_min < ((sv = (*min - *pt) * *invdir)) )
			rp->r_min = sv;
	}  else  {
		/*
		 *  Direction along this axis is NEAR 0, (ray aligned
		 *  with axis), merely check against the boundaries.
		 */
		if( (*min > *pt) || (*max < *pt) )
			return(0);	/* MISS */;
	}

	if( rp->r_min >= rp->r_max )
		return(0);	/* MISS */
	return(1);		/* HIT */
}

bitv_or( out, in, nbits )
register bitv_t *out;
register bitv_t *in;
int nbits;
{
	register int words;

	words = (nbits+BITV_MASK)>>BITV_SHIFT;/*BITS2BYTES()/sizeof(bitv_t)*/
	while( words-- > 0 )
		*out++ |= *in++;
}
