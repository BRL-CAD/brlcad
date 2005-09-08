/*
 * bltUnixPipe.c --
 *
 *	Originally taken from tclPipe.c and tclUnixPipe.c in the Tcl
 *	distribution, implements the former Tcl_CreatePipeline API.
 *	This file contains the generic portion of the command channel
 *	driver as well as various utility routines used in managing
 *	subprocesses.
 *
 * Copyright (c) 1997 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 */

#include "bltInt.h"
#include <fcntl.h>
#include <signal.h>

#include "bltWait.h"

#if (TCL_MAJOR_VERSION == 7)
typedef pid_t Tcl_Pid;

#define FILEHANDLER_USES_TCLFILES 1

static int
Tcl_GetChannelHandle(channel, direction, clientDataPtr)
    Tcl_Channel channel;
    int direction;
    ClientData *clientDataPtr;
{
    Tcl_File file;

    file = Tcl_GetChannelFile(channel, direction);
    if (file == NULL) {
	return TCL_ERROR;
    }
    *clientDataPtr = (ClientData)Tcl_GetFileInfo(file, NULL);
    return TCL_OK;
}

#else
typedef int Tcl_File;
#endif /* TCL_MAJOR_VERSION == 7 */

/*
 *----------------------------------------------------------------------
 *
 * OpenFile --
 *
 *	Open a file for use in a pipeline.
 *
 * Results:
 *	Returns a new TclFile handle or NULL on failure.
 *
 * Side effects:
 *	May cause a file to be created on the file system.
 *
 *----------------------------------------------------------------------
 */

