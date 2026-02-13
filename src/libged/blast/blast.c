/*                         B L A S T . C
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

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* First, clear the screen */
    {
	const char *av[1] = {"zap"};
	ged_exec_zap(gedp, 1, av);
    }

    /* Draw the new object(s) */
    argv[0] = "draw";
    return ged_exec_draw(gedp, argc, argv);
}

#include "../include/plugin.h"

#define GED_BLAST_COMMANDS(X, XID) \
    X(B,     ged_blast_core, GED_CMD_DEFAULT) \
    X(blast, ged_blast_core, GED_CMD_DEFAULT)

GED_DECLARE_COMMAND_SET(GED_BLAST_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_blast", 1, GED_BLAST_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
