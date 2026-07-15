/*                        E D C O M B . C
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
/** @file libged/edcomb.c
 *
 * The edcomb command.
 *
 */

#include "ged.h"

#include <stdlib.h>

#include "bu/cmdschema.h"


static const struct bu_cmd_operand edcomb_schema_operands[] = {
    BU_CMD_OPERAND("combination", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Combination to update", "ged.db_object"),
    BU_CMD_OPERAND("region_flag", BU_CMD_VALUE_STRING, 1, 1,
	"Region flag", NULL),
    BU_CMD_OPERAND("region_fields", BU_CMD_VALUE_INTEGER, 4, 4,
	"Region ID, air, LOS, and material ID", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema edcomb_cmd_schema = {
    "edcomb", "Set combination region fields", NULL,
    edcomb_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_edcomb_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int regionid, air, mat, los;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb;
    static const char *usage = "combname region_flag region_id air los material_id";
    int operand_index;
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


    operand_index = bu_cmd_schema_parse_complete(&edcomb_cmd_schema,
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argv += operand_index + 1;

    GED_DB_LOOKUP(gedp, dp, argv[0], LOOKUP_NOISY, BRLCAD_ERROR);
    GED_CHECK_COMB(gedp, dp, BRLCAD_ERROR);

    regionid = (int)strtol(argv[2], NULL, 0);
    air = (int)strtol(argv[3], NULL, 0);
    los = (int)strtol(argv[4], NULL, 0);
    mat = (int)strtol(argv[5], NULL, 0);

    GED_DB_GET_INTERN(gedp, &intern, dp, (fastf_t *)NULL, BRLCAD_ERROR);
    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);

    if (argv[1][0] == 'R' || strtol(argv[1], NULL, 0))
	comb->region_flag = 1;
    else
	comb->region_flag = 0;

    comb->region_id = regionid;
    comb->aircode = air;
    comb->los = los;
    comb->GIFTmater = mat;
    GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_EDCOMB_COMMANDS(X, XID) \
    X(edcomb, ged_edcomb_core, GED_CMD_DEFAULT, &edcomb_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_EDCOMB_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_edcomb", 1, GED_EDCOMB_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
