/* 
 * tclWinPipe.c --
 *
 *	This file implements the Windows-specific exec pipeline functions,
 *	the "pipe" channel driver, and the "pid" Tcl command.
 *
 * Copyright (c) 1996-1997 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclWinPipe.c 1.49 97/11/06 17:33:03
 */

#include "tclWinInt.h"

#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>

/*
 * The following variable is used to tell whether this module has been
 * initialized.
 */

static int initialized = 0;

/*
 * The following defines identify the various types of applications that 
 * run under windows.  There is special case code for the various types.
 */

#define APPL_NONE	0
#define APPL_DOS	1
#define APPL_WIN3X	2
#define APPL_WIN32	3

/*
 * The following constants and structures are used to encapsulate the state
 * of various types of files used in a pipeline.
 */

#define WIN32S_PIPE 1		/* Win32s emulated pipe. */
#define WIN32S_TMPFILE 2	/* Win32s emulated temporary file. */
#define WIN_FILE 3		/* Basic Win32 file. */

/*
 * This structure encapsulates the common state associated with all file
 * types used in a pipeline.
 */

typedef struct WinFile {
    int type;			/* One of the file types defined above. */
    HANDLE handle;		/* Open file handle. */
} WinFile;

/*
 * The following structure is used to keep track of temporary files under
 * Win32s and delete the disk file when the open handle is closed.
 * The type field will be WIN32S_TMPFILE.
 */

typedef struct TmpFile {
    WinFile file;		/* Common part. */
    char name[MAX_PATH];	/* Name of temp file. */
} TmpFile;

/*
 * The following structure represents a synchronous pipe under Win32s.
 * The type field will be WIN32S_PIPE.  The handle field will refer to
 * an open file when Tcl is reading from the "pipe", otherwise it is
 * INVALID_HANDLE_VALUE.
 */

typedef struct WinPipe {
    WinFile file;		/* Common part. */
    struct WinPipe *otherPtr;	/* Pointer to the WinPipe structure that
				 * corresponds to the other end of this 
				 * pipe. */
    char *fileName;		/* The name of the staging file that gets 
				 * the data written to this pipe.  Malloc'd.
				 * and shared by both ends of the pipe.  Only
				 * when both ends are freed will fileName be
				 * freed and the file it refers to deleted. */
} WinPipe;

/*
 * This list is used to map from pids to process handles.
 */

typedef struct ProcInfo {
    HANDLE hProcess;
    DWORD dwProcessId;
    struct ProcInfo *nextPtr;
} ProcInfo;

static ProcInfo *procList;

/*
 * State flags used in the PipeInfo structure below.
 */

#define PIPE_PENDING	(1<<0)	/* Message is pending in the queue. */
#define PIPE_ASYNC	(1<<1)	/* Channel is non-blocking. */

/*
 * This structure describes per-instance data for a pipe based channel.
 */

typedef struct PipeInfo {
    Tcl_Channel channel;	/* Pointer to channel structure. */
    int validMask;		/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, or TCL_EXCEPTION: indicates
				 * which operations are valid on the file. */
    int watchMask;		/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, or TCL_EXCEPTION: indicates
				 * which events should be reported. */
    int flags;			/* State flags, see above for a list. */
    TclFile readFile;		/* Output from pipe. */
    TclFile writeFile;		/* Input from pipe. */
    TclFile errorFile;		/* Error output from pipe. */
    int numPids;		/* Number of processes attached to pipe. */
    Tcl_Pid *pidPtr;		/* Pids of attached processes. */
    struct PipeInfo *nextPtr;	/* Pointer to next registered pipe. */
} PipeInfo;

/*
 * The following pointer refers to the head of the list of pipes
 * that are being watched for file events.
 */

static PipeInfo *firstPipePtr;

/*
 * The following structure is what is added to the Tcl event queue when
 * pipe events are generated.
 */

typedef struct PipeEvent {
    Tcl_Event header;		/* Information that is standard for
				 * all events. */
    PipeInfo *infoPtr;		/* Pointer to pipe info structure.  Note
				 * that we still have to verify that the
				 * pipe exists before dereferencing this
				 * pointer. */
} PipeEvent;

/*
 * Declarations for functions used only in this file.
 */

static int	ApplicationType(Tcl_Interp *interp, const char *fileName,
		    char *fullName);
static void	BuildCommandLine(int argc, char **argv, Tcl_DString *linePtr);
static void	CopyChannel(HANDLE dst, HANDLE src);
static BOOL	HasConsole(void);
static TclFile	MakeFile(HANDLE handle);
static char *	MakeTempFile(Tcl_DString *namePtr);
static int	PipeBlockModeProc(ClientData instanceData, int mode);
static void	PipeCheckProc _ANSI_ARGS_((ClientData clientData,
		    int flags));
static int	PipeCloseProc(ClientData instanceData, Tcl_Interp *interp);
static int	PipeEventProc(Tcl_Event *evPtr, int flags);
static void	PipeExitHandler(ClientData clientData);
static int	PipeGetHandleProc(ClientData instanceData, int direction,
		    ClientData *handlePtr);
static void	PipeInit(void);
static int	PipeInputProc(ClientData instanceData, char *buf, int toRead,
		    int *errorCode);
static int	PipeOutputProc(ClientData instanceData, char *buf, int toWrite,
		    int *errorCode);
static void	PipeWatchProc(ClientData instanceData, int mask);
static void	PipeSetupProc _ANSI_ARGS_((ClientData clientData,
		    int flags));
static int	TempFileName(char name[MAX_PATH]);

/*
 * This structure describes the channel type structure for command pipe
 * based IO.
 */

static Tcl_ChannelType pipeChannelType = {
    "pipe",			/* Type name. */
    PipeBlockModeProc,		/* Set blocking or non-blocking mode.*/
    PipeCloseProc,		/* Close proc. */
    PipeInputProc,		/* Input proc. */
    PipeOutputProc,		/* Output proc. */
    NULL,			/* Seek proc. */
    NULL,			/* Set option proc. */
    NULL,			/* Get option proc. */
    PipeWatchProc,		/* Set up notifier to watch the channel. */
    PipeGetHandleProc,		/* Get an OS handle from channel. */
};

/*
 *----------------------------------------------------------------------
 *
 * PipeInit --
 *
 *	This function initializes the static variables for this file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates a new event source.
 *
 *----------------------------------------------------------------------
 */

