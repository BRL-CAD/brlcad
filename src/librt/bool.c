/*                          B O O L . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup ray */
/** @{ */
/** @file librt/bool.c
 *
 * Ray Tracing program, Boolean region evaluator.
 *
 * Note to developers -
 * Do not use the hit_point field in these routines, as for some solid
 * types it has been filled in by the g_xxx_shot() routine, and for
 * other solid types it may not have been.  In particular, copying a
 * garbage hit_point from a structure which has not yet been filled
 * in, into a structure which won't be filled in again, gives wrong
 * results.  Thanks to Keith Bowman for finding this obscure bug.
 *
 */
/** @} */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include "bio.h"

#include "vmath.h"
#include "raytrace.h"
#include "bu.h"


/* Boolean values.  Not easy to change, but defined symbolicly */
#define BOOL_FALSE 0
#define BOOL_TRUE 1


/**
 * R T _ W E A V E 0 S E G
 *
 * If a zero thickness segment abutts another partition, it will be
 * fused in, later.
 *
 * If it is free standing, then it will remain as a zero thickness
 * partition, which probably signals going through some solid an odd
 * number of times, or hitting an NMG wire edge or NMG lone vertex.
 */
void
rt_weave0seg(struct seg *segp, struct partition *PartHdp, struct application *ap)
{
    register struct partition *pp;
    struct resource *res = ap->a_resource;
    struct rt_i *rtip = ap->a_rt_i;
    register fastf_t tol_dist;

    tol_dist = rtip->rti_tol.dist;

    RT_CK_PT_HD(PartHdp);
    RT_CK_RTI(ap->a_rt_i);
    RT_CK_RESOURCE(res);
    RT_CK_RTI(rtip);

    if (RT_G_DEBUG&DEBUG_PARTITION) {
	bu_log(
	    "rt_weave0seg:  Zero thickness seg: %s (%.18e, %.18e) %d, %d\n",
	    segp->seg_stp->st_name,
	    segp->seg_in.hit_dist,
	    segp->seg_out.hit_dist,
	    segp->seg_in.hit_surfno,
	    segp->seg_out.hit_surfno);
    }

    if (PartHdp->pt_forw == PartHdp) bu_bomb("rt_weave0seg() with empty partition list\n");

    /* See if this segment ends before start of first partition */
    if (segp->seg_out.hit_dist < PartHdp->pt_forw->pt_inhit->hit_dist) {
	GET_PT_INIT(rtip, pp, res);
	bu_ptbl_ins_unique(&pp->pt_seglist, (long *)segp);
	pp->pt_inseg = segp;
	pp->pt_inhit = &segp->seg_in;
	pp->pt_outseg = segp;
	pp->pt_outhit = &segp->seg_out;
	APPEND_PT(pp, PartHdp);
	if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("0-len segment ends before start of first partition.\n");
	return;
    }

    /*
     * Cases: seg at start of pt, in middle of pt, at end of pt, or
     * past end of pt but before start of next pt.
     *
     * XXX For the first 3 cases, we might want to make a new 0-len pt,
     * XXX especially as the NMG ray-tracer starts reporting wire hits.
     */
    for (pp=PartHdp->pt_forw; pp != PartHdp; pp=pp->pt_forw) {
	if (NEAR_EQUAL(segp->seg_in.hit_dist, pp->pt_inhit->hit_dist, tol_dist) ||
	    NEAR_EQUAL(segp->seg_out.hit_dist, pp->pt_inhit->hit_dist, tol_dist)
	    ) {
	    if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("0-len segment ends right at start of existing partition.\n");
	    return;
	}
	if (NEAR_EQUAL(segp->seg_in.hit_dist, pp->pt_outhit->hit_dist, tol_dist) ||
	    NEAR_EQUAL(segp->seg_out.hit_dist, pp->pt_outhit->hit_dist, tol_dist)
	    ) {
	    if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("0-len segment ends right at end of existing partition.\n");
	    return;
	}
	if (segp->seg_out.hit_dist <= pp->pt_outhit->hit_dist &&
	    segp->seg_in.hit_dist >= pp->pt_inhit->hit_dist) {
	    if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("0-len segment in the middle of existing partition.\n");
	    return;
	}
	if (pp->pt_forw == PartHdp ||
	    segp->seg_out.hit_dist < pp->pt_forw->pt_inhit->hit_dist) {
	    struct partition *npp;
	    if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("0-len segment after existing partition, but before next partition.\n");
	    GET_PT_INIT(rtip, npp, res);
	    bu_ptbl_ins_unique(&npp->pt_seglist, (long *)segp);
	    npp->pt_inseg = segp;
	    npp->pt_inhit = &segp->seg_in;
	    npp->pt_outseg = segp;
	    npp->pt_outhit = &segp->seg_out;
	    APPEND_PT(npp, pp);
	    return;
	}
    }
    bu_bomb("rt_weave0seg() fell out of partition loop?\n");
}


/**
 * R T _ B O O L W E A V E
 *
 * Weave a chain of segments into an existing set of partitions.  The
 * edge of each partition is an inhit or outhit of some solid (seg).
 *
 * NOTE: When the final partitions are completed, it is the users
 * responsibility to honor the inflip and outflip flags.  They can not
 * be flipped here because an outflip=1 edge and an inflip=0 edge
 * following it may in fact be the same edge.  This could be dealt
 * with by giving the partition struct a COPY of the inhit and outhit
 * rather than a pointer, but that's more cycles than the neatness is
 * worth.
 *
 * Inputs -
 * Pointer to first segment in seg chain.
 * Pointer to head of circular doubly-linked list of
 * partitions of the original ray.
 *
 * Outputs -
 * Partitions, queued on doubly-linked list specified.
 *
 * Notes -
 * It is the responsibility of the CALLER to free the seg chain, as
 * well as the partition list that we return.
 */
