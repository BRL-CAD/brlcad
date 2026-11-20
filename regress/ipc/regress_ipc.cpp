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
 * Regression tests for the pkg IPC + fbs_open_ipc + libtclcad IPC data-path.
 *
 * These tests exercise the code path using the libpkg transport API:
 *
 *   Group 1: pkg_pair_prefer() basics
 *   Group 2: round-trip data through pipe/socketpair
 *   Group 3: pkg_connect_addr() via addr string
 *   Group 4: fbs_open_ipc() smoke test
 *   Group 5: PKG_ADDR env-var end-to-end
 *   Group 6: Tcl_CreateFileHandler callback path (POSIX only)
 *   Group 7: tclcad_listen_ipc() Tcl variable path / Windows timer path
 *   Group 8: fbserv -I <addr> flag
 *   Group 9: reusable local IPC listener
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

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <set>
#include <string>
#include <thread>
#include <vector>
#ifndef _WIN32
#  include <signal.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#ifdef _WIN32
#  include <io.h>
#  ifndef X_OK
#    define X_OK 0
#  endif
#else
#  include <fcntl.h>
#endif
#include <tcl.h>

#include "bu/app.h"
#include "bu/datetime.h"
/* bu/ipc.h removed - transport handled by libpkg */
#include "bu/str.h"
#include "bu/env.h"
#include "bu/process.h"
#include "bu/snooze.h"
#include "bu/vls.h"
#include "dm/fbserv.h"
#include "tclcad/misc.h"  /* tclcad_listen_ipc() */
#include "pkg.h"

/* Portable low-level fd read/write (used when we only have the raw fd). */
#ifdef _WIN32
#  define ipc_fd_write(fd, buf, n)  ((ssize_t)_write((fd), (buf), (unsigned)(n)))
#  define ipc_fd_read(fd,  buf, n)  ((ssize_t)_read((fd),  (buf), (unsigned)(n)))
#else
#  define ipc_fd_write(fd, buf, n)  ((ssize_t)write((fd), (buf), (n)))
#  define ipc_fd_read(fd,  buf, n)  ((ssize_t)read((fd),  (buf), (n)))
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
    return pkg_get_write_fd(pc);
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
/* Group 1: pkg_pair_prefer() basics                                   */
/* ================================================================== */
static void
test_pkg_pair_basics(void)
{
    if (g_verbose) fprintf(stdout, "\n[Group 1] pkg_pair_prefer() basics\n");

    struct pkg_conn *pe = nullptr, *ce = nullptr;
    int rc = pkg_pair_prefer(&pe, &ce, NULL, NULL, PKG_TRANSPORT_AUTO);
    TEST("pkg_pair_prefer returns 0",       rc == 0);
    TEST("parent channel non-NULL",         pe != nullptr);
    TEST("child channel non-NULL",          ce != nullptr);

    if (!pe || !ce) {
        if (pe) pkg_close(pe);
        if (ce) pkg_close(ce);
        return;
    }

    TEST("parent read  fd >= 0",  pkg_get_read_fd(pe)  >= 0);
    TEST("parent write fd >= 0",  pkg_get_write_fd(pe) >= 0);
    TEST("child  read  fd >= 0",  pkg_get_read_fd(ce)  >= 0);
    TEST("child  write fd >= 0",  pkg_get_write_fd(ce) >= 0);

    const char *ae_pe = pkg_child_addr_env(pe);
    const char *ae_ce = pkg_child_addr_env(ce);
    TEST("pair address exportability is consistent",
        (ae_pe == nullptr) == (ae_ce == nullptr));
    if (ae_pe && ae_ce) {
        TEST("parent addr_env has PKG_ADDR= prefix",
            bu_strncmp(ae_pe, "PKG_ADDR=", 9) == 0);
        TEST("child addr_env has PKG_ADDR= prefix",
            bu_strncmp(ae_ce, "PKG_ADDR=", 9) == 0);
    }

#ifndef _WIN32
    int move_status = pkg_move_high_fd(ce, 64);
    TEST("moving child endpoint succeeds", move_status == 0);
    if (move_status == 0) {
        const char *moved_addr = pkg_child_addr(ce);
        int moved_read_fd = -1;
        int moved_write_fd = -1;
        bool address_matches = false;

        if (moved_addr && sscanf(moved_addr, "socket:%d", &moved_read_fd) == 1) {
            address_matches = moved_read_fd == pkg_get_read_fd(ce) &&
                moved_read_fd == pkg_get_write_fd(ce);
        } else if (moved_addr && sscanf(moved_addr, "pipe:%d,%d",
                &moved_read_fd, &moved_write_fd) == 2) {
            address_matches = moved_read_fd == pkg_get_read_fd(ce) &&
                moved_write_fd == pkg_get_write_fd(ce);
        }
        TEST("moved child address tracks inherited descriptors",
            address_matches);
    }

    if (pe && pkg_child_addr(pe)) {
        int parent_read_fd = -1;
        int parent_write_fd = -1;
        const int parent_move_status = pkg_move_high_fd(pe, 64);
        const char *parent_addr = pkg_child_addr(pe);
        const bool parent_address_matches = parent_move_status == 0 &&
            parent_addr && sscanf(parent_addr, "pipe_parent:%d,%d",
                &parent_read_fd, &parent_write_fd) == 2 &&
            parent_read_fd == pkg_get_read_fd(pe) &&
            parent_write_fd == pkg_get_write_fd(pe);
        TEST("moved parent address preserves endpoint direction",
            parent_address_matches);
    }
#endif

    pkg_close(pe);
    pkg_close(ce);

    pe = nullptr;
    ce = nullptr;
    rc = pkg_pair_prefer(&pe, &ce, NULL, NULL, PKG_TRANSPORT_TCP);
    TEST("TCP pair creation succeeds", rc == 0 && pe && ce);
    if (pe && ce) {
        TEST("TCP parent address is not falsely exportable",
            pkg_child_addr(pe) == nullptr && pkg_child_addr_env(pe) == nullptr);
        TEST("TCP child address is not falsely exportable",
            pkg_child_addr(ce) == nullptr && pkg_child_addr_env(ce) == nullptr);
    }
    if (pe) pkg_close(pe);
    if (ce) pkg_close(ce);

    pe = reinterpret_cast<struct pkg_conn *>(1);
    ce = reinterpret_cast<struct pkg_conn *>(1);
    const pkg_transport_t invalid_transport =
        static_cast<pkg_transport_t>(-1);
    rc = pkg_pair_prefer(&pe, &ce, NULL, NULL, invalid_transport);
    TEST("failed pair creation clears outputs",
        rc == -1 && pe == nullptr && ce == nullptr);
}


