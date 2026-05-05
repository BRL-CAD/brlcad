/*                  P K G _ T R A N S P O R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025-2026 United States Government as represented by
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
/** @file libpkg/pkg_transport.cpp
 *
 * Transport probe, pair, connect, mux, and fd-move implementations.
 */

#include "common.h"

#include "bio.h"
#include "bsocket.h"
#include "bnetwork.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <vector>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#  include <sys/socket.h>
#endif
#ifdef HAVE_SYS_UN_H
#  include <sys/un.h>
#endif
#ifdef HAVE_NETINET_IN_H
#  include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#  include <arpa/inet.h>
#endif
#ifndef _WIN32
#  include <unistd.h>
#  include <fcntl.h>
#  ifdef HAVE_SYS_SELECT_H
#    include <sys/select.h>
#  endif
#endif

#include "pkg.h"

/* Internal pkg.c helpers used by this translation unit (not public API). */
extern "C" {
struct pkg_conn *pkg_open_fds(int rfd, int wfd,
                              const struct pkg_switch *switchp,
                              pkg_errlog errlog);
}


/* ================================================================== */
/* Internal helpers                                                     */
/* ================================================================== */

static void
_pkg_build_addr_env(struct pkg_conn *pc)
{
    snprintf(pc->pkc_addr_env, sizeof(pc->pkc_addr_env),
             "%s=%s", PKG_ADDR_ENVVAR, pc->pkc_addr);
}


/* ================================================================== */
/* Transport 1: anonymous pipe                                          */
/* ================================================================== */

#ifndef _WIN32
static int
_try_pipe(struct pkg_conn **parent_end, struct pkg_conn **child_end,
          const struct pkg_switch *switchp, pkg_errlog errlog)
{
    int pc[2], cp[2];   /* pc: parent->child,  cp: child->parent */
    if (pipe(pc) < 0) return -1;
    if (pipe(cp) < 0) { close(pc[0]); close(pc[1]); return -1; }

    /* Parent: reads from cp[0], writes to pc[1] */
    struct pkg_conn *pe = pkg_open_fds(cp[0], pc[1], switchp, errlog);
    if (pe == PKC_ERROR || pe == PKC_NULL) {
        close(pc[0]); close(pc[1]); close(cp[0]); close(cp[1]);
        return -1;
    }

    /* Child: reads from pc[0], writes to cp[1] */
    struct pkg_conn *ce = pkg_open_fds(pc[0], cp[1], switchp, errlog);
    if (ce == PKC_ERROR || ce == PKC_NULL) {
        pkg_close(pe);
        close(pc[0]); close(cp[1]);
        return -1;
    }

    snprintf(ce->pkc_addr, sizeof(ce->pkc_addr), "pipe:%d,%d", pc[0], cp[1]);
    _pkg_build_addr_env(ce);
    snprintf(pe->pkc_addr, sizeof(pe->pkc_addr), "pipe_parent:%d,%d", cp[0], pc[1]);
    _pkg_build_addr_env(pe);

    *parent_end = pe;
    *child_end  = ce;
    return 0;
}
#else /* _WIN32 */
static int
_try_pipe(struct pkg_conn **parent_end, struct pkg_conn **child_end,
          const struct pkg_switch *switchp, pkg_errlog errlog)
{
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength              = sizeof(sa);
    sa.bInheritHandle       = TRUE;

    HANDLE hReadPC, hWritePC;
    HANDLE hReadCP, hWriteCP;
    if (!CreatePipe(&hReadPC, &hWritePC, &sa, 0) ||
            !CreatePipe(&hReadCP, &hWriteCP, &sa, 0))
        return -1;

    SetHandleInformation(hWritePC, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hReadCP,  HANDLE_FLAG_INHERIT, 0);

    int pc_r = _open_osfhandle((intptr_t)hReadPC,  _O_RDONLY | _O_BINARY);
    int pc_w = _open_osfhandle((intptr_t)hWritePC, _O_WRONLY | _O_BINARY);
    int cp_r = _open_osfhandle((intptr_t)hReadCP,  _O_RDONLY | _O_BINARY);
    int cp_w = _open_osfhandle((intptr_t)hWriteCP, _O_WRONLY | _O_BINARY);

    struct pkg_conn *pe = pkg_open_fds(cp_r, pc_w, switchp, errlog);
    if (pe == PKC_ERROR || pe == PKC_NULL) return -1;
    struct pkg_conn *ce = pkg_open_fds(pc_r, cp_w, switchp, errlog);
    if (ce == PKC_ERROR || ce == PKC_NULL) { pkg_close(pe); return -1; }

    snprintf(ce->pkc_addr, sizeof(ce->pkc_addr),
             "pipe_win:%I64x,%I64x",
             (unsigned __int64)(intptr_t)hReadPC,
             (unsigned __int64)(intptr_t)hWriteCP);
    _pkg_build_addr_env(ce);
    snprintf(pe->pkc_addr, sizeof(pe->pkc_addr), "pipe_parent:%d,%d", cp_r, pc_w);
    _pkg_build_addr_env(pe);

    *parent_end = pe;
    *child_end  = ce;
    return 0;
}
#endif /* _WIN32 */


