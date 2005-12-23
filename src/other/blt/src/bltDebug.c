
/*
 * bltDebug.c --
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

#ifndef NO_BLTDEBUG

#ifdef TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif /* HAVE_SYS_TIME_H */
#endif /* TIME_WITH_SYS_TIME */

#include "bltChain.h"
static Blt_Chain watchChain;

static Tcl_CmdTraceProc DebugProc;
static Tcl_CmdProc DebugCmd;

typedef struct {
    char *pattern;
    char *name;
} WatchInfo;

static WatchInfo *
GetWatch(name)
    char *name;
{
    Blt_ChainLink *linkPtr;
    char c;
    WatchInfo *infoPtr;

    c = name[0];
    for (linkPtr = Blt_ChainFirstLink(&watchChain); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	infoPtr = Blt_ChainGetValue(linkPtr);
	if ((infoPtr->name[0] == c) && (strcmp(name, infoPtr->name) == 0)) {
	    return infoPtr;
	}
    }
    linkPtr = Blt_ChainAllocLink(sizeof(WatchInfo));
    infoPtr = Blt_ChainGetValue(linkPtr);
    infoPtr->name = Blt_Strdup(name);
    Blt_ChainLinkAfter(&watchChain, linkPtr, (Blt_ChainLink *)NULL);
    return infoPtr;
}

static void
DeleteWatch(watchName)
    char *watchName;
{
    Blt_ChainLink *linkPtr;
    char c;
    WatchInfo *infoPtr;

    c = watchName[0];
    for (linkPtr = Blt_ChainFirstLink(&watchChain); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	infoPtr = Blt_ChainGetValue(linkPtr);
	if ((infoPtr->name[0] == c) && 
	    (strcmp(infoPtr->name, watchName) == 0)) {
	    Blt_Free(infoPtr->name);
	    Blt_ChainDeleteLink(&watchChain, linkPtr);
	    return;
	}
    }
}

