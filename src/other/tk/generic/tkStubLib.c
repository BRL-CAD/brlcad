/*
 * tkStubLib.c --
 *
 *	Stub object that will be statically linked into extensions that want
 *	to access Tk.
 *
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 * Copyright (c) 1998 Paul Duffin.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkInt.h"

#ifdef __WIN32__
#include "tkWinInt.h"
#endif

#ifdef MAC_OSX_TK
#include "tkMacOSXInt.h"
#endif

#if !(defined(__WIN32__) || defined(MAC_OSX_TK))
#include "tkUnixInt.h"
#endif

/* TODO: These ought to come in some other way */
#include "tkPlatDecls.h"
#include "tkIntXlibDecls.h"

TkStubs *tkStubsPtr = NULL;
TkPlatStubs *tkPlatStubsPtr = NULL;
TkIntStubs *tkIntStubsPtr = NULL;
TkIntPlatStubs *tkIntPlatStubsPtr = NULL;
TkIntXlibStubs *tkIntXlibStubsPtr = NULL;

/*
 * Use our own isdigit to avoid linking to libc on windows
 */

static int
isDigit(const int c)
{
    return (c >= '0' && c <= '9');
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_InitStubs --
 *
 *	Checks that the correct version of Tk is loaded and that it supports
 *	stubs. It then initialises the stub table pointers.
 *
 * Results:
 *	The actual version of Tk that satisfies the request, or NULL to
 *	indicate that an error occurred.
 *
 * Side effects:
 *	Sets the stub table pointers.
 *
 *----------------------------------------------------------------------
 */
#undef Tk_InitStubs
CONST char *
Tk_InitStubs(
    Tcl_Interp *interp,
    CONST char *version,
    int exact)
{
    const char *packageName = "Tk";
    const char *errMsg = NULL;
    ClientData clientData = NULL;
    CONST char *actualVersion = tclStubsPtr->tcl_PkgRequireEx(interp,
	    packageName, version, 0, &clientData);
    TkStubs *stubsPtr = (TkStubs *)clientData;

    if (actualVersion == NULL) {
	return NULL;
    }

    if (exact) {
	CONST char *p = version;
	int count = 0;

	while (*p) {
	    count += !isDigit(*p++);
	}
	if (count == 1) {
	    CONST char *q = actualVersion;

	    p = version;
	    while (*p && (*p == *q)) {
		p++; q++;
	    }
	    if (*p || isDigit(*q)) {
		/* Construct error message */
		tclStubsPtr->tcl_PkgRequireEx(interp, "Tk", version, 1, NULL);
		return NULL;
	    }
	} else {
	    actualVersion = tclStubsPtr->tcl_PkgRequireEx(interp, "Tk",
		    version, 1, NULL);
	    if (actualVersion == NULL) {
		return NULL;
	    }
	}
    }
    if (stubsPtr == NULL) {
	errMsg = "missing stub table pointer";
    } else {
	tkStubsPtr = stubsPtr;
	if (stubsPtr->hooks) {
	    tkPlatStubsPtr = stubsPtr->hooks->tkPlatStubs;
	    tkIntStubsPtr = stubsPtr->hooks->tkIntStubs;
	    tkIntPlatStubsPtr = stubsPtr->hooks->tkIntPlatStubs;
	    tkIntXlibStubsPtr = stubsPtr->hooks->tkIntXlibStubs;
	} else {
	    tkPlatStubsPtr = NULL;
	    tkIntStubsPtr = NULL;
	    tkIntPlatStubsPtr = NULL;
	    tkIntXlibStubsPtr = NULL;
	}
	return actualVersion;
    }
    tclStubsPtr->tcl_ResetResult(interp);
    tclStubsPtr->tcl_AppendResult(interp, "Error loading ", packageName,
	    " (requested version ", version, ", actual version ",
	    actualVersion, "): ", errMsg, NULL);
    return NULL;
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
