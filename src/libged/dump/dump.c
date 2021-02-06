/*                         D U M P . C
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
/** @file libged/dump.c
 *
 * The dump command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"

#include "../ged_private.h"


int
ged_dump_core(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_wdb *op;
    int ret;
    static const char *usage = "file.g";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    op = wdb_fopen(argv[1]);
    if (op == RT_WDB_NULL) {
	bu_vls_printf(gedp->ged_result_str, "dump: %s: cannot create", argv[1]);
	return GED_ERROR;
    }

    ret = db_dump(op, gedp->ged_wdbp->dbip);
    wdb_close(op);

    if (ret < 0) {
	bu_vls_printf(gedp->ged_result_str, "dump: %s: db_dump() error", argv[1]);
	return GED_ERROR;
    }

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl dump_cmd_impl = {
    "dump",
    ged_dump_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd dump_cmd = { &dump_cmd_impl };
const struct ged_cmd *dump_cmds[] = { &dump_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  dump_cmds, 1 };

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
