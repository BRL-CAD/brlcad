/* 
 * tkMacButton.c --
 *
 *	This file implements the Macintosh specific portion of the
 *	button widgets.
 *
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacButton.c 1.18 97/11/20 18:27:21
 */

#include "tkButton.h"
#include "tkMacInt.h"
#include <Controls.h>
#include <LowMem.h>

/*
 * Some defines used to control what type of control is drawn.
 */

#define DRAW_LABEL	0		/* Labels are treated genericly. */
#define DRAW_CONTROL	1		/* Draw using the Native control. */
#define DRAW_CUSTOM	2		/* Make our own button drawing. */

/*
 * The following structures are used to draw our controls.  Rather than
 * having many Mac controls we just use one control of each type and
 * reuse them for all Tk widgets.  When the windowRef variable is NULL
 * it means none of the data structures have been allocated.
 */

static WindowRef windowRef = NULL;
static CWindowRecord windowRecord;
static ControlRef buttonHandle;
static ControlRef checkHandle;
static ControlRef radioHandle;
static CCTabHandle buttonTabHandle;
static CCTabHandle checkTabHandle;
static CCTabHandle radioTabHandle;
static PixMapHandle oldPixPtr;

/*
 * Forward declarations for procedures defined later in this file:
 */

static int		UpdateControlColors _ANSI_ARGS_((TkButton *butPtr,
			    ControlRef controlHandle, CCTabHandle ccTabHandle,
			    RGBColor *saveColorPtr));
static void		DrawBufferedControl _ANSI_ARGS_((TkButton *butPtr,
			    GWorldPtr destPort));
static void		ChangeBackgroundWindowColor _ANSI_ARGS_((
			    WindowRef macintoshWindow, RGBColor rgbColor,
			    RGBColor *oldColor));
static void		ButtonExitProc _ANSI_ARGS_((ClientData clientData));

/*
 * The class procedure table for the button widgets.
 */

TkClassProcs tkpButtonProcs = { 
    NULL,			/* createProc. */
    TkButtonWorldChanged,	/* geometryProc. */
    NULL			/* modalProc. */
};

/*
 *----------------------------------------------------------------------
 *
 * TkpCreateButton --
 *
 *	Allocate a new TkButton structure.
 *
 * Results:
 *	Returns a newly allocated TkButton structure.
 *
 * Side effects:
 *	Registers an event handler for the widget.
 *
 *----------------------------------------------------------------------
 */

TkButton *
TkpCreateButton(
    Tk_Window tkwin)
{
    return (TkButton *) ckalloc(sizeof(TkButton));
}

/*
 *----------------------------------------------------------------------
 *
 * TkpDisplayButton --
 *
 *	This procedure is invoked to display a button widget.  It is
 *	normally invoked as an idle handler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Commands are output to X to display the button in its
 *	current mode.  The REDRAW_PENDING flag is cleared.
 *
 *----------------------------------------------------------------------
 */

