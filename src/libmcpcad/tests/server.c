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
 *   mcpcad_test_server [port | /path/to.sock] [db.g]
 *
 * A first argument beginning with '/' is taken as a Unix-domain socket
 * path and served over local IPC instead of a loopback port; a port
 * number never starts with '/', so the two cannot be confused.  IPC is
 * the transport for environments where opening a TCP port is restricted,
 * and it is the same choice 'mcp_listen ipc' offers inside MGED.
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

#include "bio.h"
#include "bnetwork.h"
#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/str.h"
#include "ged.h"
#include "mcpcad.h"
#include "pkg.h"


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


/* Serve one client on *cfd* until it hangs up or breaks the protocol. */
static void
serve_client(struct ged *gedp, int cfd)
{
    struct mcpcad_session *s = mcpcad_session_create(gedp, sock_write, &cfd);
    char chunk[4096];
    ssize_t n;

    while ((n = read(cfd, chunk, sizeof(chunk))) > 0) {
	if (mcpcad_session_input(s, chunk, (size_t)n) == MCPCAD_ERR_PROTO)
	    break;  /* not our protocol; hang up */
    }
    mcpcad_session_destroy(s);
}


/* Accept loop over a local IPC socket, one client at a time.
 *
 * libpkg owns the address format and the listener, as it does for
 * 'mcp_listen ipc' in MGED, so this file holds no AF_UNIX specifics. */
static int
serve_ipc(struct ged *gedp, const char *path)
{
    struct bu_vls addr = BU_VLS_INIT_ZERO;
    pkg_listener_t *lp;

    bu_vls_printf(&addr, "unix:%s", path);

    /* A socket left behind by a killed run blocks bind().  Removing one
     * blind would silence a live server instead, so connect first. */
    if (bu_file_exists(path, NULL)) {
	struct pkg_conn *probe = pkg_connect_addr(bu_vls_cstr(&addr), NULL, NULL);
	if (probe && probe != PKC_ERROR) {
	    pkg_close(probe);
	    bu_log("ERROR: something is already listening on %s\n", path);
	    bu_vls_free(&addr);
	    return 1;
	}
	bu_file_delete(path);
    }

    lp = pkg_listen(bu_vls_cstr(&addr), NULL, 4, NULL);
    bu_vls_free(&addr);
    if (!lp) {
	bu_log("ERROR: cannot create an IPC socket at %s\n", path);
	return 1;
    }

    printf("mcpcad scaffold listening on %s\n", path);
    fflush(stdout);

    for (;;) {
	struct pkg_conn *pc = pkg_accept(lp, NULL, NULL, 0);
	if (!pc || pc == PKC_ERROR)
	    continue;

	printf("client connected\n");
	fflush(stdout);

	serve_client(gedp, pkg_get_read_fd(pc));

	pkg_close(pc);          /* owns and closes the descriptor */
	printf("client disconnected\n");
	fflush(stdout);
    }
    /* not reached; Ctrl-C terminates */
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

    /* A path rather than a port means local IPC.  A port number cannot
     * begin with '/', so the forms are unambiguous. */
    if (argc > 1 && argv[1][0] == '/') {
	printf("database: %s\n", dbpath);
	return serve_ipc(gedp, argv[1]);
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

	if (cfd < 0)
	    continue;

	printf("client connected\n");
	fflush(stdout);

	serve_client(gedp, cfd);

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
