/*
 *			B O O L
 *
 * Ray Tracing program, Boolean region evaluator.
 *
 * Input -
 *	Pointer to first in seg chain, to be processed.
 *	All seg structs should be freed before return.
 *
 * Output -
 *	Pointer to head of circular doubly-linked list of
 *	partitions of the original ray.
 *
 * Notes -
 *	It is the responsibility of the CALLER to free the seg chain,
 *	as well as the partition list that we return.
 *
 * Author -
 *	Michael John Muuss
 *
 *	U. S. Army Ballistic Research Laboratory
 *	April 13, 1984
 *
 * TODO:
 *	Flip normals after subtraction!!
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

/* Boolean values.  Not easy to change, but defined symbolicly */
#define FALSE	0
#define TRUE	1

struct partition *FreePart = PART_NULL;		 /* Head of freelist */

/*
 *			B O O L _ R E G I O N S
 */
struct partition *
bool_regions( segp_in )
struct seg *segp_in;
{
	register struct seg *segp = segp_in;
	register struct soltab *stp;
	register struct partition *pp;
	static struct region ActRegHd;	/* Heads active circular forw list */
	static int curbin;
	static struct region *regp;
	static struct partition PartHd;	/* Heads active circular list */
	static struct partition FinalHd;	/* Heads final circ list */
	static int i;

	/* We assume that the region active chains are REGION_NULL */
	/* We assume that the solid bins st_bin are zero */

	ActRegHd.reg_active = &ActRegHd;	/* Circular forw list */
	curbin = 1;				/* bin 0 is always FALSE */
	PartHd.pt_forw = PartHd.pt_back = &PartHd;

