/* 
 * tkMenubutton.c --
 *
 *	This module implements button-like widgets that are used
 *	to invoke pull-down menus.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMenubutton.c 1.94 97/07/31 09:10:37
 */

#include "tkMenubutton.h"
#include "tkPort.h"
#include "default.h"

/*
 * Uids internal to menubuttons.
 */

static Tk_Uid aboveUid = NULL;
static Tk_Uid belowUid = NULL;
static Tk_Uid leftUid = NULL;
static Tk_Uid rightUid = NULL;
static Tk_Uid flushUid = NULL;

/*
 * Information used for parsing configuration specs:
 */

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground", "Foreground",
	DEF_MENUBUTTON_ACTIVE_BG_COLOR, Tk_Offset(TkMenuButton, activeBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground", "Foreground",
	DEF_MENUBUTTON_ACTIVE_BG_MONO, Tk_Offset(TkMenuButton, activeBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_MENUBUTTON_ACTIVE_FG_COLOR, Tk_Offset(TkMenuButton, activeFg),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_MENUBUTTON_ACTIVE_FG_MONO, Tk_Offset(TkMenuButton, activeFg),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_ANCHOR, "-anchor", "anchor", "Anchor",
	DEF_MENUBUTTON_ANCHOR, Tk_Offset(TkMenuButton, anchor), 0},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_MENUBUTTON_BG_COLOR, Tk_Offset(TkMenuButton, normalBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_MENUBUTTON_BG_MONO, Tk_Offset(TkMenuButton, normalBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_BITMAP, "-bitmap", "bitmap", "Bitmap",
	DEF_MENUBUTTON_BITMAP, Tk_Offset(TkMenuButton, bitmap),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_MENUBUTTON_BORDER_WIDTH, Tk_Offset(TkMenuButton, borderWidth), 0},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_MENUBUTTON_CURSOR, Tk_Offset(TkMenuButton, cursor),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_UID, "-direction", "direction", "Direction",
    	DEF_MENUBUTTON_DIRECTION, Tk_Offset(TkMenuButton, direction), 
	0},
    {TK_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
	"DisabledForeground", DEF_MENUBUTTON_DISABLED_FG_COLOR,
	Tk_Offset(TkMenuButton, disabledFg),
	TK_CONFIG_COLOR_ONLY|TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
	"DisabledForeground", DEF_MENUBUTTON_DISABLED_FG_MONO,
	Tk_Offset(TkMenuButton, disabledFg),
	TK_CONFIG_MONO_ONLY|TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_MENUBUTTON_FONT, Tk_Offset(TkMenuButton, tkfont), 0},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_MENUBUTTON_FG, Tk_Offset(TkMenuButton, normalFg), 0},
    {TK_CONFIG_STRING, "-height", "height", "Height",
	DEF_MENUBUTTON_HEIGHT, Tk_Offset(TkMenuButton, heightString), 0},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_MENUBUTTON_HIGHLIGHT_BG,
	Tk_Offset(TkMenuButton, highlightBgColorPtr), 0},
    {TK_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_MENUBUTTON_HIGHLIGHT, Tk_Offset(TkMenuButton, highlightColorPtr),
	0},
    {TK_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
	"HighlightThickness", DEF_MENUBUTTON_HIGHLIGHT_WIDTH,
	Tk_Offset(TkMenuButton, highlightWidth), 0},
    {TK_CONFIG_STRING, "-image", "image", "Image",
	DEF_MENUBUTTON_IMAGE, Tk_Offset(TkMenuButton, imageString),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-indicatoron", "indicatorOn", "IndicatorOn",
	DEF_MENUBUTTON_INDICATOR, Tk_Offset(TkMenuButton, indicatorOn), 0},
    {TK_CONFIG_JUSTIFY, "-justify", "justify", "Justify",
	DEF_MENUBUTTON_JUSTIFY, Tk_Offset(TkMenuButton, justify), 0},
    {TK_CONFIG_STRING, "-menu", "menu", "Menu",
	DEF_MENUBUTTON_MENU, Tk_Offset(TkMenuButton, menuName),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_PIXELS, "-padx", "padX", "Pad",
	DEF_MENUBUTTON_PADX, Tk_Offset(TkMenuButton, padX), 0},
    {TK_CONFIG_PIXELS, "-pady", "padY", "Pad",
	DEF_MENUBUTTON_PADY, Tk_Offset(TkMenuButton, padY), 0},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_MENUBUTTON_RELIEF, Tk_Offset(TkMenuButton, relief), 0},
    {TK_CONFIG_UID, "-state", "state", "State",
	DEF_MENUBUTTON_STATE, Tk_Offset(TkMenuButton, state), 0},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_MENUBUTTON_TAKE_FOCUS, Tk_Offset(TkMenuButton, takeFocus),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-text", "text", "Text",
	DEF_MENUBUTTON_TEXT, Tk_Offset(TkMenuButton, text), 0},
    {TK_CONFIG_STRING, "-textvariable", "textVariable", "Variable",
	DEF_MENUBUTTON_TEXT_VARIABLE, Tk_Offset(TkMenuButton, textVarName),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_INT, "-underline", "underline", "Underline",
	DEF_MENUBUTTON_UNDERLINE, Tk_Offset(TkMenuButton, underline), 0},
    {TK_CONFIG_STRING, "-width", "width", "Width",
	DEF_MENUBUTTON_WIDTH, Tk_Offset(TkMenuButton, widthString), 0},
    {TK_CONFIG_PIXELS, "-wraplength", "wrapLength", "WrapLength",
	DEF_MENUBUTTON_WRAP_LENGTH, Tk_Offset(TkMenuButton, wrapLength), 0},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		MenuButtonCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static void		MenuButtonEventProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static void		MenuButtonImageProc _ANSI_ARGS_((ClientData clientData,
			    int x, int y, int width, int height, int imgWidth,
			    int imgHeight));
