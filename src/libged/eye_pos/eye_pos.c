/*                         E Y E _ P O S . C
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
/** @file libged/eye_pos.c
 *
 * The eye_pos command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_eye_pos_core(struct ged *gedp, int argc, const char *argv[])
{
    point_t eye_pos;
    double scan[3];
    static const char *usage = "x y z";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get eye position */
    if (argc == 1) {
	VSCALE(eye_pos, gedp->ged_gvp->gv_eye_pos, gedp->ged_wdbp->dbip->dbi_base2local);
	bn_encode_vect(gedp->ged_result_str, eye_pos, 1);
	return GED_OK;
    }

    if (argc != 2 && argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if (argc == 2) {
	if (bn_decode_vect(eye_pos, argv[1]) != 3) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}
    } else {
	if (sscanf(argv[1], "%lf", &scan[X]) < 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_eye: bad X value %s\n", argv[1]);
	    return GED_ERROR;
	}

	if (sscanf(argv[2], "%lf", &scan[Y]) < 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_eye: bad Y value %s\n", argv[2]);
	    return GED_ERROR;
	}

	if (sscanf(argv[3], "%lf", &scan[Z]) < 1) {
	    bu_vls_printf(gedp->ged_result_str, "ged_eye: bad Z value %s\n", argv[3]);
	    return GED_ERROR;
	}

	/* convert from double to fastf_t */
	VMOVE(eye_pos, scan);
    }

    VSCALE(gedp->ged_gvp->gv_eye_pos, eye_pos, gedp->ged_wdbp->dbip->dbi_local2base);

    /* update perspective matrix */
    mike_persp_mat(gedp->ged_gvp->gv_pmat, gedp->ged_gvp->gv_eye_pos);

    bview_update(gedp->ged_gvp);

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl eye_pos_cmd_impl = {
    "eye_pos",
    ged_eye_pos_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd eye_pos_cmd = { &eye_pos_cmd_impl };
const struct ged_cmd *eye_pos_cmds[] = { &eye_pos_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  eye_pos_cmds, 1 };

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
