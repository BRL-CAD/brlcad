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

#include "common.h"



#include <stdio.h>
#include <stdlib.h>

#include "tcl.h"
#include "tk.h"

extern Tk_PhotoImageFormat tkImgFmtPIX;

int
tclcad_tk_setup(Tcl_Interp *interp)
{
    /* Note:  pix_common_file_size  is now bn_common_file_size */

    Tk_CreatePhotoImageFormat(&tkImgFmtPIX);

    return TCL_OK;
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