void
TkpDisplayButton(
    ClientData clientData)	/* Information about widget. */
{
    TkButton *butPtr = (TkButton *) clientData;
    Pixmap pixmap;
    GC gc;
    Tk_3DBorder border;
    int x = 0;			/* Initialization only needed to stop
				 * compiler warning. */
    int y, relief;
    register Tk_Window tkwin = butPtr->tkwin;
    int width, height;
    int offset;			/* 0 means this is a normal widget.  1 means
				 * it is an image button, so we offset the
				 * image to make the button appear to move
				 * up and down as the relief changes. */
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    int drawType, borderWidth;
    
    GetGWorld(&saveWorld, &saveDevice);

    butPtr->flags &= ~REDRAW_PENDING;
    if ((butPtr->tkwin == NULL) || !Tk_IsMapped(tkwin)) {
	return;
    }

    border = butPtr->normalBorder;
    if ((butPtr->state == tkDisabledUid) && (butPtr->disabledFg != NULL)) {
	gc = butPtr->disabledGC;
    } else if ((butPtr->type == TYPE_BUTTON) && (butPtr->state == tkActiveUid)) {
	gc = butPtr->activeTextGC;
	border = butPtr->activeBorder;
    } else {
	gc = butPtr->normalTextGC;
    }
    if ((butPtr->flags & SELECTED) && (butPtr->state != tkActiveUid)
	    && (butPtr->selectBorder != NULL) && !butPtr->indicatorOn) {
	border = butPtr->selectBorder;
    }

    /*
     * Override the relief specified for the button if this is a
     * checkbutton or radiobutton and there's no indicator.
     */

    relief = butPtr->relief;
    if ((butPtr->type >= TYPE_CHECK_BUTTON) && !butPtr->indicatorOn) {
	relief = (butPtr->flags & SELECTED) ? TK_RELIEF_SUNKEN
		: TK_RELIEF_RAISED;
    }

    offset = ((butPtr->type == TYPE_BUTTON) && 
	((butPtr->image != NULL) || (butPtr->bitmap != None)));

    /*
     * In order to avoid screen flashes, this procedure redraws
     * the button in a pixmap, then copies the pixmap to the
     * screen in a single operation.  This means that there's no
     * point in time where the on-sreen image has been cleared.
     */

    pixmap = Tk_GetPixmap(butPtr->display, Tk_WindowId(tkwin),
	    Tk_Width(tkwin), Tk_Height(tkwin), Tk_Depth(tkwin));
    Tk_Fill3DRectangle(tkwin, pixmap, butPtr->normalBorder, 0, 0,
	    Tk_Width(tkwin), Tk_Height(tkwin), 0, TK_RELIEF_FLAT);

   
    if (butPtr->type == TYPE_LABEL) {
	drawType = DRAW_LABEL;
    } else if (butPtr->type == TYPE_BUTTON) {
	if ((butPtr->image == None) && (butPtr->bitmap == None)) {
	    drawType = DRAW_CONTROL;
	} else {
	    drawType = DRAW_CUSTOM;
	}
    } else {
	if (butPtr->indicatorOn) {
	    drawType = DRAW_CONTROL;
	} else {
	    drawType = DRAW_CUSTOM;
	}
    }

    /*
     * Draw the native portion of the buttons.  Start by creating the control
     * if it doesn't already exist.  Then configure the Macintosh control from
     * the Tk info.  Finally, we call Draw1Control to draw to the screen.
     */

    if (drawType == DRAW_CONTROL) {
	borderWidth = 0;
	
	/*
	 * This part uses Macintosh rather than Tk calls to draw
	 * to the screen.  Make sure the ports etc. are set correctly.
	 */
	
	destPort = TkMacGetDrawablePort(pixmap);
	SetGWorld(destPort, NULL);
	DrawBufferedControl(butPtr, destPort);
    }

    if ((drawType == DRAW_CUSTOM) || (drawType == DRAW_LABEL)) {
	borderWidth = butPtr->borderWidth;
    }

    /*
     * Display image or bitmap or text for button.
     */

    if (butPtr->image != None) {
	Tk_SizeOfImage(butPtr->image, &width, &height);

	imageOrBitmap:
	TkComputeAnchor(butPtr->anchor, tkwin, 0, 0,
		butPtr->indicatorSpace + width, height, &x, &y);
	x += butPtr->indicatorSpace;

	x += offset;
	y += offset;
	if (relief == TK_RELIEF_RAISED) {
	    x -= offset;
	    y -= offset;
	} else if (relief == TK_RELIEF_SUNKEN) {
	    x += offset;
	    y += offset;
	}
	if (butPtr->image != NULL) {
	    if ((butPtr->selectImage != NULL) && (butPtr->flags & SELECTED)) {
		Tk_RedrawImage(butPtr->selectImage, 0, 0, width, height,
			pixmap, x, y);
	    } else {
		Tk_RedrawImage(butPtr->image, 0, 0, width, height, pixmap,
			x, y);
	    }
	} else {
	    XSetClipOrigin(butPtr->display, gc, x, y);
	    XCopyPlane(butPtr->display, butPtr->bitmap, pixmap, gc, 0, 0,
		    (unsigned int) width, (unsigned int) height, x, y, 1);
	    XSetClipOrigin(butPtr->display, gc, 0, 0);
	}
	y += height/2;
    } else if (butPtr->bitmap != None) {
	Tk_SizeOfBitmap(butPtr->display, butPtr->bitmap, &width, &height);
	goto imageOrBitmap;
    } else {
	TkComputeAnchor(butPtr->anchor, tkwin, butPtr->padX, butPtr->padY,
		butPtr->indicatorSpace + butPtr->textWidth, butPtr->textHeight,
		&x, &y);

	x += butPtr->indicatorSpace;

	Tk_DrawTextLayout(butPtr->display, pixmap, gc, butPtr->textLayout,
		x, y, 0, -1);
	y += butPtr->textHeight/2;
    }

    /*
     * If the button is disabled with a stipple rather than a special
     * foreground color, generate the stippled effect.  If the widget
     * is selected and we use a different background color when selected,
     * must temporarily modify the GC.
     */

    if ((butPtr->state == tkDisabledUid)
	    && ((butPtr->disabledFg == NULL) || (butPtr->image != NULL))) {
	if ((butPtr->flags & SELECTED) && !butPtr->indicatorOn
		&& (butPtr->selectBorder != NULL)) {
	    XSetForeground(butPtr->display, butPtr->disabledGC,
		    Tk_3DBorderColor(butPtr->selectBorder)->pixel);
	}
	XFillRectangle(butPtr->display, pixmap, butPtr->disabledGC,
		butPtr->inset, butPtr->inset,
		(unsigned) (Tk_Width(tkwin) - 2*butPtr->inset),
		(unsigned) (Tk_Height(tkwin) - 2*butPtr->inset));
	if ((butPtr->flags & SELECTED) && !butPtr->indicatorOn
		&& (butPtr->selectBorder != NULL)) {
	    XSetForeground(butPtr->display, butPtr->disabledGC,
		    Tk_3DBorderColor(butPtr->normalBorder)->pixel);
	}
    }

    /*
     * Draw the border and traversal highlight last.  This way, if the
     * button's contents overflow they'll be covered up by the border.
     */

    if (relief != TK_RELIEF_FLAT) {
	int inset = butPtr->highlightWidth;
	Tk_Draw3DRectangle(tkwin, pixmap, border, inset, inset,
		Tk_Width(tkwin) - 2*inset, Tk_Height(tkwin) - 2*inset,
		butPtr->borderWidth, relief);
    }

    /*
     * Copy the information from the off-screen pixmap onto the screen,
     * then delete the pixmap.
     */

    XCopyArea(butPtr->display, pixmap, Tk_WindowId(tkwin),
	    butPtr->copyGC, 0, 0, (unsigned) Tk_Width(tkwin),
	    (unsigned) Tk_Height(tkwin), 0, 0);
    Tk_FreePixmap(butPtr->display, pixmap);

    SetGWorld(saveWorld, saveDevice);
}

