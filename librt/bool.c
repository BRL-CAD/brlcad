/*
 *			B O O L . C
 *
 * Ray Tracing program, Boolean region evaluator.
 *
 * Inputs -
 *	Pointer to first segment in seg chain.
 *	Pointer to head of circular doubly-linked list of
 *	partitions of the original ray.
 *
 * Outputs -
 *	Final partitions, queued on doubly-linked list specified.
 *
 * Notes -
 *	It is the responsibility of the CALLER to free the seg chain,
 *	as well as the partition list that we return.
 *
 * Author -
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

#include <stdio.h>
#include "vmath.h"
#include "raytrace.h"
#include "debug.h"

/* Boolean values.  Not easy to change, but defined symbolicly */
#define FALSE	0
#define TRUE	1

int one_hit_flag;	/* non-zero if computation is for viewing only */

struct partition *FreePart = PT_NULL;		 /* Head of freelist */

/*
 *			B O O L _ R E G I O N S
 *
 *  Weave the segments into the partitions.
 *  The edge of each partition is an inhit or outhit of some solid (seg).
 *
 *  NOTE:  When the final partitions are completed, it is the users
 *  responsibility to honor the inflip and outflip flags.  They can
 *  not be flipped here because an outflip=1 edge and an inflip=0 edge
 *  following it may in fact be the same edge.  This could be dealt with
 *  by giving the partition struct a COPY of the inhit and outhit rather
 *  than a pointer, but that's more cycles than it's worth.
 */
void
bool_regions( segp_in, FinalHdp )
struct seg *segp_in;
struct partition *FinalHdp;	/* Heads final circ list */
{
	register struct seg *segp;
	register struct partition *pp;
	LOCAL struct region ActRegHd;	/* Heads active circular forw list */
	LOCAL int curbin;
	LOCAL struct partition PartHd;	/* Heads active circular list */

	/* We assume that the region active chains are REGION_NULL */
	/* We assume that the solid bins st_bin are zero */

	ActRegHd.reg_active = &ActRegHd;	/* Circular forw list */
	curbin = 1;				/* bin 0 is always FALSE */
	PartHd.pt_forw = PartHd.pt_back = &PartHd;

