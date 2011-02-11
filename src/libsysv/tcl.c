/*
 * Author :
 *        Bob Parker (SURVICE Engineering Company)
 *
 * Description:
 *        This file contains the Tcl initialization routine for libsysv.
 */

#include "common.h"

#include "tcl.h"
#include "sysv.h"
#include <stdio.h>

SYSV_EXPORT int
Sysv_Init(Tcl_Interp *UNUSED(interp))
{
    return TCL_OK;
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
