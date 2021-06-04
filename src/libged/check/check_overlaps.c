/*                C H E C K _ O V E R L A P S . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2021 United States Government as represented by
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

#include "common.h"
#include "ged.h"

#include <stdlib.h>
#include <string.h>

#include "../ged_private.h"
#include "./check_private.h"

struct ged_check_plot {
    struct bv_vlblock *vbp;
    struct bu_list *vhead;
};


struct overlap_list {
    struct bu_list l;
    char *reg1; 	/* overlapping region 1 */
    char *reg2; 	/* overlapping region 2 */
    size_t count;	/* number of time reported */
    double maxdepth;	/* maximum overlap depth */
};


struct overlaps_context {
    struct overlap_list *overlapList;
    size_t noverlaps;		/* Number of overlaps seen */
    size_t overlap_count;	/* Number of overlap pairs seen */
    size_t unique_overlap_count; /* Number of unique overlap pairs seen */
    int overlaps_overlay_flag;
    int rpt_overlap_flag;
    FILE *plot_overlaps;
    int overlap_color[3];
    struct ged_check_plot *overlaps_overlay_plot;
    int sem_stats;
    int sem_lists;
};


HIDDEN void
check_log_overlaps(const char *reg1, const char *reg2, double depth, vect_t ihit, vect_t ohit, void *context)
{
    struct overlaps_context *callbackdata = (struct overlaps_context*)context;
    struct overlap_list *olist= (struct overlap_list *)callbackdata->overlapList;
    struct overlap_list *new_op;
    struct overlap_list *op;

    bu_semaphore_acquire(callbackdata->sem_stats);
    callbackdata->noverlaps++;
    bu_semaphore_release(callbackdata->sem_stats);

    if (!callbackdata->rpt_overlap_flag) {
	bu_vls_printf(_ged_current_gedp->ged_result_str,
		      "OVERLAP %zu: %s\nOVERLAP %zu: %s\nOVERLAP %zu: depth %gmm\nOVERLAP %zu: in_hit_point (%g, %g, %g) mm\nOVERLAP %zu: out_hit_point (%g, %g, %g) mm\n------------------------------------------------------------\n",
		      callbackdata->noverlaps, reg1,
		      callbackdata->noverlaps, reg2,
		      callbackdata->noverlaps, depth,
		      callbackdata->noverlaps, V3ARGS(ihit),
		      callbackdata->noverlaps, V3ARGS(ohit));
	/* If we report overlaps, don't print if already noted once.
	 * Build up a linked list of known overlapping regions and compare
	 * against it.
	 */
    } else {
	BU_GET(new_op, struct overlap_list);
	bu_semaphore_acquire(callbackdata->sem_stats);
	for (BU_LIST_FOR(op, overlap_list, &(olist->l))) {
	    if ((BU_STR_EQUAL(reg1, op->reg1)) && (BU_STR_EQUAL(reg2, op->reg2))) {
		op->count++;
		if (depth > op->maxdepth)
		    op->maxdepth = depth;
		bu_semaphore_release(callbackdata->sem_stats);
		bu_free((char *) new_op, "overlap list");
		return;
	    }
	}

	for (BU_LIST_FOR(op, overlap_list, &(olist->l))) {
	    /* if this pair was seen in reverse, decrease the unique counter */
	    if ((BU_STR_EQUAL(reg1, op->reg2)) && (BU_STR_EQUAL(reg2, op->reg1))) {
		callbackdata->unique_overlap_count--;
		break;
	    }
	}

	/* we have a new overlapping region pair */
	callbackdata->overlap_count++;
	callbackdata->unique_overlap_count++;
	new_op->reg1 = (char *)bu_malloc(strlen(reg1)+1, "reg1");
	new_op->reg2 = (char *)bu_malloc(strlen(reg2)+1, "reg2");
	bu_strlcpy(new_op->reg1, reg1, strlen(reg1)+1);
	bu_strlcpy(new_op->reg2, reg2, strlen(reg2)+1);
	new_op->maxdepth = depth;
	new_op->count = 1;
	BU_LIST_INSERT(&(olist->l), &(new_op->l));
	bu_semaphore_release(callbackdata->sem_stats);
    }
}


