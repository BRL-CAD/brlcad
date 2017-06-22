/*
 * tkTest.c --
 *
 *	This file contains C command functions for a bunch of additional Tcl
 *	commands that are used for testing out Tcl's C interfaces. These
 *	commands are not normally included in Tcl applications; they're only
 *	used for testing.
 *
 * Copyright (c) 1993-1994 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 * Copyright (c) 1998-1999 by Scriptics Corporation.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkInt.h"
#include "tkText.h"

#ifdef __WIN32__
#include "tkWinInt.h"
#endif

#if defined(MAC_OSX_TK)
#include "tkMacOSXInt.h"
#include "tkScrollbar.h"
#endif

#ifdef __UNIX__
#include "tkUnixInt.h"
#endif

/*
 * The following data structure represents the master for a test image:
 */

typedef struct TImageMaster {
    Tk_ImageMaster master;	/* Tk's token for image master. */
    Tcl_Interp *interp;		/* Interpreter for application. */
    int width, height;		/* Dimensions of image. */
    char *imageName;		/* Name of image (malloc-ed). */
    char *varName;		/* Name of variable in which to log events for
				 * image (malloc-ed). */
} TImageMaster;

/*
 * The following data structure represents a particular use of a particular
 * test image.
 */

typedef struct TImageInstance {
    TImageMaster *masterPtr;	/* Pointer to master for image. */
    XColor *fg;			/* Foreground color for drawing in image. */
    GC gc;			/* Graphics context for drawing in image. */
} TImageInstance;

/*
 * The type record for test images:
 */

static int		ImageCreate(Tcl_Interp *interp,
			    char *name, int argc, Tcl_Obj *const objv[],
			    Tk_ImageType *typePtr, Tk_ImageMaster master,
			    ClientData *clientDataPtr);
static ClientData	ImageGet(Tk_Window tkwin, ClientData clientData);
static void		ImageDisplay(ClientData clientData,
			    Display *display, Drawable drawable,
			    int imageX, int imageY, int width,
			    int height, int drawableX,
			    int drawableY);
static void		ImageFree(ClientData clientData, Display *display);
static void		ImageDelete(ClientData clientData);

static Tk_ImageType imageType = {
    "test",			/* name */
    ImageCreate,		/* createProc */
    ImageGet,			/* getProc */
    ImageDisplay,		/* displayProc */
    ImageFree,			/* freeProc */
    ImageDelete,		/* deleteProc */
    NULL,			/* postscriptPtr */
    NULL			/* nextPtr */
};

/*
 * One of the following structures describes each of the interpreters created
 * by the "testnewapp" command. This information is used by the
 * "testdeleteinterps" command to destroy all of those interpreters.
 */

typedef struct NewApp {
    Tcl_Interp *interp;		/* Token for interpreter. */
    struct NewApp *nextPtr;	/* Next in list of new interpreters. */
} NewApp;

static NewApp *newAppPtr = NULL;/* First in list of all new interpreters. */

/*
 * Declaration for the square widget's class command function:
 */

extern int		SquareObjCmd(ClientData clientData,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj * const objv[]);

typedef struct CBinding {
    Tcl_Interp *interp;
    char *command;
    char *delete;
} CBinding;

/*
 * Header for trivial configuration command items.
 */

#define ODD TK_CONFIG_USER_BIT
#define EVEN (TK_CONFIG_USER_BIT << 1)

enum {
    NONE,
    ODD_TYPE,
    EVEN_TYPE
};

typedef struct TrivialCommandHeader {
    Tcl_Interp *interp;		/* The interp that this command lives in. */
    Tk_OptionTable optionTable;	/* The option table that go with this
				 * command. */
    Tk_Window tkwin;		/* For widgets, the window associated with
				 * this widget. */
    Tcl_Command widgetCmd;	/* For widgets, the command associated with
				 * this widget. */
} TrivialCommandHeader;

/*
 * Forward declarations for functions defined later in this file:
 */

static int		CBindingEvalProc(ClientData clientData,
			    Tcl_Interp *interp, XEvent *eventPtr,
			    Tk_Window tkwin, KeySym keySym);
static void		CBindingFreeProc(ClientData clientData);
int			Tktest_Init(Tcl_Interp *interp);
static int		ImageCmd(ClientData dummy,
			    Tcl_Interp *interp, int argc, const char **argv);
static int		TestcbindCmd(ClientData dummy,
			    Tcl_Interp *interp, int argc, const char **argv);
static int		TestbitmapObjCmd(ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj * const objv[]);
static int		TestborderObjCmd(ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj * const objv[]);
static int		TestcolorObjCmd(ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj * const objv[]);
static int		TestcursorObjCmd(ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj * const objv[]);
static int		TestdeleteappsCmd(ClientData dummy,
			    Tcl_Interp *interp, int argc, const char **argv);
static int		TestfontObjCmd(ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj *const objv[]);
static int		TestmakeexistCmd(ClientData dummy,
			    Tcl_Interp *interp, int argc, const char **argv);
#if !(defined(__WIN32__) || defined(MAC_OSX_TK) || defined(__CYGWIN__))
static int		TestmenubarCmd(ClientData dummy,
			    Tcl_Interp *interp, int argc, const char **argv);
#endif
#if defined(__WIN32__) || defined(MAC_OSX_TK)
static int		TestmetricsCmd(ClientData dummy,
			    Tcl_Interp *interp, int argc, const char **argv);
#endif
static int		TestobjconfigObjCmd(ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj * const objv[]);
static int		CustomOptionSet(ClientData clientData,
			    Tcl_Interp *interp, Tk_Window tkwin,
			    Tcl_Obj **value, char *recordPtr,
			    int internalOffset, char *saveInternalPtr,
			    int flags);
static Tcl_Obj *	CustomOptionGet(ClientData clientData,
			    Tk_Window tkwin, char *recordPtr,
			    int internalOffset);
static void		CustomOptionRestore(ClientData clientData,
			    Tk_Window tkwin, char *internalPtr,
			    char *saveInternalPtr);
static void		CustomOptionFree(ClientData clientData,
			    Tk_Window tkwin, char *internalPtr);
static int		TestpropCmd(ClientData dummy,
			    Tcl_Interp *interp, int argc, const char **argv);
#if !(defined(__WIN32__) || defined(MAC_OSX_TK) || defined(__CYGWIN__))
static int		TestwrapperCmd(ClientData dummy,
			    Tcl_Interp *interp, int argc, const char **argv);
#endif
static void		TrivialCmdDeletedProc(ClientData clientData);
static int		TrivialConfigObjCmd(ClientData dummy,
			    Tcl_Interp *interp, int objc,
			    Tcl_Obj * const objv[]);
static void		TrivialEventProc(ClientData clientData,
			    XEvent *eventPtr);

/*
 * External (platform specific) initialization routine:
 */

#if !(defined(__WIN32__) || defined(MAC_OSX_TK))
#define TkplatformtestInit(x) TCL_OK
#else
MODULE_SCOPE int	TkplatformtestInit(Tcl_Interp *interp);
#endif

/*
 * External legacy testing initialization routine:
 */
MODULE_SCOPE int	TkOldTestInit(Tcl_Interp *interp);

/*
 *----------------------------------------------------------------------
 *
 * Tktest_Init --
 *
 *	This function performs intialization for the Tk test suite exensions.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error message in
 *	the interp's result if an error occurs.
 *
 * Side effects:
 *	Creates several test commands.
 *
 *----------------------------------------------------------------------
 */

