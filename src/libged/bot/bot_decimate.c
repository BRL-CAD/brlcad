/*                         B O T _ D E C I M A T E . C
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
/** @file libged/bot_decimate.c
 *
 * The bot_decimate command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "rt/geom.h"
#include "ged.h"


struct bot_decimate_args {
    int print_help;
    fastf_t max_chord_error;
    fastf_t max_normal_error;
    fastf_t min_edge_length;
    fastf_t feature_size;
};


static const struct bu_cmd_option bot_decimate_schema_options[] = {
    BU_CMD_FLAG("h", "help", struct bot_decimate_args, print_help,
	"Print command help"),
    BU_CMD_NONNEGATIVE_NUMBER("c", NULL, struct bot_decimate_args,
	max_chord_error, "error", "Maximum chord error"),
    BU_CMD_NONNEGATIVE_NUMBER("n", NULL, struct bot_decimate_args,
	max_normal_error, "error", "Maximum normal error"),
    BU_CMD_NONNEGATIVE_NUMBER("e", NULL, struct bot_decimate_args,
	min_edge_length, "length", "Minimum edge length"),
    BU_CMD_NONNEGATIVE_NUMBER("f", NULL, struct bot_decimate_args,
	feature_size, "size", "Feature size for the GCT decimator"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand bot_decimate_schema_operands[] = {
    BU_CMD_OPERAND("new_bot", BU_CMD_VALUE_STRING, 1, 1,
	"Name for the decimated BOT", NULL),
    BU_CMD_OPERAND("current_bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Source BOT object", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};


static int
bot_decimate_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;
    int ret;
    int help;
    int has_feature;
    int has_legacy;

    help = bu_cmd_schema_option_present(schema, argc, argv, "help");
    flat.validation.custom_validate = NULL;
    if (help)
	flat.operands = NULL;
    ret = bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
    if (ret || result->state != BU_CMD_VALIDATE_VALID || help)
	return ret;

    has_feature = bu_cmd_schema_option_present(schema, argc, argv, "f");
    has_legacy = bu_cmd_schema_option_present(schema, argc, argv, "c") ||
	bu_cmd_schema_option_present(schema, argc, argv, "n") ||
	bu_cmd_schema_option_present(schema, argc, argv, "e");
    if (!has_feature && !has_legacy) {
	result->state = BU_CMD_VALIDATE_INCOMPLETE;
	result->token_start = cursor_arg < argc ? cursor_arg : argc;
	result->token_end = result->token_start;
	result->expected = BU_CMD_EXPECT_OPTION;
	result->completion_type = BU_CMD_VALUE_NUMBER;
	result->hint = "select -f or one of -c, -n, and -e";
	return 0;
    }
    if (has_feature && has_legacy) {
	result->state = BU_CMD_VALIDATE_INVALID;
	result->token_start = cursor_arg < argc ? cursor_arg : argc;
	result->token_end = result->token_start;
	result->expected = BU_CMD_EXPECT_OPTION;
	result->completion_type = BU_CMD_VALUE_NUMBER;
	result->hint = "-f may not be used with -c, -n, or -e";
	return 0;
    }

    return 0;
}


const struct bu_cmd_schema ged_bot_decimate_schema = {
    "bot_decimate", "Reduce BOT triangle count", bot_decimate_schema_options,
    bot_decimate_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_decimate_schema_validate, NULL)
};


int
ged_bot_decimate_core(struct ged *gedp, int argc, const char *argv[])
{
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    struct directory *dp;
    struct bot_decimate_args args = {0, -1.0, -1.0, -1.0, -1.0};
    const char *cmdname;
    int operand_index;
    static const char *usage = "-f feature_size (to use the newer GCT decimator)"
			       "\nOR: -c maximum_chord_error -n maximum_normal_error -e minimum_edge_length new_bot_name current_bot_name";

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

    cmdname = argv[0];

    operand_index = bu_cmd_schema_parse_complete(&ged_bot_decimate_schema,
	&args, gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

    if (args.print_help) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmdname, usage);
	return GED_HELP;
	}

    /* make sure new solid does not already exist */
    GED_CHECK_EXISTS(gedp, argv[0], LOOKUP_QUIET, BRLCAD_ERROR);

    /* make sure current solid does exist */
    GED_DB_LOOKUP(gedp, dp, argv[1], LOOKUP_QUIET, BRLCAD_ERROR);

    /* import the current solid */
    RT_DB_INTERNAL_INIT(&intern);
    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    GED_DB_GET_INTERN(gedp, &intern, dp, NULL, BRLCAD_ERROR);

    /* make sure this is a BOT solid */
    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s is not a BOT solid\n", argv[1]);
	rt_db_free_internal(&intern);
	return BRLCAD_ERROR;
    }

    bot = (struct rt_bot_internal *)intern.idb_ptr;

    RT_BOT_CK_MAGIC(bot);

    /* convert maximum error, edge length, and feature size to mm */
    if (args.max_chord_error > 0.0) {
	args.max_chord_error = args.max_chord_error * gedp->dbip->dbi_local2base;
    }

    if (args.min_edge_length > 0.0) {
	args.min_edge_length = args.min_edge_length * gedp->dbip->dbi_local2base;
    }

    if (args.feature_size >= 0.0) {
	/* use the new GCT decimator */
	const size_t orig_num_faces = bot->num_faces;
	size_t edges_removed;
	args.feature_size *= gedp->dbip->dbi_local2base;
	edges_removed = rt_bot_decimate_gct(bot, args.feature_size);
	bu_log("original face count = %zu\n", orig_num_faces);
	bu_log("\tedges removed = %zu\n", edges_removed);
	bu_log("\tnew face count = %zu\n", bot->num_faces);
    } else {
	/* use the old decimator */
	if (rt_bot_decimate(bot, args.max_chord_error, args.max_normal_error,
		args.min_edge_length) < 0) {
	    bu_vls_printf(gedp->ged_result_str, "Decimation Error\n");
	    rt_db_free_internal(&intern);
	    return BRLCAD_ERROR;
	}
    }

    /* save the result to the database */
    /* XXX - should this be rt_db_put_internal() instead? */
    if (wdb_put_internal(wdbp, argv[0], &intern, 1.0) < 0) {
	bu_vls_printf(gedp->ged_result_str,
		      "Failed to write decimated BOT back to database\n");
	return BRLCAD_ERROR;
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
