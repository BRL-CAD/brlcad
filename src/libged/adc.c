/*                           A D C . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file adc.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bio.h"
#include "bu.h"
#include "vmath.h"
#include "ged.h"
#include "dm.h"

static void ged_adc_model_To_adc_view(struct ged_view *gvp);
static void ged_adc_grid_To_adc_view(struct ged_view *gvp);
static void ged_adc_view_To_adc_grid(struct ged_view *gvp);
static void ged_adc_reset(struct ged_view *gvp);
static void ged_adc_vls_print(struct ged_view *gvp, fastf_t base2local, struct bu_vls *out_vp);

HIDDEN void
adc_usage(struct bu_vls *vp, const char *name)
{
    bu_vls_printf(vp, "Usage: %s \n", name);
    bu_vls_printf(vp, "%s", " adc vname                           toggle display of angle/distance cursor\n");
    bu_vls_printf(vp, "%s", " adc vname vars                      print a list of all variables (i.e. var = val)\n");
    bu_vls_printf(vp, "%s", " adc vname draw [0|1]                set or get the draw parameter\n");
    bu_vls_printf(vp, "%s", " adc vname a1   [#]                  set or get angle1\n");
    bu_vls_printf(vp, "%s", " adc vname a2   [#]                  set or get angle2\n");
    bu_vls_printf(vp, "%s", " adc vname dst  [#]                  set or get radius (distance) of tick\n");
    bu_vls_printf(vp, "%s", " adc vname odst [#]                  set or get radius (distance) of tick (+-2047)\n");
    bu_vls_printf(vp, "%s", " adc vname hv   [# #]                set or get position (grid coordinates)\n");
    bu_vls_printf(vp, "%s", " adc vname xyz  [# # #]              set or get position (model coordinates)\n");
    bu_vls_printf(vp, "%s", " adc vname x [#]                     set or get horizontal position (+-2047)\n");
    bu_vls_printf(vp, "%s", " adc vname y [#]                     set or get vertical position (+-2047)\n");
    bu_vls_printf(vp, "%s", " adc vname dh #                      add to horizontal position (grid coordinates)\n");
    bu_vls_printf(vp, "%s", " adc vname dv #                      add to vertical position (grid coordinates)\n");
    bu_vls_printf(vp, "%s", " adc vname dx #                      add to X position (model coordinates)\n");
    bu_vls_printf(vp, "%s", " adc vname dy #                      add to Y position (model coordinates)\n");
    bu_vls_printf(vp, "%s", " adc vname dz #                      add to Z position (model coordinates)\n");
    bu_vls_printf(vp, "%s", " adc vname anchor_pos [0|1]          anchor ADC to current position in model coordinates\n");
    bu_vls_printf(vp, "%s", " adc vname anchor_a1  [0|1]          anchor angle1 to go through anchorpoint_a1\n");
    bu_vls_printf(vp, "%s", " adc vname anchor_a2  [0|1]          anchor angle2 to go through anchorpoint_a2\n");
    bu_vls_printf(vp, "%s", " adc vname anchor_dst [0|1]          anchor tick distance to go through anchorpoint_dst\n");
    bu_vls_printf(vp, "%s", " adc vname anchorpoint_a1  [# # #]   set or get anchor point for angle1\n");
    bu_vls_printf(vp, "%s", " adc vname anchorpoint_a2  [# # #]   set or get anchor point for angle2\n");
    bu_vls_printf(vp, "%s", " adc vname anchorpoint_dst [# # #]   set or get anchor point for tick distance\n");
    bu_vls_printf(vp, "%s", " adc vname -i                        any of the above appropriate commands will interpret parameters as increments\n");
    bu_vls_printf(vp, "%s", " adc vname reset                     reset angles, location, and tick distance\n");
    bu_vls_printf(vp, "%s", " adc vname help                      prints this help message\n");
}


/*
 * Note - this needs to be rewritten to accept keyword/value pairs so
 *        that multiple attributes can be set with a single command call.
 */
