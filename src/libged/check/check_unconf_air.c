/*                C H E C K _ U N C O N F _ A I R . C
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

#include "ged.h"

#include "../ged_private.h"
#include "./check_private.h"


struct unconf_air_context {
    FILE *plot_unconf_air;
    struct regions_list *unconfAirList;
    int unconfAir_color[3];
    double tolerance;
};

HIDDEN void
unconf_air(const struct xray *ray,
	   const struct partition *ipart,
	   const struct partition *opart,
	   void* callback_data)
{
    struct unconf_air_context *context = (struct unconf_air_context*) callback_data;
    point_t ihit, ohit;
    double depth;
    VJOIN1(ihit, ray->r_pt, ipart->pt_inhit->hit_dist, ray->r_dir);
    VJOIN1(ohit, ray->r_pt, opart->pt_outhit->hit_dist, ray->r_dir);
    depth = ipart->pt_inhit->hit_dist - opart->pt_outhit->hit_dist;

    if (depth < context->tolerance) {
	/* too small to matter */
	return;
    }

    bu_semaphore_acquire(BU_SEM_GENERAL);

    add_to_list(context->unconfAirList,
		ipart->pt_regionp->reg_name,
		opart->pt_regionp->reg_name,
		depth,
		ihit);
    bu_semaphore_release(BU_SEM_GENERAL);

    if (context->plot_unconf_air) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	pl_color(context->plot_unconf_air, V3ARGS(context->unconfAir_color));
	pdv_3line(context->plot_unconf_air, ohit, ihit);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
}


int check_unconf_air(struct current_state *state,
		  struct db_i *dbip,
		  char **tobjtab,
		  int tnobjs,
		  struct check_parameters *options)
{
    FILE *plot_unconf_air = NULL;
    char *name = "unconf_air.plot3";
    struct unconf_air_context callbackdata;
    int unconfAir_color[3] = { 0, 255, 0}; /* green */

    struct regions_list unconfAirList;
    BU_LIST_INIT(&(unconfAirList.l));

    if (options->plot_files) {
	plot_unconf_air = fopen(name, "wb");
	if (plot_unconf_air == (FILE *)NULL) {
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "cannot open plot file %s\n", name);
	}
    }

    callbackdata.unconfAirList = &unconfAirList;
    callbackdata.plot_unconf_air = plot_unconf_air;
    callbackdata.tolerance = options->overlap_tolerance;
    VMOVE(callbackdata.unconfAir_color,unconfAir_color);

    analyze_register_unconf_air_callback(state, unconf_air, &callbackdata);
    if (perform_raytracing(state, dbip, tobjtab, tnobjs, ANALYSIS_UNCONF_AIR)) {
	clear_list(&unconfAirList);
	return BRLCAD_ERROR;
    }

    print_verbose_debug(options);
    print_list(&unconfAirList, options->units, "Unconfined Air");
    clear_list(&unconfAirList);

    if (plot_unconf_air) {
	fclose(plot_unconf_air);
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
