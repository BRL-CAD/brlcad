/*              I M P O R T F G 4 S E C T I O N . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2022 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file libged/importFg4Section.c
 *
 * This file calls the functions in wdb_importFg4Section.c with the
 * correct wdb, which is derived from the passed in gedp. It imports a
 * Fastgen4 section from a string. This section can only contain
 * GRIDs, CTRIs and CQUADs.
 *
 */
#include "common.h"

#include "../ged_private.h"


extern int wdb_importFg4Section_cmd(void *data, int argc, const char *argv[]);

int
ged_importFg4Section_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "obj section";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    return wdb_importFg4Section_cmd(gedp->ged_wdbp, argc, argv);
}
/** @} */

#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl importFg4Section_cmd_impl = {
    "importFg4Section",
    ged_importFg4Section_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd importFg4Section_cmd = { &importFg4Section_cmd_impl };
const struct ged_cmd *importFg4Section_cmds[] = { &importFg4Section_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  importFg4Section_cmds, 1 };

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
