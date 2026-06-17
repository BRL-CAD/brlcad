/*                      F B S E R V . C P P
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
/** @file fbserv.cpp
 *
 *  These are the Qt specific callbacks used for I/O between client
 *  and server.
 *
 *  TODO - Look into QLocalSocket, and whether we might be able to
 *  generalize libpkg (or even just use parts of it) to allow us
 *  to communicate using that mechanism...
 *
 *  Initial thought - optional callback functions to replace
 *  select, read, etc - if not set default to current behavior,
 *  if set do the callback instead of those calls...
 */

#include "common.h"

/* bu/ipc.h removed - transport handled by libpkg */
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "dm.h"
#include "./fbserv.h"
#include "qtcad/QgGL.h"
#include "qtcad/QgSW.h"
#include <QElapsedTimer>

namespace {
static const qint64 DRAIN_TIME_BUDGET_MS = 4;
static const size_t DRAIN_BYTES_BUDGET = 512 * 1024;
static const int DRAIN_ITERATION_CAP = 256;
}

void
QFBSocket::client_handler()
{
    QTCAD_SLOT("QFBSocket::client_handler", 1);
    bu_log("client_handler\n");

    /* If our client slot has already been torn down (e.g. socket
     * disconnected, drop_client called), there is nothing to do. */
    if (!fbsp || ind < 0 || ind >= MAX_CLIENTS)
	return;
    struct pkg_conn *pkc = fbsp->fbs_clients[ind].fbsc_pkg;
    if (!pkc || !s)
	return;

    /* Set the current framebuffer pointer for callback functions */
    pkc->pkc_server_data = (void *)fbsp->fbs_fbp;

    // Read data.  NOTE:  we're using the Qt read routines rather than
    // pkg_suckin, so we can't call fbs_existing_client_hander from libdm.
    // Initially tried pkg_suckin, but it didn't seem to work with the socket
    // as set up by Qt.
    QByteArray dbuff = s->read(s->bytesAvailable());

    // We may not have processed all the read data last time, so append
    // this to anything left over from before
    buff.append(dbuff);

    // If we don't have anything, we're done
    if (!buff.length())
	return;

    // Now that we have the data read using Qt methods, prepare for processing
    // using libpkg data structures.
    pkc->pkc_inbuf = (char *)realloc(pkc->pkc_inbuf, buff.length());
    memcpy(pkc->pkc_inbuf, buff.data(), buff.length());
    pkc->pkc_incur = 0;
    pkc->pkc_inlen = pkc->pkc_inend = buff.length();

    // Now it's up to libpkg - if anything is left over, we'll know it after
    // processing.  Clear buff so we're ready to preserve remaining data for
    // the next processing cycle.
    buff.clear();

    // Use the defined callbacks to handle the data sent from the client
    if ((pkg_process(pkc)) < 0)
	bu_log("client_handler pkg_process error encountered\n");

    /* Phase E1 (ert reliability): the unprocessed remainder of pkc_inbuf
     * is the byte range [pkc_incur, pkc_inend) — *not* [pkc_inend,
     * pkc_inlen) (which would be uninitialized buffer past the data).
     * Preserve only the unread tail for the next read cycle. */
    if (pkc->pkc_incur < pkc->pkc_inend) {
	buff.append(&pkc->pkc_inbuf[pkc->pkc_incur],
		    pkc->pkc_inend - pkc->pkc_incur);
    }

    emit updated();

    // If we've got callbacks, execute them now.
    if (fbsp->fbs_callback != (void (*)(void *))FBS_CALLBACK_NULL) {
	/* need to cast func pointer explicitly to get the function call */
	void (*cfp)(void *);
	cfp = (void (*)(void *))fbsp->fbs_callback;
	cfp(fbsp->fbs_clientData);
    }
}

/* Phase D2 (ert reliability): when the remote rt closes the TCP
 * connection, tear down the fbserv client slot cleanly so the
 * pkg_conn / fd / chan are released and a follow-on rt session can
 * occupy the slot.  Without this, a finished rt leaves an orphan
 * QFBSocket and pkg_conn alive until application exit. */
