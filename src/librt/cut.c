/*                           C U T . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2021 United States Government as represented by
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
/** @file librt/cut.c
 *
 * Cut space into lots of small boxes (RPPs actually).
 *
 * Call tree for default path through the code:
 *	rt_cut_it()
 *		rt_cut_extend() for all solids in model
 *		rt_ct_optim()
 *			rt_ct_old_assess()
 *			rt_ct_box()
 *				rt_ct_populate_box()
 *					rt_ck_overlap()
 *
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu/parallel.h"
#include "bu/sort.h"
#include "vmath.h"
#include "raytrace.h"
#include "bg/plane.h"
#include "bview/plot3.h"


HIDDEN int rt_ck_overlap(const vect_t min, const vect_t max, const struct soltab *stp, const struct rt_i *rtip);
HIDDEN int rt_ct_box(struct rt_i *rtip, union cutter *cutp, int axis, double where, int force);
HIDDEN void rt_ct_optim(struct rt_i *rtip, union cutter *cutp, size_t depth);
HIDDEN void rt_ct_free(struct rt_i *rtip, union cutter *cutp);
HIDDEN void rt_ct_release_storage(union cutter *cutp);

HIDDEN void rt_ct_measure(struct rt_i *rtip, union cutter *cutp, int depth);
HIDDEN union cutter *rt_ct_get(struct rt_i *rtip);
HIDDEN void rt_plot_cut(FILE *fp, struct rt_i *rtip, union cutter *cutp, int lvl);

extern void rt_pr_cut_info(const struct rt_i *rtip, const char *str);
HIDDEN int rt_ct_old_assess(register union cutter *, register int, double *, double *);

#define AXIS(depth)	((depth)%3)	/* cuts: X, Y, Z, repeat */


/**
 * Process all the nodes in the global array rtip->rti_cuts_waiting,
 * until none remain.  This routine is run in parallel.
 */
void
rt_cut_optimize_parallel(int cpu, void *arg)
{
    struct rt_i *rtip = (struct rt_i *)arg;
    union cutter *cp;
    int i;

    if (!arg && RT_G_DEBUG&RT_DEBUG_CUT)
	bu_log("rt_cut_optimized_parallel(%d): NULL rtip\n", cpu);

    RT_CK_RTI(rtip);
    for (;;) {

	bu_semaphore_acquire(RT_SEM_WORKER);
	i = rtip->rti_cuts_waiting.end--;	/* get first free index */
	bu_semaphore_release(RT_SEM_WORKER);
	i -= 1;				/* change to last used index */

	if (i < 0) break;

	cp = (union cutter *)BU_PTBL_GET(&rtip->rti_cuts_waiting, i);

	rt_ct_optim(rtip, cp, Z);
    }
}


int
rt_split_mostly_empty_cells(struct rt_i *rtip, union cutter *cutp)
{
    point_t max, min;
    struct soltab *stp;
    struct rt_piecelist pl;
    fastf_t range[3], empty[3], tmp;
    int upper_or_lower[3];
    fastf_t max_empty;
    int max_empty_dir;
    size_t i;
    int num_splits=0;

    switch (cutp->cut_type) {
	case CUT_CUTNODE:
	    num_splits += rt_split_mostly_empty_cells(rtip, cutp->cn.cn_l);
	    num_splits += rt_split_mostly_empty_cells(rtip, cutp->cn.cn_r);
	    break;
	case CUT_BOXNODE:
	    /* find the actual bounds of stuff in this cell */
	    if (cutp->bn.bn_len == 0 && cutp->bn.bn_piecelen == 0) {
		break;
	    }
	    VSETALL(min, MAX_FASTF);
	    VREVERSE(max, min);

	    for (i=0; i<cutp->bn.bn_len; i++) {
		stp = cutp->bn.bn_list[i];
		VMIN(min, stp->st_min);
		VMAX(max, stp->st_max);
	    }

	    for (i=0; i<cutp->bn.bn_piecelen; i++) {
		size_t j;

		pl = cutp->bn.bn_piecelist[i];
		for (j=0; j<pl.npieces; j++) {
		    int piecenum;

		    piecenum = pl.pieces[j];
		    VMIN(min, pl.stp->st_piece_rpps[piecenum].min);
		    VMAX(max, pl.stp->st_piece_rpps[piecenum].max);
		}
	    }

	    /* clip min and max to the bounds of this cell */
	    for (i=X; i<=Z; i++) {
		if (min[i] < cutp->bn.bn_min[i]) {
		    min[i] = cutp->bn.bn_min[i];
		}
		if (max[i] > cutp->bn.bn_max[i]) {
		    max[i] = cutp->bn.bn_max[i];
		}
	    }

	    /* min and max now have the real bounds of data in this cell */
	    VSUB2(range, cutp->bn.bn_max, cutp->bn.bn_min);
	    for (i=X; i<=Z; i++) {
		empty[i] = cutp->bn.bn_max[i] - max[i];
		upper_or_lower[i] = 1; /* upper section is empty */
		tmp = min[i] - cutp->bn.bn_min[i];
		if (tmp > empty[i]) {
		    empty[i] = tmp;
		    upper_or_lower[i] = 0;	/* lower section is empty */
		}
	    }
	    max_empty = empty[X];
	    max_empty_dir = X;
	    if (empty[Y] > max_empty) {
		max_empty = empty[Y];
		max_empty_dir = Y;
	    }
	    if (empty[Z] > max_empty) {
		max_empty = empty[Z];
		max_empty_dir = Z;
	    }
	    if (max_empty / range[max_empty_dir] > 0.5) {
		/* this cell is over 50% empty in this direction, split it */

		fastf_t where;

		/* select cutting plane, but move it slightly off any geometry */
		if (upper_or_lower[max_empty_dir]) {
		    where = max[max_empty_dir] + rtip->rti_tol.dist;
		    if (where >= cutp->bn.bn_max[max_empty_dir]) {
			return num_splits;
		    }
		} else {
		    where = min[max_empty_dir] - rtip->rti_tol.dist;
		    if (where <= cutp->bn.bn_min[max_empty_dir]) {
			return num_splits;
		    }
		}
		if (where - cutp->bn.bn_min[max_empty_dir] < 2.0 ||
		    cutp->bn.bn_max[max_empty_dir] - where < 2.0) {
		    /* will make a box too small */
		    return num_splits;
		}
		if (rt_ct_box(rtip, cutp, max_empty_dir, where, 1)) {
		    num_splits++;
		    num_splits += rt_split_mostly_empty_cells(rtip, cutp->cn.cn_l);
		    num_splits += rt_split_mostly_empty_cells(rtip, cutp->cn.cn_r);
		}
	    }
	    break;
    }

    return num_splits;
}


