/*
 * bltWinop.c --
 *
 *	This module implements simple window commands for the BLT toolkit.
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

#ifndef NO_WINOP

#include "bltImage.h"
#include <X11/Xutil.h>
#ifndef WIN32
#include <X11/Xproto.h>
#endif

static Tcl_CmdProc WinopCmd;

static int
GetRealizedWindow(interp, string, tkwinPtr)
    Tcl_Interp *interp;
    char *string;
    Tk_Window *tkwinPtr;
{
    Tk_Window tkwin;

    tkwin = Tk_NameToWindow(interp, string, Tk_MainWindow(interp));
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    if (Tk_WindowId(tkwin) == None) {
	Tk_MakeWindowExist(tkwin);
    }
    *tkwinPtr = tkwin;
    return TCL_OK;
}

static Window
StringToWindow(interp, string)
    Tcl_Interp *interp;
    char *string;
{
    int xid;

    if (string[0] == '.') {
	Tk_Window tkwin;

	if (GetRealizedWindow(interp, string, &tkwin) != TCL_OK) {
	    return None;
	}
	if (Tk_IsTopLevel(tkwin)) {
	    return Blt_GetRealWindowId(tkwin);
	} else {
	    return Tk_WindowId(tkwin);
	}
    } else if (Tcl_GetInt(interp, string, &xid) == TCL_OK) {
#ifdef WIN32
	static TkWinWindow tkWinWindow;

	tkWinWindow.handle = (HWND)xid;
	tkWinWindow.winPtr = NULL;
	tkWinWindow.type = TWD_WINDOW;
	return (Window)&tkWinWindow;
#else
	return (Window)xid;
#endif
    }
    return None;
}

#ifdef WIN32

static int
GetWindowSize(
    Tcl_Interp *interp,
    Window window,
    int *widthPtr, 
    int *heightPtr)
{
    int result;
    RECT region;
    TkWinWindow *winPtr = (TkWinWindow *)window;

    result = GetWindowRect(winPtr->handle, &region);
    if (result) {
	*widthPtr = region.right - region.left;
	*heightPtr = region.bottom - region.top;
	return TCL_OK;
    }
    return TCL_ERROR;
}

#else

/*
 *----------------------------------------------------------------------
 *
 * XGeometryErrorProc --
 *
 *	Flags errors generated from XGetGeometry calls to the X server.
 *
 * Results:
 *	Always returns 0.
 *
 * Side Effects:
 *	Sets a flag, indicating an error occurred.
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static int
XGeometryErrorProc(clientData, errEventPtr)
    ClientData clientData;
    XErrorEvent *errEventPtr;
{
    int *errorPtr = clientData;

    *errorPtr = TCL_ERROR;
    return 0;
}

static int
GetWindowSize(interp, window, widthPtr, heightPtr)
    Tcl_Interp *interp;
    Window window;
    int *widthPtr, *heightPtr;
{
    int result;
    int any = -1;
    int x, y, borderWidth, depth;
    Window root;
    Tk_ErrorHandler handler;
    Tk_Window tkwin;
    
    tkwin = Tk_MainWindow(interp);
    handler = Tk_CreateErrorHandler(Tk_Display(tkwin), any, X_GetGeometry, 
	    any, XGeometryErrorProc, &result);
    result = XGetGeometry(Tk_Display(tkwin), window, &root, &x, &y, 
	  (unsigned int *)widthPtr, (unsigned int *)heightPtr,
	  (unsigned int *)&borderWidth, (unsigned int *)&depth);
    Tk_DeleteErrorHandler(handler);
    XSync(Tk_Display(tkwin), False);
    if (result) {
	return TCL_OK;
    }
    return TCL_ERROR;
}
#endif /*WIN32*/


