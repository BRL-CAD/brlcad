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
static char RCSshoot[] = "@(#)$Header$ (BRL)";
#endif

char CopyRight_Notice[] = "@(#) Copyright (C) 1985 by the United States Army";

#include <stdio.h>
#include "../h/machine.h"
#include "../h/vmath.h"
#include "../h/raytrace.h"
#include "debug.h"

/*
 *			R T _ S H O O T R A Y
 *  
 *  Given a ray, shoot it at all the relevant parts of the model,
 *  (building the HeadSeg chain), and then call rt_boolregions()
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
 *  NOTE:  The appliction functions may call rt_shootray() recursively.
 *	Thus, none of the local variables may be static.
 */
int
rt_shootray( ap )
register struct application *ap;
{
	auto struct seg *HeadSeg;
	auto int ret;
	auto vect_t inv_dir;		/* inverses of ap->a_ray.r_dir */
	auto vect_t point;
	auto fastf_t	box_start, box_end, model_end;
	auto fastf_t	last_bool_start;
	auto union bitv_elem *solidbits;/* bits for all solids shot so far */
	auto bitv_t *regionbits;	/* bits for all involved regions */
	auto int trybool;
	auto char *status;
	auto union cutter *lastcut;
	auto struct partition InitialPart; /* Head of Initial Partitions */
	auto struct partition FinalPart;	/* Head of Final Partitions */
	auto struct seg *waitsegs;	/* segs awaiting rt_boolweave() */
	register union cutter *cutp;
	auto int hack_flag;

	ap->a_rt_i->rti_nrays++;
	if( ap->a_rt_i->needprep )
		rt_prep(ap->a_rt_i);

	InitialPart.pt_forw = InitialPart.pt_back = &InitialPart;
	FinalPart.pt_forw = FinalPart.pt_back = &FinalPart;

	HeadSeg = SEG_NULL;
	solidbits = BITV_NULL;		/* not allocated yet */

	if(rt_g.debug&(DEBUG_ALLRAYS|DEBUG_SHOOT|DEBUG_PARTITION)) {
		rt_log("**********shootray  %d,%d lvl=%d\n",
			ap->a_x, ap->a_y, ap->a_level );
		VPRINT("Pnt", ap->a_ray.r_pt);
		VPRINT("Dir", ap->a_ray.r_dir);
		fflush(stderr);		/* In case of instant death */
	}

	/* Verify that direction vector has unit length */
	if(rt_g.debug) {
		FAST fastf_t f, diff;
		f = ap->a_ray.r_dir[X] * ap->a_ray.r_dir[X] +
			ap->a_ray.r_dir[Y] * ap->a_ray.r_dir[Y] +
			ap->a_ray.r_dir[Z] * ap->a_ray.r_dir[Z];
		if( NEAR_ZERO(f, 0.0001) )  {
			rt_bomb("rt_shootray:  zero length dir vector\n");
			return(0);
		}
		diff = f - 1;
		if( !NEAR_ZERO( diff, 0.0001 ) )  {
			rt_log("rt_shootray: non-unit dir vect (x%d y%d lvl%d)\n",
				ap->a_x, ap->a_y, ap->a_level );
			f = 1/f;
			VSCALE( ap->a_ray.r_dir, ap->a_ray.r_dir, f );
		}
	}

	/* Compute the inverse of the direction cosines */
#define ZERO_COS	1.0e-10
	if( !NEAR_ZERO( ap->a_ray.r_dir[X], ZERO_COS ) )  {
		inv_dir[X]=1.0/ap->a_ray.r_dir[X];
	} else {
		inv_dir[X] = INFINITY;
		ap->a_ray.r_dir[X] = 0.0;
	}
	if( !NEAR_ZERO( ap->a_ray.r_dir[Y], ZERO_COS ) )  {
		inv_dir[Y]=1.0/ap->a_ray.r_dir[Y];
	} else {
		inv_dir[Y] = INFINITY;
		ap->a_ray.r_dir[Y] = 0.0;
	}
	if( !NEAR_ZERO( ap->a_ray.r_dir[Z], ZERO_COS ) )  {
		inv_dir[Z]=1.0/ap->a_ray.r_dir[Z];
	} else {
		inv_dir[Z] = INFINITY;
		ap->a_ray.r_dir[Z] = 0.0;
	}

	/*
	 *  If ray does not enter the model RPP, skip on.
	 */
	hack_flag = 0;
	if( !rt_in_rpp( &ap->a_ray, inv_dir, ap->a_rt_i->mdl_min, ap->a_rt_i->mdl_max )  ||
	    ap->a_ray.r_max <= 0.0 )  {
	    	if( ap->a_rt_i->rti_inf_box.bn.bn_len > 0 )  {
	    		/*
	    		 *  Because there are some infinite solids in this
	    		 *  model, we need to shoot at them even outside
	    		 *  the model RPP.  Thus, this hack.
	    		 */
	    		hack_flag = 1;
	    	}  else  {
			ap->a_rt_i->nmiss_model++;
			ret = ap->a_miss( ap );
			status = "MISS model";
			goto out;
	    	}
	}

top:
	if(rt_g.debug&DEBUG_SHOOT) rt_log("rt_shootray:  loop top, hack=%d\n", hack_flag);
	if( hack_flag == 0 )  {
		/* Push ray starting point to edge of model RPP */
		box_start = ap->a_ray.r_min;
		model_end = ap->a_ray.r_max;
		if( box_start < -10.0 )
			box_start = -10.0;	/* don't look back far */
		lastcut = CUTTER_NULL;
	}  else  {
		cutp = &(ap->a_rt_i->rti_inf_box);
		lastcut = cutp;
    		box_start = -100000;
		box_end = ap->a_ray.r_max = INFINITY;
    		model_end = box_end * 0.99999;
		waitsegs = SEG_NULL;
	}
	last_bool_start = box_start;

	if( solidbits == BITV_NULL )  {
		GET_BITV( solidbits );	/* see rt_get_bitv() for details */
		regionbits = &solidbits->be_v[2+(BITS2BYTES(ap->a_rt_i->nsolids)/sizeof(bitv_t))];
		BITZERO(solidbits->be_v,ap->a_rt_i->nsolids);
		BITZERO(regionbits,ap->a_rt_i->nregions);
	}
	trybool = 0;
	if( hack_flag )  goto middle;

	/*
	 *  While the ray remains inside model space,
	 *  push from box to box until ray emerges from
	 *  model space again (or first hit is found, if user is impatient).
	 */
	while( box_start < model_end )  {
		struct soltab **stpp;

		waitsegs = SEG_NULL;

		/* Move point (not box_start) slightly into the new box */
		{
			FAST fastf_t f;
			f = box_start + 0.005;
			VJOIN1( point, ap->a_ray.r_pt, f, ap->a_ray.r_dir );
		}
		cutp = &(ap->a_rt_i->rti_CutHead);
		while( cutp->cut_type == CUT_CUTNODE )  {
			if( point[cutp->cn.cn_axis] >= cutp->cn.cn_point )  {
				cutp=cutp->cn.cn_r;
			}  else  {
				cutp=cutp->cn.cn_l;
			}
		}
		if(cutp==CUTTER_NULL || cutp->cut_type != CUT_BOXNODE)
			rt_bomb("leaf not boxnode");

		/* Don't get stuck within the same box for long */
		if( cutp==lastcut )  {
			rt_log("%d,%d box push failed %g\n", ap->a_x, ap->a_y, box_end);
			box_end += 1.0;		/* Advance 1mm */
			box_start = box_end;
			continue;
		}
		lastcut = cutp;

		if( !rt_in_rpp(&ap->a_ray, inv_dir,
		     cutp->bn.bn_min, cutp->bn.bn_max) )  {
			rt_log("\nrt_shootray:  ray misses box? (%g,%g) (%g,%g) \n",ap->a_ray.r_min, ap->a_ray.r_max, box_start, box_end);
		     	if(rt_g.debug&DEBUG_SHOOT)  {
				VPRINT("r_pt", ap->a_ray.r_pt);
			     	VPRINT("Point", point);
				VPRINT("Dir", ap->a_ray.r_dir);
			     	rt_pr_cut( cutp, 0 );
		     	}
		     	if( box_end >= INFINITY )  break;
			box_end += 1.0;		/* Advance 1mm */
			box_start = box_end;
		     	continue;
		}
middle:
		box_end = ap->a_ray.r_max;	

		if(rt_g.debug&DEBUG_SHOOT) {
			rt_log("ray (%g, %g) %g\n", box_start, box_end, model_end);
			VPRINT("Point", point);
			rt_pr_cut( cutp, 0 );
		}

		/* Consider all objects within the box */
		stpp = &(cutp->bn.bn_list[cutp->bn.bn_len-1]);
		for( ; stpp >= cutp->bn.bn_list; stpp-- )  {
			register struct soltab *stp;
			register struct seg *newseg;

			stp = *stpp;
			if( BITTEST( solidbits->be_v, stp->st_bit ) )  {
				if(rt_g.debug&DEBUG_SHOOT)rt_log("skipping %s\n", stp->st_name);
				ap->a_rt_i->nmiss_tree++;
				continue;	/* already shot */
			}

			/* Shoot a ray */
			if(rt_g.debug&DEBUG_SHOOT)rt_log("shooting %s\n", stp->st_name);
			BITSET( solidbits->be_v, stp->st_bit );

			/* If ray does not strike the bounding RPP, skip on */
			if(
			   rt_functab[stp->st_id].ft_use_rpp &&
			   ( !rt_in_rpp( &ap->a_ray, inv_dir,
			      stp->st_min, stp->st_max ) ||
			      ap->a_ray.r_max < -10.0 )
			)  {
				ap->a_rt_i->nmiss_solid++;
				continue;	/* MISS */
			}

			ap->a_rt_i->nshots++;
			if( (newseg = rt_functab[stp->st_id].ft_shot( 
				stp, &ap->a_ray )
			     ) == SEG_NULL )  {
				ap->a_rt_i->nmiss++;
				continue;	/* MISS */
			}
			if(rt_g.debug&DEBUG_SHOOT)  rt_pr_seg(newseg);
			trybool++;	/* flag to rerun bool, below */

			/* Add seg chain to list awaiting rt_boolweave() */
			{
				register struct seg *seg2 = newseg;
				while( seg2->seg_next != SEG_NULL )
					seg2 = seg2->seg_next;
				seg2->seg_next = waitsegs;
				waitsegs = newseg;
			}
			/* Would be even better done by per-partition bitv */
			rt_bitv_or( regionbits, stp->st_regions, stp->st_maxreg);
			if(rt_g.debug&DEBUG_PARTITION)
				rt_pr_bitv( "shoot Regionbits", regionbits, ap->a_rt_i->nregions);
			ap->a_rt_i->nhits++;
		}

		/* Special case for efficiency -- first hit only */
		/* Only run this every three hits, to balance cost/benefit */
		if( trybool>=3 && ap->a_onehit && waitsegs != SEG_NULL )  {
			/* Weave these segments into partition list */
			rt_boolweave( waitsegs, &InitialPart );

			/* Add segment chain to list of used segments */
			{
				register struct seg *seg2 = waitsegs;
				while( seg2->seg_next != SEG_NULL )
					seg2 = seg2->seg_next;
				seg2->seg_next = HeadSeg;
				HeadSeg = waitsegs;
				waitsegs = SEG_NULL;
			}
			/* Evaluate regions upto box_end */
			rt_boolfinal( &InitialPart, &FinalPart,
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
	/*
	 *  Ray has finally left known space --
	 *  Weave any remaining segments into the partition list.
	 */
	if( waitsegs != SEG_NULL )  {
		rt_boolweave( waitsegs, &InitialPart );

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

	/* HeadSeg chain now has all segments hit by this ray */
	if( HeadSeg == SEG_NULL )  {
		ret = ap->a_miss( ap );
		status = "MISS solids";
		if( hack_flag == 0 && ap->a_rt_i->rti_inf_box.bn.bn_len > 0 )  {
	    		/*
	    		 *  Because there are some infinite solids in this
	    		 *  model, we need to shoot at them even outside
	    		 *  the model RPP.  Thus, this hack.
	    		 */
	    		hack_flag = 1;
			goto top;
		}
		goto out;
	}

	/*
	 *  All intersections of the ray with the model have
	 *  been computed.  Evaluate the boolean trees over each partition.
	 */
	rt_boolfinal( &InitialPart, &FinalPart, 0.0, model_end, regionbits, ap);

	if( FinalPart.pt_forw == &FinalPart )  {
		ret = ap->a_miss( ap );
		status = "MISS bool";
		if( hack_flag == 0 && ap->a_rt_i->rti_inf_box.bn.bn_len > 0 )  {
	    		/*
	    		 *  Because there are some infinite solids in this
	    		 *  model, we need to shoot at them even outside
	    		 *  the model RPP.  Thus, this hack.
	    		 */
	    		hack_flag = 1;
			goto top;
		}
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
			rt_functab[stp->st_id].ft_norm(
				pp->pt_inhit, stp, &(ap->a_ray) );

			if( ap->a_onehit )  break;
			stp = pp->pt_outseg->seg_stp;
			rt_functab[stp->st_id].ft_norm(
				pp->pt_outhit, stp, &(ap->a_ray) );
		}
	}
	if(rt_g.debug&DEBUG_SHOOT)  rt_pr_partitions(&FinalPart,"a_hit()");

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
	if( solidbits != BITV_NULL)  {
		FREE_BITV( solidbits );
	}
	if(rt_g.debug&(DEBUG_SHOOT|DEBUG_ALLRAYS))
		rt_log( "%s, ret%d\n", status, ret);
	if( rt_g.debug )  fflush(stderr);
	return( ret );
}

/*
 *			R T _ I N _ R P P
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
rt_in_rpp( rp, invdir, min, max )
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
	if( rp->r_dir[X] < 0.0 )  {
		/* Heading towards smaller numbers */
		/* if( *min > *pt )  miss */
		if( (sv = (*min - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > sv)
			rp->r_max = sv;
		if( rp->r_min < (st = (*max - *pt) * *invdir) )
			rp->r_min = st;
	}  else if( rp->r_dir[X] > 0.0 )  {
		/* Heading towards larger numbers */
		/* if( *max < *pt )  miss */
		if( (st = (*max - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > st)
			rp->r_max = st;
		if( rp->r_min < ((sv = (*min - *pt) * *invdir)) )
			rp->r_min = sv;
	}  else  {
		/*
		 *  Direction cosines along this axis is NEAR 0,
		 *  which implies that the ray is perpendicular to the axis,
		 *  so merely check position against the boundaries.
		 */
		if( (*min > *pt) || (*max < *pt) )
			return(0);	/* MISS */
	}

	/* Y axis */
	pt++; invdir++; max++; min++;
	if( rp->r_dir[Y] < 0.0 )  {
		if( (sv = (*min - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > sv)
			rp->r_max = sv;
		if( rp->r_min < (st = (*max - *pt) * *invdir) )
			rp->r_min = st;
	}  else if( rp->r_dir[Y] > 0.0 )  {
		if( (st = (*max - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > st)
			rp->r_max = st;
		if( rp->r_min < ((sv = (*min - *pt) * *invdir)) )
			rp->r_min = sv;
	}  else  {
		if( (*min > *pt) || (*max < *pt) )
			return(0);	/* MISS */
	}

	/* Z axis */
	pt++; invdir++; max++; min++;
	if( rp->r_dir[Z] < 0.0 )  {
		if( (sv = (*min - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > sv)
			rp->r_max = sv;
		if( rp->r_min < (st = (*max - *pt) * *invdir) )
			rp->r_min = st;
	}  else if( rp->r_dir[Z] > 0.0 )  {
		if( (st = (*max - *pt) * *invdir) < 0.0 )
			return(0);	/* MISS */
		if(rp->r_max > st)
			rp->r_max = st;
		if( rp->r_min < ((sv = (*min - *pt) * *invdir)) )
			rp->r_min = sv;
	}  else  {
		if( (*min > *pt) || (*max < *pt) )
			return(0);	/* MISS */
	}

	/* If equal, RPP is actually a plane */
	if( rp->r_min > rp->r_max )
		return(0);	/* MISS */
	return(1);		/* HIT */
}

/*
 *			R T _ B I T V _ O R
 */
void
rt_bitv_or( out, in, nbits )
register bitv_t *out;
register bitv_t *in;
int nbits;
{
	register int words;

	words = (nbits+BITV_MASK)>>BITV_SHIFT;/*BITS2BYTES()/sizeof(bitv_t)*/
	while( words-- > 0 )
		*out++ |= *in++;
}

/*
 *  			R T _ G E T _ B I T V
 *  
 *  This routine is called by the GET_BITV macro when the freelist
 *  is exhausted.  Rather than simply getting one additional structure,
 *  we get a whole batch, saving overhead.  When this routine is called,
 *  the bitv resource must already be locked.
 *  malloc() locking is done in rt_malloc.
 *
 *  Also note that there is a bit of trickery going on here:
 *  the *real* size of be_v[] array is determined at runtime, here.
 */
void
rt_get_bitv()  {
	register char *cp;
	register int bytes;
	register int size;		/* size of structure to really get */

	size = BITS2BYTES(rt_i.nsolids) + BITS2BYTES(rt_i.nregions) + 4*sizeof(bitv_t);
	size = (size+sizeof(long)-1) & ~(sizeof(long)-1);
	bytes = rt_byte_roundup(16*size);
	if( (cp = rt_malloc(bytes, "rt_get_bitv")) == (char *)0 )  {
		rt_log("rt_get_bitv: malloc failure\n");
		exit(17);
	}
	while( bytes >= size )  {
		((union bitv_elem *)cp)->be_next = rt_i.FreeBitv;
		rt_i.FreeBitv = (union bitv_elem *)cp;
		cp += size;
		bytes -= size;
	}
}
