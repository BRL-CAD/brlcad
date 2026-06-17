/*                         S H A D E D _ M O D E . C
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
/** @file libged/shaded_mode.c
 *
 * The shaded_mode command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "ged/bsg_ged_draw.h"
#include "../ged_private.h"

/*
 * Set/get the shaded mode.
 *
 * Usage:
 * shaded_mode [0|1|2]
 *
 */
int
ged_shaded_mode_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "[0|1|2]";

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc > 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    /* get shaded mode */
    if (argc == 1) {
	bsg_draw_mode mode = ged_draw_default_mode(gedp);
	bu_vls_printf(gedp->ged_result_str, "%d", (int)mode);
	return BRLCAD_OK;
    }

    /* set shaded mode */
    if (argc == 2) {
	int shaded_mode;

	if (sscanf(argv[1], "%d", &shaded_mode) != 1)
	    goto bad;

	if (shaded_mode < 0 || 2 < shaded_mode)
	    goto bad;

	struct ged_draw_transaction txn =
	    ged_draw_transaction_make_value(GED_DRAW_TXN_DEFAULT_DRAW_MODE,
					    NULL, (double)shaded_mode);
	ged_draw_apply_transaction(gedp, &txn, NULL);
	return BRLCAD_OK;
    }

bad:
    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
    return BRLCAD_ERROR;
}


#include "../include/plugin.h"

#define GED_SHADED_MODE_COMMANDS(X, XID) \
    X(shaded_mode, ged_shaded_mode_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_SHADED_MODE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_shaded_mode", 1, GED_SHADED_MODE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
