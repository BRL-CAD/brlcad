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
	LOCAL vect_t diff;	/* diff between shot base & solid center */
	FAST fastf_t distsq;	/* distance**2 */
	auto struct seg *HeadSeg; /* must NOT be static (recursion ) */
	LOCAL fastf_t inout[2];	/* entry_dist, exit_dist for bounding RPP */


	/* If ray does not enter the model RPP, skip on */
	if( !in_rpp( &ap->a_ray, model_min, model_max, inout )  ||
	    inout[1] < 0.0 )  {
		nmiss++;
		ap->a_miss( ap );
		return;
	}

	if(debug&DEBUG_ALLRAYS) {
		VPRINT("\nRay Start", ap->a_ray.r_pt);
		VPRINT("Ray Direction", ap->a_ray.r_dir);
		fflush(stderr);		/* In case of instant death */
	}

	HeadSeg = SEG_NULL;

#ifndef Tree_Method
  {
	register struct region *rp;
	extern struct region *HeadRegion;

	for( rp=HeadRegion; rp != REGION_NULL; rp = rp->reg_forw )  {
		
		/* Check region bounding RPP */
		if( !in_rpp(
			&ap->a_ray, rp->reg_treetop->tr_min,
			rp->reg_treetop->tr_max, inout )  ||
		    inout[1] < 0.0 )  {
			nmiss++;
			continue;
		}

		/* Boolean TRUE signals hit of 1 or more solids in tree */
		/* At leaf node, it will calll ft_shot & add to chain */
		if( !shoot_tree( ap, rp->reg_treetop, &HeadSeg ) )
			continue;
		/* If any solid hit, add this region to active chain */
	}
 }
#else
 {
	register struct soltab *stp;

	/*
	 * For now, shoot at all solids in model.
	 */
	for( stp=HeadSolid; stp != SOLTAB_NULL; stp=stp->st_forw ) {
		register struct seg *newseg;		/* XXX */

#	ifndef never
		/* Consider bounding sphere */
		VSUB2( diff, stp->st_center, ap->a_ray.r_pt );
		distsq = VDOT(ap->a_ray.r_dir, diff);
		if( (MAGSQ(diff) - distsq*distsq) > stp->st_radsq ) {
			nmiss++;
			continue;
		}
#	endif

		/* If ray does not strike the bounding RPP, skip on */
		if( !in_rpp( &ap->a_ray, stp->st_min, stp->st_max, inout ) ) {
			nmiss++;
			continue;
		}

		/*
		 * Rays have direction.  If ray enters and exits this
		 * solid entirely behind the start point, skip on.
		 */
		if( inout[1] < 0.0 )  {
			nmiss++;
			continue;	/* enters & exits behind eye */
		}

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
 }
#endif

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
 *	bounds[0] = dist from start of ray to point at which ray ENTERS solid
 *	bounds[1] = dist from start of ray to point at which ray LEAVES solid
 */
in_rpp( rp, min, max, bounds )
struct xray *rp;
register fastf_t *min, *max;
register fastf_t *bounds;
{
	LOCAL fastf_t sv;
	LOCAL fastf_t st;
	register fastf_t *pt = &rp->r_pt[0];
	register fastf_t *dir = &rp->r_dir[0];
	register int i;

	*bounds = -INFINITY;
	bounds[1] = INFINITY;

	for( i=0; i < 3; i++, pt++, dir++, max++, min++ )  {
		if( NEAR_ZERO( *dir ) )  {
			/*
			 *  If direction component along this axis is 0,
			 *  (ie, this ray is aligned with this axis),
			 *  merely check against the boundaries.
			 */
			if( (*min > *pt) || (*max < *pt) )
				return(0);	/* MISS */;
			continue;
		}

		if( *dir < 0.0 )  {
			if( (sv = (*min - *pt) / *dir) < 0.0 )
				return(0);	/* MISS */
			if(bounds[1] > sv)
				bounds[1] = sv;
			if( *bounds < (st = (*max - *pt) / *dir) )
				*bounds = st;
		}  else  {
			if( (st = (*max - *pt) / *dir) < 0.0 )
				return(0);	/* MISS */
			if(bounds[1] > st)
				bounds[1] = st;
			if( *bounds < ((sv = (*min - *pt) / *dir)) )
				*bounds = sv;
		}
	}
	if( *bounds >= bounds[1] )
		return(0);	/* MISS */
	return(1);		/* HIT */
}

/* Boolean values.  Not easy to change, but defined symbolicly */
#define FALSE	0
#define TRUE	1

/*
 *  			S H O O T _ T R E E
 *
 *  Returns FALSE when there is definitely NO HIT.
 *  Returns TRUE when there is the Potential for a hit;
 *  bool_regions() must ultimately decide.
 */
shoot_tree( ap, tp, HeadSegp )
register struct application *ap;
register struct tree *tp;
struct seg **HeadSegp;
{
	LOCAL vect_t inout;

	/* If ray does not strike the bounding RPP, skip on */
	if( !in_rpp( &ap->a_ray, tp->tr_min, tp->tr_max, inout )  ||
	    inout[1] < 0.0 )
		return( FALSE );	/* MISS subtree */

	switch( tp->tr_op )  {

	case OP_SOLID:
	    {
		register struct seg *newseg;

		nshots++;
		newseg = functab[tp->tr_stp->st_id].ft_shot( tp->tr_stp, &ap->a_ray );
		if( newseg == SEG_NULL )
			return( FALSE );/* MISS subtree (solid) */

		/* Add segment chain to list */
		{
			register struct seg *seg2 = newseg;
			while( seg2->seg_next != SEG_NULL )
				seg2 = seg2->seg_next;
			seg2->seg_next = (*HeadSegp);
			(*HeadSegp) = newseg;
		}
		return( TRUE );		/* HIT, solid added */
	    }

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
		shoot_tree( ap, tp->tr_left, HeadSegp );
		shoot_tree( ap, tp->tr_right, HeadSegp );
		return(TRUE);
#ifdef never
	case OP_UNION:
		/* NOTE:  It is important to always evaluate both */
		if(	shoot_tree( ap, tp->tr_left, HeadSegp ) == FALSE &&
			shoot_tree( ap, tp->tr_right, HeadSegp ) == FALSE )
			return(FALSE);
		return(TRUE);			/* May have a hit */

	case OP_INTERSECT:
		return(	shoot_tree( ap, tp->tr_left, HeadSegp )  &&
			shoot_tree( ap, tp->tr_right, HeadSegp )  );

	case OP_SUBTRACT:
		if( shoot_tree( ap, tp->tr_left, HeadSegp ) == FALSE )
			return(FALSE);		/* FALSE = FALSE - X */
		/* If ray hit left solid, we MUST compute potential
		 *  hit with right solid, as only bool_regions()
		 *  can really tell the final story.  Always give TRUE.
		 */
		shoot_tree( ap, tp->tr_right, HeadSegp );
		return( TRUE );		/* May have a hit */
#endif

	default:
		fprintf(stderr,"shoot_tree: bad op=x%x", tp->tr_op );
		return( FALSE );
	}
	/* NOTREACHED */
}
