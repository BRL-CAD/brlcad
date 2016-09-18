/*                        C L I E N T . C
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
/** @file libpkg/example/client.c
 *
 * Relatively simple example file transfer program using libpkg,
 * written in a ttcp style.
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
#include "bu/malloc.h"
#include "bu/getopt.h"
#include "bu/file.h"
#include "bu/vls.h"
#include "pkg.h"
#include "ntp.h"

/* used by the client to pass the dbip and opened transfer file
 * descriptor.
 */
typedef struct _my_data_ {
    struct pkg_conn *connection;
    const char *server;
    int port;
} my_data;


/**
 * callback when a HELO message packet is received.
 *
 * We should not encounter this packet specifically since we listened
 * for it before beginning processing of packets as part of a simple
 * handshake setup.
 */
void
client_helo(struct pkg_conn *UNUSED(connection), char *buf)
{
    bu_log("Unexpected HELO encountered\n");
    free(buf);
}


/**
 * callback when a DATA message packet is received
 */
void
client_data(struct pkg_conn *connection, char *buf)
{
    bu_log("Received file data: %s\n", buf);
    bu_vls_printf((struct bu_vls *)connection->pkc_user_data, "\n%s\n", buf);
    free(buf);
}


/**
 * callback when a CIAO message packet is received
 */
void
client_ciao(struct pkg_conn *UNUSED(connection), char *buf)
{
    bu_log("CIAO encountered: %s\n", buf);
    free(buf);
}


/**
 * start up a client that connects to the given server.
 */
void
run_client(const char *server, int port, struct bu_vls *all_msgs)
{
    my_data stash;
    char s_port[MAX_DIGITS + 1] = {0};
    long bytes = 0;
    int pkg_result = 0;
    /** our callbacks for each message type */
    struct pkg_switch callbacks[] = {
	{MSG_HELO, client_helo, "HELO", NULL},
	{MSG_DATA, client_data, "DATA", NULL},
	{MSG_CIAO, client_ciao, "CIAO", NULL},
	{0, 0, (char *)0, (void*)0}
    };

    callbacks[1].pks_user_data = (void *)all_msgs;

    snprintf(s_port, MAX_DIGITS, "%d", port);
    stash.connection = pkg_open(server, s_port, "tcp", NULL, NULL, NULL, NULL);
    if (stash.connection == PKC_ERROR) {
	bu_log("Connection to %s, port %d, failed.\n", server, port);
	bu_bomb("ERROR: Unable to open a connection to the server\n");
    }
    stash.server = server;
    stash.port = port;
    stash.connection->pkc_switch = callbacks;


    /* let the server know we're cool. */
    bytes = pkg_send(MSG_HELO, MAGIC_ID, strlen(MAGIC_ID) + 1, stash.connection);
    if (bytes < 0) {
	pkg_close(stash.connection);
	bu_log("Connection to %s, port %d, seems faulty.\n", server, port);
	bu_bomb("ERROR: Unable to communicate with the server\n");
    }

    /* Server should have a message to send us.
     */
    bu_log("Processing data from server\n");
    do {
	/* process packets potentially received in a processing callback */
	pkg_result = pkg_process(stash.connection);
	if (pkg_result < 0) {
	    bu_log("Unable to process packets? Weird.\n");
	} else {
	    bu_log("Processed %d packet%s\n", pkg_result, pkg_result == 1 ? "" : "s");
	}

	/* suck in data from the network */
	pkg_result = pkg_suckin(stash.connection);
	if (pkg_result < 0) {
	    bu_log("Seemed to have trouble sucking in packets.\n");
	    break;
	} else if (pkg_result == 0) {
	    bu_log("Client closed the connection.\n");
	    break;
	}

	/* process new packets received */
	pkg_result = pkg_process(stash.connection);
	if (pkg_result < 0) {
	    bu_log("Unable to process packets? Weird.\n");
	} else {
	    bu_log("Processed %d packet%s\n", pkg_result, pkg_result == 1 ? "" : "s");
	}

    } while (stash.connection->pkc_type != MSG_CIAO);

    /* let the server know we're done. */
    bytes = pkg_send(MSG_CIAO, "BYE", 4, stash.connection);
    if (bytes < 0) {
	bu_log("Unable to cleanly disconnect from %s, port %d.\n", server, port);
    }

    /* flush output and close */
    pkg_close(stash.connection);

    return;
}


/**
 * main application for both the client and server
 */
int
main() {
    int port = 2000;
    const char *server_name = "127.0.0.1";
    struct bu_vls *all_msgs;
    BU_GET(all_msgs, struct bu_vls);
    bu_vls_init(all_msgs);

    /* fire up the client */
    bu_log("Connecting to %s, port %d\n", server_name, port);
    run_client(server_name, port, all_msgs);

    bu_log("All messages: %s\n", bu_vls_addr(all_msgs));

    bu_vls_free(all_msgs);
    BU_PUT(all_msgs, struct bu_vls);
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
