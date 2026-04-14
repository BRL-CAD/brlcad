/*                  R E G R E S S _ I P C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2024 United States Government as represented by
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
/**
 * @file regress_ipc.cpp
 *
 * Regression tests for the bu_ipc + fbs_open_ipc + libtclcad IPC data-path.
 *
 * These tests exercise the code added in the "Expand bu_ipc usage" PR:
 *
 *   Phase 1: fbs_open_ipc() + fbs_ipc_child_addr_env()  (libdm/fbserv.c)
 *   Phase 2: BU_IPC_ADDR env-var detection               (libdm/if_remote.c)
 *   Phase 4: Tcl_CreateFileHandler callback mechanism    (libtclcad/fbserv.c)
 *   Phase 7: fbserv -I <addr> flag                       (src/fbserv/fbserv.c)
 *   Phase 8: gtransfer IPC path                          (src/gtools/gtransfer.c)
 *
 * Tests are self-contained and headless (no display required).
 *
 * Usage (standalone):
 *   ./regress_ipc [--verbose]
 *
 * CTest:
 *   ctest -R regress-ipc
 */

#include "common.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#ifdef _WIN32
#  include <io.h>
#  ifndef X_OK
#    define X_OK 0
#  endif
#endif
#include <tcl.h>

#include "bu/app.h"
#include "bu/ipc.h"
#include "bu/str.h"
#include "bu/env.h"
#include "bu/snooze.h"
#include "dm/fbserv.h"
#include "tclcad/misc.h"  /* tclcad_listen_ipc() */
#include "pkg.h"   /* PKG_STDIO_MODE, pkc_in_fd, pkc_out_fd */

/* Portable low-level fd read/write (used when we only have the raw fd). */
#ifdef _WIN32
#  define ipc_fd_write(fd, buf, n)  ((bu_ssize_t)_write((fd), (buf), (unsigned)(n)))
#  define ipc_fd_read(fd,  buf, n)  ((bu_ssize_t)_read((fd),  (buf), (unsigned)(n)))
#else
#  define ipc_fd_write(fd, buf, n)  ((bu_ssize_t)write((fd), (buf), (n)))
#  define ipc_fd_read(fd,  buf, n)  ((bu_ssize_t)read((fd),  (buf), (n)))
#endif

/* ------------------------------------------------------------------ */
/* Test harness                                                        */
/* ------------------------------------------------------------------ */

static int g_tests_run  = 0;
static int g_tests_pass = 0;
static bool g_verbose   = false;

#define TEST(label, cond) do {                                            \
    g_tests_run++;                                                        \
    if ((int)(cond)) {                                                    \
        g_tests_pass++;                                                   \
        if (g_verbose)                                                    \
            fprintf(stdout, "PASS: %s\n", (label));                      \
    } else {                                                              \
        fprintf(stdout, "FAIL: %s\n", (label));                          \
    }                                                                     \
} while (0)

/* Return the effective write fd from a pkg_conn (pipe or socket). */
static int
pkc_wfd(struct pkg_conn *pc)
{
    if (!pc) return -1;
    return (pc->pkc_fd == PKG_STDIO_MODE) ? pc->pkc_out_fd : pc->pkc_fd;
}

/* ------------------------------------------------------------------ */
/* noop fbserv_obj callbacks                                           */
/* ------------------------------------------------------------------ */
static int  noop_is_listening(struct fbserv_obj *f)              { (void)f; return 0; }
static int  noop_listen_on_port(struct fbserv_obj *f, int p)     { (void)f; (void)p; return 1; }
static void noop_open_server(struct fbserv_obj *f)               { (void)f; }
static void noop_close_server(struct fbserv_obj *f)              { (void)f; }
static void noop_open_ipc_client(struct fbserv_obj *f, int i, void *d) { (void)f; (void)i; (void)d; }
static void noop_close_ipc_client(struct fbserv_obj *f, int i)  { (void)f; (void)i; }

