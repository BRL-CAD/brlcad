/*                         G R O U P . C
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
/** @file libged/group.c
 *
 * The group command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "wdb.h"

#include "../ged_private.h"


int
ged_group_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "gname object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    return _ged_combadd2(gedp, (char *)argv[1], argc-2, argv+2, 0, WMOP_UNION, 0, 0, NULL, 1);
}

#include "../include/plugin.h"

#define GED_GROUP_COMMANDS(X, XID) \
    X(g, ged_group_core, GED_CMD_DEFAULT) \
    X(group, ged_group_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_GROUP_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_group", 1, GED_GROUP_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
