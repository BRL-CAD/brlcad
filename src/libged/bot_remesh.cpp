/*                     B O T _ R E M E S H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019 United States Government as represented by
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
/** @file libged/bot_remesh.cpp
 *
 * The bot "remesh" sub-command.
 *
 */

#include "common.h"

#include <openvdb/openvdb.h>
#include <openvdb/tools/VolumeToMesh.h>
#include <openvdb/tools/MeshToVolume.h>

#include "rt/geom.h"

#include "./ged_private.h"


static void
bot_remesh(void)
{
    return;
}


int
ged_bot_remesh(struct ged *gedp, int argc, const char *argv[])
{
    static const char *usage = "input.bot [output.bot]";

    char *input_bot_name;
    char *output_bot_name;
    struct directory *dp_input;
    struct directory *dp_output;
    struct rt_bot_internal *input_bot;
    struct rt_db_internal intern;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    dp_input = dp_output = RT_DIR_NULL;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* check that we are using a version 5 database */
    if (db_version(gedp->ged_wdbp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str,
		      "ERROR: Unable to remesh the current (v%d) database.\n"
		      "Use \"dbupgrade\" to upgrade this database to the current version.\n",
		      db_version(gedp->ged_wdbp->dbip));
	return GED_ERROR;
    }

    if (argc > 3) {
	bu_vls_printf(gedp->ged_result_str, "ERROR: unexpected arguments encountered\n");
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    input_bot_name = output_bot_name = (char *)argv[1];
    if (argc > 2)
	output_bot_name = (char *)argv[2];

    if (!BU_STR_EQUAL(input_bot_name, output_bot_name)) {
	GED_CHECK_EXISTS(gedp, output_bot_name, LOOKUP_QUIET, GED_ERROR);
    }

    GED_DB_LOOKUP(gedp, dp_input, input_bot_name, LOOKUP_QUIET, GED_ERROR);
    GED_DB_GET_INTERNAL(gedp, &intern, dp_input, NULL, gedp->ged_wdbp->wdb_resp, GED_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a BOT primitive\n", input_bot_name);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    input_bot = (struct rt_bot_internal *)intern.idb_ptr;
    RT_BOT_CK_MAGIC(input_bot);

    bot_remesh();

    if (BU_STR_EQUAL(input_bot_name, output_bot_name)) {
	dp_output = dp_input;
    } else {
	GED_DB_DIRADD(gedp, dp_output, output_bot_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type, GED_ERROR);
    }

    /* TODO: stash a backup if overwriting the original */

    GED_DB_PUT_INTERNAL(gedp, dp_output, &intern, gedp->ged_wdbp->wdb_resp, GED_ERROR);
    rt_db_free_internal(&intern);

    return GED_OK;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
