/*
 * $Id$
 * SOURCE: tk/generic/tkStubLib.c, version 1.9 2004/03/17
 */

#define USE_TCL_STUBS 1
#include "tcl.h"

#define USE_ITCL_STUBS 1
#include "itcl.h"
#include "itclInt.h"

#ifdef Itcl_InitStubs
#undef Itcl_InitStubs
#endif

const ItclStubs *itclStubsPtr;
const ItclIntStubs *itclIntStubsPtr;

/*
 *----------------------------------------------------------------------
 *
 * Itcl_InitStubs --
 *	Load the tclOO package, initialize stub table pointer. Do not call
 *	this function directly, use Itcl_InitStubs() macro instead.
 *
 * Results:
 *	The actual version of the package that satisfies the request, or
 *	NULL to indicate that an error occurred.
 *
 * Side effects:
 *	Sets the stub table pointer.
 *
 */

const char *
Itcl_InitStubs(
    Tcl_Interp *interp,
    const char *version,
    int exact)
{
    const char *packageName = "itcl";
    const char *errMsg = NULL;
    ClientData clientData = NULL;
    ItclStubs *stubsPtr;
    ItclIntStubs *intStubsPtr;
    const char *actualVersion;
    struct ItclStubAPI *stubsAPIPtr;
    
    actualVersion =
	    Tcl_PkgRequireEx(interp, packageName, version, exact, &clientData);
    stubsAPIPtr = clientData;
    if (clientData == NULL) {
        return NULL;
    }
    stubsPtr = stubsAPIPtr->stubsPtr;
    intStubsPtr = stubsAPIPtr->intStubsPtr;

    if (actualVersion == NULL) {
	return NULL;
    }

    if (!stubsPtr || !intStubsPtr) {
	errMsg = "missing stub table pointer";
	goto error;
    }
    itclStubsPtr = stubsPtr;
    itclIntStubsPtr = intStubsPtr;
    return actualVersion;

  error:
    Tcl_ResetResult(interp);
    Tcl_AppendResult(interp, "Error loading ", packageName, " package",
	    " (requested version '", version, "', loaded version '",
	    actualVersion, "'): ", errMsg, NULL);
    return NULL;
}