	if(debug&DEBUG_PARTITION) printf("-------------------BOOL_REGIONS\n");
	for( segp = segp_in; segp != SEG_NULL; segp = segp->seg_next )  {
		register struct partition *newpp;		/* XXX */
		register float dist;				/* XXX */

		if(debug&DEBUG_PARTITION) pr_seg(segp);
		/* Make sure seg's solid's region is on active list */
		stp = segp->seg_stp;
		if( stp->st_bin == 0 )  {
			if( (stp->st_bin = curbin++) >= NBINS )
				bomb("bool_regions:  need > NBINS bins");
			regp = stp->st_regionp;
			if( (regp != REGION_NULL) &&
			   (regp->reg_active == REGION_NULL) )  {
				regp->reg_active = ActRegHd.reg_active;
				ActRegHd.reg_active = regp;
			}
		}

		/*
		 * Weave this segment into the existing partitions,
		 * creating new partitions as necessary.
		 */
		if( PartHd.pt_forw == &PartHd )  {
			/* No partitions yet, simple! */
			GET_PART_INIT( pp );
			pp->pt_solhit[stp->st_bin] = TRUE;
			pp->pt_instp = stp;
			pp->pt_inhit = &segp->seg_in;
			pp->pt_outstp = stp;
			pp->pt_outhit = &segp->seg_out;
			APPEND_PART( pp, &PartHd );
			goto done_weave;
		}
		dist = segp->seg_in.hit_dist;
		if( fdiff(dist, PartHd.pt_back->pt_outdist) > 0 )  {
			/* Segment starts beyond last partitions end */
			GET_PART_INIT( pp );
			pp->pt_solhit[stp->st_bin] = TRUE;
			pp->pt_instp = stp;
			pp->pt_inhit = &segp->seg_in;
			pp->pt_outstp = stp;
			pp->pt_outhit = &segp->seg_out;
			APPEND_PART( pp, PartHd.pt_back );
			goto done_weave;
		}
		if( fdiff(segp->seg_out.hit_dist, PartHd.pt_back->pt_outdist) > 0 )  {
			/*
			 * Segment ends beyond the end of the last
			 * partition.  Create an additional partition.
			 */
			GET_PART_INIT( pp );
			pp->pt_solhit[stp->st_bin] = TRUE;
			pp->pt_instp = PartHd.pt_back->pt_outstp;
			pp->pt_inhit = PartHd.pt_back->pt_outhit;
			pp->pt_outstp = stp;
			pp->pt_outhit = &segp->seg_out;
			APPEND_PART( pp, PartHd.pt_back );
		}
		for( pp=PartHd.pt_forw; pp != &PartHd; pp=pp->pt_forw ) {
			if( fdiff(dist, pp->pt_outdist) >= 0 )  {
				/* Seg starts after current partition */
				continue;
			}
			i = fdiff(dist, pp->pt_indist);
			if( i == 0 )  {
equal_start:			/*
				 * Segment and partition start at
				 * (roughly) the same point.
				 */
				i = fdiff(segp->seg_out.hit_dist, pp->pt_outdist);
				if( i == 0 )  {
					/*
					 * Segment and partition end
					 * (nearly) together
					 */
					pp->pt_solhit[stp->st_bin] = TRUE;
					goto done_weave;
				}
				if( i > 0 )  {
					/* Seg continues beyond part. end */
					pp->pt_solhit[stp->st_bin] = TRUE;
					dist = pp->pt_outdist;
					/* NEEDS HELP */
					continue;
				}
				/* Segment ends before partition ends */
				GET_PART( newpp );
				*newpp = *pp;		/* struct copy */
				/* new partition contains segment */
				newpp->pt_solhit[stp->st_bin] = TRUE;
				newpp->pt_outstp = pp->pt_instp = stp;
				newpp->pt_outhit = pp->pt_inhit = &segp->seg_out;
				INSERT_PART( newpp, pp );
				goto done_weave;
			}
			if( i < 0 )  {
				/*
				 * Seg starts before current partition starts,
				 * but after the previous partition ends.
				 */
				GET_PART_INIT( newpp );
				newpp->pt_solhit[stp->st_bin] = TRUE;
				newpp->pt_instp = stp;
				newpp->pt_inhit = &segp->seg_in;
				newpp->pt_outstp = stp;
				newpp->pt_outhit = &segp->seg_out;
				INSERT_PART( newpp, pp );
				if( fdiff(segp->seg_out.hit_dist, pp->pt_indist) <= 0 )  {
					/* Seg ends before partition starts */
					goto done_weave;
				}
				newpp->pt_outstp = pp->pt_instp;
				newpp->pt_outhit = pp->pt_inhit;
				/* dist = pp->pt_indist; */
				goto equal_start;
			}
			/*
			 *	dist > pp->pt_indist
			 *
			 *  Segment starts after partition starts,
			 *  but before the end of the partition.
			 */
			GET_PART( newpp );
			*newpp = *pp;			/* struct copy */
			pp->pt_solhit[stp->st_bin] = TRUE;
			/* new partition is span before seg joins partition */
			newpp->pt_outstp = pp->pt_instp = stp;
			newpp->pt_outhit = pp->pt_inhit = &segp->seg_in;
			INSERT_PART( newpp, pp );
			goto equal_start;
		}
		printf("bool_regions:  fell out of seg_weave loop\n");
		/* Sorry about the goto's, but they give added protection */
done_weave:	;
		if(debug&DEBUG_PARTITION)
			pr_partitions( &PartHd, "After weave" );
	}
	if(debug&DEBUG_PARTITION)
		pr_act_regions( &ActRegHd );

	/*
	 * For each partition, evaluate the boolean expression for
	 * all the regions.  If 0 regions result, continue with next
	 * partition.  If 1 region results, a valid hit has occured,
	 * so output a new segment.  If 2 or more regions claim the
	 * partition, then an overlap exists.
	 */
	FinalHd.pt_forw = FinalHd.pt_back = &FinalHd;	/* debug */

