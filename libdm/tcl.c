/*
 *				T C L . C
 *
 * LIBDM's tcl interface.
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
#include <math.h>
#include "tcl.h"
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "dm.h"
#include "cmd.h"

/* from libdm/query.c */
extern int dm_validXType();
extern char *dm_bestXType();

/* from libdm/dm_obj.c */
extern int Dmo_Init();

HIDDEN int dm_validXType_tcl();
HIDDEN int dm_bestXType_tcl();

HIDDEN struct bu_cmdtab cmdtab[] = {
	"dm_validXType",	dm_validXType_tcl,
	"dm_bestXType",		dm_bestXType_tcl,
	(char *)0,		(int (*)())0
};

int
Dm_Init(interp)
Tcl_Interp *interp;
{
  register struct bu_cmdtab *ctp;

  for (ctp = cmdtab; ctp->ct_name != (char *)NULL; ctp++) {
    (void)Tcl_CreateCommand(interp, ctp->ct_name, ctp->ct_func,
			    (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
  }

  /* initialize display manager object code */
  Dmo_Init(interp);

  return TCL_OK;
}

HIDDEN int
dm_validXType_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  struct bu_vls vls;

  bu_vls_init(&vls);

  if(argc != 3){
    bu_vls_printf(&vls, "helplib dm_validXType");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bu_vls_printf(&vls, "%d", dm_validXType(argv[1], argv[2]));
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_vls_free(&vls);

  return TCL_OK;
}

HIDDEN int
dm_bestXType_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  if (argc != 2) {
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "helplib dm_bestXType");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  Tcl_AppendResult(interp, dm_bestXType(argv[1]), (char *)NULL);
  return TCL_OK;
}
