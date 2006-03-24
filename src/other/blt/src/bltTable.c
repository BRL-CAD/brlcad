/*
 * bltTable.c --
 *
 *	This module implements a table-based geometry manager
 *	for the BLT toolkit.
 *
 * Copyright 1993-1998 Lucent Technologies, Inc.
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
 *
 *	The "table" geometry manager was created by George Howlett.
 */

/*
 * To do:
 *
 * 3) No way to detect if widget is already a container of another
 *    geometry manager.  This one is especially bad with toplevel
 *    widgets, causing the window manager to lock-up trying to handle the
 *    myriads of resize requests.
 *
 *    Note: This problem continues in Tk 8.x.  It's possible for a widget
 *	    to be a container for two different geometry managers.  Each manager
 *	    will set its own requested geometry for the container widget. The
 *	    winner sets the geometry last (sometimes ad infinitum).
 *
 * 7) Relative sizing of partitions?
 *
 */

#include "bltInt.h"

#include "bltTable.h"
#include <ctype.h>

#define TABLE_THREAD_KEY	"BLT Table Data"
#define TABLE_DEF_PAD		0

/*
 * Default values for widget attributes.
 */
#define DEF_TABLE_ANCHOR	"center"
#define DEF_TABLE_COLUMNS 	"0"
#define DEF_TABLE_FILL		"none"
#define DEF_TABLE_PAD		"0"
#define DEF_TABLE_PROPAGATE 	"1"
#define DEF_TABLE_RESIZE	"both"
#define DEF_TABLE_ROWS		"0"
#define DEF_TABLE_SPAN		"1"
#define DEF_TABLE_CONTROL	"normal"
#define DEF_TABLE_WEIGHT	"1.0"

#define ENTRY_DEF_PAD		0
#define ENTRY_DEF_ANCHOR	TK_ANCHOR_CENTER
#define ENTRY_DEF_FILL		FILL_NONE
#define ENTRY_DEF_SPAN		1
#define ENTRY_DEF_CONTROL	CONTROL_NORMAL
#define ENTRY_DEF_IPAD		0

#define ROWCOL_DEF_RESIZE	(RESIZE_BOTH | RESIZE_VIRGIN)
#define ROWCOL_DEF_PAD		0
#define ROWCOL_DEF_WEIGHT	1.0

static Blt_Uid rowUid, columnUid;

static void WidgetGeometryProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin));
static void WidgetCustodyProc _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin));

static Tk_GeomMgr tableMgrInfo =
{
    "table",			/* Name of geometry manager used by winfo */
    WidgetGeometryProc,		/* Procedure to for new geometry requests */
    WidgetCustodyProc,		/* Procedure when widget is taken away */
};

static int StringToLimits _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec,
	int offset));

static char *LimitsToString _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,
	Tcl_FreeProc **freeProcPtr));

static Tk_CustomOption limitsOption =
{
    StringToLimits, LimitsToString, (ClientData)0
};

static int StringToResize _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec,
	int offset));
static char *ResizeToString _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,
	Tcl_FreeProc **freeProcPtr));

static Tk_CustomOption resizeOption =
{
    StringToResize, ResizeToString, (ClientData)0
};

static int StringToControl _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, Tk_Window tkwin, char *string, char *widgRec,
	int offset));
static char *ControlToString _ANSI_ARGS_((ClientData clientData,
	Tk_Window tkwin, char *widgRec, int offset,
	Tcl_FreeProc **freeProcPtr));

static Tk_CustomOption controlOption =
{
    StringToControl, ControlToString, (ClientData)0
};

extern Tk_CustomOption bltPadOption;
extern Tk_CustomOption bltFillOption;
extern Tk_CustomOption bltDistanceOption;

