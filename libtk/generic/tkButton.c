/* 
 * tkButton.c --
 *
 *	This module implements a collection of button-like
 *	widgets for the Tk toolkit.  The widgets implemented
 *	include labels, buttons, check buttons, and radio
 *	buttons.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkButton.c 1.144 97/07/31 09:04:57
 */

#include "tkButton.h"
#include "default.h"

/*
 * Class names for buttons, indexed by one of the type values above.
 */

static char *classNames[] = {"Label", "Button", "Checkbutton", "Radiobutton"};

/*
 * The class procedure table for the button widget.
 */

static int configFlags[] = {LABEL_MASK, BUTTON_MASK,
	CHECK_BUTTON_MASK, RADIO_BUTTON_MASK};

/*
 * Information used for parsing configuration specs:
 */

Tk_ConfigSpec tkpButtonConfigSpecs[] = {
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground", "Foreground",
	DEF_BUTTON_ACTIVE_BG_COLOR, Tk_Offset(TkButton, activeBorder),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK
	|TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground", "Foreground",
	DEF_BUTTON_ACTIVE_BG_MONO, Tk_Offset(TkButton, activeBorder),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK
	|TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_BUTTON_ACTIVE_FG_COLOR, Tk_Offset(TkButton, activeFg), 
	BUTTON_MASK|TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_CHKRAD_ACTIVE_FG_COLOR, Tk_Offset(TkButton, activeFg), 
	CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-activeforeground", "activeForeground", "Background",
	DEF_BUTTON_ACTIVE_FG_MONO, Tk_Offset(TkButton, activeFg), 
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK
	|TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_ANCHOR, "-anchor", "anchor", "Anchor",
	DEF_BUTTON_ANCHOR, Tk_Offset(TkButton, anchor), ALL_MASK},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_BUTTON_BG_COLOR, Tk_Offset(TkButton, normalBorder),
	ALL_MASK | TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_BUTTON_BG_MONO, Tk_Offset(TkButton, normalBorder),
	ALL_MASK | TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *) NULL,
	(char *) NULL, 0, ALL_MASK},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, ALL_MASK},
    {TK_CONFIG_BITMAP, "-bitmap", "bitmap", "Bitmap",
	DEF_BUTTON_BITMAP, Tk_Offset(TkButton, bitmap),
	ALL_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_BUTTON_BORDER_WIDTH, Tk_Offset(TkButton, borderWidth), ALL_MASK},
    {TK_CONFIG_STRING, "-command", "command", "Command",
	DEF_BUTTON_COMMAND, Tk_Offset(TkButton, command),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_BUTTON_CURSOR, Tk_Offset(TkButton, cursor),
	ALL_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_UID, "-default", "default", "Default",
        DEF_BUTTON_DEFAULT, Tk_Offset(TkButton, defaultState), BUTTON_MASK},
    {TK_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
	"DisabledForeground", DEF_BUTTON_DISABLED_FG_COLOR,
	Tk_Offset(TkButton, disabledFg), BUTTON_MASK|CHECK_BUTTON_MASK
	|RADIO_BUTTON_MASK|TK_CONFIG_COLOR_ONLY|TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-disabledforeground", "disabledForeground",
	"DisabledForeground", DEF_BUTTON_DISABLED_FG_MONO,
	Tk_Offset(TkButton, disabledFg), BUTTON_MASK|CHECK_BUTTON_MASK
	|RADIO_BUTTON_MASK|TK_CONFIG_MONO_ONLY|TK_CONFIG_NULL_OK},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, ALL_MASK},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_BUTTON_FONT, Tk_Offset(TkButton, tkfont),
	ALL_MASK},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_BUTTON_FG, Tk_Offset(TkButton, normalFg), LABEL_MASK|BUTTON_MASK},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_CHKRAD_FG, Tk_Offset(TkButton, normalFg), CHECK_BUTTON_MASK
	|RADIO_BUTTON_MASK},
    {TK_CONFIG_STRING, "-height", "height", "Height",
	DEF_BUTTON_HEIGHT, Tk_Offset(TkButton, heightString), ALL_MASK},
    {TK_CONFIG_BORDER, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_BUTTON_HIGHLIGHT_BG,
	Tk_Offset(TkButton, highlightBorder), ALL_MASK},
    {TK_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_BUTTON_HIGHLIGHT, Tk_Offset(TkButton, highlightColorPtr),
	ALL_MASK},
    {TK_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
	"HighlightThickness",
	DEF_LABEL_HIGHLIGHT_WIDTH, Tk_Offset(TkButton, highlightWidth),
	LABEL_MASK},
    {TK_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
	"HighlightThickness",
	DEF_BUTTON_HIGHLIGHT_WIDTH, Tk_Offset(TkButton, highlightWidth),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK},
    {TK_CONFIG_STRING, "-image", "image", "Image",
	DEF_BUTTON_IMAGE, Tk_Offset(TkButton, imageString),
	ALL_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-indicatoron", "indicatorOn", "IndicatorOn",
	DEF_BUTTON_INDICATOR, Tk_Offset(TkButton, indicatorOn),
	CHECK_BUTTON_MASK|RADIO_BUTTON_MASK},
    {TK_CONFIG_JUSTIFY, "-justify", "justify", "Justify",
	DEF_BUTTON_JUSTIFY, Tk_Offset(TkButton, justify), ALL_MASK},
    {TK_CONFIG_STRING, "-offvalue", "offValue", "Value",
	DEF_BUTTON_OFF_VALUE, Tk_Offset(TkButton, offValue),
	CHECK_BUTTON_MASK},
    {TK_CONFIG_STRING, "-onvalue", "onValue", "Value",
	DEF_BUTTON_ON_VALUE, Tk_Offset(TkButton, onValue),
	CHECK_BUTTON_MASK},
    {TK_CONFIG_PIXELS, "-padx", "padX", "Pad",
	DEF_BUTTON_PADX, Tk_Offset(TkButton, padX), BUTTON_MASK},
    {TK_CONFIG_PIXELS, "-padx", "padX", "Pad",
	DEF_LABCHKRAD_PADX, Tk_Offset(TkButton, padX),
	LABEL_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK},
    {TK_CONFIG_PIXELS, "-pady", "padY", "Pad",
	DEF_BUTTON_PADY, Tk_Offset(TkButton, padY), BUTTON_MASK},
    {TK_CONFIG_PIXELS, "-pady", "padY", "Pad",
	DEF_LABCHKRAD_PADY, Tk_Offset(TkButton, padY),
	LABEL_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_BUTTON_RELIEF, Tk_Offset(TkButton, relief), BUTTON_MASK},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_LABCHKRAD_RELIEF, Tk_Offset(TkButton, relief),
	LABEL_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK},
    {TK_CONFIG_BORDER, "-selectcolor", "selectColor", "Background",
	DEF_BUTTON_SELECT_COLOR, Tk_Offset(TkButton, selectBorder),
	CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|TK_CONFIG_COLOR_ONLY
	|TK_CONFIG_NULL_OK},
    {TK_CONFIG_BORDER, "-selectcolor", "selectColor", "Background",
	DEF_BUTTON_SELECT_MONO, Tk_Offset(TkButton, selectBorder),
	CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|TK_CONFIG_MONO_ONLY
	|TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-selectimage", "selectImage", "SelectImage",
	DEF_BUTTON_SELECT_IMAGE, Tk_Offset(TkButton, selectImageString),
	CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_UID, "-state", "state", "State",
	DEF_BUTTON_STATE, Tk_Offset(TkButton, state),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_LABEL_TAKE_FOCUS, Tk_Offset(TkButton, takeFocus),
	LABEL_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_BUTTON_TAKE_FOCUS, Tk_Offset(TkButton, takeFocus),
	BUTTON_MASK|CHECK_BUTTON_MASK|RADIO_BUTTON_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-text", "text", "Text",
	DEF_BUTTON_TEXT, Tk_Offset(TkButton, text), ALL_MASK},
    {TK_CONFIG_STRING, "-textvariable", "textVariable", "Variable",
	DEF_BUTTON_TEXT_VARIABLE, Tk_Offset(TkButton, textVarName),
	ALL_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_INT, "-underline", "underline", "Underline",
	DEF_BUTTON_UNDERLINE, Tk_Offset(TkButton, underline), ALL_MASK},
    {TK_CONFIG_STRING, "-value", "value", "Value",
	DEF_BUTTON_VALUE, Tk_Offset(TkButton, onValue),
	RADIO_BUTTON_MASK},
    {TK_CONFIG_STRING, "-variable", "variable", "Variable",
	DEF_RADIOBUTTON_VARIABLE, Tk_Offset(TkButton, selVarName),
	RADIO_BUTTON_MASK},
    {TK_CONFIG_STRING, "-variable", "variable", "Variable",
	DEF_CHECKBUTTON_VARIABLE, Tk_Offset(TkButton, selVarName),
	CHECK_BUTTON_MASK|TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-width", "width", "Width",
	DEF_BUTTON_WIDTH, Tk_Offset(TkButton, widthString), ALL_MASK},
    {TK_CONFIG_PIXELS, "-wraplength", "wrapLength", "WrapLength",
	DEF_BUTTON_WRAP_LENGTH, Tk_Offset(TkButton, wrapLength), ALL_MASK},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * String to print out in error messages, identifying options for
 * widget commands for different types of labels or buttons:
 */

