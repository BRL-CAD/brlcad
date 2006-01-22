/*                           C M D . C
 * BRL-CAD
 *
 * Copyright (c) 1998-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** \defgroup cmd command
 * \ingroup libbu */
/*@{*/
/** @file ./librt/cmd.c
 *	Utility routines for handling commands.
 *
 * Author -
 *	Robert G. Parker
 *
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */
/*@}*/
#include "common.h"

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "tcl.h"
#include "machine.h"
#include "cmd.h"			/* includes bu.h */

/****** libbu/cmd.c
 *
 * NAME
 *	bu_cmd
 *
 * SYNOPSIS
 *	This function is intended to be used for parsing subcommands.
 *	If the command is found in the array of commands, the associated
 *	function is called. Otherwise, an error message is created and
 *	added to interp.
 *
 * PARAMETERS
 *	clientData	- data/state associated with the command
 *	interp		- tcl interpreter wherein this command is registered
 *			  (Note - the result of the command is also stored here)
 *	argc		- number of arguments in argv
 *	argv		- command to execute and its arguments
 *	cmds		- commands and related function pointers
 *	cmd_index	- indicates which argv element holds the subcommand
 *
 * RETURN
 *	Returns TCL_OK if successful, otherwise, TCL_ERROR.
 */
int
bu_cmd(ClientData	clientData,
       Tcl_Interp	*interp,
       int		argc,
       char		**argv,
       struct bu_cmdtab	*cmds,
       int		cmd_index)
{
	register struct bu_cmdtab *ctp;

	/* sanity */
	if (cmd_index >= argc) {
		Tcl_AppendResult(interp,
				 "missing command; must be one of:",
				 (char *)NULL);
		goto missing_cmd;
	}

	for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
		if (ctp->ct_name[0] == argv[cmd_index][0] &&
		    strcmp(ctp->ct_name, argv[cmd_index]) == 0) {
			return (*ctp->ct_func)(clientData, interp, argc, argv);
		}
	}

	Tcl_AppendResult(interp,
			 "unknown command: ",
			 argv[cmd_index], ";",
			 " must be one of: ",
			 (char *)NULL);

missing_cmd:
	for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
		Tcl_AppendResult(interp, " ", ctp->ct_name, (char *)NULL);
	}
	Tcl_AppendResult(interp, "\n", (char *)NULL);

	return TCL_ERROR;
}

/*****f* libbu/cmd.c
 *
 * NAME
 *	bu_register_cmds
 *
 * SYNOPSIS
 *	This is a convenience routine for registering an array of commands
 *	with a Tcl interpreter. Note - this is not intended for use by
 *	commands with associated state (i.e. ClientData).
 *
 * PARAMETERS
 *	interp		- Tcl interpreter wherein to register the commands
 *	cmds		- commands and related function pointers
 *
 * RETURN
 *	void
 */
void
bu_register_cmds(Tcl_Interp		*interp,
		 struct bu_cmdtab	*cmds)
{
	register struct bu_cmdtab *ctp;

	for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++)
		(void)Tcl_CreateCommand(interp, ctp->ct_name, ctp->ct_func,
					(ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
