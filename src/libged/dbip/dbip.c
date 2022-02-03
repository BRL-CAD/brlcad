/*                         D B I P . C
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
/** @file libged/dbip.c
 *
 * The dbip command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"

#include "../ged_private.h"


int
ged_dbip_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    /* FIXME: there be a hack here!
     * neither 'dbip' nor 'get_dbip' should exist.
     */

    /* oh my gawd, no you didn't.. this code needs to die. */
    bu_vls_printf(gedp->ged_result_str, "%p", (void *)gedp->dbip);

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl get_dbip_cmd_impl = {"get_dbip", ged_dbip_core, GED_CMD_DEFAULT};
const struct ged_cmd get_dbip_cmd = { &get_dbip_cmd_impl };

struct ged_cmd_impl dbip_cmd_impl = {"dbip", ged_dbip_core, GED_CMD_DEFAULT};
const struct ged_cmd dbip_cmd = { &dbip_cmd_impl };

const struct ged_cmd *dbip_cmds[] = { &get_dbip_cmd, &dbip_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  dbip_cmds, 2 };

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
