/*
 * bltBgexec.c --
 *
 *	This module implements a background "exec" command for the
 *	BLT toolkit.
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
 *
 *	The "bgexec" command was created by George Howlett.
 */

#include "bltInt.h"

#ifndef NO_BGEXEC

#include <fcntl.h>
#include <signal.h>
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#include <sys/types.h>
#include <ctype.h>
#include <sys/errno.h>

#include "bltWait.h"
#include "bltSwitch.h"

#if (TCL_MAJOR_VERSION == 7)
#define FILEHANDLER_USES_TCLFILES 1
#else
typedef int Tcl_File;
#endif

static Tcl_CmdProc BgexecCmd;

#ifdef WIN32
typedef struct {
    DWORD pid;
    HANDLE hProcess;
} Process;
#else
typedef int Process;
#endif

#if (TCL_VERSION_NUMBER <  _VERSION(8,1,0)) 
typedef void *Tcl_Encoding;	/* Make up dummy type for encoding.  */
#endif

#define ENCODING_ASCII		((Tcl_Encoding)NULL)
#define ENCODING_BINARY		((Tcl_Encoding)1)

/*
 *	As of Tcl 7.6, we're using our own version of the old
 *	Tcl_CreatePipeline routine.  I would have tried to use
 *	Tcl_OpenCommandChannel but you can't get at the array of
 *	process ids, unless of course you pry open the undocumented
 *	structure PipeStatus as clientData.  Nor could I figure out
 *	how to set one side of the pipe to be non-blocking.  The whole
 *	channel API seems overly complex for what its supposed to
 *	do. [And maybe that's why it keeps changing every release.]
 */
extern int Blt_CreatePipeline _ANSI_ARGS_((Tcl_Interp *interp, int argc,
	char **argv, Process **pidPtrPtr, int *inPipePtr, int *outPipePtr,
	int *errFilePtr));

#ifdef WIN32
#define read(fd, buf, size)	Blt_AsyncRead((fd),(buf),(size))
#define close(fd)		CloseHandle((HANDLE)fd)
#define Tcl_CreateFileHandler	Blt_CreateFileHandler
#define Tcl_DeleteFileHandler	Blt_DeleteFileHandler
#define kill			KillProcess
#define waitpid			WaitProcess
#endif

#define READ_AGAIN	(0)
#define READ_EOF	(-1)
#define READ_ERROR	(-2)

/* The wait-related definitions are taken from tclUnix.h */

#define TRACE_FLAGS (TCL_TRACE_WRITES | TCL_TRACE_UNSETS | TCL_GLOBAL_ONLY)

#define BLOCK_SIZE	1024	/* Size of allocation blocks for buffer */
#define DEF_BUFFER_SIZE	(BLOCK_SIZE * 8)
#define MAX_READS       100	/* Maximum number of successful reads
			         * before stopping to let Tcl catch up
			         * on events */

#ifndef NSIG
#define NSIG 		32	/* Number of signals available */
#endif /*NSIG*/

#ifndef SIGINT
#define SIGINT		2
#endif /* SIGINT */

#ifndef SIGQUIT
#define SIGQUIT		3
#endif /* SIGQUIT */

#ifndef SIGKILL
#define SIGKILL		9
#endif /* SIGKILL */

#ifndef SIGTERM
#define SIGTERM		14
#endif /* SIGTERM */

typedef struct {
    int number;
    char *name;
} SignalId;

static SignalId signalIds[] =
{
#ifdef SIGABRT
    {SIGABRT, "SIGABRT"},
#endif
#ifdef SIGALRM
    {SIGALRM, "SIGALRM"},
#endif
#ifdef SIGBUS
    {SIGBUS, "SIGBUS"},
#endif
#ifdef SIGCHLD
    {SIGCHLD, "SIGCHLD"},
#endif
#if defined(SIGCLD) && (!defined(SIGCHLD) || (SIGCLD != SIGCHLD))
    {SIGCLD, "SIGCLD"},
#endif
#ifdef SIGCONT
    {SIGCONT, "SIGCONT"},
#endif
#if defined(SIGEMT) && (!defined(SIGXCPU) || (SIGEMT != SIGXCPU))
    {SIGEMT, "SIGEMT"},
#endif
#ifdef SIGFPE
    {SIGFPE, "SIGFPE"},
#endif
#ifdef SIGHUP
    {SIGHUP, "SIGHUP"},
#endif
#ifdef SIGILL
    {SIGILL, "SIGILL"},
#endif
#ifdef SIGINT
    {SIGINT, "SIGINT"},
#endif
#ifdef SIGIO
    {SIGIO, "SIGIO"},
#endif
#if defined(SIGIOT) && (!defined(SIGABRT) || (SIGIOT != SIGABRT))
    {SIGIOT, "SIGIOT"},
#endif
#ifdef SIGKILL
    {SIGKILL, "SIGKILL"},
#endif
#if defined(SIGLOST) && (!defined(SIGIOT) || (SIGLOST != SIGIOT)) && (!defined(SIGURG) || (SIGLOST != SIGURG))
    {SIGLOST, "SIGLOST"},
#endif
#ifdef SIGPIPE
    {SIGPIPE, "SIGPIPE"},
#endif
#if defined(SIGPOLL) && (!defined(SIGIO) || (SIGPOLL != SIGIO))
    {SIGPOLL, "SIGPOLL"},
#endif
#ifdef SIGPROF
    {SIGPROF, "SIGPROF"},
#endif
#if defined(SIGPWR) && (!defined(SIGXFSZ) || (SIGPWR != SIGXFSZ))
    {SIGPWR, "SIGPWR"},
#endif
#ifdef SIGQUIT
    {SIGQUIT, "SIGQUIT"},
#endif
#ifdef SIGSEGV
    {SIGSEGV, "SIGSEGV"},
#endif
#ifdef SIGSTOP
    {SIGSTOP, "SIGSTOP"},
#endif
#ifdef SIGSYS
    {SIGSYS, "SIGSYS"},
#endif
#ifdef SIGTERM
    {SIGTERM, "SIGTERM"},
#endif
#ifdef SIGTRAP
    {SIGTRAP, "SIGTRAP"},
#endif
#ifdef SIGTSTP
    {SIGTSTP, "SIGTSTP"},
#endif
#ifdef SIGTTIN
    {SIGTTIN, "SIGTTIN"},
#endif
#ifdef SIGTTOU
    {SIGTTOU, "SIGTTOU"},
#endif
#if defined(SIGURG) && (!defined(SIGIO) || (SIGURG != SIGIO))
    {SIGURG, "SIGURG"},
#endif
#if defined(SIGUSR1) && (!defined(SIGIO) || (SIGUSR1 != SIGIO))
    {SIGUSR1, "SIGUSR1"},
#endif
#if defined(SIGUSR2) && (!defined(SIGURG) || (SIGUSR2 != SIGURG))
    {SIGUSR2, "SIGUSR2"},
#endif
#ifdef SIGVTALRM
    {SIGVTALRM, "SIGVTALRM"},
#endif
#ifdef SIGWINCH
    {SIGWINCH, "SIGWINCH"},
#endif
#ifdef SIGXCPU
    {SIGXCPU, "SIGXCPU"},
#endif
#ifdef SIGXFSZ
    {SIGXFSZ, "SIGXFSZ"},
#endif
    {-1, "unknown signal"},
};

/*
 * Sink buffer:
 *   ____________________
 *  |                    |  "size"	current allocated length of buffer.
 *  |                    |
 *  |--------------------|  "fill"      fill point (# characters in buffer).
 *  |  Raw               |
 *  |--------------------|  "mark"      Marks end of cooked characters.
 *  |                    |
 *  |  Cooked            |
 *  |                    |
 *  |                    |
 *  |--------------------|  "lastMark"  Mark end of processed characters.
 *  |                    |
 *  |                    |
 *  |  Processed         |
 *  |                    |
 *  |____________________| 0
 */