void
rt_cut_it(register struct rt_i *rtip, int ncpu)
{
    register struct soltab *stp;
    union cutter *finp;	/* holds the finite solids */
    FILE *plotfp;
    int num_splits=0;

    if (ncpu < 1) ncpu = 1; /* sanity */

    /* Make a list of all solids into one special boxnode, then refine. */
    BU_ALLOC(finp, union cutter);
    finp->cut_type = CUT_BOXNODE;
    VMOVE(finp->bn.bn_min, rtip->mdl_min);
    VMOVE(finp->bn.bn_max, rtip->mdl_max);
    finp->bn.bn_len = 0;
    finp->bn.bn_maxlen = rtip->nsolids+1;
    finp->bn.bn_list = (struct soltab **)bu_calloc(
	finp->bn.bn_maxlen, sizeof(struct soltab *),
	"rt_cut_it: initial list alloc");

    rtip->rti_inf_box.cut_type = CUT_BOXNODE;

    RT_VISIT_ALL_SOLTABS_START(stp, rtip) {
	/* Ignore "dead" solids in the list.  (They failed prep) */
	if (stp->st_aradius <= 0) continue;

	/* Infinite and finite solids all get lumped together */
	rt_cut_extend(finp, stp, rtip);

	if (stp->st_aradius >= INFINITY) {
	    /* Also add infinite solids to a special BOXNODE */
	    rt_cut_extend(&rtip->rti_inf_box, stp, rtip);
	}
    } RT_VISIT_ALL_SOLTABS_END;

    /* Dynamic decisions on tree limits.  Note that there will be
     * (2**rtip->rti_cutdepth)*rtip->rti_cutlen potential leaf slots.
     * Also note that solids will typically span several leaves.
     */
    rtip->rti_cutlen = lrint(floor(log((double)(rtip->nsolids+1))));  /* ln ~= log2, nsolids+1 to avoid log(0) */
    rtip->rti_cutdepth = 2 * rtip->rti_cutlen;
    if (rtip->rti_cutlen < 3) rtip->rti_cutlen = 3;
    if (rtip->rti_cutdepth < 12) rtip->rti_cutdepth = 12;
    if (rtip->rti_cutdepth > 24) rtip->rti_cutdepth = 24;     /* !! */
    if (RT_G_DEBUG&RT_DEBUG_CUT)
	bu_log("Before Space Partitioning: Max Tree Depth=%zu, Cutoff primitive count=%zu\n",
	       rtip->rti_cutdepth, rtip->rti_cutlen);

    bu_ptbl_init(&rtip->rti_cuts_waiting, rtip->nsolids,
		 "rti_cuts_waiting ptbl");

    if (rtip->rti_hasty_prep) {
	rtip->rti_space_partition = RT_PART_NUBSPT;
	rtip->rti_cutdepth = 6;
    }

    switch (rtip->rti_space_partition) {
	case RT_PART_NUBSPT: {
	    rtip->rti_CutHead = *finp;	/* union copy */
	    rt_ct_optim(rtip, &rtip->rti_CutHead, 0);
	    /* one more pass to find cells that are mostly empty */
	    num_splits = rt_split_mostly_empty_cells(rtip,  &rtip->rti_CutHead);

	    if (RT_G_DEBUG&RT_DEBUG_CUT) {
		bu_log("rt_split_mostly_empty_cells(): split %d cells\n", num_splits);
	    }

	    break; }
	default:
	    bu_bomb("rt_cut_it: unknown space partitioning method\n");
    }

    bu_free(finp, "union cutter");

    /* Measure the depth of tree, find max # of RPPs in a cut node */

    bu_hist_init(&rtip->rti_hist_cellsize, 0.0, 400.0, 400);
    bu_hist_init(&rtip->rti_hist_cell_pieces, 0.0, 400.0, 400);
    bu_hist_init(&rtip->rti_hist_cutdepth, 0.0,
		 (fastf_t)rtip->rti_cutdepth+1, rtip->rti_cutdepth+1);
    memset(rtip->rti_ncut_by_type, 0, sizeof(rtip->rti_ncut_by_type));
    rt_ct_measure(rtip, &rtip->rti_CutHead, 0);
    if (RT_G_DEBUG&RT_DEBUG_CUT) {
	rt_pr_cut_info(rtip, "Cut");
    }

    if (RT_G_DEBUG&RT_DEBUG_CUTDETAIL) {
	/* Produce a voluminous listing of the cut tree */
	rt_pr_cut(&rtip->rti_CutHead, 0);
    }

    if (RT_G_DEBUG&RT_DEBUG_PL_BOX) {
	/* Debugging code to plot cuts */
	if ((plotfp=fopen("rtcut.plot3", "wb"))!=NULL) {
	    pdv_3space(plotfp, rtip->rti_pmin, rtip->rti_pmax);
	    /* Plot all the cutting boxes */
	    rt_plot_cut(plotfp, rtip, &rtip->rti_CutHead, 0);
	    (void)fclose(plotfp);
	}
    }
}


