/*                         K I L L A L L . C
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
/** @file libged/killall.c
 *
 * The killall command.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/opt.h"

#include "../ged_private.h"

struct killall_args {
    int print;
};

#define KILLALL_OPTIONS(args) \
    BU_OPT_FLAG(args, "n", NULL, print, \
	"Report affected objects without changing the database"),

BU_OPT_DESC_BUILDER(killall_options, struct killall_args, KILLALL_OPTIONS);
static const ged_opt_spec killall_opt_spec =
    GED_OPT("killall", "Delete objects and all references to them",
	killall_options, "options-first objects:object+");


int
ged_killall_core(struct ged *gedp, int argc, const char *argv[])
{
    struct killall_args args = {0};
    const char *command = argv[0];
    const char **refs_argv = NULL;
    int object_count;
    int ret;

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
    object_count = bu_opt_parse_build_with_policy(gedp->ged_result_str,
	argc, argv, killall_options, &args, BU_OPT_PARSE_OPTIONS_FIRST);
    if (object_count < 1) {
	ged_cmd_help_append(gedp->ged_result_str, command, command);
	return BRLCAD_ERROR;
    }


    if (args.print) {
	/* Objects that would be killed are in the first sublist */
	bu_vls_printf(gedp->ged_result_str, "{");
	for (int i = 0; i < object_count; i++)
	    bu_vls_printf(gedp->ged_result_str, "%s ", argv[i]);
	bu_vls_printf(gedp->ged_result_str, "} {");
    }

    gedp->ged_internal_call = 1;
    refs_argv = (const char **)bu_calloc((size_t)object_count + 2,
	sizeof(char *), "killall killrefs argv");
    refs_argv[0] = "killrefs";
    if (args.print)
	refs_argv[1] = "-n";
    for (int i = 0; i < object_count; i++)
	refs_argv[i + (args.print ? 2 : 1)] = argv[i];
    if ((ret = ged_exec_killrefs(gedp,
	object_count + (args.print ? 2 : 1), refs_argv)) != BRLCAD_OK) {
	gedp->ged_internal_call = 0;
	bu_free((void *)refs_argv, "killall killrefs argv");
	bu_vls_printf(gedp->ged_result_str, "KILL skipped because of earlier errors.\n");
	return ret;
    }
    gedp->ged_internal_call = 0;
    bu_free((void *)refs_argv, "killall killrefs argv");

    if (args.print) {
	/* Close the sublist of objects that reference the would-be killed objects. */
	bu_vls_printf(gedp->ged_result_str, "}");
	return BRLCAD_OK;
    }

    /* ALL references removed...now KILL the object[s] */
    /* Build a new argv that inserts the "-q" (quiet) flag so kill does
     * not emit noisy db_lookup failures for objects that only exist as
     * references.  Only the killall path is affected; standalone kill is
     * unchanged. */
    {
	int i;
	const char **kill_argv = (const char **)bu_calloc((size_t)object_count + 3,
	    sizeof(char *), "killall kill_argv");

	kill_argv[0] = "kill";
	kill_argv[1] = "-q";
	for (i = 0; i < object_count; i++)
	    kill_argv[i + 2] = argv[i];
	kill_argv[object_count + 2] = NULL;

	ret = ged_exec_kill(gedp, object_count + 2, kill_argv);

	bu_free((void *)kill_argv, "killall kill_argv");
    }

    return ret;
}

#include "../include/plugin.h"

#define GED_KILLALL_COMMANDS(X, XID) \
    X(killall, ged_killall_core, GED_CMD_DEFAULT, &killall_opt_spec) \

GED_DECLARE_COMMAND_SET_WITH_OPT_SPEC(GED_KILLALL_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_OPT_SPEC("libged_killall", 1, GED_KILLALL_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