/*
 *----------------------------------------------------------------------
 *
 * TkpComputeButtonGeometry --
 *
 *	After changes in a button's text or bitmap, this procedure
 *	recomputes the button's geometry and passes this information
 *	along to the geometry manager for the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The button's window may change size.
 *
 *----------------------------------------------------------------------
 */

void
TkpComputeButtonGeometry(
    TkButton *butPtr)	/* Button whose geometry may have changed. */
{
    int width, height, avgWidth;
    Tk_FontMetrics fm;

    if (butPtr->highlightWidth < 0) {
	butPtr->highlightWidth = 0;
    }
    if ((butPtr->type == TYPE_BUTTON) && (butPtr->image == None)
	    && (butPtr->bitmap == None)) {
	butPtr->inset = 0;
    } else if ((butPtr->type != TYPE_LABEL) && butPtr->indicatorOn) {
	butPtr->inset = 0;
    } else {
	butPtr->inset = butPtr->borderWidth;
    }

    /*
     * The highlight width corresponds to the default ring on the Macintosh.
     * As such, the highlight width is only added if the button is the default
     * button.  The actual width of the default ring is one less than the
     * highlight width as there is also one pixel of spacing.
     */

    if (butPtr->defaultState != tkDisabledUid) {
	butPtr->inset += butPtr->highlightWidth;
    }
    butPtr->indicatorSpace = 0;
    if (butPtr->image != NULL) {
	Tk_SizeOfImage(butPtr->image, &width, &height);
	imageOrBitmap:
	if (butPtr->width > 0) {
	    width = butPtr->width;
	}
	if (butPtr->height > 0) {
	    height = butPtr->height;
	}
	if ((butPtr->type >= TYPE_CHECK_BUTTON) && butPtr->indicatorOn) {
	    butPtr->indicatorSpace = height;
	    if (butPtr->type == TYPE_CHECK_BUTTON) {
		butPtr->indicatorDiameter = (65*height)/100;
	    } else {
		butPtr->indicatorDiameter = (75*height)/100;
	    }
	}
    } else if (butPtr->bitmap != None) {
	Tk_SizeOfBitmap(butPtr->display, butPtr->bitmap, &width, &height);
	goto imageOrBitmap;
    } else {
	Tk_FreeTextLayout(butPtr->textLayout);
	butPtr->textLayout = Tk_ComputeTextLayout(butPtr->tkfont,
		butPtr->text, -1, butPtr->wrapLength, butPtr->justify, 0,
		&butPtr->textWidth, &butPtr->textHeight);

	width = butPtr->textWidth;
	height = butPtr->textHeight;
	avgWidth = Tk_TextWidth(butPtr->tkfont, "0", 1);
	Tk_GetFontMetrics(butPtr->tkfont, &fm);

	if (butPtr->width > 0) {
	    width = butPtr->width * avgWidth;
	}
	if (butPtr->height > 0) {
	    height = butPtr->height * fm.linespace;
	}
	if ((butPtr->type >= TYPE_CHECK_BUTTON) && butPtr->indicatorOn) {
	    butPtr->indicatorDiameter = fm.linespace;
	    if (butPtr->type == TYPE_CHECK_BUTTON) {
		butPtr->indicatorDiameter = (80*butPtr->indicatorDiameter)/100;
	    }
	    butPtr->indicatorSpace = butPtr->indicatorDiameter + avgWidth;
	}
    }

    /*
     * When issuing the geometry request, add extra space for the indicator,
     * if any, and for the border and padding, plus if this is an image two 
     * extra pixels so the display can be offset by 1 pixel in either
     * direction for the raised or lowered effect.
     */

    if ((butPtr->image == NULL) && (butPtr->bitmap == None)) {
	width += 2*butPtr->padX;
	height += 2*butPtr->padY;
    }
    if ((butPtr->type == TYPE_BUTTON) && 
	((butPtr->image != NULL) || (butPtr->bitmap != None))) {
	width += 2;
	height += 2;
    }
    Tk_GeometryRequest(butPtr->tkwin, (int) (width + butPtr->indicatorSpace
	    + 2*butPtr->inset), (int) (height + 2*butPtr->inset));
    Tk_SetInternalBorder(butPtr->tkwin, butPtr->inset);
}

