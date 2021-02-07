/*                           C M D . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2021 United States Government as represented by
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

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/str.h"

int
bu_cmd_valid(const struct bu_cmdtab *cmds, const char *cmdname)
{
    const struct bu_cmdtab *ctp = NULL;

    /* sanity */
    if (UNLIKELY(!cmds || !cmdname)) {
	return BRLCAD_ERROR;
    }

    for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	if (BU_STR_EQUAL(ctp->ct_name, cmdname)) {
	    return BRLCAD_OK;
	}
    }

    return BRLCAD_ERROR;
}

int
bu_cmd(const struct bu_cmdtab *cmds, int argc, const char **argv, int cmd_index, void *data, int *retval)
{
    const struct bu_cmdtab *ctp = NULL;

    /* sanity */
    if (UNLIKELY(cmd_index >= argc)) {
	return BRLCAD_ERROR;
    }

    for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	if (BU_STR_EQUAL(ctp->ct_name, argv[cmd_index])) {
	    if (retval) {
		*retval = (*ctp->ct_func)(data, argc, argv);
	    } else {
		(void)(*ctp->ct_func)(data, argc, argv);
	    }
	    return BRLCAD_OK;
	}
    }

    bu_log("unknown command: %s; must be one of: ", argv[cmd_index]);

    for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	bu_log(" %s", ctp->ct_name);
    }
    bu_log("\n");

    return BRLCAD_ERROR;
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
