/*                      F B S E R V . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2024 United States Government as represented by
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

#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "dm.h"
#include "./fbserv.h"
#include "qtcad/QgGL.h"
#include "qtcad/QgSW.h"

void
QFBSocket::client_handler()
{
    QTCAD_SLOT("QFBSocket::client_handler", 1);
    bu_log("client_handler\n");

    // Get the current libpkg connection
    struct pkg_conn *pkc = fbsp->fbs_clients[ind].fbsc_pkg;

    // Set the current framebuffer pointer for callback functions
    pkg_conn_server_data_set(pkc, (void *)fbsp->fbs_fbp);

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
    char **pkc_inbuf = pkg_conn_inbuf(pkc);
    if (!pkc_inbuf)
	return;
    *pkc_inbuf = (char *)realloc(*pkc_inbuf, buff.length());
    memcpy(*pkc_inbuf, buff.data(), buff.length());
    int *pkc_incur = pkg_conn_incur(pkc);
    int *pkc_inlen = pkg_conn_inlen(pkc);
    int *pkc_inend = pkg_conn_inend(pkc);
    if (pkc_incur)
       *pkc_incur = 0;
    if (pkc_inlen)
       *pkc_inlen = buff.length();
    if (pkc_inend)
       *pkc_inend = buff.length();

    // Now it's up to libpkg - if anything is left over, we'll know it after
    // processing.  Clear buff so we're ready to preserve remaining data for
    // the next processing cycle.
    buff.clear();

    // Use the defined callbacks to handle the data sent from the client
    if ((pkg_process(pkc)) < 0)
	bu_log("client_handler pkg_process error encountered\n");

    if (*pkc_inend != *pkc_inlen - 1) {
	// If pkg_process didn't use all of the read data, store the rest for
	// the next cycle.
	//
	// TODO - need to find a way to test to to make sure we're copying the
	// right part of the buffer
	buff.append(&(*pkc_inbuf)[*pkc_inend], *pkc_inlen - *pkc_inend);
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
    struct pkg_conn *pc = pkg_conn_create(fd);
    pkg_conn_msg_handlers_set(pc, fbs_pkg_switch());

    fs->ind = fbs_new_client(fbsp, pc, (void *)fs);
    if (fs->ind == -1) {
	bu_log("new connection failed");
	pkg_conn_destroy(pc);
	tcps->close();
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

    QgGL *ctx = (QgGL *)dm_get_ctx(fb_get_dm(fbsp->fbs_fbp));
    if (ctx) {
	QObject::connect(s, &QFBSocket::updated, ctx, &QgGL::need_update, Qt::QueuedConnection);
    }
}
#endif

// Because swrast uses a bview as its context pointer, we need to unpack the
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
    delete s;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

