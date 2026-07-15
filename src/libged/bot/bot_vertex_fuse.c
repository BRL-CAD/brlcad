/*                         B O T _ V E R T E X _ F U S E . C
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
/** @file libged/bot_vertex_fuse.c
 *
 * The bot_vertex_fuse command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmdschema.h"
#include "rt/geom.h"

#include "../ged_private.h"

struct bot_vertex_fuse_args {
    int print_help;
};

static int
bot_vertex_fuse_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    struct bu_cmd_schema flat = *schema;

    flat.validation.custom_validate = NULL;
    if (bu_cmd_schema_option_present(schema, argc, argv, "help"))
	flat.operands = NULL;
    return bu_cmd_schema_validate(&flat, argc, argv, cursor_arg, result);
}

static const struct bu_cmd_option bot_vertex_fuse_options[] = {
    BU_CMD_FLAG("h", "help", struct bot_vertex_fuse_args, print_help,
	"Print command help"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand bot_vertex_fuse_operands[] = {
    BU_CMD_OPERAND("new_bot", BU_CMD_VALUE_STRING, 1, 1,
	"Name for the fused BoT", NULL),
    BU_CMD_OPERAND("old_bot", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Source BoT object", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
const struct bu_cmd_schema ged_bot_vertex_fuse_schema = {
    "bot_vertex_fuse", "Fuse duplicate BoT vertices", bot_vertex_fuse_options,
    bot_vertex_fuse_operands, BU_CMD_PARSE_INTERSPERSED,
    BU_CMD_SCHEMA_CONSTRAINTS(bot_vertex_fuse_schema_validate, NULL)
};

static void
bot_vertex_fuse_usage(struct bu_vls *result, const char *cmd)
{
    char *option_help = bu_cmd_schema_describe(&ged_bot_vertex_fuse_schema);

    bu_vls_sprintf(result, "Usage: %s [--help] new_bot old_bot\n", cmd);
    if (option_help) {
	bu_vls_printf(result, "\nOptions:\n%s", option_help);
	bu_free(option_help, "help str");
    }
}


int
ged_bot_vertex_fuse_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *old_dp, *new_dp;
    struct rt_db_internal intern;
    struct rt_bot_internal *bot;
    int count=0;
    struct rt_wdb *wdbp;
    struct bot_vertex_fuse_args args = {0};
    const char *cmd;
    int operand_index;

    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    cmd = argv[0];
    argc--; argv++;
    if (!argc) {
	bot_vertex_fuse_usage(gedp->ged_result_str, cmd);
	return GED_HELP;
    }

    operand_index = bu_cmd_schema_parse_complete(&ged_bot_vertex_fuse_schema,
	&args, gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	bot_vertex_fuse_usage(gedp->ged_result_str, cmd);
	return BRLCAD_ERROR;
    }
    if (args.print_help) {
	bot_vertex_fuse_usage(gedp->ged_result_str, cmd);
	return GED_HELP;
    }
    argc -= operand_index;
    argv += operand_index;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

    GED_DB_LOOKUP(gedp, old_dp, argv[1], LOOKUP_NOISY, BRLCAD_ERROR & GED_QUIET);
    GED_DB_GET_INTERN(gedp, &intern, old_dp, bn_mat_identity, BRLCAD_ERROR);

    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD || intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT solid!\n", cmd, argv[1]);
	return BRLCAD_ERROR;
    }

    bot = (struct rt_bot_internal *)intern.idb_ptr;
    RT_BOT_CK_MAGIC(bot);

    count = rt_bot_vertex_fuse(bot, &wdbp->wdb_tol);
    bu_vls_printf(gedp->ged_result_str, "Fused %d vertices\n", count);

    GED_DB_DIRADD(gedp, new_dp, argv[0], RT_DIR_PHONY_ADDR, 0, RT_DIR_SOLID, (void *)&intern.idb_type, BRLCAD_ERROR);
    GED_DB_PUT_INTERN(gedp, new_dp, &intern, BRLCAD_ERROR);

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