void
QFBSocket::on_disconnected()
{
    QTCAD_SLOT("QFBSocket::on_disconnected", 1);
    if (!fbsp || ind < 0 || ind >= MAX_CLIENTS)
	return;
    if (fbsp->fbs_clients[ind].fbsc_fd == 0)
	return;	/* already dropped */
    fbs_drop_client(fbsp, ind);
}


QFBServer::QFBServer(struct fbserv_obj *fp)
{
    fbsp = fp;
}

QFBServer::~QFBServer()
{
}

void
QFBServer::on_Connect()
{
    QTCAD_SLOT("QFBServer::on_Connect", 1);
    // Have a new connection pending, accept it.
    QTcpSocket *tcps = nextPendingConnection();

    bu_log("new connection");

    QFBSocket *fs = new QFBSocket;
    fs->s = tcps;
    fs->fbsp = fbsp;

    int fd = tcps->socketDescriptor();
    bu_log("fd: %d\n", fd);
    struct pkg_conn *pc = pkg_adopt_socket(fd, fbs_pkg_switch(), 0);
    if (pc == PKC_ERROR) {
	bu_log("new connection failed (pkg_adopt_socket)");
	tcps->close();
	delete fs;
	return;
    }

    fs->ind = fbs_new_client(fbsp, pc, (void *)fs);
    if (fs->ind == -1) {
	bu_log("new connection failed");
	pkg_close(pc);
	tcps->close();
	delete fs;
    }
}

/* Check if we're already listening. */
int
qdm_is_listening(struct fbserv_obj *fbsp)
{
    bu_log("is_listening\n");
    if (fbsp->fbs_listener.fbsl_fd >= 0) {
	return 1;
    }
    return 0;
}

int
qdm_listen_on_port(struct fbserv_obj *fbsp, int available_port)
{
    bu_log("listen on port\n");
    QFBServer *nl = new QFBServer(fbsp);
    nl->port = available_port;
    if (!nl->listen(QHostAddress::LocalHost, available_port)) {
	bu_log("Failed to start listening on %d\n", available_port);
	delete nl;
	return 0;
    }
    fbsp->fbs_listener.fbsl_chan = (void *)nl;
    fbsp->fbs_listener.fbsl_fd = nl->socketDescriptor();
    if (fbsp->fbs_listener.fbsl_fd >= 0)
	return 1;
    return 0;
}

void
qdm_open_server_handler(struct fbserv_obj *fbsp)
{
    bu_log("open_server_handler\n");
    QFBServer *nl = (QFBServer *)fbsp->fbs_listener.fbsl_chan;
    if (!nl->isListening())
	bu_log("not listening!\n");
    QObject::connect(nl, &QTcpServer::newConnection, nl, &QFBServer::on_Connect, Qt::QueuedConnection);
}

void
qdm_close_server_handler(struct fbserv_obj *fbsp)
{
    bu_log("close_server_handler\n");
    QFBServer *nl = (QFBServer *)fbsp->fbs_listener.fbsl_chan;
    delete nl;
}

#ifdef BRLCAD_OPENGL
void
qdm_open_client_handler(struct fbserv_obj *fbsp, int i, void *data)
{
    bu_log("open_client_handler\n");
    fbsp->fbs_clients[i].fbsc_chan = data;
    QFBSocket *s = (QFBSocket *)data;
    QObject::connect(s->s, &QTcpSocket::readyRead, s, &QFBSocket::client_handler, Qt::QueuedConnection);
    /* Phase D2: tear down on remote disconnect. */
    QObject::connect(s->s, &QTcpSocket::disconnected, s, &QFBSocket::on_disconnected, Qt::QueuedConnection);

    QgGL *ctx = (QgGL *)dm_get_ctx(fb_get_dm(fbsp->fbs_fbp));
    if (ctx) {
	QObject::connect(s, &QFBSocket::updated, ctx, &QgGL::need_update, Qt::QueuedConnection);
    }
}
#endif

