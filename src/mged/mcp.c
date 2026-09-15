/*                          M C P . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file mged/mcp.c
 *
 * The 'mcp_listen' command: expose this MGED's geometry database to
 * external extensions (MCP servers, scripts) over a local connection,
 * using the libmcpcad command pipeline.
 *
 * Two transports carry the same protocol:
 *
 *   TCP   a loopback port, via Tcl_OpenTcpServer - the same mechanism
 *         fbserv uses (see fbserv.c).
 *   IPC   a Unix-domain socket, via libpkg's pkg_listen(), for sites
 *         where opening a TCP port is restricted or trips security
 *         tooling.  libpkg also offers FIFOs, but connecting to one is a
 *         handshake every client would have to reimplement against an
 *         address format its header documents as opaque; a socket is an
 *         ordinary connect() in any language.  The same pkg_listen()
 *         call takes an "npipe:" address on Windows.
 *
 * Which transport is in use is confined to the mcp_transport vtable
 * below; nothing else in this file, and nothing in libmcpcad, branches
 * on it.
 *
 * Both the accept callback and the per-client read callback fire on the
 * main thread, so commands run synchronously alongside human GUI input
 * with no locking, exactly as the GSoC design intends.  When MGED is
 * superseded by a Qt/MOOSE front end, only this file changes: the same
 * libmcpcad session is driven from a QTcpServer/QProcess instead.
 */

#include "common.h"

#include <errno.h>
#include <string.h>

#include "tcl.h"

#include "bio.h"
#include "bnetwork.h"
#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "ged.h"
#include "mcpcad.h"
#include "pkg.h"

#include "./mged.h"

#define MCP_DEFAULT_PORT 5959
#define MCP_PORT_TRIES   64
#define MCP_READ_CHUNK   4096
#define MCP_IPC_BACKLOG  8


/**********************************************************************
 * Transport: how one client's bytes move.
 *
 * The only place that knows TCP from IPC.  A client holds an opaque
 * handle plus the vtable that understands it.
 **********************************************************************/

struct mcp_client;

struct mcp_transport {
    /* >0 bytes read, 0 on clean EOF, <0 on an unrecoverable error */
    int (*recv)(struct mcp_client *c, char *buf, size_t len);
    void (*send)(struct mcp_client *c, const char *data, size_t len);
    /* stop watching the descriptor and release the handle */
    void (*detach)(struct mcp_client *c);
};

struct mcp_client {
    const struct mcp_transport *tp;
    void *handle;                 /* Tcl_Channel, or struct pkg_conn * */
    struct mcpcad_session *sess;
};


/* --- TCP: a Tcl channel from Tcl_OpenTcpServer ------------------- */

static int
mcp_tcp_recv(struct mcp_client *c, char *buf, size_t len)
{
    Tcl_Channel chan = (Tcl_Channel)c->handle;
    int n = Tcl_Read(chan, buf, (int)len);

    if (n > 0)
	return n;
    return Tcl_Eof(chan) ? 0 : -1;
}

static void
mcp_tcp_send(struct mcp_client *c, const char *data, size_t len)
{
    Tcl_Channel chan = (Tcl_Channel)c->handle;

    Tcl_Write(chan, data, (int)len);
    Tcl_Flush(chan);
}

static void
mcp_tcp_detach(struct mcp_client *c)
{
    Tcl_Channel chan = (Tcl_Channel)c->handle;

    Tcl_DeleteChannelHandler(chan, NULL, (ClientData)c);
    Tcl_Close(NULL, chan);
}

static const struct mcp_transport mcp_tcp = {
    mcp_tcp_recv, mcp_tcp_send, mcp_tcp_detach
};


/* --- IPC: a descriptor from libpkg ------------------------------- */

