/*                         M O V E . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file libged/move.c
 *
 * The move command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/str.h"
#include "ged/event.h"

#include "../ged_private.h"


int
ged_move_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int event_batch = 0;
    static const char *usage = "from to";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* The destination is an object name, not a path - reject slashes so
     * we don't end up creating an object whose name contains the path
     * separator (which produces broken, unlistable hierarchies).
     */
    if (strchr(argv[2], '/') != NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: destination name may not contain slashes", argv[2]);
	return BRLCAD_ERROR;
    }

    if ((dp = db_lookup(gedp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return BRLCAD_ERROR;

    if (db_lookup(gedp->dbip, argv[2], LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: already exists", argv[2]);
	return BRLCAD_ERROR;
    }

    event_batch = ged_event_batch_begin(gedp) == GED_EVENT_OK;

    if (_ged_rename_db_object(gedp, dp, argv[2]) != BRLCAD_OK) {
	if (event_batch)
	    (void)ged_event_batch_end(gedp, NULL);
	return BRLCAD_ERROR;
    }

    _ged_dl_rename_display(gedp, argv[1], argv[2], 0);

    struct ged_event event;
    ged_event_init(&event);
    event.kind = GED_EVENT_OBJECT_RENAMED;
    event.name = argv[1];
    event.new_name = argv[2];
    (void)ged_event_publish(gedp, &event, 1, NULL);
    if (event_batch)
	(void)ged_event_batch_end(gedp, NULL);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_MOVE_COMMANDS(X, XID) \
    X(move, ged_move_core, GED_CMD_DEFAULT) \
    X(mv, ged_move_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_MOVE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_move", 1, GED_MOVE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
