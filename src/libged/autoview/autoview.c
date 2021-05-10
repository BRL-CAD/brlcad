/*                         A U T O V I E W . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/autoview.c
 *
 * The autoview command.
 *
 */

#include "common.h"
#include "dm.h"
#include "../ged_private.h"

extern int ged_autoview2_core(struct ged *gedp, int argc, const char *argv[]);
/*
 * Auto-adjust the view so that all displayed geometry is in view
 *
 * Usage:
 * autoview
 *
 */
int
ged_autoview_core(struct ged *gedp, int argc, const char *argv[])
{
    const char *cmd2 = getenv("GED_TEST_NEW_CMD_FORMS");
    if (BU_STR_EQUAL(cmd2, "1"))
       return ged_autoview2_core(gedp, argc, argv);

    int is_empty = 1;
    vect_t min, max;
    vect_t center = VINIT_ZERO;
    vect_t radial;
    vect_t sqrt_small;

    /* less than or near zero uses default, 0.5 model scale == 2.0 view factor */
    fastf_t factor = -1.0;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s [scale]", argv[0]);
	return GED_ERROR;
    }

    /* parse the optional scale argument */
    if (argc > 1) {
	double scale = 0.0;
	int ret = sscanf(argv[1], "%lf", &scale);
	if (ret != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ERROR: Expecting floating point scale value after %s\n", argv[0]);
	    return GED_ERROR;
	}
	if (scale > 0.0) {
	    factor = 1.0 / scale;
	}
    }

    /* set the default if unset or insane */
    if (factor < SQRT_SMALL_FASTF) {
	factor = 2.0; /* 2 is half the view */
    }

    VSETALL(sqrt_small, SQRT_SMALL_FASTF);

    is_empty = dl_bounding_sph(gedp->ged_gdp->gd_headDisplay, &min, &max, 1);

    if (is_empty) {
	/* Nothing is in view */
	VSETALL(radial, 1000.0);
    } else {
	VADD2SCALE(center, max, min, 0.5);
	VSUB2(radial, max, center);
    }

    /* make sure it's not inverted */
    VMAX(radial, sqrt_small);

    /* make sure it's not too small */
    if (VNEAR_ZERO(radial, SQRT_SMALL_FASTF))
	VSETALL(radial, 1.0);

    MAT_IDN(gedp->ged_gvp->gv_center);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, center);
    gedp->ged_gvp->gv_scale = radial[X];
    V_MAX(gedp->ged_gvp->gv_scale, radial[Y]);
    V_MAX(gedp->ged_gvp->gv_scale, radial[Z]);

    gedp->ged_gvp->gv_size = factor * gedp->ged_gvp->gv_scale;
    gedp->ged_gvp->gv_isize = 1.0 / gedp->ged_gvp->gv_size;
    bv_update(gedp->ged_gvp);

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl autoview_cmd_impl = { "autoview", ged_autoview_core, GED_CMD_DEFAULT };
const struct ged_cmd autoview_cmd = { &autoview_cmd_impl };

const struct ged_cmd *autoview_cmds[] = { &autoview_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  autoview_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
