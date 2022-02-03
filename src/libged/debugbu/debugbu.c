/*                         D E B U G B U . C
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
/** @file libged/debugbu.c
 *
 * The debugbu command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/debug.h"
#include "../ged_private.h"


int
ged_debugbu_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[hex_code]";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* get libbu's debug bit vector */
    if (argc == 1) {
	bu_vls_printb(gedp->ged_result_str, "Possible flags", 0xffffffffL, BU_DEBUG_FORMAT);
	bu_vls_printf(gedp->ged_result_str, "\n");
    } else {
	/* set libbu's debug bit vector */
	if (sscanf(argv[1], "%x", (unsigned int *)&bu_debug) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return BRLCAD_ERROR;
	}
    }

    bu_vls_printb(gedp->ged_result_str, "bu_debug", bu_debug, BU_DEBUG_FORMAT);
    bu_vls_printf(gedp->ged_result_str, "\n");

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl debugbu_cmd_impl = {
    "debugbu",
    ged_debugbu_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd debugbu_cmd = { &debugbu_cmd_impl };
const struct ged_cmd *debugbu_cmds[] = { &debugbu_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  debugbu_cmds, 1 };

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
