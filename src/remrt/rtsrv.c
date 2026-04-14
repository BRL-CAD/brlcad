/*                         R T S R V . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2026 United States Government as represented by
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
/** @file rtsrv.c
 *
 * Remote Ray Tracing service program, using RT library.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#include "bio.h"
#include "bresource.h"
#include "bsocket.h"

#include "bu/app.h"
#include "bu/ipc.h"
#include "bu/str.h"
#include "bu/process.h"
#include "bu/snooze.h"
#include "bu/vls.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "optical/debug.h"
#include "pkg.h"
#include "dm.h"
#include "icv.h"

#include "../rt/rtuif.h"
#include "../rt/ext.h"
#include "./protocol.h"
#include "./auth.h"
/* Enable TLS client-side functions */
#define REMRT_TLS_IMPL
#include "./tls_wrap.h"


struct bu_list WorkHead;

struct pkg_queue {
    struct bu_list l;
    unsigned short type;
    char *buf;
};


/***** Variables shared with viewing model *** */
struct fb *fbp = FB_NULL;	/* Framebuffer handle */
FILE *outfp = NULL;	/* optional pixel output file */

int srv_startpix = 0;	/* offset for view_pixel */
int srv_scanlen = REMRT_MAX_PIXELS;	/* max assignment */
unsigned char *scanbuf = NULL;
/***** end of sharing with viewing model *****/

extern int grid_setup(struct bu_vls *err);
extern void worker(int, void *);
extern void application_init(void);
extern void light_cleanup(void); /* from liboptical/sh_light.c */

/***** variables shared with worker() ******/
struct application APP;
int report_progress = 0;	/* !0 = user wants progress report */
/***** end variables shared with worker() *****/

/* Variables shared elsewhere */
extern fastf_t rt_dist_tol;	/* Value for rti_tol.dist */
extern fastf_t rt_perp_tol;	/* Value for rti_tol.perp */
static char idbuf[132] = {0};		/* First ID record info */

/* State flags */
static int seen_dirbuild = 0;
static int seen_gettrees = 0;
static int seen_matrix = 0;

/* Set to 1 when the remrt connection is lost mid-render so the main loop
 * can break cleanly rather than calling bu_exit() from a hook function.  */
static volatile int rtsrv_connection_lost = 0;

/* Frame number for which grid vectors and lighting have been initialized.
 * Set to -1 by prepare() and ph_matrix() to force re-setup on the next
 * ph_lines() call (mimicking rt's do_frame() order: do_prep -> grid_setup
 * -> view_2init -> do_run). */
static int last_setup_frame = -1;

static char *title_file = NULL;
static char *title_obj = NULL;	/* name of file and first object */

static size_t avail_cpus = 0;	/* # of cpus avail on this system */

/* store program parameters in case of restart */
static int original_argc;
static char **original_argv;

struct icv_image *bif = NULL;


/*
 * Package Handlers.
 */
void ph_unexp(struct pkg_conn *pc, char *buf);   /* foobar message handler */
void ph_enqueue(struct pkg_conn *pc, char *buf); /* Adds message to linked list */
void ph_dirbuild(struct pkg_conn *pc, char *buf);
void ph_gettrees(struct pkg_conn *pc, char *buf);
void ph_matrix(struct pkg_conn *pc, char *buf);
void ph_options(struct pkg_conn *pc, char *buf);
void ph_lines(struct pkg_conn *pc, char *buf);
void ph_end(struct pkg_conn *pc, char *buf);
void ph_restart(struct pkg_conn *pc, char *buf);
void ph_loglvl(struct pkg_conn *pc, char *buf);
void ph_cd(struct pkg_conn *pc, char *buf);

void prepare(void);

struct pkg_switch pkgswitch[] = {
    { MSG_DIRBUILD,	ph_dirbuild,	"DirBuild", NULL },
    { MSG_GETTREES,	ph_enqueue,	"Get Trees", NULL },
    { MSG_MATRIX,	ph_enqueue,	"Set Matrix", NULL },
    { MSG_OPTIONS,	ph_enqueue,	"Options", NULL },
    { MSG_LINES,	ph_enqueue,	"Compute lines", NULL },
    { MSG_END,		ph_end,		"End", NULL },
    { MSG_PRINT,	ph_unexp,	"Log Message", NULL },
    { MSG_LOGLVL,	ph_loglvl,	"Change log level", NULL },
    { MSG_RESTART,	ph_restart,	"Restart", NULL },
    { MSG_CD,		ph_cd,		"Change Dir", NULL },
    { 0,		0,		NULL, NULL }
};