/* ================================================================== */
/* Group 2: round-trip data through pipe/socketpair                    */
/* ================================================================== */
static void
test_pkg_roundtrip(void)
{
    if (g_verbose) fprintf(stdout, "\n[Group 2] pkg_pair round-trip data\n");

    struct pkg_conn *pe = nullptr, *ce = nullptr;
    if (pkg_pair_prefer(&pe, &ce, NULL, NULL, PKG_TRANSPORT_AUTO) != 0 || !pe || !ce) {
        TEST("pkg_pair_prefer for round-trip", 0);
        if (pe) pkg_close(pe);
        if (ce) pkg_close(ce);
        return;
    }
    TEST("pkg_pair_prefer for round-trip", 1);

    int pe_wfd = pkg_get_write_fd(pe);
    int ce_rfd = pkg_get_read_fd(ce);
    int ce_wfd = pkg_get_write_fd(ce);
    int pe_rfd = pkg_get_read_fd(pe);

    static const char msg1[] = "PARENT_TO_CHILD";
    ssize_t n = ipc_fd_write(pe_wfd, msg1, sizeof(msg1)-1);
    TEST("parent writes to child", n == (ssize_t)(sizeof(msg1)-1));

    char buf[64]; memset(buf, 0, sizeof(buf));
    n = ipc_fd_read(ce_rfd, buf, sizeof(msg1)-1);
    TEST("child reads from parent", n == (ssize_t)(sizeof(msg1)-1));
    TEST("parent→child data matches", memcmp(buf, msg1, sizeof(msg1)-1) == 0);

    static const char msg2[] = "CHILD_TO_PARENT";
    n = ipc_fd_write(ce_wfd, msg2, sizeof(msg2)-1);
    TEST("child writes to parent", n == (ssize_t)(sizeof(msg2)-1));

    memset(buf, 0, sizeof(buf));
    n = ipc_fd_read(pe_rfd, buf, sizeof(msg2)-1);
    TEST("parent reads from child", n == (ssize_t)(sizeof(msg2)-1));
    TEST("child→parent data matches", memcmp(buf, msg2, sizeof(msg2)-1) == 0);

    pkg_close(pe); pkg_close(ce);
}


static void
test_pkg_borrowed_socket(void)
{
    struct pkg_conn *parent = nullptr;
    struct pkg_conn *child = nullptr;
    struct pkg_conn *borrowed = nullptr;
    const char payload = 'b';
    char received = 0;

    int rc = pkg_pair_prefer(&parent, &child, NULL, NULL,
	PKG_TRANSPORT_TCP);
    TEST("TCP pair for borrowed socket",
	rc == 0 && parent != nullptr && child != nullptr);
    if (rc != 0 || !parent || !child) {
	if (parent) pkg_close(parent);
	if (child) pkg_close(child);
	return;
    }

    borrowed = pkg_borrow_socket(pkg_get_read_fd(parent), NULL, NULL);
    TEST("borrow framework-owned socket",
	borrowed != PKC_ERROR && borrowed != PKC_NULL);
    if (borrowed != PKC_ERROR && borrowed != PKC_NULL)
	pkg_close(borrowed);

    TEST("borrowed wrapper leaves socket usable",
	pkg_send(102, &payload, sizeof(payload), child) == 1 &&
	pkg_waitfor(102, &received, sizeof(received), parent) == 1 &&
	received == payload);

    pkg_close(parent);
    pkg_close(child);
}


static void
test_pkg_framed_traffic(void)
{
    static const int message_count = 128;
    static const size_t payload_size = 2048;
    static const int request_type = 100;
    static const int reply_type = 101;
    char request[payload_size];
    char reply[payload_size];
    memset(request, 0x5a, sizeof(request));
    memset(reply, 0xa5, sizeof(reply));

    struct pkg_conn *parent = nullptr;
    struct pkg_conn *child = nullptr;
    int rc = pkg_pair_prefer(&parent, &child, NULL, NULL,
        PKG_TRANSPORT_AUTO);
    TEST("pkg_pair_prefer for framed traffic",
        rc == 0 && parent != nullptr && child != nullptr);
    if (rc != 0 || !parent || !child) {
        if (parent) pkg_close(parent);
        if (child) pkg_close(child);
        return;
    }

    bool sends_ok = true;
    bool reverse_input_polled = true;
    bool payloads_match = true;
    for (int i = 0; i < message_count; ++i) {
        if (pkg_2send(reply_type, reply, payload_size / 2,
                reply + payload_size / 2, payload_size / 2, child) !=
                (int)payload_size ||
            pkg_send(request_type, request, payload_size, parent) !=
                (int)payload_size) {
            sends_ok = false;
            break;
        }

        if (parent->pkc_inend <= parent->pkc_incur)
            reverse_input_polled = false;

        char *received_reply = pkg_bwaitfor(reply_type, parent);
        char *received_request = pkg_bwaitfor(request_type, child);
        if (!received_reply || !received_request ||
            memcmp(received_reply, reply, payload_size) != 0 ||
            memcmp(received_request, request, payload_size) != 0)
            payloads_match = false;
        free(received_reply);
        free(received_request);
        if (!payloads_match)
            break;
    }

    TEST("framed pipe traffic sends complete messages", sends_ok);
    TEST("send-side checkin polls reverse pipe traffic",
        reverse_input_polled);
    TEST("framed pipe traffic preserves payloads", payloads_match);
    pkg_close(parent);
    pkg_close(child);
}


