/*
 * Author :
 *        Bob Parker (SURVICE Engineering Company)
 *
 * Description:
 *        This file contains the Tcl initialization routine for libsysv.
 */

#include "common.h"

#include "tcl.h"
#include <stdio.h>


#ifndef SYSV_EXPORT
#  if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#    ifdef SYSV_EXPORT_DLL
#      define SYSV_EXPORT __declspec(dllexport)
#    else
#      define SYSV_EXPORT __declspec(dllimport)
#    endif
#  else
#    define SYSV_EXPORT
#  endif
#endif


SYSV_EXPORT int
Sysv_Init(Tcl_Interp *interp)
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
