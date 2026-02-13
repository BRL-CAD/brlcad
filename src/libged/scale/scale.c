/*                         S C A L E . C
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
/** @file libged/scale.c
 *
 * The scale command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bv/defines.h"

#include "../ged_private.h"


int
ged_scale_core(struct ged *gedp, int argc, const char *argv[])
{
    int ret;
    fastf_t sf1;
    fastf_t sf2;
    fastf_t sf3;

    if ((ret = ged_scale_args(gedp, argc, argv, &sf1, &sf2, &sf3)) != BRLCAD_OK)
	return ret;

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Can not scale xyz independently on a view.");
	return BRLCAD_ERROR;
    }

    if (sf1 <= SMALL_FASTF || INFINITY < sf1)
	return BRLCAD_OK;

    /* scale the view */
    gedp->ged_gvp->gv_scale *= sf1;

    if (gedp->ged_gvp->gv_scale < BV_MINVIEWSIZE)
	gedp->ged_gvp->gv_scale = BV_MINVIEWSIZE;
    gedp->ged_gvp->gv_size = 2.0 * gedp->ged_gvp->gv_scale;
    gedp->ged_gvp->gv_isize = 1.0 / gedp->ged_gvp->gv_size;
    bv_update(gedp->ged_gvp);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_SCALE_COMMANDS(X, XID) \
    X(scale, ged_scale_core, GED_CMD_DEFAULT) \
    X(sca, ged_scale_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_SCALE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_scale", 1, GED_SCALE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