/* Driving a raw descriptor from the event loop needs Tcl_CreateFileHandler,
 * which tclDecls.h declares for UNIX and macOS but not for Windows.  MGED
 * already keys that distinction off USE_TCL_CHAN (see mged.h -- the same key
 * fbserv.c uses to choose its I/O mechanism), so the transport is built only
 * where descriptors are usable and the command reports its absence plainly
 * elsewhere.  Local IPC is demonstrated on Linux; Windows would want a named
 * pipe implementation, tested separately.
 */
#ifndef USE_TCL_CHAN

static int
mcp_ipc_recv(struct mcp_client *c, char *buf, size_t len)
{
    ssize_t n = read(pkg_get_read_fd((struct pkg_conn *)c->handle), buf, len);

    if (n > 0)
	return (int)n;
    if (n == 0)
	return 0;                     /* clean EOF */
    /* The descriptor is non-blocking, so "no data yet" is the common case
     * and must not be mistaken for a dead connection. */
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
	return -1;
    return 0;                         /* a real error; treat as hang-up */
}

static void
mcp_ipc_send(struct mcp_client *c, const char *data, size_t len)
{
    int fd = pkg_get_write_fd((struct pkg_conn *)c->handle);
    size_t off = 0;

    while (off < len) {
	ssize_t w = write(fd, data + off, len - off);
	if (w > 0) {
	    off += (size_t)w;
	    continue;
	}
	/* Non-blocking: a full socket buffer is not an error.  Replies can
	 * exceed it (a spec dump, a verification report), so wait for room
	 * rather than truncating the frame. */
	if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
	    fd_set wset;
	    if (fd >= FD_SETSIZE)
		return;                 /* cannot select on it; drop the reply */
	    FD_ZERO(&wset);
	    FD_SET(fd, &wset);
	    if (select(fd + 1, NULL, &wset, NULL, NULL) <= 0)
		return;
	    continue;
	}
	if (w < 0 && errno == EINTR)
	    continue;
	return;   /* client hung up; the read side tears it down */
    }
}

static void
mcp_ipc_detach(struct mcp_client *c)
{
    struct pkg_conn *conn = (struct pkg_conn *)c->handle;

    Tcl_DeleteFileHandler(pkg_get_read_fd(conn));
    pkg_close(conn);              /* owns and closes the descriptor */
}

static const struct mcp_transport mcp_ipc = {
    mcp_ipc_recv, mcp_ipc_send, mcp_ipc_detach
};

#endif /* USE_TCL_CHAN */


/**********************************************************************
 * Client: one connection's session, transport-agnostic.
 **********************************************************************/

/* libmcpcad write callback; ctx is the owning client. */
static void
mcp_client_write(const char *data, size_t len, void *ctx)
{
    struct mcp_client *c = (struct mcp_client *)ctx;

    if (c && c->tp)
	c->tp->send(c, data, len);
}

static void
mcp_client_close(struct mcp_client *c)
{
    if (!c)
	return;
    c->tp->detach(c);
    mcpcad_session_destroy(c->sess);
    BU_PUT(c, struct mcp_client);
}

/* Fires whenever a connected client has bytes (or has hung up).  Reads
 * what is available, feeds it to the session, and tears the connection
 * down on EOF or an unrecoverable protocol fault. */
static void
mcp_client_readable(ClientData clientData, int UNUSED(mask))
{
    struct mged_state *s = MGED_STATE;
    struct mcp_client *c = (struct mcp_client *)clientData;
    char buf[MCP_READ_CHUNK];
    /* A file handler is level-triggered, and the redraw below does real
     * display-manager work that can service X/Tk events.  Without a guard
     * this handler can be re-entered while an earlier call is still on the
     * stack, running a second command batch and a second redraw through
     * display state the first one is mid-way through building. */
    static int in_handler = 0;
    int n;

    if (!c || in_handler)
	return;
    in_handler = 1;

    n = c->tp->recv(c, buf, sizeof(buf));
    if (n == 0) {
	mcp_client_close(c);
	in_handler = 0;
	return;
    }
    if (n < 0) {
	in_handler = 0;
	return;                   /* nothing readable yet */
    }

    if (mcpcad_session_input(c->sess, buf, (size_t)n) == MCPCAD_ERR_PROTO) {
	mcp_client_close(c);      /* not our protocol; hang up */
	in_handler = 0;
	return;
    }

    /* The commands just ran on the live gedp; repaint any attached
     * display so geometry changes (a 'draw', an edit) appear in the
     * MGED window without the user touching the GUI.  refresh()
     * iterates active_dm_set and guards each entry, so this is a safe
     * no-op when MGED is running headless with no display manager. */
    s->update_views = 1;
    refresh(s);
    in_handler = 0;
}