/* ================================================================== */
/* Transport 2: socketpair (POSIX only)                                 */
/* ================================================================== */

#if defined(HAVE_SYS_UN_H) && !defined(_WIN32)
static int
_try_socketpair(struct pkg_conn **parent_end, struct pkg_conn **child_end,
                const struct pkg_switch *switchp, pkg_errlog errlog)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) return -1;

    struct pkg_conn *pe = pkg_open_fds(sv[0], sv[0], switchp, errlog);
    if (pe == PKC_ERROR || pe == PKC_NULL) {
        close(sv[0]); close(sv[1]); return -1;
    }
    struct pkg_conn *ce = pkg_open_fds(sv[1], sv[1], switchp, errlog);
    if (ce == PKC_ERROR || ce == PKC_NULL) {
        pkg_close(pe); close(sv[1]); return -1;
    }

    snprintf(ce->pkc_addr, sizeof(ce->pkc_addr), "socket:%d", sv[1]);
    _pkg_build_addr_env(ce);
    snprintf(pe->pkc_addr, sizeof(pe->pkc_addr), "socket_parent:%d", sv[0]);
    _pkg_build_addr_env(pe);

    *parent_end = pe;
    *child_end  = ce;
    return 0;
}
#endif


/* ================================================================== */
/* Transport 3: TCP loopback (synchronous connect + accept)             */
/* ================================================================== */

static int
_try_tcp(struct pkg_conn **parent_end, struct pkg_conn **child_end,
         const struct pkg_switch *switchp, pkg_errlog errlog)
{
    int lsock, csock, psock;
    struct sockaddr_in sa, bound, csa, peer;
    socklen_t blen, plen;
    int port;
    int opt = 1;

#ifdef _WIN32
    {   WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa); }
    lsock = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (lsock == (int)INVALID_SOCKET) return -1;
#else
    lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock < 0) return -1;
#endif
    setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    memset(&sa, 0, sizeof(sa));
    sa.sin_family      = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sa.sin_port        = 0;
    if (bind(lsock, (struct sockaddr*)&sa, sizeof(sa)) < 0 ||
            listen(lsock, 1) < 0) {
#ifdef _WIN32
        closesocket((SOCKET)lsock);
#else
        close(lsock);
#endif
        return -1;
    }

    blen = sizeof(bound);
    memset(&bound, 0, sizeof(bound));
    getsockname(lsock, (struct sockaddr*)&bound, &blen);
    port = ntohs(bound.sin_port);

#ifdef _WIN32
    csock = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (csock == (int)INVALID_SOCKET) { closesocket((SOCKET)lsock); return -1; }
#else
    csock = socket(AF_INET, SOCK_STREAM, 0);
    if (csock < 0) { close(lsock); return -1; }
#endif

    memset(&csa, 0, sizeof(csa));
    csa.sin_family      = AF_INET;
    csa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    csa.sin_port        = htons((uint16_t)port);
    if (connect(csock, (struct sockaddr*)&csa, sizeof(csa)) < 0) {
#ifdef _WIN32
        closesocket((SOCKET)csock); closesocket((SOCKET)lsock);
#else
        close(csock); close(lsock);
#endif
        return -1;
    }

    plen = sizeof(peer);
    memset(&peer, 0, sizeof(peer));
