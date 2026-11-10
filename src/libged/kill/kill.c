/*                         K I L L . C
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
/** @file libged/kill.c
 *
 * The kill command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/arg.h"

#include "../ged_private.h"


struct kill_args {
    int force;
    int no_delete;
    int quiet;
};

static const struct bu_arg_option kill_options[] = {
    BU_ARG_FLAG("f", NULL, struct kill_args, force,
	"Permit deletion of protected global data"),
    BU_ARG_FLAG("n", NULL, struct kill_args, no_delete,
	"Report objects without deleting them"),
    BU_ARG_FLAG("q", NULL, struct kill_args, quiet,
	"Suppress lookup messages"),
    BU_ARG_OPTION_NULL
};
static const struct bu_arg_operand kill_operands[] = {
    BU_ARG_OPERAND("objects", BU_ARG_VALUE_DB_OBJECT, 1,
	BU_ARG_COUNT_UNLIMITED, "Database objects to delete",
	"ged.db_object_any"),
    BU_ARG_OPERAND_NULL
};
static const struct bu_arg_operand kill_force_operands[] = {
    BU_ARG_OPERAND("objects", BU_ARG_VALUE_DB_OBJECT, 1,
	BU_ARG_COUNT_UNLIMITED, "Database objects to delete",
	"ged.db_object_all"),
    BU_ARG_OPERAND_NULL
};
static const char * const kill_force_option[] = {"f", NULL};
static const char * const kill_modes[] = {"f", "n", NULL};
static const struct bu_arg_schema_case kill_cases[] = {
    BU_ARG_SCHEMA_CASE("force", "Permit protected global data",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, kill_force_option,
	kill_force_operands, NULL),
    BU_ARG_SCHEMA_CASE_DEFAULT("objects", "Delete ordinary database objects",
	kill_operands, NULL),
    BU_ARG_SCHEMA_CASE_NULL
};
static const struct bu_arg_constraint kill_constraints[] = {
    BU_ARG_CONSTRAINT(BU_ARG_CONSTRAINT_OPTION_COUNT, BU_ARG_CONDITION_ALWAYS,
	kill_modes, 0, 1, "-f and -n are mutually exclusive"),
    BU_ARG_CONSTRAINT_NULL
};
static const struct bu_arg_schema kill_cmd_schema =
    BU_ARG_SCHEMA("kill", "Delete database objects", kill_options, NULL,
	BU_ARG_PARSE_INTERSPERSED,
	BU_ARG_SCHEMA_META_CASES(NULL, kill_constraints, NULL, NULL, kill_cases));


int
ged_kill_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int i;
    int is_phony;
    int verbose = LOOKUP_NOISY;
    struct kill_args args = {0, 0, 0};
    int object_count;
    int operand_index;
    const char **objects = NULL;
    const char *command = argv[0];

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return GED_HELP;
    }

    argc--; argv++;
    operand_index = bu_arg_schema_parse_complete(&kill_cmd_schema, &args,
	gedp->ged_result_str, argc, argv);
    if (operand_index < 0) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return BRLCAD_ERROR;
    }
    object_count = argc - operand_index;
    objects = argv + operand_index;
    if (args.quiet) {
	verbose = LOOKUP_QUIET;
    }

	if (args.no_delete) {
	bu_vls_printf(gedp->ged_result_str, "{");
	for (i = 0; i < object_count; i++)
	    bu_vls_printf(gedp->ged_result_str, "%s ", objects[i]);
	bu_vls_printf(gedp->ged_result_str, "} {}");

	return BRLCAD_OK;
    }

	for (i = 0; i < object_count; i++) {
	if ((dp = db_lookup(gedp->dbip, objects[i], verbose)) != RT_DIR_NULL) {
	    if (!args.force && dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY && dp->d_minor_type == 0) {
		bu_vls_printf(gedp->ged_result_str, "You attempted to delete the _GLOBAL object.\n");
		bu_vls_printf(gedp->ged_result_str, "\tIf you delete the \"_GLOBAL\" object you will be losing some important information\n");
		bu_vls_printf(gedp->ged_result_str, "\tsuch as your preferred units and the title of the database.\n");
		bu_vls_printf(gedp->ged_result_str, "\tUse the \"-f\" option, if you really want to do this.\n");
		continue;
	    }

	    is_phony = (dp->d_addr == RT_DIR_PHONY_ADDR);

	    /* don't worry about phony objects */
	    if (is_phony)
		continue;

	    _dl_eraseAllNamesFromDisplay(gedp, objects[i], 0);

	    if (db_delete(gedp->dbip, dp) != 0 || db_dirdelete(gedp->dbip, dp) != 0) {
		/* Abort kill processing on first error */
		bu_vls_printf(gedp->ged_result_str, "an error occurred while deleting %s", objects[i]);
		return BRLCAD_ERROR;
	    }
	}
    }

    /* Update references. */
    db_update_nref(gedp->dbip);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_KILL_COMMANDS(X, XID) \
    X(kill, ged_kill_core, GED_CMD_DEFAULT, &kill_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_KILL_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_kill", 1, GED_KILL_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
