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
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./debug.h"

struct resource rt_uniresource;		/* Resources for uniprocessor */

#ifdef cray
#define AUTO register
#else
#define AUTO auto
#endif

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
 *  must have unit length;  this is mandatory, and is not ordinarily checked,
 *  in the name of efficiency.
 *
 *  Input:  Pointer to an application structure, with these mandatory fields:
 *	a_ray.r_pt	Starting point of ray to be fired
 *	a_ray.r_dir	UNIT VECTOR with direction to fire in (dir cosines)
 *	a_hit		Routine to call when something is hit
 *	a_miss		Routine to call when ray misses everything
 *
 *  Calls user's a_miss() or a_hit() routine as appropriate.
 *  Passes a_hit() routine list of partitions, with only hit_dist
 *  fields valid.  Normal computation deferred to user code,
 *  to avoid needless computation here.
 *
 *  Returns: whatever the application function returns (an int).
 *
 *  NOTE:  The appliction functions may call rt_shootray() recursively.
 *	Thus, none of the local variables may be static.
 *
 *  An open issue for execution in a PARALLEL environment is locking
 *  of the statistics variables.
 */
int
rt_shootray( ap )
register struct application *ap;
{
	AUTO struct seg		*HeadSeg;
	AUTO int		ret;
	auto vect_t		inv_dir;	/* inverses of ap->a_ray.r_dir */
	AUTO fastf_t		box_start, box_end, model_end;
	AUTO fastf_t		last_bool_start;
	AUTO union bitv_elem	*solidbits;	/* bits for all solids shot so far */
	AUTO bitv_t		*regionbits;	/* bits for all involved regions */
	AUTO int		trybool;
	AUTO char		*status;
	AUTO union cutter	*lastcut;
	auto struct partition	InitialPart;	/* Head of Initial Partitions */
	auto struct partition	FinalPart;	/* Head of Final Partitions */
	AUTO struct seg		*waitsegs;	/* segs awaiting rt_boolweave() */
	AUTO struct soltab	**stpp;
	auto struct xray	newray;		/* closer ray start */
	AUTO fastf_t		dist_corr;	/* correction distance */
	register union cutter	*cutp;
	AUTO fastf_t		odist_corr, obox_start, obox_end;	/* temp */
	AUTO int		push_flag = 0;
	double			fraction;
	int			exponent;

	RT_AP_CHECK(ap);
	if( ap->a_resource == RESOURCE_NULL )  {
		ap->a_resource = &rt_uniresource;
		rt_uniresource.re_magic = RESOURCE_MAGIC;
rt_log("rt_shootray:  defaulting a_resource to &rt_uniresource\n");
	}
	RT_RESOURCE_CHECK(ap->a_resource);

	if(rt_g.debug&(DEBUG_ALLRAYS|DEBUG_SHOOT|DEBUG_PARTITION)) {
		rt_g.rtg_logindent += 2;
		rt_log("\n**********shootray cpu=%d  %d,%d lvl=%d (%s)\n",
			ap->a_resource->re_cpu,
			ap->a_x, ap->a_y,
			ap->a_level,
			ap->a_purpose != (char *)0 ? ap->a_purpose : "?" );
		VPRINT("Pnt", ap->a_ray.r_pt);
		VPRINT("Dir", ap->a_ray.r_dir);
	}

	/* Since this count provides the RTFM, it must be semaphored */
	RES_ACQUIRE( &rt_g.res_stats );
	ap->a_rt_i->rti_nrays++;
	RES_RELEASE( &rt_g.res_stats );

	if( ap->a_rt_i->needprep )
		rt_prep(ap->a_rt_i);

	InitialPart.pt_forw = InitialPart.pt_back = &InitialPart;
	FinalPart.pt_forw = FinalPart.pt_back = &FinalPart;

	HeadSeg = SEG_NULL;
	waitsegs = SEG_NULL;

	/* XXX WARNING this needs attention for multiple rt_i sizes */
	GET_BITV( ap->a_rt_i, solidbits, ap->a_resource );	/* see rt_get_bitv() for details */
	bzero( (char *)solidbits, ap->a_rt_i->rti_bv_bytes );
	regionbits = &solidbits->be_v[2+(BITS2BYTES(ap->a_rt_i->nsolids)/sizeof(bitv_t))];

	/* Verify that direction vector has unit length */
	if(rt_g.debug) {
		FAST fastf_t f, diff;
		f = MAGSQ(ap->a_ray.r_dir);
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
	if( !NEAR_ZERO( ap->a_ray.r_dir[X], SQRT_SMALL_FASTF ) )  {
		inv_dir[X]=1.0/ap->a_ray.r_dir[X];
	} else {
		inv_dir[X] = INFINITY;
		ap->a_ray.r_dir[X] = 0.0;
	}
	if( !NEAR_ZERO( ap->a_ray.r_dir[Y], SQRT_SMALL_FASTF ) )  {
		inv_dir[Y]=1.0/ap->a_ray.r_dir[Y];
	} else {
		inv_dir[Y] = INFINITY;
		ap->a_ray.r_dir[Y] = 0.0;
	}
	if( !NEAR_ZERO( ap->a_ray.r_dir[Z], SQRT_SMALL_FASTF ) )  {
		inv_dir[Z]=1.0/ap->a_ray.r_dir[Z];
	} else {
		inv_dir[Z] = INFINITY;
		ap->a_ray.r_dir[Z] = 0.0;
	}

	/*
	 *  If there are infinite solids in the model,
	 *  shoot at all of them, all of the time
	 *  (until per-solid classification is implemented)
	 *  before considering the model RPP.
	 *  This code is a streamlined version of the real version.
	 */
	if( ap->a_rt_i->rti_inf_box.bn.bn_len > 0 )  {
		/* Consider all objects within the box */
		cutp = &(ap->a_rt_i->rti_inf_box);
		stpp = &(cutp->bn.bn_list[cutp->bn.bn_len-1]);
		for( ; stpp >= cutp->bn.bn_list; stpp-- )  {
			register struct soltab *stp;
			register struct seg *newseg;

			stp = *stpp;
			if( stp->st_id < 0 || stp->st_id >= rt_nfunctab )
				rt_bomb("rt_shootray:  bad st_id 1");

			/* Shoot a ray */
			if(rt_g.debug&DEBUG_SHOOT)rt_log("shooting %s\n", stp->st_name);
			BITSET( solidbits->be_v, stp->st_bit );
			ap->a_rt_i->nshots++;
			if( (newseg = rt_functab[stp->st_id].ft_shot( 
				stp, &ap->a_ray, ap )
			     ) == SEG_NULL )  {
				ap->a_rt_i->nmiss++;
				continue;	/* MISS */
			}
			if(rt_g.debug&DEBUG_SHOOT)  rt_pr_seg(newseg);
			/* Add seg chain to list awaiting rt_boolweave() */
			{
				register struct seg *seg2 = newseg;
				while( seg2->seg_next != SEG_NULL )
					seg2 = seg2->seg_next;
				seg2->seg_next = waitsegs;
				waitsegs = newseg;
			}
			/* Would be even better done by per-partition bitv */
			{
				register int words;
				register bitv_t *in = stp->st_regions;
				register bitv_t *out = regionbits;
				/* BITS2BYTES() / sizeof(bitv_t) */
				words = (stp->st_maxreg+BITV_MASK)>>BITV_SHIFT;
#				ifdef VECTORIZE
#					include "noalias.h"
					for( ; words > 0; words-- )
						regionbits[words-1] |= in[words-1];
#				else
					while( words-- > 0 )
						*out++ |= *in++;
#				endif
			}

			if(rt_g.debug&DEBUG_PARTITION)
				rt_pr_bitv( "shoot Regionbits", regionbits, ap->a_rt_i->nregions);
			ap->a_rt_i->nhits++;
		}
	}

	/*
	 *  If ray does not enter the model RPP, skip on.
	 *  If ray ends exactly at the model RPP, trace it.
	 */
	if( !rt_in_rpp( &ap->a_ray, inv_dir, ap->a_rt_i->mdl_min, ap->a_rt_i->mdl_max )  ||
	    ap->a_ray.r_max < 0.0 )  {
	    	if( waitsegs != SEG_NULL )  {
	    		/* Go handle the infinite objects we hit */
	    		model_end = INFINITY;
	    		goto weave;
	    	}
		ap->a_rt_i->nmiss_model++;
		ret = ap->a_miss( ap );
		status = "MISS model";
		goto out;
	}

	/*
	 *  The interesting part of the ray starts at distance 0.
	 *  If the ray enters the model at a negative distance,
	 *  (ie, the ray starts within the model RPP),
	 *  we only look at little bit behind (BACKING_DIST) to see if we are
	 *  just coming out of something, but never further back than
	 *  the intersection with the model RPP.
	 *  If the ray enters the model at a positive distance,
	 *  we always start there.
	 *  It is vital that we never pick a start point outside the
	 *  model RPP, or the space partitioning tree will pick the
	 *  wrong box and the ray will miss it.
	 *
	 *  BACKING_DIST should probably be determined by floating point
	 *  noise factor due to model RPP size -vs- number of bits of
	 *  floating point mantissa significance, rather than a constant,
	 *  but that is too hideous to think about here.
	 *  Also note that applications that really depend on knowing
	 *  what region they are leaving from should probably back their
	 *  own start-point up, rather than depending on it here, but
	 *  it isn't much trouble here.
	 */
#define BACKING_DIST	(-2.0)		/* mm to look behind start point */
#define OFFSET_DIST	0.01		/* mm to advance point into box */
	box_start = ap->a_ray.r_min;
	if( box_start < BACKING_DIST )
		box_start = BACKING_DIST; /* Only look a little bit behind */
	box_start -= OFFSET_DIST;	/* Compensate for OFFSET_DIST below on 1st loop */
	box_end = model_end = ap->a_ray.r_max;
	lastcut = CUTTER_NULL;
	last_bool_start = BACKING_DIST;
	trybool = 0;
	newray = ap->a_ray;		/* struct copy */
	odist_corr = obox_start = obox_end = -99;

	/*
	 *  While the ray remains inside model space,
	 *  push from box to box until ray emerges from
	 *  model space again (or first hit is found, if user is impatient).
	 *  It is vitally important to always stay within the model RPP, or
	 *  the space partitoning tree will pick wrong boxes & miss them.
	 */
RT_RESOURCE_CHECK(ap->a_resource);	/* XXX */
	for(;;)  {
		/*
		 * Move newray point (not box_start)
		 * slightly into the new box.
		 * Note that a box is never less than 1mm wide per axis.
		 */
		dist_corr = box_start + OFFSET_DIST;
		if( dist_corr >= model_end )
			break;	/* done! */
		VJOIN1( newray.r_pt, ap->a_ray.r_pt,
			dist_corr, ap->a_ray.r_dir );
		if( rt_g.debug&DEBUG_BOXING) {
			rt_log("dist_corr=%g\n", dist_corr);
			VPRINT("newray.r_pt", newray.r_pt);
		}

		cutp = &(ap->a_rt_i->rti_CutHead);
		while( cutp->cut_type == CUT_CUTNODE )  {
			if( newray.r_pt[cutp->cn.cn_axis] >=
			    cutp->cn.cn_point )  {
				cutp=cutp->cn.cn_r;
			}  else  {
				cutp=cutp->cn.cn_l;
			}
		}
		if(cutp==CUTTER_NULL || cutp->cut_type != CUT_BOXNODE)
			rt_bomb("leaf not boxnode");

		/* Don't get stuck within the same box for long */
		if( cutp==lastcut )  {
			if( rt_g.debug&DEBUG_SHOOT )  {
				rt_log("%d,%d box push dist_corr o=%e n=%e model_end=%e\n",
					ap->a_x, ap->a_y,
					odist_corr, dist_corr, model_end );
				rt_log("box_start o=%e n=%e, box_end o=%e n=%e\n",
					obox_start, box_start,
					obox_end, box_end );
				VPRINT("a_ray.r_pt", ap->a_ray.r_pt);
			     	VPRINT("Point", newray.r_pt);
				VPRINT("Dir", newray.r_dir);
			     	rt_pr_cut( cutp, 0 );
			}

			/* Advance 1mm, or smallest value that hardware
			 * floating point resolution will allow.
			 */
			fraction = frexp( box_end, &exponent );
			if( exponent <= 0 )  {
				/* Never advance less than 1mm */
				box_start = box_end + 1.0;
			} else {
				if( sizeof(fastf_t) <= 4 )
					fraction += 1.0e-5;
				else
					fraction += 1.0e-14;
				box_start = ldexp( fraction, exponent );
			}
			push_flag++;
			continue;
		}
		if( push_flag )  {
			push_flag = 0;
			rt_log("%d,%d Escaped %d. dist_corr=%e, box_start=%e, box_end=%e\n",
				ap->a_x, ap->a_y, push_flag,
				dist_corr, box_start, box_end );
		}
		lastcut = cutp;

		if( !rt_in_rpp(&newray, inv_dir,
		     cutp->bn.bn_min, cutp->bn.bn_max) )  {
rt_log("\nrt_shootray:  missed box: rmin,rmax(%g,%g) box(%g,%g)\n",
				newray.r_min, newray.r_max,
				box_start, box_end);
/**		     	if(rt_g.debug&DEBUG_SHOOT)  { ***/ {
				VPRINT("a_ray.r_pt", ap->a_ray.r_pt);
			     	VPRINT("Point", newray.r_pt);
				VPRINT("Dir", newray.r_dir);
				VPRINT("inv_dir", inv_dir);
			     	rt_pr_cut( cutp, 0 );
				(void)rt_DB_rpp(&newray, inv_dir,
				     cutp->bn.bn_min, cutp->bn.bn_max);
		     	}
		     	if( box_end >= INFINITY )  break;
			box_start = box_end;

			/* Advance 1mm, or smallest value that hardware
			 * floating point resolution will allow.
			 */
			fraction = frexp( box_end, &exponent );
			if( exponent <= 0 )  {
				/* Never advance less than 1mm */
				box_end += 1.0;
			} else {
				if( sizeof(fastf_t) <= 4 )
					fraction += 1.0e-5;
				else
					fraction += 1.0e-14;
				box_end = ldexp( fraction, exponent );
			}
		     	continue;
		}
		odist_corr = dist_corr;
		obox_start = box_start;
		obox_end = box_end;
		box_start = dist_corr + newray.r_min;
		box_end = dist_corr + newray.r_max;

		if(rt_g.debug&DEBUG_SHOOT) {
			rt_log("ray (%g, %g) %g\n", box_start, box_end, model_end);
			VPRINT("Point", newray.r_pt);
			rt_pr_cut( cutp, 0 );
		}

		/* Consider all objects within the box */
		stpp = &(cutp->bn.bn_list[cutp->bn.bn_len-1]);
		for( ; stpp >= cutp->bn.bn_list; stpp-- )  {
			register struct soltab *stp;
			register struct seg *newseg;

			stp = *stpp;
			if( stp->st_id < 0 || stp->st_id >= rt_nfunctab )
				rt_bomb("rt_shootray:  bad st_id 2");

			if( BITTEST( solidbits->be_v, stp->st_bit ) )  {
				if(rt_g.debug&DEBUG_SHOOT)rt_log("eff skip %s\n", stp->st_name);
				ap->a_rt_i->nmiss_tree++;
				continue;	/* already shot */
			}

			/* Shoot a ray */
			BITSET( solidbits->be_v, stp->st_bit );

			/* Check against bounding RPP, if desired by solid */
			if( rt_functab[stp->st_id].ft_use_rpp )  {
				if( !rt_in_rpp( &newray, inv_dir,
				    stp->st_min, stp->st_max ) )  {
					if(rt_g.debug&DEBUG_SHOOT)rt_log("rpp miss %s\n", stp->st_name);
					ap->a_rt_i->nmiss_solid++;
					continue;	/* MISS */
				}
				if( dist_corr + newray.r_max < BACKING_DIST )  {
					if(rt_g.debug&DEBUG_SHOOT)rt_log("rpp skip %s, dist_corr=%g, r_max=%g\n", stp->st_name, dist_corr, newray.r_max);
					ap->a_rt_i->nmiss_solid++;
					continue;	/* MISS */
				}
			}

			if(rt_g.debug&DEBUG_SHOOT)rt_log("shooting %s\n", stp->st_name);
			ap->a_rt_i->nshots++;
			if( (newseg = rt_functab[stp->st_id].ft_shot( 
				stp, &newray, ap )
			     ) == SEG_NULL )  {
				ap->a_rt_i->nmiss++;
				continue;	/* MISS */
			}

			/* Add seg chain to list awaiting rt_boolweave() */
			{
				register struct seg *seg2 = newseg;
				for(;;)  {
					/* Restore to original distance */
					seg2->seg_in.hit_dist += dist_corr;
					seg2->seg_out.hit_dist += dist_corr;
					if( seg2->seg_next == SEG_NULL )
						break;
					seg2 = seg2->seg_next;
				}
				if(rt_g.debug&DEBUG_SHOOT)  rt_pr_seg(newseg);
				seg2->seg_next = waitsegs;
				waitsegs = newseg;
			}
			trybool++;	/* flag to rerun bool, below */

			/* Would be even better done by per-partition bitv */
			{
				register int words;
				register bitv_t *in = stp->st_regions;
				register bitv_t *out = regionbits;
				/* BITS2BYTES() / sizeof(bitv_t) */
				words = (stp->st_maxreg+BITV_MASK)>>BITV_SHIFT;
#				ifdef VECTORIZE
#					include "noalias.h"
					for( ; words > 0; words-- )
						regionbits[words-1] |= in[words-1];
#				else
					while( words-- > 0 )
						*out++ |= *in++;
#				endif
			}

			if(rt_g.debug&DEBUG_PARTITION)
				rt_pr_bitv( "shoot Regionbits", regionbits, ap->a_rt_i->nregions);
			ap->a_rt_i->nhits++;
		}

		/* Special case for efficiency -- first hit only */
		/* Only run this every three hits, to balance cost/benefit */
		if( trybool>=3 && ap->a_onehit && waitsegs != SEG_NULL )  {
RT_RESOURCE_CHECK(ap->a_resource);	/* XXX */
			/* Weave these segments into partition list */
			rt_boolweave( waitsegs, &InitialPart, ap );

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
weave:
	if( waitsegs != SEG_NULL )  {
		register struct seg *seg2 = waitsegs;

		rt_boolweave( waitsegs, &InitialPart, ap );

		/* Add segment chain to list of used segments */
		while( seg2->seg_next != SEG_NULL )
			seg2 = seg2->seg_next;
		seg2->seg_next = HeadSeg;
		HeadSeg = waitsegs;
		waitsegs = SEG_NULL;
	}

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
	rt_boolfinal( &InitialPart, &FinalPart, BACKING_DIST,
		ap->a_rt_i->rti_inf_box.bn.bn_len > 0 ? INFINITY : model_end,
		regionbits, ap);

	if( FinalPart.pt_forw == &FinalPart )  {
		ret = ap->a_miss( ap );
		status = "MISS bool";
		goto freeup;
	}

	/*
	 *  Ray/model intersections exist.  Pass the list to the
	 *  user's a_hit() routine.  Note that only the hit_dist
	 *  elements of pt_inhit and pt_outhit have been computed yet.
	 *  To compute both hit_point and hit_normal, use the
	 *
	 *  	RT_HIT_NORM( hitp, stp, rayp )
	 *
	 *  macro.  To compute just hit_point, use
	 *
	 *  VJOIN1( hitp->hit_point, rp->r_pt, hitp->hit_dist, rp->r_dir );
	 */
hitit:
	if(rt_g.debug&DEBUG_SHOOT)  rt_pr_partitions(ap->a_rt_i,&FinalPart,"a_hit()");

RT_RESOURCE_CHECK(ap->a_resource);	/* XXX */
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
			FREE_PT(newpp, ap->a_resource);
		}
		/* Free up final partition list */
		for( pp = FinalPart.pt_forw; pp != &FinalPart;  )  {
			register struct partition *newpp;
			newpp = pp;
			pp = pp->pt_forw;
			FREE_PT(newpp, ap->a_resource);
		}
	}
	{
		register struct seg *segp;

		while( HeadSeg != SEG_NULL )  {
			segp = HeadSeg->seg_next;
			FREE_SEG( HeadSeg, ap->a_resource );
			HeadSeg = segp;
		}
	}
out:
	if( solidbits != BITV_NULL)  {
		FREE_BITV( solidbits, ap->a_resource );
	}
	if(rt_g.debug&(DEBUG_ALLRAYS|DEBUG_SHOOT|DEBUG_PARTITION))  {
		if( rt_g.rtg_logindent > 0 )
			rt_g.rtg_logindent -= 2;
		else
			rt_g.rtg_logindent = 0;
		rt_log("----------shootray cpu=%d  %d,%d lvl=%d (%s) %s ret=%d\n",
			ap->a_resource->re_cpu,
			ap->a_x, ap->a_y,
			ap->a_level,
			ap->a_purpose != (char *)0 ? ap->a_purpose : "?",
			status, ret);
	}
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
#ifdef VECTORIZE
#	include "noalias.h"
	for( ; words > 0; words-- )
		out[words-1] |= in[words-1];
#else
	while( words-- > 0 )
		*out++ |= *in++;
#endif
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
rt_get_bitv(rtip, res)
struct rt_i		*rtip;
register struct resource *res;
{
	register char *cp;
	register int bytes;
	register int size;		/* size of structure to really get */

	size = rtip->rti_bv_bytes;
	if( size < 1 )  rt_bomb("rt_get_bitv");
	size = (size+sizeof(long)-1) & ~(sizeof(long)-1);
	bytes = rt_byte_roundup(16*size);
	if( (cp = rt_malloc(bytes, "rt_get_bitv")) == (char *)0 )  {
		rt_log("rt_get_bitv: malloc failure\n");
		exit(17);
	}
	while( bytes >= size )  {
		((union bitv_elem *)cp)->be_next = res->re_bitv;
		res->re_bitv = (union bitv_elem *)cp;
		res->re_bitvlen++;
		cp += size;
		bytes -= size;
	}
}

/* For debugging */
rt_DB_rpp( rp, invdir, min, max )
register struct xray *rp;
register fastf_t *invdir;	/* inverses of rp->r_dir[] */
register fastf_t *min;
register fastf_t *max;
{
	register fastf_t *pt = &rp->r_pt[0];
	FAST fastf_t sv;

	/* Start with infinite ray, and trim it down */
	rp->r_min = -INFINITY;
	rp->r_max = INFINITY;

	/* X axis */
rt_log("r_dir[X] = %g\n", rp->r_dir[X]);
	if( rp->r_dir[X] < 0.0 )  {
		/* Heading towards smaller numbers */
		/* if( *min > *pt )  miss */
		sv = (*min - *pt) * *invdir;
rt_log("sv=%g, r_max=%g\n", sv, rp->r_max);
		if( sv < 0.0 )
			goto miss;
		if(rp->r_max > sv)
			rp->r_max = sv;
		st = (*max - *pt) * *invdir;
rt_log("st=%g, r_min=%g\n", st, rp->r_min);
		if( rp->r_min < st )
			rp->r_min = st;
rt_log("r_min=%g, r_max=%g\n", rp->r_min, rp->r_max);
	}  else if( rp->r_dir[X] > 0.0 )  {
		/* Heading towards larger numbers */
		/* if( *max < *pt )  miss */
		st = (*max - *pt) * *invdir;
rt_log("st=%g, r_max=%g\n", st, rp->r_max);
		if( st < 0.0 )
			goto miss;
		if(rp->r_max > st)
			rp->r_max = st;
		sv = (*min - *pt) * *invdir;
rt_log("sv=%g, r_min=%g\n", sv, rp->r_min);
		if( rp->r_min < sv )
			rp->r_min = sv;
rt_log("r_min=%g, r_max=%g\n", rp->r_min, rp->r_max);
	}  else  {
		/*
		 *  Direction cosines along this axis is NEAR 0,
		 *  which implies that the ray is perpendicular to the axis,
		 *  so merely check position against the boundaries.
		 */
		if( (*min > *pt) || (*max < *pt) )
			goto miss;
	}

	/* Y axis */
	pt++; invdir++; max++; min++;
rt_log("r_dir[Y] = %g\n", rp->r_dir[Y]);
	if( rp->r_dir[Y] < 0.0 )  {
		/* Heading towards smaller numbers */
		/* if( *min > *pt )  miss */
		sv = (*min - *pt) * *invdir;
rt_log("sv=%g, r_max=%g\n", sv, rp->r_max);
		if( sv < 0.0 )
			goto miss;
		if(rp->r_max > sv)
			rp->r_max = sv;
		st = (*max - *pt) * *invdir;
rt_log("st=%g, r_min=%g\n", st, rp->r_min);
		if( rp->r_min < st )
			rp->r_min = st;
rt_log("r_min=%g, r_max=%g\n", rp->r_min, rp->r_max);
	}  else if( rp->r_dir[Y] > 0.0 )  {
		/* Heading towards larger numbers */
		/* if( *max < *pt )  miss */
		st = (*max - *pt) * *invdir;
rt_log("st=%g, r_max=%g\n", st, rp->r_max);
		if( st < 0.0 )
			goto miss;
		if(rp->r_max > st)
			rp->r_max = st;
		sv = (*min - *pt) * *invdir;
rt_log("sv=%g, r_min=%g\n", sv, rp->r_min);
		if( rp->r_min < sv )
			rp->r_min = sv;
rt_log("r_min=%g, r_max=%g\n", rp->r_min, rp->r_max);
	}  else  {
		if( (*min > *pt) || (*max < *pt) )
			goto miss;
	}

	/* Z axis */
	pt++; invdir++; max++; min++;
rt_log("r_dir[Z] = %g\n", rp->r_dir[Z]);
	if( rp->r_dir[Z] < 0.0 )  {
		/* Heading towards smaller numbers */
		/* if( *min > *pt )  miss */
		sv = (*min - *pt) * *invdir;
rt_log("sv=%g, r_max=%g\n", sv, rp->r_max);
		if( sv < 0.0 )
			goto miss;
		if(rp->r_max > sv)
			rp->r_max = sv;
		st = (*max - *pt) * *invdir;
rt_log("st=%g, r_min=%g\n", st, rp->r_min);
		if( rp->r_min < st )
			rp->r_min = st;
rt_log("r_min=%g, r_max=%g\n", rp->r_min, rp->r_max);
	}  else if( rp->r_dir[Z] > 0.0 )  {
		/* Heading towards larger numbers */
		/* if( *max < *pt )  miss */
		st = (*max - *pt) * *invdir;
rt_log("st=%g, r_max=%g\n", st, rp->r_max);
		if( st < 0.0 )
			goto miss;
		if(rp->r_max > st)
			rp->r_max = st;
		sv = (*min - *pt) * *invdir;
rt_log("sv=%g, r_min=%g\n", sv, rp->r_min);
		if( rp->r_min < sv )
			rp->r_min = sv;
rt_log("r_min=%g, r_max=%g\n", rp->r_min, rp->r_max);
	}  else  {
		if( (*min > *pt) || (*max < *pt) )
			goto miss;
	}

	/* If equal, RPP is actually a plane */
	if( rp->r_min > rp->r_max )
		goto miss;
	rt_log("HIT:  %g..%g\n", rp->r_min, rp->r_max );
	return(1);		/* HIT */
miss:
	rt_log("MISS\n");
	return(0);		/* MISS */
}

#define SEG_MISS(SEG)		(SEG).seg_stp=(struct soltab *) 0;	

/* Stub function which will "similate" a call to a vector shot routine */
/*void*/
rt_vstub( stp, rp, segp, n, ap)
struct soltab	       *stp[]; /* An array of solid pointers */
struct xray		*rp[]; /* An array of ray pointers */
struct  seg            segp[]; /* array of segs (results returned) */
int		  	    n; /* Number of ray/object pairs */
struct application        *ap; /* pointer to an application */
{
	register int    i;
	register struct seg *tmp_seg;

	/* go through each ray/solid pair and call a scalar function */
	for (i = 0; i < n; i++) {
		if (stp[i] != 0){ /* skip call if solid table pointer is NULL */
			/* do scalar call */
			tmp_seg =
			    rt_functab[stp[i]->st_id].ft_shot(stp[i], rp[i], ap);

			/* place results in segp array */
			if ( tmp_seg == 0) {
				SEG_MISS(segp[i]);
			}
			else {
				segp[i] = *tmp_seg; /* structure copy */
				FREE_SEG(tmp_seg, ap->a_resource);
			}
		}
	}
}
