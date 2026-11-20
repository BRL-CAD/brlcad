/*                   T E S T _ Q T _ I P C . C P P
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
 * @file test_qt_ipc.cpp
 *
 * Qt IPC mechanism tests — validates the event-loop notification path
 * that qged uses for the ert command's rt→framebuffer pixel data flow
 *
 * Two test groups:
 *
 *   Group A: Qt notification delivery
 *     Replicates exactly the pattern used by QFBIPCSocket in
 *     src/qged/fbserv.cpp.  Watches the parent connection returned by
 *     fbs_open_ipc(), writes from the child end, and verifies the Qt
 *     event loop invokes the handler.
 *
 *   Group B: fbs_open_ipc() + inline Qt client handler smoke test
 *     Calls fbs_open_ipc() with an inline open/close handler pair that
 *     mirrors qdm_open_ipc_sw_client_handler(), verifies the notifier is
 *     created, then sends a MSG_FBCLOSE frame from the child end and confirms
 *     pkg_suckin / pkg_process run without error.
 *
 * Run with QT_QPA_PLATFORM=offscreen so no real display is required.
 *
 * Usage:
 *   QT_QPA_PLATFORM=offscreen ./test_qt_ipc [--verbose]
 *
 * CTest:
 *   ctest -R regress-qt-ipc
 */

#include "common.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "bio.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#ifndef _WIN32
#  include <QSocketNotifier>
#endif

/* bu/ipc.h removed - transport handled by libpkg */
#include "bu/str.h"
#include "bu/env.h"
#include "bu/app.h"
#include "dm/fbserv.h"
#include "dm.h"    /* MSG_FBCLOSE, NET_LONG_LEN */
#include "pkg.h"

/* Portable low-level fd write (used when we only have the raw fd). */
#ifdef _WIN32
#  include <io.h>
#  define ipc_fd_write(fd, buf, n)  ((ssize_t)_write((fd), (buf), (unsigned)(n)))
#else
#  define ipc_fd_write(fd, buf, n)  ((ssize_t)write((fd), (buf), (n)))
#endif

/* ------------------------------------------------------------------ */
/* Test harness                                                        */
/* ------------------------------------------------------------------ */

static int  g_tests_run  = 0;
static int  g_tests_pass = 0;
static bool g_verbose    = false;

#define TEST(label, cond) do {                                             \
    g_tests_run++;                                                         \
    bool _pass = (bool)(cond);                                             \
    if (_pass) {                                                           \
	g_tests_pass++;                                                    \
	if (g_verbose)                                                     \
	    fprintf(stdout, "PASS: %s\n", (label));                        \
    } else {                                                               \
	fprintf(stdout, "FAIL: %s\n", (label));                            \
    }                                                                      \
} while (0)

#ifdef _WIN32
using IpcNotifier = QTimer;
static const int IPC_POLL_MS = 10;
#else
using IpcNotifier = QSocketNotifier;
#endif

static bool
notifier_enabled(const IpcNotifier *notifier)
{
    if (!notifier)
	return false;
#ifdef _WIN32
    return notifier->isActive();
#else
    return notifier->isEnabled();
#endif
}

static bool
wait_for_notification(const int &calls)
{
    const int NOTIFICATION_TIMEOUT_MS = 2000;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    timeout.start(NOTIFICATION_TIMEOUT_MS);

    /* Wait for events so the Windows polling timer can fire. */
    while (calls == 0 && timeout.isActive())
	loop.processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents);
    return calls > 0;
}

/* ------------------------------------------------------------------ */
/* Stub fbserv_obj callbacks (server-TCP path — not exercised here)   */
/* ------------------------------------------------------------------ */

static int  noop_is_listening(struct fbserv_obj *) { return 0; }
static int  noop_listen_on_port(struct fbserv_obj *, int) { return -1; }
static void noop_open_server(struct fbserv_obj *)  {}
static void noop_close_server(struct fbserv_obj *) {}

static void
noop_open_client(struct fbserv_obj *, int, void *) {}

static void
noop_close_client(struct fbserv_obj *, int) {}


/* ================================================================== */
/* Group A: Qt event-loop notification delivery                        */
/* ================================================================== */

