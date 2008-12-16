/*
 * bltCutbuffer.c --
 *
 * Copyright 1993-1998 Lucent Technologies, Inc.
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

#ifndef NO_CUTBUFFER

#ifndef WIN32
#include <X11/Xproto.h>
#endif

static int
GetCutNumber(interp, string, bufferPtr)
    Tcl_Interp *interp;
    char *string;
    int *bufferPtr;
{
    int number;

    if (Tcl_GetInt(interp, string, &number) != TCL_OK) {
	return TCL_ERROR;
    }
    if ((number < 0) || (number > 7)) {
	Tcl_AppendResult(interp, "bad buffer # \"", string, "\"", (char *)NULL);
	return TCL_ERROR;
    }
    *bufferPtr = number;
    return TCL_OK;
}

/* ARGSUSED */
static int
RotateErrorProc(clientData, errEventPtr)
    ClientData clientData;
    XErrorEvent *errEventPtr;
{
    int *errorPtr = clientData;

    *errorPtr = TCL_ERROR;
    return 0;
}

static int
GetOp(interp, tkwin, argc, argv)
    Tcl_Interp *interp;
    Tk_Window tkwin;
    int argc;
    char **argv;
{
    char *string;
    int buffer;
    int nBytes;

    buffer = 0;
    if (argc == 3) {
	if (GetCutNumber(interp, argv[2], &buffer) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    string = XFetchBuffer(Tk_Display(tkwin), &nBytes, buffer);
    if (string != NULL) {
	int limit;
	register char *p;
	register int i;
	int c;

	if (string[nBytes - 1] == '\0') {
	    limit = nBytes - 1;
	} else {
	    limit = nBytes;
	}
	for (p = string, i = 0; i < limit; i++, p++) {
	    c = (unsigned char)*p;
	    if (c == 0) {
		*p = ' ';	/* Convert embedded NUL bytes */
	    }
	}
	if (limit == nBytes) {
	    char *newPtr;

	    /*
	     * Need to copy the string into a bigger buffer so we can
	     * add a NUL byte on the end.
	     */
	    newPtr = Blt_Malloc(nBytes + 1);
	    assert(newPtr);
	    memcpy(newPtr, string, nBytes);
	    newPtr[nBytes] = '\0';
	    Blt_Free(string);
	    string = newPtr;
	}
	Tcl_SetResult(interp, string, TCL_DYNAMIC);
    }
    return TCL_OK;
}

static int
RotateOp(interp, tkwin, argc, argv)
    Tcl_Interp *interp;
    Tk_Window tkwin;
    int argc;
    char **argv;
{
    int count;
    int result;
    Tk_ErrorHandler handler;

    count = 1;			/* Default: rotate one position */
    if (argc == 3) {
	if (Tcl_GetInt(interp, argv[2], &count) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((count < 0) || (count > 8)) {
	    Tcl_AppendResult(interp, "bad rotate count \"", argv[2], "\"",
		(char *)NULL);
	    return TCL_ERROR;
	}
    }
    result = TCL_OK;
    handler = Tk_CreateErrorHandler(Tk_Display(tkwin), BadMatch,
	X_RotateProperties, -1, RotateErrorProc, &result);
    XRotateBuffers(Tk_Display(tkwin), count);
    Tk_DeleteErrorHandler(handler);
    XSync(Tk_Display(tkwin), False);
    if (result != TCL_OK) {
	Tcl_AppendResult(interp, "can't rotate cutbuffers unless all are set",
	    (char *)NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}


static int
SetOp(interp, tkwin, argc, argv)
    Tcl_Interp *interp;
    Tk_Window tkwin;
    int argc;
    char **argv;
{
    int buffer;

    buffer = 0;
    if (argc == 4) {
	if (GetCutNumber(interp, argv[3], &buffer) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    XStoreBuffer(Tk_Display(tkwin), argv[2], strlen(argv[2]) + 1, buffer);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * BLT Sub-command specification:
 *
 *	- Name of the sub-command.
 *	- Minimum number of characters needed to unambiguously
 *        recognize the sub-command.
 *	- Pointer to the function to be called for the sub-command.
 *	- Minimum number of arguments accepted.
 *	- Maximum number of arguments accepted.
 *	- String to be displayed for usage.
 *
 *--------------------------------------------------------------
 */
static Blt_OpSpec cbOps[] =
{
    {"get", 1, (Blt_Op)GetOp, 2, 3, "?buffer?",},
    {"rotate", 1, (Blt_Op)RotateOp, 2, 3, "?count?",},
    {"set", 1, (Blt_Op)SetOp, 3, 4, "value ?buffer?",},
};
static int numCbOps = sizeof(cbOps) / sizeof(Blt_OpSpec);


/*
 *----------------------------------------------------------------------
 *
 * CutBufferCmd --
 *
 *	This procedure is invoked to process the "cutbuffer" Tcl
 *	command. See the user documentation for details on what it does.
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
CutbufferCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter.*/
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin;
    Blt_Op proc;
    int result;

    proc = Blt_GetOp(interp, numCbOps, cbOps, BLT_OP_ARG1, argc, argv, 0);
    if (proc == NULL) {
	return TCL_ERROR;
    }
    tkwin = Tk_MainWindow(interp);
    result = (*proc) (interp, tkwin, argc, argv);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_CutbufferInit --
 *
 *	This procedure is invoked to initialize the "cutbuffer" Tcl
 *	command. See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
int
Blt_CutbufferInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec =
    {"cutbuffer", CutbufferCmd,};

    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

#endif /* NO_CUTBUFFER */
