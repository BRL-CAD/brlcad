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
    pkg_close(client);
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
}

int
PKGServer::psend(int type, const char *data)
{
    bu_vls_sprintf(&buffer, "%s", data);
    return pkg_send(type, bu_vls_addr(&buffer), (size_t)bu_vls_strlen(&buffer)+1, client);
}

void
PKGServer::waitfor_client()
{
    do {
	(void)pkg_process(client);
	(void)pkg_suckin(client);
	(void)pkg_process(client);
    } while (client->pkc_type != MSG_CIAO);

    /* Confirm the client is done */
    (void)pkg_bwaitfor(MSG_CIAO , client);
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

    /* send the first message to the server */
    if (tcps->psend(MSG_DATA, "This is a message from the server.") < 0)
	goto failure;

    /* send another message to the server */
    if (tcps->psend(MSG_DATA, "Yet another message from the server.") < 0)
	goto failure;

    /* Tell the client we're done */
    if (tcps->psend(MSG_CIAO, "DONE") < 0)
	bu_log("Connection to client seems faulty.\n");

    /* Wait to hear from the client */
    tcps->waitfor_client();

    /* shut down the server, one-time use */
    delete tcps;
    return 0;
failure:
    bu_log("Unable to successfully send message");
    delete tcps;
    return 0;
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
