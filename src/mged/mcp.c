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
 * external extensions (MCP servers, scripts) over a local socket,
 * using the libmcpcad command pipeline.
 *
 * The listener is hooked into MGED's existing Tcl event loop via
 * Tcl_OpenTcpServer + Tcl_CreateChannelHandler - the same mechanism
 * fbserv uses (see fbserv.c).  Because both the accept callback and
 * the per-client read callback fire on the main thread, commands run
 * synchronously alongside human GUI input with no locking, exactly
 * as the GSoC design intends.  When MGED is superseded by a Qt/MOOSE
 * front end, only this file changes: the same libmcpcad session is
 * driven from a QTcpServer/QProcess instead.
 *
 * Loopback only, by policy - a TCP listener on a routable interface
 * trips corporate/lab security tooling.  This is a development-stage
 * transport; the plan of record is to migrate to libpkg/stdio IPC.
 */

#include "common.h"

#include <string.h>

#include "tcl.h"

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "ged.h"
#include "mcpcad.h"

#include "./mged.h"

#define MCP_DEFAULT_PORT 5959
#define MCP_PORT_TRIES   64
#define MCP_READ_CHUNK   4096

/* The single listen channel for this MGED instance (one server). */
static Tcl_Channel mcp_listen_chan = NULL;
static int mcp_listen_port = 0;

/* Per-client context: a libmcpcad session plus the channel it speaks
 * over.  One of these is heap-allocated per accepted connection and
 * handed to the channel read handler as its ClientData. */
struct mcp_client {
    Tcl_Channel chan;
    struct mcpcad_session *sess;
};


/* libmcpcad write callback: ship reply bytes back over the channel.
 * ctx is the owning mcp_client. */
static void
mcp_chan_write(const char *data, size_t len, void *ctx)
{
    struct mcp_client *c = (struct mcp_client *)ctx;

    if (!c || !c->chan)
	return;
    Tcl_Write(c->chan, data, (int)len);
    Tcl_Flush(c->chan);
}


static void
mcp_client_close(struct mcp_client *c)
{
    if (!c)
	return;
    if (c->chan) {
	Tcl_DeleteChannelHandler(c->chan, NULL, (ClientData)c);
	Tcl_Close(NULL, c->chan);
    }
    mcpcad_session_destroy(c->sess);
    BU_PUT(c, struct mcp_client);
}


/* Fires whenever a connected client has bytes (or has hung up).
 * Reads what is available, feeds it to the session, and tears the
 * connection down on EOF or an unrecoverable protocol fault. */
static void
mcp_client_readable(ClientData clientData, int UNUSED(mask))
{
    struct mged_state *s = MGED_STATE;
    struct mcp_client *c = (struct mcp_client *)clientData;
    char buf[MCP_READ_CHUNK];
    int n;

    if (!c)
	return;

    n = Tcl_Read(c->chan, buf, sizeof(buf));
    if (n <= 0) {
	if (Tcl_Eof(c->chan))
	    mcp_client_close(c);
	return;
    }

    if (mcpcad_session_input(c->sess, buf, (size_t)n) == MCPCAD_ERR_PROTO) {
	mcp_client_close(c);  /* not our protocol; hang up */
	return;
    }

    /* The commands just ran on the live gedp; repaint any attached
     * display so geometry changes (a 'draw', an edit) appear in the
     * MGED window without the user touching the GUI.  refresh()
     * iterates active_dm_set and guards each entry, so this is a safe
     * no-op when MGED is running headless with no display manager. */
    s->update_views = 1;
    refresh(s);
}


/* Tcl_OpenTcpServer accept callback: stand up a session bound to this
 * MGED's live database and start listening for the client's bytes. */
static void
mcp_accept(ClientData UNUSED(clientData), Tcl_Channel chan,
	   char *UNUSED(host), int UNUSED(port))
{
    struct mged_state *s = MGED_STATE;
    struct mcp_client *c;

    /* binary + non-blocking: the protocol is raw bytes, and we must
     * not let Tcl's newline translation or encoding mangle the
     * length-prefixed frames, nor block the event loop on a read */
    Tcl_SetChannelOption(NULL, chan, "-translation", "binary");
    Tcl_SetChannelOption(NULL, chan, "-blocking", "0");
    Tcl_SetChannelOption(NULL, chan, "-buffering", "none");

    BU_GET(c, struct mcp_client);
    c->chan = chan;
    c->sess = mcpcad_session_create(s->gedp, mcp_chan_write, c);

    Tcl_CreateChannelHandler(chan, TCL_READABLE, mcp_client_readable,
			     (ClientData)c);
}


static void
mcp_stop(void)
{
    if (mcp_listen_chan) {
	Tcl_Close(NULL, mcp_listen_chan);
	mcp_listen_chan = NULL;
	mcp_listen_port = 0;
    }
}


/*
 * mcp_listen [port|off|status]
 *
 *   (no args) / port : start the listener on 127.0.0.1 (port chosen
 *                      automatically if not given), report the port
 *   off              : stop the listener
 *   status           : report whether listening and on what port
 */
int
f_mcp_listen(ClientData UNUSED(clientData), Tcl_Interp *interp,
	     int argc, const char *argv[])
{
    int want_port = MCP_DEFAULT_PORT;
    int i;

    if (argc > 2) {
	Tcl_AppendResult(interp, "Usage: mcp_listen [port|off|status]", NULL);
	return TCL_ERROR;
    }

    if (argc == 2 && BU_STR_EQUAL(argv[1], "off")) {
	mcp_stop();
	Tcl_AppendResult(interp, "mcp_listen: stopped", NULL);
	return TCL_OK;
    }

    if (argc == 2 && BU_STR_EQUAL(argv[1], "status")) {
	struct bu_vls v = BU_VLS_INIT_ZERO;
	if (mcp_listen_chan)
	    bu_vls_printf(&v, "listening on 127.0.0.1:%d", mcp_listen_port);
	else
	    bu_vls_printf(&v, "not listening");
	Tcl_AppendResult(interp, bu_vls_cstr(&v), NULL);
	bu_vls_free(&v);
	return TCL_OK;
    }

    if (mcp_listen_chan) {
	Tcl_AppendResult(interp, "mcp_listen: already listening (use "
			 "'mcp_listen off' first)", NULL);
	return TCL_ERROR;
    }

    if (argc == 2) {
	int p = atoi(argv[1]);
	if (p <= 0 || p > 65535) {
	    Tcl_AppendResult(interp, "mcp_listen: invalid port", NULL);
	    return TCL_ERROR;
	}
	want_port = p;
    }

    /* loopback only - NULL host would bind all interfaces */
    for (i = 0; i < MCP_PORT_TRIES; i++) {
	mcp_listen_chan = Tcl_OpenTcpServer(interp, want_port + i,
					    "127.0.0.1", mcp_accept, NULL);
	if (mcp_listen_chan) {
	    mcp_listen_port = want_port + i;
	    break;
	}
    }

    if (!mcp_listen_chan) {
	Tcl_AppendResult(interp, "mcp_listen: could not bind a loopback "
			 "port", NULL);
	return TCL_ERROR;
    }

    {
	struct bu_vls v = BU_VLS_INIT_ZERO;
	bu_vls_printf(&v, "mcp_listen: listening on 127.0.0.1:%d",
		      mcp_listen_port);
	Tcl_AppendResult(interp, bu_vls_cstr(&v), NULL);
	bu_log("%s\n", bu_vls_cstr(&v));
	bu_vls_free(&v);
    }

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
