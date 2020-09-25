/*                         V I E W 2 M O D E L _ V E C . C
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
ged_view2model_core_vec(struct ged *gedp, int argc, const char *argv[])
{
    point_t model_vec;
    point_t view_vec;
    mat_t inv_Viewrot;
    double scan[3];
    static const char *usage = "x y z";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

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

    return GED_OK;

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl view2model_vec_cmd_impl = {
    "view2model_vec",
    ged_view2model_core_vec,
    GED_CMD_DEFAULT
};

const struct ged_cmd view2model_vec_cmd = { &view2model_vec_cmd_impl };
const struct ged_cmd *view2model_vec_cmds[] = { &view2model_vec_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  view2model_vec_cmds, 1 };

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
