/*                        F B S E R V . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file fbserv/fbserv.c
 *
 * Remote libfb server (originally rfbd).
 *
 * There are three ways this program can be run: Inetd Daemon - every
 * PKG connection invokes a new copy of us, courtesy of inetd.  We
 * process a single frame buffer open/process/close cycle and then
 * exit.  A full installation includes setting up inetd and
 * /etc/services to start one of these, with these entries:
 *
 * remotefb stream tcp nowait nobody   /usr/brlcad/bin/fbserv fbserv
 * remotefb 5558/tcp # remote frame buffer
 *
 * Stand-Alone Daemon - once started we run forever, forking a copy of
 * ourselves for each new connection.  Each child is essentially like
 * above, i.e. one open/process/close cycle.  Useful for running a
 * daemon on a totally "unmodified" system, or when inetd is not
 * available.  A child process is necessary because different
 * framebuffers may be specified in each open.
 *
 * Single-Frame-Buffer Server - we open a particular frame buffer at
 * invocation time and leave it open.  We will accept multiple
 * connections for this frame buffer.  Frame buffer open and close
 * requests are effectively ignored.  Major purpose is to create
 * "reattachable" frame buffers when using libfb on a window system.
 * In this case there is no hardware to preserve "state" information
 * (image data, color maps, etc.).  By leaving the frame buffer open,
 * the daemon keeps this state in memory.  Requests can be interleaved
 * from different clients.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_SYSLOG_H
#  include <syslog.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>		/* For struct timeval */
#endif

#include "bresource.h"
#include "bnetwork.h"
#include "bsocket.h"
#include "bio.h"

#include "bu/app.h"
#include "bu/color.h"
#include "bu/exit.h"
#include "bu/getopt.h"
#include "bu/interrupt.h"
/* bu/ipc.h removed - transport handled by libpkg */
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/snooze.h"
#include "bu/str.h"
#include "vmath.h"
#include "dm.h"
#include "pkg.h"

/* Enable token generation for the server (fbserv.c generates and forwards
 * the token; server.c does the per-connection verification). */
#define FBSERV_AUTH_SERVER
#include "./auth.h"

/* Enable TLS server-side functions */
#define FBSERV_TLS_IMPL
#include "./tls_wrap.h"


fd_set select_list;			/* master copy */
int max_fd;


static int use_syslog;	/* error messages to stderr if 0 */

static char *framebuffer = NULL;	/* frame buffer name */
static int width = 0;		/* use default size */
static int height = 0;
static int port = 0;
static int port_set = 0;		/* !0 if user supplied port num */
static int once_only = 0;
static pkg_listener_t *netlistener = NULL; /* TCP listener */
static int netfd; /* fd for select(), from pkg_get_listener_fd() */

/*
 * Session authentication token.  Generated once at startup.
 * Printed to stderr so that the invoking script/user can pass it to
 * clients via the FBSERV_TOKEN environment variable.
 * An empty string means no authentication is required.
 */
static char session_token[FBSERV_AUTH_TOKEN_LEN + 1] = {0};

/*
 * When non-zero, ALL connecting clients must supply a valid token via
 * MSG_FBAUTH before sending MSG_FBOPEN.  When zero (the default),
 * clients without a token are still accepted.
 */
static int require_auth = 0;

/*
 * When non-zero, attempt to set up TLS after accepting each TCP connection.
 */
static int use_tls = 0;

/*
 * When non-NULL (set by -I flag), connect to the inherited IPC address
 * instead of binding a TCP listen socket.  This mirrors the -I mode in
 * rtsrv and allows a parent process to bypass TCP for local connections.
 */
static const char *ipc_addr_flag = NULL;

/*
 * Per-client authentication state (parallel to clients[]).
 * 1 = client has sent a valid MSG_FBAUTH, 0 = not yet authenticated.
 */
static int clients_auth[MAX_CLIENTS];

/*
 * Per-client deferred-drop flag (parallel to clients[]).
 * Set to 1 inside a pkg_process dispatch handler to request that
 * main_loop drop this client AFTER pkg_process returns.  We must not
 * call pkg_close() (which frees the pkg_conn) while pkg_process is
 * still walking its internal message loop over the same pointer.
 */
