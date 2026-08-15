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

#include "bu/opt.h"

#include "../ged_private.h"


struct kill_args {
    int force;
    int no_delete;
    int quiet;
};

#define KILL_OPTIONS(args) \
    BU_OPT_FLAG(args, "f", NULL, force, \
	"Permit deletion of protected global data"), \
    BU_OPT_FLAG(args, "n", NULL, no_delete, \
	"Report objects without deleting them"), \
    BU_OPT_FLAG(args, "q", NULL, quiet, \
	"Suppress lookup messages"),

BU_OPT_DESC_BUILDER(kill_options, struct kill_args, KILL_OPTIONS);

static const ged_opt_rule kill_opt_rules[] = {
    GED_RULE_OPTIONS("f n", 0, 1, "-f and -n are mutually exclusive"),
    GED_RULE_DB_OBJECTS("objects", GED_OPT_DB_ANY, "f", GED_OPT_DB_ALL),
    GED_RULE_NULL
};
static const ged_opt_spec kill_opt_spec =
    GED_OPT_WITH("kill", "Delete database objects", kill_options,
	"interspersed objects:object@ged.db_object_any+", kill_opt_rules);


int
ged_kill_core(struct ged *gedp, int argc, const char *argv[])
{
    struct directory *dp;
    int i;
    int is_phony;
    int verbose = LOOKUP_NOISY;
    struct kill_args args = {0, 0, 0};
    int object_count = 0;
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

	object_count = bu_opt_parse_build(gedp->ged_result_str, argc - 1,
	argv + 1, kill_options, &args);
    if (object_count < 0 || (args.force && args.no_delete)) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return BRLCAD_ERROR;
    }
    if (!object_count) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return GED_HELP;
    }

    objects = argv + 1;
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
    X(kill, ged_kill_core, GED_CMD_DEFAULT, &kill_opt_spec) \

GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC(GED_KILL_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_OPT_SPEC("libged_kill", 1, GED_KILL_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
