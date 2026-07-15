/*                         B O T _ S M O O T H . C
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
/** @file libged/bot_smooth.c
 *
 * The bot_smooth command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "rt/geom.h"

#include "../ged_private.h"

struct bot_smooth_args {
    int print_help;
    fastf_t tolerance_angle;
};

static int
bot_smooth_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;

    flat.validation.custom_validate = NULL;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help"))
	flat.operands = NULL;
    return bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
}

static const struct bu_cmd_option bot_smooth_options[] = {
    BU_CMD_FLAG("h", "help", struct bot_smooth_args, print_help,
	"Print command help"),
    BU_CMD_NUMBER("t", NULL, struct bot_smooth_args, tolerance_angle, "degrees",
	"Maximum angle between faces to smooth"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand bot_smooth_operands[] = {
    BU_CMD_OPERAND("new_bot", BU_CMD_VALUE_STRING, 1, 1,
	"Name for the smoothed BoT", NULL),
    BU_CMD_OPERAND("old_bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Source BoT object", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
const struct bu_cmd_schema ged_bot_smooth_schema = {
    "bot_smooth", "Compute BoT surface normals", bot_smooth_options,
    bot_smooth_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_smooth_schema_validate, NULL)
};

static void
bot_smooth_usage(struct bu_vls *result, const char *cmd)
{
    char *option_help = bu_cmd_schema_describe(&ged_bot_smooth_schema);

    bu_vls_sprintf(result, "Usage: %s [--help] [-t degrees] new_bot old_bot\n", cmd);
    if (option_help) {
	bu_vls_printf(result, "\nOptions:\n%s", option_help);
	bu_free(option_help, "help str");
    }
}


int
ged_bot_smooth_core(struct ged *gedp, int argc, const char *argv[])
{
    char *new_bot_name, *old_bot_name;
    struct directory *dp_old, *dp_new;
    struct rt_bot_internal *old_bot;
    struct rt_db_internal intern;
    struct bot_smooth_args args = {0, 180.0};
    const char *cmd;
    int operand_index;

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    dp_old = dp_new = RT_DIR_NULL;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    cmd = argv[0];
    argc--; argv++;
    if (!argc) {
	bot_smooth_usage(gedp->ged_result_str, cmd);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&ged_bot_smooth_schema,
	&args, gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bot_smooth_usage(gedp->ged_result_str, cmd);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_smooth_usage(gedp->ged_result_str, cmd);
	return GED_HELP;
    }
    argc -= operand_index;
    argv += operand_index;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    /* check that we are using a version 5 database */
    if (db_version(gedp->dbip) < 5) {
	bu_vls_printf(gedp->ged_result_str, "This is an older database version.\nIt does not support BOT surface normals.\nUse \"dbupgrade\" to upgrade this database to the current version.\n");
	return BRLCAD_ERROR;
    }
    new_bot_name = (char *)argv[0];
    old_bot_name = (char *)argv[1];

    GED_DB_LOOKUP(gedp, dp_old, old_bot_name, LOOKUP_QUIET, BRLCAD_ERROR);

    if (!BU_STR_EQUAL(old_bot_name, new_bot_name)) {
	GED_CHECK_EXISTS(gedp, new_bot_name, LOOKUP_QUIET, BRLCAD_ERROR);
    } else {
	dp_new = dp_old;
    }

    GED_DB_GET_INTERN(gedp, &intern, dp_old, NULL, BRLCAD_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a BOT primitive\n", old_bot_name);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    old_bot = (struct rt_bot_internal *)intern.idb_ptr;
    RT_BOT_CK_MAGIC(old_bot);

    if (rt_bot_smooth(old_bot, old_bot_name, gedp->dbip, args.tolerance_angle*DEG2RAD)) {
	bu_vls_printf(gedp->ged_result_str, "Failed to smooth %s\n", old_bot_name);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    if (dp_new == RT_DIR_NULL) {
	GED_DB_DIRADD(gedp, dp_new, new_bot_name, RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type, BRLCAD_ERROR);
    }

    GED_DB_PUT_INTERN(gedp, dp_new, &intern, BRLCAD_ERROR);
    rt_db_free_internal(&intern);

    return BRLCAD_OK;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