static void
PipeInit()
{
    initialized = 1;
    firstPipePtr = NULL;
    procList = NULL;
    Tcl_CreateEventSource(PipeSetupProc, PipeCheckProc, NULL);
    Tcl_CreateExitHandler(PipeExitHandler, NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * PipeExitHandler --
 *
 *	This function is called to cleanup the pipe module before
 *	Tcl is unloaded.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Removes the pipe event source.
 *
 *----------------------------------------------------------------------
 */

static void
PipeExitHandler(clientData)
    ClientData clientData;	/* Old window proc */
{
    Tcl_DeleteEventSource(PipeSetupProc, PipeCheckProc, NULL);
    initialized = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * PipeSetupProc --
 *
 *	This procedure is invoked before Tcl_DoOneEvent blocks waiting
 *	for an event.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adjusts the block time if needed.
 *
 *----------------------------------------------------------------------
 */

void
PipeSetupProc(data, flags)
    ClientData data;		/* Not used. */
    int flags;			/* Event flags as passed to Tcl_DoOneEvent. */
{
    PipeInfo *infoPtr;
    Tcl_Time blockTime = { 0, 0 };

    if (!(flags & TCL_FILE_EVENTS)) {
	return;
    }
    
    /*
     * Check to see if there is a watched pipe.  If so, poll.
     */

    for (infoPtr = firstPipePtr; infoPtr != NULL; infoPtr = infoPtr->nextPtr) {
	if (infoPtr->watchMask) {
	    Tcl_SetMaxBlockTime(&blockTime);
	    break;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PipeCheckProc --
 *
 *	This procedure is called by Tcl_DoOneEvent to check the pipe
 *	event source for events. 
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May queue an event.
 *
 *----------------------------------------------------------------------
 */

static void
PipeCheckProc(data, flags)
    ClientData data;		/* Not used. */
    int flags;			/* Event flags as passed to Tcl_DoOneEvent. */
{
    PipeInfo *infoPtr;
    PipeEvent *evPtr;

    if (!(flags & TCL_FILE_EVENTS)) {
	return;
    }
    
    /*
     * Queue events for any watched pipes that don't already have events
     * queued.
     */

    for (infoPtr = firstPipePtr; infoPtr != NULL; infoPtr = infoPtr->nextPtr) {
	if (infoPtr->watchMask && !(infoPtr->flags & PIPE_PENDING)) {
	    infoPtr->flags |= PIPE_PENDING;
	    evPtr = (PipeEvent *) ckalloc(sizeof(PipeEvent));
	    evPtr->header.proc = PipeEventProc;
	    evPtr->infoPtr = infoPtr;
	    Tcl_QueueEvent((Tcl_Event *) evPtr, TCL_QUEUE_TAIL);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MakeFile --
 *
 *	This function constructs a new TclFile from a given data and
 *	type value.
 *
 * Results:
 *	Returns a newly allocated WinFile as a TclFile.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TclFile
MakeFile(handle)
    HANDLE handle;		/* Type-specific data. */
{
    WinFile *filePtr;

    filePtr = (WinFile *) ckalloc(sizeof(WinFile));
    filePtr->type = WIN_FILE;
    filePtr->handle = handle;

    return (TclFile)filePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpMakeFile --
 *
 *	Make a TclFile from a channel.
 *
 * Results:
 *	Returns a new TclFile or NULL on failure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TclFile
TclpMakeFile(channel, direction)
    Tcl_Channel channel;	/* Channel to get file from. */
    int direction;		/* Either TCL_READABLE or TCL_WRITABLE. */
{
    HANDLE handle;

    if (Tcl_GetChannelHandle(channel, direction, 
	    (ClientData *) &handle) == TCL_OK) {
	return MakeFile(handle);
    } else {
	return (TclFile) NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TempFileName --
 *
 *	Gets a temporary file name and deals with the fact that the
 *	temporary file path provided by Windows may not actually exist
 *	if the TMP or TEMP environment variables refer to a 
 *	non-existent directory.
 *
 * Results:    
 *	0 if error, non-zero otherwise.  If non-zero is returned, the
 *	name buffer will be filled with a name that can be used to 
 *	construct a temporary file.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TempFileName(name)
    char name[MAX_PATH];	/* Buffer in which name for temporary 
				 * file gets stored. */
{
    if ((GetTempPath(MAX_PATH, name) == 0) ||
	    (GetTempFileName(name, "TCL", 0, name) == 0)) {
	name[0] = '.';
	name[1] = '\0';
	if (GetTempFileName(name, "TCL", 0, name) == 0) {
	    return 0;
	}
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpCreateTempFile --
 *
 *	This function opens a unique file with the property that it
 *	will be deleted when its file handle is closed.  The temporary
 *	file is created in the system temporary directory.
 *
 * Results:
 *	Returns a valid TclFile, or NULL on failure.
 *
 * Side effects:
 *	Creates a new temporary file.
 *
 *----------------------------------------------------------------------
 */

TclFile
TclpCreateTempFile(contents, namePtr)
    char *contents;		/* String to write into temp file, or NULL. */
    Tcl_DString *namePtr;	/* If non-NULL, pointer to initialized 
				 * DString that is filled with the name of 
				 * the temp file that was created. */
{
    char name[MAX_PATH];
    HANDLE handle;

    if (TempFileName(name) == 0) {
	return NULL;
    }

    handle = CreateFile(name, GENERIC_READ | GENERIC_WRITE, 0, NULL,
	    CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,
	    NULL);
    if (handle == INVALID_HANDLE_VALUE) {
	goto error;
    }

    /*
     * Write the file out, doing line translations on the way.
     */

    if (contents != NULL) {
	DWORD result, length;
	char *p;
	
	for (p = contents; *p != '\0'; p++) {
	    if (*p == '\n') {
		length = p - contents;
		if (length > 0) {
		    if (!WriteFile(handle, contents, length, &result, NULL)) {
			goto error;
		    }
		}
		if (!WriteFile(handle, "\r\n", 2, &result, NULL)) {
		    goto error;
		}
		contents = p+1;
	    }
	}
	length = p - contents;
	if (length > 0) {
	    if (!WriteFile(handle, contents, length, &result, NULL)) {
		goto error;
	    }
	}
    }

    if (SetFilePointer(handle, 0, NULL, FILE_BEGIN) == 0xFFFFFFFF) {
	goto error;
    }

    if (namePtr != NULL) {
        Tcl_DStringAppend(namePtr, name, -1);
    }

    /*
     * Under Win32s a file created with FILE_FLAG_DELETE_ON_CLOSE won't
     * actually be deleted when it is closed, so we have to do it ourselves.
     */

    if (TclWinGetPlatformId() == VER_PLATFORM_WIN32s) {
	TmpFile *tmpFilePtr = (TmpFile *) ckalloc(sizeof(TmpFile));
	tmpFilePtr->file.type = WIN32S_TMPFILE;
	tmpFilePtr->file.handle = handle;
	strcpy(tmpFilePtr->name, name);
	return (TclFile)tmpFilePtr;
    } else {
	return MakeFile(handle);
    }

  error:
    TclWinConvertError(GetLastError());
    CloseHandle(handle);
    DeleteFile(name);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpOpenFile --
 *
 *	This function opens files for use in a pipeline.
 *
 * Results:
 *	Returns a newly allocated TclFile structure containing the
 *	file handle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TclFile
TclpOpenFile(path, mode)
    char *path;
    int mode;
{
    HANDLE handle;
    DWORD accessMode, createMode, shareMode, flags;
    SECURITY_ATTRIBUTES sec;

    /*
     * Map the access bits to the NT access mode.
     */

    switch (mode & (O_RDONLY | O_WRONLY | O_RDWR)) {
	case O_RDONLY:
	    accessMode = GENERIC_READ;
	    break;
	case O_WRONLY:
	    accessMode = GENERIC_WRITE;
	    break;
	case O_RDWR:
	    accessMode = (GENERIC_READ | GENERIC_WRITE);
	    break;
	default:
	    TclWinConvertError(ERROR_INVALID_FUNCTION);
	    return NULL;
    }

    /*
     * Map the creation flags to the NT create mode.
     */

    switch (mode & (O_CREAT | O_EXCL | O_TRUNC)) {
	case (O_CREAT | O_EXCL):
	case (O_CREAT | O_EXCL | O_TRUNC):
	    createMode = CREATE_NEW;
	    break;
	case (O_CREAT | O_TRUNC):
	    createMode = CREATE_ALWAYS;
	    break;
	case O_CREAT:
	    createMode = OPEN_ALWAYS;
	    break;
	case O_TRUNC:
	case (O_TRUNC | O_EXCL):
	    createMode = TRUNCATE_EXISTING;
	    break;
	default:
	    createMode = OPEN_EXISTING;
	    break;
    }

    /*
     * If the file is not being created, use the existing file attributes.
     */

    flags = 0;
    if (!(mode & O_CREAT)) {
	flags = GetFileAttributes(path);
	if (flags == 0xFFFFFFFF) {
	    flags = 0;
	}
    }

    /*
     * Set up the security attributes so this file is not inherited by
     * child processes.
     */

    sec.nLength = sizeof(sec);
    sec.lpSecurityDescriptor = NULL;
    sec.bInheritHandle = 0;

    /*
     * Set up the file sharing mode.  We want to allow simultaneous access.
     */

    shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;

    /*
     * Now we get to create the file.
     */

    handle = CreateFile(path, accessMode, shareMode, &sec, createMode, flags,
            (HANDLE) NULL);
    if (handle == INVALID_HANDLE_VALUE) {
	DWORD err = GetLastError();
	if ((err & 0xffffL) == ERROR_OPEN_FAILED) {
	    err = (mode & O_CREAT) ? ERROR_FILE_EXISTS : ERROR_FILE_NOT_FOUND;
	}
        TclWinConvertError(err);
        return NULL;
    }

    /*
     * Seek to the end of file if we are writing.
     */

    if (mode & O_WRONLY) {
	SetFilePointer(handle, 0, NULL, FILE_END);
    }

    return MakeFile(handle);
}

/*
 *----------------------------------------------------------------------
 *
 * TclpCreatePipe --
 *
 *      Creates an anonymous pipe.  Under Win32s, creates a temp file
 *	that is used to simulate a pipe.
 *
 * Results:
 *      Returns 1 on success, 0 on failure. 
 *
 * Side effects:
 *      Creates a pipe.
 *
 *----------------------------------------------------------------------
 */

int
TclpCreatePipe(readPipe, writePipe)
    TclFile *readPipe;	/* Location to store file handle for
				 * read side of pipe. */
    TclFile *writePipe;	/* Location to store file handle for
				 * write side of pipe. */
{
    HANDLE readHandle, writeHandle;

    if (CreatePipe(&readHandle, &writeHandle, NULL, 0) != 0) {
	*readPipe = MakeFile(readHandle);
	*writePipe = MakeFile(writeHandle);
	return 1;
    }

    if (TclWinGetPlatformId() == VER_PLATFORM_WIN32s) {
	WinPipe *readPipePtr, *writePipePtr;
	char buf[MAX_PATH];

	if (TempFileName(buf) != 0) {
	    readPipePtr = (WinPipe *) ckalloc(sizeof(WinPipe));
	    writePipePtr = (WinPipe *) ckalloc(sizeof(WinPipe));

	    readPipePtr->file.type = WIN32S_PIPE;
	    readPipePtr->otherPtr = writePipePtr;
	    readPipePtr->fileName = strcpy(ckalloc(strlen(buf) + 1), buf);
	    readPipePtr->file.handle = INVALID_HANDLE_VALUE;
	    writePipePtr->file.type = WIN32S_PIPE;
	    writePipePtr->otherPtr = readPipePtr;
	    writePipePtr->fileName = readPipePtr->fileName;
	    writePipePtr->file.handle = INVALID_HANDLE_VALUE;

	    *readPipe = (TclFile)readPipePtr;
	    *writePipe = (TclFile)writePipePtr;

	    return 1;
	}
    }

    TclWinConvertError(GetLastError());
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpCloseFile --
 *
 *	Closes a pipeline file handle.  These handles are created by
 *	TclpOpenFile, TclpCreatePipe, or TclpMakeFile.
 *
 * Results:
 *	0 on success, -1 on failure.
 *
 * Side effects:
 *	The file is closed and deallocated.
 *
 *----------------------------------------------------------------------
 */

int
TclpCloseFile(file)
    TclFile file;	/* The file to close. */
{
    WinFile *filePtr = (WinFile *) file;
    WinPipe *pipePtr;

    switch (filePtr->type) {
	case WIN_FILE:
	case WIN32S_TMPFILE:
	    if (CloseHandle(filePtr->handle) == FALSE) {
		TclWinConvertError(GetLastError());
		ckfree((char *) filePtr);
		return -1;
	    }
	    /*
	     * Simulate deleting the file on close for Win32s.
	     */

	    if (filePtr->type == WIN32S_TMPFILE) {
		DeleteFile(((TmpFile*)filePtr)->name);
	    }
	    break;

	case WIN32S_PIPE:
	    pipePtr = (WinPipe *) file;

	    if (pipePtr->otherPtr != NULL) {
		pipePtr->otherPtr->otherPtr = NULL;
	    } else {
		if (pipePtr->file.handle != INVALID_HANDLE_VALUE) {
		    CloseHandle(pipePtr->file.handle);
		}
		DeleteFile(pipePtr->fileName);
		ckfree((char *) pipePtr->fileName);
	    }
	    break;

	default:
	    panic("Tcl_CloseFile: unexpected file type");
    }

    ckfree((char *) filePtr);
    return 0;
}

/*
 *--------------------------------------------------------------------------
 *
 * TclpGetPid --
 *
 *	Given a HANDLE to a child process, return the process id for that
 *	child process.
 *
 * Results:
 *	Returns the process id for the child process.  If the pid was not 
 *	known by Tcl, either because the pid was not created by Tcl or the 
 *	child process has already been reaped, -1 is returned.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------------------
 */

unsigned long
TclpGetPid(pid)
    Tcl_Pid pid;		/* The HANDLE of the child process. */
{
    ProcInfo *infoPtr;
    
    for (infoPtr = procList; infoPtr != NULL; infoPtr = infoPtr->nextPtr) {
	if (infoPtr->hProcess == (HANDLE) pid) {
	    return infoPtr->dwProcessId;
	}
    }
    return (unsigned long) -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpCreateProcess --
 *
 *	Create a child process that has the specified files as its 
 *	standard input, output, and error.  The child process runs
 *	synchronously under Win32s and asynchronously under Windows NT
 *	and Windows 95, and runs with the same environment variables
 *	as the creating process.
 *
 *	The complete Windows search path is searched to find the specified 
 *	executable.  If an executable by the given name is not found, 
 *	automatically tries appending ".com", ".exe", and ".bat" to the 
 *	executable name.
 *
 * Results:
 *	The return value is TCL_ERROR and an error message is left in
 *	interp->result if there was a problem creating the child 
 *	process.  Otherwise, the return value is TCL_OK and *pidPtr is
 *	filled with the process id of the child process.
 * 
 * Side effects:
 *	A process is created.
 *	
 *----------------------------------------------------------------------
 */

int
TclpCreateProcess(interp, argc, argv, inputFile, outputFile, errorFile, 
	pidPtr)
    Tcl_Interp *interp;		/* Interpreter in which to leave errors that
				 * occurred when creating the child process.
				 * Error messages from the child process
				 * itself are sent to errorFile. */
    int argc;			/* Number of arguments in following array. */
    char **argv;		/* Array of argument strings.  argv[0]
				 * contains the name of the executable
				 * converted to native format (using the
				 * Tcl_TranslateFileName call).  Additional
				 * arguments have not been converted. */
    TclFile inputFile;		/* If non-NULL, gives the file to use as
				 * input for the child process.  If inputFile
				 * file is not readable or is NULL, the child
				 * will receive no standard input. */
    TclFile outputFile;		/* If non-NULL, gives the file that
				 * receives output from the child process.  If
				 * outputFile file is not writeable or is
				 * NULL, output from the child will be
				 * discarded. */
    TclFile errorFile;		/* If non-NULL, gives the file that
				 * receives errors from the child process.  If
				 * errorFile file is not writeable or is NULL,
				 * errors from the child will be discarded.
				 * errorFile may be the same as outputFile. */
    Tcl_Pid *pidPtr;		/* If this procedure is successful, pidPtr
				 * is filled with the process id of the child
				 * process. */
{
    int result, applType, createFlags;
    Tcl_DString cmdLine;
    STARTUPINFO startInfo;
    PROCESS_INFORMATION procInfo;
    SECURITY_ATTRIBUTES secAtts;
    HANDLE hProcess, h, inputHandle, outputHandle, errorHandle;
    char execPath[MAX_PATH];
    char *originalName;
    WinFile *filePtr;

    if (!initialized) {
	PipeInit();
    }

    applType = ApplicationType(interp, argv[0], execPath);
    if (applType == APPL_NONE) {
	return TCL_ERROR;
    }
    originalName = argv[0];
    argv[0] = execPath;

    result = TCL_ERROR;
    Tcl_DStringInit(&cmdLine);

    if (TclWinGetPlatformId() == VER_PLATFORM_WIN32s) {
	/*
	 * Under Win32s, there are no pipes.  In order to simulate pipe
	 * behavior, the child processes are run synchronously and their
	 * I/O is redirected from/to temporary files before the next 
	 * stage of the pipeline is started.
	 */

	MSG msg;
	DWORD status;
	DWORD args[4];
	void *trans[5];
	char *inputFileName, *outputFileName;
	Tcl_DString inputTempFile, outputTempFile;

	BuildCommandLine(argc, argv, &cmdLine);

	ZeroMemory(&startInfo, sizeof(startInfo));
	startInfo.cb = sizeof(startInfo);

	Tcl_DStringInit(&inputTempFile);
	Tcl_DStringInit(&outputTempFile);
	outputHandle = INVALID_HANDLE_VALUE;

	inputFileName = NULL;
	outputFileName = NULL;
	if (inputFile != NULL) {
	    filePtr = (WinFile *) inputFile;
	    switch (filePtr->type) {
		case WIN_FILE:
		case WIN32S_TMPFILE: {
		    h = INVALID_HANDLE_VALUE;
		    inputFileName = MakeTempFile(&inputTempFile);
		    if (inputFileName != NULL) {
			h = CreateFile(inputFileName, GENERIC_WRITE, 0, 
				NULL, CREATE_ALWAYS, 0, NULL);
		    }
		    if (h == INVALID_HANDLE_VALUE) {
			Tcl_AppendResult(interp, "couldn't duplicate input handle: ", 
				Tcl_PosixError(interp), (char *) NULL);
			goto end32s;
		    }
		    CopyChannel(h, filePtr->handle);
		    CloseHandle(h);
		    break;
		}
		case WIN32S_PIPE: {
		    inputFileName = ((WinPipe*)inputFile)->fileName;
		    break;
		}
	    }
	}
	if (inputFileName == NULL) {
	    inputFileName = "nul";
	}
	if (outputFile != NULL) {
	    filePtr = (WinFile *)outputFile;
	    if (filePtr->type == WIN_FILE) {
		outputFileName = MakeTempFile(&outputTempFile);
		if (outputFileName == NULL) {
		    Tcl_AppendResult(interp, "couldn't duplicate output handle: ",
			    Tcl_PosixError(interp), (char *) NULL);
		    goto end32s;
		}
		outputHandle = filePtr->handle;
	    } else if (filePtr->type == WIN32S_PIPE) {
		outputFileName = ((WinPipe*)outputFile)->fileName;
	    }
	}
	if (outputFileName == NULL) {
	    outputFileName = "nul";
	}

	if (applType == APPL_DOS) {
	    args[0] = (DWORD) Tcl_DStringValue(&cmdLine);
	    args[1] = (DWORD) inputFileName;
	    args[2] = (DWORD) outputFileName;
	    trans[0] = &args[0];
	    trans[1] = &args[1];
	    trans[2] = &args[2];
	    trans[3] = NULL;
	    if (TclWinSynchSpawn(args, 0, trans, pidPtr) != 0) {
		result = TCL_OK;
	    }
	} else if (applType == APPL_WIN3X) {
	    args[0] = (DWORD) Tcl_DStringValue(&cmdLine);
	    trans[0] = &args[0];
	    trans[1] = NULL;
	    if (TclWinSynchSpawn(args, 1, trans, pidPtr) != 0) {
		result = TCL_OK;
	    }
	} else {
	    if (CreateProcess(NULL, Tcl_DStringValue(&cmdLine), NULL, NULL, 
		    FALSE, DETACHED_PROCESS, NULL, NULL, &startInfo, 
		    &procInfo) != 0) {
		CloseHandle(procInfo.hThread);
		while (1) {
		    if (GetExitCodeProcess(procInfo.hProcess, &status) == FALSE) {
			break;
		    }
		    if (status != STILL_ACTIVE) {
			break;
		    }
		    if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) == TRUE) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		    }
		}
		*pidPtr = (Tcl_Pid) procInfo.hProcess;
		if (*pidPtr != 0) {
		    ProcInfo *procPtr = (ProcInfo *) ckalloc(sizeof(ProcInfo));
		    procPtr->hProcess = procInfo.hProcess;
		    procPtr->dwProcessId = procInfo.dwProcessId;
		    procPtr->nextPtr = procList;
		    procList = procPtr;
		}
		result = TCL_OK;
	    }
	}
	if (result != TCL_OK) {
	    TclWinConvertError(GetLastError());
	    Tcl_AppendResult(interp, "couldn't execute \"", originalName,
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	}

	end32s:
	if (outputHandle != INVALID_HANDLE_VALUE) {
	    /*
	     * Now copy stuff from temp file to actual output handle. Don't
	     * close outputHandle because it is associated with the output
	     * file owned by the caller.
	     */

	    h = CreateFile(outputFileName, GENERIC_READ, 0, NULL, OPEN_ALWAYS,
		    0, NULL);
	    if (h != INVALID_HANDLE_VALUE) {
		CopyChannel(outputHandle, h);
	    }
	    CloseHandle(h);
	}

	if (inputFileName == Tcl_DStringValue(&inputTempFile)) {
	    DeleteFile(inputFileName);
	}
	
	if (outputFileName == Tcl_DStringValue(&outputTempFile)) {
	    DeleteFile(outputFileName);
	}

	Tcl_DStringFree(&inputTempFile);
	Tcl_DStringFree(&outputTempFile);
        Tcl_DStringFree(&cmdLine);
	return result;
    }
    hProcess = GetCurrentProcess();

    /*
     * STARTF_USESTDHANDLES must be used to pass handles to child process.
     * Using SetStdHandle() and/or dup2() only works when a console mode 
     * parent process is spawning an attached console mode child process.
     */

    ZeroMemory(&startInfo, sizeof(startInfo));
    startInfo.cb = sizeof(startInfo);
    startInfo.dwFlags   = STARTF_USESTDHANDLES;
    startInfo.hStdInput	= INVALID_HANDLE_VALUE;
    startInfo.hStdOutput= INVALID_HANDLE_VALUE;
    startInfo.hStdError = INVALID_HANDLE_VALUE;

    secAtts.nLength = sizeof(SECURITY_ATTRIBUTES);
    secAtts.lpSecurityDescriptor = NULL;
    secAtts.bInheritHandle = TRUE;

    /*
     * We have to check the type of each file, since we cannot duplicate 
     * some file types.  
     */

    inputHandle = INVALID_HANDLE_VALUE;
    if (inputFile != NULL) {
	filePtr = (WinFile *)inputFile;
	if (filePtr->type == WIN_FILE) {
	    inputHandle = filePtr->handle;
	}
    }
    outputHandle = INVALID_HANDLE_VALUE;
    if (outputFile != NULL) {
	filePtr = (WinFile *)outputFile;
	if (filePtr->type == WIN_FILE) {
	    outputHandle = filePtr->handle;
	}
    }
    errorHandle = INVALID_HANDLE_VALUE;
    if (errorFile != NULL) {
	filePtr = (WinFile *)errorFile;
	if (filePtr->type == WIN_FILE) {
	    errorHandle = filePtr->handle;
	}
    }

    /*
     * Duplicate all the handles which will be passed off as stdin, stdout
     * and stderr of the child process. The duplicate handles are set to
     * be inheritable, so the child process can use them.
     */

    if (inputHandle == INVALID_HANDLE_VALUE) {
	/* 
	 * If handle was not set, stdin should return immediate EOF.
	 * Under Windows95, some applications (both 16 and 32 bit!) 
	 * cannot read from the NUL device; they read from console
	 * instead.  When running tk, this is fatal because the child 
	 * process would hang forever waiting for EOF from the unmapped 
	 * console window used by the helper application.
	 *
	 * Fortunately, the helper application detects a closed pipe 
	 * as an immediate EOF and can pass that information to the 
	 * child process.
	 */

	if (CreatePipe(&startInfo.hStdInput, &h, &secAtts, 0) != FALSE) {
	    CloseHandle(h);
	}
    } else {
	DuplicateHandle(hProcess, inputHandle, hProcess, &startInfo.hStdInput,
		0, TRUE, DUPLICATE_SAME_ACCESS);
    }
    if (startInfo.hStdInput == INVALID_HANDLE_VALUE) {
	TclWinConvertError(GetLastError());
	Tcl_AppendResult(interp, "couldn't duplicate input handle: ",
		Tcl_PosixError(interp), (char *) NULL);
	goto end;
    }

    if (outputHandle == INVALID_HANDLE_VALUE) {
	/*
	 * If handle was not set, output should be sent to an infinitely 
	 * deep sink.  Under Windows 95, some 16 bit applications cannot
	 * have stdout redirected to NUL; they send their output to
	 * the console instead.  Some applications, like "more" or "dir /p", 
	 * when outputting multiple pages to the console, also then try and
	 * read from the console to go the next page.  When running tk, this
	 * is fatal because the child process would hang forever waiting
	 * for input from the unmapped console window used by the helper
	 * application.
	 *
	 * Fortunately, the helper application will detect a closed pipe
	 * as a sink.
	 */

	if ((TclWinGetPlatformId() == VER_PLATFORM_WIN32_WINDOWS) 
		&& (applType == APPL_DOS)) {
	    if (CreatePipe(&h, &startInfo.hStdOutput, &secAtts, 0) != FALSE) {
		CloseHandle(h);
	    }
	} else {
	    startInfo.hStdOutput = CreateFile("NUL:", GENERIC_WRITE, 0,
		    &secAtts, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	}
    } else {
	DuplicateHandle(hProcess, outputHandle, hProcess, &startInfo.hStdOutput, 
		0, TRUE, DUPLICATE_SAME_ACCESS);
    }
    if (startInfo.hStdOutput == INVALID_HANDLE_VALUE) {
	TclWinConvertError(GetLastError());
	Tcl_AppendResult(interp, "couldn't duplicate output handle: ",
		Tcl_PosixError(interp), (char *) NULL);
	goto end;
    }

    if (errorHandle == INVALID_HANDLE_VALUE) {
	/*
	 * If handle was not set, errors should be sent to an infinitely
	 * deep sink.
	 */

	startInfo.hStdError = CreateFile("NUL:", GENERIC_WRITE, 0,
		&secAtts, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    } else {
	DuplicateHandle(hProcess, errorHandle, hProcess, &startInfo.hStdError, 
		0, TRUE, DUPLICATE_SAME_ACCESS);
    } 
    if (startInfo.hStdError == INVALID_HANDLE_VALUE) {
	TclWinConvertError(GetLastError());
	Tcl_AppendResult(interp, "couldn't duplicate error handle: ",
		Tcl_PosixError(interp), (char *) NULL);
	goto end;
    }
    /* 
     * If we do not have a console window, then we must run DOS and
     * WIN32 console mode applications as detached processes. This tells
     * the loader that the child application should not inherit the
     * console, and that it should not create a new console window for
     * the child application.  The child application should get its stdio 
     * from the redirection handles provided by this application, and run
     * in the background.
     *
     * If we are starting a GUI process, they don't automatically get a 
     * console, so it doesn't matter if they are started as foreground or
     * detached processes.  The GUI window will still pop up to the
     * foreground.
     */

    if (TclWinGetPlatformId() == VER_PLATFORM_WIN32_NT) {
	if (HasConsole()) {
	    createFlags = 0;
	} else if (applType == APPL_DOS) {
	    /*
	     * Under NT, 16-bit DOS applications will not run unless they
	     * can be attached to a console.  If we are running without a
	     * console, run the 16-bit program as an normal process inside
	     * of a hidden console application, and then run that hidden
	     * console as a detached process.
	     */

	    startInfo.wShowWindow = SW_HIDE;
	    startInfo.dwFlags |= STARTF_USESHOWWINDOW;
	    createFlags = CREATE_NEW_CONSOLE;
	    Tcl_DStringAppend(&cmdLine, "cmd.exe /c ", -1);
	} else {
	    createFlags = DETACHED_PROCESS;
	} 
    } else {
	if (HasConsole()) {
	    createFlags = 0;
	} else {
	    createFlags = DETACHED_PROCESS;
	}
	
	if (applType == APPL_DOS) {
	    /*
	     * Under Windows 95, 16-bit DOS applications do not work well 
	     * with pipes:
	     *
	     * 1. EOF on a pipe between a detached 16-bit DOS application 
	     * and another application is not seen at the other
	     * end of the pipe, so the listening process blocks forever on 
	     * reads.  This inablity to detect EOF happens when either a 
	     * 16-bit app or the 32-bit app is the listener.  
	     *
	     * 2. If a 16-bit DOS application (detached or not) blocks when 
	     * writing to a pipe, it will never wake up again, and it
	     * eventually brings the whole system down around it.
	     *
	     * The 16-bit application is run as a normal process inside
	     * of a hidden helper console app, and this helper may be run
	     * as a detached process.  If any of the stdio handles is
	     * a pipe, the helper application accumulates information 
	     * into temp files and forwards it to or from the DOS 
	     * application as appropriate.  This means that DOS apps 
	     * must receive EOF from a stdin pipe before they will actually
	     * begin, and must finish generating stdout or stderr before 
	     * the data will be sent to the next stage of the pipe.
	     *
	     * The helper app should be located in the same directory as
	     * the tcl dll.
	     */

	    if (createFlags != 0) {
		startInfo.wShowWindow = SW_HIDE;
		startInfo.dwFlags |= STARTF_USESHOWWINDOW;
		createFlags = CREATE_NEW_CONSOLE;
	    }
	    Tcl_DStringAppend(&cmdLine, "tclpip" STRINGIFY(TCL_MAJOR_VERSION) 
		    STRINGIFY(TCL_MINOR_VERSION) ".dll ", -1);
	}
    }
    
    /*
     * cmdLine gets the full command line used to invoke the executable,
     * including the name of the executable itself.  The command line
     * arguments in argv[] are stored in cmdLine separated by spaces. 
     * Special characters in individual arguments from argv[] must be 
     * quoted when being stored in cmdLine.
     *
     * When calling any application, bear in mind that arguments that 
     * specify a path name are not converted.  If an argument contains 
     * forward slashes as path separators, it may or may not be 
     * recognized as a path name, depending on the program.  In general,
     * most applications accept forward slashes only as option 
     * delimiters and backslashes only as paths.
     *
     * Additionally, when calling a 16-bit dos or windows application, 
     * all path names must use the short, cryptic, path format (e.g., 
     * using ab~1.def instead of "a b.default").  
     */

    BuildCommandLine(argc, argv, &cmdLine);

    if (!CreateProcess(NULL, Tcl_DStringValue(&cmdLine), NULL, NULL, TRUE, 
	    createFlags, NULL, NULL, &startInfo, &procInfo)) {
	TclWinConvertError(GetLastError());
	Tcl_AppendResult(interp, "couldn't execute \"", originalName,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	goto end;
    }

    if (applType == APPL_DOS) {
	WaitForSingleObject(hProcess, 50);
    }

    /* 
     * "When an application spawns a process repeatedly, a new thread 
     * instance will be created for each process but the previous 
     * instances may not be cleaned up.  This results in a significant 
     * virtual memory loss each time the process is spawned.  If there 
     * is a WaitForInputIdle() call between CreateProcess() and
     * CloseHandle(), the problem does not occur." PSS ID Number: Q124121
     */

    WaitForInputIdle(procInfo.hProcess, 5000);
    CloseHandle(procInfo.hThread);

    *pidPtr = (Tcl_Pid) procInfo.hProcess;
    if (*pidPtr != 0) {
	ProcInfo *procPtr = (ProcInfo *) ckalloc(sizeof(ProcInfo));
	procPtr->hProcess = procInfo.hProcess;
	procPtr->dwProcessId = procInfo.dwProcessId;
	procPtr->nextPtr = procList;
	procList = procPtr;
    }
    result = TCL_OK;

    end:
    Tcl_DStringFree(&cmdLine);
    if (startInfo.hStdInput != INVALID_HANDLE_VALUE) {
        CloseHandle(startInfo.hStdInput);
    }
    if (startInfo.hStdOutput != INVALID_HANDLE_VALUE) {
        CloseHandle(startInfo.hStdOutput);
    }
    if (startInfo.hStdError != INVALID_HANDLE_VALUE) {
	CloseHandle(startInfo.hStdError);
    }
    return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HasConsole --
 *
 *	Determines whether the current application is attached to a
 *	console.
 *
 * Results:
 *	Returns TRUE if this application has a console, else FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static BOOL
HasConsole()
{
    HANDLE handle = CreateFile("CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE,
	    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
	return TRUE;
    } else {
        return FALSE;
    }
}

/*
 *--------------------------------------------------------------------
 *
 * ApplicationType --
 *
 *	Search for the specified program and identify if it refers to a DOS,
 *	Windows 3.X, or Win32 program.  Used to determine how to invoke 
 *	a program, or if it can even be invoked.
 *
 *	It is possible to almost positively identify DOS and Windows 
 *	applications that contain the appropriate magic numbers.  However, 
 *	DOS .com files do not seem to contain a magic number; if the program 
 *	name ends with .com and could not be identified as a Windows .com
 *	file, it will be assumed to be a DOS application, even if it was
 *	just random data.  If the program name does not end with .com, no 
 *	such assumption is made.
 *
 *	The Win32 procedure GetBinaryType incorrectly identifies any 
 *	junk file that ends with .exe as a dos executable and some 
 *	executables that don't end with .exe as not executable.  Plus it 
 *	doesn't exist under win95, so I won't feel bad about reimplementing
 *	functionality.
 *
 * Results:
 *	The return value is one of APPL_DOS, APPL_WIN3X, or APPL_WIN32
 *	if the filename referred to the corresponding application type.
 *	If the file name could not be found or did not refer to any known 
 *	application type, APPL_NONE is returned and an error message is 
 *	left in interp.  .bat files are identified as APPL_DOS.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ApplicationType(interp, originalName, fullPath)
    Tcl_Interp *interp;		/* Interp, for error message. */
    const char *originalName;	/* Name of the application to find. */
    char fullPath[MAX_PATH];	/* Filled with complete path to 
				 * application. */
{
    int applType, i;
    HANDLE hFile;
    char *ext, *rest;
    char buf[2];
    DWORD read;
    IMAGE_DOS_HEADER header;
    static char extensions[][5] = {"", ".com", ".exe", ".bat"};

    /* Look for the program as an external program.  First try the name
     * as it is, then try adding .com, .exe, and .bat, in that order, to
     * the name, looking for an executable.
     *
     * Using the raw SearchPath() procedure doesn't do quite what is 
     * necessary.  If the name of the executable already contains a '.' 
     * character, it will not try appending the specified extension when
     * searching (in other words, SearchPath will not find the program 
     * "a.b.exe" if the arguments specified "a.b" and ".exe").   
     * So, first look for the file as it is named.  Then manually append 
     * the extensions, looking for a match.  
     */

    applType = APPL_NONE;
    for (i = 0; i < (int) (sizeof(extensions) / sizeof(extensions[0])); i++) {
	lstrcpyn(fullPath, originalName, MAX_PATH - 5);
        lstrcat(fullPath, extensions[i]);
	
	SearchPath(NULL, fullPath, NULL, MAX_PATH, fullPath, &rest);

	/*
	 * Ignore matches on directories or data files, return if identified
	 * a known type.
	 */

	if (GetFileAttributes(fullPath) & FILE_ATTRIBUTE_DIRECTORY) {
	    continue;
	}

	ext = strrchr(fullPath, '.');
	if ((ext != NULL) && (strcmpi(ext, ".bat") == 0)) {
	    applType = APPL_DOS;
	    break;
	}

	hFile = CreateFile(fullPath, GENERIC_READ, FILE_SHARE_READ, NULL, 
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
	    continue;
	}

	header.e_magic = 0;
	ReadFile(hFile, (void *) &header, sizeof(header), &read, NULL);
	if (header.e_magic != IMAGE_DOS_SIGNATURE) {
	    /* 
	     * Doesn't have the magic number for relocatable executables.  If 
	     * filename ends with .com, assume it's a DOS application anyhow.
	     * Note that we didn't make this assumption at first, because some
	     * supposed .com files are really 32-bit executables with all the
	     * magic numbers and everything.  
	     */

	    CloseHandle(hFile);
	    if ((ext != NULL) && (strcmpi(ext, ".com") == 0)) {
		applType = APPL_DOS;
		break;
	    }
	    continue;
	}
	if (header.e_lfarlc != sizeof(header)) {
	    /* 
	     * All Windows 3.X and Win32 and some DOS programs have this value
	     * set here.  If it doesn't, assume that since it already had the 
	     * other magic number it was a DOS application.
	     */

	    CloseHandle(hFile);
	    applType = APPL_DOS;
	    break;
	}

	/* 
	 * The DWORD at header.e_lfanew points to yet another magic number.
	 */

	buf[0] = '\0';
	SetFilePointer(hFile, header.e_lfanew, NULL, FILE_BEGIN);
	ReadFile(hFile, (void *) buf, 2, &read, NULL);
	CloseHandle(hFile);

	if ((buf[0] == 'N') && (buf[1] == 'E')) {
	    applType = APPL_WIN3X;
	} else if ((buf[0] == 'P') && (buf[1] == 'E')) {
	    applType = APPL_WIN32;
	} else {
	    /*
	     * Strictly speaking, there should be a test that there
	     * is an 'L' and 'E' at buf[0..1], to identify the type as 
	     * DOS, but of course we ran into a DOS executable that 
	     * _doesn't_ have the magic number -- specifically, one
	     * compiled using the Lahey Fortran90 compiler.
	     */

	    applType = APPL_DOS;
	}
	break;
    }

    if (applType == APPL_NONE) {
	TclWinConvertError(GetLastError());
	Tcl_AppendResult(interp, "couldn't execute \"", originalName,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	return APPL_NONE;
    }

    if ((applType == APPL_DOS) || (applType == APPL_WIN3X)) {
	/* 
	 * Replace long path name of executable with short path name for 
	 * 16-bit applications.  Otherwise the application may not be able
	 * to correctly parse its own command line to separate off the 
	 * application name from the arguments.
	 */

	GetShortPathName(fullPath, fullPath, MAX_PATH);
    }
    return applType;
}

/*    
 *----------------------------------------------------------------------
 *
 * BuildCommandLine --
 *
 *	The command line arguments are stored in linePtr separated
 *	by spaces, in a form that CreateProcess() understands.  Special 
 *	characters in individual arguments from argv[] must be quoted 
 *	when being stored in cmdLine.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
BuildCommandLine(argc, argv, linePtr)
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
    Tcl_DString *linePtr;	/* Initialized Tcl_DString that receives the
				 * command line. */
{
    char *start, *special;
    int quote, i;

    for (i = 0; i < argc; i++) {
	if (i > 0) {
	    Tcl_DStringAppend(linePtr, " ", 1);	
	}

	quote = 0;
	for (start = argv[i]; *start != '\0'; start++) {
	    if (isspace(*start)) {
		quote = 1;
		Tcl_DStringAppend(linePtr, "\"", 1);
    		break;
	    }
	}

	start = argv[i];	    
	for (special = argv[i]; ; ) {
	    if ((*special == '\\') && 
		    (special[1] == '\\' || special[1] == '"')) {
		Tcl_DStringAppend(linePtr, start, special - start);
		start = special;
		while (1) {
		    special++;
		    if (*special == '"') {
			/* 
			 * N backslashes followed a quote -> insert 
			 * N * 2 + 1 backslashes then a quote.
			 */

			Tcl_DStringAppend(linePtr, start, special - start);
			break;
		    }
		    if (*special != '\\') {
			break;
		    }
		}
		Tcl_DStringAppend(linePtr, start, special - start);
		start = special;
	    }
	    if (*special == '"') {
		Tcl_DStringAppend(linePtr, start, special - start);
		Tcl_DStringAppend(linePtr, "\\\"", 2);
		start = special + 1;
	    }
	    if (*special == '\0') {
		break;
	    }
	    special++;
	}
	Tcl_DStringAppend(linePtr, start, special - start);
	if (quote) {
	    Tcl_DStringAppend(linePtr, "\"", 1);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * MakeTempFile --
 *
 *	Helper function for TclpCreateProcess under Win32s.  Makes a 
 *	temporary file that _won't_ go away automatically when it's file
 *	handle is closed.  Used for simulated pipes, which are written
 *	in one pass and reopened and read in the next pass.
 *
 * Results:
 *	namePtr is filled with the name of the temporary file.
 *
 * Side effects:
 *	A temporary file with the name specified by namePtr is created.  
 *	The caller is responsible for deleting this temporary file.
 *
 *----------------------------------------------------------------------
 */

static char *
MakeTempFile(namePtr)
    Tcl_DString *namePtr;	/* Initialized Tcl_DString that is filled 
				 * with the name of the temporary file that 
				 * was created. */
{
    char name[MAX_PATH];

    if (TempFileName(name) == 0) {
	return NULL;
    }

    Tcl_DStringAppend(namePtr, name, -1);
    return Tcl_DStringValue(namePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * CopyChannel --
 *
 *	Helper function used by TclpCreateProcess under Win32s.  Copies
 *	what remains of source file to destination file; source file 
 *	pointer need not be positioned at the beginning of the file if
 *	all of source file is not desired, but data is copied up to end 
 *	of source file.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
CopyChannel(dst, src)
    HANDLE dst;			/* Destination file. */
    HANDLE src;			/* Source file. */
{
    char buf[8192];
    DWORD dwRead, dwWrite;

    while (ReadFile(src, buf, sizeof(buf), &dwRead, NULL) != FALSE) {
	if (dwRead == 0) {
	    break;
	}
	if (WriteFile(dst, buf, dwRead, &dwWrite, NULL) == FALSE) {
	    break;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclpCreateCommandChannel --
 *
 *	This function is called by Tcl_OpenCommandChannel to perform
 *	the platform specific channel initialization for a command
 *	channel.
 *
 * Results:
 *	Returns a new channel or NULL on failure.
 *
 * Side effects:
 *	Allocates a new channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
TclpCreateCommandChannel(readFile, writeFile, errorFile, numPids, pidPtr)
    TclFile readFile;		/* If non-null, gives the file for reading. */
    TclFile writeFile;		/* If non-null, gives the file for writing. */
    TclFile errorFile;		/* If non-null, gives the file where errors
				 * can be read. */
    int numPids;		/* The number of pids in the pid array. */
    Tcl_Pid *pidPtr;		/* An array of process identifiers. */
{
    char channelName[20];
    int channelId;
    PipeInfo *infoPtr = (PipeInfo *) ckalloc((unsigned) sizeof(PipeInfo));

    if (!initialized) {
	PipeInit();
    }

    infoPtr->watchMask = 0;
    infoPtr->flags = 0;
    infoPtr->readFile = readFile;
    infoPtr->writeFile = writeFile;
    infoPtr->errorFile = errorFile;
    infoPtr->numPids = numPids;
    infoPtr->pidPtr = pidPtr;

    /*
     * Use one of the fds associated with the channel as the
     * channel id.
     */

    if (readFile) {
	WinPipe *pipePtr = (WinPipe *) readFile;
	if (pipePtr->file.type == WIN32S_PIPE
		&& pipePtr->file.handle == INVALID_HANDLE_VALUE) {
	    pipePtr->file.handle = CreateFile(pipePtr->fileName, GENERIC_READ,
		    0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	channelId = (int) pipePtr->file.handle;
    } else if (writeFile) {
	channelId = (int) ((WinFile*)writeFile)->handle;
    } else if (errorFile) {
	channelId = (int) ((WinFile*)errorFile)->handle;
    } else {
	channelId = 0;
    }

    infoPtr->validMask = 0;
    if (readFile != NULL) {
        infoPtr->validMask |= TCL_READABLE;
    }
    if (writeFile != NULL) {
        infoPtr->validMask |= TCL_WRITABLE;
    }

    /*
     * For backward compatibility with previous versions of Tcl, we
     * use "file%d" as the base name for pipes even though it would
     * be more natural to use "pipe%d".
     */

    sprintf(channelName, "file%d", channelId);
    infoPtr->channel = Tcl_CreateChannel(&pipeChannelType, channelName,
            (ClientData) infoPtr, infoPtr->validMask);

    /*
     * Pipes have AUTO translation mode on Windows and ^Z eof char, which
     * means that a ^Z will be appended to them at close. This is needed
     * for Windows programs that expect a ^Z at EOF.
     */

    Tcl_SetChannelOption((Tcl_Interp *) NULL, infoPtr->channel,
	    "-translation", "auto");
    Tcl_SetChannelOption((Tcl_Interp *) NULL, infoPtr->channel,
	    "-eofchar", "\032 {}");
    return infoPtr->channel;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetAndDetachPids --
 *
 *	Stores a list of the command PIDs for a command channel in
 *	interp->result.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies interp->result.
 *
 *----------------------------------------------------------------------
 */

void
TclGetAndDetachPids(interp, chan)
    Tcl_Interp *interp;
    Tcl_Channel chan;
{
    PipeInfo *pipePtr;
    Tcl_ChannelType *chanTypePtr;
    int i;
    char buf[20];

    /*
     * Punt if the channel is not a command channel.
     */

    chanTypePtr = Tcl_GetChannelType(chan);
    if (chanTypePtr != &pipeChannelType) {
        return;
    }

    pipePtr = (PipeInfo *) Tcl_GetChannelInstanceData(chan);
    for (i = 0; i < pipePtr->numPids; i++) {
        sprintf(buf, "%lu", TclpGetPid(pipePtr->pidPtr[i]));
        Tcl_AppendElement(interp, buf);
        Tcl_DetachPids(1, &(pipePtr->pidPtr[i]));
    }
    if (pipePtr->numPids > 0) {
        ckfree((char *) pipePtr->pidPtr);
        pipePtr->numPids = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PipeBlockModeProc --
 *
 *	Set blocking or non-blocking mode on channel.
 *
 * Results:
 *	0 if successful, errno when failed.
 *
 * Side effects:
 *	Sets the device into blocking or non-blocking mode.
 *
 *----------------------------------------------------------------------
 */

static int
PipeBlockModeProc(instanceData, mode)
    ClientData instanceData;	/* Instance data for channel. */
    int mode;			/* TCL_MODE_BLOCKING or
                                 * TCL_MODE_NONBLOCKING. */
{
    PipeInfo *infoPtr = (PipeInfo *) instanceData;
    
    /*
     * Pipes on Windows can not be switched between blocking and nonblocking,
     * hence we have to emulate the behavior. This is done in the input
     * function by checking against a bit in the state. We set or unset the
     * bit here to cause the input function to emulate the correct behavior.
     */

    if (mode == TCL_MODE_NONBLOCKING) {
	infoPtr->flags |= PIPE_ASYNC;
    } else {
	infoPtr->flags &= ~(PIPE_ASYNC);
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * PipeCloseProc --
 *
 *	Closes a pipe based IO channel.
 *
 * Results:
 *	0 on success, errno otherwise.
 *
 * Side effects:
 *	Closes the physical channel.
 *
 *----------------------------------------------------------------------
 */

static int
PipeCloseProc(instanceData, interp)
    ClientData instanceData;	/* Pointer to PipeInfo structure. */
    Tcl_Interp *interp;		/* For error reporting. */
{
    PipeInfo *pipePtr = (PipeInfo *) instanceData;
    Tcl_Channel errChan;
    int errorCode, result;
    PipeInfo *infoPtr, **nextPtrPtr;

    /*
     * Remove the file from the list of watched files.
     */

    for (nextPtrPtr = &firstPipePtr, infoPtr = *nextPtrPtr; infoPtr != NULL;
	    nextPtrPtr = &infoPtr->nextPtr, infoPtr = *nextPtrPtr) {
	if (infoPtr == (PipeInfo *)pipePtr) {
	    *nextPtrPtr = infoPtr->nextPtr;
	    break;
	}
    }

    errorCode = 0;
    if (pipePtr->readFile != NULL) {
	if (TclpCloseFile(pipePtr->readFile) != 0) {
	    errorCode = errno;
	}
    }
    if (pipePtr->writeFile != NULL) {
	if (TclpCloseFile(pipePtr->writeFile) != 0) {
	    if (errorCode == 0) {
		errorCode = errno;
	    }
	}
    }
    
    /*
     * Wrap the error file into a channel and give it to the cleanup
     * routine.  If we are running in Win32s, just delete the error file
     * immediately, because it was never used.
     */

    if (pipePtr->errorFile) {
	WinFile *filePtr;
	OSVERSIONINFO os;

	os.dwOSVersionInfoSize = sizeof(os);
	GetVersionEx(&os);
	if (os.dwPlatformId == VER_PLATFORM_WIN32s) {
	    TclpCloseFile(pipePtr->errorFile);
	    errChan = NULL;
	} else {
	    filePtr = (WinFile*)pipePtr->errorFile;
	    errChan = Tcl_MakeFileChannel((ClientData) filePtr->handle,
		    TCL_READABLE);
	}
    } else {
        errChan = NULL;
    }
    result = TclCleanupChildren(interp, pipePtr->numPids, pipePtr->pidPtr,
            errChan);
    if (pipePtr->numPids > 0) {
        ckfree((char *) pipePtr->pidPtr);
    }
    ckfree((char*) pipePtr);

    if (errorCode == 0) {
        return result;
    }
    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * PipeInputProc --
 *
 *	Reads input from the IO channel into the buffer given. Returns
 *	count of how many bytes were actually read, and an error indication.
 *
 * Results:
 *	A count of how many bytes were read is returned and an error
 *	indication is returned in an output argument.
 *
 * Side effects:
 *	Reads input from the actual channel.
 *
 *----------------------------------------------------------------------
 */

static int
PipeInputProc(instanceData, buf, bufSize, errorCode)
    ClientData instanceData;		/* Pipe state. */
    char *buf;				/* Where to store data read. */
    int bufSize;			/* How much space is available
                                         * in the buffer? */
    int *errorCode;			/* Where to store error code. */
{
    PipeInfo *infoPtr = (PipeInfo *) instanceData;
    WinFile *filePtr = (WinFile*) infoPtr->readFile;
    DWORD count;
    DWORD bytesRead;

    *errorCode = 0;
    if (filePtr->type == WIN32S_PIPE) {
	if (((WinPipe *)filePtr)->otherPtr != NULL) {
	    panic("PipeInputProc: child process isn't finished writing");
	}
	if (filePtr->handle == INVALID_HANDLE_VALUE) {
	    filePtr->handle = CreateFile(((WinPipe *)filePtr)->fileName,
		    GENERIC_READ, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL,
		    NULL);
	}
	if (filePtr->handle == INVALID_HANDLE_VALUE) {
	    goto error;
	}
    } else {
	/*
	 * Pipes will block until the requested number of bytes has been
	 * read.  To avoid blocking unnecessarily, we look ahead and only
	 * read as much as is available.
	 */

	if (PeekNamedPipe(filePtr->handle, (LPVOID) NULL, (DWORD) 0,
		(LPDWORD) NULL, &count, (LPDWORD) NULL) == TRUE) {
	    if ((count != 0) && ((DWORD) bufSize > count)) {
		bufSize = (int) count;

		/*
		 * This code is commented out because on Win95 we don't get
		 * notifier of eof on a pipe unless we try to read it.
		 * The correct solution is to move to threads.
		 */

/* 	    } else if ((count == 0) && (infoPtr->flags & PIPE_ASYNC)) { */
/* 		errno = *errorCode = EAGAIN; */
/* 		return -1; */
	    } else if ((count == 0) && !(infoPtr->flags & PIPE_ASYNC)) {
		bufSize = 1;
	    }
	} else {
	    goto error;
	}
    }

    /*
     * Note that we will block on reads from a console buffer until a
     * full line has been entered.  The only way I know of to get
     * around this is to write a console driver.  We should probably
     * do this at some point, but for now, we just block.
     */

    if (ReadFile(filePtr->handle, (LPVOID) buf, (DWORD) bufSize, &bytesRead,
            (LPOVERLAPPED) NULL) == FALSE) {
	goto error;
    }
    
    return bytesRead;

    error:
    TclWinConvertError(GetLastError());
    if (errno == EPIPE) {
	return 0;
    }
    *errorCode = errno;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * PipeOutputProc --
 *
 *	Writes the given output on the IO channel. Returns count of how
 *	many characters were actually written, and an error indication.
 *
 * Results:
 *	A count of how many characters were written is returned and an
 *	error indication is returned in an output argument.
 *
 * Side effects:
 *	Writes output on the actual channel.
 *
 *----------------------------------------------------------------------
 */

static int
PipeOutputProc(instanceData, buf, toWrite, errorCode)
    ClientData instanceData;		/* Pipe state. */
    char *buf;				/* The data buffer. */
    int toWrite;			/* How many bytes to write? */
    int *errorCode;			/* Where to store error code. */
{
    PipeInfo *infoPtr = (PipeInfo *) instanceData;
    WinFile *filePtr = (WinFile*) infoPtr->writeFile;
    DWORD bytesWritten;
    
    *errorCode = 0;
    if (WriteFile(filePtr->handle, (LPVOID) buf, (DWORD) toWrite,
	    &bytesWritten, (LPOVERLAPPED) NULL) == FALSE) {
        TclWinConvertError(GetLastError());
        if (errno == EPIPE) {
            return 0;
        }
        *errorCode = errno;
        return -1;
    }
    return bytesWritten;
}

/*
 *----------------------------------------------------------------------
 *
 * PipeEventProc --
 *
 *	This function is invoked by Tcl_ServiceEvent when a file event
 *	reaches the front of the event queue.  This procedure invokes
 *	Tcl_NotifyChannel on the pipe.
 *
 * Results:
 *	Returns 1 if the event was handled, meaning it should be removed
 *	from the queue.  Returns 0 if the event was not handled, meaning
 *	it should stay on the queue.  The only time the event isn't
 *	handled is if the TCL_FILE_EVENTS flag bit isn't set.
 *
 * Side effects:
 *	Whatever the notifier callback does.
 *
 *----------------------------------------------------------------------
 */

static int
PipeEventProc(evPtr, flags)
    Tcl_Event *evPtr;		/* Event to service. */
    int flags;			/* Flags that indicate what events to
				 * handle, such as TCL_FILE_EVENTS. */
{
    PipeEvent *pipeEvPtr = (PipeEvent *)evPtr;
    PipeInfo *infoPtr;
    WinFile *filePtr;
    int mask;
/*    DWORD count;*/

    if (!(flags & TCL_FILE_EVENTS)) {
	return 0;
    }

    /*
     * Search through the list of watched pipes for the one whose handle
     * matches the event.  We do this rather than simply dereferencing
     * the handle in the event so that pipes can be deleted while the
     * event is in the queue.
     */

    for (infoPtr = firstPipePtr; infoPtr != NULL; infoPtr = infoPtr->nextPtr) {
	if (pipeEvPtr->infoPtr == infoPtr) {
	    infoPtr->flags &= ~(PIPE_PENDING);
	    break;
	}
    }

    /*
     * Remove stale events.
     */

    if (!infoPtr) {
	return 1;
    }

    /*
     * If we aren't on Win32s, check to see if the pipe is readable.  Note
     * that we can't tell if a pipe is writable, so we always report it
     * as being writable.
     */

    filePtr = (WinFile*) ((PipeInfo*)infoPtr)->readFile;
    if (filePtr->type != WIN32S_PIPE) {

	/*
	 * On windows 95, PeekNamedPipe returns 0 on eof so we can't
	 * distinguish underflow from eof.  The correct solution is to
	 * switch to the threaded implementation.
	 */
	mask = TCL_WRITABLE|TCL_READABLE;
/* 	if (PeekNamedPipe(filePtr->handle, (LPVOID) NULL, (DWORD) 0, */
/* 		(LPDWORD) NULL, &count, (LPDWORD) NULL) == TRUE) { */
/* 	    if (count != 0) { */
/* 		mask |= TCL_READABLE; */
/* 	    } */
/* 	} else { */

	    /*
	     * If the pipe has been closed by the other side, then 
	     * mark the pipe as readable, but not writable.
	     */

/* 	    if (GetLastError() == ERROR_BROKEN_PIPE) { */
/* 		mask = TCL_READABLE; */
/* 	    } */
/* 	} */
    } else {
	mask = TCL_READABLE | TCL_WRITABLE;
    }

    /*
     * Inform the channel of the events.
     */

    Tcl_NotifyChannel(infoPtr->channel, infoPtr->watchMask & mask);
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * PipeWatchProc --
 *
 *	Called by the notifier to set up to watch for events on this
 *	channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
PipeWatchProc(instanceData, mask)
    ClientData instanceData;		/* Pipe state. */
    int mask;				/* What events to watch for; OR-ed
                                         * combination of TCL_READABLE,
                                         * TCL_WRITABLE and TCL_EXCEPTION. */
{
    PipeInfo **nextPtrPtr, *ptr;
    PipeInfo *infoPtr = (PipeInfo *) instanceData;
    int oldMask = infoPtr->watchMask;

    /*
     * For now, we just send a message to ourselves so we can poll the
     * channel for readable events.
     */

    infoPtr->watchMask = mask & infoPtr->validMask;
    if (infoPtr->watchMask) {
	Tcl_Time blockTime = { 0, 0 };
	if (!oldMask) {
	    infoPtr->nextPtr = firstPipePtr;
	    firstPipePtr = infoPtr;
	}
	Tcl_SetMaxBlockTime(&blockTime);
    } else {
	if (oldMask) {
	    /*
	     * Remove the pipe from the list of watched pipes.
	     */

	    for (nextPtrPtr = &firstPipePtr, ptr = *nextPtrPtr;
		 ptr != NULL;
		 nextPtrPtr = &ptr->nextPtr, ptr = *nextPtrPtr) {
		if (infoPtr == ptr) {
		    *nextPtrPtr = ptr->nextPtr;
		    break;
		}
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PipeGetHandleProc --
 *
 *	Called from Tcl_GetChannelHandle to retrieve OS handles from
 *	inside a command pipeline based channel.
 *
 * Results:
 *	Returns TCL_OK with the fd in handlePtr, or TCL_ERROR if
 *	there is no handle for the specified direction. 
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
PipeGetHandleProc(instanceData, direction, handlePtr)
    ClientData instanceData;	/* The pipe state. */
    int direction;		/* TCL_READABLE or TCL_WRITABLE */
    ClientData *handlePtr;	/* Where to store the handle.  */
{
    PipeInfo *infoPtr = (PipeInfo *) instanceData;
    WinFile *filePtr; 

    if (direction == TCL_READABLE && infoPtr->readFile) {
	filePtr = (WinFile*) infoPtr->readFile;
	if (filePtr->type == WIN32S_PIPE) {
	    if (filePtr->handle == INVALID_HANDLE_VALUE) {
		filePtr->handle = CreateFile(((WinPipe *)filePtr)->fileName,
			GENERIC_READ, 0, NULL, OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL, NULL);
	    }
	    if (filePtr->handle == INVALID_HANDLE_VALUE) {
		return TCL_ERROR;
	    }
	}
	*handlePtr = (ClientData) filePtr->handle;
	return TCL_OK;
    }
    if (direction == TCL_WRITABLE && infoPtr->writeFile) {
	filePtr = (WinFile*) infoPtr->writeFile;
	*handlePtr = (ClientData) filePtr->handle;
	return TCL_OK;
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WaitPid --
 *
 *	Emulates the waitpid system call.
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

Tcl_Pid
Tcl_WaitPid(pid, statPtr, options)
    Tcl_Pid pid;
    int *statPtr;
    int options;
{
    ProcInfo *infoPtr, **prevPtrPtr;
    int flags;
    Tcl_Pid result;
    DWORD ret;

    if (!initialized) {
	PipeInit();
    }

    /*
     * If no pid is specified, do nothing.
     */
    
    if (pid == 0) {
	*statPtr = 0;
	return 0;
    }

    /*
     * Find the process on the process list.
     */

    prevPtrPtr = &procList;
    for (infoPtr = procList; infoPtr != NULL;
	    prevPtrPtr = &infoPtr->nextPtr, infoPtr = infoPtr->nextPtr) {
	 if (infoPtr->hProcess == (HANDLE) pid) {
	    break;
	}
    }

    /*
     * If the pid is not one of the processes we know about (we started it)
     * then do nothing.
     */
    
    if (infoPtr == NULL) {
        *statPtr = 0;
	return 0;
    }

    /*
     * Officially "wait" for it to finish. We either poll (WNOHANG) or
     * wait for an infinite amount of time.
     */
    
    if (options & WNOHANG) {
	flags = 0;
    } else {
	flags = INFINITE;
    }
    ret = WaitForSingleObject(infoPtr->hProcess, flags);
    if (ret == WAIT_TIMEOUT) {
	*statPtr = 0;
	if (options & WNOHANG) {
	    return 0;
	} else {
	    result = 0;
	}
    } else if (ret != WAIT_FAILED) {
	GetExitCodeProcess(infoPtr->hProcess, (DWORD*)statPtr);
	*statPtr = ((*statPtr << 8) & 0xff00);
	result = pid;
    } else {
	errno = ECHILD;
        *statPtr = ECHILD;
	result = (Tcl_Pid) -1;
    }

    /*
     * Remove the process from the process list and close the process handle.
     */

    CloseHandle(infoPtr->hProcess);
    *prevPtrPtr = infoPtr->nextPtr;
    ckfree((char*)infoPtr);

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PidObjCmd --
 *
 *	This procedure is invoked to process the "pid" Tcl command.
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
int
Tcl_PidObjCmd(dummy, interp, objc, objv)
    ClientData dummy;		/* Not used. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int objc;			/* Number of arguments. */
    Tcl_Obj *CONST *objv;	/* Argument strings. */
{
    Tcl_Channel chan;
    Tcl_ChannelType *chanTypePtr;
    PipeInfo *pipePtr;
    int i;
    Tcl_Obj *resultPtr;
    char buf[20];

    if (objc > 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "?channelId?");
	return TCL_ERROR;
    }
    if (objc == 1) {
	resultPtr = Tcl_GetObjResult(interp);
	sprintf(buf, "%lu", (unsigned long) getpid());
	Tcl_SetStringObj(resultPtr, buf, -1);
    } else {
        chan = Tcl_GetChannel(interp, Tcl_GetStringFromObj(objv[1], NULL),
		NULL);
        if (chan == (Tcl_Channel) NULL) {
	    return TCL_ERROR;
	}
	chanTypePtr = Tcl_GetChannelType(chan);
	if (chanTypePtr != &pipeChannelType) {
	    return TCL_OK;
	}

        pipePtr = (PipeInfo *) Tcl_GetChannelInstanceData(chan);
	resultPtr = Tcl_GetObjResult(interp);
        for (i = 0; i < pipePtr->numPids; i++) {
	    sprintf(buf, "%lu", TclpGetPid(pipePtr->pidPtr[i]));
	    Tcl_ListObjAppendElement(/*interp*/ NULL, resultPtr,
		    Tcl_NewStringObj(buf, -1));
	}
    }
    return TCL_OK;
}
