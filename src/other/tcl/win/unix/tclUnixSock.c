/*
 * tclUnixSock.c --
 *
 *	This file contains Unix-specific socket related code.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) $Id$
 */

#include "tclInt.h"

/*
 * Helper macros to make parts of this file clearer. The macros do exactly
 * what they say on the tin. :-) They also only ever refer to their arguments
 * once, and so can be used without regard to side effects.
 */

#define SET_BITS(var, bits)	((var) |= (bits))
#define CLEAR_BITS(var, bits)	((var) &= ~(bits))

/*
 * This structure describes per-instance state of a tcp based channel.
 */

typedef struct TcpState {
    Tcl_Channel channel;	/* Channel associated with this file. */
    int fd;			/* The socket itself. */
    int flags;			/* ORed combination of the bitfields defined
				 * below. */
    Tcl_TcpAcceptProc *acceptProc;
				/* Proc to call on accept. */
    ClientData acceptProcData;	/* The data for the accept proc. */
} TcpState;

/*
 * These bits may be ORed together into the "flags" field of a TcpState
 * structure.
 */

#define TCP_ASYNC_SOCKET	(1<<0)	/* Asynchronous socket. */
#define TCP_ASYNC_CONNECT	(1<<1)	/* Async connect in progress. */

/*
 * The following defines the maximum length of the listen queue. This is the
 * number of outstanding yet-to-be-serviced requests for a connection on a
 * server socket, more than this number of outstanding requests and the
 * connection request will fail.
 */

#ifndef SOMAXCONN
#   define SOMAXCONN	100
#endif /* SOMAXCONN */

#if (SOMAXCONN < 100)
#   undef  SOMAXCONN
#   define SOMAXCONN	100
#endif /* SOMAXCONN < 100 */

/*
 * The following defines how much buffer space the kernel should maintain for
 * a socket.
 */

#define SOCKET_BUFSIZE	4096

/*
 * Static routines for this file:
 */

static TcpState *	CreateSocket(Tcl_Interp *interp, int port,
			    const char *host, int server, const char *myaddr,
			    int myport, int async);
static int		CreateSocketAddress(struct sockaddr_in *sockaddrPtr,
			    const char *host, int port, int willBind,
			    const char **errorMsgPtr);
static void		TcpAccept(ClientData data, int mask);
static int		TcpBlockModeProc(ClientData data, int mode);
static int		TcpCloseProc(ClientData instanceData,
			    Tcl_Interp *interp);
static int		TcpClose2Proc(ClientData instanceData,
				      Tcl_Interp *interp,
				      int flags);
static int		TcpGetHandleProc(ClientData instanceData,
			    int direction, ClientData *handlePtr);
static int		TcpGetOptionProc(ClientData instanceData,
			    Tcl_Interp *interp, const char *optionName,
			    Tcl_DString *dsPtr);
static int		TcpInputProc(ClientData instanceData, char *buf,
			    int toRead, int *errorCode);
static int		TcpOutputProc(ClientData instanceData,
			    const char *buf, int toWrite, int *errorCode);
static void		TcpWatchProc(ClientData instanceData, int mask);
static int		WaitForConnect(TcpState *statePtr, int *errorCodePtr);
/*
 * This structure describes the channel type structure for TCP socket
 * based IO:
 */

static const Tcl_ChannelType tcpChannelType = {
    "tcp",			/* Type name. */
    TCL_CHANNEL_VERSION_5,	/* v5 channel */
    TcpCloseProc,		/* Close proc. */
    TcpInputProc,		/* Input proc. */
    TcpOutputProc,		/* Output proc. */
    NULL,			/* Seek proc. */
    NULL,			/* Set option proc. */
    TcpGetOptionProc,		/* Get option proc. */
    TcpWatchProc,		/* Initialize notifier. */
    TcpGetHandleProc,		/* Get OS handles out of channel. */
    TcpClose2Proc,		/* Close2 proc. */
    TcpBlockModeProc,		/* Set blocking or non-blocking mode.*/
    NULL,			/* flush proc. */
    NULL,			/* handler proc. */
    NULL,			/* wide seek proc. */
    NULL,			/* thread action proc. */
    NULL			/* truncate proc. */
};


/*
 * The following variable holds the network name of this host.
 */

static TclInitProcessGlobalValueProc InitializeHostName;
static ProcessGlobalValue hostName =
	{0, 0, NULL, NULL, InitializeHostName, NULL, NULL};