#ifdef _WIN32
    psock = (int)accept((SOCKET)lsock, (struct sockaddr*)&peer, &plen);
    closesocket((SOCKET)lsock);
    if (psock == (int)INVALID_SOCKET) { closesocket((SOCKET)csock); return -1; }
#else
    psock = accept(lsock, (struct sockaddr*)&peer, &plen);
    close(lsock);
    if (psock < 0) { close(csock); return -1; }
#endif

    struct pkg_conn *pe = pkg_open_fds(psock, psock, switchp, errlog);
    if (pe == PKC_ERROR || pe == PKC_NULL) {
#ifdef _WIN32
        closesocket((SOCKET)psock); closesocket((SOCKET)csock);
#else
        close(psock); close(csock);
#endif
        return -1;
    }
    struct pkg_conn *ce = pkg_open_fds(csock, csock, switchp, errlog);
    if (ce == PKC_ERROR || ce == PKC_NULL) {
        pkg_close(pe);
#ifdef _WIN32
        closesocket((SOCKET)csock);
#else
        close(csock);
#endif
        return -1;
    }

    snprintf(ce->pkc_addr, sizeof(ce->pkc_addr), "tcp:%d", port);
    _pkg_build_addr_env(ce);
    snprintf(pe->pkc_addr, sizeof(pe->pkc_addr), "tcp_server:%d", port);
    _pkg_build_addr_env(pe);

    *parent_end = pe;
    *child_end  = ce;
    return 0;
}


/* ================================================================== */
/* Probe order helper                                                   */
/* ================================================================== */

static int
_try_one(pkg_transport_t t, struct pkg_conn **pe, struct pkg_conn **ce,
         const struct pkg_switch *switchp, pkg_errlog errlog)
{
    switch (t) {
        case PKG_TRANSPORT_PIPE:
            return _try_pipe(pe, ce, switchp, errlog);
#if defined(HAVE_SYS_UN_H) && !defined(_WIN32)
        case PKG_TRANSPORT_SOCKET:
            return _try_socketpair(pe, ce, switchp, errlog);
#endif
        case PKG_TRANSPORT_TCP:
            return _try_tcp(pe, ce, switchp, errlog);
        default:
            return -1;
    }
}

static pkg_transport_t
_read_prefer_env(void)
{
    const char *v = getenv(PKG_TRANSPORT_PREFER_ENVVAR);
    if (!v) return PKG_TRANSPORT_AUTO;
    if (strcmp(v, "pipe")   == 0) return PKG_TRANSPORT_PIPE;
    if (strcmp(v, "socket") == 0) return PKG_TRANSPORT_SOCKET;
    if (strcmp(v, "tcp")    == 0) return PKG_TRANSPORT_TCP;
    return PKG_TRANSPORT_AUTO;
}


/* ================================================================== */
/* Public: pkg_pair / pkg_pair_prefer                                   */
/* ================================================================== */

int
pkg_pair_prefer(struct pkg_conn **parent_end, struct pkg_conn **child_end,
                const struct pkg_switch *switchp, pkg_errlog errlog,
                pkg_transport_t preferred)
{
    pkg_transport_t order[3];
    int i, pos;

    if (!parent_end || !child_end) return -1;

    order[0] = PKG_TRANSPORT_PIPE;
    order[1] = PKG_TRANSPORT_SOCKET;
    order[2] = PKG_TRANSPORT_TCP;

    if (preferred > PKG_TRANSPORT_AUTO) {
        pos = 0;
        for (i = 0; i < 3; ++i)
            if (order[i] == preferred) { pos = i; break; }
        if (pos > 0) {
            pkg_transport_t tmp = order[0];
            order[0] = order[pos];
            order[pos] = tmp;
        }
    }

    for (i = 0; i < 3; ++i) {
        if (_try_one(order[i], parent_end, child_end, switchp, errlog) == 0)
            return 0;
    }
    return -1;
}