static int
OpenFile(fname, mode)
    char *fname;		/* The name of the file to open. */
    int mode;			/* In what mode to open the file? */
{
    int fd;

    fd = open(fname, mode, 0666);
    if (fd != -1) {
	fcntl(fd, F_SETFD, FD_CLOEXEC);

	/*
	 * If the file is being opened for writing, seek to the end
	 * so we can append to any data already in the file.
	 */

	if (mode & O_WRONLY) {
	    lseek(fd, 0, SEEK_END);
	}
	return fd;
    }
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateTempFile --
 *
 *	This function creates a temporary file initialized with an
 *	optional string, and returns a file handle with the file pointer
 *	at the beginning of the file.
 *
 * Results:
 *	A handle to a file.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
CreateTempFile(contents)
    char *contents;		/* String to write into temp file, or NULL. */
{
    char fileName[L_tmpnam];
    int fd;
    size_t length = (contents == NULL) ? 0 : strlen(contents);

    mkstemp(fileName);
    fd = OpenFile(fileName, O_RDWR | O_CREAT | O_TRUNC);
    unlink(fileName);

    if ((fd >= 0) && (length > 0)) {
	for (;;) {
	    if (write(fd, contents, length) != -1) {
		break;
	    } else if (errno != EINTR) {
		close(fd);
		return -1;
	    }
	}
	lseek(fd, 0, SEEK_SET);
    }
    return fd;
}

/*
 *----------------------------------------------------------------------
 *
 * CreatePipe --
 *
 *      Creates a pipe - simply calls the pipe() function.
 *
 * Results:
 *      Returns 1 on success, 0 on failure.
 *
 * Side effects:
 *      Creates a pipe.
 *
 *----------------------------------------------------------------------
 */

static int
CreatePipe(inFilePtr, outFilePtr)
    int *inFilePtr;		/* (out) Descriptor for read side of pipe. */
    int *outFilePtr;		/* (out) Descriptor for write side of pipe. */
{
    int pipeIds[2];

    if (pipe(pipeIds) != 0) {
	return 0;
    }
    fcntl(pipeIds[0], F_SETFD, FD_CLOEXEC);
    fcntl(pipeIds[1], F_SETFD, FD_CLOEXEC);

    *inFilePtr = pipeIds[0];
    *outFilePtr = pipeIds[1];
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * CloseFile --
 *
 *	Implements a mechanism to close a UNIX file.
 *
 * Results:
 *	Returns 0 on success, or -1 on error, setting errno.
 *
 * Side effects:
 *	The file is closed.
 *
 *----------------------------------------------------------------------
 */

static int
CloseFile(fd)
    int fd;			/* File descriptor to be closed. */
{
    if ((fd == 0) || (fd == 1) || (fd == 2)) {
	return 0;		/* Don't close stdin, stdout or stderr. */
    }
#if (TCL_MAJOR_VERSION > 7)
    Tcl_DeleteFileHandler(fd);
#endif
    return close(fd);
}

/*
 *----------------------------------------------------------------------
 *
 * RestoreSignals --
 *
 *      This procedure is invoked in a forked child process just before
 *      exec-ing a new program to restore all signals to their default
 *      settings.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Signal settings get changed.
 *
 *----------------------------------------------------------------------
 */

static void
RestoreSignals()
{
#ifdef SIGABRT
    signal(SIGABRT, SIG_DFL);
#endif
#ifdef SIGALRM
    signal(SIGALRM, SIG_DFL);
#endif
#ifdef SIGFPE
    signal(SIGFPE, SIG_DFL);
#endif
#ifdef SIGHUP
    signal(SIGHUP, SIG_DFL);
#endif
#ifdef SIGILL
    signal(SIGILL, SIG_DFL);
#endif
#ifdef SIGINT
    signal(SIGINT, SIG_DFL);
#endif
#ifdef SIGPIPE
    signal(SIGPIPE, SIG_DFL);
#endif
#ifdef SIGQUIT
    signal(SIGQUIT, SIG_DFL);
#endif
#ifdef SIGSEGV
    signal(SIGSEGV, SIG_DFL);
#endif
#ifdef SIGTERM
    signal(SIGTERM, SIG_DFL);
#endif
#ifdef SIGUSR1
    signal(SIGUSR1, SIG_DFL);
#endif
#ifdef SIGUSR2
    signal(SIGUSR2, SIG_DFL);
#endif
#ifdef SIGCHLD
    signal(SIGCHLD, SIG_DFL);
#endif
#ifdef SIGCONT
    signal(SIGCONT, SIG_DFL);
#endif
#ifdef SIGTSTP
    signal(SIGTSTP, SIG_DFL);
#endif
#ifdef SIGTTIN
    signal(SIGTTIN, SIG_DFL);
#endif
#ifdef SIGTTOU
    signal(SIGTTOU, SIG_DFL);
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * SetupStdFile --
 *
 *	Set up stdio file handles for the child process, using the
 *	current standard channels if no other files are specified.
 *	If no standard channel is defined, or if no file is associated
 *	with the channel, then the corresponding standard fd is closed.
 *
 * Results:
 *	Returns 1 on success, or 0 on failure.
 *
 * Side effects:
 *	Replaces stdio fds.
 *
 *----------------------------------------------------------------------
 */

static int
SetupStdFile(fd, type)
    int fd;			/* File descriptor to dup, or -1. */
    int type;			/* One of TCL_STDIN, TCL_STDOUT, TCL_STDERR */
{
    int targetFd = 0;		/* Initializations here needed only to */
    int direction = 0;		/* prevent warnings about using uninitialized
				 * variables. */

    switch (type) {
    case TCL_STDIN:
	targetFd = 0;
	direction = TCL_READABLE;
	break;
    case TCL_STDOUT:
	targetFd = 1;
	direction = TCL_WRITABLE;
	break;
    case TCL_STDERR:
	targetFd = 2;
	direction = TCL_WRITABLE;
	break;
    }
    if (fd < 0) {
	Tcl_Channel channel;

	channel = Tcl_GetStdChannel(type);
	if (channel) {
	    Tcl_GetChannelHandle(channel, direction, (ClientData *)&fd);
	}
    }
    if (fd >= 0) {
	if (fd != targetFd) {
	    if (dup2(fd, targetFd) == -1) {
		return 0;
	    }
	    /*
             * Must clear the close-on-exec flag for the target FD, since
             * some systems (e.g. Ultrix) do not clear the CLOEXEC flag on
             * the target FD.
             */

	    fcntl(targetFd, F_SETFD, 0);
	} else {
	    /*
	     * Since we aren't dup'ing the file, we need to explicitly clear
	     * the close-on-exec flag.
	     */
	    fcntl(fd, F_SETFD, 0);
	}
    } else {
	close(targetFd);
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateProcess --
 *
 *	Create a child process that has the specified files as its
 *	standard input, output, and error.  The child process runs
 *	asynchronously and runs with the same environment variables
 *	as the creating process.
 *
 *	The path is searched to find the specified executable.
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

/* ARGSUSED */
static int
CreateProcess(interp, argc, argv, inputFile, outputFile, errorFile, pidPtr)
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
    int inputFile;		/* If non-NULL, gives the file to use as
				 * input for the child process.  If inputFile
				 * file is not readable or is NULL, the child
				 * will receive no standard input. */
    int outputFile;		/* If non-NULL, gives the file that
				 * receives output from the child process.  If
				 * outputFile file is not writeable or is
				 * NULL, output from the child will be
				 * discarded. */
    int errorFile;		/* If non-NULL, gives the file that
				 * receives errors from the child process.  If
				 * errorFile file is not writeable or is NULL,
				 * errors from the child will be discarded.
				 * errorFile may be the same as outputFile. */
    int *pidPtr;		/* If this procedure is successful, pidPtr
				 * is filled with the process id of the child
				 * process. */
{
    int errPipeIn, errPipeOut;
    int joinThisError, count, status, fd;
    char errSpace[200];
    int pid;

    errPipeIn = errPipeOut = -1;
    pid = -1;

    /*
     * Create a pipe that the child can use to return error
     * information if anything goes wrong.
     */

    if (CreatePipe(&errPipeIn, &errPipeOut) == 0) {
	Tcl_AppendResult(interp, "can't create pipe: ",
	    Tcl_PosixError(interp), (char *)NULL);
	goto error;
    }
    joinThisError = (errorFile == outputFile);
    pid = fork();
    if (pid == 0) {
	fd = errPipeOut;

	/*
	 * Set up stdio file handles for the child process.
	 */

	if (!SetupStdFile(inputFile, TCL_STDIN) ||
	    !SetupStdFile(outputFile, TCL_STDOUT) ||
	    (!joinThisError && !SetupStdFile(errorFile, TCL_STDERR)) ||
	    (joinThisError &&
		((dup2(1, 2) == -1) || (fcntl(2, F_SETFD, 0) != 0)))) {
	    sprintf(errSpace, "%dforked process can't set up input/output: ",
		errno);
	    write(fd, errSpace, (size_t) strlen(errSpace));
	    _exit(1);
	}
	/*
	 * Close the input side of the error pipe.
	 */

	RestoreSignals();
	execvp(argv[0], &argv[0]);
	sprintf(errSpace, "%dcan't execute \"%.150s\": ", errno, argv[0]);
	write(fd, errSpace, (size_t) strlen(errSpace));
	_exit(1);
    }
    if (pid == -1) {
	Tcl_AppendResult(interp, "can't fork child process: ",
	    Tcl_PosixError(interp), (char *)NULL);
	goto error;
    }
    /*
     * Read back from the error pipe to see if the child started
     * up OK.  The info in the pipe (if any) consists of a decimal
     * errno value followed by an error message.
     */

    CloseFile(errPipeOut);
    errPipeOut = -1;

    fd = errPipeIn;
    count = read(fd, errSpace, (size_t) (sizeof(errSpace) - 1));
    if (count > 0) {
	char *end;

	errSpace[count] = 0;
	errno = strtol(errSpace, &end, 10);
	Tcl_AppendResult(interp, end, Tcl_PosixError(interp), (char *)NULL);
	goto error;
    }
    CloseFile(errPipeIn);
    *pidPtr = pid;
    return TCL_OK;

  error:
    if (pid != -1) {
	/*
	 * Reap the child process now if an error occurred during its
	 * startup.
	 */
	Tcl_WaitPid((Tcl_Pid)pid, &status, WNOHANG);
    }
    if (errPipeIn >= 0) {
	CloseFile(errPipeIn);
    }
    if (errPipeOut >= 0) {
	CloseFile(errPipeOut);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * FileForRedirect --
 *
 *	This procedure does much of the work of parsing redirection
 *	operators.  It handles "@" if specified and allowed, and a file
 *	name, and opens the file if necessary.
 *
 * Results:
 *	The return value is the descriptor number for the file.  If an
 *	error occurs then NULL is returned and an error message is left
 *	in interp->result.  Several arguments are side-effected; see
 *	the argument list below for details.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
FileForRedirect(interp, spec, atOK, arg, nextArg, flags, skipPtr, closePtr)
    Tcl_Interp *interp;		/* Intepreter to use for error reporting. */
    char *spec;			/* Points to character just after
				 * redirection character. */
    char *arg;			/* Pointer to entire argument containing
				 * spec:  used for error reporting. */
    int atOK;			/* Non-zero means that '@' notation can be
				 * used to specify a channel, zero means that
				 * it isn't. */
    char *nextArg;		/* Next argument in argc/argv array, if needed
				 * for file name or channel name.  May be
				 * NULL. */
    int flags;			/* Flags to use for opening file or to
				 * specify mode for channel. */
    int *skipPtr;		/* Filled with 1 if redirection target was
				 * in spec, 2 if it was in nextArg. */
    int *closePtr;		/* Filled with one if the caller should
				 * close the file when done with it, zero
				 * otherwise. */
{
    int writing = (flags & O_WRONLY);
    Tcl_Channel chan;
    int fd;
    int direction;

    *skipPtr = 1;
    if ((atOK != 0) && (*spec == '@')) {
	spec++;
	if (*spec == '\0') {
	    spec = nextArg;
	    if (spec == NULL) {
		goto badLastArg;
	    }
	    *skipPtr = 2;
	}
	chan = Tcl_GetChannel(interp, spec, NULL);
	if (chan == NULL) {
	    return -1;
	}
	direction = (writing) ? TCL_WRITABLE : TCL_READABLE;
	if (Tcl_GetChannelHandle(chan, direction, (ClientData *)&fd) != TCL_OK) {
	    fd = -1;
	}
	if (fd < 0) {
	    Tcl_AppendResult(interp, "channel \"", Tcl_GetChannelName(chan),
		"\" wasn't opened for ",
		((writing) ? "writing" : "reading"), (char *)NULL);
	    return -1;
	}
	if (writing) {
	    /*
	     * Be sure to flush output to the file, so that anything
	     * written by the child appears after stuff we've already
	     * written.
	     */
	    Tcl_Flush(chan);
	}
    } else {
	char *name;
	Tcl_DString nameString;

	if (*spec == '\0') {
	    spec = nextArg;
	    if (spec == NULL) {
		goto badLastArg;
	    }
	    *skipPtr = 2;
	}
	name = Tcl_TranslateFileName(interp, spec, &nameString);

	if (name != NULL) {
	    fd = OpenFile(name, flags);
	} else {
	    fd = -1;
	}
	Tcl_DStringFree(&nameString);
	if (fd < 0) {
	    Tcl_AppendResult(interp, "can't ",
		((writing) ? "write" : "read"), " file \"", spec, "\": ",
		Tcl_PosixError(interp), (char *)NULL);
	    return -1;
	}
	*closePtr = 1;
    }
    return fd;

  badLastArg:
    Tcl_AppendResult(interp, "can't specify \"", arg,
	"\" as last word in command", (char *)NULL);
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * Blt_CreatePipeline --
 *
 *	Given an argc/argv array, instantiate a pipeline of processes
 *	as described by the argv.
 *
 * Results:
 *	The return value is a count of the number of new processes
 *	created, or -1 if an error occurred while creating the pipeline.
 *	*pidArrayPtr is filled in with the address of a dynamically
 *	allocated array giving the ids of all of the processes.  It
 *	is up to the caller to free this array when it isn't needed
 *	anymore.  If inPipePtr is non-NULL, *inPipePtr is filled in
 *	with the file id for the input pipe for the pipeline (if any):
 *	the caller must eventually close this file.  If outPipePtr
 *	isn't NULL, then *outPipePtr is filled in with the file id
 *	for the output pipe from the pipeline:  the caller must close
 *	this file.  If errPipePtr isn't NULL, then *errPipePtr is filled
 *	with a file id that may be used to read error output after the
 *	pipeline completes.
 *
 * Side effects:
 *	Processes and pipes are created.
 *
 *----------------------------------------------------------------------
 */

int
Blt_CreatePipeline(interp, argc, argv, pidArrayPtr, inPipePtr,
    outPipePtr, errPipePtr)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    int argc;			/* Number of entries in argv. */
    char **argv;		/* Array of strings describing commands in
				 * pipeline plus I/O redirection with <,
				 * <<,  >, etc.  Argv[argc] must be NULL. */
    int **pidArrayPtr;		/* Word at *pidArrayPtr gets filled in with
				 * address of array of pids for processes
				 * in pipeline (first pid is first process
				 * in pipeline). */
    int *inPipePtr;		/* If non-NULL, input to the pipeline comes
				 * from a pipe (unless overridden by
				 * redirection in the command).  The file
				 * id with which to write to this pipe is
				 * stored at *inPipePtr.  NULL means command
				 * specified its own input source. */
    int *outPipePtr;		/* If non-NULL, output to the pipeline goes
				 * to a pipe, unless overriden by redirection
				 * in the command.  The file id with which to
				 * read frome this pipe is stored at
				 * *outPipePtr.  NULL means command specified
				 * its own output sink. */
    int *errPipePtr;		/* If non-NULL, all stderr output from the
				 * pipeline will go to a temporary file
				 * created here, and a descriptor to read
				 * the file will be left at *errPipePtr.
				 * The file will be removed already, so
				 * closing this descriptor will be the end
				 * of the file.  If this is NULL, then
				 * all stderr output goes to our stderr.
				 * If the pipeline specifies redirection
				 * then the file will still be created
				 * but it will never get any data. */
{
    int *pidPtr = NULL;		/* Points to malloc-ed array holding all
				 * the pids of child processes. */
    int nPids;			/* Actual number of processes that exist
				 * at *pidPtr right now. */
    int cmdCount;		/* Count of number of distinct commands
				 * found in argc/argv. */
    char *inputLiteral = NULL;	/* If non-null, then this points to a
				 * string containing input data (specified
				 * via <<) to be piped to the first process
				 * in the pipeline. */
    int inputFd = -1;		/* If != NULL, gives file to use as input for
				 * first process in pipeline (specified via <
				 * or <@). */
    int inputClose = 0;		/* If non-zero, then inputFd should be
    				 * closed when cleaning up. */
    int outputFd = -1;		/* Writable file for output from last command
				 * in pipeline (could be file or pipe).  NULL
				 * means use stdout. */
    int outputClose = 0;	/* If non-zero, then outputFd should be
    				 * closed when cleaning up. */
    int errorFd = -1;		/* Writable file for error output from all
				 * commands in pipeline.  NULL means use
				 * stderr. */
    int errorClose = 0;		/* If non-zero, then errorFd should be
    				 * closed when cleaning up. */
    char *p;
    int skip, lastBar, lastArg, i, j, atOK, flags, errorToOutput;
    Tcl_DString execBuffer;
    int pipeIn;
    int curInFd, curOutFd, curErrFd;

    if (inPipePtr != NULL) {
	*inPipePtr = -1;
    }
    if (outPipePtr != NULL) {
	*outPipePtr = -1;
    }
    if (errPipePtr != NULL) {
	*errPipePtr = -1;
    }
    Tcl_DStringInit(&execBuffer);

    pipeIn = curInFd = curOutFd = -1;
    nPids = 0;

    /*
     * First, scan through all the arguments to figure out the structure
     * of the pipeline.  Process all of the input and output redirection
     * arguments and remove them from the argument list in the pipeline.
     * Count the number of distinct processes (it's the number of "|"
     * arguments plus one) but don't remove the "|" arguments because
     * they'll be used in the second pass to seperate the individual
     * child processes.  Cannot start the child processes in this pass
     * because the redirection symbols may appear anywhere in the
     * command line -- e.g., the '<' that specifies the input to the
     * entire pipe may appear at the very end of the argument list.
     */

    lastBar = -1;
    cmdCount = 1;
    for (i = 0; i < argc; i++) {
	skip = 0;
	p = argv[i];
	switch (*p++) {
	case '\\':
	    continue;

	case '|':
	    if (*p == '&') {
		p++;
	    }
	    if (*p == '\0') {
		if ((i == (lastBar + 1)) || (i == (argc - 1))) {
		    Tcl_AppendResult(interp, 
				     "illegal use of | or |& in command",
				     (char *)NULL);
		    goto error;
		}
	    }
	    lastBar = i;
	    cmdCount++;
	    break;

	case '<':
	    if (inputClose != 0) {
		inputClose = 0;
		CloseFile(inputFd);
	    }
	    if (*p == '<') {
		inputFd = -1;
		inputLiteral = p + 1;
		skip = 1;
		if (*inputLiteral == '\0') {
		    inputLiteral = argv[i + 1];
		    if (inputLiteral == NULL) {
			Tcl_AppendResult(interp, "can't specify \"", argv[i],
			    "\" as last word in command", (char *)NULL);
			goto error;
		    }
		    skip = 2;
		}
	    } else {
		inputLiteral = NULL;
		inputFd = FileForRedirect(interp, p, 1, argv[i], argv[i + 1],
		    O_RDONLY, &skip, &inputClose);
		if (inputFd < 0) {
		    goto error;
		}
	    }
	    break;

	case '>':
	    atOK = 1;
	    flags = O_WRONLY | O_CREAT | O_TRUNC;
	    errorToOutput = 0;
	    if (*p == '>') {
		p++;
		atOK = 0;
		flags = O_WRONLY | O_CREAT;
	    }
	    if (*p == '&') {
		if (errorClose != 0) {
		    errorClose = 0;
		    CloseFile(errorFd);
		}
		errorToOutput = 1;
		p++;
	    }
	    if (outputClose != 0) {
		outputClose = 0;
		CloseFile(outputFd);
	    }
	    outputFd = FileForRedirect(interp, p, atOK, argv[i], argv[i + 1],
		flags, &skip, &outputClose);
	    if (outputFd < 0) {
		goto error;
	    }
	    if (errorToOutput) {
		errorClose = 0;
		errorFd = outputFd;
	    }
	    break;

	case '2':
	    if (*p != '>') {
		break;
	    }
	    p++;
	    atOK = 1;
	    flags = O_WRONLY | O_CREAT | O_TRUNC;
	    if (*p == '>') {
		p++;
		atOK = 0;
		flags = O_WRONLY | O_CREAT;
	    }
	    if (errorClose != 0) {
		errorClose = 0;
		CloseFile(errorFd);
	    }
	    errorFd = FileForRedirect(interp, p, atOK, argv[i], argv[i + 1],
		flags, &skip, &errorClose);
	    if (errorFd < 0) {
		goto error;
	    }
	    break;
	}

	if (skip != 0) {
	    for (j = i + skip; j < argc; j++) {
		argv[j - skip] = argv[j];
	    }
	    argc -= skip;
	    i -= 1;
	}
    }

    if (inputFd == -1) {
	if (inputLiteral != NULL) {
	    /*
	     * The input for the first process is immediate data coming from
	     * Tcl.  Create a temporary file for it and put the data into the
	     * file.
	     */
	    inputFd = CreateTempFile(inputLiteral);
	    if (inputFd < 0) {
		Tcl_AppendResult(interp,
		    "can't create input file for command: ",
		    Tcl_PosixError(interp), (char *)NULL);
		goto error;
	    }
	    inputClose = 1;
	} else if (inPipePtr != NULL) {
	    /*
	     * The input for the first process in the pipeline is to
	     * come from a pipe that can be written from by the caller.
	     */

	    if (CreatePipe(&inputFd, inPipePtr) == 0) {
		Tcl_AppendResult(interp,
		    "can't create input pipe for command: ",
		    Tcl_PosixError(interp), (char *)NULL);
		goto error;
	    }
	    inputClose = 1;
	} else {
	    /*
	     * The input for the first process comes from stdin.
	     */

	    inputFd = 0;
	}
    }
    if (outputFd == -1) {
	if (outPipePtr != NULL) {
	    /*
	     * Output from the last process in the pipeline is to go to a
	     * pipe that can be read by the caller.
	     */

	    if (CreatePipe(outPipePtr, &outputFd) == 0) {
		Tcl_AppendResult(interp,
		    "can't create output pipe for command: ",
		    Tcl_PosixError(interp), (char *)NULL);
		goto error;
	    }
	    outputClose = 1;
	} else {
	    /*
	     * The output for the last process goes to stdout.
	     */
	    outputFd = 1;
	}
    }
    if (errorFd == -1) {
	if (errPipePtr != NULL) {
	    /*
	     * Stderr from the last process in the pipeline is to go to a
	     * pipe that can be read by the caller.
	     */
	    if (CreatePipe(errPipePtr, &errorFd) == 0) {
		Tcl_AppendResult(interp,
		    "can't create error pipe for command: ",
		    Tcl_PosixError(interp), (char *)NULL);
		goto error;
	    }
	    errorClose = 1;
	} else {
	    /*
	     * Errors from the pipeline go to stderr.
	     */
	    errorFd = 2;
	}
    }
    /*
     * Scan through the argc array, creating a process for each
     * group of arguments between the "|" characters.
     */

    Tcl_ReapDetachedProcs();
    pidPtr = Blt_Malloc((unsigned)(cmdCount * sizeof(int)));

    curInFd = inputFd;

    lastArg = 0;		/* Suppress compiler warning */
    for (i = 0; i < argc; i = lastArg + 1) {
	int joinThisError;
	int pid;

	/*
	 * Convert the program name into native form.
	 */

	argv[i] = Tcl_TranslateFileName(interp, argv[i], &execBuffer);
	if (argv[i] == NULL) {
	    goto error;
	}
	/*
	 * Find the end of the current segment of the pipeline.
	 */
	joinThisError = 0;
	for (lastArg = i; lastArg < argc; lastArg++) {
	    if (argv[lastArg][0] == '|') {
		if (argv[lastArg][1] == '\0') {
		    break;
		}
		if ((argv[lastArg][1] == '&') && (argv[lastArg][2] == '\0')) {
		    joinThisError = 1;
		    break;
		}
	    }
	}
	argv[lastArg] = NULL;

	/*
	 * If this is the last segment, use the specified outputFile.
	 * Otherwise create an intermediate pipe.  pipeIn will become the
	 * curInFile for the next segment of the pipe.
	 */

	if (lastArg == argc) {
	    curOutFd = outputFd;
	} else {
	    if (CreatePipe(&pipeIn, &curOutFd) == 0) {
		Tcl_AppendResult(interp, "can't create pipe: ",
		    Tcl_PosixError(interp), (char *)NULL);
		goto error;
	    }
	}

	if (joinThisError != 0) {
	    curErrFd = curOutFd;
	} else {
	    curErrFd = errorFd;
	}

	if (CreateProcess(interp, lastArg - i, argv + i,
		curInFd, curOutFd, curErrFd, &pid) != TCL_OK) {
	    goto error;
	}
	Tcl_DStringFree(&execBuffer);

	pidPtr[nPids] = pid;
	nPids++;


	/*
	 * Close off our copies of file descriptors that were set up for
	 * this child, then set up the input for the next child.
	 */

	if ((curInFd >= 0) && (curInFd != inputFd)) {
	    CloseFile(curInFd);
	}
	curInFd = pipeIn;
	pipeIn = -1;

	if ((curOutFd >= 0) && (curOutFd != outputFd)) {
	    CloseFile(curOutFd);
	}
	curOutFd = -1;
    }

    *pidArrayPtr = pidPtr;

    /*
     * All done.  Cleanup open files lying around and then return.
     */

  cleanup:
    Tcl_DStringFree(&execBuffer);

    if (inputClose) {
	CloseFile(inputFd);
    }
    if (outputClose) {
	CloseFile(outputFd);
    }
    if (errorClose) {
	CloseFile(errorFd);
    }
    return nPids;

    /*
     * An error occurred.  There could have been extra files open, such
     * as pipes between children.  Clean them all up.  Detach any child
     * processes that have been created.
     */

  error:
    if (pipeIn >= 0) {
	CloseFile(pipeIn);
    }
    if ((curOutFd >= 0) && (curOutFd != outputFd)) {
	CloseFile(curOutFd);
    }
    if ((curInFd >= 0) && (curInFd != inputFd)) {
	CloseFile(curInFd);
    }
    if ((inPipePtr != NULL) && (*inPipePtr >= 0)) {
	CloseFile(*inPipePtr);
	*inPipePtr = -1;
    }
    if ((outPipePtr != NULL) && (*outPipePtr >= 0)) {
	CloseFile(*outPipePtr);
	*outPipePtr = -1;
    }
    if ((errPipePtr != NULL) && (*errPipePtr >= 0)) {
	CloseFile(*errPipePtr);
	*errPipePtr = -1;
    }
    if (pidPtr != NULL) {
	for (i = 0; i < nPids; i++) {
	    if (pidPtr[i] != -1) {
#if (TCL_MAJOR_VERSION == 7)
		Tcl_DetachPids(1, &pidPtr[i]);
#else
		Tcl_DetachPids(1, (Tcl_Pid *)&pidPtr[i]);
#endif
	    }
	}
	Blt_Free(pidPtr);
    }
    nPids = -1;
    goto cleanup;
}