static Tk_ConfigSpec rowConfigSpecs[] =
{
    {TK_CONFIG_CUSTOM, "-height", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(RowColumn, reqSize), TK_CONFIG_NULL_OK,
	&limitsOption},
    {TK_CONFIG_CUSTOM, "-pady", (char *)NULL, (char *)NULL,
	DEF_TABLE_PAD, Tk_Offset(RowColumn, pad),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-resize", (char *)NULL, (char *)NULL,
	DEF_TABLE_RESIZE, Tk_Offset(RowColumn, resize),
	TK_CONFIG_DONT_SET_DEFAULT, &resizeOption},
    {TK_CONFIG_DOUBLE, "-weight", (char *)NULL, (char *)NULL,
	DEF_TABLE_WEIGHT, Tk_Offset(RowColumn, weight),
	TK_CONFIG_NULL_OK | TK_CONFIG_DONT_SET_DEFAULT, &limitsOption},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

static Tk_ConfigSpec columnConfigSpecs[] =
{
    {TK_CONFIG_CUSTOM, "-padx", (char *)NULL, (char *)NULL,
	DEF_TABLE_PAD, Tk_Offset(RowColumn, pad),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-resize", (char *)NULL, (char *)NULL,
	DEF_TABLE_RESIZE, Tk_Offset(RowColumn, resize),
	TK_CONFIG_DONT_SET_DEFAULT, &resizeOption},
    {TK_CONFIG_DOUBLE, "-weight", (char *)NULL, (char *)NULL,
	DEF_TABLE_WEIGHT, Tk_Offset(RowColumn, weight),
	TK_CONFIG_NULL_OK | TK_CONFIG_DONT_SET_DEFAULT, &limitsOption},
    {TK_CONFIG_CUSTOM, "-width", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(RowColumn, reqSize), TK_CONFIG_NULL_OK,
	&limitsOption},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};


static Tk_ConfigSpec entryConfigSpecs[] =
{
    {TK_CONFIG_ANCHOR, "-anchor", (char *)NULL, (char *)NULL,
	DEF_TABLE_ANCHOR, Tk_Offset(Entry, anchor),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_INT, "-columnspan", "columnSpan", (char *)NULL,
	DEF_TABLE_SPAN, Tk_Offset(Entry, column.span),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-columncontrol", "columnControl", (char *)NULL,
	DEF_TABLE_CONTROL, Tk_Offset(Entry, column.control),
	TK_CONFIG_DONT_SET_DEFAULT, &controlOption},
    {TK_CONFIG_SYNONYM, "-cspan", "columnSpan", (char *)NULL,
	(char *)NULL, Tk_Offset(Entry, column.span), 0},
    {TK_CONFIG_SYNONYM, "-ccontrol", "columnControl", (char *)NULL,
	(char *)NULL, Tk_Offset(Entry, column.control), 0},
    {TK_CONFIG_CUSTOM, "-fill", (char *)NULL, (char *)NULL,
	DEF_TABLE_FILL, Tk_Offset(Entry, fill),
	TK_CONFIG_DONT_SET_DEFAULT, &bltFillOption},
    {TK_CONFIG_SYNONYM, "-height", "reqHeight", (char *)NULL,
	(char *)NULL, Tk_Offset(Entry, reqHeight), 0},
    {TK_CONFIG_CUSTOM, "-padx", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(Entry, padX), 0, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-pady", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(Entry, padY), 0, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-ipadx", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(Entry, ipadX), 0, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-ipady", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(Entry, ipadY), 0, &bltDistanceOption},
    {TK_CONFIG_CUSTOM, "-reqheight", "reqHeight", (char *)NULL,
	(char *)NULL, Tk_Offset(Entry, reqHeight), TK_CONFIG_NULL_OK,
	&limitsOption},
    {TK_CONFIG_CUSTOM, "-reqwidth", "reqWidth", (char *)NULL,
	(char *)NULL, Tk_Offset(Entry, reqWidth), TK_CONFIG_NULL_OK,
	&limitsOption},
    {TK_CONFIG_INT, "-rowspan", "rowSpan", (char *)NULL,
	DEF_TABLE_SPAN, Tk_Offset(Entry, row.span),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-rowcontrol", "rowControl", (char *)NULL,
	DEF_TABLE_CONTROL, Tk_Offset(Entry, row.control),
	TK_CONFIG_DONT_SET_DEFAULT, &controlOption},
    {TK_CONFIG_SYNONYM, "-rspan", "rowSpan", (char *)NULL,
	(char *)NULL, Tk_Offset(Entry, row.span), 0},
    {TK_CONFIG_SYNONYM, "-rcontrol", "rowControl", (char *)NULL,
	(char *)NULL, Tk_Offset(Entry, row.control), 0},
    {TK_CONFIG_SYNONYM, "-width", "reqWidth", (char *)NULL,
	(char *)NULL, Tk_Offset(Entry, reqWidth), 0},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};


static Tk_ConfigSpec tableConfigSpecs[] =
{
    {TK_CONFIG_CUSTOM, "-padx", (char *)NULL, (char *)NULL,
	DEF_TABLE_PAD, Tk_Offset(Table, padX),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_CUSTOM, "-pady", (char *)NULL, (char *)NULL,
	DEF_TABLE_PAD, Tk_Offset(Table, padY),
	TK_CONFIG_DONT_SET_DEFAULT, &bltPadOption},
    {TK_CONFIG_BOOLEAN, "-propagate", (char *)NULL, (char *)NULL,
	DEF_TABLE_PROPAGATE, Tk_Offset(Table, propagate),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-reqheight", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(Table, reqHeight), TK_CONFIG_NULL_OK,
	&limitsOption},
    {TK_CONFIG_CUSTOM, "-reqwidth", (char *)NULL, (char *)NULL,
	(char *)NULL, Tk_Offset(Table, reqWidth), TK_CONFIG_NULL_OK,
	&limitsOption},
    {TK_CONFIG_END, NULL, NULL, NULL, NULL, 0, 0}
};

/*
 * Forward declarations
 */
static void ArrangeTable _ANSI_ARGS_((ClientData clientData));
static void DestroyTable _ANSI_ARGS_((DestroyData dataPtr));
static void DestroyEntry _ANSI_ARGS_((Entry * entryPtr));
static void TableEventProc _ANSI_ARGS_((ClientData clientData,
	XEvent *eventPtr));
static void BinEntry _ANSI_ARGS_((Table *tablePtr, Entry * entryPtr));
static RowColumn *InitSpan _ANSI_ARGS_((PartitionInfo * infoPtr, int start,
	int span));

static EntrySearchProc FindEntry;
static Tcl_CmdProc TableCmd;
static Tcl_InterpDeleteProc TableInterpDeleteProc;
static Tk_EventProc WidgetEventProc;

/*
 * ----------------------------------------------------------------------------
 *
 * StringToLimits --
 *
 *	Converts the list of elements into zero or more pixel values which
 *	determine the range of pixel values possible.  An element can be in
 *	any form accepted by Tk_GetPixels. The list has a different meaning
 *	based upon the number of elements.
 *
 *	    # of elements:
 *
 *	    0 - the limits are reset to the defaults.
 *	    1 - the minimum and maximum values are set to this
 *		value, freezing the range at a single value.
 *	    2 - first element is the minimum, the second is the
 *		maximum.
 *	    3 - first element is the minimum, the second is the
 *		maximum, and the third is the nominal value.
 *
 *	Any element may be the empty string which indicates the default.
 *
 * Results:
 *	The return value is a standard Tcl result.  The min and max fields
 *	of the range are set.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToLimits(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Widget of table */
    char *string;		/* New width list */
    char *widgRec;		/* Widget record */
    int offset;			/* Offset of limits */
{
    Limits *limitsPtr = (Limits *)(widgRec + offset);
    char **elemArr;
    int nElem;
    int limArr[3];
    Tk_Window winArr[3];
    int flags;

    elemArr = NULL;
    nElem = 0;

    /* Initialize limits to default values */
    limArr[2] = LIMITS_NOM;
    limArr[1] = LIMITS_MAX;
    limArr[0] = LIMITS_MIN;
    winArr[0] = winArr[1] = winArr[2] = NULL;
    flags = 0;

    if (string != NULL) {
	int size;
	int i;

	if (Tcl_SplitList(interp, string, &nElem, &elemArr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (nElem > 3) {
	    Tcl_AppendResult(interp, "wrong # limits \"", string, "\"",
		(char *)NULL);
	    goto error;
	}
	for (i = 0; i < nElem; i++) {
	    if (elemArr[i][0] == '\0') {
		continue;	/* Empty string: use default value */
	    }
	    flags |= (LIMITS_SET_BIT << i);
	    if ((elemArr[i][0] == '.') &&
		((elemArr[i][1] == '\0') || isalpha(UCHAR(elemArr[i][1])))) {
		Tk_Window tkwin2;

		/* Widget specified: save pointer to widget */
		tkwin2 = Tk_NameToWindow(interp, elemArr[i], tkwin);
		if (tkwin2 == NULL) {
		    goto error;
		}
		winArr[i] = tkwin2;
	    } else {
		if (Tk_GetPixels(interp, tkwin, elemArr[i], &size) != TCL_OK) {
		    goto error;
		}
		if ((size < LIMITS_MIN) || (size > LIMITS_MAX)) {
		    Tcl_AppendResult(interp, "bad limits \"", string, "\"",
			(char *)NULL);
		    goto error;
		}
		limArr[i] = size;
	    }
	}
	Blt_Free(elemArr);
    }
    /*
    * Check the limits specified.  We can't check the requested
    * size of widgets.
    */
    switch (nElem) {
    case 1:
	flags |= (LIMITS_SET_MIN | LIMITS_SET_MAX);
	if (winArr[0] == NULL) {
	    limArr[1] = limArr[0];	/* Set minimum and maximum to value */
	} else {
	    winArr[1] = winArr[0];
	}
	break;

    case 2:
	if ((winArr[0] == NULL) && (winArr[1] == NULL) &&
	    (limArr[1] < limArr[0])) {
	    Tcl_AppendResult(interp, "bad range \"", string,
		"\": min > max", (char *)NULL);
	    return TCL_ERROR;	/* Minimum is greater than maximum */
	}
	break;

    case 3:
	if ((winArr[0] == NULL) && (winArr[1] == NULL)) {
	    if (limArr[1] < limArr[0]) {
		Tcl_AppendResult(interp, "bad range \"", string,
		    "\": min > max", (char *)NULL);
		return TCL_ERROR;	/* Minimum is greater than maximum */
	    }
	    if ((winArr[2] == NULL) &&
		((limArr[2] < limArr[0]) || (limArr[2] > limArr[1]))) {
		Tcl_AppendResult(interp, "nominal value \"", string,
		    "\" out of range", (char *)NULL);
		return TCL_ERROR;	/* Nominal is outside of range defined
					 * by minimum and maximum */
	    }
	}
	break;
    }
    limitsPtr->min = limArr[0];
    limitsPtr->max = limArr[1];
    limitsPtr->nom = limArr[2];
    limitsPtr->wMin = winArr[0];
    limitsPtr->wMax = winArr[1];
    limitsPtr->wNom = winArr[2];
    limitsPtr->flags = flags;
    return TCL_OK;
  error:
    Blt_Free(elemArr);
    return TCL_ERROR;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ResetLimits --
 *
 *	Resets the limits to their default values.
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------------
 */
INLINE static void
ResetLimits(limitsPtr)
    Limits *limitsPtr;		/* Limits to be imposed on the value */
{
    limitsPtr->flags = 0;
    limitsPtr->min = LIMITS_MIN;
    limitsPtr->max = LIMITS_MAX;
    limitsPtr->nom = LIMITS_NOM;
    limitsPtr->wNom = limitsPtr->wMax = limitsPtr->wMin = NULL;
}

/*
 * ----------------------------------------------------------------------------
 *
 * GetBoundedWidth --
 *
 *	Bounds a given width value to the limits described in the limit
 *	structure.  The initial starting value may be overridden by the
 *	nominal value in the limits.
 *
 * Results:
 *	Returns the constrained value.
 *
 * ----------------------------------------------------------------------------
 */
static int
GetBoundedWidth(width, limitsPtr)
    int width;			/* Initial value to be constrained */
    Limits *limitsPtr;		/* Limits to be imposed on the value */
{
    /*
     * Check widgets for requested width values;
     */
    if (limitsPtr->wMin != NULL) {
	limitsPtr->min = Tk_ReqWidth(limitsPtr->wMin);
    }
    if (limitsPtr->wMax != NULL) {
	limitsPtr->max = Tk_ReqWidth(limitsPtr->wMax);
    }
    if (limitsPtr->wNom != NULL) {
	limitsPtr->nom = Tk_ReqWidth(limitsPtr->wNom);
    }
    if (limitsPtr->flags & LIMITS_SET_NOM) {
	width = limitsPtr->nom;	/* Override initial value */
    }
    if (width < limitsPtr->min) {
	width = limitsPtr->min;	/* Bounded by minimum value */
    } else if (width > limitsPtr->max) {
	width = limitsPtr->max;	/* Bounded by maximum value */
    }
    return width;
}

/*
 * ----------------------------------------------------------------------------
 *
 * GetBoundedHeight --
 *
 *	Bounds a given value to the limits described in the limit
 *	structure.  The initial starting value may be overridden by the
 *	nominal value in the limits.
 *
 * Results:
 *	Returns the constrained value.
 *
 * ----------------------------------------------------------------------------
 */
static int
GetBoundedHeight(height, limitsPtr)
    int height;			/* Initial value to be constrained */
    Limits *limitsPtr;		/* Limits to be imposed on the value */
{
    /*
     * Check widgets for requested height values;
     */
    if (limitsPtr->wMin != NULL) {
	limitsPtr->min = Tk_ReqHeight(limitsPtr->wMin);
    }
    if (limitsPtr->wMax != NULL) {
	limitsPtr->max = Tk_ReqHeight(limitsPtr->wMax);
    }
    if (limitsPtr->wNom != NULL) {
	limitsPtr->nom = Tk_ReqHeight(limitsPtr->wNom);
    }
    if (limitsPtr->flags & LIMITS_SET_NOM) {
	height = limitsPtr->nom;/* Override initial value */
    }
    if (height < limitsPtr->min) {
	height = limitsPtr->min;/* Bounded by minimum value */
    } else if (height > limitsPtr->max) {
	height = limitsPtr->max;/* Bounded by maximum value */
    }
    return height;
}

/*
 * ----------------------------------------------------------------------------
 *
 * NameOfLimits --
 *
 *	Convert the values into a list representing the limits.
 *
 * Results:
 *	The static string representation of the limits is returned.
 *
 * ----------------------------------------------------------------------------
 */
static char *
NameOfLimits(limitsPtr)
    Limits *limitsPtr;
{
    Tcl_DString buffer;
#define STRING_SPACE 200
    static char string[STRING_SPACE + 1];

    Tcl_DStringInit(&buffer);

    if (limitsPtr->wMin != NULL) {
	Tcl_DStringAppendElement(&buffer, Tk_PathName(limitsPtr->wMin));
    } else if (limitsPtr->flags & LIMITS_SET_MIN) {
	Tcl_DStringAppendElement(&buffer, Blt_Itoa(limitsPtr->min));
    } else {
	Tcl_DStringAppendElement(&buffer, "");
    }

    if (limitsPtr->wMax != NULL) {
	Tcl_DStringAppendElement(&buffer, Tk_PathName(limitsPtr->wMax));
    } else if (limitsPtr->flags & LIMITS_SET_MAX) {
	Tcl_DStringAppendElement(&buffer, Blt_Itoa(limitsPtr->max));
    } else {
	Tcl_DStringAppendElement(&buffer, "");
    }

    if (limitsPtr->wNom != NULL) {
	Tcl_DStringAppendElement(&buffer, Tk_PathName(limitsPtr->wNom));
    } else if (limitsPtr->flags & LIMITS_SET_NOM) {
	Tcl_DStringAppendElement(&buffer, Blt_Itoa(limitsPtr->nom));
    } else {
	Tcl_DStringAppendElement(&buffer, "");
    }
    strncpy(string, Tcl_DStringValue(&buffer), STRING_SPACE);
    string[STRING_SPACE] = '\0';
    return string;
}

/*
 * ----------------------------------------------------------------------------
 *
 * LimitsToString --
 *
 *	Convert the limits of the pixel values allowed into a list.
 *
 * Results:
 *	The string representation of the limits is returned.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
LimitsToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Row/column structure record */
    int offset;			/* Offset of widget RowColumn record */
    Tcl_FreeProc **freeProcPtr;	/* Memory deallocation routine */
{
    Limits *limitsPtr = (Limits *)(widgRec + offset);

    return NameOfLimits(limitsPtr);
}

/*
 * ----------------------------------------------------------------------------
 *
 * StringToResize --
 *
 *	Converts the resize mode into its numeric representation.  Valid
 *	mode strings are "none", "expand", "shrink", or "both".
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToResize(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* Resize style string */
    char *widgRec;		/* Entry structure record */
    int offset;			/* Offset of style in record */
{
    int *resizePtr = (int *)(widgRec + offset);
    unsigned int length;
    char c;

    c = string[0];
    length = strlen(string);
    if ((c == 'n') && (strncmp(string, "none", length) == 0)) {
	*resizePtr = RESIZE_NONE;
    } else if ((c == 'b') && (strncmp(string, "both", length) == 0)) {
	*resizePtr = RESIZE_BOTH;
    } else if ((c == 'e') && (strncmp(string, "expand", length) == 0)) {
	*resizePtr = RESIZE_EXPAND;
    } else if ((c == 's') && (strncmp(string, "shrink", length) == 0)) {
	*resizePtr = RESIZE_SHRINK;
    } else {
	Tcl_AppendResult(interp, "bad resize argument \"", string,
	    "\": should be \"none\", \"expand\", \"shrink\", or \"both\"",
	    (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * NameOfResize --
 *
 *	Converts the resize value into its string representation.
 *
 * Results:
 *	Returns a pointer to the static name string.
 *
 * ----------------------------------------------------------------------------
 */
static char *
NameOfResize(resize)
    int resize;
{
    switch (resize & RESIZE_BOTH) {
    case RESIZE_NONE:
	return "none";
    case RESIZE_EXPAND:
	return "expand";
    case RESIZE_SHRINK:
	return "shrink";
    case RESIZE_BOTH:
	return "both";
    default:
	return "unknown resize value";
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * ResizeToString --
 *
 *	Returns resize mode string based upon the resize flags.
 *
 * Results:
 *	The resize mode string is returned.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
ResizeToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Row/column structure record */
    int offset;			/* Offset of resize in RowColumn record */
    Tcl_FreeProc **freeProcPtr;	/* Not used. */
{
    int resize = *(int *)(widgRec + offset);

    return NameOfResize(resize);
}

/*
 * ----------------------------------------------------------------------------
 *
 * StringToControl --
 *
 *	Converts the control string into its numeric representation.
 *	Valid control strings are "none", "normal", and "full".
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToControl(clientData, interp, tkwin, string, widgRec, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    Tk_Window tkwin;		/* Not used. */
    char *string;		/* Control style string */
    char *widgRec;		/* Entry structure record */
    int offset;			/* Offset of style in record */
{
    double *controlPtr = (double *)(widgRec + offset);
    unsigned int length;
    int bool;
    char c;

    c = string[0];
    length = strlen(string);
    if (Tcl_GetBoolean(NULL, string, &bool) == TCL_OK) {
	*controlPtr = bool;
	return TCL_OK;
    }
    if ((c == 'n') && (length > 1) &&
	(strncmp(string, "normal", length) == 0)) {
	*controlPtr = CONTROL_NORMAL;
    } else if ((c == 'n') && (length > 1) &&
	(strncmp(string, "none", length) == 0)) {
	*controlPtr = CONTROL_NONE;
    } else if ((c == 'f') && (strncmp(string, "full", length) == 0)) {
	*controlPtr = CONTROL_FULL;
    } else {
	double control;

	if ((Tcl_GetDouble(interp, string, &control) != TCL_OK) ||
	    (control < 0.0)) {
	    Tcl_AppendResult(interp, "bad control argument \"", string,
		"\": should be \"normal\", \"none\", or \"full\"",
		(char *)NULL);
	    return TCL_ERROR;
	}
	*controlPtr = control;
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * NameOfControl --
 *
 *	Converts the control value into its string representation.
 *
 * Results:
 *	Returns a pointer to the static name string.
 *
 * ----------------------------------------------------------------------------
 */
static char *
NameOfControl(control)
    double control;
{
    if (control == CONTROL_NORMAL) {
	return "normal";
    } else if (control == CONTROL_NONE) {
	return "none";
    } else if (control == CONTROL_FULL) {
	return "full";
    } else {
	static char string[TCL_DOUBLE_SPACE + 1];

	sprintf(string, "%g", control);
	return string;
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * ControlToString --
 *
 *	Returns control mode string based upon the control flags.
 *
 * Results:
 *	The control mode string is returned.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static char *
ControlToString(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Not used. */
    char *widgRec;		/* Row/column structure record */
    int offset;			/* Offset of control in RowColumn record */
    Tcl_FreeProc **freeProcPtr;	/* Not used. */
{
    double control = *(double *)(widgRec + offset);

    return NameOfControl(control);
}


static void
EventuallyArrangeTable(tablePtr)
    Table *tablePtr;
{
    if (!(tablePtr->flags & ARRANGE_PENDING)) {
	tablePtr->flags |= ARRANGE_PENDING;
	Tcl_DoWhenIdle(ArrangeTable, tablePtr);
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * TableEventProc --
 *
 *	This procedure is invoked by the Tk event handler when the
 *	container widget is reconfigured or destroyed.
 *
 *	The table will be rearranged at the next idle point if the
 *	container widget has been resized or moved. There's a
 *	distinction made between parent and non-parent container
 *	arrangements.  If the container is moved and it's the parent
 *	of the widgets, they're are moved automatically.  If it's
 *	not the parent, those widgets need to be moved manually.
 *	This can be a performance hit in rare cases where we're
 *	scrolling the container (by moving the window) and there
 *	are lots of non-child widgets arranged insided.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the table associated with tkwin to have its
 *	layout re-computed and drawn at the next idle point.
 *
 * ----------------------------------------------------------------------------
 */
static void
TableEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about widget */
    XEvent *eventPtr;		/* Information about event */
{
    register Table *tablePtr = clientData;

    if (eventPtr->type == ConfigureNotify) {
	if ((tablePtr->container.width != Tk_Width(tablePtr->tkwin)) ||
	    (tablePtr->container.height != Tk_Height(tablePtr->tkwin))
	    || (tablePtr->flags & NON_PARENT)) {
	    EventuallyArrangeTable(tablePtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	if (tablePtr->flags & ARRANGE_PENDING) {
	    Tcl_CancelIdleCall(ArrangeTable, tablePtr);
	}
	tablePtr->tkwin = NULL;
	Tcl_EventuallyFree(tablePtr, DestroyTable);
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * WidgetEventProc --
 *
 *	This procedure is invoked by the Tk event handler when
 *	StructureNotify events occur in a widget managed by the table.
 *	For example, when a managed widget is destroyed, it frees the
 *	corresponding entry structure and arranges for the table
 *	layout to be re-computed at the next idle point.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the managed widget was deleted, the Entry structure gets
 *	cleaned up and the table is rearranged.
 *
 * ----------------------------------------------------------------------------
 */
static void
WidgetEventProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to Entry structure for widget
				 * referred to by eventPtr. */
    XEvent *eventPtr;		/* Describes what just happened. */
{
    Entry *entryPtr = (Entry *) clientData;
    Table *tablePtr = entryPtr->tablePtr;

    if (eventPtr->type == ConfigureNotify) {
	int borderWidth;

	tablePtr->flags |= REQUEST_LAYOUT;
	borderWidth = Tk_Changes(entryPtr->tkwin)->border_width;
	if (entryPtr->borderWidth != borderWidth) {
	    entryPtr->borderWidth = borderWidth;
	    EventuallyArrangeTable(tablePtr);
	}
    } else if (eventPtr->type == DestroyNotify) {
	entryPtr->tkwin = NULL;
	DestroyEntry(entryPtr);
	tablePtr->flags |= REQUEST_LAYOUT;
	EventuallyArrangeTable(tablePtr);
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * WidgetCustodyProc --
 *
 * 	This procedure is invoked when a widget has been stolen by
 * 	another geometry manager.  The information and memory
 * 	associated with the widget is released.
 *
 * Results:
 *	None.
 *
 * Side effects:
  *	Arranges for the table to have its layout re-arranged at the
 *	next idle point.
 *
 * ----------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
WidgetCustodyProc(clientData, tkwin)
    ClientData clientData;	/* Information about the widget */
    Tk_Window tkwin;		/* Not used. */
{
    Entry *entryPtr = (Entry *) clientData;
    Table *tablePtr = entryPtr->tablePtr;

    if (Tk_IsMapped(entryPtr->tkwin)) {
	Tk_UnmapWindow(entryPtr->tkwin);
    }
    Tk_UnmaintainGeometry(entryPtr->tkwin, tablePtr->tkwin);
    entryPtr->tkwin = NULL;
    DestroyEntry(entryPtr);
    tablePtr->flags |= REQUEST_LAYOUT;
    EventuallyArrangeTable(tablePtr);
}

/*
 * ----------------------------------------------------------------------------
 *
 * WidgetGeometryProc --
 *
 *	This procedure is invoked by Tk_GeometryRequest for widgets
 *	managed by the table geometry manager.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the table to have its layout re-computed and
 *	re-arranged at the next idle point.
 *
 * ---------------------------------------------------------------------------- */
/* ARGSUSED */
static void
WidgetGeometryProc(clientData, tkwin)
    ClientData clientData;	/* Information about widget that got new
				 * preferred geometry.  */
    Tk_Window tkwin;		/* Other Tk-related information about the
			         * widget. */
{
    Entry *entryPtr = (Entry *) clientData;

    entryPtr->tablePtr->flags |= REQUEST_LAYOUT;
    EventuallyArrangeTable(entryPtr->tablePtr);
}

/*
 * ----------------------------------------------------------------------------
 *
 * FindEntry --
 *
 *	Searches for the table entry corresponding to the given
 *	widget.
 *
 * Results:
 *	If a structure associated with the widget exists, a pointer to
 *	that structure is returned. Otherwise NULL.
 *
 * ----------------------------------------------------------------------------
 */
static Entry *
FindEntry(tablePtr, tkwin)
    Table *tablePtr;
    Tk_Window tkwin;		/* Widget associated with table entry */
{
    Blt_HashEntry *hPtr;

    hPtr = Blt_FindHashEntry(&(tablePtr->entryTable), (char *)tkwin);
    if (hPtr == NULL) {
	return NULL;
    }
    return (Entry *) Blt_GetHashValue(hPtr);
}


static int
GetEntry(interp, tablePtr, string, entryPtrPtr)
    Tcl_Interp *interp;
    Table *tablePtr;
    char *string;
    Entry **entryPtrPtr;
{
    Tk_Window tkwin;
    Entry *entryPtr;

    tkwin = Tk_NameToWindow(interp, string, tablePtr->tkwin);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    entryPtr = FindEntry(tablePtr, tkwin);
    if (entryPtr == NULL) {
	Tcl_AppendResult(interp, "\"", Tk_PathName(tkwin),
	    "\" is not managed by any table", (char *)NULL);
	return TCL_ERROR;
    }
    *entryPtrPtr = entryPtr;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * CreateEntry --
 *
 *	This procedure creates and initializes a new Entry structure
 *	to hold a widget.  A valid widget has a parent widget that is
 *	either a) the container widget itself or b) a mutual ancestor
 *	of the container widget.
 *
 * Results:
 *	Returns a pointer to the new structure describing the new
 *	widget entry.  If an error occurred, then the return
 *	value is NULL and an error message is left in interp->result.
 *
 * Side effects:
 *	Memory is allocated and initialized for the Entry structure.
 *
 * ---------------------------------------------------------------------------- */
static Entry *
CreateEntry(tablePtr, tkwin)
    Table *tablePtr;
    Tk_Window tkwin;
{
    register Entry *entryPtr;
    int dummy;
    Tk_Window parent, ancestor;

    /*
     * Check that this widget can be managed by this table.  A valid
     * widget has a parent widget that either
     *
     *    1) is the container widget, or
     *    2) is a mutual ancestor of the container widget.
     */
    ancestor = Tk_Parent(tkwin);
    for (parent = tablePtr->tkwin; (parent != ancestor) &&
	(!Tk_IsTopLevel(parent)); parent = Tk_Parent(parent)) {
	/* empty */
    }
    if (ancestor != parent) {
	Tcl_AppendResult(tablePtr->interp, "can't manage \"",
	    Tk_PathName(tkwin), "\" in table \"", Tk_PathName(tablePtr->tkwin),
	    "\"", (char *)NULL);
	return NULL;
    }
    entryPtr = Blt_Calloc(1, sizeof(Entry));
    assert(entryPtr);

    /* Initialize the entry structure */

    entryPtr->tkwin = tkwin;
    entryPtr->tablePtr = tablePtr;
    entryPtr->borderWidth = Tk_Changes(tkwin)->border_width;
    entryPtr->fill = ENTRY_DEF_FILL;
    entryPtr->row.control = entryPtr->column.control = ENTRY_DEF_CONTROL;
    entryPtr->anchor = ENTRY_DEF_ANCHOR;
    entryPtr->row.span = entryPtr->column.span = ENTRY_DEF_SPAN;
    ResetLimits(&(entryPtr->reqWidth));
    ResetLimits(&(entryPtr->reqHeight));

    /*
     * Add the entry to the following data structures.
     *
     * 	1) A chain of widgets managed by the table.
     *   2) A hash table of widgets managed by the table.
     */
    entryPtr->linkPtr = Blt_ChainAppend(tablePtr->chainPtr, entryPtr);
    entryPtr->hashPtr = Blt_CreateHashEntry(&(tablePtr->entryTable),
	(char *)tkwin, &dummy);
    Blt_SetHashValue(entryPtr->hashPtr, entryPtr);

    Tk_CreateEventHandler(tkwin, StructureNotifyMask, WidgetEventProc, 
	entryPtr);
    Tk_ManageGeometry(tkwin, &tableMgrInfo, (ClientData)entryPtr);

    return entryPtr;
}

/*
 * ----------------------------------------------------------------------------
 *
 * DestroyEntry --
 *
 *	Removes the Entry structure from the hash table and frees
 *	the memory allocated by it.  If the table is still in use
 *	(i.e. was not called from DestoryTable), remove its entries
 *	from the lists of row and column sorted partitions.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the entry is freed up.
 *
 * ----------------------------------------------------------------------------
 */
static void
DestroyEntry(entryPtr)
    Entry *entryPtr;
{
    Table *tablePtr = entryPtr->tablePtr;

    if (entryPtr->row.linkPtr != NULL) {
	Blt_ChainDeleteLink(entryPtr->row.chainPtr, entryPtr->row.linkPtr);
    }
    if (entryPtr->column.linkPtr != NULL) {
	Blt_ChainDeleteLink(entryPtr->column.chainPtr,
	    entryPtr->column.linkPtr);
    }
    if (entryPtr->linkPtr != NULL) {
	Blt_ChainDeleteLink(tablePtr->chainPtr, entryPtr->linkPtr);
    }
    if (entryPtr->tkwin != NULL) {
	Tk_DeleteEventHandler(entryPtr->tkwin, StructureNotifyMask,
	      WidgetEventProc, (ClientData)entryPtr);
	Tk_ManageGeometry(entryPtr->tkwin, (Tk_GeomMgr *)NULL,
			  (ClientData)entryPtr);
	if ((tablePtr->tkwin != NULL) && 
	    (Tk_Parent(entryPtr->tkwin) != tablePtr->tkwin)) {
	    Tk_UnmaintainGeometry(entryPtr->tkwin, tablePtr->tkwin);
	}
	if (Tk_IsMapped(entryPtr->tkwin)) {
	    Tk_UnmapWindow(entryPtr->tkwin);
	}
    }
    if (entryPtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(&(tablePtr->entryTable), entryPtr->hashPtr);
    }
    Blt_Free(entryPtr);
}

/*
 * ----------------------------------------------------------------------------
 *
 * ConfigureEntry --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	one or more entries.  Entries hold information about widgets
 *	managed by the table geometry manager.
 *
 * 	Note: You can query only one widget at a time.  But several
 * 	      can be reconfigured at once.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	The table layout is recomputed and rearranged at the next idle
 *	point.
 *
 * ----------------------------------------------------------------------------
 */
static int
ConfigureEntry(tablePtr, interp, entryPtr, argc, argv)
    Table *tablePtr;
    Tcl_Interp *interp;
    Entry *entryPtr;
    int argc;			/* Option-value arguments */
    char **argv;
{
    int oldRowSpan, oldColSpan;

    if (entryPtr->tablePtr != tablePtr) {
	Tcl_AppendResult(interp, "widget  \"", Tk_PathName(entryPtr->tkwin),
	    "\" does not belong to table \"", Tk_PathName(tablePtr->tkwin),
	    "\"", (char *)NULL);
	return TCL_ERROR;
    }
    if (argc == 0) {
	return Tk_ConfigureInfo(interp, entryPtr->tkwin, entryConfigSpecs,
	    (char *)entryPtr, (char *)NULL, 0);
    } else if (argc == 1) {
	return Tk_ConfigureInfo(interp, entryPtr->tkwin, entryConfigSpecs,
	    (char *)entryPtr, argv[0], 0);
    }
    oldRowSpan = entryPtr->row.span;
    oldColSpan = entryPtr->column.span;

    if (Tk_ConfigureWidget(interp, entryPtr->tkwin, entryConfigSpecs,
	    argc, argv, (char *)entryPtr, TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((entryPtr->column.span < 1) || (entryPtr->column.span > USHRT_MAX)) {
	Tcl_AppendResult(interp, "bad column span specified for \"",
	    Tk_PathName(entryPtr->tkwin), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    if ((entryPtr->row.span < 1) || (entryPtr->row.span > USHRT_MAX)) {
	Tcl_AppendResult(interp, "bad row span specified for \"",
	    Tk_PathName(entryPtr->tkwin), "\"", (char *)NULL);
	return TCL_ERROR;
    }
    if ((oldColSpan != entryPtr->column.span) ||
	(oldRowSpan != entryPtr->row.span)) {
	BinEntry(tablePtr, entryPtr);
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * PrintEntry --
 *
 *	Returns the name, position and options of a widget in the table.
 *
 * Results:
 *	Returns a standard Tcl result.  A list of the widget
 *	attributes is left in interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static void
PrintEntry(entryPtr, resultPtr)
    Entry *entryPtr;
    Tcl_DString *resultPtr;
{
    char string[200];

    sprintf(string, "    %d,%d  ", entryPtr->row.rcPtr->index,
	entryPtr->column.rcPtr->index);
    Tcl_DStringAppend(resultPtr, string, -1);
    Tcl_DStringAppend(resultPtr, Tk_PathName(entryPtr->tkwin), -1);
    if (entryPtr->ipadX != ENTRY_DEF_PAD) {
	Tcl_DStringAppend(resultPtr, " -ipadx ", -1);
	Tcl_DStringAppend(resultPtr, Blt_Itoa(entryPtr->ipadX), -1);
    }
    if (entryPtr->ipadY != ENTRY_DEF_PAD) {
	Tcl_DStringAppend(resultPtr, " -ipady ", -1);
	Tcl_DStringAppend(resultPtr, Blt_Itoa(entryPtr->ipadY), -1);
    }
    if (entryPtr->row.span != ENTRY_DEF_SPAN) {
	Tcl_DStringAppend(resultPtr, " -rowspan ", -1);
	Tcl_DStringAppend(resultPtr, Blt_Itoa(entryPtr->row.span), -1);
    }
    if (entryPtr->column.span != ENTRY_DEF_SPAN) {
	Tcl_DStringAppend(resultPtr, " -columnspan ", -1);
	Tcl_DStringAppend(resultPtr, Blt_Itoa(entryPtr->column.span), -1);
    }
    if (entryPtr->anchor != ENTRY_DEF_ANCHOR) {
	Tcl_DStringAppend(resultPtr, " -anchor ", -1);
	Tcl_DStringAppend(resultPtr, Tk_NameOfAnchor(entryPtr->anchor), -1);
    }
    if ((entryPtr->padLeft != ENTRY_DEF_PAD) ||
	(entryPtr->padRight != ENTRY_DEF_PAD)) {
	Tcl_DStringAppend(resultPtr, " -padx ", -1);
	sprintf(string, "{%d %d}", entryPtr->padLeft, entryPtr->padRight);
	Tcl_DStringAppend(resultPtr, string, -1);
    }
    if ((entryPtr->padTop != ENTRY_DEF_PAD) ||
	(entryPtr->padBottom != ENTRY_DEF_PAD)) {
	Tcl_DStringAppend(resultPtr, " -pady ", -1);
	sprintf(string, "{%d %d}", entryPtr->padTop, entryPtr->padBottom);
	Tcl_DStringAppend(resultPtr, string, -1);
    }
    if (entryPtr->fill != ENTRY_DEF_FILL) {
	Tcl_DStringAppend(resultPtr, " -fill ", -1);
	Tcl_DStringAppend(resultPtr, Blt_NameOfFill(entryPtr->fill), -1);
    }
    if (entryPtr->column.control != ENTRY_DEF_CONTROL) {
	Tcl_DStringAppend(resultPtr, " -columncontrol ", -1);
	Tcl_DStringAppend(resultPtr, NameOfControl(entryPtr->column.control), -1);
    }
    if (entryPtr->row.control != ENTRY_DEF_CONTROL) {
	Tcl_DStringAppend(resultPtr, " -rowcontrol ", -1);
	Tcl_DStringAppend(resultPtr, NameOfControl(entryPtr->row.control), -1);
    }
    if ((entryPtr->reqWidth.nom != LIMITS_NOM) ||
	(entryPtr->reqWidth.min != LIMITS_MIN) ||
	(entryPtr->reqWidth.max != LIMITS_MAX)) {
	Tcl_DStringAppend(resultPtr, " -reqwidth {", -1);
	Tcl_DStringAppend(resultPtr, NameOfLimits(&(entryPtr->reqWidth)), -1);
	Tcl_DStringAppend(resultPtr, "}", -1);
    }
    if ((entryPtr->reqHeight.nom != LIMITS_NOM) ||
	(entryPtr->reqHeight.min != LIMITS_MIN) ||
	(entryPtr->reqHeight.max != LIMITS_MAX)) {
	Tcl_DStringAppend(resultPtr, " -reqheight {", -1);
	Tcl_DStringAppend(resultPtr, NameOfLimits(&(entryPtr->reqHeight)), -1);
	Tcl_DStringAppend(resultPtr, "}", -1);
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * InfoEntry --
 *
 *	Returns the name, position and options of a widget in the table.
 *
 * Results:
 *	Returns a standard Tcl result.  A list of the widget
 *	attributes is left in interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InfoEntry(interp, tablePtr, entryPtr)
    Tcl_Interp *interp;
    Table *tablePtr;
    Entry *entryPtr;
{
    Tcl_DString dString;

    if (entryPtr->tablePtr != tablePtr) {
	Tcl_AppendResult(interp, "widget  \"", Tk_PathName(entryPtr->tkwin),
	    "\" does not belong to table \"", Tk_PathName(tablePtr->tkwin),
	    "\"", (char *)NULL);
	return TCL_ERROR;
    }
    Tcl_DStringInit(&dString);
    PrintEntry(entryPtr, &dString);
    Tcl_DStringResult(interp, &dString);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * CreateRowColumn --
 *
 *	Creates and initializes a structure that manages the size of a
 *	row or column in the table. There will be one of these
 *	structures allocated for each row and column in the table,
 *	regardless if a widget is contained in it or not.
 *
 * Results:
 *	Returns a pointer to the newly allocated row or column
 *	structure.
 *
 * ----------------------------------------------------------------------------
 */
static RowColumn *
CreateRowColumn()
{
    RowColumn *rcPtr;

    rcPtr = Blt_Malloc(sizeof(RowColumn));
    rcPtr->resize = ROWCOL_DEF_RESIZE;
    ResetLimits(&(rcPtr->reqSize));
    rcPtr->nomSize = LIMITS_NOM;
    rcPtr->pad.side1 = rcPtr->pad.side2 = ROWCOL_DEF_PAD;
    rcPtr->size = rcPtr->index = rcPtr->minSpan = 0;
    rcPtr->weight = ROWCOL_DEF_WEIGHT;
    return rcPtr;
}

static PartitionInfo *
ParseRowColumn2(tablePtr, string, numberPtr)
    Table *tablePtr;
    char *string;
    int *numberPtr;
{
    char c;
    int n;
    PartitionInfo *infoPtr;

    c = tolower(string[0]);
    if (c == 'c') {
	infoPtr = &(tablePtr->columnInfo);
    } else if (c == 'r') {
	infoPtr = &(tablePtr->rowInfo);
    } else {
	Tcl_AppendResult(tablePtr->interp, "bad index \"", string,
	    "\": must start with \"r\" or \"c\"", (char *)NULL);
	return NULL;
    }
    /* Handle row or column configuration queries */
    if (Tcl_GetInt(tablePtr->interp, string + 1, &n) != TCL_OK) {
	return NULL;
    }
    *numberPtr = (int)n;
    return infoPtr;
}

static PartitionInfo *
ParseRowColumn(tablePtr, string, numberPtr)
    Table *tablePtr;
    char *string;
    int *numberPtr;
{
    int n;
    PartitionInfo *infoPtr;

    infoPtr = ParseRowColumn2(tablePtr, string, &n);
    if (infoPtr == NULL) {
	return NULL;
    }
    if ((n < 0) || (n >= Blt_ChainGetLength(infoPtr->chainPtr))) {
	Tcl_AppendResult(tablePtr->interp, "bad ", infoPtr->type, " index \"",
	    string, "\"", (char *)NULL);
	return NULL;
    }
    *numberPtr = (int)n;
    return infoPtr;
}

/*
 * ----------------------------------------------------------------------------
 *
 * GetRowColumn --
 *
 *	Gets the designated row or column from the table.  If the row
 *	or column index is greater than the size of the table, new
 *	rows/columns will be automatically allocated.
 *
 * Results:
 *	Returns a pointer to the row or column structure.
 *
 * ----------------------------------------------------------------------------
 */
static RowColumn *
GetRowColumn(infoPtr, n)
    PartitionInfo *infoPtr;
    int n;
{
    Blt_ChainLink *linkPtr;
    RowColumn *rcPtr;
    register int i;

    for (i = Blt_ChainGetLength(infoPtr->chainPtr); i <= n; i++) {
	rcPtr = CreateRowColumn();
	rcPtr->index = i;
	rcPtr->linkPtr = Blt_ChainAppend(infoPtr->chainPtr, (ClientData)rcPtr);
    }
    linkPtr = Blt_ChainGetNthLink(infoPtr->chainPtr, n);
    if (linkPtr == NULL) {
	return NULL;
    }
    return Blt_ChainGetValue(linkPtr);
}

/*
 * ----------------------------------------------------------------------------
 *
 * DeleteRowColumn --
 *
 *	Deletes a span of rows/columns from the table. The number of
 *	rows/columns to be deleted is given by span.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The size of the column partition array may be extended and
 *	initialized.
 *
 * ----------------------------------------------------------------------------
 */
static void
DeleteRowColumn(tablePtr, infoPtr, rcPtr)
    Table *tablePtr;
    PartitionInfo *infoPtr;
    RowColumn *rcPtr;
{
    Blt_ChainLink *linkPtr, *nextPtr;
    Entry *entryPtr;

    /*
     * Remove any entries that start in the row/column to be deleted.
     * They point to memory that will be freed.
     */
    if (infoPtr->type == rowUid) {
	for (linkPtr = Blt_ChainFirstLink(tablePtr->chainPtr); linkPtr != NULL;
	    linkPtr = nextPtr) {
	    nextPtr = Blt_ChainNextLink(linkPtr);
	    entryPtr = Blt_ChainGetValue(linkPtr);
	    if (entryPtr->row.rcPtr->index == rcPtr->index) {
		DestroyEntry(entryPtr);
	    }
	}
    } else {
	for (linkPtr = Blt_ChainFirstLink(tablePtr->chainPtr); linkPtr != NULL;
	    linkPtr = nextPtr) {
	    nextPtr = Blt_ChainNextLink(linkPtr);
	    entryPtr = Blt_ChainGetValue(linkPtr);
	    if (entryPtr->column.rcPtr->index == rcPtr->index) {
		DestroyEntry(entryPtr);
	    }
	}
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * ConfigureRowColumn --
 *
 *	This procedure is called to process an argv/argc list in order
 *	to configure a row or column in the table geometry manager.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result holds an error message.
 *
 * Side effects:
 *	Partition configuration options (bounds, resize flags, etc)
 *	get set.  New partitions may be created as necessary. The
 *	table is recalculated and arranged at the next idle point.
 *
 * ----------------------------------------------------------------------------
 */
static int
ConfigureRowColumn(tablePtr, infoPtr, pattern, argc, argv)
    Table *tablePtr;		/* Table to be configured */
    PartitionInfo *infoPtr;
    char *pattern;
    int argc;
    char **argv;
{
    RowColumn *rcPtr;
    register Blt_ChainLink *linkPtr;
    char string[200];
    int nMatches;

    nMatches = 0;
    for (linkPtr = Blt_ChainFirstLink(infoPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	sprintf(string, "%c%d", pattern[0], rcPtr->index);
	if (Tcl_StringMatch(string, pattern)) {
	    if (argc == 0) {
		return Tk_ConfigureInfo(tablePtr->interp, tablePtr->tkwin,
		    infoPtr->configSpecs, (char *)rcPtr, NULL, 0);
	    } else if (argc == 1) {
		return Tk_ConfigureInfo(tablePtr->interp, tablePtr->tkwin,
		    infoPtr->configSpecs, (char *)rcPtr, argv[0], 0);
	    } else {
		if (Tk_ConfigureWidget(tablePtr->interp, tablePtr->tkwin,
			infoPtr->configSpecs, argc, argv, (char *)rcPtr,
			TK_CONFIG_ARGV_ONLY) != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    nMatches++;
	}
    }
    if (nMatches == 0) {
	int n;

	/* 
	 * We found no existing partitions matching this pattern, so 
	 * see if this designates an new partition (one beyond the 
	 * current range).  
	 */
	if ((Tcl_GetInt(NULL, pattern + 1, &n) != TCL_OK) || (n < 0)) {
	    Tcl_AppendResult(tablePtr->interp, "pattern \"", pattern, 
		     "\" matches no ", infoPtr->type, " in table \"", 
		     Tk_PathName(tablePtr->tkwin), "\"", (char *)NULL);
	    return TCL_ERROR;
	}
	rcPtr = GetRowColumn(infoPtr, n);
	assert(rcPtr);
	if (Tk_ConfigureWidget(tablePtr->interp, tablePtr->tkwin,
	       infoPtr->configSpecs, argc, argv, (char *)rcPtr,
	       TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    EventuallyArrangeTable(tablePtr);
    return TCL_OK;
}

static void
PrintRowColumn(interp, infoPtr, rcPtr, resultPtr)
    Tcl_Interp *interp;
    PartitionInfo *infoPtr;
    RowColumn *rcPtr;
    Tcl_DString *resultPtr;
{
    char string[200];
    char *padFmt, *sizeFmt;

    if (infoPtr->type == rowUid) {
	padFmt = " -pady {%d %d}";
	sizeFmt = " -height {%s}";
    } else {
	padFmt = " -padx {%d %d}";
	sizeFmt = " -width {%s}";
    }
    if (rcPtr->resize != ROWCOL_DEF_RESIZE) {
	Tcl_DStringAppend(resultPtr, " -resize ", -1);
	Tcl_DStringAppend(resultPtr, NameOfResize(rcPtr->resize), -1);
    }
    if ((rcPtr->pad.side1 != ROWCOL_DEF_PAD) ||
	(rcPtr->pad.side2 != ROWCOL_DEF_PAD)) {
	sprintf(string, padFmt, rcPtr->pad.side1, rcPtr->pad.side2);
	Tcl_DStringAppend(resultPtr, string, -1);
    }
    if (rcPtr->weight != ROWCOL_DEF_WEIGHT) {
	Tcl_DStringAppend(resultPtr, " -weight ", -1);
	Tcl_DStringAppend(resultPtr, Blt_Dtoa(interp, rcPtr->weight), -1);
    }
    if ((rcPtr->reqSize.min != LIMITS_MIN) ||
	(rcPtr->reqSize.nom != LIMITS_NOM) ||
	(rcPtr->reqSize.max != LIMITS_MAX)) {
	sprintf(string, sizeFmt, NameOfLimits(&(rcPtr->reqSize)));
	Tcl_DStringAppend(resultPtr, string, -1);
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * InfoRowColumn --
 *
 *	Returns the options of a partition in the table.
 *
 * Results:
 *	Returns a standard Tcl result.  A list of the partition
 *	attributes is left in interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InfoRowColumn(tablePtr, interp, pattern)
    Table *tablePtr;
    Tcl_Interp *interp;
    char *pattern;
{
    RowColumn *rcPtr;
    char string[200];
    PartitionInfo *infoPtr;
    char c;
    Blt_ChainLink *linkPtr, *lastPtr;
    Tcl_DString dString;

    c = pattern[0];
    if ((c == 'r') || (c == 'R')) {
	infoPtr = &(tablePtr->rowInfo);
    } else {
	infoPtr = &(tablePtr->columnInfo);
    }
    Tcl_DStringInit(&dString);
    lastPtr = Blt_ChainLastLink(infoPtr->chainPtr);
    for (linkPtr = Blt_ChainFirstLink(infoPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	sprintf(string, "%c%d", infoPtr->type[0], rcPtr->index);
	if (Tcl_StringMatch(string, pattern)) {
	    Tcl_DStringAppend(&dString, string, -1);
	    PrintRowColumn(interp, infoPtr, rcPtr, &dString);
	    if (linkPtr != lastPtr) {
		Tcl_DStringAppend(&dString, " \\\n", -1);
	    } else {
		Tcl_DStringAppend(&dString, "\n", -1);
	    }
	}
    }
    Tcl_DStringResult(interp, &dString);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * InitSpan --
 *
 *	Checks the size of the column partitions and extends the size
 *	if a larger array is needed.
 *
 * Results:
 *	Returns 1 if the column exists.  Otherwise 0 is returned and
 *	interp->result contains an error message.
 *
 * Side effects:
 *	The size of the column partition array may be extended and
 *	initialized.
 *
 * ----------------------------------------------------------------------------
 */
static RowColumn *
InitSpan(infoPtr, start, span)
    PartitionInfo *infoPtr;
    int start, span;
{
    int length;
    RowColumn *rcPtr;
    register int i;
    Blt_ChainLink *linkPtr;

    length = Blt_ChainGetLength(infoPtr->chainPtr);
    for (i = length; i < (start + span); i++) {
	rcPtr = CreateRowColumn();
	rcPtr->index = i;
	rcPtr->linkPtr = Blt_ChainAppend(infoPtr->chainPtr, (ClientData)rcPtr);
    }
    linkPtr = Blt_ChainGetNthLink(infoPtr->chainPtr, start);
    return Blt_ChainGetValue(linkPtr);
}

/*
 * ----------------------------------------------------------------------------
 *
 * Blt_GetTable --
 *
 *	Searches for a table associated by the path name of the widget
 *	container.
 *
 *	Errors may occur because
 *	  1) pathName isn't a valid for any Tk widget, or
 *	  2) there's no table associated with that widget as a container.
 *
 * Results:
 *	If a table entry exists, a pointer to the Table structure is
 *	returned. Otherwise NULL is returned.
 *
 * ----------------------------------------------------------------------------
 */
/*LINTLIBRARY*/
int
Blt_GetTable(dataPtr, interp, pathName, tablePtrPtr)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;		/* Interpreter to report errors back to. */
    char *pathName;		/* Path name of the container widget. */
    Table **tablePtrPtr;
{
    Blt_HashEntry *hPtr;
    Tk_Window tkwin;

    tkwin = Tk_NameToWindow(interp, pathName, Tk_MainWindow(interp));
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    hPtr = Blt_FindHashEntry(&(dataPtr->tableTable), (char *)tkwin);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "no table associated with widget \"",
	    pathName, "\"", (char *)NULL);
	return TCL_ERROR;
    }
    *tablePtrPtr = (Table *)Blt_GetHashValue(hPtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * CreateTable --
 *
 *	This procedure creates and initializes a new Table structure
 *	with tkwin as its container widget. The internal structures
 *	associated with the table are initialized.
 *
 * Results:
 *	Returns the pointer to the new Table structure describing the
 *	new table geometry manager.  If an error occurred, the return
 *	value will be NULL and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	Memory is allocated and initialized, an event handler is set
 *	up to watch tkwin, etc.
 *
 * ----------------------------------------------------------------------------
 */
static Table *
CreateTable(dataPtr, interp, pathName)
    TableInterpData *dataPtr;
    Tcl_Interp *interp;		/* Interpreter associated with table. */
    char *pathName;		/* Path name of the container widget to be
				 * associated with the new table. */
{
    register Table *tablePtr;
    Tk_Window tkwin;
    int dummy;
    Blt_HashEntry *hPtr;

    tkwin = Tk_NameToWindow(interp, pathName, Tk_MainWindow(interp));
    if (tkwin == NULL) {
	return NULL;
    }
    tablePtr = Blt_Calloc(1, sizeof(Table));
    assert(tablePtr);
    tablePtr->tkwin = tkwin;
    tablePtr->interp = interp;
    tablePtr->rowInfo.type = rowUid;
    tablePtr->rowInfo.configSpecs = rowConfigSpecs;
    tablePtr->rowInfo.chainPtr = Blt_ChainCreate();
    tablePtr->columnInfo.type = columnUid;
    tablePtr->columnInfo.configSpecs = columnConfigSpecs;
    tablePtr->columnInfo.chainPtr = Blt_ChainCreate();
    tablePtr->propagate = TRUE;

    tablePtr->arrangeProc = ArrangeTable;
    Blt_InitHashTable(&(tablePtr->entryTable), BLT_ONE_WORD_KEYS);
    tablePtr->findEntryProc = FindEntry;

    ResetLimits(&(tablePtr->reqWidth));
    ResetLimits(&(tablePtr->reqHeight));

    tablePtr->chainPtr = Blt_ChainCreate();
    tablePtr->rowInfo.list = Blt_ListCreate(BLT_ONE_WORD_KEYS);
    tablePtr->columnInfo.list = Blt_ListCreate(BLT_ONE_WORD_KEYS);

    Tk_CreateEventHandler(tablePtr->tkwin, StructureNotifyMask,
	TableEventProc, (ClientData)tablePtr);
    hPtr = Blt_CreateHashEntry(&(dataPtr->tableTable), (char *)tkwin, &dummy);
    tablePtr->hashPtr = hPtr;
    tablePtr->tablePtr = &(dataPtr->tableTable);
    Blt_SetHashValue(hPtr, (ClientData)tablePtr);
    return tablePtr;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ConfigureTable --
 *
 *	This procedure is called to process an argv/argc list in order
 *	to configure the table geometry manager.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Table configuration options (-padx, -pady, etc.) get set.  The
 *	table is recalculated and arranged at the next idle point.
 *
 * ----------------------------------------------------------------------------
 */
static int
ConfigureTable(tablePtr, interp, argc, argv)
    Table *tablePtr;		/* Table to be configured */
    Tcl_Interp *interp;		/* Interpreter to report results back to */
    int argc;
    char **argv;		/* Option-value pairs */
{
    if (argc == 0) {
	return Tk_ConfigureInfo(interp, tablePtr->tkwin, tableConfigSpecs,
	    (char *)tablePtr, (char *)NULL, 0);
    } else if (argc == 1) {
	return Tk_ConfigureInfo(interp, tablePtr->tkwin, tableConfigSpecs,
	    (char *)tablePtr, argv[0], 0);
    }
    if (Tk_ConfigureWidget(interp, tablePtr->tkwin, tableConfigSpecs,
	    argc, argv, (char *)tablePtr, TK_CONFIG_ARGV_ONLY) != TCL_OK) {
	return TCL_ERROR;
    }
    /* Arrange for the table layout to be computed at the next idle point. */
    tablePtr->flags |= REQUEST_LAYOUT;
    EventuallyArrangeTable(tablePtr);
    return TCL_OK;
}

static void
PrintTable(tablePtr, resultPtr)
    Table *tablePtr;
    Tcl_DString *resultPtr;
{
    char string[200];

    if ((tablePtr->padLeft != TABLE_DEF_PAD) ||
	(tablePtr->padRight != TABLE_DEF_PAD)) {
	sprintf(string, " -padx {%d %d}", tablePtr->padLeft, tablePtr->padRight);
	Tcl_DStringAppend(resultPtr, string, -1);
    }
    if ((tablePtr->padTop != TABLE_DEF_PAD) ||
	(tablePtr->padBottom != TABLE_DEF_PAD)) {
	sprintf(string, " -pady {%d %d}", tablePtr->padTop, tablePtr->padBottom);
	Tcl_DStringAppend(resultPtr, string, -1);
    }
    if (!tablePtr->propagate) {
	Tcl_DStringAppend(resultPtr, " -propagate no", -1);
    }
    if ((tablePtr->reqWidth.min != LIMITS_MIN) ||
	(tablePtr->reqWidth.nom != LIMITS_NOM) ||
	(tablePtr->reqWidth.max != LIMITS_MAX)) {
	Tcl_DStringAppend(resultPtr, " -reqwidth {%s}", -1);
	Tcl_DStringAppend(resultPtr, NameOfLimits(&(tablePtr->reqWidth)), -1);
    }
    if ((tablePtr->reqHeight.min != LIMITS_MIN) ||
	(tablePtr->reqHeight.nom != LIMITS_NOM) ||
	(tablePtr->reqHeight.max != LIMITS_MAX)) {
	Tcl_DStringAppend(resultPtr, " -reqheight {%s}", -1);
	Tcl_DStringAppend(resultPtr, NameOfLimits(&(tablePtr->reqHeight)), -1);
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * DestroyPartitions --
 *
 *	Clear each of the lists managing the entries.  The entries in
 *	the lists of row and column spans are themselves lists which
 *	need to be cleared.
 *
 * ----------------------------------------------------------------------------
 */
static void
DestroyPartitions(infoPtr)
    PartitionInfo *infoPtr;
{
    if (infoPtr->list != NULL) {
	Blt_Chain *chainPtr;
	Blt_ListNode node;

	for (node = Blt_ListFirstNode(infoPtr->list); node != NULL;
	    node = Blt_ListNextNode(node)) {
	    chainPtr = (Blt_Chain *)Blt_ListGetValue(node);
	    if (chainPtr != NULL) {
		Blt_ChainDestroy(chainPtr);
	    }
	}
	Blt_ListDestroy(infoPtr->list);
    }
    if (infoPtr->chainPtr != NULL) {
	Blt_ChainLink *linkPtr;
	RowColumn *rcPtr;

	for (linkPtr = Blt_ChainFirstLink(infoPtr->chainPtr);
	    linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	    rcPtr = Blt_ChainGetValue(linkPtr);
	    Blt_Free(rcPtr);
	}
	Blt_ChainDestroy(infoPtr->chainPtr);
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * DestroyTable --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release to
 *	clean up the Table structure at a safe time (when no-one is using
 *	it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the table geometry manager is freed up.
 *
 * ----------------------------------------------------------------------------
 */
static void
DestroyTable(dataPtr)
    DestroyData dataPtr;	/* Table structure */
{
    Blt_ChainLink *linkPtr;
    Entry *entryPtr;
    Table *tablePtr = (Table *)dataPtr;

    /* Release the chain of entries. */
    for (linkPtr = Blt_ChainFirstLink(tablePtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	entryPtr = Blt_ChainGetValue(linkPtr);
	entryPtr->linkPtr = NULL; /* Don't disrupt this chain of entries */
	DestroyEntry(entryPtr);
    }
    Blt_ChainDestroy(tablePtr->chainPtr);

    DestroyPartitions(&(tablePtr->rowInfo));
    DestroyPartitions(&(tablePtr->columnInfo));
    Blt_DeleteHashTable(&(tablePtr->entryTable));
    if (tablePtr->hashPtr != NULL) {
	Blt_DeleteHashEntry(tablePtr->tablePtr, tablePtr->hashPtr);
    }
    Blt_Free(tablePtr);
}

/*
 * ----------------------------------------------------------------------------
 *
 * BinEntry --
 *
 *	Adds the entry to the lists of both row and column spans.  The
 *	layout of the table is done in order of partition spans, from
 *	shorted to longest.  The widgets spanning a particular number of
 *	partitions are stored in a linked list.  Each list is in turn,
 *	contained within a master list.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The entry is added to both the lists of row and columns spans.
 *	This will effect the layout of the widgets.
 *
 * ----------------------------------------------------------------------------
 */
static void
BinEntry(tablePtr, entryPtr)
    Table *tablePtr;
    Entry *entryPtr;
{
    Blt_ListNode node;
    Blt_List list;
    Blt_Chain *chainPtr;
    int key;

    /*
     * Remove the entry from both row and column lists.  It will be
     * re-inserted into the table at the new position.
     */
    if (entryPtr->column.linkPtr != NULL) {
	Blt_ChainUnlinkLink(entryPtr->column.chainPtr,
	    entryPtr->column.linkPtr);
    }
    if (entryPtr->row.linkPtr != NULL) {
	Blt_ChainUnlinkLink(entryPtr->row.chainPtr, entryPtr->row.linkPtr);
    }
    list = tablePtr->rowInfo.list;
    key = 0;			/* Initialize key to bogus span */
    for (node = Blt_ListFirstNode(list); node != NULL;
	node = Blt_ListNextNode(node)) {
	key = (int)Blt_ListGetKey(node);
	if (entryPtr->row.span <= key) {
	    break;
	}
    }
    if (key != entryPtr->row.span) {
	Blt_ListNode newNode;

	/*
	 * Create a new list (bucket) to hold entries of that size
	 * span and and link it into the list of buckets.
	 */
	newNode = Blt_ListCreateNode(list, (char *)entryPtr->row.span);
	Blt_ListSetValue(newNode, (char *)Blt_ChainCreate());
	Blt_ListLinkBefore(list, newNode, node);
	node = newNode;
    }
    chainPtr = (Blt_Chain *) Blt_ListGetValue(node);
    if (entryPtr->row.linkPtr == NULL) {
	entryPtr->row.linkPtr = Blt_ChainAppend(chainPtr, entryPtr);
    } else {
	Blt_ChainLinkBefore(chainPtr, entryPtr->row.linkPtr, NULL);
    }
    entryPtr->row.chainPtr = chainPtr;

    list = tablePtr->columnInfo.list;
    key = 0;
    for (node = Blt_ListFirstNode(list); node != NULL;
	node = Blt_ListNextNode(node)) {
	key = (int)Blt_ListGetKey(node);
	if (entryPtr->column.span <= key) {
	    break;
	}
    }
    if (key != entryPtr->column.span) {
	Blt_ListNode newNode;

	/*
	 * Create a new list (bucket) to hold entries of that size
	 * span and and link it into the list of buckets.
	 */
	newNode = Blt_ListCreateNode(list, (char *)entryPtr->column.span);
	Blt_ListSetValue(newNode, (char *)Blt_ChainCreate());
	Blt_ListLinkBefore(list, newNode, node);
	node = newNode;
    }
    chainPtr = (Blt_Chain *) Blt_ListGetValue(node);

    /* Add the new entry to the span bucket */
    if (entryPtr->column.linkPtr == NULL) {
	entryPtr->column.linkPtr =
	    Blt_ChainAppend(chainPtr, entryPtr);
    } else {
	Blt_ChainLinkBefore(chainPtr, entryPtr->column.linkPtr, NULL);
    }
    entryPtr->column.chainPtr = chainPtr;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ParseIndex --
 *
 *	Parse the entry index string and return the row and column
 *	numbers in their respective parameters.  The format of a table
 *	entry index is row,column where row is the row number and
 *	column is the column number.  Rows and columns are numbered
 *	starting from zero.
 *
 * Results:
 *	Returns a standard Tcl result.  If TCL_OK is returned, the row
 *	and column numbers are returned via rowPtr and columnPtr
 *	respectively.
 *
 * ----------------------------------------------------------------------------
 */
static int
ParseIndex(interp, string, rowPtr, columnPtr)
    Tcl_Interp *interp;
    char *string;
    int *rowPtr, *columnPtr;
{
    char *comma;
    long row, column;
    int result;

    comma = strchr(string, ',');
    if (comma == NULL) {
	Tcl_AppendResult(interp, "bad index \"", string,
	    "\": should be \"row,column\"", (char *)NULL);
	return TCL_ERROR;

    }
    *comma = '\0';
    result = ((Tcl_ExprLong(interp, string, &row) != TCL_OK) ||
	(Tcl_ExprLong(interp, comma + 1, &column) != TCL_OK));
    *comma = ',';		/* Repair the argument */
    if (result) {
	return TCL_ERROR;
    }
    if ((row < 0) || (row > (long)USHRT_MAX)) {
	Tcl_AppendResult(interp, "bad index \"", string,
	    "\": row is out of range", (char *)NULL);
	return TCL_ERROR;

    }
    if ((column < 0) || (column > (long)USHRT_MAX)) {
	Tcl_AppendResult(interp, "bad index \"", string,
	    "\": column is out of range", (char *)NULL);
	return TCL_ERROR;
    }
    *rowPtr = (int)row;
    *columnPtr = (int)column;
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ManageEntry --
 *
 *	Inserts the given widget into the table at a given row and
 *	column position.  The widget can already be managed by this or
 *	another table.  The widget will be simply moved to the new
 *	location in this table.
 *
 *	The new widget is inserted into both a hash table (this is
 *	used to locate the information associated with the widget) and
 *	a list (used to indicate relative ordering of widgets).
 *
 * Results:
 *	Returns a standard Tcl result.  If an error occurred, TCL_ERROR is
 *	returned and an error message is left in interp->result.
 *
 * Side Effects:
 *	The table is re-computed and arranged at the next idle point.
 *
 * ---------------------------------------------------------------------------- */
static int
ManageEntry(interp, tablePtr, tkwin, row, column, argc, argv)
    Tcl_Interp *interp;
    Table *tablePtr;
    Tk_Window tkwin;
    int row, column;
    int argc;
    char **argv;
{
    Entry *entryPtr;
    int result = TCL_OK;

    entryPtr = FindEntry(tablePtr, tkwin);
    if ((entryPtr != NULL) && (entryPtr->tablePtr != tablePtr)) {
	/* The entry for the widget already exists. If it's
	 * managed by another table, delete it.  */
	DestroyEntry(entryPtr);
	entryPtr = NULL;
    }
    if (entryPtr == NULL) {
	entryPtr = CreateEntry(tablePtr, tkwin);
	if (entryPtr == NULL) {
	    return TCL_ERROR;
	}
    }
    if (argc > 0) {
	result = Tk_ConfigureWidget(tablePtr->interp, entryPtr->tkwin,
	    entryConfigSpecs, argc, argv, (char *)entryPtr,
	    TK_CONFIG_ARGV_ONLY);
    }
    if ((entryPtr->column.span < 1) || (entryPtr->row.span < 1)) {
	Tcl_AppendResult(tablePtr->interp, "bad span specified for \"",
	    Tk_PathName(tkwin), "\"", (char *)NULL);
	DestroyEntry(entryPtr);
	return TCL_ERROR;
    }
    entryPtr->column.rcPtr = InitSpan(&(tablePtr->columnInfo), column,
	entryPtr->column.span);
    entryPtr->row.rcPtr = InitSpan(&(tablePtr->rowInfo), row,
	entryPtr->row.span);
    /*
     * Insert the entry into both the row and column layout lists
     */
    BinEntry(tablePtr, entryPtr);

    return result;
}

/*
 * ----------------------------------------------------------------------------
 *
 * BuildTable --
 *
 *	Processes an argv/argc list of table entries to add and
 *	configure new widgets into the table.  A table entry consists
 *	of the widget path name, table index, and optional
 *	configuration options.  The first argument in the argv list is
 *	the name of the table.  If no table exists for the given
 *	widget, a new one is created.
 *
 * Results:
 *	Returns a standard Tcl result.  If an error occurred,
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side Effects:
 *	Memory is allocated, a new table is possibly created, etc.
 *	The table is re-computed and arranged at the next idle point.
 *
 * ----------------------------------------------------------------------------
 */
static int
BuildTable(tablePtr, interp, argc, argv)
    Table *tablePtr;		/* Table to manage new widgets */
    Tcl_Interp *interp;		/* Interpreter to report errors back to */
    int argc;			/*  */
    char **argv;		/* List of widgets, indices, and options */
{
    Tk_Window tkwin;
    int row, column;
    int nextRow, nextColumn;
    register int i;

    /* Process any options specific to the table */
    for (i = 2; i < argc; i += 2) {
	if (argv[i][0] != '-') {
	    break;
	}
    }
    if (i > argc) {
	i = argc;
    }
    if (i > 2) {
	if (ConfigureTable(tablePtr, interp, i - 2, argv + 2) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    nextRow = tablePtr->nRows;
    nextColumn = 0;
    argc -= i, argv += i;
    while (argc > 0) {
	/*
	 * Allow the name of the widget and row/column index to be
	 * specified in any order.
	 */
	if (argv[0][0] == '.') {
	    tkwin = Tk_NameToWindow(interp, argv[0], tablePtr->tkwin);
	    if (tkwin == NULL) {
		return TCL_ERROR;
	    }
	    if ((argc == 1) || (argv[1][0] == '-')) {
		/* No row,column index, use defaults instead */
		row = nextRow, column = nextColumn;
		argc--, argv++;
	    } else {
		if (ParseIndex(interp, argv[1], &row, &column) != TCL_OK) {
		    return TCL_ERROR;	/* Invalid row,column index */
		}
		/* Skip over the widget pathname and table index. */
		argc -= 2, argv += 2;
	    }
	} else {
	    if (ParseIndex(interp, argv[0], &row, &column) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (argc == 1) {
		Tcl_AppendResult(interp, "missing widget pathname after \"",
			 argv[0], "\"", (char *)NULL);
		return TCL_ERROR;
	    }
	    tkwin = Tk_NameToWindow(interp, argv[1], tablePtr->tkwin);
	    if (tkwin == NULL) {
		return TCL_ERROR;
	    }
	    /* Skip over the widget pathname and table index. */
	    argc -= 2, argv += 2;
	}

	/* Find the end of the widget's option-value pairs */
	for (i = 0; i < argc; i += 2) {
	    if (argv[i][0] != '-') {
		break;
	    }
	}
	if (i > argc) {
	    i = argc;
	}
	if (ManageEntry(interp, tablePtr, tkwin, row,
		column, i, argv) != TCL_OK) {
	    return TCL_ERROR;
	}
	nextColumn = column + 1;
	argc -= i, argv += i;
    }
    /* Arrange for the new table layout to be calculated. */
    tablePtr->flags |= REQUEST_LAYOUT;
    EventuallyArrangeTable(tablePtr);

    Tcl_SetResult(interp, Tk_PathName(tablePtr->tkwin), TCL_VOLATILE);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ParseItem --
 *
 *	Parses a string representing an item in the table.  An item
 *	may be one of the following:
 *		Rn	- Row index, where n is the index of row
 *		Cn	- Column index, where n is the index of column
 *		r,c	- Cell index, where r is the row index and c
 *			  is the column index.
 *
 * Results:
 *	Returns a standard Tcl result.  If no error occurred, TCL_OK
 *	is returned.  *RowPtr* will return the row index.  *ColumnPtr*
 *	will return the column index.  If the row or column index is
 *	not applicable, -1 is returned via *rowPtr* or *columnPtr*.
 *
 * ----------------------------------------------------------------------------
 */
static int
ParseItem(tablePtr, string, rowPtr, columnPtr)
    Table *tablePtr;
    char *string;
    int *rowPtr, *columnPtr;
{
    char c;
    long partNum;

    c = tolower(string[0]);
    *rowPtr = *columnPtr = -1;
    if (c == 'r') {
	if (Tcl_ExprLong(tablePtr->interp, string + 1, &partNum) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((partNum < 0) || (partNum >= tablePtr->nRows)) {
	    Tcl_AppendResult(tablePtr->interp, "row index \"", string,
		"\" is out of range", (char *)NULL);
	    return TCL_ERROR;
	}
	*rowPtr = (int)partNum;
    } else if (c == 'c') {
	if (Tcl_ExprLong(tablePtr->interp, string + 1, &partNum) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((partNum < 0) || (partNum >= tablePtr->nColumns)) {
	    Tcl_AppendResult(tablePtr->interp, "column index \"", string,
		"\" is out of range", (char *)NULL);
	    return TCL_ERROR;
	}
	*columnPtr = (int)partNum;
    } else {
	if (ParseIndex(tablePtr->interp, string,
		rowPtr, columnPtr) != TCL_OK) {
	    return TCL_ERROR;	/* Invalid row,column index */
	}
	if ((*rowPtr < 0) || (*rowPtr >= tablePtr->nRows) ||
	    (*columnPtr < 0) || (*columnPtr >= tablePtr->nColumns)) {
	    Tcl_AppendResult(tablePtr->interp, "index \"", string,
		"\" is out of range", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * TranslateAnchor --
 *
 * 	Translate the coordinates of a given bounding box based upon
 * 	the anchor specified.  The anchor indicates where the given xy
 * 	position is in relation to the bounding box.
 *
 *  		nw --- n --- ne
 *  		|            |     x,y ---+
 *  		w   center   e      |     |
 *  		|            |      +-----+
 *  		sw --- s --- se
 *
 * Results:
 *	The translated coordinates of the bounding box are returned.
 *
 * ----------------------------------------------------------------------------
 */
static void
TranslateAnchor(dx, dy, anchor, xPtr, yPtr)
    int dx, dy;			/* Difference between outer and inner regions
				 */
    Tk_Anchor anchor;		/* Direction of the anchor */
    int *xPtr, *yPtr;
{
    int x, y;

    x = y = 0;
    switch (anchor) {
    case TK_ANCHOR_NW:		/* Upper left corner */
	break;
    case TK_ANCHOR_W:		/* Left center */
	y = (dy / 2);
	break;
    case TK_ANCHOR_SW:		/* Lower left corner */
	y = dy;
	break;
    case TK_ANCHOR_N:		/* Top center */
	x = (dx / 2);
	break;
    case TK_ANCHOR_CENTER:	/* Centered */
	x = (dx / 2);
	y = (dy / 2);
	break;
    case TK_ANCHOR_S:		/* Bottom center */
	x = (dx / 2);
	y = dy;
	break;
    case TK_ANCHOR_NE:		/* Upper right corner */
	x = dx;
	break;
    case TK_ANCHOR_E:		/* Right center */
	x = dx;
	y = (dy / 2);
	break;
    case TK_ANCHOR_SE:		/* Lower right corner */
	x = dx;
	y = dy;
	break;
    }
    *xPtr = (*xPtr) + x;
    *yPtr = (*yPtr) + y;
}

/*
 * ----------------------------------------------------------------------------
 *
 * GetReqWidth --
 *
 *	Returns the width requested by the widget starting in the
 *	given entry.  The requested space also includes any internal
 *	padding which has been designated for this widget.
 *
 *	The requested width of the widget is always bounded by the limits
 *	set in entryPtr->reqWidth.
 *
 * Results:
 *	Returns the requested width of the widget.
 *
 * ----------------------------------------------------------------------------
 */
static int
GetReqWidth(entryPtr)
    Entry *entryPtr;
{
    int width;

    width = Tk_ReqWidth(entryPtr->tkwin) + (2 * entryPtr->ipadX);
    width = GetBoundedWidth(width, &(entryPtr->reqWidth));
    return width;
}

/*
 * ----------------------------------------------------------------------------
 *
 * GetReqHeight --
 *
 *	Returns the height requested by the widget starting in the
 *	given entry.  The requested space also includes any internal
 *	padding which has been designated for this widget.
 *
 *	The requested height of the widget is always bounded by the
 *	limits set in entryPtr->reqHeight.
 *
 * Results:
 *	Returns the requested height of the widget.
 *
 * ----------------------------------------------------------------------------
 */
static int
GetReqHeight(entryPtr)
    Entry *entryPtr;
{
    int height;

    height = Tk_ReqHeight(entryPtr->tkwin) + (2 * entryPtr->ipadY);
    height = GetBoundedHeight(height, &(entryPtr->reqHeight));
    return height;
}

/*
 * ----------------------------------------------------------------------------
 *
 * GetTotalSpan --
 *
 *	Sums the row/column space requirements for the entire table.
 *
 * Results:
 *	Returns the space currently used in the span of partitions.
 *
 * ----------------------------------------------------------------------------
 */
static int
GetTotalSpan(infoPtr)
    PartitionInfo *infoPtr;
{
    register int spaceUsed;
    Blt_ChainLink *linkPtr;
    RowColumn *rcPtr;		/* Start of partitions */

    spaceUsed = 0;
    for (linkPtr = Blt_ChainFirstLink(infoPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	spaceUsed += rcPtr->size;
    }
    return spaceUsed;
}

/*
 * ----------------------------------------------------------------------------
 *
 * GetSpan --
 *
 *	Determines the space used by rows/columns for an entry.
 *
 * Results:
 *	Returns the space currently used in the span of partitions.
 *
 * ----------------------------------------------------------------------------
 */
static int
GetSpan(infoPtr, entryPtr)
    PartitionInfo *infoPtr;
    Entry *entryPtr;
{
    RowColumn *startPtr;
    register int spaceUsed;
    int count;
    Blt_ChainLink *linkPtr;
    RowColumn *rcPtr;		/* Start of partitions */
    int span;			/* Number of partitions spanned */

    if (infoPtr->type == rowUid) {
	rcPtr = entryPtr->row.rcPtr;
	span = entryPtr->row.span;
    } else {
	rcPtr = entryPtr->column.rcPtr;
	span = entryPtr->column.span;
    }

    count = spaceUsed = 0;
    linkPtr = rcPtr->linkPtr;
    startPtr = Blt_ChainGetValue(linkPtr);
    for ( /*empty*/ ; (linkPtr != NULL) && (count < span);
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	spaceUsed += rcPtr->size;
	count++;
    }
    /*
     * Subtract off the padding on either side of the span, since the
     * widget can't grow into it.
     */
    spaceUsed -= (startPtr->pad.side1 + rcPtr->pad.side2 + infoPtr->ePad);
    return spaceUsed;
}

/*
 * ----------------------------------------------------------------------------
 *
 * GrowSpan --
 *
 *	Expand the span by the amount of the extra space needed.  This
 *	procedure is used in LayoutPartitions to grow the partitions
 *	to their minimum nominal size, starting from a zero width and
 *	height space.
 *
 *	This looks more complicated than it really is.  The idea is to
 *	make the size of the partitions correspond to the smallest
 *	entry spans.  For example, if widget A is in column 1 and
 *	widget B spans both columns 0 and 1, any extra space needed to
 *	fit widget B should come from column 0.
 *
 *	On the first pass we try to add space to partitions which have
 *	not been touched yet (i.e. have no nominal size).  Since the
 *	row and column lists are sorted in ascending order of the
 *	number of rows or columns spanned, the space is distributed
 *	amongst the smallest spans first.
 *
 *	The second pass handles the case of widgets which have the
 *	same span.  For example, if A and B, which span the same
 *	number of partitions are the only widgets to span column 1,
 *	column 1 would grow to contain the bigger of the two slices of
 *	space.
 *
 *	If there is still extra space after the first two passes, this
 *	means that there were no partitions of with no widget spans or
 *	the same order span that could be expanded. The third pass
 *	will try to remedy this by parcelling out the left over space
 *	evenly among the rest of the partitions.
 *
 *	On each pass, we have to keep iterating over the span, evenly
 *	doling out slices of extra space, because we may hit partition
 *	limits as space is donated.  In addition, if there are left
 *	over pixels because of round-off, this will distribute them as
 *	evenly as possible.  For the worst case, it will take *span*
 *	passes to expand the span.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The partitions in the span may be expanded.
 *
 * ----------------------------------------------------------------------------
 */
static void
GrowSpan(infoPtr, entryPtr, growth)
    PartitionInfo *infoPtr;
    Entry *entryPtr;
    int growth;			/* The amount of extra space needed to
				 * grow the span. */
{
    register RowColumn *rcPtr;
    Blt_ChainLink *linkPtr;
    int spaceLeft, ration;
    int nOpen;			/* # of partitions with space available */
    register int n;
    RowColumn *startPtr;	/* Starting (column/row) partition  */
    int span;			/* Number of partitions in the span */

    if (infoPtr->type == rowUid) {
	startPtr = entryPtr->row.rcPtr;
	span = entryPtr->row.span;
    } else {
	startPtr = entryPtr->column.rcPtr;
	span = entryPtr->column.span;
    }

    /*
     * ------------------------------------------------------------------------
     *
     * Pass 1: First add space to rows/columns that haven't determined
     *	       their nominal sizes yet.
     *
     * ------------------------------------------------------------------------
     */

    nOpen = 0;
    /* Find out how many partitions have no size yet */
    linkPtr = startPtr->linkPtr;
    for (n = 0; n < span; n++) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	if ((rcPtr->nomSize == LIMITS_NOM) && (rcPtr->maxSize > rcPtr->size)) {
	    nOpen++;
	}
	linkPtr = Blt_ChainNextLink(linkPtr);
    }

    while ((nOpen > 0) && (growth > 0)) {
	ration = growth / nOpen;
	if (ration == 0) {
	    ration = 1;
	}
	linkPtr = startPtr->linkPtr;
	for (n = 0; (n < span) && (growth > 0); n++) {
	    rcPtr = Blt_ChainGetValue(linkPtr);
	    spaceLeft = rcPtr->maxSize - rcPtr->size;
	    if ((rcPtr->nomSize == LIMITS_NOM) && (spaceLeft > 0)) {
		if (ration < spaceLeft) {
		    growth -= ration;
		    rcPtr->size += ration;
		} else {
		    growth -= spaceLeft;
		    rcPtr->size += spaceLeft;
		    nOpen--;
		}
		rcPtr->minSpan = span;
		rcPtr->control = entryPtr;
	    }
	    linkPtr = Blt_ChainNextLink(linkPtr);
	}
    }

    /*
     * ------------------------------------------------------------------------
     *
     * Pass 2: Add space to partitions which have the same minimum span
     *
     * ------------------------------------------------------------------------
     */

    nOpen = 0;
    linkPtr = startPtr->linkPtr;
    for (n = 0; n < span; n++) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	if ((rcPtr->minSpan == span) && (rcPtr->maxSize > rcPtr->size)) {
	    nOpen++;
	}
	linkPtr = Blt_ChainNextLink(linkPtr);
    }
    while ((nOpen > 0) && (growth > 0)) {
	ration = growth / nOpen;
	if (ration == 0) {
	    ration = 1;
	}
	linkPtr = startPtr->linkPtr;
	for (n = 0; (n < span) && (growth > 0); n++) {
	    rcPtr = Blt_ChainGetValue(linkPtr);
	    spaceLeft = rcPtr->maxSize - rcPtr->size;
	    if ((rcPtr->minSpan == span) && (spaceLeft > 0)) {
		if (ration < spaceLeft) {
		    growth -= ration;
		    rcPtr->size += ration;
		} else {
		    growth -= spaceLeft;
		    rcPtr->size += spaceLeft;
		    nOpen--;
		}
		rcPtr->control = entryPtr;
	    }
	    linkPtr = Blt_ChainNextLink(linkPtr);
	}
    }

    /*
     * ------------------------------------------------------------------------
     *
     * Pass 3: Try to expand all the partitions with space still available
     *
     * ------------------------------------------------------------------------
     */

    /* Find out how many partitions still have space available */
    nOpen = 0;
    linkPtr = startPtr->linkPtr;
    for (n = 0; n < span; n++) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	if ((rcPtr->resize & RESIZE_EXPAND) && (rcPtr->maxSize > rcPtr->size)) {
	    nOpen++;
	}
	/* Set the nominal size of the row/column. */
	rcPtr->nomSize = rcPtr->size;
	linkPtr = Blt_ChainNextLink(linkPtr);
    }
    while ((nOpen > 0) && (growth > 0)) {
	ration = growth / nOpen;
	if (ration == 0) {
	    ration = 1;
	}
	linkPtr = startPtr->linkPtr;
	for (n = 0; (n < span) && (growth > 0); n++) {
	    rcPtr = Blt_ChainGetValue(linkPtr);
	    linkPtr = Blt_ChainNextLink(linkPtr);
	    if (!(rcPtr->resize & RESIZE_EXPAND)) {
		continue;
	    }
	    spaceLeft = rcPtr->maxSize - rcPtr->size;
	    if (spaceLeft > 0) {
		if (ration < spaceLeft) {
		    growth -= ration;
		    rcPtr->size += ration;
		} else {
		    growth -= spaceLeft;
		    rcPtr->size += spaceLeft;
		    nOpen--;
		}
		rcPtr->nomSize = rcPtr->size;
		rcPtr->control = entryPtr;
	    }
	}
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * AdjustPartitions --
 *
 *	Adjust the span by the amount of the extra space needed.  If
 *	the amount (adjustSpace) is negative, shrink the span,
 *	otherwise expand it.  Size constraints on the partitions may
 *	prevent any or all of the spacing adjustments.
 *
 *	This is very much like the GrowSpan procedure, but in this
 *	case we are shrinking or expanding all the (row or column)
 *	partitions. It uses a two pass approach, first giving space to
 *	partitions which not are smaller/larger than their nominal
 *	sizes. This is because constraints on the partitions may cause
 *	resizing to be non-linear.
 *
 *	If there is still extra space, this means that all partitions
 *	are at least to their nominal sizes.  The second pass will try
 *	to add/remove the left over space evenly among all the
 *	partitions which still have space available.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The size of the partitions in the span may be increased or
 *	decreased.
 *
 * ----------------------------------------------------------------------------
 */
static void
AdjustPartitions(infoPtr, adjustment)
    PartitionInfo *infoPtr;	/* Array of (column/row) partitions  */
    int adjustment;		/* The amount of extra space to grow or shrink
				 * the span. If negative, it represents the
				 * amount of space to remove */
{
    register RowColumn *rcPtr;
    int ration;			/* Amount of space to ration to each
				 * row/column. */
    int delta;			/* Amount of space needed */
    int spaceLeft;		/* Amount of space still available */
    int size;			/* Amount of space requested for a particular
				 * row/column. */
    int nOpen;			/* Number of rows/columns that still can
				 * be adjusted. */
    Blt_Chain *chainPtr;
    Blt_ChainLink *linkPtr;
    double totalWeight;

    chainPtr = infoPtr->chainPtr;

    /*
     * ------------------------------------------------------------------------
     *
     * Pass 1: First adjust the size of rows/columns that still haven't
     *	      reached their nominal size.
     *
     * ------------------------------------------------------------------------
     */
    delta = adjustment;

    nOpen = 0;
    totalWeight = 0.0;
    for (linkPtr = Blt_ChainFirstLink(chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	if (rcPtr->weight > 0.0) {
	    if (delta < 0) {
		spaceLeft = rcPtr->size - rcPtr->nomSize;
	    } else {
		spaceLeft = rcPtr->nomSize - rcPtr->size;
	    }
	    if (spaceLeft > 0) {
		nOpen++;
		totalWeight += rcPtr->weight;
	    }
	}
    }

    while ((nOpen > 0) && (totalWeight > 0.0) && (delta != 0)) {
	ration = (int)(delta / totalWeight);
	if (ration == 0) {
	    ration = (delta > 0) ? 1 : -1;
	}
	for (linkPtr = Blt_ChainFirstLink(chainPtr);
	    (linkPtr != NULL) && (delta != 0);
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    rcPtr = Blt_ChainGetValue(linkPtr);
	    if (rcPtr->weight > 0.0) {
		spaceLeft = rcPtr->nomSize - rcPtr->size;
		if (((delta > 0) && (spaceLeft > 0)) ||
		    ((delta < 0) && (spaceLeft < 0))) {
		    size = (int)(ration * rcPtr->weight);
		    if (size > delta) {
			size = delta;
		    }
		    if (ABS(size) < ABS(spaceLeft)) {
			delta -= size;
			rcPtr->size += size;
		    } else {
			delta -= spaceLeft;
			rcPtr->size += spaceLeft;
			nOpen--;
			totalWeight -= rcPtr->weight;
		    }
		}
	    }
	}
    }
    /*
     * ------------------------------------------------------------------------
     *
     * Pass 2: Adjust the partitions with space still available
     *
     * ------------------------------------------------------------------------
     */

    nOpen = 0;
    totalWeight = 0.0;
    for (linkPtr = Blt_ChainFirstLink(chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	if (rcPtr->weight > 0.0) {
	    if (delta > 0) {
		spaceLeft = rcPtr->maxSize - rcPtr->size;
	    } else {
		spaceLeft = rcPtr->size - rcPtr->minSize;
	    }
	    if (spaceLeft > 0) {
		nOpen++;
		totalWeight += rcPtr->weight;
	    }
	}
    }
    while ((nOpen > 0) && (totalWeight > 0.0) && (delta != 0)) {
	ration = (int)(delta / totalWeight);
	if (ration == 0) {
	    ration = (delta > 0) ? 1 : -1;
	}
	linkPtr = Blt_ChainFirstLink(chainPtr);
	for ( /*empty*/ ; (linkPtr != NULL) && (delta != 0);
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    rcPtr = Blt_ChainGetValue(linkPtr);
	    if (rcPtr->weight > 0.0) {
		if (delta > 0) {
		    spaceLeft = rcPtr->maxSize - rcPtr->size;
		} else {
		    spaceLeft = rcPtr->minSize - rcPtr->size;
		}
		if (((delta > 0) && (spaceLeft > 0)) ||
		    ((delta < 0) && (spaceLeft < 0))) {
		    size = (int)(ration * rcPtr->weight);
		    if (size > delta) {
			size = delta;
		    }
		    if (ABS(size) < ABS(spaceLeft)) {
			delta -= size;
			rcPtr->size += size;
		    } else {
			delta -= spaceLeft;
			rcPtr->size += spaceLeft;
			nOpen--;
			totalWeight -= rcPtr->weight;
		    }
		}
	    }
	}
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * ResetPartitions --
 *
 *	Sets/resets the size of each row and column partition to the
 *	minimum limit of the partition (this is usually zero). This
 *	routine gets called when new widgets are added, deleted, or
 *	resized.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The size of each partition is re-initialized to its minimum
 * 	size.
 *
 * ----------------------------------------------------------------------------
 */
static void
ResetPartitions(tablePtr, infoPtr, limitsProc)
    Table *tablePtr;
    PartitionInfo *infoPtr;
    LimitsProc *limitsProc;
{
    register RowColumn *rcPtr;
    register Blt_ChainLink *linkPtr;
    int pad, size;

    for (linkPtr = Blt_ChainFirstLink(infoPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);

	/*
	 * The constraint procedure below also has the desired
	 * side-effect of setting the minimum, maximum, and nominal
	 * values to the requested size of its associated widget (if
	 * one exists).
	 */
	size = (*limitsProc) (0, &(rcPtr->reqSize));

	pad = PADDING(rcPtr->pad) + infoPtr->ePad;
	if (rcPtr->reqSize.flags & LIMITS_SET_NOM) {

	    /*
	     * This could be done more cleanly.  We want to ensure
	     * that the requested nominal size is not overridden when
	     * determining the normal sizes.  So temporarily fix min
	     * and max to the nominal size and reset them back later.
	     */
	    rcPtr->minSize = rcPtr->maxSize = rcPtr->size =
		rcPtr->nomSize = size + pad;

	} else {
	    /* The range defaults to 0..MAXINT */
	    rcPtr->minSize = rcPtr->reqSize.min + pad;
	    rcPtr->maxSize = rcPtr->reqSize.max + pad;
	    rcPtr->nomSize = LIMITS_NOM;
	    rcPtr->size = pad;
	}
	rcPtr->minSpan = 0;
	rcPtr->control = NULL;
	rcPtr->count = 0;
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * SetNominalSizes
 *
 *	Sets the normal sizes for each partition.  The partition size
 *	is the requested widget size plus an amount of padding.  In
 *	addition, adjust the min/max bounds of the partition depending
 *	upon the resize flags (whether the partition can be expanded
 *	or shrunk from its normal size).
 *
 * Results:
 *	Returns the total space needed for the all the partitions.
 *
 * Side Effects:
 *	The nominal size of each partition is set.  This is later used
 *	to determine how to shrink or grow the table if the container
 *	can't be resized to accommodate the exact size requirements
 *	of all the partitions.
 *
 * ----------------------------------------------------------------------------
 */
static int
SetNominalSizes(tablePtr, infoPtr)
    Table *tablePtr;
    PartitionInfo *infoPtr;
{
    register RowColumn *rcPtr;
    Blt_ChainLink *linkPtr;
    int pad, size, total;

    total = 0;
    for (linkPtr = Blt_ChainFirstLink(infoPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	pad = PADDING(rcPtr->pad) + infoPtr->ePad;

	/*
	 * Restore the real bounds after temporarily setting nominal
	 * size.  These values may have been set in ResetPartitions to
	 * restrict the size of the paritition to the requested range.
	 */

	rcPtr->minSize = rcPtr->reqSize.min + pad;
	rcPtr->maxSize = rcPtr->reqSize.max + pad;

	size = rcPtr->size;
	if (size > rcPtr->maxSize) {
	    size = rcPtr->maxSize;
	} else if (size < rcPtr->minSize) {
	    size = rcPtr->minSize;
	}
	if ((infoPtr->ePad > 0) && (size < tablePtr->editPtr->minSize)) {
	    size = tablePtr->editPtr->minSize;
	}
	rcPtr->nomSize = rcPtr->size = size;

	/*
	 * If a partition can't be resized (to either expand or
	 * shrink), hold its respective limit at its normal size.
	 */
	if (!(rcPtr->resize & RESIZE_EXPAND)) {
	    rcPtr->maxSize = rcPtr->nomSize;
	}
	if (!(rcPtr->resize & RESIZE_SHRINK)) {
	    rcPtr->minSize = rcPtr->nomSize;
	}
	if (rcPtr->control == NULL) {
	    /* If a row/column contains no entries, then its size
	     * should be locked. */
	    if (rcPtr->resize & RESIZE_VIRGIN) {
		rcPtr->maxSize = rcPtr->minSize = size;
	    } else {
		if (!(rcPtr->resize & RESIZE_EXPAND)) {
		    rcPtr->maxSize = size;
		}
		if (!(rcPtr->resize & RESIZE_SHRINK)) {
		    rcPtr->minSize = size;
		}
	    }
	    rcPtr->nomSize = size;
	}
	total += rcPtr->nomSize;
    }
    return total;
}

/*
 * ----------------------------------------------------------------------------
 *
 * LockPartitions
 *
 *	Sets the maximum size of a row or column, if the partition
 *	has a widget that controls it.
 *
 * Results:
 *	None.
 *
 * ----------------------------------------------------------------------------
 */
static void
LockPartitions(infoPtr)
    PartitionInfo *infoPtr;
{
    register RowColumn *rcPtr;
    Blt_ChainLink *linkPtr;

    for (linkPtr = Blt_ChainFirstLink(infoPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	if (rcPtr->control != NULL) {
	    /* Partition is controlled by this widget */
	    rcPtr->maxSize = rcPtr->size;
	}
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * LayoutPartitions --
 *
 *	Calculates the normal space requirements for both the row and
 *	column partitions.  Each widget is added in order of the
 *	number of rows or columns spanned, which defines the space
 *	needed among in the partitions spanned.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *
 * 	The sum of normal sizes set here will be used as the normal size
 * 	for the container widget.
 *
 * ----------------------------------------------------------------------------
 */
static void
LayoutPartitions(tablePtr)
    Table *tablePtr;
{
    register Blt_ListNode node;
    Blt_Chain *chainPtr;
    Blt_ChainLink *linkPtr;
    register Entry *entryPtr;
    int needed, used, total;
    PartitionInfo *infoPtr;

    infoPtr = &(tablePtr->columnInfo);

    ResetPartitions(tablePtr, infoPtr, GetBoundedWidth);

    for (node = Blt_ListFirstNode(infoPtr->list); node != NULL;
	node = Blt_ListNextNode(node)) {
	chainPtr = (Blt_Chain *) Blt_ListGetValue(node);

	for (linkPtr = Blt_ChainFirstLink(chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    entryPtr = Blt_ChainGetValue(linkPtr);
	    if (entryPtr->column.control != CONTROL_FULL) {
		continue;
	    }
	    needed = GetReqWidth(entryPtr) + PADDING(entryPtr->padX) +
		2 * (entryPtr->borderWidth + tablePtr->eEntryPad);
	    if (needed <= 0) {
		continue;
	    }
	    used = GetSpan(infoPtr, entryPtr);
	    if (needed > used) {
		GrowSpan(infoPtr, entryPtr, needed - used);
	    }
	}
    }

    LockPartitions(infoPtr);

    for (node = Blt_ListFirstNode(infoPtr->list); node != NULL;
	node = Blt_ListNextNode(node)) {
	chainPtr = (Blt_Chain *) Blt_ListGetValue(node);

	for (linkPtr = Blt_ChainFirstLink(chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    entryPtr = Blt_ChainGetValue(linkPtr);

	    needed = GetReqWidth(entryPtr) + PADDING(entryPtr->padX) +
		2 * (entryPtr->borderWidth + tablePtr->eEntryPad);

	    if (entryPtr->column.control >= 0.0) {
		needed = (int)(needed * entryPtr->column.control);
	    }
	    if (needed <= 0) {
		continue;
	    }
	    used = GetSpan(infoPtr, entryPtr);
	    if (needed > used) {
		GrowSpan(infoPtr, entryPtr, needed - used);
	    }
	}
    }
    total = SetNominalSizes(tablePtr, infoPtr);
    tablePtr->normal.width = GetBoundedWidth(total, &(tablePtr->reqWidth)) +
	PADDING(tablePtr->padX) +
	2 * (tablePtr->eTablePad + Tk_InternalBorderWidth(tablePtr->tkwin));

    infoPtr = &(tablePtr->rowInfo);

    ResetPartitions(tablePtr, infoPtr, GetBoundedHeight);

    for (node = Blt_ListFirstNode(infoPtr->list); node != NULL;
	node = Blt_ListNextNode(node)) {
	chainPtr = (Blt_Chain *) Blt_ListGetValue(node);

	for (linkPtr = Blt_ChainFirstLink(chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    entryPtr = Blt_ChainGetValue(linkPtr);
	    if (entryPtr->row.control != CONTROL_FULL) {
		continue;
	    }
	    needed = GetReqHeight(entryPtr) + PADDING(entryPtr->padY) +
		2 * (entryPtr->borderWidth + tablePtr->eEntryPad);
	    if (needed <= 0) {
		continue;
	    }
	    used = GetSpan(infoPtr, entryPtr);
	    if (needed > used) {
		GrowSpan(infoPtr, entryPtr, needed - used);
	    }
	}
    }

    LockPartitions(&(tablePtr->rowInfo));

    for (node = Blt_ListFirstNode(infoPtr->list); node != NULL;
	node = Blt_ListNextNode(node)) {
	chainPtr = Blt_ChainGetValue(node);

	for (linkPtr = Blt_ChainFirstLink(chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    entryPtr = Blt_ChainGetValue(linkPtr);
	    needed = GetReqHeight(entryPtr) + PADDING(entryPtr->padY) +
		2 * (entryPtr->borderWidth + tablePtr->eEntryPad);
	    if (entryPtr->row.control >= 0.0) {
		needed = (int)(needed * entryPtr->row.control);
	    }
	    if (needed <= 0) {
		continue;
	    }
	    used = GetSpan(infoPtr, entryPtr);
	    if (needed > used) {
		GrowSpan(infoPtr, entryPtr, needed - used);
	    }
	}
    }
    total = SetNominalSizes(tablePtr, infoPtr);
    tablePtr->normal.height = GetBoundedHeight(total, &(tablePtr->reqHeight)) +
	PADDING(tablePtr->padY) +
	2 * (tablePtr->eTablePad + Tk_InternalBorderWidth(tablePtr->tkwin));
}

/*
 * ----------------------------------------------------------------------------
 *
 * ArrangeEntries
 *
 *	Places each widget at its proper location.  First determines
 *	the size and position of the each widget.  It then considers the
 *	following:
 *
 *	  1. translation of widget position its parent widget.
 *	  2. fill style
 *	  3. anchor
 *	  4. external and internal padding
 *	  5. widget size must be greater than zero
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The size of each partition is re-initialized its minimum size.
 *
 * ----------------------------------------------------------------------------
 */
static void
ArrangeEntries(tablePtr)
    Table *tablePtr;		/* Table widget structure */
{
    register Blt_ChainLink *linkPtr;
    register Entry *entryPtr;
    register int spanWidth, spanHeight;
    int x, y;
    int winWidth, winHeight;
    int dx, dy;
    int maxX, maxY;
    int extra;

    maxX = tablePtr->container.width -
	(Tk_InternalBorderWidth(tablePtr->tkwin) + tablePtr->padRight +
	tablePtr->eTablePad);
    maxY = tablePtr->container.height -
	(Tk_InternalBorderWidth(tablePtr->tkwin) + tablePtr->padBottom +
	tablePtr->eTablePad);

    for (linkPtr = Blt_ChainFirstLink(tablePtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	entryPtr = Blt_ChainGetValue(linkPtr);

	x = entryPtr->column.rcPtr->offset +
	    entryPtr->column.rcPtr->pad.side1 +
	    entryPtr->padLeft +
	    Tk_Changes(entryPtr->tkwin)->border_width +
	    tablePtr->eEntryPad;
	y = entryPtr->row.rcPtr->offset +
	    entryPtr->row.rcPtr->pad.side1 +
	    entryPtr->padTop +
	    Tk_Changes(entryPtr->tkwin)->border_width +
	    tablePtr->eEntryPad;

	/*
	 * Unmap any widgets that start beyond of the right edge of
	 * the container.
	 */
	if ((x >= maxX) || (y >= maxY)) {
	    if (Tk_IsMapped(entryPtr->tkwin)) {
		if (Tk_Parent(entryPtr->tkwin) != tablePtr->tkwin) {
		    Tk_UnmaintainGeometry(entryPtr->tkwin, tablePtr->tkwin);
		}
		Tk_UnmapWindow(entryPtr->tkwin);
	    }
	    continue;
	}
	extra = 2 * (entryPtr->borderWidth + tablePtr->eEntryPad);
	spanWidth = GetSpan(&(tablePtr->columnInfo), entryPtr) -
	    (extra + PADDING(entryPtr->padX));
	spanHeight = GetSpan(&(tablePtr->rowInfo), entryPtr) -
	    (extra + PADDING(entryPtr->padY));

	winWidth = GetReqWidth(entryPtr);
	winHeight = GetReqHeight(entryPtr);

	/*
	 *
	 * Compare the widget's requested size to the size of the span.
	 *
	 * 1) If the widget is larger than the span or if the fill flag
	 *    is set, make the widget the size of the span. Check that the
	 *    new size is within the bounds set for the widget.
	 *
	 * 2) Otherwise, position the widget in the space according to its
	 *    anchor.
	 *
	 */
	if ((spanWidth <= winWidth) || (entryPtr->fill & FILL_X)) {
	    winWidth = spanWidth;
	    if (winWidth > entryPtr->reqWidth.max) {
		winWidth = entryPtr->reqWidth.max;
	    }
	}
	if ((spanHeight <= winHeight) || (entryPtr->fill & FILL_Y)) {
	    winHeight = spanHeight;
	    if (winHeight > entryPtr->reqHeight.max) {
		winHeight = entryPtr->reqHeight.max;
	    }
	}
	dx = dy = 0;
	if (spanWidth > winWidth) {
	    dx = (spanWidth - winWidth);
	}
	if (spanHeight > winHeight) {
	    dy = (spanHeight - winHeight);
	}
	if ((dx > 0) || (dy > 0)) {
	    TranslateAnchor(dx, dy, entryPtr->anchor, &x, &y);
	}
	/*
	 * Clip the widget at the bottom and/or right edge of the
	 * container.
	 */
	if (winWidth > (maxX - x)) {
	    winWidth = (maxX - x);
	}
	if (winHeight > (maxY - y)) {
	    winHeight = (maxY - y);
	}

	/*
	 * If the widget is too small (i.e. it has only an external
	 * border) then unmap it.
	 */
	if ((winWidth < 1) || (winHeight < 1)) {
	    if (Tk_IsMapped(entryPtr->tkwin)) {
		if (tablePtr->tkwin != Tk_Parent(entryPtr->tkwin)) {
		    Tk_UnmaintainGeometry(entryPtr->tkwin, tablePtr->tkwin);
		}
		Tk_UnmapWindow(entryPtr->tkwin);
	    }
	    continue;
	}

	/*
	 * Resize and/or move the widget as necessary.
	 */
	entryPtr->x = x;
	entryPtr->y = y;

	if (tablePtr->tkwin != Tk_Parent(entryPtr->tkwin)) {
	    Tk_MaintainGeometry(entryPtr->tkwin, tablePtr->tkwin, x, y,
		winWidth, winHeight);
	} else {
	    if ((x != Tk_X(entryPtr->tkwin)) || 
		(y != Tk_Y(entryPtr->tkwin)) ||
		(winWidth != Tk_Width(entryPtr->tkwin)) ||
		(winHeight != Tk_Height(entryPtr->tkwin))) {
		Tk_MoveResizeWindow(entryPtr->tkwin, x, y, winWidth, winHeight);
	    }
	    if (!Tk_IsMapped(entryPtr->tkwin)) {
		Tk_MapWindow(entryPtr->tkwin);
	    }
	}
    }
}

/*
 * ----------------------------------------------------------------------------
 *
 * ArrangeTable --
 *
 *
 * Results:
 *	None.
 *
 * Side Effects:
 * 	The widgets in the table are possibly resized and redrawn.
 *
 * ----------------------------------------------------------------------------
 */
static void
ArrangeTable(clientData)
    ClientData clientData;
{
    Table *tablePtr = clientData;
    int width, height;
    int offset;
    int padX, padY;
    int outerPad;
    RowColumn *columnPtr, *rowPtr;
    Blt_ChainLink *linkPtr;

#ifdef notdef
    fprintf(stderr, "ArrangeTable(%s)\n", Tk_PathName(tablePtr->tkwin));
#endif
    Tcl_Preserve(tablePtr);
    tablePtr->flags &= ~ARRANGE_PENDING;

    tablePtr->rowInfo.ePad = tablePtr->columnInfo.ePad = tablePtr->eTablePad =
	tablePtr->eEntryPad = 0;
    if (tablePtr->editPtr != NULL) {
	tablePtr->rowInfo.ePad = tablePtr->columnInfo.ePad =
	    tablePtr->editPtr->gridLineWidth;
	tablePtr->eTablePad = tablePtr->editPtr->gridLineWidth;
	tablePtr->eEntryPad = tablePtr->editPtr->entryPad;
    }
    /*
     * If the table has no children anymore, then don't do anything at
     * all: just leave the container widget's size as-is.
     */
    if ((Blt_ChainGetLength(tablePtr->chainPtr) == 0) ||
	(tablePtr->tkwin == NULL)) {
	Tcl_Release(tablePtr);
	return;
    }
    if (tablePtr->flags & REQUEST_LAYOUT) {
	tablePtr->flags &= ~REQUEST_LAYOUT;
	LayoutPartitions(tablePtr);
    }
    /*
     * Initially, try to fit the partitions exactly into the container
     * by resizing the container.  If the widget's requested size is
     * different, send a request to the container widget's geometry
     * manager to resize.
     */
    if ((tablePtr->propagate) &&
	((tablePtr->normal.width != Tk_ReqWidth(tablePtr->tkwin)) ||
	    (tablePtr->normal.height != Tk_ReqHeight(tablePtr->tkwin)))) {
	Tk_GeometryRequest(tablePtr->tkwin, tablePtr->normal.width,
	    tablePtr->normal.height);
	EventuallyArrangeTable(tablePtr);
	Tcl_Release(tablePtr);
	return;
    }
    /*
     * Save the width and height of the container so we know when its
     * size has changed during ConfigureNotify events.
     */
    tablePtr->container.width = Tk_Width(tablePtr->tkwin);
    tablePtr->container.height = Tk_Height(tablePtr->tkwin);
    outerPad = 2 * (Tk_InternalBorderWidth(tablePtr->tkwin) +
	tablePtr->eTablePad);
    padX = outerPad + tablePtr->columnInfo.ePad + PADDING(tablePtr->padX);
    padY = outerPad + tablePtr->rowInfo.ePad + PADDING(tablePtr->padY);

    width = GetTotalSpan(&(tablePtr->columnInfo)) + padX;
    height = GetTotalSpan(&(tablePtr->rowInfo)) + padY;

    /*
     * If the previous geometry request was not fulfilled (i.e. the
     * size of the container is different from partitions' space
     * requirements), try to adjust size of the partitions to fit the
     * widget.
     */
    if (tablePtr->container.width != width) {
	AdjustPartitions(&(tablePtr->columnInfo),
	    tablePtr->container.width - width);
	width = GetTotalSpan(&(tablePtr->columnInfo)) + padX;
    }
    if (tablePtr->container.height != height) {
	AdjustPartitions(&(tablePtr->rowInfo),
	    tablePtr->container.height - height);
	height = GetTotalSpan(&(tablePtr->rowInfo)) + padY;
    }
    /*
     * If after adjusting the size of the partitions the space
     * required does not equal the size of the widget, do one of the
     * following:
     *
     * 1) If it's smaller, center the table in the widget.
     * 2) If it's bigger, clip the partitions that extend beyond
     *    the edge of the container.
     *
     * Set the row and column offsets (including the container's
     * internal border width). To be used later when positioning the
     * widgets.
     */
    offset = Tk_InternalBorderWidth(tablePtr->tkwin) + tablePtr->padLeft +
	tablePtr->eTablePad;
    if (width < tablePtr->container.width) {
	offset += (tablePtr->container.width - width) / 2;
    }
    for (linkPtr = Blt_ChainFirstLink(tablePtr->columnInfo.chainPtr);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	columnPtr = Blt_ChainGetValue(linkPtr);
	columnPtr->offset = offset + tablePtr->columnInfo.ePad;
	offset += columnPtr->size;
    }
    offset = Tk_InternalBorderWidth(tablePtr->tkwin) + tablePtr->padTop +
	tablePtr->eTablePad;
    if (height < tablePtr->container.height) {
	offset += (tablePtr->container.height - height) / 2;
    }
    for (linkPtr = Blt_ChainFirstLink(tablePtr->rowInfo.chainPtr);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	rowPtr = Blt_ChainGetValue(linkPtr);
	rowPtr->offset = offset + tablePtr->rowInfo.ePad;
	offset += rowPtr->size;
    }
    ArrangeEntries(tablePtr);
    if (tablePtr->editPtr != NULL) {
	/* Redraw the editor */
	(*tablePtr->editPtr->drawProc) (tablePtr->editPtr);
    }
    Tcl_Release(tablePtr);
}

/*
 * ----------------------------------------------------------------------------
 *
 * ArrangeOp --
 *
 *	Forces layout of the table geometry manager.  This is useful
 *	mostly for debugging the geometry manager.  You can get the
 *	geometry manager to calculate the normal (requested) width and
 *	height of each row and column.  Otherwise, you need to first
 *	withdraw the container widget, invoke "update", and then query
 *	the geometry manager.
 *
 * Results:
 *	Returns a standard Tcl result.  If the table is successfully
 *	rearranged, TCL_OK is returned. Otherwise, TCL_ERROR is returned
 *	and an error message is left in interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ArrangeOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;		/* Interpreter to report errors to */
    int argc;
    char **argv;		/* Path name of container associated with
				 * the table */
{
    Table *tablePtr;

    if (Blt_GetTable(dataPtr, interp, argv[2], &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    tablePtr->flags |= REQUEST_LAYOUT;
    ArrangeTable(tablePtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * CgetOp --
 *
 *	Returns the name, position and options of a widget in the table.
 *
 * Results:
 *	Returns a standard Tcl result.  A list of the widget attributes
 *	is left in interp->result.
 *
 * --------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
CgetOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Table *tablePtr;
    int length;
    char c;
    int n;
    PartitionInfo *infoPtr;

    if (Blt_GetTable(dataPtr, interp, argv[2], &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (argc == 4) {
	return Tk_ConfigureValue(interp, tablePtr->tkwin, tableConfigSpecs,
	    (char *)tablePtr, argv[3], 0);
    }
    c = argv[3][0];
    length = strlen(argv[3]);
    if (c == '.') {		/* Configure widget */
	Entry *entryPtr;

	if (GetEntry(interp, tablePtr, argv[3], &entryPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	return Tk_ConfigureValue(interp, entryPtr->tkwin, entryConfigSpecs,
	    (char *)entryPtr, argv[4], 0);
    } else if ((c == 'c') && (strncmp(argv[3], "container", length) == 0)) {
	return Tk_ConfigureValue(interp, tablePtr->tkwin, tableConfigSpecs,
	    (char *)tablePtr, argv[4], 0);
    }
    infoPtr = ParseRowColumn(tablePtr, argv[3], &n);
    if (infoPtr == NULL) {
	return TCL_ERROR;
    }
    return Tk_ConfigureValue(interp, tablePtr->tkwin, infoPtr->configSpecs,
	(char *)GetRowColumn(infoPtr, n), argv[4], 0);
}

/*
 * ----------------------------------------------------------------------------
 *
 * ConfigureOp --
 *
 *	Returns the name, position and options of a widget in the table.
 *
 * Results:
 *	Returns a standard Tcl result.  A list of the table configuration
 *	option information is left in interp->result.
 *
 * --------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ConfigureOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Table *tablePtr;
    int length;
    char c1, c2;
    int count;
    int result;
    char **items;
    register int i;

    if (Blt_GetTable(dataPtr, interp, argv[2], &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    /*
     * Find the end of the items. Search until we see an option (-).
     */
    argc -= 3, argv += 3;
    for (count = 0; count < argc; count++) {
	if (argv[count][0] == '-') {
	    break;
	}
    }
    items = argv;		/* Save the start of the item list */
    argc -= count;		/* Move beyond the items to the options */
    argv += count;

    result = TCL_ERROR;		/* Suppress compiler warning */

    if (count == 0) {
	result = ConfigureTable(tablePtr, interp, argc, argv);
    }
    for (i = 0; i < count; i++) {
	c1 = items[i][0];
	c2 = items[i][1];
	length = strlen(items[i]);
	if (c1 == '.') {		/* Configure widget */
	    Entry *entryPtr;

	    if (GetEntry(interp, tablePtr, items[i], &entryPtr) != TCL_OK) {
		return TCL_ERROR;
	    }
	    result = ConfigureEntry(tablePtr, interp, entryPtr, argc, argv);
	} else if ((c1 == 'r') || (c1 == 'R')) {
	    result = ConfigureRowColumn(tablePtr, &(tablePtr->rowInfo),
		items[i], argc, argv);
	} else if ((c1 == 'c') && (c2 == 'o') &&
	    (strncmp(argv[3], "container", length) == 0)) {
	    result = ConfigureTable(tablePtr, interp, argc, argv);
	} else if ((c1 == 'c') || (c1 == 'C')) {
	    result = ConfigureRowColumn(tablePtr, &(tablePtr->columnInfo),
		items[i], argc, argv);
	} else {
	    Tcl_AppendResult(interp, "unknown item \"", items[i],
		"\": should be widget, row or column index, or \"container\"",
		(char *)NULL);
	    return TCL_ERROR;
	}
	if (result == TCL_ERROR) {
	    break;
	}
	if ((i + 1) < count) {
	    Tcl_AppendResult(interp, "\n", (char *)NULL);
	}
    }
    tablePtr->flags |= REQUEST_LAYOUT;
    EventuallyArrangeTable(tablePtr);
    return result;
}

/*
 * ----------------------------------------------------------------------------
 *
 * DeleteOp --
 *
 *	Deletes the specified rows and/or columns from the table.
 *	Note that the row/column indices can be fixed only after
 *	all the deletions have occurred.
 *
 *		table delete .f r0 r1 r4 c0
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
DeleteOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Table *tablePtr;
    char c;
    Blt_ChainLink *linkPtr, *nextPtr;
    PartitionInfo *infoPtr;
    char string[200];
    int matches;
    register int i;
    RowColumn *rcPtr;

    if (Blt_GetTable(dataPtr, interp, argv[2], &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 3; i < argc; i++) {
	c = tolower(argv[i][0]);
	if ((c != 'r') && (c != 'c')) {
	    Tcl_AppendResult(interp, "bad index \"", argv[i],
		"\": must start with \"r\" or \"c\"", (char *)NULL);
	    return TCL_ERROR;
	}
    }
    matches = 0;
    for (i = 3; i < argc; i++) {
	c = tolower(argv[i][0]);
	infoPtr = (c == 'r') ? &(tablePtr->rowInfo) : &(tablePtr->columnInfo);
	for (linkPtr = Blt_ChainFirstLink(infoPtr->chainPtr); linkPtr != NULL;
	    linkPtr = nextPtr) {
	    nextPtr = Blt_ChainNextLink(linkPtr);
	    rcPtr = Blt_ChainGetValue(linkPtr);
	    sprintf(string, "%c%d", argv[i][0], rcPtr->index);
	    if (Tcl_StringMatch(string, argv[i])) {
		matches++;
		DeleteRowColumn(tablePtr, infoPtr, rcPtr);
		Blt_ChainDeleteLink(infoPtr->chainPtr, linkPtr);
	    }
	}
    }
    if (matches > 0) {		/* Fix indices */
	i = 0;
	for (linkPtr = Blt_ChainFirstLink(tablePtr->columnInfo.chainPtr);
	    linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	    rcPtr = Blt_ChainGetValue(linkPtr);
	    rcPtr->index = i++;
	}
	i = 0;
	for (linkPtr = Blt_ChainFirstLink(tablePtr->rowInfo.chainPtr);
	    linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	    rcPtr = Blt_ChainGetValue(linkPtr);
	    rcPtr->index = i++;
	}
	tablePtr->flags |= REQUEST_LAYOUT;
	EventuallyArrangeTable(tablePtr);
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * JoinOp --
 *
 *	Joins the specified span of rows/columns together into a
 *	partition.  The row/column indices can be fixed only after
 *	all the deletions have occurred.
 *
 *		table join .f r0 r3
 *		table join .f c2 c4
 * Results:
 *	Returns a standard Tcl result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
JoinOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Table *tablePtr;
    Blt_ChainLink *linkPtr, *nextPtr, *fromPtr;
    PartitionInfo *infoPtr, *info2Ptr;
    Entry *entryPtr;
    int from, to;		/* Indices marking the span of
				 * partitions to be joined together.  */
    int start, end;		/* Entry indices. */
    register int i;
    RowColumn *rcPtr;

    if (Blt_GetTable(dataPtr, interp, argv[2], &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    infoPtr = ParseRowColumn(tablePtr, argv[3], &from);
    if (infoPtr == NULL) {
	return TCL_ERROR;
    }
    info2Ptr = ParseRowColumn(tablePtr, argv[4], &to);
    if (info2Ptr == NULL) {
	return TCL_ERROR;
    }
    if (infoPtr != info2Ptr) {
	Tcl_AppendResult(interp,
	    "\"from\" and \"to\" must both be rows or columns",
	    (char *)NULL);
	return TCL_ERROR;
    }
    if (from >= to) {
	return TCL_OK;		/* No-op. */
    }
    fromPtr = Blt_ChainGetNthLink(infoPtr->chainPtr, from);
    rcPtr = Blt_ChainGetValue(fromPtr);

    /*
     * ---------------------------------------------------------------
     *
     *	Reduce the span of all entries that currently cross any of the
     *	trailing rows/columns.  Also, if the entry starts in one of
     *	these rows/columns, moved it to the designated "joined"
     *	row/column.
     *
     * ---------------------------------------------------------------
     */
    if (infoPtr->type == rowUid) {
	for (linkPtr = Blt_ChainFirstLink(tablePtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    entryPtr = Blt_ChainGetValue(linkPtr);
	    start = entryPtr->row.rcPtr->index + 1;
	    end = entryPtr->row.rcPtr->index + entryPtr->row.span - 1;
	    if ((end < from) || ((start > to))) {
		continue;
	    }
	    entryPtr->row.span -= to - start + 1;
	    if (start >= from) {/* Entry starts in a trailing partition. */
		entryPtr->row.rcPtr = rcPtr;
	    }
	}
    } else {
	for (linkPtr = Blt_ChainFirstLink(tablePtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    entryPtr = Blt_ChainGetValue(linkPtr);
	    start = entryPtr->column.rcPtr->index + 1;
	    end = entryPtr->column.rcPtr->index + entryPtr->column.span - 1;
	    if ((end < from) || ((start > to))) {
		continue;
	    }
	    entryPtr->column.span -= to - start + 1;
	    if (start >= from) {/* Entry starts in a trailing partition. */
		entryPtr->column.rcPtr = rcPtr;
	    }
	}
    }
    linkPtr = Blt_ChainNextLink(fromPtr);
    for (i = from + 1; i <= to; i++) {
	nextPtr = Blt_ChainNextLink(linkPtr);
	rcPtr = Blt_ChainGetValue(linkPtr);
	DeleteRowColumn(tablePtr, infoPtr, rcPtr);
	Blt_ChainDeleteLink(infoPtr->chainPtr, linkPtr);
	linkPtr = nextPtr;
    }
    i = 0;
    for (linkPtr = Blt_ChainFirstLink(infoPtr->chainPtr);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	rcPtr->index = i++;
    }
    tablePtr->flags |= REQUEST_LAYOUT;
    EventuallyArrangeTable(tablePtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ExtentsOp --
 *
 *	Returns a list of all the pathnames of the widgets managed by
 *	a table.  The table is determined from the name of the
 *	container widget associated with the table.
 *
 *		table extents .frame r0 c0 container
 *
 * Results:
 *	Returns a standard Tcl result.  If no error occurred, TCL_OK is
 *	returned and a list of widgets managed by the table is left in
 *	interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ExtentsOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;		/* Interpreter to return results to. */
    int argc;			/* # of arguments */
    char **argv;		/* Command line arguments. */
{
    Table *tablePtr;
    Blt_ChainLink *linkPtr;
    RowColumn *rcPtr;
    RowColumn *c1Ptr, *r1Ptr, *c2Ptr, *r2Ptr;
    PartitionInfo *infoPtr;
    int x, y, width, height;
    char string[200];
    char c;

    if (Blt_GetTable(dataPtr, interp, argv[2], &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    c = tolower(argv[3][0]);
    if (c == 'r') {
	infoPtr = &(tablePtr->rowInfo);
    } else if (c == 'c') {
	infoPtr = &(tablePtr->columnInfo);
    } else {
	Tcl_AppendResult(interp, "unknown item \"", argv[3],
	    "\": should be widget, row, or column", (char *)NULL);
	return TCL_ERROR;
    }
    for (linkPtr = Blt_ChainFirstLink(infoPtr->chainPtr);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	sprintf(string, "%c%d", argv[3][0], rcPtr->index);
	if (Tcl_StringMatch(string, argv[3])) {
	    if (c == 'r') {
		r1Ptr = r2Ptr = rcPtr;
		c1Ptr = GetRowColumn(&(tablePtr->columnInfo), 0);
		c2Ptr = GetRowColumn(&(tablePtr->columnInfo), 
				     tablePtr->nColumns - 1);
	    } else {
		c1Ptr = c2Ptr = rcPtr;
		r1Ptr = GetRowColumn(&(tablePtr->rowInfo), 0);
		r2Ptr = GetRowColumn(&(tablePtr->rowInfo), 
				     tablePtr->nRows - 1);
	    }
	    x = c1Ptr->offset;
	    y = r1Ptr->offset;
	    width = c2Ptr->offset + c2Ptr->size - x;
	    height = r2Ptr->offset + r2Ptr->size - y;
	    sprintf(string, "%c%d %d %d %d %d\n", argv[3][0], rcPtr->index,
		x, y, width, height);
	    Tcl_AppendResult(interp, string, (char *)NULL);
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ForgetOp --
 *
 *	Processes an argv/argc list of widget names and purges their
 *	entries from their respective tables.  The widgets are unmapped and
 *	the tables are rearranged at the next idle point.  Note that all
 *	the named widgets do not need to exist in the same table.
 *
 * Results:
 *	Returns a standard Tcl result.  If an error occurred, TCL_ERROR is
 *	returned and an error message is left in interp->result.
 *
 * Side Effects:
 *	Memory is deallocated (the entry is destroyed), etc.  The
 *	affected tables are is re-computed and arranged at the next idle
 *	point.
 *
 * ----------------------------------------------------------------------------
 */
static int
ForgetOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Entry *entryPtr;
    register int i;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Table *tablePtr;
    Tk_Window tkwin, mainWindow;
    
    tablePtr = NULL;
    mainWindow = Tk_MainWindow(interp);
    for (i = 2; i < argc; i++) {
	entryPtr = NULL;
	tkwin = Tk_NameToWindow(interp, argv[i], mainWindow);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	for (hPtr = Blt_FirstHashEntry(&(dataPtr->tableTable), &cursor);
	    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	    tablePtr = (Table *)Blt_GetHashValue(hPtr);
	    if (tablePtr->interp != interp) {
		continue;
	    }
	    entryPtr = FindEntry(tablePtr, tkwin);
	    if (entryPtr != NULL) {
		break;
	    }
	}
	if (entryPtr == NULL) {
	    Tcl_AppendResult(interp, "\"", argv[i],
		"\" is not managed by any table", (char *)NULL);
	    return TCL_ERROR;
	}
	if (Tk_IsMapped(entryPtr->tkwin)) {
	    Tk_UnmapWindow(entryPtr->tkwin);
	}
	/* Arrange for the call back here in the loop, because the
	 * widgets may not belong to the same table.  */
	tablePtr->flags |= REQUEST_LAYOUT;
	EventuallyArrangeTable(tablePtr);
	DestroyEntry(entryPtr);
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * InfoOp --
 *
 *	Returns the options of a widget or partition in the table.
 *
 * Results:
 *	Returns a standard Tcl result.  A list of the widget attributes
 *	is left in interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InfoOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Table *tablePtr;
    int result;
    char c;
    register int i;

    if (Blt_GetTable(dataPtr, interp, argv[2], &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    for (i = 3; i < argc; i++) {
	c = argv[i][0];
	if (c == '.') {		/* Entry information */
	    Entry *entryPtr;

	    if (GetEntry(interp, tablePtr, argv[i], &entryPtr) != TCL_OK) {
		return TCL_ERROR;
	    }
	    result = InfoEntry(interp, tablePtr, entryPtr);
	} else if ((c == 'r') || (c == 'R') || (c == 'c') || (c == 'C')) {
	    result = InfoRowColumn(tablePtr, interp, argv[i]);
	} else {
	    Tcl_AppendResult(interp, "unknown item \"", argv[i],
		"\": should be widget, row, or column", (char *)NULL);
	    return TCL_ERROR;
	}
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((i + 1) < argc) {
	    Tcl_AppendResult(interp, "\n", (char *)NULL);
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * InsertOp --
 *
 *	Inserts a span of rows/columns into the table.
 *
 *		table insert .f r0 2
 *		table insert .f c0 5
 *
 * Results:
 *	Returns a standard Tcl result.  A list of the widget
 *	attributes is left in interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
InsertOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{ 
    Table *tablePtr;
    long int span;
    int before;
    PartitionInfo *infoPtr;
    RowColumn *rcPtr;
    register int i;
    Blt_ChainLink *beforePtr, *linkPtr;
    int linkBefore;

    if (Blt_GetTable(dataPtr, interp, argv[2], &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    linkBefore = TRUE;
    if (argv[3][0] == '-') {
	if (strcmp(argv[3], "-before") == 0) {
	    linkBefore = TRUE;
	    argv++; argc--;
	} else if (strcmp(argv[3], "-after") == 0) {
	    linkBefore = FALSE;
	    argv++; argc--;
	}	    
    } 
    if (argc == 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
			 "insert ", argv[2], "row|column ?span?", (char *)NULL);
	return TCL_ERROR;
    }
    infoPtr = ParseRowColumn(tablePtr, argv[3], &before);
    if (infoPtr == NULL) {
	return TCL_ERROR;
    }
    span = 1;
    if ((argc > 4) && (Tcl_ExprLong(interp, argv[4], &span) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (span < 1) {
	Tcl_AppendResult(interp, "span value \"", argv[4],
	    "\" can't be negative", (char *)NULL);
	return TCL_ERROR;
    }
    beforePtr = Blt_ChainGetNthLink(infoPtr->chainPtr, before);
    /*
     * Insert the new rows/columns from the designated point in the
     * chain.
     */
    for (i = 0; i < span; i++) {
	rcPtr = CreateRowColumn();
	linkPtr = Blt_ChainNewLink();
	Blt_ChainSetValue(linkPtr, rcPtr);
	if (linkBefore) {
	    Blt_ChainLinkBefore(infoPtr->chainPtr, linkPtr, beforePtr);
	} else {
	    Blt_ChainLinkAfter(infoPtr->chainPtr, linkPtr, beforePtr);
	}
	rcPtr->linkPtr = linkPtr;
    }
    i = 0;
    for (linkPtr = Blt_ChainFirstLink(infoPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	/* Reset the indices of the trailing rows/columns.  */
	rcPtr->index = i++;
    }
    tablePtr->flags |= REQUEST_LAYOUT;
    EventuallyArrangeTable(tablePtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * SplitOp --
 *
 *	Splits a single row/column into multiple partitions. Any
 *	widgets that span this row/column will be automatically
 *	corrected to include the new rows/columns.
 *
 *		table split .f r0 3
 *		table split .f c2 2
 * Results:
 *	Returns a standard Tcl result.  A list of the widget
 *	attributes is left in interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SplitOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Table *tablePtr;
    int number, split;
    int start, end;
    PartitionInfo *infoPtr;
    RowColumn *rcPtr;
    register int i;
    Blt_ChainLink *afterPtr, *linkPtr;
    Entry *entryPtr;

    if (Blt_GetTable(dataPtr, interp, argv[2], &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    infoPtr = ParseRowColumn(tablePtr, argv[3], &number);
    if (infoPtr == NULL) {
	return TCL_ERROR;
    }
    split = 2;
    if (argc > 4) {
	if (Tcl_GetInt(interp, argv[4], &split) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (split < 2) {
	Tcl_AppendResult(interp, "bad split value \"", argv[4],
	    "\": should be 2 or greater", (char *)NULL);
	return TCL_ERROR;
    }
    afterPtr = Blt_ChainGetNthLink(infoPtr->chainPtr, number);

    /*
     * Append (split - 1) additional rows/columns starting
     * from the current point in the chain.
     */

    for (i = 1; i < split; i++) {
	rcPtr = CreateRowColumn();
	linkPtr = Blt_ChainNewLink();
	Blt_ChainSetValue(linkPtr, rcPtr);
	Blt_ChainLinkAfter(infoPtr->chainPtr, linkPtr, afterPtr);
	rcPtr->linkPtr = linkPtr;
    }

    /*
     * Also increase the span of all entries that span this
     * row/column by split - 1.
     */
    if (infoPtr->type == rowUid) {
	for (linkPtr = Blt_ChainFirstLink(tablePtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    entryPtr = Blt_ChainGetValue(linkPtr);
	    start = entryPtr->row.rcPtr->index;
	    end = entryPtr->row.rcPtr->index + entryPtr->row.span;
	    if ((start <= number) && (number < end)) {
		entryPtr->row.span += (split - 1);
	    }
	}
    } else {
	for (linkPtr = Blt_ChainFirstLink(tablePtr->chainPtr); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    entryPtr = Blt_ChainGetValue(linkPtr);
	    start = entryPtr->column.rcPtr->index;
	    end = entryPtr->column.rcPtr->index + entryPtr->column.span;
	    if ((start <= number) && (number < end)) {
		entryPtr->column.span += (split - 1);
	    }
	}
    }
    /*
     * Be careful to renumber the rows or columns only after
     * processing each entry.  Otherwise row/column numbering
     * will be out of sync with the index.
     */
    i = number;
    for (linkPtr = afterPtr; linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	rcPtr->index = i++;	/* Renumber the trailing indices.  */
    }

    tablePtr->flags |= REQUEST_LAYOUT;
    EventuallyArrangeTable(tablePtr);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------
 *
 * RowColumnSearch --
 *
 * 	Searches for the row or column designated by an x or y
 *	coordinate.
 *
 * Results:
 *	Returns a pointer to the row/column containing the given point.
 *	If no row/column contains the coordinate, NULL is returned.
 *
 * ----------------------------------------------------------------------
 */
static RowColumn *
RowColumnSearch(infoPtr, x)
    PartitionInfo *infoPtr;
    int x;			/* Search coordinate  */
{
    Blt_ChainLink *linkPtr;
    RowColumn *rcPtr;

    for (linkPtr = Blt_ChainFirstLink(infoPtr->chainPtr);
	linkPtr != NULL; linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	if (x > (rcPtr->offset + rcPtr->size)) {
	    break;		/* Too far, can't find row/column. */
	}
	if (x > rcPtr->offset) {
	    return rcPtr;
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * LocateOp --
 *
 *
 *	Returns the row,column index given a screen coordinate.
 *
 * Results:
 *	Returns a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static int
LocateOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    int x, y;
    RowColumn *rowPtr, *columnPtr;
    Table *tablePtr;

    if (Blt_GetTable(dataPtr, interp, argv[2], &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_GetPixels(interp, tablePtr->tkwin, argv[3], PIXELS_ANY, &x)
	!= TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_GetPixels(interp, tablePtr->tkwin, argv[4], PIXELS_ANY, &y)
	!= TCL_OK) {
	return TCL_ERROR;
    }
    rowPtr = RowColumnSearch(&(tablePtr->rowInfo), y);
    if (rowPtr == NULL) {
	return TCL_OK;
    }
    columnPtr = RowColumnSearch(&(tablePtr->columnInfo), x);
    if (columnPtr == NULL) {
	return TCL_OK;
    }
    Tcl_AppendElement(interp, Blt_Itoa(rowPtr->index));
    Tcl_AppendElement(interp, Blt_Itoa(columnPtr->index));
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * ContainersOp --
 *
 *	Returns a list of tables currently in use. A table is
 *	associated by the name of its container widget.  All tables
 *	matching a given pattern are included in this list.  If no
 *	pattern is present (argc == 0), all tables are included.
 *
 * Results:
 *	Returns a standard Tcl result.  If no error occurred, TCL_OK is
 *	returned and a list of tables is left in interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
ContainersOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;		/* Interpreter to return list of names to */
    int argc;
    char **argv;		/* Contains 0-1 arguments: search pattern */
{
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    register Table *tablePtr;
    char *pattern;

    pattern = NULL;
    if (argc > 2) {
	if (argv[2][0] == '-') {
	    unsigned int length;

	    length = strlen(argv[2]);
	    if ((length > 1) && (argv[2][1] == 'p') &&
		(strncmp(argv[2], "-pattern", length) == 0)) {
		pattern = argv[3];
		goto search;
	    } else if ((length > 1) && (argv[2][1] == 's') &&
		(strncmp(argv[2], "-slave", length) == 0)) {
		Tk_Window tkwin;

		if (argc != 4) {
		    Tcl_AppendResult(interp, "needs widget argument for \"",
			argv[2], "\"", (char *)NULL);
		    return TCL_ERROR;
		}
		tkwin = Tk_NameToWindow(interp, argv[3],
		    Tk_MainWindow(interp));
		if (tkwin == NULL) {
		    return TCL_ERROR;
		}
		for (hPtr = Blt_FirstHashEntry(&(dataPtr->tableTable), &cursor);
		    hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
		    tablePtr = (Table *)Blt_GetHashValue(hPtr);
		    if (FindEntry(tablePtr, tkwin) != NULL) {
			Tcl_AppendElement(interp, Tk_PathName(tablePtr->tkwin));
		    }
		}
		return TCL_OK;
	    } else {
		Tcl_AppendResult(interp, "bad switch \"", argv[2], "\" : \
should be \"-pattern\", or \"-slave\"", (char *)NULL);
		return TCL_ERROR;
	    }
	} else {
	    pattern = argv[2];
	}
    }
  search:
    for (hPtr = Blt_FirstHashEntry(&(dataPtr->tableTable), &cursor);
	hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	tablePtr = (Table *)Blt_GetHashValue(hPtr);
	if (tablePtr->interp == interp) {
	    if ((pattern == NULL) ||
		(Tcl_StringMatch(Tk_PathName(tablePtr->tkwin), pattern))) {
		Tcl_AppendElement(interp, Tk_PathName(tablePtr->tkwin));
	    }
	}
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * SaveOp --
 *
 *	Returns a list of all the commands necessary to rebuild the
 *	the table.  This includes the layout of the widgets and any
 *	row, column, or table options set.
 *
 * Results:
 *	Returns a standard Tcl result.  If no error occurred, TCL_OK is
 *	returned and a list of widget path names is left in interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SaveOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Table *tablePtr;
    Blt_ChainLink *linkPtr, *lastPtr;
    Entry *entryPtr;
    PartitionInfo *infoPtr;
    RowColumn *rcPtr;
    Tcl_DString dString;
    int start, last;

    if (Blt_GetTable(dataPtr, interp, argv[2], &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    Tcl_DStringInit(&dString);
    Tcl_DStringAppend(&dString, "\n# Table widget layout\n\n", -1);
    Tcl_DStringAppend(&dString, argv[0], -1);
    Tcl_DStringAppend(&dString, " ", -1);
    Tcl_DStringAppend(&dString, Tk_PathName(tablePtr->tkwin), -1);
    Tcl_DStringAppend(&dString, " \\\n", -1);
    lastPtr = Blt_ChainLastLink(tablePtr->chainPtr);
    for (linkPtr = Blt_ChainFirstLink(tablePtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	entryPtr = Blt_ChainGetValue(linkPtr);
	PrintEntry(entryPtr, &dString);
	if (linkPtr != lastPtr) {
	    Tcl_DStringAppend(&dString, " \\\n", -1);
	}
    }
    Tcl_DStringAppend(&dString, "\n\n# Row configuration options\n\n", -1);
    infoPtr = &(tablePtr->rowInfo);
    for (linkPtr = Blt_ChainFirstLink(infoPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	start = Tcl_DStringLength(&dString);
	Tcl_DStringAppend(&dString, argv[0], -1);
	Tcl_DStringAppend(&dString, " configure ", -1);
	Tcl_DStringAppend(&dString, Tk_PathName(tablePtr->tkwin), -1);
	Tcl_DStringAppend(&dString, " r", -1);
	Tcl_DStringAppend(&dString, Blt_Itoa(rcPtr->index), -1);
	last = Tcl_DStringLength(&dString);
	PrintRowColumn(interp, infoPtr, rcPtr, &dString);
	if (Tcl_DStringLength(&dString) == last) {
	    Tcl_DStringSetLength(&dString, start);
	} else {
	    Tcl_DStringAppend(&dString, "\n", -1);
	}
    }
    Tcl_DStringAppend(&dString, "\n\n# Column configuration options\n\n", -1);
    infoPtr = &(tablePtr->columnInfo);
    for (linkPtr = Blt_ChainFirstLink(infoPtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	rcPtr = Blt_ChainGetValue(linkPtr);
	start = Tcl_DStringLength(&dString);
	Tcl_DStringAppend(&dString, argv[0], -1);
	Tcl_DStringAppend(&dString, " configure ", -1);
	Tcl_DStringAppend(&dString, Tk_PathName(tablePtr->tkwin), -1);
	Tcl_DStringAppend(&dString, " c", -1);
	Tcl_DStringAppend(&dString, Blt_Itoa(rcPtr->index), -1);
	last = Tcl_DStringLength(&dString);
	PrintRowColumn(interp, infoPtr, rcPtr, &dString);
	if (Tcl_DStringLength(&dString) == last) {
	    Tcl_DStringSetLength(&dString, start);
	} else {
	    Tcl_DStringAppend(&dString, "\n", -1);
	}
    }
    start = Tcl_DStringLength(&dString);
    Tcl_DStringAppend(&dString, "\n\n# Table configuration options\n\n", -1);
    Tcl_DStringAppend(&dString, argv[0], -1);
    Tcl_DStringAppend(&dString, " configure ", -1);
    Tcl_DStringAppend(&dString, Tk_PathName(tablePtr->tkwin), -1);
    last = Tcl_DStringLength(&dString);
    PrintTable(tablePtr, &dString);
    if (Tcl_DStringLength(&dString) == last) {
	Tcl_DStringSetLength(&dString, start);
    } else {
	Tcl_DStringAppend(&dString, "\n", -1);
    }
    Tcl_DStringResult(interp, &dString);
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * SearchOp --
 *
 *	Returns a list of all the pathnames of the widgets managed by
 *	a table geometry manager.  The table is given by the path name of a
 *	container widget associated with the table.
 *
 * Results:
 *	Returns a standard Tcl result.  If no error occurred, TCL_OK is
 *	returned and a list of widget path names is left in interp->result.
 *
 * ----------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SearchOp(dataPtr, interp, argc, argv)
    TableInterpData *dataPtr;	/* Interpreter-specific data. */
    Tcl_Interp *interp;		/* Interpreter to return list of names to */
    int argc;			/* Number of arguments */
    char **argv;		/* Contains 1-2 arguments: pathname of container
				 * widget associated with the table and search
				 * pattern */
{
    Table *tablePtr;
    Blt_ChainLink *linkPtr;
    Entry *entryPtr;
    int rspan, cspan, rstart, cstart;
    char *pattern;
    char c;
    int flags;
    register int i;

#define	MATCH_PATTERN		(1<<0)	/* Find widgets whose path names
					 * match a given pattern */
#define	MATCH_INDEX_SPAN	(1<<1)	/* Find widgets that span index  */
#define	MATCH_INDEX_START	(1<<2)	/* Find widgets that start at index */


    if (Blt_GetTable(dataPtr, interp, argv[2], &tablePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    flags = 0;
    pattern = NULL;
    rspan = cspan = rstart = cstart = 0;

    /* Parse switches and arguments first */
    for (i = 3; i < argc; i += 2) {
	if (argv[i][0] == '-') {
	    unsigned int length;

	    if ((i + 1) == argc) {
		Tcl_AppendResult(interp, "switch \"", argv[i], "\" needs value",
		    (char *)NULL);
		return TCL_ERROR;
	    }
	    length = strlen(argv[i]);
	    c = argv[i][1];
	    if ((c == 'p') && (length > 1) &&
		(strncmp(argv[3], "-pattern", length) == 0)) {
		flags |= MATCH_PATTERN;
		pattern = argv[4];
	    } else if ((c == 's') && (length > 2) &&
		(strncmp(argv[i], "-start", length) == 0)) {
		flags |= MATCH_INDEX_START;
		if (ParseItem(tablePtr, argv[i + 1],
			&rstart, &cstart) != TCL_OK) {
		    return TCL_ERROR;
		}
	    } else if ((c == 's') && (length > 2) &&
		(strncmp(argv[i], "-span", length) == 0)) {
		flags |= MATCH_INDEX_SPAN;
		if (ParseItem(tablePtr, argv[4],
			&rspan, &cspan) != TCL_OK) {
		    return TCL_ERROR;
		}
	    } else {
		Tcl_AppendResult(interp, "bad switch \"", argv[3], "\" : \
should be \"-pattern\", \"-span\", or \"-start\"", (char *)NULL);
		return TCL_ERROR;
	    }
	} else {
	    if ((i + 1) == argc) {
		pattern = argv[i];
		flags |= MATCH_PATTERN;
	    }
	}
    }

    /* Then try to match entries with the search criteria */

    for (linkPtr = Blt_ChainFirstLink(tablePtr->chainPtr); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	entryPtr = Blt_ChainGetValue(linkPtr);
	if ((flags & MATCH_PATTERN) && (pattern != NULL)) {
	    if (Tcl_StringMatch(Tk_PathName(entryPtr->tkwin), pattern)) {
		goto match;
	    }
	}
	if (flags & MATCH_INDEX_SPAN) {
	    if ((rspan >= 0) && ((entryPtr->row.rcPtr->index <= rspan) ||
		    ((entryPtr->row.rcPtr->index + entryPtr->row.span) > rspan))) {
		goto match;
	    }
	    if ((cspan >= 0) && ((entryPtr->column.rcPtr->index <= cspan) ||
		    ((entryPtr->column.rcPtr->index + entryPtr->column.span)
			> cspan))) {
		goto match;
	    }
	}
	if (flags & MATCH_INDEX_START) {
	    if ((rstart >= 0) && (entryPtr->row.rcPtr->index == rstart)) {
		goto match;
	    }
	    if ((cstart >= 0) && (entryPtr->column.rcPtr->index == cstart)) {
		goto match;
	    }
	}
	continue;
      match:
	Tcl_AppendElement(interp, Tk_PathName(entryPtr->tkwin));
    }
    return TCL_OK;
}

/*
 * ----------------------------------------------------------------------------
 *
 * Table operations.
 *
 * The fields for Blt_OpSpec are as follows:
 *
 *   - operation name
 *   - minimum number of characters required to disambiguate the operation name.
 *   - function associated with operation.
 *   - minimum number of arguments required.
 *   - maximum number of arguments allowed (0 indicates no limit).
 *   - usage string
 *
 * ----------------------------------------------------------------------------
 */
static Blt_OpSpec operSpecs[] =
{
    {"arrange", 1, (Blt_Op)ArrangeOp, 3, 3, "container",},
    {"cget", 2, (Blt_Op)CgetOp, 4, 5,
	"container ?row|column|widget? option",},
    {"configure", 3, (Blt_Op)ConfigureOp, 3, 0,
	"container ?row|column|widget?... ?option value?...",},
    {"containers", 3, (Blt_Op)ContainersOp, 2, 4, "?switch? ?arg?",},
    {"delete", 1, (Blt_Op)DeleteOp, 3, 0,
	"container row|column ?row|column?",},
    {"extents", 1, (Blt_Op)ExtentsOp, 4, 4,
	"container row|column|widget",},
    {"forget", 1, (Blt_Op)ForgetOp, 3, 0, "widget ?widget?...",},
    {"info", 3, (Blt_Op)InfoOp, 3, 0,
	"container ?row|column|widget?...",},
    {"insert", 3, (Blt_Op)InsertOp, 4, 6,
	"container ?-before|-after? row|column ?count?",},
    {"join", 1, (Blt_Op)JoinOp, 5, 5, "container first last",},
    {"locate", 2, (Blt_Op)LocateOp, 5, 5, "container x y",},
    {"save", 2, (Blt_Op)SaveOp, 3, 3, "container",},
    {"search", 2, (Blt_Op)SearchOp, 3, 0, "container ?switch arg?...",},
    {"split", 2, (Blt_Op)SplitOp, 4, 5, "container row|column div",},
};

static int nSpecs = sizeof(operSpecs) / sizeof(Blt_OpSpec);

/*
 * ----------------------------------------------------------------------------
 *
 * TableCmd --
 *
 *	This procedure is invoked to process the Tcl command that
 *	corresponds to the table geometry manager.  See the user
 *	documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 * ----------------------------------------------------------------------------
 */
static int
TableCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Interpreter-specific data. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    TableInterpData *dataPtr = clientData;
    Blt_Op proc;
    int result;

    if ((argc > 1) && (argv[1][0] == '.')) {
	Table *tablePtr;

	if (Blt_GetTable(clientData, interp, argv[1], &tablePtr) != TCL_OK) {
	    Tcl_ResetResult(interp);
	    tablePtr = CreateTable(dataPtr, interp, argv[1]);
	    if (tablePtr == NULL) {
		return TCL_ERROR;
	    }
	}
	return BuildTable(tablePtr, interp, argc, argv);
    }
    proc = Blt_GetOp(interp, nSpecs, operSpecs, BLT_OP_ARG1, argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    result = (*proc) (dataPtr, interp, argc, argv);
    return result;
}


/*
 * -----------------------------------------------------------------------
 *
 * TableInterpDeleteProc --
 *
 *	This is called when the interpreter hosting the table command 
 *	is destroyed.  
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Destroys all the hash table maintaining the names of the table
 *	geomtry managers.
 *
 * ------------------------------------------------------------------------
 */
/* ARGSUSED */
static void
TableInterpDeleteProc(clientData, interp)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;
{
    TableInterpData *dataPtr = clientData;
    Blt_HashEntry *hPtr;
    Blt_HashSearch cursor;
    Table *tablePtr;

    for (hPtr = Blt_FirstHashEntry(&(dataPtr->tableTable), &cursor);
	 hPtr != NULL; hPtr = Blt_NextHashEntry(&cursor)) {
	tablePtr = (Table *)Blt_GetHashValue(hPtr);
	tablePtr->hashPtr = NULL;
	DestroyTable((DestroyData)tablePtr);
    }
    Blt_DeleteHashTable(&(dataPtr->tableTable));
    Tcl_DeleteAssocData(interp, TABLE_THREAD_KEY);
    Blt_Free(dataPtr);
}

static TableInterpData *
GetTableInterpData(interp)
     Tcl_Interp *interp;
{
    TableInterpData *dataPtr;
    Tcl_InterpDeleteProc *proc;

    dataPtr = (TableInterpData *)
	Tcl_GetAssocData(interp, TABLE_THREAD_KEY, &proc);
    if (dataPtr == NULL) {
	dataPtr = Blt_Malloc(sizeof(TableInterpData));
	assert(dataPtr);
	Tcl_SetAssocData(interp, TABLE_THREAD_KEY, TableInterpDeleteProc, 
		dataPtr);
	Blt_InitHashTable(&(dataPtr->tableTable), BLT_ONE_WORD_KEYS);
    }
    return dataPtr;
}


/*
 * ----------------------------------------------------------------------------
 *
 * Blt_TableInit --
 *
 *	This procedure is invoked to initialize the Tcl command that
 *	corresponds to the table geometry manager.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates the new command and adds an entry into a global Tcl
 *	associative array.
 *
 * ---------------------------------------------------------------------------
 */
int
Blt_TableInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec = {"table", TableCmd, };
    TableInterpData *dataPtr;

    dataPtr = GetTableInterpData(interp);
    cmdSpec.clientData = dataPtr;
    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    rowUid = (Blt_Uid)Tk_GetUid("row");
    columnUid = (Blt_Uid)Tk_GetUid("column");
    return TCL_OK;
}
