/*                         M O V E . C
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
/** @file libged/move.c
 *
 * The move command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "bu/str.h"

#include "../ged_private.h"


static int
move_destination_name_validate(struct bu_vls *msg, const char *arg)
{
    if (strchr(arg, '/') != NULL) {
	if (msg)
	    bu_vls_printf(msg, "%s: destination name may not contain slashes", arg);
	return -1;
    }

    return 0;
}


static const struct bu_cmd_operand move_schema_operands[] = {
    BU_CMD_OPERAND("source_object", BU_CMD_VALUE_DB_OBJECT, 1, 1,
	"Existing database object", "ged.db_object"),
    BU_CMD_OPERAND_VALIDATE("output_object", BU_CMD_VALUE_STRING, 1, 1,
	move_destination_name_validate, "New name for the object", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema move_cmd_schema = {
    "move", "Rename a database object", NULL, move_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};
static const struct bu_cmd_schema mv_cmd_schema = {
    "mv", "Rename a database object", NULL, move_schema_operands,
    BU_CMD_PARSE_STOP_AT_FIRST_OPERAND, {NULL}
};


static void
move_show_help(struct ged *gedp, const char *command)
{
    bu_vls_printf(gedp->ged_result_str, "Usage: %s source_object output_object", command);
}


int
ged_move_core(struct ged *gedp, int argc, const char *argv[])
{
    struct display_list *gdlp;
    struct directory *dp;
    struct rt_db_internal intern;
    int operand_index;
    int parse_dummy = 0;
    const struct bu_cmd_schema *schema;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	move_show_help(gedp, argv[0]);
	return GED_HELP;
    }

    schema = BU_STR_EQUAL(argv[0], "mv") ? &mv_cmd_schema : &move_cmd_schema;
    operand_index = bu_cmd_schema_parse_complete(schema, &parse_dummy,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0 || argc - 1 - operand_index != 2) {
	move_show_help(gedp, argv[0]);
	return BRLCAD_ERROR;
    }
    argv += operand_index + 1;

    if ((dp = db_lookup(gedp->dbip,  argv[0], LOOKUP_NOISY)) == RT_DIR_NULL)
	return BRLCAD_ERROR;

    if (db_lookup(gedp->dbip, argv[1], LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "%s: already exists", argv[1]);
	return BRLCAD_ERROR;
    }

    if (rt_db_get_internal(&intern, dp, gedp->dbip, (fastf_t *)NULL) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database read error, aborting");
	return BRLCAD_ERROR;
    }

    /* Change object name in the in-memory directory. */
    if (db_rename(gedp->dbip, dp, argv[1]) < 0) {
	rt_db_free_internal(&intern);
	bu_vls_printf(gedp->ged_result_str, "error in db_rename to %s, aborting", argv[1]);
	return BRLCAD_ERROR;
    }

    /* Re-write to the database.  New name is applied on the way out. */
    if (rt_db_put_internal(dp, gedp->dbip, &intern) < 0) {
	bu_vls_printf(gedp->ged_result_str, "Database write error, aborting");
	return BRLCAD_ERROR;
    }

    /* Change object name if it matches the first element in the display list path. */
    for (BU_LIST_FOR(gdlp, display_list, gedp->i->ged_gdp->gd_headDisplay)) {
	int first = 1;
	int found = 0;
	struct bu_vls new_path = BU_VLS_INIT_ZERO;
	char *dupstr = bu_strdup(bu_vls_addr(&gdlp->dl_path));
	char *tok = strtok(dupstr, "/");

	while (tok) {
	    if (first) {
		first = 0;

		if (BU_STR_EQUAL(tok, argv[0])) {
		    found = 1;
		    bu_vls_printf(&new_path, "%s", argv[1]);
		} else
		    break; /* no need to go further */
	    } else
		bu_vls_printf(&new_path, "/%s", tok);

	    tok = strtok((char *)NULL, "/");
	}

	if (found) {
	    bu_vls_free(&gdlp->dl_path);
	    bu_vls_printf(&gdlp->dl_path, "%s", bu_vls_addr(&new_path));
	}

	free((void *)dupstr);
	bu_vls_free(&new_path);
    }

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_MOVE_COMMANDS(X, XID) \
    X(move, ged_move_core, GED_CMD_DEFAULT, &move_cmd_schema) \
    X(mv, ged_move_core, GED_CMD_DEFAULT, &mv_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_MOVE_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_move", 1, GED_MOVE_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
