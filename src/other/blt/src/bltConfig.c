/*
 * bltConfig.c --
 *
 *	This module implements custom configuration options for the BLT
 *	toolkit.
 *
 * Copyright 1991-1998 Lucent Technologies, Inc.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that the copyright notice and warranty
 * disclaimer appear in supporting documentation, and that the names
 * of Lucent Technologies any of their entities not be used in
 * advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.
 *
 * Lucent Technologies disclaims all warranties with regard to this
 * software, including all implied warranties of merchantability and
 * fitness.  In no event shall Lucent Technologies be liable for any
 * special, indirect or consequential damages or any damages
 * whatsoever resulting from loss of use, data or profits, whether in
 * an action of contract, negligence or other tortuous action, arising
 * out of or in connection with the use or performance of this
 * software.
 */

#include "bltInt.h"
#if defined(__STDC__)
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "bltTile.h"

static int StringToFill _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec,
	int flags));
static char *FillToString _ANSI_ARGS_((ClientData, Tk_Window, char *, int,
	Tcl_FreeProc **));

Tk_CustomOption bltFillOption =
{
    StringToFill, FillToString, (ClientData)0
};

static int StringToPad _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec,
	int offset));
static char *PadToString _ANSI_ARGS_((ClientData clientData, Tk_Window tkwin,
	char *widgRec, int offset, Tcl_FreeProc **freeProcPtr));

Tk_CustomOption bltPadOption =
{
    StringToPad, PadToString, (ClientData)0
};

static int StringToDistance _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec,
	int flags));
static char *DistanceToString _ANSI_ARGS_((ClientData, Tk_Window, char *, int,
	Tcl_FreeProc **));

Tk_CustomOption bltDistanceOption =
{
    StringToDistance, DistanceToString, (ClientData)PIXELS_NONNEGATIVE
};

Tk_CustomOption bltPositiveDistanceOption =
{
    StringToDistance, DistanceToString, (ClientData)PIXELS_POSITIVE
};

Tk_CustomOption bltAnyDistanceOption =
{
    StringToDistance, DistanceToString, (ClientData)PIXELS_ANY
};

static int StringToCount _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec,
	int flags));
static char *CountToString _ANSI_ARGS_((ClientData, Tk_Window, char *, int,
	Tcl_FreeProc **));

Tk_CustomOption bltCountOption =
{
    StringToCount, CountToString, (ClientData)COUNT_NONNEGATIVE
};

Tk_CustomOption bltPositiveCountOption =
{
    StringToCount, CountToString, (ClientData)COUNT_POSITIVE
};

static int StringToDashes _ANSI_ARGS_((ClientData, Tcl_Interp *, Tk_Window,
	char *, char *, int));
static char *DashesToString _ANSI_ARGS_((ClientData, Tk_Window, char *, int,
	Tcl_FreeProc **));

Tk_CustomOption bltDashesOption =
{
    StringToDashes, DashesToString, (ClientData)0
};

static int StringToShadow _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	Tk_Window tkwin, char *string, char *widgRec, int offset));
static char *ShadowToString _ANSI_ARGS_((ClientData clientData, Tk_Window tkwin,
	char *widgRec, int offset, Tcl_FreeProc **freeProcPtr));

Tk_CustomOption bltShadowOption =
{
    StringToShadow, ShadowToString, (ClientData)0
};

static int StringToUid _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec,
	int flags));
static char *UidToString _ANSI_ARGS_((ClientData, Tk_Window, char *, int,
	Tcl_FreeProc **));

Tk_CustomOption bltUidOption =
{
    StringToUid, UidToString, (ClientData)0
};

static int StringToState _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec,
	int flags));
static char *StateToString _ANSI_ARGS_((ClientData, Tk_Window, char *, int,
	Tcl_FreeProc **));

Tk_CustomOption bltStateOption =
{
    StringToState, StateToString, (ClientData)0
};

