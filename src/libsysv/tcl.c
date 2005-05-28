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
#include "machine.h"


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
#ifdef BRLCAD_DEBUG
Sysv_d_Init(Tcl_Interp *interp)
#else
Sysv_Init(Tcl_Interp *interp)
#endif
{
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
