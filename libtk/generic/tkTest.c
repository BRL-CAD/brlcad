/* 
 * tkTest.c --
 *
 *	This file contains C command procedures for a bunch of additional
 *	Tcl commands that are used for testing out Tcl's C interfaces.
 *	These commands are not normally included in Tcl applications;
 *	they're only used for testing.
 *
 * Copyright (c) 1993-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkTest.c 1.50 97/11/06 16:56:32
 */

#include "tkInt.h"
#include "tkPort.h"	

#ifdef __WIN32__
#include "tkWinInt.h"
#endif

#ifdef MAC_TCL
#include "tkScrollbar.h"
#endif

#ifdef __UNIX__
#include "tkUnixInt.h"
#endif

/*
 * The following data structure represents the master for a test
 * image:
 */

typedef struct TImageMaster {
    Tk_ImageMaster master;	/* Tk's token for image master. */
    Tcl_Interp *interp;		/* Interpreter for application. */
    int width, height;		/* Dimensions of image. */
    char *imageName;		/* Name of image (malloc-ed). */
    char *varName;		/* Name of variable in which to log
				 * events for image (malloc-ed). */
} TImageMaster;

/*
 * The following data structure represents a particular use of a
 * particular test image.
 */

typedef struct TImageInstance {
    TImageMaster *masterPtr;	/* Pointer to master for image. */
    XColor *fg;			/* Foreground color for drawing in image. */
    GC gc;			/* Graphics context for drawing in image. */
} TImageInstance;

/*
 * The type record for test images:
 */

static int		ImageCreate _ANSI_ARGS_((Tcl_Interp *interp,
			    char *name, int argc, char **argv,
			    Tk_ImageType *typePtr, Tk_ImageMaster master,
			    ClientData *clientDataPtr));
static ClientData	ImageGet _ANSI_ARGS_((Tk_Window tkwin,
			    ClientData clientData));
static void		ImageDisplay _ANSI_ARGS_((ClientData clientData,
			    Display *display, Drawable drawable, 
			    int imageX, int imageY, int width,
			    int height, int drawableX,
			    int drawableY));
static void		ImageFree _ANSI_ARGS_((ClientData clientData,
			    Display *display));
static void		ImageDelete _ANSI_ARGS_((ClientData clientData));

static Tk_ImageType imageType = {
    "test",			/* name */
    ImageCreate,		/* createProc */
    ImageGet,			/* getProc */
    ImageDisplay,		/* displayProc */
    ImageFree,			/* freeProc */
    ImageDelete,		/* deleteProc */
    (Tk_ImageType *) NULL	/* nextPtr */
};

/*
 * One of the following structures describes each of the interpreters
 * created by the "testnewapp" command.  This information is used by
 * the "testdeleteinterps" command to destroy all of those interpreters.
 */

typedef struct NewApp {
    Tcl_Interp *interp;		/* Token for interpreter. */
    struct NewApp *nextPtr;	/* Next in list of new interpreters. */
} NewApp;

static NewApp *newAppPtr = NULL;
				/* First in list of all new interpreters. */

/*
 * Declaration for the square widget's class command procedure:
 */

extern int SquareCmd _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, int argc, char *argv[]));

typedef struct CBinding {
    Tcl_Interp *interp;
    char *command;
    char *delete;
} CBinding;

/*
 * Forward declarations for procedures defined later in this file:
 */

static int		CBindingEvalProc _ANSI_ARGS_((ClientData clientData, 
			    Tcl_Interp *interp, XEvent *eventPtr,
			    Tk_Window tkwin, KeySym keySym));
static void		CBindingFreeProc _ANSI_ARGS_((ClientData clientData));
int			Tktest_Init _ANSI_ARGS_((Tcl_Interp *interp));
static int		ImageCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestcbindCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
#ifdef __WIN32__
static int		TestclipboardCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
#endif
static int		TestdeleteappsCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestmakeexistCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestmenubarCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
#if defined(__WIN32__) || defined(MAC_TCL)
static int		TestmetricsCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
#endif
static int		TestsendCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		TestpropCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
#if !(defined(__WIN32__) || defined(MAC_TCL))
static int		TestwrapperCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
#endif