static char *optionStrings[] = {
    "cget or configure",
    "cget, configure, flash, or invoke",
    "cget, configure, deselect, flash, invoke, select, or toggle",
    "cget, configure, deselect, flash, invoke, or select"
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		ButtonCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static int		ButtonCreate _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv,
			    int type));
static void		ButtonEventProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static void		ButtonImageProc _ANSI_ARGS_((ClientData clientData,
			    int x, int y, int width, int height,
			    int imgWidth, int imgHeight));
static void		ButtonSelectImageProc _ANSI_ARGS_((
			    ClientData clientData, int x, int y, int width,
			    int height, int imgWidth, int imgHeight));
static char *		ButtonTextVarProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
static char *		ButtonVarProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
static int		ButtonWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static int		ConfigureButton _ANSI_ARGS_((Tcl_Interp *interp,
			    TkButton *butPtr, int argc, char **argv,
			    int flags));
static void		DestroyButton _ANSI_ARGS_((TkButton *butPtr));


/*
 *--------------------------------------------------------------
 *
 * Tk_ButtonCmd, Tk_CheckbuttonCmd, Tk_LabelCmd, Tk_RadiobuttonCmd --
 *
 *	These procedures are invoked to process the "button", "label",
 *	"radiobutton", and "checkbutton" Tcl commands.  See the
 *	user documentation for details on what they do.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.  These procedures are just wrappers;
 *	they call ButtonCreate to do all of the real work.
 *
 *--------------------------------------------------------------
 */