	if(debug&DEBUG_PARTITION) fprintf(stderr,"-------------------BOOL_REGIONS\n");
	for( segp = segp_in; segp != SEG_NULL; segp = segp->seg_next )  {
		register struct partition *newpp;		/* XXX */
		register struct seg *lastseg;
		register struct hit *lasthit;
		LOCAL lastflip;

		/* Make sure seg's solid's region is on active list */
		if( segp->seg_stp->st_bin == 0 )  {
			register struct region *regp;		/* XXX */

			if( (segp->seg_stp->st_bin = curbin++) >= NBINS )
				rtbomb("bool_regions:  need > NBINS bins");
			regp = segp->seg_stp->st_regionp;
			if( (regp != REGION_NULL) &&
			   (regp->reg_active == REGION_NULL) )  {
				regp->reg_active = ActRegHd.reg_active;
				ActRegHd.reg_active = regp;
			}
		}
		if(debug&DEBUG_PARTITION) pr_seg(segp);

		/*
		 * Weave this segment into the existing partitions,
		 * creating new partitions as necessary.
		 */
		if( PartHd.pt_forw == &PartHd )  {
			/* No partitions yet, simple! */
			GET_PT_INIT( pp );
			pp->pt_solhit[segp->seg_stp->st_bin] = TRUE;
			pp->pt_inseg = segp;
			pp->pt_inhit = &segp->seg_in;
			pp->pt_outseg = segp;
			pp->pt_outhit = &segp->seg_out;
			APPEND_PT( pp, &PartHd );
			goto done_weave;
		}
		if( segp->seg_in.hit_dist >= PartHd.pt_back->pt_outdist )  {
			/*
			 * Segment starts exactly at last partition's end,
			 * or beyond last partitions end.  Make new partition.
			 */
			GET_PT_INIT( pp );
			pp->pt_solhit[segp->seg_stp->st_bin] = TRUE;
			pp->pt_inseg = segp;
			pp->pt_inhit = &segp->seg_in;
			pp->pt_outseg = segp;
			pp->pt_outhit = &segp->seg_out;
			APPEND_PT( pp, PartHd.pt_back );
			goto done_weave;
		}

		lastseg = segp;
		lasthit = &segp->seg_in;
		lastflip = 0;
		for( pp=PartHd.pt_forw; pp != &PartHd; pp=pp->pt_forw ) {
			register int i;		/* XXX */

			if( lasthit->hit_dist >= pp->pt_outdist )  {
				/* Seg starts beyond the END of the
				 * current partition.
				 *	PPPP
				 *	        SSSS
				 *
				 * Or, Seg starts almost "precisely" at the
				 * end of the current partition.
				 *	PPPP
				 *	    SSSS
				 * Advance to next partition.
				 */
				 continue;
			}

			/* Segment starts before current partition ends */
			if( (i=fdiff(lasthit->hit_dist, pp->pt_indist)) == 0){
equal_start:
				/*
				 * Segment and partition start at
				 * (roughly) the same point.
				 * When fuseing 2 points together
				 * (ie, when fdiff()==0), the two
				 * points MUST be forced to become
				 * exactly equal!
				 */
				if( (i = fdiff(segp->seg_out.hit_dist, pp->pt_outdist)) == 0 )  {
					/*
					 * Segment and partition start & end
					 * (nearly) together.
					 *	PPPP
					 *	SSSS
					 */
					pp->pt_solhit[segp->seg_stp->st_bin] = TRUE;
					goto done_weave;
				}
				if( i > 0 )  {
					/*
					 * Seg & partition start at roughly
					 * the same spot,
					 * seg extends beyond partition end.
					 *	PPPP
					 *	SSSSSSSS
					 *	pp  |  newpp
					 */
					pp->pt_solhit[segp->seg_stp->st_bin] = TRUE;
					lasthit = pp->pt_outhit;
					lastseg = pp->pt_outseg;
					lastflip = 1;
					continue;
				}
				/*
				 *  Segment + Partition start together,
				 *  segment ends before partition ends.
				 *	PPPPPPPPPP
				 *	SSSSSS
				 *	newpp| pp
				 */
				GET_PT( newpp );
				*newpp = *pp;		/* struct copy */
				/* new partition contains segment */
				newpp->pt_solhit[segp->seg_stp->st_bin] = TRUE;
				newpp->pt_outseg = segp;
				newpp->pt_outhit = &segp->seg_out;
				newpp->pt_outflip = 0;
				pp->pt_inseg = segp;
				pp->pt_inhit = &segp->seg_out;
				pp->pt_inflip = 1;
				INSERT_PT( newpp, pp );
				goto done_weave;
			}
			if( i < 0 )  {
				/*
				 * Seg starts before current partition starts,
				 * but after the previous partition ends.
				 *	SSSSSSSS...
				 *	     PPPPP...
				 *	newpp|pp
				 */
				GET_PT_INIT( newpp );
				newpp->pt_solhit[segp->seg_stp->st_bin] = TRUE;
				newpp->pt_inseg = lastseg;
				newpp->pt_inhit = lasthit;
				newpp->pt_inflip = lastflip;
				if( segp->seg_out.hit_dist <= pp->pt_indist ){
					/*
					 * Seg starts and ends before current
					 * partition, but after previous
					 * partition ends (diff < 0).
					 *		SSSS
					 *	pppp		PPPPP...
					 *		newpp	pp
					 *
					 * Seg starts before current
					 * partition starts, and ends
					 * at the start of the partition.
					 * (diff == 0).
					 *	SSSSSS
					 *	     PPPPP
					 *	newpp|pp
					 */
					newpp->pt_outseg = segp;
					newpp->pt_outhit = &segp->seg_out;
					newpp->pt_outflip = 0;
					INSERT_PT( newpp, pp );
					goto done_weave;
				}
				/*
				 *  Seg starts before current partition
				 *  starts, and ends after the start of the
				 *  partition.  (diff > 0).
				 *	SSSSSSSSSS
				 *	      PPPPPPP
				 *	newpp| pp | ...
				 */
				newpp->pt_outseg = pp->pt_inseg;
				newpp->pt_outhit = pp->pt_inhit;
				newpp->pt_outflip = 1;
				lastseg = pp->pt_inseg;
				lasthit = pp->pt_inhit;
				lastflip = newpp->pt_outflip;
				INSERT_PT( newpp, pp );
				goto equal_start;
			}
			/*
			 * i > 0
			 *
			 * lasthit->hit_dist > pp->pt_indist
			 *
			 *  Segment starts after partition starts,
			 *  but before the end of the partition.
			 *  Note:  pt_solhit will be marked in equal_start.
			 *	PPPPPPPPPPPP
			 *	     SSSS...
			 *	newpp|pp
			 */
			GET_PT( newpp );
			*newpp = *pp;			/* struct copy */
			/* new part. is the span before seg joins partition */
			pp->pt_inseg = segp;
			pp->pt_inhit = &segp->seg_in;
			pp->pt_inflip = 0;
			newpp->pt_outseg = segp;
			newpp->pt_outhit = &segp->seg_in;
			newpp->pt_outflip = 1;
			INSERT_PT( newpp, pp );
			goto equal_start;
		}

		/*
		 *  Segment has portion which extends beyond the end
		 *  of the last partition.  Tack on the remainder.
		 *  	PPPPP
		 *  	     SSSSS
		 */
		GET_PT_INIT( newpp );
		newpp->pt_solhit[segp->seg_stp->st_bin] = TRUE;
		newpp->pt_inseg = lastseg;
		newpp->pt_inhit = lasthit;
		newpp->pt_inflip = lastflip;
		newpp->pt_outseg = segp;
		newpp->pt_outhit = &segp->seg_out;
		APPEND_PT( newpp, PartHd.pt_back );

done_weave:	; /* Sorry about the goto's, but they give clarity */
		if(debug&DEBUG_PARTITION)
			pr_partitions( &PartHd, "After weave" );
	}
	if(debug&DEBUG_PARTITION)  {
		pr_act_regions( &ActRegHd );
		pr_bins();
	}