int
ged_adc(struct ged	*gedp,
	int		argc,
	const char	*argv[])
{
    char *command;
    char *parameter;
    char **argp = (char **)argv;
    point_t user_pt;		/* Value(s) provided by user */
    point_t scaled_pos;
    int incr_flag;
    int i;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(&gedp->ged_result_str, 0);

    if (argc < 2 || 6 < argc) {
	adc_usage(&gedp->ged_result_str, argv[0]);
	return GED_ERROR;
    }

    command = (char *)argv[0];

    if (BU_STR_EQUAL(argv[1], "-i")) {
	if (argc < 5) {
	    bu_vls_printf(&gedp->ged_result_str, "%s: -i option specified without an op-val pair", command);
	    return GED_ERROR;
	}

	incr_flag = 1;
	parameter = (char *)argv[2];
	argc -= 3;
	argp += 3;
    } else {
	incr_flag = 0;
	parameter = (char *)argv[1];
	argc -= 2;
	argp += 2;
    }

    for (i = 0; i < argc; ++i)
	if (sscanf(argp[i], "%lf", &user_pt[i]) != 1) {
	    adc_usage(&gedp->ged_result_str, command);
	    return GED_ERROR;
	}

    if (BU_STR_EQUAL(parameter, "draw")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_adc.gas_draw);
	    return GED_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		gedp->ged_gvp->gv_adc.gas_draw = 1;
	    else
		gedp->ged_gvp->gv_adc.gas_draw = 0;

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s draw' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "a1")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%g", gedp->ged_gvp->gv_adc.gas_a1);
	    return GED_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_a1) {
		if (incr_flag)
		    gedp->ged_gvp->gv_adc.gas_a1 += user_pt[0];
		else
		    gedp->ged_gvp->gv_adc.gas_a1 = user_pt[0];

		gedp->ged_gvp->gv_adc.gas_dv_a1 = (1.0 - (gedp->ged_gvp->gv_adc.gas_a1 / 45.0)) * GED_MAX;
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s a1' command accepts only 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "a2")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%g", gedp->ged_gvp->gv_adc.gas_a2);
	    return GED_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_a2) {
		if (incr_flag)
		    gedp->ged_gvp->gv_adc.gas_a2 += user_pt[0];
		else
		    gedp->ged_gvp->gv_adc.gas_a2 = user_pt[0];

		gedp->ged_gvp->gv_adc.gas_dv_a2 = (1.0 - (gedp->ged_gvp->gv_adc.gas_a2 / 45.0)) * GED_MAX;
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s a2' command accepts only 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "dst")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%g", gedp->ged_gvp->gv_adc.gas_dst * gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
	    return GED_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_dst) {
		if (incr_flag)
		    gedp->ged_gvp->gv_adc.gas_dst += user_pt[0] / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
		else
		    gedp->ged_gvp->gv_adc.gas_dst = user_pt[0] / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);

		gedp->ged_gvp->gv_adc.gas_dv_dist = (gedp->ged_gvp->gv_adc.gas_dst / M_SQRT1_2 - 1.0) * GED_MAX;
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s dst' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "odst")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_adc.gas_dv_dist);
	    return GED_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_dst) {
		if (incr_flag)
		    gedp->ged_gvp->gv_adc.gas_dv_dist += user_pt[0];
		else
		    gedp->ged_gvp->gv_adc.gas_dv_dist = user_pt[0];

		gedp->ged_gvp->gv_adc.gas_dst = (gedp->ged_gvp->gv_adc.gas_dv_dist * INV_GED + 1.0) * M_SQRT1_2;
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s odst' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "dh")) {
	if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_pos) {
		gedp->ged_gvp->gv_adc.gas_pos_grid[X] += user_pt[0] / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
		ged_adc_grid_To_adc_view(gedp->ged_gvp);
		MAT4X3PNT(gedp->ged_gvp->gv_adc.gas_pos_model, gedp->ged_gvp->gv_view2model, gedp->ged_gvp->gv_adc.gas_pos_view);
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s dh' command requires 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "dv")) {
	if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_pos) {
		gedp->ged_gvp->gv_adc.gas_pos_grid[Y] += user_pt[0] / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
		ged_adc_grid_To_adc_view(gedp->ged_gvp);
		MAT4X3PNT(gedp->ged_gvp->gv_adc.gas_pos_model, gedp->ged_gvp->gv_view2model, gedp->ged_gvp->gv_adc.gas_pos_view);
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s dv' command requires 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "hv")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%g %g",
			  gedp->ged_gvp->gv_adc.gas_pos_grid[X] * gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local,
			  gedp->ged_gvp->gv_adc.gas_pos_grid[Y] * gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
	    return GED_OK;
	} else if (argc == 2) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_pos) {
		if (incr_flag) {
		    gedp->ged_gvp->gv_adc.gas_pos_grid[X] += user_pt[X] / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
		    gedp->ged_gvp->gv_adc.gas_pos_grid[Y] += user_pt[Y] / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
		} else {
		    gedp->ged_gvp->gv_adc.gas_pos_grid[X] = user_pt[X] / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
		    gedp->ged_gvp->gv_adc.gas_pos_grid[Y] = user_pt[Y] / (gedp->ged_gvp->gv_scale * gedp->ged_wdbp->dbip->dbi_base2local);
		}

		gedp->ged_gvp->gv_adc.gas_pos_grid[Z] = 0.0;
		ged_adc_grid_To_adc_view(gedp->ged_gvp);
		MAT4X3PNT(gedp->ged_gvp->gv_adc.gas_pos_model, gedp->ged_gvp->gv_view2model, gedp->ged_gvp->gv_adc.gas_pos_model);
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s hv' command requires 0 or 2 arguments\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "dx")) {
	if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_pos) {
		gedp->ged_gvp->gv_adc.gas_pos_model[X] += user_pt[0] * gedp->ged_wdbp->dbip->dbi_local2base;
		ged_adc_model_To_adc_view(gedp->ged_gvp);
		ged_adc_view_To_adc_grid(gedp->ged_gvp);
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s dx' command requires 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "dy")) {
	if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_pos) {
		gedp->ged_gvp->gv_adc.gas_pos_model[Y] += user_pt[0] * gedp->ged_wdbp->dbip->dbi_local2base;
		ged_adc_model_To_adc_view(gedp->ged_gvp);
		ged_adc_view_To_adc_grid(gedp->ged_gvp);
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s dy' command requires 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "dz")) {
	if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_pos) {
		gedp->ged_gvp->gv_adc.gas_pos_model[Z] += user_pt[0] * gedp->ged_wdbp->dbip->dbi_local2base;
		ged_adc_model_To_adc_view(gedp->ged_gvp);
		ged_adc_view_To_adc_grid(gedp->ged_gvp);
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s dz' command requires 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "xyz")) {
	if (argc == 0) {
	    VSCALE(scaled_pos, gedp->ged_gvp->gv_adc.gas_pos_model, gedp->ged_wdbp->dbip->dbi_base2local);
	    bu_vls_printf(&gedp->ged_result_str, "%g %g %g", V3ARGS(scaled_pos));
	    return GED_OK;
	} else if (argc == 3) {
	    VSCALE(user_pt, user_pt, gedp->ged_wdbp->dbip->dbi_local2base);

	    if (incr_flag) {
		VADD2(gedp->ged_gvp->gv_adc.gas_pos_model, gedp->ged_gvp->gv_adc.gas_pos_model, user_pt);
	    } else {
		VMOVE(gedp->ged_gvp->gv_adc.gas_pos_model, user_pt);
	    }

	    ged_adc_model_To_adc_view(gedp->ged_gvp);
	    ged_adc_view_To_adc_grid(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s xyz' command requires 0 or 3 arguments\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "x")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_adc.gas_dv_x);
	    return GED_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_pos) {
		if (incr_flag) {
		    gedp->ged_gvp->gv_adc.gas_dv_x += user_pt[0];
		} else {
		    gedp->ged_gvp->gv_adc.gas_dv_x = user_pt[0];
		}

		gedp->ged_gvp->gv_adc.gas_pos_view[X] = gedp->ged_gvp->gv_adc.gas_dv_x * INV_GED;
		gedp->ged_gvp->gv_adc.gas_pos_view[Y] = gedp->ged_gvp->gv_adc.gas_dv_y * INV_GED;
		ged_adc_view_To_adc_grid(gedp->ged_gvp);
		MAT4X3PNT(gedp->ged_gvp->gv_adc.gas_pos_model, gedp->ged_gvp->gv_view2model, gedp->ged_gvp->gv_adc.gas_pos_view);
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s x' command requires 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "y")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_adc.gas_dv_y);
	    return GED_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_adc.gas_anchor_pos) {
		if (incr_flag) {
		    gedp->ged_gvp->gv_adc.gas_dv_y += user_pt[0];
		} else {
		    gedp->ged_gvp->gv_adc.gas_dv_y = user_pt[0];
		}

		gedp->ged_gvp->gv_adc.gas_pos_view[X] = gedp->ged_gvp->gv_adc.gas_dv_x * INV_GED;
		gedp->ged_gvp->gv_adc.gas_pos_view[Y] = gedp->ged_gvp->gv_adc.gas_dv_y * INV_GED;
		ged_adc_view_To_adc_grid(gedp->ged_gvp);
		MAT4X3PNT(gedp->ged_gvp->gv_adc.gas_pos_model, gedp->ged_gvp->gv_view2model, gedp->ged_gvp->gv_adc.gas_pos_view);
	    }

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s y' command requires 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "anchor_pos")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_adc.gas_anchor_pos);
	    return GED_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i < 0 || 2 < i) {
		bu_vls_printf(&gedp->ged_result_str, "The '%d anchor_pos' parameter accepts values of 0, 1, or 2.", i);
		return GED_ERROR;
	    }

	    gedp->ged_gvp->gv_adc.gas_anchor_pos = i;
	    ged_calc_adc_pos(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s anchor_pos' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "anchor_a1")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_adc.gas_anchor_a1);
	    return GED_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		gedp->ged_gvp->gv_adc.gas_anchor_a1 = 1;
	    else
		gedp->ged_gvp->gv_adc.gas_anchor_a1 = 0;

	    ged_calc_adc_a1(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s anchor_a1' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "anchorpoint_a1")) {
	if (argc == 0) {
	    VSCALE(scaled_pos, gedp->ged_gvp->gv_adc.gas_anchor_pt_a1, gedp->ged_wdbp->dbip->dbi_base2local);
	    bu_vls_printf(&gedp->ged_result_str, "%g %g %g", V3ARGS(scaled_pos));

	    return GED_OK;
	} else if (argc == 3) {
	    VSCALE(user_pt, user_pt, gedp->ged_wdbp->dbip->dbi_local2base);

	    if (incr_flag) {
		VADD2(gedp->ged_gvp->gv_adc.gas_anchor_pt_a1, gedp->ged_gvp->gv_adc.gas_anchor_pt_a1, user_pt);
	    } else {
		VMOVE(gedp->ged_gvp->gv_adc.gas_anchor_pt_a1, user_pt);
	    }

	    ged_calc_adc_a1(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s anchorpoint_a1' command accepts 0 or 3 arguments\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "anchor_a2")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_adc.gas_anchor_a2);

	    return GED_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		gedp->ged_gvp->gv_adc.gas_anchor_a2 = 1;
	    else
		gedp->ged_gvp->gv_adc.gas_anchor_a2 = 0;

	    ged_calc_adc_a2(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s anchor_a2' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "anchorpoint_a2")) {
	if (argc == 0) {
	    VSCALE(scaled_pos, gedp->ged_gvp->gv_adc.gas_anchor_pt_a2, gedp->ged_wdbp->dbip->dbi_base2local);

	    bu_vls_printf(&gedp->ged_result_str, "%g %g %g", V3ARGS(scaled_pos));

	    return GED_OK;
	} else if (argc == 3) {
	    VSCALE(user_pt, user_pt, gedp->ged_wdbp->dbip->dbi_local2base);

	    if (incr_flag) {
		VADD2(gedp->ged_gvp->gv_adc.gas_anchor_pt_a2, gedp->ged_gvp->gv_adc.gas_anchor_pt_a2, user_pt);
	    } else {
		VMOVE(gedp->ged_gvp->gv_adc.gas_anchor_pt_a2, user_pt);
	    }

	    ged_calc_adc_a2(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s anchorpoint_a2' command accepts 0 or 3 arguments\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "anchor_dst")) {
	if (argc == 0) {
	    bu_vls_printf(&gedp->ged_result_str, "%d", gedp->ged_gvp->gv_adc.gas_anchor_dst);

	    return GED_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i) {
		gedp->ged_gvp->gv_adc.gas_anchor_dst = 1;
	    } else
		gedp->ged_gvp->gv_adc.gas_anchor_dst = 0;

	    ged_calc_adc_dst(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s anchor_dst' command accepts 0 or 1 argument\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "anchorpoint_dst")) {
	if (argc == 0) {
	    VSCALE(scaled_pos, gedp->ged_gvp->gv_adc.gas_anchor_pt_dst, gedp->ged_wdbp->dbip->dbi_base2local);
	    bu_vls_printf(&gedp->ged_result_str, "%g %g %g", V3ARGS(scaled_pos));

	    return GED_OK;
	} else if (argc == 3) {
	    VSCALE(user_pt, user_pt, gedp->ged_wdbp->dbip->dbi_local2base);

	    if (incr_flag) {
		VADD2(gedp->ged_gvp->gv_adc.gas_anchor_pt_dst, gedp->ged_gvp->gv_adc.gas_anchor_pt_dst, user_pt);
	    } else {
		VMOVE(gedp->ged_gvp->gv_adc.gas_anchor_pt_dst, user_pt);
	    }

	    ged_calc_adc_dst(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s anchorpoint_dst' command accepts 0 or 3 arguments\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "reset")) {
	if (argc == 0) {
	    ged_adc_reset(gedp->ged_gvp);

	    return GED_OK;
	}

	bu_vls_printf(&gedp->ged_result_str, "The '%s reset' command accepts no arguments\n", command);
	return GED_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "vars")) {
	ged_adc_vls_print(gedp->ged_gvp, gedp->ged_wdbp->dbip->dbi_base2local, &gedp->ged_result_str);
	return GED_OK;
    }

    if (BU_STR_EQUAL(parameter, "help")) {
	adc_usage(&gedp->ged_result_str, command);
	return GED_HELP;
    }

    bu_vls_printf(&gedp->ged_result_str, "%s: unrecognized command '%s'\n", command, parameter);
    adc_usage(&gedp->ged_result_str, command);

    return GED_ERROR;
}

static void
ged_adc_model_To_adc_view(struct ged_view *gvp)
{
    MAT4X3PNT(gvp->gv_adc.gas_pos_view, gvp->gv_model2view, gvp->gv_adc.gas_pos_model);
    gvp->gv_adc.gas_dv_x = gvp->gv_adc.gas_pos_view[X] * GED_MAX;
    gvp->gv_adc.gas_dv_y = gvp->gv_adc.gas_pos_view[Y] * GED_MAX;
}

static void
ged_adc_grid_To_adc_view(struct ged_view *gvp)
{
    point_t model_pt;
    point_t view_pt;

    VSETALL(model_pt, 0.0);
    MAT4X3PNT(view_pt, gvp->gv_model2view, model_pt);
    VADD2(gvp->gv_adc.gas_pos_view, view_pt, gvp->gv_adc.gas_pos_grid);
    gvp->gv_adc.gas_dv_x = gvp->gv_adc.gas_pos_view[X] * GED_MAX;
    gvp->gv_adc.gas_dv_y = gvp->gv_adc.gas_pos_view[Y] * GED_MAX;
}

static void
ged_adc_view_To_adc_grid(struct ged_view *gvp)
{
    point_t model_pt;
    point_t view_pt;

    VSETALL(model_pt, 0.0);
    MAT4X3PNT(view_pt, gvp->gv_model2view, model_pt);
    VSUB2(gvp->gv_adc.gas_pos_grid, gvp->gv_adc.gas_pos_view, view_pt);
}

void
ged_calc_adc_pos(struct ged_view *gvp)
{
    if (gvp->gv_adc.gas_anchor_pos == 1) {
	ged_adc_model_To_adc_view(gvp);
	ged_adc_view_To_adc_grid(gvp);
    } else if (gvp->gv_adc.gas_anchor_pos == 2) {
	ged_adc_grid_To_adc_view(gvp);
	MAT4X3PNT(gvp->gv_adc.gas_pos_model, gvp->gv_view2model, gvp->gv_adc.gas_pos_view);
    } else {
	ged_adc_view_To_adc_grid(gvp);
	MAT4X3PNT(gvp->gv_adc.gas_pos_model, gvp->gv_view2model, gvp->gv_adc.gas_pos_view);
    }
}

void
ged_calc_adc_a1(struct ged_view *gvp)
{
    if (gvp->gv_adc.gas_anchor_a1) {
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, gvp->gv_model2view, gvp->gv_adc.gas_anchor_pt_a1);
	dx = view_pt[X] * GED_MAX - gvp->gv_adc.gas_dv_x;
	dy = view_pt[Y] * GED_MAX - gvp->gv_adc.gas_dv_y;

	if (!ZERO(dx) || !ZERO(dy)) {
	    gvp->gv_adc.gas_a1 = RAD2DEG*atan2(dy, dx);
	    gvp->gv_adc.gas_dv_a1 = (1.0 - (gvp->gv_adc.gas_a1 / 45.0)) * GED_MAX;
	}
    }
}

void
ged_calc_adc_a2(struct ged_view *gvp)
{
    if (gvp->gv_adc.gas_anchor_a2) {
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, gvp->gv_model2view, gvp->gv_adc.gas_anchor_pt_a2);
	dx = view_pt[X] * GED_MAX - gvp->gv_adc.gas_dv_x;
	dy = view_pt[Y] * GED_MAX - gvp->gv_adc.gas_dv_y;

	if (!ZERO(dx) || !ZERO(dy)) {
	    gvp->gv_adc.gas_a2 = RAD2DEG*atan2(dy, dx);
	    gvp->gv_adc.gas_dv_a2 = (1.0 - (gvp->gv_adc.gas_a2 / 45.0)) * GED_MAX;
	}
    }
}

void
ged_calc_adc_dst(struct ged_view *gvp)
{
    if (gvp->gv_adc.gas_anchor_dst) {
	fastf_t dist;
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, gvp->gv_model2view, gvp->gv_adc.gas_anchor_pt_dst);

	dx = view_pt[X] * GED_MAX - gvp->gv_adc.gas_dv_x;
	dy = view_pt[Y] * GED_MAX - gvp->gv_adc.gas_dv_y;
	dist = sqrt(dx * dx + dy * dy);
	gvp->gv_adc.gas_dst = dist * INV_GED;
	gvp->gv_adc.gas_dv_dist = (dist / M_SQRT1_2) - GED_MAX;
    } else
	gvp->gv_adc.gas_dst = (gvp->gv_adc.gas_dv_dist * INV_GED + 1.0) * M_SQRT1_2;
}

static void
ged_adc_reset(struct ged_view *gvp)
{
    gvp->gv_adc.gas_dv_x = gvp->gv_adc.gas_dv_y = 0;
    gvp->gv_adc.gas_dv_a1 = gvp->gv_adc.gas_dv_a2 = 0;
    gvp->gv_adc.gas_dv_dist = 0;

    VSETALL(gvp->gv_adc.gas_pos_view, 0.0);
    MAT4X3PNT(gvp->gv_adc.gas_pos_model, gvp->gv_view2model, gvp->gv_adc.gas_pos_view);
    gvp->gv_adc.gas_dst = (gvp->gv_adc.gas_dv_dist * INV_GED + 1.0) * M_SQRT1_2;
    gvp->gv_adc.gas_a1 = gvp->gv_adc.gas_a2 = 45.0;
    ged_adc_view_To_adc_grid(gvp);

    VSETALL(gvp->gv_adc.gas_anchor_pt_a1, 0.0);
    VSETALL(gvp->gv_adc.gas_anchor_pt_a2, 0.0);
    VSETALL(gvp->gv_adc.gas_anchor_pt_dst, 0.0);

    gvp->gv_adc.gas_anchor_pos = 0;
    gvp->gv_adc.gas_anchor_a1 = 0;
    gvp->gv_adc.gas_anchor_a2 = 0;
    gvp->gv_adc.gas_anchor_dst = 0;
}

static void
ged_adc_vls_print(struct ged_view *gvp, fastf_t base2local, struct bu_vls *out_vp)
{
    bu_vls_printf(out_vp, "draw = %d\n", gvp->gv_adc.gas_draw);
    bu_vls_printf(out_vp, "a1 = %.15e\n", gvp->gv_adc.gas_a1);
    bu_vls_printf(out_vp, "a2 = %.15e\n", gvp->gv_adc.gas_a2);
    bu_vls_printf(out_vp, "dst = %.15e\n", gvp->gv_adc.gas_dst * gvp->gv_scale * base2local);
    bu_vls_printf(out_vp, "odst = %d\n", gvp->gv_adc.gas_dv_dist);
    bu_vls_printf(out_vp, "hv = %.15e %.15e\n",
		  gvp->gv_adc.gas_pos_grid[X] * gvp->gv_scale * base2local,
		  gvp->gv_adc.gas_pos_grid[Y] * gvp->gv_scale * base2local);
    bu_vls_printf(out_vp, "xyz = %.15e %.15e %.15e\n",
		  gvp->gv_adc.gas_pos_model[X] * base2local,
		  gvp->gv_adc.gas_pos_model[Y] * base2local,
		  gvp->gv_adc.gas_pos_model[Z] * base2local);
    bu_vls_printf(out_vp, "x = %d\n", gvp->gv_adc.gas_dv_x);
    bu_vls_printf(out_vp, "y = %d\n", gvp->gv_adc.gas_dv_y);
    bu_vls_printf(out_vp, "anchor_pos = %d\n", gvp->gv_adc.gas_anchor_pos);
    bu_vls_printf(out_vp, "anchor_a1 = %d\n", gvp->gv_adc.gas_anchor_a1);
    bu_vls_printf(out_vp, "anchor_a2 = %d\n", gvp->gv_adc.gas_anchor_a2);
    bu_vls_printf(out_vp, "anchor_dst = %d\n", gvp->gv_adc.gas_anchor_dst);
    bu_vls_printf(out_vp, "anchorpoint_a1 = %.15e %.15e %.15e\n",
		  gvp->gv_adc.gas_anchor_pt_a1[X] * base2local,
		  gvp->gv_adc.gas_anchor_pt_a1[Y] * base2local,
		  gvp->gv_adc.gas_anchor_pt_a1[Z] * base2local);
    bu_vls_printf(out_vp, "anchorpoint_a2 = %.15e %.15e %.15e\n",
		  gvp->gv_adc.gas_anchor_pt_a2[X] * base2local,
		  gvp->gv_adc.gas_anchor_pt_a2[Y] * base2local,
		  gvp->gv_adc.gas_anchor_pt_a2[Z] * base2local);
    bu_vls_printf(out_vp, "anchorpoint_dst = %.15e %.15e %.15e\n",
		  gvp->gv_adc.gas_anchor_pt_dst[X] * base2local,
		  gvp->gv_adc.gas_anchor_pt_dst[Y] * base2local,
		  gvp->gv_adc.gas_anchor_pt_dst[Z] * base2local);
}


static void
dm_draw_ticks(struct dm *dmp, struct ged_view *gvp, fastf_t angle)
{
    fastf_t c_tdist;
    fastf_t d1, d2;
    fastf_t t1, t2;
    fastf_t x1, Y1;       /* not "y1", due to conflict with math lib */
    fastf_t x2, y2;

    /*
     * Position tic marks from dial 9.
     */
    /* map -2048 - 2047 into 0 - 2048 * sqrt (2) */
    /* Tick distance */
    c_tdist = ((fastf_t)(gvp->gv_adc.gas_dv_dist) + GED_MAX) * M_SQRT1_2;

    d1 = c_tdist * cos (angle);
    d2 = c_tdist * sin (angle);
    t1 = 20.0 * sin (angle);
    t2 = 20.0 * cos (angle);

    /* Quadrant 1 */
    x1 = gvp->gv_adc.gas_dv_x + d1 + t1;
    Y1 = gvp->gv_adc.gas_dv_y + d2 - t2;
    x2 = gvp->gv_adc.gas_dv_x + d1 -t1;
    y2 = gvp->gv_adc.gas_dv_y + d2 + t2;
    if (ged_clip(&x1, &Y1, &x2, &y2) == 0) {
	DM_DRAW_LINE_2D(dmp,
			GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->dm_aspect,
			GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->dm_aspect);
    }

    /* Quadrant 2 */
    x1 = gvp->gv_adc.gas_dv_x - d2 + t2;
    Y1 = gvp->gv_adc.gas_dv_y + d1 + t1;
    x2 = gvp->gv_adc.gas_dv_x - d2 - t2;
    y2 = gvp->gv_adc.gas_dv_y + d1 - t1;
    if (ged_clip(&x1, &Y1, &x2, &y2) == 0) {
	DM_DRAW_LINE_2D(dmp,
			GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->dm_aspect,
			GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->dm_aspect);
    }

    /* Quadrant 3 */
    x1 = gvp->gv_adc.gas_dv_x - d1 - t1;
    Y1 = gvp->gv_adc.gas_dv_y - d2 + t2;
    x2 = gvp->gv_adc.gas_dv_x - d1 + t1;
    y2 = gvp->gv_adc.gas_dv_y - d2 - t2;
    if (ged_clip(&x1, &Y1, &x2, &y2) == 0) {
	DM_DRAW_LINE_2D(dmp,
			GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->dm_aspect,
			GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->dm_aspect);
    }

    /* Quadrant 4 */
    x1 = gvp->gv_adc.gas_dv_x + d2 - t2;
    Y1 = gvp->gv_adc.gas_dv_y - d1 - t1;
    x2 = gvp->gv_adc.gas_dv_x + d2 + t2;
    y2 = gvp->gv_adc.gas_dv_y - d1 + t1;
    if (ged_clip(&x1, &Y1, &x2, &y2) == 0) {
	DM_DRAW_LINE_2D(dmp,
			GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->dm_aspect,
			GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->dm_aspect);
    }
}


/**
 * Compute and display the angle/distance cursor.
 */
void
dm_draw_adc(struct dm *dmp, struct ged_view *gvp)
{
    fastf_t x1, Y1;	/* not "y1", due to conflict with math lib */
    fastf_t x2, y2;
    fastf_t x3, y3;
    fastf_t x4, y4;
    fastf_t d1, d2;
    fastf_t angle1, angle2;

    ged_calc_adc_pos(gvp);
    ged_calc_adc_a1(gvp);
    ged_calc_adc_a2(gvp);
    ged_calc_adc_dst(gvp);

    DM_SET_FGCOLOR(dmp,
		   gvp->gv_adc.gas_line_color[0],
		   gvp->gv_adc.gas_line_color[1],
		   gvp->gv_adc.gas_line_color[2], 1, 1.0);
    DM_SET_LINE_ATTR(dmp, gvp->gv_adc.gas_line_width, 0);

    /* Horizontal */
    DM_DRAW_LINE_2D(dmp,
		    GED_TO_PM1(GED_MIN), GED_TO_PM1(gvp->gv_adc.gas_dv_y) * dmp->dm_aspect,
		    GED_TO_PM1(GED_MAX), GED_TO_PM1(gvp->gv_adc.gas_dv_y) * dmp->dm_aspect);

    /* Vertical */
    DM_DRAW_LINE_2D(dmp,
		    GED_TO_PM1(gvp->gv_adc.gas_dv_x), GED_TO_PM1(GED_MAX),
		    GED_TO_PM1(gvp->gv_adc.gas_dv_x), GED_TO_PM1(GED_MIN));

    angle1 = gvp->gv_adc.gas_a1 * DEG2RAD;
    angle2 = gvp->gv_adc.gas_a2 * DEG2RAD;

    /* sin for X and cos for Y to reverse sense of knob */
    d1 = cos (angle1) * 8000.0;
    d2 = sin (angle1) * 8000.0;
    x1 = gvp->gv_adc.gas_dv_x + d1;
    Y1 = gvp->gv_adc.gas_dv_y + d2;
    x2 = gvp->gv_adc.gas_dv_x - d1;
    y2 = gvp->gv_adc.gas_dv_y - d2;

    x3 = gvp->gv_adc.gas_dv_x + d2;
    y3 = gvp->gv_adc.gas_dv_y - d1;
    x4 = gvp->gv_adc.gas_dv_x - d2;
    y4 = gvp->gv_adc.gas_dv_y + d1;

    DM_DRAW_LINE_2D(dmp,
		    GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->dm_aspect,
		    GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->dm_aspect);
    DM_DRAW_LINE_2D(dmp,
		    GED_TO_PM1(x3), GED_TO_PM1(y3) * dmp->dm_aspect,
		    GED_TO_PM1(x4), GED_TO_PM1(y4) * dmp->dm_aspect);

    d1 = cos(angle2) * 8000.0;
    d2 = sin(angle2) * 8000.0;
    x1 = gvp->gv_adc.gas_dv_x + d1;
    Y1 = gvp->gv_adc.gas_dv_y + d2;
    x2 = gvp->gv_adc.gas_dv_x - d1;
    y2 = gvp->gv_adc.gas_dv_y - d2;

    x3 = gvp->gv_adc.gas_dv_x + d2;
    y3 = gvp->gv_adc.gas_dv_y - d1;
    x4 = gvp->gv_adc.gas_dv_x - d2;
    y4 = gvp->gv_adc.gas_dv_y + d1;

    DM_SET_LINE_ATTR(dmp, gvp->gv_adc.gas_line_width, 1);
    DM_DRAW_LINE_2D(dmp,
		    GED_TO_PM1(x1), GED_TO_PM1(Y1) * dmp->dm_aspect,
		    GED_TO_PM1(x2), GED_TO_PM1(y2) * dmp->dm_aspect);
    DM_DRAW_LINE_2D(dmp,
		    GED_TO_PM1(x3), GED_TO_PM1(y3) * dmp->dm_aspect,
		    GED_TO_PM1(x4), GED_TO_PM1(y4) * dmp->dm_aspect);
    DM_SET_LINE_ATTR(dmp, gvp->gv_adc.gas_line_width, 0);

    DM_SET_FGCOLOR(dmp,
		   gvp->gv_adc.gas_tick_color[0],
		   gvp->gv_adc.gas_tick_color[1],
		   gvp->gv_adc.gas_tick_color[2], 1, 1.0);
    dm_draw_ticks(dmp, gvp, 0.0);
    dm_draw_ticks(dmp, gvp, angle1);
    dm_draw_ticks(dmp, gvp, angle2);
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
