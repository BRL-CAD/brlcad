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

struct overlaps_context {
    struct regions_list *overlapList;
    size_t numberOfOverlaps;
    int overlaps_overlay_flag;
    FILE *plot_overlaps;
    int overlap_color[3];
    struct ged_check_plot overlaps_overlay_plot;
};


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
    add_to_list(context->overlapList, reg1->reg_name, reg2->reg_name, depth, ihit);
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
    struct regions_list overlapList;

    FILE *plot_overlaps = NULL;
    char *name = "overlaps.plot3";
    int overlap_color[3] = { 255, 255, 0 };	/* yellow */

    BU_LIST_INIT(&(overlapList.l));

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
    }

    print_list(&overlapList, options->units, "Overlaps");
    clear_list(&overlapList);

    if (options->overlaps_overlay_flag) {
	_ged_cvt_vlblock_to_solids(_ged_current_gedp, check_plot.vbp, "OVERLAPS", 0);
	bn_vlblock_free(check_plot.vbp);
    }

    if (plot_overlaps) {
	fclose(plot_overlaps);
	bu_vls_printf(_ged_current_gedp->ged_result_str, "\nplot file saved as %s",name);
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
