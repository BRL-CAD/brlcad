/*                         C O P Y . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2021 United States Government as represented by
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
/** @file libged/copy.c
 *
 * The copy command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/str.h"
#include "bu/units.h"

#include "../ged_private.h"


int
ged_copy_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *from_dp;
    struct bu_external external;
    static const char *usage = "from to";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 3) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    GED_DB_LOOKUP(gedp, from_dp, argv[1], LOOKUP_NOISY, GED_ERROR & GED_QUIET);
    GED_CHECK_EXISTS(gedp, argv[2], LOOKUP_QUIET, GED_ERROR);

    if (db_get_external(&external, from_dp, gedp->dbip)) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting\n");
	return GED_ERROR;
    }

    if (wdb_export_external(gedp->ged_wdbp, &external, argv[2],
			    from_dp->d_flags,  from_dp->d_minor_type) < 0) {
	bu_free_external(&external);
	bu_vls_printf(gedp->ged_result_str,
		      "Failed to write new object (%s) to database - aborting!!\n",
		      argv[2]);
	return GED_ERROR;
    }

    bu_free_external(&external);

    return GED_OK;
}

#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl copy_cmd_impl = {"copy", ged_copy_core, GED_CMD_DEFAULT};
const struct ged_cmd copy_cmd = { &copy_cmd_impl };

struct ged_cmd_impl cp_cmd_impl = {"cp", ged_copy_core, GED_CMD_DEFAULT};
const struct ged_cmd cp_cmd = { &cp_cmd_impl };

const struct ged_cmd *copy_cmds[] = { &copy_cmd, &cp_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  copy_cmds, 2 };

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
