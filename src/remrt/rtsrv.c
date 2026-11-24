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
#if defined(_WIN32) && !defined(__CYGWIN__)
#  include <direct.h>
#  include <process.h>
#endif
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

#include "bu/arg.h"
#include "bu/app.h"
#include "bu/datetime.h"
#include "bu/file.h"
/* bu/ipc.h removed - transport handled by libpkg */
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/process.h"
#include "bu/vls.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "optical/debug.h"
#include "optical/light.h"
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

__BEGIN_DECLS

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
/* light_cleanup() is declared in optical/light.h */

/***** variables shared with worker() ******/
struct application APP;
int report_progress = 0;	/* !0 = user wants progress report */
/***** end variables shared with worker() *****/

static char idbuf[132] = {0};		/* First ID record info */

/* State flags */
static int seen_dirbuild = 0;
static int seen_gettrees = 0;
static int seen_matrix = 0;

/* Set to 1 when the remrt connection is lost mid-render so the main loop
 * can break cleanly rather than calling bu_exit() from a hook function.  */
static volatile int rtsrv_connection_lost = 0;
static int rtsrv_exit_status = 0;

#define RTSRV_CMD_REPLY_TIMEOUT_MS 10000

struct rtsrv_command_reply {
    int received;
    int success;
};

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
void ph_cmd_reply(struct pkg_conn *pc, char *buf);

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
    { MSG_CMD_REPLY,	ph_cmd_reply,	"Command status", NULL },
    { 0,		0,		NULL, NULL }
};


struct pkg_conn *pcsrv;	/* PKG connection to server */
const char *control_host;	/* name of host running controller */
const char *tcp_port;		/* TCP port on control_host */
int debug = 0;		/* 0=off, 1=debug, 2=verbose */

/*
 * Session authentication token, supplied via "-S <token>" on the
 * command line when remrt auto-launches this process via SSH.
 * TCP connections require this token; local inherited IPC receives it too.
 */
static char srv_auth_token[REMRT_AUTH_TOKEN_LEN + 1] = {0};
#ifdef HAVE_OPENSSL_SSL_H
static char srv_tls_fingerprint[REMRT_TLS_FINGERPRINT_HEX_LEN + 1] = {0};
#endif

struct rtsrv_args {
    int help;
    int debug;
    unsigned int rt_debug_mask;
    unsigned int optical_debug_mask;
    const char *auth_token;
    const char *tls_fingerprint;
    const char *ipc_addr;
};

/* forward declarations for log/bomb hook helpers defined later in this file */
static int rtsrv_log_hook(void *clientdata, void *str);
static int rtsrv_bomb_hook(void *clientdata, void *str);
static void rtsrv_install_log_hook(void);

static int
rtsrv_auth_token_validate(struct bu_vls *msg, const char *token)
{
    size_t i;

    if (!token || strlen(token) != REMRT_AUTH_TOKEN_LEN) {
	if (msg)
	    bu_vls_printf(msg, "session token must contain exactly %d hexadecimal characters\n",
		REMRT_AUTH_TOKEN_LEN);
	return -1;
    }

    for (i = 0; i < REMRT_AUTH_TOKEN_LEN; i++) {
	if (!isxdigit((unsigned char)token[i])) {
	    if (msg)
		bu_vls_printf(msg, "session token contains a non-hexadecimal character\n");
	    return -1;
	}
    }

    return 0;
}

#ifdef HAVE_OPENSSL_SSL_H
static int
rtsrv_tls_fingerprint_validate(struct bu_vls *msg, const char *fingerprint)
{
    if (remrt_tls_fingerprint_valid(fingerprint))
	return 0;
    if (msg)
	bu_vls_printf(msg, "dispatcher TLS fingerprint must contain exactly %d hexadecimal characters\n",
	    REMRT_TLS_FINGERPRINT_HEX_LEN);
    return -1;
}
#endif