static int clients_pending_drop[MAX_CLIENTS];

/*
 * TLS server context.  Initialized at startup when HAVE_OPENSSL_SSL_H
 * is defined and -T flag is given (or FBSERV_TLS env var is set).
 */
#ifdef HAVE_OPENSSL_SSL_H
static SSL_CTX *fbserv_ssl_ctx = NULL;
#endif


#define MAX_CLIENTS 32
struct pkg_conn *clients[MAX_CLIENTS];

int verbose = 0;

/* from server.c */
extern struct pkg_switch pkg_switch[];
extern struct fb *fb_server_fbp;
extern fd_set *fb_server_select_list;
extern int *fb_server_max_fd;
extern int fb_server_got_fb_free;       /* !0 => we have received an fb_free */
extern int fb_server_refuse_fb_free;    /* !0 => don't accept fb_free() */
extern int fb_server_retain_on_close;   /* !0 => we are holding a reusable FB open */

/* auth state accessors for server.c handlers */
int fbserv_client_auth_ok(int idx)   { return clients_auth[idx]; }
int fbserv_require_auth(void)        { return require_auth; }
const char *fbserv_session_token(void) { return session_token; }
void fbserv_set_client_auth(int idx, int val) { clients_auth[idx] = val; }

/* Request that main_loop drop client idx after the current pkg_process
 * dispatch returns.  Safe to call from inside a handler because it does
 * NOT free the pkg_conn — pkg_process still needs that pointer. */
void fbserv_request_drop(int idx) {
    if (idx >= 0 && idx < MAX_CLIENTS)
	clients_pending_drop[idx] = 1;
}

/**
 * Find the clients[] slot index for the given connection.
 * Returns -1 if not found.
 */
int
fbserv_conn_idx(struct pkg_conn *pcp)
{
    int i;
    for (i = 0; i < MAX_CLIENTS; i++) {
	if (clients[i] == pcp)
	    return i;
    }
    return -1;
}

/* Hidden args: -p<port_num> -F<frame_buffer> */
static char usage[] = "\
Usage: fbserv port_num\n\
	  (for a stand-alone daemon)\n\
   or  fbserv [-v] [-T] [-A] [-{sS} squaresize]\n\
	  [-{wW} width] [-{nN} height] -p port_num -F frame_buffer\n\
	  (for a single-frame-buffer server)\n\
          (if '-p' and '-F' are both omitted, port_num and frame_buffer\n\
           must appear in that order)\n\
   or  fbserv [-v] [-F frame_buffer] -I ipc_addr\n\
	  (for an IPC inherited-fd mode; -p is ignored)\n\
  -T  enable TLS encryption (requires OpenSSL build)\n\
  -A  strict auth: reject clients that do not send MSG_FBAUTH\n\
  -I  connect to the IPC channel at ipc_addr instead of binding TCP\n\
\n\
Token authentication (works with or without TLS):\n\
  Set FBSERV_TOKEN=<64-hex-chars> before starting fbserv to use a\n\
  pre-known token instead of having one auto-generated.  The same\n\
  token must be set in FBSERV_TOKEN for every client (e.g. rt, pix-fb)\n\
  that connects to this server.\n\
  If FBSERV_TOKEN is NOT set, fbserv generates a fresh token at\n\
  startup and prints it to stderr.\n\
";

int
get_args(int argc, char **argv)
{
    int c;
    int enable_tls = 0;

    while ((c = bu_getopt(argc, argv, "vTAI:F:s:w:n:S:W:N:p:h?")) != -1) {
	switch (c) {
	    case 'v':
		verbose = 1;
		break;
	    case 'T':
		enable_tls = 1;
		break;
	    case 'A':
		require_auth = 1;
		break;
	    case 'I':
		ipc_addr_flag = bu_optarg;
		port_set = 1;   /* suppress "no port" usage error */
		break;
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    case 's':
	    case 'S':
		height = width = atoi(bu_optarg);
		break;
	    case 'w':
	    case 'W':
		width = atoi(bu_optarg);
		break;
	    case 'n':
	    case 'N':
		height = atoi(bu_optarg);
		break;
	    case 'p':
		port = atoi(bu_optarg);
		port_set = 1;
		break;

	    default:		/* '?' */
		return 0;
	}
    }
    /* If no "-p port", port comes from 1st extra */
    if ((bu_optind < argc) && (port_set == 0)) {
	port = atoi(argv[bu_optind++]);
	port_set = 1;
    }
    /* If no "-F framebuffer", fb comes from 2nd extra */
    if ((bu_optind < argc) && (framebuffer == NULL)) {
	framebuffer = argv[bu_optind++];
    }
    if (argc > bu_optind)
	return 0;	/* print usage */

    use_tls = enable_tls;
    return 1;		/* OK */
}