struct pkg_conn *pcsrv;	/* PKG connection to server */
char *control_host;	/* name of host running controller */
char *tcp_port;		/* TCP port on control_host */
int debug = 0;		/* 0=off, 1=debug, 2=verbose */

/*
 * Session authentication token, supplied via "-S <token>" on the
 * command line when remrt auto-launches this process via SSH.
 * An empty string means no authentication (passive/manual start).
 */
static char srv_auth_token[REMRT_AUTH_TOKEN_LEN + 1] = {0};

char srv_usage[] = "Usage: rtsrv [-d] [-S token] [-I ipc-addr] control-host tcp-port [cmd]\n"
		   "       rtsrv [-d] [-S token]  -I ipc-addr\n";

/* forward declarations for log/bomb hook helpers defined later in this file */
static int rtsrv_log_hook(void *clientdata, void *str);
static int rtsrv_bomb_hook(void *clientdata, void *str);
static void rtsrv_install_log_hook(void);


int
main(int argc, char **argv)
{
    int n;
    const char *ipc_addr = NULL;   /* set by -I flag; enables IPC mode */

    bu_setprogname(argv[0]);

    if (argc < 2) {
	fprintf(stderr, "%s", srv_usage);
	return 1;
    }
    original_argc = argc;
    original_argv = bu_argv_dup(argc, (const char **)argv);
    while (argc > 1 && argv[1][0] == '-') {
	if (BU_STR_EQUAL(argv[1], "-d")) {
	    debug++;
	} else if (BU_STR_EQUAL(argv[1], "-x")) {
	    sscanf(argv[2], "%x", (unsigned int *)&rt_debug);
	    argc--; argv++;
	} else if (BU_STR_EQUAL(argv[1], "-X")) {
	    sscanf(argv[2], "%x", (unsigned int *)&optical_debug);
	    argc--; argv++;
	} else if (BU_STR_EQUAL(argv[1], "-S")) {
	    /* Session authentication token from remrt */
	    if (argc < 3) {
		fprintf(stderr, "%s", srv_usage);
		return 3;
	    }
	    bu_strlcpy(srv_auth_token, argv[2],
		       sizeof(srv_auth_token));
	    argc--; argv++;
	} else if (BU_STR_EQUAL(argv[1], "-I")) {
	    /* IPC connection address supplied by remrt via -I <addr>.
	     * When present, no host/port positional args are needed.  */
	    if (argc < 3) {
		fprintf(stderr, "%s", srv_usage);
		return 3;
	    }
	    ipc_addr = argv[2];
	    argc--; argv++;
	} else {
	    fprintf(stderr, "%s", srv_usage);
	    return 3;
	}
	argc--; argv++;
    }

    /* Determine IPC mode: -I flag takes explicit precedence; fall back to
     * the BU_IPC_ADDR environment variable (BU_IPC_ADDR_ENVVAR) set by
     * the parent before fork() (see add_host_local() in remrt.c).         */
    {
	bu_ipc_chan_t *ch = NULL;
	if (ipc_addr) {
	    ch = bu_ipc_connect(ipc_addr);
	    if (!ch) {
		fprintf(stderr, "rtsrv: bu_ipc_connect(%s) failed\n", ipc_addr);
		return 1;
	    }
	} else {
	    ch = bu_ipc_connect_env();
	}

	if (ch) {
	    /* IPC mode: remrt created a socketpair, moved the child-end fd
	     * above the close(3..19) sweep in bu_process_create(), and
	     * advertised it via -I or BU_IPC_ADDR (BU_IPC_ADDR_ENVVAR).
	     * bu_ipc_connect (or bu_ipc_connect_env) wraps the already-
	     * inherited fd.  Wrap it into a pkg_conn so the rest of the code
	     * is transport-agnostic.  No host/port positional args consumed. */
	    pcsrv = pkg_open_fds(bu_ipc_fileno(ch),
				 bu_ipc_fileno_write(ch),
				 pkgswitch, NULL);
	    bu_ipc_detach(ch);   /* pkg_conn owns the fds now */
	    if (pcsrv == PKC_ERROR || pcsrv == PKC_NULL) {
		fprintf(stderr, "rtsrv: pkg_open_fds() failed in IPC mode\n");
		return 1;
	    }
	    if (debug)
		fprintf(stderr, "rtsrv: IPC mode active (addr=%s)\n",
			ipc_addr ? ipc_addr : getenv(BU_IPC_ADDR_ENVVAR));
	} else {
	    /* Normal TCP mode */
	    if (argc != 3 && argc != 4) {
		fprintf(stderr, "%s", srv_usage);
		return 2;
	    }

	    control_host = argv[1];
	    tcp_port = argv[2];

	    /* Note that the LIBPKG error logger can not be
	     * "bu_log", as that can cause bu_log to be entered recursively.
	     * Given the special version of bu_log in use here,
	     * that will result in a deadlock in bu_semaphore_acquire(res_syscall)!
	     * libpkg will default to stderr via pkg_errlog(), which is fine.
	     */
	    pcsrv = pkg_open(control_host, tcp_port, "tcp", "", "", pkgswitch, NULL);
	    if (pcsrv == PKC_ERROR) {
		fprintf(stderr, "rtsrv: unable to contact %s, port %s\n",
			control_host, tcp_port);
		return 1;
	    }
	}
    }

#ifdef HAVE_OPENSSL_SSL_H
    /* Attempt TLS handshake with the remrt dispatcher.  If remrt was
     * built with TLS support it will do SSL_accept() on its side;
     * if not (or the handshake fails) we continue as plaintext for
     * backward compatibility.
     * Skip TLS in IPC mode — the bu_ipc transport runs on the same
     * machine and the shared-memory or socketpair channel does not
     * need encryption.                                               */
    if (control_host) {
	SSL_CTX *tls_ctx = remrt_tls_client_ctx();
	if (tls_ctx) {
	    if (remrt_tls_connect(tls_ctx, pcsrv) == REMRT_TLS_OK) {
		if (debug)
		    fprintf(stderr, "rtsrv: TLS established with %s\n",
			    control_host);
	    } else {
		fprintf(stderr, "rtsrv: TLS handshake failed; "
			"continuing as plaintext\n");
	    }
	    SSL_CTX_free(tls_ctx);
	}
    }
#endif

    /* Redirect bu_log() and bu_bomb() through the package connection so
     * all log output is forwarded to the remrt dispatcher instead of
     * going to stderr.  Must be done after pcsrv is open. */
    rtsrv_install_log_hook();

#ifdef SIGPIPE
    /* Ignore SIGPIPE so that a broken remrt connection causes pkg_send()
     * to return EPIPE rather than killing this process with a signal.
     * The main loop already handles negative pkg_send returns gracefully. */
    (void)signal(SIGPIPE, SIG_IGN);
#endif

    if (argc == 4) {
	/* Slip one command to dispatcher */
	(void)pkg_send(MSG_CMD, argv[3], strlen(argv[3])+1, pcsrv);

	/* Prevent chasing the package with an immediate TCP close */
	bu_snooze(BU_SEC2USEC(1));

	pkg_close(pcsrv);
	return 0;
    }

#ifdef SO_SNDBUF
    /* increase the default send buffer size to 32k since we're
     * sending pixels more than likely.
     * Skip for pipe transport where pkc_fd == PKG_STDIO_MODE (-3) — setsockopt
     * on a non-socket fd would fail with EBADF.
     */
    if (pcsrv->pkc_fd != PKG_STDIO_MODE) {
	int val = 32767;
	n = setsockopt(pcsrv->pkc_fd, SOL_SOCKET, SO_SNDBUF, (const void *)&val, sizeof(val));
	if (n < 0)
	    perror("setsockopt: SO_SNDBUF");
    }
#endif

    if (!debug) {
	FILE *fp;

	/* DAEMONIZE */

#ifdef HAVE_FORK
	{
	    int i;

	    /* Get a fresh process */
	    i = fork();
	    if (i < 0)
		perror("fork");
	    else if (i)
		return 0;
	}

	/* Go into our own process group */
	n = bu_pid();
#  ifdef HAVE_SETPGID
	if (setpgid(n, n) < 0)
	    perror("setpgid");
#  else
	/* SysV uses setpgrp with no args and it can't fail,
	 * obsoleted by setpgid.
	 */
	setpgrp();
#  endif

	/* Create our own session */
#  ifdef HAVE_SETSID
	if (setsid() < 0)
	    perror("setsid");
#  endif
#endif /* HAVE_FORK */

	/* TODO: need to change directory (e.g., to TEMP) so we don't
	 * make the initial working directory unlinkable on some OS.
	 * However, for rtsrv, we first need a more robust way for
	 * getting geometry from remrt.  See gtransfer.
	 */

	/* Drop to the lowest sensible priority. */
	bu_nice_set(19);

	/* Close off the world */

#ifdef HAVE_WINDOWS_H
#  define DEVNULL "\\\\.\\NUL"
#else
#  define DEVNULL "/dev/null"
#endif

	fp = freopen(DEVNULL, "r", stdin);
	if (fp == NULL)
	    perror("freopen STDIN");

	fp = freopen(DEVNULL, "w", stdout);
	if (fp == NULL)
	    perror("freopen STDOUT");

#if defined(HAVE_SYS_IOCTL_H) && defined(TIOCNOTTY)
	fp = freopen("/dev/tty", "a", stderr);
	if (fp == NULL)
	    perror("freopen(/dev/tty) STDERR");

	(void)ioctl(fileno(fp), TIOCNOTTY, 0);
	(void)fclose(fp);
#endif

	fp = freopen(DEVNULL, "w", stderr);
	if (fp == NULL)
	    perror("freopen STDERR");
    }

    /* Send our version string, optionally including the session token.
     * Format: PROTOCOL_VERSION REMRT_AUTH_TOKEN_PREFIX <hex-token>
     * The token is only appended when one was supplied on the command
     * line (i.e., this process was auto-launched by remrt via SSH).
     */
    {
	struct bu_vls ver = BU_VLS_INIT_ZERO;
	bu_vls_strcat(&ver, PROTOCOL_VERSION);
	if (srv_auth_token[0] != '\0') {
	    bu_vls_strcat(&ver, REMRT_AUTH_TOKEN_PREFIX);
	    bu_vls_strcat(&ver, srv_auth_token);
	}
	if (pkg_send(MSG_VERSION,
		     bu_vls_addr(&ver), bu_vls_strlen(&ver) + 1,
		     pcsrv) < 0) {
	    fprintf(stderr, "pkg_send MSG_VERSION error\n");
	    bu_vls_free(&ver);
	    return 1;
	}
	if (debug)
	    fprintf(stderr, "PROTOCOL_VERSION='%s'\n", bu_vls_addr(&ver));
	bu_vls_free(&ver);
    }

    /*
     * Now that the fork() has been done, it is safe to initialize
     * the parallel processing support.
     */

    avail_cpus = bu_avail_cpus();

    npsw = avail_cpus;

    bu_log("using %zd of %zu cpus\n", npsw, avail_cpus);

    /* Before option processing, do application-specific initialization */
    RT_APPLICATION_INIT(&APP);
    application_init();

    /* excessive overlap reporting can saturate remrt */
    APP.a_logoverlap = rt_silent_logoverlap;

    BU_LIST_INIT(&WorkHead);

    for (;;) {
	struct pkg_queue *lp;
	fd_set ifds;
	struct timeval tv;

	/* Exit cleanly if the log hook detected a broken connection. */
	if (rtsrv_connection_lost) {
	    fprintf(stderr, "rtsrv: connection to remrt lost, exiting\n");
	    break;
	}

	/* First, process any packages in library buffers */
	if (pkg_process(pcsrv) < 0) {
	    bu_log("pkg_get error\n");
	    break;
	}

	/* Second, see if any input to read */
	FD_ZERO(&ifds);
	/* For pipe transport pkg_open_fds() uses PKG_STDIO_MODE (-3) as pkc_fd;
	 * pkc_in_fd holds the real read-end file descriptor in that case.
	 * Using pkc_fd directly with FD_SET would pass -3, which triggers
	 * glibc's out-of-range assertion and aborts the process on Linux.    */
	{
	    int sel_fd = (pcsrv->pkc_fd == PKG_STDIO_MODE) ? pcsrv->pkc_in_fd : pcsrv->pkc_fd;
	    FD_SET(sel_fd, &ifds);
	    tv.tv_sec = BU_LIST_NON_EMPTY(&WorkHead) ? 0L : 9999L;
	    tv.tv_usec = 0L;

	    if (select(sel_fd+1, &ifds, (fd_set *)0, (fd_set *)0, &tv) != 0) {
		n = pkg_suckin(pcsrv);
		if (n < 0) {
		    bu_log("pkg_suckin error\n");
		    break;
		} else if (n == 0) {
		    /* EOF detected */
		    break;
		} else {
		    /* All is well */
		}
	    }
	}

	/* Third, process any new packages in library buffers */
	if (pkg_process(pcsrv) < 0) {
	    bu_log("pkg_get error\n");
	    break;
	}

	/* Finally, more work may have just arrived, check our list */
	if (BU_LIST_NON_EMPTY(&WorkHead)) {
	    lp = BU_LIST_FIRST(pkg_queue, &WorkHead);
	    BU_LIST_DEQUEUE(&lp->l);
	    switch (lp->type) {
		case MSG_MATRIX:
		    ph_matrix((struct pkg_conn *)0, lp->buf);
		    break;
		case MSG_LINES:
		    ph_lines((struct pkg_conn *)0, lp->buf);
		    break;
		case MSG_OPTIONS:
		    ph_options((struct pkg_conn *)0, lp->buf);
		    break;
		case MSG_GETTREES:
		    ph_gettrees((struct pkg_conn *)0, lp->buf);
		    break;
		default:
		    bu_log("bad list element, type=%d\n", lp->type);
		    return 33;
	    }
	    BU_PUT(lp, struct pkg_queue);
	}
    }

    return 0;		/* bu_exit(0, NULL) */
}


