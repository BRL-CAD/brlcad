/*                         B O T _ M E R G E . C
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
/** @file libged/bot_merge.c
 *
 * The bot_merge command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "rt/geom.h"

#include "../ged_private.h"

struct bot_merge_args {
    int print_help;
};

static int
bot_merge_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;

    flat.validation.custom_validate = NULL;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help"))
	flat.operands = NULL;
    return bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
}

static const struct bu_cmd_option bot_merge_options[] = {
    BU_CMD_FLAG("h", "help", struct bot_merge_args, print_help,
	"Print command help"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand bot_merge_operands[] = {
    BU_CMD_OPERAND("destination", BU_CMD_VALUE_STRING, 1, 1,
	"Name for the merged BoT", NULL),
    BU_CMD_OPERAND("source", BU_CMD_VALUE_DB_OBJECT, 1, BU_CMD_COUNT_UNLIMITED,
	"Source BoT object", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
const struct bu_cmd_schema ged_bot_merge_schema = {
    "bot_merge", "Merge BoT objects", bot_merge_options,
    bot_merge_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_merge_schema_validate, NULL)
};

static void
bot_merge_usage(struct bu_vls *result, const char *cmd)
{
    char *option_help = bu_cmd_schema_describe(&ged_bot_merge_schema);

    bu_vls_sprintf(result, "Usage: %s [--help] destination source [source ...]\n", cmd);
    if (option_help) {
	bu_vls_printf(result, "\nOptions:\n%s", option_help);
	bu_free(option_help, "help str");
    }
}


int
ged_bot_merge_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp, *new_dp;
    struct rt_db_internal intern;
    struct rt_bot_internal **bots;
    int i, idx;
    struct bot_merge_args args = {0};
    const char *cmd;
    int operand_index;

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    cmd = argv[0];
    argc--; argv++;
    if (!argc) {
	bot_merge_usage(gedp->ged_result_str, cmd);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&ged_bot_merge_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bot_merge_usage(gedp->ged_result_str, cmd);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_merge_usage(gedp->ged_result_str, cmd);
	return GED_HELP;
    }
    argc -= operand_index;
    argv += operand_index;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);

    bots = (struct rt_bot_internal **)bu_calloc(argc - 1, sizeof(struct rt_bot_internal *), "bot internal");

    /* read in all the bots */
    for (idx = 0, i = 2; i < argc; ++i) {
	if ((dp = db_lookup(gedp->dbip, argv[i], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    continue;
	}

	GED_DB_GET_INTERN(gedp, &intern, dp, bn_mat_identity, BRLCAD_ERROR);

	if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	    bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT solid!  Skipping.\n", cmd, argv[i]);
	    rt_db_free_internal(&intern);
	    continue;
	}

	bots[idx] = (struct rt_bot_internal *)intern.idb_ptr;

	intern.idb_ptr = (void *)0;
	rt_db_free_internal(&intern);

	RT_BOT_CK_MAGIC(bots[idx]);
	idx++;
    }

    if (idx == 0) {
	bu_vls_printf(gedp->ged_result_str, "%s: No BOT solids given.\n", cmd);
	bu_free(bots, "bots");
	return BRLCAD_ERROR;
    }

    RT_DB_INTERNAL_INIT(&intern);
    intern.idb_type = ID_BOT;
    intern.idb_major_type = DB5_MAJORTYPE_BRLCAD;
    intern.idb_minor_type = DB5_MINORTYPE_BRLCAD_BOT;
    intern.idb_meth = &OBJ[ID_BOT];
    intern.idb_ptr = rt_bot_merge(idx, (const struct rt_bot_internal * const *)(bots));

    GED_DB_DIRADD(gedp, new_dp, argv[1], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERN(gedp, new_dp, &intern, BRLCAD_ERROR);

    for (i = 0; i < idx; ++i) {
	/* fill in an rt_db_internal so we can free it */
	struct rt_db_internal internal;
	RT_DB_INTERNAL_INIT(&internal);
	internal.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	internal.idb_minor_type = ID_BOT;
	internal.idb_meth = &OBJ[ID_BOT];
	internal.idb_ptr = bots[i];

	rt_db_free_internal(&internal);
    }

    bu_free(bots, "bots");

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