int
Tktest_Init(
    Tcl_Interp *interp)		/* Interpreter for application. */
{
    static int initialized = 0;

    /*
     * Create additional commands for testing Tk.
     */

    if (Tcl_PkgProvide(interp, "Tktest", TK_VERSION) == TCL_ERROR) {
        return TCL_ERROR;
    }

    Tcl_CreateObjCommand(interp, "square", SquareObjCmd,
	    (ClientData) NULL, NULL);
    Tcl_CreateCommand(interp, "testcbind", TestcbindCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);
    Tcl_CreateObjCommand(interp, "testbitmap", TestbitmapObjCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);
    Tcl_CreateObjCommand(interp, "testborder", TestborderObjCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);
    Tcl_CreateObjCommand(interp, "testcolor", TestcolorObjCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);
    Tcl_CreateObjCommand(interp, "testcursor", TestcursorObjCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);
    Tcl_CreateCommand(interp, "testdeleteapps", TestdeleteappsCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);
    Tcl_CreateCommand(interp, "testembed", TkpTestembedCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);
    Tcl_CreateObjCommand(interp, "testobjconfig", TestobjconfigObjCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);
    Tcl_CreateObjCommand(interp, "testfont", TestfontObjCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);
    Tcl_CreateCommand(interp, "testmakeexist", TestmakeexistCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);
    Tcl_CreateCommand(interp, "testprop", TestpropCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);
    Tcl_CreateCommand(interp, "testtext", TkpTesttextCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);

#if defined(__WIN32__) || defined(MAC_OSX_TK)
    Tcl_CreateCommand(interp, "testmetrics", TestmetricsCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);
#elif !defined(__CYGWIN__)
    Tcl_CreateCommand(interp, "testmenubar", TestmenubarCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);
    Tcl_CreateCommand(interp, "testsend", TkpTestsendCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);
    Tcl_CreateCommand(interp, "testwrapper", TestwrapperCmd,
	    (ClientData) Tk_MainWindow(interp), NULL);
#endif /* __WIN32__ || MAC_OSX_TK */

    /*
     * Create test image type.
     */

    if (!initialized) {
	initialized = 1;
	Tk_CreateImageType(&imageType);
    }

    /*
     *	Enable testing of legacy interfaces.
     */

    if (TkOldTestInit(interp) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * And finally add any platform specific test commands.
     */

    return TkplatformtestInit(interp);
}

/*
 *----------------------------------------------------------------------
 *
 * TestcbindCmd --
 *
 *	This function implements the "testcbinding" command. It provides a set
 *	of functions for testing C bindings in tkBind.c.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Depends on option; see below.
 *
 *----------------------------------------------------------------------
 */

static int
TestcbindCmd(
    ClientData clientData,	/* Main window for application. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    TkWindow *winPtr;
    Tk_Window tkwin;
    ClientData object;
    CBinding *cbindPtr;


    if (argc < 4 || argc > 5) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" bindtag pattern command ?deletecommand?", NULL);
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
CBindingEvalProc(
    ClientData clientData,
    Tcl_Interp *interp,
    XEvent *eventPtr,
    Tk_Window tkwin,
    KeySym keySym)
{
    CBinding *cbindPtr;

    cbindPtr = (CBinding *) clientData;

    return Tcl_EvalEx(interp, cbindPtr->command, -1, TCL_EVAL_GLOBAL);
}

static void
CBindingFreeProc(
    ClientData clientData)
{
    CBinding *cbindPtr = (CBinding *) clientData;

    if (cbindPtr->delete != NULL) {
	Tcl_EvalEx(cbindPtr->interp, cbindPtr->delete, -1, TCL_EVAL_GLOBAL);
	ckfree((char *) cbindPtr->delete);
    }
    ckfree((char *) cbindPtr->command);
    ckfree((char *) cbindPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TestbitmapObjCmd --
 *
 *	This function implements the "testbitmap" command, which is used to
 *	test color resource handling in tkBitmap tmp.c.
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
TestbitmapObjCmd(
    ClientData clientData,	/* Main window for application. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "bitmap");
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, TkDebugBitmap(Tk_MainWindow(interp),
	    Tcl_GetString(objv[1])));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestborderObjCmd --
 *
 *	This function implements the "testborder" command, which is used to
 *	test color resource handling in tkBorder.c.
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
TestborderObjCmd(
    ClientData clientData,	/* Main window for application. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "border");
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, TkDebugBorder(Tk_MainWindow(interp),
	    Tcl_GetString(objv[1])));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestcolorObjCmd --
 *
 *	This function implements the "testcolor" command, which is used to
 *	test color resource handling in tkColor.c.
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
TestcolorObjCmd(
    ClientData clientData,	/* Main window for application. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "color");
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, TkDebugColor(Tk_MainWindow(interp),
	    Tcl_GetString(objv[1])));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestcursorObjCmd --
 *
 *	This function implements the "testcursor" command, which is used to
 *	test color resource handling in tkCursor.c.
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
TestcursorObjCmd(
    ClientData clientData,	/* Main window for application. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "cursor");
	return TCL_ERROR;
    }
    Tcl_SetObjResult(interp, TkDebugCursor(Tk_MainWindow(interp),
	    Tcl_GetString(objv[1])));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TestdeleteappsCmd --
 *
 *	This function implements the "testdeleteapps" command. It cleans up
 *	all the interpreters left behind by the "testnewapp" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	All the intepreters created by previous calls to "testnewapp" get
 *	deleted.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TestdeleteappsCmd(
    ClientData clientData,	/* Main window for application. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
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
 * TestobjconfigObjCmd --
 *
 *	This function implements the "testobjconfig" command, which is used to
 *	test the functions in tkConfig.c.
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
TestobjconfigObjCmd(
    ClientData clientData,	/* Main window for application. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    static const char *options[] = {
	"alltypes", "chain1", "chain2", "configerror", "delete", "info",
	"internal", "new", "notenoughparams", "twowindows", NULL
    };
    enum {
	ALL_TYPES, CHAIN1, CHAIN2, CONFIG_ERROR,
	DEL,			/* Can't use DELETE: VC++ compiler barfs. */
	INFO, INTERNAL, NEW, NOT_ENOUGH_PARAMS, TWO_WINDOWS
    };
    static Tk_OptionTable tables[11];
				/* Holds pointers to option tables created by
				 * commands below; indexed with same values as
				 * "options" array. */
    static const Tk_ObjCustomOption CustomOption = {
	"custom option",
	CustomOptionSet,
	CustomOptionGet,
	CustomOptionRestore,
	CustomOptionFree,
	(ClientData) 1
    };
    Tk_Window mainWin = (Tk_Window) clientData;
    Tk_Window tkwin;
    int index, result = TCL_OK;

    /*
     * Structures used by the "chain1" subcommand and also shared by the
     * "chain2" subcommand:
     */

    typedef struct ExtensionWidgetRecord {
	TrivialCommandHeader header;
	Tcl_Obj *base1ObjPtr;
	Tcl_Obj *base2ObjPtr;
	Tcl_Obj *extension3ObjPtr;
	Tcl_Obj *extension4ObjPtr;
	Tcl_Obj *extension5ObjPtr;
    } ExtensionWidgetRecord;
    static const Tk_OptionSpec baseSpecs[] = {
	{TK_OPTION_STRING, "-one", "one", "One", "one",
		Tk_Offset(ExtensionWidgetRecord, base1ObjPtr), -1},
	{TK_OPTION_STRING, "-two", "two", "Two", "two",
		Tk_Offset(ExtensionWidgetRecord, base2ObjPtr), -1},
	{TK_OPTION_END}
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "command");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], options, "command", 0, &index)
	    != TCL_OK) {
	return TCL_ERROR;
    }

    switch (index) {
    case ALL_TYPES: {
	typedef struct TypesRecord {
	    TrivialCommandHeader header;
	    Tcl_Obj *booleanPtr;
	    Tcl_Obj *integerPtr;
	    Tcl_Obj *doublePtr;
	    Tcl_Obj *stringPtr;
	    Tcl_Obj *stringTablePtr;
	    Tcl_Obj *colorPtr;
	    Tcl_Obj *fontPtr;
	    Tcl_Obj *bitmapPtr;
	    Tcl_Obj *borderPtr;
	    Tcl_Obj *reliefPtr;
	    Tcl_Obj *cursorPtr;
	    Tcl_Obj *activeCursorPtr;
	    Tcl_Obj *justifyPtr;
	    Tcl_Obj *anchorPtr;
	    Tcl_Obj *pixelPtr;
	    Tcl_Obj *mmPtr;
	    Tcl_Obj *customPtr;
	} TypesRecord;
	TypesRecord *recordPtr;
	static const char *stringTable[] = {
	    "one", "two", "three", "four", NULL
	};
	static const Tk_OptionSpec typesSpecs[] = {
	    {TK_OPTION_BOOLEAN, "-boolean", "boolean", "Boolean", "1",
		Tk_Offset(TypesRecord, booleanPtr), -1, 0, 0, 0x1},
	    {TK_OPTION_INT, "-integer", "integer", "Integer", "7",
		Tk_Offset(TypesRecord, integerPtr), -1, 0, 0, 0x2},
	    {TK_OPTION_DOUBLE, "-double", "double", "Double", "3.14159",
		Tk_Offset(TypesRecord, doublePtr), -1, 0, 0, 0x4},
	    {TK_OPTION_STRING, "-string", "string", "String",
		"foo", Tk_Offset(TypesRecord, stringPtr), -1,
		TK_CONFIG_NULL_OK, 0, 0x8},
	    {TK_OPTION_STRING_TABLE,
		"-stringtable", "StringTable", "stringTable",
		"one", Tk_Offset(TypesRecord, stringTablePtr), -1,
		TK_CONFIG_NULL_OK, (ClientData) stringTable, 0x10},
	    {TK_OPTION_COLOR, "-color", "color", "Color",
		"red", Tk_Offset(TypesRecord, colorPtr), -1,
		TK_CONFIG_NULL_OK, (ClientData) "black", 0x20},
	    {TK_OPTION_FONT, "-font", "font", "Font", "Helvetica 12",
		Tk_Offset(TypesRecord, fontPtr), -1,
		TK_CONFIG_NULL_OK, 0, 0x40},
	    {TK_OPTION_BITMAP, "-bitmap", "bitmap", "Bitmap", "gray50",
		Tk_Offset(TypesRecord, bitmapPtr), -1,
		TK_CONFIG_NULL_OK, 0, 0x80},
	    {TK_OPTION_BORDER, "-border", "border", "Border",
		"blue", Tk_Offset(TypesRecord, borderPtr), -1,
		TK_CONFIG_NULL_OK, (ClientData) "white", 0x100},
	    {TK_OPTION_RELIEF, "-relief", "relief", "Relief", "raised",
		Tk_Offset(TypesRecord, reliefPtr), -1,
		TK_CONFIG_NULL_OK, 0, 0x200},
	    {TK_OPTION_CURSOR, "-cursor", "cursor", "Cursor", "xterm",
		Tk_Offset(TypesRecord, cursorPtr), -1,
		TK_CONFIG_NULL_OK, 0, 0x400},
	    {TK_OPTION_JUSTIFY, "-justify", NULL, NULL, "left",
		Tk_Offset(TypesRecord, justifyPtr), -1,
		TK_CONFIG_NULL_OK, 0, 0x800},
	    {TK_OPTION_ANCHOR, "-anchor", "anchor", "Anchor", NULL,
		Tk_Offset(TypesRecord, anchorPtr), -1,
		TK_CONFIG_NULL_OK, 0, 0x1000},
	    {TK_OPTION_PIXELS, "-pixel", "pixel", "Pixel",
		"1", Tk_Offset(TypesRecord, pixelPtr), -1,
		TK_CONFIG_NULL_OK, 0, 0x2000},
	    {TK_OPTION_CUSTOM, "-custom", NULL, NULL,
		"", Tk_Offset(TypesRecord, customPtr), -1,
		TK_CONFIG_NULL_OK, (ClientData)&CustomOption, 0x4000},
	    {TK_OPTION_SYNONYM, "-synonym", NULL, NULL,
		NULL, 0, -1, 0, (ClientData) "-color", 0x8000},
	    {TK_OPTION_END}
	};
	Tk_OptionTable optionTable;
	Tk_Window tkwin;

	optionTable = Tk_CreateOptionTable(interp, typesSpecs);
	tables[index] = optionTable;
	tkwin = Tk_CreateWindowFromPath(interp, (Tk_Window) clientData,
		Tcl_GetString(objv[2]), NULL);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	Tk_SetClass(tkwin, "Test");

	recordPtr = (TypesRecord *) ckalloc(sizeof(TypesRecord));
	recordPtr->header.interp = interp;
	recordPtr->header.optionTable = optionTable;
	recordPtr->header.tkwin = tkwin;
	recordPtr->booleanPtr = NULL;
	recordPtr->integerPtr = NULL;
	recordPtr->doublePtr = NULL;
	recordPtr->stringPtr = NULL;
	recordPtr->colorPtr = NULL;
	recordPtr->fontPtr = NULL;
	recordPtr->bitmapPtr = NULL;
	recordPtr->borderPtr = NULL;
	recordPtr->reliefPtr = NULL;
	recordPtr->cursorPtr = NULL;
	recordPtr->justifyPtr = NULL;
	recordPtr->anchorPtr = NULL;
	recordPtr->pixelPtr = NULL;
	recordPtr->mmPtr = NULL;
	recordPtr->stringTablePtr = NULL;
	recordPtr->customPtr = NULL;
	result = Tk_InitOptions(interp, (char *) recordPtr, optionTable,
		tkwin);
	if (result == TCL_OK) {
	    recordPtr->header.widgetCmd = Tcl_CreateObjCommand(interp,
		    Tcl_GetString(objv[2]), TrivialConfigObjCmd,
		    (ClientData) recordPtr, TrivialCmdDeletedProc);
	    Tk_CreateEventHandler(tkwin, StructureNotifyMask,
		    TrivialEventProc, (ClientData) recordPtr);
	    result = Tk_SetOptions(interp, (char *) recordPtr, optionTable,
		    objc-3, objv+3, tkwin, NULL, NULL);
	    if (result != TCL_OK) {
		Tk_DestroyWindow(tkwin);
	    }
	} else {
	    Tk_DestroyWindow(tkwin);
	    ckfree((char *) recordPtr);
	}
	if (result == TCL_OK) {
	    Tcl_SetObjResult(interp, objv[2]);
	}
	break;
    }

    case CHAIN1: {
	ExtensionWidgetRecord *recordPtr;
	Tk_Window tkwin;
	Tk_OptionTable optionTable;

	tkwin = Tk_CreateWindowFromPath(interp, (Tk_Window) clientData,
		Tcl_GetString(objv[2]), NULL);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	Tk_SetClass(tkwin, "Test");
	optionTable = Tk_CreateOptionTable(interp, baseSpecs);
	tables[index] = optionTable;

	recordPtr = (ExtensionWidgetRecord *)
		ckalloc(sizeof(ExtensionWidgetRecord));
	recordPtr->header.interp = interp;
	recordPtr->header.optionTable = optionTable;
	recordPtr->header.tkwin = tkwin;
	recordPtr->base1ObjPtr = recordPtr->base2ObjPtr = NULL;
	recordPtr->extension3ObjPtr = recordPtr->extension4ObjPtr = NULL;
	result = Tk_InitOptions(interp, (char *)recordPtr, optionTable, tkwin);
	if (result == TCL_OK) {
	    result = Tk_SetOptions(interp, (char *) recordPtr, optionTable,
		    objc-3, objv+3, tkwin, NULL, NULL);
	    if (result != TCL_OK) {
		Tk_FreeConfigOptions((char *) recordPtr, optionTable, tkwin);
	    }
	}
	if (result == TCL_OK) {
	    recordPtr->header.widgetCmd = Tcl_CreateObjCommand(interp,
		    Tcl_GetString(objv[2]), TrivialConfigObjCmd,
		    (ClientData) recordPtr, TrivialCmdDeletedProc);
	    Tk_CreateEventHandler(tkwin, StructureNotifyMask,
		    TrivialEventProc, (ClientData) recordPtr);
	    Tcl_SetObjResult(interp, objv[2]);
	}
	break;
    }

    case CHAIN2: {
	ExtensionWidgetRecord *recordPtr;
	static const Tk_OptionSpec extensionSpecs[] = {
	    {TK_OPTION_STRING, "-three", "three", "Three", "three",
		Tk_Offset(ExtensionWidgetRecord, extension3ObjPtr), -1},
	    {TK_OPTION_STRING, "-four", "four", "Four", "four",
		Tk_Offset(ExtensionWidgetRecord, extension4ObjPtr), -1},
	    {TK_OPTION_STRING, "-two", "two", "Two", "two and a half",
		Tk_Offset(ExtensionWidgetRecord, base2ObjPtr), -1},
	    {TK_OPTION_STRING,
		"-oneAgain", "oneAgain", "OneAgain", "one again",
		Tk_Offset(ExtensionWidgetRecord, extension5ObjPtr), -1},
	    {TK_OPTION_END, NULL, NULL, NULL, NULL, 0, -1, 0,
		(ClientData) baseSpecs}
	};
	Tk_Window tkwin;
	Tk_OptionTable optionTable;

	tkwin = Tk_CreateWindowFromPath(interp, (Tk_Window) clientData,
		Tcl_GetString(objv[2]), NULL);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	Tk_SetClass(tkwin, "Test");
	optionTable = Tk_CreateOptionTable(interp, extensionSpecs);
	tables[index] = optionTable;

	recordPtr = (ExtensionWidgetRecord *) ckalloc(
		sizeof(ExtensionWidgetRecord));
	recordPtr->header.interp = interp;
	recordPtr->header.optionTable = optionTable;
	recordPtr->header.tkwin = tkwin;
	recordPtr->base1ObjPtr = recordPtr->base2ObjPtr = NULL;
	recordPtr->extension3ObjPtr = recordPtr->extension4ObjPtr = NULL;
	recordPtr->extension5ObjPtr = NULL;
	result = Tk_InitOptions(interp, (char *)recordPtr, optionTable, tkwin);
	if (result == TCL_OK) {
	    result = Tk_SetOptions(interp, (char *) recordPtr, optionTable,
		    objc-3, objv+3, tkwin, NULL, NULL);
	    if (result != TCL_OK) {
		Tk_FreeConfigOptions((char *) recordPtr, optionTable, tkwin);
	    }
	}
	if (result == TCL_OK) {
	    recordPtr->header.widgetCmd = Tcl_CreateObjCommand(interp,
		    Tcl_GetString(objv[2]), TrivialConfigObjCmd,
		    (ClientData) recordPtr, TrivialCmdDeletedProc);
	    Tk_CreateEventHandler(tkwin, StructureNotifyMask,
		    TrivialEventProc, (ClientData) recordPtr);
	    Tcl_SetObjResult(interp, objv[2]);
	}
	break;
    }

    case CONFIG_ERROR: {
	typedef struct ErrorWidgetRecord {
	    Tcl_Obj *intPtr;
	} ErrorWidgetRecord;
	ErrorWidgetRecord widgetRecord;
	static const Tk_OptionSpec errorSpecs[] = {
	    {TK_OPTION_INT, "-int", "integer", "Integer", "bogus",
		Tk_Offset(ErrorWidgetRecord, intPtr)},
	    {TK_OPTION_END}
	};
	Tk_OptionTable optionTable;

	widgetRecord.intPtr = NULL;
	optionTable = Tk_CreateOptionTable(interp, errorSpecs);
	tables[index] = optionTable;
	return Tk_InitOptions(interp, (char *) &widgetRecord, optionTable,
		(Tk_Window) NULL);
    }

    case DEL:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "tableName");
	    return TCL_ERROR;
	}
	if (Tcl_GetIndexFromObj(interp, objv[2], options, "table", 0,
		&index) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (tables[index] != NULL) {
	    Tk_DeleteOptionTable(tables[index]);
	}
	break;

    case INFO:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "tableName");
	    return TCL_ERROR;
	}
	if (Tcl_GetIndexFromObj(interp, objv[2], options, "table", 0,
		&index) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_SetObjResult(interp, TkDebugConfig(interp, tables[index]));
	break;

    case INTERNAL: {
	/*
	 * This command is similar to the "alltypes" command except that it
	 * stores all the configuration options as internal forms instead of
	 * objects.
	 */

	typedef struct InternalRecord {
	    TrivialCommandHeader header;
	    int boolean;
	    int integer;
	    double doubleValue;
	    char *string;
	    int index;
	    XColor *colorPtr;
	    Tk_Font tkfont;
	    Pixmap bitmap;
	    Tk_3DBorder border;
	    int relief;
	    Tk_Cursor cursor;
	    Tk_Justify justify;
	    Tk_Anchor anchor;
	    int pixels;
	    double mm;
	    Tk_Window tkwin;
	    char *custom;
	} InternalRecord;
	InternalRecord *recordPtr;
	static const char *const internalStringTable[] = {
	    "one", "two", "three", "four", NULL
	};
	static const Tk_OptionSpec internalSpecs[] = {
	    {TK_OPTION_BOOLEAN, "-boolean", "boolean", "Boolean", "1",
		-1, Tk_Offset(InternalRecord, boolean), 0, 0, 0x1},
	    {TK_OPTION_INT, "-integer", "integer", "Integer", "148962237",
		-1, Tk_Offset(InternalRecord, integer), 0, 0, 0x2},
	    {TK_OPTION_DOUBLE, "-double", "double", "Double", "3.14159",
		-1, Tk_Offset(InternalRecord, doubleValue), 0, 0, 0x4},
	    {TK_OPTION_STRING, "-string", "string", "String", "foo",
		-1, Tk_Offset(InternalRecord, string),
		TK_CONFIG_NULL_OK, 0, 0x8},
	    {TK_OPTION_STRING_TABLE,
		"-stringtable", "StringTable", "stringTable", "one",
		-1, Tk_Offset(InternalRecord, index),
		TK_CONFIG_NULL_OK, (ClientData) internalStringTable, 0x10},
	    {TK_OPTION_COLOR, "-color", "color", "Color", "red",
		-1, Tk_Offset(InternalRecord, colorPtr),
		TK_CONFIG_NULL_OK, (ClientData) "black", 0x20},
	    {TK_OPTION_FONT, "-font", "font", "Font", "Helvetica 12",
		-1, Tk_Offset(InternalRecord, tkfont),
		TK_CONFIG_NULL_OK, 0, 0x40},
	    {TK_OPTION_BITMAP, "-bitmap", "bitmap", "Bitmap", "gray50",
		-1, Tk_Offset(InternalRecord, bitmap),
		TK_CONFIG_NULL_OK, 0, 0x80},
	    {TK_OPTION_BORDER, "-border", "border", "Border", "blue",
		-1, Tk_Offset(InternalRecord, border),
		TK_CONFIG_NULL_OK, (ClientData) "white", 0x100},
	    {TK_OPTION_RELIEF, "-relief", "relief", "Relief", "raised",
		-1, Tk_Offset(InternalRecord, relief),
		TK_CONFIG_NULL_OK, 0, 0x200},
	    {TK_OPTION_CURSOR, "-cursor", "cursor", "Cursor", "xterm",
		-1, Tk_Offset(InternalRecord, cursor),
		TK_CONFIG_NULL_OK, 0, 0x400},
	    {TK_OPTION_JUSTIFY, "-justify", NULL, NULL, "left",
		-1, Tk_Offset(InternalRecord, justify),
		TK_CONFIG_NULL_OK, 0, 0x800},
	    {TK_OPTION_ANCHOR, "-anchor", "anchor", "Anchor", NULL,
		-1, Tk_Offset(InternalRecord, anchor),
		TK_CONFIG_NULL_OK, 0, 0x1000},
	    {TK_OPTION_PIXELS, "-pixel", "pixel", "Pixel", "1",
		-1, Tk_Offset(InternalRecord, pixels),
		TK_CONFIG_NULL_OK, 0, 0x2000},
	    {TK_OPTION_WINDOW, "-window", "window", "Window", NULL,
		-1, Tk_Offset(InternalRecord, tkwin),
		TK_CONFIG_NULL_OK, 0, 0},
	    {TK_OPTION_CUSTOM, "-custom", NULL, NULL, "",
		-1, Tk_Offset(InternalRecord, custom),
		TK_CONFIG_NULL_OK, (ClientData)&CustomOption, 0x4000},
	    {TK_OPTION_SYNONYM, "-synonym", NULL, NULL,
		NULL, -1, -1, 0, (ClientData) "-color", 0x8000},
	    {TK_OPTION_END}
	};
	Tk_OptionTable optionTable;
	Tk_Window tkwin;

	optionTable = Tk_CreateOptionTable(interp, internalSpecs);
	tables[index] = optionTable;
	tkwin = Tk_CreateWindowFromPath(interp, (Tk_Window) clientData,
		Tcl_GetString(objv[2]), NULL);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	Tk_SetClass(tkwin, "Test");

	recordPtr = (InternalRecord *) ckalloc(sizeof(InternalRecord));
	recordPtr->header.interp = interp;
	recordPtr->header.optionTable = optionTable;
	recordPtr->header.tkwin = tkwin;
	recordPtr->boolean = 0;
	recordPtr->integer = 0;
	recordPtr->doubleValue = 0.0;
	recordPtr->string = NULL;
	recordPtr->index = 0;
	recordPtr->colorPtr = NULL;
	recordPtr->tkfont = NULL;
	recordPtr->bitmap = None;
	recordPtr->border = NULL;
	recordPtr->relief = TK_RELIEF_FLAT;
	recordPtr->cursor = NULL;
	recordPtr->justify = TK_JUSTIFY_LEFT;
	recordPtr->anchor = TK_ANCHOR_N;
	recordPtr->pixels = 0;
	recordPtr->mm = 0.0;
	recordPtr->tkwin = NULL;
	recordPtr->custom = NULL;
	result = Tk_InitOptions(interp, (char *) recordPtr, optionTable,
		tkwin);
	if (result == TCL_OK) {
	    recordPtr->header.widgetCmd = Tcl_CreateObjCommand(interp,
		    Tcl_GetString(objv[2]), TrivialConfigObjCmd,
		    (ClientData) recordPtr, TrivialCmdDeletedProc);
	    Tk_CreateEventHandler(tkwin, StructureNotifyMask,
		    TrivialEventProc, (ClientData) recordPtr);
	    result = Tk_SetOptions(interp, (char *) recordPtr, optionTable,
		    objc - 3, objv + 3, tkwin, NULL, NULL);
	    if (result != TCL_OK) {
		Tk_DestroyWindow(tkwin);
	    }
	} else {
	    Tk_DestroyWindow(tkwin);
	    ckfree((char *) recordPtr);
	}
	if (result == TCL_OK) {
	    Tcl_SetObjResult(interp, objv[2]);
	}
	break;
    }

    case NEW: {
	typedef struct FiveRecord {
	    TrivialCommandHeader header;
	    Tcl_Obj *one;
	    Tcl_Obj *two;
	    Tcl_Obj *three;
	    Tcl_Obj *four;
	    Tcl_Obj *five;
	} FiveRecord;
	FiveRecord *recordPtr;
	static const Tk_OptionSpec smallSpecs[] = {
	    {TK_OPTION_INT, "-one", "one", "One", "1",
		Tk_Offset(FiveRecord, one), -1},
	    {TK_OPTION_INT, "-two", "two", "Two", "2",
		Tk_Offset(FiveRecord, two), -1},
	    {TK_OPTION_INT, "-three", "three", "Three", "3",
		Tk_Offset(FiveRecord, three), -1},
	    {TK_OPTION_INT, "-four", "four", "Four", "4",
		Tk_Offset(FiveRecord, four), -1},
	    {TK_OPTION_STRING, "-five", NULL, NULL, NULL,
		Tk_Offset(FiveRecord, five), -1},
	    {TK_OPTION_END}
	};

	if (objc < 3) {
	    Tcl_WrongNumArgs(interp, 1, objv, "new name ?options?");
	    return TCL_ERROR;
	}

	recordPtr = (FiveRecord *) ckalloc(sizeof(FiveRecord));
	recordPtr->header.interp = interp;
	recordPtr->header.optionTable = Tk_CreateOptionTable(interp,
		smallSpecs);
	tables[index] = recordPtr->header.optionTable;
	recordPtr->header.tkwin = NULL;
	recordPtr->one = recordPtr->two = recordPtr->three = NULL;
	recordPtr->four = recordPtr->five = NULL;
	Tcl_SetObjResult(interp, objv[2]);
	result = Tk_InitOptions(interp, (char *) recordPtr,
		recordPtr->header.optionTable, (Tk_Window) NULL);
	if (result == TCL_OK) {
	    result = Tk_SetOptions(interp, (char *) recordPtr,
		    recordPtr->header.optionTable, objc - 3, objv + 3,
		    (Tk_Window) NULL, NULL, NULL);
	    if (result == TCL_OK) {
		recordPtr->header.widgetCmd = Tcl_CreateObjCommand(interp,
			Tcl_GetString(objv[2]), TrivialConfigObjCmd,
			(ClientData) recordPtr, TrivialCmdDeletedProc);
	    } else {
		Tk_FreeConfigOptions((char *) recordPtr,
			recordPtr->header.optionTable, (Tk_Window) NULL);
	    }
	}
	if (result != TCL_OK) {
	    ckfree((char *) recordPtr);
	}

	break;
    }
    case NOT_ENOUGH_PARAMS: {
	typedef struct NotEnoughRecord {
	    Tcl_Obj *fooObjPtr;
	} NotEnoughRecord;
	NotEnoughRecord record;
	static const Tk_OptionSpec errorSpecs[] = {
	    {TK_OPTION_INT, "-foo", "foo", "Foo", "0",
		Tk_Offset(NotEnoughRecord, fooObjPtr)},
	    {TK_OPTION_END}
	};
	Tcl_Obj *newObjPtr = Tcl_NewStringObj("-foo", -1);
	Tk_OptionTable optionTable;

	record.fooObjPtr = NULL;

	tkwin = Tk_CreateWindowFromPath(interp, mainWin, ".config", NULL);
	Tk_SetClass(tkwin, "Config");
	optionTable = Tk_CreateOptionTable(interp, errorSpecs);
	tables[index] = optionTable;
	Tk_InitOptions(interp, (char *) &record, optionTable, tkwin);
	if (Tk_SetOptions(interp, (char *) &record, optionTable, 1,
		&newObjPtr, tkwin, NULL, NULL) != TCL_OK) {
	    result = TCL_ERROR;
	}
	Tcl_DecrRefCount(newObjPtr);
	Tk_FreeConfigOptions( (char *) &record, optionTable, tkwin);
	Tk_DestroyWindow(tkwin);
	return result;
    }

    case TWO_WINDOWS: {
	typedef struct SlaveRecord {
	    TrivialCommandHeader header;
	    Tcl_Obj *windowPtr;
	} SlaveRecord;
	SlaveRecord *recordPtr;
	static const Tk_OptionSpec slaveSpecs[] = {
	    {TK_OPTION_WINDOW, "-window", "window", "Window", ".bar",
		Tk_Offset(SlaveRecord, windowPtr), -1, TK_CONFIG_NULL_OK},
	    {TK_OPTION_END}
	};
	Tk_Window tkwin = Tk_CreateWindowFromPath(interp,
		(Tk_Window) clientData, Tcl_GetString(objv[2]), NULL);

	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	Tk_SetClass(tkwin, "Test");

	recordPtr = (SlaveRecord *) ckalloc(sizeof(SlaveRecord));
	recordPtr->header.interp = interp;
	recordPtr->header.optionTable = Tk_CreateOptionTable(interp,
		slaveSpecs);
	tables[index] = recordPtr->header.optionTable;
	recordPtr->header.tkwin = tkwin;
	recordPtr->windowPtr = NULL;

	result = Tk_InitOptions(interp,  (char *) recordPtr,
		recordPtr->header.optionTable, tkwin);
	if (result == TCL_OK) {
	    result = Tk_SetOptions(interp, (char *) recordPtr,
		    recordPtr->header.optionTable, objc - 3, objv + 3,
		    tkwin, NULL, NULL);
	    if (result == TCL_OK) {
		recordPtr->header.widgetCmd = Tcl_CreateObjCommand(interp,
			Tcl_GetString(objv[2]), TrivialConfigObjCmd,
			(ClientData) recordPtr, TrivialCmdDeletedProc);
		Tk_CreateEventHandler(tkwin, StructureNotifyMask,
			TrivialEventProc, (ClientData) recordPtr);
		Tcl_SetObjResult(interp, objv[2]);
	    } else {
		Tk_FreeConfigOptions((char *) recordPtr,
			recordPtr->header.optionTable, tkwin);
	    }
	}
	if (result != TCL_OK) {
	    Tk_DestroyWindow(tkwin);
	    ckfree((char *) recordPtr);
	}
    }
    }

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TrivialConfigObjCmd --
 *
 *	This command is used to test the configuration package. It only
 *	handles the "configure" and "cget" subcommands.
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
TrivialConfigObjCmd(
    ClientData clientData,	/* Main window for application. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    int result = TCL_OK;
    static const char *options[] = {
	"cget", "configure", "csave", NULL
    };
    enum {
	CGET, CONFIGURE, CSAVE
    };
    Tcl_Obj *resultObjPtr;
    int index, mask;
    TrivialCommandHeader *headerPtr = (TrivialCommandHeader *) clientData;
    Tk_Window tkwin = headerPtr->tkwin;
    Tk_SavedOptions saved;

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg...?");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], options, "command", 0,
	    &index) != TCL_OK) {
	return TCL_ERROR;
    }

    Tcl_Preserve(clientData);

    switch (index) {
    case CGET:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "option");
	    result = TCL_ERROR;
	    goto done;
	}
	resultObjPtr = Tk_GetOptionValue(interp, (char *) clientData,
		headerPtr->optionTable, objv[2], tkwin);
	if (resultObjPtr != NULL) {
	    Tcl_SetObjResult(interp, resultObjPtr);
	    result = TCL_OK;
	} else {
	    result = TCL_ERROR;
	}
	break;
    case CONFIGURE:
	if (objc == 2) {
	    resultObjPtr = Tk_GetOptionInfo(interp, (char *) clientData,
		    headerPtr->optionTable, NULL, tkwin);
	    if (resultObjPtr == NULL) {
		result = TCL_ERROR;
	    } else {
		Tcl_SetObjResult(interp, resultObjPtr);
	    }
	} else if (objc == 3) {
	    resultObjPtr = Tk_GetOptionInfo(interp, (char *) clientData,
		    headerPtr->optionTable, objv[2], tkwin);
	    if (resultObjPtr == NULL) {
		result = TCL_ERROR;
	    } else {
		Tcl_SetObjResult(interp, resultObjPtr);
	    }
	} else {
	    result = Tk_SetOptions(interp, (char *) clientData,
		    headerPtr->optionTable, objc - 2, objv + 2,
		    tkwin, NULL, &mask);
	    if (result == TCL_OK) {
		Tcl_SetIntObj(Tcl_GetObjResult(interp), mask);
	    }
	}
	break;
    case CSAVE:
	result = Tk_SetOptions(interp, (char *) clientData,
		headerPtr->optionTable, objc - 2, objv + 2,
		tkwin, &saved, &mask);
	Tk_FreeSavedOptions(&saved);
	if (result == TCL_OK) {
	    Tcl_SetIntObj(Tcl_GetObjResult(interp), mask);
	}
	break;
    }
  done:
    Tcl_Release(clientData);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TrivialCmdDeletedProc --
 *
 *	This function is invoked when a widget command is deleted. If the
 *	widget isn't already in the process of being destroyed, this command
 *	destroys it.
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
TrivialCmdDeletedProc(
    ClientData clientData)	/* Pointer to widget record for widget. */
{
    TrivialCommandHeader *headerPtr = (TrivialCommandHeader *) clientData;
    Tk_Window tkwin = headerPtr->tkwin;

    if (tkwin != NULL) {
	Tk_DestroyWindow(tkwin);
    } else if (headerPtr->optionTable != NULL) {
	/*
	 * This is a "new" object, which doesn't have a window, so we can't
	 * depend on cleaning up in the event function. Free its resources
	 * here.
	 */

	Tk_FreeConfigOptions((char *) clientData,
		headerPtr->optionTable, (Tk_Window) NULL);
	Tcl_EventuallyFree(clientData, TCL_DYNAMIC);
    }
}

/*
 *--------------------------------------------------------------
 *
 * TrivialEventProc --
 *
 *	A dummy event proc.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get cleaned up.
 *
 *--------------------------------------------------------------
 */

static void
TrivialEventProc(
    ClientData clientData,	/* Information about window. */
    XEvent *eventPtr)		/* Information about event. */
{
    TrivialCommandHeader *headerPtr = (TrivialCommandHeader *) clientData;

    if (eventPtr->type == DestroyNotify) {
	if (headerPtr->tkwin != NULL) {
	    Tk_FreeConfigOptions((char *) clientData,
		    headerPtr->optionTable, headerPtr->tkwin);
	    headerPtr->optionTable = NULL;
	    headerPtr->tkwin = NULL;
	    Tcl_DeleteCommandFromToken(headerPtr->interp,
		    headerPtr->widgetCmd);
	}
	Tcl_EventuallyFree(clientData, TCL_DYNAMIC);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TestfontObjCmd --
 *
 *	This function implements the "testfont" command, which is used to test
 *	TkFont objects.
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
TestfontObjCmd(
    ClientData clientData,	/* Main window for application. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[])	/* Argument objects. */
{
    static const char *options[] = {"counts", "subfonts", NULL};
    enum option {COUNTS, SUBFONTS};
    int index;
    Tk_Window tkwin;
    Tk_Font tkfont;

    tkwin = (Tk_Window) clientData;

    if (objc < 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "option fontName");
	return TCL_ERROR;
    }

    if (Tcl_GetIndexFromObj(interp, objv[1], options, "command", 0, &index)
	    != TCL_OK) {
	return TCL_ERROR;
    }

    switch ((enum option) index) {
    case COUNTS:
	Tcl_SetObjResult(interp,
		TkDebugFont(Tk_MainWindow(interp), Tcl_GetString(objv[2])));
	break;
    case SUBFONTS:
	tkfont = Tk_AllocFontFromObj(interp, tkwin, objv[2]);
	if (tkfont == NULL) {
	    return TCL_ERROR;
	}
	TkpGetSubFonts(interp, tkfont);
	Tk_FreeFont(tkfont);
	break;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ImageCreate --
 *
 *	This function is called by the Tk image code to create "test" images.
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
ImageCreate(
    Tcl_Interp *interp,		/* Interpreter for application containing
				 * image. */
    char *name,			/* Name to use for image. */
    int objc,			/* Number of arguments. */
    Tcl_Obj *const objv[],	/* Argument strings for options (doesn't
				 * include image name or type). */
    Tk_ImageType *typePtr,	/* Pointer to our type record (not used). */
    Tk_ImageMaster master,	/* Token for image, to be used by us in later
				 * callbacks. */
    ClientData *clientDataPtr)	/* Store manager's token for image here; it
				 * will be returned in later callbacks. */
{
    TImageMaster *timPtr;
    char *varName;
    int i;

    varName = "log";
    for (i = 0; i < objc; i += 2) {
	if (strcmp(Tcl_GetString(objv[i]), "-variable") != 0) {
	    Tcl_AppendResult(interp, "bad option name \"",
		    Tcl_GetString(objv[i]), "\"", NULL);
	    return TCL_ERROR;
	}
	if ((i+1) == objc) {
	    Tcl_AppendResult(interp, "no value given for \"",
		    Tcl_GetString(objv[i]), "\" option", NULL);
	    return TCL_ERROR;
	}
	varName = Tcl_GetString(objv[i+1]);
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
    Tcl_CreateCommand(interp, name, ImageCmd, (ClientData) timPtr, NULL);
    *clientDataPtr = (ClientData) timPtr;
    Tk_ImageChanged(master, 0, 0, 30, 15, 30, 15);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ImageCmd --
 *
 *	This function implements the commands corresponding to individual
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
ImageCmd(
    ClientData clientData,	/* Main window for application. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    TImageMaster *timPtr = (TImageMaster *) clientData;
    int x, y, width, height;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], "option ?arg arg ...?", NULL);
	return TCL_ERROR;
    }
    if (strcmp(argv[1], "changed") == 0) {
	if (argc != 8) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " changed x y width height imageWidth imageHeight", NULL);
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
		"\": must be changed", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ImageGet --
 *
 *	This function is called by Tk to set things up for using a test image
 *	in a particular widget.
 *
 * Results:
 *	The return value is a token for the image instance, which is used in
 *	future callbacks to ImageDisplay and ImageFree.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static ClientData
ImageGet(
    Tk_Window tkwin,		/* Token for window in which image will be
				 * used. */
    ClientData clientData)	/* Pointer to TImageMaster for image. */
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
 *	This function is invoked to redisplay part or all of an image in a
 *	given drawable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The image gets partially redrawn, as an "X" that shows the exact
 *	redraw area.
 *
 *----------------------------------------------------------------------
 */

static void
ImageDisplay(
    ClientData clientData,	/* Pointer to TImageInstance for image. */
    Display *display,		/* Display to use for drawing. */
    Drawable drawable,		/* Where to redraw image. */
    int imageX, int imageY,	/* Origin of area to redraw, relative to
				 * origin of image. */
    int width, int height,	/* Dimensions of area to redraw. */
    int drawableX, int drawableY)
				/* Coordinates in drawable corresponding to
				 * imageX and imageY. */
{
    TImageInstance *instPtr = (TImageInstance *) clientData;
    char buffer[200 + TCL_INTEGER_SPACE * 6];

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
 *	This function is called when an instance of an image is no longer
 *	used.
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
ImageFree(
    ClientData clientData,	/* Pointer to TImageInstance for instance. */
    Display *display)		/* Display where image was to be drawn. */
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
 *	This function is called to clean up a test image when an application
 *	goes away.
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
ImageDelete(
    ClientData clientData)	/* Pointer to TImageMaster for image. When
				 * this function is called, no more instances
				 * exist. */
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
 *	This function implements the "testmakeexist" command. It calls
 *	Tk_MakeWindowExist on each of its arguments to force the windows to be
 *	created.
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
TestmakeexistCmd(
    ClientData clientData,	/* Main window for application. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    Tk_Window mainWin = (Tk_Window) clientData;
    int i;
    Tk_Window tkwin;

    for (i = 1; i < argc; i++) {
	tkwin = Tk_NameToWindow(interp, argv[i], mainWin);
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
 *	This function implements the "testmenubar" command. It is used to test
 *	the Unix facilities for creating space above a toplevel window for a
 *	menubar.
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
#if !(defined(__WIN32__) || defined(MAC_OSX_TK) || defined(__CYGWIN__))
static int
TestmenubarCmd(
    ClientData clientData,	/* Main window for application. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
#ifdef __UNIX__
    Tk_Window mainWin = (Tk_Window) clientData;
    Tk_Window tkwin, menubar;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args;  must be \"", argv[0],
		" option ?arg ...?\"", NULL);
	return TCL_ERROR;
    }

    if (strcmp(argv[1], "window") == 0) {
	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args;  must be \"", argv[0],
		    "window toplevel menubar\"", NULL);
	    return TCL_ERROR;
	}
	tkwin = Tk_NameToWindow(interp, argv[2], mainWin);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	if (argv[3][0] == 0) {
	    TkUnixSetMenubar(tkwin, NULL);
	} else {
	    menubar = Tk_NameToWindow(interp, argv[3], mainWin);
	    if (menubar == NULL) {
		return TCL_ERROR;
	    }
	    TkUnixSetMenubar(tkwin, menubar);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be  window", NULL);
	return TCL_ERROR;
    }

    return TCL_OK;
#else
    Tcl_SetResult(interp, "testmenubar is supported only under Unix",
	    TCL_STATIC);
    return TCL_ERROR;
#endif
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TestmetricsCmd --
 *
 *	This function implements the testmetrics command. It provides a way to
 *	determine the size of various widget components.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#if defined(__WIN32__) || defined(MAC_OSX_TK)
static int
TestmetricsCmd(
    ClientData clientData,	/* Main window for application. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    char buf[TCL_INTEGER_SPACE];
    int val;

#ifdef __WIN32__
    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args;  must be \"", argv[0],
		" option ?arg ...?\"", NULL);
	return TCL_ERROR;
    }
#else
    Tk_Window tkwin = (Tk_Window) clientData;
    TkWindow *winPtr;

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # args;  must be \"", argv[0],
		" option window\"", NULL);
	return TCL_ERROR;
    }

    winPtr = (TkWindow *) Tk_NameToWindow(interp, argv[2], tkwin);
    if (winPtr == NULL) {
	return TCL_ERROR;
    }
#endif

    if (strcmp(argv[1], "cyvscroll") == 0) {
#ifdef __WIN32__
	val = GetSystemMetrics(SM_CYVSCROLL);
#else
	val = ((TkScrollbar *) winPtr->instanceData)->width;
#endif
    } else  if (strcmp(argv[1], "cxhscroll") == 0) {
#ifdef __WIN32__
	val = GetSystemMetrics(SM_CXHSCROLL);
#else
	val = ((TkScrollbar *) winPtr->instanceData)->width;
#endif
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be cxhscroll or cyvscroll", NULL);
	return TCL_ERROR;
    }
    sprintf(buf, "%d", val);
    Tcl_AppendResult(interp, buf, NULL);
    return TCL_OK;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TestpropCmd --
 *
 *	This function implements the "testprop" command. It fetches and prints
 *	the value of a property on a window.
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
TestpropCmd(
    ClientData clientData,	/* Main window for application. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    Tk_Window mainWin = (Tk_Window) clientData;
    int result, actualFormat;
    unsigned long bytesAfter, length, value;
    Atom actualType, propName;
    unsigned char *property, *p;
    char *end;
    Window w;
    char buffer[30];

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # args;  must be \"", argv[0],
		" window property\"", NULL);
	return TCL_ERROR;
    }

    w = strtoul(argv[1], &end, 0);
    propName = Tk_InternAtom(mainWin, argv[2]);
    property = NULL;
    result = XGetWindowProperty(Tk_Display(mainWin),
	    w, propName, 0, 100000, False, AnyPropertyType,
	    &actualType, &actualFormat, &length,
	    &bytesAfter, &property);
    if ((result == Success) && (actualType != None)) {
	if ((actualFormat == 8) && (actualType == XA_STRING)) {
	    for (p = property; ((unsigned long)(p-property)) < length; p++) {
		if (*p == 0) {
		    *p = '\n';
		}
	    }
	    Tcl_SetResult(interp, (/*!unsigned*/char*)property, TCL_VOLATILE);
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

#if !(defined(__WIN32__) || defined(MAC_OSX_TK) || defined(__CYGWIN__))
/*
 *----------------------------------------------------------------------
 *
 * TestwrapperCmd --
 *
 *	This function implements the "testwrapper" command. It provides a way
 *	from Tcl to determine the extra window Tk adds in between the toplevel
 *	window and the window decorations.
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
TestwrapperCmd(
    ClientData clientData,	/* Main window for application. */
    Tcl_Interp *interp,		/* Current interpreter. */
    int argc,			/* Number of arguments. */
    const char **argv)		/* Argument strings. */
{
    TkWindow *winPtr, *wrapperPtr;
    Tk_Window tkwin;

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args;  must be \"", argv[0],
		" window\"", NULL);
	return TCL_ERROR;
    }

    tkwin = (Tk_Window) clientData;
    winPtr = (TkWindow *) Tk_NameToWindow(interp, argv[1], tkwin);
    if (winPtr == NULL) {
	return TCL_ERROR;
    }

    wrapperPtr = TkpGetWrapperWindow(winPtr);
    if (wrapperPtr != NULL) {
	char buf[TCL_INTEGER_SPACE];

	TkpPrintWindowId(buf, Tk_WindowId(wrapperPtr));
	Tcl_SetResult(interp, buf, TCL_VOLATILE);
    }
    return TCL_OK;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * CustomOptionSet, CustomOptionGet, CustomOptionRestore, CustomOptionFree --
 *
 *	Handlers for object-based custom configuration options. See
 *	Testobjconfigcommand.
 *
 * Results:
 *	See user documentation for expected results from these functions.
 *		CustomOptionSet		Standard Tcl Result.
 *		CustomOptionGet		Tcl_Obj * containing value.
 *		CustomOptionRestore	None.
 *		CustomOptionFree	None.
 *
 * Side effects:
 *	Depends on the function.
 *		CustomOptionSet		Sets option value to new setting.
 *		CustomOptionGet		Creates a new Tcl_Obj.
 *		CustomOptionRestore	Resets option value to original value.
 *		CustomOptionFree	Free storage for internal rep of
 *					option.
 *
 *----------------------------------------------------------------------
 */

static int
CustomOptionSet(
    ClientData clientData,
    Tcl_Interp *interp,
    Tk_Window tkwin,
    Tcl_Obj **value,
    char *recordPtr,
    int internalOffset,
    char *saveInternalPtr,
    int flags)
{
    int objEmpty, length;
    char *newStr, *string, *internalPtr;

    objEmpty = 0;

    if (internalOffset >= 0) {
	internalPtr = recordPtr + internalOffset;
    } else {
	internalPtr = NULL;
    }

    /*
     * See if the object is empty.
     */

    if (value == NULL) {
	objEmpty = 1;
    } else if ((*value)->bytes != NULL) {
	objEmpty = ((*value)->length == 0);
    } else {
	Tcl_GetStringFromObj((*value), &length);
	objEmpty = (length == 0);
    }

    if ((flags & TK_OPTION_NULL_OK) && objEmpty) {
	*value = NULL;
    } else {
	string = Tcl_GetStringFromObj((*value), &length);
	Tcl_UtfToUpper(string);
	if (strcmp(string, "BAD") == 0) {
	    Tcl_SetResult(interp, "expected good value, got \"BAD\"",
		    TCL_STATIC);
	    return TCL_ERROR;
	}
    }
    if (internalPtr != NULL) {
	if ((*value) != NULL) {
	    string = Tcl_GetStringFromObj((*value), &length);
	    newStr = ckalloc((size_t) (length + 1));
	    strcpy(newStr, string);
	} else {
	    newStr = NULL;
	}
	*((char **) saveInternalPtr) = *((char **) internalPtr);
	*((char **) internalPtr) = newStr;
    }

    return TCL_OK;
}

static Tcl_Obj *
CustomOptionGet(
    ClientData clientData,
    Tk_Window tkwin,
    char *recordPtr,
    int internalOffset)
{
    return (Tcl_NewStringObj(*(char **)(recordPtr + internalOffset), -1));
}

static void
CustomOptionRestore(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr,
    char *saveInternalPtr)
{
    *(char **)internalPtr = *(char **)saveInternalPtr;
    return;
}

static void
CustomOptionFree(
    ClientData clientData,
    Tk_Window tkwin,
    char *internalPtr)
{
    if (*(char **)internalPtr != NULL) {
	ckfree(*(char **)internalPtr);
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