static void
test_pkg_mux_many_connections(void)
{
    static const size_t connection_count = 72;
    std::vector<struct pkg_conn *> parents;
    std::vector<struct pkg_conn *> children;
    pkg_mux_t *mux = pkg_mux_create();
    bool setup_ok = mux != nullptr;

    for (size_t i = 0; setup_ok && i < connection_count; i++) {
        struct pkg_conn *parent = nullptr;
        struct pkg_conn *child = nullptr;
        setup_ok = pkg_pair_prefer(&parent, &child, NULL, NULL,
            PKG_TRANSPORT_TCP) == 0 && parent && child &&
            pkg_mux_add_conn(mux, parent) == 0;
        if (parent) parents.push_back(parent);
        if (child) children.push_back(child);
    }
    TEST("pkg_mux registers more than 64 socket connections", setup_ok);

    static const char byte = 'x';
    bool last_ready = false;
    if (setup_ok) {
        int count = pkg_send(999, &byte, 1, children.back());
        int ready = pkg_mux_wait(mux, 1000);
        last_ready = count == 1 && ready > 0 &&
            pkg_mux_is_ready_conn(mux, parents.back());
    }
    TEST("pkg_mux observes readiness beyond 64 sockets", last_ready);

    pkg_mux_destroy(mux);
    for (struct pkg_conn *connection : parents) pkg_close(connection);
    for (struct pkg_conn *connection : children) pkg_close(connection);
}


static void
test_pkg_mux_state(void)
{
    struct pkg_conn *parent = nullptr;
    struct pkg_conn *child = nullptr;
    pkg_mux_t *mux = pkg_mux_create();

    TEST("pkg_mux_wait rejects null mux", pkg_mux_wait(nullptr, 0) == -1);
    bool setup_ok = mux &&
        pkg_pair_prefer(&parent, &child, NULL, NULL, PKG_TRANSPORT_TCP) == 0 &&
        parent && child && pkg_mux_add_conn(mux, parent) == 0;
    TEST("pkg_mux stale-readiness setup succeeds", setup_ok);

    if (setup_ok) {
        static const char byte = 'x';
        int fd = pkg_get_read_fd(parent);
        bool ready = pkg_send(999, &byte, 1, child) == 1 &&
            pkg_mux_wait(mux, 1000) > 0 && pkg_mux_is_ready_fd(mux, fd);
        TEST("pkg_mux records ready descriptor", ready);
        pkg_mux_remove_fd(mux, fd);
        TEST("empty pkg_mux wait clears stale readiness",
            pkg_mux_wait(mux, 0) == 0 && !pkg_mux_is_ready_fd(mux, fd));
    }

    pkg_mux_destroy(mux);
    if (parent) pkg_close(parent);
    if (child) pkg_close(child);
}


#if !defined(_WIN32) && defined(HAVE_POLL_H) && defined(HAVE_SYS_TIME_H)
static volatile sig_atomic_t g_mux_signal_write_fd = -1;
static volatile sig_atomic_t g_mux_signal_wrote = 0;


static void
mux_alarm_handler(int UNUSED(signum))
{
    static const unsigned char byte = 'x';
    int fd = (int)g_mux_signal_write_fd;
    int saved_errno = errno;

    g_mux_signal_wrote = fd >= 0 && write(fd, &byte, sizeof(byte)) > 0;
    errno = saved_errno;
}


static void
test_pkg_mux_eintr(void)
{
    int pipe_fds[2] = {-1, -1};
    pkg_mux_t *mux = pkg_mux_create();
    struct sigaction action = {};
    struct sigaction previous_action = {};
    struct itimerval timer = {};
    bool setup_ok = mux && pipe(pipe_fds) == 0;
    bool handler_installed = false;

    if (setup_ok) {
        action.sa_handler = mux_alarm_handler;
        sigemptyset(&action.sa_mask);
        handler_installed = sigaction(SIGALRM, &action,
            &previous_action) == 0;
        setup_ok = handler_installed &&
            pkg_mux_add_fd(mux, pipe_fds[0], 0) == 0;
    }
    TEST("pkg_mux EINTR setup succeeds", setup_ok);

    bool resumed = false;
    if (setup_ok) {
        g_mux_signal_write_fd = pipe_fds[1];
        g_mux_signal_wrote = 0;
        timer.it_value.tv_usec = 50000;
        if (setitimer(ITIMER_REAL, &timer, NULL) == 0) {
            int ready = pkg_mux_wait(mux, -1);
            resumed = g_mux_signal_wrote && ready > 0 &&
                pkg_mux_is_ready_fd(mux, pipe_fds[0]);
        }
    }
    timer.it_value.tv_usec = 0;
    (void)setitimer(ITIMER_REAL, &timer, NULL);
    if (handler_installed)
        (void)sigaction(SIGALRM, &previous_action, NULL);
    g_mux_signal_write_fd = -1;
    TEST("infinite pkg_mux wait resumes after EINTR", resumed);

    if (pipe_fds[0] >= 0) close(pipe_fds[0]);
    if (pipe_fds[1] >= 0) close(pipe_fds[1]);
    pkg_mux_destroy(mux);
}
#endif


