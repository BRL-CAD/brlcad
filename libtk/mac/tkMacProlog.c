/* 
 * tkMacProlog.c --
 *
 *	Implements a method on the Macintosh to get the prolog
 *	from the resource fork of our application (or the shared
 *	library).
 *
 * Copyright (c) 1996-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacProlog.c 1.6 97/05/21 10:01:07
 */

#include "tkInt.h"
#include "tclMacInt.h"
#include <Resources.h>

/*
 *--------------------------------------------------------------
 *
 * TkGetNativeProlog --
 *
 *	Locate and load the postscript prolog from the resource
 *	fork of the application.  If it can't be found then we
 *	will try looking for the file in the system folder.
 *
 * Results:
 *	A standard Tcl Result.  If everything is OK the prolog
 *	will be located in the result string of the interpreter.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
TkGetNativeProlog(
    Tcl_Interp *interp)		/* Places the prolog in the result. */
{
    Handle resource;
    char *stringPtr;
    int releaseIt;
    

    resource = Tcl_MacFindResource(interp, 'TEXT', "prolog", -1,
        NULL, &releaseIt);
			    
    if (resource != NULL) {
	stringPtr = Tcl_MacConvertTextResource(resource);
	Tcl_SetResult(interp, stringPtr, TCL_DYNAMIC);
        if (releaseIt) {            
            ReleaseResource(resource);
        }
        return TCL_OK;
    } else {
	return TkGetProlog(interp);
    }
}
