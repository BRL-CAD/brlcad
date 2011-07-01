/*                         B O T _ S M O O T H . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/bot_smooth.c
 *
 * The bot_smooth command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "cmd.h"
#include "rtgeom.h"

#include "./ged_private.h"


int
ged_bot_smooth(struct ged *gedp, int argc, const char *argv[])
{
    char *new_bot_name, *old_bot_name;
    struct directory *dp_old, *dp_new;
    struct rt_bot_internal *old_bot;
    struct rt_db_internal intern;
    fastf_t tolerance_angle=180.0;
    int arg_index=1;
    static const char *usage = "[-t ntol] new_bot old_bot";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    dp_old = dp_new = RT_DIR_NULL;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* check that we are using a version 5 database */
    if (db_version(gedp->ged_wdbp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str, "This is an older database version.\nIt does not support BOT surface normals.\nUse \"dbupgrade\" to upgrade this database to the current version.\n");
	return GED_ERROR;
    }

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    while (*argv[arg_index] == '-') {
	/* this is an option */
	if (BU_STR_EQUAL(argv[arg_index], "-t")) {
	    arg_index++;
	    tolerance_angle = atof(argv[arg_index]);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	    return GED_ERROR;
	}
	arg_index++;
    }

    if (arg_index >= argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    new_bot_name = (char *)argv[arg_index++];
    old_bot_name = (char *)argv[arg_index];

    GED_DB_LOOKUP(gedp, dp_old, old_bot_name, LOOKUP_QUIET, GED_ERROR);

    if (!BU_STR_EQUAL(old_bot_name, new_bot_name)) {
	GED_CHECK_EXISTS(gedp, new_bot_name, LOOKUP_QUIET, GED_ERROR);
    } else {
	dp_new = dp_old;
    }

    GED_DB_GET_INTERNAL(gedp, &intern, dp_old, NULL, gedp->ged_wdbp->wdb_resp, GED_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a BOT primitive\n", old_bot_name);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    old_bot = (struct rt_bot_internal *)intern.idb_ptr;
    RT_BOT_CK_MAGIC(old_bot);

    if (rt_bot_smooth(old_bot, old_bot_name, gedp->ged_wdbp->dbip, tolerance_angle*M_PI/180.0)) {
	bu_vls_printf(gedp->ged_result_str, "Failed to smooth %s\n", old_bot_name);
	rt_db_free_internal(&intern);
	return GED_ERROR;
    }

    if (dp_new == RT_DIR_NULL) {
	GED_DB_DIRADD(gedp, dp_new, new_bot_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (genptr_t)&intern.idb_type, GED_ERROR);
    }

    GED_DB_PUT_INTERNAL(gedp, dp_new, &intern, gedp->ged_wdbp->wdb_resp, GED_ERROR);
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
