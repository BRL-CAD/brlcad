/*                 R E G R E S S _ P K G . C P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2021 United States Government as represented by
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
/** @file regress_pkg.cpp
 *
 * Brief description
 *
 */

#define MAGIC_ID	"RPKG"
#define MSG_HELO	1
#define MSG_DATA	2
#define MSG_CIAO	3
#define MAX_PORT_DIGITS      5

#include "common.h"

#include <iostream>
#include <thread>

/* system headers */
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include "bio.h"

/* interface headers */
#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/snooze.h"
#include "bu/str.h"
#include "bu/time.h"
#include "bu/vls.h"
#include "pkg.h"

/*
 * callback when a HELO message packet is received.
 *
 * We should not encounter this packet specifically since we listened
 * for it before beginning processing of packets as part of a simple
 * handshake setup.
 */
void
server_helo(struct pkg_conn *UNUSED(connection), char *UNUSED(buf))
{
    bu_exit(-1, "Unexpected HELO encountered\n");
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
server_main(int UNUSED(argc), const char *UNUSED(argv)) {
    int port = 2000;
    struct pkg_conn *client;
    int netfd;
    char portname[MAX_PORT_DIGITS + 1] = {0};
    /* int pkg_result  = 0; */
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

    /* ignore broken pipes, on platforms where we have SIGPIPE */
#ifdef SIGPIPE
    (void)signal(SIGPIPE, SIG_IGN);
#endif

    /* start up the server on the given port */
    snprintf(portname, MAX_PORT_DIGITS, "%d", port);
    netfd = pkg_permserver(portname, "tcp", 0, 0);
    if (netfd < 0) {
	bu_exit(-1, "Unable to start the server");
    }

    /* listen for a good client indefinitely.  this is a simple
     * handshake that waits for a HELO message from the client.  if it
     * doesn't get one, the server continues to wait.
     */
    int64_t timer = bu_gettime();
    bu_log("Listening on port %d\n", port);
    do {
	client = pkg_getclient(netfd, callbacks, NULL, 1);
	if (client == PKC_NULL) {
	    int wait_time = 5;
	    if ((bu_gettime() - timer) > BU_SEC2USEC(wait_time)) {
		bu_log("Connection inactive for > %d seconds, quitting.\n", wait_time);
		bu_exit(1, "Timeout - inactive");
	    }
	    bu_log("No client input available, waiting...\n");
	    bu_snooze(BU_SEC2USEC((double)wait_time / 10.0));
	    continue;
	} else if (client == PKC_ERROR) {
	    pkg_close(client);
	    client = PKC_NULL;
	    bu_log("ERROR: client == PKC_ERROR\n");
	    bu_exit(-1, "Server exiting\n");
	    continue;
	}

	// Something happened - reset idle timer
	timer = bu_gettime();

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

    /* send the first message to the server */
    bu_vls_sprintf(&buffer, "This is a message from the server.");
    bytes = pkg_send(MSG_DATA, bu_vls_addr(&buffer), (size_t)bu_vls_strlen(&buffer)+1, client);
    if (bytes < 0) goto failure;

    /* send another message to the server */
    bu_vls_sprintf(&buffer, "Yet another message from the server.");
    bytes = pkg_send(MSG_DATA, bu_vls_addr(&buffer), (size_t)bu_vls_strlen(&buffer)+1, client);
    if (bytes < 0) goto failure;

    /* Tell the client we're done */
    bytes = pkg_send(MSG_CIAO, "DONE", 5, client);
    if (bytes < 0) {
	bu_exit(-1, "Connection to client seems faulty.\n");
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
    bu_vls_free(&buffer);
    bu_exit(-1, "Unable to successfully send message.\n");

    return -1;
}

/* callback when an unexpected message packet is received. */
void
client_unexpected(struct pkg_conn *UNUSED(connection), char *UNUSED(buf))
{
    bu_exit(-1, "Unexpected message package encountered\n");
}

/* callback when a DATA message packet is received */
void
client_data(struct pkg_conn *connection, char *buf)
{
    bu_log("Received file data: %s\n", buf);
    bu_vls_printf((struct bu_vls *)connection->pkc_user_data, "\n%s\n", buf);
    free(buf);
}

/* callback when a CIAO message packet is received */
void
client_ciao(struct pkg_conn *UNUSED(connection), char *buf)
{
    bu_log("CIAO received from server: %s\n", buf);
    free(buf);
}

int
client_main(int UNUSED(argc), const char **UNUSED(argv)) {
    int port = 2000;
    const char *server = "127.0.0.1";
    struct bu_vls all_msgs = BU_VLS_INIT_ZERO;
    struct pkg_conn *connection = PKC_ERROR;
    char s_port[MAX_PORT_DIGITS + 1] = {0};
    long bytes = 0;
    int pkg_result = 0;

    /** our callbacks for each message type */
    struct pkg_switch callbacks[] = {
	{MSG_HELO, client_unexpected, "HELO", NULL},
	{MSG_DATA, client_data, "DATA", NULL},
	{MSG_CIAO, client_ciao, "CIAO", NULL},
	{0, 0, (char *)0, (void*)0}
    };

    /* Collect data from more than one server communication for later use */
    callbacks[1].pks_user_data = (void *)&all_msgs;

    /* fire up the client */
    bu_log("Connecting to %s, port %d\n", server, port);
    snprintf(s_port, MAX_PORT_DIGITS, "%d", port);

    int wait_time = 5;
    int64_t timer = bu_gettime();

    connection = pkg_open(server, s_port, "tcp", NULL, NULL, NULL, NULL);
    while (connection == PKC_ERROR && (bu_gettime() - timer) < BU_SEC2USEC(wait_time)) {
	bu_snooze(BU_SEC2USEC((double)wait_time / 10.0));
	connection = pkg_open(server, s_port, "tcp", NULL, NULL, NULL, NULL);
    }

    if (connection == PKC_ERROR) {
	bu_log("Connection to %s, port %d, failed.\n", server, port);
	bu_log("ERROR: unable to connect to server after %d seconds trying\n", wait_time);
	bu_exit(-1, "Client exiting\n");
    }

    connection->pkc_switch = callbacks;

    /* let the server know we're cool. */
    bytes = pkg_send(MSG_HELO, MAGIC_ID, strlen(MAGIC_ID) + 1, connection);
    if (bytes < 0) {
	pkg_close(connection);
	bu_log("Connection to %s, port %d, seems faulty.\n", server, port);
	bu_exit(-1, "ERROR: Unable to communicate with the server\n");
    }

    /* Server should have a message to send us.
    */
    bu_log("Processing data from server\n");
    do {
	/* process packets potentially received in a processing callback */
	pkg_result = pkg_process(connection);
	if (pkg_result < 0) {
	    bu_log("Unable to process packets? Weird.\n");
	} else if (pkg_result > 0) {
	    bu_log("Processed %d packet%s\n", pkg_result, pkg_result == 1 ? "" : "s");
	}

	/* suck in data from the network */
	pkg_result = pkg_suckin(connection);
	if (pkg_result < 0) {
	    bu_log("Seemed to have trouble sucking in packets.\n");
	    break;
	} else if (pkg_result == 0) {
	    bu_log("Client closed the connection.\n");
	    break;
	}

	/* process new packets received */
	pkg_result = pkg_process(connection);
	if (pkg_result < 0) {
	    bu_log("Unable to process packets? Weird.\n");
	} else if (pkg_result > 0) {
	    bu_log("Processed %d packet%s\n", pkg_result, pkg_result == 1 ? "" : "s");
	}

    } while (connection->pkc_type != MSG_CIAO);

    /* server's done, send our own message back to it */
    bytes = pkg_send(MSG_DATA, "Message from client", 20, connection);
    if (bytes < 0) {
	bu_exit(-1, "Unable to cleanly send message from client, server %s, port %d.\n", server, port);
    }

    /* let the server know we're done. */
    bytes = pkg_send(MSG_CIAO, "DONE", 5, connection);
    if (bytes < 0) {
	bu_exit(-1, "Unable to cleanly send DONE from client, server %s, port %d.\n", server, port);
    }

    /* wrap up */
    bytes = pkg_send(MSG_CIAO, "BYE", 4, connection);
    if (bytes < 0) {
	bu_exit(-1, "Unable to cleanly disconnect from %s, port %d.\n", server, port);
    }

    /* flush output and close */
    pkg_close(connection);

    bu_log("All messages: %s\n", bu_vls_addr(&all_msgs));

    bu_vls_free(&all_msgs);
    return 0;
}

class cmd_result {
    public:
	int cmd_ret = 0;
};

void
run_server(cmd_result &r)
{
    std::cout << "SERVER thread starting" << std::endl;
    r.cmd_ret = server_main(0, NULL);
    std::cout << "SERVER thread ending" << std::endl;
}

void
run_client(cmd_result &r)
{
    std::cout << "CLIENT thread starting" << std::endl;
    r.cmd_ret = client_main(0, NULL);
    std::cout << "CLIENT thread ending" << std::endl;
}

int
main(int argc, const char *argv[])
{
    if (argc != 1) {
	std::cout << "Usage: regress_pkg\n";
	return -1;
    }

    bu_setprogname(argv[0]);

    // Set up the information the threads will need
    cmd_result s, c;

    // Launch the client and server
    std::cout << "Launching server" << std::endl;
    std::thread server(run_server, std::ref(s));
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    std::cout << "Launching client" << std::endl;
    std::thread client(run_client, std::ref(c));
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Wait for both to finish up, client first so we know it tried
    std::cout << "Waiting for client to exit" << std::endl;
    client.join();
    std::cout << "Waiting for server to exit" << std::endl;
    server.join();

    // If either client or server had a problem, overall test fails
    return (s.cmd_ret || c.cmd_ret) ? 1 : 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
