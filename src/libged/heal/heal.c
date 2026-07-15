/*                          H E A L . C
 * BRL-CAD
 *
 * Copyright (c) 2016-2026 United States Government as represented by
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
/** @file heal.c
 *
 * Heal command for the mesh healing operations
 *
 */

#include "common.h"
#include "analyze.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bg/chull.h"
#include "bu/cmdschema.h"
#include "rt/geom.h"
#include "wdb.h"
#include "../ged_private.h"


static const struct bu_cmd_operand heal_operands[] = {
    BU_CMD_OPERAND("bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"BOT solid to heal", "ged.db_object"),
    BU_CMD_OPERAND("zipper_tolerance", BU_CMD_VALUE_NUMBER, 0, 1,
	"Optional zipper tolerance", NULL),
    BU_CMD_OPERAND_NULL
};

static const struct bu_cmd_schema heal_cmd_schema = {
    "heal", "Heal a BOT mesh", NULL, heal_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


int
ged_heal_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *bot_dp;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    const char *cmd = argv[0];
    const char *primitive = NULL;

    double zipper_tol = 0;
    int operand_index = 0;
    int parse_dummy = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

	if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s <bot_solid>", argv[0]);
	return GED_HELP;
    }

	operand_index = bu_cmd_schema_parse_complete(&heal_cmd_schema, &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0 || argc - 1 - operand_index < 1 || argc - 1 - operand_index > 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s <bot_solid> [zipper_tolerance]", argv[0]);
	return BRLCAD_ERROR;
    }

    primitive = argv[1 + operand_index];

	if (argc - 1 - operand_index > 1)
	zipper_tol = atof(argv[2 + operand_index]);

    /* get bot */
    GED_DB_LOOKUP(gedp, bot_dp, primitive, LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
    GED_DB_GET_INTERN(gedp, &intern, bot_dp, bn_mat_identity, BRLCAD_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT solid!", cmd, primitive);
	return BRLCAD_ERROR;
    }

    bot = (struct rt_bot_internal *)intern.idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    analyze_heal_bot(bot, zipper_tol);
    rt_db_put_internal(bot_dp, gedp->dbip, &intern);

    bu_vls_printf(gedp->ged_result_str, "Healed Mesh!");

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_HEAL_COMMANDS(X, XID) \
    X(heal, ged_heal_core, GED_CMD_DEFAULT, &heal_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_HEAL_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_heal", 1, GED_HEAL_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