/*
 *----------------------------------------------------------------------
 *
 * InitializeHostName --
 *
 * 	This routine sets the process global value of the name of the local
 * 	host on which the process is running.
 *
 * Results:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
InitializeHostName(
    char **valuePtr,
    int *lengthPtr,
    Tcl_Encoding *encodingPtr)
{
    const char *native = NULL;

#ifndef NO_UNAME
    struct utsname u;
    struct hostent *hp;

    memset(&u, (int) 0, sizeof(struct utsname));
    if (uname(&u) > -1) {				/* INTL: Native. */
        hp = TclpGetHostByName(u.nodename);		/* INTL: Native. */
	if (hp == NULL) {
	    /*
	     * Sometimes the nodename is fully qualified, but gets truncated
	     * as it exceeds SYS_NMLN. See if we can just get the immediate
	     * nodename and get a proper answer that way.
	     */

	    char *dot = strchr(u.nodename, '.');

	    if (dot != NULL) {
		char *node = ckalloc((unsigned) (dot - u.nodename + 1));

		memcpy(node, u.nodename, (size_t) (dot - u.nodename));
		node[dot - u.nodename] = '\0';
		hp = TclpGetHostByName(node);
		ckfree(node);
	    }
	}
        if (hp != NULL) {
	    native = hp->h_name;
        } else {
	    native = u.nodename;
        }
    }
    if (native == NULL) {
	native = tclEmptyStringRep;
    }
#else
    /*
     * Uname doesn't exist; try gethostname instead.
     *
     * There is no portable macro for the maximum length of host names
     * returned by gethostbyname(). We should only trust SYS_NMLN if it is at
     * least 255 + 1 bytes to comply with DNS host name limits.
     *
     * Note: SYS_NMLN is a restriction on "uname" not on gethostbyname!
     *
     * For example HP-UX 10.20 has SYS_NMLN == 9, while gethostbyname() can
     * return a fully qualified name from DNS of up to 255 bytes.
     *
     * Fix suggested by Viktor Dukhovni (viktor@esm.com)
     */

#    if defined(SYS_NMLN) && (SYS_NMLEN >= 256)
    char buffer[SYS_NMLEN];
#    else
    char buffer[256];
#    endif

    if (gethostname(buffer, sizeof(buffer)) > -1) {	/* INTL: Native. */
	native = buffer;
    }
#endif

    *encodingPtr = Tcl_GetEncoding(NULL, NULL);
    *lengthPtr = strlen(native);
    *valuePtr = ckalloc((unsigned) (*lengthPtr) + 1);
    memcpy(*valuePtr, native, (size_t)(*lengthPtr)+1);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_GetHostName --
 *
 *	Returns the name of the local host.
 *
 * Results:
 *	A string containing the network name for this machine, or an empty
 *	string if we can't figure out the name. The caller must not modify or
 *	free this string.
 *
 * Side effects:
 *	Caches the name to return for future calls.
 *
 *----------------------------------------------------------------------
 */

const char *
Tcl_GetHostName(void)
{
    return Tcl_GetString(TclGetProcessGlobalValue(&hostName));
}

