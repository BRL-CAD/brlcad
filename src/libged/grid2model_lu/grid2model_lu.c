/*                         G R I D 2 M O D E L _ L U . C
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
/** @file libged/grid2model_lu.c
 *
 * The grid2model_lu command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_grid2model_lu_core(struct ged *gedp, int argc, const char *argv[])
{
    fastf_t f;
    point_t view_pt;
    point_t model_pt = VINIT_ZERO;
    point_t mo_view_pt;           /* model origin in view space */
    point_t diff;
    double scan[3];
    static const char *usage = "u v";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 3)
	goto bad;

    if (sscanf(argv[1], "%lf", &scan[X]) != 1 ||
	sscanf(argv[2], "%lf", &scan[Y]) != 1)
	goto bad;
    scan[Z] = 0.0;

    f = 1.0 / (gedp->ged_gvp->gv_scale * gedp->dbip->dbi_base2local);
    VSCALE(diff, scan, f);
    MAT4X3PNT(mo_view_pt, gedp->ged_gvp->gv_model2view, model_pt);
    VADD2(view_pt, mo_view_pt, diff);
    MAT4X3PNT(model_pt, gedp->ged_gvp->gv_view2model, view_pt);
    VSCALE(model_pt, model_pt, gedp->dbip->dbi_base2local);
    bn_encode_vect(gedp->ged_result_str, model_pt, 1);

    return BRLCAD_OK;

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl grid2model_lu_cmd_impl = {
    "grid2model_lu",
    ged_grid2model_lu_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd grid2model_lu_cmd = { &grid2model_lu_cmd_impl };
const struct ged_cmd *grid2model_lu_cmds[] = { &grid2model_lu_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  grid2model_lu_cmds, 1 };

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