// Because swrast uses a bsg_view as its context pointer, we need to unpack the
// app data to get our Qt widget ctx when using that display method.  In other
// words, the swrast backend is generic - it has no knowledge of Qt - and the
// Qt widget we need to notify for update/redraw purposes is coming (from the
// libdm perspective) solely from the application - which is why it lives in
// the user data slot rather than the context.  (The swrast offscreen rendering
// context is still present and relevant, hence the need for a separate user
// pointer.)  The advantage of using a generic swrast backend is that such a
// setup allows us to use the same logic both for Qt widget rendering and
// headless image generation.
void
qdm_open_sw_client_handler(struct fbserv_obj *fbsp, int i, void *data)
{
    bu_log("open_client_handler\n");
    fbsp->fbs_clients[i].fbsc_chan = data;
    QFBSocket *s = (QFBSocket *)data;
    QObject::connect(s->s, &QTcpSocket::readyRead, s, &QFBSocket::client_handler, Qt::QueuedConnection);
    /* Phase D2: tear down on remote disconnect. */
    QObject::connect(s->s, &QTcpSocket::disconnected, s, &QFBSocket::on_disconnected, Qt::QueuedConnection);

    QgSW *ctx = (QgSW *)dm_get_udata(fb_get_dm(fbsp->fbs_fbp));
    if (ctx) {
	QObject::connect(s, &QFBSocket::updated, ctx, &QgSW::need_update, Qt::QueuedConnection);
    }
}

void
qdm_close_client_handler(struct fbserv_obj *fbsp, int i)
{
    bu_log("close_client_handler\n");
    QFBSocket *s = (QFBSocket *)fbsp->fbs_clients[i].fbsc_chan;
    if (s) {
	/* Phase D2 (ert reliability): use deleteLater() to be safe when
	 * called from within a slot of `s` itself (e.g. on_disconnected). */
	s->deleteLater();
    }
    fbsp->fbs_clients[i].fbsc_chan = NULL;
}


/* -----------------------------------------------------------------------
 * Phase 5: IPC client handler for qged (pipe/socketpair instead of TCP).
 *
 * QFBIPCSocket registers the pre-connected raw file descriptor from libpkg
 * ipc with Qt's event loop via QSocketNotifier.  When the fd is readable,
 * ipc_handler() reads one message frame from the pkg_conn and dispatches it
 * via pkg_process().
 *
 * This avoids QTcpSocket entirely for the local rt→framebuffer data path.
 * ----------------------------------------------------------------------- */

void
QFBIPCSocket::ipc_handler()
{
    QTCAD_SLOT("QFBIPCSocket::ipc_handler", 1);

    if (!fbsp || ind < 0 || ind >= MAX_CLIENTS)
	return;

    struct fbserv_client *fbsc = &fbsp->fbs_clients[ind];
    struct pkg_conn *pkc = fbsc->fbsc_pkg;
    if (!pkc) {
	if (notifier) notifier->setEnabled(false);
	return;
    }

    /* Phase H2 (ert reliability): pkc_server_data is set once at slot
     * registration time in fbs_open_ipc(); rewriting it on every event
     * is at best redundant and at worst racy if a handler invoked from
     * pkg_process() recurses through the event loop.  Do not re-stamp
     * it here. */

    /* Phase C3 (ert reliability): drain the fd in a tight loop while
     * notifications are disabled.  Qt's QSocketNotifier is level-
     * triggered: with a slow GUI thread it can re-enqueue activated()
     * events faster than we can service them, and a slot scheduled
     * for an already-drained fd would otherwise call read() and (on a
     * blocking fd) hang the GUI thread inside _pkg_io_read.  We rely
     * on pkg_suckin's EAGAIN handling (Phase C2) to break the loop
     * cleanly when the kernel buffer is empty. */
    if (notifier)
	notifier->setEnabled(false);

    int got_real_eof = 0;
    int got_error = 0;
    int data_read = 0;
    size_t bytes_drained = 0;
    QElapsedTimer drain_timer;
    drain_timer.start();
    /* Adaptive burst draining:
     *  - stop if we spend too long in one notifier callback (UI fairness)
     *  - or if we have already drained a large burst of data
     *  - with an absolute iteration cap as a final guardrail. */
    for (int iter = 0; iter < DRAIN_ITERATION_CAP; ++iter) {
	int r = pkg_suckin(pkc);
	if (r > 0) {
	    data_read = 1;
	    bytes_drained += (size_t)r;
	    if (bytes_drained >= DRAIN_BYTES_BUDGET || drain_timer.hasExpired(DRAIN_TIME_BUDGET_MS))
		break;
	    continue;
	}
	if (r < 0) {
	    got_error = 1;
	    break;
	}
	/* r == 0 — either EOF or EAGAIN/no-data on a non-blocking fd. */
	if (pkc->pkc_would_block)
	    break;	/* no more data right now */
	got_real_eof = 1;
	break;
    }

    if (data_read) {
	if (pkg_process(pkc) < 0)
	    bu_log("QFBIPCSocket::ipc_handler: pkg_process error\n");
	emit updated();

	if (fbsp->fbs_callback != (void (*)(void *))FBS_CALLBACK_NULL) {
	    void (*cfp)(void *) = (void (*)(void *))fbsp->fbs_callback;
	    cfp(fbsp->fbs_clientData);
	}
    }

    /* Phase D1 (ert reliability): on EOF or unrecoverable read error,
     * tear the client slot down on this thread.  We are already on the
     * GUI thread (the slot is invoked by Qt's notifier dispatch), so
     * direct teardown is safe.  fbs_drop_client() invokes the registered
     * close-IPC handler (qdm_close_ipc_client_handler), which uses
     * deleteLater() — meaning `this` remains valid through the rest of
     * this slot but is destroyed at the next event-loop iteration. */
    if (got_real_eof || got_error) {
	bu_log("QFBIPCSocket::ipc_handler: ind=%d got_real_eof=%d got_error=%d data_read=%d -> dropping client\n",
	       ind, got_real_eof, got_error, data_read);
	fbs_drop_client(fbsp, ind);
	return;
    }

    if (notifier)
	notifier->setEnabled(true);
}