/* Bind a session to an accepted connection and start watching it. */
static struct mcp_client *
mcp_client_new(const struct mcp_transport *tp, void *handle)
{
    struct mged_state *s = MGED_STATE;
    struct mcp_client *c;

    BU_GET(c, struct mcp_client);
    c->tp = tp;
    c->handle = handle;
    c->sess = mcpcad_session_create(s->gedp, mcp_client_write, c);
    return c;
}


/**********************************************************************
 * Listener lifecycle.  One at a time: a client connects to one or the
 * other, and two live listeners would only make 'status' ambiguous.
 **********************************************************************/

static Tcl_Channel mcp_tcp_listener = NULL;
static int mcp_tcp_port = 0;
static pkg_listener_t *mcp_ipc_listener = NULL;
static struct bu_vls mcp_ipc_path = BU_VLS_INIT_ZERO;


static void
mcp_tcp_accept(ClientData UNUSED(clientData), Tcl_Channel chan,
	       char *UNUSED(host), int UNUSED(port))
{
    struct mcp_client *c;

    /* binary + non-blocking: the protocol is raw bytes, and we must not
     * let Tcl's newline translation or encoding mangle the
     * length-prefixed frames, nor block the event loop on a read */
    Tcl_SetChannelOption(NULL, chan, "-translation", "binary");
    Tcl_SetChannelOption(NULL, chan, "-blocking", "0");
    Tcl_SetChannelOption(NULL, chan, "-buffering", "none");

    c = mcp_client_new(&mcp_tcp, (void *)chan);
    Tcl_CreateChannelHandler(chan, TCL_READABLE, mcp_client_readable,
			     (ClientData)c);
}