/*
 * Determine if a file descriptor corresponds to an open socket.
 * Used to detect when we are started from INETD which gives us an
 * open socket connection on fd 0.
 */
int
is_socket(int fd)
{
    struct sockaddr saddr;
    socklen_t namelen = 0;

    if (getsockname(fd, &saddr, &namelen) == 0) {
	return 1;
    }
    return 0;
}

/*
 * Communication error.  An error occurred on the PKG link.
 * It may be local, or it may be between us and the client we are serving.
 * We send a copy to syslog or stderr.
 * Don't send one down the wire, this can cause loops.
 */
static void
communications_error(const char *str)
{
#if defined(HAVE_SYSLOG_H)
    if (use_syslog) {
	syslog(LOG_ERR, "%s", str);
    } else {
	fprintf(stderr, "%s", str);
    }
#else
    fprintf(stderr, "%s", str);
#endif
    if (verbose) {
	fprintf(stderr, "%s", str);
    }
}


static void
fbserv_setup_socket(int fd)
{
    int on = 1;

#if defined(SO_KEEPALIVE)
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0) {
#  ifdef HAVE_SYSLOG_H
	syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %s", strerror(errno));
#  endif
    }
#endif
#if defined(SO_RCVBUF)
    /* try to set our buffers up larger */
    {
	int m = -1, n = -1;
	int val;
	int size;

	for (size = 256; size > 16; size /= 2) {
	    val = size * 1024;
	    m = setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
			   (char *)&val, sizeof(val));
	    val = size * 1024;
	    n = setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
			   (char *)&val, sizeof(val));
	    if (m >= 0 && n >= 0) break;
	}

	if (m < 0 || n < 0)
	    perror("setsockopt");
    }
#endif
}

void
fbserv_drop_client(int sub)
{
    int fd;

    if (clients[sub] == PKC_NULL)
	return;

    fd = pkg_get_read_fd(clients[sub]);

    FD_CLR(fd, &select_list);
    pkg_close(clients[sub]);
    clients[sub] = PKC_NULL;
    clients_auth[sub] = 0;
    clients_pending_drop[sub] = 0;
}

static void
fbserv_new_client(struct pkg_conn *pcp)
{
    int i;

    if (pcp == PKC_ERROR)
	return;

    for (i = MAX_CLIENTS-1; i >= 0; i--) {
	if (clients[i] != NULL) continue;
	/* Found an available slot */
	clients[i] = pcp;
	clients_auth[i] = 0;  /* not yet authenticated */
	clients_pending_drop[i] = 0;
	{
	    int rfd = pkg_get_read_fd(pcp);
	    FD_SET(rfd, &select_list);
	    V_MAX(max_fd, rfd);
	    fbserv_setup_socket(rfd);
	}

#ifdef HAVE_OPENSSL_SSL_H
	/* Optional TLS: perform server-side handshake before any PKG
	 * messages are exchanged. */
	if (use_tls && fbserv_ssl_ctx) {
	    if (fbserv_tls_accept(fbserv_ssl_ctx, pcp) == FBSERV_TLS_OK) {
		if (verbose)
		    fprintf(stderr, "fbserv: TLS established with client %d\n", i);
	    } else {
		fprintf(stderr, "fbserv: TLS handshake failed for client %d; "
			"dropping connection\n", i);
		FD_CLR(pkg_get_read_fd(pcp), &select_list);
		pkg_close(pcp);
		clients[i] = PKC_NULL;
		clients_auth[i] = 0;
		return;
	    }
	}
#endif

	return;
    }
    fprintf(stderr, "fbserv: too many clients\n");
    pkg_close(pcp);
}



