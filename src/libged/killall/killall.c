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

#include "bu/cmd.h"

#include "../ged_private.h"


int
ged_killall_core(struct ged *gedp, int argc, const char *argv[])
{
    int nflag;
    int ret;
    static const char *usage = "[-n] object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* Process the -n option */
    if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'n' && argv[1][2] == '\0') {
	int i;
	nflag = 1;

	/* Objects that would be killed are in the first sublist */
	bu_vls_printf(gedp->ged_result_str, "{");
	for (i = 2; i < argc; i++)
	    bu_vls_printf(gedp->ged_result_str, "%s ", argv[i]);
	bu_vls_printf(gedp->ged_result_str, "} {");
    } else
	nflag = 0;

    gedp->ged_internal_call = 1;
    argv[0] = "killrefs";
    if ((ret = ged_exec_killrefs(gedp, argc, argv)) != BRLCAD_OK) {
	gedp->ged_internal_call = 0;
	bu_vls_printf(gedp->ged_result_str, "KILL skipped because of earlier errors.\n");
	return ret;
    }
    gedp->ged_internal_call = 0;

    if (nflag) {
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
	const char **kill_argv = (const char **)bu_calloc(argc + 2, sizeof(char *), "killall kill_argv");

	kill_argv[0] = "kill";
	kill_argv[1] = "-q";
	for (i = 1; i < argc; i++)
	    kill_argv[i + 1] = argv[i];
	kill_argv[argc + 1] = NULL;

	ret = ged_exec_kill(gedp, argc + 1, kill_argv);

	bu_free((void *)kill_argv, "killall kill_argv");
    }

    return ret;
}

#include "../include/plugin.h"

static const struct bu_cmd_option killall_schema_options[] = {
    BU_CMD_FLAG_UNBOUND("n", NULL, "n", "Report affected objects without changing the database"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand killall_schema_operands[] = {
    BU_CMD_OPERAND("objects", BU_CMD_VALUE_STRING, 1, BU_CMD_COUNT_UNLIMITED,
	"Objects to delete with all references", "ged.db_object"),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema killall_cmd_schema = {
    "killall", "Delete objects and all references to them", killall_schema_options,
    killall_schema_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    {NULL}
};

#define GED_KILLALL_COMMANDS(X, XID) \
    X(killall, ged_killall_core, GED_CMD_DEFAULT, &killall_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_KILLALL_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_killall", 1, GED_KILLALL_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
