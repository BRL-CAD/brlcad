/*
 * tkButton.h --
 *
 *	Declarations of types and functions used to implement
 *	button-like widgets.
 *
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkButton.h 1.5 97/06/06 11:19:24
 */

#ifndef _TKBUTTON
#define _TKBUTTON

#ifndef _TKINT
#include "tkInt.h"
#endif

/*
 * A data structure of the following type is kept for each
 * widget managed by this file:
 */

typedef struct {
    Tk_Window tkwin;		/* Window that embodies the button.  NULL
				 * means that the window has been destroyed. */
    Display *display;		/* Display containing widget.  Needed to
				 * free up resources after tkwin is gone. */
    Tcl_Interp *interp;		/* Interpreter associated with button. */
    Tcl_Command widgetCmd;	/* Token for button's widget command. */
    int type;			/* Type of widget:  restricts operations
				 * that may be performed on widget.  See
				 * below for possible values. */

    /*
     * Information about what's in the button.
     */

    char *text;			/* Text to display in button (malloc'ed)
				 * or NULL. */
    int underline;		/* Index of character to underline.  < 0 means
				 * don't underline anything. */
    char *textVarName;		/* Name of variable (malloc'ed) or NULL.
				 * If non-NULL, button displays the contents
				 * of this variable. */
    Pixmap bitmap;		/* Bitmap to display or None.  If not None
				 * then text and textVar are ignored. */
    char *imageString;		/* Name of image to display (malloc'ed), or
				 * NULL.  If non-NULL, bitmap, text, and
				 * textVarName are ignored. */
    Tk_Image image;		/* Image to display in window, or NULL if
				 * none. */
    char *selectImageString;	/* Name of image to display when selected
				 * (malloc'ed), or NULL. */
    Tk_Image selectImage;	/* Image to display in window when selected,
				 * or NULL if none.  Ignored if image is
				 * NULL. */

    /*
     * Information used when displaying widget:
     */

    Tk_Uid state;		/* State of button for display purposes:
				 * normal, active, or disabled. */
    Tk_3DBorder normalBorder;	/* Structure used to draw 3-D
				 * border and background when window
				 * isn't active.  NULL means no such
				 * border exists. */
    Tk_3DBorder activeBorder;	/* Structure used to draw 3-D
				 * border and background when window
				 * is active.  NULL means no such
				 * border exists. */
    int borderWidth;		/* Width of border. */
    int relief;			/* 3-d effect: TK_RELIEF_RAISED, etc. */
    int highlightWidth;		/* Width in pixels of highlight to draw
				 * around widget when it has the focus.
				 * <= 0 means don't draw a highlight. */
    Tk_3DBorder highlightBorder;
				/* Structure used to draw 3-D default ring
				 * and focus highlight area when highlight
				 * is off. */
    XColor *highlightColorPtr;	/* Color for drawing traversal highlight. */

    int inset;			/* Total width of all borders, including
				 * traversal highlight and 3-D border.
				 * Indicates how much interior stuff must
				 * be offset from outside edges to leave
				 * room for borders. */
    Tk_Font tkfont;		/* Information about text font, or NULL. */
    XColor *normalFg;		/* Foreground color in normal mode. */
    XColor *activeFg;		/* Foreground color in active mode.  NULL
				 * means use normalFg instead. */
    XColor *disabledFg;		/* Foreground color when disabled.  NULL
				 * means use normalFg with a 50% stipple
				 * instead. */
    GC normalTextGC;		/* GC for drawing text in normal mode.  Also
				 * used to copy from off-screen pixmap onto
				 * screen. */
    GC activeTextGC;		/* GC for drawing text in active mode (NULL
				 * means use normalTextGC). */
    Pixmap gray;		/* Pixmap for displaying disabled text if
				 * disabledFg is NULL. */
    GC disabledGC;		/* Used to produce disabled effect.  If
				 * disabledFg isn't NULL, this GC is used to
				 * draw button text or icon.  Otherwise
				 * text or icon is drawn with normalGC and
				 * this GC is used to stipple background
				 * across it.  For labels this is None. */
    GC copyGC;			/* Used for copying information from an
				 * off-screen pixmap to the screen. */
    char *widthString;		/* Value of -width option.  Malloc'ed. */
    char *heightString;		/* Value of -height option.  Malloc'ed. */
    int width, height;		/* If > 0, these specify dimensions to request
				 * for window, in characters for text and in
				 * pixels for bitmaps.  In this case the actual
				 * size of the text string or bitmap is
				 * ignored in computing desired window size. */
    int wrapLength;		/* Line length (in pixels) at which to wrap
				 * onto next line.  <= 0 means don't wrap
				 * except at newlines. */
    int padX, padY;		/* Extra space around text (pixels to leave
				 * on each side).  Ignored for bitmaps and
				 * images. */
    Tk_Anchor anchor;		/* Where text/bitmap should be displayed
				 * inside button region. */
    Tk_Justify justify;		/* Justification to use for multi-line text. */
    int indicatorOn;		/* True means draw indicator, false means
				 * don't draw it. */
    Tk_3DBorder selectBorder;	/* For drawing indicator background, or perhaps
				 * widget background, when selected. */
    int textWidth;		/* Width needed to display text as requested,
				 * in pixels. */
    int textHeight;		/* Height needed to display text as requested,
				 * in pixels. */
    Tk_TextLayout textLayout;	/* Saved text layout information. */
    int indicatorSpace;		/* Horizontal space (in pixels) allocated for
				 * display of indicator. */
    int indicatorDiameter;	/* Diameter of indicator, in pixels. */
    Tk_Uid defaultState;	/* State of default ring: normal, active, or
				 * disabled. */
        
    /*
     * For check and radio buttons, the fields below are used
     * to manage the variable indicating the button's state.
     */

    char *selVarName;		/* Name of variable used to control selected
				 * state of button.  Malloc'ed (if
				 * not NULL). */
    char *onValue;		/* Value to store in variable when
				 * this button is selected.  Malloc'ed (if
				 * not NULL). */
    char *offValue;		/* Value to store in variable when this
				 * button isn't selected.  Malloc'ed
				 * (if not NULL).  Valid only for check
				 * buttons. */

    /*
     * Miscellaneous information:
     */

    Tk_Cursor cursor;		/* Current cursor for window, or None. */
    char *takeFocus;		/* Value of -takefocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    char *command;		/* Command to execute when button is
				 * invoked; valid for buttons only.
				 * If not NULL, it's malloc-ed. */
    int flags;			/* Various flags;  see below for
				 * definitions. */
} TkButton;