static void
init_fbs_noops(struct fbserv_obj *fbs)
{
    memset(fbs, 0, sizeof(*fbs));
    fbs->fbs_listener.fbsl_fd      = -1;
    fbs->fbs_is_listening          = noop_is_listening;
    fbs->fbs_listen_on_port        = noop_listen_on_port;
    fbs->fbs_open_server_handler   = noop_open_server;
    fbs->fbs_close_server_handler  = noop_close_server;
    fbs->fbs_open_client_handler   = NULL;
    fbs->fbs_close_client_handler  = noop_close_ipc_client;
    fbs->fbs_open_ipc_client_handler  = noop_open_ipc_client;
    fbs->fbs_close_ipc_client_handler = noop_close_ipc_client;
}


/* ================================================================== */
/* Group 1: bu_ipc_pair() basics                                       */
/* ================================================================== */
static void
test_bu_ipc_pair_basics(void)
{
    if (g_verbose) fprintf(stdout, "\n[Group 1] bu_ipc_pair() basics\n");

    bu_ipc_chan_t *pe = nullptr, *ce = nullptr;
    int rc = bu_ipc_pair(&pe, &ce);
    TEST("bu_ipc_pair returns 0",        rc == 0);
    TEST("parent channel non-NULL",      pe != nullptr);
    TEST("child channel non-NULL",       ce != nullptr);

    if (!pe || !ce) {
        if (pe) bu_ipc_close(pe);
        if (ce) bu_ipc_close(ce);
        return;
    }

    TEST("parent read  fd >= 0",  bu_ipc_fileno(pe)       >= 0);
    TEST("parent write fd >= 0",  bu_ipc_fileno_write(pe) >= 0);
    TEST("child  read  fd >= 0",  bu_ipc_fileno(ce)       >= 0);
    TEST("child  write fd >= 0",  bu_ipc_fileno_write(ce) >= 0);

    const char *ae_pe = bu_ipc_addr_env(pe);
    const char *ae_ce = bu_ipc_addr_env(ce);
    TEST("parent addr_env non-NULL",               ae_pe != nullptr);
    TEST("child  addr_env non-NULL",               ae_ce != nullptr);
    TEST("parent addr_env has BU_IPC_ADDR= prefix",
         ae_pe && bu_strncmp(ae_pe, "BU_IPC_ADDR=", 12) == 0);
    TEST("child  addr_env has BU_IPC_ADDR= prefix",
         ae_ce && bu_strncmp(ae_ce, "BU_IPC_ADDR=", 12) == 0);

    bu_ipc_close(pe);
    bu_ipc_close(ce);
}


/* ================================================================== */
/* Group 2: round-trip data through pipe/socketpair                    */
/* ================================================================== */
static void
test_bu_ipc_roundtrip(void)
{
    if (g_verbose) fprintf(stdout, "\n[Group 2] bu_ipc round-trip data\n");

    bu_ipc_chan_t *pe = nullptr, *ce = nullptr;
    if (bu_ipc_pair(&pe, &ce) != 0 || !pe || !ce) {
        TEST("bu_ipc_pair for round-trip", 0);
        if (pe) bu_ipc_close(pe);
        if (ce) bu_ipc_close(ce);
        return;
    }
    TEST("bu_ipc_pair for round-trip", 1);

    static const char msg1[] = "PARENT_TO_CHILD";
    bu_ssize_t n = bu_ipc_write(pe, msg1, sizeof(msg1)-1);
    TEST("parent writes to child", n == (bu_ssize_t)(sizeof(msg1)-1));

    char buf[64]; memset(buf, 0, sizeof(buf));
    n = bu_ipc_read(ce, buf, sizeof(msg1)-1);
    TEST("child reads from parent", n == (bu_ssize_t)(sizeof(msg1)-1));
    TEST("parent→child data matches", memcmp(buf, msg1, sizeof(msg1)-1) == 0);

    static const char msg2[] = "CHILD_TO_PARENT";
    n = bu_ipc_write(ce, msg2, sizeof(msg2)-1);
    TEST("child writes to parent", n == (bu_ssize_t)(sizeof(msg2)-1));

    memset(buf, 0, sizeof(buf));
    n = bu_ipc_read(pe, buf, sizeof(msg2)-1);
    TEST("parent reads from child", n == (bu_ssize_t)(sizeof(msg2)-1));
    TEST("child→parent data matches", memcmp(buf, msg2, sizeof(msg2)-1) == 0);

    bu_ipc_close(pe); bu_ipc_close(ce);
}