static void
test_pkg_ipc_addr_concurrency(void)
{
    static const size_t address_count = 64;
    std::vector<std::string> addresses(address_count);
    std::vector<int> statuses(address_count, -1);
    std::vector<std::thread> workers;

    workers.reserve(address_count);
    for (size_t i = 0; i < address_count; i++) {
        workers.emplace_back([&, i]() {
            char addr[MAXPATHLEN] = {0};
            statuses[i] = pkg_ipc_addr(addr, sizeof(addr), "regress-ipc-thread");
            if (statuses[i] == 0)
                addresses[i] = addr;
        });
    }
    for (std::thread &worker : workers)
        worker.join();

    bool available = statuses[0] == 0;
    bool consistent = true;
    std::set<std::string> unique_addresses;
    for (size_t i = 0; i < address_count; i++) {
        consistent = consistent && ((statuses[i] == 0) == available);
        if (available)
            consistent = consistent && !addresses[i].empty();
        if (!addresses[i].empty())
            unique_addresses.insert(addresses[i]);
    }
    TEST("concurrent pkg_ipc_addr availability is consistent", consistent);
    TEST("concurrent pkg_ipc_addr calls are unique",
        !available || unique_addresses.size() == address_count);
}


/* ================================================================== */
/* Group 3: pkg_connect_addr() via addr string                         */
/* ================================================================== */
static void
test_pkg_connect(void)
{
    if (g_verbose) fprintf(stdout, "\n[Group 3] pkg_connect_addr() via addr string\n");

    const char *invalid_addresses[] = {
        "pipe:999999,999998junk",
        "pipe:-1,4",
        "socket:4junk",
        "tcp:70000",
        "tcp:4444junk"
    };
    for (const char *invalid_address : invalid_addresses) {
        struct pkg_conn *invalid = pkg_connect_addr(invalid_address, NULL, NULL);
        TEST("pkg_connect_addr rejects malformed address", invalid == PKC_ERROR);
        if (invalid && invalid != PKC_ERROR)
            pkg_close(invalid);
    }

    struct pkg_conn *pe = nullptr, *ce = nullptr;
    if (pkg_pair_prefer(&pe, &ce, NULL, NULL, PKG_TRANSPORT_AUTO) != 0 || !pe || !ce) {
        TEST("pkg_pair_prefer for connect test", 0);
        if (pe) pkg_close(pe);
        if (ce) pkg_close(ce);
        return;
    }
    TEST("pkg_pair_prefer for connect test", 1);

    const char *addr = pkg_child_addr(ce);
    if (!addr && g_verbose)
        fprintf(stdout, "  pair transport is not exportable; skipping child-addr connect test\n");

    if (addr) {
        /* pkg_connect_addr parses the addr string and creates a new pkg_conn
         * wrapping the same inherited fds that ce holds.  After pkg_close(conn)
         * those fds are closed, so we must not also call pkg_close(ce).      */
        struct pkg_conn *conn = pkg_connect_addr(addr, NULL, NULL);
        TEST("pkg_connect_addr via child addr succeeds", conn != nullptr && conn != PKC_ERROR);

        if (conn && conn != PKC_ERROR) {
            int rfd = pkg_get_read_fd(conn);
            TEST("connected channel read fd >= 0", rfd >= 0);

            /* Write from parent (pe), read via connected channel (conn wraps ce's fds) */
            static const char msg[] = "CONNECT_OK";
            int pe_wfd = pkg_get_write_fd(pe);
            ssize_t n = ipc_fd_write(pe_wfd, msg, sizeof(msg)-1);
            TEST("parent write succeeds", n == (ssize_t)(sizeof(msg)-1));

            char buf[64]; memset(buf, 0, sizeof(buf));
            n = ipc_fd_read(rfd, buf, sizeof(msg)-1);
            TEST("connected-channel read succeeds", n == (ssize_t)(sizeof(msg)-1));
            TEST("connected-channel data matches", memcmp(buf, msg, sizeof(msg)-1) == 0);

            /* conn owns ce's fds — close conn and skip closing ce to avoid double-close */
            pkg_close(conn);
            ce = nullptr;
        }
    }

    struct pkg_conn *pe2 = nullptr, *ce2 = nullptr;
    if (pkg_pair_prefer(&pe2, &ce2, NULL, NULL, PKG_TRANSPORT_PIPE) != 0 || !pe2 || !ce2) {
        TEST("pkg_pair_prefer for parent connect test", 0);
        if (pe2) pkg_close(pe2);
        if (ce2) pkg_close(ce2);
        pkg_close(pe);
        return;
    }
    TEST("pkg_pair_prefer for parent connect test", 1);

    const char *parent_addr = pkg_child_addr(pe2);
    if (parent_addr) {
        struct pkg_conn *parent_conn = pkg_connect_addr(parent_addr, NULL, NULL);
        TEST("pkg_connect_addr via parent addr succeeds", parent_conn != nullptr && parent_conn != PKC_ERROR);

        if (parent_conn && parent_conn != PKC_ERROR) {
            int rfd = pkg_get_read_fd(parent_conn);
            TEST("parent-connected channel read fd >= 0", rfd >= 0);

            static const char msg[] = "PARENT_OK";
            int pe2_wfd = pkg_get_write_fd(parent_conn);
            ssize_t n = ipc_fd_write(pe2_wfd, msg, sizeof(msg)-1);
            TEST("parent write succeeds", n == (ssize_t)(sizeof(msg)-1));

            char buf[64]; memset(buf, 0, sizeof(buf));
            int ce2_rfd = pkg_get_read_fd(ce2);
            n = ipc_fd_read(ce2_rfd, buf, sizeof(msg)-1);
            TEST("child-side read succeeds", n == (ssize_t)(sizeof(msg)-1));
            TEST("parent-connected data matches", memcmp(buf, msg, sizeof(msg)-1) == 0);

            /* parent_conn owns pe2's fds — close it and skip closing pe2 to avoid double-close */
            pkg_close(parent_conn);
            parent_conn = nullptr;
            pe2 = nullptr;
        }
    } else if (g_verbose) {
        fprintf(stdout, "  pair transport is not exportable; skipping parent-addr connect test\n");
    }

    if (pe2) pkg_close(pe2);
    if (ce2) pkg_close(ce2);

    pkg_close(pe);
    if (ce) pkg_close(ce);
}