void
rt_cut_extend(register union cutter *cutp, struct soltab *stp, const struct rt_i *rtip)
{
    RT_CK_SOLTAB(stp);
    RT_CK_RTI(rtip);

    BU_ASSERT(cutp->cut_type == CUT_BOXNODE);

    if (RT_G_DEBUG&RT_DEBUG_CUTDETAIL) {
	bu_log("rt_cut_extend(cutp=%p) %s npieces=%ld\n",
	       (void *)cutp, stp->st_name, stp->st_npieces);
    }

    if (stp->st_npieces > 0) {
	struct rt_piecelist *plp;
	register int i;

	if (cutp->bn.bn_piecelist == NULL) {
	    /* Allocate enough piecelist's to hold all solids */
	    BU_ASSERT(rtip->nsolids > 0);
	    cutp->bn.bn_piecelist = (struct rt_piecelist *) bu_calloc(
		sizeof(struct rt_piecelist), (rtip->nsolids + 2),
		"rt_ct_box bn_piecelist (root node)");
	    cutp->bn.bn_piecelen = 0;	/* sanity */
	    cutp->bn.bn_maxpiecelen = rtip->nsolids + 2;
	}
	plp = &cutp->bn.bn_piecelist[cutp->bn.bn_piecelen++];
	plp->magic = RT_PIECELIST_MAGIC;
	plp->stp = stp;

	/* List every index that this solid has */
	plp->npieces = stp->st_npieces;
	plp->pieces = (long *)bu_calloc(plp->npieces, sizeof(long), "pieces[]");
	for (i = stp->st_npieces-1; i >= 0; i--)
	    plp->pieces[i] = i;

	return;
    }

    /* No pieces, list the entire solid on bn_list */
    if (cutp->bn.bn_len >= cutp->bn.bn_maxlen) {
	/* Need to get more space in list.  */
	if (cutp->bn.bn_maxlen <= 0) {
	    /* Initial allocation */
	    if (rtip->rti_cutlen > rtip->nsolids)
		cutp->bn.bn_maxlen = rtip->rti_cutlen;
	    else
		cutp->bn.bn_maxlen = rtip->nsolids + 2;
	    cutp->bn.bn_list = (struct soltab **)bu_calloc(
		cutp->bn.bn_maxlen, sizeof(struct soltab *),
		"rt_cut_extend: initial list alloc");
	} else {
	    cutp->bn.bn_maxlen *= 8;
	    cutp->bn.bn_list = (struct soltab **) bu_realloc(
		(void *)cutp->bn.bn_list,
		sizeof(struct soltab *) * cutp->bn.bn_maxlen,
		"rt_cut_extend: list extend");
	}
    }
    cutp->bn.bn_list[cutp->bn.bn_len++] = stp;
}


#define PIECE_BLOCK 512


/**
 * Given that 'outp' has been given a bounding box smaller than that
 * of 'inp', copy over everything which still fits in the smaller box.
 *
 * Returns -
 * 0 if outp has the same number of items as inp
 * 1 if outp has fewer items than inp
 */
HIDDEN int
rt_ct_populate_box(union cutter *outp, const union cutter *inp, struct rt_i *rtip)
{
    register int i;
    int success = 0;
    const struct bn_tol *tol = &rtip->rti_tol;

    /* Examine the solids */
    outp->bn.bn_len = 0;
    outp->bn.bn_maxlen = inp->bn.bn_len;
    if (outp->bn.bn_maxlen > 0) {
	outp->bn.bn_list = (struct soltab **) bu_calloc(
	    outp->bn.bn_maxlen, sizeof(struct soltab *),
	    "bn_list");
	for (i = inp->bn.bn_len-1; i >= 0; i--) {
	    struct soltab *stp = inp->bn.bn_list[i];
	    if (!rt_ck_overlap(outp->bn.bn_min, outp->bn.bn_max,
			       stp, rtip))
		continue;
	    outp->bn.bn_list[outp->bn.bn_len++] = stp;
	}
	if (outp->bn.bn_len < inp->bn.bn_len) success = 1;
    } else {
	outp->bn.bn_list = (struct soltab **)NULL;
    }

    /* Examine the solid pieces */
    outp->bn.bn_piecelen = 0;
    if (inp->bn.bn_piecelen <= 0) {
	outp->bn.bn_piecelist = (struct rt_piecelist *)NULL;
	outp->bn.bn_maxpiecelen = 0;
	return success;
    }

    outp->bn.bn_piecelist = (struct rt_piecelist *) bu_calloc(inp->bn.bn_piecelen, sizeof(struct rt_piecelist), "rt_piecelist");
    outp->bn.bn_maxpiecelen = inp->bn.bn_piecelen;

    for (i = inp->bn.bn_piecelen-1; i >= 0; i--) {
	struct rt_piecelist *plp = &inp->bn.bn_piecelist[i];	/* input */
	struct soltab *stp = plp->stp;
	struct rt_piecelist *olp = &outp->bn.bn_piecelist[outp->bn.bn_piecelen]; /* output */
	int j, k;
	long piece_list[PIECE_BLOCK];	/* array of pieces */
	long piece_count=0;		/* count of used slots in above array */
	long *more_pieces=NULL;		/* dynamically allocated array for overflow of above array */
	long more_piece_count=0;	/* number of slots used in dynamic array */
	long more_piece_len=0;		/* allocated length of dynamic array */

	RT_CK_PIECELIST(plp);
	RT_CK_SOLTAB(stp);

	/* Loop for every piece of this solid */
	for (j = plp->npieces-1; j >= 0; j--) {
	    long indx = plp->pieces[j];
	    struct bound_rpp *rpp = &stp->st_piece_rpps[indx];
	    if (!V3RPP_OVERLAP_TOL(outp->bn.bn_min, outp->bn.bn_max, rpp->min, rpp->max, tol->dist))
		continue;
	    if (piece_count < PIECE_BLOCK) {
		piece_list[piece_count++] = indx;
	    } else if (more_piece_count >= more_piece_len) {
		/* this should be an extremely rare occurrence */
		more_piece_len += PIECE_BLOCK;
		more_pieces = (long *)bu_realloc(more_pieces, more_piece_len * sizeof(long),
						 "more_pieces");
		more_pieces[more_piece_count++] = indx;
	    } else {
		more_pieces[more_piece_count++] = indx;
	    }
	}
	olp->npieces = piece_count + more_piece_count;
	if (olp->npieces > 0) {
	    /* This solid contributed pieces to the output box */
	    olp->magic = RT_PIECELIST_MAGIC;
	    olp->stp = stp;
	    outp->bn.bn_piecelen++;
	    olp->pieces = (long *)bu_calloc(olp->npieces, sizeof(long), "olp->pieces[]");
	    for (j=0; j<piece_count; j++) {
		olp->pieces[j] = piece_list[j];
	    }
	    k = piece_count;
	    for (j=0; j<more_piece_count; j++) {
		olp->pieces[k++] = more_pieces[j];
	    }
	    if (more_pieces) {
		bu_free((char *)more_pieces, "more_pieces");
	    }
	    if (olp->npieces < plp->npieces) success = 1;
	} else {
	    olp->pieces = NULL;
	    /* if (plp->npieces > 0) success = 1; */
	}
    }

    return success;
}


