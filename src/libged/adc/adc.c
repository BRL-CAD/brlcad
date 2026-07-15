/*                           A D C . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2026 United States Government as represented by
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
/** @file libged/adc.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>


#include "vmath.h"
#include "bu/cmdschema.h"
#include "ged.h"

#include "../ged_private.h"


struct adc_native_args {
    int increment;
};

static void adc_usage(struct bu_vls *vp, const char *name);
static int adc_native_preflight(struct ged *gedp, int argc, const char *argv[],
	struct adc_native_args *args, int *parameter_index);

void ged_calc_adc_pos(struct bview *gvp);
void ged_calc_adc_a1(struct bview *gvp);
void ged_calc_adc_a2(struct bview *gvp);
void ged_calc_adc_dst(struct bview *gvp);

static void
adc_vls_print(struct bview *gvp, fastf_t base2local, struct bu_vls *out_vp)
{
    bu_vls_printf(out_vp, "draw = %d\n", gvp->gv_s->gv_adc.draw);
    bu_vls_printf(out_vp, "a1 = %.15e\n", gvp->gv_s->gv_adc.a1);
    bu_vls_printf(out_vp, "a2 = %.15e\n", gvp->gv_s->gv_adc.a2);
    bu_vls_printf(out_vp, "dst = %.15e\n", gvp->gv_s->gv_adc.dst * gvp->gv_scale * base2local);
    bu_vls_printf(out_vp, "odst = %d\n", gvp->gv_s->gv_adc.dv_dist);
    bu_vls_printf(out_vp, "hv = %.15e %.15e\n",
		  gvp->gv_s->gv_adc.pos_grid[X] * gvp->gv_scale * base2local,
		  gvp->gv_s->gv_adc.pos_grid[Y] * gvp->gv_scale * base2local);
    bu_vls_printf(out_vp, "xyz = %.15e %.15e %.15e\n",
		  gvp->gv_s->gv_adc.pos_model[X] * base2local,
		  gvp->gv_s->gv_adc.pos_model[Y] * base2local,
		  gvp->gv_s->gv_adc.pos_model[Z] * base2local);
    bu_vls_printf(out_vp, "x = %d\n", gvp->gv_s->gv_adc.dv_x);
    bu_vls_printf(out_vp, "y = %d\n", gvp->gv_s->gv_adc.dv_y);
    bu_vls_printf(out_vp, "anchor_pos = %d\n", gvp->gv_s->gv_adc.anchor_pos);
    bu_vls_printf(out_vp, "anchor_a1 = %d\n", gvp->gv_s->gv_adc.anchor_a1);
    bu_vls_printf(out_vp, "anchor_a2 = %d\n", gvp->gv_s->gv_adc.anchor_a2);
    bu_vls_printf(out_vp, "anchor_dst = %d\n", gvp->gv_s->gv_adc.anchor_dst);
    bu_vls_printf(out_vp, "anchorpoint_a1 = %.15e %.15e %.15e\n",
		  gvp->gv_s->gv_adc.anchor_pt_a1[X] * base2local,
		  gvp->gv_s->gv_adc.anchor_pt_a1[Y] * base2local,
		  gvp->gv_s->gv_adc.anchor_pt_a1[Z] * base2local);
    bu_vls_printf(out_vp, "anchorpoint_a2 = %.15e %.15e %.15e\n",
		  gvp->gv_s->gv_adc.anchor_pt_a2[X] * base2local,
		  gvp->gv_s->gv_adc.anchor_pt_a2[Y] * base2local,
		  gvp->gv_s->gv_adc.anchor_pt_a2[Z] * base2local);
    bu_vls_printf(out_vp, "anchorpoint_dst = %.15e %.15e %.15e\n",
		  gvp->gv_s->gv_adc.anchor_pt_dst[X] * base2local,
		  gvp->gv_s->gv_adc.anchor_pt_dst[Y] * base2local,
		  gvp->gv_s->gv_adc.anchor_pt_dst[Z] * base2local);
}


/*
 * Note - this needs to be rewritten to accept keyword/value pairs so
 * that multiple attributes can be set with a single command call.
 */