/*
 * Generic routine to add a newly arrived PKG to a linked list, for
 * later processing.  Note that the buffer will be freed when the list
 * element is processed.  Presently used for MATRIX and LINES
 * messages.
 */
void
ph_enqueue(struct pkg_conn *pc, char *buf)
{
    struct pkg_queue *lp;

    if (debug)
	fprintf(stderr, "ph_enqueue: %s\n", buf);

    BU_GET(lp, struct pkg_queue);
    lp->type = pc->pkc_type;
    lp->buf = buf;
    BU_LIST_INSERT(&WorkHead, &lp->l);
}


void
ph_cd(struct pkg_conn *UNUSED(pc), char *buf)
{
    /* Strip any trailing whitespace to guard against newlines in the path */
    {
	char *p = buf + strlen(buf);
	while (p > buf && isspace((unsigned char)p[-1]))
	    *--p = '\0';
    }
    if (debug)
	fprintf(stderr, "ph_cd %s\n", buf);
    if (chdir(buf) < 0)
	bu_exit(1, "ph_cd: chdir(%s) failure\n", buf);
    (void)free(buf);
}


void
ph_restart(struct pkg_conn *UNUSED(pc), char *buf)
{

    if (debug)
	fprintf(stderr, "ph_restart %s\n", buf);
    bu_log("Restarting\n");
    pkg_close(pcsrv);
    execvp(original_argv[0], original_argv);
    /* should not reach */
    bu_argv_free(original_argc, original_argv);
    perror("rtsrv");
    bu_exit(1, NULL);
}


