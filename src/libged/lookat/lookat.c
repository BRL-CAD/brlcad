/*                         L O O K A T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2020 United States Government as represented by
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
/** @file libged/lookat.c
 *
 * The lookat command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_lookat_core(struct ged *gedp, int argc, const char *argv[])
{
    point_t look;
    point_t eye;
    point_t tmp;
    point_t new_center;
    vect_t dir;
    fastf_t new_az, new_el;
    double scan[3];
    static const char *usage = "x y z";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2 && argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (argc == 2) {
	if (bn_decode_vect(look, argv[1]) != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}
    } else {
	if (sscanf(argv[1], "%lf", &scan[X]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_lookat_core: bad X value - %s\n", argv[1]);
	    return GED_ERROR;
	}

	if (sscanf(argv[2], "%lf", &scan[Y]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_lookat_core: bad Y value - %s\n", argv[2]);
	    return GED_ERROR;
	}

	if (sscanf(argv[3], "%lf", &scan[Z]) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_lookat_core: bad Z value - %s\n", argv[3]);
	    return GED_ERROR;
	}

	/* convert from double to fastf_t */
	VMOVE(look, scan);
    }

    VSCALE(look, look, gedp->ged_wdbp->dbip->dbi_local2base);

    VSET(tmp, 0.0, 0.0, 1.0);
    MAT4X3PNT(eye, gedp->ged_gvp->gv_view2model, tmp);

    VSUB2(dir, eye, look);
    VUNITIZE(dir);
    bn_ae_vec(&new_az, &new_el, dir);

    VSET(gedp->ged_gvp->gv_aet, new_az, new_el, gedp->ged_gvp->gv_aet[Z]);
    _ged_mat_aet(gedp->ged_gvp);

    VJOIN1(new_center, eye, -gedp->ged_gvp->gv_scale, dir);
    MAT_DELTAS_VEC_NEG(gedp->ged_gvp->gv_center, new_center);

    bview_update(gedp->ged_gvp);

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl lookat_cmd_impl = {
    "lookat",
    ged_lookat_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd lookat_cmd = { &lookat_cmd_impl };
const struct ged_cmd *lookat_cmds[] = { &lookat_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  lookat_cmds, 1 };

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
