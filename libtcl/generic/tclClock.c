/* 
 * tclClock.c --
 *
 *	Contains the time and date related commands.  This code
 *	is derived from the time and date facilities of TclX,
 *	by Mark Diekhans and Karl Lehenbauer.
 *
 * Copyright 1991-1995 Karl Lehenbauer and Mark Diekhans.
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tcl.h"
#include "tclInt.h"
#include "tclPort.h"

/*
 * The date parsing stuff uses lexx and has tons o statics.
 */

TCL_DECLARE_MUTEX(clockMutex)

/*
 * Function prototypes for local procedures in this file:
 */

static int		FormatClock _ANSI_ARGS_((Tcl_Interp *interp,
			    unsigned long clockVal, int useGMT,
			    char *format));

/*
 *-------------------------------------------------------------------------
 *
 * Tcl_ClockObjCmd --
 *
 *	This procedure is invoked to process the "clock" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *-------------------------------------------------------------------------
 */

int
Tcl_ClockObjCmd (client, interp, objc, objv)
    ClientData client;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int objc;				/* Number of arguments. */
    Tcl_Obj *CONST objv[];		/* Argument values. */
{
    Tcl_Obj *resultPtr;
    int index;
    Tcl_Obj *CONST *objPtr;
    int useGMT = 0;
    char *format = "%a %b %d %X %Z %Y";
    int dummy;
    unsigned long baseClock, clockVal;
    long zone;
    Tcl_Obj *baseObjPtr = NULL;
    char *scanStr;
    
    static char *switches[] =
	    {"clicks", "format", "scan", "seconds", (char *) NULL};
    static char *formatSwitches[] = {"-format", "-gmt", (char *) NULL};
    static char *scanSwitches[] = {"-base", "-gmt", (char *) NULL};

    resultPtr = Tcl_GetObjResult(interp);
    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg ...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], switches, "option", 0, &index)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    switch (index) {
	case 0:			/* clicks */
	    if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, NULL);
		return TCL_ERROR;
	    }
	    Tcl_SetLongObj(resultPtr, (long) TclpGetClicks());
	    return TCL_OK;
	case 1:			/* format */
	    if ((objc < 3) || (objc > 7)) {
		wrongFmtArgs:
		Tcl_WrongNumArgs(interp, 2, objv,
			"clockval ?-format string? ?-gmt boolean?");
		return TCL_ERROR;
	    }

	    if (Tcl_GetLongFromObj(interp, objv[2], (long*) &clockVal)
		    != TCL_OK) {
		return TCL_ERROR;
	    }
    
	    objPtr = objv+3;
	    objc -= 3;
	    while (objc > 1) {
		if (Tcl_GetIndexFromObj(interp, objPtr[0], formatSwitches,
			"switch", 0, &index) != TCL_OK) {
		    return TCL_ERROR;
		}
		switch (index) {
		    case 0:		/* -format */
			format = Tcl_GetStringFromObj(objPtr[1], &dummy);
			break;
		    case 1:		/* -gmt */
			if (Tcl_GetBooleanFromObj(interp, objPtr[1],
				&useGMT) != TCL_OK) {
			    return TCL_ERROR;
			}
			break;
		}
		objPtr += 2;
		objc -= 2;
	    }
	    if (objc != 0) {
		goto wrongFmtArgs;
	    }
	    return FormatClock(interp, (unsigned long) clockVal, useGMT,
		    format);
	case 2:			/* scan */
	    if ((objc < 3) || (objc > 7)) {
		wrongScanArgs:
		Tcl_WrongNumArgs(interp, 2, objv,
			"dateString ?-base clockValue? ?-gmt boolean?");
		return TCL_ERROR;
	    }

	    objPtr = objv+3;
	    objc -= 3;
	    while (objc > 1) {
		if (Tcl_GetIndexFromObj(interp, objPtr[0], scanSwitches,
			"switch", 0, &index) != TCL_OK) {
		    return TCL_ERROR;
		}
		switch (index) {
		    case 0:		/* -base */
			baseObjPtr = objPtr[1];
			break;
		    case 1:		/* -gmt */
			if (Tcl_GetBooleanFromObj(interp, objPtr[1],
				&useGMT) != TCL_OK) {
			    return TCL_ERROR;
			}
			break;
		}
		objPtr += 2;
		objc -= 2;
	    }
	    if (objc != 0) {
		goto wrongScanArgs;
	    }

	    if (baseObjPtr != NULL) {
		if (Tcl_GetLongFromObj(interp, baseObjPtr,
			(long*) &baseClock) != TCL_OK) {
		    return TCL_ERROR;
		}
	    } else {
		baseClock = TclpGetSeconds();
	    }

	    if (useGMT) {
		zone = -50000; /* Force GMT */
	    } else {
		zone = TclpGetTimeZone((unsigned long) baseClock);
	    }

	    scanStr = Tcl_GetStringFromObj(objv[2], &dummy);
	    Tcl_MutexLock(&clockMutex);
	    if (TclGetDate(scanStr, (unsigned long) baseClock, zone,
		    (unsigned long *) &clockVal) < 0) {
		Tcl_MutexUnlock(&clockMutex);
		Tcl_AppendStringsToObj(resultPtr,
			"unable to convert date-time string \"",
			scanStr, "\"", (char *) NULL);
		return TCL_ERROR;
	    }
	    Tcl_MutexUnlock(&clockMutex);

	    Tcl_SetLongObj(resultPtr, (long) clockVal);
	    return TCL_OK;
	case 3:			/* seconds */
	    if (objc != 2) {
		Tcl_WrongNumArgs(interp, 2, objv, NULL);
		return TCL_ERROR;
	    }
	    Tcl_SetLongObj(resultPtr, (long) TclpGetSeconds());
	    return TCL_OK;
	default:
	    return TCL_ERROR;	/* Should never be reached. */
    }
}

