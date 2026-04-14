/*                        F B S E R V . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup libstruct fb */
/** @{ */
/**
 *
 *  These are the Tcl specific callbacks used for I/O between client
 *  and server.  So far, experiments to simply use file handlers for
 *  Windows as well as other platforms haven't been successful, which
 *  is why we've had to accept the increased complexity of this code.
 */
/** @} */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <tcl.h>
#include "bio.h"
#include "bnetwork.h"
#include "dm.h"
#include "tclcad.h"

/* We need to use different Tcl I/O mechanisms on different
 * platforms.  Make a define for that. */
#if defined(_WIN32) && !defined(__CYGWIN__)
#define USE_TCL_CHAN
#endif

/*
 * Communication error.  An error occurred on the PKG link.
 */
static void
comm_error(const char *str)
{
    bu_log("%s", str);
}

#ifdef USE_TCL_CHAN
/* Per SVN commit r29234, this function is needed because Windows uses
 * Tcl_OpenTcpServer rather than pkg_permserver to set up communication
 * on Windows (Tcl_OpenTcpServer's output is saved on fbsl_chan rather
 * than fbsl_fd.)  This function set up a pkg_conn structure and
 * assigns the fbsl_fd number for that scenario. */
static struct pkg_conn *
fbs_makeconn(int fd, const struct pkg_switch *switchp)
{
    register struct pkg_conn *pc;
#ifdef HAVE_WINSOCK_H
    WSADATA wsaData;
    WORD wVersionRequested; /* initialize Windows socket networking,
			     * increment reference count.
			     */
#endif

    if ((pc = (struct pkg_conn *)malloc(sizeof(struct pkg_conn))) == PKC_NULL) {
	comm_error("fbs_makeconn: malloc failure\n");
	return PKC_ERROR;
    }

#ifdef HAVE_WINSOCK_H
    wVersionRequested = MAKEWORD(1, 1);
    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
	comm_error("fbs_makeconn:  could not find a usable WinSock DLL\n");
	return PKC_ERROR;
    }
#endif

    memset((char *)pc, 0, sizeof(struct pkg_conn));
    pc->pkc_magic = PKG_MAGIC;
    pc->pkc_fd = fd;
    pc->pkc_switch = switchp;
    pc->pkc_errlog = 0;
    pc->pkc_left = -1;
    pc->pkc_buf = (char *)0;
    pc->pkc_curpos = (char *)0;
    pc->pkc_strpos = 0;
    pc->pkc_incur = pc->pkc_inend = 0;

    return pc;
}
#endif

/*
 * Accept any new client connections.
 */
static void
#ifdef USE_TCL_CHAN
new_client_handler(ClientData clientData, Tcl_Channel chan, char *UNUSED(host), int UNUSED(port))
#else
new_client_handler(ClientData clientData, int UNUSED(port))
#endif
{
    struct fbserv_listener *fbslp = (struct fbserv_listener *)clientData;
    struct fbserv_obj *fbsp = fbslp->fbsl_fbsp;
    struct pkg_switch *pswitch = fbs_pkg_switch();
    int fd = fbslp->fbsl_fd;
    void *cdata = NULL;
    struct pkg_conn *pcp = NULL;

#ifdef USE_TCL_CHAN
    uintptr_t pfd = (uintptr_t)fbslp->fbsl_fd;
    if (Tcl_GetChannelHandle(chan, TCL_READABLE, (ClientData *)&pfd) != TCL_OK)
	return;
    cdata = (void *)chan;
    // TODO - we need the uintptr_t above, or we introduce stack corruption on
    // Windows.  Less certain about why we're casting back to int and using
    // it for fbs_makeconn...  presumably it's not really a valid pkc_fd, so
    // does it make sense to assign it there?
    pcp = fbs_makeconn((int)pfd, pswitch);
#else
    pcp = pkg_getclient(fd, pswitch, comm_error, 0);
#endif

    fbs_new_client(fbsp, pcp, cdata);
}

