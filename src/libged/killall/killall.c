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
    int ret;
    int internal_call;
    const char **args;
    static const char *usage = "[-n] [--] object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc == 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_HELP;
    }

    /* Process the -n option */
    const int nflag = BU_STR_EQUAL(argv[1], "-n");
    int first = 1 + nflag;
    if (first < argc && BU_STR_EQUAL(argv[first], "--"))
	++first;
    if (nflag) {
	/* Objects that would be killed are in the first sublist */
	bu_vls_printf(gedp->ged_result_str, "{");
	for (int i = first; i < argc; i++)
	    bu_vls_printf(gedp->ged_result_str, "%s ", argv[i]);
	bu_vls_printf(gedp->ged_result_str, "} {");
    } else {
	GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    }

    args = (const char **)bu_calloc(argc + 3, sizeof(*args), "killall arguments");
    args[0] = "killrefs";
    memcpy(args + 1, argv + 1, (argc - 1) * sizeof(*args));
    internal_call = gedp->ged_internal_call;
    gedp->ged_internal_call = 1;
    ret = ged_exec_killrefs(gedp, argc, args);
    gedp->ged_internal_call = internal_call;
    if (ret != BRLCAD_OK) {
	bu_vls_printf(gedp->ged_result_str, "KILL skipped because of earlier errors.\n");
    } else if (nflag) {
	/* Close the sublist of objects that reference the would-be killed objects. */
	bu_vls_printf(gedp->ged_result_str, "}");
    } else {
	/* Some names may exist only as dangling references. */
	args[0] = "kill";
	args[1] = "-q";
	args[2] = "--";
	memcpy(args + 3, argv + first, (argc - first) * sizeof(*args));
	ret = ged_exec_kill(gedp, argc - first + 3, args);
    }

    bu_free(args, "killall arguments");
    return ret;
}

#include "../include/plugin.h"

#define GED_KILLALL_COMMANDS(X, XID) \
    X(killall, ged_killall_core, GED_CMD_DEFAULT) \

GED_DECLARE_COMMAND_SET(GED_KILLALL_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_killall", 1, GED_KILLALL_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