/*
 * Loop forever handling clients as they come and go.
 * Access to the framebuffer may be interleaved, if the user
 * wants it that way.
 */
static void
main_loop(void)
{
    int nopens = 0;
    int ncloses = 0;

    while (!fb_server_got_fb_free) {
	long refresh_rate = 60000000; /* old default */
	long usec_to_sec = 1000000;
	fd_set infds;
	struct timeval tv;
	int i;

	if (fb_server_fbp) {
	    if (fb_poll_rate(fb_server_fbp) > 0)
		refresh_rate = fb_poll_rate(fb_server_fbp);
	}

	infds = select_list;	/* struct copy */

	tv.tv_sec = refresh_rate / usec_to_sec;
	tv.tv_usec = refresh_rate % usec_to_sec;
	if ((select(max_fd+1, &infds, (fd_set *)0, (fd_set *)0, (struct timeval *)&tv) == 0)) {
	    /* Process fb events while waiting for client */
	    /*printf("select timeout waiting for client\n");*/
	    if (fb_server_fbp) {
		if (fb_poll(fb_server_fbp)) {
		    return;
		}
	    }
	    continue;
	}
	/* Handle any events from the framebuffer */
	if (fb_is_set_fd(fb_server_fbp, &infds)) {
	    fb_poll(fb_server_fbp);
	}

	/* Accept any new client connections */
	if (netfd > 0 && FD_ISSET(netfd, &infds)) {
	    fbserv_new_client(pkg_accept(netlistener, pkg_switch, communications_error, 0));
	    nopens++;
	}

	/* Process arrivals from existing clients */
	/* First, pull the data out of the kernel buffers */
	for (i = MAX_CLIENTS-1; i >= 0; i--) {
	    if (clients[i] == NULL) continue;
	    if (pkg_process(clients[i]) < 0) {
		fprintf(stderr, "pkg_process error encountered (1)\n");
	    }
	    /* A handler (e.g. token-mismatch auth) may have requested a
	     * deferred drop.  Act on it now that pkg_process has returned
	     * and is no longer holding a reference to clients[i]. */
	    if (clients_pending_drop[i]) {
		fbserv_drop_client(i);
		ncloses++;
		continue;
	    }
	    if (clients[i] == NULL) continue;
	    if (! FD_ISSET(pkg_get_read_fd(clients[i]), &infds)) continue;
	    if (pkg_suckin(clients[i]) <= 0) {
		/* Probably EOF */
		fbserv_drop_client(i);
		ncloses++;
		continue;
	    }
	}
	/* Second, process all the finished ones that we just got */
	for (i = MAX_CLIENTS-1; i >= 0; i--) {
	    if (clients[i] == NULL) continue;
	    if (pkg_process(clients[i]) < 0) {
		fprintf(stderr, "pkg_process error encountered (2)\n");
	    }
	    /* Deferred drop from second-pass handler */
	    if (clients_pending_drop[i]) {
		fbserv_drop_client(i);
		ncloses++;
	    }
	}
	if (once_only && nopens > 1 && ncloses > 1)
	    return;
    }
}


static void
init_syslog(void)
{
#ifdef HAVE_SYSLOG_H
    use_syslog = 1;

#  if defined(LOG_NOWAIT) && defined(LOG_DAEMON)
    openlog("fbserv", LOG_PID|LOG_NOWAIT, LOG_DAEMON);	/* 4.3 style */
#  else
    openlog("fbserv", LOG_PID);				/* 4.2 style */
#  endif
#endif
}


int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);

#define PORTSZ 32
    char portname[PORTSZ];

    max_fd = 0;

    /* No disk files on remote machine */
    _fb_disk_enable = 0;
    memset((void *)clients, 0, sizeof(struct pkg_conn *) * MAX_CLIENTS);
    memset(clients_auth, 0, sizeof(clients_auth));

#ifdef SIGPIPE
    (void)signal(SIGPIPE, SIG_IGN);
#endif

    FD_ZERO(&select_list);
    fb_server_select_list = &select_list;
    fb_server_max_fd = &max_fd;

