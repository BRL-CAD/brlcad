/* 
 * tkScale.c --
 *
 *	This module implements a scale widgets for the Tk toolkit.
 *	A scale displays a slider that can be adjusted to change a
 *	value;  it also displays numeric labels and a textual label,
 *	if desired.
 *	
 *	The modifications to use floating-point values are based on
 *	an implementation by Paul Mackerras.  The -variable option
 *	is due to Henning Schulzrinne.  All of these are used with
 *	permission.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkScale.c 1.88 97/07/31 09:11:57
 */

#include "tkPort.h"
#include "default.h"
#include "tkInt.h"
#include "tclMath.h"
#include "tkScale.h"

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground", "Foreground",
	DEF_SCALE_ACTIVE_BG_COLOR, Tk_Offset(TkScale, activeBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground", "Foreground",
	DEF_SCALE_ACTIVE_BG_MONO, Tk_Offset(TkScale, activeBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_SCALE_BG_COLOR, Tk_Offset(TkScale, bgBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_SCALE_BG_MONO, Tk_Offset(TkScale, bgBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_DOUBLE, "-bigincrement", "bigIncrement", "BigIncrement",
	DEF_SCALE_BIG_INCREMENT, Tk_Offset(TkScale, bigIncrement), 0},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_SCALE_BORDER_WIDTH, Tk_Offset(TkScale, borderWidth), 0},
    {TK_CONFIG_STRING, "-command", "command", "Command",
	DEF_SCALE_COMMAND, Tk_Offset(TkScale, command), TK_CONFIG_NULL_OK},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_SCALE_CURSOR, Tk_Offset(TkScale, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_INT, "-digits", "digits", "Digits",
	DEF_SCALE_DIGITS, Tk_Offset(TkScale, digits), 0},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_SCALE_FONT, Tk_Offset(TkScale, tkfont),
	0},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_SCALE_FG_COLOR, Tk_Offset(TkScale, textColorPtr),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_SCALE_FG_MONO, Tk_Offset(TkScale, textColorPtr),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_DOUBLE, "-from", "from", "From",
	DEF_SCALE_FROM, Tk_Offset(TkScale, fromValue), 0},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_SCALE_HIGHLIGHT_BG,
	Tk_Offset(TkScale, highlightBgColorPtr), 0},
    {TK_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_SCALE_HIGHLIGHT, Tk_Offset(TkScale, highlightColorPtr), 0},
    {TK_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
	"HighlightThickness",
	DEF_SCALE_HIGHLIGHT_WIDTH, Tk_Offset(TkScale, highlightWidth), 0},
    {TK_CONFIG_STRING, "-label", "label", "Label",
	DEF_SCALE_LABEL, Tk_Offset(TkScale, label), TK_CONFIG_NULL_OK},
    {TK_CONFIG_PIXELS, "-length", "length", "Length",
	DEF_SCALE_LENGTH, Tk_Offset(TkScale, length), 0},
    {TK_CONFIG_UID, "-orient", "orient", "Orient",
	DEF_SCALE_ORIENT, Tk_Offset(TkScale, orientUid), 0},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_SCALE_RELIEF, Tk_Offset(TkScale, relief), 0},
    {TK_CONFIG_INT, "-repeatdelay", "repeatDelay", "RepeatDelay",
	DEF_SCALE_REPEAT_DELAY, Tk_Offset(TkScale, repeatDelay), 0},
    {TK_CONFIG_INT, "-repeatinterval", "repeatInterval", "RepeatInterval",
	DEF_SCALE_REPEAT_INTERVAL, Tk_Offset(TkScale, repeatInterval), 0},
    {TK_CONFIG_DOUBLE, "-resolution", "resolution", "Resolution",
	DEF_SCALE_RESOLUTION, Tk_Offset(TkScale, resolution), 0},
    {TK_CONFIG_BOOLEAN, "-showvalue", "showValue", "ShowValue",
	DEF_SCALE_SHOW_VALUE, Tk_Offset(TkScale, showValue), 0},
    {TK_CONFIG_PIXELS, "-sliderlength", "sliderLength", "SliderLength",
	DEF_SCALE_SLIDER_LENGTH, Tk_Offset(TkScale, sliderLength), 0},
    {TK_CONFIG_RELIEF, "-sliderrelief", "sliderRelief", "SliderRelief",
	DEF_SCALE_SLIDER_RELIEF, Tk_Offset(TkScale, sliderRelief),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_UID, "-state", "state", "State",
	DEF_SCALE_STATE, Tk_Offset(TkScale, state), 0},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_SCALE_TAKE_FOCUS, Tk_Offset(TkScale, takeFocus),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_DOUBLE, "-tickinterval", "tickInterval", "TickInterval",
	DEF_SCALE_TICK_INTERVAL, Tk_Offset(TkScale, tickInterval), 0},
    {TK_CONFIG_DOUBLE, "-to", "to", "To",
	DEF_SCALE_TO, Tk_Offset(TkScale, toValue), 0},
    {TK_CONFIG_COLOR, "-troughcolor", "troughColor", "Background",
	DEF_SCALE_TROUGH_COLOR, Tk_Offset(TkScale, troughColorPtr),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-troughcolor", "troughColor", "Background",
	DEF_SCALE_TROUGH_MONO, Tk_Offset(TkScale, troughColorPtr),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_STRING, "-variable", "variable", "Variable",
	DEF_SCALE_VARIABLE, Tk_Offset(TkScale, varName), TK_CONFIG_NULL_OK},
    {TK_CONFIG_PIXELS, "-width", "width", "Width",
	DEF_SCALE_WIDTH, Tk_Offset(TkScale, width), 0},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		ComputeFormat _ANSI_ARGS_((TkScale *scalePtr));
