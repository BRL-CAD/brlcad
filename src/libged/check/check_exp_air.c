/*                C H E C K _ E X P _ A I R . C
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


struct exp_air_context {
    struct regions_list *exposedAirList;
    FILE *plot_exp_air;
    int expAir_color[3];
};

HIDDEN void
exposed_air(const struct partition *pp,
	    point_t last_out_point,
	    point_t pt,
	    point_t opt,
	    void* callback_data)
{
    struct exp_air_context *context = (struct exp_air_context*) callback_data;
    /* this shouldn't be air */
    bu_semaphore_acquire(BU_SEM_GENERAL);
    add_to_list(context->exposedAirList,
		pp->pt_regionp->reg_name,
		(char *)NULL,
		pp->pt_outhit->hit_dist - pp->pt_inhit->hit_dist,
		last_out_point);
    bu_semaphore_release(BU_SEM_GENERAL);

    if (context->plot_exp_air) {
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	pl_color(context->plot_exp_air, V3ARGS(context->expAir_color));
	pdv_3line(context->plot_exp_air, pt, opt);
	bu_semaphore_release(BU_SEM_SYSCALL);
    }
}


int check_exp_air(struct current_state *state,
		  struct db_i *dbip,
		  char **tobjtab,
		  int tnobjs,
		  struct check_parameters *options)
{
    FILE *plot_exp_air = NULL;
    char *name = "exp_air.plot3";
    struct exp_air_context callbackdata;
    int expAir_color[3] = { 255, 128, 255 }; /* magenta */

    struct regions_list exposedAirList;
    BU_LIST_INIT(&(exposedAirList.l));

    if (options->plot_files) {
	plot_exp_air = fopen(name, "wb");
	if (plot_exp_air == (FILE *)NULL) {
	    bu_vls_printf(_ged_current_gedp->ged_result_str, "cannot open plot file %s\n", name);
	}
    }

    callbackdata.exposedAirList = &exposedAirList;
    callbackdata.plot_exp_air = plot_exp_air;
    VMOVE(callbackdata.expAir_color,expAir_color);

    analyze_register_exp_air_callback(state, exposed_air, &callbackdata);

    if (perform_raytracing(state, dbip, tobjtab, tnobjs, ANALYSIS_EXP_AIR)) {
	clear_list(&exposedAirList);
	return BRLCAD_ERROR;
    }

    print_verbose_debug(options);
    print_list(&exposedAirList, options->units, "Exposed Air");
    clear_list(&exposedAirList);

    if (plot_exp_air) {
	fclose(plot_exp_air);
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
