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
#include "ged.h"
#include "ged/event_txn.h"

static void
track_notify_added_if_present(struct ged *gedp, const char *name)
{
    if (!gedp || !name)
	return;

    if (db_lookup(gedp->dbip, name, LOOKUP_QUIET) != RT_DIR_NULL)
	(void)ged_event_notify_object_added(gedp, name, NULL);
}

/*
 *
 * Adds track given "wheel" info.
 *
 */
int
ged_track_core(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "basename rX1 rX2 rZ rR dX dZ dR iX iZ iR minX minY th";

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

    if (argc != 15) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    int event_batch_opened = (ged_event_batch_begin(gedp) > 0);
    int ret = ged_track2(gedp->ged_result_str, wdbp, argv);
    if (ret == BRLCAD_OK) {
	struct bu_vls name = BU_VLS_INIT_ZERO;
	for (int i = 0; i < 10; i++) {
	    bu_vls_sprintf(&name, "%s.s.%d", argv[1], i);
	    track_notify_added_if_present(gedp, bu_vls_cstr(&name));
	}
	const int region_ids[] = {0, 1, 4, 5, 8, 9};
	for (size_t i = 0; i < sizeof(region_ids)/sizeof(region_ids[0]); i++) {
	    bu_vls_sprintf(&name, "%s.r.%d", argv[1], region_ids[i]);
	    track_notify_added_if_present(gedp, bu_vls_cstr(&name));
	}
	track_notify_added_if_present(gedp, argv[1]);
	bu_vls_free(&name);
    }
    if (event_batch_opened)
	ged_event_batch_end(gedp, NULL);
    return ret;
}


#include "../include/plugin.h"

#define GED_TRACK_COMMANDS(X, XID) \
    X(track, ged_track_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_TRACK_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_track", 1, GED_TRACK_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
