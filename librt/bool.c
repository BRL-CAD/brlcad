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
#include "../h/machine.h"
#include "../h/vmath.h"
#include "../h/raytrace.h"
#include "debug.h"

/* Boolean values.  Not easy to change, but defined symbolicly */
#define FALSE	0
#define TRUE	1

struct partition *FreePart = PT_NULL;		/* Head of freelist */
extern struct region *HeadRegion;		/* READ-ONLY */

/*
 *			B O O L _ W E A V E
 *
 *  Weave a chain of segments into an existing set of partitions.
 *  The edge of each partition is an inhit or outhit of some solid (seg).
 *
 *  NOTE:  When the final partitions are completed, it is the users
 *  responsibility to honor the inflip and outflip flags.  They can
 *  not be flipped here because an outflip=1 edge and an inflip=0 edge
 *  following it may in fact be the same edge.  This could be dealt with
 *  by giving the partition struct a COPY of the inhit and outhit rather
 *  than a pointer, but that's more cycles than the neatness is worth.
 */
void
bool_weave( segp_in, PartHdp )
struct seg *segp_in;
struct partition *PartHdp;
{
	register struct seg *segp;
	register struct partition *pp;

	if(debug&DEBUG_PARTITION) rtlog("-------------------BOOL_WEAVE\n");
	for( segp = segp_in; segp != SEG_NULL; segp = segp->seg_next )  {
		register struct partition *newpp;		/* XXX */
		register struct seg *lastseg;
		register struct hit *lasthit;
		LOCAL lastflip;

		if(debug&DEBUG_PARTITION) pr_seg(segp);

		/* Totally ignore things behind the start position */
		if( segp->seg_out.hit_dist < -EPSILON )
			continue;

		/*  Eliminate very thin segments, or they will cause
		 *  trouble below.
		 */
		if( fdiff(segp->seg_in.hit_dist,segp->seg_out.hit_dist)==0 ) {
			rtlog("bool_weave:  Thin seg discarded: %s (%f,%f)\n",
				segp->seg_stp->st_name,
				segp->seg_in.hit_dist,
				segp->seg_out.hit_dist );
			continue;
		}

		/*
		 * Weave this segment into the existing partitions,
		 * creating new partitions as necessary.
		 */
		if( PartHdp->pt_forw == PartHdp )  {
			/* No partitions yet, simple! */
			GET_PT_INIT( pp );
			BITSET(pp->pt_solhit, segp->seg_stp->st_bit);
			pp->pt_inseg = segp;
			pp->pt_inhit = &segp->seg_in;
			pp->pt_outseg = segp;
			pp->pt_outhit = &segp->seg_out;
			APPEND_PT( pp, PartHdp );
			goto done_weave;
		}
		if( segp->seg_in.hit_dist >= PartHdp->pt_back->pt_outhit->hit_dist )  {
			/*
			 * Segment starts exactly at last partition's end,
			 * or beyond last partitions end.  Make new partition.
			 */
			GET_PT_INIT( pp );
			BITSET(pp->pt_solhit, segp->seg_stp->st_bit);
			pp->pt_inseg = segp;
			pp->pt_inhit = &segp->seg_in;
			pp->pt_outseg = segp;
			pp->pt_outhit = &segp->seg_out;
			APPEND_PT( pp, PartHdp->pt_back );
			goto done_weave;
		}

		lastseg = segp;
		lasthit = &segp->seg_in;
		lastflip = 0;
		for( pp=PartHdp->pt_forw; pp != PartHdp; pp=pp->pt_forw ) {
			register int i;		/* XXX */

			if( (i=fdiff(lasthit->hit_dist, pp->pt_outhit->hit_dist)) > 0 )  {
				/* Seg starts beyond the END of the
				 * current partition.
				 *	PPPP
				 *	        SSSS
				 * Advance to next partition.
				 */
				continue;
			}
			if( i == 0 )  {
				/*
				 * Seg starts almost "precisely" at the
				 * end of the current partition.
				 *	PPPP
				 *	    SSSS
				 * FUSE an exact match of the endpoints,
				 * advance to next partition.
				 */
				lasthit->hit_dist = pp->pt_outhit->hit_dist;
				VMOVE(lasthit->hit_point, pp->pt_outhit->hit_point);
				continue;
			}
			/*
			 * i < 0,  Seg starts before current partition ends
			 *	PPPPPPPPPPP
			 *	  SSSS...
			 */

			if( (i=fdiff(lasthit->hit_dist, pp->pt_inhit->hit_dist)) == 0){
equal_start:
				/*
				 * Segment and partition start at
				 * (roughly) the same point.
				 * When fuseing 2 points together
				 * (ie, when fdiff()==0), the two
				 * points MUST be forced to become
				 * exactly equal!
				 */
				if( (i = fdiff(segp->seg_out.hit_dist, pp->pt_outhit->hit_dist)) == 0 )  {
					/*
					 * Segment and partition start & end
					 * (nearly) together.
					 *	PPPP
					 *	SSSS
					 */
					BITSET(pp->pt_solhit, segp->seg_stp->st_bit);
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
					BITSET(pp->pt_solhit, segp->seg_stp->st_bit);
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
				COPY_PT(newpp,pp);
				/* new partition contains segment */
				BITSET(newpp->pt_solhit, segp->seg_stp->st_bit);
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
				BITSET(newpp->pt_solhit, segp->seg_stp->st_bit);
				newpp->pt_inseg = lastseg;
				newpp->pt_inhit = lasthit;
				newpp->pt_inflip = lastflip;
				if( (i=fdiff(segp->seg_out.hit_dist, pp->pt_inhit->hit_dist)) < 0 ){
					/*
					 * Seg starts and ends before current
					 * partition, but after previous
					 * partition ends (diff < 0).
					 *		SSSS
					 *	pppp		PPPPP...
					 *		newpp	pp
					 */
					newpp->pt_outseg = segp;
					newpp->pt_outhit = &segp->seg_out;
					newpp->pt_outflip = 0;
					INSERT_PT( newpp, pp );
					goto done_weave;
				}
				if( i==0 )  {
					/* Seg starts before current
					 * partition starts, and ends at or
					 * near the start of the partition.
					 * (diff == 0).  FUSE the points.
					 *	SSSSSS
					 *	     PPPPP
					 *	newpp|pp
					 * NOTE: only copy hit point, not
					 * normals or norm private stuff.
					 */
					newpp->pt_outseg = segp;
					newpp->pt_outhit = &segp->seg_out;
					newpp->pt_outhit->hit_dist = pp->pt_inhit->hit_dist;
					VMOVE(newpp->pt_outhit->hit_point, pp->pt_inhit->hit_point);
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
			 * lasthit->hit_dist > pp->pt_inhit->hit_dist
			 *
			 *  Segment starts after partition starts,
			 *  but before the end of the partition.
			 *  Note:  pt_solhit will be marked in equal_start.
			 *	PPPPPPPPPPPP
			 *	     SSSS...
			 *	newpp|pp
			 */
			GET_PT( newpp );
			COPY_PT( newpp, pp );
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
		BITSET(newpp->pt_solhit, segp->seg_stp->st_bit);
		newpp->pt_inseg = lastseg;
		newpp->pt_inhit = lasthit;
		newpp->pt_inflip = lastflip;
		newpp->pt_outseg = segp;
		newpp->pt_outhit = &segp->seg_out;
		APPEND_PT( newpp, PartHdp->pt_back );

done_weave:	; /* Sorry about the goto's, but they give clarity */
		if(debug&DEBUG_PARTITION)
			pr_partitions( PartHdp, "After weave" );
	}
}

/*
 *  			B O O L _ F I N A L
 *
 * For each partition, evaluate the boolean expression tree.
 * If 0 regions result, continue with next partition.
 * If 1 region results, a valid hit has occured, so transfer
 * the partition from the Input list to the Final list.
 * If 2 or more regions claim the partition, then an overlap exists.
 */
bool_final( InputHdp, FinalHdp, startdist, enddist, regionbits, ap )
struct partition *InputHdp;
struct partition *FinalHdp;
fastf_t startdist, enddist;
bitv_t *regionbits;
struct application *ap;
{
	register struct partition *pp;
	LOCAL struct region *lastregion;
	register int hitcnt;
	LOCAL struct region *TrueRg[2];
	register int i;
	extern union tree *RootTree;

	pp = InputHdp->pt_forw;
	while( pp != InputHdp )  {
		hitcnt = 0;
		if(debug&DEBUG_PARTITION)  {
			rtlog("bool_final: (%f,%f)\n", startdist, enddist );
			pr_pt( pp );
		}

		/* Sanity checks on sorting.  Remove later. */
		if( pp->pt_inhit->hit_dist >= pp->pt_outhit->hit_dist )  {
			rtlog("bool_final: thin or inverted partition %.8x\n", pp);
			pr_partitions( InputHdp, "With problem" );
		}
		if( pp->pt_forw != InputHdp && pp->pt_outhit->hit_dist > pp->pt_forw->pt_inhit->hit_dist )  {
			rtlog("bool_final:  sorting defect!\n");
			if( !(debug & DEBUG_PARTITION) )
				pr_partitions( InputHdp, "With DEFECT" );
			return; /* give up */
		}

		/* If partition begins beyond current box, stop */
		if( pp->pt_inhit->hit_dist > enddist )
			return;

		/*
		 * If partition exists entirely behind start position, or
		 * if partition ends before current box starts,
		 * discard it, as we should never need to look back.
		 */
		if( pp->pt_outhit->hit_dist < -EPSILON
		    || pp->pt_outhit->hit_dist < startdist
		)  {
			register struct partition *zappp;
			if(debug&DEBUG_PARTITION)rtlog("bool_final discarding partition x%x\n", pp);
			zappp = pp;
			pp = pp->pt_forw;
			DEQUEUE_PT(zappp);
			FREE_PT(zappp);
			continue;
		}

#ifndef NEW
		/* Evaluate the boolean trees of any regions involved */
		for( i=0; i < nregions; i++ )  {
			register struct region *regp;

/**			if( !BITTEST(regionbits, i) )  continue; **/
			{
				register int j;
				if( (j = regionbits[i>>BITV_SHIFT])==0 )  {
					i = ((i+1+BITV_MASK)&(~BITV_MASK))-1;
					continue;
				}
				if( !(j & (1<<(i&BITV_MASK))) )  continue;
			}
			regp = Regions[i];
			if(debug&DEBUG_PARTITION)
				rtlog("%.8x=%s: ", regp, regp->reg_name );
			if( bool_eval( regp->reg_treetop, pp, TrueRg ) == FALSE )  {
				if(debug&DEBUG_PARTITION) rtlog("FALSE\n");
				continue;
			} else {
				if(debug&DEBUG_PARTITION) rtlog("TRUE\n");
			}
			/* This region claims partition */
			if( ++hitcnt <= 1 )  {
				lastregion = regp;
				continue;
			}
#define OVERLAP_TOL	0.5	/* 1/2 of a milimeter */
			/* Two regions claim this partition */
			if((	(	regp->reg_aircode == 0
				    &&	lastregion->reg_aircode == 0 
				) /* Neither are air */
			     ||	(	regp->reg_aircode != 0
				   &&	lastregion->reg_aircode != 0
				   &&	regp->reg_aircode != lastregion->reg_aircode
				) /* Both are air, but different types */
			   ) &&
			   (	pp->pt_outhit->hit_dist -
				pp->pt_inhit->hit_dist
				> OVERLAP_TOL /* Permissable overlap */
			   ) )  {
				rtlog("OVERLAP: %s %s (x%d y%d lvl%d)\n",
					regp->reg_name,
					lastregion->reg_name,
					ap->a_x, ap->a_y, ap->a_level );
				pr_pt( pp );
			} else {
				/* One region is air, or overlap is
				 * within tolerance.  */
				if( lastregion->reg_aircode == 0 )
					lastregion = regp;
				hitcnt--;
			}
		}
		if( hitcnt == 0 )  {
			pp=pp->pt_forw;			/* onwards! */
			continue;
		}
#else
		if( (hitcnt = bool_eval( RootTree, pp, TrueRg )) == FALSE )  {
			if(debug&DEBUG_PARTITION) rtlog("FALSE\n");
			pp = pp->pt_forw;
			continue;
		}
		if( hitcnt < 0 )  {
			/*  GUARD error:  overlap */
			rtlog("OVERLAP: %s %s (%f,%f)\n",
				TrueRg[0]->reg_name,
				TrueRg[1]->reg_name,
				pp->pt_inhit->hit_dist,
				pp->pt_outhit->hit_dist );
		}
		lastregion = TrueRg[0];
		if(debug&DEBUG_PARTITION) rtlog("TRUE\n");
#endif
		if( pp->pt_outhit->hit_dist <= 0.001 /* milimeters */ )  {
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
			if(	lastregion == FinalHdp->pt_back->pt_regionp
			    &&	fdiff(	newpp->pt_inhit->hit_dist,
					FinalHdp->pt_back->pt_outhit->hit_dist
					) == 0
			)  {
				/* Adjacent partitions, same region, so join */
				FinalHdp->pt_back->pt_outhit = newpp->pt_outhit;
				FinalHdp->pt_back->pt_outflip = newpp->pt_outflip;
				FinalHdp->pt_back->pt_outseg = newpp->pt_outseg;
				FREE_PT( newpp );
				newpp = FinalHdp->pt_back;
			}  else  {
				APPEND_PT( newpp, FinalHdp->pt_back );
			}

			/* Shameless efficiency hack:
			 * If the application is for viewing only,
			 * the first hit beyond the start point is
			 * all we care about.
			 */
			if( ap->a_onehit && newpp->pt_inhit->hit_dist > 0.0 )
				break;

		}
	}
	if( debug&DEBUG_PARTITION )
		pr_partitions( FinalHdp, "bool_final: Partitions returned" );
	/* Caller must free both partition chains */
}

/*
 *  			B O O L _ E V A L
 *  
 *  Using a stack to recall state, evaluate a boolean expression
 *  without recursion.
 *
 *  For use with XOR, a pointer to the "first valid subtree" would
 *  be a useful addition, for bool_regions().
 *
 *  Returns -
 *	!0	tree is TRUE
 *	 0	tree is FALSE
 *	-1	tree is in error (GUARD)
 */
int
bool_eval( treep, partp, trueregp )
register union tree *treep;	/* Tree to evaluate */
struct partition *partp;	/* Partition to evaluate */
struct region **trueregp;	/* XOR true (and overlap) return */
{
#define STACKDEPTH	128
	LOCAL union tree *stackpile[STACKDEPTH];
	static union tree tree_not;		/* for OP_NOT nodes */
	static union tree tree_guard;		/* for OP_GUARD nodes */
	static union tree tree_xnop;		/* for OP_XNOP nodes */
	register union tree **sp;
	register int ret;

	if( treep->tr_op != OP_XOR )
		trueregp[0] = treep->tr_regionp;
	else
		trueregp[0] = trueregp[1] = REGION_NULL;
	sp = stackpile;
	*sp++ = TREE_NULL;
stack:
	switch( treep->tr_op )  {
	case OP_SOLID:
		ret = treep->tr_a.tu_stp->st_bit;	/* register temp */
		ret = BITTEST( partp->pt_solhit, ret );
		goto pop;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
		*sp++ = treep;
		if( sp >= &stackpile[STACKDEPTH] )  {
			rtlog("bool_eval: stack overflow!\n");
			return(TRUE);	/* screw up output */
		}
		treep = treep->tr_b.tb_left;
		goto stack;
	default:
		rtlog("bool_eval:  bad stack op x%x\n",treep->tr_op);
		return(TRUE);	/* screw up output */
	}
pop:
	if( (treep = *--sp) == TREE_NULL )
		return(ret);		/* top of tree again */
	/*
	 *  Here, each operation will look at the operation just
	 *  completed (the left branch of the tree generally),
	 *  and rewrite the top of the stack and/or branch
	 *  accordingly.
	 */
	switch( treep->tr_op )  {
	case OP_SOLID:
		rtlog("bool_eval:  pop SOLID?\n");
		return(TRUE);	/* screw up output */
	case OP_UNION:
		if( ret )  goto pop;	/* TRUE, we are done */
		/* lhs was false, rewrite as rhs tree */
		treep = treep->tr_b.tb_right;
		goto stack;
	case OP_INTERSECT:
		if( !ret )  {
			ret = FALSE;
			goto pop;
		}
		/* lhs was true, rewrite as rhs tree */
		treep = treep->tr_b.tb_right;
		goto stack;
	case OP_SUBTRACT:
		if( !ret )  goto pop;	/* FALSE, we are done */
		/* lhs was true, rewrite as NOT of rhs tree */
		/* We introduce the special NOT operator here */
		tree_not.tr_op = OP_NOT;
		*sp++ = &tree_not;
		treep = treep->tr_b.tb_right;
		goto stack;
	case OP_NOT:
		/* Special operation for subtraction */
		ret = !ret;
		goto pop;
	case OP_XOR:
		if( ret )  {
			/* lhs was true, rhs better not be, or we have
			 * an overlap condition.  Rewrite as guard node
			 * followed by rhs.
			 */
			if( treep->tr_b.tb_left->tr_regionp )
				trueregp[0] = treep->tr_b.tb_left->tr_regionp;
			tree_guard.tr_op = OP_GUARD;
			treep = treep->tr_b.tb_right;
			*sp++ = treep;		/* temp val for guard node */
			*sp++ = &tree_guard;
		} else {
			/* lhs was false, rewrite as xnop node and
			 * result of rhs.
			 */
			tree_xnop.tr_op = OP_XNOP;
			treep = treep->tr_b.tb_right;
			*sp++ = treep;		/* temp val for xnop */
			*sp++ = &tree_xnop;
		}
		goto stack;
	case OP_GUARD:
		/*
		 *  Special operation for XOR.  lhs was true.
		 *  If rhs subtree was true, an
		 *  overlap condition exists (both sides of the XOR are
		 *  TRUE).  Return error condition.
		 *  If subtree is false, then return TRUE (from lhs).
		 */
		if( ret )  {
			/* stacked temp val: rhs */
			if( sp[-1]->tr_regionp )
				trueregp[1] = sp[-1]->tr_regionp;
			return(-1);	/* GUARD error */
		}
		ret = TRUE;
		sp--;			/* pop temp val */
		goto pop;
	case OP_XNOP:
		/*
		 *  Special NOP for XOR.  lhs was false.
		 *  If rhs is true, take note of it's regionp.
		 */
		sp--;			/* pop temp val */
		if( ret )  {
			if( (*sp)->tr_regionp )
				trueregp[0] = (*sp)->tr_regionp;
		}
		goto pop;
	default:
		rtlog("bool_eval:  bad pop op x%x\n",treep->tr_op);
		return(TRUE);	/* screw up output */
	}
	/* NOTREACHED */
}

/* Called with address of head of chain */
void
pr_partitions( phead, title )
register struct partition *phead;
char *title;
{
	register struct partition *pp;
	register int i;

	rtlog("------%s\n", title);
	for( pp = phead->pt_forw; pp != phead; pp = pp->pt_forw ) {
		pr_pt(pp);
	}
	rtlog("------\n");
}

pr_pt( pp )
register struct partition *pp;
{
	rtlog("%.8x: PT %s %s (%f,%f)",
		pp,
		pp->pt_inseg->seg_stp->st_name,
		pp->pt_outseg->seg_stp->st_name,
		pp->pt_inhit->hit_dist, pp->pt_outhit->hit_dist );
	rtlog("%s%s\n",
		pp->pt_inflip ? " Iflip" : "",
		pp->pt_outflip ?" Oflip" : "" );
	pr_bitv( "Solids", pp->pt_solhit, nsolids );
}

/*
 *  			P R _ B I T V
 *
 *  Print the bits set in a bit vector.
 */
pr_bitv( str, bv, len )
char *str;
register bitv_t *bv;
register int len;
{
	register int i;
	rtlog("%s: ", str);
	for( i=0; i<len; i++ )
		if( BITTEST(bv,i) )
			rtlog("%d, ", i );
	rtlog("\n");
}

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
	/* d = Max(Abs(a),Abs(b)) */
	d = (a >= 0.0) ? a : -a;
	if( b >= 0.0 )
		{
		if( b > d )  d = b;
		}
	else
		if( (-b) > d )  d = (-b);
	if( d <= EPSILON )
		return(0);	/* both nearly zero */
	if( diff < 0.0 )  diff = -diff;
	if( diff < 0.000001 * d )
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
	FAST fastf_t	d;
	FAST fastf_t	diff;

	/* d = Max(Abs(a),Abs(b)) */
	d = (a >= 0.0) ? a : -a;
	if( b >= 0.0 )
		if( b > d )  d = b;
	else
		if( (-b) > d )  d = (-b);
	if( d==0.0 )
		return( 0.0 );
	if( (diff = a - b) < 0.0 )  diff = -diff;
	return( diff / d );
}

/*
 *			P R _ S E G
 */
void
pr_seg(segp)
register struct seg *segp;
{
	rtlog("%.8x: SEG %s (%f,%f) bit=%d\n",
		segp,
		segp->seg_stp->st_name,
		segp->seg_in.hit_dist,
		segp->seg_out.hit_dist,
		segp->seg_stp->st_bit );
}

/*
 *  			P R _ H I T
 */
void
pr_hit( str, hitp )
char *str;
register struct hit *hitp;
{
	rtlog("HIT %s dist=%f\n", str, hitp->hit_dist );
	VPRINT("HIT Point ", hitp->hit_point );
	VPRINT("HIT Normal", hitp->hit_normal );
}