int
pkg_pair(struct pkg_conn **parent_end, struct pkg_conn **child_end,
         const struct pkg_switch *switchp, pkg_errlog errlog)
{
    return pkg_pair_prefer(parent_end, child_end, switchp, errlog, _read_prefer_env());
}


/* ================================================================== */
/* Address API                                                          */
/* ================================================================== */

const char *
pkg_child_addr_env(struct pkg_conn *pc)
{
    if (!pc || pc == PKC_ERROR) return NULL;
    if (pc->pkc_addr[0] != '\0' &&
            (pc->pkc_addr_env[0] == '\0' ||
             strstr(pc->pkc_addr_env, pc->pkc_addr) == NULL)) {
        _pkg_build_addr_env(pc);
    }
    return (pc->pkc_addr_env[0] != '\0') ? pc->pkc_addr_env : NULL;
}

const char *
pkg_child_addr(struct pkg_conn *pc)
{
    if (!pc || pc == PKC_ERROR) return NULL;
    return (pc->pkc_addr[0] != '\0') ? pc->pkc_addr : NULL;
}


/* ================================================================== */
/* pkg_connect_addr / pkg_connect_env                                   */
/* ================================================================== */

struct pkg_conn *
pkg_connect_addr(const char *addr, const struct pkg_switch *switchp, pkg_errlog errlog)
{
    if (!addr) return PKC_ERROR;

#ifdef _WIN32
    if (strncmp(addr, "pipe_win:", 9) == 0) {
        unsigned __int64 hr64 = 0, hw64 = 0;
        if (sscanf(addr + 9, "%I64x,%I64x", &hr64, &hw64) == 2) {
            HANDLE hRead  = (HANDLE)(intptr_t)hr64;
            HANDLE hWrite = (HANDLE)(intptr_t)hw64;
            int rfd = _open_osfhandle((intptr_t)hRead,  _O_RDONLY | _O_BINARY);
            int wfd = _open_osfhandle((intptr_t)hWrite, _O_WRONLY | _O_BINARY);
            if (rfd >= 0 && wfd >= 0) {
                struct pkg_conn *pc = pkg_adopt_fds(rfd, wfd, switchp, errlog);
                if (pc != PKC_ERROR && pc != PKC_NULL) {
                    snprintf(pc->pkc_addr, sizeof(pc->pkc_addr), "%s", addr);
                    _pkg_build_addr_env(pc);
                }
                return pc;
            }
            if (rfd >= 0) _close(rfd);
            if (wfd >= 0) _close(wfd);
        }
        return PKC_ERROR;
    }
#endif

    if (strncmp(addr, "pipe:", 5) == 0) {
        int rfd = -1, wfd = -1;
        if (sscanf(addr + 5, "%d,%d", &rfd, &wfd) == 2) {
            struct pkg_conn *pc = pkg_adopt_fds(rfd, wfd, switchp, errlog);
            if (pc != PKC_ERROR && pc != PKC_NULL) {
                snprintf(pc->pkc_addr, sizeof(pc->pkc_addr), "%s", addr);
                _pkg_build_addr_env(pc);
            }
            return pc;
        }
        return PKC_ERROR;
    }

    if (strncmp(addr, "socket:", 7) == 0) {
        int sfd = -1;
        if (sscanf(addr + 7, "%d", &sfd) == 1) {
            struct pkg_conn *pc = pkg_adopt_fds(sfd, sfd, switchp, errlog);
            if (pc != PKC_ERROR && pc != PKC_NULL) {
                snprintf(pc->pkc_addr, sizeof(pc->pkc_addr), "%s", addr);
                _pkg_build_addr_env(pc);
            }
            return pc;
        }
        return PKC_ERROR;
    }

    if (strncmp(addr, "tcp:", 4) == 0) {
        int port = 0;
        if (sscanf(addr + 4, "%d", &port) == 1) {
            int csock;
            struct sockaddr_in csa;
#ifdef _WIN32
            {   WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa); }
            csock = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (csock == (int)INVALID_SOCKET) return PKC_ERROR;
#else
            csock = socket(AF_INET, SOCK_STREAM, 0);
            if (csock < 0) return PKC_ERROR;
#endif
            memset(&csa, 0, sizeof(csa));
            csa.sin_family      = AF_INET;
            csa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            csa.sin_port        = htons((uint16_t)port);
            if (connect(csock, (struct sockaddr*)&csa, sizeof(csa)) < 0) {
#ifdef _WIN32
                closesocket((SOCKET)csock);
#else
                close(csock);
#endif
                return PKC_ERROR;
            }
            struct pkg_conn *pc = pkg_adopt_fds(csock, csock, switchp, errlog);
            if (pc != PKC_ERROR && pc != PKC_NULL) {
                snprintf(pc->pkc_addr, sizeof(pc->pkc_addr), "%s", addr);
                _pkg_build_addr_env(pc);
            }
            return pc;
        }
        return PKC_ERROR;
    }

    return PKC_ERROR;
}