/*
 *----------------------------------------------------------------------
 *
 * TclpHasSockets --
 *
 *	Detect if sockets are available on this platform.
 *
 * Results:
 *	Returns TCL_OK.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclpHasSockets(
    Tcl_Interp *interp)		/* Not used. */
{
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclpFinalizeSockets --
 *
 *	Performs per-thread socket subsystem finalization.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclpFinalizeSockets(void)
{
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpBlockModeProc --
 *
 *	This function is invoked by the generic IO level to set blocking and
 *	nonblocking mode on a TCP socket based channel.
 *
 * Results:
 *	0 if successful, errno when failed.
 *
 * Side effects:
 *	Sets the device into blocking or nonblocking mode.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TcpBlockModeProc(
    ClientData instanceData,	/* Socket state. */
    int mode)			/* The mode to set. Can be one of
				 * TCL_MODE_BLOCKING or
				 * TCL_MODE_NONBLOCKING. */
{
    TcpState *statePtr = (TcpState *) instanceData;

    if (mode == TCL_MODE_BLOCKING) {
	CLEAR_BITS(statePtr->flags, TCP_ASYNC_SOCKET);
    } else {
	SET_BITS(statePtr->flags, TCP_ASYNC_SOCKET);
    }
    if (TclUnixSetBlockingMode(statePtr->fd, mode) < 0) {
	return errno;
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * WaitForConnect --
 *
 *	Wait for a connection on an asynchronously opened socket to be
 *	completed.  In nonblocking mode, just test if the connection
 *	has completed without blocking.
 *
 * Results:
 * 	0 if the connection has completed, -1 if still in progress
 * 	or there is an error.
 *
 *----------------------------------------------------------------------
 */

static int
WaitForConnect(
    TcpState *statePtr,		/* State of the socket. */
    int *errorCodePtr)		/* Where to store errors? */
{
    int timeOut;		/* How long to wait. */
    int state;			/* Of calling TclWaitForFile. */

    /*
     * If an asynchronous connect is in progress, attempt to wait for it to
     * complete before reading.
     */

    if (statePtr->flags & TCP_ASYNC_CONNECT) {
	if (statePtr->flags & TCP_ASYNC_SOCKET) {
	    timeOut = 0;
	} else {
	    timeOut = -1;
	}
	errno = 0;
	state = TclUnixWaitForFile(statePtr->fd,
		TCL_WRITABLE | TCL_EXCEPTION, timeOut);
	if (state & TCL_EXCEPTION) {
	    return -1;
	}
	if (state & TCL_WRITABLE) {
	    CLEAR_BITS(statePtr->flags, TCP_ASYNC_CONNECT);
	} else if (timeOut == 0) {
	    *errorCodePtr = errno = EWOULDBLOCK;
	    return -1;
	}
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpInputProc --
 *
 *	This function is invoked by the generic IO level to read input from a
 *	TCP socket based channel.
 *
 *	NOTE: We cannot share code with FilePipeInputProc because here we must
 *	use recv to obtain the input from the channel, not read.
 *
 * Results:
 *	The number of bytes read is returned or -1 on error. An output
 *	argument contains the POSIX error code on error, or zero if no error
 *	occurred.
 *
 * Side effects:
 *	Reads input from the input device of the channel.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TcpInputProc(
    ClientData instanceData,	/* Socket state. */
    char *buf,			/* Where to store data read. */
    int bufSize,		/* How much space is available in the
				 * buffer? */
    int *errorCodePtr)		/* Where to store error code. */
{
    TcpState *statePtr = (TcpState *) instanceData;
    int bytesRead;

    *errorCodePtr = 0;
    if (WaitForConnect(statePtr, errorCodePtr) != 0) {
	return -1;
    }
    bytesRead = recv(statePtr->fd, buf, (size_t) bufSize, 0);
    if (bytesRead > -1) {
	return bytesRead;
    }
    if (errno == ECONNRESET) {
	/*
	 * Turn ECONNRESET into a soft EOF condition.
	 */

	return 0;
    }
    *errorCodePtr = errno;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpOutputProc --
 *
 *	This function is invoked by the generic IO level to write output to a
 *	TCP socket based channel.
 *
 *	NOTE: We cannot share code with FilePipeOutputProc because here we
 *	must use send, not write, to get reliable error reporting.
 *
 * Results:
 *	The number of bytes written is returned. An output argument is set to
 *	a POSIX error code if an error occurred, or zero.
 *
 * Side effects:
 *	Writes output on the output device of the channel.
 *
 *----------------------------------------------------------------------
 */

static int
TcpOutputProc(
    ClientData instanceData,	/* Socket state. */
    const char *buf,		/* The data buffer. */
    int toWrite,		/* How many bytes to write? */
    int *errorCodePtr)		/* Where to store error code. */
{
    TcpState *statePtr = (TcpState *) instanceData;
    int written;

    *errorCodePtr = 0;
    if (WaitForConnect(statePtr, errorCodePtr) != 0) {
	return -1;
    }
    written = send(statePtr->fd, buf, (size_t) toWrite, 0);
    if (written > -1) {
	return written;
    }
    *errorCodePtr = errno;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpCloseProc --
 *
 *	This function is invoked by the generic IO level to perform
 *	channel-type-specific cleanup when a TCP socket based channel is
 *	closed.
 *
 * Results:
 *	0 if successful, the value of errno if failed.
 *
 * Side effects:
 *	Closes the socket of the channel.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TcpCloseProc(
    ClientData instanceData,	/* The socket to close. */
    Tcl_Interp *interp)		/* For error reporting - unused. */
{
    TcpState *statePtr = (TcpState *) instanceData;
    int errorCode = 0;

    /*
     * Delete a file handler that may be active for this socket if this is a
     * server socket - the file handler was created automatically by Tcl as
     * part of the mechanism to accept new client connections. Channel
     * handlers are already deleted in the generic IO channel closing code
     * that called this function, so we do not have to delete them here.
     */

    Tcl_DeleteFileHandler(statePtr->fd);

    if (close(statePtr->fd) < 0) {
	errorCode = errno;
    }
    ckfree((char *) statePtr);

    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpClose2Proc --
 *
 *	This function is called by the generic IO level to perform the channel
 *	type specific part of a half-close: namely, a shutdown() on a socket.
 *
 * Results:
 *	0 if successful, the value of errno if failed.
 *
 * Side effects:
 *	Shuts down one side of the socket.
 *
 *----------------------------------------------------------------------
 */

static int
TcpClose2Proc(
    ClientData instanceData,	/* The socket to close. */
    Tcl_Interp *interp,		/* For error reporting. */
    int flags)			/* Flags that indicate which side to close. */
{
    TcpState *statePtr = (TcpState *) instanceData;
    int errorCode = 0;
    int sd;

    /*
     * Shutdown the OS socket handle.
     */
    switch(flags)
	{
	case TCL_CLOSE_READ:
	    sd=SHUT_RD;
	    break;
	case TCL_CLOSE_WRITE:
	    sd=SHUT_WR;
	    break;
	default:
	    if (interp) {
		Tcl_AppendResult(interp, "Socket close2proc called bidirectionally", NULL);
	    }
	    return TCL_ERROR;
	}
    if (shutdown(statePtr->fd,sd)<0) {
	errorCode = errno;
    }

    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpGetOptionProc --
 *
 *	Computes an option value for a TCP socket based channel, or a list of
 *	all options and their values.
 *
 *	Note: This code is based on code contributed by John Haxby.
 *
 * Results:
 *	A standard Tcl result. The value of the specified option or a list of
 *	all options and their values is returned in the supplied DString. Sets
 *	Error message if needed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
TcpGetOptionProc(
    ClientData instanceData,	/* Socket state. */
    Tcl_Interp *interp,		/* For error reporting - can be NULL. */
    const char *optionName,	/* Name of the option to retrieve the value
				 * for, or NULL to get all options and their
				 * values. */
    Tcl_DString *dsPtr)		/* Where to store the computed value;
				 * initialized by caller. */
{
    TcpState *statePtr = (TcpState *) instanceData;
    struct sockaddr_in sockname;
    struct sockaddr_in peername;
    struct hostent *hostEntPtr;
    socklen_t size = sizeof(struct sockaddr_in);
    size_t len = 0;
    char buf[TCL_INTEGER_SPACE];

    if (optionName != NULL) {
	len = strlen(optionName);
    }

    if ((len > 1) && (optionName[1] == 'e') &&
	    (strncmp(optionName, "-error", len) == 0)) {
	socklen_t optlen = sizeof(int);
	int err, ret;

	ret = getsockopt(statePtr->fd, SOL_SOCKET, SO_ERROR,
		(char *)&err, &optlen);
	if (ret < 0) {
	    err = errno;
	}
	if (err != 0) {
	    Tcl_DStringAppend(dsPtr, Tcl_ErrnoMsg(err), -1);
	}
	return TCL_OK;
    }

    if ((len == 0) ||
	    ((len > 1) && (optionName[1] == 'p') &&
		    (strncmp(optionName, "-peername", len) == 0))) {
	if (getpeername(statePtr->fd, (struct sockaddr *) &peername,
		&size) >= 0) {
	    if (len == 0) {
		Tcl_DStringAppendElement(dsPtr, "-peername");
		Tcl_DStringStartSublist(dsPtr);
	    }
	    Tcl_DStringAppendElement(dsPtr, inet_ntoa(peername.sin_addr));
	    hostEntPtr = TclpGetHostByAddr(			/* INTL: Native. */
		    (char *) &peername.sin_addr,
		    sizeof(peername.sin_addr), AF_INET);
	    if (hostEntPtr != NULL) {
		Tcl_DString ds;

		Tcl_ExternalToUtfDString(NULL, hostEntPtr->h_name, -1, &ds);
		Tcl_DStringAppendElement(dsPtr, Tcl_DStringValue(&ds));
		Tcl_DStringFree(&ds);
	    } else {
		Tcl_DStringAppendElement(dsPtr, inet_ntoa(peername.sin_addr));
	    }
	    TclFormatInt(buf, ntohs(peername.sin_port));
	    Tcl_DStringAppendElement(dsPtr, buf);
	    if (len == 0) {
		Tcl_DStringEndSublist(dsPtr);
	    } else {
		return TCL_OK;
	    }
	} else {
	    /*
	     * getpeername failed - but if we were asked for all the options
	     * (len==0), don't flag an error at that point because it could be
	     * an fconfigure request on a server socket (which have no peer).
	     * Same must be done on win&mac.
	     */

	    if (len) {
		if (interp) {
		    Tcl_AppendResult(interp, "can't get peername: ",
			    Tcl_PosixError(interp), NULL);
		}
		return TCL_ERROR;
	    }
	}
    }

    if ((len == 0) ||
	    ((len > 1) && (optionName[1] == 's') &&
	    (strncmp(optionName, "-sockname", len) == 0))) {
	if (getsockname(statePtr->fd, (struct sockaddr *) &sockname,
		&size) >= 0) {
	    if (len == 0) {
		Tcl_DStringAppendElement(dsPtr, "-sockname");
		Tcl_DStringStartSublist(dsPtr);
	    }
	    Tcl_DStringAppendElement(dsPtr, inet_ntoa(sockname.sin_addr));
	    hostEntPtr = TclpGetHostByAddr(			/* INTL: Native. */
		    (char *) &sockname.sin_addr,
		    sizeof(sockname.sin_addr), AF_INET);
	    if (hostEntPtr != NULL) {
		Tcl_DString ds;

		Tcl_ExternalToUtfDString(NULL, hostEntPtr->h_name, -1, &ds);
		Tcl_DStringAppendElement(dsPtr, Tcl_DStringValue(&ds));
		Tcl_DStringFree(&ds);
	    } else {
		Tcl_DStringAppendElement(dsPtr, inet_ntoa(sockname.sin_addr));
	    }
	    TclFormatInt(buf, ntohs(sockname.sin_port));
	    Tcl_DStringAppendElement(dsPtr, buf);
	    if (len == 0) {
		Tcl_DStringEndSublist(dsPtr);
	    } else {
		return TCL_OK;
	    }
	} else {
	    if (interp) {
		Tcl_AppendResult(interp, "can't get sockname: ",
			Tcl_PosixError(interp), NULL);
	    }
	    return TCL_ERROR;
	}
    }

    if (len > 0) {
	return Tcl_BadChannelOption(interp, optionName, "peername sockname");
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpWatchProc --
 *
 *	Initialize the notifier to watch the fd from this channel.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets up the notifier so that a future event on the channel will be
 *	seen by Tcl.
 *
 *----------------------------------------------------------------------
 */

static void
TcpWatchProc(
    ClientData instanceData,	/* The socket state. */
    int mask)			/* Events of interest; an OR-ed combination of
				 * TCL_READABLE, TCL_WRITABLE and
				 * TCL_EXCEPTION. */
{
    TcpState *statePtr = (TcpState *) instanceData;

    /*
     * Make sure we don't mess with server sockets since they will never be
     * readable or writable at the Tcl level. This keeps Tcl scripts from
     * interfering with the -accept behavior.
     */

    if (!statePtr->acceptProc) {
	if (mask) {
	    Tcl_CreateFileHandler(statePtr->fd, mask,
		    (Tcl_FileProc *) Tcl_NotifyChannel,
		    (ClientData) statePtr->channel);
	} else {
	    Tcl_DeleteFileHandler(statePtr->fd);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TcpGetHandleProc --
 *
 *	Called from Tcl_GetChannelHandle to retrieve OS handles from inside a
 *	TCP socket based channel.
 *
 * Results:
 *	Returns TCL_OK with the fd in handlePtr, or TCL_ERROR if there is no
 *	handle for the specified direction.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
TcpGetHandleProc(
    ClientData instanceData,	/* The socket state. */
    int direction,		/* Not used. */
    ClientData *handlePtr)	/* Where to store the handle. */
{
    TcpState *statePtr = (TcpState *) instanceData;

    *handlePtr = (ClientData) INT2PTR(statePtr->fd);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateSocket --
 *
 *	This function opens a new socket in client or server mode and
 *	initializes the TcpState structure.
 *
 * Results:
 *	Returns a new TcpState, or NULL with an error in the interp's result,
 *	if interp is not NULL.
 *
 * Side effects:
 *	Opens a socket.
 *
 *----------------------------------------------------------------------
 */

static TcpState *
CreateSocket(
    Tcl_Interp *interp,		/* For error reporting; can be NULL. */
    int port,			/* Port number to open. */
    const char *host,		/* Name of host on which to open port. NULL
				 * implies INADDR_ANY */
    int server,			/* 1 if socket should be a server socket, else
				 * 0 for a client socket. */
    const char *myaddr,		/* Optional client-side address */
    int myport,			/* Optional client-side port */
    int async)			/* If nonzero and creating a client socket,
				 * attempt to do an async connect. Otherwise
				 * do a synchronous connect or bind. */
{
    int status = 0, sock = -1;
    struct sockaddr_in sockaddr;	/* socket address */
    struct sockaddr_in mysockaddr;	/* Socket address for client */
    TcpState *statePtr;
    const char *errorMsg = NULL;

    if (!CreateSocketAddress(&sockaddr, host, port, 0, &errorMsg)) {
	goto error;
    }
    if ((myaddr != NULL || myport != 0) &&
	    !CreateSocketAddress(&mysockaddr, myaddr, myport, 1, &errorMsg)) {
	goto error;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
	goto error;
    }

    /*
     * Set the close-on-exec flag so that the socket will not get inherited by
     * child processes.
     */

    fcntl(sock, F_SETFD, FD_CLOEXEC);

    /*
     * Set kernel space buffering
     */

    TclSockMinimumBuffers(sock, SOCKET_BUFSIZE);

    status = 0;
    if (server) {
	/*
	 * Set up to reuse server addresses automatically and bind to the
	 * specified port.
	 */

	int reuseaddr = 1;
	(void) setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
		      (char *) &reuseaddr, sizeof(reuseaddr));
	status = bind(sock, (struct sockaddr *) &sockaddr,
		sizeof(struct sockaddr));
	if (status != -1) {
	    status = listen(sock, SOMAXCONN);
	}
    } else {
	if (myaddr != NULL || myport != 0) {
	    int reuseaddr = 1;
	    (void) setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		    (char *) &reuseaddr, sizeof(reuseaddr));
	    status = bind(sock, (struct sockaddr *) &mysockaddr,
		    sizeof(struct sockaddr));
	    if (status < 0) {
		goto error;
	    }
	}

	/*
	 * Attempt to connect. The connect may fail at present with an
	 * EINPROGRESS but at a later time it will complete. The caller will
	 * set up a file handler on the socket if she is interested in being
	 * informed when the connect completes.
	 */

	if (async) {
	    status = TclUnixSetBlockingMode(sock, TCL_MODE_NONBLOCKING);
	    if (status < 0) {
		goto error;
	    }
	}

	status = connect(sock, (struct sockaddr *) &sockaddr,
		sizeof(sockaddr));
	if (status < 0) {
	    if (errno == EINPROGRESS) {
		status = 0;
	    } else {
		goto error;
	    }
	} 
	if (async) {
	    /*
	     * Restore blocking mode.
	     */
	    status = TclUnixSetBlockingMode(sock, TCL_MODE_BLOCKING);
	}
    }

    if (status < 0) {
error:
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "couldn't open socket: ",
		    Tcl_PosixError(interp), NULL);
	    if (errorMsg != NULL) {
		Tcl_AppendResult(interp, " (", errorMsg, ")", NULL);
	    }
	}
	if (sock != -1) {
	    close(sock);
	}
	return NULL;
    }

    /*
     * Allocate a new TcpState for this socket.
     */

    statePtr = (TcpState *) ckalloc((unsigned) sizeof(TcpState));
    statePtr->flags = async ? TCP_ASYNC_CONNECT : 0;
    statePtr->fd = sock;

    return statePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * CreateSocketAddress --
 *
 *	This function initializes a sockaddr structure for a host and port.
 *
 * Results:
 *	1 if the host was valid, 0 if the host could not be converted to an IP
 *	address.
 *
 * Side effects:
 *	Fills in the *sockaddrPtr structure.
 *
 *----------------------------------------------------------------------
 */

static int
CreateSocketAddress(
    struct sockaddr_in *sockaddrPtr,	/* Socket address */
    const char *host,			/* Host. NULL implies INADDR_ANY */
    int port,				/* Port number */
    int willBind,			/* Is this an address to bind() to or
					 * to connect() to? */
    const char **errorMsgPtr)		/* Place to store the error message
					 * detail, if available. */
{
#ifdef HAVE_GETADDRINFO
    struct addrinfo hints, *resPtr = NULL;
    char *native;
    Tcl_DString ds;
    int result;

    if (host == NULL) {
	sockaddrPtr->sin_family = AF_INET;
	sockaddrPtr->sin_addr.s_addr = INADDR_ANY;
    addPort:
	sockaddrPtr->sin_port = htons((unsigned short) (port & 0xFFFF));
	return 1;
    }

    (void) memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (willBind) {
	hints.ai_flags |= AI_PASSIVE;
    }

    /*
     * Note that getaddrinfo() *is* thread-safe. If a platform doesn't get
     * that right, it shouldn't use this part of the code.
     */

    native = Tcl_UtfToExternalDString(NULL, host, -1, &ds);
    result = getaddrinfo(native, NULL, &hints, &resPtr);
    Tcl_DStringFree(&ds);
    if (result == 0) {
	memcpy(sockaddrPtr, resPtr->ai_addr, sizeof(struct sockaddr_in));
	freeaddrinfo(resPtr);
	goto addPort;
    }

    /*
     * Ought to use gai_strerror() here...
     */

    switch (result) {
    case EAI_NONAME:
    case EAI_SERVICE:
#if defined(EAI_ADDRFAMILY) && EAI_ADDRFAMILY != EAI_NONAME
    case EAI_ADDRFAMILY:
#endif
#if defined(EAI_NODATA) && EAI_NODATA != EAI_NONAME
    case EAI_NODATA:
#endif
	*errorMsgPtr = gai_strerror(result);
	errno = EHOSTUNREACH;
	return 0;
    case EAI_SYSTEM:
	return 0;
    default:
	*errorMsgPtr = gai_strerror(result);
	errno = ENXIO;
	return 0;
    }
#else /* !HAVE_GETADDRINFO */
    struct in_addr addr;		/* For 64/32 bit madness */

    (void) memset(sockaddrPtr, '\0', sizeof(struct sockaddr_in));
    sockaddrPtr->sin_family = AF_INET;
    sockaddrPtr->sin_port = htons((unsigned short) (port & 0xFFFF));
    if (host == NULL) {
	addr.s_addr = INADDR_ANY;
    } else {
	struct hostent *hostent;	/* Host database entry */
	Tcl_DString ds;
	const char *native;

	if (host == NULL) {
	    native = NULL;
	} else {
	    native = Tcl_UtfToExternalDString(NULL, host, -1, &ds);
	}
	addr.s_addr = inet_addr(native);		/* INTL: Native. */

	/*
	 * This is 0xFFFFFFFF to ensure that it compares as a 32bit -1 on
	 * either 32 or 64 bits systems.
	 */

	if (addr.s_addr == 0xFFFFFFFF) {
	    hostent = TclpGetHostByName(native);	/* INTL: Native. */
	    if (hostent != NULL) {
		memcpy(&addr, hostent->h_addr_list[0],
			(size_t) hostent->h_length);
	    } else {
#ifdef	EHOSTUNREACH
		errno = EHOSTUNREACH;
#else /* !EHOSTUNREACH */
#ifdef ENXIO
		errno = ENXIO;
#endif /* ENXIO */
#endif /* EHOSTUNREACH */
		if (native != NULL) {
		    Tcl_DStringFree(&ds);
		}
		return 0;	/* Error. */
	    }
	}
	if (native != NULL) {
	    Tcl_DStringFree(&ds);
	}
    }

    /*
     * NOTE: On 64 bit machines the assignment below is rumored to not do the
     * right thing. Please report errors related to this if you observe
     * incorrect behavior on 64 bit machines such as DEC Alphas. Should we
     * modify this code to do an explicit memcpy?
     */

    sockaddrPtr->sin_addr.s_addr = addr.s_addr;
    return 1;			/* Success. */
#endif /* HAVE_GETADDRINFO */
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_OpenTcpClient --
 *
 *	Opens a TCP client socket and creates a channel around it.
 *
 * Results:
 *	The channel or NULL if failed. An error message is returned in the
 *	interpreter on failure.
 *
 * Side effects:
 *	Opens a client socket and creates a new channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_OpenTcpClient(
    Tcl_Interp *interp,		/* For error reporting; can be NULL. */
    int port,			/* Port number to open. */
    const char *host,		/* Host on which to open port. */
    const char *myaddr,		/* Client-side address */
    int myport,			/* Client-side port */
    int async)			/* If nonzero, attempt to do an asynchronous
				 * connect. Otherwise we do a blocking
				 * connect. */
{
    TcpState *statePtr;
    char channelName[16 + TCL_INTEGER_SPACE];

    /*
     * Create a new client socket and wrap it in a channel.
     */

    statePtr = CreateSocket(interp, port, host, 0, myaddr, myport, async);
    if (statePtr == NULL) {
	return NULL;
    }

    statePtr->acceptProc = NULL;
    statePtr->acceptProcData = NULL;

    sprintf(channelName, "sock%d", statePtr->fd);

    statePtr->channel = Tcl_CreateChannel(&tcpChannelType, channelName,
	    (ClientData) statePtr, (TCL_READABLE | TCL_WRITABLE));
    if (Tcl_SetChannelOption(interp, statePtr->channel, "-translation",
	    "auto crlf") == TCL_ERROR) {
	Tcl_Close(NULL, statePtr->channel);
	return NULL;
    }
    return statePtr->channel;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MakeTcpClientChannel --
 *
 *	Creates a Tcl_Channel from an existing client TCP socket.
 *
 * Results:
 *	The Tcl_Channel wrapped around the preexisting TCP socket.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_MakeTcpClientChannel(
    ClientData sock)		/* The socket to wrap up into a channel. */
{
    return TclpMakeTcpClientChannelMode(sock, (TCL_READABLE | TCL_WRITABLE));
}

/*
 *----------------------------------------------------------------------
 *
 * TclpMakeTcpClientChannelMode --
 *
 *	Creates a Tcl_Channel from an existing client TCP socket
 *	with given mode.
 *
 * Results:
 *	The Tcl_Channel wrapped around the preexisting TCP socket.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
TclpMakeTcpClientChannelMode(
    ClientData sock,		/* The socket to wrap up into a channel. */
    int mode)			/* ORed combination of TCL_READABLE and
				 * TCL_WRITABLE to indicate file mode. */
{
    TcpState *statePtr;
    char channelName[16 + TCL_INTEGER_SPACE];

    statePtr = (TcpState *) ckalloc((unsigned) sizeof(TcpState));
    statePtr->fd = PTR2INT(sock);
    statePtr->flags = 0;
    statePtr->acceptProc = NULL;
    statePtr->acceptProcData = NULL;

    sprintf(channelName, "sock%d", statePtr->fd);

    statePtr->channel = Tcl_CreateChannel(&tcpChannelType, channelName,
	    (ClientData) statePtr, mode);
    if (Tcl_SetChannelOption(NULL, statePtr->channel, "-translation",
	    "auto crlf") == TCL_ERROR) {
	Tcl_Close(NULL, statePtr->channel);
	return NULL;
    }
    return statePtr->channel;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_OpenTcpServer --
 *
 *	Opens a TCP server socket and creates a channel around it.
 *
 * Results:
 *	The channel or NULL if failed. If an error occurred, an error message
 *	is left in the interp's result if interp is not NULL.
 *
 * Side effects:
 *	Opens a server socket and creates a new channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_OpenTcpServer(
    Tcl_Interp *interp,		/* For error reporting - may be NULL. */
    int port,			/* Port number to open. */
    const char *myHost,		/* Name of local host. */
    Tcl_TcpAcceptProc *acceptProc,
				/* Callback for accepting connections from new
				 * clients. */
    ClientData acceptProcData)	/* Data for the callback. */
{
    TcpState *statePtr;
    char channelName[16 + TCL_INTEGER_SPACE];

    /*
     * Create a new client socket and wrap it in a channel.
     */

    statePtr = CreateSocket(interp, port, myHost, 1, NULL, 0, 0);
    if (statePtr == NULL) {
	return NULL;
    }

    statePtr->acceptProc = acceptProc;
    statePtr->acceptProcData = acceptProcData;

    /*
     * Set up the callback mechanism for accepting connections from new
     * clients.
     */

    Tcl_CreateFileHandler(statePtr->fd, TCL_READABLE, TcpAccept,
	    (ClientData) statePtr);
    sprintf(channelName, "sock%d", statePtr->fd);
    statePtr->channel = Tcl_CreateChannel(&tcpChannelType, channelName,
	    (ClientData) statePtr, 0);
    return statePtr->channel;
}

/*
 *----------------------------------------------------------------------
 *
 * TcpAccept --
 *	Accept a TCP socket connection.	 This is called by the event loop.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Creates a new connection socket. Calls the registered callback for the
 *	connection acceptance mechanism.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
TcpAccept(
    ClientData data,		/* Callback token. */
    int mask)			/* Not used. */
{
    TcpState *sockState;	/* Client data of server socket. */
    int newsock;		/* The new client socket */
    TcpState *newSockState;	/* State for new socket. */
    struct sockaddr_in addr;	/* The remote address */
    socklen_t len;		/* For accept interface */
    char channelName[16 + TCL_INTEGER_SPACE];

    sockState = (TcpState *) data;

    len = sizeof(struct sockaddr_in);
    newsock = accept(sockState->fd, (struct sockaddr *) &addr, &len);
    if (newsock < 0) {
	return;
    }

    /*
     * Set close-on-exec flag to prevent the newly accepted socket from being
     * inherited by child processes.
     */

    (void) fcntl(newsock, F_SETFD, FD_CLOEXEC);

    newSockState = (TcpState *) ckalloc((unsigned) sizeof(TcpState));

    newSockState->flags = 0;
    newSockState->fd = newsock;
    newSockState->acceptProc = NULL;
    newSockState->acceptProcData = NULL;

    sprintf(channelName, "sock%d", newsock);
    newSockState->channel = Tcl_CreateChannel(&tcpChannelType, channelName,
	    (ClientData) newSockState, (TCL_READABLE | TCL_WRITABLE));

    Tcl_SetChannelOption(NULL, newSockState->channel, "-translation",
	    "auto crlf");

    if (sockState->acceptProc != NULL) {
	sockState->acceptProc(sockState->acceptProcData,
		newSockState->channel, inet_ntoa(addr.sin_addr),
		ntohs(addr.sin_port));
    }
}

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