/*
 * The only argument is the name of the database file.
 */
void
ph_dirbuild(struct pkg_conn *UNUSED(pc), char *buf)
{
    long max_argc = 0;
    char **argv = NULL;
    struct rt_i *rtip = NULL;
    size_t n = 0;

    if (debug)
	fprintf(stderr, "ph_dirbuild: %s\n", buf);

    /* assume maximal delimiter chars */
    max_argc = (strlen(buf) / 2) + 1;
    argv = (char **)bu_calloc(max_argc+1, sizeof(char *), "alloc argv");

    if ((bu_argv_from_string(argv, max_argc, buf)) <= 0) {
	/* No words in input */
	(void)free(buf);
	bu_free(argv, "free argv");
	return;
    }

    if (seen_dirbuild) {
	bu_log("ph_dirbuild:  MSG_DIRBUILD already seen, ignored\n");
	(void)free(buf);
	bu_free(argv, "free argv");
	return;
    }

    title_file = bu_strdup(argv[0]);
    bu_free(argv, "free argv");

    /* Build directory of GED database */
    if ((rtip=rt_dirbuild(title_file, idbuf, sizeof(idbuf))) == RTI_NULL) {
	/* Log via bu_log so the message is forwarded to remrt over the pkg
	 * connection before we exit, then signal the main loop to exit
	 * cleanly (matching the ph_lines pattern).  Using bu_exit() here
	 * races against the MSG_PRINT flush since the process terminates
	 * immediately after all hooks return.                            */
	bu_log("ph_dirbuild:  rt_dirbuild(%s) failure\n", title_file);
	rtsrv_connection_lost = 1;
	return;
    }
    APP.a_rt_i = rtip;
    seen_dirbuild = 1;

    /*
     * Initialize all the per-CPU memory resources.  Go for the max,
     * as TCL interface may change npsw as we run.
     */
    memset(resource, 0, sizeof(resource));
    for (n=0; n < MAX_PSW; n++) {
	rt_init_resource(&resource[n], n, rtip);
    }

    if (pkg_send(MSG_DIRBUILD_REPLY, idbuf, strlen(idbuf)+1, pcsrv) < 0)
	fprintf(stderr, "MSG_DIRBUILD_REPLY error\n");
}