typedef struct {
    char *name;			/* Name of the sink */

    char *doneVar;		/* Name of a Tcl variable (malloc'ed)
				 * set to the collected data of the
				 * last UNIX subprocess. */

    char *updateVar;		/* Name of a Tcl variable (malloc'ed)
				 * updated as data is read from the
				 * pipe. */

    char **updateCmd;		/* Start of a Tcl command executed
				 * whenever data is read from the
				 * pipe. */

#if (TCL_MAJOR_VERSION >= 8)
    Tcl_Obj **objv;		/*  */
    int objc;			/*  */
#endif

    int flags;			

    Tcl_File file;		/* Used for backward compatability
				 * with Tcl 7.5 */
    Tcl_Encoding encoding;
    int fd;			/* File descriptor of the pipe. */
    int status;

    int echo;			/* Indicates if the pipeline's stderr stream
				 * should be echoed */
    unsigned char *byteArr;	/* Stores pipeline output (malloc-ed):
				 * Initially points to static storage
				 */
    size_t size;		/* Size of dynamically allocated buffer. */

    size_t fill;		/* # of bytes read into the buffer. Marks
				 * the current fill point of the buffer. */

    size_t mark;		/* # of bytes translated (cooked). */
    size_t lastMark;		/* # of bytes as of the last read.
				 * This indicates the start of the new
				 * data in the buffer since the last
				 * time the "update" variable was
				 * set. */

    unsigned char staticSpace[DEF_BUFFER_SIZE];	/* Static space */

} Sink;

#define SINK_BUFFERED		(1<<0)
#define SINK_KEEP_NL		(1<<1)
#define SINK_NOTIFY		(1<<2)

typedef struct {
    char *statVar;		/* Name of a Tcl variable set to the
				 * exit status of the last
				 * process. Setting this variable
				 * triggers the termination of all
				 * subprocesses (regardless whether
				 * they have already completed) */

    int signalNum;		/* If non-zero, indicates the signal
				 * to send subprocesses when cleaning
				 * up.*/

    int keepNewline;		/* If non-zero, indicates to set Tcl
				 * output variables with trailing
				 * newlines intact */

    int lineBuffered;		/* If non-zero, indicates provide data
				 * to update variable and update proc on
				 * a line-by-line basis. */

    int interval;		/* Interval to poll for the exiting
				 * processes */
    char *outputEncodingName;	/* Name of decoding scheme to use when
				 * translating output data. */
    char *errorEncodingName;	/* Name of decoding scheme to use when
				 * translating output data. */

    /* Private */
    Tcl_Interp *interp;		/* Interpreter containing variables */

    int nProcs;			/* Number of processes in pipeline */
    Process *procArr;		/* Array of process tokens from pipeline.
				 * The token for Unix are pid_t, while
				 * for Win32 they're handles. */

    int traced;			/* Indicates that the status variable
				 * is currently being traced. */
    int detached;		/* Indicates that the pipeline is
				 * detached from standard I/O, running
				 * in the background. */
    Tcl_TimerToken timerToken;	/* Token for timer handler which polls
				 * for the exit status of each
				 * sub-process. If zero, there's no
				 * timer handler queued. */

    int *exitCodePtr;		/* Pointer to a memory location to
				 * contain the last process' exit
				 * code. */
    int *donePtr;

    Sink sink1, sink2;

} BackgroundInfo;


static Blt_SwitchParseProc StringToSignal;
static Blt_SwitchCustom killSignalSwitch =
{
    StringToSignal, (Blt_SwitchFreeProc *)NULL, (ClientData)0,
};

static Blt_SwitchSpec switchSpecs[] = 
{
    {BLT_SWITCH_STRING, "-decodeoutput", 
         Blt_Offset(BackgroundInfo, outputEncodingName), 0},
    {BLT_SWITCH_STRING, "-decodeerror", 
         Blt_Offset(BackgroundInfo, errorEncodingName), 0},
    {BLT_SWITCH_BOOLEAN, "-echo", 
         Blt_Offset(BackgroundInfo, sink2.echo), 0},
    {BLT_SWITCH_STRING, "-error", 
         Blt_Offset(BackgroundInfo, sink2.doneVar), 0},
    {BLT_SWITCH_STRING, "-update", 
	Blt_Offset(BackgroundInfo, sink1.updateVar), 0},
    {BLT_SWITCH_STRING, "-output", 
        Blt_Offset(BackgroundInfo, sink1.doneVar), 0},
    {BLT_SWITCH_STRING, "-lasterror", 
	Blt_Offset(BackgroundInfo, sink2.updateVar), 0},
    {BLT_SWITCH_STRING, "-lastoutput", 
	Blt_Offset(BackgroundInfo, sink1.updateVar), 0},
    {BLT_SWITCH_LIST, "-onerror", 
	Blt_Offset(BackgroundInfo, sink2.updateCmd), 0},
    {BLT_SWITCH_LIST, "-onoutput", 
	Blt_Offset(BackgroundInfo, sink1.updateCmd), 0},
    {BLT_SWITCH_BOOLEAN, "-keepnewline", 
	Blt_Offset(BackgroundInfo, keepNewline), 0},
    {BLT_SWITCH_BOOLEAN, "-check", 
	Blt_Offset(BackgroundInfo, interval), 0},
    {BLT_SWITCH_CUSTOM, "-killsignal", 
	Blt_Offset(BackgroundInfo, signalNum), 0, &killSignalSwitch},
    {BLT_SWITCH_BOOLEAN, "-linebuffered", 
	Blt_Offset(BackgroundInfo, lineBuffered), 0},
    {BLT_SWITCH_END, NULL, 0, 0}
};

static char *VariableProc _ANSI_ARGS_((ClientData clientData,
	Tcl_Interp *interp, char *part1, char *part2, int flags));
static void TimerProc _ANSI_ARGS_((ClientData clientData));
static void StdoutProc _ANSI_ARGS_((ClientData clientData, int mask));
static void StderrProc _ANSI_ARGS_((ClientData clientData, int mask));

/*
 *----------------------------------------------------------------------
 *
 * GetSignal --
 *
 *	Convert a string represent a signal number into its integer
 *	value.
 *
 * Results:
 *	The return value is a standard Tcl result.
 *
 *----------------------------------------------------------------------
 */
