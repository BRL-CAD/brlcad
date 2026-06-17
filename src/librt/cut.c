/*                           C U T . C
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
/** @file librt/cut.c
 *
 * Shared spatial partitioning setup, dispatch, and CUT_BOXNODE /
 * CUT_CUTNODE support routines.  Individual partitioning methods live in
 * separate cut_*.c files.
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu/parallel.h"
#include "bu/sort.h"
#include "bu/str.h"
#include "vmath.h"
#include "raytrace.h"
#include "bg/plane.h"
#include "bsg/plot3.h"
#include "cut_private.h"


static void rt_ct_measure(struct rt_i *rtip, union cutter *cutp, size_t depth);
static void rt_plot_cut(FILE *fp, struct rt_i *rtip, union cutter *cutp, int lvl);

#define AXIS(depth)	((depth)%3)	/* cuts: X, Y, Z, repeat */


const char *
rt_cut_method_name(int method)
{
    switch (method) {
	case RT_PART_NUBSPT:
	    return "NUBSP";
	case RT_PART_NULL:
	    return "NULL";
	default:
	    return "unknown";
    }
}


void
rt_cut_select_from_env(struct rt_i *rtip)
{
    const char *method;

    RT_CK_RTI(rtip);

    /* Prep-time only: never consult the environment from the ray hot path. */
    method = getenv("LIBRT_SPACE_PARTITION");
    if (!method || !method[0])
	return;

    if (BU_STR_EQUIV(method, "default") ||
	BU_STR_EQUIV(method, "nubsp") ||
	BU_STR_EQUIV(method, "bsp") ||
	BU_STR_EQUAL(method, "0"))
    {
	rtip->rti_space_partition = RT_PART_NUBSPT;
	return;
    }

    if (BU_STR_EQUIV(method, "null") ||
	BU_STR_EQUIV(method, "none") ||
	BU_STR_EQUIV(method, "noop") ||
	BU_STR_EQUIV(method, "no-op") ||
	BU_STR_EQUAL(method, "1"))
    {
	rtip->rti_space_partition = RT_PART_NULL;
	return;
    }

    bu_log("WARNING: unknown LIBRT_SPACE_PARTITION value '%s', using %s\n",
	   method, rt_cut_method_name(rtip->rti_space_partition));
}