/*
 * External (platform specific) initialization routine:
 */

EXTERN int		TkplatformtestInit _ANSI_ARGS_((
			    Tcl_Interp *interp));
#ifndef MAC_TCL
#define TkplatformtestInit(x) TCL_OK
#endif

/*
 *----------------------------------------------------------------------
 *
 * Tktest_Init --
 *
 *	This procedure performs intialization for the Tk test
 *	suite exensions.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in interp->result if an error occurs.
 *
 * Side effects:
 *	Creates several test commands.
 *
 *----------------------------------------------------------------------
 */

int
Tktest_Init(interp)
    Tcl_Interp *interp;		/* Interpreter for application. */
{
    static int initialized = 0;

    /*
     * Create additional commands for testing Tk.
     */

    if (Tcl_PkgProvide(interp, "Tktest", TK_VERSION) == TCL_ERROR) {
        return TCL_ERROR;
    }

    Tcl_CreateCommand(interp, "square", SquareCmd,
	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL);
#ifdef __WIN32__
    Tcl_CreateCommand(interp, "testclipboard", TestclipboardCmd,
	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL);
#endif
    Tcl_CreateCommand(interp, "testcbind", TestcbindCmd,
	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testdeleteapps", TestdeleteappsCmd,
	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testembed", TkpTestembedCmd,
	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testmakeexist", TestmakeexistCmd,
	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testmenubar", TestmenubarCmd,
	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL);
#if defined(__WIN32__) || defined(MAC_TCL)
    Tcl_CreateCommand(interp, "testmetrics", TestmetricsCmd,
	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL);
#endif
    Tcl_CreateCommand(interp, "testprop", TestpropCmd,
	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testsend", TestsendCmd,
	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL);
#if !(defined(__WIN32__) || defined(MAC_TCL))
    Tcl_CreateCommand(interp, "testwrapper", TestwrapperCmd,
	    (ClientData) Tk_MainWindow(interp), (Tcl_CmdDeleteProc *) NULL);
#endif

/*
     * Create test image type.
     */

    if (!initialized) {
	initialized = 1;
	Tk_CreateImageType(&imageType);
    }

    /*
     * And finally add any platform specific test commands.
     */
    
    return TkplatformtestInit(interp);
}

/*
 *----------------------------------------------------------------------
 *
 * TestclipboardCmd --
 *
 *	This procedure implements the testclipboard command. It provides
 *	a way to determine the actual contents of the Windows clipboard.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifdef __WIN32__
static int
TestclipboardCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window for application. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    TkWindow *winPtr = (TkWindow *) clientData;
    HGLOBAL handle;
    char *data;

    if (OpenClipboard(NULL)) {
	handle = GetClipboardData(CF_TEXT);
	if (handle != NULL) {
	    data = GlobalLock(handle);
	    Tcl_AppendResult(interp, data, (char *) NULL);
	    GlobalUnlock(handle);
	}
	CloseClipboard();
    }
    return TCL_OK;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TestcbindCmd --
 *
 *	This procedure implements the "testcbinding" command.  It provides
 *	a set of functions for testing C bindings in tkBind.c.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Depends on option;  see below.
 *
 *----------------------------------------------------------------------
 */