/* ================================================================== */
/* Group 4: fbs_open_ipc() smoke test                                  */
/* ================================================================== */
static int
run_ipc_listener_child(const char *addr, const char *send_msg, const char *expect_msg)
{
    struct pkg_conn *pc = pkg_connect_addr(addr, NULL, NULL);
    if (!pc || pc == PKC_ERROR)
        return 2;

    if (ipc_fd_write(pkg_get_write_fd(pc), send_msg, strlen(send_msg)) !=
            (ssize_t)strlen(send_msg)) {
        pkg_close(pc);
        return 3;
    }

    char buf[64] = {0};
    if (ipc_fd_read(pkg_get_read_fd(pc), buf, strlen(expect_msg)) !=
            (ssize_t)strlen(expect_msg)) {
        pkg_close(pc);
        return 4;
    }

    int ok = (memcmp(buf, expect_msg, strlen(expect_msg)) == 0);
    pkg_close(pc);
    return ok ? 0 : 5;
}

#ifndef _WIN32
static void
test_pkg_ipc_listener(void)
{
    if (g_verbose) fprintf(stdout, "\n[Group 9] reusable local IPC listener\n");

    char addr[MAXPATHLEN] = {0};
    int rc = pkg_ipc_addr(addr, sizeof(addr), "regress-ipc");
#ifdef HAVE_SYS_UN_H
    TEST("pkg_ipc_addr succeeds", rc == 0);
    TEST("pkg_ipc_addr returns listener address",
         rc == 0 && pkg_addr_is_ipc_listener(addr));
    TEST("pkg_ipc_addr returns UNIX-domain address",
         rc == 0 && (bu_strncmp(addr, "unix:", 5) == 0 ||
                     bu_strncmp(addr, "unix-abstract:", 14) == 0));
#else
    TEST("pkg_ipc_addr reports unavailable UNIX-domain listeners", rc != 0);
#endif

    if (rc != 0)
        return;

    pkg_listener_t *listener = pkg_listen(addr, NULL, 8, NULL);
    TEST("pkg_listen on local IPC address succeeds", listener != NULL);
    if (!listener)
        return;

    const char *client_msg[2] = {"IPC_CLIENT_ONE", "IPC_CLIENT_TWO"};
    const char *server_msg[2] = {"IPC_ACK_ONE", "IPC_ACK_TWO"};

    for (int i = 0; i < 2; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            TEST("fork client", 0);
            continue;
        }
        if (pid == 0) {
            _exit(run_ipc_listener_child(addr, client_msg[i], server_msg[i]));
        }

        struct pkg_conn *server = pkg_accept(listener, NULL, NULL, 0);
        TEST("pkg_accept local IPC client succeeds",
             server != NULL && server != PKC_ERROR);

        if (server && server != PKC_ERROR) {
            char buf[64] = {0};
            ssize_t n = ipc_fd_read(pkg_get_read_fd(server), buf, strlen(client_msg[i]));
            TEST("server reads local IPC client data",
                 n == (ssize_t)strlen(client_msg[i]));
            TEST("local IPC client data matches",
                 memcmp(buf, client_msg[i], strlen(client_msg[i])) == 0);

            n = ipc_fd_write(pkg_get_write_fd(server), server_msg[i], strlen(server_msg[i]));
            TEST("server writes local IPC reply",
                 n == (ssize_t)strlen(server_msg[i]));
            pkg_close(server);
        }

        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            TEST("wait for local IPC child", 0);
        } else {
            TEST("local IPC child exits cleanly",
                 WIFEXITED(status) && WEXITSTATUS(status) == 0);
        }
    }

    pkg_listener_close(listener);
}
#endif

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
        TEST("fbs_ipc_child_addr_env non-NULL",             ae != nullptr);
        TEST("fbs_ipc_child_addr_env has PKG_ADDR= prefix",
             ae && bu_strncmp(ae, "PKG_ADDR=", 9) == 0);
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

        /* fbsc_fd must be a valid (>= 0) file descriptor.
         * This is required by callers like Tcl_CreateFileHandler
         * and select()-based loops.              */
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


static void
test_embedded_fb_tls_refusal(void)
{
    struct fbserv_obj fbs;
    struct fbserv_client *server_client = nullptr;
    const char request = 1;
    char response = 1;

    init_fbs_noops(&fbs);
    TEST("fbs_open_ipc for TLS refusal",
	fbs_open_ipc(&fbs) == BRLCAD_OK);
    if (!fbs.fbs_listener.fbsl_ipc_child)
	return;

    for (int i = 0; i < MAX_CLIENTS; i++) {
	if (fbs.fbs_clients[i].fbsc_pkg) {
	    server_client = &fbs.fbs_clients[i];
	    break;
	}
    }
    TEST("embedded framebuffer server client found", server_client != nullptr);
    if (!server_client) {
	fbs_close(&fbs);
	return;
    }

    TEST("TLS capability request sent",
	pkg_send(MSG_FBTLS, &request, sizeof(request),
	    fbs.fbs_listener.fbsl_ipc_child) == (int)sizeof(request));
    fbs_existing_client_handler(server_client, 0);
    TEST("embedded framebuffer rejects TLS",
	pkg_waitfor(MSG_FBTLS, &response, sizeof(response),
	    fbs.fbs_listener.fbsl_ipc_child) == 1 && response == 0);
    TEST("TLS requester dropped after refusal", server_client->fbsc_pkg == nullptr);
    fbs_close(&fbs);
}


