/*                         Z A P . C
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
/** @file libged/zap.c
 *
 * The zap command.
 *
 */

#include "common.h"

#include <stdlib.h>


#include "../ged_private.h"


/*
 * Erase all currently displayed geometry
 *
 * Usage:
 * zap
 *
 */
int
ged_zap_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    dl_zap(gedp, gedp->free_scene_obj);

    /* The application may need to adjust itself when the view
     * is cleared.  Since the blast command may immediately
     * re-populate the display list, we set a flag in the view
     * to inform the app a zap operation has taken place. */
    if (gedp->ged_gvp) {
	gedp->ged_gvp->gv_cleared = 1;
    }

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl clear_cmd_impl = {"clear", ged_zap_core, GED_CMD_DEFAULT};
const struct ged_cmd clear_cmd = { &clear_cmd_impl };

struct ged_cmd_impl zap_cmd_impl = {"zap", ged_zap_core, GED_CMD_DEFAULT};
const struct ged_cmd zap_cmd = { &zap_cmd_impl };

struct ged_cmd_impl Z_cmd_impl = {"Z", ged_zap_core, GED_CMD_DEFAULT};
const struct ged_cmd Z_cmd = { &Z_cmd_impl };

const struct ged_cmd *zap_cmds[] = { &clear_cmd, &zap_cmd, &Z_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  zap_cmds, 3 };

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
