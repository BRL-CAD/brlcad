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
#include "bu/getopt.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/snooze.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "pkg.h"
#include "ncp.h"

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
    bu_log("SERVER: Unexpected HELO encountered\n");
    free(buf);
}

/* callback when a DATA message packet is received */
void
server_data(struct pkg_conn *UNUSED(connection), char *buf)
{
    bu_log("SERVER: Received message from client: %s\n", buf);
    free(buf);
}


/* callback when a CIAO message packet is received */
void
server_ciao(struct pkg_conn *UNUSED(connection), char *buf)
{
    bu_log("SERVER: CIAO encountered: %s\n", buf);
    free(buf);
}

int
main() {
    int count = 0;
    struct bu_vls line = BU_VLS_INIT_ZERO;
    bu_vls_extend(&line, 101);

    struct pkg_conn *client;
    struct bu_vls buffer = BU_VLS_INIT_ZERO;
    char *msgbuffer;
    long bytes = 0;
    /** our server callbacks for each message type */
    struct pkg_switch callbacks[] = {
	{MSG_HELO, server_helo, "HELO", NULL},
	{MSG_DATA, server_data, "DATA", NULL},
	{MSG_CIAO, server_ciao, "CIAO", NULL},
	{0, 0, (char *)0, (void*)0}
    };

    // The goal here is to launch a subprocess and communicate with it via
    // stdin/stdout, rather than using a TCP socket.  First we need to launch
    // the process:
    struct bu_process *p = NULL;
    int pac = 1;
    const char *pav[2];
    pav[0] = "pkg_stdio_subp";
    pav[1] = NULL;
    bu_process_exec(&p, pav[0], pac, pav, 0, 0);
    if (!p)
	bu_bomb("Failed to start the subprocess");

    // Get the subprocess stdin and stdout fds
    int infd = bu_process_fileno(p, BU_PROCESS_STDIN);
    int outfd = bu_process_fileno(p, BU_PROCESS_STDOUT);
    if (infd < 0 || outfd < 0) {
	bu_bomb("Unable to get the subprocess I/O channels");
    }

    /* listen for a good client indefinitely.  this is a simple
     * handshake that waits for a HELO message from the client.  if it
     * doesn't get one, the server continues to wait.
     */
    bu_log("Listening for output from subprocess %d fd %d\n", bu_process_pid(p), outfd);
    client = pkg_getclient(PKG_STDIO_MODE, callbacks, NULL, 0);
    if (client == PKC_ERROR) {
	bu_bomb("Fatal error setting up client connection.\n");
    }
    client->pkc_in_fd = outfd;
    client->pkc_out_fd = infd;

    do {
	/* got a connection, process it */
	msgbuffer = pkg_bwaitfor (MSG_HELO, client);
	if (msgbuffer == NULL) {
	    bu_log("Failed to process the client connection, still waiting\n");
	    bu_snooze(BU_SEC2USEC(4));
	} else {
	    bu_log("msgbuffer: %s\n", msgbuffer);
	    /* validate magic header that client should have sent */
	    if (!BU_STR_EQUAL(msgbuffer, MAGIC_ID)) {
		bu_log("Bizarre corruption, received a HELO without at matching MAGIC ID!\n");
		pkg_close(client);
		client = PKC_NULL;
	    }
	}

	// Because the client is a subprocess, it is up to the server to relay
	// what the client may be trying to report via stderr
	count = 0;
	bu_vls_trunc(&line, 0);
	if (bu_process_read((char *)bu_vls_addr(&line), &count, p, BU_PROCESS_STDERR, 100) > 0) {
	    bu_log("STDERR client output: %s\n", bu_vls_cstr(&line));
	}

    } while (client == PKC_NULL);

    /* send the first message from the server */
    bu_vls_sprintf(&buffer, "This is a message from the server.");
    bytes = pkg_send(MSG_DATA, bu_vls_addr(&buffer), (size_t)bu_vls_strlen(&buffer)+1, client);
    if (bytes < 0) goto failure;
    count = 0;
    bu_vls_trunc(&line, 0);
    if (bu_process_read((char *)bu_vls_addr(&line), &count, p, BU_PROCESS_STDERR, 100) > 0) {
	bu_log("STDERR client output: %s\n", bu_vls_cstr(&line));
    }

    /* send another message from the server */
    bu_vls_sprintf(&buffer, "Yet another message from the server.");
    bytes = pkg_send(MSG_DATA, bu_vls_addr(&buffer), (size_t)bu_vls_strlen(&buffer)+1, client);
    if (bytes < 0) goto failure;
    count = 0;
    bu_vls_trunc(&line, 0);
    if (bu_process_read((char *)bu_vls_addr(&line), &count, p, BU_PROCESS_STDERR, 100) > 0) {
	bu_log("STDERR client output: %s\n", bu_vls_cstr(&line));
    }

    /* Tell the client we're done */
    bytes = pkg_send(MSG_CIAO, "DONE", 5, client);
    if (bytes < 0) {
	bu_log("Connection to client seems faulty.\n");
    }
    bu_vls_trunc(&line, 0);
    if (bu_process_read((char *)bu_vls_addr(&line), &count, p, BU_PROCESS_STDERR, 100) > 0) {
	bu_log("STDERR client output: %s\n", bu_vls_cstr(&line));
    }

    /* Wait to hear from the client */
    do {
	(void)pkg_process(client);
	(void)pkg_suckin(client);
	(void)pkg_process(client);
    } while (client->pkc_type != MSG_CIAO);


    /* Confirm the client is done */
    (void)pkg_bwaitfor(MSG_CIAO , client);

    /* shut down the server, one-time use */
    pkg_close(client);
    return 0;
failure:
    pkg_close(client);
    bu_log("Unable to successfully send message");
    bu_vls_free(&buffer);
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