	/*
	 * For each partition, evaluate the boolean expression for
	 * all the regions.  If 0 regions result, continue with next
	 * partition.  If 1 region results, a valid hit has occured,
	 * so output a new segment.  If 2 or more regions claim the
	 * partition, then an overlap exists.
	 */
	FinalHdp->pt_forw = FinalHdp->pt_back = FinalHdp;

	pp = PartHd.pt_forw;
	while( pp != &PartHd )  {
		register struct region *lastregion;
		register struct region *regp;
		LOCAL int hitcnt;

		hitcnt = 0;
		if(debug&DEBUG_PARTITION)
			fprintf(stderr,"considering partition %.8x\n", pp );

		/* Sanity check of sorting.  Remove later. */
		if( pp->pt_forw != &PartHd && pp->pt_outdist > pp->pt_forw->pt_indist )  {
			auto struct partition FakeHead;
			auto odebug;
			fprintf(stderr,"bool_regions:  sorting defect!\n");
			if( debug & DEBUG_PARTITION ) return; /* give up */
			for( segp = segp_in; segp != SEG_NULL; segp = segp->seg_next )  {
				pr_seg(segp);
			}
			pr_partitions( &PartHd, "With DEFECT" );
			odebug = debug;
			debug |= DEBUG_PARTITION;
			bool_regions( segp_in, &FakeHead );
			debug = odebug;
			/* Wastes some dynamic memory */
		}

		regp = ActRegHd.reg_active;
		for( ; regp != &ActRegHd; regp = regp->reg_active )  {
			if(debug&DEBUG_PARTITION)  {
				fprintf(stderr,"%.8x: %s\n", regp, regp->reg_name );
				pr_tree( regp->reg_treetop, 0 );
			}
			if( bool_eval( regp->reg_treetop, pp ) == FALSE )
				continue;
			/* region claims partition */
			if( ++hitcnt > 1 ) {
				fprintf(stderr,"OVERLAP: %s %s (%f,%f)\n",
					regp->reg_name,
					lastregion->reg_name,
					pp->pt_indist,
					pp->pt_outdist );
			} else {
				lastregion = regp;
			}
		}
		if( hitcnt == 0 )  {
			pp=pp->pt_forw;			/* onwards! */
			continue;
		}
		if( pp->pt_outdist <= EPSILON )  {
			/* partition is behind start point (k=0), ignore */
			pp=pp->pt_forw;
			continue;
		}

		/* Add this partition to the result queue */
		{
			register struct partition *newpp;	/* XXX */

			newpp = pp;
			pp=pp->pt_forw;
			DEQUEUE_PT( newpp );
			newpp->pt_regionp = lastregion;
			APPEND_PT( newpp, FinalHdp->pt_back );

			/* Shameless efficiency hack:
			 * If the application is for viewing only,
			 * the first hit beyond the start point is
			 * all we care about.
			 */
			if( one_hit_flag && newpp->pt_indist > 0.0 )
				break;
		}
	}
	if( debug&DEBUG_PARTITION )
		pr_partitions( FinalHdp, "Final Partitions" );

