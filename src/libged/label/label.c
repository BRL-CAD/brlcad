/*                         L A B E L . C
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
/** @file libged/label.c
 *
 * The label command.
 *
 */

#include "ged.h"

/*
 * Arrange for objects' label(s) to be drawn.
 *
 * Usage:
 * label object(s)
 *
 */
int
ged_label_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "object(s)";

    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_vls_printf(gedp->ged_result_str, "Not yet implemented!\n%s\n%s\n", argv[0], usage);
    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_LABEL_COMMANDS(X, XID) \
    X(label, ged_label_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_LABEL_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_label", 1, GED_LABEL_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
