/* 
 * tkMessage.c --
 *
 *	This module implements a message widgets for the Tk
 *	toolkit.  A message widget displays a multi-line string
 *	in a window according to a particular aspect ratio.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tkPort.h"
#include "default.h"
#include "tkInt.h"

/*
 * A data structure of the following type is kept for each message
 * widget managed by this file:
 */

typedef struct {
    Tk_Window tkwin;		/* Window that embodies the message.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Display *display;		/* Display containing widget.  Used, among
				 * other things, so that resources can be
				 * freed even after tkwin has gone away. */
    Tcl_Interp *interp;		/* Interpreter associated with message. */
    Tcl_Command widgetCmd;	/* Token for message's widget command. */

    /*
     * Information used when displaying widget:
     */

    char *string;		/* String displayed in message. */
    int numChars;		/* Number of characters in string, not
				 * including terminating NULL. */
    char *textVarName;		/* Name of variable (malloc'ed) or NULL.
				 * If non-NULL, message displays the contents
				 * of this variable. */
    Tk_3DBorder border;		/* Structure used to draw 3-D border and
				 * background.  NULL means a border hasn't
				 * been created yet. */
    int borderWidth;		/* Width of border. */
    int relief;			/* 3-D effect: TK_RELIEF_RAISED, etc. */
    int highlightWidth;		/* Width in pixels of highlight to draw
				 * around widget when it has the focus.
				 * <= 0 means don't draw a highlight. */
    XColor *highlightBgColorPtr;
				/* Color for drawing traversal highlight
				 * area when highlight is off. */
    XColor *highlightColorPtr;	/* Color for drawing traversal highlight. */
    Tk_Font tkfont;		/* Information about text font, or NULL. */
    XColor *fgColorPtr;		/* Foreground color in normal mode. */
    int padX, padY;		/* User-requested extra space around text. */
    int width;			/* User-requested width, in pixels.  0 means
				 * compute width using aspect ratio below. */
    int aspect;			/* Desired aspect ratio for window
				 * (100*width/height). */
    int msgWidth;		/* Width in pixels needed to display
				 * message. */
    int msgHeight;		/* Height in pixels needed to display
				 * message. */
    Tk_Anchor anchor;		/* Where to position text within window region
				 * if window is larger or smaller than
				 * needed. */
    Tk_Justify justify;		/* Justification for text. */

    GC textGC;			/* GC for drawing text in normal mode. */
    Tk_TextLayout textLayout;	/* Saved layout information. */

    /*
     * Miscellaneous information:
     */

    Tk_Cursor cursor;		/* Current cursor for window, or None. */
    char *takeFocus;		/* Value of -takefocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    int flags;			/* Various flags;  see below for
				 * definitions. */
} Message;

/*
 * Flag bits for messages:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 * GOT_FOCUS:			Non-zero means this button currently
 *				has the input focus.
 */

#define REDRAW_PENDING		1
#define GOT_FOCUS		4

/*
 * Information used for argv parsing.
 */

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_ANCHOR, "-anchor", "anchor", "Anchor",
	DEF_MESSAGE_ANCHOR, Tk_Offset(Message, anchor), 0},
    {TK_CONFIG_INT, "-aspect", "aspect", "Aspect",
	DEF_MESSAGE_ASPECT, Tk_Offset(Message, aspect), 0},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_MESSAGE_BG_COLOR, Tk_Offset(Message, border),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_MESSAGE_BG_MONO, Tk_Offset(Message, border),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_MESSAGE_BORDER_WIDTH, Tk_Offset(Message, borderWidth), 0},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_MESSAGE_CURSOR, Tk_Offset(Message, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_MESSAGE_FONT, Tk_Offset(Message, tkfont), 0},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_MESSAGE_FG, Tk_Offset(Message, fgColorPtr), 0},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_MESSAGE_HIGHLIGHT_BG,
	Tk_Offset(Message, highlightBgColorPtr), 0},
    {TK_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_MESSAGE_HIGHLIGHT, Tk_Offset(Message, highlightColorPtr), 0},
    {TK_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
	"HighlightThickness",
	DEF_MESSAGE_HIGHLIGHT_WIDTH, Tk_Offset(Message, highlightWidth), 0},
    {TK_CONFIG_JUSTIFY, "-justify", "justify", "Justify",
	DEF_MESSAGE_JUSTIFY, Tk_Offset(Message, justify), 0},
    {TK_CONFIG_PIXELS, "-padx", "padX", "Pad",
	DEF_MESSAGE_PADX, Tk_Offset(Message, padX), 0},
    {TK_CONFIG_PIXELS, "-pady", "padY", "Pad",
	DEF_MESSAGE_PADY, Tk_Offset(Message, padY), 0},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_MESSAGE_RELIEF, Tk_Offset(Message, relief), 0},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_MESSAGE_TAKE_FOCUS, Tk_Offset(Message, takeFocus),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-text", "text", "Text",
	DEF_MESSAGE_TEXT, Tk_Offset(Message, string), 0},
    {TK_CONFIG_STRING, "-textvariable", "textVariable", "Variable",
	DEF_MESSAGE_TEXT_VARIABLE, Tk_Offset(Message, textVarName),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_PIXELS, "-width", "width", "Width",
	DEF_MESSAGE_WIDTH, Tk_Offset(Message, width), 0},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		MessageCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static void		MessageEventProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static char *		MessageTextVarProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
