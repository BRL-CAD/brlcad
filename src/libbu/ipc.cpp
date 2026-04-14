/*                          I P C . C P P
 * BRL-CAD
 *
 * Copyright (c) 2025 United States Government as represented by
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
/** @file libbu/ipc.cpp
 *
 * Transport-agnostic byte-stream IPC channel implementation.
 *
 * ### Transport probe order (default)
 *
 * 1. **Anonymous pipe** — pipe(2) on POSIX, CreatePipe() on Windows.
 *    On Windows, libuv uses Windows Named Pipes internally for its
 *    UV_CREATE_PIPE transport because Windows anonymous pipes lack
 *    overlapped I/O support.  bu_ipc's pipe transport on Windows uses
 *    CreatePipe() directly, which is appropriate for the blocking I/O
 *    this API provides; libuv handles the async side separately via
 *    uv_pipe_open().
 *
 *    Rationale for #1: zero kernel overhead beyond the pipe buffer; no
 *    filesystem path; no port allocation; works in containers, chroots,
 *    and sandboxes with no network stack at all.
 *
 * 2. **socketpair** — socketpair(AF_UNIX, SOCK_STREAM, 0) on POSIX only.
 *    Bidirectional by default (no need for two pipe() calls).  Same
 *    inheritance model as anonymous pipes.
 *    Rationale for #2: marginally more overhead than a pipe but
 *    bidirectional on one fd per process.  Unavailable on Windows without
 *    a compatibility shim.
 *
 * 3. **TCP loopback** — ephemeral port on 127.0.0.1.
 *    Rationale for last place: universally available; however requires a
 *    free port, is affected by OS firewall rules even on loopback, and
 *    has higher latency/overhead than the local transports.
 *
 * ### Why named pipes are NOT in the probe order (POSIX)
 *
 * Named pipes (mkfifo) use the same kernel implementation as anonymous
 * pipes but require a filesystem path, leaving a cleanup burden, and
 * introduce name collision risk if multiple processes create them
 * concurrently.  For parent-spawns-child IPC, anonymous pipes are
 * strictly superior.  (On Windows, the pipe transport already uses
 * Windows Named Pipes under the hood where appropriate.)
 *
 * ### PIMPL design
 *
 * All transport state lives inside bu_ipc_chan (defined in this file
 * only — not in any public header).  The public API never exposes raw
 * file descriptors, paths, or port numbers through the handle itself;
 * bu_ipc_addr() and bu_ipc_addr_env() return strings suitable for
 * passing to child processes via per-spawn environment arrays.
 *
 * ### Why per-spawn env is thread-safe
 *
 * bu_ipc_addr_env() returns a "KEY=VALUE" string stored inside the
 * chan struct.  The caller places this pointer in a per-spawn env array
 * (e.g. uv_process_options_t.env[]) — this is NOT a call to setenv().
 * Each concurrent spawn uses its own bu_ipc_chan_t with its own string;
 * there is no shared mutable state between simultaneous renders.
 *
 * ### Why libuv is NOT used here
 *
 * libbu is a foundational library.  Adding a mandatory libuv dependency
 * would impose it everywhere, including lightweight tools with no event
 * loop.  bu_ipc uses only POSIX/Win32 primitives.  The fd from
 * bu_ipc_fileno() is passed to libuv's uv_pipe_open() by the caller.
 */

#include "common.h"

#include "bio.h"
#include "bsocket.h"
#include "bnetwork.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cerrno>

#include "bu/defines.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/ipc.h"


/* ================================================================== */
/* Internal channel structure (the PIMPL implementation)               */
/* ================================================================== */

struct bu_ipc_chan {
    bu_ipc_type_t type;

    int fd;           /* read end (or bidirectional for socket/TCP) */
    int fd_write;     /* write end (differs from fd for pipe transport) */
    int listen_fd;    /* TCP: listener not yet accept()ed */

    /* Strings stored here so the public API can return stable pointers. */
    std::string addr;         /* raw address ("pipe:r,w", "socket:fd", "tcp:port") */
    std::string addr_env;     /* "BU_IPC_ADDR=<addr>" for per-spawn env arrays */
    std::string sock_path;    /* Unix socket path to unlink on close */

#ifdef _WIN32
    HANDLE hRead;
    HANDLE hWrite;
#endif
};