/* ================================================================== */
/* Group 3: bu_ipc_connect() via addr string                           */
/* ================================================================== */
static void
test_bu_ipc_connect(void)
{
    if (g_verbose) fprintf(stdout, "\n[Group 3] bu_ipc_connect() via addr string\n");

    bu_ipc_chan_t *pe = nullptr, *ce = nullptr;
    if (bu_ipc_pair(&pe, &ce) != 0 || !pe || !ce) {
        TEST("bu_ipc_pair for connect test", 0);
        if (pe) bu_ipc_close(pe);
        if (ce) bu_ipc_close(ce);
        return;
    }
    TEST("bu_ipc_pair for connect test", 1);

    const char *ae = bu_ipc_addr_env(ce);
    const char *eq = ae ? strchr(ae, '=') : nullptr;
    TEST("child addr_env has '=' separator", eq != nullptr);

    if (eq) {
        bu_ipc_chan_t *conn = bu_ipc_connect(eq+1);
        TEST("bu_ipc_connect via child addr succeeds", conn != nullptr);

        if (conn) {
            int rfd = bu_ipc_fileno(conn);
            TEST("connected channel read fd >= 0", rfd >= 0);

            /* Write from parent, read via connected channel */
            static const char msg[] = "CONNECT_OK";
            bu_ssize_t n = bu_ipc_write(pe, msg, sizeof(msg)-1);
            TEST("parent write succeeds", n == (bu_ssize_t)(sizeof(msg)-1));

            char buf[64]; memset(buf, 0, sizeof(buf));
            n = bu_ipc_read(conn, buf, sizeof(msg)-1);
            TEST("connected-channel read succeeds", n == (bu_ssize_t)(sizeof(msg)-1));
            TEST("connected-channel data matches", memcmp(buf, msg, sizeof(msg)-1) == 0);

            /* Detach: conn wraps the same fds as ce — don't close them twice */
            bu_ipc_detach(conn);
        }
    }

    bu_ipc_close(pe); bu_ipc_close(ce);
}


/* ================================================================== */
/* Group 4: fbs_open_ipc() smoke test                                  */
/* ================================================================== */
static void
test_fbs_open_ipc_smoke(void)
{
    if (g_verbose) fprintf(stdout, "\n[Group 4] fbs_open_ipc() smoke test\n");

    struct fbserv_obj fbs;
    init_fbs_noops(&fbs);

    int rc = fbs_open_ipc(&fbs);
    TEST("fbs_open_ipc returns BRLCAD_OK", rc == BRLCAD_OK);

    if (rc == BRLCAD_OK) {
        const char *ae = fbs_ipc_child_addr_env(&fbs);
        TEST("fbs_ipc_child_addr_env non-NULL",              ae != nullptr);
        TEST("fbs_ipc_child_addr_env has BU_IPC_ADDR= pfx",
             ae && bu_strncmp(ae, "BU_IPC_ADDR=", 12) == 0);
        TEST("fbsl_ipc_child non-NULL",
             fbs.fbs_listener.fbsl_ipc_child != nullptr);

        int found_ipc = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (fbs.fbs_clients[i].fbsc_pkg != nullptr) {
                found_ipc = fbs.fbs_clients[i].fbsc_is_ipc;
                break;
            }
        }
        TEST("IPC client slot has fbsc_is_ipc set", found_ipc != 0);

        /* fbsc_fd must be a valid (>= 0) file descriptor,
         * not PKG_STDIO_MODE (-3).  This is required by callers like
         * Tcl_CreateFileHandler and select()-based loops.              */
        int valid_fd = 0;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (fbs.fbs_clients[i].fbsc_is_ipc) {
                valid_fd = (fbs.fbs_clients[i].fbsc_fd >= 0) ? 1 : 0;
                break;
            }
        }
        TEST("IPC client fbsc_fd is a valid (>= 0) fd", valid_fd);

        fbs_close(&fbs);
        TEST("fbsl_ipc_child NULL after fbs_close",
             fbs.fbs_listener.fbsl_ipc_child == nullptr);
    }
}