/*
 * Possible "type" values for buttons.  These are the kinds of
 * widgets supported by this file.  The ordering of the type
 * numbers is significant:  greater means more features and is
 * used in the code.
 */

#define TYPE_LABEL		0
#define TYPE_BUTTON		1
#define TYPE_CHECK_BUTTON	2
#define TYPE_RADIO_BUTTON	3

/*
 * Flag bits for buttons:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 * SELECTED:			Non-zero means this button is selected,
 *				so special highlight should be drawn.
 * GOT_FOCUS:			Non-zero means this button currently
 *				has the input focus.
 */

#define REDRAW_PENDING		1
#define SELECTED		2
#define GOT_FOCUS		4

/*
 * Mask values used to selectively enable entries in the
 * configuration specs:
 */

#define LABEL_MASK		TK_CONFIG_USER_BIT
#define BUTTON_MASK		TK_CONFIG_USER_BIT << 1
#define CHECK_BUTTON_MASK	TK_CONFIG_USER_BIT << 2
#define RADIO_BUTTON_MASK	TK_CONFIG_USER_BIT << 3
#define ALL_MASK		(LABEL_MASK | BUTTON_MASK \
	| CHECK_BUTTON_MASK | RADIO_BUTTON_MASK)

/*
 * Declaration of variables shared between the files in the button module.
 */

extern TkClassProcs tkpButtonProcs;
extern Tk_ConfigSpec tkpButtonConfigSpecs[];

/*
 * Declaration of procedures used in the implementation of the button
 * widget. 
 */

EXTERN void		TkButtonWorldChanged _ANSI_ARGS_((
			    ClientData instanceData));
EXTERN void		TkpComputeButtonGeometry _ANSI_ARGS_((
			    TkButton *butPtr));
EXTERN TkButton *	TkpCreateButton _ANSI_ARGS_((Tk_Window tkwin));
#ifndef TkpDestroyButton
EXTERN void 		TkpDestroyButton _ANSI_ARGS_((TkButton *butPtr));
#endif
#ifndef TkpDisplayButton
EXTERN void		TkpDisplayButton _ANSI_ARGS_((ClientData clientData));
#endif
EXTERN int		TkInvokeButton  _ANSI_ARGS_((TkButton *butPtr));

#endif /* _TKBUTTON */