static bu_ipc_chan_t *
make_chan()
{
    auto *c     = new bu_ipc_chan_t();
    c->type     = BU_IPC_PIPE;
    c->fd       = -1;
    c->fd_write = -1;
    c->listen_fd= -1;
#ifdef _WIN32
    c->hRead    = INVALID_HANDLE_VALUE;
    c->hWrite   = INVALID_HANDLE_VALUE;
#endif
    return c;
}

/* Build the addr_env string after addr is set. */
static void
build_addr_env(bu_ipc_chan_t *c)
{
    c->addr_env = std::string(BU_IPC_ADDR_ENVVAR) + "=" + c->addr;
}


/* ================================================================== */
/* Transport 1: anonymous pipe                                          */
/* ================================================================== */

#ifndef _WIN32
static int
try_pipe(bu_ipc_chan_t **parent_end, bu_ipc_chan_t **child_end)
{
    int pc[2], cp[2];   /* pc: parent→child,  cp: child→parent */
    if (pipe(pc) < 0 || pipe(cp) < 0)
	return -1;

    /* Parent: reads from cp[0], writes to pc[1] */
    auto *pe = make_chan();
    pe->type     = BU_IPC_PIPE;
    pe->fd       = cp[0];
    pe->fd_write = pc[1];

    /* Child: reads from pc[0], writes to cp[1] */
    auto *ce = make_chan();
    ce->type     = BU_IPC_PIPE;
    ce->fd       = pc[0];
    ce->fd_write = cp[1];

    char buf[64];
    snprintf(buf, sizeof(buf), "pipe:%d,%d", pc[0], cp[1]);
    ce->addr = buf;
    build_addr_env(ce);

    snprintf(buf, sizeof(buf), "pipe_parent:%d,%d", cp[0], pc[1]);
    pe->addr = buf;
    build_addr_env(pe);

    *parent_end = pe;
    *child_end  = ce;
    return 0;
}
#else /* _WIN32 */
static int
try_pipe(bu_ipc_chan_t **parent_end, bu_ipc_chan_t **child_end)
{
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength              = sizeof(sa);
    sa.bInheritHandle       = TRUE;

    HANDLE hReadPC, hWritePC;   /* parent→child */
    HANDLE hReadCP, hWriteCP;   /* child→parent */
    if (!CreatePipe(&hReadPC, &hWritePC, &sa, 0) ||
	    !CreatePipe(&hReadCP, &hWriteCP, &sa, 0))
	return -1;

    /* Don't let the parent's write ends propagate to grandchildren */
    SetHandleInformation(hWritePC, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hReadCP,  HANDLE_FLAG_INHERIT, 0);

    int pc_r = _open_osfhandle((intptr_t)hReadPC,  _O_RDONLY | _O_BINARY);
    int pc_w = _open_osfhandle((intptr_t)hWritePC, _O_WRONLY | _O_BINARY);
    int cp_r = _open_osfhandle((intptr_t)hReadCP,  _O_RDONLY | _O_BINARY);
    int cp_w = _open_osfhandle((intptr_t)hWriteCP, _O_WRONLY | _O_BINARY);

    auto *pe = make_chan();
    pe->type = BU_IPC_PIPE; pe->fd = cp_r; pe->fd_write = pc_w;
    pe->hRead = hReadCP; pe->hWrite = hWritePC;

    auto *ce = make_chan();
    ce->type = BU_IPC_PIPE; ce->fd = pc_r; ce->fd_write = cp_w;
    ce->hRead = hReadPC; ce->hWrite = hWriteCP;

    char buf[128];
    /* On Windows, the child-end address encodes the HANDLE values (not CRT
     * fd numbers), because CRT fd numbers are process-local and cannot be
     * used by a spawned child.  The child-end HANDLEs are marked inheritable
     * so they survive CreateProcess().  The child calls bu_ipc_connect() with
     * this address, which calls _open_osfhandle() to reconstruct CRT fds from
     * the inherited HANDLEs.                                                  */
    snprintf(buf, sizeof(buf), "pipe_win:%I64x,%I64x",
	     (unsigned __int64)(intptr_t)hReadPC,
	     (unsigned __int64)(intptr_t)hWriteCP);
    ce->addr = buf; build_addr_env(ce);
    snprintf(buf, sizeof(buf), "pipe_parent:%d,%d", cp_r, pc_w);
    pe->addr = buf; build_addr_env(pe);

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
try_socketpair(bu_ipc_chan_t **parent_end, bu_ipc_chan_t **child_end)
{
    int sv[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0)
	return -1;

    auto *pe = make_chan();
    pe->type = BU_IPC_SOCKET; pe->fd = sv[0]; pe->fd_write = sv[0];

    auto *ce = make_chan();
    ce->type = BU_IPC_SOCKET; ce->fd = sv[1]; ce->fd_write = sv[1];

    char buf[64];
    snprintf(buf, sizeof(buf), "socket:%d", sv[1]);
    ce->addr = buf; build_addr_env(ce);
    snprintf(buf, sizeof(buf), "socket_parent:%d", sv[0]);
    pe->addr = buf; build_addr_env(pe);

    *parent_end = pe;
    *child_end  = ce;
    return 0;
}
#endif


/* ================================================================== */
/* Transport 3: TCP loopback                                            */
/* ================================================================== */

static int
try_tcp(bu_ipc_chan_t **parent_end, bu_ipc_chan_t **child_end)
{
#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    int lsock = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (lsock == INVALID_SOCKET) return -1;
#else
    int lsock = socket(AF_INET, SOCK_STREAM, 0);
    if (lsock < 0) return -1;
#endif

    {   int opt = 1;
	setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)); }

    struct sockaddr_in sa = {};
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

    struct sockaddr_in bound = {};
    socklen_t blen = sizeof(bound);
    getsockname(lsock, (struct sockaddr*)&bound, &blen);
    int port = ntohs(bound.sin_port);

    char buf[64];
    snprintf(buf, sizeof(buf), "tcp:%d", port);

    auto *ce = make_chan();
    ce->type      = BU_IPC_TCP;
    ce->listen_fd = -1;
    ce->addr      = buf;
    build_addr_env(ce);

    auto *pe = make_chan();
    pe->type      = BU_IPC_TCP;
    pe->listen_fd = lsock;
    snprintf(buf, sizeof(buf), "tcp_server:%d", port);
    pe->addr = buf;
    build_addr_env(pe);

    *parent_end = pe;
    *child_end  = ce;
    return 0;
}


