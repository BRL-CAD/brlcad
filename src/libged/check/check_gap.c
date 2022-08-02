/*                C H E C K _ G A P . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2022 United States Government as represented by
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

#include <stdlib.h>
#include <string.h>

#include "ged.h"

#include "../ged_private.h"
#include "./check_private.h"

struct gap_context {
    struct regions_list *gapList;
    FILE *plot_gaps;
    int gap_color[3];
};


HIDDEN void
gaps(const struct xray* ray, const struct partition *pp, double gap_dist, point_t pt, void* callback_data)
{
    struct gap_context *context = (struct gap_context*) callback_data;
    /* we only want to report unique pairs */
    bu_semaphore_acquire(BU_SEM_GENERAL);
    add_to_list(context->gapList, pp->pt_regionp->reg_name, pp->pt_back->pt_regionp->reg_name, gap_dist, pt);
    bu_semaphore_release(BU_SEM_GENERAL);

    /* let's plot */
    if (context->plot_gaps) {
	vect_t gapEnd;
	VJOIN1(gapEnd, pt, -gap_dist, ray->r_dir);
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	pl_color(context->plot_gaps, V3ARGS(context->gap_color));
	pdv_3line(context->plot_gaps, pt, gapEnd);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
}


int check_gap(struct current_state *state,
	      struct db_i *dbip,
	      char **tobjtab,
	      int tnobjs,
	      struct check_parameters *options)
{
    FILE *plot_gaps = NULL;
    char *name = "gaps.plot3";
    struct gap_context callbackdata;
    int gap_color[3] = { 128, 192, 255 };    /* cyan */

    struct regions_list gapList;
    BU_LIST_INIT(&(gapList.l));

    if (options->plot_files) {
	plot_gaps = fopen(name, "wb");
	if (plot_gaps == (FILE *)NULL) {
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "cannot open plot file %s\n", name);
	}
    }

    callbackdata.gapList = &gapList;
    callbackdata.plot_gaps = plot_gaps;
    VMOVE(callbackdata.gap_color, gap_color);

    analyze_register_gaps_callback(state, gaps, &callbackdata);

    if (perform_raytracing(state, dbip, tobjtab, tnobjs, ANALYSIS_GAP)) {
	clear_list(&gapList);
	return BRLCAD_ERROR;
    }

    print_verbose_debug(options);
    print_list(&gapList, options->units, "Gaps");
    clear_list(&gapList);

    if (plot_gaps) {
	fclose(plot_gaps);
	bu_vls_printf(_ged_current_gedp->ged_result_str, "\nplot file saved as %s",name);
    }

    return BRLCAD_OK;
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
