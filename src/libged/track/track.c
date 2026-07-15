/*                         T R A C K . C
 * BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
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
/** @file libged/track.c
 *
 * Adds "tracks" to the data file given the required info
 *
 * Acknowledgements:
 * Modifications by Bob Parker (SURVICE Engineering):
 * *- adapt for use in LIBRT's database object
 * *- removed prompting for input
 * *- removed signal catching
 * *- added basename parameter
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>


#include "vmath.h"
#include "bn.h"
#include "rt/geom.h"
#include "raytrace.h"
#include "wdb.h"
#include "bu/cmdschema.h"
#include "ged.h"


static const struct bu_cmd_operand track_schema_operands[] = {
    BU_CMD_OPERAND("basename", BU_CMD_VALUE_STRING, 1, 1,
	"Track object basename", NULL),
    BU_CMD_OPERAND("dimensions", BU_CMD_VALUE_NUMBER, 13, 13,
	"Track geometry dimensions", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema track_cmd_schema = {
    "track", "Create track geometry", NULL,
    track_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};

/*
 *
 * Adds track given "wheel" info.
 *
 */
int
ged_track_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "basename rX1 rX2 rZ rR dX dZ dR iX iZ iR minX minY th";
    int parse_dummy = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* ged_track2 receives argv including the command name, so validate the
     * positional suffix without shifting its legacy calling convention. */
    if (bu_cmd_schema_parse_complete(&track_cmd_schema, &parse_dummy,
		gedp->ged_result_str, argc - 1, argv + 1) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    int ret = ged_track2(gedp->ged_result_str, wdbp, argv);
    return ret;
}


#include "../include/plugin.h"

#define GED_TRACK_COMMANDS(X, XID) \
    X(track, ged_track_core, GED_CMD_DEFAULT, &track_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_TRACK_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_track", 1, GED_TRACK_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
