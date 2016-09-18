/*                       S E R V E R . C
 * BRL-CAD
 *
 * Copyright (c) 2006-2016 United States Government as represented by
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
#include "pkg.h"
#include "ntp.h"

/**
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


/**
 * callback when a DATA message packet is received
 */
void
server_data(struct pkg_conn *UNUSED(connection), char *buf)
{
    bu_log("Received file data: %s\n", buf);
    free(buf);
}


/**
 * callback when a CIAO message packet is received
 */
void
server_ciao(struct pkg_conn *UNUSED(connection), char *buf)
{
    bu_log("CIAO encountered: %s\n", buf);
    free(buf);
}


/**
 * start up a server that listens for a single client.
 */
void
run_server(int port) {
    struct pkg_conn *client;
    int netfd;
    char portname[MAX_DIGITS + 1] = {0};
    /* int pkg_result  = 0; */
    char *buffer, *msgbuffer;
    long bytes = 0;

    /** our server callbacks for each message type */
    struct pkg_switch callbacks[] = {
	{MSG_HELO, server_helo, "HELO", NULL},
	{MSG_DATA, server_data, "DATA", NULL},
	{MSG_CIAO, server_ciao, "CIAO", NULL},
	{0, 0, (char *)0, (void*)0}
    };

    /* start up the server on the given port */
    snprintf(portname, MAX_DIGITS, "%d", port);
    netfd = pkg_permserver(portname, "tcp", 0, 0);
    if (netfd < 0) {
	bu_bomb("Unable to start the server");
    }

    /* listen for a good client indefinitely.  this is a simple
     * handshake that waits for a HELO message from the client.  if it
     * doesn't get one, the server continues to wait.
     */
    do {
	client = pkg_getclient(netfd, callbacks, NULL, 0);
	if (client == PKC_NULL) {
	    bu_log("Connection seems to be busy, waiting...\n");
	    sleep(10);
	    continue;
	} else if (client == PKC_ERROR) {
	    bu_log("Fatal error accepting client connection.\n");
	    pkg_close(client);
	    client = PKC_NULL;
	    continue;
	}

	/* got a connection, process it */
	msgbuffer = pkg_bwaitfor (MSG_HELO, client);
	if (msgbuffer == NULL) {
	    bu_log("Failed to process the client connection, still waiting\n");
	    pkg_close(client);
	    client = PKC_NULL;
	} else {
	    bu_log("msgbuffer: %s\n", msgbuffer);
	    /* validate magic header that client should have sent */
	    if (!BU_STR_EQUAL(msgbuffer, MAGIC_ID)) {
		bu_log("Bizarre corruption, received a HELO without at matching MAGIC ID!\n");
		pkg_close(client);
		client = PKC_NULL;
	    }
	}
    } while (client == PKC_NULL);

    /* have client, will send file */
    buffer = (char *)bu_calloc(2048, 1, "buffer allocation");
    snprintf(buffer, 2048, "This is a message from the server.");

    /* send the message to the server */
    bytes = pkg_send(MSG_DATA, buffer, (size_t)strlen(buffer), client);
    if (bytes < 0) {
	pkg_close(client);
	bu_log("Unable to successfully send message");
	bu_free(buffer, "buffer release");
	return;
    }

    /* send another message to the server */
    snprintf(buffer, 2048, "Yet another message from the server.");
    bytes = pkg_send(MSG_DATA, buffer, (size_t)strlen(buffer), client);
    if (bytes < 0) {
	pkg_close(client);
	bu_log("Unable to successfully send message");
	bu_free(buffer, "buffer release");
	return;
    }

    /* Tell the client we're done */
    bytes = pkg_send(MSG_CIAO, "DONE", 5, client);
    if (bytes < 0) {
	bu_log("Connection to client seems faulty.\n");
    }

    /* Confirm the client is done */
    buffer = pkg_bwaitfor(MSG_CIAO , client);
    bu_log("buffer: %s\n", buffer);

    /* shut down the server, one-time use */
    pkg_close(client);
}

/**
 * main application for both the client and server
 */
int
main() {
    int port = 2000;

    /* ignore broken pipes, on platforms where we have SIGPIPE */
#ifdef SIGPIPE
    (void)signal(SIGPIPE, SIG_IGN);
#endif

    /* fire up the server */
    bu_log("Listening on port %d\n", port);
    run_server(port);

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