HIDDEN void
printOverlaps(void *context, struct check_parameters *options)
{
    struct overlaps_context *overlapData = (struct overlaps_context*)context;
    struct overlap_list *olist= (struct overlap_list *)overlapData->overlapList;
    struct overlap_list *op;
    struct overlap_list *backop;
    struct overlap_list *nextop;
    size_t object_counter = 0;

    if (overlapData->rpt_overlap_flag) {
	struct bu_vls str = BU_VLS_INIT_ZERO;
	/* using counters instead of the actual variables to be able to
	 * summarize after checking for matching pairs
	 */
	size_t overlap_counter = overlapData->overlap_count;
	size_t unique_overlap_counter = overlapData->unique_overlap_count;

	/* iterate over the overlap pairs and output one OVERLAP section
	 * per unordered pair.  a summary is output at the end.
	 */
	bu_vls_printf(&str, "OVERLAP PAIRS\n------------------------------------------\n");
	for (BU_LIST_FOR(op, overlap_list, &(olist->l))) {

	    for (BU_LIST_FOR_BACKWARDS(backop, overlap_list, &(op->l))) {
		if ((BU_STR_EQUAL(op->reg2, backop->reg1)) && (BU_STR_EQUAL(op->reg1, backop->reg2))) break;
		if (BU_LIST_IS_HEAD(&(backop->l), &(olist->l))) break;
	    }
	    if (backop && BU_LIST_NOT_HEAD(&(backop->l), &(olist->l))) continue;

	    bu_vls_printf(&str, "%s and %s overlap\n", op->reg1, op->reg2);

	    nextop = (struct overlap_list *)NULL;
	    /* if there are still matching pairs to search for */
	    if (overlap_counter > unique_overlap_counter) {
		/* iterate until end of pairs or we find a
		 * reverse matching pair (done inside loop
		 * explicitly)*/
		BU_LIST_TAIL(&(olist->l), op, nextop, struct overlap_list) {
		    if ((BU_STR_EQUAL(op->reg1, nextop->reg2)) && (BU_STR_EQUAL(op->reg2, nextop->reg1)))
			break;
		}
		/* when we leave the loop, nextop is either
		 * null (hit tail of list) or the matching
		 * reverse pair */
	    }

	    bu_vls_printf(&str, "\t<%s, %s>: %zu overlap%c detected, maximum depth is %g %s\n",
			  op->reg1, op->reg2, op->count, op->count>1 ? 's' : (char) 0, op->maxdepth/options->units[LINE]->val, options->units[LINE]->name);
	    if (nextop && BU_LIST_NOT_HEAD(nextop, &(olist->l))) {
		    bu_vls_printf(&str,
				  "\t<%s, %s>: %zu overlap%c detected, maximum depth is %g %s\n",
				  nextop->reg1, nextop->reg2, nextop->count,
				  nextop->count > 1 ? 's' : (char)0, nextop->maxdepth/options->units[LINE]->val, options->units[LINE]->name);
		    /* counter the decrement below to account for
		 * the matched reverse pair
		 */
		    unique_overlap_counter++;
	    }

	    /* decrement so we may stop scanning for unique overlaps asap */
	    unique_overlap_counter--;
	    overlap_counter--;
	}

	if (overlapData->noverlaps) {
	    bu_vls_printf(&str, "==========================================\n");
	    bu_vls_printf(&str, "SUMMARY\n");

	    bu_vls_printf(&str, "\t%zu overlap%s detected\n",
			  overlapData->noverlaps, (overlapData->noverlaps == 1) ? "" : "s");
	    bu_vls_printf(&str, "\t%zu unique overlapping pair%s (%zu ordered pair%s)\n",
			  overlapData->unique_overlap_count,
			  (overlapData->unique_overlap_count == 1) ? "" : "s",
			  overlapData->overlap_count, (overlapData->overlap_count == 1) ? "" : "s");

	    if (olist) {
		bu_vls_printf(&str, "\tOverlapping objects: ");

		for (BU_LIST_FOR(op, overlap_list, &(olist->l))) {
		    /* iterate over the list and see if we already printed this one */
		    for (BU_LIST_FOR_BACKWARDS(backop, overlap_list, &(op->l))) {
			if ((BU_STR_EQUAL(op->reg1, backop->reg1)) || (BU_STR_EQUAL(op->reg1, backop->reg2)))
			    break;
			if (BU_LIST_IS_HEAD(&(backop->l), &(olist->l))) break;
		    }
		    /* if we got to the head of the list (backop points to the match) */
		    if (BU_LIST_IS_HEAD(&(backop->l), &(olist->l))) {
			bu_vls_printf(&str, "%s  ", op->reg1);
			object_counter++;
		    }

		   /* iterate over the list again up to where we are to see if the second
		    * region was already printed */
		    for (BU_LIST_FOR_BACKWARDS(backop, overlap_list, &(op->l))) {
			if ((BU_STR_EQUAL(op->reg2, backop->reg1)) || (BU_STR_EQUAL(op->reg2, backop->reg2)))
			    break;
			if (BU_LIST_IS_HEAD(&(backop->l), &(olist->l))) break;
		    }
		    /* if we got to the head of the list (backop points to the match) */
		    if (BU_LIST_IS_HEAD(&(backop->l), &(olist->l))) {
			bu_vls_printf(&str, "%s  ", op->reg2);
			object_counter++;
		    }
		}
		bu_vls_printf(&str, "\n\t%zu unique overlapping object%s detected\n",
			      object_counter, (object_counter == 1) ? "" : "s");
	    }
	} else {
		bu_vls_printf(&str, "%zu overlap%s detected\n\n",
			      overlapData->noverlaps, (overlapData->noverlaps==1)?"":"s");
	}
	bu_vls_vlscat(_ged_current_gedp->ged_result_str, &str);
	bu_vls_free(&str);
    }
}


