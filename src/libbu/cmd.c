/*                           C M D . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2011 United States Government as represented by
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
#include "bio.h"

#include "tcl.h"
#include "bu.h"
#include "cmd.h"


int
bu_cmd(void *data, int argc, const char **argv, struct bu_cmdtab *cmds, int cmd_index /*, const int *retval */)
{
    struct bu_cmdtab *ctp = NULL;

    /* sanity */
    if (UNLIKELY(cmd_index >= argc)) {
	bu_log("missing command; must be one of:");
	goto missing_cmd;
    }

    for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	if (ctp->ct_name[0] == argv[cmd_index][0] &&
	    BU_STR_EQUAL(ctp->ct_name, argv[cmd_index])) {
	    /* retval = (*ctp->ct_func(data, argc, argv);
	     * return BRLCAD_OK;
	     */
	    return (*ctp->ct_func)(data, argc, argv);
	}
    }

    bu_log("unknown command: %s; must be one of: ", argv[cmd_index]);

missing_cmd:
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
