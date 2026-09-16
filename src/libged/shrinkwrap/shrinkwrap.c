/*                     S H R I N K W R A P . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file libged/shrinkwrap.c
 *
 * The shrinkwrap command.
 *
 */

#include "common.h"

#include "../ged_private.h"


int
ged_shrinkwrap_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    bu_vls_trunc(gedp->ged_result_str, 0);
    bu_vls_printf(gedp->ged_result_str, "%s command is not yet implemented\n", argv[0]);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_SHRINKWRAP_COMMANDS(X, XID) \
    X(shrinkwrap, ged_shrinkwrap_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_SHRINKWRAP_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_shrinkwrap", 1, GED_SHRINKWRAP_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