/*
 *----------------------------------------------------------------------
 *
 * TkpDestroyButton --
 *
 *	Free data structures associated with the button control.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Restores the default control state.
 *
 *----------------------------------------------------------------------
 */

void
TkpDestroyButton(
    TkButton *butPtr)
{
    /* Do nothing. */
}

/*
 *--------------------------------------------------------------
 *
 * DrawBufferedControl --
 *
 *	This function uses a dummy Macintosh window to allow
 *	drawing Mac controls to any GWorld (including off-screen
 *	bitmaps).  In addition, this code may apply custom
 *	colors passed in the TkButton.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Control is to the GWorld.  Static state created on
 *	first invocation of this routine.
 *
 *--------------------------------------------------------------
 */

static void
DrawBufferedControl(
    TkButton *butPtr,		/* Tk button. */
    GWorldPtr destPort)		/* Off screen GWorld. */
{
    ControlRef controlHandle;
    CCTabHandle ccTabHandle;
    int windowColorChanged = false;
    RGBColor saveBackColor;

    if (windowRef == NULL) {
	Rect geometry = {0, 0, 10, 10};
	CWindowPeek windowList;

	/*
	 * Create a dummy window that we can draw to.  We will
	 * actually replace this windows bitmap with a the one
	 * we want to draw to at a later time.  This window and
	 * the data structures attached to it are only deallocated
	 * on exit of the application.
	 */
	
	windowRef = NewCWindow(NULL, &geometry, "\pempty", false, 
	    zoomDocProc, (WindowRef) -1, true, 0);
	if (windowRef == NULL) {
	    panic("Can't allocate buffer window.");
	}
	
	/*
	 * Now add the three standard controls to hidden window.  We
	 * only create one of each and reuse them for every widget in
	 * Tk.
	 */
	
	SetPort(windowRef);
	buttonHandle = NewControl(windowRef, &geometry, "\p",
		false, 1, 0, 1, pushButProc, (SInt32) 0);
	checkHandle = NewControl(windowRef, &geometry, "\p",
		false, 1, 0, 1, checkBoxProc, (SInt32) 0);
	radioHandle = NewControl(windowRef, &geometry, "\p",
		false, 1, 0, 1, radioButProc, (SInt32) 0);
	((CWindowPeek) windowRef)->visible = true;

	buttonTabHandle = (CCTabHandle) NewHandle(sizeof(CtlCTab));
	checkTabHandle = (CCTabHandle) NewHandle(sizeof(CtlCTab));
	radioTabHandle = (CCTabHandle) NewHandle(sizeof(CtlCTab));

	/*
	 * Remove our window from the window list.  This way our
	 * applications and others will not be confused that this
	 * window exists - but no one knows about it.
	 */

	windowList = (CWindowPeek) LMGetWindowList();
	if (windowList == (CWindowPeek) windowRef) {
	    LMSetWindowList((WindowRef) windowList->nextWindow);
	} else {
	    while ((windowList != NULL) 
		    && (windowList->nextWindow != (CWindowPeek) windowRef)) {
		windowList = windowList->nextWindow;
	    }
	    if (windowList != NULL) {
		windowList->nextWindow = windowList->nextWindow->nextWindow;
	    }
	}
	((CWindowPeek) windowRef)->nextWindow = NULL;

	/* 
	 * Create an exit handler to clean up this mess if we our
	 * unloaded etc.  We need to remember the windows portPixMap
	 * so it isn't leaked.
	 *
	 * TODO: The ButtonExitProc doesn't currently work and the
	 * code it includes will crash the Mac on exit from Tk.
	 
	 oldPixPtr = ((CWindowPeek) windowRef)->port.portPixMap;
	 Tcl_CreateExitHandler(ButtonExitProc, (ClientData) NULL);
	 */
    }
    
    /*
     * Set up control in hidden window to match what we need
     * to draw in the buffered window.
     */

    switch (butPtr->type) {
	case TYPE_BUTTON:
	    controlHandle = buttonHandle;
	    ccTabHandle = buttonTabHandle;
	    break;
	case TYPE_RADIO_BUTTON:
	    controlHandle = radioHandle;
	    ccTabHandle = radioTabHandle;
	    break;
	case TYPE_CHECK_BUTTON:
	    controlHandle = checkHandle;
	    ccTabHandle = checkTabHandle;
	    break;
    }
    (**controlHandle).contrlRect.left = butPtr->inset;
    (**controlHandle).contrlRect.top = butPtr->inset;
    (**controlHandle).contrlRect.right = Tk_Width(butPtr->tkwin) 
	    - butPtr->inset;
    (**controlHandle).contrlRect.bottom = Tk_Height(butPtr->tkwin) 
	    - butPtr->inset;
    if ((**controlHandle).contrlVis != 255) {
	(**controlHandle).contrlVis = 255;
    }
    if (butPtr->flags & SELECTED) {
	(**controlHandle).contrlValue = 1;
    } else {
	(**controlHandle).contrlValue = 0;
    }
    if (butPtr->state == tkActiveUid) {
	switch (butPtr->type) {
	    case TYPE_BUTTON:
		(**controlHandle).contrlHilite = kControlButtonPart;
		break;
	    case TYPE_RADIO_BUTTON:
		(**controlHandle).contrlHilite = kControlRadioButtonPart;
		break;
	    case TYPE_CHECK_BUTTON:
		(**controlHandle).contrlHilite = kControlCheckBoxPart;
		break;
	}
    } else if (butPtr->state == tkDisabledUid) {
	(**controlHandle).contrlHilite = kControlInactivePart;
    } else {
	(**controlHandle).contrlHilite = kControlNoPart;
    }

    /*
     * Now swap in the passed in GWorld for the portBits of our fake
     * window.  We also adjust various fields in the WindowRecord to make
     * the system think this is a normal window.
     */

    ((CWindowPeek) windowRef)->port.portPixMap = destPort->portPixMap;
    ((CWindowPeek) windowRef)->port.portRect = destPort->portRect;
    RectRgn(((CWindowPeek) windowRef)->port.visRgn, &destPort->portRect);
    RectRgn(((CWindowPeek) windowRef)->strucRgn, &destPort->portRect);
    RectRgn(((CWindowPeek) windowRef)->updateRgn, &destPort->portRect);
    RectRgn(((CWindowPeek) windowRef)->contRgn, &destPort->portRect);
    PortChanged(windowRef);
    
    /*
     * Before we draw the control we must add the hidden window back to the
     * main window list.  Otherwise, radiobuttons and checkbuttons will draw
     * incorrectly.  I don't really know why - but clearly the control draw
     * proc needs to have the controls window in the window list.
     */

    ((CWindowPeek) windowRef)->nextWindow = (CWindowPeek) LMGetWindowList();
    LMSetWindowList(windowRef);

    /*
     * Now we can set the port to our doctered up window.  We next need
     * to muck with the colors for the port & window to draw the control
     * with the proper Tk colors.  If we need to we also draw a default
     * ring for buttons.
     */

    SetPort(windowRef);
    windowColorChanged = UpdateControlColors(butPtr, controlHandle, 
	ccTabHandle, &saveBackColor);
    Draw1Control(controlHandle);
    if ((butPtr->type == TYPE_BUTTON) && 
	    (butPtr->defaultState == tkActiveUid)) {
	Rect box = (**controlHandle).contrlRect;
	RGBColor rgbColor;

	TkSetMacColor(butPtr->highlightColorPtr->pixel, &rgbColor);
	RGBForeColor(&rgbColor);
	PenSize(butPtr->highlightWidth - 1, butPtr->highlightWidth - 1);
	InsetRect(&box, -butPtr->highlightWidth, -butPtr->highlightWidth);
	FrameRoundRect(&box, 16, 16);
    }
    if (windowColorChanged) {
	RGBColor dummyColor;
	ChangeBackgroundWindowColor(windowRef, saveBackColor, &dummyColor);
    }
    
    /*
     * Clean up: remove the hidden window from the main window list.
     */

    LMSetWindowList((WindowRef) ((CWindowPeek) windowRef)->nextWindow);
}