static int
mcp_tcp_start(Tcl_Interp *interp, int want_port)
{
    int i;

    /* loopback only - NULL host would bind all interfaces */
    for (i = 0; i < MCP_PORT_TRIES; i++) {
	mcp_tcp_listener = Tcl_OpenTcpServer(interp, want_port + i,
					     "127.0.0.1", mcp_tcp_accept, NULL);
	if (mcp_tcp_listener) {
	    mcp_tcp_port = want_port + i;
	    break;
	}
    }

    if (!mcp_tcp_listener) {
	Tcl_AppendResult(interp, "mcp_listen: could not bind a loopback "
			 "port", NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}


/* Local IPC needs Tcl_CreateFileHandler, which tclDecls.h declares for UNIX
 * and macOS but not for Windows.  MGED already keys that distinction off
 * USE_TCL_CHAN (see mged.h, the same key fbserv.c uses to choose its I/O
 * mechanism), so the transport is built only where descriptors are usable and
 * the command reports its absence elsewhere.  IPC is demonstrated on Linux;
 * Windows would want a named pipe implementation, tested separately.
 */
#ifndef USE_TCL_CHAN

static void
mcp_ipc_accept(ClientData UNUSED(clientData), int UNUSED(mask))
{
    struct mcp_client *c;
    struct pkg_conn *conn;
    int fd;

    if (!mcp_ipc_listener)
	return;

    /* non-blocking, as the TCP path sets on its channel: a blocking
     * descriptor would stall MGED's whole event loop inside read() */
    conn = pkg_accept(mcp_ipc_listener, NULL, NULL, 1);
    if (!conn || conn == PKC_ERROR)
	return;

    fd = pkg_get_read_fd(conn);
    if (fd < 0) {
	pkg_close(conn);
	return;
    }

    c = mcp_client_new(&mcp_ipc, (void *)conn);
    Tcl_CreateFileHandler(fd, TCL_READABLE, mcp_client_readable,
			  (ClientData)c);
}


/* Is a live listener already accepting on *addr*?
 *
 * Connecting is the only reliable test: a socket left behind by a killed
 * session looks identical on disk to one being served.  libpkg knows how to
 * reach its own addresses, so ask it rather than opening a socket here -- that
 * keeps the address format its business and this file free of AF_UNIX
 * specifics.
 */
static int
mcp_ipc_in_use(const char *addr)
{
    struct pkg_conn *probe = pkg_connect_addr(addr, NULL, NULL);

    if (!probe || probe == PKC_ERROR)
	return 0;
    pkg_close(probe);
    return 1;
}


/* *path* may be NULL for a default under the temp directory. */
static int
mcp_ipc_start(Tcl_Interp *interp, const char *path)
{
    struct bu_vls addr = BU_VLS_INIT_ZERO;

    bu_vls_trunc(&mcp_ipc_path, 0);
    if (path && path[0]) {
	bu_vls_strcpy(&mcp_ipc_path, path);
    } else {
	/* pid-qualified so two MGED sessions cannot collide */
	bu_vls_printf(&mcp_ipc_path, "%s/brlcad-mcp-%d.sock",
		      bu_dir(NULL, 0, BU_DIR_TEMP, NULL), bu_pid());
    }

    bu_vls_printf(&addr, "unix:%s", bu_vls_cstr(&mcp_ipc_path));

    /* A leftover socket file makes bind() fail with EADDRINUSE, and the
     * resulting message does not say which file to remove.  MGED only
     * unlinks its own path on a clean 'mcp_listen off', so a killed or
     * crashed session does leave one behind.
     *
     * Clearing it blind would be worse than the problem: if another MGED is
     * live on that path, unlinking makes its socket unreachable while it
     * carries on believing it is serving.  Connecting is the only way to
     * tell a stale file from a live listener, so try it first. */
    if (bu_file_exists(bu_vls_cstr(&mcp_ipc_path), NULL)) {
	if (mcp_ipc_in_use(bu_vls_cstr(&addr))) {
	    Tcl_AppendResult(interp, "mcp_listen: something is already "
			     "listening on ", bu_vls_cstr(&mcp_ipc_path),
			     NULL);
	    bu_vls_trunc(&mcp_ipc_path, 0);
	    bu_vls_free(&addr);
	    return TCL_ERROR;
	}
	bu_file_delete(bu_vls_cstr(&mcp_ipc_path));
    }

    mcp_ipc_listener = pkg_listen(bu_vls_cstr(&addr), NULL,
				  MCP_IPC_BACKLOG, NULL);
    bu_vls_free(&addr);

    if (!mcp_ipc_listener) {
	Tcl_AppendResult(interp, "mcp_listen: could not create an IPC "
			 "socket at ", bu_vls_cstr(&mcp_ipc_path), NULL);
	bu_vls_trunc(&mcp_ipc_path, 0);
	return TCL_ERROR;
    }

    Tcl_CreateFileHandler(pkg_get_listener_fd(mcp_ipc_listener),
			  TCL_READABLE, mcp_ipc_accept, NULL);
    return TCL_OK;
}

#else /* USE_TCL_CHAN */

static int
mcp_ipc_start(Tcl_Interp *interp, const char *UNUSED(path))
{
    Tcl_AppendResult(interp, "mcp_listen: local IPC is not available on this "
		     "platform; use 'mcp_listen [port]' instead", NULL);
    return TCL_ERROR;
}

#endif /* USE_TCL_CHAN */


static void
mcp_stop(void)
{
    if (mcp_tcp_listener) {
	Tcl_Close(NULL, mcp_tcp_listener);
	mcp_tcp_listener = NULL;
	mcp_tcp_port = 0;
    }
    if (mcp_ipc_listener) {
#ifndef USE_TCL_CHAN
	Tcl_DeleteFileHandler(pkg_get_listener_fd(mcp_ipc_listener));
#endif
	pkg_listener_close(mcp_ipc_listener);   /* unlinks the socket path */
	mcp_ipc_listener = NULL;
	bu_vls_trunc(&mcp_ipc_path, 0);
    }
}


static int
mcp_listening(void)
{
    return (mcp_tcp_listener || mcp_ipc_listener);
}


/* Where we are listening, in a form the user can hand to a client. */
static void
mcp_where(struct bu_vls *out)
{
    if (mcp_tcp_listener)
	bu_vls_printf(out, "127.0.0.1:%d", mcp_tcp_port);
    else if (mcp_ipc_listener)
	bu_vls_printf(out, "%s", bu_vls_cstr(&mcp_ipc_path));
}


/**********************************************************************
 * The command.  Argument dispatch only; no transport details.
 *
 * mcp_listen [port|ipc [path]|off|status]
 *
 *   (no args) / port : start a TCP listener on 127.0.0.1, port chosen
 *                      automatically if not given
 *   ipc [path]       : start a local IPC listener on a Unix-domain
 *                      socket, path chosen automatically if not given
 *   off              : stop the listener
 *   status           : report whether listening, and where
 **********************************************************************/
int
f_mcp_listen(ClientData UNUSED(clientData), Tcl_Interp *interp,
	     int argc, const char *argv[])
{
    int is_ipc = (argc >= 2 && BU_STR_EQUAL(argv[1], "ipc"));
    struct bu_vls v = BU_VLS_INIT_ZERO;
    int ret;

    if (argc > 3 || (argc == 3 && !is_ipc)) {
	Tcl_AppendResult(interp,
			 "Usage: mcp_listen [port|ipc [path]|off|status]", NULL);
	return TCL_ERROR;
    }

    if (argc == 2 && BU_STR_EQUAL(argv[1], "off")) {
	mcp_stop();
	Tcl_AppendResult(interp, "mcp_listen: stopped", NULL);
	return TCL_OK;
    }

    if (argc == 2 && BU_STR_EQUAL(argv[1], "status")) {
	if (mcp_listening()) {
	    bu_vls_strcpy(&v, "listening on ");
	    mcp_where(&v);
	} else {
	    bu_vls_strcpy(&v, "not listening");
	}
	Tcl_AppendResult(interp, bu_vls_cstr(&v), NULL);
	bu_vls_free(&v);
	return TCL_OK;
    }

    if (mcp_listening()) {
	Tcl_AppendResult(interp, "mcp_listen: already listening (use "
			 "'mcp_listen off' first)", NULL);
	return TCL_ERROR;
    }

    if (is_ipc) {
	ret = mcp_ipc_start(interp, (argc == 3) ? argv[2] : NULL);
    } else {
	int want_port = MCP_DEFAULT_PORT;

	if (argc == 2) {
	    int p = atoi(argv[1]);
	    if (p <= 0 || p > 65535) {
		Tcl_AppendResult(interp, "mcp_listen: invalid port", NULL);
		return TCL_ERROR;
	    }
	    want_port = p;
	}
	ret = mcp_tcp_start(interp, want_port);
    }

    if (ret != TCL_OK)
	return ret;

    bu_vls_strcpy(&v, "mcp_listen: listening on ");
    mcp_where(&v);
    Tcl_AppendResult(interp, bu_vls_cstr(&v), NULL);
    bu_log("%s\n", bu_vls_cstr(&v));
    bu_vls_free(&v);

    return TCL_OK;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
