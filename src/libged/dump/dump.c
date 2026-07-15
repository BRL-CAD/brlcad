/*                         D U M P . C
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

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    if (argc != 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    op = wdb_fopen(argv[1]);
    if (op == RT_WDB_NULL) {
	bu_vls_printf(gedp->ged_result_str, "dump: %s: cannot create", argv[1]);
	return BRLCAD_ERROR;
    }

    ret = db_dump(op, gedp->dbip);
    db_close(op->dbip);

    if (ret < 0) {
	bu_vls_printf(gedp->ged_result_str, "dump: %s: db_dump() error", argv[1]);
	return BRLCAD_ERROR;
    }

    return BRLCAD_OK;
}

#include "../include/plugin.h"

GED_DEFINE_TYPED_OPERAND_SCHEMA(dump, "dump", "Write the database to a file", "output_file", BU_CMD_VALUE_FILE, 1, 1, "Output database file");

#define GED_DUMP_COMMANDS(X, XID) \
    X(dump, ged_dump_core, GED_CMD_DEFAULT, &dump_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_DUMP_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_dump", 1, GED_DUMP_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