static int StringToList _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	Tk_Window tkwin, char *string, char *widgRec, int flags));
static char *ListToString _ANSI_ARGS_((ClientData, Tk_Window, char *, int,
	Tcl_FreeProc **));

Tk_CustomOption bltListOption =
{
    StringToList, ListToString, (ClientData)0
};

static int StringToTile _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	Tk_Window tkwin, char *value, char *widgRec, int flags));
static char *TileToString _ANSI_ARGS_((ClientData clientData, Tk_Window tkwin,
	char *widgRec, int offset, Tcl_FreeProc **freeProcPtr));

Tk_CustomOption bltTileOption =
{
    StringToTile, TileToString, (ClientData)0
};

/*
 *----------------------------------------------------------------------
 *
 * Blt_NameOfFill --
 *
 *	Converts the integer representing the fill direction into a string.
 *
 *----------------------------------------------------------------------
 */
char *
Blt_NameOfFill(fill)
    int fill;
{
    switch (fill) {
    case FILL_X:
	return "x";
    case FILL_Y:
	return "y";
    case FILL_NONE:
	return "none";
    case FILL_BOTH:
	return "both";
    default:
	return "unknown value";
    }
}

/*
 *----------------------------------------------------------------------
 *
 * StringToFill --
 *
 *	Converts the fill style string into its numeric representation.
 *
 *	Valid style strings are:
 *
 *	  "none"   Use neither plane.
 * 	  "x"	   X-coordinate plane.
 *	  "y"	   Y-coordinate plane.
 *	  "both"   Use both coordinate planes.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToFill(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* Fill style string */
    char *widgRec;		/* Cubicle structure record */
    int offset;			/* Offset of style in record */
{
    int *fillPtr = (int *)(widgRec + offset);
    unsigned int length;
    char c;

    c = string[0];
    length = strlen(string);
    if ((c == 'n') && (strncmp(string, "none", length) == 0)) {
	*fillPtr = FILL_NONE;
    } else if ((c == 'x') && (strncmp(string, "x", length) == 0)) {
	*fillPtr = FILL_X;
    } else if ((c == 'y') && (strncmp(string, "y", length) == 0)) {
	*fillPtr = FILL_Y;
    } else if ((c == 'b') && (strncmp(string, "both", length) == 0)) {
	*fillPtr = FILL_BOTH;
    } else {
	Tcl_AppendResult(interp, "bad argument \"", string,
	    "\": should be \"none\", \"x\", \"y\", or \"both\"", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FillToString --
 *
 *	Returns the fill style string based upon the fill flags.
 *
 * Results:
 *	The fill style string is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
FillToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget structure record */
    int offset;			/* Offset of fill in widget record */
    Tcl_FreeProc **freeProcPtr;	/* Not used. */
{
    int fill = *(int *)(widgRec + offset);

    return Blt_NameOfFill(fill);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_StringToFlag --
 *
 *	Converts the fill style string into its numeric representation.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
int
Blt_StringToFlag(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Bit mask to be tested in status word */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* Fill style string */
    char *widgRec;		/* Cubicle structure record */
    int offset;			/* Offset of style in record */
{
    unsigned int mask = (unsigned int)clientData;	/* Bit to be tested */
    int *flagPtr = (int *)(widgRec + offset);
    int bool;

    if (Tcl_GetBoolean(interp, string, &bool) != TCL_OK) {
	return TCL_ERROR;
    }
    if (bool) {
	*flagPtr |= mask;
    } else {
	*flagPtr &= ~mask;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_FlagToString --
 *
 *	Returns the fill style string based upon the fill flags.
 *
 * Results:
 *	The fill style string is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
char *
Blt_FlagToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Bit mask to be test in status word */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget structure record */
    int offset;			/* Offset of fill in widget record */
    Tcl_FreeProc **freeProcPtr;	/* Not Used. */
{
    unsigned int mask = (unsigned int)clientData;	/* Bit to be tested */
    unsigned int bool = *(unsigned int *)(widgRec + offset);

    return (bool & mask) ? "1" : "0";
}


/*
 *----------------------------------------------------------------------
 *
 * Blt_GetPixels --
 *
 *	Like Tk_GetPixels, but checks for negative, zero.
 *
 * Results:
 *	A standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
int
Blt_GetPixels(interp, tkwin, string, check, valuePtr)
    Tcl_Interp *interp;
    Tk_Window tkwin;
    char *string;
    int check;			/* Can be PIXELS_POSITIVE, PIXELS_NONNEGATIVE,
				 * or PIXELS_ANY, */
    int *valuePtr;
{
    int length;

    if (Tk_GetPixels(interp, tkwin, string, &length) != TCL_OK) {
	return TCL_ERROR;
    }
    if (length >= SHRT_MAX) {
	Tcl_AppendResult(interp, "bad distance \"", string, "\": ",
	    "too big to represent", (char *)NULL);
	return TCL_ERROR;
    }
    switch (check) {
    case PIXELS_NONNEGATIVE:
	if (length < 0) {
	    Tcl_AppendResult(interp, "bad distance \"", string, "\": ",
		"can't be negative", (char *)NULL);
	    return TCL_ERROR;
	}
	break;
    case PIXELS_POSITIVE:
	if (length <= 0) {
	    Tcl_AppendResult(interp, "bad distance \"", string, "\": ",
		"must be positive", (char *)NULL);
	    return TCL_ERROR;
	}
	break;
    case PIXELS_ANY:
	break;
    }
    *valuePtr = length;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StringToDistance --
 *
 *	Like TK_CONFIG_PIXELS, but adds an extra check for negative
 *	values.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToDistance(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Indicated how to check distance */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window */
    char *string;		/* Pixel value string */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of pixel size in record */
{
    int *valuePtr = (int *)(widgRec + offset);
    return Blt_GetPixels(interp, tkwin, string, (int)clientData, valuePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DistanceToString --
 *
 *	Returns the string representing the positive pixel size.
 *
 * Results:
 *	The pixel size string is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
DistanceToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget structure record */
    int offset;			/* Offset in widget record */
    Tcl_FreeProc **freeProcPtr;	/* Not used. */
{
    int value = *(int *)(widgRec + offset);
    char *result;

    result = Blt_Strdup(Blt_Itoa(value));
    assert(result);
    *freeProcPtr = (Tcl_FreeProc *)Blt_Free;
    return result;
}

int
Blt_GetInt(interp, string, check, valuePtr)
    Tcl_Interp *interp;
    char *string;
    int check;			/* Can be COUNT_POSITIVE, COUNT_NONNEGATIVE,
				 * or COUNT_ANY, */
    int *valuePtr;
{
    int count;

    if (Tcl_GetInt(interp, string, &count) != TCL_OK) {
	return TCL_ERROR;
    }
    switch (check) {
    case COUNT_NONNEGATIVE:
	if (count < 0) {
	    Tcl_AppendResult(interp, "bad value \"", string, "\": ",
		"can't be negative", (char *)NULL);
	    return TCL_ERROR;
	}
	break;
    case COUNT_POSITIVE:
	if (count <= 0) {
	    Tcl_AppendResult(interp, "bad value \"", string, "\": ",
		"must be positive", (char *)NULL);
	    return TCL_ERROR;
	}
	break;
    case COUNT_ANY:
	break;
    }
    *valuePtr = count;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StringToCount --
 *
 *	Like TK_CONFIG_PIXELS, but adds an extra check for negative
 *	values.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToCount(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Indicated how to check distance */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* Pixel value string */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of pixel size in record */
{
    int *valuePtr = (int *)(widgRec + offset);
    return Blt_GetInt(interp, string, (int)clientData, valuePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * CountToString --
 *
 *	Returns the string representing the positive pixel size.
 *
 * Results:
 *	The pixel size string is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
CountToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget structure record */
    int offset;			/* Offset in widget record */
    Tcl_FreeProc **freeProcPtr;	/* Not used. */
{
    int value = *(int *)(widgRec + offset);
    char *result;

    result = Blt_Strdup(Blt_Itoa(value));
    assert(result);
    *freeProcPtr = (Tcl_FreeProc *)Blt_Free;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * StringToPad --
 *
 *	Convert a string into two pad values.  The string may be in one of
 *	the following forms:
 *
 *	    n    - n is a non-negative integer. This sets both
 *		   pad values to n.
 *	  {n m}  - both n and m are non-negative integers. side1
 *		   is set to n, side2 is set to m.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side Effects:
 *	The padding structure passed is updated with the new values.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToPad(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window */
    char *string;		/* Pixel value string */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of pad in widget */
{
    Blt_Pad *padPtr = (Blt_Pad *)(widgRec + offset);
    int nElem;
    int pad, result;
    char **padArr;

    if (Tcl_SplitList(interp, string, &nElem, &padArr) != TCL_OK) {
	return TCL_ERROR;
    }
    result = TCL_ERROR;
    if ((nElem < 1) || (nElem > 2)) {
	Tcl_AppendResult(interp, "wrong # elements in padding list",
	    (char *)NULL);
	goto error;
    }
    if (Blt_GetPixels(interp, tkwin, padArr[0], PIXELS_NONNEGATIVE, &pad)
	!= TCL_OK) {
	goto error;
    }
    padPtr->side1 = pad;
    if ((nElem > 1) &&
	(Blt_GetPixels(interp, tkwin, padArr[1], PIXELS_NONNEGATIVE, &pad)
	    != TCL_OK)) {
	goto error;
    }
    padPtr->side2 = pad;
    result = TCL_OK;

  error:
    Blt_Free(padArr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * PadToString --
 *
 *	Converts the two pad values into a Tcl list.  Each pad has two
 *	pixel values.  For vertical pads, they represent the top and bottom
 *	margins.  For horizontal pads, they're the left and right margins.
 *	All pad values are non-negative integers.
 *
 * Results:
 *	The padding list is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
PadToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Structure record */
    int offset;			/* Offset of pad in record */
    Tcl_FreeProc **freeProcPtr;	/* Not used. */
{
    Blt_Pad *padPtr = (Blt_Pad *)(widgRec + offset);
    char *result;
    char string[200];

    sprintf(string, "%d %d", padPtr->side1, padPtr->side2);
    result = Blt_Strdup(string);
    if (result == NULL) {
	return "out of memory";
    }
    *freeProcPtr = (Tcl_FreeProc *)Blt_Free;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * StringToShadow --
 *
 *	Convert a string into two pad values.  The string may be in one of
 *	the following forms:
 *
 *	    n    - n is a non-negative integer. This sets both
 *		   pad values to n.
 *	  {n m}  - both n and m are non-negative integers. side1
 *		   is set to n, side2 is set to m.
 *
 * Results:
 *	If the string is successfully converted, TCL_OK is returned.
 *	Otherwise, TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side Effects:
 *	The padding structure passed is updated with the new values.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToShadow(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window */
    char *string;		/* Pixel value string */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of pad in widget */
{
    Shadow *shadowPtr = (Shadow *) (widgRec + offset);
    XColor *colorPtr;
    int dropOffset;

    colorPtr = NULL;
    dropOffset = 0;
    if ((string != NULL) && (string[0] != '\0')) {
	int nElem;
	char **elemArr;

	if (Tcl_SplitList(interp, string, &nElem, &elemArr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((nElem < 1) || (nElem > 2)) {
	    Tcl_AppendResult(interp, "wrong # elements in drop shadow value",
		(char *)NULL);
	    Blt_Free(elemArr);
	    return TCL_ERROR;
	}
	colorPtr = Tk_GetColor(interp, tkwin, Tk_GetUid(elemArr[0]));
	if (colorPtr == NULL) {
	    Blt_Free(elemArr);
	    return TCL_ERROR;
	}
	dropOffset = 1;
	if (nElem == 2) {
	    if (Blt_GetPixels(interp, tkwin, elemArr[1], PIXELS_NONNEGATIVE,
		    &dropOffset) != TCL_OK) {
		Tk_FreeColor(colorPtr);
		Blt_Free(elemArr);
		return TCL_ERROR;
	    }
	}
	Blt_Free(elemArr);
    }
    if (shadowPtr->color != NULL) {
	Tk_FreeColor(shadowPtr->color);
    }
    shadowPtr->color = colorPtr;
    shadowPtr->offset = dropOffset;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ShadowToString --
 *
 *	Converts the two pad values into a Tcl list.  Each pad has two
 *	pixel values.  For vertical pads, they represent the top and bottom
 *	margins.  For horizontal pads, they're the left and right margins.
 *	All pad values are non-negative integers.
 *
 * Results:
 *	The padding list is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
ShadowToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Structure record */
    int offset;			/* Offset of pad in record */
    Tcl_FreeProc **freeProcPtr;	/* Not used. */
{
    Shadow *shadowPtr = (Shadow *) (widgRec + offset);
    char *result;

    result = "";
    if (shadowPtr->color != NULL) {
	char string[200];

	sprintf(string, "%s %d", Tk_NameOfColor(shadowPtr->color),
	    shadowPtr->offset);
	result = Blt_Strdup(string);
	*freeProcPtr = (Tcl_FreeProc *)Blt_Free;
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * GetDashes --
 *
 *	Converts a Tcl list of dash values into a dash list ready for
 *	use with XSetDashes.
 *
 * 	A valid list dash values can have zero through 11 elements
 *	(PostScript limit).  Values must be between 1 and 255. Although
 *	a list of 0 (like the empty string) means no dashes.
 *
 * Results:
 *	A standard Tcl result. If the list represented a valid dash
 *	list TCL_OK is returned and *dashesPtr* will contain the
 *	valid dash list. Otherwise, TCL_ERROR is returned and
 *	interp->result will contain an error message.
 *
 *
 *----------------------------------------------------------------------
 */
static int
GetDashes(interp, string, dashesPtr)
    Tcl_Interp *interp;
    char *string;
    Blt_Dashes *dashesPtr;
{
    if ((string == NULL) || (*string == '\0')) {
	dashesPtr->values[0] = 0;
    } else if (strcmp(string, "dash") == 0) {	/* 5 2 */
	dashesPtr->values[0] = 5;
	dashesPtr->values[1] = 2;
	dashesPtr->values[2] = 0;
    } else if (strcmp(string, "dot") == 0) {	/* 1 */
	dashesPtr->values[0] = 1;
	dashesPtr->values[1] = 0;
    } else if (strcmp(string, "dashdot") == 0) {	/* 2 4 2 */
	dashesPtr->values[0] = 2;
	dashesPtr->values[1] = 4;
	dashesPtr->values[2] = 2;
	dashesPtr->values[3] = 0;
    } else if (strcmp(string, "dashdotdot") == 0) {	/* 2 4 2 2 */
	dashesPtr->values[0] = 2;
	dashesPtr->values[1] = 4;
	dashesPtr->values[2] = 2;
	dashesPtr->values[3] = 2;
	dashesPtr->values[4] = 0;
    } else {
	int nValues;
	char **strArr;
	long int value;
	register int i;

	if (Tcl_SplitList(interp, string, &nValues, &strArr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (nValues > 11) {	/* This is the postscript limit */
	    Tcl_AppendResult(interp, "too many values in dash list \"", 
			     string, "\"", (char *)NULL);
	    Blt_Free(strArr);
	    return TCL_ERROR;
	}
	for (i = 0; i < nValues; i++) {
	    if (Tcl_ExprLong(interp, strArr[i], &value) != TCL_OK) {
		Blt_Free(strArr);
		return TCL_ERROR;
	    }
	    /*
	     * Backward compatibility:
	     * Allow list of 0 to turn off dashes
	     */
	    if ((value == 0) && (nValues == 1)) {
		break;
	    }
	    if ((value < 1) || (value > 255)) {
		Tcl_AppendResult(interp, "dash value \"", strArr[i],
		    "\" is out of range", (char *)NULL);
		Blt_Free(strArr);
		return TCL_ERROR;
	    }
	    dashesPtr->values[i] = (unsigned char)value;
	}
	/* Make sure the array ends with a NUL byte  */
	dashesPtr->values[i] = 0;
	Blt_Free(strArr);
    }
    return TCL_OK;

}

/*
 *----------------------------------------------------------------------
 *
 * StringToDashes --
 *
 *	Convert the list of dash values into a dashes array.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 * Side Effects:
 *	The Dashes structure is updated with the new dash list.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToDashes(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* New dash value list */
    char *widgRec;		/* Widget record */
    int offset;			/* offset to Dashes structure */
{
    Blt_Dashes *dashesPtr = (Blt_Dashes *)(widgRec + offset);

    return GetDashes(interp, string, dashesPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DashesToString --
 *
 *	Convert the dashes array into a list of values.
 *
 * Results:
 *	The string representing the dashes returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
DashesToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget record */
    int offset;			/* offset of Dashes in record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation scheme to use */
{
    Blt_Dashes *dashesPtr = (Blt_Dashes *)(widgRec + offset);
    Tcl_DString dString;
    unsigned char *p;
    char *result;

    if (dashesPtr->values[0] == 0) {
	return "";
    }
    Tcl_DStringInit(&dString);
    for (p = dashesPtr->values; *p != 0; p++) {
	Tcl_DStringAppendElement(&dString, Blt_Itoa(*p));
    }
    result = Tcl_DStringValue(&dString);
    if (result == dString.staticSpace) {
	result = Blt_Strdup(result);
    }
    *freeProcPtr = (Tcl_FreeProc *)Blt_Free;
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * StringToUid --
 *
 *	Converts the string to a BLT Uid. Blt Uid's are hashed, reference
 *	counted strings.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToUid(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* Fill style string */
    char *widgRec;		/* Cubicle structure record */
    int offset;			/* Offset of style in record */
{
    Blt_Uid *uidPtr = (Blt_Uid *)(widgRec + offset);
    Blt_Uid newId;

    newId = NULL;
    if ((string != NULL) && (*string != '\0')) {
	newId = Blt_GetUid(string);
    }
    if (*uidPtr != NULL) {
	Blt_FreeUid(*uidPtr);
    }
    *uidPtr = newId;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UidToString --
 *
 *	Returns the fill style string based upon the fill flags.
 *
 * Results:
 *	The fill style string is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
UidToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget structure record */
    int offset;			/* Offset of fill in widget record */
    Tcl_FreeProc **freeProcPtr;	/* Not used. */
{
    Blt_Uid uid = *(Blt_Uid *)(widgRec + offset);

    return (uid == NULL) ? "" : uid;
}

/*
 *----------------------------------------------------------------------
 *
 * StringToState --
 *
 *	Converts the string to a state value. Valid states are
 *	disabled, normal.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToState(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* String representation of option value */
    char *widgRec;		/* Widget structure record */
    int offset;			/* Offset of field in record */
{
    int *statePtr = (int *)(widgRec + offset);

    if (strcmp(string, "normal") == 0) {
	*statePtr = STATE_NORMAL;
    } else if (strcmp(string, "disabled") == 0) {
	*statePtr = STATE_DISABLED;
    } else if (strcmp(string, "active") == 0) {
	*statePtr = STATE_ACTIVE;
    } else {
	Tcl_AppendResult(interp, "bad state \"", string,
	    "\": should be normal, active, or disabled", (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * StateToString --
 *
 *	Returns the string representation of the state configuration field
 *
 * Results:
 *	The string is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
StateToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget structure record */
    int offset;			/* Offset of fill in widget record */
    Tcl_FreeProc **freeProcPtr;	/* Not used. */
{
    int state = *(int *)(widgRec + offset);

    switch (state) {
    case STATE_ACTIVE:
	return "active";
    case STATE_DISABLED:
	return "disabled";
    case STATE_NORMAL:
	return "normal";
    default:
	return "???";
    }
}

/*
 *----------------------------------------------------------------------
 *
 * StringToList --
 *
 *	Converts the string to a list.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToList(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* String representation of option value */
    char *widgRec;		/* Widget structure record */
    int offset;			/* Offset of field in record */
{
    char ***listPtr = (char ***)(widgRec + offset);
    char **elemArr;
    int nElem;

    if (*listPtr != NULL) {
	Blt_Free(*listPtr);
	*listPtr = NULL;
    }
    if ((string == NULL) || (*string == '\0')) {
	return TCL_OK;
    }
    if (Tcl_SplitList(interp, string, &nElem, &elemArr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (nElem > 0) {
	*listPtr = elemArr;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ListToString --
 *
 *	Returns the string representation of the state configuration field
 *
 * Results:
 *	The string is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
ListToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget structure record. */
    int offset;			/* Offset of fill in widget record. */
    Tcl_FreeProc **freeProcPtr;	/* Not used. */
{
    char **list = *(char ***)(widgRec + offset);
    register char **p;
    char *result;
    Tcl_DString dString;

    if (list == NULL) {
	return "";
    }
    Tcl_DStringInit(&dString);
    for (p = list; *p != NULL; p++) {
	Tcl_DStringAppendElement(&dString, *p);
    }
    result = Tcl_DStringValue(&dString);
    if (result == dString.staticSpace) {
	result = Blt_Strdup(result);
    }
    Tcl_DStringFree(&dString);
    *freeProcPtr = (Tcl_FreeProc *)Blt_Free;
    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * StringToTile --
 *
 *	Converts the name of an image into a tile.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToTile(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Window on same display as tile */
    char *string;		/* Name of image */
    char *widgRec;		/* Widget structure record */
    int offset;			/* Offset of tile in record */
{
    Blt_Tile *tilePtr = (Blt_Tile *)(widgRec + offset);
    Blt_Tile tile, oldTile;

    oldTile = *tilePtr;
    tile = NULL;
    if ((string != NULL) && (*string != '\0')) {
	if (Blt_GetTile(interp, tkwin, string, &tile) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    /* Don't delete the information for the old tile, until we know
     * that we successfully allocated a new one. */
    if (oldTile != NULL) {
	Blt_FreeTile(oldTile);
    }
    *tilePtr = tile;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TileToString --
 *
 *	Returns the name of the tile.
 *
 * Results:
 *	The name of the tile is returned.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
TileToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget structure record */
    int offset;			/* Offset of tile in record */
    Tcl_FreeProc **freeProcPtr;	/* Not used. */
{
    Blt_Tile tile = *(Blt_Tile *)(widgRec + offset);

    return Blt_NameOfTile(tile);
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ConfigModified --
 *
 *      Given the configuration specifications and one or more option
 *	patterns (terminated by a NULL), indicate if any of the matching
 *	configuration options has been reset.
 *
 * Results:
 *      Returns 1 if one of the options has changed, 0 otherwise.
 *
 *----------------------------------------------------------------------
 */
int Blt_ConfigModified
TCL_VARARGS_DEF(Tk_ConfigSpec *, arg1)
{
    va_list argList;
    Tk_ConfigSpec *specs;
    register Tk_ConfigSpec *specPtr;
    register char *option;

    specs = TCL_VARARGS_START(Tk_ConfigSpec *, arg1, argList);
    while ((option = va_arg(argList, char *)) != NULL) {
	for (specPtr = specs; specPtr->type != TK_CONFIG_END; specPtr++) {
	    if ((Tcl_StringMatch(specPtr->argvName, option)) &&
		(specPtr->specFlags & TK_CONFIG_OPTION_SPECIFIED)) {
		va_end(argList);
		return 1;
	    }
	}
    }
    va_end(argList);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_ConfigureWidgetComponent --
 *
 *	Configures a component of a widget.  This is useful for
 *	widgets that have multiple components which aren't uniquely
 *	identified by a Tk_Window. It allows us, for example, set
 *	resources for axes of the graph widget. The graph really has
 *	only one window, but its convenient to specify components in a
 *	hierarchy of options.
 *
 *		*graph.x.logScale yes
 *		*graph.Axis.logScale yes
 *		*graph.temperature.scaleSymbols yes
 *		*graph.Element.scaleSymbols yes
 *
 *	This is really a hack to work around the limitations of the Tk
 *	resource database.  It creates a temporary window, needed to
 *	call Tk_ConfigureWidget, using the name of the component.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side Effects:
 *	A temporary window is created merely to pass to Tk_ConfigureWidget.
 *
 *----------------------------------------------------------------------
 */
int
Blt_ConfigureWidgetComponent(interp, parent, resName, className, specsPtr, 
	argc, argv, widgRec, flags)
    Tcl_Interp *interp;
    Tk_Window parent;		/* Window to associate with component */
    char resName[];		/* Name of component */
    char className[];
    Tk_ConfigSpec *specsPtr;
    int argc;
    char *argv[];
    char *widgRec;
    int flags;
{
    Tk_Window tkwin;
    int result;
    char *tempName;
    int isTemporary = FALSE;

    tempName = Blt_Strdup(resName);

    /* Window name can't start with an upper case letter */
    tempName[0] = tolower(resName[0]);

    /*
     * Create component if a child window by the component's name
     * doesn't already exist.
     */
    tkwin = Blt_FindChild(parent, tempName);
    if (tkwin == NULL) {
	tkwin = Tk_CreateWindow(interp, parent, tempName, (char *)NULL);
	isTemporary = TRUE;
    }
    if (tkwin == NULL) {
	Tcl_AppendResult(interp, "can't find window in \"", 
		 Tk_PathName(parent), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    assert(Tk_Depth(tkwin) == Tk_Depth(parent));
    Blt_Free(tempName);

    Tk_SetClass(tkwin, className);
    result = Tk_ConfigureWidget(interp, tkwin, specsPtr, argc, argv, widgRec,
	flags);
    if (isTemporary) {
	Tk_DestroyWindow(tkwin);
    }
    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Blt_StringToEnum --
 *
 *	Converts the string into its enumerated type.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
int
Blt_StringToEnum(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Vectors of valid strings. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* String to match. */
    char *widgRec;		/* Widget record. */
    int offset;			/* Offset of field in record */
{
    int *enumPtr = (int *)(widgRec + offset);
    char c;
    register char **p;
    register int i;
    int count;

    c = string[0];
    count = 0;
    for (p = (char **)clientData; *p != NULL; p++) {
	if ((c == p[0][0]) && (strcmp(string, *p) == 0)) {
	    *enumPtr = count;
	    return TCL_OK;
	}
	count++;
    }
    *enumPtr = -1;

    Tcl_AppendResult(interp, "bad value \"", string, "\": should be ",
		(char *)NULL);
    p = (char **)clientData; 
    if (count > 0) {
	Tcl_AppendResult(interp, p[0], (char *)NULL);
    }
    for (i = 1; i < (count - 1); i++) {
	Tcl_AppendResult(interp, " ", p[i], ", ", (char *)NULL);
    }
    if (count > 1) {
	Tcl_AppendResult(interp, " or ", p[count - 1], ".", (char *)NULL);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_EnumToString --
 *
 *	Returns the string associated with the enumerated type.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
char *
Blt_EnumToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* List of strings. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of field in widget record */
    Tcl_FreeProc **freeProcPtr;	/* Not used. */
{
    int value = *(int *)(widgRec + offset);
    char **p;
    int count;

    count = 0;
    for (p = (char **)clientData; *p != NULL; p++) {
	count++;
    }
    if ((value >= count) || (value < 0)) {
	return "unknown value";
    }
    p = (char **)clientData;
    return p[value];
}

