/*                        C L I E N T . C
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
#include "bu/snooze.h"
#include "pkg.h"
#include "ncp.h"

/* callback when an unexpected message packet is received. */
void
client_unexpected(struct pkg_conn *UNUSED(connection), char *buf)
{
    bu_log("CLIENT: Unexpected message package encountered\n");
    free(buf);
}


/* callback when a DATA message packet is received */
void
client_data(struct pkg_conn *connection, char *buf)
{
    bu_log("CLIENT: Received file data: %s\n", buf);
    bu_vls_printf((struct bu_vls *)connection->pkc_user_data, "\n%s\n", buf);
    free(buf);
}

/* callback when a CIAO message packet is received */
void
client_ciao(struct pkg_conn *UNUSED(connection), char *buf)
{
    bu_log("CLIENT: CIAO received from server: %s\n", buf);
    free(buf);
}

int
main() {
    struct bu_vls all_msgs = BU_VLS_INIT_ZERO;
    struct pkg_conn *connection;
    long bytes = 0;
    int pkg_result = 0;
    /** our callbacks for each message type */
    struct pkg_switch callbacks[] = {
	{MSG_HELO, client_unexpected, "HELO", NULL},
	{MSG_DATA, client_data, "DATA", NULL},
	{MSG_CIAO, client_ciao, "CIAO", NULL},
	{0, 0, (char *)0, (void*)0}
    };

    bu_snooze(BU_SEC2USEC(4));

    /* Collect data from more than one server communication for later use */
    callbacks[1].pks_user_data = (void *)&all_msgs;

    /* fire up the client */
    connection = pkg_open("STDIO", NULL, NULL, NULL, NULL, NULL, NULL);
    if (connection == PKC_ERROR) {
	bu_bomb("CLIENT: ERROR: Unable to open a STDIO connection\n");
    }
    connection->pkc_in_fd = fileno(stdin);
    connection->pkc_out_fd = fileno(stdout);
    if (connection->pkc_in_fd < 0 || connection->pkc_out_fd < 0) {
	// If we can't get valid file descriptors (for example, if we're a
	// Windows GUI mode program) we can't operate in this mode.
	bu_bomb("CLIENT: ERROR: Unable to open a STDIO connection\n");
    }

    connection->pkc_switch = callbacks;

    /* let the server know we're cool. */
    bytes = pkg_send(MSG_HELO, MAGIC_ID, strlen(MAGIC_ID) + 1, connection);
    if (bytes < 0) {
	pkg_close(connection);
	bu_log("CLIENT: STDIO connection seems faulty.\n");
	bu_bomb("CLIENT: ERROR: Unable to communicate with the server\n");
    }

    /* Server should have a message to send us.
    */
    bu_log("CLIENT: Processing data from server\n");
    do {
	/* process packets potentially received in a processing callback */
	pkg_result = pkg_process(connection);
	if (pkg_result < 0) {
	    bu_log("CLIENT: Unable to process packets? Weird.\n");
	} else if (pkg_result > 0) {
	    bu_log("CLIENT: Processed %d packet%s\n", pkg_result, pkg_result == 1 ? "" : "s");
	}

	/* suck in data from the network */
	pkg_result = pkg_suckin(connection);
	if (pkg_result < 0) {
	    bu_log("CLIENT: Seemed to have trouble sucking in packets.\n");
	    break;
	} else if (pkg_result == 0) {
	    bu_log("CLIENT: Client closed the connection.\n");
	    break;
	}

	/* process new packets received */
	pkg_result = pkg_process(connection);
	if (pkg_result < 0) {
	    bu_log("CLIENT: Unable to process packets? Weird.\n");
	} else if (pkg_result > 0) {
	    bu_log("CLIENT: Processed %d packet%s\n", pkg_result, pkg_result == 1 ? "" : "s");
	}

    } while (connection->pkc_type != MSG_CIAO);

    /* server's done, send our own message back to it */
    bytes = pkg_send(MSG_DATA, "Message from client", 20, connection);

    /* let the server know we're done. */
    bytes = pkg_send(MSG_CIAO, "DONE", 5, connection);
    bytes = pkg_send(MSG_CIAO, "BYE", 4, connection);
    if (bytes < 0) {
	bu_log("CLIENT: Unable to cleanly disconnect.\n");
    }

    /* flush output and close */
    pkg_close(connection);

    bu_log("CLIENT: All messages: %s\n", bu_vls_addr(&all_msgs));

    bu_vls_free(&all_msgs);
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