void
rt_boolweave(struct seg *out_hd, struct seg *in_hd, struct partition *PartHdp, struct application *ap)
{
    register struct seg *segp;
    register struct partition *pp;
    struct resource *res = ap->a_resource;
    struct rt_i *rtip = ap->a_rt_i;

    register fastf_t diff, diff_se;
    register fastf_t tol_dist;

    RT_CK_PT_HD(PartHdp);

    tol_dist = rtip->rti_tol.dist;

    RT_CK_RTI(ap->a_rt_i);
    RT_CK_RESOURCE(res);
    RT_CK_RTI(rtip);

    if (RT_G_DEBUG&DEBUG_PARTITION) {
	bu_log("In rt_boolweave, tol_dist = %g\n", tol_dist);
	rt_pr_partitions(rtip, PartHdp, "-----------------BOOL_WEAVE");
    }

    while (BU_LIST_NON_EMPTY(&(in_hd->l))) {
	register struct partition *newpp = PT_NULL;
	register struct seg *lastseg = RT_SEG_NULL;
	register struct hit *lasthit = HIT_NULL;
	int lastflip = 0;

	segp = BU_LIST_FIRST(seg, &(in_hd->l));
	RT_CHECK_SEG(segp);
	RT_CK_HIT(&(segp->seg_in));
	RT_CK_RAY(segp->seg_in.hit_rayp);
	RT_CK_HIT(&(segp->seg_out));
	RT_CK_RAY(segp->seg_out.hit_rayp);
	if (RT_G_DEBUG&DEBUG_PARTITION) {
	    point_t pt;

	    bu_log("************ Input segment:\n");
	    rt_pr_seg(segp);
	    rt_pr_hit(" In", &segp->seg_in);
	    VJOIN1(pt, ap->a_ray.r_pt, segp->seg_in.hit_dist, ap->a_ray.r_dir);
	    /* XXX needs indentation added here */
	    VPRINT(" IPoint", pt);

	    rt_pr_hit("Out", &segp->seg_out);
	    VJOIN1(pt, ap->a_ray.r_pt, segp->seg_out.hit_dist, ap->a_ray.r_dir);
	    /* XXX needs indentation added here */
	    VPRINT(" OPoint", pt);
	    bu_log("***********\n");
	}
	if ((size_t)segp->seg_stp->st_bit >= rtip->nsolids)
	    bu_bomb("rt_boolweave: st_bit");

	BU_LIST_DEQUEUE(&(segp->l));
	BU_LIST_INSERT(&(out_hd->l), &(segp->l));

	/* Make nearly zero be exactly zero */
	if (NEAR_ZERO(segp->seg_in.hit_dist, tol_dist))
	    segp->seg_in.hit_dist = 0;
	if (NEAR_ZERO(segp->seg_out.hit_dist, tol_dist))
	    segp->seg_out.hit_dist = 0;

	/* Totally ignore things behind the start position */
	if (segp->seg_out.hit_dist < -10.0)
	    continue;

	if (segp->seg_stp->st_aradius < INFINITY &&
	    !(segp->seg_in.hit_dist >= -INFINITY &&
	      segp->seg_out.hit_dist <= INFINITY)) {
	    bu_log("rt_boolweave:  Defective %s segment %s (%.18e, %.18e) %d, %d\n",
		   rt_functab[segp->seg_stp->st_id].ft_name,
		   segp->seg_stp->st_name,
		   segp->seg_in.hit_dist,
		   segp->seg_out.hit_dist,
		   segp->seg_in.hit_surfno,
		   segp->seg_out.hit_surfno);
	    continue;
	}
	if (segp->seg_in.hit_dist > segp->seg_out.hit_dist) {
	    bu_log("rt_boolweave:  Inside-out %s segment %s (%.18e, %.18e) %d, %d\n",
		   rt_functab[segp->seg_stp->st_id].ft_name,
		   segp->seg_stp->st_name,
		   segp->seg_in.hit_dist,
		   segp->seg_out.hit_dist,
		   segp->seg_in.hit_surfno,
		   segp->seg_out.hit_surfno);
	    continue;
	}

	/*
	 * Weave this segment into the existing partitions, creating
	 * new partitions as necessary.
	 */
	if (PartHdp->pt_forw == PartHdp) {
	    /* No partitions yet, simple! */
	    GET_PT_INIT(rtip, pp, res);
	    bu_ptbl_ins_unique(&pp->pt_seglist, (long *)segp);
	    pp->pt_inseg = segp;
	    pp->pt_inhit = &segp->seg_in;
	    pp->pt_outseg = segp;
	    pp->pt_outhit = &segp->seg_out;
	    APPEND_PT(pp, PartHdp);
	    if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("No partitions yet, segment forms first partition\n");
	    goto done_weave;
	}

	if (ap->a_no_booleans) {
	    lastseg = segp;
	    lasthit = &segp->seg_in;
	    lastflip = 0;
	    /* Just sort in ascending in-dist order */
	    for (pp=PartHdp->pt_forw; pp != PartHdp; pp=pp->pt_forw) {
		if (lasthit->hit_dist < pp->pt_inhit->hit_dist) {
		    if (RT_G_DEBUG&DEBUG_PARTITION) {
			bu_log("Insert nobool seg before next pt\n");
		    }
		    GET_PT_INIT(rtip, newpp, res);
		    bu_ptbl_ins_unique(&newpp->pt_seglist, (long *)segp);
		    newpp->pt_inseg = segp;
		    newpp->pt_inhit = &segp->seg_in;
		    newpp->pt_outseg = segp;
		    newpp->pt_outhit = &segp->seg_out;
		    INSERT_PT(newpp, pp);
		    goto done_weave;
		}
	    }
	    if (RT_G_DEBUG&DEBUG_PARTITION) {
		bu_log("Append nobool seg at end of list\n");
	    }
	    GET_PT_INIT(rtip, newpp, res);
	    bu_ptbl_ins_unique(&newpp->pt_seglist, (long *)segp);
	    newpp->pt_inseg = segp;
	    newpp->pt_inhit = &segp->seg_in;
	    newpp->pt_outseg = segp;
	    newpp->pt_outhit = &segp->seg_out;
	    INSERT_PT(newpp, PartHdp);
	    goto done_weave;
	}

	/* Check for zero-thickness segment, within tol */
	diff = segp->seg_in.hit_dist - segp->seg_out.hit_dist;
	if (NEAR_ZERO(diff, tol_dist)) {
	    rt_weave0seg(segp, PartHdp, ap);
	    goto done_weave;
	}

	if (segp->seg_in.hit_dist >= PartHdp->pt_back->pt_outhit->hit_dist) {
	    /*
	     * Segment starts exactly at last partition's end, or
	     * beyond last partitions end.  Make new partition.
	     */
	    if (RT_G_DEBUG&DEBUG_PARTITION) {
		bu_log("seg starts beyond last partition end. (%g, %g) Appending new partition\n",
		       PartHdp->pt_back->pt_inhit->hit_dist,
		       PartHdp->pt_back->pt_outhit->hit_dist);
	    }
	    GET_PT_INIT(rtip, pp, res);
	    bu_ptbl_ins_unique(&pp->pt_seglist, (long *)segp);
	    pp->pt_inseg = segp;
	    pp->pt_inhit = &segp->seg_in;
	    pp->pt_outseg = segp;
	    pp->pt_outhit = &segp->seg_out;
	    APPEND_PT(pp, PartHdp->pt_back);
	    goto done_weave;
	}

	/* Loop through current partition list weaving the current
	 * input segment into the list. The following three variables
	 * keep track of the current starting point of the input
	 * segment. The starting point of the segment moves to higher
	 * hit_dist values (as it is woven in) until it is entirely
	 * consumed.
	 */
	lastseg = segp;
	lasthit = &segp->seg_in;
	lastflip = 0;
	for (pp=PartHdp->pt_forw; pp != PartHdp; pp=pp->pt_forw) {

	    if (RT_G_DEBUG&DEBUG_PARTITION) {
		bu_log("At start of loop:\n");
		bu_log("	remaining input segment: (%.12e - %.12e)\n",
		       lasthit->hit_dist, segp->seg_out.hit_dist);
		bu_log("	current partition: (%.12e - %.12e)\n",
		       pp->pt_inhit->hit_dist, pp->pt_outhit->hit_dist);
		rt_pr_partitions(rtip, PartHdp, "At start of loop");
	    }

	    diff_se = lasthit->hit_dist - pp->pt_outhit->hit_dist;
	    if (diff_se > tol_dist) {
		/* Seg starts beyond the END of the
		 * current partition.
		 *	PPPP
		 *	        SSSS
		 * Advance to next partition.
		 */
		if (RT_G_DEBUG&DEBUG_PARTITION) {
		    bu_log("seg start beyond partition end, skipping.  (%g, %g)\n",
			   pp->pt_inhit->hit_dist,
			   pp->pt_outhit->hit_dist);
		}
		continue;
	    }
	    if (RT_G_DEBUG&DEBUG_PARTITION) rt_pr_pt(rtip, pp);
	    diff = lasthit->hit_dist - pp->pt_inhit->hit_dist;
	    if (diff_se > -(tol_dist) && diff > tol_dist) {
		/*
		 * Seg starts almost "precisely" at the
		 * end of the current partition.
		 *	PPPP
		 *	    SSSS
		 * FUSE an exact match of the endpoints,
		 * advance to next partition.
		 */
		lasthit->hit_dist = pp->pt_outhit->hit_dist;
		if (RT_G_DEBUG&DEBUG_PARTITION) {
		    bu_log("seg start fused to partition end, diff=%g\n", diff);
		}
		continue;
	    }

	    /*
	     * diff < ~~0
	     * Seg starts before current partition ends
	     *	PPPPPPPPPPP
	     *	  SSSS...
	     */
	    if (diff > tol_dist) {
		/*
		 * lasthit->hit_dist > pp->pt_inhit->hit_dist
		 * pp->pt_inhit->hit_dist < lasthit->hit_dist
		 *
		 * Segment starts after partition starts,
		 * but before the end of the partition.
		 * Note:  pt_seglist will be updated in equal_start.
		 *	PPPPPPPPPPPP
		 *	     SSSS...
		 *	newpp|pp
		 */
		RT_DUP_PT(rtip, newpp, pp, res);
		/* new partition is the span before seg joins partition */
		pp->pt_inseg = segp;
		pp->pt_inhit = &segp->seg_in;
		pp->pt_inflip = 0;
		newpp->pt_outseg = segp;
		newpp->pt_outhit = &segp->seg_in;
		newpp->pt_outflip = 1;
		INSERT_PT(newpp, pp);
		if (RT_G_DEBUG&DEBUG_PARTITION) {
		    bu_log("seg starts within p. Split p at seg start, advance. (diff = %g)\n", diff);
		    bu_log("newpp starts at %.12e, pp starts at %.12e\n",
			   newpp->pt_inhit->hit_dist,
			   pp->pt_inhit->hit_dist);
		    bu_log("newpp = %p, pp = %p\n", (void *)newpp, (void *)pp);
		}
		goto equal_start;
	    }
	    if (diff > -(tol_dist)) {
		/*
		 * Make a subtle but important distinction here.  Even
		 * though the two distances are "equal" within
		 * tolerance, they are not exactly the same.  If the
		 * new segment is slightly closer to the ray origin,
		 * then use its IN point.
		 *
		 * This is an attempt to reduce the deflected normals
		 * sometimes seen along the edges of e.g. a cylinder
		 * unioned with an ARB8, where the ray hits the top of
		 * the cylinder and the *side* face of the ARB8 rather
		 * than the top face of the ARB8.
		 */
		diff = segp->seg_in.hit_dist - pp->pt_inhit->hit_dist;
		if (!pp->pt_back ||
		    pp->pt_back == PartHdp ||
		    pp->pt_back->pt_outhit->hit_dist <=
		    segp->seg_in.hit_dist) {
		    if (NEAR_ZERO(diff, tol_dist) &&
			diff < 0) {
			if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("changing partition start point to segment start point\n");
			pp->pt_inseg = segp;
			pp->pt_inhit = &segp->seg_in;
			pp->pt_inflip = 0;
		    }
		}
	    equal_start:
		if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("equal_start\n");
		/*
		 * Segment and partition start at (roughly) the same
		 * point.  When fuseing 2 points together i.e., when
		 * NEAR_ZERO(diff, tol) is true, the two points MUST
		 * be forced to become exactly equal!
		 */
		diff = segp->seg_out.hit_dist - pp->pt_outhit->hit_dist;
		if (diff > tol_dist) {
		    /*
		     * Seg & partition start at roughly
		     * the same spot,
		     * seg extends beyond partition end.
		     *	PPPP
		     *	SSSSSSSS
		     *	pp  |  newpp
		     */
		    bu_ptbl_ins_unique(&pp->pt_seglist, (long *)segp);
		    lasthit = pp->pt_outhit;
		    lastseg = pp->pt_outseg;
		    lastflip = 1;
		    if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("seg spans partition and extends beyond it\n");
		    continue;
		}
		if (diff > -(tol_dist)) {
		    /*
		     * diff ~= 0
		     * Segment and partition start & end
		     * (nearly) together.
		     *	PPPP
		     *	SSSS
		     */
		    bu_ptbl_ins_unique(&pp->pt_seglist, (long *)segp);
		    if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("same start&end\n");
		    goto done_weave;
		} else {
		    /*
		     * diff < ~0
		     *
		     * Segment + Partition start together,
		     * segment ends before partition ends.
		     *	PPPPPPPPPP
		     *	SSSSSS
		     *	newpp| pp
		     */
		    RT_DUP_PT(rtip, newpp, pp, res);
		    /* new partition contains segment */
		    bu_ptbl_ins_unique(&newpp->pt_seglist, (long *)segp);
		    newpp->pt_outseg = segp;
		    newpp->pt_outhit = &segp->seg_out;
		    newpp->pt_outflip = 0;
		    pp->pt_inseg = segp;
		    pp->pt_inhit = &segp->seg_out;
		    pp->pt_inflip = 1;
		    INSERT_PT(newpp, pp);
		    if (RT_G_DEBUG&DEBUG_PARTITION) {
			bu_log("start together, seg shorter than partition\n");
			bu_log("newpp starts at %.12e, pp starts at %.12e\n",
			       newpp->pt_inhit->hit_dist,
			       pp->pt_inhit->hit_dist);
			bu_log("newpp = %p, pp = %p\n", (void *)newpp, (void *)pp);
		    }
		    goto done_weave;
		}
		/* NOTREACHED */
	    } else {
		/*
		 * diff < ~~0
		 *
		 * Seg starts before current partition starts,
		 * but after the previous partition ends.
		 *	SSSSSSSS...
		 *	     PPPPP...
		 *	newpp|pp
		 */
		GET_PT_INIT(rtip, newpp, res);
		bu_ptbl_ins_unique(&newpp->pt_seglist, (long *)segp);
		newpp->pt_inseg = lastseg;
		newpp->pt_inhit = lasthit;
		newpp->pt_inflip = lastflip;
		diff = segp->seg_out.hit_dist - pp->pt_inhit->hit_dist;
		if (diff < -(tol_dist)) {
		    /*
		     * diff < ~0
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
		    INSERT_PT(newpp, pp);
		    if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("seg between 2 partitions\n");
		    goto done_weave;
		}
		if (diff < tol_dist) {
		    /*
		     * diff ~= 0
		     *
		     * Seg starts before current
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
		    newpp->pt_outflip = 0;
		    INSERT_PT(newpp, pp);
		    if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("seg ends at partition start, fuse\n");
		    goto done_weave;
		}
		/*
		 * Seg starts before current partition
		 * starts, and ends after the start of the
		 * partition.  (diff > 0).
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
		INSERT_PT(newpp, pp);
		if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("insert seg before p start, ends after p ends.  Making new partition for initial portion.\n");
		goto equal_start;
	    }
	    /* NOTREACHED */
	}

	/*
	 * Segment has portion which extends beyond the end
	 * of the last partition.  Tack on the remainder.
	 *  	PPPPP
	 *  	     SSSSS
	 */
	if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("seg extends beyond partition end\n");
	GET_PT_INIT(rtip, newpp, res);
	bu_ptbl_ins_unique(&newpp->pt_seglist, (long *)segp);
	newpp->pt_inseg = lastseg;
	newpp->pt_inhit = lasthit;
	newpp->pt_inflip = lastflip;
	newpp->pt_outseg = segp;
	newpp->pt_outhit = &segp->seg_out;
	APPEND_PT(newpp, PartHdp->pt_back);

    done_weave:	; /* Sorry about the goto's, but they give clarity */
	if (RT_G_DEBUG&DEBUG_PARTITION)
	    rt_pr_partitions(rtip, PartHdp, "After weave");
    }
    if (RT_G_DEBUG&DEBUG_PARTITION)
	bu_log("--------------------Leaving Booleweave\n");
}


/**
 * R T _ D E F O V E R L A P
 *
 * Default handler for overlaps in rt_boolfinal().
 *
 * Returns -
 * 0 to eliminate partition with overlap entirely
 * 1 to retain partition in output list, claimed by reg1
 * 2 to retain partition in output list, claimed by reg2
 */
int
rt_defoverlap (register struct application *ap, register struct partition *pp, struct region *reg1, struct region *reg2, struct partition *pheadp)
{
    RT_CK_AP(ap);
    RT_CK_PT(pp);
    RT_CK_REGION(reg1);
    RT_CK_REGION(reg2);

    /*
     * Apply heuristics as to which region should claim partition.
     */
    if (reg1->reg_aircode != 0) {
	/* reg1 was air, replace with reg2 */
	return 2;
    }
    if (pp->pt_back != pheadp) {
	/* Repeat a prev region, if that is a choice */
	if (pp->pt_back->pt_regionp == reg1)
	    return 1;
	if (pp->pt_back->pt_regionp == reg2)
	    return 2;
    }

    /* To provide some consistency from ray to ray, use lowest bit # */
    if (reg1->reg_bit < reg2->reg_bit)
	return 1;
    return 2;
}


/**
 * R T _ G E T _ R E G I O N _ S E G L I S T _ F O R _ P A R T I T I O N
 *
 * Given one of the regions that is involved in a given partition
 * (whether the boolean formula for this region is BOOL_TRUE in this
 * part or not), return a bu_ptbl list containing all the segments in
 * this partition which came from solids that appear as terms in the
 * boolean formula for the given region.
 *
 * The bu_ptbl is initialized here, and must be freed by the caller.
 * It will contain a pointer to at least one segment.
 */
void
rt_get_region_seglist_for_partition(struct bu_ptbl *sl, const struct partition *pp, const struct region *regp)
{
    const struct seg **segpp;

    bu_ptbl_init(sl, 8, "region seglist for partition");

    /* Walk the partitions segment list */
    for (BU_PTBL_FOR(segpp, (const struct seg **), &pp->pt_seglist)) {
	const struct region **srpp;

	RT_CK_SEG(*segpp);
	/* For every segment in part, walk the solid's region list */
	for (BU_PTBL_FOR(srpp, (const struct region **), &(*segpp)->seg_stp->st_regions)) {
	    RT_CK_REGION(*srpp);

	    if (*srpp != regp) continue;
	    /* This segment is part of a solid in this region */
	    bu_ptbl_ins_unique(sl, (long *)(*segpp));
	}
    }

    if (BU_PTBL_LEN(sl) <= 0) bu_bomb("rt_get_region_seglist_for_partition() didn't find any segments\n");
}


/**
 * R T _ T R E E _ M A X _ R A Y N U M
 *
 * Find the maximum value of the raynum (seg_rayp->index) encountered
 * in the segments contributing to this region.
 *
 * Returns -
 *  # Maximum ray index
 * -1 If no rays are contributing segs for this region.
 */
int
rt_tree_max_raynum(register const union tree *tp, register const struct partition *pp)
{
    RT_CK_TREE(tp);
    RT_CK_PARTITION(pp);

    switch (tp->tr_op) {
	case OP_NOP:
	    return -1;

	case OP_SOLID:
	    {
		register struct soltab *seek_stp = tp->tr_a.tu_stp;
		register struct seg **segpp;
		for (BU_PTBL_FOR(segpp, (struct seg **), &pp->pt_seglist)) {
		    if ((*segpp)->seg_stp != seek_stp) continue;
		    return (*segpp)->seg_in.hit_rayp->index;
		}
	    }
	    /* Maybe it hasn't been shot yet, or ray missed */
	    return -1;

	case OP_NOT:
	    return rt_tree_max_raynum(tp->tr_b.tb_left, pp);

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    {
		int a = rt_tree_max_raynum(tp->tr_b.tb_left, pp);
		int b = rt_tree_max_raynum(tp->tr_b.tb_right, pp);
		if (a > b) return a;
		return b;
	    }
	default:
	    bu_bomb("rt_tree_max_raynum: bad op\n");
    }
    return 0;
}


/**
 * R T _ F A S T G E N _ V O L _ V O L _ O V E R L A P
 *
 * Handle FASTGEN volume/volume overlap.  Look at underlying segs.  If
 * one is less than 1/4", take the longer.  Otherwise take the
 * shorter.
 *
 * Required to null out one of the two regions.
 */
void
rt_fastgen_vol_vol_overlap(struct region **fr1, struct region **fr2, const struct partition *pp)
{
    struct bu_ptbl sl1, sl2;
    const struct seg *s1 = (const struct seg *)NULL;
    const struct seg *s2 = (const struct seg *)NULL;
    fastf_t s1_in_dist;
    fastf_t s2_in_dist;
    fastf_t depth;
    struct seg **segpp;

    RT_CK_REGION(*fr1);
    RT_CK_REGION(*fr2);

    if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("Resolving FASTGEN volume/volume overlap: %s %s\n", (*fr1)->reg_name, (*fr2)->reg_name);

    rt_get_region_seglist_for_partition(&sl1, pp, *fr1);
    rt_get_region_seglist_for_partition(&sl2, pp, *fr2);

    s1_in_dist = MAX_FASTF;
    s2_in_dist = MAX_FASTF;
    for (BU_PTBL_FOR(segpp, (struct seg **), &sl1))
    {
	if ((*segpp)->seg_in.hit_dist < s1_in_dist)
	{
	    s1 = (*segpp);
	    s1_in_dist = s1->seg_in.hit_dist;
	}
    }
    for (BU_PTBL_FOR(segpp, (struct seg **), &sl2))
    {
	if ((*segpp)->seg_in.hit_dist < s2_in_dist)
	{
	    s2 = (*segpp);
	    s2_in_dist = s2->seg_in.hit_dist;
	}
    }
    RT_CK_SEG(s1);
    RT_CK_SEG(s2);

    depth = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;

    /* 6.35mm = 1/4 inch */
    if (depth < 6.35) {
	/* take region with lowest inhit */
	if (s1->seg_in.hit_dist < s2->seg_in.hit_dist) {
	    /* keep fr1, delete fr2 */
	    *fr2 = REGION_NULL;
	} else {
	    /* keep fr2, depete fr1 */
	    *fr1 = REGION_NULL;
	}
    } else {
	/*
	 * take the region with largest inhit
	 */
	if (s1->seg_in.hit_dist >= s2->seg_in.hit_dist) {
	    /* keep fr1, delete fr2 */
	    *fr2 = REGION_NULL;
	} else {
	    *fr1 = REGION_NULL;
	}
    }

    bu_ptbl_free(&sl1);
    bu_ptbl_free(&sl2);
}


/**
 * R T _ F D I F F
 *
 * Compares two floating point numbers.  If they are within "epsilon"
 * of each other, they are considered the same.
 *
 * NOTE: This is a "fuzzy" difference.  It is important NOT to use the
 * results of this function in compound comparisons, because a return
 * of 0 makes no statement about the PRECISE relationships between the
 * two numbers.  Eg, * if (rt_fdiff(a, b) <= 0) is poison!
 *
 * Returns -
 * -1 if a < b
 *  0 if a ~= b
 * +1 if a > b
 *
 * DEVELOPER DEPRECATION NOTICE: use NEAR_ZERO instead.
 */
int
rt_fdiff(double a, double b)
{
    register double diff;
    register double d;
    register int ret;

    /* d = Max(Abs(a), Abs(b)) */
    d = (a >= 0.0) ? a : -a;
    if (b >= 0.0) {
	if (b > d) d = b;
    } else {
	if ((-b) > d) d = (-b);
    }
    if (d <= 1.0e-6) {
	ret = 0;	/* both nearly zero */
	goto out;
    }
    if (d >= INFINITY) {
	if (ZERO(a - b)) {
	    ret = 0;
	    goto out;
	}
	if (a < b) {
	    ret = -1;
	    goto out;
	}
	ret = 1;
	goto out;
    }
    if ((diff = a - b) < 0.0) diff = -diff;
    if (diff < 0.001) {
	ret = 0;	/* absolute difference is small, < 1/1000mm */
	goto out;
    }
    if (diff < 0.000001 * d) {
	ret = 0;	/* relative difference is small, < 1ppm */
	goto out;
    }
    if (a < b) {
	ret = -1;
	goto out;
    }
    ret = 1;
out:
    if (RT_G_DEBUG&DEBUG_FDIFF) bu_log("rt_fdiff(%.18e, %.18e)=%d\n", a, b, ret);
    return ret;
}


/**
 * R T _ F A S T G E N _ P L A T E _ V O L _ O V E R L A P
 *
 * Handle FASTGEN plate/volume overlap.
 *
 * Measure width of _preceeding_ partition, which must have been
 * claimed by the volume mode region fr2.
 *
 * If less than 1/4", delete preceeding partition, and plate wins this
 * part.  If less than 1/4", plate wins this part, previous partition
 * untouched.  If previous partition is claimed by plate mode region
 * fr1, then overlap is left in output???
 *
 * Required to null out one of the two regions.
 */
void
rt_fastgen_plate_vol_overlap(struct region **fr1, struct region **fr2, struct partition *pp, struct application *ap)
{
    struct partition *prev;
    fastf_t depth;

    RT_CK_REGION(*fr1);
    RT_CK_REGION(*fr2);
    RT_CK_PT(pp);
    RT_CK_AP(ap);

    if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("Resolving FASTGEN plate/volume overlap: %s %s\n", (*fr1)->reg_name, (*fr2)->reg_name);

    prev = pp->pt_back;
    if (prev->pt_magic == PT_HD_MAGIC) {
	/* No prev partition, this is the first.  d=0, plate wins */
	*fr2 = REGION_NULL;
	return;
    }

    /* arbitrary tolerance is the dominant absolute tolerance from f_diff() */
    if (!NEAR_EQUAL(prev->pt_outhit->hit_dist, pp->pt_inhit->hit_dist, 0.001)) {
	/* There is a gap between previous partition and this one.  So
	 * both plate and vol start at same place, d=0, plate wins.
	 */
	*fr2 = REGION_NULL;
	return;
    }

    if (prev->pt_regionp == *fr2) {
	/* previous part is volume mode, ends at start of pp */
	depth = prev->pt_outhit->hit_dist - prev->pt_inhit->hit_dist;
	/* 6.35mm = 1/4 inch */
	if (depth < 6.35) {
	    /* Delete previous partition from list */
	    DEQUEUE_PT(prev);
	    FREE_PT(prev, ap->a_resource);

	    /* plate mode fr1 wins this partition */
	    *fr2 = REGION_NULL;
	} else {
	    /* Leave previous partition alone */
	    /* plate mode fr1 wins this partition */
	    *fr2 = REGION_NULL;
	}
    } else if (prev->pt_regionp == *fr1) {
	/* previous part is plate mode, ends at start of pp */
	/* d < 0, leave overlap in output??? */
	/* For now, volume mode region just loses. */
	*fr2 = REGION_NULL;
    } else {
	/* Some other region preceeds this partition */
	/* So both plate and vol start at same place, d=0, plate wins */
	*fr2 = REGION_NULL;
    }
}


/**
 * R T _ D E F A U L T _ M U L T I O V E R L A P
 *
 * Default version of a_multioverlap().
 *
 * Resolve the overlap of multiple regions withing a single partition.
 * There are no null pointers in the table (they have been compressed
 * out by our caller).  Consider BU_PTBL_LEN(regiontable) overlapping
 * regions, and reduce to zero or one "claiming" regions, by setting
 * pointers in the bu_ptbl of non-claiming regions to NULL.
 *
 * This default routine reproduces the behavior of BRL-CAD Release 5.0
 * by considering the regions pairwise and calling the old
 * a_overlap().
 *
 * An application which knew how to handle multiple overlapping air
 * regions would provide its own very different version of this
 * routine as the a_multioverlap() handler.
 *
 * This routine is for resolving overlaps only, and should not print
 * any messages in normal operation; a_logoverlap() is for logging.
 *
 * InputHdp is the list of partitions up to this point.  It allows us
 * to look at the regions that have come before in deciding what to do
 */
void
rt_default_multioverlap(struct application *ap, struct partition *pp, struct bu_ptbl *regiontable, struct partition *InputHdp)
{
    struct region *lastregion = (struct region *)NULL;
    int n_regions;
    int n_fastgen = 0;
    int code;
    size_t i;
    int counter;

    RT_CK_AP(ap);
    RT_CK_PARTITION(pp);
    BU_CK_PTBL(regiontable);
    RT_CK_PT_HD(InputHdp);

    if (ap->a_overlap == RT_AFN_NULL)
	ap->a_overlap = rt_defoverlap;

    /* Count number of FASTGEN regions */
    n_regions = BU_PTBL_LEN(regiontable);
    for (counter = n_regions-1; counter >= 0; counter--) {
	struct region *regp = (struct region *)BU_PTBL_GET(regiontable, counter);
	RT_CK_REGION(regp);
	if (regp->reg_is_fastgen != REGION_NON_FASTGEN) n_fastgen++;
    }

    /*
     * Resolve all FASTGEN overlaps before considering BRL-CAD
     * overlaps, because FASTGEN overlaps have strict rules.
     */
    if (n_fastgen >= 2) {
	struct region **fr1;
	struct region **fr2;

	if (RT_G_DEBUG&DEBUG_PARTITION) {
	    bu_log("I see %d FASTGEN overlaps in this partition\n", n_fastgen);
	    for (BU_PTBL_FOR(fr1, (struct region **), regiontable)) {
		if (*fr1 == REGION_NULL) continue;
		rt_pr_region(*fr1);
	    }
	}

	/*
	 * First, resolve volume_mode/volume_mode overlaps because
	 * they are a simple choice.  The searches run from high to
	 * low in the ptbl array.
	 */
	for (BU_PTBL_FOR(fr1, (struct region **), regiontable)) {
	    if (*fr1 == REGION_NULL) continue;
	    RT_CK_REGION(*fr1);
	    if ((*fr1)->reg_is_fastgen != REGION_FASTGEN_VOLUME)
		continue;
	    for (fr2 = fr1-1; fr2 >= (struct region **)BU_PTBL_BASEADDR(regiontable); fr2--) {
		if (*fr2 == REGION_NULL) continue;
		RT_CK_REGION(*fr2);
		if ((*fr2)->reg_is_fastgen != REGION_FASTGEN_VOLUME)
		    continue;
		rt_fastgen_vol_vol_overlap(fr1, fr2, pp);
		if (*fr1 == REGION_NULL) break;
	    }
	}

	/* Second, resolve plate_mode/volume_mode overlaps */
	for (BU_PTBL_FOR(fr1, (struct region **), regiontable)) {
	    if (*fr1 == REGION_NULL) continue;
	    RT_CK_REGION(*fr1);
	    if ((*fr1)->reg_is_fastgen != REGION_FASTGEN_PLATE)
		continue;
	    for (BU_PTBL_FOR(fr2, (struct region **), regiontable)) {
		if (*fr2 == REGION_NULL) continue;
		RT_CK_REGION(*fr2);
		if ((*fr2)->reg_is_fastgen != REGION_FASTGEN_VOLUME)
		    continue;
		rt_fastgen_plate_vol_overlap(fr1, fr2, pp, ap);
		if (*fr1 == REGION_NULL) break;
	    }
	}


	/* Finally, think up a way to pass plate/plate overlaps on */
	n_fastgen = 0;
	for (counter = n_regions-1; counter >= 0; counter--) {
	    struct region *regp = (struct region *)BU_PTBL_GET(regiontable, counter);
	    if (regp == REGION_NULL) continue;	/* empty slot in table */
	    RT_CK_REGION(regp);
	    if (regp->reg_is_fastgen != REGION_NON_FASTGEN) n_fastgen++;
	}

	/* Compress out any null pointers in the table */
	bu_ptbl_rm(regiontable, (long *)NULL);
    }

    lastregion = (struct region *)BU_PTBL_GET(regiontable, 0);
    RT_CK_REGION(lastregion);

    if (BU_PTBL_LEN(regiontable) > 1 && ap->a_rt_i->rti_save_overlaps != 0) {
	/*
	 * Snapshot current state of overlap list, so that downstream
	 * application code can resolve any FASTGEN plate/plate
	 * overlaps.  The snapshot is not taken at the top of the
	 * routine because nobody is interested in FASTGEN vol/plate
	 * or vol/vol overlaps.  The list is terminated with a NULL
	 * pointer, placed courtesy of bu_calloc().
	 */
	pp->pt_overlap_reg = (struct region **)bu_calloc(
	    BU_PTBL_LEN(regiontable)+1, sizeof(struct region *),
	    "pt_overlap_reg");
	memcpy((char *)pp->pt_overlap_reg,
	       (char *)BU_PTBL_BASEADDR(regiontable),
	       BU_PTBL_LEN(regiontable) * sizeof(struct region *));
    }

    /* Examine the overlapping regions, pairwise */
    for (i=1; i < BU_PTBL_LEN(regiontable); i++) {
	struct region *regp = (struct region *)BU_PTBL_GET(regiontable, i);
	if (regp == REGION_NULL) continue;	/* empty slot in table */
	RT_CK_REGION(regp);

	code = -1;				/* For debug out in policy */

	/*
	 * Two or more regions claim this partition
	 */
	if (lastregion->reg_aircode != 0 && regp->reg_aircode == 0) {
	    /* last region is air, replace with solid regp */
	    goto code2;
	} else if (lastregion->reg_aircode == 0 && regp->reg_aircode != 0) {
	    /* last region solid, regp is air, keep last */
	    goto code1;
	} else if (lastregion->reg_aircode != 0 &&
		   regp->reg_aircode != 0 &&
		   regp->reg_aircode == lastregion->reg_aircode) {
	    /* both are same air, keep last */
	    goto code1;
	}

	/*
	 * If a FASTGEN region overlaps a non-FASTGEN region, the
	 * non-FASTGEN ("traditional BRL-CAD") region wins.  No
	 * mixed-mode geometry like this will be built by the
	 * fastgen-to-BRL-CAD converters, only by human editors.
	 */
	if (lastregion->reg_is_fastgen != regp->reg_is_fastgen) {
	    if (lastregion->reg_is_fastgen)
		goto code2;		/* keep regp */
	    if (regp->reg_is_fastgen)
		goto code1;		/* keep lastregion */
	}

	/*
	 * To support ray bundles, find partition with the lower
	 * contributing ray number (closer to center of bundle), and
	 * retain that one.
	 */
	{
	    int r1 = rt_tree_max_raynum(lastregion->reg_treetop, pp);
	    int r2 = rt_tree_max_raynum(regp->reg_treetop, pp);

	    /* Only use this algorithm if one is not the main ray */
	    if (r1 > 0 || r2 > 0) {
		if (RT_G_DEBUG&DEBUG_PARTITION) {
		    bu_log("Potential overlay along ray bundle: r1=%d, r2=%d, resolved to %s\n", r1, r2,
			   (r1<r2)?lastregion->reg_name:regp->reg_name);
		}
		if (r1 < r2) {
		    goto code1;	/* keep lastregion */
		}
		goto code2;		/* keep regp */
	    }
	}

	/*
	 * Hand overlap to old-style application-specific
	 * overlap handler, or default.
	 * 0 = destroy partition,
	 * 1 = keep part, claiming region=lastregion
	 * 2 = keep part, claiming region=regp
	 */
	code = ap->a_overlap(ap, pp, lastregion, regp, InputHdp);

	/* Implement the policy in "code" */
	if (code == 0) {
	    /*
	     * Destroy the whole partition.
	     */
	    if (RT_G_DEBUG&DEBUG_PARTITION)
		bu_log("rt_default_multioverlap:  overlap code=0, partition=%p deleted\n", (void *)pp);
	    bu_ptbl_reset(regiontable);
	    return;
	} else if (code == 1) {
	code1:
	    /* Keep partition, claiming region = lastregion */
	    if (RT_G_DEBUG&DEBUG_PARTITION)
		bu_log("rt_default_multioverlap:  overlap policy=1, code=%d, p retained in region=%s\n",
		       code, lastregion->reg_name);
	    BU_PTBL_CLEAR_I(regiontable, i);
	} else {
	code2:
	    /* Keep partition, claiming region = regp */
	    bu_ptbl_zero(regiontable, (long *)lastregion);
	    lastregion = regp;
	    if (RT_G_DEBUG&DEBUG_PARTITION)
		bu_log("rt_default_multioverlap:  overlap policy!=(0, 1) code=%d, p retained in region=%s\n",
		       code, lastregion->reg_name);
	}
    }
}


/**
 * R T _ S I L E N T _ L O G O V E R L A P
 *
 * If an application doesn't want any logging from LIBRT, it should
 * just set ap->a_logoverlap = rt_silent_logoverlap.
 */
void
rt_silent_logoverlap(struct application *ap, const struct partition *pp, const struct bu_ptbl *regiontable, const struct partition *UNUSED(InputHdp))
{
    RT_CK_AP(ap);
    RT_CK_PT(pp);
    BU_CK_PTBL(regiontable);
    return;
}


/**
 * R T _ D E F A U L T _ L O G O V E R L A P
 *
 * Log a multiplicity of overlaps within a single partition.  This
 * function is intended for logging only, and a_multioverlap() is
 * intended for resolving the overlap, only.  This function can be
 * replaced by an application setting a_logoverlap().
 */
void
rt_default_logoverlap(struct application *ap, const struct partition *pp, const struct bu_ptbl *regiontable, const struct partition *UNUSED(InputHdp))
{
    point_t pt;
    static long count = 0; /* Not PARALLEL, shouldn't hurt */
    register fastf_t depth;
    size_t i;
    struct bu_vls str;

    RT_CK_AP(ap);
    RT_CK_PT(pp);
    BU_CK_PTBL(regiontable);

    /* Attempt to control tremendous error outputs */
    if (++count > 100) {
	if ((count%100) != 3) return;
	bu_log("(overlaps omitted)\n");
    }

    /*
     * Print all verbiage in one call to bu_log(),
     * so that messages will be grouped together in parallel runs.
     */
    bu_vls_init(&str);
    bu_vls_extend(&str, 80*8);
    bu_vls_putc(&str, '\n');

    /* List all the regions which evaluated to BOOL_TRUE in this partition */
    for (i=0; i < BU_PTBL_LEN(regiontable); i++) {
	struct region *regp = (struct region *)BU_PTBL_GET(regiontable, i);

	if (regp == REGION_NULL) continue;
	RT_CK_REGION(regp);

	bu_vls_printf(&str, "OVERLAP%zu: %s\n", i+1, regp->reg_name);
    }

    /* List all the information common to this whole partition */
    bu_vls_printf(&str, "OVERLAPa: dist=(%g, %g) isol=%s osol=%s\n",
		  pp->pt_inhit->hit_dist, pp->pt_outhit->hit_dist,
		  pp->pt_inseg->seg_stp->st_name,
		  pp->pt_outseg->seg_stp->st_name);

    depth = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
    VJOIN1(pt, ap->a_ray.r_pt, pp->pt_inhit->hit_dist,
	   ap->a_ray.r_dir);

    bu_vls_printf(&str, "OVERLAPb: depth %.5fmm at (%g, %g, %g) x%d y%d lvl%d\n",
		  depth, pt[X], pt[Y], pt[Z],
		  ap->a_x, ap->a_y, ap->a_level);

    bu_log("%s", bu_vls_addr(&str));
    bu_vls_free(&str);
}


/**
 * R T _ O V E R L A P _ T A B L E S _ E Q U A L
 *
 * Overlap tables are NULL terminated arrays of region pointers.  The
 * order of entries may be different between the two.
 *
 * Returns -
 * 1 The tables match
 * 0 The tables do not match
 */
int
rt_overlap_tables_equal(struct region *const*a, struct region *const*b)
{
    int alen=0, blen=0;
    register struct region *const*app;
    register struct region *const*bpp;

    if (a == NULL && b == NULL)
	return 1;

    if (a == NULL || b == NULL)
	return 0;

    /* First step, compare lengths */
    for (app = a; *app != NULL; app++) alen++;
    for (bpp = b; *bpp != NULL; bpp++) blen++;
    if (alen != blen) return 0;

    /* Second step, compare contents */
    for (app = a; *app != NULL; app++) {
	register const struct region *t = *app;
	for (bpp = b; *bpp != NULL; bpp++) {
	    if (*bpp == t) goto b_ok;
	}
	/* 't' not found in b table, no match */
	return 0;
    b_ok:		;
    }
    /* Everything matches */
    return 1;
}


/**
 * R T _ T R E E _ T E S T _ R E A D Y
 *
 * Test to see if a region is ready to be evaluated over a given
 * partition, i.e. if all the prerequisites have been satisfied.
 *
 * Returns -
 * !0 Region is ready for (correct) evaluation.
 *  0 Region is not ready
 */
int
rt_tree_test_ready(register const union tree *tp, register const struct bu_bitv *solidbits, register const struct region *regionp, register const struct partition *pp)
{
    RT_CK_TREE(tp);
    BU_CK_BITV(solidbits);
    RT_CK_REGION(regionp);
    RT_CK_PT(pp);

    switch (tp->tr_op) {
	case OP_NOP:
	    return 1;

	case OP_SOLID:
	    if (BU_BITTEST(solidbits, tp->tr_a.tu_stp->st_bit)) {
		/* This solid's been shot, segs are valid. */
		return 1;
	    }

	    /*
	     * This solid has not been shot yet.
	     */
	    return 0;

	case OP_NOT:
	    return !rt_tree_test_ready(tp->tr_b.tb_left, solidbits, regionp, pp);

	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    if (!rt_tree_test_ready(tp->tr_b.tb_left, solidbits, regionp, pp))
		return 0;
	    return rt_tree_test_ready(tp->tr_b.tb_right, solidbits, regionp, pp);

	default:
	    bu_bomb("rt_tree_test_ready: bad op\n");
    }
    return 0;
}


/**
 * R T _ B O O L _ P A R T I T I O N _ E L I G I B L E
 *
 * If every solid in every region participating in this ray-partition
 * has already been intersected with the ray, then this partition can
 * be safely evaluated.
 *
 * Returns -
 * !0 Partition is ready for (correct) evaluation.
 *  0 Partition is not ready
 */
int
rt_bool_partition_eligible(register const struct bu_ptbl *regiontable, register const struct bu_bitv *solidbits, register const struct partition *pp)
{
    struct region **regpp;

    BU_CK_PTBL(regiontable);
    BU_CK_BITV(solidbits);
    RT_CK_PT(pp);

    for (BU_PTBL_FOR(regpp, (struct region **), regiontable)) {
	register struct region *regp;

	regp = *regpp;
	RT_CK_REGION(regp);

	/* Check region prerequisites */
	if (!rt_tree_test_ready(regp->reg_treetop, solidbits, regp, pp)) {
	    return 0;
	}
    }
    return 1;
}


/**
 * R T _ G R O W _ B O O L S T A C K
 *
 * Increase the size of re_boolstack to double the previous size.
 * Depend on bu_realloc() to copy the previous data to the new area
 * when the size is increased.
 *
 * Return the new pointer for what was previously the last element.
 */
void
rt_grow_boolstack(register struct resource *resp)
{
    if (resp->re_boolstack == (union tree **)0 || resp->re_boolslen <= 0) {
	resp->re_boolslen = 128;	/* default len */
	resp->re_boolstack = (union tree **)bu_malloc(sizeof(union tree *) * resp->re_boolslen,	"initial boolstack");
    } else {
	resp->re_boolslen <<= 1;
	resp->re_boolstack = (union tree **)bu_realloc((char *)resp->re_boolstack, sizeof(union tree *) * resp->re_boolslen, "extend boolstack");
    }
}


/**
 * R T _ B O O L E V A L
 *
 * Using a stack to recall state, evaluate a boolean expression
 * without recursion.
 *
 * For use with XOR, a pointer to the "first valid subtree" would
 * be a useful addition, for rt_boolregions().
 *
 * Returns -
 * !0 tree is BOOL_TRUE
 *  0 tree is BOOL_FALSE
 * -1 tree is in error (GUARD)
 */
int
rt_booleval(register union tree *treep, struct partition *partp, struct region **trueregp, struct resource *resp)
/* Tree to evaluate */
/* Partition to evaluate */
/* XOR true (and overlap) return */
/* resource pointer for this CPU */
{
    static union tree tree_not[MAX_PSW];	/* for OP_NOT nodes */
    static union tree tree_guard[MAX_PSW];	/* for OP_GUARD nodes */
    static union tree tree_xnop[MAX_PSW];	/* for OP_XNOP nodes */
    register union tree **sp;
    register int ret;
    register union tree **stackend;

    RT_CK_TREE(treep);
    RT_CK_PT(partp);
    RT_CK_RESOURCE(resp);
    if (treep->tr_op != OP_XOR)
	trueregp[0] = treep->tr_regionp;
    else
	trueregp[0] = trueregp[1] = REGION_NULL;
    while ((sp = resp->re_boolstack) == (union tree **)0)
	rt_grow_boolstack(resp);
    stackend = &(resp->re_boolstack[resp->re_boolslen]);
    *sp++ = TREE_NULL;
stack:
    switch (treep->tr_op) {
	case OP_NOP:
	    ret = 0;
	    goto pop;
	case OP_SOLID:
	    {
		register struct soltab *seek_stp = treep->tr_a.tu_stp;
		register struct seg **segpp;
		for (BU_PTBL_FOR(segpp, (struct seg **), &partp->pt_seglist)) {
		    if ((*segpp)->seg_stp == seek_stp) {
			ret = 1;
			goto pop;
		    }
		}
		ret = 0;
	    }
	    goto pop;
	case OP_UNION:
	case OP_INTERSECT:
	case OP_SUBTRACT:
	case OP_XOR:
	    *sp++ = treep;
	    if (sp >= stackend) {
		register int off = sp - resp->re_boolstack;
		rt_grow_boolstack(resp);
		sp = &(resp->re_boolstack[off]);
		stackend = &(resp->re_boolstack[resp->re_boolslen]);
	    }
	    treep = treep->tr_b.tb_left;
	    goto stack;
	default:
	    bu_log("rt_booleval:  bad stack op [%d]\n", treep->tr_op);
	    return BOOL_TRUE;	/* screw up output */
    }
pop:
    if ((treep = *--sp) == TREE_NULL)
	return ret;		/* top of tree again */
    /*
     * Here, each operation will look at the operation just completed
     * (the left branch of the tree generally), and rewrite the top of
     * the stack and/or branch accordingly.
     */
    switch (treep->tr_op) {
	case OP_SOLID:
	    bu_log("rt_booleval:  pop SOLID?\n");
	    return BOOL_TRUE;	/* screw up output */
	case OP_UNION:
	    if (ret) goto pop;	/* BOOL_TRUE, we are done */
	    /* lhs was false, rewrite as rhs tree */
	    treep = treep->tr_b.tb_right;
	    goto stack;
	case OP_INTERSECT:
	    if (!ret) {
		ret = BOOL_FALSE;
		goto pop;
	    }
	    /* lhs was true, rewrite as rhs tree */
	    treep = treep->tr_b.tb_right;
	    goto stack;
	case OP_SUBTRACT:
	    if (!ret) goto pop;	/* BOOL_FALSE, we are done */
	    /* lhs was true, rewrite as NOT of rhs tree */
	    /* We introduce the special NOT operator here */
	    tree_not[resp->re_cpu].tr_op = OP_NOT;
	    *sp++ = &tree_not[resp->re_cpu];
	    treep = treep->tr_b.tb_right;
	    goto stack;
	case OP_NOT:
	    /* Special operation for subtraction */
	    ret = !ret;
	    goto pop;
	case OP_XOR:
	    if (ret) {
		/* lhs was true, rhs better not be, or we have an
		 * overlap condition.  Rewrite as guard node followed
		 * by rhs.
		 */
		if (treep->tr_b.tb_left->tr_regionp)
		    trueregp[0] = treep->tr_b.tb_left->tr_regionp;
		tree_guard[resp->re_cpu].tr_op = OP_GUARD;
		treep = treep->tr_b.tb_right;
		*sp++ = treep;		/* temp val for guard node */
		*sp++ = &tree_guard[resp->re_cpu];
	    } else {
		/* lhs was false, rewrite as xnop node and result of
		 * rhs.
		 */
		tree_xnop[resp->re_cpu].tr_op = OP_XNOP;
		treep = treep->tr_b.tb_right;
		*sp++ = treep;		/* temp val for xnop */
		*sp++ = &tree_xnop[resp->re_cpu];
	    }
	    goto stack;
	case OP_GUARD:
	    /*
	     * Special operation for XOR.  lhs was true.  If rhs
	     * subtree was true, an overlap condition exists (both
	     * sides of the XOR are BOOL_TRUE).  Return error
	     * condition.  If subtree is false, then return BOOL_TRUE
	     * (from lhs).
	     */
	    if (ret) {
		/* stacked temp val: rhs */
		if (sp[-1]->tr_regionp)
		    trueregp[1] = sp[-1]->tr_regionp;
		return -1;	/* GUARD error */
	    }
	    ret = BOOL_TRUE;
	    sp--;			/* pop temp val */
	    goto pop;
	case OP_XNOP:
	    /*
	     * Special NOP for XOR.  lhs was false.  If rhs is true,
	     * take note of its regionp.
	     */
	    sp--;			/* pop temp val */
	    if (ret) {
		if ((*sp)->tr_regionp)
		    trueregp[0] = (*sp)->tr_regionp;
	    }
	    goto pop;
	default:
	    bu_log("rt_booleval:  bad pop op [%d]\n", treep->tr_op);
	    return BOOL_TRUE;	/* screw up output */
    }
    /* NOTREACHED */
}


/**
 * R T _ B O O L F I N A L
 *
 * Consider each partition on the sorted & woven input partition list.
 * If the partition ends before this box's start, discard it
 * immediately.  If the partition begins beyond this box's end,
 * return.
 *
 * Next, evaluate the boolean expression tree for all regions that
 * have some presence in the partition.
 *
 * If 0 regions result, continue with next partition.
 *
 * If 1 region results, a valid hit has occured, so transfer the
 * partition from the Input list to the Final list.
 *
 * If 2 or more regions claim the partition, then an overlap exists.
 *
 * If the overlap handler gives a non-zero return, then the
 * overlapping partition is kept, with the region ID being the first
 * one encountered.
 *
 * Otherwise, the partition is eliminated from further consideration.
 *
 * All partitions in the indicated range of the ray are evaluated.
 * All partitions which really exist (booleval is true) are appended
 * to the Final partition list.  All partitions on the Final partition
 * list have completely valid entry and exit information, except for
 * the last partition's exit information when a_onehit!=0 and a_onehit
 * is odd.
 *
 * The flag a_onehit is interpreted as follows:
 *
 * If a_onehit = 0, then the ray is traced to +infinity, and all hit
 * points in the final partition list are valid.
 *
 * If a_onehit != 0, the ray is traced through a_onehit hit points.
 * (Recall that each partition has 2 hit points, entry and exit).
 * Thus, if a_onehit is odd, the value of pt_outhit.hit_dist in the
 * last partition may be incorrect; this should not mater because the
 * application specifically said it only wanted pt_inhit there.  This
 * is most commonly seen when a_onehit = 1, which is useful for
 * lighting models.  Not having to correctly determine the exit point
 * can result in a significant savings of computer time.
 *
 * If a_onehit is negative, it indicates the number of non-air hits
 * needed.
 *
 * Returns -
 * 0 If more partitions need to be done
 * 1 Requested number of hits are available in FinalHdp
 *
 * The caller must free whatever is in both partition chains.
 *
 * NOTES for code improvements -
 *
 * With a_onehit != 0, it is difficult to stop at the 'enddist' value
 * (or the a_ray_length value), and always get correct results.  Need
 * to take into account some additional factors:
 *
 * 1) A region shouldn't be evaluated until all its solids have been
 * intersected, to prevent the "CERN" problem of out points being
 * wrong because subtracted solids aren't intersected yet.
 *
 * Maybe "all" solids don't have to be intersected, but some strong
 * statements are needed along these lines.
 *
 * A region is definitely ready to be evaluated IF all its solids
 * have been intersected.
 *
 * 2) A partition shouldn't be evaluated until all the regions within
 * it are ready to be evaluated.
 */
int
rt_boolfinal(struct partition *InputHdp, struct partition *FinalHdp, fastf_t startdist, fastf_t enddist, struct bu_ptbl *regiontable, struct application *ap, const struct bu_bitv *solidbits)
{
    struct region *lastregion = (struct region *)NULL;
    struct region *TrueRg[2];
    register struct partition *pp;
    register int claiming_regions;
    int hits_avail = 0;
    int hits_needed;
    int ret = 0;
    int indefinite_outpt = 0;
    char *reason = (char *)NULL;
    fastf_t diff;

#define HITS_TODO (hits_needed - hits_avail)

    RT_CK_PT_HD(InputHdp);
    RT_CK_PT_HD(FinalHdp);
    BU_CK_PTBL(regiontable);
    RT_CK_RTI(ap->a_rt_i);
    BU_CK_BITV(solidbits);

    if (RT_G_DEBUG&DEBUG_PARTITION) {
	bu_log("\nrt_boolfinal(%g, %g) x%d y%d lvl%d START\n",
	       startdist, enddist,
	       ap->a_x, ap->a_y, ap->a_level);
    }

    if (!ap->a_multioverlap)
	ap->a_multioverlap = rt_default_multioverlap;

    if (!ap->a_logoverlap)
	ap->a_logoverlap = rt_default_logoverlap;

    if (enddist <= 0) {
	reason = "not done, behind start point";
	ret = 0;
	goto out;
    }

    if (ap->a_onehit < 0)
	hits_needed = -ap->a_onehit;
    else
	hits_needed = ap->a_onehit;

    if (ap->a_onehit != 0) {
	register struct partition *npp = FinalHdp->pt_forw;

	for (; npp != FinalHdp; npp = npp->pt_forw) {
	    if (npp->pt_inhit->hit_dist < 0.0)
		continue;
	    if (ap->a_onehit < 0 && npp->pt_regionp->reg_aircode != 0)
		continue;	/* skip air hits */
	    hits_avail += 2;	/* both in and out listed */
	}
	if (hits_avail >= hits_needed) {
	    reason = "a_onehit request satisfied at outset";
	    ret = 1;
	    goto out;
	}
    }

    if (ap->a_no_booleans) {
	while ((pp = InputHdp->pt_forw) != InputHdp) {
	    RT_CK_PT(pp);
	    DEQUEUE_PT(pp);
	    pp->pt_regionp = (struct region *)
		BU_PTBL_GET(&pp->pt_inseg->seg_stp->st_regions, 0);
	    RT_CK_REGION(pp->pt_regionp);
	    if (RT_G_DEBUG&DEBUG_PARTITION) {
		rt_pr_pt(ap->a_rt_i, pp);
	    }
	    INSERT_PT(pp, FinalHdp);
	}
	ret = 0;
	reason = "No a_onehit processing in a_no_booleans mode";
	goto out;
    }

    pp = InputHdp->pt_forw;
    while (pp != InputHdp) {
	RT_CK_PT(pp);
	claiming_regions = 0;
	if (RT_G_DEBUG&DEBUG_PARTITION) {
	    bu_log("\nrt_boolfinal(%g, %g) x%d y%d lvl%d, next input pp\n",
		   startdist, enddist,
		   ap->a_x, ap->a_y, ap->a_level);
	    rt_pr_pt(ap->a_rt_i, pp);
	}
	RT_CHECK_SEG(pp->pt_inseg);
	RT_CHECK_SEG(pp->pt_outseg);

	/* Force "very thin" partitions to have exactly zero thickness. */
	if (NEAR_EQUAL(pp->pt_inhit->hit_dist, pp->pt_outhit->hit_dist, ap->a_rt_i->rti_tol.dist)) {
	    if (RT_G_DEBUG&DEBUG_PARTITION) bu_log(
		"rt_boolfinal:  Zero thickness partition, prims %s %s (%.18e, %.18e) x%d y%d lvl%d\n",
		pp->pt_inseg->seg_stp->st_name,
		pp->pt_outseg->seg_stp->st_name,
		pp->pt_inhit->hit_dist,
		pp->pt_outhit->hit_dist,
		ap->a_x, ap->a_y, ap->a_level);
	    pp->pt_outhit->hit_dist = pp->pt_inhit->hit_dist;
	}


	/* Sanity checks on sorting. */
	if (pp->pt_inhit->hit_dist > pp->pt_outhit->hit_dist) {
	    bu_log("rt_boolfinal: inverted partition %p x%d y%d lvl%d\n",
		   (void *)pp,
		   ap->a_x, ap->a_y, ap->a_level);
	    rt_pr_partitions(ap->a_rt_i, InputHdp, "With problem");
	}
	if (pp->pt_forw != InputHdp) {
	    diff = pp->pt_outhit->hit_dist - pp->pt_forw->pt_inhit->hit_dist;
	    if (!ZERO(diff)) {
		if (NEAR_ZERO(diff, ap->a_rt_i->rti_tol.dist)) {
		    if (RT_G_DEBUG&DEBUG_PARTITION) bu_log("rt_boolfinal:  fusing 2 partitions %p %p\n",
							   (void *)pp, (void *)pp->pt_forw);
		    pp->pt_forw->pt_inhit->hit_dist = pp->pt_outhit->hit_dist;
		} else if (diff > 0) {
		    bu_log("rt_boolfinal:  sorting defect %e > %e! x%d y%d lvl%d, diff = %g\n",
			   pp->pt_outhit->hit_dist,
			   pp->pt_forw->pt_inhit->hit_dist,
			   ap->a_x, ap->a_y, ap->a_level, diff);
		    bu_log("sort defect is between parts %p and %p\n",
			   (void *)pp, (void *)pp->pt_forw);
		    if (!(RT_G_DEBUG & DEBUG_PARTITION))
			rt_pr_partitions(ap->a_rt_i, InputHdp, "With DEFECT");
		    ret = 0;
		    reason = "ERROR, sorting defect, give up";
		    goto out;
		}
	    }
	}

	/*
	 * If partition is behind ray start point, discard it.
	 *
	 * Partition may start before current box starts, because it's
	 * still waiting for all its solids to be shot.
	 */
	if (pp->pt_outhit->hit_dist <= 0.001 /* milimeters */) {
	    register struct partition *zap_pp;
	    if (RT_G_DEBUG&DEBUG_PARTITION)
		bu_log("discarding partition %p behind ray start, out dist=%g\n",
		       (void *)pp, pp->pt_outhit->hit_dist);
	    zap_pp = pp;
	    pp = pp->pt_forw;
	    DEQUEUE_PT(zap_pp);
	    FREE_PT(zap_pp, ap->a_resource);
	    continue;
	}

	/*
	 * If partition begins beyond current box end, the state of
	 * the partition is not fully known yet, and new partitions
	 * might be added in front of this one, so stop now.
	 */
	diff = pp->pt_inhit->hit_dist - enddist;
	if (diff > ap->a_rt_i->rti_tol.dist) {
	    if (RT_G_DEBUG&DEBUG_PARTITION)
		bu_log("partition begins %g beyond current box end, returning\n", diff);
	    reason = "partition begins beyond current box end";
	    ret = 0;
	    goto out;
	}

	/*
	 * If partition ends somewhere beyond the end of the current
	 * box, the condition of the outhit information is not fully
	 * known yet.  The partition might be broken or shortened by
	 * subsequent segments, not discovered until entering future
	 * boxes.
	 */
	diff = pp->pt_outhit->hit_dist - enddist;
	if (diff > ap->a_rt_i->rti_tol.dist) {
	    if (RT_G_DEBUG&DEBUG_PARTITION)
		bu_log("partition ends beyond current box end\n");
	    if (ap->a_onehit != 1) {
		ret = 0;
		reason = "a_onehit != 1, trace remaining boxes";
		goto out;
	    }
	    /* pt_outhit may not be correct */
	    indefinite_outpt = 1;
	} else {
	    indefinite_outpt = 0;
	}

	/* XXX a_ray_length checking should be done here, not in
	 * rt_shootray()
	 */

	/* Start with a clean slate when evaluating this partition */
	bu_ptbl_reset(regiontable);

	/*
	 * For each segment's solid that lies in this partition, add
	 * the list of regions that refer to that solid into the
	 * "regiontable" array.
	 */
	{
	    struct seg **segpp;

	    if (RT_G_DEBUG&DEBUG_PARTITION)
		bu_log("Building region table:\n");
	    for (BU_PTBL_FOR(segpp, (struct seg **), &pp->pt_seglist)) {
		struct soltab *stp = (*segpp)->seg_stp;
		RT_CK_SOLTAB(stp);
		bu_ptbl_cat_uniq(regiontable, &stp->st_regions);
	    }
	}

	if (RT_G_DEBUG&DEBUG_PARTITION) {
	    struct region **regpp;
	    bu_log("Region table for this partition:\n");
	    for (BU_PTBL_FOR(regpp, (struct region **), regiontable)) {
		register struct region *regp;

		regp = *regpp;
		RT_CK_REGION(regp);
		bu_log("%p %s\n", (void *)regp, regp->reg_name);
	    }
	}

	if (indefinite_outpt) {
	    if (RT_G_DEBUG&DEBUG_PARTITION)
		bu_log("indefinite out point, checking partition eligibility for early evaluation.\n");
	    /*
	     * More hits still needed.  HITS_TODO > 0.  If every solid
	     * in every region participating in this partition has
	     * been intersected, then it is OK to evaluate it now.
	     */
	    if (!rt_bool_partition_eligible(regiontable, solidbits, pp)) {
		ret = 0;
		reason = "Partition not yet eligible for evaluation";
		goto out;
	    }
	    if (RT_G_DEBUG&DEBUG_PARTITION)
		bu_log("Partition is eligibile for evaluation.\n");
	}

	/* Evaluate the boolean trees of any regions involved */
	{
	    struct region **regpp;
	    for (BU_PTBL_FOR(regpp, (struct region **), regiontable)) {
		register struct region *regp;

		regp = *regpp;
		RT_CK_REGION(regp);
		if (RT_G_DEBUG&DEBUG_PARTITION) {
		    rt_pr_tree_val(regp->reg_treetop, pp, 2, 0);
		    rt_pr_tree_val(regp->reg_treetop, pp, 1, 0);
		    rt_pr_tree_val(regp->reg_treetop, pp, 0, 0);
		    bu_log("%p=bit%d, %s: ", (void *)regp, regp->reg_bit, regp->reg_name);
		}
		if (regp->reg_all_unions) {
		    if (RT_G_DEBUG&DEBUG_PARTITION)
			bu_log("BOOL_TRUE (all union)\n");
		    claiming_regions++;
		    lastregion = regp;
		    continue;
		}
		if (rt_booleval(regp->reg_treetop, pp, TrueRg,
				ap->a_resource) == BOOL_FALSE) {
		    if (RT_G_DEBUG&DEBUG_PARTITION)
			bu_log("BOOL_FALSE\n");
		    /* Null out non-claiming region's pointer */
		    *regpp = REGION_NULL;
		    continue;
		}
		/* This region claims partition */
		if (RT_G_DEBUG&DEBUG_PARTITION)
		    bu_log("BOOL_TRUE (eval)\n");
		claiming_regions++;
		lastregion = regp;
	    }
	}
	if (RT_G_DEBUG&DEBUG_PARTITION)
	    bu_log("rt_boolfinal:  claiming_regions=%d (%g <-> %g)\n",
		   claiming_regions, pp->pt_inhit->hit_dist, pp->pt_outhit->hit_dist);
	if (claiming_regions == 0) {
	    if (RT_G_DEBUG&DEBUG_PARTITION)
		bu_log("rt_boolfinal moving past partition %p\n", (void *)pp);
	    pp = pp->pt_forw;		/* onwards! */
	    continue;
	}

	if (claiming_regions > 1) {
	    /*
	     * There is an overlap between two or more regions, invoke
	     * multi-overlap handler.
	     */
	    if (RT_G_DEBUG&DEBUG_PARTITION)
		bu_log("rt_boolfinal:  invoking a_multioverlap() pp=%p\n", (void *)pp);
	    bu_ptbl_rm(regiontable, (long *)NULL);
	    ap->a_logoverlap(ap, pp, regiontable, InputHdp);
	    ap->a_multioverlap(ap, pp, regiontable, InputHdp);

	    /* Count number of remaining regions, s/b 0 or 1 */
	    claiming_regions = 0;
	    {
		register struct region **regpp;
		for (BU_PTBL_FOR(regpp, (struct region **), regiontable)) {
		    if (*regpp != REGION_NULL) {
			claiming_regions++;
			lastregion = *regpp;
		    }
		}
	    }

	    /*
	     * If claiming_regions == 0, discard partition.
	     * If claiming_regions > 1, signal error and discard.
	     * There is nothing further we can do to fix it.
	     */
	    if (claiming_regions > 1) {
		bu_log("rt_boolfinal() a_multioverlap() failed to resolve overlap, discarding bad partition:\n");
		rt_pr_pt(ap->a_rt_i, pp);
	    }

	    if (claiming_regions != 1) {
		register struct partition *zap_pp;

		if (RT_G_DEBUG&DEBUG_PARTITION)
		    bu_log("rt_boolfinal discarding overlap partition %p\n", (void *)pp);
		zap_pp = pp;
		pp = pp->pt_forw;		/* onwards! */
		DEQUEUE_PT(zap_pp);
		FREE_PT(zap_pp, ap->a_resource);
		continue;
	    }
	}

	/*
	 * claiming_regions == 1
	 *
	 * Remove this partition from the input queue, and append it
	 * to the result queue.
	 */
	{
	    register struct partition *newpp;
	    register struct partition *lastpp;
	    if (RT_G_DEBUG&DEBUG_PARTITION)
		bu_log("Appending partition to result queue: %g, %g\n",
		       pp->pt_inhit->hit_dist, pp->pt_outhit->hit_dist);
	    newpp = pp;
	    pp=pp->pt_forw;				/* onwards! */
	    DEQUEUE_PT(newpp);
	    RT_CHECK_SEG(newpp->pt_inseg);		/* sanity */
	    RT_CHECK_SEG(newpp->pt_outseg);		/* sanity */
	    /* Record the "owning" region.  pt_regionp = NULL before here. */
	    newpp->pt_regionp = lastregion;

	    /* See if this new partition extends the previous last
	     * partition, "exactly" matching.
	     */
	    lastpp = FinalHdp->pt_back;
	    if (lastpp != FinalHdp &&
		lastregion == lastpp->pt_regionp &&
		NEAR_EQUAL(newpp->pt_inhit->hit_dist,
			   lastpp->pt_outhit->hit_dist,
			   ap->a_rt_i->rti_tol.dist) &&
		(ap->a_rt_i->rti_save_overlaps == 0 ||
		 rt_overlap_tables_equal(
		     lastpp->pt_overlap_reg,
		     newpp->pt_overlap_reg))
		) {
		/* same region, merge by extending last final partition */
		if (RT_G_DEBUG&DEBUG_PARTITION)
		    bu_log("rt_boolfinal 'exact match', extending last partition, discarding %p\n", (void *)newpp);
		RT_CK_PT(lastpp);
		RT_CHECK_SEG(lastpp->pt_inseg);	/* sanity */
		RT_CHECK_SEG(lastpp->pt_outseg);/* sanity */
		if (RT_G_DEBUG&DEBUG_PARTITION)
		    bu_log("rt_boolfinal collapsing %p %p\n", (void *)lastpp, (void *)newpp);
		lastpp->pt_outhit = newpp->pt_outhit;
		lastpp->pt_outflip = newpp->pt_outflip;
		lastpp->pt_outseg = newpp->pt_outseg;

		/* Don't lose the fact that the two solids of this
		 * partition contributed.
		 */
		bu_ptbl_ins_unique(&lastpp->pt_seglist, (long *)newpp->pt_inseg);
		bu_ptbl_ins_unique(&lastpp->pt_seglist, (long *)newpp->pt_outseg);

		FREE_PT(newpp, ap->a_resource);
		newpp = lastpp;
	    } else {
		APPEND_PT(newpp, lastpp);
		if (!(ap->a_onehit < 0 && newpp->pt_regionp->reg_aircode != 0))
		    hits_avail += 2;
	    }

	    RT_CHECK_SEG(newpp->pt_inseg);		/* sanity */
	    RT_CHECK_SEG(newpp->pt_outseg);		/* sanity */
	}

	/* See if it's worthwhile breaking out of partition loop early */
	if (ap->a_onehit != 0 && HITS_TODO <= 0) {
	    ret = 1;
	    if (pp == InputHdp)
		reason = "a_onehit satisfied at bottom";
	    else
		reason = "a_onehit satisfied early";
	    goto out;
	}
    }
    if (ap->a_onehit != 0 && HITS_TODO <= 0) {
	ret = 1;
	reason = "a_onehit satisfied at end";
    } else {
	ret = 0;
	reason = "more partitions needed";
    }
out:
    if (RT_G_DEBUG&DEBUG_PARTITION) {
	bu_log("rt_boolfinal() ret=%d, %s\n", ret, reason);
	rt_pr_partitions(ap->a_rt_i, FinalHdp, "rt_boolfinal: Final partition list at return:");
	rt_pr_partitions(ap->a_rt_i, InputHdp, "rt_boolfinal: Input/pending partition list at return:");
	bu_log("rt_boolfinal() ret=%d, %s\n", ret, reason);
    }
    return ret;
}


/**
 * R T _ R E L D I F F
 *
 * Compute the relative difference of two floating point numbers.
 *
 * Returns 0.0 if exactly the same, otherwise ratio of difference,
 * relative to the larger of the two (range 0.0-1.0)
 *
 * DEVELOPER DEPRECATION NOTICE: use NEAR_ZERO instead.
 */
double
rt_reldiff(double a, double b)
{
    register fastf_t d;
    register fastf_t diff;

    /* d = Max(Abs(a), Abs(b)) */
    d = (a >= 0.0) ? a : -a;
    if (b >= 0.0) {
	if (b > d) d = b;
    } else {
	if ((-b) > d) d = (-b);
    }
    if (ZERO(d))
	return 0.0;
    if ((diff = a - b) < 0.0) diff = -diff;
    return diff / d;
}


/**
 * R T _ P A R T I T I O N _ L E N
 *
 * Return the length of a partition linked list.
 */
int
rt_partition_len(const struct partition *partheadp)
{
    register struct partition *pp;
    register long count = 0;

    pp = partheadp->pt_forw;
    if (pp == PT_NULL)
	return 0;		/* Header not initialized yet */
    for (; pp != partheadp; pp = pp->pt_forw) {
	if (pp->pt_magic != 0) {
	    /* Partitions on the free queue have pt_magic = 0 */
	    RT_CK_PT(pp);
	}
	if (++count > 1000000) bu_bomb("partition length > 10000000 elements\n");
    }
    return (int)count;
}


/**
 * XXX This routine seems to free things more than once.  For a
 * temporary measure, don't free things.
 */
void
rt_rebuild_overlaps(struct partition *PartHdp, struct application *ap, int rebuild_fastgen_plates_only)
{
    struct partition *pp, *next, *curr;
    struct region *pp_reg;
    struct partition *pp_open;
    struct bu_ptbl open_parts;
    int i, j;

    RT_CK_PT_HD(PartHdp);
    RT_CK_AP(ap);

    bu_ptbl_init(&open_parts, 0, "Open partitions");

    pp = PartHdp->pt_forw;
    while (pp != PartHdp) {
	RT_CK_PARTITION(pp);
	next = pp->pt_forw;

	if (rebuild_fastgen_plates_only && pp->pt_regionp->reg_is_fastgen != REGION_FASTGEN_PLATE) {
	    bu_ptbl_trunc(&open_parts, 0);
	    pp = next;
	    continue;
	}

	for (i=0; i<BU_PTBL_END(&open_parts); i++) {
	    int keep_open=0;

	    if (!pp)
		break;

	    pp_open = (struct partition *)BU_PTBL_GET(&open_parts, i);
	    if (!pp_open)
		continue;
	    RT_CK_PARTITION(pp_open);

	    if (pp->pt_overlap_reg) {
		j = -1;
		while ((pp_reg = pp->pt_overlap_reg[++j])) {
		    if (pp_reg == (struct region *)(-1))
			continue;
		    RT_CK_REGION(pp_reg);

		    if (pp_reg == pp_open->pt_regionp) {
			/* add this partition to pp_open */
			pp_open->pt_outseg = pp->pt_outseg;
			pp_open->pt_outhit = pp->pt_outhit;
			pp_open->pt_outflip = pp->pt_outflip;
			bu_ptbl_cat_uniq(&pp_open->pt_seglist, &pp->pt_seglist);

			/* mark it as used */
			pp->pt_overlap_reg[j] = (struct region *)(-1);
			if (pp_reg == pp->pt_regionp)
			    pp->pt_regionp = (struct region *)NULL;

			/* keep pp_open open */
			keep_open = 1;
		    }
		}
	    } else {
		if (pp->pt_regionp == pp_open->pt_regionp && pp->pt_inhit->hit_dist <= pp_open->pt_outhit->hit_dist) {
		    /* add this partition to pp_open */
		    pp_open->pt_outseg = pp->pt_outseg;
		    pp_open->pt_outhit = pp->pt_outhit;
		    pp_open->pt_outflip = pp->pt_outflip;
		    bu_ptbl_cat_uniq(&pp_open->pt_seglist, &pp->pt_seglist);

		    /* eliminate this partition */
		    BU_LIST_DEQUEUE((struct bu_list *)pp)
			pp->pt_overlap_reg = NULL;	/* sanity */
		    pp = (struct partition *)NULL;

		    /* keep pp_open open */
		    keep_open = 1;
		}
	    }

	    if (!keep_open) {
		BU_PTBL_CLEAR_I(&open_parts, i);
	    }
	}

	/* if all region claims have been removed, eliminate the partition */
	if (pp && pp->pt_overlap_reg) {
	    int reg_count=0;

	    /* count remaining region claims */
	    j = -1;
	    while ((pp_reg = pp->pt_overlap_reg[++j]))
		if (pp_reg != (struct region *)(-1)) {
		    RT_CK_REGION(pp_reg);
		    reg_count++;
		}

	    if (!reg_count) {
		BU_LIST_DEQUEUE((struct bu_list *)pp)
		    bu_free((char *)pp->pt_overlap_reg, "overlap list");
		pp->pt_overlap_reg = NULL;	/* sanity */
		pp = (struct partition *)NULL;
	    }
	}

	/* any remaining region claims must produce new partitions */
	if (pp) {
	    if (pp->pt_overlap_reg) {
		j = -1;
		curr = pp;
		while ((pp_reg = pp->pt_overlap_reg[++j])) {
		    struct partition *new_pp;

		    if (pp_reg == (struct region *)(-1))
			continue;
		    RT_CK_REGION(pp_reg);

		    if (rebuild_fastgen_plates_only &&
			pp_reg->reg_is_fastgen != REGION_FASTGEN_PLATE)
			continue;

		    /* if the original partition is available, just use it */
		    if (!pp->pt_regionp || pp->pt_regionp == pp_reg) {
			pp->pt_regionp = pp_reg;
			bu_ptbl_ins(&open_parts, (long *)pp);
		    } else {
			/* create a new partition, link it to the end of the current pp,
			 * and add it to the open list */
			RT_DUP_PT(ap->a_rt_i, new_pp, pp, ap->a_resource)
			    new_pp->pt_regionp = pp_reg;
			new_pp->pt_overlap_reg = (struct region **)NULL;
			BU_LIST_APPEND((struct bu_list *)curr, (struct bu_list *)new_pp)
			    bu_ptbl_ins(&open_parts, (long *)new_pp);
			curr = new_pp;
		    }
		}
	    } else {
		if (rebuild_fastgen_plates_only) {
		    if (pp->pt_regionp->reg_is_fastgen == REGION_FASTGEN_PLATE) {
			bu_ptbl_ins(&open_parts, (long *)pp);
		    }
		} else {
		    bu_ptbl_ins(&open_parts, (long *)pp);
		}
	    }
	    if (pp->pt_overlap_reg) {
		bu_free((char *)pp->pt_overlap_reg, "overlap list");
		pp->pt_overlap_reg = (struct region **)NULL;
	    }
	}
	pp = next;
    }

    bu_ptbl_free(&open_parts);
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