/**
 * Cut the given box node with a plane along the given axis, at the
 * specified distance "where".  Convert the caller's box node into a
 * cut node, allocating two additional box nodes for the new leaves.
 *
 * If, according to the classifier, both sides have the same number of
 * solids, then nothing is changed, and an error is returned.
 *
 * The storage strategy used is to make the maximum length of each of
 * the two child boxnodes be the current length of the source node.
 *
 * Returns -
 * 0 failure
 * 1 success
 */
HIDDEN int
rt_ct_box(struct rt_i *rtip, register union cutter *cutp, register int axis, double where, int force)
{
    register union cutter *rhs, *lhs;
    int success = 0;

    RT_CK_RTI(rtip);
    if (RT_G_DEBUG&RT_DEBUG_CUTDETAIL) {
	bu_log("rt_ct_box(%p, %c) %g .. %g .. %g\n",
	       (void *)cutp, "XYZ345"[axis],
	       cutp->bn.bn_min[axis],
	       where,
	       cutp->bn.bn_max[axis]);
    }

    /* LEFT side */
    lhs = rt_ct_get(rtip);
    lhs->bn.bn_type = CUT_BOXNODE;
    VMOVE(lhs->bn.bn_min, cutp->bn.bn_min);
    VMOVE(lhs->bn.bn_max, cutp->bn.bn_max);
    lhs->bn.bn_max[axis] = where;

    success = rt_ct_populate_box(lhs, cutp, rtip);

    /* RIGHT side */
    rhs = rt_ct_get(rtip);
    rhs->bn.bn_type = CUT_BOXNODE;
    VMOVE(rhs->bn.bn_min, cutp->bn.bn_min);
    VMOVE(rhs->bn.bn_max, cutp->bn.bn_max);
    rhs->bn.bn_min[axis] = where;

    success += rt_ct_populate_box(rhs, cutp, rtip);

    /* Check to see if complexity didn't decrease */
    if (success == 0 && !force) {
	/*
	 * This cut operation did no good, release storage,
	 * and let caller attempt something else.
	 */
	if (RT_G_DEBUG&RT_DEBUG_CUTDETAIL) {
	    static char axis_str[] = "XYZw";
	    bu_log("rt_ct_box:  no luck, len=%zu, axis=%c\n",
		   cutp->bn.bn_len, axis_str[axis]);
	}
	rt_ct_free(rtip, rhs);
	rt_ct_free(rtip, lhs);
	return 0;		/* fail */
    }

    /* Success, convert callers box node into a cut node */
    rt_ct_release_storage(cutp);

    cutp->cut_type = CUT_CUTNODE;
    cutp->cn.cn_axis = axis;
    cutp->cn.cn_point = where;
    cutp->cn.cn_l = lhs;
    cutp->cn.cn_r = rhs;
    return 1;			/* success */
}


/**
 * See if any part of the solid is contained within the bounding box
 * (RPP).
 *
 * If the solid RPP at least partly overlaps the bounding RPP, invoke
 * the per-solid "classifier" method to perform a more rigorous check.
 *
 * Returns -
 * !0 if object overlaps box.
 *  0 if no overlap.
 */
HIDDEN int
rt_ck_overlap(register const fastf_t *min, register const fastf_t *max, register const struct soltab *stp, register const struct rt_i *rtip)
{
    RT_CHECK_SOLTAB(stp);

    if (RT_G_DEBUG&RT_DEBUG_BOXING) {
	bu_log("rt_ck_overlap(%s)\n", stp->st_name);
	VPRINT(" box min", min);
	VPRINT(" sol min", stp->st_min);
	VPRINT(" box max", max);
	VPRINT(" sol max", stp->st_max);
    }

    /* Ignore "dead" solids in the list.  (They failed prep) */
    if (stp->st_aradius <= 0)
	return 0;

    /* If the object fits in a box (i.e., it's not infinite), and that
     * box doesn't overlap with the bounding RPP, we know it's a miss.
     */
    if (stp->st_aradius < INFINITY) {
	if (V3RPP_DISJOINT(stp->st_min, stp->st_max, min, max))
	    return 0;
    }

    /* RPP overlaps, invoke per-solid method for detailed check */
    if (OBJ[stp->st_id].ft_classify &&
	OBJ[stp->st_id].ft_classify(stp, min, max, &rtip->rti_tol) == BG_CLASSIFY_OUTSIDE)
	return 0;

    /* don't know, check it */
    return 1;
}


/**
 * Returns the total number of solids and solid "pieces" in a boxnode.
 */
HIDDEN size_t
rt_ct_piececount(const union cutter *cutp)
{
    long i;
    size_t count;

    BU_ASSERT(cutp->cut_type == CUT_BOXNODE);

    count = cutp->bn.bn_len;

    if (cutp->bn.bn_piecelen <= 0 || !cutp->bn.bn_piecelist)
	return count;

    for (i = cutp->bn.bn_piecelen-1; i >= 0; i--) {
	count += cutp->bn.bn_piecelist[i].npieces;
    }
    return count;
}


/*
 * Optimize a cut tree.  Work on nodes which are over the pre-set
 * limits, subdividing until either the limit on tree depth runs out,
 * or until subdivision no longer gives different results, which could
 * easily be the case when several solids involved in a CSG operation
 * overlap in space.
 */