/* ================================================================== */
/* TCP lazy-accept                                                      */
/* ================================================================== */

static int
tcp_accept(bu_ipc_chan_t *chan)
{
    if (chan->fd >= 0) return 0;
    if (chan->listen_fd < 0) return -1;

    struct sockaddr_in peer = {};
    socklen_t plen = sizeof(peer);
#ifdef _WIN32
    int csock = (int)accept((SOCKET)chan->listen_fd, (struct sockaddr*)&peer, &plen);
    if (csock == INVALID_SOCKET) return -1;
    closesocket((SOCKET)chan->listen_fd);
#else
    int csock = accept(chan->listen_fd, (struct sockaddr*)&peer, &plen);
    if (csock < 0) return -1;
    close(chan->listen_fd);
#endif
    chan->listen_fd = -1;
    chan->fd = chan->fd_write = csock;
    return 0;
}


/* ================================================================== */
/* Probe order helper                                                   */
/* ================================================================== */

/* Try the given transport; return 0 on success. */
static int
try_one(bu_ipc_type_t t, bu_ipc_chan_t **pe, bu_ipc_chan_t **ce)
{
    switch (t) {
	case BU_IPC_PIPE:
	    return try_pipe(pe, ce);
#if defined(HAVE_SYS_UN_H) && !defined(_WIN32)
	case BU_IPC_SOCKET:
	    return try_socketpair(pe, ce);
#endif
	case BU_IPC_TCP:
	    return try_tcp(pe, ce);
	default:
	    return -1;
    }
}

/* Parse BU_IPC_PREFER_ENVVAR → bu_ipc_type_t (or 0 if unset/unknown). */
static bu_ipc_type_t
read_prefer_env()
{
    const char *v = getenv(BU_IPC_PREFER_ENVVAR);
    if (!v) return (bu_ipc_type_t)0;
    if (BU_STR_EQUAL(v, "pipe")) return BU_IPC_PIPE;
    if (BU_STR_EQUAL(v, "socket")) return BU_IPC_SOCKET;
    if (BU_STR_EQUAL(v, "tcp")) return BU_IPC_TCP;
    bu_log("bu_ipc: unknown BU_IPC_PREFER value '%s' — ignoring\n", v);
    return (bu_ipc_type_t)0;
}


