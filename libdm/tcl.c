#include "conf.h"
#include "tk.h"
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "dm.h"

extern int dm_validXType();
extern char *dm_bestXType();

static int dm_validXType_tcl();
static int dm_bestXType_tcl();

struct cmdtab {
  char *ct_name;
  int (*ct_func)();
};

static struct cmdtab cmdtab[] = {
  "dm_validXType", dm_validXType_tcl,
  "dm_bestXType", dm_bestXType_tcl,
  0, 0
};

int
dm_tclInit(interp)
Tcl_Interp *interp;
{
  register struct cmdtab *ctp;

  for(ctp = cmdtab; ctp->ct_name != NULL; ctp++){
    (void)Tcl_CreateCommand(interp, ctp->ct_name, ctp->ct_func,
			   (ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
  }

  return TCL_OK;
}

static int
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

static int
dm_bestXType_tcl(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int     argc;
char    **argv;
{
  if(argc != 2){
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