HIDDEN void
rt_ct_optim(struct rt_i *rtip, register union cutter *cutp, size_t depth)
{
    size_t oldlen;

    if (cutp->cut_type == CUT_CUTNODE) {
	rt_ct_optim(rtip, cutp->cn.cn_l, depth+1);
	rt_ct_optim(rtip, cutp->cn.cn_r, depth+1);
	return;
    }
    if (cutp->cut_type != CUT_BOXNODE) {
	bu_log("rt_ct_optim: bad node [%d]\n", cutp->cut_type);
	return;
    }

    oldlen = rt_ct_piececount(cutp);	/* save before rt_ct_box() */
    if (RT_G_DEBUG&RT_DEBUG_CUTDETAIL)
	bu_log("rt_ct_optim(cutp=%p, depth=%zu) piececount=%zu\n", (void *)cutp, depth, oldlen);

    /*
     * BOXNODE (leaf)
     */
    if (oldlen <= 1)
	return;		/* this box is already optimal */
    if (depth > rtip->rti_cutdepth) return;		/* too deep */

    /* Attempt to subdivide finer than rtip->rti_cutlen near treetop */
    /**** XXX This test can be improved ****/
    if (depth >= 6 && oldlen <= rtip->rti_cutlen)
	return;				/* Fine enough */

    /* Old (Release 3.7) way */
    {
	int did_a_cut;
	int i;
	int axis;
	double where, offcenter;
	/*
	 * In general, keep subdividing until things don't get any
	 * better.  Really we might want to proceed for 2-3 levels.
	 *
	 * First, make certain this is a worthwhile cut.  In absolute
	 * terms, each box must be at least 1mm wide after cut.
	 */
	axis = AXIS(depth);
	did_a_cut = 0;
	for (i=0; i<3; i++, axis += 1) {
	    if (axis > Z) {
		axis = X;
	    }
	    if (cutp->bn.bn_max[axis]-cutp->bn.bn_min[axis] < 2.0) {
		continue;
	    }
	    if (rt_ct_old_assess(cutp, axis, &where, &offcenter) <= 0) {
		continue;
	    }
	    if (rt_ct_box(rtip, cutp, axis, where, 0) == 0) {
		continue;
	    } else {
		did_a_cut = 1;
		break;
	    }
	}

	if (!did_a_cut) {
	    return;
	}
	if (rt_ct_piececount(cutp->cn.cn_l) >= oldlen &&
	    rt_ct_piececount(cutp->cn.cn_r) >= oldlen) {
	    if (RT_G_DEBUG&RT_DEBUG_CUTDETAIL)
		bu_log("rt_ct_optim(cutp=%p, depth=%zu) oldlen=%zu, lhs=%zu, rhs=%zu, hopeless\n",
		       (void *)cutp, depth, oldlen,
		       rt_ct_piececount(cutp->cn.cn_l),
		       rt_ct_piececount(cutp->cn.cn_r));
	    return; /* hopeless */
	}
    }

    /* Box node is now a cut node, recurse */
    rt_ct_optim(rtip, cutp->cn.cn_l, depth+1);
    rt_ct_optim(rtip, cutp->cn.cn_r, depth+1);
}


/**
 * NOTE: Changing from rt_ct_assess() to this seems to result in a
 * *massive* change in cut tree size.
 *
 * This version results in nbins=22, maxlen=3, avg=1.09, while new
 * version results in nbins=42, maxlen=3, avg=1.667 (on moss.g).
 */
HIDDEN int
rt_ct_old_assess(register union cutter *cutp, register int axis, double *where_p, double *offcenter_p)
{
    double val;
    double offcenter;		/* Closest distance from midpoint */
    double where;		/* Point closest to midpoint */
    double middle;		/* midpoint */
    double d;
    fastf_t max, min;
    register size_t i;
    long il;
    register double left, right;

    if (RT_G_DEBUG&RT_DEBUG_CUTDETAIL)
	bu_log("rt_ct_old_assess(%p, %c)\n", (void *)cutp, "XYZ345"[axis]);

    /* In absolute terms, each box must be at least 1mm wide after cut. */
    if ((right=cutp->bn.bn_max[axis])-(left=cutp->bn.bn_min[axis]) < 2.0)
	return 0;

    /*
     * Split distance between min and max in half.  Find the closest
     * edge of a solid's bounding RPP to the mid-point, and split
     * there.  This should ordinarily guarantee that at least one side
     * of the cut has one less item in it.
     */
    min = MAX_FASTF;
    max = -min;
    where = left;
    middle = (left + right) * 0.5;
    offcenter = middle - where;	/* how far off 'middle', 'where' is */
    for (i=0; i < cutp->bn.bn_len; i++) {
	val = cutp->bn.bn_list[i]->st_min[axis];
	if (val < min) min = val;
	if (val > max) max = val;
	d = val - middle;
	if (d < 0) d = (-d);
	if (d < offcenter) {
	    offcenter = d;
	    where = val-0.1;
	}
	val = cutp->bn.bn_list[i]->st_max[axis];
	if (val < min) min = val;
	if (val > max) max = val;
	d = val - middle;
	if (d < 0) d = (-d);
	if (d < offcenter) {
	    offcenter = d;
	    where = val+0.1;
	}
    }

    /* Loop over all the solid pieces */
    for (il = cutp->bn.bn_piecelen-1; il >= 0; il--) {
	struct rt_piecelist *plp = &cutp->bn.bn_piecelist[il];
	struct soltab *stp = plp->stp;
	int j;

	RT_CK_PIECELIST(plp);
	for (j = plp->npieces-1; j >= 0; j--) {
	    int indx = plp->pieces[j];
	    struct bound_rpp *rpp = &stp->st_piece_rpps[indx];

	    val = rpp->min[axis];
	    if (val < min) min = val;
	    if (val > max) max = val;
	    d = val - middle;
	    if (d < 0) d = (-d);
	    if (d < offcenter) {
		offcenter = d;
		where = val-0.1;
	    }
	    val = rpp->max[axis];
	    if (val < min) min = val;
	    if (val > max) max = val;
	    d = val - middle;
	    if (d < 0) d = (-d);
	    if (d < offcenter) {
		offcenter = d;
		where = val+0.1;
	    }
	}
    }

    if (RT_G_DEBUG&RT_DEBUG_CUTDETAIL)bu_log("rt_ct_old_assess() left=%g, where=%g, right=%g, offcenter=%g\n",

					  left, where, right, offcenter);

    if (where < min || where > max) {
	/* this will make an empty cell.  try splitting the range
	 * instead
	 */
	where = (max + min) / 2.0;
	offcenter = where - middle;
	if (offcenter < 0) {
	    offcenter = -offcenter;
	}
    }

    if (where <= left || where >= right)
	return 0;	/* not reasonable */

    if (where - left <= 1.0 || right - where <= 1.0)
	return 0;	/* cut will be too small */

    /* We are going to cut */
    *where_p = where;
    *offcenter_p = offcenter;
    return 1;
}


/*
 * This routine must run in parallel
 */
