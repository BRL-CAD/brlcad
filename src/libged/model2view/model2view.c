/*                         M O D E L 2 V I E W . C
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

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


/**
 * Given a point in model space coordinates (in mm) convert it to view
 * (screen) coordinates which must be scaled to correspond to actual
 * screen coordinates. If no input coordinates are supplied, the
 * model2view matrix is displayed.
 */
int
ged_model2view_core(struct ged *gedp, int argc, const char *argv[])
{
    point_t view_pt;
    double model_pt[3]; /* intentionally double for scan */
    static const char *usage = "[x y z]";

    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get the model2view matrix */
    if (argc == 1) {
	bn_encode_mat(gedp->ged_result_str, gedp->ged_gvp->gv_model2view, 1);
	return BRLCAD_OK;
    }

    if (argc != 4
	|| sscanf(argv[1], "%lf", &model_pt[X]) != 1
	|| sscanf(argv[2], "%lf", &model_pt[Y]) != 1
	|| sscanf(argv[3], "%lf", &model_pt[Z]) != 1)
    {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    MAT4X3PNT(view_pt, gedp->ged_gvp->gv_model2view, model_pt);
    bn_encode_vect(gedp->ged_result_str, view_pt, 1);

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl model2view_cmd_impl = {
    "model2view",
    ged_model2view_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd model2view_cmd = { &model2view_cmd_impl };
const struct ged_cmd *model2view_cmds[] = { &model2view_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  model2view_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
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