struct pkg_conn *
pkg_connect_env(const struct pkg_switch *switchp, pkg_errlog errlog)
{
    const char *addr = getenv(PKG_ADDR_ENVVAR);
    if (!addr) return PKC_ERROR;
    return pkg_connect_addr(addr, switchp, errlog);
}


/* ================================================================== */
/* pkg_adopt_fds / pkg_adopt_stdio                                      */
/* ================================================================== */

struct pkg_conn *
pkg_adopt_fds(int rfd, int wfd, const struct pkg_switch *switchp, pkg_errlog errlog)
{
    return pkg_open_fds(rfd, wfd, switchp, errlog);
}

struct pkg_conn *
pkg_adopt_stdio(const struct pkg_switch *switchp, pkg_errlog errlog)
{
    return pkg_open_fds(STDIN_FILENO, STDOUT_FILENO, switchp, errlog);
}


/* ================================================================== */
/* pkg_move_high_fd                                                     */
/* ================================================================== */

int
pkg_move_high_fd(struct pkg_conn *pc, int min_fd)
{
    if (!pc || pc == PKC_ERROR || min_fd < 3) return -1;

#ifdef _WIN32
    (void)min_fd;
    return 0;
#else
    if (pc->pkc_tx_kind != 1) {
        /* Socket/TCP: move pkc_fd */
        if (pc->pkc_fd >= 0 && pc->pkc_fd < min_fd) {
            int nfd = fcntl(pc->pkc_fd, F_DUPFD, min_fd);
            if (nfd < 0) return -1;
            close(pc->pkc_fd);
            pc->pkc_fd     = nfd;
            pc->pkc_in_fd  = nfd;
            pc->pkc_out_fd = nfd;
        }
        return 0;
    }

    /* Pipe transport: move read fd */
    if (pc->pkc_in_fd >= 0 && pc->pkc_in_fd < min_fd) {
        int nfd = fcntl(pc->pkc_in_fd, F_DUPFD, min_fd);
        if (nfd < 0) return -1;
        if (pc->pkc_out_fd == pc->pkc_in_fd)
            pc->pkc_out_fd = nfd;
        close(pc->pkc_in_fd);
        pc->pkc_in_fd = nfd;
    }

    /* Pipe transport: move write fd (if different) */
    if (pc->pkc_out_fd >= 0 && pc->pkc_out_fd < min_fd &&
            pc->pkc_out_fd != pc->pkc_in_fd)
    {
        int nfd = fcntl(pc->pkc_out_fd, F_DUPFD, min_fd);
        if (nfd < 0) return -1;
        close(pc->pkc_out_fd);
        pc->pkc_out_fd = nfd;
    }

    /* Update address string with new fd numbers */
    if (pc->pkc_addr[0] != '\0') {
        snprintf(pc->pkc_addr, sizeof(pc->pkc_addr), "pipe:%d,%d",
                 pc->pkc_in_fd, pc->pkc_out_fd);
        _pkg_build_addr_env(pc);
    }

    return 0;
#endif
}


/* ================================================================== */
/* pkg_mux_t: cross-platform read-readiness multiplexer                */
/* ================================================================== */

#ifndef _WIN32

struct pkg_mux {
    fd_set watch;
    fd_set ready;
    int maxfd;
};