static const struct bu_arg_option rtsrv_options[] = {
    BU_ARG_FLAG("h", "help", struct rtsrv_args, help,
	"Print help and exit"),
    BU_ARG_ALIAS("?", NULL, "help", 1),
    BU_ARG_COUNTING_FLAG("d", "debug", struct rtsrv_args, debug,
	BU_ARG_VALUE_FLAG,
	"Stay in the foreground instead of daemonizing and enable protocol tracing written to standard error"),
    BU_ARG_VALUE("x", "rt-debug", struct rtsrv_args, rt_debug_mask,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set the librt hexadecimal debug flags, with the same meaning as rt -x"),
    BU_ARG_VALUE("X", "optical-debug", struct rtsrv_args, optical_debug_mask,
	BU_ARG_VALUE_HEX_INTEGER, BU_ARG_REQUIRED, "mask",
	"Set the optical-shader hexadecimal debug flags, with the same meaning as rt -X"),
    BU_ARG_VALUE_VALIDATE("S", "session-token", struct rtsrv_args, auth_token,
	BU_ARG_VALUE_STRING, rtsrv_auth_token_validate, "token",
	"Authenticate with the 64-character hexadecimal token printed by remrt; launched workers normally receive it automatically"),
#ifdef HAVE_OPENSSL_SSL_H
    BU_ARG_VALUE_VALIDATE("C", "tls-fingerprint", struct rtsrv_args, tls_fingerprint,
	BU_ARG_VALUE_STRING, rtsrv_tls_fingerprint_validate, "sha256-pin",
	"Require TLS and authenticate the dispatcher with its SHA-256 public-key pin"),
#endif
    BU_ARG_VALUE("I", "ipc-address", struct rtsrv_args, ipc_addr,
	BU_ARG_VALUE_STRING, BU_ARG_REQUIRED, "address",
	"Connect through the supplied local IPC channel instead of TCP; an address may also be inherited through PKG_ADDR"),
    BU_ARG_OPTION_NULL
};

static const struct bu_arg_operand rtsrv_ipc_operands[] = {
    BU_ARG_OPERAND_NULL
};

static const struct bu_arg_operand rtsrv_tcp_operands[] = {
    BU_ARG_OPERAND("control-host", BU_ARG_VALUE_STRING, 1, 1,
	"Host running the remrt dispatcher", NULL),
    BU_ARG_OPERAND("tcp-port", BU_ARG_VALUE_STRING, 1, 1,
	"TCP port number or service used by the remrt dispatcher", NULL),
    BU_ARG_OPERAND("command", BU_ARG_VALUE_STRING, 0, 1,
	"Single command to send before exiting", NULL),
    BU_ARG_OPERAND_NULL
};

static const char * const rtsrv_ipc_options[] = {"ipc-address", NULL};
static const struct bu_arg_schema_case rtsrv_cases[] = {
    BU_ARG_SCHEMA_CASE("ipc", "Connect through an explicit IPC channel",
	BU_ARG_CONDITION_ANY_OPTION_PRESENT, rtsrv_ipc_options,
	rtsrv_ipc_operands, NULL),
    BU_ARG_SCHEMA_CASE_DEFAULT("tcp", "Connect to a remrt TCP dispatcher",
	rtsrv_tcp_operands, NULL),
    BU_ARG_SCHEMA_CASE_NULL
};

static const struct bu_arg_schema rtsrv_schema = BU_ARG_SCHEMA(
    "rtsrv", "Run a remote ray-tracing worker for remrt", rtsrv_options,
    NULL, BU_ARG_PARSE_OPTIONS_FIRST,
    BU_ARG_SCHEMA_META_HELP(NULL, NULL, NULL, NULL, rtsrv_cases));

static const struct bu_arg_grammar rtsrv_grammar = {
    &rtsrv_schema, NULL, NULL
};

static int
rtsrv_help(FILE *stream, int status)
{
    char *help = bu_arg_schema_help(&rtsrv_schema, "rtsrv");

    bu_flog(stream, "%s", help ? help : "");
    bu_free(help, "rtsrv help");
    return status;
}


static int
rtsrv_send_version(int message_type)
{
    struct bu_vls version = BU_VLS_INIT_ZERO;
    int result;

    bu_vls_strcat(&version, PROTOCOL_VERSION);
    if (srv_auth_token[0] != '\0') {
	bu_vls_strcat(&version, REMRT_AUTH_TOKEN_PREFIX);
	bu_vls_strcat(&version, srv_auth_token);
    }
    size_t length = bu_vls_strlen(&version) + 1;
    result = pkg_send(message_type, bu_vls_addr(&version), length, pcsrv);
    if (result != (int)length)
	fprintf(stderr, "rtsrv: unable to send protocol version\n");
    else if (debug)
	fprintf(stderr, "PROTOCOL_VERSION='%s'\n", bu_vls_addr(&version));
    bu_vls_free(&version);
    return result == (int)length ? BRLCAD_OK : BRLCAD_ERROR;
}


static int
rtsrv_run_command(const char *command)
{
    static const struct pkg_switch command_switch[] = {
	{ MSG_CMD_REPLY, ph_cmd_reply, "Command status", NULL },
	{ 0, NULL, NULL, NULL }
    };
    struct rtsrv_command_reply reply = {0, 0};
    pkg_mux_t *mux;
    int64_t deadline;
    const int usec_per_ms = 1000;
    size_t command_length = strlen(command) + 1;

    if (rtsrv_send_version(MSG_CMD_VERSION) != BRLCAD_OK ||
	pkg_send(MSG_CMD, command, command_length, pcsrv) != (int)command_length)
	return EXIT_FAILURE;

    mux = pkg_mux_create();
    if (!mux || pkg_mux_add_conn(mux, pcsrv) < 0) {
	pkg_mux_destroy(mux);
	return EXIT_FAILURE;
    }

    pcsrv->pkc_switch = command_switch;
    pcsrv->pkc_server_data = &reply;
    deadline = bu_gettime() + (int64_t)RTSRV_CMD_REPLY_TIMEOUT_MS * usec_per_ms;
    while (pkg_process(pcsrv) >= 0 && !reply.received) {
	int64_t remaining = deadline - bu_gettime();
	if (remaining <= 0)
	    break;
	int ready = pkg_conn_poll(pcsrv);
	if (ready == 0) {
	    int wait_ms = (int)((remaining + usec_per_ms - 1) / usec_per_ms);
	    ready = pkg_mux_wait(mux, wait_ms);
	}
	if (ready <= 0 || pkg_suckin(pcsrv) <= 0)
	    break;
    }
    pcsrv->pkc_server_data = NULL;
    pkg_mux_destroy(mux);
    if (!reply.received)
	fprintf(stderr, "rtsrv: no complete command response from dispatcher\n");
    return reply.success ? EXIT_SUCCESS : EXIT_FAILURE;
}

__END_DECLS

int
main(int argc, char **argv)
{
    struct rtsrv_args args = {0};
    struct bu_vls msg = BU_VLS_INIT_ZERO;
    const char * const *operands;
    size_t operand_count;
    int introspection;
    int n;
    int operand_index;
    pkg_mux_t *input_mux = NULL;

    bu_setprogname(argv[0]);

    introspection = bu_arg_introspect(&msg, &rtsrv_grammar, argc - 1,
	(const char * const *)(argv + 1));
    if (introspection != 0) {
	bu_flog(introspection > 0 ? stdout : stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return introspection > 0 ? 0 : 1;
    }

    operand_index = bu_arg_schema_parse(&rtsrv_schema, &args, &msg,
	argc - 1, (const char **)(argv + 1));
    if (operand_index < 0) {
	bu_flog(stderr, "%s", bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 3;
    }
    bu_vls_free(&msg);
    if (args.help)
	return rtsrv_help(stdout, 0);

    operands = (const char * const *)(argv + operand_index + 1);
    operand_count = (size_t)(argc - operand_index - 1);
    if ((args.ipc_addr && operand_count != 0) ||
	(!args.ipc_addr && operand_count != 0 &&
	 operand_count != 2 && operand_count != 3))
	return rtsrv_help(stderr, 2);

    original_argc = argc;
    original_argv = bu_argv_dup(argc, (const char **)argv);
    debug = args.debug;
    rt_debug = args.rt_debug_mask;
    optical_debug = args.optical_debug_mask;
    if (args.auth_token)
	bu_strlcpy(srv_auth_token, args.auth_token, sizeof(srv_auth_token));
#ifdef HAVE_OPENSSL_SSL_H
    if (args.tls_fingerprint)
	bu_strlcpy(srv_tls_fingerprint, args.tls_fingerprint,
	    sizeof(srv_tls_fingerprint));
#endif

    /* Determine IPC mode: -I flag takes explicit precedence; fall back to
     * the PKG_ADDR environment variable (PKG_ADDR_ENVVAR) set by
     * the parent before fork() (see add_host_local() in remrt.c).         */
    {
	if (args.ipc_addr) {
	    pcsrv = pkg_connect_addr(args.ipc_addr, pkgswitch, NULL);
	    if (pcsrv == PKC_ERROR || pcsrv == PKC_NULL) {
		fprintf(stderr, "rtsrv: pkg_connect_addr(%s) failed\n", args.ipc_addr);
		return 1;
	    }
	} else if (operand_count == 0) {
	    pcsrv = pkg_connect_env(pkgswitch, NULL);
	} else {
	    pcsrv = PKC_ERROR;
	}

	if (pcsrv != PKC_ERROR && pcsrv != PKC_NULL) {
	    /* IPC mode: remrt created a local transport and explicitly
	     * inherited its child endpoint, advertised via -I or PKG_ADDR.
	     * pkg_connect_addr/env wraps the already-inherited fd into a
	     * pkg_conn so the rest of the code is transport-agnostic. */
	    if (debug) {
		const char *active_addr = args.ipc_addr ? args.ipc_addr : getenv(PKG_ADDR_ENVVAR);
		fprintf(stderr, "rtsrv: IPC mode active (addr=%s)\n",
			active_addr ? active_addr : "unknown");
	    }
	} else {
	    /* Normal TCP mode */
	    if (operand_count != 2 && operand_count != 3)
		return rtsrv_help(stderr, 2);

	    control_host = operands[0];
	    tcp_port = operands[1];
	    if (srv_auth_token[0] == '\0') {
		fprintf(stderr, "rtsrv: -S session token is required for TCP connections\n");
		return 2;
	    }

#ifdef HAVE_OPENSSL_SSL_H
	    if (srv_tls_fingerprint[0] != '\0' &&
		!remrt_tls_fingerprint_valid(srv_tls_fingerprint)) {
		fprintf(stderr, "rtsrv: -C requires a valid dispatcher TLS fingerprint\n");
		return 2;
	    }
#endif

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
    if (!control_host && srv_tls_fingerprint[0] != '\0') {
	fprintf(stderr, "rtsrv: -C cannot be used with local IPC\n");
	pkg_close(pcsrv);
	return 2;
    }

    /* A certificate pin explicitly requests TLS.  Skip TLS in IPC mode --
     * the libpkg transport is inherited locally and needs no encryption. */
    if (control_host && srv_tls_fingerprint[0] != '\0') {
	SSL_CTX *tls_ctx = remrt_tls_client_ctx();
	if (!tls_ctx || remrt_tls_connect(tls_ctx, pcsrv,
		srv_tls_fingerprint) != REMRT_TLS_OK) {
	    fprintf(stderr, "rtsrv: TLS negotiation with %s failed\n",
		control_host);
	    if (tls_ctx)
		SSL_CTX_free(tls_ctx);
	    pkg_close(pcsrv);
	    return 1;
	}
	if (debug)
	    fprintf(stderr, "rtsrv: TLS established with %s\n", control_host);
	SSL_CTX_free(tls_ctx);
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

    if (operand_count == 3) {
	int command_status = rtsrv_run_command(operands[2]);
	pkg_close(pcsrv);
	return command_status;
    }

#ifdef SO_SNDBUF
    /* increase the default send buffer size to 32k since we're
     * sending pixels more than likely.  No-op for pipe transports.
     */
    pkg_set_send_buffer(pcsrv, 32767);
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

	fp = freopen(bu_file_null(), "r", stdin);
	if (fp == NULL)
	    perror("freopen STDIN");

	fp = freopen(bu_file_null(), "w", stdout);
	if (fp == NULL)
	    perror("freopen STDOUT");

#if defined(HAVE_SYS_IOCTL_H) && defined(TIOCNOTTY)
	fp = freopen("/dev/tty", "a", stderr);
	if (fp == NULL) {
	    perror("freopen(/dev/tty) STDERR");
	} else {
	    (void)ioctl(fileno(fp), TIOCNOTTY, 0);
	    (void)fclose(fp);
	}
#endif

	fp = freopen(bu_file_null(), "w", stderr);
	if (fp == NULL)
	    perror("freopen STDERR");
    }

    if (rtsrv_send_version(MSG_VERSION) != BRLCAD_OK)
	return 1;

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

    /* Do not use select() here.  On Windows the local transport uses an
     * anonymous pipe, and Winsock select() only accepts sockets.  The libpkg
     * multiplexer knows how to wait for each supported transport. */
    input_mux = pkg_mux_create();
    if (!input_mux || pkg_mux_add_conn(input_mux, pcsrv) < 0) {
	bu_log("unable to create libpkg input multiplexer\n");
	pkg_mux_destroy(input_mux);
	return 1;
    }

    for (;;) {
	struct pkg_queue *lp;

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

	/* Second, see if any input is ready on the active transport. */
	{
	    int wait_ms = BU_LIST_NON_EMPTY(&WorkHead) ? 0 : 9999000;
	    int wait_ret = pkg_mux_wait(input_mux, wait_ms);
	    if (wait_ret < 0) {
		bu_log("pkg_mux_wait error\n");
		break;
	    }

	    if (wait_ret > 0 && pkg_mux_is_ready_conn(input_mux, pcsrv)) {
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
		    pkg_mux_destroy(input_mux);
		    return 33;
	    }
	    BU_PUT(lp, struct pkg_queue);
	}
    }

    pkg_mux_destroy(input_mux);
    return rtsrv_exit_status;
}


void
ph_cmd_reply(struct pkg_conn *pc, char *buf)
{
    struct rtsrv_command_reply *reply = (struct rtsrv_command_reply *)pc->pkc_server_data;
    if (reply) {
	reply->received = 1;
	reply->success = buf && pc->pkc_len == sizeof(REMRT_CMD_REPLY_OK) &&
	    memcmp(buf, REMRT_CMD_REPLY_OK, sizeof(REMRT_CMD_REPLY_OK)) == 0;
	if (!reply->success)
	    fprintf(stderr, "rtsrv: dispatcher rejected command\n");
    } else {
	fprintf(stderr, "rtsrv: dispatcher rejected connection\n");
	rtsrv_exit_status = 1;
	rtsrv_connection_lost = 1;
    }
    if (buf)
	free(buf);
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
#if defined(_WIN32) && !defined(__CYGWIN__)
    _execvp(original_argv[0], (const char * const *)original_argv);
#else
    execvp(original_argv[0], original_argv);
#endif
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
