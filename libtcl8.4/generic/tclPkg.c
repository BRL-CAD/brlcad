/* 
 * tclPkg.c --
 *
 *	This file implements package and version control for Tcl via
 *	the "package" command and a few C APIs.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tclInt.h"

/*
 * Each invocation of the "package ifneeded" command creates a structure
 * of the following type, which is used to load the package into the
 * interpreter if it is requested with a "package require" command.
 */

typedef struct PkgAvail {
    char *version;		/* Version string; malloc'ed. */
    char *script;		/* Script to invoke to provide this version
				 * of the package.  Malloc'ed and protected
				 * by Tcl_Preserve and Tcl_Release. */
    struct PkgAvail *nextPtr;	/* Next in list of available versions of
				 * the same package. */
} PkgAvail;

/*
 * For each package that is known in any way to an interpreter, there
 * is one record of the following type.  These records are stored in
 * the "packageTable" hash table in the interpreter, keyed by
 * package name such as "Tk" (no version number).
 */

typedef struct Package {
    char *version;		/* Version that has been supplied in this
				 * interpreter via "package provide"
				 * (malloc'ed).  NULL means the package doesn't
				 * exist in this interpreter yet. */
    PkgAvail *availPtr;		/* First in list of all available versions
				 * of this package. */
    ClientData clientData;	/* Client data. */
} Package;

/*
 * Prototypes for procedures defined in this file:
 */

static int		CheckVersion _ANSI_ARGS_((Tcl_Interp *interp,
			    CONST char *string));
static int		ComparePkgVersions _ANSI_ARGS_((CONST char *v1, 
                            CONST char *v2,
			    int *satPtr));
static Package *	FindPackage _ANSI_ARGS_((Tcl_Interp *interp,
			    CONST char *name));

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PkgProvide / Tcl_PkgProvideEx --
 *
 *	This procedure is invoked to declare that a particular version
 *	of a particular package is now present in an interpreter.  There
 *	must not be any other version of this package already
 *	provided in the interpreter.
 *
 * Results:
 *	Normally returns TCL_OK;  if there is already another version
 *	of the package loaded then TCL_ERROR is returned and an error
 *	message is left in the interp's result.
 *
 * Side effects:
 *	The interpreter remembers that this package is available,
 *	so that no other version of the package may be provided for
 *	the interpreter.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_PkgProvide(interp, name, version)
    Tcl_Interp *interp;		/* Interpreter in which package is now
				 * available. */
    CONST char *name;		/* Name of package. */
    CONST char *version;	/* Version string for package. */
{
    return Tcl_PkgProvideEx(interp, name, version, (ClientData) NULL);
}