HIDDEN void
overlap(const struct xray *ray,
	const struct partition *pp,
	const struct region *reg1,
	const struct region *reg2,
	double depth,
	void* callback_data)
{
    struct overlaps_context *context = (struct overlaps_context*) callback_data;
    struct hit *ihitp = pp->pt_inhit;
    struct hit *ohitp = pp->pt_outhit;
    vect_t ihit;
    vect_t ohit;

    VJOIN1(ihit, ray->r_pt, ihitp->hit_dist, ray->r_dir);
    VJOIN1(ohit, ray->r_pt, ohitp->hit_dist, ray->r_dir);

    if (context->overlaps_overlay_flag) {
	bu_semaphore_acquire(context->sem_stats);
	BV_ADD_VLIST(context->overlaps_overlay_plot->vbp->free_vlist_hd, context->overlaps_overlay_plot->vhead, ihit, BV_VLIST_LINE_MOVE);
	BV_ADD_VLIST(context->overlaps_overlay_plot->vbp->free_vlist_hd, context->overlaps_overlay_plot->vhead, ohit, BV_VLIST_LINE_DRAW);
	bu_semaphore_release(context->sem_stats);
    }

    bu_semaphore_acquire(context->sem_lists);
    check_log_overlaps(reg1->reg_name, reg2->reg_name, depth, ihit, ohit, context);
    bu_semaphore_release(context->sem_lists);

    if (context->plot_overlaps) {
	pl_color(context->plot_overlaps, V3ARGS(context->overlap_color));
	pdv_3line(context->plot_overlaps, ihit, ohit);
    }
}


