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

extern long nsolids;		/* total # of solids participating */
extern long nregions;		/* total # of regions participating */
extern long nshots;		/* # of ray-meets-solid "shots" */
extern long nmiss;		/* # of ray-misses-solid's-sphere "shots" */

extern struct soltab *HeadSolid;
extern struct functab functab[];

extern struct partition *bool_regions();

/*
 *  			S H O O T R A Y
 *  
 *  Given a ray, shoot it at all the relevant parts of the model,
 *  building the HeadSeg chain (which is the implicit return
 *  from this routine).
 *
 *  This is where higher-level pruning should be done.
 *
 *  This code is executed more often than any other part, generally.
 */
shootray( ap )
register struct application *ap;
{
	register struct soltab *stp;
	static vect_t diff;	/* diff between shot base & solid center */
	FAST fastf_t distsq;	/* distance**2 */
	static struct seg *HeadSeg;

	if(debug&DEBUG_ALLRAYS) {
		VPRINT("\nRay Start", ap->a_ray.r_pt);
		VPRINT("Ray Direction", ap->a_ray.r_dir);
		fflush(stdout);		/* In case of instant death */
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

		/* Here we should also consider the bounding RPP */

		nshots++;
		newseg = functab[stp->st_id].ft_shot( stp, &ap->a_ray );
		if( newseg == SEG_NULL )
			continue;

		/* First, some checking */
		if( newseg->seg_in.hit_dist > newseg->seg_out.hit_dist )  {
			static struct hit temp;		/* XXX */
			printf("ERROR %s %s: in/out reversal (%f,%f)\n",
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
		register struct partition *PartHeadp;
		/*
		 *  All intersections of the ray with the model have
		 *  been computed.  Evaluate the boolean functions.
		 */
		PartHeadp = bool_regions( HeadSeg );

		if( PartHeadp->pt_forw == PartHeadp )  {
			ap->a_miss( ap );
		}  else  {
			register struct partition *pp;
			/*
			 * Hand final partitioned intersection list
			 * to the application.
			 */
			ap->a_hit( ap, PartHeadp );

			/* Free up partition list */
			for( pp = PartHeadp->pt_forw; pp != PartHeadp;  )  {
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
	if( debug )  fflush(stdout);
}