/*
 * Each word in the command buffer is the name of a treetop.
 */
void
ph_gettrees(struct pkg_conn *UNUSED(pc), char *buf)
{
    long max_argc = 0;
    char **argv = NULL;
    int argc = 0;
    struct rt_i *rtip = APP.a_rt_i;

    RT_CK_RTI(rtip);

    if (debug)
	fprintf(stderr, "ph_gettrees: %s\n", buf);

    /* Copy values from command line options into rtip */
    rtip->useair = use_air;
    if (rt_dist_tol > 0) {
	rtip->rti_tol.dist = rt_dist_tol;
	rtip->rti_tol.dist_sq = rt_dist_tol * rt_dist_tol;
    }
    if (rt_perp_tol > 0) {
	rtip->rti_tol.perp = rt_perp_tol;
	rtip->rti_tol.para = 1 - rt_perp_tol;
    }

    /* assume maximal delimiter chars */
    max_argc = (strlen(buf) / 2) + 1;
    argv = (char **)bu_calloc(max_argc+1, sizeof(char *), "alloc argv");

    if ((argc = bu_argv_from_string(argv, max_argc, buf)) <= 0) {
	/* No words in input */
	(void)free(buf);
	bu_free(argv, "free argv");
	return;
    }
    title_obj = bu_strdup(argv[0]);

    if (rtip->needprep == 0) {
	/* First clean up after the end of the previous frame */
	if (debug)bu_log("Cleaning previous model\n");
	view_end(&APP);
	view_cleanup(rtip);
	rt_clean(rtip);
    }

    /* Load the desired portion of the model */
    if (rt_gettrees(rtip, argc, (const char **)argv, npsw) < 0)
	bu_log("rt_gettrees(%s) FAILED\n", argv[0]);
    bu_free(argv, "free argv");

    seen_gettrees = 1;
    (void)free(buf);

    prepare();

    /* Acknowledge that we are ready */
    if (pkg_send(MSG_GETTREES_REPLY, title_obj, strlen(title_obj)+1, pcsrv) < 0)
	fprintf(stderr, "MSG_START error\n");
}


