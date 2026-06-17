/*                         S E T _ T R A N S P A R E N C Y . C
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
/** @file libged/set_transparency.c
 *
 * The set_transparency command.
 *
 */

#include "common.h"

#include "ged/bsg_ged_draw.h"
#include "../ged_private.h"

/*
 * Set the transparency of the specified object
 *
 * Usage:
 * set_transparency obj tr
 *
 */
int
ged_set_transparency_core(struct ged *gedp, int argc, const char *argv[])
{
    /* intentionally double for scan */
    double transparency;

    static const char *usage = "node tval";

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }


    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[2], "%lf", &transparency) != 1) {
	bu_vls_printf(gedp->ged_result_str, "dgo_set_transparency: bad transparency - %s\n", argv[2]);
	return BRLCAD_ERROR;
    }

    struct ged_draw_transaction txn =
	ged_draw_transaction_make_value(GED_DRAW_TXN_TRANSPARENCY,
					argv[1], transparency);
    ged_draw_apply_transaction(gedp, &txn, NULL);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_SET_TRANSPARENCY_COMMANDS(X, XID) \
    X(set_transparency, ged_set_transparency_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_SET_TRANSPARENCY_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_set_transparency", 1, GED_SET_TRANSPARENCY_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