HIDDEN union cutter *
rt_ct_get(struct rt_i *rtip)
{
    register union cutter *cutp;

    RT_CK_RTI(rtip);
    bu_semaphore_acquire(RT_SEM_MODEL);
    if (!rtip->rti_busy_cutter_nodes.l.magic)
	bu_ptbl_init(&rtip->rti_busy_cutter_nodes, 128, "rti_busy_cutter_nodes");

    if (rtip->rti_CutFree == CUTTER_NULL) {
	size_t bytes;

	bytes = (size_t)bu_malloc_len_roundup(64*sizeof(union cutter));
	cutp = (union cutter *)bu_calloc(1, bytes, " rt_ct_get");
	/* Remember this allocation for later */
	bu_ptbl_ins(&rtip->rti_busy_cutter_nodes, (long *)cutp);
	/* Now, dice it up */
	while (bytes >= sizeof(union cutter)) {
	    cutp->cut_forw = rtip->rti_CutFree;
	    rtip->rti_CutFree = cutp++;
	    bytes -= sizeof(union cutter);
	}
    }
    cutp = rtip->rti_CutFree;
    rtip->rti_CutFree = cutp->cut_forw;
    bu_semaphore_release(RT_SEM_MODEL);

    cutp->cut_forw = CUTTER_NULL;
    return cutp;
}


/*
 * Release subordinate storage
 */
HIDDEN void
rt_ct_release_storage(register union cutter *cutp)
{
    size_t i;

    switch (cutp->cut_type) {

	case CUT_CUTNODE:
	    break;

	case CUT_BOXNODE:
	    if (cutp->bn.bn_list) {
		bu_free((char *)cutp->bn.bn_list, "bn_list[]");
		cutp->bn.bn_list = (struct soltab **)NULL;
	    }
	    cutp->bn.bn_len = 0;
	    cutp->bn.bn_maxlen = 0;

	    if (cutp->bn.bn_piecelist) {
		for (i=0; i<cutp->bn.bn_piecelen; i++) {
		    struct rt_piecelist *olp = &cutp->bn.bn_piecelist[i];
		    if (olp->pieces) {
			bu_free((char *)olp->pieces, "olp->pieces");
		    }
		}
		bu_free((char *)cutp->bn.bn_piecelist, "bn_piecelist[]");
		cutp->bn.bn_piecelist = (struct rt_piecelist *)NULL;
	    }
	    cutp->bn.bn_piecelen = 0;
	    cutp->bn.bn_maxpiecelen = 0;
	    break;

	default:
	    bu_log("rt_ct_release_storage: Unknown type [%d]\n", cutp->cut_type);
	    break;
    }
}


/*
 * This routine must run in parallel
 */
HIDDEN void
rt_ct_free(struct rt_i *rtip, register union cutter *cutp)
{
    RT_CK_RTI(rtip);

    rt_ct_release_storage(cutp);

    /* Put on global free list */
    bu_semaphore_acquire(RT_SEM_MODEL);
    cutp->cut_forw = rtip->rti_CutFree;
    rtip->rti_CutFree = cutp;
    bu_semaphore_release(RT_SEM_MODEL);
}


void
rt_pr_cut(const union cutter *cutp, int lvl)
{
    size_t i, j;

    bu_log("%p ", (void *)cutp);
    for (i=lvl; i>0; i--)
	bu_log("   ");

    if (cutp == CUTTER_NULL) {
	bu_log("Null???\n");
	return;
    }

    switch (cutp->cut_type) {

	case CUT_CUTNODE:
	    bu_log("CUT L %c < %f\n",
		   "XYZ?"[cutp->cn.cn_axis],
		   cutp->cn.cn_point);
	    rt_pr_cut(cutp->cn.cn_l, lvl+1);

	    bu_log("%p ", (void *)cutp);
	    for (i=lvl; i>0; i--)
		bu_log("   ");
	    bu_log("CUT R %c >= %f\n",
		   "XYZ?"[cutp->cn.cn_axis],
		   cutp->cn.cn_point);
	    rt_pr_cut(cutp->cn.cn_r, lvl+1);
	    return;

	case CUT_BOXNODE:
	    bu_log("BOX Contains %zu primitives (%zu alloc), %zu primitives with pieces:\n",
		   cutp->bn.bn_len, cutp->bn.bn_maxlen,
		   cutp->bn.bn_piecelen);
	    bu_log("        ");
	    for (i=lvl; i>0; i--)
		bu_log("   ");
	    VPRINT(" min", cutp->bn.bn_min);
	    bu_log("        ");
	    for (i=lvl; i>0; i--)
		bu_log("   ");
	    VPRINT(" max", cutp->bn.bn_max);

	    /* Print names of regular solids */
	    for (i=0; i < cutp->bn.bn_len; i++) {
		bu_log("        ");
		for (j=lvl; j>0; j--)
		    bu_log("   ");
		bu_log("    %s\n",
		       cutp->bn.bn_list[i]->st_name);
	    }

	    /* Print names and piece lists of solids with pieces */
	    for (i=0; i < cutp->bn.bn_piecelen; i++) {
		struct rt_piecelist *plp = &(cutp->bn.bn_piecelist[i]);
		struct soltab *stp;

		RT_CK_PIECELIST(plp);
		stp = plp->stp;
		RT_CK_SOLTAB(stp);

		bu_log("        ");
		for (j=lvl; j>0; j--)
		    bu_log("   ");
		bu_log("    %s, %ld pieces: ",
		       stp->st_name, plp->npieces);

		/* Loop for every piece of this solid */
		for (j=0; j < plp->npieces; j++) {
		    long indx = plp->pieces[j];
		    bu_log("%ld, ", indx);
		}
		bu_log("\n");
	    }
	    return;

	default:
	    bu_log("Unknown type [%d]\n", cutp->cut_type);
	    break;
    }
    return;
}


void
rt_fr_cut(struct rt_i *rtip, register union cutter *cutp)
{
    RT_CK_RTI(rtip);
    if (cutp == CUTTER_NULL) {
	bu_log("rt_fr_cut NULL\n");
	return;
    }

    switch (cutp->cut_type) {

	case CUT_CUTNODE:
	    rt_fr_cut(rtip, cutp->cn.cn_l);
	    rt_ct_free(rtip, cutp->cn.cn_l);
	    cutp->cn.cn_l = CUTTER_NULL;

	    rt_fr_cut(rtip, cutp->cn.cn_r);
	    rt_ct_free(rtip, cutp->cn.cn_r);
	    cutp->cn.cn_r = CUTTER_NULL;
	    return;

	case CUT_BOXNODE:
	    rt_ct_release_storage(cutp);
	    return;

	default:
	    bu_log("rt_fr_cut: Unknown type [%d]\n", cutp->cut_type);
	    break;
    }
    return;
}


