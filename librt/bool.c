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
static char RCSbool[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./debug.h"

/* Boolean values.  Not easy to change, but defined symbolicly */
#define FALSE	0
#define TRUE	1

void	rt_grow_boolstack();

/*
 *			R T _ B O O L W E A V E
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
rt_boolweave( segp_in, PartHdp, ap )
struct seg		*segp_in;
struct partition	*PartHdp;
struct application	*ap;
{
	register struct seg *segp;
	register struct partition *pp;
	struct resource		*res = ap->a_resource;

	if( ap->a_rt_i->rti_magic != RTI_MAGIC )  rt_bomb("rt_boolweave:  bad rtip\n");
	if(rt_g.debug&DEBUG_PARTITION)
		rt_log("-------------------BOOL_WEAVE\n");
	for( segp = segp_in; segp != SEG_NULL; segp = segp->seg_next )  {
		register struct partition *newpp;
		register struct seg *lastseg;
		register struct hit *lasthit;
		LOCAL lastflip;

		if(rt_g.debug&DEBUG_PARTITION) rt_pr_seg(segp);
		if( segp->seg_stp->st_bit >= ap->a_rt_i->nsolids) rt_bomb("rt_boolweave: st_bit");

		/* Totally ignore things behind the start position */
		if( segp->seg_out.hit_dist < -10.0 )
			continue;

		/*  Eliminate very thin segments, or they will cause
		 *  trouble below.
		 */
		if( rt_fdiff(segp->seg_in.hit_dist,segp->seg_out.hit_dist)==0 ) {
			if(rt_g.debug&DEBUG_PARTITION)  rt_log(
				"rt_boolweave:  Thin seg discarded: %s (%g,%g)\n",
				segp->seg_stp->st_name,
				segp->seg_in.hit_dist,
				segp->seg_out.hit_dist );
			continue;
		}
		if( !(segp->seg_in.hit_dist >= -INFINITY &&
		    segp->seg_out.hit_dist <= INFINITY) )  {
		    	rt_log("rt_boolweave:  Defective segment %s (%g,%g)\n",
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
			GET_PT_INIT( ap->a_rt_i, pp, res );
			BITSET(pp->pt_solhit, segp->seg_stp->st_bit);
			pp->pt_inseg = segp;
			pp->pt_inhit = &segp->seg_in;
			pp->pt_outseg = segp;
			pp->pt_outhit = &segp->seg_out;
			APPEND_PT( pp, PartHdp );
			if(rt_g.debug&DEBUG_PARTITION) rt_log("First partition\n");
			goto done_weave;
		}
		if( segp->seg_in.hit_dist >= PartHdp->pt_back->pt_outhit->hit_dist )  {
			/*
			 * Segment starts exactly at last partition's end,
			 * or beyond last partitions end.  Make new partition.
			 */
			GET_PT_INIT( ap->a_rt_i, pp, res );
			BITSET(pp->pt_solhit, segp->seg_stp->st_bit);
			pp->pt_inseg = segp;
			pp->pt_inhit = &segp->seg_in;
			pp->pt_outseg = segp;
			pp->pt_outhit = &segp->seg_out;
			APPEND_PT( pp, PartHdp->pt_back );
			if(rt_g.debug&DEBUG_PARTITION) rt_log("seg starts beyond part end\n");
			goto done_weave;
		}

		lastseg = segp;
		lasthit = &segp->seg_in;
		lastflip = 0;
		for( pp=PartHdp->pt_forw; pp != PartHdp; pp=pp->pt_forw ) {
			register int i;

			if( (i=rt_fdiff(lasthit->hit_dist, pp->pt_outhit->hit_dist)) > 0 )  {
				/* Seg starts beyond the END of the
				 * current partition.
				 *	PPPP
				 *	        SSSS
				 * Advance to next partition.
				 */
				if(rt_g.debug&DEBUG_PARTITION)  rt_log("seg start beyond end, skipping\n");
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
				if(rt_g.debug&DEBUG_PARTITION)  rt_log("seg start fused to partition end\n");
				continue;
			}
			if(rt_g.debug&DEBUG_PARTITION)  rt_pr_pt(ap->a_rt_i, pp);

			/*
			 * i < 0,  Seg starts before current partition ends
			 *	PPPPPPPPPPP
			 *	  SSSS...
			 */

			if( (i=rt_fdiff(lasthit->hit_dist, pp->pt_inhit->hit_dist)) == 0){
equal_start:
				if(rt_g.debug&DEBUG_PARTITION) rt_log("equal_start\n");
				/*
				 * Segment and partition start at
				 * (roughly) the same point.
				 * When fuseing 2 points together
				 * (ie, when rt_fdiff()==0), the two
				 * points MUST be forced to become
				 * exactly equal!
				 */
				if( (i = rt_fdiff(segp->seg_out.hit_dist, pp->pt_outhit->hit_dist)) == 0 )  {
					/*
					 * Segment and partition start & end
					 * (nearly) together.
					 *	PPPP
					 *	SSSS
					 */
					BITSET(pp->pt_solhit, segp->seg_stp->st_bit);
					if(rt_g.debug&DEBUG_PARTITION) rt_log("same start&end\n");
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
					if(rt_g.debug&DEBUG_PARTITION) rt_log("seg spans p and beyond\n");
					continue;
				}
				/*
				 *  Segment + Partition start together,
				 *  segment ends before partition ends.
				 *	PPPPPPPPPP
				 *	SSSSSS
				 *	newpp| pp
				 */
				GET_PT( ap->a_rt_i, newpp, res );
				COPY_PT( ap->a_rt_i,newpp,pp);
				/* new partition contains segment */
				BITSET(newpp->pt_solhit, segp->seg_stp->st_bit);
				newpp->pt_outseg = segp;
				newpp->pt_outhit = &segp->seg_out;
				newpp->pt_outflip = 0;
				pp->pt_inseg = segp;
				pp->pt_inhit = &segp->seg_out;
				pp->pt_inflip = 1;
				INSERT_PT( newpp, pp );
				if(rt_g.debug&DEBUG_PARTITION) rt_log("start together, seg shorter\n");
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
				GET_PT_INIT( ap->a_rt_i, newpp, res );
				BITSET(newpp->pt_solhit, segp->seg_stp->st_bit);
				newpp->pt_inseg = lastseg;
				newpp->pt_inhit = lasthit;
				newpp->pt_inflip = lastflip;
				if( (i=rt_fdiff(segp->seg_out.hit_dist, pp->pt_inhit->hit_dist)) < 0 ){
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
					if(rt_g.debug&DEBUG_PARTITION) rt_log("seg between 2 partitions\n");
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
					if(rt_g.debug&DEBUG_PARTITION) rt_log("seg ends at p start, fuse\n");
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
				if(rt_g.debug&DEBUG_PARTITION) rt_log("insert seg before p start, ends after p ends\n");
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
			GET_PT( ap->a_rt_i, newpp, res );
			COPY_PT( ap->a_rt_i, newpp, pp );
			/* new part. is the span before seg joins partition */
			pp->pt_inseg = segp;
			pp->pt_inhit = &segp->seg_in;
			pp->pt_inflip = 0;
			newpp->pt_outseg = segp;
			newpp->pt_outhit = &segp->seg_in;
			newpp->pt_outflip = 1;
			INSERT_PT( newpp, pp );
			if(rt_g.debug&DEBUG_PARTITION) rt_log("seg starts after p starts, ends after p ends\n");
			goto equal_start;
		}

		/*
		 *  Segment has portion which extends beyond the end
		 *  of the last partition.  Tack on the remainder.
		 *  	PPPPP
		 *  	     SSSSS
		 */
		if(rt_g.debug&DEBUG_PARTITION) rt_log("seg extends beyond p end\n");
		GET_PT_INIT( ap->a_rt_i, newpp, res );
		BITSET(newpp->pt_solhit, segp->seg_stp->st_bit);
		newpp->pt_inseg = lastseg;
		newpp->pt_inhit = lasthit;
		newpp->pt_inflip = lastflip;
		newpp->pt_outseg = segp;
		newpp->pt_outhit = &segp->seg_out;
		APPEND_PT( newpp, PartHdp->pt_back );

done_weave:	; /* Sorry about the goto's, but they give clarity */
		if(rt_g.debug&DEBUG_PARTITION)
			rt_pr_partitions( ap->a_rt_i, PartHdp, "After weave" );
	}
}

/*
 *			R T _ D E F O V E R L A P
 *
 *  Default handler for overlaps in rt_boolfinal().
 *  Returns -
 *	 0	to eliminate partition with overlap entirely
 *	!0	to retain partition in output list
 */
int
rt_defoverlap( ap, pp, reg1, reg2 )
register struct application	*ap;
register struct partition	*pp;
struct region			*reg1;
struct region			*reg2;
{
	point_t	pt;
	static long count = 0;		/* Not PARALLEL, shouldn't hurt */
	register fastf_t depth;

	/* Attempt to control tremendous error outputs */
	if( ++count > 100 )  {
		if( (count%100) != 3 )  return(1);
		rt_log("(overlaps omitted)\n");
	}

	VJOIN1( pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist,
		ap->a_ray.r_dir );
	depth = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	/*
	 * An application program might want to add code here
	 * to ignore "small" overlap depths.
	 */
	rt_log("OVERLAP1: reg=%s isol=%s\n",
		reg1->reg_name, pp->pt_inseg->seg_stp->st_name);
	rt_log("OVERLAP2: reg=%s osol=%s\n",
		reg2->reg_name, pp->pt_outseg->seg_stp->st_name);
	rt_log("OVERLAP depth %gmm at (%g,%g,%g) x%d y%d lvl%d\n",
		depth, pt[X], pt[Y], pt[Z],
		ap->a_x, ap->a_y, ap->a_level );
	return(1);
}

/*
 *			R T _ B O O L F I N A L
 *
 * For each partition, evaluate the boolean expression tree.
 * If 0 regions result, continue with next partition.
 * If 1 region results, a valid hit has occured, so transfer
 * the partition from the Input list to the Final list.
 * If 2 or more regions claim the partition, then an overlap exists.
 * If the overlap handler gives a non-zero return, then the overlapping
 * partition is kept, with the region ID being the first one encountered.
 * Otherwise, the partition is eliminated from further consideration. 
 */
void
rt_boolfinal( InputHdp, FinalHdp, startdist, enddist, regionbits, ap )
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

	if( ap->a_rt_i->rti_magic != RTI_MAGIC )  rt_bomb("rt_boolweave:  bad rtip\n");
	pp = InputHdp->pt_forw;
	while( pp != InputHdp )  {
		hitcnt = 0;
		if(rt_g.debug&DEBUG_PARTITION)  {
			rt_log("rt_boolfinal: (%g,%g)\n", startdist, enddist );
			rt_pr_pt( ap->a_rt_i, pp );
		}

		/* Sanity checks on sorting.  Remove later. */
		if( pp->pt_inhit->hit_dist >= pp->pt_outhit->hit_dist )  {
			rt_log("rt_boolfinal: thin or inverted partition %.8x\n", pp);
			rt_pr_partitions( ap->a_rt_i, InputHdp, "With problem" );
		}
		if( pp->pt_forw != InputHdp && pp->pt_outhit->hit_dist > pp->pt_forw->pt_inhit->hit_dist )  {
			rt_log("rt_boolfinal:  sorting defect!\n");
			if( !(rt_g.debug & DEBUG_PARTITION) )
				rt_pr_partitions( ap->a_rt_i, InputHdp, "With DEFECT" );
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
		if( pp->pt_outhit->hit_dist < -10.0
		    || pp->pt_outhit->hit_dist < startdist
		)  {
			register struct partition *zappp;
			if(rt_g.debug&DEBUG_PARTITION)rt_log("rt_boolfinal discarding partition x%x\n", pp);
			zappp = pp;
			pp = pp->pt_forw;
			DEQUEUE_PT(zappp);
			FREE_PT(zappp, ap->a_resource);
			continue;
		}

		/* Evaluate the boolean trees of any regions involved */
		for( i=0; i < ap->a_rt_i->nregions; i++ )  {
			register struct region *regp;

/**			if( !BITTEST(regionbits, i) )  continue; **/
			{
				register bitv_t j;
				if( (j = regionbits[i>>BITV_SHIFT])==0 )  {
					i = ((i+1+BITV_MASK)&(~BITV_MASK))-1;
					continue;
				}
				if( !(j & (((bitv_t)1)<<(i&BITV_MASK))) )
					continue;
			}
			regp = ap->a_rt_i->Regions[i];
			if(rt_g.debug&DEBUG_PARTITION)
				rt_log("%.8x=bit%d, %s: ", regp, i, regp->reg_name );
			if( rt_booleval( regp->reg_treetop, pp, TrueRg,
			    ap->a_resource ) == FALSE )  {
				if(rt_g.debug&DEBUG_PARTITION) rt_log("FALSE\n");
				continue;
			} else {
				if(rt_g.debug&DEBUG_PARTITION) rt_log("TRUE\n");
			}
			/* This region claims partition */
			if( ++hitcnt <= 1 )  {
				lastregion = regp;
				continue;
			}

			/*
			 * Two or more regions claim this partition
			 */
			if((	(	regp->reg_aircode == 0
				    &&	lastregion->reg_aircode == 0 
				) /* Neither are air */
			     ||	(	regp->reg_aircode != 0
				   &&	lastregion->reg_aircode != 0
				   &&	regp->reg_aircode != lastregion->reg_aircode
				) /* Both are air, but different types */
			   ) )  {
			   	/*
			   	 *  Hand overlap to application-specific
			   	 *  overlap handler, or default.
			   	 */
			   	if( ap->a_overlap == RT_AFN_NULL )
			   		ap->a_overlap = rt_defoverlap;
				if( ap->a_overlap(ap, pp, regp, lastregion) )  {
				    	/* non-zero => retain last partition */
					if( lastregion->reg_aircode != 0 )
						lastregion = regp;
				    	hitcnt--;	/* now = 1 */
				}
			} else {
				/* last region is air, replace with solid */
				if( lastregion->reg_aircode != 0 )
					lastregion = regp;
				hitcnt--;		/* now = 1 */
			}
		}
		if( hitcnt == 0 )  {
			pp=pp->pt_forw;			/* onwards! */
			continue;
		}
		if( pp->pt_outhit->hit_dist <= 0.001 /* milimeters */ )  {
			/* partition is behind start point (k=0), ignore */
			pp=pp->pt_forw;
			continue;
		}

		/* Add this partition to the result queue */
		{
			register struct partition *newpp;

			newpp = pp;
			pp=pp->pt_forw;
			DEQUEUE_PT( newpp );
			newpp->pt_regionp = lastregion;
			if( hitcnt > 1 )  {
				/* Overlapping partition, discard it.	*/
				FREE_PT(newpp, ap->a_resource);
			}  else if( lastregion == FinalHdp->pt_back->pt_regionp
			         &&  rt_fdiff(	newpp->pt_inhit->hit_dist,
					FinalHdp->pt_back->pt_outhit->hit_dist
					) == 0
			)  {
				/* Adjacent partitions, same region, so join */
				FinalHdp->pt_back->pt_outhit = newpp->pt_outhit;
				FinalHdp->pt_back->pt_outflip = newpp->pt_outflip;
				FinalHdp->pt_back->pt_outseg = newpp->pt_outseg;
				FREE_PT( newpp, ap->a_resource );
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
	if( rt_g.debug&DEBUG_PARTITION )
		rt_pr_partitions( ap->a_rt_i, FinalHdp, "rt_boolfinal: Partitions returned" );
	/* Caller must free both partition chains */
}

/*
 *  			R T _ B O O L E V A L
 *  
 *  Using a stack to recall state, evaluate a boolean expression
 *  without recursion.
 *
 *  For use with XOR, a pointer to the "first valid subtree" would
 *  be a useful addition, for rt_boolregions().
 *
 *  Returns -
 *	!0	tree is TRUE
 *	 0	tree is FALSE
 *	-1	tree is in error (GUARD)
 */
int
rt_booleval( treep, partp, trueregp, resp )
register union tree *treep;	/* Tree to evaluate */
struct partition *partp;	/* Partition to evaluate */
struct region	**trueregp;	/* XOR true (and overlap) return */
struct resource	*resp;		/* resource pointer for this CPU */
{
	static union tree tree_not;		/* for OP_NOT nodes */
	static union tree tree_guard;		/* for OP_GUARD nodes */
	static union tree tree_xnop;		/* for OP_XNOP nodes */
	register union tree **sp;
	register int ret;
	register union tree **stackend;

	if( treep->tr_op != OP_XOR )
		trueregp[0] = treep->tr_regionp;
	else
		trueregp[0] = trueregp[1] = REGION_NULL;
	while( (sp = resp->re_boolstack) == (union tree **)0 )
		rt_grow_boolstack( resp );
	stackend = &(resp->re_boolstack[resp->re_boolslen]);
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
		if( sp >= stackend )  {
			register int off = sp - resp->re_boolstack;
			rt_grow_boolstack( resp );
			sp = &(resp->re_boolstack[off]);
			stackend = &(resp->re_boolstack[resp->re_boolslen]);
		}
		treep = treep->tr_b.tb_left;
		goto stack;
	default:
		rt_log("rt_booleval:  bad stack op x%x\n",treep->tr_op);
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
		rt_log("rt_booleval:  pop SOLID?\n");
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
		rt_log("rt_booleval:  bad pop op x%x\n",treep->tr_op);
		return(TRUE);	/* screw up output */
	}
	/* NOTREACHED */
}

/*
 *			R T _ P R _ P A R T I T I O N S
 *
 */
void
rt_pr_partitions( rtip, phead, title )
struct rt_i		*rtip;
register struct partition *phead;
char *title;
{
	register struct partition *pp;

	if( rtip->rti_magic != RTI_MAGIC )  rt_bomb("rt_pr_partitions:  bad rtip\n");

	rt_log("------%s\n", title);
	for( pp = phead->pt_forw; pp != phead; pp = pp->pt_forw ) {
		rt_pr_pt(rtip, pp);
	}
	rt_log("------\n");
}

/*
 *			R T _ P R _ P T
 */
void
rt_pr_pt( rtip, pp )
struct rt_i		*rtip;
register struct partition *pp;
{
	if( rtip->rti_magic != RTI_MAGIC )  rt_bomb("rt_pr_pt:  bad rtip\n");

	rt_log("%.8x: PT %s %s (%g,%g)",
		pp,
		pp->pt_inseg->seg_stp->st_name,
		pp->pt_outseg->seg_stp->st_name,
		pp->pt_inhit->hit_dist, pp->pt_outhit->hit_dist );
	rt_log("%s%s\n",
		pp->pt_inflip ? " Iflip" : "",
		pp->pt_outflip ?" Oflip" : "" );
	rt_pr_bitv( "Solids", pp->pt_solhit, rtip->nsolids );
}

/*
 *			R T _ P R _ B I T V
 *
 *  Print the bits set in a bit vector.
 */
void
rt_pr_bitv( str, bv, len )
char *str;
register bitv_t *bv;
register int len;
{
	register int i;
	rt_log("%s: ", str);
	for( i=0; i<len; i++ )
		if( BITTEST(bv,i) )
			rt_log("%d, ", i );
	rt_log("\n");
}

/*
 *			R T _ F D I F F
 *  
 *  Compares two floating point numbers.  If they are within "epsilon"
 *  of each other, they are considered the same.
 *  NOTE:  This is a "fuzzy" difference.  It is important NOT to
 *  use the results of this function in compound comparisons,
 *  because a return of 0 makes no statement about the PRECISE
 *  relationships between the two numbers.  Eg,
 *	if( rt_fdiff( a, b ) <= 0 )
 *  is poison!
 *
 *  Returns -
 *  	-1	if a < b
 *  	 0	if a ~= b
 *  	+1	if a > b
 */
int
rt_fdiff( a, b )
double a, b;
{
	FAST double diff;
	FAST double d;
	register int ret;

	/* d = Max(Abs(a),Abs(b)) */
	d = (a >= 0.0) ? a : -a;
	if( b >= 0.0 )  {
		if( b > d )  d = b;
	} else {
		if( (-b) > d )  d = (-b);
	}
	if( d <= 1.0e-6 )  {
		ret = 0;	/* both nearly zero */
		goto out;
	}
	if( d >= INFINITY )  {
		if( a == b )  {
			ret = 0;
			goto out;
		}
		if( a < b )  {
			ret = -1;
			goto out;
		}
		ret = 1;
		goto out;
	}
	if( (diff = a - b) < 0.0 )  diff = -diff;
	if( diff < 0.001 )  {
		ret = 0;	/* absolute difference is small, < 1/1000mm */
		goto out;
	}
	if( diff < 0.000001 * d )  {
		ret = 0;	/* relative difference is small, < 1ppm */
		goto out;
	}
	if( a < b )  {
		ret = -1;
		goto out;
	}
	ret = 1;
out:
	if(rt_g.debug&DEBUG_FDIFF) rt_log("rt_fdiff(%g,%g)=%d\n", a, b, ret);
	return(ret);
}

/*
 *			R T _ R E L D I F F
 *
 * Compute the relative difference of two floating point numbers.
 *
 * Returns -
 *	0.0 if exactly the same, otherwise
 *	ratio of difference, relative to the larger of the two (range 0.0-1.0)
 */
double
rt_reldiff( a, b )
double	a, b;
{
	FAST fastf_t	d;
	FAST fastf_t	diff;

	/* d = Max(Abs(a),Abs(b)) */
	d = (a >= 0.0) ? a : -a;
	if( b >= 0.0 )  {
		if( b > d )  d = b;
	} else {
		if( (-b) > d )  d = (-b);
	}
	if( d==0.0 )
		return( 0.0 );
	if( (diff = a - b) < 0.0 )  diff = -diff;
	return( diff / d );
}

/*
 *			R T _ P R _ S E G
 */
void
rt_pr_seg(segp)
register struct seg *segp;
{
	rt_log("%.8x: SEG %s (%g,%g) bit=%d\n",
		segp,
		segp->seg_stp->st_name,
		segp->seg_in.hit_dist,
		segp->seg_out.hit_dist,
		segp->seg_stp->st_bit );
}

/*
 *			R T _ P R _ H I T
 */
void
rt_pr_hit( str, hitp )
char *str;
register struct hit *hitp;
{
	rt_log("HIT %s dist=%g\n", str, hitp->hit_dist );
	VPRINT("HIT Point ", hitp->hit_point );
	VPRINT("HIT Normal", hitp->hit_normal );
}

/*
 *			R T _ G R O W _ B O O L S T A C K
 *
 *  Increase the size of re_boolstack to double the previous size.
 *  Depend on rt_realloc() to copy the previous data to the new area
 *  when the size is increased.
 *  Return the new pointer for what was previously the last element.
 */
void
rt_grow_boolstack( resp )
register struct resource	*resp;
{
	if( resp->re_boolstack == (union tree **)0 || resp->re_boolslen <= 0 )  {
		resp->re_boolslen = 128;	/* default len */
		resp->re_boolstack = (union tree **)rt_malloc(
			sizeof(union tree *) * resp->re_boolslen,
			"initial boolstack");
	} else {
		resp->re_boolslen <<= 1;
		resp->re_boolstack = (union tree **)rt_realloc(
			(char *)resp->re_boolstack,
			sizeof(union tree *) * resp->re_boolslen,
			"extend boolstack" );
	}
}

/*
 *			R T _ P R I N T B
 *
 *  Print a value a la the %b format of the kernel's printf
 *
 *    s		title string
 *    v		the integer with the bits in it
 *    bits	a string which starts with the desired base, then followed by
 *		words preceeded with embedded low-value bytes indicating
 *		bit number plus one,
 *		in little-endian order, eg:
 *		"\010\2Bit_one\1BIT_zero"
 */
void
rt_printb(s, v, bits)
char *s;
register unsigned long v;
register char *bits;
{
	register int i, any = 0;
	register char c;

	if (*bits++ == 8)
		rt_log("%s=0%o <", s, v);
	else
		rt_log("%s=x%x <", s, v);
	while (i = *bits++) {
		if (v & (1L << (i-1))) {
			if (any)
				rt_log(",");
			any = 1;
			for (; (c = *bits) > 32; bits++)
				rt_log("%c", c);
		} else
			for (; *bits > 32; bits++)
				;
	}
	rt_log(">");
}