#ifdef BRLCAD_OPENGL
void
qdm_open_ipc_client_handler(struct fbserv_obj *fbsp, int i, void *UNUSED(data))
{
    bu_log("open_ipc_client_handler (GL)\n");

    QFBIPCSocket *s = new QFBIPCSocket;
    s->ind  = i;
    s->fbsp = fbsp;
    s->notifier = new QSocketNotifier(fbsp->fbs_clients[i].fbsc_fd,
				      QSocketNotifier::Read, s);
    fbsp->fbs_clients[i].fbsc_chan = (void *)s;

    QObject::connect(s->notifier, &QSocketNotifier::activated,
		     s, &QFBIPCSocket::ipc_handler, Qt::QueuedConnection);

    QgGL *ctx = (QgGL *)dm_get_ctx(fb_get_dm(fbsp->fbs_fbp));
    if (ctx) {
	QObject::connect(s, &QFBIPCSocket::updated,
			 ctx, &QgGL::need_update, Qt::QueuedConnection);
    }
}
#endif

void
qdm_open_ipc_sw_client_handler(struct fbserv_obj *fbsp, int i, void *UNUSED(data))
{
    bu_log("open_ipc_client_handler (SW)\n");

    QFBIPCSocket *s = new QFBIPCSocket;
    s->ind  = i;
    s->fbsp = fbsp;
    s->notifier = new QSocketNotifier(fbsp->fbs_clients[i].fbsc_fd,
				      QSocketNotifier::Read, s);
    fbsp->fbs_clients[i].fbsc_chan = (void *)s;

    QObject::connect(s->notifier, &QSocketNotifier::activated,
		     s, &QFBIPCSocket::ipc_handler, Qt::QueuedConnection);

    QgSW *ctx = (QgSW *)dm_get_udata(fb_get_dm(fbsp->fbs_fbp));
    if (ctx) {
	QObject::connect(s, &QFBIPCSocket::updated,
			 ctx, &QgSW::need_update, Qt::QueuedConnection);
    }
}

void
qdm_close_ipc_client_handler(struct fbserv_obj *fbsp, int i)
{
    bu_log("close_ipc_client_handler\n");
    QFBIPCSocket *s = (QFBIPCSocket *)fbsp->fbs_clients[i].fbsc_chan;
    if (s) {
	/* Phase D1 (ert reliability): use deleteLater() so the close
	 * handler is safe to call from within a slot of `s` itself
	 * (e.g. ipc_handler() detecting EOF and asking for a drop).
	 * Disable the notifier first so no further activations fire
	 * after the QObject pointer is otherwise considered dead. */
	if (s->notifier)
	    s->notifier->setEnabled(false);
	s->deleteLater();
    }
    fbsp->fbs_clients[i].fbsc_chan = NULL;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
