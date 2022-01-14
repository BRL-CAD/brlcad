/*                         G R I D 2 V I E W _ L U . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/grid2view_lu.c
 *
 * The grid2view_lu command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_grid2view_lu_core(struct ged *gedp, int argc, const char *argv[])
{
    fastf_t f;
    point_t view_pt;
    point_t model_pt = VINIT_ZERO;
    point_t mo_view_pt;           /* model origin in view space */
    double scan[3];
    static const char *usage = "u v";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 3)
	goto bad;

    if (sscanf(argv[1], "%lf", &scan[X]) != 1 ||
	sscanf(argv[2], "%lf", &scan[Y]) != 1)
	goto bad;
    scan[Z] = 0.0;

    MAT4X3PNT(mo_view_pt, gedp->ged_gvp->gv_model2view, model_pt);
    f = gedp->ged_gvp->gv_scale * gedp->dbip->dbi_base2local;
    VSCALE(mo_view_pt, mo_view_pt, f);
    VADD2(view_pt, mo_view_pt, scan);
    bn_encode_vect(gedp->ged_result_str, view_pt, 1);

    return GED_OK;

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return GED_ERROR;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl grid2view_lu_cmd_impl = {
    "grid2view_lu",
    ged_grid2view_lu_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd grid2view_lu_cmd = { &grid2view_lu_cmd_impl };
const struct ged_cmd *grid2view_lu_cmds[] = { &grid2view_lu_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  grid2view_lu_cmds, 1 };

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
