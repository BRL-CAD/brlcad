/*                        I T E M . C
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
/** @file libged/item.c
 *
 * The item command.
 *
 */

#include "common.h"
#include <stdlib.h>
#include "bu/cmdschema.h"
#include "ged.h"


static const struct bu_cmd_operand item_schema_operands[] = {
    BU_CMD_OPERAND("region", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Region object", "ged.db_object"),
    BU_CMD_OPERAND("ident", BU_CMD_VALUE_INTEGER, 1, 1,
	"Region identifier", NULL),
    BU_CMD_OPERAND("air_material_los", BU_CMD_VALUE_INTEGER, 0, 3,
	"Optional air, material, and LOS values", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema item_cmd_schema = {
    "item", "Set region item attributes", NULL,
    item_schema_operands, BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_item_core(struct ged *gedp, int argc, const char *argv[])
{
    int status = BRLCAD_OK;
    struct directory *dp = NULL;
    int GIFTmater = 0;
    int GIFTmater_set = 0;
    int air = 0;
    int ident = 0;
    int los = 0;
    int los_set = 0;
    struct rt_db_internal intern;
    struct rt_comb_internal *comb = NULL;
    static const char *usage = "region ident [air [material [los]]]";
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


    operand_index = bu_cmd_schema_parse_complete(&item_cmd_schema,
	&parse_dummy, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

    GED_DB_LOOKUP(gedp, dp, argv[0], LOOKUP_NOISY, BRLCAD_ERROR);
    GED_CHECK_COMB(gedp, dp, BRLCAD_ERROR);
    GED_CHECK_REGION(gedp, dp, BRLCAD_ERROR);

    ident = (int)strtol(argv[1], NULL, 0);

    /*
     * If <air> is not included, it is assumed to be zero.
     * If, on the other hand, either of <GIFTmater> and <los>
     * is not included, it is left unchanged.
     */
    if (argc > 2) {
	air = (int)strtol(argv[2], NULL, 0);
    }

    if (argc > 3) {
	GIFTmater = (int)strtol(argv[3], NULL, 0);
	GIFTmater_set = 1;
    }

    if (argc > 4) {
	los = (int)strtol(argv[4], NULL, 0);
	los_set = 1;
    }

    GED_DB_GET_INTERN(gedp, &intern, dp, (fastf_t *)NULL, BRLCAD_ERROR);

    comb = (struct rt_comb_internal *)intern.idb_ptr;
    RT_CK_COMB(comb);
    comb->region_id = ident;
    comb->aircode = air;

    if (GIFTmater_set) {
	comb->GIFTmater = GIFTmater;
    }

    if (los_set) {
	comb->los = los;
    }

    GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);

    return status;
}


#include "../include/plugin.h"

#define GED_ITEM_COMMANDS(X, XID) \
    X(item, ged_item_core, GED_CMD_DEFAULT, &item_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_ITEM_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_item", 1, GED_ITEM_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