void
rt_cut_it(register struct rt_i *rtip, int ncpu)
{
    register struct soltab *stp;
    union cutter *finp;	/* holds the finite solids */
    FILE *plotfp;

    /* Make a list of all solids into one special boxnode, then refine. */
    BU_ALLOC(finp, union cutter);
    finp->cut_type = CUT_BOXNODE;
    VMOVE(finp->bn.bn_min, rtip->mdl_min);
    VMOVE(finp->bn.bn_max, rtip->mdl_max);
    finp->bn.bn_len = 0;
    finp->bn.bn_maxlen = rtip->stats.nsolids+1;
    finp->bn.bn_list = (struct soltab **)bu_calloc(
	finp->bn.bn_maxlen, sizeof(struct soltab *),
	"rt_cut_it: initial list alloc");

    rtip->i->rti_inf_box.cut_type = CUT_BOXNODE;

    RT_VISIT_ALL_SOLTABS_START(stp, rtip) {
	/* Ignore "dead" solids in the list.  (They failed prep) */
	if (stp->st_aradius <= 0) continue;

	/* Infinite and finite solids all get lumped together */
	rt_cut_extend(finp, stp, rtip);

	if (stp->st_aradius >= INFINITY) {
	    /* Also add infinite solids to a special BOXNODE */
	    rt_cut_extend(&rtip->i->rti_inf_box, stp, rtip);
	}
    } RT_VISIT_ALL_SOLTABS_END;

    switch (rtip->rti_space_partition) {
	case RT_PART_NUBSPT:
	    rt_cut_nubsp_build(rtip, finp, ncpu);
	    break;
	case RT_PART_NULL:
	    rt_cut_null_build(rtip, finp, ncpu);
	    break;
	default:
	    bu_bomb("rt_cut_it: unknown space partitioning method\n");
    }

    bu_free(finp, "union cutter");

    /* Measure the depth of tree, find max # of RPPs in a cut node */

    bu_hist_init(&rtip->i->rti_hist_cellsize, 0.0, 400.0, 400);
    bu_hist_init(&rtip->i->rti_hist_cell_pieces, 0.0, 400.0, 400);
    bu_hist_init(&rtip->i->rti_hist_cutdepth, 0.0,
		 (fastf_t)rtip->i->rti_cutdepth+1, rtip->i->rti_cutdepth+1);
    memset(rtip->stats.rti_ncut_by_type, 0, sizeof(rtip->stats.rti_ncut_by_type));
    rt_ct_measure(rtip, &rtip->i->rti_CutHead, 0);
    if (RT_G_DEBUG&RT_DEBUG_CUT) {
	rt_pr_cut_info(rtip, "Cut");
    }

    if (RT_G_DEBUG&RT_DEBUG_CUTDETAIL) {
	/* Produce a voluminous listing of the cut tree */
	rt_pr_cut(&rtip->i->rti_CutHead, 0);
    }

    if (RT_G_DEBUG&RT_DEBUG_PL_BOX) {
	/* Debugging code to plot cuts */
	if ((plotfp=fopen("rtcut.plot3", "wb"))!=NULL) {
	    pdv_3space(plotfp, rtip->rti_pmin, rtip->rti_pmax);
	    /* Plot all the cutting boxes */
	    rt_plot_cut(plotfp, rtip, &rtip->i->rti_CutHead, 0);
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
	    BU_ASSERT(rtip->stats.nsolids > 0);
	    cutp->bn.bn_piecelist = (struct rt_piecelist *) bu_calloc(
		sizeof(struct rt_piecelist), (rtip->stats.nsolids + 2),
		"rt_ct_box bn_piecelist (root node)");
	    cutp->bn.bn_piecelen = 0;	/* sanity */
	    cutp->bn.bn_maxpiecelen = rtip->stats.nsolids + 2;
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
	    if (rtip->i->rti_cutlen > rtip->stats.nsolids)
		cutp->bn.bn_maxlen = rtip->i->rti_cutlen;
	    else
		cutp->bn.bn_maxlen = rtip->stats.nsolids + 2;
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


/**
 * Returns the total number of solids and solid "pieces" in a boxnode.
 */
size_t
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
 * This routine must run in parallel
 */
union cutter *
rt_ct_get(struct rt_i *rtip)
{
    register union cutter *cutp;

    RT_CK_RTI(rtip);
    bu_semaphore_acquire(RT_SEM_MODEL);
    if (!rtip->i->rti_busy_cutter_nodes.l.magic)
	bu_ptbl_init(&rtip->i->rti_busy_cutter_nodes, 128, "rti_busy_cutter_nodes");

    if (rtip->i->rti_CutFree == CUTTER_NULL) {
	size_t bytes;

	//bytes = (size_t)bu_malloc_len_roundup(64*sizeof(union cutter));
	bytes = sizeof(union cutter);
	cutp = (union cutter *)bu_calloc(1, bytes, " rt_ct_get");
	/* Remember this allocation for later */
	bu_ptbl_ins(&rtip->i->rti_busy_cutter_nodes, (long *)cutp);
	/* Now, dice it up */
	while (bytes >= sizeof(union cutter)) {
	    cutp->cut_forw = rtip->i->rti_CutFree;
	    rtip->i->rti_CutFree = cutp++;
	    bytes -= sizeof(union cutter);
	}
    }
    cutp = rtip->i->rti_CutFree;
    rtip->i->rti_CutFree = cutp->cut_forw;
    bu_semaphore_release(RT_SEM_MODEL);

    cutp->cut_forw = CUTTER_NULL;
    return cutp;
}


/*
 * Release subordinate storage
 */
void
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
void
rt_ct_free(struct rt_i *rtip, register union cutter *cutp)
{
    RT_CK_RTI(rtip);

    rt_ct_release_storage(cutp);

    /* Put on global free list */
    bu_semaphore_acquire(RT_SEM_MODEL);
    cutp->cut_forw = rtip->i->rti_CutFree;
    rtip->i->rti_CutFree = cutp;
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
		bu_log("    %s, %zu pieces: ",
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


static void
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
static void
rt_ct_measure(register struct rt_i *rtip, register union cutter *cutp, size_t depth)
{
    register size_t len;

    RT_CK_RTI(rtip);
    switch (cutp->cut_type) {
	case CUT_CUTNODE:
	    rtip->stats.rti_ncut_by_type[CUT_CUTNODE]++;
	    rt_ct_measure(rtip, cutp->cn.cn_l, len = (depth+1));
	    rt_ct_measure(rtip, cutp->cn.cn_r, len);
	    return;
	case CUT_BOXNODE:
	    rtip->stats.rti_ncut_by_type[CUT_BOXNODE]++;
	    len = cutp->bn.bn_len;
	    rtip->stats.rti_cut_totobj += len;
	    if (rtip->stats.rti_cut_maxlen < len)
		rtip->stats.rti_cut_maxlen = len;
	    if (rtip->stats.rti_cut_maxdepth < depth)
		rtip->stats.rti_cut_maxdepth = depth;
	    BU_HIST_TALLY(&rtip->i->rti_hist_cellsize, len);
	    len = rt_ct_piececount(cutp) - len;
	    BU_HIST_TALLY(&rtip->i->rti_hist_cell_pieces, len);
	    BU_HIST_TALLY(&rtip->i->rti_hist_cutdepth, depth);
	    if (len == 0) {
		rtip->stats.nempty_cells++;
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

    if (rtip->i->rti_cuts_waiting.l.magic)
	bu_ptbl_free(&rtip->i->rti_cuts_waiting);

    /* Abandon the linked list of diced-up structures */
    rtip->i->rti_CutFree = CUTTER_NULL;

    if (!BU_LIST_IS_INITIALIZED(&rtip->i->rti_busy_cutter_nodes.l))
	return;

    /* Release the blocks we got from bu_calloc() */
    for (BU_PTBL_FOR(p, (void **), &rtip->i->rti_busy_cutter_nodes)) {
	bu_free(*p, "rt_ct_get");
    }
    bu_ptbl_free(&rtip->i->rti_busy_cutter_nodes);
}


void
rt_pr_cut_info(const struct rt_i *rtip, const char *str)
{
    RT_CK_RTI(rtip);

    bu_log("%s %s: %zu cut, %zu box (%zu empty)\n",
	   str,
	   rt_cut_method_name(rtip->rti_space_partition),
	   rtip->stats.rti_ncut_by_type[CUT_CUTNODE],
	   rtip->stats.rti_ncut_by_type[CUT_BOXNODE],
	   rtip->stats.nempty_cells);
    bu_log("Cut: maxdepth=%zu, nbins=%zu, maxlen=%zu, avg=%g\n",
	   rtip->stats.rti_cut_maxdepth,
	   rtip->stats.rti_ncut_by_type[CUT_BOXNODE],
	   rtip->stats.rti_cut_maxlen,
	   rtip->stats.rti_ncut_by_type[CUT_BOXNODE] ?
	   ((double)rtip->stats.rti_cut_totobj) /
	   rtip->stats.rti_ncut_by_type[CUT_BOXNODE] : 0.0);
    bu_hist_pr(&rtip->i->rti_hist_cellsize,
	       "cut_tree: Number of primitives per leaf cell");
    bu_hist_pr(&rtip->i->rti_hist_cell_pieces,
	       "cut_tree: Number of primitive pieces per leaf cell");
    bu_hist_pr(&rtip->i->rti_hist_cutdepth,
	       "cut_tree: Depth (height)");
}


void
remove_from_bsp(struct soltab *stp, union cutter *cutp, struct bn_tol *tol)
{
    switch (cutp->cut_type) {
	case CUT_BOXNODE:
	    if (stp->st_npieces) {
		size_t remove_count, new_count;
		struct rt_piecelist *new_piece_list;

		remove_count = 0;
		for (size_t idx = 0; idx<cutp->bn.bn_piecelen; idx++) {
		    if (cutp->bn.bn_piecelist[idx].stp == stp) {
			remove_count++;
		    }
		}

		if (remove_count) {
		    new_count = cutp->bn.bn_piecelen - remove_count;
		    if (cutp->bn.bn_piecelen - remove_count > 0) {
			new_piece_list = (struct rt_piecelist *)bu_calloc(
			    new_count,
			    sizeof(struct rt_piecelist),
			    "bn_piecelist");

			size_t i = 0;
			for (size_t idx = 0; idx<cutp->bn.bn_piecelen; idx++) {
			    if (cutp->bn.bn_piecelist[idx].stp != stp) {
				new_piece_list[i] = cutp->bn.bn_piecelist[idx];
				i++;
			    }
			}
		    } else {
			new_count = 0;
			new_piece_list = NULL;
		    }

		    for (size_t idx = 0; idx<cutp->bn.bn_piecelen; idx++) {
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
		for (size_t idx = 0; idx < cutp->bn.bn_len; idx++) {
		    if (cutp->bn.bn_list[idx] == stp) {
			/* found it, now remove it */
			cutp->bn.bn_len--;
			for (size_t i = idx; i < cutp->bn.bn_len; i++) {
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
nfill_out_bsp(struct rt_i *rtip, union cutter *cutp, fastf_t bb[6])
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
	    nfill_out_bsp(rtip, cutp->cn.cn_r, bb);
	    bb2[cutp->cn.cn_axis + 3] = cutp->cn.cn_point;
	    nfill_out_bsp(rtip, cutp->cn.cn_l, bb2);
	    break;
	default:
	    bu_log("nfill_out_bsp(): unrecognized cut type (%d) in BSP!\n", cutp->cut_type);
	    bu_bomb("nfill_out_bsp(): unrecognized cut type in BSP!\n");
    }

}

void
fill_out_bsp(struct rt_i *rtip, union cutter *cutp, struct resource *UNUSED(resp), fastf_t bb[6])
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
	    nfill_out_bsp(rtip, cutp->cn.cn_r, bb);
	    bb2[cutp->cn.cn_axis + 3] = cutp->cn.cn_point;
	    nfill_out_bsp(rtip, cutp->cn.cn_l, bb2);
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