static void		ComputeScaleGeometry _ANSI_ARGS_((TkScale *scalePtr));
static int		ConfigureScale _ANSI_ARGS_((Tcl_Interp *interp,
			    TkScale *scalePtr, int argc, char **argv,
			    int flags));
static void		DestroyScale _ANSI_ARGS_((char *memPtr));
static void		ScaleCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static void		ScaleEventProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static char *		ScaleVarProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
static int		ScaleWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static void		ScaleWorldChanged _ANSI_ARGS_((
			    ClientData instanceData));

/*
 * The structure below defines scale class behavior by means of procedures
 * that can be invoked from generic window code.
 */

static TkClassProcs scaleClass = {
    NULL,			/* createProc. */
    ScaleWorldChanged,		/* geometryProc. */
    NULL			/* modalProc. */
};


/*
 *--------------------------------------------------------------
 *
 * Tk_ScaleCmd --
 *
 *	This procedure is invoked to process the "scale" Tcl
 *	command.  See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
Tk_ScaleCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    register TkScale *scalePtr;
    Tk_Window new;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    new = Tk_CreateWindowFromPath(interp, tkwin, argv[1], (char *) NULL);
    if (new == NULL) {
	return TCL_ERROR;
    }
    scalePtr = TkpCreateScale(new);

    /*
     * Initialize fields that won't be initialized by ConfigureScale,
     * or which ConfigureScale expects to have reasonable values
     * (e.g. resource pointers).
     */

    scalePtr->tkwin = new;
    scalePtr->display = Tk_Display(new);
    scalePtr->interp = interp;
    scalePtr->widgetCmd = Tcl_CreateCommand(interp,
	    Tk_PathName(scalePtr->tkwin), ScaleWidgetCmd,
	    (ClientData) scalePtr, ScaleCmdDeletedProc);
    scalePtr->orientUid = NULL;
    scalePtr->vertical = 0;
    scalePtr->width = 0;
    scalePtr->length = 0;
    scalePtr->value = 0;
    scalePtr->varName = NULL;
    scalePtr->fromValue = 0;
    scalePtr->toValue = 0;
    scalePtr->tickInterval = 0;
    scalePtr->resolution = 1;
    scalePtr->bigIncrement = 0.0;
    scalePtr->command = NULL;
    scalePtr->repeatDelay = 0;
    scalePtr->repeatInterval = 0;
    scalePtr->label = NULL;
    scalePtr->labelLength = 0;
    scalePtr->state = tkNormalUid;
    scalePtr->borderWidth = 0;
    scalePtr->bgBorder = NULL;
    scalePtr->activeBorder = NULL;
    scalePtr->sliderRelief = TK_RELIEF_RAISED;
    scalePtr->troughColorPtr = NULL;
    scalePtr->troughGC = None;
    scalePtr->copyGC = None;
    scalePtr->tkfont = NULL;
    scalePtr->textColorPtr = NULL;
    scalePtr->textGC = None;
    scalePtr->relief = TK_RELIEF_FLAT;
    scalePtr->highlightWidth = 0;
    scalePtr->highlightBgColorPtr = NULL;
    scalePtr->highlightColorPtr = NULL;
    scalePtr->inset = 0;
    scalePtr->sliderLength = 0;
    scalePtr->showValue = 0;
    scalePtr->horizLabelY = 0;
    scalePtr->horizValueY = 0;
    scalePtr->horizTroughY = 0;
    scalePtr->horizTickY = 0;
    scalePtr->vertTickRightX = 0;
    scalePtr->vertValueRightX = 0;
    scalePtr->vertTroughX = 0;
    scalePtr->vertLabelX = 0;
    scalePtr->cursor = None;
    scalePtr->takeFocus = NULL;
    scalePtr->flags = NEVER_SET;

    Tk_SetClass(scalePtr->tkwin, "Scale");
    TkSetClassProcs(scalePtr->tkwin, &scaleClass, (ClientData) scalePtr);
    Tk_CreateEventHandler(scalePtr->tkwin,
	    ExposureMask|StructureNotifyMask|FocusChangeMask,
	    ScaleEventProc, (ClientData) scalePtr);
    if (ConfigureScale(interp, scalePtr, argc-2, argv+2, 0) != TCL_OK) {
	goto error;
    }

    interp->result = Tk_PathName(scalePtr->tkwin);
    return TCL_OK;

    error:
    Tk_DestroyWindow(scalePtr->tkwin);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * ScaleWidgetCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a widget managed by this module.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