int
ged_adc_core(struct ged *gedp,
	int argc,
	const char *argv[])
{
    char *command;
    char *parameter;
    char **argp = (char **)argv;
    point_t user_pt;		/* Value(s) provided by user */
    point_t scaled_pos;
    struct adc_native_args native_args = {0};
    int parameter_index;
    int incr_flag;
    int i;

    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    struct bview *v = gedp->ged_gvp;
    fastf_t gv_scale = (gedp->dbip) ? v->gv_scale * gedp->dbip->dbi_base2local : v->gv_scale;
    double sval = (gedp->dbip) ? gedp->dbip->dbi_local2base : 1.0;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);


    if (adc_native_preflight(gedp, argc, argv, &native_args, &parameter_index) != BRLCAD_OK) {
	adc_usage(gedp->ged_result_str, argv[0]);
	return BRLCAD_ERROR;
    }

    command = (char *)argv[0];

    incr_flag = native_args.increment;
    parameter = (char *)argv[parameter_index];
    argc -= parameter_index + 1;
    argp += parameter_index + 1;

    for (i = 0; i < argc; ++i) {
	if (!bu_cmd_number_from_str(&user_pt[i], argp[i])) {
	    adc_usage(gedp->ged_result_str, command);
	    return BRLCAD_ERROR;
	}
    }

    if (BU_STR_EQUAL(parameter, "draw")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_adc.draw);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		gedp->ged_gvp->gv_s->gv_adc.draw = 1;
	    else
		gedp->ged_gvp->gv_s->gv_adc.draw = 0;

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s draw' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "a1")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%g", gedp->ged_gvp->gv_s->gv_adc.a1);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_s->gv_adc.anchor_a1) {
		if (incr_flag)
		    gedp->ged_gvp->gv_s->gv_adc.a1 += user_pt[0];
		else
		    gedp->ged_gvp->gv_s->gv_adc.a1 = user_pt[0];

		gedp->ged_gvp->gv_s->gv_adc.dv_a1 = (1.0 - (gedp->ged_gvp->gv_s->gv_adc.a1 / 45.0)) * BV_MAX;
	    }

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s a1' command accepts only 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "a2")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%g", gedp->ged_gvp->gv_s->gv_adc.a2);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_s->gv_adc.anchor_a2) {
		if (incr_flag)
		    gedp->ged_gvp->gv_s->gv_adc.a2 += user_pt[0];
		else
		    gedp->ged_gvp->gv_s->gv_adc.a2 = user_pt[0];

		gedp->ged_gvp->gv_s->gv_adc.dv_a2 = (1.0 - (gedp->ged_gvp->gv_s->gv_adc.a2 / 45.0)) * BV_MAX;
	    }

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s a2' command accepts only 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "dst")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%g", v->gv_s->gv_adc.dst * gv_scale);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_s->gv_adc.anchor_dst) {
		if (incr_flag)
		    gedp->ged_gvp->gv_s->gv_adc.dst += user_pt[0] / gv_scale;
		else
		    gedp->ged_gvp->gv_s->gv_adc.dst = user_pt[0] / gv_scale;

		gedp->ged_gvp->gv_s->gv_adc.dv_dist = (gedp->ged_gvp->gv_s->gv_adc.dst / M_SQRT1_2 - 1.0) * BV_MAX;
	    }

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s dst' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "odst")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_adc.dv_dist);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_s->gv_adc.anchor_dst) {
		if (incr_flag)
		    gedp->ged_gvp->gv_s->gv_adc.dv_dist += user_pt[0];
		else
		    gedp->ged_gvp->gv_s->gv_adc.dv_dist = user_pt[0];

		gedp->ged_gvp->gv_s->gv_adc.dst = (gedp->ged_gvp->gv_s->gv_adc.dv_dist * INV_BV + 1.0) * M_SQRT1_2;
	    }

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s odst' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "dh")) {
	if (argc == 1) {
	    if (!gedp->ged_gvp->gv_s->gv_adc.anchor_pos) {
		gedp->ged_gvp->gv_s->gv_adc.pos_grid[X] += user_pt[0] / gv_scale;
		adc_grid_to_adc_view(&(gedp->ged_gvp->gv_s->gv_adc), gedp->ged_gvp->gv_view2model, BV_MAX);
		MAT4X3PNT(gedp->ged_gvp->gv_s->gv_adc.pos_model, gedp->ged_gvp->gv_view2model, gedp->ged_gvp->gv_s->gv_adc.pos_view);
	    }

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s dh' command requires 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "dv")) {
	if (argc == 1) {
	    if (!gedp->ged_gvp->gv_s->gv_adc.anchor_pos) {
		gedp->ged_gvp->gv_s->gv_adc.pos_grid[Y] += user_pt[0] / gv_scale;
		adc_grid_to_adc_view(&(gedp->ged_gvp->gv_s->gv_adc), gedp->ged_gvp->gv_view2model, BV_MAX);
		MAT4X3PNT(gedp->ged_gvp->gv_s->gv_adc.pos_model, gedp->ged_gvp->gv_view2model, gedp->ged_gvp->gv_s->gv_adc.pos_view);
	    }

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s dv' command requires 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "hv")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%g %g",
			  gedp->ged_gvp->gv_s->gv_adc.pos_grid[X] * gv_scale,
			  gedp->ged_gvp->gv_s->gv_adc.pos_grid[Y] * gv_scale);
	    return BRLCAD_OK;
	} else if (argc == 2) {
	    if (!gedp->ged_gvp->gv_s->gv_adc.anchor_pos) {
		if (incr_flag) {
		    gedp->ged_gvp->gv_s->gv_adc.pos_grid[X] += user_pt[X] / gv_scale;
		    gedp->ged_gvp->gv_s->gv_adc.pos_grid[Y] += user_pt[Y] / gv_scale;
		} else {
		    gedp->ged_gvp->gv_s->gv_adc.pos_grid[X] = user_pt[X] / gv_scale;
		    gedp->ged_gvp->gv_s->gv_adc.pos_grid[Y] = user_pt[Y] / gv_scale;
		}

		gedp->ged_gvp->gv_s->gv_adc.pos_grid[Z] = 0.0;
		adc_grid_to_adc_view(&(gedp->ged_gvp->gv_s->gv_adc), gedp->ged_gvp->gv_view2model, BV_MAX);
		MAT4X3PNT(gedp->ged_gvp->gv_s->gv_adc.pos_model, gedp->ged_gvp->gv_view2model, gedp->ged_gvp->gv_s->gv_adc.pos_model);
	    }

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s hv' command requires 0 or 2 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "dx")) {
	if (argc == 1) {
	    if (!gedp->ged_gvp->gv_s->gv_adc.anchor_pos) {
		double pval = (gedp->dbip) ? (user_pt[0] * gedp->dbip->dbi_local2base) : user_pt[0];
		gedp->ged_gvp->gv_s->gv_adc.pos_model[X] += pval;
		adc_model_to_adc_view(&(gedp->ged_gvp->gv_s->gv_adc), gedp->ged_gvp->gv_model2view, BV_MAX);
		adc_view_to_adc_grid(&(gedp->ged_gvp->gv_s->gv_adc), gedp->ged_gvp->gv_model2view);
	    }

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s dx' command requires 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "dy")) {
	if (argc == 1) {
	    if (!gedp->ged_gvp->gv_s->gv_adc.anchor_pos) {
		double pval = (gedp->dbip) ? (user_pt[0] * gedp->dbip->dbi_local2base) : user_pt[0];
		gedp->ged_gvp->gv_s->gv_adc.pos_model[Y] += pval;
		adc_model_to_adc_view(&(gedp->ged_gvp->gv_s->gv_adc), gedp->ged_gvp->gv_model2view, BV_MAX);
		adc_view_to_adc_grid(&(gedp->ged_gvp->gv_s->gv_adc), gedp->ged_gvp->gv_model2view);
	    }

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s dy' command requires 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "dz")) {
	if (argc == 1) {
	    if (!gedp->ged_gvp->gv_s->gv_adc.anchor_pos) {
		double pval = (gedp->dbip) ? (user_pt[0] * gedp->dbip->dbi_local2base) : user_pt[0];
		gedp->ged_gvp->gv_s->gv_adc.pos_model[Z] += pval;
		adc_model_to_adc_view(&(gedp->ged_gvp->gv_s->gv_adc), gedp->ged_gvp->gv_model2view, BV_MAX);
		adc_view_to_adc_grid(&(gedp->ged_gvp->gv_s->gv_adc), gedp->ged_gvp->gv_model2view);
	    }

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s dz' command requires 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "xyz")) {
	if (argc == 0) {
	    VSCALE(scaled_pos, gedp->ged_gvp->gv_s->gv_adc.pos_model, sval);
	    bu_vls_printf(gedp->ged_result_str, "%g %g %g", V3ARGS(scaled_pos));
	    return BRLCAD_OK;
	} else if (argc == 3) {
	    VSCALE(user_pt, user_pt, sval);

	    if (incr_flag) {
		VADD2(gedp->ged_gvp->gv_s->gv_adc.pos_model, gedp->ged_gvp->gv_s->gv_adc.pos_model, user_pt);
	    } else {
		VMOVE(gedp->ged_gvp->gv_s->gv_adc.pos_model, user_pt);
	    }

	    adc_model_to_adc_view(&(gedp->ged_gvp->gv_s->gv_adc), gedp->ged_gvp->gv_model2view, BV_MAX);
	    adc_view_to_adc_grid(&(gedp->ged_gvp->gv_s->gv_adc), gedp->ged_gvp->gv_model2view);

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s xyz' command requires 0 or 3 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "x")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_adc.dv_x);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_s->gv_adc.anchor_pos) {
		if (incr_flag) {
		    gedp->ged_gvp->gv_s->gv_adc.dv_x += user_pt[0];
		} else {
		    gedp->ged_gvp->gv_s->gv_adc.dv_x = user_pt[0];
		}

		gedp->ged_gvp->gv_s->gv_adc.pos_view[X] = gedp->ged_gvp->gv_s->gv_adc.dv_x * INV_BV;
		gedp->ged_gvp->gv_s->gv_adc.pos_view[Y] = gedp->ged_gvp->gv_s->gv_adc.dv_y * INV_BV;
		adc_view_to_adc_grid(&(gedp->ged_gvp->gv_s->gv_adc), gedp->ged_gvp->gv_model2view);
		MAT4X3PNT(gedp->ged_gvp->gv_s->gv_adc.pos_model, gedp->ged_gvp->gv_view2model, gedp->ged_gvp->gv_s->gv_adc.pos_view);
	    }

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s x' command requires 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "y")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_adc.dv_y);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    if (!gedp->ged_gvp->gv_s->gv_adc.anchor_pos) {
		if (incr_flag) {
		    gedp->ged_gvp->gv_s->gv_adc.dv_y += user_pt[0];
		} else {
		    gedp->ged_gvp->gv_s->gv_adc.dv_y = user_pt[0];
		}

		gedp->ged_gvp->gv_s->gv_adc.pos_view[X] = gedp->ged_gvp->gv_s->gv_adc.dv_x * INV_BV;
		gedp->ged_gvp->gv_s->gv_adc.pos_view[Y] = gedp->ged_gvp->gv_s->gv_adc.dv_y * INV_BV;
		adc_view_to_adc_grid(&(gedp->ged_gvp->gv_s->gv_adc), gedp->ged_gvp->gv_model2view);
		MAT4X3PNT(gedp->ged_gvp->gv_s->gv_adc.pos_model, gedp->ged_gvp->gv_view2model, gedp->ged_gvp->gv_s->gv_adc.pos_view);
	    }

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s y' command requires 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "anchor_pos")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_adc.anchor_pos);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i < 0 || 2 < i) {
		bu_vls_printf(gedp->ged_result_str, "The '%d anchor_pos' parameter accepts values of 0, 1, or 2.", i);
		return BRLCAD_ERROR;
	    }

	    gedp->ged_gvp->gv_s->gv_adc.anchor_pos = i;
	    ged_calc_adc_pos(gedp->ged_gvp);

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s anchor_pos' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "anchor_a1")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_adc.anchor_a1);
	    return BRLCAD_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		gedp->ged_gvp->gv_s->gv_adc.anchor_a1 = 1;
	    else
		gedp->ged_gvp->gv_s->gv_adc.anchor_a1 = 0;

	    ged_calc_adc_a1(gedp->ged_gvp);

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s anchor_a1' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "anchorpoint_a1")) {
	if (argc == 0) {
	    VSCALE(scaled_pos, gedp->ged_gvp->gv_s->gv_adc.anchor_pt_a1, sval);
	    bu_vls_printf(gedp->ged_result_str, "%g %g %g", V3ARGS(scaled_pos));

	    return BRLCAD_OK;
	} else if (argc == 3) {
	    VSCALE(user_pt, user_pt, sval);

	    if (incr_flag) {
		VADD2(gedp->ged_gvp->gv_s->gv_adc.anchor_pt_a1, gedp->ged_gvp->gv_s->gv_adc.anchor_pt_a1, user_pt);
	    } else {
		VMOVE(gedp->ged_gvp->gv_s->gv_adc.anchor_pt_a1, user_pt);
	    }

	    ged_calc_adc_a1(gedp->ged_gvp);

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s anchorpoint_a1' command accepts 0 or 3 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "anchor_a2")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_adc.anchor_a2);

	    return BRLCAD_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i)
		gedp->ged_gvp->gv_s->gv_adc.anchor_a2 = 1;
	    else
		gedp->ged_gvp->gv_s->gv_adc.anchor_a2 = 0;

	    ged_calc_adc_a2(gedp->ged_gvp);

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s anchor_a2' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "anchorpoint_a2")) {
	if (argc == 0) {
	    VSCALE(scaled_pos, gedp->ged_gvp->gv_s->gv_adc.anchor_pt_a2, sval);

	    bu_vls_printf(gedp->ged_result_str, "%g %g %g", V3ARGS(scaled_pos));

	    return BRLCAD_OK;
	} else if (argc == 3) {
	    VSCALE(user_pt, user_pt, sval);

	    if (incr_flag) {
		VADD2(gedp->ged_gvp->gv_s->gv_adc.anchor_pt_a2, gedp->ged_gvp->gv_s->gv_adc.anchor_pt_a2, user_pt);
	    } else {
		VMOVE(gedp->ged_gvp->gv_s->gv_adc.anchor_pt_a2, user_pt);
	    }

	    ged_calc_adc_a2(gedp->ged_gvp);

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s anchorpoint_a2' command accepts 0 or 3 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "anchor_dst")) {
	if (argc == 0) {
	    bu_vls_printf(gedp->ged_result_str, "%d", gedp->ged_gvp->gv_s->gv_adc.anchor_dst);

	    return BRLCAD_OK;
	} else if (argc == 1) {
	    i = (int)user_pt[X];

	    if (i) {
		gedp->ged_gvp->gv_s->gv_adc.anchor_dst = 1;
	    } else
		gedp->ged_gvp->gv_s->gv_adc.anchor_dst = 0;

	    ged_calc_adc_dst(gedp->ged_gvp);

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s anchor_dst' command accepts 0 or 1 argument\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "anchorpoint_dst")) {
	if (argc == 0) {
	    VSCALE(scaled_pos, gedp->ged_gvp->gv_s->gv_adc.anchor_pt_dst, sval);
	    bu_vls_printf(gedp->ged_result_str, "%g %g %g", V3ARGS(scaled_pos));

	    return BRLCAD_OK;
	} else if (argc == 3) {
	    VSCALE(user_pt, user_pt, sval);

	    if (incr_flag) {
		VADD2(gedp->ged_gvp->gv_s->gv_adc.anchor_pt_dst, gedp->ged_gvp->gv_s->gv_adc.anchor_pt_dst, user_pt);
	    } else {
		VMOVE(gedp->ged_gvp->gv_s->gv_adc.anchor_pt_dst, user_pt);
	    }

	    ged_calc_adc_dst(gedp->ged_gvp);

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s anchorpoint_dst' command accepts 0 or 3 arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "reset")) {
	if (argc == 0) {
	    adc_reset(&(gedp->ged_gvp->gv_s->gv_adc), gedp->ged_gvp->gv_view2model, gedp->ged_gvp->gv_model2view);

	    return BRLCAD_OK;
	}

	bu_vls_printf(gedp->ged_result_str, "The '%s reset' command accepts no arguments\n", command);
	return BRLCAD_ERROR;
    }

    if (BU_STR_EQUAL(parameter, "vars")) {
	adc_vls_print(gedp->ged_gvp, sval, gedp->ged_result_str);
	return BRLCAD_OK;
    }

    if (BU_STR_EQUAL(parameter, "help")) {
	adc_usage(gedp->ged_result_str, command);
	return GED_HELP;
    }

    bu_vls_printf(gedp->ged_result_str, "%s: unrecognized command '%s'\n", command, parameter);
    adc_usage(gedp->ged_result_str, command);

    return BRLCAD_ERROR;
}


void
ged_calc_adc_pos(struct bview *gvp)
{
    if (gvp->gv_s->gv_adc.anchor_pos == 1) {
	adc_model_to_adc_view(&(gvp->gv_s->gv_adc), gvp->gv_model2view, BV_MAX);
	adc_view_to_adc_grid(&(gvp->gv_s->gv_adc), gvp->gv_model2view);
    } else if (gvp->gv_s->gv_adc.anchor_pos == 2) {
	adc_grid_to_adc_view(&(gvp->gv_s->gv_adc), gvp->gv_view2model, BV_MAX);
	MAT4X3PNT(gvp->gv_s->gv_adc.pos_model, gvp->gv_view2model, gvp->gv_s->gv_adc.pos_view);
    } else {
	adc_view_to_adc_grid(&(gvp->gv_s->gv_adc), gvp->gv_model2view);
	MAT4X3PNT(gvp->gv_s->gv_adc.pos_model, gvp->gv_view2model, gvp->gv_s->gv_adc.pos_view);
    }
}


void
ged_calc_adc_a1(struct bview *gvp)
{
    if (gvp->gv_s->gv_adc.anchor_a1) {
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, gvp->gv_model2view, gvp->gv_s->gv_adc.anchor_pt_a1);
	dx = view_pt[X] * BV_MAX - gvp->gv_s->gv_adc.dv_x;
	dy = view_pt[Y] * BV_MAX - gvp->gv_s->gv_adc.dv_y;

	if (!ZERO(dx) || !ZERO(dy)) {
	    gvp->gv_s->gv_adc.a1 = RAD2DEG*atan2(dy, dx);
	    gvp->gv_s->gv_adc.dv_a1 = (1.0 - (gvp->gv_s->gv_adc.a1 / 45.0)) * BV_MAX;
	}
    }
}


void
ged_calc_adc_a2(struct bview *gvp)
{
    if (gvp->gv_s->gv_adc.anchor_a2) {
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, gvp->gv_model2view, gvp->gv_s->gv_adc.anchor_pt_a2);
	dx = view_pt[X] * BV_MAX - gvp->gv_s->gv_adc.dv_x;
	dy = view_pt[Y] * BV_MAX - gvp->gv_s->gv_adc.dv_y;

	if (!ZERO(dx) || !ZERO(dy)) {
	    gvp->gv_s->gv_adc.a2 = RAD2DEG*atan2(dy, dx);
	    gvp->gv_s->gv_adc.dv_a2 = (1.0 - (gvp->gv_s->gv_adc.a2 / 45.0)) * BV_MAX;
	}
    }
}


void
ged_calc_adc_dst(struct bview *gvp)
{
    if (gvp->gv_s->gv_adc.anchor_dst) {
	fastf_t dist;
	fastf_t dx, dy;
	point_t view_pt;

	MAT4X3PNT(view_pt, gvp->gv_model2view, gvp->gv_s->gv_adc.anchor_pt_dst);

	dx = view_pt[X] * BV_MAX - gvp->gv_s->gv_adc.dv_x;
	dy = view_pt[Y] * BV_MAX - gvp->gv_s->gv_adc.dv_y;
	dist = sqrt(dx * dx + dy * dy);
	gvp->gv_s->gv_adc.dst = dist * INV_BV;
	gvp->gv_s->gv_adc.dv_dist = (dist / M_SQRT1_2) - BV_MAX;
    } else
	gvp->gv_s->gv_adc.dst = (gvp->gv_s->gv_adc.dv_dist * INV_BV + 1.0) * M_SQRT1_2;
}


#include "../include/plugin.h"
static const char * const adc_binary_values[] = {"0", "1", NULL};
static const char * const adc_anchor_position_values[] = {"0", "1", "2", NULL};

static const struct bu_cmd_operand adc_optional_number_operands[] = {
    BU_CMD_OPERAND("value", BU_CMD_VALUE_NUMBER, 0, 1, "New value", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand adc_required_number_operands[] = {
    BU_CMD_OPERAND("delta", BU_CMD_VALUE_NUMBER, 1, 1, "Increment value", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand adc_optional_binary_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("enabled", BU_CMD_VALUE_KEYWORD, 0, 1,
	"0 (off) or 1 (on)", NULL, adc_binary_values),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand adc_optional_anchor_position_operands[] = {
    BU_CMD_OPERAND_KEYWORDS("mode", BU_CMD_VALUE_KEYWORD, 0, 1,
	"0 (free), 1 (model), or 2 (grid)", NULL, adc_anchor_position_values),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand adc_optional_pair_operands[] = {
    BU_CMD_OPERAND("position", BU_CMD_VALUE_NUMBER, 0, 2,
	"Horizontal and vertical grid position", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_operand adc_optional_point_operands[] = {
    BU_CMD_OPERAND("point", BU_CMD_VALUE_NUMBER, 0, 3,
	"Model-coordinate XYZ point", NULL),
    BU_CMD_OPERAND_NULL
};


static int
adc_optional_count_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result,
	size_t required_count, const char *hint)
{
    struct bu_cmd_schema flat = *schema;
    int ret;

    flat.validation.custom_validate = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID || !argc || argc == required_count)
	return ret;
    bu_cmd_validate_result_clear(result);
    result->state = argc < required_count ? BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_INVALID;
    result->token_start = cursor_arg < argc ? cursor_arg : argc;
    result->token_end = result->token_start;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->completion_type = BU_CMD_VALUE_NUMBER;
    result->hint = hint;
    return 0;
}


static int
adc_optional_pair_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    return adc_optional_count_validate(schema, argc, argv, cursor_arg, result, 2,
	"zero or two numeric coordinates required");
}


static int
adc_optional_point_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    return adc_optional_count_validate(schema, argc, argv, cursor_arg, result, 3,
	"zero or three numeric coordinates required");
}


static const struct bu_cmd_option adc_root_options[] = {
    BU_CMD_FLAG("i", NULL, struct adc_native_args, increment,
	"Interpret applicable values as increments"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_schema adc_root_schema = {
    "adc", "Inspect and configure the angle-distance cursor", adc_root_options,
    NULL, BU_CMD_PARSE_OPTIONS_FIRST, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_vars_schema = {
    "vars", "Print all ADC variables", NULL, NULL,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_draw_schema = {
    "draw", "Query or set ADC drawing", NULL, adc_optional_binary_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_a1_schema = {
    "a1", "Query or set angle 1", NULL, adc_optional_number_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_a2_schema = {
    "a2", "Query or set angle 2", NULL, adc_optional_number_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_dst_schema = {
    "dst", "Query or set tick distance", NULL, adc_optional_number_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_odst_schema = {
    "odst", "Query or set view tick distance", NULL, adc_optional_number_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_hv_schema = {
    "hv", "Query or set grid position", NULL, adc_optional_pair_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(adc_optional_pair_validate, NULL)
};
static const struct bu_cmd_schema adc_xyz_schema = {
    "xyz", "Query or set model position", NULL, adc_optional_point_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(adc_optional_point_validate, NULL)
};
static const struct bu_cmd_schema adc_x_schema = {
    "x", "Query or set horizontal position", NULL, adc_optional_number_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_y_schema = {
    "y", "Query or set vertical position", NULL, adc_optional_number_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_dh_schema = {
    "dh", "Increment horizontal grid position", NULL, adc_required_number_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_dv_schema = {
    "dv", "Increment vertical grid position", NULL, adc_required_number_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_dx_schema = {
    "dx", "Increment model X position", NULL, adc_required_number_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_dy_schema = {
    "dy", "Increment model Y position", NULL, adc_required_number_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_dz_schema = {
    "dz", "Increment model Z position", NULL, adc_required_number_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_anchor_pos_schema = {
    "anchor_pos", "Query or set position anchoring", NULL,
    adc_optional_anchor_position_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_anchor_a1_schema = {
    "anchor_a1", "Query or set angle 1 anchoring", NULL, adc_optional_binary_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_anchor_a2_schema = {
    "anchor_a2", "Query or set angle 2 anchoring", NULL, adc_optional_binary_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_anchor_dst_schema = {
    "anchor_dst", "Query or set distance anchoring", NULL, adc_optional_binary_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_anchorpoint_a1_schema = {
    "anchorpoint_a1", "Query or set angle 1 anchor point", NULL, adc_optional_point_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(adc_optional_point_validate, NULL)
};
static const struct bu_cmd_schema adc_anchorpoint_a2_schema = {
    "anchorpoint_a2", "Query or set angle 2 anchor point", NULL, adc_optional_point_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(adc_optional_point_validate, NULL)
};
static const struct bu_cmd_schema adc_anchorpoint_dst_schema = {
    "anchorpoint_dst", "Query or set distance anchor point", NULL, adc_optional_point_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(adc_optional_point_validate, NULL)
};
static const struct bu_cmd_schema adc_reset_schema = {
    "reset", "Reset ADC state", NULL, NULL,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
static const struct bu_cmd_schema adc_help_schema = {
    "help", "Print ADC help", NULL, NULL,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};

static const struct bu_cmd_tree_node adc_subcommands[] = {
    BU_CMD_TREE_NODE(&adc_vars_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_draw_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_a1_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_a2_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_dst_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_odst_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_hv_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_xyz_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_x_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_y_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_dh_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_dv_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_dx_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_dy_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_dz_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_anchor_pos_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_anchor_a1_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_anchor_a2_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_anchor_dst_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_anchorpoint_a1_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_anchorpoint_a2_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_anchorpoint_dst_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_reset_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE(&adc_help_schema, NULL, NULL, BU_CMD_TREE_CHILD_AFTER_OPTIONS, NULL),
    BU_CMD_TREE_NODE_NULL
};
static const struct bu_cmd_tree adc_tree = {
    &adc_root_schema, adc_subcommands, BU_CMD_TREE_CHILD_AFTER_OPTIONS
};


static void
adc_usage(struct bu_vls *vp, const char *name)
{
    char *help = bu_cmd_tree_describe(&adc_tree);

    if (help) {
	bu_vls_strcat(vp, help);
	bu_free(help, "ADC native schema help");
	return;
    }
    bu_vls_printf(vp, "Usage: %s [-i] <subcommand>\n", name);
}


static int
adc_native_preflight(struct ged *gedp, int argc, const char *argv[],
	struct adc_native_args *args, int *parameter_index)
{
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const struct bu_cmd_tree_node *node;
    int child_offset;
    int ret = BRLCAD_ERROR;

    if (!gedp || !argv || !args || !parameter_index || argc < 1)
	return BRLCAD_ERROR;
    child_offset = bu_cmd_schema_parse(&adc_root_schema, args, &msg,
	argc - 1, argv + 1);
    if (child_offset < 0) {
	if (bu_vls_strlen(&msg))
	    bu_vls_vlscat(gedp->ged_result_str, &msg);
	goto done;
    }
    if (child_offset >= argc - 1) {
	bu_vls_printf(gedp->ged_result_str, "%s: subcommand required\n", argv[0]);
	goto done;
    }
    node = bu_cmd_tree_find_subcommand(&adc_tree, argv[child_offset + 1]);
    if (!node) {
	bu_vls_printf(gedp->ged_result_str, "%s: unrecognized command '%s'\n",
	    argv[0], argv[child_offset + 1]);
	goto done;
    }
    if (bu_cmd_schema_parse_complete(node->schema, NULL, &msg,
	argc - child_offset - 2, argv + child_offset + 2) < 0) {
	if (bu_vls_strlen(&msg))
	    bu_vls_vlscat(gedp->ged_result_str, &msg);
	goto done;
    }
    *parameter_index = child_offset + 1;
    ret = BRLCAD_OK;

done:
    bu_vls_free(&msg);
    return ret;
}


static int
adc_grammar_validate(struct ged *gedp, const char *input, size_t cursor_pos,
	struct ged_cmd_validate_result *result)
{
    return ged_cmd_tree_validate(gedp, &adc_tree, input, cursor_pos, result);
}


static int
adc_grammar_analyze(struct ged *gedp, const char *input, struct ged_cmd_analysis *analysis)
{
    return ged_cmd_tree_analyze(gedp, &adc_tree, input, analysis);
}


static char *
adc_grammar_json(void)
{
    return bu_cmd_tree_describe_json(&adc_tree);
}


static int
adc_grammar_lint(struct bu_vls *msgs)
{
    return bu_cmd_tree_lint(&adc_tree, msgs);
}


static const struct ged_cmd_grammar adc_grammar = {
    "adc", "Inspect and configure the angle-distance cursor", adc_grammar_validate,
    adc_grammar_analyze, adc_grammar_json, adc_grammar_lint
};

#define GED_ADC_COMMANDS(LX, LXID, NX, NXID, GX, GXID) \
    GX(adc, ged_adc_core, GED_CMD_DEFAULT, &adc_grammar) \

GED_DECLARE_COMMAND_SET_WITH_MIXED_SCHEMA(GED_ADC_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_MIXED_SCHEMA("libged_adc", 1, GED_ADC_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