void
process_cmd(char *buf)
{
    char *cp;
    char *sp;
    char *ep;
    int len;
    extern struct command_tab rt_do_tab[];	/* from do.c */

    /* Parse the string */
    len = strlen(buf);
    ep = buf+len;
    sp = buf;
    cp = buf;
    while (sp < ep) {
	/* Find next semi-colon */
	while (*cp && *cp != ';') cp++;
	*cp++ = '\0';
	/* Process this command */
	if (debug)
	    bu_log("process_cmd '%s'\n", sp);
	if (rt_do_cmd(APP.a_rt_i, sp, rt_do_tab) < 0)
	    bu_exit(1, "process_cmd: error on '%s'\n", sp);
	sp = cp;
    }
}


void
ph_options(struct pkg_conn *UNUSED(pc), char *buf)
{

    if (debug)
	fprintf(stderr, "ph_options: %s\n", buf);

    process_cmd(buf);

    /* Just in case command processed was "opt -P" */
    /* need to decouple parsing of user args from the npsw processor
     * thread count being used, which should be an unsigned/size_t
     * type. -- CSM */
/* if (npsw < 0) { */
/* /\* Negative number means "all but #" available *\/ */
/* npsw = avail_cpus - npsw; */
/* } */

    /* basic bounds sanity */
    if (npsw > MAX_PSW)
	npsw = MAX_PSW;
    if (npsw <= 0)
	npsw = 1;

    if (width <= 0 || height <= 0)
	bu_exit(3, "ph_options:  width=%zu, height=%zu\n", width, height);
    (void)free(buf);
}


void
ph_matrix(struct pkg_conn *UNUSED(pc), char *buf)
{
#ifndef NO_MAGIC_CHECKING
    struct rt_i *rtip = APP.a_rt_i;

    RT_CK_RTI(rtip);
#endif

    if (debug)
	fprintf(stderr, "ph_matrix: %s\n", buf);

    /* Start options in a known state */
    AmbientIntensity = 0.4;
    /* 0/0/1 background -- must match rt's initialize_option_defaults() */
    background[0] = background[1] = 0.0;
    background[2] = 1.0/255.0; /* slightly non-black */
    hypersample = 0;
    jitter = 0;
    rt_perspective = 0;
    eye_backoff = M_SQRT2;
    aspect = 1;
    stereo = 0;
    use_air = 0;
    width = height = 0;
    cell_width = cell_height = 0;
    lightmodel = 0;
    incr_mode = 0;
    rt_dist_tol = 0;
    rt_perp_tol = 0;

    process_cmd(buf);
    free(buf);
    seen_matrix = 1;
    /* New view parameters arrived: force grid+lighting re-setup on the
     * next ph_lines() so that grid_setup() uses the updated viewsize,
     * eye_model, and Viewrotscale values. */
    last_setup_frame = -1;
}


