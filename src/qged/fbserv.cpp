/*                      F B S E R V . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
#include "qtcad/QtGL.h"
#include "qtcad/QtSW.h"

void
QFBSocket::client_handler()
{
    bu_log("client_handler\n");

    // Get the current libpkg connection
    struct pkg_conn *pkc = fbsp->fbs_clients[ind].fbsc_pkg;

    // Set the current framebuffer pointer for callback functions
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

    if (pkc->pkc_inend != pkc->pkc_inlen - 1) {
	// If pkg_process didn't use all of the read data, store the rest for
	// the next cycle.
	//
	// TODO - need to find a way to test to to make sure we're copying the
	// right part of the buffer
	buff.append(&pkc->pkc_inbuf[pkc->pkc_inend], pkc->pkc_inlen - pkc->pkc_inend);
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
    bu_log("create QFBServer\n");
    fbsp = fp;
}

QFBServer::~QFBServer()
{
}

void
QFBServer::on_Connect()
{
    // Have a new connection pending, accept it.
    QTcpSocket *tcps = nextPendingConnection();

    bu_log("new connection");

    QFBSocket *fs = new QFBSocket;
    fs->s = tcps;
    fs->fbsp = fbsp;

    int fd = tcps->socketDescriptor();
    bu_log("fd: %d\n", fd);
    struct pkg_conn *pc;
    BU_GET(pc, struct pkg_conn);
    pc->pkc_magic = PKG_MAGIC;
    pc->pkc_fd = fd;
    pc->pkc_switch = fbs_pkg_switch();
    pc->pkc_errlog = 0;
    pc->pkc_left = -1;
    pc->pkc_buf = (char *)0;
    pc->pkc_curpos = (char *)0;
    pc->pkc_strpos = 0;
    pc->pkc_incur = pc->pkc_inend = 0;

    fs->ind = fbs_new_client(fbsp, pc, (void *)fs);
    if (fs->ind == -1) {
	bu_log("new connection failed");
	BU_PUT(pc, struct pkg_conn);
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

    QtGL *ctx = (QtGL *)dm_get_ctx(fb_get_dm(fbsp->fbs_fbp));
    if (ctx) {
	QObject::connect(s, &QFBSocket::updated, ctx, &QtGL::need_update, Qt::QueuedConnection);
    }
}
#endif

// Because swrast uses a bview as its context pointer, we need to unpack the app data
// when using that display method
void
qdm_open_sw_client_handler(struct fbserv_obj *fbsp, int i, void *data)
{
    bu_log("open_client_handler\n");
    fbsp->fbs_clients[i].fbsc_chan = data;
    QFBSocket *s = (QFBSocket *)data;
    QObject::connect(s->s, &QTcpSocket::readyRead, s, &QFBSocket::client_handler, Qt::QueuedConnection);

    QtSW *ctx = (QtSW *)dm_get_udata(fb_get_dm(fbsp->fbs_fbp));
    if (ctx) {
	QObject::connect(s, &QFBSocket::updated, ctx, &QtSW::need_update, Qt::QueuedConnection);
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