/* ================================================================== */
/* Group 5: PKG_ADDR env-var end-to-end (pkg IPC path)                */
/* ================================================================== */
static void
test_env_var_end_to_end(void)
{
    if (g_verbose) fprintf(stdout, "\n[Group 5] PKG_ADDR env-var end-to-end\n");

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
    bu_setenv(PKG_ADDR_ENVVAR, addr_val, 1);

    /* Simulate rem_open() in if_remote.c: getenv → pkg_connect_addr */
    const char *from_env = getenv(PKG_ADDR_ENVVAR);
    TEST("getenv(PKG_ADDR_ENVVAR) returns non-NULL", from_env != nullptr);
    TEST("getenv value matches addr_val",
         from_env && BU_STR_EQUAL(from_env, addr_val));

    if (from_env) {
        struct pkg_conn *child_conn = pkg_connect_addr(from_env, NULL, NULL);
        TEST("rem_open-style pkg_connect_addr succeeds",
             child_conn != nullptr && child_conn != PKC_ERROR);

        if (child_conn && child_conn != PKC_ERROR) {
            int child_rfd = pkg_get_read_fd(child_conn);
            TEST("child channel read fd >= 0", child_rfd >= 0);

            /* Pixel data: parent (fbserv side) → child (rt side) */
            static const char pkt[] = "PIXEL_PAYLOAD_0123456789";
            ssize_t n = ipc_fd_write(pwfd, pkt, sizeof(pkt)-1);
            TEST("parent pkg_conn write to child", n == (ssize_t)(sizeof(pkt)-1));

            char buf[64]; memset(buf, 0, sizeof(buf));
            n = ipc_fd_read(child_rfd, buf, sizeof(pkt)-1);
            TEST("child reads pixel data", n == (ssize_t)(sizeof(pkt)-1));
            TEST("pixel data matches end-to-end",
                 memcmp(buf, pkt, sizeof(pkt)-1) == 0);

            /* conn wraps the same underlying fds as ce (inherited from the
             * address string), so closing conn closes those fds.  Clear
             * fbsl_ipc_child so fbs_close() does not double-close them.  */
            pkg_close(child_conn);
            fbs.fbs_listener.fbsl_ipc_child = nullptr;
        }
    }

    bu_setenv(PKG_ADDR_ENVVAR, "", 1);
    fbs_close(&fbs);
}


static int
run_inherited_ipc_child(void)
{
    static const char message[] = "INHERITED_IPC";
    struct pkg_conn *connection = pkg_connect_env(NULL, NULL);
    if (!connection || connection == PKC_ERROR)
        return 2;
    ssize_t written = ipc_fd_write(pkg_get_write_fd(connection), message,
        strlen(message));
    pkg_close(connection);
    return (written == (ssize_t)strlen(message)) ? 0 : 3;
}


static void
test_process_ipc_inheritance(const char *executable)
{
    static const char expected[] = "INHERITED_IPC";
    struct fbserv_obj fbs;
    init_fbs_noops(&fbs);
    TEST("fbs_open_ipc for process inheritance",
        fbs_open_ipc(&fbs) == BRLCAD_OK);
    if (!fbs.fbs_listener.fbsl_ipc_child)
        return;

    struct pkg_conn *parent = nullptr;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (fbs.fbs_clients[i].fbsc_is_ipc) {
            parent = fbs.fbs_clients[i].fbsc_pkg;
            break;
        }
    }
    TEST("process inheritance parent connection registered", parent != nullptr);
    if (!parent) {
        fbs_close(&fbs);
        return;
    }

    const char *env_overrides[2] = {fbs_ipc_child_addr_env(&fbs), nullptr};
    int inherit_fds[2] = {-1, -1};
    struct bu_process_create_info create_info = {};
    create_info.env_overrides = env_overrides;
    create_info.inherit_fd_count = fbs_ipc_child_fds(&fbs, inherit_fds);
    create_info.inherit_fds = inherit_fds;

    bool inheritance_flags_set = create_info.inherit_fd_count > 0;
    for (size_t i = 0; i < create_info.inherit_fd_count; i++) {
#ifdef _WIN32
        HANDLE handle = (HANDLE)_get_osfhandle(inherit_fds[i]);
        inheritance_flags_set = inheritance_flags_set &&
            handle != INVALID_HANDLE_VALUE;
        DWORD flags = 0;
        inheritance_flags_set = inheritance_flags_set &&
            GetHandleInformation(handle, &flags) &&
            (flags & HANDLE_FLAG_INHERIT);
#else
        int flags = fcntl(inherit_fds[i], F_GETFD);
        inheritance_flags_set = inheritance_flags_set && flags >= 0 &&
            fcntl(inherit_fds[i], F_SETFD, flags | FD_CLOEXEC) == 0;
#endif
    }
    TEST("IPC descriptors have required inheritance flags", inheritance_flags_set);

    const char *child_argv[] = {executable, "--inherited-ipc-child", nullptr};
    struct bu_process *process = nullptr;
    bu_process_create_ex(&process, child_argv, &create_info);

    bool inheritance_flags_restored = inheritance_flags_set;
    for (size_t i = 0; i < create_info.inherit_fd_count; i++) {
#ifdef _WIN32
        HANDLE handle = (HANDLE)_get_osfhandle(inherit_fds[i]);
        DWORD flags = 0;
        inheritance_flags_restored = inheritance_flags_restored &&
            handle != INVALID_HANDLE_VALUE && GetHandleInformation(handle, &flags) &&
            (flags & HANDLE_FLAG_INHERIT);
#else
        int flags = fcntl(inherit_fds[i], F_GETFD);
        inheritance_flags_restored = inheritance_flags_restored &&
            flags >= 0 && (flags & FD_CLOEXEC);
#endif
    }
    TEST("process creation preserves parent descriptor flags",
        inheritance_flags_restored);
    fbs_close_ipc_child(&fbs);
    TEST("IPC child process starts", process != nullptr);

    char message[sizeof(expected)] = {0};
    ssize_t received = process ? ipc_fd_read(pkg_get_read_fd(parent), message,
        strlen(expected)) : -1;
    TEST("IPC child process writes inherited channel",
        received == (ssize_t)strlen(expected));
    TEST("IPC child process payload matches",
        received > 0 && memcmp(message, expected, strlen(expected)) == 0);
    TEST("IPC child process exits cleanly",
        process && bu_process_wait_n(&process, 0) == 0);
    fbs_close(&fbs);
}


