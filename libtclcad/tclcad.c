/*
 *            T C L C A D . C
 *
 * Initialization routines for the BRL-CAD/Tcl links.
 *
 * Author --
 *    Glenn Durfee
 *
 * Army copyright foo
 */

#include "conf.h"

#include <stdio.h>
#include <stdlib.h>

#include "tcl.h"
#include "tk.h"
#include "tclcad.h"

extern Tk_PhotoImageFormat tkImgFmtPIX;

int
cmd_pix_common_file_size(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
    int width, height;
    
    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			 " fileName\"", NULL);
	return TCL_ERROR;
    }

    if (bn_common_file_size(&width, &height, argv[1], 3) > 0) {
	sprintf(interp->result, "%d %d", width, height);
	return TCL_OK;
    }

#if 0
    Tcl_AppendResult(interp, "cannot determine dimensions of ", argv[1], NULL);
    return TCL_ERROR;
#else
    Tcl_SetResult(interp, "0 0", TCL_STATIC);
    return TCL_OK;
#endif
}

int
TclCad_Init(interp)
Tcl_Interp *interp;
{
    Tk_CreatePhotoImageFormat(&tkImgFmtPIX);

    (void)Tcl_CreateCommand(interp, "pix_common_file_size",
			    cmd_pix_common_file_size, (ClientData)NULL,
			    (Tcl_CmdDeleteProc *)NULL);

    return TCL_OK;
}
    
    
    
    