static void
test_qt_notification_delivery(void)
{
    if (g_verbose)
	fprintf(stdout, "\n[Group A] Qt IPC notification delivery\n");

    /* ---- 1. Set up a minimal fbserv_obj ---- */
    struct fbserv_obj fbs;
    memset(&fbs, 0, sizeof(fbs));
    fbs.fbs_listener.fbsl_fd    = -1;
    fbs.fbs_is_listening        = noop_is_listening;
    fbs.fbs_listen_on_port      = noop_listen_on_port;
    fbs.fbs_open_server_handler = noop_open_server;
    fbs.fbs_close_server_handler= noop_close_server;
    fbs.fbs_open_client_handler = noop_open_client;
    fbs.fbs_close_client_handler= noop_close_client;
    /* No IPC-specific handler — fbs_open_ipc falls back to open_client_handler */

    /* ---- 2. Open the IPC channel ---- */
    int rc = fbs_open_ipc(&fbs);
    TEST("A1: fbs_open_ipc returns BRLCAD_OK", rc == BRLCAD_OK);
    if (rc != BRLCAD_OK) return;

    /* ---- 3. Find the parent-end pkg_conn and its read fd ---- */
    struct pkg_conn *ppc = nullptr;
    int parent_rfd = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
	if (fbs.fbs_clients[i].fbsc_pkg) {
	    ppc = fbs.fbs_clients[i].fbsc_pkg;
	    parent_rfd = fbs.fbs_clients[i].fbsc_fd;
	    break;
	}
    }
    TEST("A2: parent pkg_conn registered", ppc != nullptr);
    TEST("A3: fbsc_fd is valid (>= 0)", parent_rfd >= 0);
    if (!ppc || parent_rfd < 0) { fbs_close(&fbs); return; }

    /* ---- 4. Watch the parent connection for input ----
     * This mirrors exactly what qdm_open_ipc_sw_client_handler does
     * in src/qged/fbserv.cpp.                                       */
    int notifier_fired = 0;
#ifdef _WIN32
    QTimer notifier;
    notifier.setInterval(IPC_POLL_MS);
    QObject::connect(&notifier, &QTimer::timeout, [&]() {
	if (pkg_conn_poll(ppc) != 1)
	    return;
	notifier_fired++;
	notifier.stop();
    });
    notifier.start();
#else
    QSocketNotifier notifier(parent_rfd, QSocketNotifier::Read);
    QObject::connect(&notifier, &QSocketNotifier::activated, [&]() {
	notifier_fired++;
	notifier.setEnabled(false);
    });
#endif

    TEST("A4: Qt IPC notification enabled", notifier_enabled(&notifier));

    /* ---- 5. Write from the child end to make parent_rfd readable ---- */
    struct pkg_conn *ce = fbs.fbs_listener.fbsl_ipc_child;
    TEST("A5: fbsl_ipc_child non-NULL after fbs_open_ipc", ce != nullptr);
    if (!ce) { fbs_close(&fbs); return; }

    int ce_wfd = pkg_get_write_fd(ce);
    TEST("A6: child write fd valid", ce_wfd >= 0);
    if (ce_wfd < 0) { fbs_close(&fbs); return; }

    static const char payload[] = "QT_NOTIFIER_TEST";
    ssize_t n = ipc_fd_write(ce_wfd, payload, sizeof(payload)-1);
    TEST("A7: write from child end succeeds", n == (ssize_t)(sizeof(payload)-1));
    if (n < 0) { fbs_close(&fbs); return; }

    /* ---- 6. Let Qt dispatch the IPC callback ---- */
    TEST("A8: Qt IPC notification fired after data available",
	 wait_for_notification(notifier_fired));

    fbs_close(&fbs);
}


/* ================================================================== */
/* Group B: inline Qt client handler + pkg_suckin/pkg_process          */
/* ================================================================== */

/* State tracked by the inline handler */
struct QtIpcState {
    IpcNotifier *notifier = nullptr;
    int ind = -1;
    struct fbserv_obj *fbsp = nullptr;
    int frames_processed = 0;
    int handler_calls = 0;
};
static QtIpcState g_qipc;

static void
qt_process_ipc(void)
{
    g_qipc.handler_calls++;
#ifdef _WIN32
    g_qipc.notifier->stop();
#else
    g_qipc.notifier->setEnabled(false);
#endif
    struct pkg_conn *pc = g_qipc.fbsp->fbs_clients[g_qipc.ind].fbsc_pkg;
    if (pc && pkg_suckin(pc) > 0)
	g_qipc.frames_processed = pkg_process(pc);
}

static void
qt_open_ipc_client_handler(struct fbserv_obj *fbsp, int i, void *)
{
    /* Replicates qdm_open_ipc_sw_client_handler */
    g_qipc.ind  = i;
    g_qipc.fbsp = fbsp;
#ifdef _WIN32
    struct pkg_conn *pc = fbsp->fbs_clients[i].fbsc_pkg;
    g_qipc.notifier = new QTimer;
    g_qipc.notifier->setInterval(IPC_POLL_MS);
    QObject::connect(g_qipc.notifier, &QTimer::timeout, [pc]() {
	if (pkg_conn_poll(pc) == 1)
	    qt_process_ipc();
    });
    g_qipc.notifier->start();
#else
    g_qipc.notifier = new QSocketNotifier(fbsp->fbs_clients[i].fbsc_fd, QSocketNotifier::Read);
    QObject::connect(g_qipc.notifier, &QSocketNotifier::activated, &qt_process_ipc);
#endif
}

static void
qt_close_ipc_client_handler(struct fbserv_obj *, int)
{
    if (g_qipc.notifier) {
	delete g_qipc.notifier;
	g_qipc.notifier = nullptr;
    }
}

