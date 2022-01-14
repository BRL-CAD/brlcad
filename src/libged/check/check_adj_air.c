/*                C H E C K _ A D J _ A I R . C
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

struct adj_air_context {
    struct regions_list *adjAirList;
    FILE *plot_adjair;
    int adjAir_color[3];
};


HIDDEN void
adj_air(const struct xray* ray, const struct partition *pp, point_t pt, void* callback_data)
{
    struct adj_air_context *context = (struct adj_air_context*) callback_data;
    bu_semaphore_acquire(BU_SEM_GENERAL);
    add_to_list(context->adjAirList, pp->pt_back->pt_regionp->reg_name, pp->pt_regionp->reg_name, 0.0, pt);
    bu_semaphore_release(BU_SEM_GENERAL);

    if (context->plot_adjair) {
	double d = pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist;
	point_t aapt;
	d *= 0.25;
	VJOIN1(aapt, pt, d, ray->r_dir);

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	pl_color(context->plot_adjair, V3ARGS(context->adjAir_color));
	pdv_3line(context->plot_adjair, pt, aapt);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
}


int check_adj_air(struct current_state *state,
		  struct db_i *dbip,
		  char **tobjtab,
		  int tnobjs,
		  struct check_parameters *options)
{
    FILE *plot_adjair = NULL;
    char *name = "adj_air.plot3";
    struct adj_air_context callbackdata;
    int adjAir_color[3] = { 128, 255, 192 }; /* pale green */

    struct regions_list adjAirList;
    BU_LIST_INIT(&(adjAirList.l));

    if (options->plot_files) {
	plot_adjair = fopen(name, "wb");
	if (plot_adjair == (FILE *)NULL) {
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "cannot open plot file %s\n", name);
	}
    }

    callbackdata.adjAirList = &adjAirList;
    callbackdata.plot_adjair = plot_adjair;
    VMOVE(callbackdata.adjAir_color, adjAir_color);

    analyze_register_adj_air_callback(state, adj_air, &callbackdata);

    if (perform_raytracing(state, dbip, tobjtab, tnobjs, ANALYSIS_ADJ_AIR)) {
	clear_list(&adjAirList);
	return GED_ERROR;
    }

    print_verbose_debug(options);
    print_list(&adjAirList, options->units, "Adjacent Air");
    clear_list(&adjAirList);

    if (plot_adjair) {
	fclose(plot_adjair);
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