/* ================================================================== */
/* Public: bu_ipc_pair / bu_ipc_pair_prefer                            */
/* ================================================================== */

int
bu_ipc_pair_prefer(bu_ipc_chan_t **parent_end,
	bu_ipc_chan_t **child_end,
	bu_ipc_type_t   preferred)
{
    if (!parent_end || !child_end) return -1;

    /* Build probe order: preferred first, then others. */
    bu_ipc_type_t order[3] = { BU_IPC_PIPE, BU_IPC_SOCKET, BU_IPC_TCP };
    if ((int)preferred > 0) {
	/* Rotate preferred to front */
	int pos = 0;
	for (int i = 0; i < 3; ++i)
	    if (order[i] == preferred) { pos = i; break; }
	if (pos > 0) {
	    bu_ipc_type_t tmp = order[0];
	    order[0] = order[pos];
	    order[pos] = tmp;
	}
    }

    for (int i = 0; i < 3; ++i) {
	if (try_one(order[i], parent_end, child_end) == 0)
	    return 0;
	if (i < 2)
	    bu_log("bu_ipc_pair: transport %d failed, trying next\n",
		    (int)order[i]);
    }
    bu_log("bu_ipc_pair: all transports failed\n");
    return -1;
}

int
bu_ipc_pair(bu_ipc_chan_t **parent_end, bu_ipc_chan_t **child_end)
{
    return bu_ipc_pair_prefer(parent_end, child_end, read_prefer_env());
}


/* ================================================================== */
/* Address API                                                          */
/* ================================================================== */

const char *
bu_ipc_addr(const bu_ipc_chan_t *chan)
{
    return chan ? chan->addr.c_str() : nullptr;
}

const char *
bu_ipc_addr_env(bu_ipc_chan_t *chan)
{
    if (!chan) return nullptr;
    /* Lazily update if addr changed (shouldn't happen after pair, but be safe) */
    if (chan->addr_env.empty() ||
	    chan->addr_env.find(chan->addr) == std::string::npos)
	build_addr_env(chan);
    return chan->addr_env.c_str();
}


/* ================================================================== */
/* bu_ipc_connect / bu_ipc_connect_env                                 */
/* ================================================================== */

