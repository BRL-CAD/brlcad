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
#include "vmath.h"
#include "raytrace.h"
#include "debug.h"

int debug = DEBUG_OFF;	/* non-zero for debugging, see debug.h */
long nsolids;		/* total # of solids participating */
long nregions;		/* total # of regions participating */
long nshots;		/* # of calls to ft_shot() */
long nmiss_model;	/* rays missed model RPP */
long nmiss_tree;	/* rays missed sub-tree RPP */
long nmiss_solid;	/* rays missed solid RPP */
long nmiss;		/* solid ft_shot() returned a miss */
long nhits;		/* solid ft_shot() returned a hit */
struct soltab *HeadSolid = SOLTAB_NULL;
struct seg *FreeSeg = SEG_NULL;		/* Head of freelist */

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
 */
int
shootray( ap )
register struct application *ap;
{
	extern struct region *HeadRegion;
	register struct region *rp;
	auto struct seg *HeadSeg;	/* must NOT be static (recursion) */
	register int ret;

	/*
	 *  If ray does not enter the model RPP, skip on.
	 *  For models with non-cubical shape, this test
	 *  actually trims off a substantial number of rays,
	 *  and is well worth it.  When the model fills the view,
	 *  then this is just wasted time;  perhaps a flag is needed?
	 */
	if( !in_rpp( &ap->a_ray, model_min, model_max )  ||
	    ap->a_ray.r_max <= 0.0 )  {
		nmiss_model++;
		return( ap->a_miss( ap ) );
	}

	if(debug&DEBUG_ALLRAYS) {
		VPRINT("\nRay Start", ap->a_ray.r_pt);
		VPRINT("Ray Direction", ap->a_ray.r_dir);
		fflush(stderr);		/* In case of instant death */
	}

	HeadSeg = SEG_NULL;

	/* For now, consider every region in the model */
	for( rp=HeadRegion; rp != REGION_NULL; rp = rp->reg_forw )  {
		/*
		 * Boolean TRUE signals hit of 1 or more solids in tree
		 * At leaf node, it will call ft_shot & add to seg chain.
		 */
		(void)shoot_tree( ap, rp->reg_treetop, &HeadSeg );
	}

	/* HeadSeg chain now has all segments hit by this ray */
	if( HeadSeg == SEG_NULL )  {
		ret = ap->a_miss( ap );
	}  else  {
		auto struct partition PartHead;
		/*
		 *  All intersections of the ray with the model have
		 *  been computed.  Evaluate the boolean functions.
		 */
		bool_regions( HeadSeg, &PartHead );

		if( PartHead.pt_forw == &PartHead )  {
			ret = ap->a_miss( ap );
		}  else  {
			register struct partition *pp;
			/*
			 * Hand final partitioned intersection list
			 * to the application.
			 */
			ret = ap->a_hit( ap, &PartHead );

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
in_rpp( rp, min, max )
register struct xray *rp;
register fastf_t *min, *max;
{
	LOCAL fastf_t sv;
	LOCAL fastf_t st;
	register fastf_t *pt = &rp->r_pt[0];
	register fastf_t *dir = &rp->r_dir[0];
	register int i;

	rp->r_min = -INFINITY;
	rp->r_max = INFINITY;

	for( i=0; i < 3; i++, pt++, dir++, max++, min++ )  {
		if( *dir < -EPSILON )  {
			if( (sv = (*min - *pt) / *dir) < 0.0 )
				return(0);	/* MISS */
			if(rp->r_max > sv)
				rp->r_max = sv;
			if( rp->r_min < (st = (*max - *pt) / *dir) )
				rp->r_min = st;
		}  else if( *dir > EPSILON )  {
			if( (st = (*max - *pt) / *dir) < 0.0 )
				return(0);	/* MISS */
			if(rp->r_max > st)
				rp->r_max = st;
			if( rp->r_min < ((sv = (*min - *pt) / *dir)) )
				rp->r_min = sv;
		}  else  {
			/*
			 *  If direction component along this axis is NEAR 0,
			 *  (ie, this ray is aligned with this axis),
			 *  merely check against the boundaries.
			 */
			if( (*min > *pt) || (*max < *pt) )
				return(0);	/* MISS */;
		}
	}
	if( rp->r_min >= rp->r_max )
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
	if( tp->tr_op == OP_SOLID )  {
		register struct seg *newseg;

		/* If ray does not strike the bounding RPP, skip on */
		if( !in_rpp( &ap->a_ray, tp->tr_min, tp->tr_max )  ||
		    ap->a_ray.r_max <= 0.0 )  {
			nmiss_solid++;
			return( FALSE );	/* MISS subtree */
		}

#	ifdef never
		/* Consider bounding sphere */
		VSUB2( diff, stp->st_center, ap->a_ray.r_pt );
		distsq = VDOT(ap->a_ray.r_dir, diff);
		if( (MAGSQ(diff) - distsq*distsq) > stp->st_radsq ) {
			nmiss_solid++;
			continue;
		}
#	endif

		nshots++;
		if( (newseg = functab[tp->tr_stp->st_id].ft_shot(
			tp->tr_stp,
			&ap->a_ray )
		     ) == SEG_NULL )  {
			nmiss++;
			return( FALSE );	/* MISS */
		}

		/* First, some checking */
		if( newseg->seg_in.hit_dist > newseg->seg_out.hit_dist )  {
			LOCAL struct hit temp;		/* XXX */
			fprintf(stderr,"ERROR %s: in/out reversal (%f,%f)\n",
				newseg->seg_stp->st_name,
				newseg->seg_in.hit_dist,
				newseg->seg_out.hit_dist );
			temp = newseg->seg_in;		/* struct copy */
			newseg->seg_in = newseg->seg_out; /* struct copy */
			newseg->seg_out = temp;		/* struct copy */
		}

		/* Discard seg entirely behind the start point of the ray */
		if( newseg->seg_out.hit_dist < 0 )  {
			FREE_SEG( newseg );
			return( FALSE );	/* MISS */
		}

		/* Add segment chain to list */
		{
			register struct seg *seg2 = newseg;
			while( seg2->seg_next != SEG_NULL )
				seg2 = seg2->seg_next;
			seg2->seg_next = (*HeadSegp);
			(*HeadSegp) = newseg;
		}
		nhits++;
		return( TRUE );		/* HIT, solid added */
	}

	/* If ray does not strike the bounding RPP, skip on */
	if( !in_rpp( &ap->a_ray, tp->tr_min, tp->tr_max )  ||
	    ap->a_ray.r_max <= 0.0 )  {
		nmiss_tree++;
		return( FALSE );	/* MISS subtree */
	}

    {
	register int flag;

	switch( tp->tr_op )  {

	case OP_UNION:
		/* NOTE:  It is important to always evaluate both */
		flag = shoot_tree( ap, tp->tr_left, HeadSegp );
		if( shoot_tree( ap, tp->tr_right, HeadSegp ) == FALSE  &&
		    flag == FALSE )
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

	default:
		fprintf(stderr,"shoot_tree: bad op=x%x", tp->tr_op );
		return( FALSE );
	  }
    }
	/* NOTREACHED */
}
