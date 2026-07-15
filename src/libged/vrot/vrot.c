/*                         V R O T . C
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
/** @file libged/vrot.c
 *
 * The vrot command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "../ged_private.h"


int
ged_vrot_core(struct ged *gedp, int argc, const char *argv[])
{
    char *av[6];
    char coord;
    int ac;
    int i;
    int ret;
    mat_t rmat;
    static const char *usage = "x y z";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (argc != 2 && argc != 4) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    av[0] = (char *)argv[0];
    av[1] = "-v";
    ac = argc+1;
    for (i = 1; i < argc; ++i)
	av[i+1] = (char *)argv[i];
    av[i+1] = (char *)0;

    if ((ret = ged_rot_args(gedp, ac, (const char **)av, &coord, rmat)) != BRLCAD_OK)
	return ret;

    return _ged_do_rot(gedp, coord, rmat, NULL);
}


#include "../include/plugin.h"

GED_DEFINE_TYPED_OPERAND_SCHEMA(vrot, "vrot", "Rotate the view", "angles", BU_CMD_VALUE_NUMBER, 3, 3, "X Y Z rotation angles");

#define GED_VROT_COMMANDS(X, XID) \
    X(vrot, ged_vrot_core, GED_CMD_DEFAULT, &vrot_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_VROT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_vrot", 1, GED_VROT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