#ifndef _WIN32
    /*
     * Inetd Daemon.
     * Check to see if we were invoked by /etc/inetd.  If so
     * we will have an open network socket on fd=0.  Become
     * a Transient PKG server if this is so.
     */
    netfd = 0;
    if (is_socket(netfd)) {
	init_syslog();
	fbserv_new_client(pkg_adopt_stdio(pkg_switch, communications_error));
	max_fd = 8;
	once_only = 1;
	main_loop();
	return 0;
    }
#endif

    /* for now, make them set a port_num, for usage message */
    if (!get_args(argc, argv) || !port_set) {
	(void)fputs(usage, stderr);
	return 1;
    }

    /* Phase 7: IPC inherited-fd mode (-I <ipc_addr>).
     * When the parent passes -I, connect to the IPC channel at the given
     * address (pipe fd pair, socketpair, or TCP loopback port), register it
     * as a single client, and run the main loop in once_only mode.
     * This is analogous to the -I flag in rtsrv and allows the parent to
     * avoid TCP port binding entirely for local fbserv instances.            */
    if (ipc_addr_flag) {
	struct pkg_conn *pcp;
	int pkgr;

	if (framebuffer != NULL) {
	    if ((fb_server_fbp = fb_open(framebuffer, width, height)) == FB_NULL)
		bu_exit(1, NULL);
	    max_fd = fb_set_fd(fb_server_fbp, &select_list);
	    fb_server_retain_on_close = 1;
	}

	pcp = pkg_connect_addr(ipc_addr_flag, pkg_switch, communications_error);
	if (pcp == PKC_ERROR || pcp == PKC_NULL) {
	    fprintf(stderr, "fbserv: pkg_connect_addr('%s') failed\n",
		    ipc_addr_flag);
	    return 1;
	}

	/* IPC service loop: pkg_suckin/pkg_process without select().
	 * We bypass fbserv_new_client() + main_loop() because those use
	 * FD_SET on a transport read fd, which is not always meaningful
	 * for split-fd transports (pkg_get_read_fd() returns the read end
	 * but the select-based main_loop is geared for bidirectional fds).
	 * pkg_suckin/pkg_process work transport-agnostically through the
	 * pkg_conn so the simple poll loop below is correct here.            */
	do {
	    pkgr = pkg_process(pcp);
	    if (pkgr < 0) break;
	    pkgr = pkg_suckin(pcp);
	    if (pkgr <= 0) break;
	    pkgr = pkg_process(pcp);
	} while (pkgr >= 0);

	pkg_close(pcp);
	if (fb_server_fbp != FB_NULL)
	    fb_close(fb_server_fbp);
	return 0;
    }

    /* Session authentication token.
     * If FBSERV_TOKEN is already set in the environment, use that value
     * so that the invoking script/app can pre-supply a known token and
     * give the same value to client programs (e.g. rt, pix-fb).  This
     * works regardless of whether TLS is enabled — the token provides
     * session isolation even on plain TCP connections.
     * If FBSERV_TOKEN is not set, generate a fresh random token. */
    {
	const char *env_token = getenv("FBSERV_TOKEN");
	if (env_token && strlen(env_token) == FBSERV_AUTH_TOKEN_LEN) {
	    bu_strlcpy(session_token, env_token, sizeof(session_token));
	    fprintf(stderr, "fbserv: Using pre-supplied session token from FBSERV_TOKEN\n");
	} else {
	    fbserv_generate_token(session_token);
	    fprintf(stderr, "fbserv: Session token: %s\n", session_token);
	    fprintf(stderr, "fbserv: Set FBSERV_TOKEN=%s in client environment\n",
		    session_token);
	}
    }

#ifdef HAVE_OPENSSL_SSL_H
    /* Optional TLS.  Check -T flag or FBSERV_TLS environment variable. */
    if (use_tls || getenv("FBSERV_TLS")) {
	const char *certfile = getenv("FBSERV_TLS_CERT");
	const char *keyfile  = getenv("FBSERV_TLS_KEY");
	fbserv_ssl_ctx = fbserv_tls_server_ctx(certfile, keyfile);
	if (fbserv_ssl_ctx) {
	    fprintf(stderr, "fbserv: TLS enabled (%s)\n",
		    (certfile && keyfile) ? "custom cert" : "self-signed cert");
	} else {
	    fprintf(stderr, "fbserv: WARNING: TLS context creation failed; "
		    "continuing without TLS\n");
	}
    }