int
Tk_ButtonCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    return ButtonCreate(clientData, interp, argc, argv, TYPE_BUTTON);
}

int
Tk_CheckbuttonCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    return ButtonCreate(clientData, interp, argc, argv, TYPE_CHECK_BUTTON);
}

int
Tk_LabelCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    return ButtonCreate(clientData, interp, argc, argv, TYPE_LABEL);
}

int
Tk_RadiobuttonCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    return ButtonCreate(clientData, interp, argc, argv, TYPE_RADIO_BUTTON);
}

/*
 *--------------------------------------------------------------
 *
 * ButtonCreate --
 *
 *	This procedure does all the real work of implementing the
 *	"button", "label", "radiobutton", and "checkbutton" Tcl
 *	commands.  See the user documentation for details on what it does.
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
ButtonCreate(clientData, interp, argc, argv, type)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
    int type;			/* Type of button to create: TYPE_LABEL,
				 * TYPE_BUTTON, TYPE_CHECK_BUTTON, or
				 * TYPE_RADIO_BUTTON. */
{
    register TkButton *butPtr;
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

    Tk_SetClass(new, classNames[type]);
    butPtr = TkpCreateButton(new);

    TkSetClassProcs(new, &tkpButtonProcs, (ClientData) butPtr);

    /*
     * Initialize the data structure for the button.
     */

    butPtr->tkwin = new;
    butPtr->display = Tk_Display(new);
    butPtr->widgetCmd = Tcl_CreateCommand(interp, Tk_PathName(butPtr->tkwin),
	    ButtonWidgetCmd, (ClientData) butPtr, ButtonCmdDeletedProc);
    butPtr->interp = interp;
    butPtr->type = type;
    butPtr->text = NULL;
    butPtr->underline = -1;
    butPtr->textVarName = NULL;
    butPtr->bitmap = None;
    butPtr->imageString = NULL;
    butPtr->image = NULL;
    butPtr->selectImageString = NULL;
    butPtr->selectImage = NULL;
    butPtr->state = tkNormalUid;
    butPtr->normalBorder = NULL;
    butPtr->activeBorder = NULL;
    butPtr->borderWidth = 0;
    butPtr->relief = TK_RELIEF_FLAT;
    butPtr->highlightWidth = 0;
    butPtr->highlightBorder = NULL;
    butPtr->highlightColorPtr = NULL;
    butPtr->inset = 0;
    butPtr->tkfont = NULL;
    butPtr->normalFg = NULL;
    butPtr->activeFg = NULL;
    butPtr->disabledFg = NULL;
    butPtr->normalTextGC = None;
    butPtr->activeTextGC = None;
    butPtr->gray = None;
    butPtr->disabledGC = None;
    butPtr->copyGC = None;
    butPtr->widthString = NULL;
    butPtr->heightString = NULL;
    butPtr->width = 0;
    butPtr->height = 0;
    butPtr->wrapLength = 0;
    butPtr->padX = 0;
    butPtr->padY = 0;
    butPtr->anchor = TK_ANCHOR_CENTER;
    butPtr->justify = TK_JUSTIFY_CENTER;
    butPtr->textLayout = NULL;
    butPtr->indicatorOn = 0;
    butPtr->selectBorder = NULL;
    butPtr->indicatorSpace = 0;
    butPtr->indicatorDiameter = 0;
    butPtr->defaultState = tkDisabledUid;
    butPtr->selVarName = NULL;
    butPtr->onValue = NULL;
    butPtr->offValue = NULL;
    butPtr->cursor = None;
    butPtr->command = NULL;
    butPtr->takeFocus = NULL;
    butPtr->flags = 0;

    Tk_CreateEventHandler(butPtr->tkwin,
	    ExposureMask|StructureNotifyMask|FocusChangeMask,
	    ButtonEventProc, (ClientData) butPtr);

    if (ConfigureButton(interp, butPtr, argc - 2, argv + 2,
	    configFlags[type]) != TCL_OK) {
	Tk_DestroyWindow(butPtr->tkwin);
	return TCL_ERROR;
    }

    interp->result = Tk_PathName(butPtr->tkwin);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ButtonWidgetCmd --
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
ButtonWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about button widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    register TkButton *butPtr = (TkButton *) clientData;
    int result = TCL_OK;
    size_t length;
    int c;

    if (argc < 2) {
	sprintf(interp->result,
		"wrong # args: should be \"%.50s option ?arg arg ...?\"",
		argv[0]);
	return TCL_ERROR;
    }
    Tcl_Preserve((ClientData) butPtr);
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
	result = Tk_ConfigureValue(interp, butPtr->tkwin, tkpButtonConfigSpecs,
		(char *) butPtr, argv[2], configFlags[butPtr->type]);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc == 2) {
	    result = Tk_ConfigureInfo(interp, butPtr->tkwin,
		    tkpButtonConfigSpecs, (char *) butPtr, (char *) NULL,
		    configFlags[butPtr->type]);
	} else if (argc == 3) {
	    result = Tk_ConfigureInfo(interp, butPtr->tkwin,
		    tkpButtonConfigSpecs, (char *) butPtr, argv[2],
		    configFlags[butPtr->type]);
	} else {
	    result = ConfigureButton(interp, butPtr, argc-2, argv+2,
		    configFlags[butPtr->type] | TK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 'd') && (strncmp(argv[1], "deselect", length) == 0)
	    && (butPtr->type >= TYPE_CHECK_BUTTON)) {
	if (argc > 2) {
	    sprintf(interp->result,
		    "wrong # args: should be \"%.50s deselect\"",
		    argv[0]);
	    goto error;
	}
	if (butPtr->type == TYPE_CHECK_BUTTON) {
	    if (Tcl_SetVar(interp, butPtr->selVarName, butPtr->offValue,
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		result = TCL_ERROR;
	    }
	} else if (butPtr->flags & SELECTED) {
	    if (Tcl_SetVar(interp, butPtr->selVarName, "",
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		result = TCL_ERROR;
	    };
	}
    } else if ((c == 'f') && (strncmp(argv[1], "flash", length) == 0)
	    && (butPtr->type != TYPE_LABEL)) {
	int i;

	if (argc > 2) {
	    sprintf(interp->result,
		    "wrong # args: should be \"%.50s flash\"",
		    argv[0]);
	    goto error;
	}
	if (butPtr->state != tkDisabledUid) {
	    for (i = 0; i < 4; i++) {
		butPtr->state = (butPtr->state == tkNormalUid)
			? tkActiveUid : tkNormalUid;
		Tk_SetBackgroundFromBorder(butPtr->tkwin,
			(butPtr->state == tkActiveUid) ? butPtr->activeBorder
			: butPtr->normalBorder);
		TkpDisplayButton((ClientData) butPtr);

		/*
		 * Special note: must cancel any existing idle handler
		 * for TkpDisplayButton;  it's no longer needed, and TkpDisplayButton
		 * cleared the REDRAW_PENDING flag.
		 */

		Tcl_CancelIdleCall(TkpDisplayButton, (ClientData) butPtr);
		XFlush(butPtr->display);
		Tcl_Sleep(50);
	    }
	}
    } else if ((c == 'i') && (strncmp(argv[1], "invoke", length) == 0)
	    && (butPtr->type > TYPE_LABEL)) {
	if (argc > 2) {
	    sprintf(interp->result,
		    "wrong # args: should be \"%.50s invoke\"",
		    argv[0]);
	    goto error;
	}
	if (butPtr->state != tkDisabledUid) {
	    result = TkInvokeButton(butPtr);
	}
    } else if ((c == 's') && (strncmp(argv[1], "select", length) == 0)
	    && (butPtr->type >= TYPE_CHECK_BUTTON)) {
	if (argc > 2) {
	    sprintf(interp->result,
		    "wrong # args: should be \"%.50s select\"",
		    argv[0]);
	    goto error;
	}
	if (Tcl_SetVar(interp, butPtr->selVarName, butPtr->onValue,
		TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
	    result = TCL_ERROR;
	}
    } else if ((c == 't') && (strncmp(argv[1], "toggle", length) == 0)
	    && (length >= 2) && (butPtr->type == TYPE_CHECK_BUTTON)) {
	if (argc > 2) {
	    sprintf(interp->result,
		    "wrong # args: should be \"%.50s toggle\"",
		    argv[0]);
	    goto error;
	}
	if (butPtr->flags & SELECTED) {
	    if (Tcl_SetVar(interp, butPtr->selVarName, butPtr->offValue,
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		result = TCL_ERROR;
	    }
	} else {
	    if (Tcl_SetVar(interp, butPtr->selVarName, butPtr->onValue,
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		result = TCL_ERROR;
	    }
	}
    } else {
	sprintf(interp->result,
		"bad option \"%.50s\": must be %s", argv[1],
		optionStrings[butPtr->type]);
	goto error;
    }
    Tcl_Release((ClientData) butPtr);
    return result;

    error:
    Tcl_Release((ClientData) butPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyButton --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a button at a safe time
 *	(when no-one is using it anymore).
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
DestroyButton(butPtr)
    TkButton *butPtr;		/* Info about button widget. */
{
    /*
     * Free up all the stuff that requires special handling, then
     * let Tk_FreeOptions handle all the standard option-related
     * stuff.
     */

    if (butPtr->textVarName != NULL) {
	Tcl_UntraceVar(butPtr->interp, butPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonTextVarProc, (ClientData) butPtr);
    }
    if (butPtr->image != NULL) {
	Tk_FreeImage(butPtr->image);
    }
    if (butPtr->selectImage != NULL) {
	Tk_FreeImage(butPtr->selectImage);
    }
    if (butPtr->normalTextGC != None) {
	Tk_FreeGC(butPtr->display, butPtr->normalTextGC);
    }
    if (butPtr->activeTextGC != None) {
	Tk_FreeGC(butPtr->display, butPtr->activeTextGC);
    }
    if (butPtr->gray != None) {
	Tk_FreeBitmap(butPtr->display, butPtr->gray);
    }
    if (butPtr->disabledGC != None) {
	Tk_FreeGC(butPtr->display, butPtr->disabledGC);
    }
    if (butPtr->copyGC != None) {
	Tk_FreeGC(butPtr->display, butPtr->copyGC);
    }
    if (butPtr->selVarName != NULL) {
	Tcl_UntraceVar(butPtr->interp, butPtr->selVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonVarProc, (ClientData) butPtr);
    }
    Tk_FreeTextLayout(butPtr->textLayout);
    Tk_FreeOptions(tkpButtonConfigSpecs, (char *) butPtr, butPtr->display,
	    configFlags[butPtr->type]);
    Tcl_EventuallyFree((ClientData)butPtr, TCL_DYNAMIC);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureButton --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a button widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as text string, colors, font,
 *	etc. get set for butPtr;  old resources get freed, if there
 *	were any.  The button is redisplayed.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureButton(interp, butPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    register TkButton *butPtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    Tk_Image image;

    /*
     * Eliminate any existing trace on variables monitored by the button.
     */

    if (butPtr->textVarName != NULL) {
	Tcl_UntraceVar(interp, butPtr->textVarName, 
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonTextVarProc, (ClientData) butPtr);
    }
    if (butPtr->selVarName != NULL) {
	Tcl_UntraceVar(interp, butPtr->selVarName, 
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonVarProc, (ClientData) butPtr);
    }

    

    if (Tk_ConfigureWidget(interp, butPtr->tkwin, tkpButtonConfigSpecs,
	    argc, argv, (char *) butPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * A few options need special processing, such as setting the
     * background from a 3-D border, or filling in complicated
     * defaults that couldn't be specified to Tk_ConfigureWidget.
     */

    if ((butPtr->state == tkActiveUid) && !Tk_StrictMotif(butPtr->tkwin)) {
	Tk_SetBackgroundFromBorder(butPtr->tkwin, butPtr->activeBorder);
    } else {
	Tk_SetBackgroundFromBorder(butPtr->tkwin, butPtr->normalBorder);
	if ((butPtr->state != tkNormalUid) && (butPtr->state != tkActiveUid)
		&& (butPtr->state != tkDisabledUid)) {
	    Tcl_AppendResult(interp, "bad state value \"", butPtr->state,
		    "\": must be normal, active, or disabled", (char *) NULL);
	    butPtr->state = tkNormalUid;
	    return TCL_ERROR;
	}
    }

    if ((butPtr->defaultState != tkActiveUid)
	    && (butPtr->defaultState != tkDisabledUid)
	    && (butPtr->defaultState != tkNormalUid)) {
	Tcl_AppendResult(interp, "bad -default value \"", butPtr->defaultState,
		"\": must be normal, active, or disabled", (char *) NULL);
	butPtr->defaultState = tkDisabledUid;
	return TCL_ERROR;
    }

    if (butPtr->highlightWidth < 0) {
	butPtr->highlightWidth = 0;
    }

    if (butPtr->padX < 0) {
	butPtr->padX = 0;
    }
    if (butPtr->padY < 0) {
	butPtr->padY = 0;
    }

    if (butPtr->type >= TYPE_CHECK_BUTTON) {
	char *value;

	if (butPtr->selVarName == NULL) {
	    butPtr->selVarName = (char *) ckalloc((unsigned)
		    (strlen(Tk_Name(butPtr->tkwin)) + 1));
	    strcpy(butPtr->selVarName, Tk_Name(butPtr->tkwin));
	}

	/*
	 * Select the button if the associated variable has the
	 * appropriate value, initialize the variable if it doesn't
	 * exist, then set a trace on the variable to monitor future
	 * changes to its value.
	 */

	value = Tcl_GetVar(interp, butPtr->selVarName, TCL_GLOBAL_ONLY);
	butPtr->flags &= ~SELECTED;
	if (value != NULL) {
	    if (strcmp(value, butPtr->onValue) == 0) {
		butPtr->flags |= SELECTED;
	    }
	} else {
	    if (Tcl_SetVar(interp, butPtr->selVarName,
		    (butPtr->type == TYPE_CHECK_BUTTON) ? butPtr->offValue : "",
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		return TCL_ERROR;
	    }
	}
	Tcl_TraceVar(interp, butPtr->selVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonVarProc, (ClientData) butPtr);
    }

    /*
     * Get the images for the widget, if there are any.  Allocate the
     * new images before freeing the old ones, so that the reference
     * counts don't go to zero and cause image data to be discarded.
     */

    if (butPtr->imageString != NULL) {
	image = Tk_GetImage(butPtr->interp, butPtr->tkwin,
		butPtr->imageString, ButtonImageProc, (ClientData) butPtr);
	if (image == NULL) {
	    return TCL_ERROR;
	}
    } else {
	image = NULL;
    }
    if (butPtr->image != NULL) {
	Tk_FreeImage(butPtr->image);
    }
    butPtr->image = image;
    if (butPtr->selectImageString != NULL) {
	image = Tk_GetImage(butPtr->interp, butPtr->tkwin,
		butPtr->selectImageString, ButtonSelectImageProc,
		(ClientData) butPtr);
	if (image == NULL) {
	    return TCL_ERROR;
	}
    } else {
	image = NULL;
    }
    if (butPtr->selectImage != NULL) {
	Tk_FreeImage(butPtr->selectImage);
    }
    butPtr->selectImage = image;

    if ((butPtr->image == NULL) && (butPtr->bitmap == None)
	    && (butPtr->textVarName != NULL)) {
	/*
	 * The button must display the value of a variable: set up a trace
	 * on the variable's value, create the variable if it doesn't
	 * exist, and fetch its current value.
	 */

	char *value;

	value = Tcl_GetVar(interp, butPtr->textVarName, TCL_GLOBAL_ONLY);
	if (value == NULL) {
	    if (Tcl_SetVar(interp, butPtr->textVarName, butPtr->text,
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		return TCL_ERROR;
	    }
	} else {
	    if (butPtr->text != NULL) {
		ckfree(butPtr->text);
	    }
	    butPtr->text = (char *) ckalloc((unsigned) (strlen(value) + 1));
	    strcpy(butPtr->text, value);
	}
	Tcl_TraceVar(interp, butPtr->textVarName,
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		ButtonTextVarProc, (ClientData) butPtr);
    }

    if ((butPtr->bitmap != None) || (butPtr->image != NULL)) {
	if (Tk_GetPixels(interp, butPtr->tkwin, butPtr->widthString,
		&butPtr->width) != TCL_OK) {
	    widthError:
	    Tcl_AddErrorInfo(interp, "\n    (processing -width option)");
	    return TCL_ERROR;
	}
	if (Tk_GetPixels(interp, butPtr->tkwin, butPtr->heightString,
		&butPtr->height) != TCL_OK) {
	    heightError:
	    Tcl_AddErrorInfo(interp, "\n    (processing -height option)");
	    return TCL_ERROR;
	}
    } else {
	if (Tcl_GetInt(interp, butPtr->widthString, &butPtr->width)
		!= TCL_OK) {
	    goto widthError;
	}
	if (Tcl_GetInt(interp, butPtr->heightString, &butPtr->height)
		!= TCL_OK) {
	    goto heightError;
	}
    }
    
    TkButtonWorldChanged((ClientData) butPtr);
    return TCL_OK;
}

/*
 *---------------------------------------------------------------------------
 *
 * TkButtonWorldChanged --
 *
 *      This procedure is called when the world has changed in some
 *      way and the widget needs to recompute all its graphics contexts
 *	and determine its new geometry.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Button will be relayed out and redisplayed.
 *
 *---------------------------------------------------------------------------
 */
 
void
TkButtonWorldChanged(instanceData)
    ClientData instanceData;	/* Information about widget. */
{
    XGCValues gcValues;
    GC newGC;
    unsigned long mask;
    TkButton *butPtr;

    butPtr = (TkButton *) instanceData;

    /*
     * Recompute GCs.
     */

    gcValues.font = Tk_FontId(butPtr->tkfont);
    gcValues.foreground = butPtr->normalFg->pixel;
    gcValues.background = Tk_3DBorderColor(butPtr->normalBorder)->pixel;
    
    /*
     * Note: GraphicsExpose events are disabled in normalTextGC because it's
     * used to copy stuff from an off-screen pixmap onto the screen (we know
     * that there's no problem with obscured areas).
     */

    gcValues.graphics_exposures = False;
    mask = GCForeground | GCBackground | GCFont | GCGraphicsExposures;
    newGC = Tk_GetGC(butPtr->tkwin, mask, &gcValues);
    if (butPtr->normalTextGC != None) {
	Tk_FreeGC(butPtr->display, butPtr->normalTextGC);
    }
    butPtr->normalTextGC = newGC;

    if (butPtr->activeFg != NULL) {
	gcValues.font = Tk_FontId(butPtr->tkfont);
	gcValues.foreground = butPtr->activeFg->pixel;
	gcValues.background = Tk_3DBorderColor(butPtr->activeBorder)->pixel;
	mask = GCForeground | GCBackground | GCFont;
	newGC = Tk_GetGC(butPtr->tkwin, mask, &gcValues);
	if (butPtr->activeTextGC != None) {
	    Tk_FreeGC(butPtr->display, butPtr->activeTextGC);
	}
	butPtr->activeTextGC = newGC;
    }

    if (butPtr->type != TYPE_LABEL) {
	gcValues.font = Tk_FontId(butPtr->tkfont);
	gcValues.background = Tk_3DBorderColor(butPtr->normalBorder)->pixel;
	if ((butPtr->disabledFg != NULL) && (butPtr->imageString == NULL)) {
	    gcValues.foreground = butPtr->disabledFg->pixel;
	    mask = GCForeground | GCBackground | GCFont;
	} else {
	    gcValues.foreground = gcValues.background;
	    mask = GCForeground;
	    if (butPtr->gray == None) {
		butPtr->gray = Tk_GetBitmap(NULL, butPtr->tkwin, 
			Tk_GetUid("gray50"));
	    }
	    if (butPtr->gray != None) {
		gcValues.fill_style = FillStippled;
		gcValues.stipple = butPtr->gray;
		mask |= GCFillStyle | GCStipple;
	    }
	}
	newGC = Tk_GetGC(butPtr->tkwin, mask, &gcValues);
	if (butPtr->disabledGC != None) {
	    Tk_FreeGC(butPtr->display, butPtr->disabledGC);
	}
	butPtr->disabledGC = newGC;
    }

    if (butPtr->copyGC == None) {
	butPtr->copyGC = Tk_GetGC(butPtr->tkwin, 0, &gcValues);
    }

    TkpComputeButtonGeometry(butPtr);

    /*
     * Lastly, arrange for the button to be redisplayed.
     */

    if (Tk_IsMapped(butPtr->tkwin) && !(butPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(TkpDisplayButton, (ClientData) butPtr);
	butPtr->flags |= REDRAW_PENDING;
    }
}

/*
 *--------------------------------------------------------------
 *
 * ButtonEventProc --
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
ButtonEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    TkButton *butPtr = (TkButton *) clientData;
    if ((eventPtr->type == Expose) && (eventPtr->xexpose.count == 0)) {
	goto redraw;
    } else if (eventPtr->type == ConfigureNotify) {
	/*
	 * Must redraw after size changes, since layout could have changed
	 * and borders will need to be redrawn.
	 */

	goto redraw;
    } else if (eventPtr->type == DestroyNotify) {
	TkpDestroyButton(butPtr);
	if (butPtr->tkwin != NULL) {
	    butPtr->tkwin = NULL;
            Tcl_DeleteCommandFromToken(butPtr->interp, butPtr->widgetCmd);
	}
	if (butPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(TkpDisplayButton, (ClientData) butPtr);
	}
	DestroyButton(butPtr);
    } else if (eventPtr->type == FocusIn) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    butPtr->flags |= GOT_FOCUS;
	    if (butPtr->highlightWidth > 0) {
		goto redraw;
	    }
	}
    } else if (eventPtr->type == FocusOut) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    butPtr->flags &= ~GOT_FOCUS;
	    if (butPtr->highlightWidth > 0) {
		goto redraw;
	    }
	}
    }
    return;

    redraw:
    if ((butPtr->tkwin != NULL) && !(butPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(TkpDisplayButton, (ClientData) butPtr);
	butPtr->flags |= REDRAW_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonCmdDeletedProc --
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
ButtonCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    TkButton *butPtr = (TkButton *) clientData;
    Tk_Window tkwin = butPtr->tkwin;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tkwin != NULL) {
	butPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkInvokeButton --
 *
 *	This procedure is called to carry out the actions associated
 *	with a button, such as invoking a Tcl command or setting a
 *	variable.  This procedure is invoked, for example, when the
 *	button is invoked via the mouse.
 *
 * Results:
 *	A standard Tcl return value.  Information is also left in
 *	interp->result.
 *
 * Side effects:
 *	Depends on the button and its associated command.
 *
 *----------------------------------------------------------------------
 */

int
TkInvokeButton(butPtr)
    register TkButton *butPtr;		/* Information about button. */
{
    if (butPtr->type == TYPE_CHECK_BUTTON) {
	if (butPtr->flags & SELECTED) {
	    if (Tcl_SetVar(butPtr->interp, butPtr->selVarName, butPtr->offValue,
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		return TCL_ERROR;
	    }
	} else {
	    if (Tcl_SetVar(butPtr->interp, butPtr->selVarName, butPtr->onValue,
		    TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
		return TCL_ERROR;
	    }
	}
    } else if (butPtr->type == TYPE_RADIO_BUTTON) {
	if (Tcl_SetVar(butPtr->interp, butPtr->selVarName, butPtr->onValue,
		TCL_GLOBAL_ONLY|TCL_LEAVE_ERR_MSG) == NULL) {
	    return TCL_ERROR;
	}
    }
    if ((butPtr->type != TYPE_LABEL) && (butPtr->command != NULL)) {
	return TkCopyAndGlobalEval(butPtr->interp, butPtr->command);
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ButtonVarProc --
 *
 *	This procedure is invoked when someone changes the
 *	state variable associated with a radio button.  Depending
 *	on the new value of the button's variable, the button
 *	may be selected or deselected.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The button may become selected or deselected.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static char *
ButtonVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about button. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    register TkButton *butPtr = (TkButton *) clientData;
    char *value;

    /*
     * If the variable is being unset, then just re-establish the
     * trace unless the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	butPtr->flags &= ~SELECTED;
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_TraceVar(interp, butPtr->selVarName,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    ButtonVarProc, clientData);
	}
	goto redisplay;
    }

    /*
     * Use the value of the variable to update the selected status of
     * the button.
     */

    value = Tcl_GetVar(interp, butPtr->selVarName, TCL_GLOBAL_ONLY);
    if (value == NULL) {
	value = "";
    }
    if (strcmp(value, butPtr->onValue) == 0) {
	if (butPtr->flags & SELECTED) {
	    return (char *) NULL;
	}
	butPtr->flags |= SELECTED;
    } else if (butPtr->flags & SELECTED) {
	butPtr->flags &= ~SELECTED;
    } else {
	return (char *) NULL;
    }

    redisplay:
    if ((butPtr->tkwin != NULL) && Tk_IsMapped(butPtr->tkwin)
	    && !(butPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(TkpDisplayButton, (ClientData) butPtr);
	butPtr->flags |= REDRAW_PENDING;
    }
    return (char *) NULL;
}

/*
 *--------------------------------------------------------------
 *
 * ButtonTextVarProc --
 *
 *	This procedure is invoked when someone changes the variable
 *	whose contents are to be displayed in a button.
 *
 * Results:
 *	NULL is always returned.
 *
 * Side effects:
 *	The text displayed in the button will change to match the
 *	variable.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static char *
ButtonTextVarProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Information about button. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Not used. */
    char *name2;		/* Not used. */
    int flags;			/* Information about what happened. */
{
    register TkButton *butPtr = (TkButton *) clientData;
    char *value;

    /*
     * If the variable is unset, then immediately recreate it unless
     * the whole interpreter is going away.
     */

    if (flags & TCL_TRACE_UNSETS) {
	if ((flags & TCL_TRACE_DESTROYED) && !(flags & TCL_INTERP_DESTROYED)) {
	    Tcl_SetVar(interp, butPtr->textVarName, butPtr->text,
		    TCL_GLOBAL_ONLY);
	    Tcl_TraceVar(interp, butPtr->textVarName,
		    TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		    ButtonTextVarProc, clientData);
	}
	return (char *) NULL;
    }

    value = Tcl_GetVar(interp, butPtr->textVarName, TCL_GLOBAL_ONLY);
    if (value == NULL) {
	value = "";
    }
    if (butPtr->text != NULL) {
	ckfree(butPtr->text);
    }
    butPtr->text = (char *) ckalloc((unsigned) (strlen(value) + 1));
    strcpy(butPtr->text, value);
    TkpComputeButtonGeometry(butPtr);

    if ((butPtr->tkwin != NULL) && Tk_IsMapped(butPtr->tkwin)
	    && !(butPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(TkpDisplayButton, (ClientData) butPtr);
	butPtr->flags |= REDRAW_PENDING;
    }
    return (char *) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonImageProc --
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
ButtonImageProc(clientData, x, y, width, height, imgWidth, imgHeight)
    ClientData clientData;		/* Pointer to widget record. */
    int x, y;				/* Upper left pixel (within image)
					 * that must be redisplayed. */
    int width, height;			/* Dimensions of area to redisplay
					 * (may be <= 0). */
    int imgWidth, imgHeight;		/* New dimensions of image. */
{
    register TkButton *butPtr = (TkButton *) clientData;

    if (butPtr->tkwin != NULL) {
	TkpComputeButtonGeometry(butPtr);
	if (Tk_IsMapped(butPtr->tkwin) && !(butPtr->flags & REDRAW_PENDING)) {
	    Tcl_DoWhenIdle(TkpDisplayButton, (ClientData) butPtr);
	    butPtr->flags |= REDRAW_PENDING;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonSelectImageProc --
 *
 *	This procedure is invoked by the image code whenever the manager
 *	for an image does something that affects the size of contents
 *	of the image displayed in a button when it is selected.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May arrange for the button to get redisplayed.
 *
 *----------------------------------------------------------------------
 */

static void
ButtonSelectImageProc(clientData, x, y, width, height, imgWidth, imgHeight)
    ClientData clientData;		/* Pointer to widget record. */
    int x, y;				/* Upper left pixel (within image)
					 * that must be redisplayed. */
    int width, height;			/* Dimensions of area to redisplay
					 * (may be <= 0). */
    int imgWidth, imgHeight;		/* New dimensions of image. */
{
    register TkButton *butPtr = (TkButton *) clientData;

    /*
     * Don't recompute geometry:  it's controlled by the primary image.
     */

    if ((butPtr->flags & SELECTED) && (butPtr->tkwin != NULL)
	    && Tk_IsMapped(butPtr->tkwin)
	    && !(butPtr->flags & REDRAW_PENDING)) {
	Tcl_DoWhenIdle(TkpDisplayButton, (ClientData) butPtr);
	butPtr->flags |= REDRAW_PENDING;
    }
}