void
prepare(void)
{
    struct rt_i *rtip = APP.a_rt_i;

    RT_CK_RTI(rtip);

    if (debug)
	fprintf(stderr, "prepare()\n");

    /*
     * initialize application -- it will allocate 1 line and
     * set buf_mode=1, as well as do mlib_init().
     */
    (void)view_init(&APP, title_file, title_obj, 0, 0);

    do_prep(rtip);

    if (rtip->stats.nsolids <= 0)
	bu_exit(3, "ph_matrix: No solids remain after prep.\n");

    /*
     * Do NOT call grid_setup() or view_2init() here.  These are
     * deferred to ph_lines() so that the initialization order matches
     * rt's do_frame() exactly:
     *
     *   do_prep() -> grid_setup() -> view_2init() -> do_run()
     *
     * Calling view_2init() (which invokes light_maker() using the
     * current view2model) in the same stack frame as grid_setup()
     * guarantees that the light positions and the ray-origin grid are
     * built from the identical view2model matrix, achieving pixel-exact
     * parity with standalone rt.
     */
    last_setup_frame = -1;  /* trigger grid+lighting setup in ph_lines() */

    rtip->stats.nshots = 0;
    rtip->stats.nmiss_model = 0;
    rtip->stats.nmiss_tree = 0;
    rtip->stats.nmiss_solid = 0;
    rtip->stats.nmiss = 0;
    rtip->stats.nhits = 0;
    rtip->stats.rti_nrays = 0;

}


/*
 * Process pixels from 'a' to 'b' inclusive.  The results are sent
 * back all at once.  Limitation: may not do more than 'width' pixels
 * at once, because that is the size of the buffer (for now).
 */
void
ph_lines(struct pkg_conn *UNUSED(pc), char *buf)
{
    int a, b, fr;
    struct line_info info;
    struct rt_i *rtip = APP.a_rt_i;
    struct bu_external ext;
    int ret;

    RT_CK_RTI(rtip);

    if (debug > 1)
	fprintf(stderr, "ph_lines: %s\n", buf);
    if (!seen_gettrees) {
	bu_log("ph_lines:  no MSG_GETTREES yet\n");
	return;
    }
    if (!seen_matrix) {
	bu_log("ph_lines:  no MSG_MATRIX yet\n");
	return;
    }

    a=0;
    b=0;
    fr=0;
    if (sscanf(buf, "%d %d %d", &a, &b, &fr) != 3)
	bu_exit(2, "ph_lines:  %s conversion error\n", buf);

    srv_startpix = a;		/* buffer un-offset for view_pixel */

    /* FIXME: if we do less than we were assigned, remrt is just going
     * to drop us!  If we go over our scanlen, we'll probably overstep
     * memory in the view front-end.
     */
    if (b-a+1 > srv_scanlen)
	b = a + srv_scanlen - 1;

    rtip->stats.rti_nrays = 0;
    info.li_startpix = a;
    info.li_endpix = b;
    info.li_frame = fr;

    /* Set up grid vectors and lighting on the first MSG_LINES for each
     * frame (identified by the frame number 'fr').  This mirrors the
     * order used by rt's do_frame():
     *
     *   do_prep() [called in prepare()] -> grid_setup() -> view_2init()
     *   -> do_run()
     *
     * Doing grid_setup() and view_2init() here — immediately before
     * do_run() — guarantees that both the light positions (set by
     * light_maker() inside view_2init()) and the per-pixel ray origins
     * (computed by do_run() using viewbase_model / dx_model / dy_model)
     * are derived from the same view2model matrix, producing pixel-exact
     * parity with standalone rt.
     *
     * For multi-frame animations the light list carries over from the
     * previous frame when the geometry has not changed (no new
     * MSG_GETTREES / prepare() call).  light_cleanup() discards the
     * stale lights so that view_2init() recreates them with the
     * updated view2model.
     */
    if (fr != last_setup_frame) {
	struct bu_vls err = BU_VLS_INIT_ZERO;
	int setup;

	/* Discard lights from any previous frame so light_maker() inside
	 * view_2init() will build fresh ones from the current view2model.
	 * light_cleanup() is safe to call even when LightHead is empty. */
	light_cleanup();

	setup = grid_setup(&err);
	if (setup) {
	    bu_log("ph_lines: grid_setup failed: %s\n", bu_vls_cstr(&err));
	    bu_vls_free(&err);
	    free(buf);
	    /* Signal the main loop to exit so that remrt detects the
	     * dropped connection and requeues the pixel range to
	     * fr_todo, rather than waiting indefinitely for a
	     * MSG_PIXELS reply that will never arrive.              */
	    rtsrv_connection_lost = 1;
	    return;
	}
	bu_vls_free(&err);

	view_2init(&APP, NULL);
	last_setup_frame = fr;
    }

    rt_prep_timer();
    do_run(a, b);
    info.li_nrays = rtip->stats.rti_nrays;
    info.li_cpusec = rt_read_timer((char *)0, 0);
    info.li_percent = 42.0;	/* for now */

    if (!bu_struct_export(&ext, (void *)&info, desc_line_info))
	bu_exit(98, "ph_lines: bu_struct_export failure\n");

    if (debug) {
	fprintf(stderr, "PIXELS fr=%d pix=%d..%d, rays=%d, cpu=%g\n",
		info.li_frame,
		info.li_startpix, info.li_endpix,
		info.li_nrays, info.li_cpusec);
    }

    ret = pkg_2send(MSG_PIXELS, (const char *)ext.ext_buf, ext.ext_nbytes, (const char *)scanbuf, (b-a+1)*3, pcsrv);
    if (ret < 0) {
	fprintf(stderr, "MSG_PIXELS send error\n");
	bu_free_external(&ext);
    }

    bu_free_external(&ext);
}


