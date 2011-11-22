/*                          T P K G . C
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
/** @file libpkg/tpkg.c
 *
 * Relatively simple example file transfer program using libpkg,
 * written in a ttcp style.
 *
 * To compile from an install:
 * gcc -I/usr/brlcad/include -L/usr/brlcad/lib -o tpkg tpkg.c -lpkg -lbu
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


/* used by the client to pass the dbip and opened transfer file
 * descriptor.
 */
typedef struct _my_data_ {
    struct pkg_conn *connection;
    const char *server;
    int port;
} my_data;

/* simple network transport protocol. connection starts with a HELO,
 * then a variable number of GEOM/ARGS messages, then a CIAO to end.
 */
#define MAGIC_ID	"TPKG"
#define MSG_HELO	1
#define MSG_DATA	2
#define MSG_CIAO	3

/* maximum number of digits on a port number */
#define MAX_DIGITS	5


/**
 * print a usage statement when invoked with bad, help, or no arguments
 */
static void
usage(const char *msg, const char *argv0)
{
    if (msg) {
	bu_log("%s\n", msg);
    }
    bu_log("Client Usage: %s [-t] [-p#] host file\n\t-p#\tport number to send to (default 2000)\n\thost\thostname or IP address of receiving server\n\tfile\tsome file to transfer\n", argv0 ? argv0 : MAGIC_ID);
    bu_log("Server Usage: %s -r [-p#]\n\t-p#\tport number to listen on (default 2000)\n", argv0 ? argv0 : MAGIC_ID);

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
    int pkg_result  = 0;
    char *buffer;

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
	buffer = pkg_bwaitfor (MSG_HELO, client);
	if (buffer == NULL) {
	    bu_log("Failed to process the client connection, still waiting\n");
	    pkg_close(client);
	    client = PKC_NULL;
	} else {
	    /* validate magic header that client should have sent */
	    if (!BU_STR_EQUAL(buffer, MAGIC_ID)) {
		bu_log("Bizarre corruption, received a HELO without at matching MAGIC ID!\n");
		pkg_close(client);
		client = PKC_NULL;
	    }
	}
    } while (client == PKC_NULL);

    /* we got a validated client, process packets from the
     * connection.  boilerplate triple-call loop.
     */
    bu_log("Processing data from client\n");
    do {
	/* process packets potentially received in a processing callback */
	pkg_result = pkg_process(client);
	if (pkg_result < 0) {
	    bu_log("Unable to process packets? Weird.\n");
	} else {
	    bu_log("Processed %d packet%s\n", pkg_result, pkg_result == 1 ? "" : "s");
	}

	/* suck in data from the network */
	pkg_result = pkg_suckin(client);
	if (pkg_result < 0) {
	    bu_log("Seemed to have trouble sucking in packets.\n");
	    break;
	} else if (pkg_result == 0) {
	    bu_log("Client closed the connection.\n");
	    break;
	}

	/* process new packets received */
	pkg_result = pkg_process(client);
	if (pkg_result < 0) {
	    bu_log("Unable to process packets? Weird.\n");
	} else {
	    bu_log("Processed %d packet%s\n", pkg_result, pkg_result == 1 ? "" : "s");
	}
    } while (client != NULL);

    /* shut down the server, one-time use */
    pkg_close(client);
}


/**
 * start up a client that connects to the given server, and sends
 * serialized file data.
 */
void
run_client(const char *server, int port, const char *file)
{
    my_data stash;
    char s_port[MAX_DIGITS + 1] = {0};
    long bytes = 0;
    FILE *fp = (FILE *)NULL;
    static const unsigned int TPKG_BUFSIZE = 2048;
    char *buffer;

    buffer = (char *)bu_calloc(TPKG_BUFSIZE, 1, "buffer allocation");

    /* make sure the file can be opened */
    fp = fopen(file, "rb");
    if (fp == NULL) {
	bu_log("Unable to open %s\n", file);
	bu_bomb("Unable to read file\n");
    }

    /* open a connection to the server */
    validate_port(port);

    snprintf(s_port, MAX_DIGITS, "%d", port);
    stash.connection = pkg_open(server, s_port, "tcp", NULL, NULL, NULL, NULL);
    if (stash.connection == PKC_ERROR) {
	bu_log("Connection to %s, port %d, failed.\n", server, port);
	bu_bomb("ERROR: Unable to open a connection to the server\n");
    }
    stash.server = server;
    stash.port = port;

    /* let the server know we're cool.  also, send the database title
     * along with the MAGIC ident just because we can.
     */
    bytes = pkg_send(MSG_HELO, MAGIC_ID, strlen(MAGIC_ID) + 1, stash.connection);
    if (bytes < 0) {
	pkg_close(stash.connection);
	bu_log("Connection to %s, port %d, seems faulty.\n", server, port);
	bu_bomb("ERROR: Unable to communicate with the server\n");
    }

    /* send the file data to the server */
    while (!feof(fp) && !ferror(fp)) {
	bytes = fread(buffer, 1, 2048, fp);
	bu_log("Read %ld bytes from %s\n", bytes, file);

	if (bytes > 0) {
	    bytes = pkg_send(MSG_DATA, buffer, (size_t)bytes, stash.connection);
	    if (bytes < 0) {
		pkg_close(stash.connection);
		bu_log("Unable to successfully send data to %s, port %d.\n", stash.server, stash.port);
		bu_free(buffer, "buffer release");
		return;
	    }
	}
    }

    /* let the server know we're done.  not necessary, but polite. */
    bytes = pkg_send(MSG_CIAO, "BYE", 4, stash.connection);
    if (bytes < 0) {
	bu_log("Unable to cleanly disconnect from %s, port %d.\n", server, port);
    }

    /* flush output and close */
    pkg_close(stash.connection);
    fclose(fp);
    bu_free(buffer, "buffer release");

    return;
}


/**
 * main application for both the client and server
 */
int
main(int argc, char *argv[]) {
    const char * const argv0 = argv[0];
    int c;
    int server = 0; /* not a server by default */
    int port = 2000;

    /* client stuff */
    const char *server_name = NULL;
    const char *file = NULL;

    if (argc < 2) {
	usage("ERROR: Missing arguments\n", argv[0]);
    }

    /* process the command-line arguments after the application name */
    while ((c = bu_getopt(argc, argv, "tTrRp:P:hH")) != -1) {
	switch (c) {
	    case 't':
	    case 'T':
		/* sending */
		server = 0;
		break;
	    case 'r':
	    case 'R':
		/* receiving */
		server = 1;
		break;
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

    if (server) {
	if (argc > 0) {
	    usage("ERROR: Unexpected extra server arguments\n", argv0);
	}

	/* ignore broken pipes */
#ifdef SIGPIPE
	(void)signal(SIGPIPE, SIG_IGN);
#endif

	/* fire up the server */
	bu_log("Listening on port %d\n", port);
	run_server(port);

	return 0;
    }

    /* prep up the client */
    if (argc < 1) {
	usage("ERROR: Missing hostname and file arguments\n", argv0);
    } else if (argc < 2) {
	usage("ERROR: Missing file argument\n", argv0);
    } else if (argc > 2) {
	usage("ERROR: Too many arguments provided\n", argv0);
    }

    server_name = *argv++;
    file = *argv++;

    /* make sure the file exists */
    if (!bu_file_exists(file)) {
	bu_log("File does not exist: %s\n", file);
	bu_bomb("Need a file to transfer\n");
    }

    /* fire up the client */
    bu_log("Connecting to %s, port %d\n", server_name, port);
    run_client(server_name, port, file);

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
