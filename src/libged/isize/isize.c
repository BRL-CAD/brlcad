/*                         I S I Z E . C
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
/** @file libged/isize.c
 *
 * The isize command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_isize_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* get the isize (i.e. inverse view size) */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "%g",
		      gedp->ged_gvp->gv_isize * gedp->dbip->dbi_base2local);
	return BRLCAD_OK;
    }

    bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
    return BRLCAD_ERROR;
}

#include "../include/plugin.h"

#define GED_ISIZE_COMMANDS(X, XID) \
    X(isize, ged_isize_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_ISIZE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_isize", 1, GED_ISIZE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