int print_on = 1;

void
ph_loglvl(struct pkg_conn *UNUSED(pc), char *buf)
{
    if (debug)
	fprintf(stderr, "ph_loglvl %s\n", buf);

    if (buf[0] == '0')
	print_on = 0;
    else
	print_on = 1;

    (void)free(buf);
}


/*
 * bu_log hook: forwards formatted log strings to the remrt dispatcher
 * over the package connection.  Replaces the old bu_log() override that
 * caused LNK2005 multiply-defined-symbol errors on Windows (where the
 * DLL export from libbu cannot be silently overridden by an object file).
 */
static int
rtsrv_log_hook(void *clientdata, void *str)
{
    int ret;
    (void)clientdata;

    if (print_on == 0)
	return 0;

    if (pcsrv == PKC_NULL || pcsrv == PKC_ERROR) {
	fprintf(stderr, "%s", (const char *)str);
	return 0;
    }

    if (debug)
	fprintf(stderr, "%s", (const char *)str);

    ret = pkg_send(MSG_PRINT, (const char *)str, strlen((const char *)str)+1, pcsrv);
    if (ret < 0) {
	fprintf(stderr, "pkg_send MSG_PRINT failed\n");
	/* Mark the connection as lost; the main loop will break cleanly.
	 * Do NOT call bu_exit() here — this hook may be invoked from inside
	 * a parallel rendering thread, and bu_exit() from such a context is
	 * not safe.                                                          */
	rtsrv_connection_lost = 1;
    }
    return 0;
}


/*
 * bu_bomb hook: forwards the bomb string to the remrt dispatcher before
 * bu_bomb() terminates the process.
 */
static int
rtsrv_bomb_hook(void *clientdata, void *str)
{
    static const char *bomb_msg = "RTSRV terminated by bu_bomb()\n";
    int ret;
    (void)clientdata;

    if (pcsrv != PKC_NULL && pcsrv != PKC_ERROR) {
	ret = pkg_send(MSG_PRINT, (const char *)str, strlen((const char *)str)+1, pcsrv);
	if (ret < 0)
	    fprintf(stderr, "bu_bomb MSG_PRINT failed\n");

	ret = pkg_send(MSG_PRINT, bomb_msg, strlen(bomb_msg)+1, pcsrv);
	if (ret < 0)
	    fprintf(stderr, "bu_bomb MSG_PRINT failed\n");
    }

    if (debug)
	fprintf(stderr, "\n%s\n", (const char *)str);
    fflush(stderr);

    /* bu_bomb() will terminate the process after all hooks return */
    return 0;
}


/*
 * Install the log and bomb hooks once the package connection (pcsrv) is
 * open.  This suppresses the default stderr output from bu_log() so that
 * all logging goes over the network to the remrt dispatcher instead.
 */
static void
rtsrv_install_log_hook(void)
{
    bu_log_hook_delete_all();
    bu_log_add_hook(rtsrv_log_hook, NULL);
    bu_bomb_add_hook(rtsrv_bomb_hook, NULL);
}


void
ph_unexp(struct pkg_conn *pc, char *buf)
{
    int i;

    if (debug)
	fprintf(stderr, "ph_unexp %s\n", buf);

    for (i=0; pc->pkc_switch[i].pks_handler != NULL; i++) {
	if (pc->pkc_switch[i].pks_type == pc->pkc_type)
	    break;
    }
    bu_log("ph_unexp: unable to handle %s message: len %zu",
	   pc->pkc_switch[i].pks_title, pc->pkc_len);
    *buf = '*';
    (void)free(buf);
}


void
ph_end(struct pkg_conn *UNUSED(pc), char *UNUSED(buf))
{
    if (debug)
	fprintf(stderr, "ph_end\n");

    pkg_close(pcsrv);
    bu_exit(0, NULL);
}


void
ph_print(struct pkg_conn *UNUSED(pc), char *buf)
{
    fprintf(stderr, "msg: %s\n", buf);
    (void)free(buf);
}


/* Stub for do.c */
void
memory_summary(void)
{
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
