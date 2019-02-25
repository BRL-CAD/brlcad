/*                         R T A B O R T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2019 United States Government as represented by
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
/** @file libged/rtabort.c
 *
 * The rtabort command.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bu/path.h"
#include "bu/process.h"
#include "./ged_private.h"


/*
 * Abort the current raytrace processes (rt, rtwizard, rtcheck).
 *
 * Usage:
 * rtabort
 *
 */
int
ged_rtabort(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_subprocess *rrp;
    struct bu_vls cmdroot = BU_VLS_INIT_ZERO;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return GED_ERROR;
    }

    for (BU_LIST_FOR(rrp, ged_subprocess, &gedp->gd_headSubprocess.l)) {
	const char *cmd;
	int argcnt = bu_process_args(&cmd, NULL, rrp->p);
	bu_vls_trunc(&cmdroot, 0);
	if (argcnt > 0 && bu_path_component(&cmdroot, cmd, BU_PATH_BASENAME_EXTLESS)) {
	    if (BU_STR_EQUAL(bu_vls_cstr(&cmdroot), "rt") ||
		    BU_STR_EQUAL(bu_vls_cstr(&cmdroot), "rtwizard") ||
		    BU_STR_EQUAL(bu_vls_cstr(&cmdroot), "rtcheck")) {
		bu_terminate(bu_process_pid(rrp->p));
		rrp->aborted = 1;
	    }
	}
    }

    bu_vls_free(&cmdroot);
    return GED_OK;
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
