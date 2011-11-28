/*                       S E R V E R . C
 * BRL-CAD
 *
 * Copyright (c) 2006-2011 United States Government as represented by
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
#include <stdio.h>
#include "bio.h"

/* interface headers */
#include "bu.h"
#include "pkg.h"
#include "ntp.h"

/**
 * print a usage statement when invoked with bad, help, or no arguments
 */
static void
usage(const char *msg, const char *argv0)
{
    if (msg) {
	bu_log("%s\n", msg);
    }
    bu_log("Server Usage: %s [-p#]\n\t-p#\tport number to listen on (default 2000)\n", argv0 ? argv0 : MAGIC_ID);

    bu_log("\n%s", pkg_version());

    exit(1);
}

/**
 * simple "valid" port number check used by client and server
 */
static void
validate_port(int port) {
    if (port < 0 || port > 0xffff) {
        bu_bomb("Invalid negative port range\n");
    }
}

/**
 * callback when a HELO message packet is received.
 *
 * We should not encounter this packet specifically since we listened
 * for it before beginning processing of packets as part of a simple
 * handshake setup.
 */
void
server_helo(struct pkg_conn *connection, char *buf)
{
    connection=connection; /* quell */
    bu_log("Unexpected HELO encountered\n");
    free(buf);
}


/**
 * callback when a DATA message packet is received
 */
void
server_data(struct pkg_conn *connection, char *buf)
{
    connection=connection; /* quell */
    bu_log("Received file data\n");
    free(buf);
}


/**
 * callback when a CIAO message packet is received
 */
void
server_ciao(struct pkg_conn *connection, char *buf)
{
    connection=connection; /* quell */
    bu_log("CIAO encountered\n");
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
    //int pkg_result  = 0;
    char *buffer, *msgbuffer;
    long bytes = 0;
    FILE *fp;

    /** our server callbacks for each message type */
    struct pkg_switch callbacks[] = {
	{MSG_HELO, server_helo, "HELO", NULL},
	{MSG_DATA, server_data, "DATA", NULL},
	{MSG_CIAO, server_ciao, "CIAO", NULL},
	{0, 0, (char *)0, (void*)0}
    };

    validate_port(port);

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
    fp = fopen("lempar.c", "rb");
    buffer = (char *)bu_calloc(2048, 1, "buffer allocation");

    if (fp == NULL) {
	bu_log("Unable to open lempar.c\n");
	bu_bomb("Unable to read file\n");
    }

    /* send the file data to the server */
    while (!feof(fp) && !ferror(fp)) {
	bytes = fread(buffer, 1, 2048, fp);
	bu_log("Read %ld bytes from lempar.c\n", bytes);

	if (bytes > 0) {
	    bytes = pkg_send(MSG_DATA, buffer, (size_t)bytes, client);
	    if (bytes < 0) {
		pkg_close(client);
		bu_log("Unable to successfully send data");
		bu_free(buffer, "buffer release");
		return;
	    }
	}
    }

    /* Tell the client we're done */
    bytes = pkg_send(MSG_CIAO, "DONE", 5, client);
    if (bytes < 0) {
	bu_log("Connection to client seems faulty.\n");
    }

    /* Confirm the client is done */
    buffer = pkg_bwaitfor (MSG_CIAO , client);
    bu_log("buffer: %s\n", buffer);

    /* shut down the server, one-time use */
    pkg_close(client);
}

/**
 * main application for both the client and server
 */
int
main(int argc, char *argv[]) {
    const char * const argv0 = argv[0];
    int c;
    int port = 2000;

    if (argc > 2) {
	usage("ERROR: Incorrect number of arguments\n", argv[0]);
    }

    /* process the command-line arguments after the application name */
    while ((c = bu_getopt(argc, argv, "p:P:hH")) != -1) {
	switch (c) {
	    case 'p':
	    case 'P':
		port = atoi(bu_optarg);
		break;
	    case 'h':
	    case 'H':
		/* help */
		usage(NULL, argv0);
		break;
	    default:
		usage("ERROR: Unknown argument\n", argv0);
	}
    }

    argc -= bu_optind;
    argv += bu_optind;

    if (argc > 0) {
	usage("ERROR: Unexpected extra server arguments\n", argv0);
    }

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