#else
    if (use_tls)
	fprintf(stderr, "fbserv: WARNING: -T flag given but TLS support was "
		"not compiled in (no OpenSSL)\n");
#endif

    /* Single-Frame-Buffer Server */
    if (framebuffer != NULL) {
	fb_server_retain_on_close = 1;	/* don't ever close the frame buffer */

	/* open a frame buffer */
	if ((fb_server_fbp = fb_open(framebuffer, width, height)) == FB_NULL)
	    bu_exit(1, NULL);
	max_fd = fb_set_fd(fb_server_fbp, &select_list);

	/* check/default port */
	if (port_set) {
	    if (port < 1024)
		port += 5559;
	}
	snprintf(portname, PORTSZ, "%d", port);

	/*
	 * Hang an unending listen for PKG connections
	 */
	netlistener = pkg_listen(portname, NULL, 0, communications_error);
	if (!netlistener)
	    bu_exit(-1, NULL);
	netfd = pkg_get_listener_fd(netlistener);
	FD_SET(netfd, &select_list);
	V_MAX(max_fd, netfd);

	main_loop();
	return 0;
    }

#ifndef _WIN32
    /*
     * Stand-Alone Daemon
     */
    /* check/default port */
    if (port_set) {
	if (port < 1024)
	    port += 5559;
	sprintf(portname, "%d", port);
    } else {
	snprintf(portname, PORTSZ, "%s", "remotefb");
    }

    init_syslog();
    while ((netlistener = pkg_listen(portname, NULL, 0, communications_error)) == NULL) {
	static int error_count=0;
	bu_snooze(BU_SEC2USEC(1));
	if (error_count++ < 60) {
	    continue;
	}
	communications_error("Unable to start the stand-alone framebuffer daemon after 60 seconds, giving up.");
	return 1;
    }
    netfd = pkg_get_listener_fd(netlistener);

    while (1) {
	int fbstat;
	struct pkg_conn *pcp;

	pcp = pkg_accept(netlistener, pkg_switch, communications_error, 0);
	if (pcp == PKC_ERROR)
	    break;		/* continue is unlikely to work */

	if (fork() == 0) {
	    /* 1st level child process */
	    pkg_listener_close(netlistener);	/* Child is not listener */

	    /* Create 2nd level child process, "double detach" */
	    if (fork() == 0) {
		/* 2nd level child -- start work! */
		fbserv_new_client(pcp);
		once_only = 1;
		main_loop();
		return 0;
	    } else {
		/* 1st level child -- vanish */
		return 1;
	    }
	} else {
	    /* Parent: lingering server daemon */
	    pkg_close(pcp);	/* Daemon is not the server */
	    /* Collect status from 1st level child */
	    (void)wait(&fbstat);
	}
    }
#endif  /* _WIN32 */

    return 2;
}

#ifndef _WIN32
/*
 * Handles error or log messages from the frame buffer library.
 * We route these back to all clients in an ERROR packet.  Note that
 * this is a replacement for the default fb_log function in libfb
 * (which just writes to stderr).
 *
 * Log an FB library event, when _doprnt() is not available.
 * This version should work on practically any machine, but
 * it serves to highlight the grossness of the varargs package
 * requiring the size of a parameter to be known at compile time.
 */
void
fb_log(const char *fmt, ...)
{
    va_list ap;
    char outbuf[BU_PAGE_SIZE];			/* final output string */
    int want;
    int i;
    int nsent = 0;

    va_start(ap, fmt);
    (void)vsprintf(outbuf, fmt, ap);
    va_end(ap);

    want = strlen(outbuf)+1;
    for (i = MAX_CLIENTS-1; i >= 0; i--) {
	if (clients[i] == NULL) continue;
	if (pkg_send(MSG_ERROR, outbuf, want, clients[i]) != want) {
	    communications_error("pkg_send error in fb_log, message was:\n");
	    communications_error(outbuf);
	} else {
	    nsent++;
	}
    }
    if (nsent == 0 || verbose) {
	/* No PKG connection open yet! */
	fputs(outbuf, stderr);
	fflush(stderr);
    }
}


#endif /* _WIN32 */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
