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
#include "ray.h"
#include "debug.h"

extern long nsolids;		/* total # of solids participating */
extern long nregions;		/* total # of regions participating */
extern long nshots;		/* # of ray-meets-solid "shots" */
extern long nmiss;		/* # of ray-misses-solid's-sphere "shots" */

extern struct soltab *HeadSolid;
extern struct functab functab[];
extern struct seg *HeadSeg;

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
shootray( rayp )
register struct ray *rayp;
{
	register struct soltab *stp;
	static vect_t diff;	/* diff between shot base & solid center */
	FAST fastf_t distsq;	/* distance**2 */

	if(debug&DEBUG_ALLRAYS) {
		VPRINT("\nRay Start", rayp->r_pt);
		VPRINT("Ray Direction", rayp->r_dir);
	}

	HeadSeg = SEG_NULL;	/* Should check, actually */

	/*
	 * For now, shoot at all solids in model.
	 */
	for( stp=HeadSolid; stp != SOLTAB_NULL; stp=stp->st_forw ) {
		register struct seg *newseg;		/* XXX */

		/* Consider bounding sphere */
		VSUB2( diff, stp->st_center, rayp->r_pt );
		distsq = VDOT(rayp->r_dir, diff);
		if( (MAGSQ(diff) - distsq*distsq) > stp->st_radsq ) {
			nmiss++;
			continue;
		}
		nshots++;
		newseg = functab[stp->st_id].ft_shot( stp, rayp );
		if( newseg == SEG_NULL )
			continue;

		/* First, some checking */
		if( newseg->seg_in.hit_dist > newseg->seg_out.hit_dist )  {
			static struct hit temp;	/* XXX */
			printf("ERROR %s %s: in/out reversal (%f,%f)\n",
				functab[stp->st_id].ft_name,
				newseg->seg_stp->st_name,
				newseg->seg_in.hit_dist,
				newseg->seg_out.hit_dist );
			temp = newseg->seg_in;		/* struct copy */
			newseg->seg_in = newseg->seg_out; /* struct copy */
			newseg->seg_out = temp;		/* struct copy */
		}
		/* Add to list */
		{
			register struct seg *seg2 = newseg;
			while( seg2->seg_next != SEG_NULL )
				seg2 = seg2->seg_next;
			seg2->seg_next = HeadSeg;
			HeadSeg = newseg;
		}
	}
}
