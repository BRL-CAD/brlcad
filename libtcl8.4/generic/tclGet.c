/* 
 * tclGet.c --
 *
 *	This file contains procedures to convert strings into
 *	other forms, like integers or floating-point numbers or
 *	booleans, doing syntax checking along the way.
 *
 * Copyright (c) 1990-1993 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tclInt.h"
#include "tclPort.h"
#include "tclMath.h"


/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetInt --
 *
 *	Given a string, produce the corresponding integer value.
 *
 * Results:
 *	The return value is normally TCL_OK;  in this case *intPtr
 *	will be set to the integer value equivalent to string.  If
 *	string is improperly formed then TCL_ERROR is returned and
 *	an error message will be left in the interp's result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetInt(interp, string, intPtr)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    CONST char *string;		/* String containing a (possibly signed)
				 * integer in a form acceptable to strtol. */
    int *intPtr;		/* Place to store converted result. */
{
    char *end;
    CONST char *p = string;
    long i;

    /*
     * Note: use strtoul instead of strtol for integer conversions
     * to allow full-size unsigned numbers, but don't depend on strtoul
     * to handle sign characters;  it won't in some implementations.
     */

    errno = 0;
#ifdef TCL_STRTOUL_SIGN_CHECK
    /*
     * This special sign check actually causes bad numbers to be allowed
     * when strtoul.  I can't find a strtoul that doesn't validly handle
     * signed characters, and the C standard implies that this is all
     * unnecessary. [Bug #634856]
     */
    for ( ; isspace(UCHAR(*p)); p++) {	/* INTL: ISO space. */
	/* Empty loop body. */
    }
    if (*p == '-') {
	p++;
	i = -((long)strtoul(p, &end, 0)); /* INTL: Tcl source. */
    } else if (*p == '+') {
	p++;
	i = strtoul(p, &end, 0); /* INTL: Tcl source. */
    } else
#else
	i = strtoul(p, &end, 0); /* INTL: Tcl source. */
#endif
    if (end == p) {
	badInteger:
        if (interp != (Tcl_Interp *) NULL) {
	    Tcl_AppendResult(interp, "expected integer but got \"", string,
		    "\"", (char *) NULL);
	    TclCheckBadOctal(interp, string);
        }
	return TCL_ERROR;
    }

    /*
     * The second test below is needed on platforms where "long" is
     * larger than "int" to detect values that fit in a long but not in
     * an int.
     */

    if ((errno == ERANGE) || (((long)(int) i) != i)) {
        if (interp != (Tcl_Interp *) NULL) {
	    Tcl_SetResult(interp, "integer value too large to represent",
		    TCL_STATIC);
            Tcl_SetErrorCode(interp, "ARITH", "IOVERFLOW",
		    Tcl_GetStringResult(interp), (char *) NULL);
        }
	return TCL_ERROR;
    }
    while ((*end != '\0') && isspace(UCHAR(*end))) { /* INTL: ISO space. */
	end++;
    }
    if (*end != 0) {
	goto badInteger;
    }
    *intPtr = (int) i;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetLong --
 *
 *	Given a string, produce the corresponding long integer value.
 *	This routine is a version of Tcl_GetInt but returns a "long"
 *	instead of an "int".
 *
 * Results:
 *	The return value is normally TCL_OK; in this case *longPtr
 *	will be set to the long integer value equivalent to string. If
 *	string is improperly formed then TCL_ERROR is returned and
 *	an error message will be left in the interp's result if interp
 *	is non-NULL. 
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGetLong(interp, string, longPtr)
    Tcl_Interp *interp;		/* Interpreter used for error reporting
				 * if not NULL. */
    CONST char *string;		/* String containing a (possibly signed)
				 * long integer in a form acceptable to
				 * strtoul. */
    long *longPtr;		/* Place to store converted long result. */
{
    char *end;
    CONST char *p = string;
    long i;

    /*
     * Note: don't depend on strtoul to handle sign characters; it won't
     * in some implementations.
     */

    errno = 0;
#ifdef TCL_STRTOUL_SIGN_CHECK
    for ( ; isspace(UCHAR(*p)); p++) {	/* INTL: ISO space. */
	/* Empty loop body. */
    }
    if (*p == '-') {
	p++;
	i = -(int)strtoul(p, &end, 0); /* INTL: Tcl source. */
    } else if (*p == '+') {
	p++;
	i = strtoul(p, &end, 0); /* INTL: Tcl source. */
    } else
#else
	i = strtoul(p, &end, 0); /* INTL: Tcl source. */
#endif
    if (end == p) {
	badInteger:
        if (interp != (Tcl_Interp *) NULL) {
	    Tcl_AppendResult(interp, "expected integer but got \"", string,
		    "\"", (char *) NULL);
	    TclCheckBadOctal(interp, string);
        }
	return TCL_ERROR;
    }
    if (errno == ERANGE) {
        if (interp != (Tcl_Interp *) NULL) {
	    Tcl_SetResult(interp, "integer value too large to represent",
		    TCL_STATIC);
            Tcl_SetErrorCode(interp, "ARITH", "IOVERFLOW",
                    Tcl_GetStringResult(interp), (char *) NULL);
        }
	return TCL_ERROR;
    }
    while ((*end != '\0') && isspace(UCHAR(*end))) { /* INTL: ISO space. */
	end++;
    }
    if (*end != 0) {
	goto badInteger;
    }
    *longPtr = i;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetDouble --
 *
 *	Given a string, produce the corresponding double-precision
 *	floating-point value.
 *
 * Results:
 *	The return value is normally TCL_OK; in this case *doublePtr
 *	will be set to the double-precision value equivalent to string.
 *	If string is improperly formed then TCL_ERROR is returned and
 *	an error message will be left in the interp's result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetDouble(interp, string, doublePtr)
    Tcl_Interp *interp;		/* Interpreter used for error reporting. */
    CONST char *string;		/* String containing a floating-point number
				 * in a form acceptable to strtod. */
    double *doublePtr;		/* Place to store converted result. */
{
    char *end;
    double d;

    errno = 0;
    d = strtod(string, &end); /* INTL: Tcl source. */
    if (end == string) {
	badDouble:
        if (interp != (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp,
                    "expected floating-point number but got \"",
                    string, "\"", (char *) NULL);
        }
	return TCL_ERROR;
    }
    if (errno != 0 && (d == HUGE_VAL || d == -HUGE_VAL || d == 0)) {
        if (interp != (Tcl_Interp *) NULL) {
            TclExprFloatError(interp, d); 
        }
	return TCL_ERROR;
    }
    while ((*end != 0) && isspace(UCHAR(*end))) { /* INTL: ISO space. */
	end++;
    }
    if (*end != 0) {
	goto badDouble;
    }
    *doublePtr = d;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetBoolean --
 *
 *	Given a string, return a 0/1 boolean value corresponding
 *	to the string.
 *
 * Results:
 *	The return value is normally TCL_OK;  in this case *boolPtr
 *	will be set to the 0/1 value equivalent to string.  If
 *	string is improperly formed then TCL_ERROR is returned and
 *	an error message will be left in the interp's result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_GetBoolean(interp, string, boolPtr)
    Tcl_Interp *interp;		/* Interpreter used for error reporting. */
    CONST char *string;		/* String containing a boolean number
				 * specified either as 1/0 or true/false or
				 * yes/no. */
    int *boolPtr;		/* Place to store converted result, which
				 * will be 0 or 1. */
{
    int i;
    char lowerCase[10], c;
    size_t length;

    /*
     * Convert the input string to all lower-case. 
     * INTL: This code will work on UTF strings.
     */

    for (i = 0; i < 9; i++) {
	c = string[i];
	if (c == 0) {
	    break;
	}
	if ((c >= 'A') && (c <= 'Z')) {
	    c += (char) ('a' - 'A');
	}
	lowerCase[i] = c;
    }
    lowerCase[i] = 0;

    length = strlen(lowerCase);
    c = lowerCase[0];
    if ((c == '0') && (lowerCase[1] == '\0')) {
	*boolPtr = 0;
    } else if ((c == '1') && (lowerCase[1] == '\0')) {
	*boolPtr = 1;
    } else if ((c == 'y') && (strncmp(lowerCase, "yes", length) == 0)) {
	*boolPtr = 1;
    } else if ((c == 'n') && (strncmp(lowerCase, "no", length) == 0)) {
	*boolPtr = 0;
    } else if ((c == 't') && (strncmp(lowerCase, "true", length) == 0)) {
	*boolPtr = 1;
    } else if ((c == 'f') && (strncmp(lowerCase, "false", length) == 0)) {
	*boolPtr = 0;
    } else if ((c == 'o') && (length >= 2)) {
	if (strncmp(lowerCase, "on", length) == 0) {
	    *boolPtr = 1;
	} else if (strncmp(lowerCase, "off", length) == 0) {
	    *boolPtr = 0;
	} else {
	    goto badBoolean;
	}
    } else {
	badBoolean:
        if (interp != (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp, "expected boolean value but got \"",
                    string, "\"", (char *) NULL);
        }
	return TCL_ERROR;
    }
    return TCL_OK;
}
