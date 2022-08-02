/*                         B L A S T . C
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
/** @file libged/blast.c
 *
 * The blast command.
 *
 */

#include "ged.h"

/*
 * Erase all currently displayed geometry and draw the specified object(s)
 *
 * Usage:
 * blast object(s)
 *
 */
int
ged_blast_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    /* First, clear the screen */
    {
	const char *av[2];

	av[0] = "zap";
	av[1] = (char *)0;
	ged_exec(gedp, 1, av);
    }

    /* Draw the new object(s) */
    argv[0] = "draw";
    return ged_exec(gedp, argc, argv);
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl blast_cmd_impl = {"blast", ged_blast_core, GED_CMD_DEFAULT};
const struct ged_cmd blast_cmd = { &blast_cmd_impl };

struct ged_cmd_impl B_cmd_impl = {"B", ged_blast_core, GED_CMD_DEFAULT};
const struct ged_cmd B_cmd = { &B_cmd_impl };


const struct ged_cmd *blast_cmds[] = { &blast_cmd, &B_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  blast_cmds, 2 };

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
