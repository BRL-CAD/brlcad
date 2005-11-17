/*
 * tkimg.c --
 *
 *  Generic interface to XML parsers.
 *
 * Copyright (c) 2002 Andreas Kupries <andreas_kupries@users.sourceforge.net>
 *
 * Zveno Pty Ltd makes this software and associated documentation
 * available free of charge for any purpose.  You may make copies
 * of the software but you must include all of this notice on any copy.
 *
 * Zveno Pty Ltd does not warrant that this software is error free
 * or fit for any purpose.  Zveno Pty Ltd disclaims any liability for
 * all claims, expenses, losses, damages and costs any user may incur
 * as a result of using, copying or modifying the software.
 *
 * $Id$
 *
 */

#include "tk.h"
#include "tkimg.h"

#define TCL_DOES_STUBS \
    (TCL_MAJOR_VERSION > 8 || TCL_MAJOR_VERSION == 8 && (TCL_MINOR_VERSION > 1 || \
    (TCL_MINOR_VERSION == 1 && TCL_RELEASE_LEVEL == TCL_FINAL_RELEASE)))

/*
 * Declarations for externally visible functions.
 */

#undef TCL_STORAGE_CLASS
#ifdef BUILD_tkimg
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# ifdef USE_TKIMG_STUBS
#  define TCL_STORAGE_CLASS
# else
#  define TCL_STORAGE_CLASS DLLIMPORT
# endif
#endif

#ifdef _DEBUG
EXTERN int Tkimg_d_Init     _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int Tkimg_d_SafeInit _ANSI_ARGS_((Tcl_Interp *interp));
#else
EXTERN int Tkimg_Init     _ANSI_ARGS_((Tcl_Interp *interp));
EXTERN int Tkimg_SafeInit _ANSI_ARGS_((Tcl_Interp *interp));
#endif

#undef  TCL_STORAGE_CLASS
#define TCL_STORAGE_CLASS DLLIMPORT


#ifdef _DEBUG
extern int Png_d_Init     _ANSI_ARGS_((Tcl_Interp *interp));
extern int Z_d_Init     _ANSI_ARGS_((Tcl_Interp *interp));
extern int Tkimgpng_d_Init     _ANSI_ARGS_((Tcl_Interp *interp));
#else
extern int Png_Init     _ANSI_ARGS_((Tcl_Interp *interp));
extern int Z_Init     _ANSI_ARGS_((Tcl_Interp *interp));
extern int Tkimgpng_Init     _ANSI_ARGS_((Tcl_Interp *interp));
#endif


#ifdef ALLOW_B64
static int tob64 _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
		int argc, Tcl_Obj *CONST objv[]));
static int fromb64 _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
		int argc, Tcl_Obj *CONST objv[]));
#endif

/*
 * The variable "initialized" contains flags indicating which
 * version of Tcl or Perl we are running:
 *
 *	IMG_TCL		Tcl
 *	IMG_OBJS	using Tcl_Obj's in stead of char* (Tk 8.3 or higher)
 *      IMG_PERL	perl
 *
 * These flags will be determined at runtime (except the IMG_PERL
 * flag, for now), so we can use the same dynamic library for all
 * Tcl/Tk versions (and for Perl/Tk in the future).
 */

static int initialized = 0;

/*
 *----------------------------------------------------------------------------
 *
 * Tkimg_Init --
 *
 *  Initialisation routine for loadable module
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Creates commands in the interpreter,
 *  loads xml package.
 *
 *----------------------------------------------------------------------------
 */

int
#ifdef _DEBUG
Tkimg_d_Init (interp)
#else
Tkimg_Init (interp)
#endif
      Tcl_Interp *interp; /* Interpreter to initialise. */
{
    extern int TkimgInitUtilities _ANSI_ARGS_ ((Tcl_Interp* interp));

#if TCL_DOES_STUBS
    extern TkimgStubs tkimgStubs;
#endif

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
        return TCL_ERROR;
    }
#endif
#ifdef USE_TK_STUBS
    if (Tk_InitStubs(interp, "8.1", 0) == NULL) {
        return TCL_ERROR;
    }
#endif

    if (!initialized) {
	if (!(initialized = TkimgInitUtilities (interp))) {
	    return TCL_ERROR;
	}
    }


#ifdef _DEBUG
    if (Z_d_Init(interp) != TCL_OK) {
        return TCL_ERROR;
    }
    if (Png_d_Init(interp) != TCL_OK) {
        return TCL_ERROR;
    }
    if (Tkimgpng_d_Init(interp) != TCL_OK) {
        return TCL_ERROR;
    }
#else
    if (Png_Init(interp) != TCL_OK) {
        return TCL_ERROR;
    }
    if (Z_Init(interp) != TCL_OK) {
        return TCL_ERROR;
    }
    if (Tkimgpng_Init(interp) != TCL_OK) {
        return TCL_ERROR;
    }