int check_overlaps(struct current_state *state,
		   struct db_i *dbip,
		   char **tobjtab,
		   int tnobjs,
		   struct check_parameters *options)
{
    struct ged_check_plot check_plot;
    struct overlaps_context callbackdata;
    struct overlap_list overlapList;
    struct overlap_list *op;

    FILE *plot_overlaps = NULL;
    char *name = "overlaps.plot3";
    int overlap_color[3] = { 255, 255, 0 };	/* yellow */

    /* init overlaps list */
    BU_LIST_INIT(&(overlapList.l));
    overlapList.count = 0;
    overlapList.reg1 = "";
    overlapList.reg2 = "";
    overlapList.maxdepth = 0;

    if (options->plot_files) {
	plot_overlaps = fopen(name, "wb");
	if (plot_overlaps == (FILE *)NULL) {
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "cannot open plot file %s\n", name);
	    return GED_ERROR;
	}
    }

    if (options->overlaps_overlay_flag) {
	check_plot.vbp = bv_vlblock_init(&RTG.rtg_vlfree, 32);
	check_plot.vhead = bv_vlblock_find(check_plot.vbp, 0xFF, 0xFF, 0x00);
    }

    callbackdata.noverlaps = 0;
    callbackdata.overlap_count = 0;
    callbackdata.unique_overlap_count = 0;
    callbackdata.rpt_overlap_flag = options->rpt_overlap_flag;
    VMOVE(callbackdata.overlap_color,overlap_color);
    callbackdata.plot_overlaps = plot_overlaps;
    callbackdata.overlapList = &overlapList;
    callbackdata.overlaps_overlay_flag = options->overlaps_overlay_flag;
    callbackdata.overlaps_overlay_plot = &check_plot;
    callbackdata.sem_stats = bu_semaphore_register("check_stats");
    callbackdata.sem_lists = bu_semaphore_register("check_lists");

    /* register callback */
    analyze_register_overlaps_callback(state, overlap, &callbackdata);

    if (perform_raytracing(state, dbip, tobjtab, tnobjs, ANALYSIS_OVERLAPS)) {
	while (BU_LIST_WHILE(op, overlap_list, &(overlapList.l))){
	    bu_free(op->reg1, "reg1 name");
	    bu_free(op->reg2, "reg1 name");

	    BU_LIST_DEQUEUE(&(op->l));
	    BU_PUT(op, struct overlap_list);
	}
	return GED_ERROR;
    }

    print_verbose_debug(options);

    printOverlaps(&callbackdata, options);

    if (options->overlaps_overlay_flag) {
	const char *nview = getenv("GED_TEST_NEW_CMD_FORMS");
	if (BU_STR_EQUAL(nview, "1")) {
	    struct bview *view = _ged_current_gedp->ged_gvp;
	    struct bu_ptbl *vobjs = (view->independent) ? view->gv_view_objs : view->gv_view_shared_objs;
	    bv_vlblock_to_objs(vobjs, "check::overlaps_", check_plot.vbp, view, _ged_current_gedp->free_scene_obj, &_ged_current_gedp->vlfree);
	} else {
	    _ged_cvt_vlblock_to_solids(_ged_current_gedp, check_plot.vbp, "OVERLAPS", 0);
	}
	bv_vlblock_free(check_plot.vbp);
    }

    if (plot_overlaps) {
	fclose(plot_overlaps);
	bu_vls_printf(_ged_current_gedp->ged_result_str, "\nplot file saved as %s",name);
    }

    while (BU_LIST_WHILE(op, overlap_list, &(overlapList.l))) {
	bu_free(op->reg1, "reg1 name");
	bu_free(op->reg2, "reg1 name");

	BU_LIST_DEQUEUE(&(op->l));
	BU_PUT(op, struct overlap_list);
    }

    return GED_OK;
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