#ifndef WIN32
/*ARGSUSED*/
static int
ColormapOp(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    register int i;
    Tk_Window tkwin;
#define MAXCOLORS 256
    register XColor *colorPtr;
    XColor colorArr[MAXCOLORS];
    unsigned long int pixelValues[MAXCOLORS];
    int inUse[MAXCOLORS];
    char string[20];
    unsigned long int *indexPtr;
    int nFree;

    if (GetRealizedWindow(interp, argv[2], &tkwin) != TCL_OK) {
	return TCL_ERROR;
    }
    /* Initially, we assume all color cells are allocated. */
    memset((char *)inUse, 0, sizeof(int) * MAXCOLORS);

    /*
     * Start allocating color cells.  This will tell us which color cells
     * haven't already been allocated in the colormap.  We'll release the
     * cells as soon as we find out how many there are.
     */
    nFree = 0;
    for (indexPtr = pixelValues, i = 0; i < MAXCOLORS; i++, indexPtr++) {
	if (!XAllocColorCells(Tk_Display(tkwin), Tk_Colormap(tkwin),
		False, NULL, 0, indexPtr, 1)) {
	    break;
	}
	inUse[*indexPtr] = TRUE;/* Indicate the cell is unallocated */
	nFree++;
    }
    XFreeColors(Tk_Display(tkwin), Tk_Colormap(tkwin), pixelValues, nFree, 0);
    for (colorPtr = colorArr, i = 0; i < MAXCOLORS; i++, colorPtr++) {
	colorPtr->pixel = i;
    }
    XQueryColors(Tk_Display(tkwin), Tk_Colormap(tkwin), colorArr, MAXCOLORS);
    for (colorPtr = colorArr, i = 0; i < MAXCOLORS; i++, colorPtr++) {
	if (!inUse[colorPtr->pixel]) {
	    sprintf(string, "#%02x%02x%02x", (colorPtr->red >> 8),
		(colorPtr->green >> 8), (colorPtr->blue >> 8));
	    Tcl_AppendElement(interp, string);
	    sprintf(string, "%ld", colorPtr->pixel);
	    Tcl_AppendElement(interp, string);
	}
    }
    return TCL_OK;
}

#endif

