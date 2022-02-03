/*                        R E G D E F . C
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
/** @file libged/regdef.c
 *
 * The regdef command.
 *
 */

#include "common.h"
#include <stdlib.h>
#include "ged.h"

int
ged_regdef_core(struct ged *gedp, int argc, const char *argv[])
{
    int item, air, los, mat;
    static const char *usage = "item air los mat";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Get region defaults */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "ident %d air %d los %d material %d",
		      gedp->ged_wdbp->wdb_item_default,
		      gedp->ged_wdbp->wdb_air_default,
		      gedp->ged_wdbp->wdb_los_default,
		      gedp->ged_wdbp->wdb_mat_default);
	return BRLCAD_OK;
    }

    if (argc < 2 || 5 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    if (sscanf(argv[1], "%d", &item) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    gedp->ged_wdbp->wdb_item_default = item;

    if (argc == 2)
	return BRLCAD_OK;

    if (sscanf(argv[2], "%d", &air) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    gedp->ged_wdbp->wdb_air_default = air;
    if (air) {
	item = 0;
	gedp->ged_wdbp->wdb_item_default = 0;
    }

    if (argc == 3)
	return BRLCAD_OK;

    if (sscanf(argv[3], "%d", &los) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    gedp->ged_wdbp->wdb_los_default = los;

    if (argc == 4)
	return BRLCAD_OK;

    if (sscanf(argv[4], "%d", &mat) != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    gedp->ged_wdbp->wdb_mat_default = mat;

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl regdef_cmd_impl = {
    "regdef",
    ged_regdef_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd regdef_cmd = { &regdef_cmd_impl };
const struct ged_cmd *regdef_cmds[] = { &regdef_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  regdef_cmds, 1 };

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