/*
 * Send a minimal MSG_FBCLOSE frame from the child side.
 * Not used in automated tests (we write raw PKG bytes instead),
 * kept as documentation of the protocol helper pattern.
 *
 * static int
 * send_fbclose_via_child(struct pkg_conn *ce) { ... }
 */

static void
test_qt_handler_pkgproc(void)
{
    if (g_verbose)
	fprintf(stdout, "\n[Group B] inline Qt handler + pkg_suckin/pkg_process\n");

    /* ---- 1. Set up fbserv_obj with the Qt IPC handlers ---- */
    struct fbserv_obj fbs;
    memset(&fbs, 0, sizeof(fbs));
    fbs.fbs_listener.fbsl_fd       = -1;
    fbs.fbs_is_listening           = noop_is_listening;
    fbs.fbs_listen_on_port         = noop_listen_on_port;
    fbs.fbs_open_server_handler    = noop_open_server;
    fbs.fbs_close_server_handler   = noop_close_server;
    fbs.fbs_open_client_handler    = noop_open_client;
    fbs.fbs_close_client_handler   = noop_close_client;
    fbs.fbs_open_ipc_client_handler  = qt_open_ipc_client_handler;
    fbs.fbs_close_ipc_client_handler = qt_close_ipc_client_handler;

    g_qipc = QtIpcState{};

    /* ---- 2. fbs_open_ipc → qt_open_ipc_client_handler ---- */
    int rc = fbs_open_ipc(&fbs);
    TEST("B1: fbs_open_ipc with Qt handler returns BRLCAD_OK", rc == BRLCAD_OK);
    if (rc != BRLCAD_OK) return;

    TEST("B2: Qt notifier created by handler", g_qipc.notifier != nullptr);
    TEST("B3: Qt notifier enabled", notifier_enabled(g_qipc.notifier));

    struct pkg_conn *ce = fbs.fbs_listener.fbsl_ipc_child;
    TEST("B4: fbsl_ipc_child stored", ce != nullptr);
    if (!ce) { fbs_close(&fbs); return; }

    /* ---- 3. Send a well-formed PKG frame from the child end ---- */
    int child_wfd = pkg_get_write_fd(ce);
    TEST("B5: child write fd valid", child_wfd >= 0);
    if (child_wfd < 0) { fbs_close(&fbs); return; }

    /* Build and write a raw PKG frame for MSG_FBCLOSE to the child write fd.
     * PKG header layout (from pkg.h):
     *   pkh_magic[2]  = 0x41, 0xFE
     *   pkh_type[2]   = type, big-endian uint16
     *   pkh_len[4]    = body length, big-endian uint32
     * Total header: 8 bytes.  Body: MSG_FBCLOSE requires 5×NET_LONG_LEN bytes.
     */
    const int blen = 5 * NET_LONG_LEN;
    char frame[8 + 5 * NET_LONG_LEN];
    memset(frame, 0, sizeof(frame));
    /* magic */
    frame[0] = (char)0x41; frame[1] = (char)0xFE;
    /* type = MSG_FBCLOSE = 2, big-endian 16-bit */
    frame[2] = 0; frame[3] = (char)MSG_FBCLOSE;
    /* body length, big-endian 32-bit */
    frame[4] = (blen >> 24) & 0xff;
    frame[5] = (blen >> 16) & 0xff;
    frame[6] = (blen >>  8) & 0xff;
    frame[7] =  blen        & 0xff;
    /* body: 5 longs, all zero */
    ssize_t nw = ipc_fd_write(child_wfd, frame, sizeof(frame));
    TEST("B6: child sends PKG frame", nw == (ssize_t)sizeof(frame));
    if (nw < 0) { fbs_close(&fbs); return; }

    /* ---- 4. Let Qt dispatch the IPC callback ---- */
    TEST("B7: Qt handler called after PKG frame written",
	 wait_for_notification(g_qipc.handler_calls));
    TEST("B8: Qt handler processed one PKG frame", g_qipc.frames_processed == 1);

    fbs_close(&fbs);
}


/* ================================================================== */
/* main                                                                */
/* ================================================================== */

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    for (int i = 1; i < argc; i++) {
	if (BU_STR_EQUAL(argv[i], "--verbose") || BU_STR_EQUAL(argv[i], "-v"))
	    g_verbose = true;
    }

    /* Force offscreen Qt platform so no display is needed */
    if (!getenv("QT_QPA_PLATFORM"))
	bu_setenv("QT_QPA_PLATFORM", "offscreen", 1);

    QCoreApplication app(argc, argv);

    if (g_verbose)
	fprintf(stdout, "=== Qt IPC mechanism tests ===\n");

    test_qt_notification_delivery();
    test_qt_handler_pkgproc();

    fprintf(stdout, "\nQt IPC tests: %d/%d passed\n", g_tests_pass, g_tests_run);

    int failures = g_tests_run - g_tests_pass;
    return (failures == 0) ? 0 : 1;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