/* ================================================================== */
/* Group 5: BU_IPC_ADDR env-var end-to-end (Phase 2 + Phase 3)        */
/* ================================================================== */
static void
test_env_var_end_to_end(void)
{
    if (g_verbose) fprintf(stdout, "\n[Group 5] BU_IPC_ADDR env-var end-to-end\n");

    struct fbserv_obj fbs;
    init_fbs_noops(&fbs);

    int rc = fbs_open_ipc(&fbs);
    TEST("fbs_open_ipc for env-var test", rc == BRLCAD_OK);
    if (rc != BRLCAD_OK) return;

    /* Find the parent-end pkg_conn registered by fbs_open_ipc */
    struct pkg_conn *ppc = nullptr;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (fbs.fbs_clients[i].fbsc_pkg != nullptr) {
            ppc = fbs.fbs_clients[i].fbsc_pkg;
            break;
        }
    }
    TEST("parent pkg_conn registered", ppc != nullptr);
    if (!ppc) { fbs_close(&fbs); return; }

    int pwfd = pkc_wfd(ppc);
    TEST("parent pkg_conn write fd valid", pwfd >= 0);
    if (pwfd < 0) { fbs_close(&fbs); return; }

    /* Extract addr_val and simulate ert.cpp's env-var set */
    const char *ae = fbs_ipc_child_addr_env(&fbs);
    const char *eq = ae ? strchr(ae, '=') : nullptr;
    TEST("addr_env has '=' separator", eq != nullptr);
    if (!eq) { fbs_close(&fbs); return; }

    const char *addr_val = eq+1;
    bu_setenv(BU_IPC_ADDR_ENVVAR, addr_val, 1);

    /* Simulate rem_open() in if_remote.c: getenv → bu_ipc_connect */
    const char *from_env = getenv(BU_IPC_ADDR_ENVVAR);
    TEST("getenv(BU_IPC_ADDR_ENVVAR) returns non-NULL", from_env != nullptr);
    TEST("getenv value matches addr_val",
         from_env && BU_STR_EQUAL(from_env, addr_val));

    if (from_env) {
        bu_ipc_chan_t *child_chan = bu_ipc_connect(from_env);
        TEST("rem_open-style bu_ipc_connect succeeds", child_chan != nullptr);

        if (child_chan) {
            int child_rfd = bu_ipc_fileno(child_chan);
            TEST("child channel read fd >= 0", child_rfd >= 0);

            /* Pixel data: parent (fbserv side) → child (rt side) */
            static const char pkt[] = "PIXEL_PAYLOAD_0123456789";
            bu_ssize_t n = ipc_fd_write(pwfd, pkt, sizeof(pkt)-1);
            TEST("parent pkg_conn write to child", n == (bu_ssize_t)(sizeof(pkt)-1));

            char buf[64]; memset(buf, 0, sizeof(buf));
            n = bu_ipc_read(child_chan, buf, sizeof(pkt)-1);
            TEST("child reads pixel data", n == (bu_ssize_t)(sizeof(pkt)-1));
            TEST("pixel data matches end-to-end",
                 memcmp(buf, pkt, sizeof(pkt)-1) == 0);

            /* Detach: don't close fds that fbs still owns */
            bu_ipc_detach(child_chan);
        }
    }

    bu_setenv(BU_IPC_ADDR_ENVVAR, "", 1);
    fbs_close(&fbs);
}


/* ================================================================== */
/* Group 6: Tcl_CreateFileHandler callback path (Phase 4)              */
/* Tcl_CreateFileHandler is a POSIX-only Tcl API; not available on     */
/* Windows (which uses Tcl channels instead).                           */
/* ================================================================== */

#ifndef _WIN32

static int g_tcl_handler_invoked = 0;

static void
tcl_readable_cb(ClientData /*cd*/, int /*mask*/)
{
    g_tcl_handler_invoked++;
}

static void
tcl_open_ipc_client(struct fbserv_obj *fbsp, int i, void */*data*/)
{
    /* Exact replica of tclcad_open_client_handler (POSIX path) */
    Tcl_CreateFileHandler(fbsp->fbs_clients[i].fbsc_fd,
                          TCL_READABLE,
                          tcl_readable_cb,
                          (ClientData)&fbsp->fbs_clients[i]);
}

