/*
 *			S H O O T . C
 *
 * Ray Tracing program shot coordinator.
 *
 * Author -
 *	Michael John Muuss
 *
 *	U. S. Army Ballistic Research Laboratory
 *	May 1, 1984
 *
 * $Revision$
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "vmath.h"
#include "raytrace.h"
#include "debug.h"

int debug = DEBUG_OFF;	/* non-zero for debugging, see debug.h */
long nsolids;		/* total # of solids participating */
long nregions;		/* total # of regions participating */
long nshots;		/* # of ray-meets-solid "shots" */
long nmiss;		/* # of ray-misses-solid's-sphere "shots" */
struct soltab *HeadSolid = SOLTAB_NULL;
struct seg *FreeSeg = SEG_NULL;		/* Head of freelist */

/*
 *  			S H O O T R A Y
 *  
 *  Given a ray, shoot it at all the relevant parts of the model,
 *  building the HeadSeg chain, and calling the appropriate application
 *  routine (a_hit, a_miss).
 *
 *  This is where higher-level pruning should be done.
 *  This code is executed more often than any other part of the RT library.
 *
 *  WARNING:  The appliction functions may call shootray() recursively.
 */
void
shootray( ap )
register struct application *ap;
{
	register struct soltab *stp;
	LOCAL vect_t diff;	/* diff between shot base & solid center */
	FAST fastf_t distsq;	/* distance**2 */
	auto struct seg *HeadSeg;

	if(debug&DEBUG_ALLRAYS) {
		VPRINT("\nRay Start", ap->a_ray.r_pt);
		VPRINT("Ray Direction", ap->a_ray.r_dir);
		fflush(stderr);		/* In case of instant death */
	}

	HeadSeg = SEG_NULL;

	/*
	 * For now, shoot at all solids in model.
	 */
	for( stp=HeadSolid; stp != SOLTAB_NULL; stp=stp->st_forw ) {
		register struct seg *newseg;		/* XXX */

		/* Consider bounding sphere */
		VSUB2( diff, stp->st_center, ap->a_ray.r_pt );
		distsq = VDOT(ap->a_ray.r_dir, diff);
		if( (MAGSQ(diff) - distsq*distsq) > stp->st_radsq ) {
			nmiss++;
			continue;
		}

#ifdef never
/*** First, we have to computer the bounding RPPs right! ** */
		/* Here we also consider the bounding RPP */
		if( !in_rpp( &ap->a_ray, stp->st_min, stp->st_max ) )  {
			nmiss++;
			continue;
		}
#endif

		nshots++;
		newseg = functab[stp->st_id].ft_shot( stp, &ap->a_ray );
		if( newseg == SEG_NULL )
			continue;

		/* First, some checking */
		if( newseg->seg_in.hit_dist > newseg->seg_out.hit_dist )  {
			LOCAL struct hit temp;		/* XXX */
			fprintf(stderr,"ERROR %s %s: in/out reversal (%f,%f)\n",
				functab[stp->st_id].ft_name,
				newseg->seg_stp->st_name,
				newseg->seg_in.hit_dist,
				newseg->seg_out.hit_dist );
			temp = newseg->seg_in;		/* struct copy */
			newseg->seg_in = newseg->seg_out; /* struct copy */
			newseg->seg_out = temp;		/* struct copy */
		}
		/* Add segment chain to list */
		{
			register struct seg *seg2 = newseg;
			while( seg2->seg_next != SEG_NULL )
				seg2 = seg2->seg_next;
			seg2->seg_next = HeadSeg;
			HeadSeg = newseg;
		}
	}

	/* HeadSeg chain now has all segments hit by this ray */
	if( HeadSeg == SEG_NULL )  {
		ap->a_miss( ap );
	}  else  {
		auto struct partition PartHead;
		/*
		 *  All intersections of the ray with the model have
		 *  been computed.  Evaluate the boolean functions.
		 */
		bool_regions( HeadSeg, &PartHead );

		if( PartHead.pt_forw == &PartHead )  {
			ap->a_miss( ap );
		}  else  {
			register struct partition *pp;
			/*
			 * Hand final partitioned intersection list
			 * to the application.
			 */
			ap->a_hit( ap, &PartHead );

			/* Free up partition list */
			for( pp = PartHead.pt_forw; pp != &PartHead;  )  {
				register struct partition *newpp;
				newpp = pp;
				pp = pp->pt_forw;
				FREE_PT(newpp);
			}
		}

		/*
		 * Processing of this ray is complete, so release resources.
		 */
		while( HeadSeg != SEG_NULL )  {
			register struct seg *hsp;	/* XXX */

			hsp = HeadSeg->seg_next;
			FREE_SEG( HeadSeg );
			HeadSeg = hsp;
		}
	}
	if( debug )  fflush(stderr);
}

/*
 *			I N _ R P P . C
 *
 *  Compute the intersections of a ray with a rectangular parallelpiped (RPP)
 *  that has faces parallel to the coordinate planes
 *
 *  The algorithm here was developed by Gary Kuehl for GIFT.
 *
 * enter_dist = distance from start of ray to point at which ray enters solid
 * exit_dist  = distance from start of ray to point at which ray leaves solid
 *
 *  Returns -
 *	 0  if ray does not hit RPP,
 *	!0  if ray hits RPP.
 */
in_rpp( rp, min, max )
register struct xray *rp;
register vect_t min, max;
{
	LOCAL fastf_t enter_dist, exit_dist;
	LOCAL fastf_t sv;
	LOCAL fastf_t st;
	register int i;

	enter_dist = -INFINITY;
	exit_dist = INFINITY;

	for( i=0; i < 3; i++ )  {
		if( NEAR_ZERO( rp->r_dir[i] ) )  {
			/*
			 *  If direction component along this axis is 0,
			 *  (ie, this ray is aligned with this axis),
			 *  merely check against the boundaries.
			 */
			if( (min[i] > rp->r_pt[i]) || (max[i] < rp->r_pt[i]) )
				return(0);	/* MISS */;
			continue;
		}

		if( rp->r_dir[i] < 0.0 )  {
			sv = (min[i] - rp->r_pt[i]) / rp->r_dir[i];
			if(sv < 0.0)
				return(0);	/* MISS */
			st = (max[i] - rp->r_pt[i]) / rp->r_dir[i];
			if(exit_dist > sv)
				exit_dist = sv;
			if(enter_dist < st)
				enter_dist = st;
		}  else  {
			st = (max[i] - rp->r_pt[i]) / rp->r_dir[i];
			if(st < 0.)
				return(0);	/* MISS */
			sv = (min[i] - rp->r_pt[i]) / rp->r_dir[i];
			if(exit_dist > st)
				exit_dist = st;
			if(enter_dist < sv)
				enter_dist = sv;
		}
	}
#define	TOL .0001
	if( enter_dist+TOL >= exit_dist )
		return(0);	/* MISS */
	return(1);		/* HIT */
}
