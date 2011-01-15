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
bu_cmd(ClientData clientData, Tcl_Interp *interp, int argc, const char **argv, struct bu_cmdtab *cmds, int cmd_index)
{
    struct bu_cmdtab *ctp = NULL;

    /* sanity */
    if (UNLIKELY(cmd_index >= argc)) {
	Tcl_AppendResult(interp, "missing command; must be one of:", (char *)NULL);
	goto missing_cmd;
    }

    for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	if (ctp->ct_name[0] == argv[cmd_index][0] &&
	    BU_STR_EQUAL(ctp->ct_name, argv[cmd_index])) {
	    return (*ctp->ct_func)(clientData, interp, argc, argv);
	}
    }

    Tcl_AppendResult(interp, "unknown command: ", argv[cmd_index], ";", " must be one of: ", (char *)NULL);

missing_cmd:
    for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	Tcl_AppendResult(interp, " ", ctp->ct_name, (char *)NULL);
    }
    Tcl_AppendResult(interp, "\n", (char *)NULL);

    return TCL_ERROR;
}


void
bu_register_cmds(Tcl_Interp *interp, struct bu_cmdtab *cmds)
{
    struct bu_cmdtab *ctp = NULL;

    for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
	Tcl_CmdProc *func = (Tcl_CmdProc *)ctp->ct_func;
	(void)Tcl_CreateCommand(interp, ctp->ct_name, func, (ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
    }
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