/*ARGSUSED*/
static void
DebugProc(clientData, interp, level, command, proc, cmdClientData,
    argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Not used. */
    int level;			/* Current level */
    char *command;		/* Command before substitution */
    Tcl_CmdProc *proc;		/* Not used. */
    ClientData cmdClientData;	/* Not used. */
    int argc;
    char **argv;		/* Command after parsing, but before
				 * evaluation */
{
    static unsigned char traceStack[200];
    register int i;
    char *string;
    Tcl_Channel errChannel;
    Tcl_DString dString;
    char prompt[200];
    register char *p;
    char *lineStart;
    int count;

    /* This is pretty crappy, but there's no way to trigger stack pops */
    for (i = level + 1; i < 200; i++) {
	traceStack[i] = 0;
    }
    if (Blt_ChainGetLength(&watchChain) > 0) {
	WatchInfo *infoPtr;
	int found;
	Blt_ChainLink *linkPtr;

	found = FALSE;
	for (linkPtr = Blt_ChainFirstLink(&watchChain); linkPtr != NULL;
	    linkPtr = Blt_ChainNextLink(linkPtr)) {
	    infoPtr = Blt_ChainGetValue(linkPtr);
	    if (Tcl_StringMatch(argv[0], infoPtr->name)) {
		found = TRUE;
		break;
	    }
	}
	if ((found) && (level < 200)) {
	    traceStack[level] = 1;
	    traceStack[level + 1] = 1;
	}
	if ((level >= 200) || (!traceStack[level])) {
	    return;
	}
    }
    /*
     * Use stderr channel, for compatibility with systems that don't have a
     * tty (like WIN32).  In reality, it doesn't make a difference since
     * Tk's Win32 console can't handle large streams of data anyways.
     */
    errChannel = Tcl_GetStdChannel(TCL_STDERR);
    if (errChannel == NULL) {
	Tcl_AppendResult(interp, "can't get stderr channel", (char *)NULL);
	Tcl_BackgroundError(interp);
	return;
    }
    Tcl_DStringInit(&dString);

    sprintf(prompt, "%-2d-> ", level);
    p = command;
    /* Skip leading spaces in command line. */
    while(isspace(UCHAR(*p))) {
	p++;
    }
    lineStart = p;
    count = 0;
    for (/* empty */; *p != '\0'; /* empty */) {
	if (*p == '\n') {
	    if (count > 0) {
		Tcl_DStringAppend(&dString, "     ", -1);
	    } else {
		Tcl_DStringAppend(&dString, prompt, -1);
	    }
	    Tcl_DStringAppend(&dString, lineStart, p - lineStart);
	    Tcl_DStringAppend(&dString, "\n", -1);
	    p++;
	    lineStart = p;
	    count++;
	    if (count > 6) {
		break;
	    }
	} else {
	    p++;
	}
    }   
    while (isspace(UCHAR(*lineStart))) {
	lineStart++;
    }
    if (lineStart < p) {
	if (count > 0) {
	    Tcl_DStringAppend(&dString, "     ", -1);
	} else {
	    Tcl_DStringAppend(&dString, prompt, -1);
	}
	Tcl_DStringAppend(&dString, lineStart, p - lineStart);
	if (count <= 6) {
	    Tcl_DStringAppend(&dString, "\n", -1);
	}
    }
    if (count > 6) {
	Tcl_DStringAppend(&dString, "     ...\n", -1);
    }
    string = Tcl_Merge(argc, argv);
    lineStart = string;
    sprintf(prompt, "  <- ");
    count = 0;
    for (p = string; *p != '\0'; /* empty */) {
	if (*p == '\n') {
	    if (count > 0) {
		Tcl_DStringAppend(&dString, "     ", -1);
	    } else {
		Tcl_DStringAppend(&dString, prompt, -1);
	    }
	    count++;
	    Tcl_DStringAppend(&dString, lineStart, p - lineStart);
	    Tcl_DStringAppend(&dString, "\n", -1);
	    p++;
	    lineStart = p;
	    if (count > 6) {
		break;
	    }
	} else {
	    p++;
	}
    }   
    if (lineStart < p) {
	if (count > 0) {
	    Tcl_DStringAppend(&dString, "     ", -1);
	} else {
	    Tcl_DStringAppend(&dString, prompt, -1);
	}
	Tcl_DStringAppend(&dString, lineStart, p - lineStart);
	if (count <= 6) {
	    Tcl_DStringAppend(&dString, "\n", -1);
	}
    }
    if (count > 6) {
	Tcl_DStringAppend(&dString, "      ...\n", -1);
    }
    Tcl_DStringAppend(&dString, "\n", -1);
    Blt_Free(string);
    Tcl_Write(errChannel, (char *)Tcl_DStringValue(&dString), -1);
    Tcl_Flush(errChannel);
    Tcl_DStringFree(&dString);
}

/*ARGSUSED*/
static int
DebugCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;
    int argc;
    char **argv;
{
    static Tcl_Trace token;
    static int level;
    int newLevel;
    char c;
    int length;
    WatchInfo *infoPtr;
    Blt_ChainLink *linkPtr;
    register int i;

    if (argc == 1) {
	Tcl_SetResult(interp, Blt_Itoa(level), TCL_VOLATILE);
	return TCL_OK;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'w') && (strncmp(argv[1], "watch", length) == 0)) {
	/* Add patterns of command names to watch to the chain */
	for (i = 2; i < argc; i++) {
	    GetWatch(argv[i]);
	}
    } else if ((c == 'i') && (strncmp(argv[1], "ignore", length) == 0)) {
	for (i = 2; i < argc; i++) {
	    DeleteWatch(argv[i]);
	}
    } else {
	goto levelTest;
    }
    /* Return the current watch patterns */
    for (linkPtr = Blt_ChainFirstLink(&watchChain); linkPtr != NULL;
	linkPtr = Blt_ChainNextLink(linkPtr)) {
	infoPtr = Blt_ChainGetValue(linkPtr);
	Tcl_AppendElement(interp, infoPtr->name);
    }
    return TCL_OK;

  levelTest:
    if (Tcl_GetBoolean(interp, argv[1], &newLevel) == TCL_OK) {
	if (newLevel > 0) {
	    newLevel = 10000;	/* Max out the level */
	}
    } else if (Tcl_GetInt(interp, argv[1], &newLevel) == TCL_OK) {
	if (newLevel < 0) {
	    newLevel = 0;
	}
    } else {
	return TCL_ERROR;
    }
    if (token != 0) {
	Tcl_DeleteTrace(interp, token);
    }
    if (newLevel > 0) {
	token = Tcl_CreateTrace(interp, newLevel, DebugProc, (ClientData)0);
    }
    level = newLevel;
    Tcl_SetResult(interp, Blt_Itoa(level), TCL_VOLATILE);
    return TCL_OK;
}

int
Blt_DebugInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec =
    {"bltdebug", DebugCmd,};

    Blt_ChainInit(&watchChain);
    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}

#endif /* NO_BLTDEBUG */
