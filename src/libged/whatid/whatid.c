/*                         W H A T I D . C
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
/** @file libged/whatid.c
 *
 * The whatid command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"

#include "../ged_private.h"

static const struct bu_cmd_operand whatid_operands[] = {
    BU_CMD_OPERAND("region", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Database region", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema whatid_cmd_schema = {
    "whatid", "Report a region's identifier", NULL, whatid_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_whatid_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    int parse_dummy = 0;
    int operand_index = 0;
    static const char *usage = "region";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

	operand_index = bu_cmd_schema_parse_complete(&whatid_cmd_schema, &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0 || argc - 1 - operand_index != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }

    dp = db_lookup(gedp->dbip, argv[1 + operand_index], LOOKUP_NOISY);
    if (dp == RT_DIR_NULL)
	return BRLCAD_ERROR;

    if (!(dp->d_flags & RT_DIR_REGION)) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a region", argv[1 + operand_index]);
	return BRLCAD_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0)
	return BRLCAD_ERROR;
    comb = (struct rt_comb_internal *)intern.idb_ptr;

    bu_vls_printf(gedp->ged_result_str, "%ld", comb->region_id);
    rt_db_free_internal(&intern);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_WHATID_COMMANDS(X, XID) \
    X(whatid, ged_whatid_core, GED_CMD_DEFAULT, &whatid_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_WHATID_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_whatid", 1, GED_WHATID_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