/*
 *-----------------------------------------------------------------------------
 *
 * FormatClock --
 *
 *      Formats a time value based on seconds into a human readable
 *	string.
 *
 * Results:
 *      Standard Tcl result.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
FormatClock(interp, clockVal, useGMT, format)
    Tcl_Interp *interp;			/* Current interpreter. */
    unsigned long clockVal;	       	/* Time in seconds. */
    int useGMT;				/* Boolean */
    char *format;			/* Format string */
{
    struct tm *timeDataPtr;
    Tcl_DString buffer;
    int bufSize;
    char *p;
    Tcl_Obj *resultPtr;
    int result;
#ifndef HAVE_TM_ZONE
    int savedTimeZone = 0;	/* lint. */
    char *savedTZEnv = NULL;	/* lint. */
#endif

    resultPtr = Tcl_GetObjResult(interp);
#ifdef HAVE_TZSET
    /*
     * Some systems forgot to call tzset in localtime, make sure its done.
     */
    static int  calledTzset = 0;

    Tcl_MutexLock(&clockMutex);
    if (!calledTzset) {
        tzset();
        calledTzset = 1;
    }
    Tcl_MutexUnlock(&clockMutex);
#endif

#ifndef HAVE_TM_ZONE
    /*
     * This is a kludge for systems not having the timezone string in
     * struct tm.  No matter what was specified, they use the local
     * timezone string.
     */

    if (useGMT) {
        char *varValue;

        varValue = Tcl_GetVar2(interp, "env", "TZ", TCL_GLOBAL_ONLY);
        if (varValue != NULL) {
	    savedTZEnv = strcpy(ckalloc(strlen(varValue) + 1), varValue);
        } else {
            savedTZEnv = NULL;
	}
        Tcl_SetVar2(interp, "env", "TZ", "GMT", TCL_GLOBAL_ONLY);
        savedTimeZone = timezone;
        timezone = 0;
        tzset();
    }
#endif

    timeDataPtr = TclpGetDate((TclpTime_t) &clockVal, useGMT);
    
    /*
     * Make a guess at the upper limit on the substituted string size
     * based on the number of percents in the string.
     */

    for (bufSize = 1, p = format; *p != '\0'; p++) {
	if (*p == '%') {
	    bufSize += 40;
	} else {
	    bufSize++;
	}
    }
    Tcl_DStringInit(&buffer);
    Tcl_DStringSetLength(&buffer, bufSize);

    Tcl_MutexLock(&clockMutex);
    result = TclpStrftime(buffer.string, (unsigned int) bufSize, format,
	    timeDataPtr);
    Tcl_MutexUnlock(&clockMutex);

#ifndef HAVE_TM_ZONE
    if (useGMT) {
        if (savedTZEnv != NULL) {
            Tcl_SetVar2(interp, "env", "TZ", savedTZEnv, TCL_GLOBAL_ONLY);
            ckfree(savedTZEnv);
        } else {
            Tcl_UnsetVar2(interp, "env", "TZ", TCL_GLOBAL_ONLY);
        }
        timezone = savedTimeZone;
        tzset();
    }
#endif
    if ((result == 0) && (*format != '\0')) {
	Tcl_AppendStringsToObj(resultPtr, "bad format string \"", format, 
		"\"", (char *) NULL);
	return TCL_ERROR;
    }

    Tcl_SetStringObj(resultPtr, buffer.string, -1);
    Tcl_DStringFree(&buffer);
    return TCL_OK;
}