static int		MessageWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static void		MessageWorldChanged _ANSI_ARGS_((
			    ClientData instanceData));
static void		ComputeMessageGeometry _ANSI_ARGS_((Message *msgPtr));
static int		ConfigureMessage _ANSI_ARGS_((Tcl_Interp *interp,
			    Message *msgPtr, int argc, char **argv,
			    int flags));
static void		DestroyMessage _ANSI_ARGS_((char *memPtr));
static void		DisplayMessage _ANSI_ARGS_((ClientData clientData));

/*
 * The structure below defines message class behavior by means of procedures
 * that can be invoked from generic window code.
 */

static TkClassProcs messageClass = {
    NULL,			/* createProc. */
    MessageWorldChanged,	/* geometryProc. */
    NULL			/* modalProc. */
};


/*
 *--------------------------------------------------------------
 *
 * Tk_MessageCmd --
 *
 *	This procedure is invoked to process the "message" Tcl
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
Tk_MessageCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    register Message *msgPtr;
    Tk_Window new;
    Tk_Window tkwin = (Tk_Window) clientData;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    new = Tk_CreateWindowFromPath(interp, tkwin, argv[1], (char *) NULL);
    if (new == NULL) {
	return TCL_ERROR;
    }

    msgPtr = (Message *) ckalloc(sizeof(Message));
    msgPtr->tkwin = new;
    msgPtr->display = Tk_Display(new);
    msgPtr->interp = interp;
    msgPtr->widgetCmd = Tcl_CreateCommand(interp, Tk_PathName(msgPtr->tkwin),
	    MessageWidgetCmd, (ClientData) msgPtr, MessageCmdDeletedProc);
    msgPtr->textLayout = NULL;
    msgPtr->string = NULL;
    msgPtr->numChars = 0;
    msgPtr->textVarName = NULL;
    msgPtr->border = NULL;
    msgPtr->borderWidth = 0;
    msgPtr->relief = TK_RELIEF_FLAT;
    msgPtr->highlightWidth = 0;
    msgPtr->highlightBgColorPtr = NULL;
    msgPtr->highlightColorPtr = NULL;
    msgPtr->tkfont = NULL;
    msgPtr->fgColorPtr = NULL;
    msgPtr->textGC = None;
    msgPtr->padX = 0;
    msgPtr->padY = 0;
    msgPtr->anchor = TK_ANCHOR_CENTER;
    msgPtr->width = 0;
    msgPtr->aspect = 150;
    msgPtr->msgWidth = 0;
    msgPtr->msgHeight = 0;
    msgPtr->justify = TK_JUSTIFY_LEFT;
    msgPtr->cursor = None;
    msgPtr->takeFocus = NULL;
    msgPtr->flags = 0;

    Tk_SetClass(msgPtr->tkwin, "Message");
    TkSetClassProcs(msgPtr->tkwin, &messageClass, (ClientData) msgPtr);
    Tk_CreateEventHandler(msgPtr->tkwin,
	    ExposureMask|StructureNotifyMask|FocusChangeMask,
	    MessageEventProc, (ClientData) msgPtr);
    if (ConfigureMessage(interp, msgPtr, argc-2, argv+2, 0) != TCL_OK) {
	goto error;
    }

    Tcl_SetResult(interp, Tk_PathName(msgPtr->tkwin), TCL_STATIC);
    return TCL_OK;

    error:
    Tk_DestroyWindow(msgPtr->tkwin);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * MessageWidgetCmd --
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
MessageWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about message widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    register Message *msgPtr = (Message *) clientData;
    size_t length;
    int c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cget option\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	return Tk_ConfigureValue(interp, msgPtr->tkwin, configSpecs,
		(char *) msgPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length  >= 2)) {
	if (argc == 2) {
	    return Tk_ConfigureInfo(interp, msgPtr->tkwin, configSpecs,
		    (char *) msgPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    return Tk_ConfigureInfo(interp, msgPtr->tkwin, configSpecs,
		    (char *) msgPtr, argv[2], 0);
	} else {
	    return ConfigureMessage(interp, msgPtr, argc-2, argv+2,
		    TK_CONFIG_ARGV_ONLY);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be cget or configure", (char *) NULL);
	return TCL_ERROR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyMessage --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a message at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the message is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyMessage(memPtr)
    char *memPtr;		/* Info about message widget. */
{
    register Message *msgPtr = (Message *) memPtr;

    /*
     * Free up all the stuff that requires special handling, then
     * let Tk_FreeOptions handle all the standard option-related
     * stuff.
     */

    Tk_FreeTextLayout(msgPtr->textLayout);
    if (msgPtr->textVarName != NULL) {
	Tcl_UntraceVar(msgPtr->interp, msgPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MessageTextVarProc, (ClientData) msgPtr);
    }
    if (msgPtr->textGC != None) {
	Tk_FreeGC(msgPtr->display, msgPtr->textGC);
    }
    Tk_FreeOptions(configSpecs, (char *) msgPtr, msgPtr->display, 0);
    ckfree((char *) msgPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureMessage --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a message widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then the interp's result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for msgPtr;  old resources get freed, if there
 *	were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureMessage(interp, msgPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    register Message *msgPtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    /*
     * Eliminate any existing trace on a variable monitored by the message.
     */

    if (msgPtr->textVarName != NULL) {
	Tcl_UntraceVar(interp, msgPtr->textVarName, 
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MessageTextVarProc, (ClientData) msgPtr);
    }

    if (Tk_ConfigureWidget(interp, msgPtr->tkwin, configSpecs,
	    argc, argv, (char *) msgPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }
    
    /*
     * If the message is to display the value of a variable, then set up
     * a trace on the variable's value, create the variable if it doesn't
     * exist, and fetch its current value.
     */

    if (msgPtr->textVarName != NULL) {
	char *value;

	value = Tcl_GetVar(interp, msgPtr->textVarName, TCL_GLOBAL_ONLY);
	if (value == NULL) {
	    Tcl_SetVar(interp, msgPtr->textVarName, msgPtr->string,
		    TCL_GLOBAL_ONLY);
	} else {
	    if (msgPtr->string != NULL) {
		ckfree(msgPtr->string);
	    }
	    msgPtr->string = strcpy(ckalloc(strlen(value) + 1), value);
	}
	Tcl_TraceVar(interp, msgPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MessageTextVarProc, (ClientData) msgPtr);
    }

    /*
     * A few other options need special processing, such as setting
     * the background from a 3-D border or handling special defaults
     * that couldn't be specified to Tk_ConfigureWidget.
     */

    msgPtr->numChars = Tcl_NumUtfChars(msgPtr->string, -1);

    Tk_SetBackgroundFromBorder(msgPtr->tkwin, msgPtr->border);

    if (msgPtr->highlightWidth < 0) {
	msgPtr->highlightWidth = 0;
    }

    MessageWorldChanged((ClientData) msgPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * MessageWorldChanged --
 *
 *      This procedure is called when the world has changed in some
 *      way and the widget needs to recompute all its graphics contexts
 *	and determine its new geometry.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Message will be relayed out and redisplayed.
 *
 *---------------------------------------------------------------------------
 */
 
static void
MessageWorldChanged(instanceData)
    ClientData instanceData;	/* Information about widget. */
{
    XGCValues gcValues;
    GC gc;
    Tk_FontMetrics fm;
    Message *msgPtr;

    msgPtr = (Message *) instanceData;

    gcValues.font = Tk_FontId(msgPtr->tkfont);
    gcValues.foreground = msgPtr->fgColorPtr->pixel;
    gc = Tk_GetGC(msgPtr->tkwin, GCForeground | GCFont, &gcValues);
    if (msgPtr->textGC != None) {
	Tk_FreeGC(msgPtr->display, msgPtr->textGC);
    }
    msgPtr->textGC = gc;

    Tk_GetFontMetrics(msgPtr->tkfont, &fm);
    if (msgPtr->padX < 0) {
	msgPtr->padX = fm.ascent / 2;
    }
    if (msgPtr->padY == -1) {
	msgPtr->padY = fm.ascent / 4;
    }

    /*
     * Recompute the desired geometry for the window, and arrange for
     * the window to be redisplayed.
     */

    ComputeMessageGeometry(msgPtr);
    if ((msgPtr->tkwin != NULL) && Tk_IsMapped(msgPtr->tkwin)
	    && !(msgPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(DisplayMessage, (ClientData) msgPtr);
	msgPtr->flags |= REDRAW_PENDING;
    }
}

/*
 *--------------------------------------------------------------
 *
 * ComputeMessageGeometry --
 *
 *	Compute the desired geometry for a message window,
 *	taking into account the desired aspect ratio for the
 *	window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Tk_GeometryRequest is called to inform the geometry
 *	manager of the desired geometry for this window.
 *
 *--------------------------------------------------------------
 */

static void
ComputeMessageGeometry(msgPtr)
    register Message *msgPtr;	/* Information about window. */
{
    int width, inc, height;
    int thisWidth, thisHeight, maxWidth;
    int aspect, lowerBound, upperBound, inset;

    Tk_FreeTextLayout(msgPtr->textLayout);

    inset = msgPtr->borderWidth + msgPtr->highlightWidth;

    /*
     * Compute acceptable bounds for the final aspect ratio.
     */

    aspect = msgPtr->aspect/10;
    if (aspect < 5) {
	aspect = 5;
    }
    lowerBound = msgPtr->aspect - aspect;
    upperBound = msgPtr->aspect + aspect;

    /*
     * Do the computation in multiple passes:  start off with
     * a very wide window, and compute its height.  Then change
     * the width and try again.  Reduce the size of the change
     * and iterate until dimensions are found that approximate
     * the desired aspect ratio.  Or, if the user gave an explicit
     * width then just use that.
     */

    if (msgPtr->width > 0) {
	width = msgPtr->width;
	inc = 0;
    } else {
	width = WidthOfScreen(Tk_Screen(msgPtr->tkwin))/2;
	inc = width/2;
    }

    for ( ; ; inc /= 2) {
	msgPtr->textLayout = Tk_ComputeTextLayout(msgPtr->tkfont,
		msgPtr->string, msgPtr->numChars, width, msgPtr->justify,
		0, &thisWidth, &thisHeight);
	maxWidth = thisWidth + 2 * (inset + msgPtr->padX);
	height = thisHeight + 2 * (inset + msgPtr->padY);

	if (inc <= 2) {
	    break;
	}
	aspect = (100 * maxWidth) / height;

	if (aspect < lowerBound) {
	    width += inc;
	} else if (aspect > upperBound) {
	    width -= inc;
	} else {
	    break;
	}
	Tk_FreeTextLayout(msgPtr->textLayout);
    }
    msgPtr->msgWidth = thisWidth;
    msgPtr->msgHeight = thisHeight;
    Tk_GeometryRequest(msgPtr->tkwin, maxWidth, height);
    Tk_SetInternalBorder(msgPtr->tkwin, inset);
}

/*
 *--------------------------------------------------------------
 *
 * DisplayMessage --
 *
 *	This procedure redraws the contents of a message window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information appears on the screen.
 *
 *--------------------------------------------------------------
 */

static void
DisplayMessage(clientData)
    ClientData clientData;	/* Information about window. */
{
    register Message *msgPtr = (Message *) clientData;
    register Tk_Window tkwin = msgPtr->tkwin;
    int x, y;

    msgPtr->flags &= ~REDRAW_PENDING;
    if ((msgPtr->tkwin == NULL) || !Tk_IsMapped(tkwin)) {
	return;
    }
    Tk_Fill3DRectangle(tkwin, Tk_WindowId(tkwin), msgPtr->border, 0, 0,
	    Tk_Width(tkwin), Tk_Height(tkwin), 0, TK_RELIEF_FLAT);

    /*
     * Compute starting y-location for message based on message size
     * and anchor option.
     */

    TkComputeAnchor(msgPtr->anchor, tkwin, msgPtr->padX, msgPtr->padY,
	    msgPtr->msgWidth, msgPtr->msgHeight, &x, &y);
    Tk_DrawTextLayout(Tk_Display(tkwin), Tk_WindowId(tkwin), msgPtr->textGC,
	    msgPtr->textLayout, x, y, 0, -1);

    if (msgPtr->relief != TK_RELIEF_FLAT) {
	Tk_Draw3DRectangle(tkwin, Tk_WindowId(tkwin), msgPtr->border,
		msgPtr->highlightWidth, msgPtr->highlightWidth,
		Tk_Width(tkwin) - 2*msgPtr->highlightWidth,
		Tk_Height(tkwin) - 2*msgPtr->highlightWidth,
		msgPtr->borderWidth, msgPtr->relief);
    }
    if (msgPtr->highlightWidth != 0) {
	GC fgGC, bgGC;

	bgGC = Tk_GCForColor(msgPtr->highlightBgColorPtr, Tk_WindowId(tkwin));
	if (msgPtr->flags & GOT_FOCUS) {
	    fgGC = Tk_GCForColor(msgPtr->highlightColorPtr, Tk_WindowId(tkwin));
	    TkpDrawHighlightBorder(tkwin, fgGC, bgGC, msgPtr->highlightWidth,
		    Tk_WindowId(tkwin));
	} else {
	    TkpDrawHighlightBorder(tkwin, bgGC, bgGC, msgPtr->highlightWidth,
		    Tk_WindowId(tkwin));
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * MessageEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on messages.
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
MessageEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Message *msgPtr = (Message *) clientData;

    if (((eventPtr->type == Expose) && (eventPtr->xexpose.count == 0))
	    || (eventPtr->type == ConfigureNotify)) {
	goto redraw;
    } else if (eventPtr->type == DestroyNotify) {
	if (msgPtr->tkwin != NULL) {
	    msgPtr->tkwin = NULL;
	    Tcl_DeleteCommandFromToken(msgPtr->interp, msgPtr->widgetCmd);
	}
	if (msgPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(DisplayMessage, (ClientData) msgPtr);
	}
	Tcl_EventuallyFree((ClientData) msgPtr, DestroyMessage);
    } else if (eventPtr->type == FocusIn) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    msgPtr->flags |= GOT_FOCUS;
	    if (msgPtr->highlightWidth > 0) {
		goto redraw;
	    }
	}
    } else if (eventPtr->type == FocusOut) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    msgPtr->flags &= ~GOT_FOCUS;
	    if (msgPtr->highlightWidth > 0) {
		goto redraw;
	    }
	}
    }
    return;

    redraw:
    if ((msgPtr->tkwin != NULL) && !(msgPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(DisplayMessage, (ClientData) msgPtr);
	msgPtr->flags |= REDRAW_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MessageCmdDeletedProc --
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
MessageCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    Message *msgPtr = (Message *) clientData;
    Tk_Window tkwin = msgPtr->tkwin;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tkwin != NULL) {
	msgPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *--------------------------------------------------------------
 *
 * MessageTextVarProc --
 *
 *	This procedure is invoked when someone changes the variable
 *	whose contents are to be displayed in a message.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The text displayed in the message will change to match the
 *	variable.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static char *
MessageTextVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about message. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    register Message *msgPtr = (Message *) clientData;
    char *value;

    /*
     * If the variable is unset, then immediately recreate it unless
     * the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_SetVar(interp, msgPtr->textVarName, msgPtr->string,
		    TCL_GLOBAL_ONLY);
	    Tcl_TraceVar(interp, msgPtr->textVarName,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    MessageTextVarProc, clientData);
	}
	return (char *) NULL;
    }

    value = Tcl_GetVar(interp, msgPtr->textVarName, TCL_GLOBAL_ONLY);
    if (value == NULL) {
	value = "";
    }
    if (msgPtr->string != NULL) {
	ckfree(msgPtr->string);
    }
    msgPtr->numChars = Tcl_NumUtfChars(value, -1);
    msgPtr->string = (char *) ckalloc((unsigned) (strlen(value) + 1));
    strcpy(msgPtr->string, value);
    ComputeMessageGeometry(msgPtr);

    if ((msgPtr->tkwin != NULL) && Tk_IsMapped(msgPtr->tkwin)
	    && !(msgPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(DisplayMessage, (ClientData) msgPtr);
	msgPtr->flags |= REDRAW_PENDING;
    }
    return (char *) NULL;
}