static void
tcl_close_ipc_client(struct fbserv_obj *fbsp, int i)
{
    Tcl_DeleteFileHandler(fbsp->fbs_clients[i].fbsc_fd);
}

static void
test_tcl_file_handler(void)
{
    if (g_verbose) fprintf(stdout, "\n[Group 6] Tcl_CreateFileHandler IPC callback\n");

    Tcl_FindExecutable(nullptr);
    Tcl_Interp *interp = Tcl_CreateInterp();
    TEST("Tcl_CreateInterp succeeds", interp != nullptr);
    if (!interp) return;

    struct fbserv_obj fbs;
    memset(&fbs, 0, sizeof(fbs));
    fbs.fbs_listener.fbsl_fd      = -1;
    fbs.fbs_is_listening          = noop_is_listening;
    fbs.fbs_listen_on_port        = noop_listen_on_port;
    fbs.fbs_open_server_handler   = noop_open_server;
    fbs.fbs_close_server_handler  = noop_close_server;
    fbs.fbs_open_client_handler   = nullptr;
    fbs.fbs_close_client_handler  = tcl_close_ipc_client;
    fbs.fbs_open_ipc_client_handler  = tcl_open_ipc_client;
    fbs.fbs_close_ipc_client_handler = tcl_close_ipc_client;
    fbs.fbs_interp = interp;

    /* fbs_open_ipc → tcl_open_ipc_client → Tcl_CreateFileHandler(fbsc_fd) */
    int rc = fbs_open_ipc(&fbs);
    TEST("fbs_open_ipc with Tcl callbacks returns BRLCAD_OK", rc == BRLCAD_OK);

    if (rc == BRLCAD_OK) {
        TEST("fbsl_ipc_child non-NULL", fbs.fbs_listener.fbsl_ipc_child != nullptr);

        /* The Tcl handler is watching the parent's READ fd (fbsc_fd = pkc_in_fd).
         * To make it readable, write FROM the child end (ce→parent direction). */
        bu_ipc_chan_t *ce = fbs.fbs_listener.fbsl_ipc_child;
        int ce_wfd = bu_ipc_fileno_write(ce);
        TEST("child write fd valid for Tcl trigger", ce_wfd >= 0);

        if (ce_wfd >= 0) {
            static const char msg[] = "TCL_HANDLER_TRIGGER";
            bu_ssize_t n = bu_ipc_write(ce, msg, sizeof(msg)-1);
            TEST("write from child end succeeds", n == (bu_ssize_t)(sizeof(msg)-1));

            g_tcl_handler_invoked = 0;
            /* TCL_DONT_WAIT: process one event without blocking */
            Tcl_DoOneEvent(TCL_DONT_WAIT | TCL_FILE_EVENTS);
            TEST("Tcl_DoOneEvent invokes IPC readable callback",
                 g_tcl_handler_invoked > 0);
        }

        fbs_close(&fbs);
    }

    Tcl_DeleteInterp(interp);
}

#endif /* !_WIN32 */


/* ================================================================== */
/* Group 7: $fbserv(ipc_addr) Tcl variable path (Phase 6)             */
/* ================================================================== */

#ifndef _WIN32

