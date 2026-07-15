/*                   B O T _ F A C E _ S O R T . C
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
/** @file libged/bot_face_sort.c
 *
 * The bot_face_sort command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "rt/geom.h"
#include "ged.h"

struct bot_face_sort_args {
    int print_help;
};

static int
bot_face_sort_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;

    flat.validation.custom_validate = NULL;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help"))
	flat.operands = NULL;
    return bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
}

static const struct bu_cmd_option bot_face_sort_options[] = {
    BU_CMD_FLAG("h", "help", struct bot_face_sort_args, print_help,
	"Print command help"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand bot_face_sort_operands[] = {
    BU_CMD_OPERAND_VALIDATE("triangles_per_piece", BU_CMD_VALUE_INTEGER, 1, 1,
	bu_cmd_positive_integer_validate, "Positive triangle count per piece", NULL),
    BU_CMD_OPERAND("bot", BU_CMD_VALUE_DB_OBJECT, 1, BU_CMD_COUNT_UNLIMITED,
	"BoT object to sort", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
const struct bu_cmd_schema ged_bot_face_sort_schema = {
    "bot_face_sort", "Sort BoT faces into pieces", bot_face_sort_options,
    bot_face_sort_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_face_sort_schema_validate, NULL)
};

static void
bot_face_sort_usage(struct bu_vls *result, const char *cmd)
{
    char *option_help = bu_cmd_schema_describe(&ged_bot_face_sort_schema);

    bu_vls_sprintf(result, "Usage: %s [--help] triangles_per_piece bot [bot2 ...]\n", cmd);
    if (option_help) {
	bu_vls_printf(result, "\nOptions:\n%s", option_help);
	bu_free(option_help, "help str");
    }
}


int
ged_bot_face_sort_core(struct ged *gedp, int argc, const char *argv[])
{
    int i;
    int tris_per_piece=0;
    struct bot_face_sort_args args = {0};
    const char *cmd;
    int operand_index;

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    cmd = argv[0];
    argc--; argv++;
    if (!argc) {
	bot_face_sort_usage(gedp->ged_result_str, cmd);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&ged_bot_face_sort_schema,
	&args, gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bot_face_sort_usage(gedp->ged_result_str, cmd);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_face_sort_usage(gedp->ged_result_str, cmd);
	return GED_HELP;
    }
    argc -= operand_index;
    argv += operand_index;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    bu_cmd_integer_from_str(&tris_per_piece, argv[0]);

    for (i = 1; i < argc; i++) {
	struct directory *dp;
	struct rt_db_internal intern;
	struct rt_bot_internal *bot;

	dp = db_lookup(gedp->dbip, argv[i], LOOKUP_NOISY);
	if (dp == RT_DIR_NULL) {
	    continue;
	}

	GED_DB_GET_INTERN(gedp, &intern, dp, bn_mat_identity, BRLCAD_ERROR);

	if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	    rt_db_free_internal(&intern);
	    bu_vls_printf(gedp->ged_result_str,
			  "%s is not a BOT primitive, skipped\n",
			  dp->d_namep);
	    continue;
	}

	bot = (struct rt_bot_internal *)intern.idb_ptr;
	RT_BOT_CK_MAGIC(bot);

	bu_log("processing %s (%zu triangles)\n", dp->d_namep, bot->num_faces);

	if (rt_bot_sort_faces(bot, tris_per_piece)) {
	    rt_db_free_internal(&intern);
	    bu_vls_printf(gedp->ged_result_str,
			  "Face sort failed for %s, this BOT not sorted\n",
			  dp->d_namep);
	    continue;
	}

	GED_DB_PUT_INTERN(gedp, dp, &intern, BRLCAD_ERROR);
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