/*
 *--------------------------------------------------------------
 *
 * UpdateControlColors --
 *
 *	This function will review the colors used to display
 *	a Macintosh button.  If any non-standard colors are
 *	used we create a custom palette for the button, populate
 *	with the colors for the button and install the palette.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The Macintosh control may get a custom palette installed.
 *
 *--------------------------------------------------------------
 */

static int
UpdateControlColors(
    TkButton *butPtr,
    ControlRef controlHandle,
    CCTabHandle ccTabHandle,
    RGBColor *saveColorPtr)
{
    XColor *xcolor;
    
    xcolor = Tk_3DBorderColor(butPtr->normalBorder);

    (**ccTabHandle).ccSeed = 0;
    (**ccTabHandle).ccRider = 0;
    (**ccTabHandle).ctSize = 3;
    (**ccTabHandle).ctTable[0].value = cBodyColor;
    TkSetMacColor(xcolor->pixel,
	&(**ccTabHandle).ctTable[0].rgb);
    (**ccTabHandle).ctTable[1].value = cTextColor;
    TkSetMacColor(butPtr->normalFg->pixel,
	&(**ccTabHandle).ctTable[1].rgb);
    (**ccTabHandle).ctTable[2].value = cFrameColor;
    TkSetMacColor(butPtr->highlightColorPtr->pixel,
	&(**ccTabHandle).ctTable[2].rgb);
    SetControlColor(controlHandle, ccTabHandle);
        
    if (((xcolor->pixel >> 24) != CONTROL_BODY_PIXEL) && 
	    ((butPtr->type == TYPE_CHECK_BUTTON) ||
		    (butPtr->type == TYPE_RADIO_BUTTON))) {
	RGBColor newColor;
	
	TkSetMacColor(xcolor->pixel, &newColor);
	ChangeBackgroundWindowColor((**controlHandle).contrlOwner,
		newColor, saveColorPtr);
	return true;
    }
    
    return false;
}