HIDDEN void
rt_plot_cut(FILE *fp, struct rt_i *rtip, register union cutter *cutp, int lvl)
{
    RT_CK_RTI(rtip);
    switch (cutp->cut_type) {
	case CUT_CUTNODE:
	    rt_plot_cut(fp, rtip, cutp->cn.cn_l, lvl+1);
	    rt_plot_cut(fp, rtip, cutp->cn.cn_r, lvl+1);
	    return;
	case CUT_BOXNODE:
	    /* Should choose color based on lvl, need a table */
	    pl_color(fp,
		     (AXIS(lvl)==0)?255:0,
		     (AXIS(lvl)==1)?255:0,
		     (AXIS(lvl)==2)?255:0);
	    pdv_3box(fp, cutp->bn.bn_min, cutp->bn.bn_max);
	    return;
    }
    return;
}


/*
 * Find the maximum number of solids in a leaf node, and other
 * interesting statistics.
 */
HIDDEN void
rt_ct_measure(register struct rt_i *rtip, register union cutter *cutp, int depth)
{
    register int len;

    RT_CK_RTI(rtip);
    switch (cutp->cut_type) {
	case CUT_CUTNODE:
	    rtip->rti_ncut_by_type[CUT_CUTNODE]++;
	    rt_ct_measure(rtip, cutp->cn.cn_l, len = (depth+1));
	    rt_ct_measure(rtip, cutp->cn.cn_r, len);
	    return;
	case CUT_BOXNODE:
	    rtip->rti_ncut_by_type[CUT_BOXNODE]++;
	    rtip->rti_cut_totobj += (len = cutp->bn.bn_len);
	    if (rtip->rti_cut_maxlen < len)
		rtip->rti_cut_maxlen = len;
	    if (rtip->rti_cut_maxdepth < depth)
		rtip->rti_cut_maxdepth = depth;
	    BU_HIST_TALLY(&rtip->rti_hist_cellsize, len);
	    len = rt_ct_piececount(cutp) - len;
	    BU_HIST_TALLY(&rtip->rti_hist_cell_pieces, len);
	    BU_HIST_TALLY(&rtip->rti_hist_cutdepth, depth);
	    if (len == 0) {
		rtip->nempty_cells++;
	    }
	    return;
	default:
	    bu_log("rt_ct_measure: bad node [%d]\n", cutp->cut_type);
	    return;
    }
}


void
rt_cut_clean(struct rt_i *rtip)
{
    void **p;

    RT_CK_RTI(rtip);

    if (rtip->rti_cuts_waiting.l.magic)
	bu_ptbl_free(&rtip->rti_cuts_waiting);

    /* Abandon the linked list of diced-up structures */
    rtip->rti_CutFree = CUTTER_NULL;

    if (!BU_LIST_IS_INITIALIZED(&rtip->rti_busy_cutter_nodes.l))
	return;

    /* Release the blocks we got from bu_calloc() */
    for (BU_PTBL_FOR(p, (void **), &rtip->rti_busy_cutter_nodes)) {
	bu_free(*p, "rt_ct_get");
    }
    bu_ptbl_free(&rtip->rti_busy_cutter_nodes);
}


void
rt_pr_cut_info(const struct rt_i *rtip, const char *str)
{
    RT_CK_RTI(rtip);

    bu_log("%s %s: %d cut, %d box (%ld empty)\n",
	   str,
	   rtip->rti_space_partition == RT_PART_NUBSPT ?
	   "NUBSP" : "unknown",
	   rtip->rti_ncut_by_type[CUT_CUTNODE],
	   rtip->rti_ncut_by_type[CUT_BOXNODE],
	   rtip->nempty_cells);
    bu_log("Cut: maxdepth=%d, nbins=%d, maxlen=%d, avg=%g\n",
	   rtip->rti_cut_maxdepth,
	   rtip->rti_ncut_by_type[CUT_BOXNODE],
	   rtip->rti_cut_maxlen,
	   ((double)rtip->rti_cut_totobj) /
	   rtip->rti_ncut_by_type[CUT_BOXNODE]);
    bu_hist_pr(&rtip->rti_hist_cellsize,
	       "cut_tree: Number of primitives per leaf cell");
    bu_hist_pr(&rtip->rti_hist_cell_pieces,
	       "cut_tree: Number of primitive pieces per leaf cell");
    bu_hist_pr(&rtip->rti_hist_cutdepth,
	       "cut_tree: Depth (height)");
}


void
remove_from_bsp(struct soltab *stp, union cutter *cutp, struct bn_tol *tol)
{
    size_t idx;
    size_t i;

    switch (cutp->cut_type) {
	case CUT_BOXNODE:
	    if (stp->st_npieces) {
		int remove_count, new_count;
		struct rt_piecelist *new_piece_list;

		idx = 0;
		remove_count = 0;
		for (idx=0; idx<cutp->bn.bn_piecelen; idx++) {
		    if (cutp->bn.bn_piecelist[idx].stp == stp) {
			remove_count++;
		    }
		}

		if (remove_count) {
		    new_count = cutp->bn.bn_piecelen - remove_count;
		    if (new_count > 0) {
			new_piece_list = (struct rt_piecelist *)bu_calloc(
			    new_count,
			    sizeof(struct rt_piecelist),
			    "bn_piecelist");

			i = 0;
			for (idx=0; idx<cutp->bn.bn_piecelen; idx++) {
			    if (cutp->bn.bn_piecelist[idx].stp != stp) {
				new_piece_list[i] = cutp->bn.bn_piecelist[idx];
				i++;
			    }
			}
		    } else {
			new_count = 0;
			new_piece_list = NULL;
		    }

		    for (idx=0; idx<cutp->bn.bn_piecelen; idx++) {
			if (cutp->bn.bn_piecelist[idx].stp == stp) {
			    bu_free(cutp->bn.bn_piecelist[idx].pieces, "pieces");
			}
		    }
		    bu_free(cutp->bn.bn_piecelist, "piecelist");
		    cutp->bn.bn_piecelist = new_piece_list;
		    cutp->bn.bn_piecelen = new_count;
		    cutp->bn.bn_maxpiecelen = new_count;
		}
	    } else {
		for (idx=0; idx < cutp->bn.bn_len; idx++) {
		    if (cutp->bn.bn_list[idx] == stp) {
			/* found it, now remove it */
			cutp->bn.bn_len--;
			for (i=idx; i < cutp->bn.bn_len; i++) {
			    cutp->bn.bn_list[i] = cutp->bn.bn_list[i+1];
			}
			return;
		    }
		}
	    }
	    break;
	case CUT_CUTNODE:
	    if (stp->st_min[cutp->cn.cn_axis] > cutp->cn.cn_point + tol->dist) {
		remove_from_bsp(stp, cutp->cn.cn_r, tol);
	    } else if (stp->st_max[cutp->cn.cn_axis] < cutp->cn.cn_point - tol->dist) {
		remove_from_bsp(stp, cutp->cn.cn_l, tol);
	    } else {
		remove_from_bsp(stp, cutp->cn.cn_r, tol);
		remove_from_bsp(stp, cutp->cn.cn_l, tol);
	    }
	    break;
	default:
	    bu_log("remove_from_bsp(): unrecognized cut type (%d) in BSP!\n", cutp->cut_type);
	    bu_bomb("remove_from_bsp(): unrecognized cut type in BSP!\n");
    }
}