#endif

#ifdef ALLOW_B64 /* Undocumented feature */
    Tcl_CreateObjCommand(interp,"img_to_base64",   tob64,   (ClientData) NULL, NULL);
    Tcl_CreateObjCommand(interp,"img_from_base64", fromb64, (ClientData) NULL, NULL);
#endif

#if TCL_DOES_STUBS
    if (Tcl_PkgProvideEx(interp, TKIMG_PACKAGE_NAME, TKIMG_VERSION,
			 (ClientData) &tkimgStubs) != TCL_OK) {
        return TCL_ERROR;
    }
#else
    if (Tcl_PkgProvide(interp, TKIMG_PACKAGE_NAME, TKIMG_VERSION) != TCL_OK) {
        return TCL_ERROR;
    }
#endif

  return TCL_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * Tkimg_SafeInit --
 *
 *  Initialisation routine for loadable module in a safe interpreter.
 *
 * Results:
 *  None.
 *
 * Side effects:
 *  Creates commands in the interpreter,
 *  loads xml package.
 *
 *----------------------------------------------------------------------------
 */

int
#ifdef _DEBUG
Tkimg_d_SafeInit (interp)
#else
Tkimg_SafeInit (interp)
#endif
      Tcl_Interp *interp; /* Interpreter to initialise. */
{
#ifdef _DEBUG
    return Tkimg_d_Init(interp);
#else
    return Tkimg_Init(interp);
#endif
}

/*
 *-------------------------------------------------------------------------
 * tob64 --
 *  This function converts the contents of a file into a base-64
 *  encoded string.
 *
 * Results:
 *  none
 *
 * Side effects:
 *  none
 *
 *-------------------------------------------------------------------------
 */

#ifdef ALLOW_B64
int tob64(clientData, interp, argc, objv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    Tcl_Obj *CONST objv[];
{
    Tcl_DString dstring;
    tkimg_MFile handle;
    Tcl_Channel chan;
    char buffer[1024];
    int len;

    if (argc != 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "filename");
	return TCL_ERROR;
    }

    chan = tkimg_OpenFileChannel(interp, Tcl_GetStringFromObj(objv[1], &len), 0);
    if (!chan) {
	return TCL_ERROR;
    }

    Tcl_DStringInit(&dstring);
    tkimg_WriteInit(&dstring, &handle);

    while ((len = Tcl_Read(chan, buffer, 1024)) == 1024) {
	tkimg_Write(&handle, buffer, 1024);
    }
    if (len > 0) {
	tkimg_Write(&handle, buffer, len);
    }
    if ((Tcl_Close(interp, chan) == TCL_ERROR) || (len < 0)) {
	Tcl_DStringFree(&dstring);
	Tcl_AppendResult(interp, Tcl_GetStringFromObj(objv[1], &len),
		": ", Tcl_PosixError(interp), (char *)NULL);
	return TCL_ERROR;
    }
    tkimg_Putc(IMG_DONE, &handle);

    Tcl_DStringResult(interp, &dstring);
    return TCL_OK;
}

/*
 *-------------------------------------------------------------------------
 * fromb64 --
 *  This function converts a base-64 encoded string into binary form,
 *  which is written to a file.
 *
 * Results:
 *  none
 *
 * Side effects:
 *  none
 *
 *-------------------------------------------------------------------------
 */

int fromb64(clientData, interp, argc, objv)
    ClientData clientData;
    Tcl_Interp *interp;
    int argc;
    Tcl_Obj *CONST objv[];
{
    tkimg_MFile handle;
    Tcl_Channel chan;
    char buffer[1024];
    int len;

    if (argc != 3) {
	Tcl_WrongNumArgs(interp, 1, objv, "filename data");
	return TCL_ERROR;
    }

    chan = tkimg_OpenFileChannel(interp, Tcl_GetStringFromObj(objv[1], &len), 0644);
    if (!chan) {
	return TCL_ERROR;
    }

    handle.data = Tcl_GetStringFromObj(objv[2], &handle.length);
    handle.state = 0;

    while ((len = tkimg_Read(&handle, buffer, 1024)) == 1024) {
	if (Tcl_Write(chan, buffer, 1024) != 1024) {
	    goto writeerror;
	}
    }
    if (len > 0) {
	if (Tcl_Write(chan, buffer, len) != len) {
	    goto writeerror;
	}
    }
    if (Tcl_Close(interp, chan) == TCL_ERROR) {
	return TCL_ERROR;
    }
    return TCL_OK;

 writeerror:
    Tcl_AppendResult(interp, Tcl_GetStringFromObj(objv[1], &len), ": ",
	    Tcl_PosixError(interp), (char *)NULL);
    return TCL_ERROR;
}
#endif