int
Tcl_PkgProvideEx(interp, name, version, clientData)
    Tcl_Interp *interp;		/* Interpreter in which package is now
				 * available. */
    CONST char *name;		/* Name of package. */
    CONST char *version;	/* Version string for package. */
    ClientData clientData;      /* clientdata for this package (normally
                                 * used for C callback function table) */
{
    Package *pkgPtr;

    pkgPtr = FindPackage(interp, name);
    if (pkgPtr->version == NULL) {
	pkgPtr->version = ckalloc((unsigned) (strlen(version) + 1));
	strcpy(pkgPtr->version, version);
	pkgPtr->clientData = clientData;
	return TCL_OK;
    }
    if (ComparePkgVersions(pkgPtr->version, version, (int *) NULL) == 0) {
	if (clientData != NULL) {
	    pkgPtr->clientData = clientData;
	}
	return TCL_OK;
    }
    Tcl_AppendResult(interp, "conflicting versions provided for package \"",
	    name, "\": ", pkgPtr->version, ", then ", version, (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PkgRequire / Tcl_PkgRequireEx --
 *
 *	This procedure is called by code that depends on a particular
 *	version of a particular package.  If the package is not already
 *	provided in the interpreter, this procedure invokes a Tcl script
 *	to provide it.  If the package is already provided, this
 *	procedure makes sure that the caller's needs don't conflict with
 *	the version that is present.
 *
 * Results:
 *	If successful, returns the version string for the currently
 *	provided version of the package, which may be different from
 *	the "version" argument.  If the caller's requirements
 *	cannot be met (e.g. the version requested conflicts with
 *	a currently provided version, or the required version cannot
 *	be found, or the script to provide the required version
 *	generates an error), NULL is returned and an error
 *	message is left in the interp's result.
 *
 * Side effects:
 *	The script from some previous "package ifneeded" command may
 *	be invoked to provide the package.
 *
 *----------------------------------------------------------------------
 */

CONST char *
Tcl_PkgRequire(interp, name, version, exact)
    Tcl_Interp *interp;		/* Interpreter in which package is now
				 * available. */
    CONST char *name;		/* Name of desired package. */
    CONST char *version;	/* Version string for desired version;
				 * NULL means use the latest version
				 * available. */
    int exact;			/* Non-zero means that only the particular
				 * version given is acceptable. Zero means
				 * use the latest compatible version. */
{
    return Tcl_PkgRequireEx(interp, name, version, exact, (ClientData *) NULL);
}

CONST char *
Tcl_PkgRequireEx(interp, name, version, exact, clientDataPtr)
    Tcl_Interp *interp;		/* Interpreter in which package is now
				 * available. */
    CONST char *name;		/* Name of desired package. */
    CONST char *version;	/* Version string for desired version;
				 * NULL means use the latest version
				 * available. */
    int exact;			/* Non-zero means that only the particular
				 * version given is acceptable. Zero means
				 * use the latest compatible version. */
    ClientData *clientDataPtr;	/* Used to return the client data for this
				 * package. If it is NULL then the client
				 * data is not returned. This is unchanged
				 * if this call fails for any reason. */
{
    Package *pkgPtr;
    PkgAvail *availPtr, *bestPtr;
    char *script;
    int code, satisfies, result, pass;
    Tcl_DString command;

    /*
     * If an attempt is being made to load this into a standalone executable
     * on a platform where backlinking is not supported then this must be
     * a shared version of Tcl (Otherwise the load would have failed).
     * Detect this situation by checking that this library has been correctly
     * initialised. If it has not been then return immediately as nothing will
     * work.
     */
    
    if (tclEmptyStringRep == NULL) {

	/*
	 * OK, so what's going on here?
	 *
	 * First, what are we doing?  We are performing a check on behalf of
	 * one particular caller, Tcl_InitStubs().  When a package is
	 * stub-enabled, it is statically linked to libtclstub.a, which
	 * contains a copy of Tcl_InitStubs().  When a stub-enabled package
	 * is loaded, its *_Init() function is supposed to call
	 * Tcl_InitStubs() before calling any other functions in the Tcl
	 * library.  The first Tcl function called by Tcl_InitStubs() through
	 * the stub table is Tcl_PkgRequireEx(), so this code right here is
	 * the first code that is part of the original Tcl library in the
	 * executable that gets executed on behalf of a newly loaded
	 * stub-enabled package.
	 *
	 * One easy error for the developer/builder of a stub-enabled package
	 * to make is to forget to define USE_TCL_STUBS when compiling the
	 * package.  When that happens, the package will contain symbols
	 * that are references to the Tcl library, rather than function
	 * pointers referencing the stub table.  On platforms that lack
	 * backlinking, those unresolved references may cause the loading
	 * of the package to also load a second copy of the Tcl library,
	 * leading to all kinds of trouble.  We would like to catch that
	 * error and report a useful message back to the user.  That's
	 * what we're doing.
	 *
	 * Second, how does this work?  If we reach this point, then the
	 * global variable tclEmptyStringRep has the value NULL.  Compare
	 * that with the definition of tclEmptyStringRep near the top of
	 * the file generic/tclObj.c.  It clearly should not have the value
	 * NULL; it should point to the char tclEmptyString.  If we see it
	 * having the value NULL, then somehow we are seeing a Tcl library
	 * that isn't completely initialized, and that's an indicator for the
	 * error condition described above.  (Further explanation is welcome.)
	 *
	 * Third, so what do we do about it?  This situation indicates
	 * the package we just loaded wasn't properly compiled to be
	 * stub-enabled, yet it thinks it is stub-enabled (it called
	 * Tcl_InitStubs()).  We want to report that the package just
	 * loaded is broken, so we want to place an error message in
	 * the interpreter result and return NULL to indicate failure
	 * to Tcl_InitStubs() so that it will also fail.  (Further
	 * explanation why we don't want to Tcl_Panic() is welcome.
	 * After all, two Tcl libraries can't be a good thing!)
	 *
	 * Trouble is that's going to be tricky.  We're now using a Tcl
	 * library that's not fully initialized.  In particular, it 
	 * doesn't have a proper value for tclEmptyStringRep.  The
	 * Tcl_Obj system heavily depends on the value of tclEmptyStringRep
	 * and all of Tcl depends (increasingly) on the Tcl_Obj system, we
	 * need to correct that flaw before making the calls to set the 
	 * interpreter result to the error message.  That's the only flaw
	 * corrected; other problems with initialization of the Tcl library
	 * are not remedied, so be very careful about adding any other calls
	 * here without checking how they behave when initialization is
	 * incomplete.
	 */

	tclEmptyStringRep = &tclEmptyString;
        Tcl_AppendResult(interp, "Cannot load package \"", name, 
                "\" in standalone executable: This package is not ",
                "compiled with stub support", NULL);
        return NULL;
    }

    /*
     * It can take up to three passes to find the package:  one pass to
     * run the "package unknown" script, one to run the "package ifneeded"
     * script for a specific version, and a final pass to lookup the
     * package loaded by the "package ifneeded" script.
     */

    for (pass = 1; ; pass++) {
	pkgPtr = FindPackage(interp, name);
	if (pkgPtr->version != NULL) {
	    break;
	}

	/*
	 * The package isn't yet present.  Search the list of available
	 * versions and invoke the script for the best available version.
	 */
    
	bestPtr = NULL;
	for (availPtr = pkgPtr->availPtr; availPtr != NULL;
		availPtr = availPtr->nextPtr) {
	    if ((bestPtr != NULL) && (ComparePkgVersions(availPtr->version,
		    bestPtr->version, (int *) NULL) <= 0)) {
		continue;
	    }
	    if (version != NULL) {
		result = ComparePkgVersions(availPtr->version, version,
			&satisfies);
		if ((result != 0) && exact) {
		    continue;
		}
		if (!satisfies) {
		    continue;
		}
	    }
	    bestPtr = availPtr;
	}
	if (bestPtr != NULL) {
	    /*
	     * We found an ifneeded script for the package.  Be careful while
	     * executing it:  this could cause reentrancy, so (a) protect the
	     * script itself from deletion and (b) don't assume that bestPtr
	     * will still exist when the script completes.
	     */
	
	    script = bestPtr->script;
	    Tcl_Preserve((ClientData) script);
	    code = Tcl_GlobalEval(interp, script);
	    Tcl_Release((ClientData) script);
	    if (code != TCL_OK) {
		if (code == TCL_ERROR) {
		    Tcl_AddErrorInfo(interp,
			    "\n    (\"package ifneeded\" script)");
		}
		return NULL;
	    }
	    Tcl_ResetResult(interp);
	    pkgPtr = FindPackage(interp, name);
	    break;
	}

	/*
	 * Package not in the database.  If there is a "package unknown"
	 * command, invoke it (but only on the first pass;  after that,
	 * we should not get here in the first place).
	 */

	if (pass > 1) {
	    break;
	}
	script = ((Interp *) interp)->packageUnknown;
	if (script != NULL) {
	    Tcl_DStringInit(&command);
	    Tcl_DStringAppend(&command, script, -1);
	    Tcl_DStringAppendElement(&command, name);
	    Tcl_DStringAppend(&command, " ", 1);
	    Tcl_DStringAppend(&command, (version != NULL) ? version : "{}",
		    -1);
	    if (exact) {
		Tcl_DStringAppend(&command, " -exact", 7);
	    }
	    code = Tcl_GlobalEval(interp, Tcl_DStringValue(&command));
	    Tcl_DStringFree(&command);
	    if (code != TCL_OK) {
		if (code == TCL_ERROR) {
		    Tcl_AddErrorInfo(interp,
			    "\n    (\"package unknown\" script)");
		}
		return NULL;
	    }
	    Tcl_ResetResult(interp);
	}
    }

    if (pkgPtr->version == NULL) {
	Tcl_AppendResult(interp, "can't find package ", name,
		(char *) NULL);
	if (version != NULL) {
	    Tcl_AppendResult(interp, " ", version, (char *) NULL);
	}
	return NULL;
    }

    /*
     * At this point we know that the package is present.  Make sure that the
     * provided version meets the current requirement.
     */

    if (version == NULL) {
        if (clientDataPtr) {
	    *clientDataPtr = pkgPtr->clientData;
	}
	return pkgPtr->version;
    }
    result = ComparePkgVersions(pkgPtr->version, version, &satisfies);
    if ((satisfies && !exact) || (result == 0)) {
	if (clientDataPtr) {
	    *clientDataPtr = pkgPtr->clientData;
	}
	return pkgPtr->version;
    }
    Tcl_AppendResult(interp, "version conflict for package \"",
	    name, "\": have ", pkgPtr->version, ", need ", version,
	    (char *) NULL);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PkgPresent / Tcl_PkgPresentEx --
 *
 *	Checks to see whether the specified package is present. If it
 *	is not then no additional action is taken.
 *
 * Results:
 *	If successful, returns the version string for the currently
 *	provided version of the package, which may be different from
 *	the "version" argument.  If the caller's requirements
 *	cannot be met (e.g. the version requested conflicts with
 *	a currently provided version), NULL is returned and an error
 *	message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

CONST char *
Tcl_PkgPresent(interp, name, version, exact)
    Tcl_Interp *interp;		/* Interpreter in which package is now
				 * available. */
    CONST char *name;		/* Name of desired package. */
    CONST char *version;	/* Version string for desired version;
				 * NULL means use the latest version
				 * available. */
    int exact;			/* Non-zero means that only the particular
				 * version given is acceptable. Zero means
				 * use the latest compatible version. */
{
    return Tcl_PkgPresentEx(interp, name, version, exact, (ClientData *) NULL);
}

CONST char *
Tcl_PkgPresentEx(interp, name, version, exact, clientDataPtr)
    Tcl_Interp *interp;		/* Interpreter in which package is now
				 * available. */
    CONST char *name;		/* Name of desired package. */
    CONST char *version;	/* Version string for desired version;
				 * NULL means use the latest version
				 * available. */
    int exact;			/* Non-zero means that only the particular
				 * version given is acceptable. Zero means
				 * use the latest compatible version. */
    ClientData *clientDataPtr;	/* Used to return the client data for this
				 * package. If it is NULL then the client
				 * data is not returned. This is unchanged
				 * if this call fails for any reason. */
{
    Interp *iPtr = (Interp *) interp;
    Tcl_HashEntry *hPtr;
    Package *pkgPtr;
    int satisfies, result;

    hPtr = Tcl_FindHashEntry(&iPtr->packageTable, name);
    if (hPtr) {
	pkgPtr = (Package *) Tcl_GetHashValue(hPtr);
	if (pkgPtr->version != NULL) {
	    
	    /*
	     * At this point we know that the package is present.  Make sure
	     * that the provided version meets the current requirement.
	     */

	    if (version == NULL) {
		if (clientDataPtr) {
		    *clientDataPtr = pkgPtr->clientData;
		}
		
		return pkgPtr->version;
	    }
	    result = ComparePkgVersions(pkgPtr->version, version, &satisfies);
	    if ((satisfies && !exact) || (result == 0)) {
		if (clientDataPtr) {
		    *clientDataPtr = pkgPtr->clientData;
		}
    
		return pkgPtr->version;
	    }
	    Tcl_AppendResult(interp, "version conflict for package \"",
			     name, "\": have ", pkgPtr->version,
			     ", need ", version, (char *) NULL);
	    return NULL;
	}
    }

    if (version != NULL) {
	Tcl_AppendResult(interp, "package ", name, " ", version,
			 " is not present", (char *) NULL);
    } else {
	Tcl_AppendResult(interp, "package ", name, " is not present",
			 (char *) NULL);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PackageObjCmd --
 *
 *	This procedure is invoked to process the "package" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tcl_PackageObjCmd(dummy, interp, objc, objv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];	/* Argument objects. */
{
    static CONST char *pkgOptions[] = {
	"forget", "ifneeded", "names", "present", "provide", "require",
	"unknown", "vcompare", "versions", "vsatisfies", (char *) NULL
    };
    enum pkgOptions {
	PKG_FORGET, PKG_IFNEEDED, PKG_NAMES, PKG_PRESENT,
	PKG_PROVIDE, PKG_REQUIRE, PKG_UNKNOWN, PKG_VCOMPARE,
	PKG_VERSIONS, PKG_VSATISFIES
    };
    Interp *iPtr = (Interp *) interp;
    int optionIndex, exact, i, satisfies;
    PkgAvail *availPtr, *prevPtr;
    Package *pkgPtr;
    Tcl_HashEntry *hPtr;
    Tcl_HashSearch search;
    Tcl_HashTable *tablePtr;
    CONST char *version;
    char *argv2, *argv3, *argv4;

    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], pkgOptions, "option", 0,
	    &optionIndex) != TCL_OK) {
	return TCL_ERROR;
    }
    switch ((enum pkgOptions) optionIndex) {
	case PKG_FORGET: {
	    char *keyString;
	    for (i = 2; i < objc; i++) {
		keyString = Tcl_GetString(objv[i]);
		hPtr = Tcl_FindHashEntry(&iPtr->packageTable, keyString);
		if (hPtr == NULL) {
		    continue;	
		}
		pkgPtr = (Package *) Tcl_GetHashValue(hPtr);
		Tcl_DeleteHashEntry(hPtr);
		if (pkgPtr->version != NULL) {
		    ckfree(pkgPtr->version);
		}
		while (pkgPtr->availPtr != NULL) {
		    availPtr = pkgPtr->availPtr;
		    pkgPtr->availPtr = availPtr->nextPtr;
		    ckfree(availPtr->version);
		    Tcl_EventuallyFree((ClientData)availPtr->script, TCL_DYNAMIC);
		    ckfree((char *) availPtr);
		}
		ckfree((char *) pkgPtr);
	    }
	    break;
	}
	case PKG_IFNEEDED: {
	    int length;
	    if ((objc != 4) && (objc != 5)) {
		Tcl_WrongNumArgs(interp, 2, objv, "package version ?script?");
		return TCL_ERROR;
	    }
	    argv3 = Tcl_GetString(objv[3]);
	    if (CheckVersion(interp, argv3) != TCL_OK) {
		return TCL_ERROR;
	    }
	    argv2 = Tcl_GetString(objv[2]);
	    if (objc == 4) {
		hPtr = Tcl_FindHashEntry(&iPtr->packageTable, argv2);
		if (hPtr == NULL) {
		    return TCL_OK;
		}
		pkgPtr = (Package *) Tcl_GetHashValue(hPtr);
	    } else {
		pkgPtr = FindPackage(interp, argv2);
	    }
	    argv3 = Tcl_GetStringFromObj(objv[3], &length);
	    for (availPtr = pkgPtr->availPtr, prevPtr = NULL; availPtr != NULL;
		 prevPtr = availPtr, availPtr = availPtr->nextPtr) {
		if (ComparePkgVersions(availPtr->version, argv3, (int *) NULL)
			== 0) {
		    if (objc == 4) {
			Tcl_SetResult(interp, availPtr->script, TCL_VOLATILE);
			return TCL_OK;
		    }
		    Tcl_EventuallyFree((ClientData)availPtr->script, TCL_DYNAMIC);
		    break;
		}
	    }
	    if (objc == 4) {
		return TCL_OK;
	    }
	    if (availPtr == NULL) {
		availPtr = (PkgAvail *) ckalloc(sizeof(PkgAvail));
		availPtr->version = ckalloc((unsigned) (length + 1));
		strcpy(availPtr->version, argv3);
		if (prevPtr == NULL) {
		    availPtr->nextPtr = pkgPtr->availPtr;
		    pkgPtr->availPtr = availPtr;
		} else {
		    availPtr->nextPtr = prevPtr->nextPtr;
		    prevPtr->nextPtr = availPtr;
		}
	    }
	    argv4 = Tcl_GetStringFromObj(objv[4], &length);
	    availPtr->script = ckalloc((unsigned) (length + 1));
	    strcpy(availPtr->script, argv4);
	    break;
	}
	case PKG_NAMES: {
	    if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, NULL);
		return TCL_ERROR;
	    }
	    tablePtr = &iPtr->packageTable;
	    for (hPtr = Tcl_FirstHashEntry(tablePtr, &search); hPtr != NULL;
		 hPtr = Tcl_NextHashEntry(&search)) {
		pkgPtr = (Package *) Tcl_GetHashValue(hPtr);
		if ((pkgPtr->version != NULL) || (pkgPtr->availPtr != NULL)) {
		    Tcl_AppendElement(interp, Tcl_GetHashKey(tablePtr, hPtr));
		}
	    }
	    break;
	}
	case PKG_PRESENT: {
	    if (objc < 3) {
		presentSyntax:
		Tcl_WrongNumArgs(interp, 2, objv, "?-exact? package ?version?");
		return TCL_ERROR;
	    }
	    argv2 = Tcl_GetString(objv[2]);
	    if ((argv2[0] == '-') && (strcmp(argv2, "-exact") == 0)) {
		exact = 1;
	    } else {
		exact = 0;
	    }
	    version = NULL;
	    if (objc == (4 + exact)) {
		version =  Tcl_GetString(objv[3 + exact]);
		if (CheckVersion(interp, version) != TCL_OK) {
		    return TCL_ERROR;
		}
	    } else if ((objc != 3) || exact) {
		goto presentSyntax;
	    }
	    if (exact) {
		argv3 =  Tcl_GetString(objv[3]);
		version = Tcl_PkgPresent(interp, argv3, version, exact);
	    } else {
		version = Tcl_PkgPresent(interp, argv2, version, exact);
	    }
	    if (version == NULL) {
		return TCL_ERROR;
	    }
	    Tcl_SetObjResult( interp, Tcl_NewStringObj( version, -1 ) );
	    break;
	}
	case PKG_PROVIDE: {
	    if ((objc != 3) && (objc != 4)) {
		Tcl_WrongNumArgs(interp, 2, objv, "package ?version?");
		return TCL_ERROR;
	    }
	    argv2 = Tcl_GetString(objv[2]);
	    if (objc == 3) {
		hPtr = Tcl_FindHashEntry(&iPtr->packageTable, argv2);
		if (hPtr != NULL) {
		    pkgPtr = (Package *) Tcl_GetHashValue(hPtr);
		    if (pkgPtr->version != NULL) {
			Tcl_SetResult(interp, pkgPtr->version, TCL_VOLATILE);
		    }
		}
		return TCL_OK;
	    }
	    argv3 = Tcl_GetString(objv[3]);
	    if (CheckVersion(interp, argv3) != TCL_OK) {
		return TCL_ERROR;
	    }
	    return Tcl_PkgProvide(interp, argv2, argv3);
	}
	case PKG_REQUIRE: {
	    if (objc < 3) {
		requireSyntax:
		Tcl_WrongNumArgs(interp, 2, objv, "?-exact? package ?version?");
		return TCL_ERROR;
	    }
	    argv2 = Tcl_GetString(objv[2]);
	    if ((argv2[0] == '-') && (strcmp(argv2, "-exact") == 0)) {
		exact = 1;
	    } else {
		exact = 0;
	    }
	    version = NULL;
	    if (objc == (4 + exact)) {
		version =  Tcl_GetString(objv[3 + exact]);
		if (CheckVersion(interp, version) != TCL_OK) {
		    return TCL_ERROR;
		}
	    } else if ((objc != 3) || exact) {
		goto requireSyntax;
	    }
	    if (exact) {
		argv3 =  Tcl_GetString(objv[3]);
		version = Tcl_PkgRequire(interp, argv3, version, exact);
	    } else {
		version = Tcl_PkgRequire(interp, argv2, version, exact);
	    }
	    if (version == NULL) {
		return TCL_ERROR;
	    }
	    Tcl_SetObjResult( interp, Tcl_NewStringObj( version, -1 ) );
	    break;
	}
	case PKG_UNKNOWN: {
	    int length;
	    if (objc == 2) {
		if (iPtr->packageUnknown != NULL) {
		    Tcl_SetResult(interp, iPtr->packageUnknown, TCL_VOLATILE);
		}
	    } else if (objc == 3) {
		if (iPtr->packageUnknown != NULL) {
		    ckfree(iPtr->packageUnknown);
		}
		argv2 = Tcl_GetStringFromObj(objv[2], &length);
		if (argv2[0] == 0) {
		    iPtr->packageUnknown = NULL;
		} else {
		    iPtr->packageUnknown = (char *) ckalloc((unsigned)
			    (length + 1));
		    strcpy(iPtr->packageUnknown, argv2);
		}
	    } else {
		Tcl_WrongNumArgs(interp, 2, objv, "?command?");
		return TCL_ERROR;
	    }
	    break;
	}
	case PKG_VCOMPARE: {
	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 2, objv, "version1 version2");
		return TCL_ERROR;
	    }
	    argv3 = Tcl_GetString(objv[3]);
	    argv2 = Tcl_GetString(objv[2]);
	    if ((CheckVersion(interp, argv2) != TCL_OK)
		    || (CheckVersion(interp, argv3) != TCL_OK)) {
		return TCL_ERROR;
	    }
	    Tcl_SetIntObj(Tcl_GetObjResult(interp),
		    ComparePkgVersions(argv2, argv3, (int *) NULL));
	    break;
	}
	case PKG_VERSIONS: {
	    if (objc != 3) {
		Tcl_WrongNumArgs(interp, 2, objv, "package");
		return TCL_ERROR;
	    }
	    argv2 = Tcl_GetString(objv[2]);
	    hPtr = Tcl_FindHashEntry(&iPtr->packageTable, argv2);
	    if (hPtr != NULL) {
		pkgPtr = (Package *) Tcl_GetHashValue(hPtr);
		for (availPtr = pkgPtr->availPtr; availPtr != NULL;
		     availPtr = availPtr->nextPtr) {
		    Tcl_AppendElement(interp, availPtr->version);
		}
	    }
	    break;
	}
	case PKG_VSATISFIES: {
	    if (objc != 4) {
		Tcl_WrongNumArgs(interp, 2, objv, "version1 version2");
		return TCL_ERROR;
	    }
	    argv3 = Tcl_GetString(objv[3]);
	    argv2 = Tcl_GetString(objv[2]);
	    if ((CheckVersion(interp, argv2) != TCL_OK)
		    || (CheckVersion(interp, argv3) != TCL_OK)) {
		return TCL_ERROR;
	    }
	    ComparePkgVersions(argv2, argv3, &satisfies);
	    Tcl_SetIntObj(Tcl_GetObjResult(interp), satisfies);
	    break;
	}
	default: {
	    panic("Tcl_PackageObjCmd: bad option index to pkgOptions");
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FindPackage --
 *
 *	This procedure finds the Package record for a particular package
 *	in a particular interpreter, creating a record if one doesn't
 *	already exist.
 *
 * Results:
 *	The return value is a pointer to the Package record for the
 *	package.
 *
 * Side effects:
 *	A new Package record may be created.
 *
 *----------------------------------------------------------------------
 */

static Package *
FindPackage(interp, name)
    Tcl_Interp *interp;		/* Interpreter to use for package lookup. */
    CONST char *name;		/* Name of package to fine. */
{
    Interp *iPtr = (Interp *) interp;
    Tcl_HashEntry *hPtr;
    int new;
    Package *pkgPtr;

    hPtr = Tcl_CreateHashEntry(&iPtr->packageTable, name, &new);
    if (new) {
	pkgPtr = (Package *) ckalloc(sizeof(Package));
	pkgPtr->version = NULL;
	pkgPtr->availPtr = NULL;
	pkgPtr->clientData = NULL;
	Tcl_SetHashValue(hPtr, pkgPtr);
    } else {
	pkgPtr = (Package *) Tcl_GetHashValue(hPtr);
    }
    return pkgPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclFreePackageInfo --
 *
 *	This procedure is called during interpreter deletion to
 *	free all of the package-related information for the
 *	interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is freed.
 *
 *----------------------------------------------------------------------
 */

void
TclFreePackageInfo(iPtr)
    Interp *iPtr;		/* Interpereter that is being deleted. */
{
    Package *pkgPtr;
    Tcl_HashSearch search;
    Tcl_HashEntry *hPtr;
    PkgAvail *availPtr;

    for (hPtr = Tcl_FirstHashEntry(&iPtr->packageTable, &search);
	    hPtr != NULL;  hPtr = Tcl_NextHashEntry(&search)) {
	pkgPtr = (Package *) Tcl_GetHashValue(hPtr);
	if (pkgPtr->version != NULL) {
	    ckfree(pkgPtr->version);
	}
	while (pkgPtr->availPtr != NULL) {
	    availPtr = pkgPtr->availPtr;
	    pkgPtr->availPtr = availPtr->nextPtr;
	    ckfree(availPtr->version);
	    Tcl_EventuallyFree((ClientData)availPtr->script, TCL_DYNAMIC);
	    ckfree((char *) availPtr);
	}
	ckfree((char *) pkgPtr);
    }
    Tcl_DeleteHashTable(&iPtr->packageTable);
    if (iPtr->packageUnknown != NULL) {
	ckfree(iPtr->packageUnknown);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CheckVersion --
 *
 *	This procedure checks to see whether a version number has
 *	valid syntax.
 *
 * Results:
 *	If string is a properly formed version number the TCL_OK
 *	is returned.  Otherwise TCL_ERROR is returned and an error
 *	message is left in the interp's result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
CheckVersion(interp, string)
    Tcl_Interp *interp;		/* Used for error reporting. */
    CONST char *string;		/* Supposedly a version number, which is
				 * groups of decimal digits separated
				 * by dots. */
{
    CONST char *p = string;
    char prevChar;
    
    if (!isdigit(UCHAR(*p))) {	/* INTL: digit */
	goto error;
    }
    for (prevChar = *p, p++; *p != 0; p++) {
	if (!isdigit(UCHAR(*p)) &&
		((*p != '.') || (prevChar == '.'))) { /* INTL: digit */
	    goto error;
	}
	prevChar = *p;
    }
    if (prevChar != '.') {
	return TCL_OK;
    }

    error:
    Tcl_AppendResult(interp, "expected version number but got \"",
	    string, "\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * ComparePkgVersions --
 *
 *	This procedure compares two version numbers.
 *
 * Results:
 *	The return value is -1 if v1 is less than v2, 0 if the two
 *	version numbers are the same, and 1 if v1 is greater than v2.
 *	If *satPtr is non-NULL, the word it points to is filled in
 *	with 1 if v2 >= v1 and both numbers have the same major number
 *	or 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ComparePkgVersions(v1, v2, satPtr)
    CONST char *v1;
    CONST char *v2;		/* Versions strings, of form 2.1.3 (any
				 * number of version numbers). */
    int *satPtr;		/* If non-null, the word pointed to is
				 * filled in with a 0/1 value.  1 means
				 * v1 "satisfies" v2:  v1 is greater than
				 * or equal to v2 and both version numbers
				 * have the same major number. */
{
    int thisIsMajor, n1, n2;

    /*
     * Each iteration of the following loop processes one number from
     * each string, terminated by a ".".  If those numbers don't match
     * then the comparison is over;  otherwise, we loop back for the
     * next number.
     */

    thisIsMajor = 1;
    while (1) {
	/*
	 * Parse one decimal number from the front of each string.
	 */

	n1 = n2 = 0;
	while ((*v1 != 0) && (*v1 != '.')) {
	    n1 = 10*n1 + (*v1 - '0');
	    v1++;
	}
	while ((*v2 != 0) && (*v2 != '.')) {
	    n2 = 10*n2 + (*v2 - '0');
	    v2++;
	}

	/*
	 * Compare and go on to the next version number if the
	 * current numbers match.
	 */

	if (n1 != n2) {
	    break;
	}
	if (*v1 != 0) {
	    v1++;
	} else if (*v2 == 0) {
	    break;
	}
	if (*v2 != 0) {
	    v2++;
	}
	thisIsMajor = 0;
    }
    if (satPtr != NULL) {
	*satPtr = (n1 == n2) || ((n1 > n2) && !thisIsMajor);
    }
    if (n1 > n2) {
	return 1;
    } else if (n1 == n2) {
	return 0;
    } else {
	return -1;
    }
}