static int
TestcbindCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window for application. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    TkWindow *winPtr;
    Tk_Window tkwin;
    ClientData object;
    CBinding *cbindPtr;
    
    
    if (argc < 4 || argc > 5) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" bindtag pattern command ?deletecommand?", (char *) NULL);
	return TCL_ERROR;
    }

    tkwin = (Tk_Window) clientData;

    if (argv[1][0] == '.') {
	winPtr = (TkWindow *) Tk_NameToWindow(interp, argv[1], tkwin);
	if (winPtr == NULL) {
	    return TCL_ERROR;
	}
	object = (ClientData) winPtr->pathName;
    } else {
	winPtr = (TkWindow *) clientData;
	object = (ClientData) Tk_GetUid(argv[1]);
    }

    if (argv[3][0] == '\0') {
	return Tk_DeleteBinding(interp, winPtr->mainPtr->bindingTable,
		object, argv[2]);
    }

    cbindPtr = (CBinding *) ckalloc(sizeof(CBinding));
    cbindPtr->interp = interp;
    cbindPtr->command =
	    strcpy((char *) ckalloc(strlen(argv[3]) + 1), argv[3]);
    if (argc == 4) {
	cbindPtr->delete = NULL;
    } else {
	cbindPtr->delete =
		strcpy((char *) ckalloc(strlen(argv[4]) + 1), argv[4]);
    }

    if (TkCreateBindingProcedure(interp, winPtr->mainPtr->bindingTable,
	    object, argv[2], CBindingEvalProc, CBindingFreeProc,
	    (ClientData) cbindPtr) == 0) {
	ckfree((char *) cbindPtr->command);
	if (cbindPtr->delete != NULL) {
	    ckfree((char *) cbindPtr->delete);
	}
	ckfree((char *) cbindPtr);
	return TCL_ERROR;
    }
    return TCL_OK;
}

static int
CBindingEvalProc(clientData, interp, eventPtr, tkwin, keySym)
    ClientData clientData;
    Tcl_Interp *interp;
    XEvent *eventPtr;
    Tk_Window tkwin;
    KeySym keySym;
{
    CBinding *cbindPtr;

    cbindPtr = (CBinding *) clientData;
    
    return Tcl_GlobalEval(interp, cbindPtr->command);
}

static void
CBindingFreeProc(clientData)
    ClientData clientData;
{
    CBinding *cbindPtr = (CBinding *) clientData;
    
    if (cbindPtr->delete != NULL) {
	Tcl_GlobalEval(cbindPtr->interp, cbindPtr->delete);
	ckfree((char *) cbindPtr->delete);
    }
    ckfree((char *) cbindPtr->command);
    ckfree((char *) cbindPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TestdeleteappsCmd --
 *
 *	This procedure implements the "testdeleteapps" command.  It cleans
 *	up all the interpreters left behind by the "testnewapp" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	All the intepreters created by previous calls to "testnewapp"
 *	get deleted.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestdeleteappsCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window for application. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    NewApp *nextPtr;

    while (newAppPtr != NULL) {
	nextPtr = newAppPtr->nextPtr;
	Tcl_DeleteInterp(newAppPtr->interp);
	ckfree((char *) newAppPtr);
	newAppPtr = nextPtr;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ImageCreate --
 *
 *	This procedure is called by the Tk image code to create "test"
 *	images.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The data structure for a new image is allocated.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
ImageCreate(interp, name, argc, argv, typePtr, master, clientDataPtr)
    Tcl_Interp *interp;		/* Interpreter for application containing
				 * image. */
    char *name;			/* Name to use for image. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings for options (doesn't
				 * include image name or type). */
    Tk_ImageType *typePtr;	/* Pointer to our type record (not used). */
    Tk_ImageMaster master;	/* Token for image, to be used by us in
				 * later callbacks. */
    ClientData *clientDataPtr;	/* Store manager's token for image here;
				 * it will be returned in later callbacks. */
{
    TImageMaster *timPtr;
    char *varName;
    int i;

    varName = "log";
    for (i = 0; i < argc; i += 2) {
	if (strcmp(argv[i], "-variable") != 0) {
	    Tcl_AppendResult(interp, "bad option name \"", argv[i],
		    "\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if ((i+1) == argc) {
	    Tcl_AppendResult(interp, "no value given for \"", argv[i],
		    "\" option", (char *) NULL);
	    return TCL_ERROR;
	}
	varName = argv[i+1];
    }
    timPtr = (TImageMaster *) ckalloc(sizeof(TImageMaster));
    timPtr->master = master;
    timPtr->interp = interp;
    timPtr->width = 30;
    timPtr->height = 15;
    timPtr->imageName = (char *) ckalloc((unsigned) (strlen(name) + 1));
    strcpy(timPtr->imageName, name);
    timPtr->varName = (char *) ckalloc((unsigned) (strlen(varName) + 1));
    strcpy(timPtr->varName, varName);
    Tcl_CreateCommand(interp, name, ImageCmd, (ClientData) timPtr,
	    (Tcl_CmdDeleteProc *) NULL);
    *clientDataPtr = (ClientData) timPtr;
    Tk_ImageChanged(master, 0, 0, 30, 15, 30, 15);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ImageCmd --
 *
 *	This procedure implements the commands corresponding to individual
 *	images. 
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Forces windows to be created.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
ImageCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window for application. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    TImageMaster *timPtr = (TImageMaster *) clientData;
    int x, y, width, height;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], "option ?arg arg ...?", (char *) NULL);
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "changed") == 0) {
	if (argc != 8) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " changed x y width height imageWidth imageHeight",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if ((Tcl_GetInt(interp, argv[2], &x) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[3], &y) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[4], &width) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[5], &height) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[6], &timPtr->width) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[7], &timPtr->height) != TCL_OK)) {
	    return TCL_ERROR;
	}
	Tk_ImageChanged(timPtr->master, x, y, width, height, timPtr->width,
		timPtr->height);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be changed", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ImageGet --
 *
 *	This procedure is called by Tk to set things up for using a
 *	test image in a particular widget.
 *
 * Results:
 *	The return value is a token for the image instance, which is
 *	used in future callbacks to ImageDisplay and ImageFree.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static ClientData
