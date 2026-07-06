/*                       S E R V E R . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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
/** @file server.c
 *
 * Manual/integration TCP harness for the libmcpcad pipeline.
 *
 * NOT registered with CTest (it blocks listening) and NOT the final
 * transport - this is the development scaffold sanctioned for the
 * TCP-first approach, and the target for Python mock-client
 * integration tests.  The eventual MGED integration replaces only
 * this file's socket loop; the session it drives is identical.
 *
 * Loopback only, by policy.  One client at a time.
 *
 * Usage:
 *   mcpcad_test_server [port] [db.g]
 *
 * defaults: port 5959, scratch db in the system temp directory.
 * The protocol is length-prefixed (see mcpcad.h) - poke it with the
 * companion tool:  python3 mcpc.py 5959 ls
 *
 */

#include "common.h"

#ifndef _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "ged.h"
#include "mcpcad.h"


static void
sock_write(const char *data, size_t len, void *ctx)
{
    int fd = *(int *)ctx;

    while (len > 0) {
	ssize_t n = write(fd, data, len);
	if (n <= 0)
	    return;  /* client went away; reads will notice and clean up */
	data += n;
	len -= (size_t)n;
    }
}


int
main(int argc, char *argv[])
{
    int port = 5959;
    char dbpath[MAXPATHLEN] = {0};
    struct ged *gedp;
    int lfd;
    struct sockaddr_in addr;
    int one = 1;

    bu_setprogname(argv[0]);

    if (argc > 1)
	port = atoi(argv[1]);
    if (argc > 2) {
	bu_strlcpy(dbpath, argv[2], MAXPATHLEN);
    } else {
	bu_dir(dbpath, MAXPATHLEN, BU_DIR_TEMP, "mcpcad_server.g", NULL);
    }

    gedp = ged_open("db", dbpath, 0);
    if (!gedp) {
	bu_log("ERROR: cannot open or create %s\n", dbpath);
	return 1;
    }

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0) {
	perror("socket");
	return 1;
    }
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);  /* 127.0.0.1 ONLY */
    addr.sin_port = htons((uint16_t)port);

    if (bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
	perror("bind");
	return 1;
    }
    if (listen(lfd, 1) < 0) {
	perror("listen");
	return 1;
    }

    /* port 0 = kernel-assigned; recover the real one so clients
     * (and the integration test) can read it off our stdout */
    if (port == 0) {
	struct sockaddr_in got;
	socklen_t glen = sizeof(got);
	if (getsockname(lfd, (struct sockaddr *)&got, &glen) == 0)
	    port = ntohs(got.sin_port);
    }

    printf("mcpcad scaffold listening on 127.0.0.1:%d\n", port);
    printf("database: %s\n", dbpath);
    printf("try:  nc 127.0.0.1 %d    then type:  ls\n", port);
    fflush(stdout);

    for (;;) {
	int cfd = accept(lfd, NULL, NULL);
	struct mcpcad_session *s;
	char chunk[4096];
	ssize_t n;

	if (cfd < 0)
	    continue;

	printf("client connected\n");
	fflush(stdout);

	s = mcpcad_session_create(gedp, sock_write, &cfd);
	while ((n = read(cfd, chunk, sizeof(chunk))) > 0) {
	    if (mcpcad_session_input(s, chunk, (size_t)n) == MCPCAD_ERR_PROTO)
		break;  /* not our protocol; hang up */
	}

	mcpcad_session_destroy(s);
	close(cfd);
	printf("client disconnected\n");
	fflush(stdout);
    }

    /* not reached; Ctrl-C terminates.  gedp/lfd reclaimed by exit. */
    return 0;
}

#else /* _WIN32 */

#include <stdio.h>

int
main(void)
{
    fprintf(stderr, "mcpcad_test_server: POSIX-only development scaffold\n");
    return 1;
}

#endif /* _WIN32 */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