static void
test_tcl_fbserv_var(void)
{
    if (g_verbose) fprintf(stdout, "\n[Group 7] $fbserv(ipc_addr) Tcl variable\n");

    Tcl_Interp *interp = Tcl_CreateInterp();
    TEST("Tcl_CreateInterp for var test", interp != nullptr);
    if (!interp) return;

    struct fbserv_obj fbs;
    memset(&fbs, 0, sizeof(fbs));
    fbs.fbs_listener.fbsl_fd      = -1;
    fbs.fbs_is_listening          = noop_is_listening;
    fbs.fbs_listen_on_port        = noop_listen_on_port;
    fbs.fbs_open_server_handler   = noop_open_server;
    fbs.fbs_close_server_handler  = noop_close_server;
    fbs.fbs_open_client_handler   = nullptr;
    fbs.fbs_close_client_handler  = tcl_close_ipc_client;
    fbs.fbs_open_ipc_client_handler  = tcl_open_ipc_client;
    fbs.fbs_close_ipc_client_handler = tcl_close_ipc_client;
    fbs.fbs_interp = interp;

    int rc = fbs_open_ipc(&fbs);
    TEST("fbs_open_ipc for Tcl var test", rc == BRLCAD_OK);
    if (rc == BRLCAD_OK) {
        const char *ae = fbs_ipc_child_addr_env(&fbs);
        const char *eq = ae ? strchr(ae, '=') : nullptr;
        TEST("addr_env has '='", eq != nullptr);

        if (eq) {
            const char *addr_val = eq+1;

            /* Simulate tclcad_listen_ipc() Tcl_SetVar2 call */
            const char *sr = Tcl_SetVar2(interp, "fbserv", "ipc_addr",
                                         addr_val, TCL_GLOBAL_ONLY);
            TEST("Tcl_SetVar2($fbserv,ipc_addr) succeeds", sr != nullptr);

            const char *gr = Tcl_GetVar2(interp, "fbserv", "ipc_addr",
                                         TCL_GLOBAL_ONLY);
            TEST("Tcl_GetVar2($fbserv,ipc_addr) succeeds", gr != nullptr);
            TEST("Tcl var value matches addr_val",
                 gr && BU_STR_EQUAL(gr, addr_val));

            /* Verify a Tcl script can read and use the variable */
            const char *tcl_cmd =
                "set ::_ipc_len [string length $::fbserv(ipc_addr)]";
            int trc = Tcl_Eval(interp, tcl_cmd);
            TEST("Tcl script reads $fbserv(ipc_addr)", trc == TCL_OK);

            const char *tres = Tcl_GetVar(interp, "::_ipc_len", TCL_GLOBAL_ONLY);
            TEST("$fbserv(ipc_addr) has positive length in Tcl",
                 tres && atoi(tres) > 0);

            if (g_verbose)
                fprintf(stdout, "  $fbserv(ipc_addr) = \"%s\"\n", addr_val);
        }

        fbs_close(&fbs);
    }

    Tcl_DeleteInterp(interp);
}

#endif /* !_WIN32 */