static int
ScaleWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Information about scale
					 * widget. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    register TkScale *scalePtr = (TkScale *) clientData;
    int result = TCL_OK;
    size_t length;
    int c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Tcl_Preserve((ClientData) scalePtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cget option\"",
		    (char *) NULL);
	    goto error;
	}
	result = Tk_ConfigureValue(interp, scalePtr->tkwin, configSpecs,
		(char *) scalePtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 3)) {
	if (argc == 2) {
	    result = Tk_ConfigureInfo(interp, scalePtr->tkwin, configSpecs,
		    (char *) scalePtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Tk_ConfigureInfo(interp, scalePtr->tkwin, configSpecs,
		    (char *) scalePtr, argv[2], 0);
	} else {
	    result = ConfigureScale(interp, scalePtr, argc-2, argv+2,
		    TK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 'c') && (strncmp(argv[1], "coords", length) == 0)
	    && (length >= 3)) {
	int x, y ;
	double value;

	if ((argc != 2) && (argc != 3)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " coords ?value?\"", (char *) NULL);
	    goto error;
	}
	if (argc == 3) {
	    if (Tcl_GetDouble(interp, argv[2], &value) != TCL_OK) {
		goto error;
	    }
	} else {
	    value = scalePtr->value;
	}
	if (scalePtr->vertical) {
	    x = scalePtr->vertTroughX + scalePtr->width/2
		    + scalePtr->borderWidth;
	    y = TkpValueToPixel(scalePtr, value);
	} else {
	    x = TkpValueToPixel(scalePtr, value);
	    y = scalePtr->horizTroughY + scalePtr->width/2
		    + scalePtr->borderWidth;
	}
	sprintf(interp->result, "%d %d", x, y);
    } else if ((c == 'g') && (strncmp(argv[1], "get", length) == 0)) {
	double value;
	int x, y;

	if ((argc != 2) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " get ?x y?\"", (char *) NULL);
	    goto error;
	}
	if (argc == 2) {
	    value = scalePtr->value;
	} else {
	    if ((Tcl_GetInt(interp, argv[2], &x) != TCL_OK)
		    || (Tcl_GetInt(interp, argv[3], &y) != TCL_OK)) {
		goto error;
	    }
	    value = TkpPixelToValue(scalePtr, x, y);
	}
	sprintf(interp->result, scalePtr->format, value);
    } else if ((c == 'i') && (strncmp(argv[1], "identify", length) == 0)) {
	int x, y, thing;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " identify x y\"", (char *) NULL);
	    goto error;
	}
	if ((Tcl_GetInt(interp, argv[2], &x) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[3], &y) != TCL_OK)) {
	    goto error;
	}
	thing = TkpScaleElement(scalePtr, x,y);
	switch (thing) {
	    case TROUGH1:	interp->result = "trough1";	break;
	    case SLIDER:	interp->result = "slider";	break;
	    case TROUGH2:	interp->result = "trough2";	break;
	}
    } else if ((c == 's') && (strncmp(argv[1], "set", length) == 0)) {
	double value;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " set value\"", (char *) NULL);
	    goto error;
	}
	if (Tcl_GetDouble(interp, argv[2], &value) != TCL_OK) {
	    goto error;
	}
	if (scalePtr->state != tkDisabledUid) {
	    TkpSetScaleValue(scalePtr, value, 1, 1);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be cget, configure, coords, get, identify, or set",
		(char *) NULL);
	goto error;
    }
    Tcl_Release((ClientData) scalePtr);
    return result;

    error:
    Tcl_Release((ClientData) scalePtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyScale --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a button at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the scale is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyScale(memPtr)
    char *memPtr;	/* Info about scale widget. */
{
    register TkScale *scalePtr = (TkScale *) memPtr;

    /*
     * Free up all the stuff that requires special handling, then
     * let Tk_FreeOptions handle all the standard option-related
     * stuff.
     */

    if (scalePtr->varName != NULL) {
	Tcl_UntraceVar(scalePtr->interp, scalePtr->varName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ScaleVarProc, (ClientData) scalePtr);
    }
    if (scalePtr->troughGC != None) {
	Tk_FreeGC(scalePtr->display, scalePtr->troughGC);
    }
    if (scalePtr->copyGC != None) {
	Tk_FreeGC(scalePtr->display, scalePtr->copyGC);
    }
    if (scalePtr->textGC != None) {
	Tk_FreeGC(scalePtr->display, scalePtr->textGC);
    }
    Tk_FreeOptions(configSpecs, (char *) scalePtr, scalePtr->display, 0);
    TkpDestroyScale(scalePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureScale --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a scale widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as colors, border width,
 *	etc. get set for scalePtr;  old resources get freed,
 *	if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureScale(interp, scalePtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    register TkScale *scalePtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    size_t length;

    /*
     * Eliminate any existing trace on a variable monitored by the scale.
     */

    if (scalePtr->varName != NULL) {
	Tcl_UntraceVar(interp, scalePtr->varName, 
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ScaleVarProc, (ClientData) scalePtr);
    }

    if (Tk_ConfigureWidget(interp, scalePtr->tkwin, configSpecs,
	    argc, argv, (char *) scalePtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * If the scale is tied to the value of a variable, then set up
     * a trace on the variable's value and set the scale's value from
     * the value of the variable, if it exists.
     */

    if (scalePtr->varName != NULL) {
	char *stringValue, *end;
	double value;

	stringValue = Tcl_GetVar(interp, scalePtr->varName, TCL_GLOBAL_ONLY);
	if (stringValue != NULL) {
	    value = strtod(stringValue, &end);
	    if ((end != stringValue) && (*end == 0)) {
		scalePtr->value = TkRoundToResolution(scalePtr, value);
	    }
	}
	Tcl_TraceVar(interp, scalePtr->varName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ScaleVarProc, (ClientData) scalePtr);
    }

    /*
     * Several options need special processing, such as parsing the
     * orientation and creating GCs.
     */

    length = strlen(scalePtr->orientUid);
    if (strncmp(scalePtr->orientUid, "vertical", length) == 0) {
	scalePtr->vertical = 1;
    } else if (strncmp(scalePtr->orientUid, "horizontal", length) == 0) {
	scalePtr->vertical = 0;
    } else {
	Tcl_AppendResult(interp, "bad orientation \"", scalePtr->orientUid,
		"\": must be vertical or horizontal", (char *) NULL);
	return TCL_ERROR;
    }

    scalePtr->fromValue = TkRoundToResolution(scalePtr, scalePtr->fromValue);
    scalePtr->toValue = TkRoundToResolution(scalePtr, scalePtr->toValue);
    scalePtr->tickInterval = TkRoundToResolution(scalePtr,
	    scalePtr->tickInterval);

    /*
     * Make sure that the tick interval has the right sign so that
     * addition moves from fromValue to toValue.
     */

    if ((scalePtr->tickInterval < 0)
	    ^ ((scalePtr->toValue - scalePtr->fromValue) <  0)) {
	scalePtr->tickInterval = -scalePtr->tickInterval;
    }

    /*
     * Set the scale value to itself;  all this does is to make sure
     * that the scale's value is within the new acceptable range for
     * the scale and reflect the value in the associated variable,
     * if any.
     */

    ComputeFormat(scalePtr);
    TkpSetScaleValue(scalePtr, scalePtr->value, 1, 1);

    if (scalePtr->label != NULL) {
	scalePtr->labelLength = strlen(scalePtr->label);
    } else {
	scalePtr->labelLength = 0;
    }

    if ((scalePtr->state != tkNormalUid)
	    && (scalePtr->state != tkDisabledUid)
	    && (scalePtr->state != tkActiveUid)) {
	Tcl_AppendResult(interp, "bad state value \"", scalePtr->state,
		"\": must be normal, active, or disabled", (char *) NULL);
	scalePtr->state = tkNormalUid;
	return TCL_ERROR;
    }

    Tk_SetBackgroundFromBorder(scalePtr->tkwin, scalePtr->bgBorder);

    if (scalePtr->highlightWidth < 0) {
	scalePtr->highlightWidth = 0;
    }
    scalePtr->inset = scalePtr->highlightWidth + scalePtr->borderWidth;

    ScaleWorldChanged((ClientData) scalePtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * ScaleWorldChanged --
 *
 *      This procedure is called when the world has changed in some
 *      way and the widget needs to recompute all its graphics contexts
 *	and determine its new geometry.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Scale will be relayed out and redisplayed.
 *
 *---------------------------------------------------------------------------
 */
 
static void
ScaleWorldChanged(instanceData)
    ClientData instanceData;	/* Information about widget. */
{
    XGCValues gcValues;
    GC gc;
    TkScale *scalePtr;

    scalePtr = (TkScale *) instanceData;

    gcValues.foreground = scalePtr->troughColorPtr->pixel;
    gc = Tk_GetGC(scalePtr->tkwin, GCForeground, &gcValues);
    if (scalePtr->troughGC != None) {
	Tk_FreeGC(scalePtr->display, scalePtr->troughGC);
    }
    scalePtr->troughGC = gc;

    gcValues.font = Tk_FontId(scalePtr->tkfont);
    gcValues.foreground = scalePtr->textColorPtr->pixel;
    gc = Tk_GetGC(scalePtr->tkwin, GCForeground | GCFont, &gcValues);
    if (scalePtr->textGC != None) {
	Tk_FreeGC(scalePtr->display, scalePtr->textGC);
    }
    scalePtr->textGC = gc;

    if (scalePtr->copyGC == None) {
	gcValues.graphics_exposures = False;
	scalePtr->copyGC = Tk_GetGC(scalePtr->tkwin, GCGraphicsExposures,
	    &gcValues);
    }
    scalePtr->inset = scalePtr->highlightWidth + scalePtr->borderWidth;

    /*
     * Recompute display-related information, and let the geometry
     * manager know how much space is needed now.
     */

    ComputeScaleGeometry(scalePtr);

    TkEventuallyRedrawScale(scalePtr, REDRAW_ALL);
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeFormat --
 *
 *	This procedure is invoked to recompute the "format" field
 *	of a scale's widget record, which determines how the value
 *	of the scale is converted to a string.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The format field of scalePtr is modified.
 *
 *----------------------------------------------------------------------
 */

static void
ComputeFormat(scalePtr)
    TkScale *scalePtr;			/* Information about scale widget. */
{
    double maxValue, x;
    int mostSigDigit, numDigits, leastSigDigit, afterDecimal;
    int eDigits, fDigits;

    /*
     * Compute the displacement from the decimal of the most significant
     * digit required for any number in the scale's range.
     */

    maxValue = fabs(scalePtr->fromValue);
    x = fabs(scalePtr->toValue);
    if (x > maxValue) {
	maxValue = x;
    }
    if (maxValue == 0) {
	maxValue = 1;
    }
    mostSigDigit = (int) floor(log10(maxValue));

    /*
     * If the number of significant digits wasn't specified explicitly,
     * compute it. It's the difference between the most significant
     * digit needed to represent any number on the scale and the
     * most significant digit of the smallest difference between
     * numbers on the scale.  In other words, display enough digits so
     * that at least one digit will be different between any two adjacent
     * positions of the scale.
     */

    numDigits = scalePtr->digits;
    if (numDigits <= 0) {
	if  (scalePtr->resolution > 0) {
	    /*
	     * A resolution was specified for the scale, so just use it.
	     */

	    leastSigDigit = (int) floor(log10(scalePtr->resolution));
	} else {
	    /*
	     * No resolution was specified, so compute the difference
	     * in value between adjacent pixels and use it for the least
	     * significant digit.
	     */

	    x = fabs(scalePtr->fromValue - scalePtr->toValue);
	    if (scalePtr->length > 0) {
		x /= scalePtr->length;
	    }
	    if (x > 0){
		leastSigDigit = (int) floor(log10(x));
	    } else {
		leastSigDigit = 0;
	    }
	}
	numDigits = mostSigDigit - leastSigDigit + 1;
	if (numDigits < 1) {
	    numDigits = 1;
	}
    }

    /*
     * Compute the number of characters required using "e" format and
     * "f" format, and then choose whichever one takes fewer characters.
     */

    eDigits = numDigits + 4;
    if (numDigits > 1) {
	eDigits++;			/* Decimal point. */
    }
    afterDecimal = numDigits - mostSigDigit - 1;
    if (afterDecimal < 0) {
	afterDecimal = 0;
    }
    fDigits = (mostSigDigit >= 0) ? mostSigDigit + afterDecimal : afterDecimal;
    if (afterDecimal > 0) {
	fDigits++;			/* Decimal point. */
    }
    if (mostSigDigit < 0) {
	fDigits++;			/* Zero to left of decimal point. */
    }
    if (fDigits <= eDigits) {
	sprintf(scalePtr->format, "%%.%df", afterDecimal);
    } else {
	sprintf(scalePtr->format, "%%.%de", numDigits-1);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeScaleGeometry --
 *
 *	This procedure is called to compute various geometrical
 *	information for a scale, such as where various things get
 *	displayed.  It's called when the window is reconfigured.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Display-related numbers get changed in *scalePtr.  The
 *	geometry manager gets told about the window's preferred size.
 *
 *----------------------------------------------------------------------
 */

static void
ComputeScaleGeometry(scalePtr)
    register TkScale *scalePtr;		/* Information about widget. */
{
    char valueString[PRINT_CHARS];
    int tmp, valuePixels, x, y, extraSpace;
    Tk_FontMetrics fm;

    /*
     * Horizontal scales are simpler than vertical ones because
     * all sizes are the same (the height of a line of text);
     * handle them first and then quit.
     */

    Tk_GetFontMetrics(scalePtr->tkfont, &fm);
    if (!scalePtr->vertical) {
	y = scalePtr->inset;
	extraSpace = 0;
	if (scalePtr->labelLength != 0) {
	    scalePtr->horizLabelY = y + SPACING;
	    y += fm.linespace + SPACING;
	    extraSpace = SPACING;
	}
	if (scalePtr->showValue) {
	    scalePtr->horizValueY = y + SPACING;
	    y += fm.linespace + SPACING;
	    extraSpace = SPACING;
	} else {
	    scalePtr->horizValueY = y;
	}
	y += extraSpace;
	scalePtr->horizTroughY = y;
	y += scalePtr->width + 2*scalePtr->borderWidth;
	if (scalePtr->tickInterval != 0) {
	    scalePtr->horizTickY = y + SPACING;
	    y += fm.linespace + 2*SPACING;
	}
	Tk_GeometryRequest(scalePtr->tkwin,
		scalePtr->length + 2*scalePtr->inset, y + scalePtr->inset);
	Tk_SetInternalBorder(scalePtr->tkwin, scalePtr->inset);
	return;
    }

    /*
     * Vertical scale:  compute the amount of space needed to display
     * the scales value by formatting strings for the two end points;
     * use whichever length is longer.
     */

    sprintf(valueString, scalePtr->format, scalePtr->fromValue);
    valuePixels = Tk_TextWidth(scalePtr->tkfont, valueString, -1);

    sprintf(valueString, scalePtr->format, scalePtr->toValue);
    tmp = Tk_TextWidth(scalePtr->tkfont, valueString, -1);
    if (valuePixels < tmp) {
	valuePixels = tmp;
    }

    /*
     * Assign x-locations to the elements of the scale, working from
     * left to right.
     */

    x = scalePtr->inset;
    if ((scalePtr->tickInterval != 0) && (scalePtr->showValue)) {
	scalePtr->vertTickRightX = x + SPACING + valuePixels;
	scalePtr->vertValueRightX = scalePtr->vertTickRightX + valuePixels
		+ fm.ascent/2;
	x = scalePtr->vertValueRightX + SPACING;
    } else if (scalePtr->tickInterval != 0) {
	scalePtr->vertTickRightX = x + SPACING + valuePixels;
	scalePtr->vertValueRightX = scalePtr->vertTickRightX;
	x = scalePtr->vertTickRightX + SPACING;
    } else if (scalePtr->showValue) {
	scalePtr->vertTickRightX = x;
	scalePtr->vertValueRightX = x + SPACING + valuePixels;
	x = scalePtr->vertValueRightX + SPACING;
    } else {
	scalePtr->vertTickRightX = x;
	scalePtr->vertValueRightX = x;
    }
    scalePtr->vertTroughX = x;
    x += 2*scalePtr->borderWidth + scalePtr->width;
    if (scalePtr->labelLength == 0) {
	scalePtr->vertLabelX = 0;
    } else {
	scalePtr->vertLabelX = x + fm.ascent/2;
	x = scalePtr->vertLabelX + fm.ascent/2
		+ Tk_TextWidth(scalePtr->tkfont, scalePtr->label,
			scalePtr->labelLength);
    }
    Tk_GeometryRequest(scalePtr->tkwin, x + scalePtr->inset,
	    scalePtr->length + 2*scalePtr->inset);
    Tk_SetInternalBorder(scalePtr->tkwin, scalePtr->inset);
}

/*
 *--------------------------------------------------------------
 *
 * ScaleEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on scales.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, it is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
ScaleEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    TkScale *scalePtr = (TkScale *) clientData;

    if ((eventPtr->type == Expose) && (eventPtr->xexpose.count == 0)) {
	TkEventuallyRedrawScale(scalePtr, REDRAW_ALL);
    } else if (eventPtr->type == DestroyNotify) {
	if (scalePtr->tkwin != NULL) {
	    scalePtr->tkwin = NULL;
	    Tcl_DeleteCommandFromToken(scalePtr->interp, scalePtr->widgetCmd);
	}
	if (scalePtr->flags & REDRAW_ALL) {
	    Tcl_CancelIdleCall(TkpDisplayScale, (ClientData) scalePtr);
	}
	Tcl_EventuallyFree((ClientData) scalePtr, DestroyScale);
    } else if (eventPtr->type == ConfigureNotify) {
	ComputeScaleGeometry(scalePtr);
	TkEventuallyRedrawScale(scalePtr, REDRAW_ALL);
    } else if (eventPtr->type == FocusIn) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    scalePtr->flags |= GOT_FOCUS;
	    if (scalePtr->highlightWidth > 0) {
		TkEventuallyRedrawScale(scalePtr, REDRAW_ALL);
	    }
	}
    } else if (eventPtr->type == FocusOut) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    scalePtr->flags &= ~GOT_FOCUS;
	    if (scalePtr->highlightWidth > 0) {
		TkEventuallyRedrawScale(scalePtr, REDRAW_ALL);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ScaleCmdDeletedProc --
 *
 *	This procedure is invoked when a widget command is deleted.  If
 *	the widget isn't already in the process of being destroyed,
 *	this command destroys it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget is destroyed.
 *
 *----------------------------------------------------------------------
 */

static void
ScaleCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    TkScale *scalePtr = (TkScale *) clientData;
    Tk_Window tkwin = scalePtr->tkwin;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tkwin != NULL) {
	scalePtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *--------------------------------------------------------------
 *
 * TkEventuallyRedrawScale --
 *
 *	Arrange for part or all of a scale widget to redrawn at
 *	the next convenient time in the future.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If "what" is REDRAW_SLIDER then just the slider and the
 *	value readout will be redrawn;  if "what" is REDRAW_ALL
 *	then the entire widget will be redrawn.
 *
 *--------------------------------------------------------------
 */

void
TkEventuallyRedrawScale(scalePtr, what)
    register TkScale *scalePtr;	/* Information about widget. */
    int what;			/* What to redraw:  REDRAW_SLIDER
				 * or REDRAW_ALL. */
{
    if ((what == 0) || (scalePtr->tkwin == NULL)
	    || !Tk_IsMapped(scalePtr->tkwin)) {
	return;
    }
    if ((scalePtr->flags & REDRAW_ALL) == 0) {
	Tcl_DoWhenIdle(TkpDisplayScale, (ClientData) scalePtr);
    }
    scalePtr->flags |= what;
}

/*
 *--------------------------------------------------------------
 *
 * TkRoundToResolution --
 *
 *	Round a given floating-point value to the nearest multiple
 *	of the scale's resolution.
 *
 * Results:
 *	The return value is the rounded result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

double
TkRoundToResolution(scalePtr, value)
    TkScale *scalePtr;		/* Information about scale widget. */
    double value;		/* Value to round. */
{
    double rem, new;

    if (scalePtr->resolution <= 0) {
	return value;
    }
    new = scalePtr->resolution * floor(value/scalePtr->resolution);
    rem = value - new;
    if (rem < 0) {
	if (rem <= -scalePtr->resolution/2) {
	    new -= scalePtr->resolution;
	}
    } else {
	if (rem >= scalePtr->resolution/2) {
	    new += scalePtr->resolution;
	}
    }
    return new;
}

/*
 *----------------------------------------------------------------------
 *
 * ScaleVarProc --
 *
 *	This procedure is invoked by Tcl whenever someone modifies a
 *	variable associated with a scale widget.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The value displayed in the scale will change to match the
 *	variable's new value.  If the variable has a bogus value then
 *	it is reset to the value of the scale.
 *
 *----------------------------------------------------------------------
 */

    /* ARGSUSED */
static char *
ScaleVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about button. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    register TkScale *scalePtr = (TkScale *) clientData;
    char *stringValue, *end, *result;
    double value;

    /*
     * If the variable is unset, then immediately recreate it unless
     * the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_TraceVar(interp, scalePtr->varName,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    ScaleVarProc, clientData);
	    scalePtr->flags |= NEVER_SET;
	    TkpSetScaleValue(scalePtr, scalePtr->value, 1, 0);
	}
	return (char *) NULL;
    }

    /*
     * If we came here because we updated the variable (in TkpSetScaleValue),
     * then ignore the trace.  Otherwise update the scale with the value
     * of the variable.
     */

    if (scalePtr->flags & SETTING_VAR) {
	return (char *) NULL;
    }
    result = NULL;
    stringValue = Tcl_GetVar(interp, scalePtr->varName, TCL_GLOBAL_ONLY);
    if (stringValue != NULL) {
	value = strtod(stringValue, &end);
	if ((end == stringValue) || (*end != 0)) {
	    result = "can't assign non-numeric value to scale variable";
	} else {
	    scalePtr->value = TkRoundToResolution(scalePtr, value);
	}

	/*
	 * This code is a bit tricky because it sets the scale's value before
	 * calling TkpSetScaleValue.  This way, TkpSetScaleValue won't bother 
	 * to set the variable again or to invoke the -command.  However, it
	 * also won't redisplay the scale, so we have to ask for that
	 * explicitly.
	 */

	TkpSetScaleValue(scalePtr, scalePtr->value, 1, 0);
	TkEventuallyRedrawScale(scalePtr, REDRAW_SLIDER);
    }

    return result;
}