	/*
	 * Cleanup:  Put zeros in the bin#s of all the solids that were used,
	 * and zap the reg_active chain.
	 */
	for( segp = segp_in; segp != SEG_NULL; segp = segp->seg_next )  {
		segp->seg_stp->st_bin = 0;
	}
	{
		register struct region *regp;			/* XXX */
		for( regp=ActRegHd.reg_active; regp != &ActRegHd; )  {
			register struct region *newreg;		/* XXX */
			newreg = regp;
			regp = regp->reg_active;
			newreg->reg_active = REGION_NULL;
		}
	}
	for( pp = PartHd.pt_forw; pp != &PartHd;  )  {
		register struct partition *newpp;
		newpp = pp;
		pp = pp->pt_forw;
		FREE_PT(newpp);
	}
	/* Caller must Free seg chain and partition chain */
}

/*
 *  			B O O L _ E V A L
 *  
 *  Function -
 *  	Given a tree node, evaluate it, possibly recursing.
 */
int
bool_eval( treep, partp )
register union tree *treep;
register struct partition *partp;
{
	register int ret;

	switch( treep->tr_op )  {

	case OP_SOLID:
		ret = ( partp->pt_solhit[treep->tr_stp->st_bin] );
		break;

	case OP_UNION:
		ret =	bool_eval( treep->tr_left, partp )  ||
			bool_eval( treep->tr_right, partp ) ;
		break;

	case OP_INTERSECT:
		ret =	bool_eval( treep->tr_left, partp )  &&
			bool_eval( treep->tr_right, partp ) ;
		break;

	case OP_SUBTRACT:
		if( bool_eval( treep->tr_left, partp ) == FALSE )
			ret = FALSE;		/* FALSE = FALSE - X */
		else  {
			/* TRUE = TRUE - FALSE */
			/* FALSE = TRUE - TRUE */
			ret = !bool_eval( treep->tr_right, partp );
		}
		break;

	default:
		fprintf(stderr,"bool_eval: bad op=x%x", treep->tr_op );
		ret = TRUE;		/* screw up output, get it fixed! */
		break;
	}
	return( ret );
}

