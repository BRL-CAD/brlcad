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

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <tcl.h>
#include "bio.h"
#include "bnetwork.h"
#include "dm.h"
#include "tclcad.h"

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
    struct pkg_conn *pc = pkg_borrow_socket(fd, switchp, comm_error);
    if (pc == PKC_ERROR) {
	comm_error("fbs_makeconn: pkg_borrow_socket failure\n");
    }
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
    void *cdata = NULL;
    struct pkg_conn *pcp = NULL;

#ifdef USE_TCL_CHAN
    /* Tcl owns the channel.  The package connection still uses an int. */
    ClientData handle;
    if (Tcl_GetChannelHandle(chan, TCL_READABLE, &handle) != TCL_OK ||
	    (uintptr_t)handle > INT_MAX) {
	Tcl_Close((Tcl_Interp *)fbsp->fbs_interp, chan);
	return;
    }
    cdata = (void *)chan;
    pcp = fbs_makeconn((int)(uintptr_t)handle, pswitch);
#else
    pcp = pkg_accept(fbslp->fbsl_listener, pswitch, comm_error, 0);
#endif

    fbs_new_client(fbsp, pcp, cdata);
}

/* Check if we're already listening. */
C_DECL int
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

C_DECL int
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
    fbsp->fbs_listener.fbsl_listener = pkg_listen(portname, NULL, 0, comm_error);
    if (fbsp->fbs_listener.fbsl_listener) {
	fbsp->fbs_listener.fbsl_fd = pkg_get_listener_fd(fbsp->fbs_listener.fbsl_listener);
	return 1;
    }
#endif
    return 0;
}

C_DECL void
tclcad_open_server_handler(struct fbserv_obj *fbsp)
{
#ifdef USE_TCL_CHAN
    /* Tcl_OpenTcpServer already registered the accept callback. */
    (void)fbsp;
#else
    Tcl_CreateFileHandler(fbsp->fbs_listener.fbsl_fd, TCL_READABLE, (Tcl_FileProc *)new_client_handler, (ClientData)&fbsp->fbs_listener);
#endif
}

C_DECL void
tclcad_close_server_handler(struct fbserv_obj *fbsp)
{
#ifdef USE_TCL_CHAN
    if (fbsp->fbs_listener.fbsl_chan != NULL) {
	Tcl_Close((Tcl_Interp *)fbsp->fbs_interp, (Tcl_Channel)fbsp->fbs_listener.fbsl_chan);
	fbsp->fbs_listener.fbsl_chan = NULL;
    }
#else
    Tcl_DeleteFileHandler(fbsp->fbs_listener.fbsl_fd);
#endif
}

C_DECL void
#ifdef USE_TCL_CHAN
tclcad_open_client_handler(struct fbserv_obj *fbsp, int i, void *data)
#else
tclcad_open_client_handler(struct fbserv_obj *fbsp, int i, void *UNUSED(data))
#endif
{
#ifdef USE_TCL_CHAN
    Tcl_Channel channel = (Tcl_Channel)data;
    fbsp->fbs_clients[i].fbsc_chan = channel;
    Tcl_CreateChannelHandler(channel, TCL_READABLE,
	    fbs_existing_client_handler, (ClientData)&fbsp->fbs_clients[i]);
#else
    Tcl_CreateFileHandler(fbsp->fbs_clients[i].fbsc_fd, TCL_READABLE,
	    fbs_existing_client_handler, (ClientData)&fbsp->fbs_clients[i]);
#endif
}

C_DECL void
tclcad_close_client_handler(struct fbserv_obj *fbsp, int sub)
{
#ifdef USE_TCL_CHAN
    Tcl_Channel channel = (Tcl_Channel)fbsp->fbs_clients[sub].fbsc_chan;
    Tcl_DeleteChannelHandler(channel,
	    fbs_existing_client_handler, (ClientData)&fbsp->fbs_clients[sub]);

    Tcl_Close((Tcl_Interp *)fbsp->fbs_interp, channel);
    fbsp->fbs_clients[sub].fbsc_chan = NULL;
#else
    Tcl_DeleteFileHandler(fbsp->fbs_clients[sub].fbsc_fd);
#endif
}


#ifdef USE_TCL_CHAN
#  define TCL_IPC_POLL_MS 10

/**
 * Poll callback for IPC clients on Windows (USE_TCL_CHAN path).
 *
 * Background: on Windows, Tcl_CreateFileHandler is a no-op, and
 * Tcl_MakeFileChannel for pipe handles starts a background reader thread
 * that CONSUMES data from the pipe into Tcl's internal buffer -- which
 * prevents pkg_process() from reading the data via the raw fd.
 *
 * Instead, we install a recurring Tcl timer that uses libpkg's
 * transport-aware, non-destructive readiness poll.  When data is found the
 * same fbs_existing_client_handler() that POSIX file-handler callbacks fire
 * is called directly.  10 ms polling corresponds to the 100 fps maximum
 * framebuffer refresh rate; it adds negligible overhead compared to the
 * actual rendering time.
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

    {
	int ready = pkg_conn_poll(fbscp->fbsc_pkg);

	/* Reschedule BEFORE calling the handler: the handler may call
	 * drop_client() → tclcad_close_ipc_client_win() which cancels
	 * the new token, so we must store it first.                       */
	{
	    Tcl_TimerToken tok = Tcl_CreateTimerHandler(TCL_IPC_POLL_MS,
		tcl_ipc_poll_win, cd);
	    fbscp->fbsc_chan = (void *)tok;
	}

	if (ready)
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
	Tcl_CreateTimerHandler(TCL_IPC_POLL_MS, tcl_ipc_poll_win,
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


TCLCAD_EXPORT void
tclcad_configure_ipc_handlers(struct fbserv_obj *fbsp)
{
    if (!fbsp)
	return;

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
}


TCLCAD_EXPORT int
tclcad_listen_ipc(struct fbserv_obj *fbsp, Tcl_Interp *interp)
{
    tclcad_configure_ipc_handlers(fbsp);

    if (fbs_open_ipc(fbsp) != BRLCAD_OK) {
	bu_log("tclcad_listen_ipc: fbs_open_ipc failed\n");
	return BRLCAD_ERROR;
    }

    /* Expose the child-end address for Tcl code that launches a subprocess. */
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