/*
 *--------------------------------------------------------------
 *
 * ChangeBackgroundWindowColor --
 *
 *	This procedure will change the background color entry
 *	in the Window's colortable.  The system isn't notified
 *	of the change.  This call should only be used to fool
 *	the drawing routines for checkboxes and radiobuttons.
 *	Any change should be temporary and be reverted after
 *	the widget is drawn.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The Window's color table will be adjusted.
 *
 *--------------------------------------------------------------
 */

static void
ChangeBackgroundWindowColor(
    WindowRef macintoshWindow,	/* A Mac window whose color to change. */
    RGBColor rgbColor,		/* The new RGB Color for the background. */
    RGBColor *oldColor)		/* The old color of the background. */
{
    AuxWinHandle auxWinHandle;
    WCTabHandle winCTabHandle;
    short ctIndex;
    ColorSpecPtr rgbScan;
	
    GetAuxWin(macintoshWindow, &auxWinHandle);
    winCTabHandle = (WCTabHandle) ((**auxWinHandle).awCTable);

    /*
     * Scan through the color table until we find the content
     * (background) color for the window.  Don't tell the system
     * about the change - it will generate damage and we will get
     * into an infinite loop.
     */

    ctIndex = (**winCTabHandle).ctSize;
    while (ctIndex > -1) {
	rgbScan = ctIndex + (**winCTabHandle).ctTable;

	if (rgbScan->value == wContentColor) {
	    *oldColor = rgbScan->rgb;
	    rgbScan->rgb = rgbColor;
	    break;
	}
	ctIndex--;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonExitProc --
 *
 *	This procedure is invoked just before the application exits.
 *	It frees all of the control handles, our dummy window, etc.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory is freed.
 *
 *----------------------------------------------------------------------
 */

static void
ButtonExitProc(clientData)
    ClientData clientData;		/* Not used. */
{
    Rect pixRect = {0, 0, 10, 10};
    Rect rgnRect = {0, 0, 0, 0};

    /*
     * Restore our dummy window to it's origional state by putting it
     * back in the window list and restoring it's bits.  The destroy
     * the controls and window.
     */
 
    ((CWindowPeek) windowRef)->nextWindow = (CWindowPeek) LMGetWindowList();
    LMSetWindowList(windowRef);
    ((CWindowPeek) windowRef)->port.portPixMap = oldPixPtr;
    ((CWindowPeek) windowRef)->port.portRect = pixRect;
    RectRgn(((CWindowPeek) windowRef)->port.visRgn, &rgnRect);
    RectRgn(((CWindowPeek) windowRef)->strucRgn, &rgnRect);
    RectRgn(((CWindowPeek) windowRef)->updateRgn, &rgnRect);
    RectRgn(((CWindowPeek) windowRef)->contRgn, &rgnRect);
    PortChanged(windowRef);

    DisposeControl(buttonHandle);
    DisposeControl(checkHandle);
    DisposeControl(radioHandle);
    DisposeWindow(windowRef);
    windowRef = NULL;
}
