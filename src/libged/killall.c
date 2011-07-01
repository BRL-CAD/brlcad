/*                         K I L L A L L . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
#include "bio.h"

#include "cmd.h"

#include "./ged_private.h"


int
ged_killall(struct ged *gedp, int argc, const char *argv[])
{
    int nflag;
    int ret;
    static const char *usage = "[-n] object(s)";

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_READ_ONLY(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

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
    if ((ret = ged_killrefs(gedp, argc, argv)) != GED_OK) {
	gedp->ged_internal_call = 0;
	bu_vls_printf(gedp->ged_result_str, "KILL skipped because of earlier errors.\n");
	return ret;
    }
    gedp->ged_internal_call = 0;

    if (nflag) {
	/* Close the sublist of objects that reference the would-be killed objects. */
	bu_vls_printf(gedp->ged_result_str, "}");
	return GED_OK;
    }

    /* ALL references removed...now KILL the object[s] */
    /* reuse argv[] */
    argv[0] = "kill";
    return ged_kill(gedp, argc, argv);
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
