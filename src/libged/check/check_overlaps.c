/*                C H E C K _ O V E R L A P S . C
 * BRL-CAD
 *
 * Copyright (c) 2018 United States Government as represented by
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
    struct bn_vlblock *vbp;
    struct bu_list *vhead;
};

struct overlap_list {
    struct overlap_list *next;	/* next one */
    char *reg1;			/* overlapping region 1 */
    char *reg2;			/* overlapping region 2 */
    size_t count;		/* number of time reported */
    double maxdepth;		/* maximum overlap depth */
};

struct overlaps_context {
    struct overlap_list **overlapList;
    size_t numberOfOverlaps;
    int overlaps_overlay_flag;
    FILE *plot_overlaps;
    int overlap_color[3];
    struct ged_check_plot overlaps_overlay_plot;
};


HIDDEN void
log_overlaps(const char *reg1, const char *reg2, double depth, struct overlap_list **olist)
{
    struct overlap_list *prev_ol = (struct overlap_list *)0;
    struct overlap_list *op;		/* overlap list */
    struct overlap_list *new_op;
    BU_ALLOC(new_op, struct overlap_list);
    for (op=*olist; op; prev_ol=op, op=op->next) {
	if ((BU_STR_EQUAL(reg1, op->reg1)) && (BU_STR_EQUAL(reg2, op->reg2))) {
	    op->count++;
	    if (depth > op->maxdepth)
		op->maxdepth = depth;
	    bu_free((char *) new_op, "overlap list");
	    return;	/* already on list */
	}
    }

    /* we have a new overlapping region pair */
    op = new_op;
    if (*olist)		/* previous entry exists */
	prev_ol->next = op;
    else
	*olist = op;	/* finally initialize root */

    new_op->reg1 = (char *)bu_malloc(strlen(reg1)+1, "reg1");
    new_op->reg2 = (char *)bu_malloc(strlen(reg2)+1, "reg2");
    bu_strlcpy(new_op->reg1, reg1, strlen(reg1)+1);
    bu_strlcpy(new_op->reg2, reg2, strlen(reg2)+1);
    op->maxdepth = depth;
    op->next = NULL;
    op->count = 1;
}


HIDDEN void
overlap(struct region *reg1,
	struct region *reg2,
	double depth,
	vect_t ihit,
	vect_t ohit,
	void* callback_data)
{
    struct overlaps_context *context = (struct overlaps_context*) callback_data;

    if (context->overlaps_overlay_flag) {
	bu_semaphore_acquire(GED_SEM_WORKER);
	BN_ADD_VLIST(context->overlaps_overlay_plot.vbp->free_vlist_hd, context->overlaps_overlay_plot.vhead, ihit, BN_VLIST_LINE_MOVE);
	BN_ADD_VLIST(context->overlaps_overlay_plot.vbp->free_vlist_hd, context->overlaps_overlay_plot.vhead, ohit, BN_VLIST_LINE_DRAW);
	bu_semaphore_release(GED_SEM_WORKER);
    }

    bu_semaphore_acquire(GED_SEM_LIST);
    context->numberOfOverlaps++;
    log_overlaps(reg1->reg_name, reg2->reg_name, depth, context->overlapList);
    bu_semaphore_release(GED_SEM_LIST);

    if (context->plot_overlaps) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	pl_color(context->plot_overlaps, V3ARGS(context->overlap_color));
	pdv_3line(context->plot_overlaps, ihit, ohit);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
}


int check_overlaps(struct current_state *state,
		   struct db_i *dbip,
		   char **tobjtab,
		   int tnobjs,
		   struct check_parameters *options)
{
    int flags = 0;
    struct ged_check_plot check_plot;
    struct overlaps_context callbackdata;

    struct overlap_list *overlapList = NULL;
    struct overlap_list *nextop=(struct overlap_list *)NULL;
    struct overlap_list *op;

    FILE *plot_overlaps = NULL;
    char *name = "overlaps.plot3";
    int overlap_color[3] = { 255, 255, 0 };	/* yellow */

    if (options->plot_files) {
	if ((plot_overlaps=fopen(name, "wb")) == (FILE *)NULL) {
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "cannot open plot file %s\n", name);
	    return GED_ERROR;
	}
    }

    if (options->overlaps_overlay_flag) {
	check_plot.vbp = rt_vlblock_init();
	check_plot.vhead = bn_vlblock_find(check_plot.vbp, 0xFF, 0xFF, 0x00);
    }

    callbackdata.numberOfOverlaps = 0;
    VMOVE(callbackdata.overlap_color,overlap_color);
    callbackdata.plot_overlaps = plot_overlaps;
    callbackdata.overlapList = &overlapList;
    callbackdata.overlaps_overlay_flag = options->overlaps_overlay_flag;
    callbackdata.overlaps_overlay_plot = check_plot;

    /* register callback */
    analyze_register_overlaps_callback(state, overlap, &callbackdata);
    /* set flags */
    flags |= ANALYSIS_OVERLAPS;
    if (perform_raytracing(state, dbip, tobjtab, tnobjs, flags) == ANALYZE_ERROR){
	return GED_ERROR;
    }

    if(callbackdata.numberOfOverlaps > 0) {
	bu_vls_printf(_ged_current_gedp->ged_result_str, "\nNumber of Overlaps: %d\n", callbackdata.numberOfOverlaps);
	for (op=overlapList; op; op=op->next) {
	    bu_vls_printf(_ged_current_gedp->ged_result_str,"\t<%s, %s>: %zu overlap%c detected, maximum depth is %gmm\n", op->reg1, op->reg2, op->count, op->count>1 ? 's' : (char) 0, op->maxdepth);
	}
    } else {
	bu_vls_printf(_ged_current_gedp->ged_result_str, "\nNo overlaps detected");
    }

    if (options->overlaps_overlay_flag) {
	_ged_cvt_vlblock_to_solids(_ged_current_gedp, check_plot.vbp, "OVERLAPS", 0);
	bn_vlblock_free(check_plot.vbp);
    }

    if (plot_overlaps) {
	fclose(plot_overlaps);
	bu_vls_printf(_ged_current_gedp->ged_result_str, "\nplot file saved as %s",name);
    }

    /* Clear out the overlap list */
    op = overlapList;
    while (op) {
	/* free struct */
	nextop = op->next;
	bu_free(op->reg1,"reg1 name");
	bu_free(op->reg2,"reg2 name");
	bu_free(op, "overlap_list");
	op = nextop;
    }
    overlapList = (struct overlap_list *)NULL;

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
