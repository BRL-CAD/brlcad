/*                         V I E W 2 M O D E L _ V E C . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file libged/view2model_vec.c
 *
 * The view2model_vec command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_view2model_vec_core(struct ged *gedp, int argc, const char *argv[])
{
    point_t model_vec;
    point_t view_vec;
    mat_t inv_Viewrot;
    double scan[3];
    static const char *usage = "x y z";

    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 4)
	goto bad;

    if (sscanf(argv[1], "%lf", &scan[X]) != 1 ||
	sscanf(argv[2], "%lf", &scan[Y]) != 1 ||
	sscanf(argv[3], "%lf", &scan[Z]) != 1)
	goto bad;
    /* convert from double to fastf_t */
    VMOVE(view_vec, scan);

    bn_mat_inv(inv_Viewrot, gedp->ged_gvp->gv_rotation);
    MAT4X3PNT(model_vec, inv_Viewrot, view_vec);

    bn_encode_vect(gedp->ged_result_str, model_vec, 1);

    return BRLCAD_OK;

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}


#include "../include/plugin.h"

#define GED_VIEW2MODEL_VEC_COMMANDS(X, XID) \
    X(view2model_vec, ged_view2model_vec_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_VIEW2MODEL_VEC_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_view2model_vec", 1, GED_VIEW2MODEL_VEC_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