/* ================================================================== */
/* Group 7 (Windows): tclcad_listen_ipc() timer-based IPC path        */
/* On Windows, Tcl_CreateFileHandler is a no-op and wrapping a pipe   */
/* in a Tcl channel causes Tcl's reader thread to consume data before  */
/* pkg_process() can read it.  tclcad_listen_ipc() uses a recurring   */
/* Tcl timer (PeekNamedPipe) instead.  This test verifies:            */
/*   1. tclcad_listen_ipc returns BRLCAD_OK on Windows (not error).   */
/*   2. The timer token (fbsc_chan) is installed.                      */
/*   3. The $fbserv(ipc_addr) Tcl variable is set.                    */
/*   4. fbs_close() cleanly cancels the timer (fbsc_chan becomes NULL).*/
/* ================================================================== */
#ifdef _WIN32
static void
test_tcl_listen_ipc_win(void)
{
    if (g_verbose) fprintf(stdout, "\n[Group 7] tclcad_listen_ipc() Windows timer-IPC path\n");

    Tcl_FindExecutable(nullptr);
    Tcl_Interp *interp = Tcl_CreateInterp();
    TEST("Tcl_CreateInterp for win IPC test", interp != nullptr);
    if (!interp) return;

    struct fbserv_obj fbs;
    memset(&fbs, 0, sizeof(fbs));
    fbs.fbs_listener.fbsl_fd     = -1;
    fbs.fbs_is_listening         = noop_is_listening;
    fbs.fbs_listen_on_port       = noop_listen_on_port;
    fbs.fbs_open_server_handler  = noop_open_server;
    fbs.fbs_close_server_handler = noop_close_server;
    fbs.fbs_open_client_handler  = nullptr;
    fbs.fbs_close_client_handler = noop_close_ipc_client;
    fbs.fbs_open_ipc_client_handler  = nullptr;   /* set by tclcad_listen_ipc */
    fbs.fbs_close_ipc_client_handler = nullptr;
    fbs.fbs_interp = interp;

    /* tclcad_listen_ipc() should now succeed on Windows and install the
     * timer-based IPC polling handler.                                    */
    int rc = tclcad_listen_ipc(&fbs, interp);
    TEST("tclcad_listen_ipc returns BRLCAD_OK on Windows", rc == BRLCAD_OK);

    if (rc == BRLCAD_OK) {
        TEST("fbsl_ipc_child non-NULL after tclcad_listen_ipc",
             fbs.fbs_listener.fbsl_ipc_child != nullptr);

        /* The timer token is stored in fbsc_chan of the registered client. */
        bool timer_installed = false;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (fbs.fbs_clients[i].fbsc_is_ipc && fbs.fbs_clients[i].fbsc_chan) {
                timer_installed = true;
                break;
            }
        }
        TEST("poll timer token installed in fbsc_chan", timer_installed);

        /* Verify $fbserv(ipc_addr) was set in the interpreter. */
        const char *gr = Tcl_GetVar2(interp, "fbserv", "ipc_addr", TCL_GLOBAL_ONLY);
        TEST("$fbserv(ipc_addr) is set by tclcad_listen_ipc", gr != nullptr);
        TEST("$fbserv(ipc_addr) is non-empty", gr && gr[0] != '\0');

        if (g_verbose && gr)
            fprintf(stdout, "  $fbserv(ipc_addr) = \"%s\"\n", gr);

        /* Verify a Tcl script can read the variable. */
        int trc = Tcl_Eval(interp,
                           "set ::_ipc_len [string length $::fbserv(ipc_addr)]");
        TEST("Tcl script reads $fbserv(ipc_addr)", trc == TCL_OK);
        const char *tres = Tcl_GetVar(interp, "::_ipc_len", TCL_GLOBAL_ONLY);
        TEST("$fbserv(ipc_addr) has positive length in Tcl",
             tres && atoi(tres) > 0);

        /* fbs_close() must cancel the timer; fbsc_chan becomes NULL. */
        fbs_close(&fbs);

        bool timer_canceled = true;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (fbs.fbs_clients[i].fbsc_chan) {
                timer_canceled = false;
                break;
            }
        }
        TEST("poll timer canceled after fbs_close()", timer_canceled);
    }

    Tcl_DeleteInterp(interp);
}
#endif /* _WIN32 */



/* Spawns fbserv -F /dev/null -I <addr> and verifies it accepts the   */
/* flag without printing "Illegal option" or "Usage:".                */
/* This test uses POSIX shell features and is skipped on Windows.     */
/* ================================================================== */
#ifndef _WIN32
static void
test_fbserv_minus_I(const char *fbserv_bin)
{
    if (g_verbose) fprintf(stdout, "\n[Group 8] fbserv -I <addr> flag\n");

    bu_ipc_chan_t *pe = nullptr, *ce = nullptr;
    if (bu_ipc_pair(&pe, &ce) != 0 || !pe || !ce) {
        TEST("bu_ipc_pair for fbserv -I test", 0);
        if (pe) bu_ipc_close(pe);
        if (ce) bu_ipc_close(ce);
        return;
    }
    TEST("bu_ipc_pair for fbserv -I test", 1);

    /* Move child-end fds high so they survive exec()'s fd sweep */
    (void)bu_ipc_move_high_fd(ce, 64);

    const char *ae = bu_ipc_addr_env(ce);
    const char *eq = ae ? strchr(ae, '=') : nullptr;
    TEST("child addr_env for fbserv -I test", eq != nullptr);
    if (!eq) { bu_ipc_close(pe); bu_ipc_close(ce); return; }

    const char *addr = eq+1;
    char log_path[256];
    snprintf(log_path, sizeof(log_path), "%s", "/tmp/regress_ipc_fbserv.log");

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "LD_LIBRARY_PATH=%s "
             "%s -F /dev/null -I '%s' "
             "> %s 2>&1 &",
             getenv("LD_LIBRARY_PATH") ? getenv("LD_LIBRARY_PATH") : "",
             fbserv_bin, addr, log_path);
    int sysrc = system(cmd);
    TEST("fbserv -I spawn returns 0", sysrc == 0);

    /* Poll for the log file rather than sleeping a fixed amount.
     * fbserv -I exits quickly (connect attempt completes or fails within
     * ~100 ms in practice); cap at 2 s to avoid flakiness on slow runners. */
    FILE *log = nullptr;
    for (int attempt = 0; attempt < 20 && !log; ++attempt) {
        bu_snooze(BU_SEC2USEC(0.1));   /* 100 ms per attempt, up to 2 s total */
        log = fopen(log_path, "r");
    }

    if (log) {
        char logbuf[1024] = {0};
        size_t nr = fread(logbuf, 1, sizeof(logbuf)-1, log);
        (void)nr;
        fclose(log);
        if (g_verbose)
            fprintf(stdout, "  fbserv -I log: %s\n",
                    logbuf[0] ? logbuf : "(empty)");
        bool bad = (strstr(logbuf, "Illegal option") ||
                    strstr(logbuf, "Usage: fbserv port_num")) ? true : false;
        TEST("fbserv -I: no 'Illegal option'/'Usage' in log", !bad);
    } else {
        TEST("fbserv -I log file created", 0);
    }

    bu_ipc_close(pe);
    bu_ipc_close(ce);
    bu_snooze(BU_SEC2USEC(0.2));
}
#endif /* !_WIN32 */