pkg_mux_t *
pkg_mux_create(void)
{
    pkg_mux_t *m = new pkg_mux_t();
    FD_ZERO(&m->watch);
    FD_ZERO(&m->ready);
    m->maxfd = -1;
    return m;
}

void
pkg_mux_destroy(pkg_mux_t *m)
{
    if (!m) return;
    delete m;
}

int
pkg_mux_add_fd(pkg_mux_t *m, int fd, int /*is_socket*/)
{
    if (!m || fd < 0) return -1;
    if (FD_ISSET(fd, &m->watch)) return 0;
    FD_SET(fd, &m->watch);
    if (fd > m->maxfd) m->maxfd = fd;
    return 0;
}

int
pkg_mux_add_conn(pkg_mux_t *m, const struct pkg_conn *pc)
{
    if (!m || !pc || pc == PKC_ERROR) return -1;
    int fd = pkg_get_read_fd(pc);
    return pkg_mux_add_fd(m, fd, 0);
}

int
pkg_mux_add_listener(pkg_mux_t *m, const pkg_listener_t *L)
{
    if (!m || !L) return -1;
    int fd = pkg_get_listener_fd(L);
    return pkg_mux_add_fd(m, fd, 1);
}

void
pkg_mux_remove_fd(pkg_mux_t *m, int fd)
{
    if (!m || fd < 0) return;
    FD_CLR(fd, &m->watch);
    if (fd == m->maxfd) {
        m->maxfd = -1;
        for (int i = fd - 1; i >= 0; --i)
            if (FD_ISSET(i, &m->watch)) { m->maxfd = i; break; }
    }
}

int
pkg_mux_wait(pkg_mux_t *m, int timeout_ms)
{
    if (!m) return -1;
    m->ready = m->watch;
    struct timeval tv, *tvp = NULL;
    if (timeout_ms >= 0) {
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        tvp = &tv;
    }
    return select(m->maxfd + 1, &m->ready, NULL, NULL, tvp);
}

int
pkg_mux_is_ready_fd(const pkg_mux_t *m, int fd)
{
    if (!m || fd < 0) return 0;
    return FD_ISSET(fd, &m->ready) ? 1 : 0;
}

int
pkg_mux_is_ready_conn(const pkg_mux_t *m, const struct pkg_conn *pc)
{
    if (!m || !pc || pc == PKC_ERROR) return 0;
    return pkg_mux_is_ready_fd(m, pkg_get_read_fd(pc));
}

int
pkg_mux_is_ready_listener(const pkg_mux_t *m, const pkg_listener_t *L)
{
    if (!m || !L) return 0;
    return pkg_mux_is_ready_fd(m, pkg_get_listener_fd(L));
}

#else /* _WIN32 */

struct pkg_mux_entry {
    int    fd;
    HANDLE pipe_h;
};

struct pkg_mux {
    std::vector<pkg_mux_entry> entries;
    std::vector<int>           ready;
};

pkg_mux_t *
pkg_mux_create(void)
{
    return new pkg_mux_t();
}

void
pkg_mux_destroy(pkg_mux_t *m)
{
    if (!m) return;
    delete m;
}

int
pkg_mux_add_fd(pkg_mux_t *m, int fd, int is_socket)
{
    if (!m || fd < 0) return -1;
    for (auto &e : m->entries)
        if (e.fd == fd) return 0;
    pkg_mux_entry ent;
    ent.fd = fd;
    if (is_socket) {
        ent.pipe_h = INVALID_HANDLE_VALUE;
    } else {
        HANDLE h = (HANDLE)_get_osfhandle(fd);
        ent.pipe_h = (h != INVALID_HANDLE_VALUE && GetFileType(h) == FILE_TYPE_PIPE)
                     ? h : INVALID_HANDLE_VALUE;
    }
    m->entries.push_back(ent);
    return 0;
}

int
pkg_mux_add_conn(pkg_mux_t *m, const struct pkg_conn *pc)
{
    if (!m || !pc || pc == PKC_ERROR) return -1;
    int fd = pkg_get_read_fd(pc);
    int is_socket = (pc->pkc_tx_kind == 0) ? 1 : 0;
    return pkg_mux_add_fd(m, fd, is_socket);
}