/*ARGSUSED*/
static int
LowerOp(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    register int i;
    Window window;
    Display *display;

    display = Tk_Display(Tk_MainWindow(interp));
    for (i = 2; i < argc; i++) {
	window = StringToWindow(interp, argv[i]);
	if (window == None) {
	    return TCL_ERROR;
	}
	XLowerWindow(display, window);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
RaiseOp(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    register int i;
    Window window;
    Display *display;

    display = Tk_Display(Tk_MainWindow(interp));
    for (i = 2; i < argc; i++) {
	window = StringToWindow(interp, argv[i]);
	if (window == None) {
	    return TCL_ERROR;
	}
	XRaiseWindow(display, window);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
MapOp(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    register int i;
    Window window;
    Display *display;

    display = Tk_Display(Tk_MainWindow(interp));
    for (i = 2; i < argc; i++) {
	if (argv[i][0] == '.') {
	    Tk_Window tkwin;
	    Tk_FakeWin *fakePtr;

	    if (GetRealizedWindow(interp, argv[i], &tkwin) != TCL_OK) {
		return TCL_ERROR;
	    }
	    fakePtr = (Tk_FakeWin *) tkwin;
	    fakePtr->flags |= TK_MAPPED;
	    window = Tk_WindowId(tkwin);
	} else {
	    int xid;

	    if (Tcl_GetInt(interp, argv[i], &xid) != TCL_OK) {
		return TCL_ERROR;
	    }
	    window = (Window)xid;
	}
	XMapWindow(display, window);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
MoveOp(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;			/* Not Used. */
    char **argv;
{
    int x, y;
    Tk_Window tkwin;
    Window window;
    Display *display;

    tkwin = Tk_MainWindow(interp);
    display = Tk_Display(tkwin);
    window = StringToWindow(interp, argv[2]);
    if (window == None) {
	return TCL_ERROR;
    }
    if (Tk_GetPixels(interp, tkwin, argv[3], &x) != TCL_OK) {
	Tcl_AppendResult(interp, ": bad window x-coordinate", (char *)NULL);
	return TCL_ERROR;
    }
    if (Tk_GetPixels(interp, tkwin, argv[4], &y) != TCL_OK) {
	Tcl_AppendResult(interp, ": bad window y-coordinate", (char *)NULL);
	return TCL_ERROR;
    }
    XMoveWindow(display, window, x, y);
    return TCL_OK;
}

/*ARGSUSED*/
static int
UnmapOp(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    register int i;
    Window window;
    Display *display;

    display = Tk_Display(Tk_MainWindow(interp));
    for (i = 2; i < argc; i++) {
	if (argv[i][0] == '.') {
	    Tk_Window tkwin;
	    Tk_FakeWin *fakePtr;

	    if (GetRealizedWindow(interp, argv[i], &tkwin) != TCL_OK) {
		return TCL_ERROR;
	    }
	    fakePtr = (Tk_FakeWin *) tkwin;
	    fakePtr->flags &= ~TK_MAPPED;
	    window = Tk_WindowId(tkwin);
	} else {
	    int xid;

	    if (Tcl_GetInt(interp, argv[i], &xid) != TCL_OK) {
		return TCL_ERROR;
	    }
	    window = (Window)xid;
	}
	XMapWindow(display, window);
    }
    return TCL_OK;
}

/* ARGSUSED */
static int
ChangesOp(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;		/* Not used. */
{
    Tk_Window tkwin;

    if (GetRealizedWindow(interp, argv[2], &tkwin) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tk_IsTopLevel(tkwin)) {
	XSetWindowAttributes attrs;
	Window window;
	unsigned int mask;

	window = Blt_GetRealWindowId(tkwin);
	attrs.backing_store = WhenMapped;
	attrs.save_under = True;
	mask = CWBackingStore | CWSaveUnder;
	XChangeWindowAttributes(Tk_Display(tkwin), window, mask, &attrs);
    }
    return TCL_OK;
}

/* ARGSUSED */
static int
QueryOp(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;		/* Not used. */
{
    int rootX, rootY, childX, childY;
    Window root, child;
    unsigned int mask;
    Tk_Window tkwin = (Tk_Window)clientData;

    /* GetCursorPos */
    if (XQueryPointer(Tk_Display(tkwin), Tk_WindowId(tkwin), &root,
	    &child, &rootX, &rootY, &childX, &childY, &mask)) {
	char string[200];

	sprintf(string, "@%d,%d", rootX, rootY);
	Tcl_SetResult(interp, string, TCL_VOLATILE);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
WarpToOp(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tk_Window tkwin, mainWindow;

    mainWindow = (Tk_Window)clientData;
    if (argc > 2) {
	if (argv[2][0] == '@') {
	    int x, y;
	    Window root;

	    if (Blt_GetXY(interp, mainWindow, argv[2], &x, &y) != TCL_OK) {
		return TCL_ERROR;
	    }
	    root = RootWindow(Tk_Display(mainWindow), 
		Tk_ScreenNumber(mainWindow));
	    XWarpPointer(Tk_Display(mainWindow), None, root, 0, 0, 0, 0, x, y);
	} else {
	    if (GetRealizedWindow(interp, argv[2], &tkwin) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (!Tk_IsMapped(tkwin)) {
		Tcl_AppendResult(interp, "can't warp to unmapped window \"",
		    Tk_PathName(tkwin), "\"", (char *)NULL);
		return TCL_ERROR;
	    }
	    XWarpPointer(Tk_Display(tkwin), None, Tk_WindowId(tkwin),
		0, 0, 0, 0, Tk_Width(tkwin) / 2, Tk_Height(tkwin) / 2);
	}
    }
    return QueryOp(clientData, interp, 0, (char **)NULL);
}

#ifdef notdef
static int
ReparentOp(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    Tk_Window tkwin;

    if (GetRealizedWindow(interp, argv[2], &tkwin) != TCL_OK) {
	return TCL_ERROR;
    }
    if (argc == 4) {
	Tk_Window newParent;

	if (GetRealizedWindow(interp, argv[3], &newParent) != TCL_OK) {
	    return TCL_ERROR;
	}
	Blt_RelinkWindow2(tkwin, Blt_GetRealWindowId(tkwin), newParent, 0, 0);
    } else {
	Blt_UnlinkWindow(tkwin);
    }
    return TCL_OK;
}
#endif


/*
 * This is a temporary home for these image routines.  They will be
 * moved when a new image type is created for them.
 */
/*ARGSUSED*/
static int
ConvolveOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tk_PhotoHandle srcPhoto, destPhoto;
    Blt_ColorImage srcImage, destImage;
    Filter2D filter;
    int nValues;
    char **valueArr;
    double *kernel;
    double value, sum;
    register int i;
    int dim;
    int result = TCL_ERROR;

    srcPhoto = Blt_FindPhoto(interp, argv[2]);
    if (srcPhoto == NULL) {
	Tcl_AppendResult(interp, "source image \"", argv[2], "\" doesn't",
	    " exist or is not a photo image", (char *)NULL);
	return TCL_ERROR;
    }
    destPhoto = Blt_FindPhoto(interp, argv[3]);
    if (destPhoto == NULL) {
	Tcl_AppendResult(interp, "destination image \"", argv[3], "\" doesn't",
	    " exist or is not a photo image", (char *)NULL);
	return TCL_ERROR;
    }
    if (Tcl_SplitList(interp, argv[4], &nValues, &valueArr) != TCL_OK) {
	return TCL_ERROR;
    }
    kernel = NULL;
    if (nValues == 0) {
	Tcl_AppendResult(interp, "empty kernel", (char *)NULL);
	goto error;
    }
    dim = (int)sqrt((double)nValues);
    if ((dim * dim) != nValues) {
	Tcl_AppendResult(interp, "kernel must be square", (char *)NULL);
	goto error;
    }
    kernel = Blt_Malloc(sizeof(double) * nValues);
    sum = 0.0;
    for (i = 0; i < nValues; i++) {
	if (Tcl_GetDouble(interp, valueArr[i], &value) != TCL_OK) {
	    goto error;
	}
	kernel[i] = value;
	sum += value;
    }
    filter.kernel = kernel;
    filter.support = dim * 0.5;
    filter.sum = (sum == 0.0) ? 1.0 : sum;
    filter.scale = 1.0 / nValues;

    srcImage = Blt_PhotoToColorImage(srcPhoto);
    destImage = Blt_ConvolveColorImage(srcImage, &filter);
    Blt_FreeColorImage(srcImage);
    Blt_ColorImageToPhoto(destImage, destPhoto);
    Blt_FreeColorImage(destImage);
    result = TCL_OK;
  error:
    if (valueArr != NULL) {
	Blt_Free(valueArr);
    }
    if (kernel != NULL) {
	Blt_Free(kernel);
    }
    return result;
}

/*ARGSUSED*/
static int
QuantizeOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tk_PhotoHandle srcPhoto, destPhoto;
    Tk_PhotoImageBlock src, dest;
    Blt_ColorImage srcImage, destImage;
    int nColors;
    int result;

    srcPhoto = Blt_FindPhoto(interp, argv[2]);
    if (srcPhoto == NULL) {
	Tcl_AppendResult(interp, "source image \"", argv[2], "\" doesn't",
	    " exist or is not a photo image", (char *)NULL);
	return TCL_ERROR;
    }
    Tk_PhotoGetImage(srcPhoto, &src);
    if ((src.width <= 1) || (src.height <= 1)) {
	Tcl_AppendResult(interp, "source image \"", argv[2], "\" is empty",
	    (char *)NULL);
	return TCL_ERROR;
    }
    destPhoto = Blt_FindPhoto(interp, argv[3]);
    if (destPhoto == NULL) {
	Tcl_AppendResult(interp, "destination image \"", argv[3], "\" doesn't",
	    " exist or is not a photo image", (char *)NULL);
	return TCL_ERROR;
    }
    Tk_PhotoGetImage(destPhoto, &dest);
    if ((dest.width != src.width) || (dest.height != src.height)) {
	Tk_PhotoSetSize(destPhoto, src.width, src.height);
    }
    if (Tcl_GetInt(interp, argv[4], &nColors) != TCL_OK) {
	return TCL_ERROR;
    }
    srcImage = Blt_PhotoToColorImage(srcPhoto);
    destImage = Blt_PhotoToColorImage(destPhoto);
    result = Blt_QuantizeColorImage(srcImage, destImage, nColors);
    if (result == TCL_OK) {
	Blt_ColorImageToPhoto(destImage, destPhoto);
    }
    Blt_FreeColorImage(srcImage);
    Blt_FreeColorImage(destImage);
    return result;
}


/*ARGSUSED*/
static int
ReadJPEGOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
#if HAVE_JPEGLIB_H
    Tk_PhotoHandle photo;	/* The photo image to write into. */

    photo = Blt_FindPhoto(interp, argv[3]);
    if (photo == NULL) {
	Tcl_AppendResult(interp, "image \"", argv[3], "\" doesn't",
	    " exist or is not a photo image", (char *)NULL);
	return TCL_ERROR;
    }
    return Blt_JPEGToPhoto(interp, argv[2], photo);
#else
    Tcl_AppendResult(interp, "JPEG support not compiled", (char *)NULL);
    return TCL_ERROR;
#endif
}


/*ARGSUSED*/
static int
GradientOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tk_PhotoHandle photo;
    Tk_PhotoImageBlock src;
    XColor *leftPtr, *rightPtr;
    Tk_Window tkwin;
    double range[3];
    double left[3];
    Pix32 *destPtr;
    Blt_ColorImage destImage;

    tkwin = Tk_MainWindow(interp);
    photo = Blt_FindPhoto(interp, argv[2]);
    if (photo == NULL) {
	Tcl_AppendResult(interp, "source image \"", argv[2], "\" doesn't",
	    " exist or is not a photo image", (char *)NULL);
	return TCL_ERROR;
    }
    Tk_PhotoGetImage(photo, &src);
    leftPtr = Tk_GetColor(interp, tkwin, Tk_GetUid(argv[3]));
    if (leftPtr == NULL) {
	return TCL_ERROR;
    }
    rightPtr = Tk_GetColor(interp, tkwin, Tk_GetUid(argv[4]));
    if (leftPtr == NULL) {
	return TCL_ERROR;
    }
    left[0] = (double)(leftPtr->red >> 8);
    left[1] = (double)(leftPtr->green >> 8);
    left[2] = (double)(leftPtr->blue >> 8);
    range[0] = (double)((rightPtr->red - leftPtr->red) / 257.0);
    range[1] = (double)((rightPtr->green - leftPtr->green) / 257.0);
    range[2] = (double)((rightPtr->blue - leftPtr->blue) / 257.0);

    destImage = Blt_CreateColorImage(src.width, src.height);
    destPtr = Blt_ColorImageBits(destImage);
#define CLAMP(c)	((((c) < 0.0) ? 0.0 : ((c) > 1.0) ? 1.0 : (c)))
    if (strcmp(argv[5], "linear") == 0) {
	register int x, y;
	double t;

	for (y = 0; y < src.height; y++) {
	    for (x = 0; x < src.width; x++) {
		t = (double)x * (drand48() * 0.10 - 0.05);
		t = CLAMP(t);
		destPtr->Red = (unsigned char)(left[0] + t * range[0]);
		destPtr->Green = (unsigned char)(left[1] + t * range[1]);
		destPtr->Blue = (unsigned char)(left[2] + t * range[2]);
		destPtr->Alpha = (unsigned char)-1;
		destPtr++;
	    }
	}
    } else if (strcmp(argv[5], "radial") == 0) {
	register int x, y;
	register double dx, dy;
	double dy2;
	double t;
	double midX, midY;

	midX = midY = 0.5;
	for (y = 0; y < src.height; y++) {
	    dy = (y / (double)src.height) - midY;
	    dy2 = dy * dy;
	    for (x = 0; x < src.width; x++) {
		dx = (x / (double)src.width) - midX;
		t = 1.0  - (double)sqrt(dx * dx + dy2);
		t += t * (drand48() * 0.10 - 0.05);
		t = CLAMP(t);
		destPtr->Red = (unsigned char)(left[0] + t * range[0]);
		destPtr->Green = (unsigned char)(left[1] + t * range[1]);
		destPtr->Blue = (unsigned char)(left[2] + t * range[2]);
		destPtr->Alpha = (unsigned char)-1;
		destPtr++;
	    }
	}
    } else if (strcmp(argv[5], "rectangular") == 0) {
	register int x, y;
	register double dx, dy;
	double t, px, py;
	double midX, midY;
	double cosTheta, sinTheta;
	double angle;

	angle = M_PI_2 * -0.3;
	cosTheta = cos(angle);
	sinTheta = sin(angle);

	midX = 0.5, midY = 0.5;
	for (y = 0; y < src.height; y++) {
	    dy = (y / (double)src.height) - midY;
	    for (x = 0; x < src.width; x++) {
		dx = (x / (double)src.width) - midX;
		px = dx * cosTheta - dy * sinTheta;
		py = dx * sinTheta + dy * cosTheta;
		t = FABS(px) + FABS(py);
		t += t * (drand48() * 0.10 - 0.05);
		t = CLAMP(t);
		destPtr->Red = (unsigned char)(left[0] + t * range[0]);
		destPtr->Green = (unsigned char)(left[1] + t * range[1]);
		destPtr->Blue = (unsigned char)(left[2] + t * range[2]);
		destPtr->Alpha = (unsigned char)-1;
		destPtr++;
	    }
	}
    } else if (strcmp(argv[5], "blank") == 0) {
	register int x, y;

	for (y = 0; y < src.height; y++) {
	    for (x = 0; x < src.width; x++) {
		destPtr->Red = (unsigned char)0xFF;
		destPtr->Green = (unsigned char)0xFF;
		destPtr->Blue = (unsigned char)0xFF;
		destPtr->Alpha = (unsigned char)-1;
		destPtr++;
	    }
	}
    }
    Blt_ColorImageToPhoto(destImage, photo);
    return TCL_OK;
}

/*ARGSUSED*/
static int
ResampleOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tk_PhotoHandle srcPhoto, destPhoto;
    Tk_PhotoImageBlock src, dest;
    ResampleFilter *filterPtr, *vertFilterPtr, *horzFilterPtr;
    char *filterName;

    srcPhoto = Blt_FindPhoto(interp, argv[2]);
    if (srcPhoto == NULL) {
	Tcl_AppendResult(interp, "source image \"", argv[2], "\" doesn't",
	    " exist or is not a photo image", (char *)NULL);
	return TCL_ERROR;
    }
    destPhoto = Blt_FindPhoto(interp, argv[3]);
    if (destPhoto == NULL) {
	Tcl_AppendResult(interp, "destination image \"", argv[3], "\" doesn't",
	    " exist or is not a photo image", (char *)NULL);
	return TCL_ERROR;
    }
    filterName = (argc > 4) ? argv[4] : "none";
    if (Blt_GetResampleFilter(interp, filterName, &filterPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    vertFilterPtr = horzFilterPtr = filterPtr;
    if ((filterPtr != NULL) && (argc > 5)) {
	if (Blt_GetResampleFilter(interp, argv[5], &filterPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	vertFilterPtr = filterPtr;
    }
    Tk_PhotoGetImage(srcPhoto, &src);
    if ((src.width <= 1) || (src.height <= 1)) {
	Tcl_AppendResult(interp, "source image \"", argv[2], "\" is empty",
	    (char *)NULL);
	return TCL_ERROR;
    }
    Tk_PhotoGetImage(destPhoto, &dest);
    if ((dest.width <= 1) || (dest.height <= 1)) {
	Tk_PhotoSetSize(destPhoto, src.width, src.height);
	goto copyImage;
    }
    if ((src.width == dest.width) && (src.height == dest.height)) {
      copyImage:
	/* Source and destination image sizes are the same. Don't
	 * resample. Simply make copy of image */
	dest.width = src.width;
	dest.height = src.height;
	dest.pixelPtr = src.pixelPtr;
	dest.pixelSize = src.pixelSize;
	dest.pitch = src.pitch;
	dest.offset[0] = src.offset[0];
	dest.offset[1] = src.offset[1];
	dest.offset[2] = src.offset[2];
	Tk_PhotoPutBlock(destPhoto, &dest, 0, 0, dest.width, dest.height);
	return TCL_OK;
    }
    if (filterPtr == NULL) {
	Blt_ResizePhoto(srcPhoto, 0, 0, src.width, src.height, destPhoto);
    } else {
	Blt_ResamplePhoto(srcPhoto, 0, 0, src.width, src.height, destPhoto,
		horzFilterPtr, vertFilterPtr);
    }
    return TCL_OK;
}

/*ARGSUSED*/
static int
RotateOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tk_PhotoHandle srcPhoto, destPhoto;
    Blt_ColorImage srcImage, destImage;
    double theta;

    srcPhoto = Blt_FindPhoto(interp, argv[2]);
    if (srcPhoto == NULL) {
	Tcl_AppendResult(interp, "image \"", argv[2], "\" doesn't",
	    " exist or is not a photo image", (char *)NULL);
	return TCL_ERROR;
    }
    destPhoto = Blt_FindPhoto(interp, argv[3]);
    if (destPhoto == NULL) {
	Tcl_AppendResult(interp, "destination image \"", argv[3], "\" doesn't",
	    " exist or is not a photo image", (char *)NULL);
	return TCL_ERROR;
    }
    if (Tcl_ExprDouble(interp, argv[4], &theta) != TCL_OK) {
	return TCL_ERROR;
    }
    srcImage = Blt_PhotoToColorImage(srcPhoto);
    destImage = Blt_RotateColorImage(srcImage, theta);

    Blt_ColorImageToPhoto(destImage, destPhoto);
    Blt_FreeColorImage(srcImage);
    Blt_FreeColorImage(destImage);
    return TCL_OK;
}
/*
 * --------------------------------------------------------------------------
 *
 * SnapOp --
 *
 *	Snaps a picture of a window and stores it in a designated
 *	photo image.  The window must be completely visible or
 *	the snap will fail.
 *
 * Results:
 *	Returns a standard Tcl result.  interp->result contains
 *	the list of the graph coordinates. If an error occurred
 *	while parsing the window positions, TCL_ERROR is returned,
 *	then interp->result will contain an error message.
 *
 * -------------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
SnapOp(clientData, interp, argc, argv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tk_Window tkwin;
    int width, height, destWidth, destHeight;
    Window window;

    tkwin = Tk_MainWindow(interp);
    window = StringToWindow(interp, argv[2]);
    if (window == None) {
	return TCL_ERROR;
    }
    if (GetWindowSize(interp, window, &width, &height) != TCL_OK) {
	Tcl_AppendResult(interp, "can't get window geometry of \"", argv[2],
		 "\"", (char *)NULL);
	return TCL_ERROR;
    }
    destWidth = width, destHeight = height;
    if ((argc > 4) && (Blt_GetPixels(interp, tkwin, argv[4], PIXELS_POSITIVE,
		&destWidth) != TCL_OK)) {
	return TCL_ERROR;
    }
    if ((argc > 5) && (Blt_GetPixels(interp, tkwin, argv[5], PIXELS_POSITIVE,
		&destHeight) != TCL_OK)) {
	return TCL_ERROR;
    }
    return Blt_SnapPhoto(interp, tkwin, window, 0, 0, width, height, destWidth,
	destHeight, argv[3], GAMMA);
}

/*ARGSUSED*/
static int
SubsampleOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of the interpreter. */
    Tcl_Interp *interp;
    int argc;			/* Not used. */
    char **argv;
{
    Tk_Window tkwin;
    Tk_PhotoHandle srcPhoto, destPhoto;
    Tk_PhotoImageBlock src, dest;
    ResampleFilter *filterPtr, *vertFilterPtr, *horzFilterPtr;
    char *filterName;
    int flag;
    int x, y;
    int width, height;

    srcPhoto = Blt_FindPhoto(interp, argv[2]);
    if (srcPhoto == NULL) {
	Tcl_AppendResult(interp, "source image \"", argv[2], "\" doesn't",
	    " exist or is not a photo image", (char *)NULL);
	return TCL_ERROR;
    }
    destPhoto = Blt_FindPhoto(interp, argv[3]);
    if (destPhoto == NULL) {
	Tcl_AppendResult(interp, "destination image \"", argv[3], "\" doesn't",
	    " exist or is not a photo image", (char *)NULL);
	return TCL_ERROR;
    }
    tkwin = (Tk_Window)clientData;
    flag = PIXELS_NONNEGATIVE;
    if (Blt_GetPixels(interp, tkwin, argv[4], flag, &x) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_GetPixels(interp, tkwin, argv[5], flag, &y) != TCL_OK) {
	return TCL_ERROR;
    }
    flag = PIXELS_POSITIVE;
    if (Blt_GetPixels(interp, tkwin, argv[6], flag, &width) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Blt_GetPixels(interp, tkwin, argv[7], flag, &height) != TCL_OK) {
	return TCL_ERROR;
    }
    filterName = (argc > 8) ? argv[8] : "box";
    if (Blt_GetResampleFilter(interp, filterName, &filterPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    vertFilterPtr = horzFilterPtr = filterPtr;
    if ((filterPtr != NULL) && (argc > 9)) {
	if (Blt_GetResampleFilter(interp, argv[9], &filterPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	vertFilterPtr = filterPtr;
    }
    Tk_PhotoGetImage(srcPhoto, &src);
    Tk_PhotoGetImage(destPhoto, &dest);
    if ((src.width <= 1) || (src.height <= 1)) {
	Tcl_AppendResult(interp, "source image \"", argv[2], "\" is empty",
	    (char *)NULL);
	return TCL_ERROR;
    }
    if (((x + width) > src.width) || ((y + height) > src.height)) {
	Tcl_AppendResult(interp, "nonsensical dimensions for subregion: x=",
	    argv[4], " y=", argv[5], " width=", argv[6], " height=",
	    argv[7], (char *)NULL);
	return TCL_ERROR;
    }
    if ((dest.width <= 1) || (dest.height <= 1)) {
	Tk_PhotoSetSize(destPhoto, width, height);
    }
    if (filterPtr == NULL) {
	Blt_ResizePhoto(srcPhoto, x, y, width, height, destPhoto);
    } else {
	Blt_ResamplePhoto(srcPhoto, x, y, width, height, destPhoto, 
		horzFilterPtr, vertFilterPtr);
    }
    return TCL_OK;
}

static Blt_OpSpec imageOps[] =
{
    {"convolve", 1, (Blt_Op)ConvolveOp, 6, 6,
	"srcPhoto destPhoto filter",},
    {"gradient", 1, (Blt_Op)GradientOp, 7, 7, "photo left right type",},
    {"readjpeg", 3, (Blt_Op)ReadJPEGOp, 5, 5, "fileName photoName",},
    {"resample", 3, (Blt_Op)ResampleOp, 5, 7, 
	"srcPhoto destPhoto ?horzFilter vertFilter?",},
    {"rotate", 2, (Blt_Op)RotateOp, 6, 6, "srcPhoto destPhoto angle",},
    {"snap", 2, (Blt_Op)SnapOp, 5, 7, "window photoName ?width height?",},
    {"subsample", 2, (Blt_Op)SubsampleOp, 9, 11,
	"srcPhoto destPhoto x y width height ?horzFilter? ?vertFilter?",},
};

static int nImageOps = sizeof(imageOps) / sizeof(Blt_OpSpec);

/* ARGSUSED */
static int
ImageOp(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOp(interp, nImageOps, imageOps, BLT_OP_ARG2, argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    clientData = (ClientData)Tk_MainWindow(interp);
    result = (*proc) (clientData, interp, argc - 1, argv + 1);
    return result;
}

static Blt_OpSpec winOps[] =
{
    {"changes", 3, (Blt_Op)ChangesOp, 3, 3, "window",},
#ifndef WIN32
    {"colormap", 3, (Blt_Op)ColormapOp, 3, 3, "window",},
#endif
    {"convolve", 3, (Blt_Op)ConvolveOp, 5, 5,
	"srcPhoto destPhoto filter",},
    {"image", 1, (Blt_Op)ImageOp, 2, 0, "args",},
    {"lower", 1, (Blt_Op)LowerOp, 2, 0, "window ?window?...",},
    {"map", 2, (Blt_Op)MapOp, 2, 0, "window ?window?...",},
    {"move", 2, (Blt_Op)MoveOp, 5, 5, "window x y",},
    {"quantize", 3, (Blt_Op)QuantizeOp, 4, 5, "srcPhoto destPhoto ?nColors?",},
    {"query", 3, (Blt_Op)QueryOp, 2, 2, "",},
    {"raise", 2, (Blt_Op)RaiseOp, 2, 0, "window ?window?...",},
    {"readjpeg", 3, (Blt_Op)ReadJPEGOp, 4, 4, "fileName photoName",},
#ifdef notdef
    {"reparent", 3, (Blt_Op)ReparentOp, 3, 4, "window ?parent?",},
#endif
    {"resample", 3, (Blt_Op)ResampleOp, 4, 6,
	"srcPhoto destPhoto ?horzFilter vertFilter?",},
    {"snap", 2, (Blt_Op)SnapOp, 4, 6,
	"window photoName ?width height?",},
    {"subsample", 2, (Blt_Op)SubsampleOp, 8, 10,
	"srcPhoto destPhoto x y width height ?horzFilter? ?vertFilter?",},
    {"unmap", 1, (Blt_Op)UnmapOp, 2, 0, "window ?window?...",},
    {"warpto", 1, (Blt_Op)WarpToOp, 2, 5, "?window?",},
};

static int nWinOps = sizeof(winOps) / sizeof(Blt_OpSpec);

/* ARGSUSED */
static int
WinopCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window of interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Blt_Op proc;
    int result;

    proc = Blt_GetOp(interp, nWinOps, winOps, BLT_OP_ARG1,  argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    clientData = (ClientData)Tk_MainWindow(interp);
    result = (*proc) (clientData, interp, argc, argv);
    return result;
}

int
Blt_WinopInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec = {"winop", WinopCmd,};

    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

#endif /* NO_WINOP */