ImageGet(tkwin, clientData)
    Tk_Window tkwin;		/* Token for window in which image will
				 * be used. */
    ClientData clientData;	/* Pointer to TImageMaster for image. */
{
    TImageMaster *timPtr = (TImageMaster *) clientData;
    TImageInstance *instPtr;
    char buffer[100];
    XGCValues gcValues;

    sprintf(buffer, "%s get", timPtr->imageName);
    Tcl_SetVar(timPtr->interp, timPtr->varName, buffer,
	    TCL_GLOBAL_ONLY|TCL_APPEND_VALUE|TCL_LIST_ELEMENT);

    instPtr = (TImageInstance *) ckalloc(sizeof(TImageInstance));
    instPtr->masterPtr = timPtr;
    instPtr->fg = Tk_GetColor(timPtr->interp, tkwin, "#ff0000");
    gcValues.foreground = instPtr->fg->pixel;
    instPtr->gc = Tk_GetGC(tkwin, GCForeground, &gcValues);
    return (ClientData) instPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ImageDisplay --
 *
 *	This procedure is invoked to redisplay part or all of an
 *	image in a given drawable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The image gets partially redrawn, as an "X" that shows the
 *	exact redraw area.
 *
 *----------------------------------------------------------------------
 */

static void
ImageDisplay(clientData, display, drawable, imageX, imageY, width, height,
	drawableX, drawableY)
    ClientData clientData;	/* Pointer to TImageInstance for image. */
    Display *display;		/* Display to use for drawing. */
    Drawable drawable;		/* Where to redraw image. */
    int imageX, imageY;		/* Origin of area to redraw, relative to
				 * origin of image. */
    int width, height;		/* Dimensions of area to redraw. */
    int drawableX, drawableY;	/* Coordinates in drawable corresponding to
				 * imageX and imageY. */
{
    TImageInstance *instPtr = (TImageInstance *) clientData;
    char buffer[200];

    sprintf(buffer, "%s display %d %d %d %d %d %d",
	    instPtr->masterPtr->imageName, imageX, imageY, width, height,
	    drawableX, drawableY);
    Tcl_SetVar(instPtr->masterPtr->interp, instPtr->masterPtr->varName, buffer,
	    TCL_GLOBAL_ONLY|TCL_APPEND_VALUE|TCL_LIST_ELEMENT);
    if (width > (instPtr->masterPtr->width - imageX)) {
	width = instPtr->masterPtr->width - imageX;
    }
    if (height > (instPtr->masterPtr->height - imageY)) {
	height = instPtr->masterPtr->height - imageY;
    }
    XDrawRectangle(display, drawable, instPtr->gc, drawableX, drawableY,
	    (unsigned) (width-1), (unsigned) (height-1));
    XDrawLine(display, drawable, instPtr->gc, drawableX, drawableY,
	    (int) (drawableX + width - 1), (int) (drawableY + height - 1));
    XDrawLine(display, drawable, instPtr->gc, drawableX,
	    (int) (drawableY + height - 1),
	    (int) (drawableX + width - 1), drawableY);
}

/*
 *----------------------------------------------------------------------
 *
 * ImageFree --
 *
 *	This procedure is called when an instance of an image is
 * 	no longer used.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information related to the instance is freed.
 *
 *----------------------------------------------------------------------
 */

static void
ImageFree(clientData, display)
    ClientData clientData;	/* Pointer to TImageInstance for instance. */
    Display *display;		/* Display where image was to be drawn. */
{
    TImageInstance *instPtr = (TImageInstance *) clientData;
    char buffer[200];

    sprintf(buffer, "%s free", instPtr->masterPtr->imageName);
    Tcl_SetVar(instPtr->masterPtr->interp, instPtr->masterPtr->varName, buffer,
	    TCL_GLOBAL_ONLY|TCL_APPEND_VALUE|TCL_LIST_ELEMENT);
    Tk_FreeColor(instPtr->fg);
    Tk_FreeGC(display, instPtr->gc);
    ckfree((char *) instPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ImageDelete --
 *
 *	This procedure is called to clean up a test image when
 *	an application goes away.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information about the image is deleted.
 *
 *----------------------------------------------------------------------
 */

static void
ImageDelete(clientData)
    ClientData clientData;	/* Pointer to TImageMaster for image.  When
				 * this procedure is called, no more
				 * instances exist. */
{
    TImageMaster *timPtr = (TImageMaster *) clientData;
    char buffer[100];

    sprintf(buffer, "%s delete", timPtr->imageName);
    Tcl_SetVar(timPtr->interp, timPtr->varName, buffer,
	    TCL_GLOBAL_ONLY|TCL_APPEND_VALUE|TCL_LIST_ELEMENT);

    Tcl_DeleteCommand(timPtr->interp, timPtr->imageName);
    ckfree(timPtr->imageName);
    ckfree(timPtr->varName);
    ckfree((char *) timPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TestmakeexistCmd --
 *
 *	This procedure implements the "testmakeexist" command.  It calls
 *	Tk_MakeWindowExist on each of its arguments to force the windows
 *	to be created.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Forces windows to be created.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestmakeexistCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window for application. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tk_Window main = (Tk_Window) clientData;
    int i;
    Tk_Window tkwin;

    for (i = 1; i < argc; i++) {
	tkwin = Tk_NameToWindow(interp, argv[i], main);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	Tk_MakeWindowExist(tkwin);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestmenubarCmd --
 *
 *	This procedure implements the "testmenubar" command.  It is used
 *	to test the Unix facilities for creating space above a toplevel
 *	window for a menubar.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Changes menubar related stuff.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestmenubarCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window for application. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
#ifdef __UNIX__
    Tk_Window main = (Tk_Window) clientData;
    Tk_Window tkwin, menubar;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args;  must be \"", argv[0],
		" option ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }

    if (strcmp(argv[1], "window") == 0) {
	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args;  must be \"", argv[0],
		    "window toplevel menubar\"", (char *) NULL);
	    return TCL_ERROR;
	}
	tkwin = Tk_NameToWindow(interp, argv[2], main);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	if (argv[3][0] == 0) {
	    TkUnixSetMenubar(tkwin, NULL);
	} else {
	    menubar = Tk_NameToWindow(interp, argv[3], main);
	    if (menubar == NULL) {
		return TCL_ERROR;
	    }
	    TkUnixSetMenubar(tkwin, menubar);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be  window", (char *) NULL);
	return TCL_ERROR;
    }

    return TCL_OK;
#else
    interp->result = "testmenubar is supported only under Unix";
    return TCL_ERROR;
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * TestmetricsCmd --
 *
 *	This procedure implements the testmetrics command. It provides
 *	a way to determine the size of various widget components.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifdef __WIN32__
static int
TestmetricsCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window for application. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    char buf[200];

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args;  must be \"", argv[0],
		" option ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }

    if (strcmp(argv[1], "cyvscroll") == 0) {
	sprintf(buf, "%d", GetSystemMetrics(SM_CYVSCROLL));
	Tcl_AppendResult(interp, buf, (char *) NULL);
    } else  if (strcmp(argv[1], "cxhscroll") == 0) {
	sprintf(buf, "%d", GetSystemMetrics(SM_CXHSCROLL));
	Tcl_AppendResult(interp, buf, (char *) NULL);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be cxhscroll or cyvscroll", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}
#endif
#ifdef MAC_TCL
static int
TestmetricsCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window for application. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    TkWindow *winPtr;
    char buf[200];

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # args;  must be \"", argv[0],
		" option window\"", (char *) NULL);
	return TCL_ERROR;
    }

    winPtr = (TkWindow *) Tk_NameToWindow(interp, argv[2], tkwin);
    if (winPtr == NULL) {
	return TCL_ERROR;
    }
    
    if (strcmp(argv[1], "cyvscroll") == 0) {
	sprintf(buf, "%d", ((TkScrollbar *) winPtr->instanceData)->width);
	Tcl_AppendResult(interp, buf, (char *) NULL);
    } else  if (strcmp(argv[1], "cxhscroll") == 0) {
	sprintf(buf, "%d", ((TkScrollbar *) winPtr->instanceData)->width);
	Tcl_AppendResult(interp, buf, (char *) NULL);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be cxhscroll or cyvscroll", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TestpropCmd --
 *
 *	This procedure implements the "testprop" command.  It fetches
 *	and prints the value of a property on a window.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestpropCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window for application. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tk_Window main = (Tk_Window) clientData;
    int result, actualFormat;
    unsigned long bytesAfter, length, value;
    Atom actualType, propName;
    char *property, *p, *end;
    Window w;
    char buffer[30];

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # args;  must be \"", argv[0],
		" window property\"", (char *) NULL);
	return TCL_ERROR;
    }

    w = strtoul(argv[1], &end, 0);
    propName = Tk_InternAtom(main, argv[2]);
    property = NULL;
    result = XGetWindowProperty(Tk_Display(main),
	    w, propName, 0, 100000, False, AnyPropertyType,
	    &actualType, &actualFormat, &length,
	    &bytesAfter, (unsigned char **) &property);
    if ((result == Success) && (actualType != None)) {
	if ((actualFormat == 8) && (actualType == XA_STRING)) {
	    for (p = property; ((unsigned long)(p-property)) < length; p++) {
		if (*p == 0) {
		    *p = '\n';
		}
	    }
	    Tcl_SetResult(interp, property, TCL_VOLATILE);
	} else {
	    for (p = property; length > 0; length--) {
		if (actualFormat == 32) {
		    value = *((long *) p);
		    p += sizeof(long);
		} else if (actualFormat == 16) {
		    value = 0xffff & (*((short *) p));
		    p += sizeof(short);
		} else {
		    value = 0xff & *p;
		    p += 1;
		}
		sprintf(buffer, "0x%lx", value);
		Tcl_AppendElement(interp, buffer);
	    }
	}
    }
    if (property != NULL) {
	XFree(property);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestsendCmd --
 *
 *	This procedure implements the "testsend" command.  It provides
 *	a set of functions for testing the "send" command and support
 *	procedure in tkSend.c.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Depends on option;  see below.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestsendCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window for application. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    TkWindow *winPtr = (TkWindow *) clientData;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args;  must be \"", argv[0],
		" option ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }

#if !(defined(__WIN32__) || defined(MAC_TCL))
    if (strcmp(argv[1], "bogus") == 0) {
	XChangeProperty(winPtr->dispPtr->display,
		RootWindow(winPtr->dispPtr->display, 0),
		winPtr->dispPtr->registryProperty, XA_INTEGER, 32,
		PropModeReplace,
		(unsigned char *) "This is bogus information", 6);
    } else if (strcmp(argv[1], "prop") == 0) {
	int result, actualFormat;
	unsigned long length, bytesAfter;
	Atom actualType, propName;
	char *property, *p, *end;
	Window w;

	if ((argc != 4) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # args;  must be \"", argv[0],
		    " prop window name ?value ?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (strcmp(argv[2], "root") == 0) {
	    w = RootWindow(winPtr->dispPtr->display, 0);
	} else if (strcmp(argv[2], "comm") == 0) {
	    w = Tk_WindowId(winPtr->dispPtr->commTkwin);
	} else {
	    w = strtoul(argv[2], &end, 0);
	}
	propName = Tk_InternAtom((Tk_Window) winPtr, argv[3]);
	if (argc == 4) {
	    property = NULL;
	    result = XGetWindowProperty(winPtr->dispPtr->display,
		    w, propName, 0, 100000, False, XA_STRING,
		    &actualType, &actualFormat, &length,
		    &bytesAfter, (unsigned char **) &property);
	    if ((result == Success) && (actualType != None)
		    && (actualFormat == 8) && (actualType == XA_STRING)) {
		for (p = property; (p-property) < length; p++) {
		    if (*p == 0) {
			*p = '\n';
		    }
		}
		Tcl_SetResult(interp, property, TCL_VOLATILE);
	    }
	    if (property != NULL) {
		XFree(property);
	    }
	} else {
	    if (argv[4][0] == 0) {
		XDeleteProperty(winPtr->dispPtr->display, w, propName);
	    } else {
		for (p = argv[4]; *p != 0; p++) {
		    if (*p == '\n') {
			*p = 0;
		    }
		}
		XChangeProperty(winPtr->dispPtr->display,
			w, propName, XA_STRING, 8, PropModeReplace,
			(unsigned char *) argv[4], p-argv[4]);
	    }
	}
    } else if (strcmp(argv[1], "serial") == 0) {
	sprintf(interp->result, "%d", tkSendSerial+1);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be bogus, prop, or serial", (char *) NULL);
	return TCL_ERROR;
    }
#endif
    return TCL_OK;
}

#if !(defined(__WIN32__) || defined(MAC_TCL))
/*
 *----------------------------------------------------------------------
 *
 * TestwrapperCmd --
 *
 *	This procedure implements the "testwrapper" command.  It 
 *	provides a way from Tcl to determine the extra window Tk adds
 *	in between the toplevel window and the window decorations.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestwrapperCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window for application. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    TkWindow *winPtr, *wrapperPtr;
    Tk_Window tkwin;

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args;  must be \"", argv[0],
		" window\"", (char *) NULL);
	return TCL_ERROR;
    }
    
    tkwin = (Tk_Window) clientData;
    winPtr = (TkWindow *) Tk_NameToWindow(interp, argv[1], tkwin);
    if (winPtr == NULL) {
	return TCL_ERROR;
    }

    wrapperPtr = TkpGetWrapperWindow(winPtr);
    if (wrapperPtr != NULL) {
	TkpPrintWindowId(interp->result, Tk_WindowId(wrapperPtr));
    }
    return TCL_OK;
}
#endif