#define PIECE_BLOCK 512

void
insert_in_bsp(struct soltab *stp, union cutter *cutp)
{
    int i, j;

    switch (cutp->cut_type) {
	case CUT_BOXNODE:
	    if (stp->st_npieces == 0) {
		/* add the solid in this box */
		if (cutp->bn.bn_len >= cutp->bn.bn_maxlen) {
		    /* need more space */
		    if (cutp->bn.bn_maxlen <= 0) {
			/* Initial allocation */
			cutp->bn.bn_maxlen = 5;
			cutp->bn.bn_list = (struct soltab **)bu_calloc(
			    cutp->bn.bn_maxlen, sizeof(struct soltab *),
			    "insert_in_bsp: initial list alloc");
		    } else {
			cutp->bn.bn_maxlen += 5;
			cutp->bn.bn_list = (struct soltab **) bu_realloc(
			    (void *)cutp->bn.bn_list,
			    sizeof(struct soltab *) * cutp->bn.bn_maxlen,
			    "insert_in_bsp: list extend");
		    }
		}
		cutp->bn.bn_list[cutp->bn.bn_len++] = stp;

	    } else {
		/* this solid uses pieces, add the appropriate pieces to this box */
		long pieces[PIECE_BLOCK];
		long *more_pieces=NULL;
		long more_pieces_alloced=0;
		long more_pieces_count=0;
		long piece_count=0;
		struct rt_piecelist *plp;

		for (i=0; i<stp->st_npieces; i++) {
		    struct bound_rpp *piece_rpp=&stp->st_piece_rpps[i];
		    if (V3RPP_OVERLAP(piece_rpp->min, piece_rpp->max, cutp->bn.bn_min, cutp->bn.bn_max)) {
			if (piece_count < PIECE_BLOCK) {
			    pieces[piece_count++] = i;
			} else if (more_pieces_alloced == 0) {
			    more_pieces_alloced = stp->st_npieces - PIECE_BLOCK;
			    more_pieces = (long *)bu_calloc(more_pieces_alloced, sizeof(long),
							    "more_pieces");
			    more_pieces[more_pieces_count++] = i;
			} else {
			    more_pieces[more_pieces_count++] = i;
			}
		    }
		}

		if (cutp->bn.bn_piecelen >= cutp->bn.bn_maxpiecelen) {
		    cutp->bn.bn_piecelist = (struct rt_piecelist *)bu_realloc(cutp->bn.bn_piecelist,
									      sizeof(struct rt_piecelist) * (++cutp->bn.bn_maxpiecelen),
									      "cutp->bn.bn_piecelist");
		}

		if (!piece_count) {
		    return;
		}

		plp = &cutp->bn.bn_piecelist[cutp->bn.bn_piecelen++];
		plp->magic = RT_PIECELIST_MAGIC;
		plp->stp = stp;
		plp->npieces = piece_count + more_pieces_count;
		plp->pieces = (long *)bu_calloc(plp->npieces, sizeof(long), "plp->pieces");
		for (i=0; i<piece_count; i++) {
		    plp->pieces[i] = pieces[i];
		}
		j = piece_count;
		for (i=0; i<more_pieces_count; i++) {
		    plp->pieces[j++] = more_pieces[i];
		}

		if (more_pieces) {
		    bu_free((char *)more_pieces, "more_pieces");
		}
	    }
	    break;
	case CUT_CUTNODE:
	    if (stp->st_min[cutp->cn.cn_axis] >= cutp->cn.cn_point) {
		insert_in_bsp(stp, cutp->cn.cn_r);
	    } else if (stp->st_max[cutp->cn.cn_axis] < cutp->cn.cn_point) {
		insert_in_bsp(stp, cutp->cn.cn_l);
	    } else {
		insert_in_bsp(stp, cutp->cn.cn_r);
		insert_in_bsp(stp, cutp->cn.cn_l);
	    }
	    break;
	default:
	    bu_log("insert_in_bsp(): unrecognized cut type (%d) in BSP!\n", cutp->cut_type);
	    bu_bomb("insert_in_bsp(): unrecognized cut type in BSP!\n");
    }

}

void
fill_out_bsp(struct rt_i *rtip, union cutter *cutp, struct resource *resp, fastf_t bb[6])
{
    fastf_t bb2[6];
    int i, j;

    switch (cutp->cut_type) {
	case CUT_BOXNODE:
	    j = 3;
	    for (i=0; i<3; i++) {
		if (bb[i] >= INFINITY) {
		    /* this node is at the edge of the model BB, make it fill the BB */
		    cutp->bn.bn_min[i] = rtip->mdl_min[i];
		}
		if (bb[j] <= -INFINITY) {
		    /* this node is at the edge of the model BB, make it fill the BB */
		    cutp->bn.bn_max[i] = rtip->mdl_max[i];
		}
		j++;
	    }
	    break;
	case CUT_CUTNODE:
	    VMOVE(bb2, bb);
	    VMOVE(&bb2[3], &bb[3]);
	    bb[cutp->cn.cn_axis] = cutp->cn.cn_point;
	    fill_out_bsp(rtip, cutp->cn.cn_r, resp, bb);
	    bb2[cutp->cn.cn_axis + 3] = cutp->cn.cn_point;
	    fill_out_bsp(rtip, cutp->cn.cn_l, resp, bb2);
	    break;
	default:
	    bu_log("fill_out_bsp(): unrecognized cut type (%d) in BSP!\n", cutp->cut_type);
	    bu_bomb("fill_out_bsp(): unrecognized cut type in BSP!\n");
    }

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
