/*                   C U T _ N U B S P . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2026 United States Government as represented by
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
/** @file librt/cut_nubsp.c
 *
 * Mike Muuss' original non-uniform binary space partitioning tree.
 *
 * The method starts with a single model-sized CUT_BOXNODE populated by
 * rt_cut_it().  Leaves that contain enough primitive or primitive-piece
 * entries are recursively split by axis-aligned planes.  Splits cycle
 * through X, Y, and Z, choose a plane near the midpoint but snapped just
 * outside nearby primitive bounds, and stop when the leaf is already small,
 * the tree is too deep, the cell is too narrow, or a proposed split does
 * not reduce the amount of work in either child.
 *
 * A final pass splits cells that are mostly empty so rays can skip through
 * large empty regions.  Ray traversal remains in shoot.c: it descends
 * CUT_CUTNODEs to a CUT_BOXNODE and shoots only the primitives listed in
 * that leaf.
 */
/** @} */

#include "common.h"

#include <math.h>

#include "bu/parallel.h"
#include "vmath.h"
#include "raytrace.h"
#include "bg/plane.h"
#include "cut_private.h"


#define AXIS(depth)	((depth)%3)	/* cuts: X, Y, Z, repeat */
#define PIECE_BLOCK 512


static int rt_ck_overlap(const vect_t min, const vect_t max, const struct soltab *stp, const struct rt_i *rtip);
static int rt_ct_box(struct rt_i *rtip, union cutter *cutp, int axis, double where, int force);
static int rt_ct_old_assess(register union cutter *, register int, double *, double *);
static int rt_ct_populate_box(union cutter *outp, const union cutter *inp, struct rt_i *rtip);
static size_t split_mostly_empty_cells(struct rt_i *rtip, union cutter *cutp);
static void rt_ct_optim(struct rt_i *rtip, union cutter *cutp, size_t depth);


/**
 * Process all the nodes in the global array rtip->i->rti_cuts_waiting,
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
	i = rtip->i->rti_cuts_waiting.end--;	/* get first free index */
	bu_semaphore_release(RT_SEM_WORKER);
	i -= 1;				/* change to last used index */

	if (i < 0) break;

	cp = (union cutter *)BU_PTBL_GET(&rtip->i->rti_cuts_waiting, i);

	rt_ct_optim(rtip, cp, Z);
    }
}


void
rt_cut_nubsp_build(struct rt_i *rtip, const union cutter *root, int UNUSED(ncpu))
{
    size_t num_splits = 0;

    RT_CK_RTI(rtip);
    BU_ASSERT(root->cut_type == CUT_BOXNODE);

    /* Dynamic decisions on tree limits.  Note that there will be
     * (2**rtip->i->rti_cutdepth)*rtip->i->rti_cutlen potential leaf slots.
     * Also note that solids will typically span several leaves.
     */
    rtip->i->rti_cutlen = lrint(floor(log((double)(rtip->stats.nsolids+1))));  /* ln ~= log2, nsolids+1 to avoid log(0) */
    rtip->i->rti_cutdepth = 2 * rtip->i->rti_cutlen;
    if (rtip->i->rti_cutlen < 3) rtip->i->rti_cutlen = 3;
    if (rtip->i->rti_cutdepth < 12) rtip->i->rti_cutdepth = 12;
    if (rtip->i->rti_cutdepth > 24) rtip->i->rti_cutdepth = 24;     /* !! */

    if (rtip->rti_hasty_prep) {
	rtip->i->rti_cutdepth = 6;
    }

    if (RT_G_DEBUG&RT_DEBUG_CUT)
	bu_log("Before Space Partitioning: Max Tree Depth=%zu, Cutoff primitive count=%zu\n",
	       rtip->i->rti_cutdepth, rtip->i->rti_cutlen);

    bu_ptbl_init(&rtip->i->rti_cuts_waiting, rtip->stats.nsolids,
		 "rti_cuts_waiting ptbl");

    rtip->i->rti_CutHead = *root;	/* union copy */
    rt_ct_optim(rtip, &rtip->i->rti_CutHead, 0);

    /* one more pass to find cells that are mostly empty */
    num_splits = split_mostly_empty_cells(rtip,  &rtip->i->rti_CutHead);

    if (RT_G_DEBUG&RT_DEBUG_CUT) {
	bu_log("split_mostly_empty_cells(): split %zu cells\n", num_splits);
    }
}


static size_t
split_mostly_empty_cells(struct rt_i *rtip, union cutter *cutp)
{
    point_t max, min;
    struct soltab *stp;
    struct rt_piecelist pl;
    fastf_t range[3], empty[3], tmp;
    int upper_or_lower[3];
    fastf_t max_empty;
    int max_empty_dir;
    size_t i;
    size_t num_splits=0;

    switch (cutp->cut_type) {
	case CUT_CUTNODE:
	    num_splits += split_mostly_empty_cells(rtip, cutp->cn.cn_l);
	    num_splits += split_mostly_empty_cells(rtip, cutp->cn.cn_r);
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
		    num_splits += split_mostly_empty_cells(rtip, cutp->cn.cn_l);
		    num_splits += split_mostly_empty_cells(rtip, cutp->cn.cn_r);
		}
	    }
	    break;
    }

    return num_splits;
}


/**
 * Given that 'outp' has been given a bounding box smaller than that
 * of 'inp', copy over everything which still fits in the smaller box.
 *
 * Returns -
 * 0 if outp has the same number of items as inp
 * 1 if outp has fewer items than inp
 */
static int
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
static int
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
static int
rt_ck_overlap(const vect_t min, const vect_t max, const struct soltab *stp, const struct rt_i *rtip)
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


/*
 * Optimize a cut tree.  Work on nodes which are over the pre-set
 * limits, subdividing until either the limit on tree depth runs out,
 * or until subdivision no longer gives different results, which could
 * easily be the case when several solids involved in a CSG operation
 * overlap in space.
 */
static void
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
    if (depth > rtip->i->rti_cutdepth) return;		/* too deep */

    /* Attempt to subdivide finer than rtip->i->rti_cutlen near treetop */
    /**** XXX This test can be improved ****/
    if (depth >= 6 && oldlen <= rtip->i->rti_cutlen)
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
static int
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
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