/* ================================================================== */
/* main                                                               */
/* ================================================================== */
int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);

#ifdef SIGPIPE
    /* Ignore SIGPIPE: writes to closed peers must not crash the harness */
    signal(SIGPIPE, SIG_IGN);
#endif

    for (int i = 1; i < argc; ++i)
        if (BU_STR_EQUAL(argv[i], "--verbose") ||
            BU_STR_EQUAL(argv[i], "-v"))
            g_verbose = true;

    const char *fbserv_bin = getenv("FBSERV_BIN");
    if (!fbserv_bin) {
        /* Try to find fbserv relative to this test binary's directory.
         * CMake sets the build dir; fall back to a common path.        */
        static char fbserv_path[1024];
        /* Walk up from test binary to find bin/ */
        if (argc > 0) {
            snprintf(fbserv_path, sizeof(fbserv_path), "%s", argv[0]);
            /* Find last path separator (handle both / and \ on Windows) */
            char *slash = strrchr(fbserv_path, '/');
#ifdef _WIN32
            {
                char *bslash = strrchr(fbserv_path, '\\');
                if (!slash || (bslash && bslash > slash))
                    slash = bslash;
            }
#endif
            if (slash) {
                *slash = '\0';
                /* Check sibling bin/ from test build dir */
                char candidate[1024];
                snprintf(candidate, sizeof(candidate),
                         "%s/../bin/fbserv", fbserv_path);
                if (access(candidate, X_OK) == 0) {
                    fbserv_bin = candidate;
                }
            }
        }
    }
    if (!fbserv_bin)
        fbserv_bin = "fbserv";   /* rely on PATH */

    if (g_verbose) {
        fprintf(stdout, "\n==============================================\n");
        fprintf(stdout, "  bu_ipc / fbs_open_ipc Regression Tests\n");
        fprintf(stdout, "==============================================\n");
        fprintf(stdout, "  fbserv binary: %s\n\n", fbserv_bin);
    }

    test_bu_ipc_pair_basics();
    test_bu_ipc_roundtrip();
    test_bu_ipc_connect();
    test_fbs_open_ipc_smoke();
    test_env_var_end_to_end();
#ifndef _WIN32
    test_tcl_file_handler();
    test_tcl_fbserv_var();
    test_fbserv_minus_I(fbserv_bin);
#else
    (void)fbserv_bin;
    if (g_verbose)
        fprintf(stdout, "\n[Group 6] SKIPPED (Tcl_CreateFileHandler not available on Windows)\n");
    test_tcl_listen_ipc_win();  /* Group 7: Windows timer-IPC path */
    if (g_verbose)
        fprintf(stdout, "\n[Group 8] SKIPPED (POSIX shell spawn not available on Windows)\n");
#endif

    fprintf(stdout, "\n==============================================\n");
    fprintf(stdout, "  Results: %d / %d passed\n", g_tests_pass, g_tests_run);
    fprintf(stdout, "==============================================\n\n");

    return (g_tests_pass == g_tests_run) ? 0 : 1;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
