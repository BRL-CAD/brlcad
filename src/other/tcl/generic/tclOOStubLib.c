/*
 * $Id$
 * ORIGINAL SOURCE: tk/generic/tkStubLib.c, version 1.9 2004/03/17
 */

/*
 * We need to ensure that we use the tcl stub macros so that this file
 * contains no references to any of the tcl stub functions.
 */

#undef USE_TCL_STUBS
#define USE_TCL_STUBS

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "tcl.h"

#define USE_TCLOO_STUBS 1
#include "tclOO.h"
#include "tclOOInt.h"

MODULE_SCOPE const TclOOStubs *tclOOStubsPtr;
MODULE_SCOPE const TclOOIntStubs *tclOOIntStubsPtr;

const TclOOStubs *tclOOStubsPtr = NULL;
const TclOOIntStubs *tclOOIntStubsPtr = NULL;

/*
 *----------------------------------------------------------------------
 *
 * TclOOInitializeStubs --
 *	Load the tclOO package, initialize stub table pointer. Do not call
 *	this function directly, use Tcl_OOInitStubs() macro instead.
 *
 * Results:
 *	The actual version of the package that satisfies the request, or NULL
 *	to indicate that an error occurred.
 *
 * Side effects:
 *	Sets the stub table pointer.
 *
 *----------------------------------------------------------------------
 */

MODULE_SCOPE const char *
TclOOInitializeStubs(
    Tcl_Interp *interp, const char *version)
{
    int exact = 0;
    const char *packageName = "TclOO";
    const char *errMsg = NULL;
    ClientData clientData = NULL;
    const char *actualVersion =
	    Tcl_PkgRequireEx(interp, packageName,version, exact, &clientData);

    if (clientData == NULL) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Error loading ", packageName, " package; ",
		"package not present or incomplete", NULL);
	return NULL;
    } else {
	const TclOOStubs * const stubsPtr = clientData;
	const TclOOIntStubs * const intStubsPtr = stubsPtr->hooks ?
		stubsPtr->hooks->tclOOIntStubs : NULL;

	if (!actualVersion) {
	    return NULL;
	}

	if (!stubsPtr || !intStubsPtr) {
	    errMsg = "missing stub table pointer";
	    goto error;
	}

	tclOOStubsPtr = stubsPtr;
	tclOOIntStubsPtr = intStubsPtr;
	return actualVersion;

    error:
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "Error loading ", packageName, " package",
		" (requested version '", version, "', loaded version '",
		actualVersion, "'): ", errMsg, NULL);
	return NULL;
    }
}