int
pkg_mux_add_listener(pkg_mux_t *m, const pkg_listener_t *L)
{
    if (!m || !L) return -1;
    return pkg_mux_add_fd(m, pkg_get_listener_fd(L), 1);
}

void
pkg_mux_remove_fd(pkg_mux_t *m, int fd)
{
    if (!m) return;
    for (auto it = m->entries.begin(); it != m->entries.end(); ++it)
        if (it->fd == fd) { m->entries.erase(it); return; }
}

int
pkg_mux_wait(pkg_mux_t *m, int timeout_ms)
{
    if (!m || m->entries.empty()) return 0;
    m->ready.clear();

    HANDLE   all_handles[MAXIMUM_WAIT_OBJECTS];
    int      entry_idx[MAXIMUM_WAIT_OBJECTS];
    WSAEVENT wsa_evs[MAXIMUM_WAIT_OBJECTS];
    int      sock_hidx[MAXIMUM_WAIT_OBJECTS];
    int      n_handles = 0, n_wsa = 0;

    for (int i = 0; i < (int)m->entries.size() && n_handles < MAXIMUM_WAIT_OBJECTS; ++i) {
        auto &e = m->entries[i];
        if (e.pipe_h != INVALID_HANDLE_VALUE) {
            all_handles[n_handles] = e.pipe_h;
            entry_idx[n_handles]   = i;
            ++n_handles;
        } else {
            WSAEVENT ev = WSACreateEvent();
            if (ev == WSA_INVALID_EVENT) continue;
            WSAEventSelect((SOCKET)(uintptr_t)e.fd, ev, FD_READ | FD_ACCEPT | FD_CLOSE);
            all_handles[n_handles]   = ev;
            entry_idx[n_handles]     = i;
            sock_hidx[n_wsa]         = n_handles;
            wsa_evs[n_wsa]           = ev;
            ++n_handles; ++n_wsa;
        }
    }

    if (n_handles == 0) {
        if (timeout_ms > 0) Sleep((DWORD)timeout_ms);
        return 0;
    }

    DWORD timeout_dw = (timeout_ms < 0) ? INFINITE
                     : (timeout_ms == 0) ? 0
                     : (DWORD)timeout_ms;
    DWORD ret = WaitForMultipleObjects((DWORD)n_handles, all_handles, FALSE, timeout_dw);

    int n_ready = 0;
    if (ret >= WAIT_OBJECT_0 && ret < WAIT_OBJECT_0 + (DWORD)n_handles) {
        for (int i = 0; i < n_handles; ++i) {
            if (WaitForSingleObject(all_handles[i], 0) == WAIT_OBJECT_0) {
                m->ready.push_back(m->entries[entry_idx[i]].fd);
                ++n_ready;
            }
        }
    }

    for (int j = 0; j < n_wsa; ++j) {
        int hidx = sock_hidx[j];
        int fd   = m->entries[entry_idx[hidx]].fd;
        WSAEventSelect((SOCKET)(uintptr_t)fd, wsa_evs[j], 0);
        u_long blk = 0;
        ioctlsocket((SOCKET)(uintptr_t)fd, FIONBIO, &blk);
        WSACloseEvent(wsa_evs[j]);
    }

    return (ret == WAIT_TIMEOUT) ? 0 : (n_ready > 0 ? n_ready : -1);
}

int
pkg_mux_is_ready_fd(const pkg_mux_t *m, int fd)
{
    if (!m || fd < 0) return 0;
    for (int r : m->ready)
        if (r == fd) return 1;
    return 0;
}

int
pkg_mux_is_ready_conn(const pkg_mux_t *m, const struct pkg_conn *pc)
{
    if (!m || !pc || pc == PKC_ERROR) return 0;
    return pkg_mux_is_ready_fd(m, pkg_get_read_fd(pc));
}

int
pkg_mux_is_ready_listener(const pkg_mux_t *m, const pkg_listener_t *L)
{
    if (!m || !L) return 0;
    return pkg_mux_is_ready_fd(m, pkg_get_listener_fd(L));
}

#endif /* _WIN32 */


/* Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