/* Called with address of head of chain */
void
pr_partitions( phead, title )
register struct partition *phead;
char *title;
{
	register struct partition *pp;
	register int i;

	fprintf(stderr,"----Partitions: %s\n", title);
	fprintf(stderr,"%.8x: forw=%.8x back=%.8x (HEAD)\n",
		phead, phead->pt_forw, phead->pt_back );
	for( pp = phead->pt_forw; pp != phead; pp = pp->pt_forw ) {
		fprintf(stderr,"%.8x: forw=%.8x back=%.8x (%f,%f)",
			pp, pp->pt_forw, pp->pt_back,
			pp->pt_indist, pp->pt_outdist );
		fprintf(stderr,"%s%s\n",
			pp->pt_inflip ? " Iflip" : "",
			pp->pt_outflip ?" Oflip" : "" );
#ifdef never
		fprintf(stderr,"\t Nin=[%f,%f,%f] Nout=[%f,%f,%f]\n",
			pp->pt_inhit->hit_normal[0],
			pp->pt_inhit->hit_normal[1],
			pp->pt_inhit->hit_normal[2],
			pp->pt_outhit->hit_normal[0],
			pp->pt_outhit->hit_normal[1],
			pp->pt_outhit->hit_normal[2] );
#endif
		fprintf(stderr,"Bins: ");
		for( i=0; i<NBINS; i++ )
			if( pp->pt_solhit[i] )
				fprintf(stderr,"%d, ", i );
		putc('\n',stderr);
	}
	fprintf(stderr,"----\n");
}

static
pr_act_regions(headp)
register struct region *headp;
{
	register struct region *regp;

	fprintf(stderr,"Active Regions:\n");
	for( regp=headp->reg_active; regp != headp; regp=regp->reg_active )  {
		fprintf(stderr,"%.8x %s\n", regp, regp->reg_name );
	}
}

#define	Abs( a )	((a) >= 0 ? (a) : -(a))
#define	Max( a, b )	((a) >= (b) ? (a) : (b))

/*
 *  			F D I F F
 *  
 *  Compares two floating point numbers.  If they are within "epsilon"
 *  of each other, they are considered the same.
 *  NOTE:  This is a "fuzzy" difference.  It is important NOT to
 *  use the results of this function in compound comparisons,
 *  because a return of 0 makes no statement about the PRECISE
 *  relationships between the two numbers.  Eg,
 *	if( fdiff( a, b ) <= 0 )
 *  is poison!
 *
 *  Returns -
 *  	-1	if a < b
 *  	 0	if a ~= b
 *  	+1	if a > b
 */
int
fdiff( a, b )
double a, b;
{
	FAST double diff;
	FAST double d;

	diff = a - b;
	d = Max( Abs( a ), Abs( b ) );	/* NOTE: not efficient */
	if( d <= EPSILON )
		return(0);	/* both nearly zero */
	if( Abs(diff) < EPSILON * d )
		return( 0 );	/* relative difference is small */
	if( a < b )
		return(-1);
	return(1);
}

/*
 *			R E L D I F F
 *
 * Compute the relative difference of two floating point numbers.
 *
 * Returns -
 *	0.0 if exactly the same, otherwise
 *	ratio of difference, relative to the larger of the two (range 0.0-1.0)
 */

double
reldiff( a, b )
double	a, b;
{
	LOCAL double	d;

	d = Max( Abs( a ), Abs( b ) );	/* NOTE: not efficient */
	return( d == 0.0 ? 0.0 : Abs( a - b ) / d );
}

void
pr_bins()
{
	register struct soltab *stp;
	extern struct soltab *HeadSolid;

	fprintf(stderr,"Bins:\n");
	for( stp=HeadSolid; stp != 0; stp=stp->st_forw ) {
		if( stp->st_bin == 0 )
			continue;
		fprintf(stderr,"%d: %s\n", stp->st_bin, stp->st_name );
	}
}

/*
 *			P R _ S E G
 */
void
pr_seg(segp)
register struct seg *segp;
{
	fprintf(stderr,"%.8x: SEG %s (%f,%f) bin=%d\n",
		segp,
		segp->seg_stp->st_name,
		segp->seg_in.hit_dist,
		segp->seg_out.hit_dist,
		segp->seg_stp->st_bin );
}
