/*                         B O T _ F L I P . C
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
/** @file libged/bot_flip.c
 *
 * The bot_flip command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "bu/path.h"
#include "rt/geom.h"

#include "../ged_private.h"

struct bot_flip_args {
    int print_help;
};

static const struct bu_cmd_option bot_flip_options[] = {
    BU_CMD_FLAG("h", "help", struct bot_flip_args, print_help,
	"Print command help"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand bot_flip_operands[] = {
    BU_CMD_OPERAND("bot", BU_CMD_VALUE_DB_OBJECT, 1, BU_CMD_COUNT_UNLIMITED,
	"BoT object to flip", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
const struct bu_cmd_schema ged_bot_flip_schema = {
    "bot_flip", "Reverse BoT orientation", bot_flip_options,
    bot_flip_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, NULL)
};

static void
bot_flip_usage(struct bu_vls *result, const char *cmd)
{
    char *help = bu_cmd_schema_help(&ged_bot_flip_schema, cmd);

    if (help) {
	bu_vls_sprintf(result, "%s", help);
	bu_free(help, "command schema help");
    }
}


int
ged_bot_flip_core(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct bot_flip_args args = {0};
    const char *cmd;
    int operand_index;

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    cmd = argv[0];
    argc--; argv++;
    if (!argc) {
	bot_flip_usage(gedp->ged_result_str, cmd);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&ged_bot_flip_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bot_flip_usage(gedp->ged_result_str, cmd);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_flip_usage(gedp->ged_result_str, cmd);
	return GED_HELP;
    }
    argc -= operand_index;
    argv += operand_index;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    for (i = 0; i < argc; ++i) {
	/* Skip past any path elements */
	char *obj = bu_path_basename(argv[i], NULL);

	if (BU_STR_EQUAL(obj, ".")) {
	    /* malformed path, lookup using exactly what was provided */
	    bu_free(obj, "free bu_path_basename");
	    obj = bu_strdup(argv[i]);
	}

	if ((dp = db_lookup(gedp->dbip, obj, LOOKUP_QUIET)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s: db_lookup(%s) error\n", cmd, obj);
	} else {
	    GED_DB_GET_INTERN(gedp, &intern, dp, bn_mat_identity, BRLCAD_ERROR);

	    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
		bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT solid!\n", cmd, obj);
	    } else {
		bot = (struct rt_bot_internal *)intern.idb_ptr;
		rt_bot_flip(bot);

		GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);
	    }
	}
	bu_free(obj, "free obj");
    }

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
