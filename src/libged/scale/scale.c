/*                         S C A L E . C
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
/** @file libged/scale.c
 *
 * The scale command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bview/defines.h"

#include "../ged_private.h"


int
ged_scale_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    fastf_t sf1;
    fastf_t sf2;
    fastf_t sf3;

    if ((ret = ged_scale_args(gedp, argc, argv, &sf1, &sf2, &sf3)) != GED_OK)
	return ret;

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Can not scale xyz independently on a view.");
	return GED_ERROR;
    }

    if (sf1 <= SMALL_FASTF || INFINITY < sf1)
	return GED_OK;

    /* scale the view */
    gedp->ged_gvp->gv_scale *= sf1;

    if (gedp->ged_gvp->gv_scale < BVIEW_MINVIEWSIZE)
	gedp->ged_gvp->gv_scale = BVIEW_MINVIEWSIZE;
    gedp->ged_gvp->gv_size = 2.0 * gedp->ged_gvp->gv_scale;
    gedp->ged_gvp->gv_isize = 1.0 / gedp->ged_gvp->gv_size;
    bview_update(gedp->ged_gvp);

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl scale_cmd_impl = {"scale", ged_scale_core, GED_CMD_DEFAULT};
const struct ged_cmd scale_cmd = { &scale_cmd_impl };

struct ged_cmd_impl sca_cmd_impl = {"sca", ged_scale_core, GED_CMD_DEFAULT};
const struct ged_cmd sca_cmd = { &sca_cmd_impl };

const struct ged_cmd *scale_cmds[] = { &scale_cmd, &sca_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  scale_cmds, 2 };

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
