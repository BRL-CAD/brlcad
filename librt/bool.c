/*
 *			B O O L
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
 *	U. S. Army Ballistic Research Laboratory
 *	April 13, 1984
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

/* Boolean values.  Not easy to change, but defined symbolicly */
#define FALSE	0
#define TRUE	1

int one_hit_flag;	/* non-zero if computation is for viewing only */

struct partition *FreePart = PT_NULL;		 /* Head of freelist */

/*
 *			B O O L _ R E G I O N S
 */
void
bool_regions( segp_in, FinalHdp )
struct seg *segp_in;
struct partition *FinalHdp;	/* Heads final circ list */
{
	register struct seg *segp = segp_in;
	register struct soltab *stp;
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
		FAST fastf_t dist;				/* XXX */

		/* Make sure seg's solid's region is on active list */
		stp = segp->seg_stp;
		if( stp->st_bin == 0 )  {
			register struct region *regp;		/* XXX */

			if( (stp->st_bin = curbin++) >= NBINS )
				rtbomb("bool_regions:  need > NBINS bins");
			regp = stp->st_regionp;
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
			pp->pt_solhit[stp->st_bin] = TRUE;
			pp->pt_instp = stp;
			pp->pt_inhit = &segp->seg_in;
			pp->pt_outstp = stp;
			pp->pt_outhit = &segp->seg_out;
			APPEND_PT( pp, &PartHd );
			goto done_weave;
		}
		dist = segp->seg_in.hit_dist;
		if( fdiff(dist, PartHd.pt_back->pt_outdist) > 0 )  {
			/* Segment starts beyond last partitions end */
			GET_PT_INIT( pp );
			pp->pt_solhit[stp->st_bin] = TRUE;
			pp->pt_instp = stp;
			pp->pt_inhit = &segp->seg_in;
			pp->pt_outstp = stp;
			pp->pt_outhit = &segp->seg_out;
			APPEND_PT( pp, PartHd.pt_back );
			goto done_weave;
		}
		if( fdiff(segp->seg_out.hit_dist, PartHd.pt_back->pt_outdist) > 0 )  {
			/*
			 * Segment ends beyond the end of the last
			 * partition.  Create an additional partition.
			 */
			GET_PT_INIT( pp );
			pp->pt_solhit[stp->st_bin] = TRUE;
			pp->pt_instp = PartHd.pt_back->pt_outstp;
			pp->pt_inhit = PartHd.pt_back->pt_outhit;
			pp->pt_outstp = stp;
			pp->pt_outhit = &segp->seg_out;
			APPEND_PT( pp, PartHd.pt_back );
		}
		for( pp=PartHd.pt_forw; pp != &PartHd; pp=pp->pt_forw ) {
			register int i;		/* XXX */

			if( fdiff(dist, pp->pt_outdist) >= 0 )  {
				/* Seg starts after current partition ends,
				 * or exactly at the end.
				 */
				if( pp->pt_forw == &PartHd )  {
					/* beyond last part. end */
					GET_PT_INIT( newpp );
					newpp->pt_solhit[stp->st_bin] = TRUE;
					newpp->pt_instp = stp;
					newpp->pt_inhit = &segp->seg_in;
					newpp->pt_outstp = stp;
					newpp->pt_outhit = &segp->seg_out;
					APPEND_PT( newpp, pp );
					goto done_weave;
				}
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
					if( pp->pt_forw == &PartHd )  {
						/* beyond last part. end */
						GET_PT_INIT( newpp );
						newpp->pt_solhit[stp->st_bin] = TRUE;
						newpp->pt_instp = pp->pt_outstp;
						newpp->pt_inhit = pp->pt_outhit;
						newpp->pt_outstp = stp;
						newpp->pt_outhit = &segp->seg_out;
						APPEND_PT( newpp, pp );
						goto done_weave;
					}
					/*
					 * The rest of the work is done in the
					 * dist<indist case next pass through.
					 */
					continue;
				}
				/* Segment ends before partition ends */
				GET_PT( newpp );
				*newpp = *pp;		/* struct copy */
				/* new partition contains segment */
				newpp->pt_solhit[stp->st_bin] = TRUE;
				newpp->pt_outstp = pp->pt_instp = stp;
				newpp->pt_outhit = pp->pt_inhit = &segp->seg_out;
				INSERT_PT( newpp, pp );
				goto done_weave;
			}
			if( i < 0 )  {
				/*
				 * Seg starts before current partition starts,
				 * but after the previous partition ends.
				 */
				GET_PT_INIT( newpp );
				newpp->pt_solhit[stp->st_bin] = TRUE;
				newpp->pt_instp = stp;
				newpp->pt_inhit = &segp->seg_in;
				newpp->pt_outstp = stp;
				newpp->pt_outhit = &segp->seg_out;
				INSERT_PT( newpp, pp );
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
			 *  Note:  pt_solhit will be marked in equal_start.
			 */
			GET_PT( newpp );
			*newpp = *pp;			/* struct copy */
			/* new partition is span before seg joins partition */
			newpp->pt_outstp = pp->pt_instp = stp;
			newpp->pt_outhit = pp->pt_inhit = &segp->seg_in;
			INSERT_PT( newpp, pp );
			goto equal_start;
		}
		fprintf(stderr,"bool_regions:  fell out of seg_weave loop\n");
		/* Sorry about the goto's, but they give added protection */
done_weave:	;
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
	FinalHdp->pt_forw = FinalHdp->pt_back = FinalHdp;	/* debug */

	pp = PartHd.pt_forw;
	while( pp != &PartHd )  {
		LOCAL int hitcnt;
		LOCAL struct region *lastregion;
		register struct region *regp;

		hitcnt = 0;
		if(debug&DEBUG_PARTITION)
			fprintf(stderr,"considering partition %.8x\n", pp );

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

		/* Add this partition to the result queue */
		{
			register struct partition *newpp;	/* XXX */

			newpp = pp;
			pp=pp->pt_forw;
			DEQUEUE_PT( newpp );
			newpp->pt_regionp = lastregion;
			APPEND_PT( newpp, FinalHdp->pt_back );
			/* Shameless efficiency hack */
			if( !debug && one_hit_flag )  break;
		}
	}
	if( debug&DEBUG_PARTITION )
		pr_partitions( FinalHdp, "Final Partitions" );

	/*
	 * Cleanup:  Put zeros in the bin#s of all the solids that were used,
	 * and zap the reg_active chain.
	 */
	for( segp = segp_in; segp != SEG_NULL; segp = segp->seg_next )  {
		stp = segp->seg_stp;
		stp->st_bin = FALSE;
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
		fprintf(stderr,"bool_eval: bad op=x%x", treep->tr_op );
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
	register int i;

	fprintf(stderr,"----Partitions: %s\n", title);
	fprintf(stderr,"%.8x: forw=%.8x back=%.8x (HEAD)\n",
		phead, phead->pt_forw, phead->pt_back );
	for( pp = phead->pt_forw; pp != phead; pp = pp->pt_forw ) {
		fprintf(stderr,"%.8x: forw=%.8x back=%.8x (%f,%f)\n",
			pp, pp->pt_forw, pp->pt_back,
			pp->pt_indist, pp->pt_outdist );
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
		putchar('\n');
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