/* Check if we're already listening. */
int
tclcad_is_listening(struct fbserv_obj *fbsp)
{
#ifdef USE_TCL_CHAN
    if (fbsp->fbs_listener.fbsl_chan != NULL) {
#else
    if (fbsp->fbs_listener.fbsl_fd >= 0) {
#endif
	return 1;
    }
    return 0;
}

int
tclcad_listen_on_port(struct fbserv_obj *fbsp, int available_port)
{
    char hostname[32] = {0};
    /* XXX hardwired for now */
    sprintf(hostname, "localhost");

#ifdef USE_TCL_CHAN
    fbsp->fbs_listener.fbsl_chan = Tcl_OpenTcpServer((Tcl_Interp *)fbsp->fbs_interp, available_port, hostname, new_client_handler, (ClientData)&fbsp->fbs_listener);
    if (fbsp->fbs_listener.fbsl_chan == NULL) {
	/* This clobbers the result string which probably has junk
	 * related to the failed open.
	 */
	Tcl_DString ds;
	Tcl_DStringInit(&ds);
	Tcl_DStringResult((Tcl_Interp *)fbsp->fbs_interp, &ds);
	return 0;
    } else {
	return 1;
    }
#else
    char portname[32] = {0};
    sprintf(portname, "%d", available_port);
    fbsp->fbs_listener.fbsl_fd = pkg_permserver(portname, 0, 0, comm_error);
    if (fbsp->fbs_listener.fbsl_fd >= 0)
	return 1;
#endif
    return 0;
}

void
tclcad_open_server_handler(struct fbserv_obj *fbsp)
{
#ifdef USE_TCL_CHAN
    Tcl_GetChannelHandle(fbsp->fbs_listener.fbsl_chan, TCL_READABLE, (ClientData *)&fbsp->fbs_listener.fbsl_fd);
#else
    Tcl_CreateFileHandler(fbsp->fbs_listener.fbsl_fd, TCL_READABLE, (Tcl_FileProc *)new_client_handler, (ClientData)&fbsp->fbs_listener);
#endif
}

void
tclcad_close_server_handler(struct fbserv_obj *fbsp)
{
#ifdef USE_TCL_CHAN
    if (fbsp->fbs_listener.fbsl_chan != NULL) {
	Tcl_ChannelProc *callback = (Tcl_ChannelProc *)new_client_handler;
	Tcl_DeleteChannelHandler(fbsp->fbs_listener.fbsl_chan, callback, (ClientData)fbsp->fbs_listener.fbsl_fd);
	Tcl_Close((Tcl_Interp *)fbsp->fbs_interp, fbsp->fbs_listener.fbsl_chan);
	fbsp->fbs_listener.fbsl_chan = NULL;
    }
#else
    Tcl_DeleteFileHandler(fbsp->fbs_listener.fbsl_fd);
#endif
}

void
#ifdef USE_TCL_CHAN
tclcad_open_client_handler(struct fbserv_obj *fbsp, int i, void *data)
#else
tclcad_open_client_handler(struct fbserv_obj *fbsp, int i, void *UNUSED(data))
#endif
{
#ifdef USE_TCL_CHAN
    fbsp->fbs_clients[i].fbsc_chan = (Tcl_Channel)data;
    fbsp->fbs_clients[i].fbsc_handler = fbs_existing_client_handler;
    Tcl_CreateChannelHandler(fbsp->fbs_clients[i].fbsc_chan, TCL_READABLE,
	    fbsp->fbs_clients[i].fbsc_handler, (ClientData)&fbsp->fbs_clients[i]);
#else
    Tcl_CreateFileHandler(fbsp->fbs_clients[i].fbsc_fd, TCL_READABLE,
	    fbs_existing_client_handler, (ClientData)&fbsp->fbs_clients[i]);
#endif
}

void
tclcad_close_client_handler(struct fbserv_obj *fbsp, int sub)
{
#ifdef USE_TCL_CHAN
    Tcl_DeleteChannelHandler(fbsp->fbs_clients[sub].fbsc_chan, fbsp->fbs_clients[sub].fbsc_handler, (ClientData)fbsp->fbs_clients[sub].fbsc_fd);

    Tcl_Close((Tcl_Interp *)fbsp->fbs_interp, fbsp->fbs_clients[sub].fbsc_chan);
    fbsp->fbs_clients[sub].fbsc_chan = NULL;
#else
    Tcl_DeleteFileHandler(fbsp->fbs_clients[sub].fbsc_fd);
#endif
}


#ifdef USE_TCL_CHAN
/**
 * Poll callback for IPC clients on Windows (USE_TCL_CHAN path).
 *
 * Background: on Windows, Tcl_CreateFileHandler is a no-op, and
 * Tcl_MakeFileChannel for pipe handles starts a background reader thread
 * that CONSUMES data from the pipe into Tcl's internal buffer -- which
 * prevents pkg_process() from reading the data via the raw fd.
 *
 * Instead, we install a recurring Tcl timer that uses PeekNamedPipe()
 * (non-destructive) to check whether data is available.  When data is
 * found the same fbs_existing_client_handler() that POSIX file-handler
 * callbacks fire is called directly.  10 ms polling corresponds to the
 * 100 fps maximum framebuffer refresh rate; it adds negligible overhead
 * compared to the actual rendering time.
 */
static void
tcl_ipc_poll_win(ClientData cd)
{
    struct fbserv_client *fbscp = (struct fbserv_client *)cd;

    /* Termination guard: pkg was already closed, stop the timer. */
    if (!fbscp->fbsc_pkg || fbscp->fbsc_pkg == PKC_NULL) {
	fbscp->fbsc_chan = NULL;
	return;
    }

    /* Peek at the pipe without consuming data. */
    {
	int fd = fbscp->fbsc_fd;
	int has_data = 0;
	HANDLE h = (fd >= 0) ? (HANDLE)_get_osfhandle(fd) : INVALID_HANDLE_VALUE;
	if (h != INVALID_HANDLE_VALUE) {
	    DWORD avail = 0;
	    if (PeekNamedPipe(h, NULL, 0, NULL, &avail, NULL) && avail > 0)
		has_data = 1;
	}

	/* Reschedule BEFORE calling the handler: the handler may call
	 * drop_client() → tclcad_close_ipc_client_win() which cancels
	 * the new token, so we must store it first.                       */
	{
	    Tcl_TimerToken tok = Tcl_CreateTimerHandler(10, tcl_ipc_poll_win, cd);
	    fbscp->fbsc_chan = (void *)tok;
	}

	if (has_data)
	    fbs_existing_client_handler(cd, TCL_READABLE);
    }
}

/**
 * Open handler for IPC clients on Windows (USE_TCL_CHAN path).
 * Installs the recurring poll timer instead of a file/channel handler.
 */
static void
tclcad_open_ipc_client_win(struct fbserv_obj *fbsp, int i, void *UNUSED(data))
{
    Tcl_TimerToken tok =
	Tcl_CreateTimerHandler(10, tcl_ipc_poll_win,
			       (ClientData)&fbsp->fbs_clients[i]);
    fbsp->fbs_clients[i].fbsc_chan    = (void *)tok;
    fbsp->fbs_clients[i].fbsc_handler = NULL;
}

/**
 * Close handler for IPC clients on Windows (USE_TCL_CHAN path).
 * Cancels the poll timer installed by tclcad_open_ipc_client_win().
 */
static void
tclcad_close_ipc_client_win(struct fbserv_obj *fbsp, int sub)
{
    if (fbsp->fbs_clients[sub].fbsc_chan) {
	Tcl_DeleteTimerHandler((Tcl_TimerToken)fbsp->fbs_clients[sub].fbsc_chan);
	fbsp->fbs_clients[sub].fbsc_chan    = NULL;
	fbsp->fbs_clients[sub].fbsc_handler = NULL;
    }
}
#endif /* USE_TCL_CHAN */


/**
 * Phase 4: IPC listen path for libtclcad.
 *
 * Sets up an IPC-based (pipe/socketpair) framebuffer server on @p fbsp
 * without binding a TCP port.
 *
 * On POSIX, tclcad_open_client_handler / tclcad_close_client_handler use
 * Tcl_CreateFileHandler / Tcl_DeleteFileHandler which work for any fd.
 *
 * On Windows (USE_TCL_CHAN), Tcl_CreateFileHandler is a no-op.  Wrapping
 * a pipe HANDLE in a Tcl channel via Tcl_MakeFileChannel is also not viable
 * because Tcl's Windows pipe channel driver spawns a background reader
 * thread that CONSUMES data from the pipe into its own buffer, preventing
 * pkg_process() from reading via the raw fd.  Instead we install a
 * recurring Tcl timer (tclcad_open_ipc_client_win / tcl_ipc_poll_win) that
 * uses PeekNamedPipe() to check availability without consuming data, then
 * calls fbs_existing_client_handler() when data is ready.
 *
 * If the open succeeds and @p interp is non-NULL, the BU_IPC_ADDR child-end
 * address string is stored in the Tcl variable fbserv(ipc_addr) so that
 * Tcl scripts (rtwizard, MGED's rt.tcl) can pass it to subprocesses.
 *
 * @return BRLCAD_OK on success, BRLCAD_ERROR on failure.
 */
TCLCAD_EXPORT int
tclcad_listen_ipc(struct fbserv_obj *fbsp, Tcl_Interp *interp)
{
#ifdef USE_TCL_CHAN
    /* Windows: use a timer-based poll to avoid Tcl's pipe reader thread
     * consuming data before pkg_process() can read it.                   */
    fbsp->fbs_open_ipc_client_handler  = tclcad_open_ipc_client_win;
    fbsp->fbs_close_ipc_client_handler = tclcad_close_ipc_client_win;
#else
    /* On POSIX, tclcad_open_client_handler and tclcad_close_client_handler
     * only use the fd (via Tcl_CreateFileHandler / Tcl_DeleteFileHandler)
     * so they work identically for pipe and socketpair transports.         */
    fbsp->fbs_open_ipc_client_handler  = tclcad_open_client_handler;
    fbsp->fbs_close_ipc_client_handler = tclcad_close_client_handler;
#endif

    if (fbs_open_ipc(fbsp) != BRLCAD_OK) {
	bu_log("tclcad_listen_ipc: fbs_open_ipc failed\n");
	return BRLCAD_ERROR;
    }

    /* Phase 6 support: expose the child-end address as a Tcl variable so
     * rtwizard / MGED Tcl scripts can pass it to spawned subprocesses.     */
    if (interp) {
	const char *addr_env = fbs_ipc_child_addr_env(fbsp);
	if (addr_env) {
	    const char *eq = strchr(addr_env, '=');
	    const char *addr_val = eq ? eq + 1 : addr_env;
	    Tcl_SetVar2(interp, "fbserv", "ipc_addr", addr_val, TCL_GLOBAL_ONLY);
	}
    }

    return BRLCAD_OK;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