	pp = PartHd.pt_forw;
	while( pp != &PartHd )  {
		static int hitcnt;
		static struct region *lastregion;

		hitcnt = 0;
		if(debug&DEBUG_PARTITION)
			printf("considering partition x%x\n", pp );
		if( (regp=ActRegHd.reg_active) == &ActRegHd )  {
			lastregion = REGION_NULL;
			goto add_partition;
		}
		for( ; regp != &ActRegHd; regp = regp->reg_active )  {
			if(debug&DEBUG_PARTITION)
				printf("considering region x%x\n", regp );
			if( bool_eval( regp->reg_treetop, pp ) == FALSE )
				continue;
			/* region claims partition */
			if( ++hitcnt > 1 ) {
				printf("ERROR: region %s overlaps region %s in range (%f,%f)\n",
					regp->reg_name,
					lastregion->reg_name,
					pp->pt_indist,
					pp->pt_outdist );
				VPRINT("ERR IN Pt", pp->pt_inhit->hit_point );
				VPRINT("ERROUT Pt", pp->pt_outhit->hit_point);
			} else {
				lastregion = regp;
			}
		}
		if( hitcnt == 0 )  {
			pp=pp->pt_forw;			/* onwards! */
			continue;
		}
add_partition:	/* Add this partition to the result queue */
		{
			register struct partition *newpp;	/* XXX */

			newpp = pp;
			pp=pp->pt_forw;
			DEQUEUE_PART( newpp );
			newpp->pt_regionp = lastregion;
			APPEND_PART( newpp, FinalHd.pt_back );
		}
	}
	if( debug&DEBUG_PARTITION )
		pr_partitions( &FinalHd, "Final Partitions" );

	/*
	 * Cleanup:  Put zeros in the bin#s of all the solids that were used,
	 * and zap the reg_active chain.
	 */
	for( segp = segp_in; segp != SEG_NULL; segp = segp->seg_next )  {
		stp = segp->seg_stp;
		stp->st_bin = FALSE;
	}
	for( regp=ActRegHd.reg_active; regp != &ActRegHd; )  {
		register struct region *newreg;			/* XXX */
		newreg = regp;
		regp = regp->reg_active;
		newreg->reg_active = REGION_NULL;
	}
	for( pp = PartHd.pt_forw; pp != &PartHd;  )  {
		register struct partition *newpp;
		newpp = pp;
		pp = pp->pt_forw;
		FREE_PART(newpp);
	}
	/* Free seg chain in caller */

	return( &FinalHd );
}

/*
 *  			B O O L _ E V A L
 *  
 *  Function -
 *  	Given a tree node, evaluate it, possibly recursing.
 */
bool_eval( treep, partp )
register struct tree *treep;
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
			ret = FALSE;
		else  {
			ret = !bool_eval( treep->tr_right, partp );
			if( ret == FALSE )  {
				/* Was subtracted, flip exit normal */
				VREVERSE( partp->pt_outhit->hit_normal,
					  partp->pt_outhit->hit_normal );
			}
		}
		break;

	default:
		printf("bool_eval: bad op=x%x", treep->tr_op );
		ret = TRUE;		/* screw up output, get it fixed! */
		break;
	}
	return( ret );
}

/* Called with address of head of chain */
static
pr_partitions( phead, title )
register struct partition *phead;
char *title;
{
	register struct partition *pp;

	printf("----Partitions: %s\n", title);
	printf("%.8x: forw=%.8x back=%.8x (HEAD)\n",
		phead, phead->pt_forw, phead->pt_back );
	for( pp = phead->pt_forw; pp != phead; pp = pp->pt_forw ) {
		printf("%.8x: forw=%.8x back=%.8x (%f,%f)\n",
			pp, pp->pt_forw, pp->pt_back,
			pp->pt_indist, pp->pt_outdist );
		printf("\t Nin=[%f,%f,%f] Nout=[%f,%f,%f]\n",
			pp->pt_inhit->hit_normal[0],
			pp->pt_inhit->hit_normal[1],
			pp->pt_inhit->hit_normal[2],
			pp->pt_outhit->hit_normal[0],
			pp->pt_outhit->hit_normal[1],
			pp->pt_outhit->hit_normal[2] );
	}
	printf("----\n");
}

static
pr_act_regions(headp)
register struct region *headp;
{
	register struct region *regp;

	printf("Active Regions:\n");
	for( regp=headp->reg_active; regp != headp; regp=regp->reg_active )  {
		printf("%.8x %s\n", regp, regp->reg_name );
	}
}

/*
 *  			F D I F F
 *  
 *  Compares two floating point numbers.  If they are within "epsilon"
 *  of each other, they are considered the same.
 *  
 *  Returns -
 *  	-1	if a < b
 *  	 0	if a ~= b
 *  	+1	if a > b
 */
int
fdiff( a, b )
float a, b;
{
	register float diff;

	diff = a - b;
	if( NEAR_ZERO(diff) )
		return(0);
	if( a < b )
		return(-1);
	return(1);
}
