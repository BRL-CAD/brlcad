/*                         I N S T A N C E . C
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
/** @file libged/instance.c
 *
 * The instance command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "wdb.h"

#include "../ged_private.h"


int
ged_instance_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    db_op_t oper;
    static const char *usage = "obj comb [op]";

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

    if (argc < 3 || 4 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    if ((dp = db_lookup(gedp->ged_wdbp->dbip,  argv[1], LOOKUP_NOISY)) == RT_DIR_NULL)
	return GED_ERROR;

    oper = WMOP_UNION;
    if (argc == 4)
	oper = db_str2op(argv[3]);

    if (oper == DB_OP_NULL) {
	bu_vls_printf(gedp->ged_result_str, "bad operation: %c (0x%x)\n", argv[3][0], argv[3][0]);
	return GED_ERROR;
    }

    if (_ged_combadd(gedp, dp, (char *)argv[2], 0, oper, 0, 0) == RT_DIR_NULL)
	return GED_ERROR;

    return GED_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl instance_cmd_impl = {"instance", ged_instance_core, GED_CMD_DEFAULT};
const struct ged_cmd instance_cmd = { &instance_cmd_impl };

struct ged_cmd_impl i_cmd_impl = {"i", ged_instance_core, GED_CMD_DEFAULT};
const struct ged_cmd i_cmd = { &i_cmd_impl };


const struct ged_cmd *instance_cmds[] = { &instance_cmd, &i_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  instance_cmds, 2 };

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