static char *		MenuButtonTextVarProc _ANSI_ARGS_((
			    ClientData clientData, Tcl_Interp *interp,
			    char *name1, char *name2, int flags));
static int		MenuButtonWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static int		ConfigureMenuButton _ANSI_ARGS_((Tcl_Interp *interp,
			    TkMenuButton *mbPtr, int argc, char **argv,
			    int flags));
static void		DestroyMenuButton _ANSI_ARGS_((char *memPtr));

/*
 *--------------------------------------------------------------
 *
 * Tk_MenubuttonCmd --
 *
 *	This procedure is invoked to process the "button", "label",
 *	"radiobutton", and "checkbutton" Tcl commands.  See the
 *	user documentation for details on what it does.
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
Tk_MenubuttonCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    register TkMenuButton *mbPtr;
    Tk_Window tkwin = (Tk_Window) clientData;
    Tk_Window new;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Create the new window.
     */

    new = Tk_CreateWindowFromPath(interp, tkwin, argv[1], (char *) NULL);
    if (new == NULL) {
	return TCL_ERROR;
    }

    Tk_SetClass(new, "Menubutton");
    mbPtr = TkpCreateMenuButton(new);

    TkSetClassProcs(new, &tkpMenubuttonClass, (ClientData) mbPtr);

    /*
     * Initialize the data structure for the button.
     */

    mbPtr->tkwin = new;
    mbPtr->display = Tk_Display (new);
    mbPtr->interp = interp;
    mbPtr->widgetCmd = Tcl_CreateCommand(interp, Tk_PathName(mbPtr->tkwin),
	    MenuButtonWidgetCmd, (ClientData) mbPtr, MenuButtonCmdDeletedProc);
    mbPtr->menuName = NULL;
    mbPtr->text = NULL;
    mbPtr->underline = -1;
    mbPtr->textVarName = NULL;
    mbPtr->bitmap = None;
    mbPtr->imageString = NULL;
    mbPtr->image = NULL;
    mbPtr->state = tkNormalUid;
    mbPtr->normalBorder = NULL;
    mbPtr->activeBorder = NULL;
    mbPtr->borderWidth = 0;
    mbPtr->relief = TK_RELIEF_FLAT;
    mbPtr->highlightWidth = 0;
    mbPtr->highlightBgColorPtr = NULL;
    mbPtr->highlightColorPtr = NULL;
    mbPtr->inset = 0;
    mbPtr->tkfont = NULL;
    mbPtr->normalFg = NULL;
    mbPtr->activeFg = NULL;
    mbPtr->disabledFg = NULL;
    mbPtr->normalTextGC = None;
    mbPtr->activeTextGC = None;
    mbPtr->gray = None;
    mbPtr->disabledGC = None;
    mbPtr->leftBearing = 0;
    mbPtr->rightBearing = 0;
    mbPtr->widthString = NULL;
    mbPtr->heightString = NULL;
    mbPtr->width = 0;
    mbPtr->width = 0;
    mbPtr->wrapLength = 0;
    mbPtr->padX = 0;
    mbPtr->padY = 0;
    mbPtr->anchor = TK_ANCHOR_CENTER;
    mbPtr->justify = TK_JUSTIFY_CENTER;
    mbPtr->textLayout = NULL;
    mbPtr->indicatorOn = 0;
    mbPtr->indicatorWidth = 0;
    mbPtr->indicatorHeight = 0;
    mbPtr->cursor = None;
    mbPtr->takeFocus = NULL;
    mbPtr->flags = 0;
    if (aboveUid == NULL) {
	aboveUid = Tk_GetUid("above");
	belowUid = Tk_GetUid("below");
	leftUid = Tk_GetUid("left");
	rightUid = Tk_GetUid("right");
	flushUid = Tk_GetUid("flush");
    }
    mbPtr->direction = flushUid;

    Tk_CreateEventHandler(mbPtr->tkwin,
	    ExposureMask|StructureNotifyMask|FocusChangeMask,
	    MenuButtonEventProc, (ClientData) mbPtr);
    if (ConfigureMenuButton(interp, mbPtr, argc-2, argv+2, 0) != TCL_OK) {
	Tk_DestroyWindow(mbPtr->tkwin);
	return TCL_ERROR;
    }

    interp->result = Tk_PathName(mbPtr->tkwin);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * MenuButtonWidgetCmd --
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
MenuButtonWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about button widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    register TkMenuButton *mbPtr = (TkMenuButton *) clientData;
    int result;
    size_t length;
    int c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Tcl_Preserve((ClientData) mbPtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cget option\"",
		    (char *) NULL);
	    result = TCL_ERROR;
	} else {
	    result = Tk_ConfigureValue(interp, mbPtr->tkwin, configSpecs,
		    (char *) mbPtr, argv[2], 0);
	}
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc == 2) {
	    result = Tk_ConfigureInfo(interp, mbPtr->tkwin, configSpecs,
		    (char *) mbPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Tk_ConfigureInfo(interp, mbPtr->tkwin, configSpecs,
		    (char *) mbPtr, argv[2], 0);
	} else {
	    result = ConfigureMenuButton(interp, mbPtr, argc-2, argv+2,
		    TK_CONFIG_ARGV_ONLY);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be cget or configure",
		(char *) NULL);
	result = TCL_ERROR;
    }
    Tcl_Release((ClientData) mbPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyMenuButton --
 *
 *	This procedure is invoked to recycle all of the resources
 *	associated with a button widget.  It is invoked as a
 *	when-idle handler in order to make sure that there is no
 *	other use of the button pending at the time of the deletion.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the widget is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyMenuButton(memPtr)
    char *memPtr;		/* Info about button widget. */
{
    register TkMenuButton *mbPtr = (TkMenuButton *) memPtr;

    /*
     * Free up all the stuff that requires special handling, then
     * let Tk_FreeOptions handle all the standard option-related
     * stuff.
     */

    if (mbPtr->textVarName != NULL) {
	Tcl_UntraceVar(mbPtr->interp, mbPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MenuButtonTextVarProc, (ClientData) mbPtr);
    }
    if (mbPtr->image != NULL) {
	Tk_FreeImage(mbPtr->image);
    }
    if (mbPtr->normalTextGC != None) {
	Tk_FreeGC(mbPtr->display, mbPtr->normalTextGC);
    }
    if (mbPtr->activeTextGC != None) {
	Tk_FreeGC(mbPtr->display, mbPtr->activeTextGC);
    }
    if (mbPtr->gray != None) {
	Tk_FreeBitmap(mbPtr->display, mbPtr->gray);
    }
    if (mbPtr->disabledGC != None) {
	Tk_FreeGC(mbPtr->display, mbPtr->disabledGC);
    }
    Tk_FreeTextLayout(mbPtr->textLayout);
    Tk_FreeOptions(configSpecs, (char *) mbPtr, mbPtr->display, 0);
    ckfree((char *) mbPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureMenuButton --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a menubutton widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for mbPtr;  old resources get freed, if there
 *	were any.  The menubutton is redisplayed.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureMenuButton(interp, mbPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    register TkMenuButton *mbPtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    int result;
    Tk_Image image;

    /*
     * Eliminate any existing trace on variables monitored by the menubutton.
     */

    if (mbPtr->textVarName != NULL) {
	Tcl_UntraceVar(interp, mbPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MenuButtonTextVarProc, (ClientData) mbPtr);
    }

    result = Tk_ConfigureWidget(interp, mbPtr->tkwin, configSpecs,
	    argc, argv, (char *) mbPtr, flags);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * A few options need special processing, such as setting the
     * background from a 3-D border, or filling in complicated
     * defaults that couldn't be specified to Tk_ConfigureWidget.
     */

    if ((mbPtr->state == tkActiveUid) && !Tk_StrictMotif(mbPtr->tkwin)) {
	Tk_SetBackgroundFromBorder(mbPtr->tkwin, mbPtr->activeBorder);
    } else {
	Tk_SetBackgroundFromBorder(mbPtr->tkwin, mbPtr->normalBorder);
	if ((mbPtr->state != tkNormalUid) && (mbPtr->state != tkActiveUid)
		&& (mbPtr->state != tkDisabledUid)) {
	    Tcl_AppendResult(interp, "bad state value \"", mbPtr->state,
		    "\": must be normal, active, or disabled", (char *) NULL);
	    mbPtr->state = tkNormalUid;
	    return TCL_ERROR;
	}
    }

    if ((mbPtr->direction != aboveUid) && (mbPtr->direction != belowUid)
	    && (mbPtr->direction != leftUid) && (mbPtr->direction != rightUid)
	    && (mbPtr->direction != flushUid)) {
	Tcl_AppendResult(interp, "bad direction value \"", mbPtr->direction,
		"\": must be above, below, left, right, or flush",
		(char *) NULL);
	mbPtr->direction = belowUid;
	return TCL_ERROR;
    }
    
    if (mbPtr->highlightWidth < 0) {
	mbPtr->highlightWidth = 0;
    }

    if (mbPtr->padX < 0) {
	mbPtr->padX = 0;
    }
    if (mbPtr->padY < 0) {
	mbPtr->padY = 0;
    }

    /*
     * Get the image for the widget, if there is one.  Allocate the
     * new image before freeing the old one, so that the reference
     * count doesn't go to zero and cause image data to be discarded.
     */

    if (mbPtr->imageString != NULL) {
	image = Tk_GetImage(mbPtr->interp, mbPtr->tkwin,
		mbPtr->imageString, MenuButtonImageProc, (ClientData) mbPtr);
	if (image == NULL) {
	    return TCL_ERROR;
	}
    } else {
	image = NULL;
    }
    if (mbPtr->image != NULL) {
	Tk_FreeImage(mbPtr->image);
    }
    mbPtr->image = image;

    if ((mbPtr->image == NULL) && (mbPtr->bitmap == None)
	    && (mbPtr->textVarName != NULL)) {
	/*
	 * The menubutton displays a variable.  Set up a trace to watch
	 * for any changes in it.
	 */

	char *value;

	value = Tcl_GetVar(interp, mbPtr->textVarName, TCL_GLOBAL_ONLY);
	if (value == NULL) {
	    Tcl_SetVar(interp, mbPtr->textVarName, mbPtr->text,
		    TCL_GLOBAL_ONLY);
	} else {
	    if (mbPtr->text != NULL) {
		ckfree(mbPtr->text);
	    }
	    mbPtr->text = (char *) ckalloc((unsigned) (strlen(value) + 1));
	    strcpy(mbPtr->text, value);
	}
	Tcl_TraceVar(interp, mbPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		MenuButtonTextVarProc, (ClientData) mbPtr);
    }

    /*
     * Recompute the geometry for the button.
     */

    if ((mbPtr->bitmap != None) || (mbPtr->image != NULL)) {
	if (Tk_GetPixels(interp, mbPtr->tkwin, mbPtr->widthString,
		&mbPtr->width) != TCL_OK) {
	    widthError:
	    Tcl_AddErrorInfo(interp, "\n    (processing -width option)");
	    return TCL_ERROR;
	}
	if (Tk_GetPixels(interp, mbPtr->tkwin, mbPtr->heightString,
		&mbPtr->height) != TCL_OK) {
	    heightError:
	    Tcl_AddErrorInfo(interp, "\n    (processing -height option)");
	    return TCL_ERROR;
	}
    } else {
	if (Tcl_GetInt(interp, mbPtr->widthString, &mbPtr->width)
		!= TCL_OK) {
	    goto widthError;
	}
	if (Tcl_GetInt(interp, mbPtr->heightString, &mbPtr->height)
		!= TCL_OK) {
	    goto heightError;
	}
    }
    TkMenuButtonWorldChanged((ClientData) mbPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TkMenuButtonWorldChanged --
 *
 *      This procedure is called when the world has changed in some
 *      way and the widget needs to recompute all its graphics contexts
 *	and determine its new geometry.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      TkMenuButton will be relayed out and redisplayed.
 *
 *---------------------------------------------------------------------------
 */
 
void
TkMenuButtonWorldChanged(instanceData)
    ClientData instanceData;	/* Information about widget. */
{
    XGCValues gcValues;
    GC gc;
    unsigned long mask;
    TkMenuButton *mbPtr;

    mbPtr = (TkMenuButton *) instanceData;

    gcValues.font = Tk_FontId(mbPtr->tkfont);
    gcValues.foreground = mbPtr->normalFg->pixel;
    gcValues.background = Tk_3DBorderColor(mbPtr->normalBorder)->pixel;

    /*
     * Note: GraphicsExpose events are disabled in GC's because they're
     * used to copy stuff from an off-screen pixmap onto the screen (we know
     * that there's no problem with obscured areas).
     */

    gcValues.graphics_exposures = False;
    mask = GCForeground | GCBackground | GCFont | GCGraphicsExposures;
    gc = Tk_GetGC(mbPtr->tkwin, mask, &gcValues);
    if (mbPtr->normalTextGC != None) {
	Tk_FreeGC(mbPtr->display, mbPtr->normalTextGC);
    }
    mbPtr->normalTextGC = gc;

    gcValues.font = Tk_FontId(mbPtr->tkfont);
    gcValues.foreground = mbPtr->activeFg->pixel;
    gcValues.background = Tk_3DBorderColor(mbPtr->activeBorder)->pixel;
    mask = GCForeground | GCBackground | GCFont;
    gc = Tk_GetGC(mbPtr->tkwin, mask, &gcValues);
    if (mbPtr->activeTextGC != None) {
	Tk_FreeGC(mbPtr->display, mbPtr->activeTextGC);
    }
    mbPtr->activeTextGC = gc;

    gcValues.font = Tk_FontId(mbPtr->tkfont);
    gcValues.background = Tk_3DBorderColor(mbPtr->normalBorder)->pixel;
    if ((mbPtr->disabledFg != NULL) && (mbPtr->imageString == NULL)) {
	gcValues.foreground = mbPtr->disabledFg->pixel;
	mask = GCForeground | GCBackground | GCFont;
    } else {
	gcValues.foreground = gcValues.background;
	mask = GCForeground;
	if (mbPtr->gray == None) {
	    mbPtr->gray = Tk_GetBitmap(NULL, mbPtr->tkwin,
		    Tk_GetUid("gray50"));
	}
	if (mbPtr->gray != None) {
	    gcValues.fill_style = FillStippled;
	    gcValues.stipple = mbPtr->gray;
	    mask |= GCFillStyle | GCStipple;
	}
    }
    gc = Tk_GetGC(mbPtr->tkwin, mask, &gcValues);
    if (mbPtr->disabledGC != None) {
	Tk_FreeGC(mbPtr->display, mbPtr->disabledGC);
    }
    mbPtr->disabledGC = gc;

    TkpComputeMenuButtonGeometry(mbPtr);

    /*
     * Lastly, arrange for the button to be redisplayed.
     */

    if (Tk_IsMapped(mbPtr->tkwin) && !(mbPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(TkpDisplayMenuButton, (ClientData) mbPtr);
	mbPtr->flags |= REDRAW_PENDING;
    }
}

/*
 *--------------------------------------------------------------
 *
 * MenuButtonEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on buttons.
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
MenuButtonEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    TkMenuButton *mbPtr = (TkMenuButton *) clientData;
    if ((eventPtr->type == Expose) && (eventPtr->xexpose.count == 0)) {
	goto redraw;
    } else if (eventPtr->type == ConfigureNotify) {
	/*
	 * Must redraw after size changes, since layout could have changed
	 * and borders will need to be redrawn.
	 */

	goto redraw;
    } else if (eventPtr->type == DestroyNotify) {
	TkpDestroyMenuButton(mbPtr);
	if (mbPtr->tkwin != NULL) {
	    mbPtr->tkwin = NULL;
	    Tcl_DeleteCommandFromToken(mbPtr->interp, mbPtr->widgetCmd);
	}
	if (mbPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(TkpDisplayMenuButton, (ClientData) mbPtr);
	}
	Tcl_EventuallyFree((ClientData) mbPtr, DestroyMenuButton);
    } else if (eventPtr->type == FocusIn) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    mbPtr->flags |= GOT_FOCUS;
	    if (mbPtr->highlightWidth > 0) {
		goto redraw;
	    }
	}
    } else if (eventPtr->type == FocusOut) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    mbPtr->flags &= ~GOT_FOCUS;
	    if (mbPtr->highlightWidth > 0) {
		goto redraw;
	    }
	}
    }
    return;

    redraw:
    if ((mbPtr->tkwin != NULL) && !(mbPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(TkpDisplayMenuButton, (ClientData) mbPtr);
	mbPtr->flags |= REDRAW_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MenuButtonCmdDeletedProc --
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
MenuButtonCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    TkMenuButton *mbPtr = (TkMenuButton *) clientData;
    Tk_Window tkwin = mbPtr->tkwin;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tkwin != NULL) {
	mbPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *--------------------------------------------------------------
 *
 * MenuButtonTextVarProc --
 *
 *	This procedure is invoked when someone changes the variable
 *	whose contents are to be displayed in a menu button.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The text displayed in the menu button will change to match the
 *	variable.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static char *
MenuButtonTextVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about button. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    register TkMenuButton *mbPtr = (TkMenuButton *) clientData;
    char *value;

    /*
     * If the variable is unset, then immediately recreate it unless
     * the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_SetVar(interp, mbPtr->textVarName, mbPtr->text,
		    TCL_GLOBAL_ONLY);
	    Tcl_TraceVar(interp, mbPtr->textVarName,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    MenuButtonTextVarProc, clientData);
	}
	return (char *) NULL;
    }

    value = Tcl_GetVar(interp, mbPtr->textVarName, TCL_GLOBAL_ONLY);
    if (value == NULL) {
	value = "";
    }
    if (mbPtr->text != NULL) {
	ckfree(mbPtr->text);
    }
    mbPtr->text = (char *) ckalloc((unsigned) (strlen(value) + 1));
    strcpy(mbPtr->text, value);
    TkpComputeMenuButtonGeometry(mbPtr);

    if ((mbPtr->tkwin != NULL) && Tk_IsMapped(mbPtr->tkwin)
	    && !(mbPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(TkpDisplayMenuButton, (ClientData) mbPtr);
	mbPtr->flags |= REDRAW_PENDING;
    }
    return (char *) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * MenuButtonImageProc --
 *
 *	This procedure is invoked by the image code whenever the manager
 *	for an image does something that affects the size of contents
 *	of an image displayed in a button.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the button to get redisplayed.
 *
 *----------------------------------------------------------------------
 */

static void
MenuButtonImageProc(clientData, x, y, width, height, imgWidth, imgHeight)
    ClientData clientData;		/* Pointer to widget record. */
    int x, y;				/* Upper left pixel (within image)
					 * that must be redisplayed. */
    int width, height;			/* Dimensions of area to redisplay
					 * (may be <= 0). */
    int imgWidth, imgHeight;		/* New dimensions of image. */
{
    register TkMenuButton *mbPtr = (TkMenuButton *) clientData;

    if (mbPtr->tkwin != NULL) {
	TkpComputeMenuButtonGeometry(mbPtr);
	if (Tk_IsMapped(mbPtr->tkwin) && !(mbPtr->flags & REDRAW_PENDING)) {
	    Tcl_DoWhenIdle(TkpDisplayMenuButton, (ClientData) mbPtr);
	    mbPtr->flags |= REDRAW_PENDING;
	}
    }
}