/* ================================================================== */
/* Group 6: Tcl_CreateFileHandler callback path                        */
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
         * To make it readable, write FROM the child end. */
        struct pkg_conn *ce = fbs.fbs_listener.fbsl_ipc_child;
        int ce_wfd = pkg_get_write_fd(ce);
        TEST("child write fd valid for Tcl trigger", ce_wfd >= 0);

        if (ce_wfd >= 0) {
            static const char msg[] = "TCL_HANDLER_TRIGGER";
            ssize_t n = ipc_fd_write(ce_wfd, msg, sizeof(msg)-1);
            TEST("write from child end succeeds", n == (ssize_t)(sizeof(msg)-1));

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
/* Group 7: $fbserv(ipc_addr) Tcl variable path                        */
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
/* Tcl timer with libpkg readiness polling instead.  This test verifies: */
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
        int ipc_client = -1;
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (fbs.fbs_clients[i].fbsc_is_ipc && fbs.fbs_clients[i].fbsc_chan) {
                timer_installed = true;
                ipc_client = i;
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

        fbs_close_ipc_child(&fbs);
        int64_t deadline = bu_gettime() + BU_SEC2USEC(1);
        while (ipc_client >= 0 && fbs.fbs_clients[ipc_client].fbsc_fd != 0 &&
               bu_gettime() < deadline) {
            while (Tcl_DoOneEvent(TCL_DONT_WAIT)) {}
            bu_snooze(1000);
        }
        TEST("Windows IPC poll retires a disconnected client",
             ipc_client >= 0 && fbs.fbs_clients[ipc_client].fbsc_fd == 0);

        bool sessions_retired = true;
        for (int session = 0; session < MAX_CLIENTS + 8; ++session) {
            if (tclcad_listen_ipc(&fbs, interp) != BRLCAD_OK) {
                sessions_retired = false;
                break;
            }
            ipc_client = -1;
            for (int i = 0; i < MAX_CLIENTS; ++i) {
                if (fbs.fbs_clients[i].fbsc_is_ipc) {
                    ipc_client = i;
                    break;
                }
            }
            fbs_close_ipc_child(&fbs);
            deadline = bu_gettime() + BU_SEC2USEC(1);
            while (ipc_client >= 0 &&
                   fbs.fbs_clients[ipc_client].fbsc_fd != 0 &&
                   bu_gettime() < deadline) {
                while (Tcl_DoOneEvent(TCL_DONT_WAIT)) {}
                bu_snooze(1000);
            }
            if (ipc_client < 0 || fbs.fbs_clients[ipc_client].fbsc_fd != 0) {
                sessions_retired = false;
                break;
            }
        }
        TEST("repeated Windows IPC sessions do not exhaust client slots",
             sessions_retired);

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



/* Group 8: fbserv -I with an inherited IPC endpoint                  */
/* ================================================================== */
static void
test_fbserv_minus_I(const char *fbserv_bin)
{
    if (g_verbose) fprintf(stdout, "\n[Group 8] fbserv -I <addr> flag\n");

    struct pkg_conn *parent = nullptr;
    struct pkg_conn *child = nullptr;
    bool paired = pkg_pair_prefer(&parent, &child, NULL, NULL,
        PKG_TRANSPORT_PIPE) == 0 && parent && child;
    TEST("pkg_pair_prefer for fbserv -I test", paired);
    if (!paired) {
        if (parent) pkg_close(parent);
        if (child) pkg_close(child);
        return;
    }

    const char *args[] = {fbserv_bin, "-F", "/dev/null", "-I",
        pkg_child_addr(child), nullptr};
    int inherit_fds[] = {pkg_get_read_fd(child), pkg_get_write_fd(child)};
    struct bu_process_create_info create_info = {};
    create_info.opts = BU_PROCESS_OUT_EQ_ERR | BU_PROCESS_HIDE_WINDOW;
    create_info.inherit_fds = inherit_fds;
    create_info.inherit_fd_count = inherit_fds[0] == inherit_fds[1] ? 1 : 2;

    struct bu_process *process = nullptr;
    bu_process_create_ex(&process, args, &create_info);
    pkg_close(child);
    /* The server exits after it attaches and reads EOF from its peer. */
    pkg_close(parent);
    TEST("fbserv -I starts as a subprocess", process != nullptr);
    if (!process)
        return;

    const int cleanup_timeout_ms = 1000;
    int64_t deadline = bu_gettime() + BU_SEC2USEC(5);
    int result;
    int exit_status = -1;
    while ((result = bu_process_poll(process, &exit_status)) == 0 &&
           bu_gettime() < deadline) {
        bu_snooze(1000);
    }
    if (result == 1 && exit_status != 0) {
        char output[1024];
        int count = bu_process_read_n(process, BU_PROCESS_STDOUT, sizeof(output), output);
        if (count > 0)
            fprintf(stderr, "fbserv -I output: %.*s\n", count, output);
    }
    (void)bu_process_wait_n(&process, cleanup_timeout_ms);
    TEST("fbserv -I attaches and exits successfully at EOF", result == 1 && exit_status == 0);
}

#ifdef _WIN32
static void
test_pkg_ipc_listener_win(void)
{
    if (g_verbose) fprintf(stdout, "\n[Group 9] reusable local IPC listener (Windows named pipe)\n");

    char addr[MAXPATHLEN] = {0};
    int rc = pkg_ipc_addr(addr, sizeof(addr), "regress-ipc");
    TEST("pkg_ipc_addr succeeds", rc == 0);
    TEST("pkg_ipc_addr returns listener address",
         rc == 0 && pkg_addr_is_ipc_listener(addr));
    TEST("pkg_ipc_addr returns npipe address",
         rc == 0 && bu_strncmp(addr, "npipe:", 6) == 0);

    if (rc != 0)
        return;

    pkg_listener_t *listener = pkg_listen(addr, NULL, 8, NULL);
    TEST("pkg_listen on npipe IPC address succeeds", listener != NULL);
    if (!listener)
        return;

    struct pkg_conn *pending = pkg_accept(listener, NULL, NULL, 1);
    TEST("nonblocking npipe accept reports no pending client",
	 pending == PKC_NULL);

    const char *client_msg[2] = {"IPC_CLIENT_ONE", "IPC_CLIENT_TWO"};
    const char *server_msg[2] = {"IPC_ACK_ONE", "IPC_ACK_TWO"};

    for (int i = 0; i < 2; i++) {
        int client_rc = -1;
        std::thread client([&]() {
            client_rc = run_ipc_listener_child(addr, client_msg[i], server_msg[i]);
        });

        struct pkg_conn *server = PKC_NULL;
        if (i == 0) {
            int64_t deadline = bu_gettime() + BU_SEC2USEC(1);
            while (server == PKC_NULL && bu_gettime() < deadline) {
                server = pkg_accept(listener, NULL, NULL, 1);
                if (server == PKC_NULL)
                    bu_snooze(1000);
            }
        } else {
            server = pkg_accept(listener, NULL, NULL, 0);
        }
        TEST(i == 0 ? "nonblocking accept returns pending npipe client" :
             "blocking accept returns local IPC client",
             server != NULL && server != PKC_ERROR);

        if (!server || server == PKC_ERROR) {
            pkg_listener_close(listener);
            listener = NULL;
            client.join();
            TEST("Windows npipe client exits after accept failure", client_rc != -1);
            break;
        }

        {
            char buf[64] = {0};
            ssize_t n = ipc_fd_read(pkg_get_read_fd(server), buf, strlen(client_msg[i]));
            TEST("server reads local IPC client data",
                 n == (ssize_t)strlen(client_msg[i]));
            TEST("server local IPC client data matches",
                 memcmp(buf, client_msg[i], strlen(client_msg[i])) == 0);

            n = ipc_fd_write(pkg_get_write_fd(server),
                             server_msg[i], strlen(server_msg[i]));
            TEST("server writes local IPC reply",
                 n == (ssize_t)strlen(server_msg[i]));
            pkg_close(server);
        }

        client.join();
        TEST("Windows npipe client round-trip succeeds", client_rc == 0);
    }

    if (listener)
        pkg_listener_close(listener);
}
#endif /* _WIN32 */


/* ================================================================== */
/* main                                                               */
/* ================================================================== */
int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);

    if (argc == 2 && BU_STR_EQUAL(argv[1], "--inherited-ipc-child"))
        return run_inherited_ipc_child();

#ifdef SIGPIPE
    /* Ignore SIGPIPE: writes to closed peers must not crash the harness */
    signal(SIGPIPE, SIG_IGN);
#endif

    for (int i = 1; i < argc; ++i)
        if (BU_STR_EQUAL(argv[i], "--verbose") ||
            BU_STR_EQUAL(argv[i], "-v"))
            g_verbose = true;

    struct bu_vls fbserv_bin = BU_VLS_INIT_ZERO;
    const char *fbserv_env = getenv("FBSERV_BIN");
    if (fbserv_env)
        bu_vls_strcpy(&fbserv_bin, fbserv_env);
    if (!bu_vls_strlen(&fbserv_bin)) {
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
                struct bu_vls candidate = BU_VLS_INIT_ZERO;
                bu_vls_sprintf(&candidate, "%s/../bin/fbserv", fbserv_path);
                if (access(bu_vls_cstr(&candidate), X_OK) == 0)
                    bu_vls_sprintf(&fbserv_bin, "%s", bu_vls_cstr(&candidate));
		bu_vls_free(&candidate);
            }
        }
    }
    if (!bu_vls_strlen(&fbserv_bin))
        bu_vls_sprintf(&fbserv_bin, "%s", "fbserv");   /* rely on PATH */

    if (g_verbose) {
        fprintf(stdout, "\n==============================================\n");
        fprintf(stdout, "  pkg IPC / fbs_open_ipc Regression Tests\n");
        fprintf(stdout, "==============================================\n");
        fprintf(stdout, "  fbserv binary: %s\n\n", bu_vls_cstr(&fbserv_bin));
    }

    test_pkg_pair_basics();
    test_pkg_roundtrip();
    test_pkg_borrowed_socket();
    test_pkg_framed_traffic();
    test_pkg_mux_many_connections();
    test_pkg_mux_state();
#if !defined(_WIN32) && defined(HAVE_POLL_H) && defined(HAVE_SYS_TIME_H)
    test_pkg_mux_eintr();
#endif
    test_pkg_ipc_addr_concurrency();
    test_pkg_connect();
    test_fbs_open_ipc_smoke();
    test_embedded_fb_tls_refusal();
    test_env_var_end_to_end();
    test_process_ipc_inheritance(argv[0]);
#ifndef _WIN32
    test_pkg_ipc_listener();
    test_tcl_file_handler();
    test_tcl_fbserv_var();
#else
    test_pkg_ipc_listener_win();
    if (g_verbose)
        fprintf(stdout, "\n[Group 6] SKIPPED (Tcl_CreateFileHandler not available on Windows)\n");
    test_tcl_listen_ipc_win();  /* Group 7: Windows timer-IPC path */
#endif
    test_fbserv_minus_I(bu_vls_cstr(&fbserv_bin));

    fprintf(stdout, "\n==============================================\n");
    fprintf(stdout, "  Results: %d / %d passed\n", g_tests_pass, g_tests_run);
    fprintf(stdout, "==============================================\n\n");

    bu_vls_free(&fbserv_bin);

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
