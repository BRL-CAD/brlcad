/*                         R T A B O R T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
#include "../ged_private.h"


/*
 * Abort the current raytrace processes (rt, rtwizard, rtcheck).
 *
 * Usage:
 * rtabort
 *
 */
int
ged_rtabort_core(struct ged *gedp, int argc, const char *argv[])
{
    struct ged_subprocess *rrp;
    struct bu_vls cmdroot = BU_VLS_INIT_ZERO;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s", argv[0]);
	return BRLCAD_ERROR;
    }

    for (size_t i = 0; i < BU_PTBL_LEN(&gedp->ged_subp); i++) {
	rrp = (struct ged_subprocess *)BU_PTBL_GET(&gedp->ged_subp, i);
	const char *cmd;
	int argcnt = bu_process_args(&cmd, NULL, rrp->p);
	bu_vls_trunc(&cmdroot, 0);
	if (argcnt > 0 && bu_path_component(&cmdroot, cmd, BU_PATH_BASENAME_EXTLESS)) {
	    if (BU_STR_EQUAL(bu_vls_cstr(&cmdroot), "rt") ||
		    BU_STR_EQUAL(bu_vls_cstr(&cmdroot), "rtwizard") || BU_STR_EQUAL(bu_vls_cstr(&cmdroot), "art") ||
		    BU_STR_EQUAL(bu_vls_cstr(&cmdroot), "rtcheck"))  {
		bu_terminate(bu_process_pid(rrp->p));
		rrp->aborted = 1;
	    }
	}
    }

    bu_vls_free(&cmdroot);
    return BRLCAD_OK;
}


#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl rtabort_cmd_impl = {
    "rtabort",
    ged_rtabort_core,
    GED_CMD_DEFAULT
};

const struct ged_cmd rtabort_cmd = { &rtabort_cmd_impl };
const struct ged_cmd *rtabort_cmds[] = { &rtabort_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  rtabort_cmds, 1 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info()
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
