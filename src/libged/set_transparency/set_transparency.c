/*                         S E T _ T R A N S P A R E N C Y . C
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
/** @file libged/set_transparency.c
 *
 * The set_transparency command.
 *
 */

#include "common.h"


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
    struct directory **dpp;

    /* intentionally double for scan */
    double transparency;

    static const char *usage = "node tval";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
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

    if ((dpp = _ged_build_dpp(gedp, argv[1])) == NULL) {
	return BRLCAD_OK;
    }

    dl_set_transparency(gedp, dpp, transparency);

    if (dpp != (struct directory **)NULL)
	bu_free((void *)dpp, "ged_set_transparency_core: directory pointers");

    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl set_transparency_cmd_impl = {
    "set_transparency",
    ged_set_transparency_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd set_transparency_cmd = { &set_transparency_cmd_impl };
const struct ged_cmd *set_transparency_cmds[] = { &set_transparency_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  set_transparency_cmds, 1 };

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
