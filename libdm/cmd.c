/*
 *				C M D . C
 *
 * Utility routines for handling commands.
 * 
 * 
 * Source -
 *	SLAD CAD Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 * Author -
 *	Robert G. Parker
 */
#include "conf.h"
#include "tcl.h"
#include "machine.h"
#include "externs.h"
#include "cmd.h"

/*
 * Generic command parser.
 */
int
do_cmd(clientData, interp, argc, argv, cmds, cmd_index)
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char **argv;
     struct cmdtab *cmds;
     int cmd_index;
{
  register struct cmdtab *ctp;

  /* sanity */
  if (cmd_index >= argc) {
    Tcl_AppendResult(interp,
		     "missing command; must be one of:",
		     (char *)NULL);
    goto missing_cmd;
  }

  for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
    if (strcmp(ctp->ct_name, argv[cmd_index]) == 0) {
      return (*ctp->ct_func)(clientData, interp, argc, argv);
    }
  }

  Tcl_AppendResult(interp,
		   "unknown command; must be one of:",
		   (char *)NULL);
missing_cmd:
  for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++) {
    Tcl_AppendResult(interp, " ", ctp->ct_name, (char *)NULL);
  }
  Tcl_AppendResult(interp, "\n", (char *)NULL);

  return TCL_ERROR;
}

/*
 * Generic routine to register commands with a Tcl interpreter.
 */
void
register_cmds(interp, cmds)
     Tcl_Interp *interp;
     struct cmdtab *cmds;
{
  register struct cmdtab *ctp;

  for (ctp = cmds; ctp->ct_name != (char *)NULL; ctp++)
    (void)Tcl_CreateCommand(interp, ctp->ct_name, ctp->ct_func,
			    (ClientData)ctp, (Tcl_CmdDeleteProc *)NULL);
}