/*ARGSUSED*/
static int
StringToSignal(clientData, interp, switchName, string, record, offset)
    ClientData clientData;	/* Contains a pointer to the tabset containing
				 * this image. */
    Tcl_Interp *interp;		/* Interpreter to send results back to */
    char *switchName;		/* Not used. */
    char *string;		/* String representation */
    char *record;		/* Structure record */
    int offset;			/* Offset to field in structure */
{
    int *signalPtr = (int *)(record + offset);
    int signalNum;

    if ((string == NULL) || (*string == '\0')) {
	*signalPtr = 0;
	return TCL_OK;
    }
    if (isdigit(UCHAR(string[0]))) {
	if (Tcl_GetInt(interp, string, &signalNum) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	char *name;
	register SignalId *sigPtr;

	name = string;

	/*  Clip off any "SIG" prefix from the signal name */
	if ((name[0] == 'S') && (name[1] == 'I') && (name[2] == 'G')) {
	    name += 3;
	}
	signalNum = -1;
	for (sigPtr = signalIds; sigPtr->number > 0; sigPtr++) {
	    if (strcmp(sigPtr->name + 3, name) == 0) {
		signalNum = sigPtr->number;
		break;
	    }
	}
	if (signalNum < 0) {
	    Tcl_AppendResult(interp, "unknown signal \"", string, "\"",
		(char *)NULL);
	    return TCL_ERROR;
	}
    }
    if ((signalNum < 0) || (signalNum > NSIG)) {
	/* Outside range of signals */
	Tcl_AppendResult(interp, "signal number \"", string,
	    "\" is out of range", (char *)NULL);
	return TCL_ERROR;
    }
    *signalPtr = signalNum;
    return TCL_OK;
}


/*
 *----------------------------------------------------------------------
 *
 * GetSinkData --
 *
 *	Returns the data currently saved in the buffer
 *
 *----------------------------------------------------------------------
 */
static void
GetSinkData(sinkPtr, dataPtr, lengthPtr)
    Sink *sinkPtr;
    unsigned char **dataPtr;
    size_t *lengthPtr;
{
    size_t length;

    sinkPtr->byteArr[sinkPtr->mark] = '\0';
    length = sinkPtr->mark;
    if ((sinkPtr->mark > 0) && (sinkPtr->encoding != ENCODING_BINARY)) {
	unsigned char *last;

	last = sinkPtr->byteArr + (sinkPtr->mark - 1);
	if ((!(sinkPtr->flags & SINK_KEEP_NL)) && (*last == '\n')) {
	    length--;
	}
    }
    *dataPtr = sinkPtr->byteArr;
    *lengthPtr = length;
}

/*
 *----------------------------------------------------------------------
 *
 * NextBlock --
 *
 *	Returns the next block of data since the last time this
 *	routine was called.
 *
 *---------------------------------------------------------------------- 
 */
static unsigned char *
NextBlock(sinkPtr, lengthPtr)
    Sink *sinkPtr;
    int *lengthPtr;
{
    unsigned char *string;
    int length;

    string = sinkPtr->byteArr + sinkPtr->lastMark;
    length = sinkPtr->mark - sinkPtr->lastMark;
    sinkPtr->lastMark = sinkPtr->mark;
    if (length > 0) {
	if ((!(sinkPtr->flags & SINK_KEEP_NL)) && 
	    (string[length - 1] == '\n')) {
	    length--;
	}
	*lengthPtr = length;
	return string;
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * NextLine --
 *
 *	Returns the next line of data.
 *
 *----------------------------------------------------------------------
 */
static unsigned char *
NextLine(sinkPtr, lengthPtr)
    Sink *sinkPtr;
    int *lengthPtr;
{
    if (sinkPtr->mark > sinkPtr->lastMark) {
	unsigned char *string;
	int newBytes;
	register int i;

	string = sinkPtr->byteArr + sinkPtr->lastMark;
	newBytes = sinkPtr->mark - sinkPtr->lastMark;
	for (i = 0; i < newBytes; i++) {
	    if (string[i] == '\n') {
		int length;
		
		length = i + 1;
		sinkPtr->lastMark += length;
		if (!(sinkPtr->flags & SINK_KEEP_NL)) {
		    length--;		/* Backup over the newline. */
		}
		*lengthPtr = length;
		return string;
	    }
	}
	/* Newline not found.  On errors or EOF, also return a partial line. */
	if (sinkPtr->status < 0) {
	    *lengthPtr = newBytes;
	    sinkPtr->lastMark = sinkPtr->mark;
	    return string;
	}
    }
    return NULL;
}
/*
 *----------------------------------------------------------------------
 *
 * ResetSink --
 *
 *	Removes the bytes already processed from the buffer, possibly
 *	resetting it to empty.  This used when we don't care about
 *	keeping all the data collected from the channel (no -output
 *	flag and the process is detached).
 *
 *---------------------------------------------------------------------- 
 */
static void
ResetSink(sinkPtr)
    Sink *sinkPtr;
{ 
    if ((sinkPtr->flags & SINK_BUFFERED) && 
	(sinkPtr->fill > sinkPtr->lastMark)) {
	register size_t i, j;

	/* There may be bytes remaining in the buffer, awaiting
	 * another read before we see the next newline.  So move the
	 * bytes to the front of the array. */

 	for (i = 0, j = sinkPtr->lastMark; j < sinkPtr->fill; i++, j++) {
	    sinkPtr->byteArr[i] = sinkPtr->byteArr[j];
	}
	/* Move back the fill point and processed point. */
	sinkPtr->fill -= sinkPtr->lastMark;
	sinkPtr->mark -= sinkPtr->lastMark;
    } else {
	sinkPtr->mark = sinkPtr->fill = 0;
    }
    sinkPtr->lastMark = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * InitSink --
 *
 *	Initializes the buffer's storage.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Storage is cleared.
 *
 *----------------------------------------------------------------------
 */
static void
InitSink(bgPtr, sinkPtr, name, encoding)
    BackgroundInfo *bgPtr;
    Sink *sinkPtr;
    char *name;
    Tcl_Encoding encoding;
{
    sinkPtr->name = name;
    sinkPtr->echo = FALSE;
    sinkPtr->fd = -1;
    sinkPtr->file = (Tcl_File)NULL;
    sinkPtr->byteArr = sinkPtr->staticSpace;
    sinkPtr->size = DEF_BUFFER_SIZE;
    sinkPtr->encoding = encoding;
    if (bgPtr->keepNewline) {
	sinkPtr->flags |= SINK_KEEP_NL;
    }
    if (bgPtr->lineBuffered) {
	sinkPtr->flags |= SINK_BUFFERED;
    }	
    if ((sinkPtr->updateCmd != NULL) || 
	(sinkPtr->updateVar != NULL) ||
	(sinkPtr->echo)) {
	sinkPtr->flags |= SINK_NOTIFY;
    }
#if (TCL_MAJOR_VERSION >= 8)
    if (sinkPtr->updateCmd != NULL) {
	Tcl_Obj **objArr;
	char **p;
	int count;
	register int i;

	count = 0;
	for (p = sinkPtr->updateCmd; *p != NULL; p++) {
	    count++;
	}
	objArr = Blt_Malloc((count + 1) * sizeof(Tcl_Obj *));
	for (i = 0; i < count; i++) {
	    objArr[i] = Tcl_NewStringObj(sinkPtr->updateCmd[i], -1);
	    Tcl_IncrRefCount(objArr[i]);
	}
	sinkPtr->objv = objArr;
	sinkPtr->objc = count + 1;
    }
#endif
    ResetSink(sinkPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * FreeSinkBuffer --
 *
 *	Frees the buffer's storage, freeing any malloc'ed space.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
FreeSinkBuffer(sinkPtr)
    Sink *sinkPtr;
{
    if (sinkPtr->byteArr != sinkPtr->staticSpace) {
	Blt_Free(sinkPtr->byteArr);
    }
    sinkPtr->fd = -1;
    sinkPtr->file = (Tcl_File)NULL;
#if (TCL_MAJOR_VERSION >= 8)
    if (sinkPtr->objv != NULL) {
	register int i;

	for (i = 0; i < sinkPtr->objc - 1; i++) {
	    Tcl_DecrRefCount(sinkPtr->objv[i]);
	}
	Blt_Free(sinkPtr->objv);
    }
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * ExtendSinkBuffer --
 *
 *	Doubles the size of the current buffer.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static int
ExtendSinkBuffer(sinkPtr)
    Sink *sinkPtr;
{
    unsigned char *arrayPtr;
    register unsigned char *srcPtr, *destPtr, *endPtr;
    /*
     * Allocate a new array, double the old size
     */
    sinkPtr->size += sinkPtr->size;
    arrayPtr = Blt_Malloc(sizeof(unsigned char) * sinkPtr->size);
    if (arrayPtr == NULL) {
	return -1;
    }
    srcPtr = sinkPtr->byteArr;
    endPtr = sinkPtr->byteArr + sinkPtr->fill;
    destPtr = arrayPtr;
    while (srcPtr < endPtr) {
	*destPtr++ = *srcPtr++;
    }
    if (sinkPtr->byteArr != sinkPtr->staticSpace) {
	Blt_Free(sinkPtr->byteArr);
    }
    sinkPtr->byteArr = arrayPtr;

    return (sinkPtr->size - sinkPtr->fill); /* Return bytes left. */
}

/*
 *----------------------------------------------------------------------
 *
 * ReadBytes --
 *
 *	Reads and appends any available data from a given file descriptor
 *	to the buffer.
 *
 * Results:
 *	Returns TCL_OK when EOF is found, TCL_RETURN if reading
 *	data would block, and TCL_ERROR if an error occurred.
 *
 *----------------------------------------------------------------------
 */
static void
ReadBytes(sinkPtr)
    Sink *sinkPtr;
{
    int nBytes, bytesLeft;
    register int i;
    unsigned char *array;

    /*
     * ------------------------------------------------------------------
     *
     * 	Worry about indefinite postponement.
     *
     * 	Typically we want to stay in the read loop as long as it takes
     * 	to collect all the data that's currently available.  But if
     * 	it's coming in at a constant high rate, we need to arbitrarily
     * 	break out at some point. This allows for both setting the
     * 	update variable and the Tk program to handle idle events.
     *
     * ------------------------------------------------------------------
     */

    for (i = 0; i < MAX_READS; i++) {

	/* Allocate a larger buffer when the number of remaining bytes
	 * is below the threshold BLOCK_SIZE.  */

	bytesLeft = sinkPtr->size - sinkPtr->fill;

	if (bytesLeft < BLOCK_SIZE) {
	    bytesLeft = ExtendSinkBuffer(sinkPtr);
	    if (bytesLeft < 0) {
		sinkPtr->status = READ_ERROR;
		return;
	    }
	}
	array = sinkPtr->byteArr + sinkPtr->fill;

	/*
	 * Read into a buffer but make sure we leave room for a
	 * trailing NUL byte.
	 */
	nBytes = read(sinkPtr->fd, array, bytesLeft - 1);
	if (nBytes == 0) {	/* EOF: break out of loop. */
	    sinkPtr->status = READ_EOF;
	    return;
	}
	if (nBytes < 0) {
#ifdef O_NONBLOCK
#define BLOCKED		EAGAIN
#else
#define BLOCKED		EWOULDBLOCK
#endif /*O_NONBLOCK*/
	    /* Either an error has occurred or no more data is
	     * currently available to read.  */
	    if (errno == BLOCKED) {
		sinkPtr->status = READ_AGAIN;
		return;
	    }
	    sinkPtr->byteArr[0] = '\0';
	    sinkPtr->status = READ_ERROR;
	    return;
	}
	sinkPtr->fill += nBytes;
	sinkPtr->byteArr[sinkPtr->fill] = '\0';
    }
    sinkPtr->status = nBytes;
}

#define IsOpenSink(sinkPtr)  ((sinkPtr)->fd != -1)

static void
CloseSink(interp, sinkPtr) 
    Tcl_Interp *interp;
    Sink *sinkPtr;
{
    if (IsOpenSink(sinkPtr)) {
	close(sinkPtr->fd);
#ifdef FILEHANDLER_USES_TCLFILES
	Tcl_DeleteFileHandler(sinkPtr->file);
	Tcl_FreeFile(sinkPtr->file);
#else
	Tcl_DeleteFileHandler(sinkPtr->fd);
#endif
	sinkPtr->file = (Tcl_File)NULL;
	sinkPtr->fd = -1;

#if WINDEBUG
	PurifyPrintf("CloseSink: set done var %s\n", sinkPtr->name);
#endif
	if (sinkPtr->doneVar != NULL) {
	    unsigned char *data;
	    size_t length;
	    /* 
	     * If data is to be collected, set the "done" variable
	     * with the contents of the buffer.  
	     */
	    GetSinkData(sinkPtr, &data, &length);
#if (TCL_VERSION_NUMBER <  _VERSION(8,1,0)) 
	    data[length] = '\0';
	    if (Tcl_SetVar(interp, sinkPtr->doneVar, data, 
			   TCL_GLOBAL_ONLY) == NULL) {
		Tcl_BackgroundError(interp);
	    }
#else
	    if (Tcl_SetVar2Ex(interp, sinkPtr->doneVar, NULL, 
			      Tcl_NewByteArrayObj(data, length),
			      (TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG)) == NULL) {
		Tcl_BackgroundError(interp);
	    }
#endif
	}
#if WINDEBUG
	PurifyPrintf("CloseSink %s: done\n", sinkPtr->name);
#endif
    }
}
/*
 *----------------------------------------------------------------------
 *
 * CookSink --
 *
 *	For Windows, translate CR/NL combinations to NL alone.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	The size of the byte array may shrink and array contents
 *	shifted as carriage returns are found and removed.
 *
 *----------------------------------------------------------------------
 */
static void
CookSink(interp, sinkPtr)
    Tcl_Interp *interp;
    Sink *sinkPtr;
{
    unsigned char *srcPtr, *endPtr;
#ifdef WIN32
    size_t oldMark;

    oldMark = sinkPtr->mark;
#endif
    if (sinkPtr->encoding == ENCODING_BINARY) { /* binary */
	/* No translation needed. */
	sinkPtr->mark = sinkPtr->fill; 
    } else if (sinkPtr->encoding == ENCODING_ASCII) { /* ascii */
#if (TCL_VERSION_NUMBER <  _VERSION(8,1,0)) 
	/* Convert NUL bytes to question marks. */
	srcPtr = sinkPtr->byteArr + sinkPtr->mark;
	endPtr = sinkPtr->byteArr + sinkPtr->fill;
	while (srcPtr < endPtr) {
	    if (*srcPtr == '\0') {
		*srcPtr = '?';
	    }
	    srcPtr++;
	}
#endif /* < 8.1.0 */
	/* One-to-one translation. mark == fill. */
	sinkPtr->mark = sinkPtr->fill;
#if (TCL_VERSION_NUMBER >= _VERSION(8,1,0)) 
    } else { /* unicode. */
	int nSrcCooked, nCooked;
	int result;
	size_t cookedSize, spaceLeft, needed;
	size_t nRaw, nLeftOver;
	unsigned char *destPtr;
	unsigned char *raw, *cooked;
	unsigned char leftover[100];
	
	raw = sinkPtr->byteArr + sinkPtr->mark;
	nRaw = sinkPtr->fill - sinkPtr->mark;
	/* Ideally, the cooked buffer size should be smaller */
	cookedSize = nRaw * TCL_UTF_MAX + 1;
	cooked = Blt_Malloc(cookedSize);
	result = Tcl_ExternalToUtf(interp, sinkPtr->encoding, 
			(char *)raw, nRaw, 0, NULL, (char *)cooked, 
			cookedSize, &nSrcCooked, &nCooked, NULL);
	nLeftOver = 0;
	if (result == TCL_CONVERT_MULTIBYTE) {
	    /* 
	     * Last multibyte sequence wasn't completed.  Save the
	     * extra characters in a temporary buffer.  
	     */
	    nLeftOver = (nRaw - nSrcCooked);
	    srcPtr = sinkPtr->byteArr + (sinkPtr->mark + nSrcCooked); 
	    endPtr = srcPtr + nLeftOver;
	    destPtr = leftover;
	    while (srcPtr < endPtr) {
		*destPtr++ = *srcPtr++;
	    }
	} 
	/*
	 * Create a bigger 
	 */
						 
	needed = nLeftOver + nCooked;
	spaceLeft = sinkPtr->size - sinkPtr->mark;
	if (spaceLeft >= needed) {
	    spaceLeft = ExtendSinkBuffer(sinkPtr);
	}
	assert(spaceLeft > needed);
	/* 
	 * Replace the characters from the mark with the translated 
	 * characters.
	 */
	srcPtr = cooked;
	endPtr = cooked + nCooked;
	destPtr = sinkPtr->byteArr + sinkPtr->mark;
	while (srcPtr < endPtr) {
	    *destPtr++ = *srcPtr++;
	}
	/* Add the number of newly translated characters to the mark */
	sinkPtr->mark += nCooked;
	
	srcPtr = leftover;
	endPtr = leftover + nLeftOver;
	while (srcPtr < endPtr) {
	    *destPtr++ = *srcPtr++;
	}
	sinkPtr->fill = sinkPtr->mark + nLeftOver;
#endif /* >= 8.1.0  */
    }
#ifdef WIN32
    /* 
     * Translate CRLF character sequences to LF characters.  We have to
     * do this after converting the string to UTF from UNICODE.
     */
    if (sinkPtr->encoding != ENCODING_BINARY) {
	int count;
	unsigned char *destPtr;

	destPtr = srcPtr = sinkPtr->byteArr + oldMark;
	endPtr = sinkPtr->byteArr + sinkPtr->fill;
	*endPtr = '\0';
	count = 0;
	for (endPtr--; srcPtr < endPtr; srcPtr++) {
	    if ((*srcPtr == '\r') && (*(srcPtr + 1) == '\n')) {
		count++;
		continue;		/* Skip the CR in CR/LF sequences. */
	    }
	    if (srcPtr != destPtr) {
		*destPtr = *srcPtr;	/* Collapse the string, overwriting
					 * the \r's encountered. */
	    }
	    destPtr++;
	}
	sinkPtr->mark -= count;
	sinkPtr->fill -= count;
	*destPtr = *srcPtr;	/* Copy the last byte */
	if (*destPtr == '\r') {
	    sinkPtr->mark--;
	}
    }
#endif /* WIN32 */
}

#ifdef WIN32
/*
 *----------------------------------------------------------------------
 *
 * WaitProcess --
 *
 *	Emulates the waitpid system call under the Win32 API.
 *
 * Results:
 *	Returns 0 if the process is still alive, -1 on an error, or
 *	the pid on a clean close.
 *
 * Side effects:
 *	Unless WNOHANG is set and the wait times out, the process
 *	information record will be deleted and the process handle
 *	will be closed.
 *
 *----------------------------------------------------------------------
 */
#define WINDEBUG 0
static int
WaitProcess(
    Process child,
    int *statusPtr,
    int flags)
{
    int result;
    DWORD status, exitCode;
    int timeout;

#if WINDEBUG
    PurifyPrintf("WAITPID(%x)\n", child.hProcess);
#endif
    *statusPtr = 0;
    if (child.hProcess == INVALID_HANDLE_VALUE) {
	errno = EINVAL;
	return -1;
    }
#if WINDEBUG
    PurifyPrintf("WAITPID: waiting for 0x%x\n", child.hProcess);
#endif
    timeout = (flags & WNOHANG) ? 0 : INFINITE;
    status = WaitForSingleObject(child.hProcess, timeout);
				 
#if WINDEBUG
    PurifyPrintf("WAITPID: wait status is %d\n", status);
#endif
    switch (status) {
    case WAIT_FAILED:
	errno = ECHILD;
	*statusPtr = ECHILD;
	result = -1;
	break;

    case WAIT_TIMEOUT:
	if (timeout == 0) {
	    return 0;		/* Try again */
	}
	result = 0;
	break;

    default:
    case WAIT_ABANDONED:
    case WAIT_OBJECT_0:
	GetExitCodeProcess(child.hProcess, &exitCode);
	*statusPtr = ((exitCode << 8) & 0xff00);
#if WINDEBUG
	PurifyPrintf("WAITPID: exit code of %d is %d (%x)\n", child.hProcess,
	    *statusPtr, exitCode);
#endif
	result = child.pid;
	assert(result != -1);
	break;
    }
    CloseHandle(child.hProcess);
    return result;
}

static BOOL CALLBACK
EnumWindowsProc(HWND hWnd, LPARAM lParam)
{
    DWORD pid = 0;
    Process *procPtr = (Process *)lParam;

    GetWindowThreadProcessId(hWnd, &pid);
    if (pid == procPtr->pid) {
	PostMessage(hWnd, WM_CLOSE, 0, 0);
    }
    return TRUE;
}

/*
 *----------------------------------------------------------------------
 *
 * KillProcess --
 *
 *	Emulates the UNIX kill system call under Win32 API.
 *
 * Results:
 *	Returns 0 if the process is killed, -1 on an error.
 *
 * Side effects:
 *	Process is terminated.
 *
 *----------------------------------------------------------------------
 */
static int
KillProcess(Process proc, int signal)
{
    DWORD status;

    if ((proc.hProcess == NULL) || (proc.hProcess == INVALID_HANDLE_VALUE)) {
	errno = EINVAL;
	return -1;
    }

    EnumWindows(EnumWindowsProc, (LPARAM)&proc);

    /* 
     * Wait on the handle. If it signals, great. If it times out,
     * then call TerminateProcess on it.  
     *
     * On Windows 95/98 this also has the added benefit of stopping
     * KERNEL32.dll from dumping.  The 2 second number is arbitrary.
     * (1 second seems to fail intermittently).
     */
    status = WaitForSingleObject(proc.hProcess, 2000);
    if (status == WAIT_OBJECT_0) {
	return 0;
    }
    if (!TerminateProcess(proc.hProcess, 1)) {
#if WINDEBUG
	PurifyPrintf("can't terminate process (handle=%d): %s\n",
		     proc.hProcess, Blt_LastError());
#endif /* WINDEBUG */
	return -1;
    }
    return 0;
}

#endif /* WIN32 */

#if (TCL_VERSION_NUMBER < _VERSION(8,1,0)) 

static void
NotifyOnUpdate(interp, sinkPtr, data, nBytes)
    Tcl_Interp *interp;
    Sink *sinkPtr;
    unsigned char *data;
    int nBytes;
{
    char save;

#if WINDEBUG_0
    PurifyPrintf("read %s\n", data);
#endif
    if (data[0] == '\0') {
	return;
    }
    save = data[nBytes];
    data[nBytes] = '\0';
    if (sinkPtr->echo) {
	Tcl_Channel channel;
	
	channel = Tcl_GetStdChannel(TCL_STDERR);
	if (channel == NULL) {
	    Tcl_AppendResult(interp, "can't get stderr channel", (char *)NULL);
	    Tcl_BackgroundError(interp);
	    sinkPtr->echo = FALSE;
	} else {
	    Tcl_Write(channel, data, nBytes);
	    if (save == '\n') {
		Tcl_Write(channel, "\n", 1);
	    }
	    Tcl_Flush(channel);
	}
    }
    if (sinkPtr->updateCmd != NULL) {
	Tcl_DString dString;
	int result;
	register char **p;

	Tcl_DStringInit(&dString);
	for (p = sinkPtr->updateCmd; *p != NULL; p++) {
	    Tcl_DStringAppendElement(&dString, *p);
	}
	Tcl_DStringAppendElement(&dString, data);
	result = Tcl_GlobalEval(interp, Tcl_DStringValue(&dString));
	Tcl_DStringFree(&dString);
	if (result != TCL_OK) {
	    Tcl_BackgroundError(interp);
	}
    }
    if (sinkPtr->updateVar != NULL) {
	int flags;
	char *result;

	flags = (TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG);
	result = Tcl_SetVar(interp, sinkPtr->updateVar, data, flags);
	if (result == NULL) {
	    Tcl_BackgroundError(interp);
	}
    }
    data[nBytes] = save;
}

#else 

static void
NotifyOnUpdate(interp, sinkPtr, data, nBytes)
    Tcl_Interp *interp;
    Sink *sinkPtr;
    unsigned char *data;
    int nBytes;
{
    Tcl_Obj *objPtr;

#if WINDEBUG_0
    PurifyPrintf("read %s\n", data);
#endif
    if ((nBytes == 0) || (data[0] == '\0')) {
	return;
    }
    if (sinkPtr->echo) {
	Tcl_Channel channel;
	
	channel = Tcl_GetStdChannel(TCL_STDERR);
	if (channel == NULL) {
	    Tcl_AppendResult(interp, "can't get stderr channel", (char *)NULL);
	    Tcl_BackgroundError(interp);
	    sinkPtr->echo = FALSE;
	} else {
	    if (data[nBytes] == '\n') {
		objPtr = Tcl_NewByteArrayObj(data, nBytes + 1);
	    } else {
		objPtr = Tcl_NewByteArrayObj(data, nBytes);
	    }
	    Tcl_WriteObj(channel, objPtr);
	    Tcl_Flush(channel);
	}
    }

    objPtr = Tcl_NewByteArrayObj(data, nBytes);
    Tcl_IncrRefCount(objPtr);
    if (sinkPtr->objv != NULL) {
	int result;

	sinkPtr->objv[sinkPtr->objc - 1] = objPtr;
	result = Tcl_EvalObjv(interp, sinkPtr->objc, sinkPtr->objv, 0);
	if (result != TCL_OK) {
	    Tcl_BackgroundError(interp);
	}
    }
    if (sinkPtr->updateVar != NULL) {
	Tcl_Obj *result;

	result = Tcl_SetVar2Ex(interp, sinkPtr->updateVar, NULL, objPtr, 
	       (TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG));
	if (result == NULL) {
	    Tcl_BackgroundError(interp);
	}
    }
    Tcl_DecrRefCount(objPtr);
}

#endif /* < 8.1.0 */

static int
CollectData(bgPtr, sinkPtr)
    BackgroundInfo *bgPtr;
    Sink *sinkPtr;
{
    if ((bgPtr->detached) && (sinkPtr->doneVar == NULL)) {
	ResetSink(sinkPtr);
    }
    ReadBytes(sinkPtr);
    CookSink(bgPtr->interp, sinkPtr);
    if ((sinkPtr->mark > sinkPtr->lastMark) && 
	(sinkPtr->flags & SINK_NOTIFY)) {
	unsigned char *data;
	int length;

	if (sinkPtr->flags & SINK_BUFFERED) {
	    /* For line-by-line updates, call NotifyOnUpdate for each
	     * new complete line.  */
	    while ((data = NextLine(sinkPtr, &length)) != NULL) {
		NotifyOnUpdate(bgPtr->interp, sinkPtr, data, length);
 	    }
	} else {
	    data = NextBlock(sinkPtr, &length);
	    NotifyOnUpdate(bgPtr->interp, sinkPtr, data, length);
	}
    }
    if (sinkPtr->status >= 0) {
	return TCL_OK;
    }
    if (sinkPtr->status == READ_ERROR) {
	Tcl_AppendResult(bgPtr->interp, "can't read data from ", sinkPtr->name,
	    ": ", Tcl_PosixError(bgPtr->interp), (char *)NULL);
	Tcl_BackgroundError(bgPtr->interp);
	return TCL_ERROR;
    }
#if WINDEBUG
    PurifyPrintf("CollectData %s: done\n", sinkPtr->name);
#endif
    return TCL_RETURN;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateSinkHandler --
 *
 *	Creates a file handler for the given sink.  The file
 *	descriptor is also set for non-blocking I/O.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The memory allocated to the BackgroundInfo structure released.
 *
 *----------------------------------------------------------------------
 */
static int
CreateSinkHandler(bgPtr, sinkPtr, proc)
    BackgroundInfo *bgPtr;
    Sink *sinkPtr;
    Tcl_FileProc *proc;
{
#ifndef WIN32
    int flags;

    flags = fcntl(sinkPtr->fd, F_GETFL);
#ifdef O_NONBLOCK
    flags |= O_NONBLOCK;
#else
    flags |= O_NDELAY;
#endif
    if (fcntl(sinkPtr->fd, F_SETFL, flags) < 0) {
	Tcl_AppendResult(bgPtr->interp, "can't set file descriptor ",
	    Blt_Itoa(sinkPtr->fd), " to non-blocking:",
	    Tcl_PosixError(bgPtr->interp), (char *)NULL);
	return TCL_ERROR;
    }
#endif /* WIN32 */
#ifdef FILEHANDLER_USES_TCLFILES
    sinkPtr->file = Tcl_GetFile((ClientData)sinkPtr->fd, TCL_UNIX_FD);
    Tcl_CreateFileHandler(sinkPtr->file, TCL_READABLE, proc, bgPtr);
#else
    Tcl_CreateFileHandler(sinkPtr->fd, TCL_READABLE, proc, bgPtr);
#endif /* FILEHANDLER_USES_TCLFILES */
    return TCL_OK;
}

static void
DisableTriggers(bgPtr)
    BackgroundInfo *bgPtr;	/* Background info record. */
{

    if (bgPtr->traced) {
	Tcl_UntraceVar(bgPtr->interp, bgPtr->statVar, TRACE_FLAGS, 
		VariableProc, bgPtr);
	bgPtr->traced = FALSE;
    }
    if (IsOpenSink(&bgPtr->sink1)) {
	CloseSink(bgPtr->interp, &bgPtr->sink1);
    }
    if (IsOpenSink(&bgPtr->sink2)) {
	CloseSink(bgPtr->interp, &bgPtr->sink2);
    }
    if (bgPtr->timerToken != (Tcl_TimerToken) 0) {
	Tcl_DeleteTimerHandler(bgPtr->timerToken);
	bgPtr->timerToken = 0;
    }
    if (bgPtr->donePtr != NULL) {
	*bgPtr->donePtr = TRUE;
    }
}


/*
 *----------------------------------------------------------------------
 *
 * FreeBackgroundInfo --
 *
 *	Releases the memory allocated for the backgrounded process.
 *
 *----------------------------------------------------------------------
 */
static void
FreeBackgroundInfo(bgPtr)
    BackgroundInfo *bgPtr;
{
    Blt_FreeSwitches(switchSpecs, (char *)bgPtr, 0);
    if (bgPtr->statVar != NULL) {
	Blt_Free(bgPtr->statVar);
    }
    if (bgPtr->procArr != NULL) {
	Blt_Free(bgPtr->procArr);
    }
    Blt_Free(bgPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyBackgroundInfo --
 *
 * 	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 * 	to clean up the internal structure (BackgroundInfo) at a safe
 * 	time (when no one is using it anymore).
 *
 * Results:
 *	None.b
 *
 * Side effects:
 *	The memory allocated to the BackgroundInfo structure released.
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static void
DestroyBackgroundInfo(bgPtr)
    BackgroundInfo *bgPtr;	/* Background info record. */
{
    DisableTriggers(bgPtr);
    FreeSinkBuffer(&bgPtr->sink2);
    FreeSinkBuffer(&bgPtr->sink1);
    if (bgPtr->procArr != NULL) {
	register int i;

	for (i = 0; i < bgPtr->nProcs; i++) {
	    if (bgPtr->signalNum > 0) {
		kill(bgPtr->procArr[i], bgPtr->signalNum);
	    }
#ifdef WIN32
	    Tcl_DetachPids(1, (Tcl_Pid *)&bgPtr->procArr[i].pid);
#else
#if (TCL_MAJOR_VERSION == 7)
	    Tcl_DetachPids(1, &bgPtr->procArr[i]);
#else
	    Tcl_DetachPids(1, (Tcl_Pid *)bgPtr->procArr[i]);
#endif /* TCL_MAJOR_VERSION == 7 */
#endif /* WIN32 */
	}
    }
    FreeBackgroundInfo(bgPtr);
    Tcl_ReapDetachedProcs();
}

/*
 * ----------------------------------------------------------------------
 *
 * VariableProc --
 *
 *	Kills all currently running subprocesses (given the specified
 *	signal). This procedure is called when the user sets the status
 *	variable associated with this group of child subprocesses.
 *
 * Results:
 *	Always returns NULL.  Only called from a variable trace.
 *
 * Side effects:
 *	The subprocesses are signaled for termination using the
 *	specified kill signal.  Additionally, any resources allocated
 *	to track the subprocesses is released.
 *
 * ----------------------------------------------------------------------
 */
/* ARGSUSED */
static char *
VariableProc(
    ClientData clientData,	/* File output information. */
    Tcl_Interp *interp,
    char *part1,
    char *part2,		/* Not Used. */
    int flags)
{
    if (flags & TRACE_FLAGS) {
	BackgroundInfo *bgPtr = clientData;

	/* Kill all child processes that remain alive. */
	if ((bgPtr->procArr != NULL) && (bgPtr->signalNum > 0)) {
	    register int i;

	    for (i = 0; i < bgPtr->nProcs; i++) {
		kill(bgPtr->procArr[i], bgPtr->signalNum);
	    }
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TimerProc --
 *
 *	This is a timer handler procedure which gets called
 *	periodically to reap any of the sub-processes if they have
 *	terminated.  After the last process has terminated, the
 *	contents of standard output are stored
 *	in the output variable, which triggers the cleanup proc (using
 *	a variable trace). The status the last process to exit is
 *	written to the status variable.
 *
 * Results:
 *	None.  Called from the Tcl event loop.
 *
 * Side effects:
 *	Many. The contents of procArr is shifted, leaving only those
 *	sub-processes which have not yet terminated.  If there are
 *	still subprocesses left, this procedure is placed in the timer
 *	queue again. Otherwise the output and possibly the status
 *	variables are updated.  The former triggers the cleanup
 *	routine which will destroy the information and resources
 *	associated with these background processes.
 *
 *----------------------------------------------------------------------
 */
static void
TimerProc(clientData)
    ClientData clientData;
{
    BackgroundInfo *bgPtr = clientData;
    register int i;
    unsigned int lastPid;
    int pid;
    enum PROCESS_STATUS { 
	PROCESS_EXITED, PROCESS_STOPPED, PROCESS_KILLED, PROCESS_UNKNOWN
    } pcode;
    WAIT_STATUS_TYPE waitStatus, lastStatus;
    int nLeft;			/* Number of processes still not reaped */
    char string[200];
    Tcl_DString dString;
    int code;
    CONST char *result;

    lastPid = (unsigned int)-1;
    *((int *)&waitStatus) = 0;
    *((int *)&lastStatus) = 0;

    nLeft = 0;
    for (i = 0; i < bgPtr->nProcs; i++) {
#ifdef WIN32
	pid = WaitProcess(bgPtr->procArr[i], (int *)&waitStatus, WNOHANG);
#else
	pid = waitpid(bgPtr->procArr[i], (int *)&waitStatus, WNOHANG);
#endif
	if (pid == 0) {		/*  Process has not terminated yet */
	    if (nLeft < i) {
		bgPtr->procArr[nLeft] = bgPtr->procArr[i];
	    }
	    nLeft++;		/* Count the number of processes left */
	} else if (pid != -1) {
	    /*
	     * Save the status information associated with the subprocess.
	     * We'll use it only if this is the last subprocess to be reaped.
	     */
	    lastStatus = waitStatus;
	    lastPid = (unsigned int)pid;
	}
    }
    bgPtr->nProcs = nLeft;

    if ((nLeft > 0) || (IsOpenSink(&bgPtr->sink1)) || 
	(IsOpenSink(&bgPtr->sink2))) {
	/* Keep polling for the status of the children that are left */
	bgPtr->timerToken = Tcl_CreateTimerHandler(bgPtr->interval, TimerProc,
	   bgPtr);
#if WINDEBUG
	PurifyPrintf("schedule TimerProc(nProcs=%d)\n", nLeft);
#endif
	return;
    }

    /*
     * All child processes have completed.  Set the status variable
     * with the status of the last process reaped.  The status is a
     * list of an error token, the exit status, and a message.
     */

    code = WEXITSTATUS(lastStatus);
    Tcl_DStringInit(&dString);
    if (WIFEXITED(lastStatus)) {
	Tcl_DStringAppendElement(&dString, "EXITED");
	pcode = PROCESS_EXITED;
    } else if (WIFSIGNALED(lastStatus)) {
	Tcl_DStringAppendElement(&dString, "KILLED");
	pcode = PROCESS_KILLED;
	code = -1;
    } else if (WIFSTOPPED(lastStatus)) {
	Tcl_DStringAppendElement(&dString, "STOPPED");
	pcode = PROCESS_STOPPED;
	code = -1;
    } else {
	Tcl_DStringAppendElement(&dString, "UNKNOWN");
	pcode = PROCESS_UNKNOWN;
    }
#ifdef WIN32
    sprintf(string, "%u", lastPid);
    Tcl_DStringAppendElement(&dString, string);
#else
    Tcl_DStringAppendElement(&dString, Blt_Itoa(lastPid));
#endif
    Tcl_DStringAppendElement(&dString, Blt_Itoa(code));
    switch(pcode) {
    case PROCESS_EXITED:
	Tcl_DStringAppendElement(&dString, "child completed normally");
	break;
    case PROCESS_KILLED:
	Tcl_DStringAppendElement(&dString, 
				Tcl_SignalMsg((int)(WTERMSIG(lastStatus))));
	break;
    case PROCESS_STOPPED:
	Tcl_DStringAppendElement(&dString, 
		 Tcl_SignalMsg((int)(WSTOPSIG(lastStatus))));
	break;
    case PROCESS_UNKNOWN:
	sprintf(string, "child completed with unknown status 0x%x",
	    *((int *)&lastStatus));
	Tcl_DStringAppendElement(&dString, string);
	break;
    }
    if (bgPtr->exitCodePtr != NULL) {
	*bgPtr->exitCodePtr = code;
    }
    DisableTriggers(bgPtr);
    result = Tcl_SetVar(bgPtr->interp, bgPtr->statVar, 
	Tcl_DStringValue(&dString), TCL_GLOBAL_ONLY);
    Tcl_DStringFree(&dString);
    if (result == NULL) {
	Tcl_BackgroundError(bgPtr->interp);
    }
    if (bgPtr->detached) {
	DestroyBackgroundInfo(bgPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Stdoutproc --
 *
 *	This procedure is called when output from the detached pipeline
 *	is available.  The output is read and saved in a buffer in the
 *	BackgroundInfo structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Data is stored in the buffer.  This character array may
 *	be increased as more space is required to contain the output
 *	of the pipeline.
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static void
StdoutProc(clientData, mask)
    ClientData clientData;	/* File output information. */
    int mask;			/* Not used. */
{
    BackgroundInfo *bgPtr = clientData;

    if (CollectData(bgPtr, &bgPtr->sink1) == TCL_OK) {
	return;
    }
    /*
     * Either EOF or an error has occurred.  In either case, close the
     * sink. Note that closing the sink will also remove the file
     * handler, so this routine will not be called again.
     */
    CloseSink(bgPtr->interp, &bgPtr->sink1);

    /*
     * If both sinks (stdout and stderr) are closed, this doesn't
     * necessarily mean that the process has terminated.  Set up a
     * timer handler to periodically poll for the exit status of each
     * process.  Initially check at the next idle interval.
     */
    if (!IsOpenSink(&bgPtr->sink2)) {
	bgPtr->timerToken = Tcl_CreateTimerHandler(0, TimerProc, clientData);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * StderrProc --
 *
 *	This procedure is called when error from the detached pipeline
 *	is available.  The error is read and saved in a buffer in the
 *	BackgroundInfo structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Data is stored in the buffer.  This character array may
 *	be increased as more space is required to contain the stderr
 *	of the pipeline.
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static void
StderrProc(clientData, mask)
    ClientData clientData;	/* File output information. */
    int mask;			/* Not used. */
{
    BackgroundInfo *bgPtr = clientData;

    if (CollectData(bgPtr, &bgPtr->sink2) == TCL_OK) {
	return;
    }
    /*
     * Either EOF or an error has occurred.  In either case, close the
     * sink. Note that closing the sink will also remove the file
     * handler, so this routine will not be called again.
     */
    CloseSink(bgPtr->interp, &bgPtr->sink2);

    /*
     * If both sinks (stdout and stderr) are closed, this doesn't
     * necessarily mean that the process has terminated.  Set up a
     * timer handler to periodically poll for the exit status of each
     * process.  Initially check at the next idle interval.
     */
    if (!IsOpenSink(&bgPtr->sink1)) {
	bgPtr->timerToken = Tcl_CreateTimerHandler(0, TimerProc, clientData);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * BgexecCmd --
 *
 *	This procedure is invoked to process the "bgexec" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */
/* ARGSUSED */
static int
BgexecCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Thread-specific data. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    int *outFdPtr, *errFdPtr;
    int nProcs;
    Process *pidPtr;
    char *lastArg;
    BackgroundInfo *bgPtr;
    int i;
    int detached;
    Tcl_Encoding encoding;

    if (argc < 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " varName ?options? command ?arg...?\"", (char *)NULL);
	return TCL_ERROR;
    }

    /* Check if the command line is to be run detached (the last
     * argument is "&") */
    lastArg = argv[argc - 1];
    detached = ((lastArg[0] == '&') && (lastArg[1] == '\0'));
    if (detached) {
	argc--;
	argv[argc] = NULL;	/* Remove the '&' argument */
    }
    bgPtr = Blt_Calloc(1, sizeof(BackgroundInfo));
    assert(bgPtr);

    /* Initialize the background information record */
    bgPtr->interp = interp;
    bgPtr->signalNum = SIGKILL;
    bgPtr->nProcs = -1;
    bgPtr->interval = 1000;
    bgPtr->detached = detached;
    bgPtr->keepNewline = FALSE;
    bgPtr->statVar = Blt_Strdup(argv[1]);

    /* Try to clean up any detached processes */
    Tcl_ReapDetachedProcs();

    i = Blt_ProcessSwitches(interp, switchSpecs, argc - 2, argv + 2, 
	(char *)bgPtr, BLT_SWITCH_ARGV_PARTIAL);
    if (i < 0) {
	FreeBackgroundInfo(bgPtr);
	return TCL_ERROR;
    }
    i += 2;
    /* Must be at least one argument left as the command to execute. */
    if (argc <= i) {
	Tcl_AppendResult(interp, "missing command to execute: should be \"",
	    argv[0], " varName ?options? command ?arg...?\"", (char *)NULL);
	FreeBackgroundInfo(bgPtr);
	return TCL_ERROR;
    }

    /* Put a trace on the exit status variable.  The will also allow
     * the user to prematurely terminate the pipeline by simply
     * setting it.  */
    Tcl_TraceVar(interp, bgPtr->statVar, TRACE_FLAGS, VariableProc, bgPtr);
    bgPtr->traced = TRUE;

    encoding = ENCODING_ASCII;
    if (bgPtr->outputEncodingName != NULL) {
	if (strcmp(bgPtr->outputEncodingName, "binary") == 0) {
	    encoding = ENCODING_BINARY;
	} else {
#if (TCL_VERSION_NUMBER >= _VERSION(8,1,0)) 
	    encoding = Tcl_GetEncoding(interp, bgPtr->outputEncodingName);
	    if (encoding == NULL) {
		goto error;
	    }
#endif
	}
    }
    InitSink(bgPtr, &bgPtr->sink1, "stdout", encoding);
    if (bgPtr->errorEncodingName != NULL) {
	if (strcmp(bgPtr->errorEncodingName, "binary") == 0) {
	    encoding = ENCODING_BINARY;
	} else {
#if (TCL_VERSION_NUMBER >= _VERSION(8,1,0)) 
	    encoding = Tcl_GetEncoding(interp, bgPtr->errorEncodingName);
	    if (encoding == NULL) {
		goto error;
	    }
#endif
	}
    }
    InitSink(bgPtr, &bgPtr->sink2, "stderr", encoding);

    outFdPtr = errFdPtr = (int *)NULL;
#ifdef WIN32
    if ((!bgPtr->detached) || 
	(bgPtr->sink1.doneVar != NULL) || 
	(bgPtr->sink1.updateVar != NULL) || 
	(bgPtr->sink1.updateCmd != NULL)) {
	outFdPtr = &bgPtr->sink1.fd;
    }
#else
    outFdPtr = &bgPtr->sink1.fd;
#endif
    if ((bgPtr->sink2.doneVar != NULL) || 
	(bgPtr->sink2.updateVar != NULL) ||
	(bgPtr->sink2.updateCmd != NULL) || 
	(bgPtr->sink2.echo)) {
	errFdPtr = &bgPtr->sink2.fd;
    }
    nProcs = Blt_CreatePipeline(interp, argc - i, argv + i, &pidPtr,
	(int *)NULL, outFdPtr, errFdPtr);
    if (nProcs < 0) {
	goto error;
    }
    bgPtr->procArr = pidPtr;
    bgPtr->nProcs = nProcs;

    if (bgPtr->sink1.fd == -1) {

	/* If output has been redirected, start polling immediately
	 * for the exit status of each process.  Normally, this is
	 * done only after stdout has been closed by the last process,
	 * but here stdout has been redirected. The default polling
	 * interval is every 1 second.  */

	bgPtr->timerToken = Tcl_CreateTimerHandler(bgPtr->interval, TimerProc,
	   bgPtr);

    } else if (CreateSinkHandler(bgPtr, &bgPtr->sink1, StdoutProc) != TCL_OK) {
	goto error;
    }
    if ((bgPtr->sink2.fd != -1) &&
	(CreateSinkHandler(bgPtr, &bgPtr->sink2, StderrProc) != TCL_OK)) {
 	goto error;
    }
    if (bgPtr->detached) {	
	char string[200];

	/* If detached, return a list of the child process ids instead
	 * of the output of the pipeline. */
	for (i = 0; i < nProcs; i++) {
#ifdef WIN32
	    sprintf(string, "%u", (unsigned int)bgPtr->procArr[i].pid);
#else 
	    sprintf(string, "%d", bgPtr->procArr[i]);
#endif
	    Tcl_AppendElement(interp, string);
	}
    } else {
	int exitCode;
	int done;

	bgPtr->exitCodePtr = &exitCode;
	bgPtr->donePtr = &done;

	exitCode = done = 0;
	while (!done) {
	    Tcl_DoOneEvent(0);
	}
	DisableTriggers(bgPtr);
	if ((exitCode == 0) && (bgPtr->sink1.doneVar == NULL)) {
	    unsigned char *data;
	    size_t length;

	    /* Return the output of the pipeline. */
	    GetSinkData(&bgPtr->sink1, &data, &length);
#if (TCL_VERSION_NUMBER <  _VERSION(8,1,0)) 
	    data[length] = '\0';
	    Tcl_SetResult(interp, data, TCL_VOLATILE);
#else
	    Tcl_SetObjResult(interp, Tcl_NewByteArrayObj(data, length));
#endif
	}
	/* Clean up resources used. */
	DestroyBackgroundInfo(bgPtr);
	if (exitCode != 0) {
	    Tcl_AppendResult(interp, "child process exited abnormally",
		(char *)NULL);
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
  error:
    DisableTriggers(bgPtr);
    DestroyBackgroundInfo(bgPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_BgexecInit --
 *
 *	This procedure is invoked to initialize the "bgexec" Tcl
 *	command.  See the user documentation for details on what it
 *	does.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */
int
Blt_BgexecInit(interp)
    Tcl_Interp *interp;
{
    static Blt_CmdSpec cmdSpec = {"bgexec", BgexecCmd, };

    if (Blt_InitCmd(interp, "blt", &cmdSpec) == NULL) {
	return TCL_ERROR;
    }
    return TCL_OK;
}
#endif /* NO_BGEXEC */