bu_ipc_chan_t *
bu_ipc_connect(const char *addr)
{
    if (!addr) return nullptr;
    auto *c = make_chan();
    c->addr = addr;

#ifdef _WIN32
    /* Windows pipe transport: address encodes inheritable HANDLE values so
     * that a spawned child can reconstruct valid CRT fds via _open_osfhandle().
     * Format: "pipe_win:<read_handle_hex>,<write_handle_hex>"              */
    if (bu_strncmp(addr, "pipe_win:", 9) == 0) {
	unsigned __int64 hr64 = 0, hw64 = 0;
	if (sscanf(addr + 9, "%I64x,%I64x", &hr64, &hw64) == 2) {
	    HANDLE hRead  = (HANDLE)(intptr_t)hr64;
	    HANDLE hWrite = (HANDLE)(intptr_t)hw64;
	    int rfd = _open_osfhandle((intptr_t)hRead,  _O_RDONLY | _O_BINARY);
	    int wfd = _open_osfhandle((intptr_t)hWrite, _O_WRONLY | _O_BINARY);
	    if (rfd >= 0 && wfd >= 0) {
		c->type     = BU_IPC_PIPE;
		c->fd       = rfd;
		c->fd_write = wfd;
		c->hRead    = hRead;
		c->hWrite   = hWrite;
		build_addr_env(c);
		return c;
	    }
	    /* Clean up if _open_osfhandle failed on one side */
	    if (rfd >= 0) _close(rfd);
	    if (wfd >= 0) _close(wfd);
	    delete c;
	    return nullptr;
	}
    }
#endif /* _WIN32 */

    if (bu_strncmp(addr, "pipe:", 5) == 0) {
	int rfd = -1, wfd = -1;
	if (sscanf(addr + 5, "%d,%d", &rfd, &wfd) == 2) {
	    c->type = BU_IPC_PIPE; c->fd = rfd; c->fd_write = wfd;
	    build_addr_env(c);
	    return c;
	}
    }

    if (bu_strncmp(addr, "socket:", 7) == 0) {
	int sfd = -1;
	if (sscanf(addr + 7, "%d", &sfd) == 1) {
	    c->type = BU_IPC_SOCKET; c->fd = sfd; c->fd_write = sfd;
	    build_addr_env(c);
	    return c;
	}
    }

    if (bu_strncmp(addr, "tcp:", 4) == 0) {
	int port = 0;
	if (sscanf(addr + 4, "%d", &port) == 1) {
#ifdef _WIN32
	    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
	    int csock = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	    if (csock == INVALID_SOCKET) { delete c; return nullptr; }
#else
	    int csock = socket(AF_INET, SOCK_STREAM, 0);
	    if (csock < 0) { delete c; return nullptr; }
#endif
	    struct sockaddr_in sa = {};
	    sa.sin_family      = AF_INET;
	    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	    sa.sin_port        = htons((uint16_t)port);
	    if (connect(csock, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
#ifdef _WIN32
		closesocket((SOCKET)csock);
#else
		close(csock);
#endif
		delete c; return nullptr;
	    }
	    c->type = BU_IPC_TCP; c->fd = csock; c->fd_write = csock;
	    build_addr_env(c);
	    return c;
	}
    }

    bu_log("bu_ipc_connect: unrecognized address '%s'\n", addr);
    delete c;
    return nullptr;
}

bu_ipc_chan_t *
bu_ipc_connect_env()
{
    const char *addr = getenv(BU_IPC_ADDR_ENVVAR);
    if (!addr) {
	bu_log("bu_ipc_connect_env: %s not set\n", BU_IPC_ADDR_ENVVAR);
	return nullptr;
    }
    return bu_ipc_connect(addr);
}


/* ================================================================== */
/* Data transfer                                                        */
/* ================================================================== */

bu_ssize_t
bu_ipc_write(bu_ipc_chan_t *chan, const void *buf, size_t nbytes)
{
    if (!chan || !buf) return -1;
    if (chan->type == BU_IPC_TCP && tcp_accept(chan) < 0) return -1;

    int wfd = (chan->fd_write >= 0) ? chan->fd_write : chan->fd;
    if (wfd < 0) return -1;

    const char *p = (const char *)buf;
    size_t rem = nbytes;
    while (rem > 0) {
#ifdef _WIN32
	int n;
	if (chan->type == BU_IPC_TCP) {
	    /* wfd is a WinSock SOCKET after tcp_accept(); must use send(). */
	    n = send((SOCKET)wfd, p, (int)(rem > 65536 ? 65536 : rem), 0);
	} else {
	    n = _write(wfd, p, (unsigned)(rem > 65536 ? 65536 : rem));
	}
#else
	ssize_t n = write(wfd, p, rem);
#endif
	if (n <= 0) return -1;
	p   += n;
	rem -= (size_t)n;
    }
    return (bu_ssize_t)nbytes;
}

bu_ssize_t
bu_ipc_read(bu_ipc_chan_t *chan, void *buf, size_t nbytes)
{
    if (!chan || !buf) return -1;
    if (chan->type == BU_IPC_TCP && tcp_accept(chan) < 0) return -1;

    int rfd = chan->fd;
    if (rfd < 0) return -1;

    char *p = (char *)buf;
    size_t rem = nbytes;
    while (rem > 0) {
#ifdef _WIN32
	int n;
	if (chan->type == BU_IPC_TCP) {
	    /* rfd is a WinSock SOCKET after tcp_accept(); must use recv(). */
	    n = recv((SOCKET)rfd, p, (int)(rem > 65536 ? 65536 : rem), 0);
	    if (n == 0) return 0;  /* graceful close */
	} else {
	    n = _read(rfd, p, (unsigned)(rem > 65536 ? 65536 : rem));
	    if (n == 0) return 0;
	}
#else
	ssize_t n = read(rfd, p, rem);
	if (n == 0) return 0;
#endif
	if (n < 0)  return -1;
	p   += n;
	rem -= (size_t)n;
    }
    return (bu_ssize_t)nbytes;
}


/* ================================================================== */
/* Introspection                                                        */
/* ================================================================== */

int
bu_ipc_fileno(bu_ipc_chan_t *chan)
{
    if (!chan) return -1;
    /* For TCP parent-end channels accept() must complete before the fd is
     * valid.  Block here until the child connects (the child must already
     * have been spawned by the caller before calling bu_ipc_fileno).      */
    if (chan->type == BU_IPC_TCP && chan->fd < 0)
	tcp_accept(chan);
    return chan->fd;
}

int
bu_ipc_fileno_write(bu_ipc_chan_t *chan)
{
    if (!chan) return -1;
    /* Ensure TCP accept is complete so fd_write is valid. */
    if (chan->type == BU_IPC_TCP && chan->fd < 0)
	tcp_accept(chan);
    return (chan->fd_write >= 0) ? chan->fd_write : chan->fd;
}

bu_ipc_type_t
bu_ipc_type(const bu_ipc_chan_t *chan)
{
    return chan ? chan->type : BU_IPC_PIPE;
}


/* ================================================================== */
/* Cleanup                                                              */
/* ================================================================== */

void
bu_ipc_close(bu_ipc_chan_t *chan)
{
    if (!chan) return;

#ifdef _WIN32
    if (chan->type == BU_IPC_TCP) {
	/* TCP fds are WinSock sockets, not CRT fds — must use closesocket(). */
	if (chan->fd       >= 0) closesocket((SOCKET)chan->fd);
	if (chan->fd_write >= 0 && chan->fd_write != chan->fd)
	    closesocket((SOCKET)chan->fd_write);
    } else {
	if (chan->fd       >= 0) _close(chan->fd);
	if (chan->fd_write >= 0 && chan->fd_write != chan->fd) _close(chan->fd_write);
    }
    if (chan->listen_fd >= 0) closesocket((SOCKET)chan->listen_fd);
#else
    if (chan->fd       >= 0) close(chan->fd);
    if (chan->fd_write >= 0 && chan->fd_write != chan->fd) close(chan->fd_write);
    if (chan->listen_fd >= 0) close(chan->listen_fd);
#endif

    if (!chan->sock_path.empty())
	(void)bu_file_delete(chan->sock_path.c_str());

    delete chan;
}


void
bu_ipc_detach(bu_ipc_chan_t *chan)
{
    /* Caller has transferred fd ownership elsewhere; just free the struct.
     * Do NOT close any fds — that responsibility now belongs to the caller. */
    delete chan;
}


int
bu_ipc_move_high_fd(bu_ipc_chan_t *chan, int min_fd)
{
    if (!chan || min_fd < 3) return -1;

#ifdef _WIN32
    /* On Windows, HANDLE-based pipes have no concept of "fd number". */
    (void)min_fd;
    return 0;
#else
    /* Move fd (read end, or bidirectional) if it is below min_fd. */
    if (chan->fd >= 0 && chan->fd < min_fd) {
	int nfd = fcntl(chan->fd, F_DUPFD, min_fd);
	if (nfd < 0) return -1;
	if (chan->fd_write == chan->fd)
	    chan->fd_write = nfd;
	close(chan->fd);
	chan->fd = nfd;
	/* Update the address string to reflect the new fd number. */
	char buf[64];
	if (chan->type == BU_IPC_SOCKET)
	    snprintf(buf, sizeof(buf), "socket:%d", chan->fd);
	else if (chan->type == BU_IPC_PIPE)
	    snprintf(buf, sizeof(buf), "pipe:%d,%d", chan->fd, chan->fd_write);
	else
	    snprintf(buf, sizeof(buf), "tcp:%d", chan->fd);
	chan->addr = buf;
    }

    /* For pipe transport: move write end too (it differs from fd). */
    if (chan->type == BU_IPC_PIPE
	    && chan->fd_write >= 0
	    && chan->fd_write < min_fd
	    && chan->fd_write != chan->fd)
    {
	int nfd = fcntl(chan->fd_write, F_DUPFD, min_fd);
	if (nfd < 0) return -1;
	close(chan->fd_write);
	chan->fd_write = nfd;
	char buf[64];
	snprintf(buf, sizeof(buf), "pipe:%d,%d", chan->fd, chan->fd_write);
	chan->addr = buf;
    }

    return 0;
#endif
}


/* ================================================================== */
/* bu_ipc_mux: cross-platform read-readiness multiplexer               */
/* ================================================================== */

#ifndef _WIN32

/* POSIX implementation: thin wrapper around select(). */

struct bu_ipc_mux {
    fd_set watch;
    fd_set ready;
    int maxfd;
};

bu_ipc_mux_t *
bu_ipc_mux_create(void)
{
    bu_ipc_mux_t *m = new bu_ipc_mux_t();
    FD_ZERO(&m->watch);
    FD_ZERO(&m->ready);
    m->maxfd = -1;
    return m;
}

void
bu_ipc_mux_destroy(bu_ipc_mux_t *m)
{
    if (!m) return;
    delete m;
}

int
bu_ipc_mux_add(bu_ipc_mux_t *m, int fd)
{
    if (!m || fd < 0) return -1;
    if (FD_ISSET(fd, &m->watch)) return 0;  /* duplicate */
    FD_SET(fd, &m->watch);
    if (fd > m->maxfd) m->maxfd = fd;
    return 0;
}

void
bu_ipc_mux_remove(bu_ipc_mux_t *m, int fd)
{
    if (!m || fd < 0) return;
    FD_CLR(fd, &m->watch);
    if (fd == m->maxfd) {
        m->maxfd = -1;
        for (int i = fd - 1; i >= 0; --i) {
            if (FD_ISSET(i, &m->watch)) { m->maxfd = i; break; }
        }
    }
}

int
bu_ipc_mux_wait(bu_ipc_mux_t *m, int timeout_ms)
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
bu_ipc_mux_is_ready(const bu_ipc_mux_t *m, int fd)
{
    if (!m || fd < 0) return 0;
    return FD_ISSET(fd, &m->ready) ? 1 : 0;
}

int
bu_ipc_mux_add_socket(bu_ipc_mux_t *m, int socket_fd)
{
    /* POSIX: sockets are regular fds; identical to bu_ipc_mux_add(). */
    return bu_ipc_mux_add(m, socket_fd);
}

#else /* _WIN32 */

/* Windows implementation:
 *  - Anonymous pipe CRT fds -> WaitForMultipleObjects() directly on HANDLE
 *  - WinSock socket fds     -> WSAEventSelect() converts to a waitable event
 *
 * After WaitForMultipleObjects fires, all remaining handles are checked with
 * a zero-timeout to collect every simultaneously-ready fd.  Socket handles
 * are reset to blocking mode before the function returns so that subsequent
 * recv() calls in libpkg work correctly.
 */

struct bu_ipc_mux_entry {
    int    fd;
    HANDLE pipe_h;   /* valid pipe HANDLE, or INVALID_HANDLE_VALUE for sockets */
};

struct bu_ipc_mux {
    std::vector<bu_ipc_mux_entry> entries;
    std::vector<int>              ready;
};

bu_ipc_mux_t *
bu_ipc_mux_create(void)
{
    return new bu_ipc_mux_t();
}

void
bu_ipc_mux_destroy(bu_ipc_mux_t *m)
{
    if (!m) return;
    delete m;
}

int
bu_ipc_mux_add(bu_ipc_mux_t *m, int fd)
{
    if (!m || fd < 0) return -1;
    for (auto &e : m->entries)
        if (e.fd == fd) return 0;  /* duplicate */

    bu_ipc_mux_entry ent;
    ent.fd = fd;
    HANDLE h = (HANDLE)_get_osfhandle(fd);
    ent.pipe_h = (h != INVALID_HANDLE_VALUE && GetFileType(h) == FILE_TYPE_PIPE)
                 ? h : INVALID_HANDLE_VALUE;
    m->entries.push_back(ent);
    return 0;
}

int
bu_ipc_mux_add_socket(bu_ipc_mux_t *m, int socket_fd)
{
    /* On Windows, WinSock SOCKETs are not CRT fds; calling _get_osfhandle()
     * on them triggers the invalid-parameter handler.  Force the WSA path by
     * setting pipe_h = INVALID_HANDLE_VALUE unconditionally.               */
    if (!m || socket_fd < 0) return -1;
    for (auto &e : m->entries)
        if (e.fd == socket_fd) return 0;  /* duplicate */

    bu_ipc_mux_entry ent;
    ent.fd = socket_fd;
    ent.pipe_h = INVALID_HANDLE_VALUE;  /* always WSA path */
    m->entries.push_back(ent);
    return 0;
}

void
bu_ipc_mux_remove(bu_ipc_mux_t *m, int fd)
{
    if (!m) return;
    for (auto it = m->entries.begin(); it != m->entries.end(); ++it) {
        if (it->fd == fd) { m->entries.erase(it); return; }
    }
}

int
bu_ipc_mux_wait(bu_ipc_mux_t *m, int timeout_ms)
{
    if (!m || m->entries.empty()) return 0;
    m->ready.clear();

    /* Build two parallel arrays:
     *   all_handles[] - handles passed to WaitForMultipleObjects
     *   entry_idx[]   - index into m->entries for each handle
     * For socket entries we create a temporary WSAEVENT.
     * For pipe entries we use the pipe HANDLE directly. */

    HANDLE   all_handles[MAXIMUM_WAIT_OBJECTS];
    int      entry_idx[MAXIMUM_WAIT_OBJECTS];
    WSAEVENT wsa_evs[MAXIMUM_WAIT_OBJECTS];   /* socket events to close later */
    int      sock_entry_for_ev[MAXIMUM_WAIT_OBJECTS]; /* entry_idx for socket events */
    int      n_handles = 0, n_wsa = 0;

    for (int i = 0; i < (int)m->entries.size() && n_handles < MAXIMUM_WAIT_OBJECTS; ++i) {
        auto &e = m->entries[i];
        if (e.pipe_h != INVALID_HANDLE_VALUE) {
            /* Pipe: waitable directly */
            all_handles[n_handles] = e.pipe_h;
            entry_idx[n_handles]   = i;
            ++n_handles;
        } else {
            /* Socket: wrap in a WSAEVENT */
            WSAEVENT ev = WSACreateEvent();
            if (ev == WSA_INVALID_EVENT) continue;
            WSAEventSelect((SOCKET)(uintptr_t)e.fd, ev, FD_READ | FD_ACCEPT | FD_CLOSE);
            all_handles[n_handles]       = ev;
            entry_idx[n_handles]         = i;
            sock_entry_for_ev[n_wsa]     = n_handles;
            wsa_evs[n_wsa]               = ev;
            ++n_handles;
            ++n_wsa;
        }
    }

    if (n_handles == 0) {
        /* No monitorable fds */
        if (timeout_ms > 0) Sleep((DWORD)timeout_ms);
        return 0;
    }

    /* Wait */
    DWORD timeout_dw = (timeout_ms < 0) ? INFINITE
                     : (timeout_ms == 0) ? 0
                     : (DWORD)timeout_ms;
    DWORD ret = WaitForMultipleObjects((DWORD)n_handles, all_handles, FALSE, timeout_dw);

    int n_ready = 0;
    if (ret >= WAIT_OBJECT_0 && ret < WAIT_OBJECT_0 + (DWORD)n_handles) {
        /* At least one handle ready; sweep all with zero timeout to collect rest */
        for (int i = 0; i < n_handles; ++i) {
            DWORD r = WaitForSingleObject(all_handles[i], 0);
            if (r == WAIT_OBJECT_0) {
                m->ready.push_back(m->entries[entry_idx[i]].fd);
                ++n_ready;
            }
        }
    }

    /* Cleanup: reset socket event associations and restore blocking mode */
    for (int j = 0; j < n_wsa; ++j) {
        int hidx = sock_entry_for_ev[j];
        int fd   = m->entries[entry_idx[hidx]].fd;
        WSAEventSelect((SOCKET)(uintptr_t)fd, wsa_evs[j], 0);
        u_long blk = 0;
        ioctlsocket((SOCKET)(uintptr_t)fd, FIONBIO, &blk);
        WSACloseEvent(wsa_evs[j]);
    }

    return (ret == WAIT_TIMEOUT) ? 0 : (n_ready > 0 ? n_ready : -1);
}

int
bu_ipc_mux_is_ready(const bu_ipc_mux_t *m, int fd)
{
    if (!m || fd < 0) return 0;
    for (int r : m->ready)
        if (r == fd) return 1;
    return 0;
}

#endif /* _WIN32 */


/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
