/*                       S E R V E R . C
 * BRL-CAD
 *
 * Copyright (c) 2006-2021 United States Government as represented by
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
/** @file libpkg/example/server.c
 *
 * Basic pkg server.
 *
 */

#include "common.h"

/* system headers */
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "bio.h"

/* interface headers */
#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "bu/getopt.h"
#include "bu/vls.h"
#include "bu/snooze.h"
#include "pkg.h"
#include "ncp.h"

#include <QApplication>

PKGServer::PKGServer()
    : QTcpServer()
{
    client = NULL;
}

PKGServer::~PKGServer()
{
    if (client->pkc_inbuf)
	free(client->pkc_inbuf);
    BU_PUT(client, struct pkg_conn);
    bu_vls_free(&buffer);
}

void
PKGServer::pgetc()
{
    // Have a new connection pending, accept it.
    s = nextPendingConnection();


    int fd = s->socketDescriptor();
    bu_log("fd: %d\n", fd);
    BU_GET(client, struct pkg_conn);
    client->pkc_magic = PKG_MAGIC;
    client->pkc_fd = fd;
    client->pkc_switch = callbacks;
    client->pkc_errlog = 0;
    client->pkc_left = -1;
    client->pkc_buf = (char *)0;
    client->pkc_curpos = (char *)0;
    client->pkc_strpos = 0;
    client->pkc_incur = client->pkc_inend = 0;

    int have_hello = 0;
    do {
	/* got a connection, process it */
	msgbuffer = pkg_bwaitfor (MSG_HELO, client);
	if (msgbuffer == NULL) {
	    bu_log("Failed to process the client connection, still waiting\n");
	} else {
	    bu_log("msgbuffer: %s\n", msgbuffer);
	    have_hello = 1;
	    /* validate magic header that client should have sent */
	    if (!BU_STR_EQUAL(msgbuffer, MAGIC_ID)) {
		bu_log("Bizarre corruption, received a HELO without at matching MAGIC ID!\n");
	    }
	}
    } while (!have_hello);

    // Kick things off with a message
    psend(MSG_DATA, "This is a message from the server.");

    // For subsequent communications, when the client sends us something
    // trigger the print_and_respond slot.
    QObject::connect(s, &QTcpSocket::readyRead, this, &PKGServer::print_and_respond);
}

void
PKGServer::print_and_respond()
{
    // Grab what the client sent us.  We use Qt's mechanisms for this rather than
    // libpkg - pkg_suckin doesn't appear to work with the Qt socket.
    QByteArray dbuff = s->read(client->pkc_inlen);

    // Now that we have the data using Qt, prepare it for processing by libpkg
    client->pkc_inbuf = (char *)realloc(client->pkc_inbuf, dbuff.length());
    memcpy(client->pkc_inbuf, dbuff.data(), dbuff.length());
    client->pkc_incur = 0;
    client->pkc_inlen = client->pkc_inend = dbuff.length();

    // Buffer is read and staged - process
    pkg_process(client);

    // Once we have confirmation from the client it got the done message, exit
    if (client->pkc_type == MSG_CIAO) {
	bu_log("MSG_CIAO\n");
	bu_exit(0, "done");
    }

    // We should have gotten a MSG_DATA input
    if (client->pkc_type == MSG_DATA) {
	bu_log("MSG_DATA\n");
    }

    // We want to go back and forth a bit before quitting - count up
    // communications cycles and once we hit the threshold send the client the
    // DONE message.
    if (cycles < 10) {
	psend(MSG_DATA, "Acknowledged");
	cycles++;
    } else {
	psend(MSG_CIAO, "DONE");
    }
}

int
PKGServer::psend(int type, const char *data)
{
    bu_vls_sprintf(&buffer, "%s ", data);
    return pkg_send(type, bu_vls_addr(&buffer), (size_t)bu_vls_strlen(&buffer)+1, client);
}

/*
 * callback when a HELO message packet is received.
 *
 * We should not encounter this packet specifically since we listened
 * for it before beginning processing of packets as part of a simple
 * handshake setup.
 */
void
server_helo(struct pkg_conn *UNUSED(connection), char *buf)
{
    bu_log("Unexpected HELO encountered\n");
    free(buf);
}

/* callback when a DATA message packet is received */
void
server_data(struct pkg_conn *UNUSED(connection), char *buf)
{
    bu_log("Received message from client: %s\n", buf);
    free(buf);
}


/* callback when a CIAO message packet is received */
void
server_ciao(struct pkg_conn *UNUSED(connection), char *buf)
{
    bu_log("CIAO encountered: %s\n", buf);
    free(buf);
}

int
main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    PKGServer *tcps = new PKGServer;

    /* ignore broken pipes, on platforms where we have SIGPIPE */
#ifdef SIGPIPE
    (void)signal(SIGPIPE, SIG_IGN);
#endif

    /** our server callbacks for each message type */
    struct pkg_switch callbacks[] = {
	{MSG_HELO, server_helo, "HELO", NULL},
	{MSG_DATA, server_data, "DATA", NULL},
	{MSG_CIAO, server_ciao, "CIAO", NULL},
	{0, 0, (char *)0, (void*)0}
    };
    tcps->callbacks = (struct pkg_switch *)callbacks;

    QObject::connect(
	    tcps, &PKGServer::newConnection,
	    tcps, &PKGServer::pgetc,
	    Qt::QueuedConnection
	    );

    // Start listening for a connection on the default port
    tcps->listen(QHostAddress::LocalHost, tcps->port);

    app.exec();
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
