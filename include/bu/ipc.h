/*                          I P C . H
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
/** @addtogroup bu_ipc */
/** @{ */
/** @file bu/ipc.h
 *
 * @brief
 *  Transport-agnostic byte-stream IPC channel API.
 *
 * bu_ipc provides a clean send/receive abstraction over several local
 * inter-process communication transports.  The channel setup routine
 * probes available mechanisms in preference order and returns an opaque
 * (PIMPL) handle; callers never need to know which transport was selected.
 *
 * ### Motivation
 *
 * BRL-CAD historically used TCP sockets (via libpkg / fbserv) for
 * incremental display of raytrace results.  While TCP works everywhere,
 * it requires port allocation, may be blocked by firewalls, and has
 * higher overhead than local IPC.  bu_ipc replaces that with a
 * "try in order" approach:
 *
 *   anonymous pipe    →  fastest, zero kernel overhead beyond the pipe;
 *                        backed by kernel memory with no filesystem artifact
 *   socketpair        →  POSIX bidirectional socket pair; slightly more
 *                        overhead than a pipe but no path coordination needed
 *   TCP loopback      →  universal last resort; works everywhere but needs
 *                        a free port and is slower than local transports
 *
 * The caller never observes which transport was chosen; the opaque
 * bu_ipc_chan_t handle presents the same read/write surface in all cases.
 *
 *
 * ### Why named pipes are NOT in the probe order
 *
 * POSIX named pipes (FIFOs, created with mkfifo(3)) use exactly the same
 * kernel code path as anonymous pipes but require:
 *   - A temporary filesystem path (collision risk, cleanup burden).
 *   - Coordination of that path between the two processes.
 *   - Half-duplex semantics identical to anonymous pipes.
 *
 * For the parent-spawns-child pattern that bu_ipc targets, anonymous pipes
 * are strictly superior: the fd pair is inherited by the child automatically
 * with no path management.  Named pipes add cost without benefit here.
 *
 * On Windows the situation is different: Windows anonymous pipes created
 * with CreatePipe() do not support overlapped (async) I/O.  libuv therefore
 * uses Windows Named Pipes with a generated UUID path when UV_CREATE_PIPE
 * is requested.  This is transparent to bu_ipc callers — on Windows, the
 * "anonymous pipe" transport silently uses Windows Named Pipes under the
 * hood, which is exactly the right thing to do on that platform.
 *
 *
 * ### PIMPL design: why environment variables are safe here
 *
 * A channel pair created by bu_ipc_pair() stores all transport-specific
 * state inside the opaque bu_ipc_chan_t struct (PIMPL pattern).  Callers
 * hold handles, not raw file descriptors or port numbers.
 *
 * The child-end address is retrieved with bu_ipc_addr(), which returns a
 * pointer into the channel's internal storage — it is NOT written into the
 * parent process's global environment.
 *
 * The typical usage pattern is:
 *
 * @code
 *   bu_ipc_chan_t *p, *c;
 *   bu_ipc_pair(&p, &c);
 *
 *   // Spawn the child, passing the child-end address as a
 *   // PROCESS-SPECIFIC environment variable in the spawn options
 *   // (e.g. uv_process_options_t.env[]).  This is NOT setenv() — the
 *   // variable only exists in the child's environment, not in the
 *   // parent's global process environment.
 *   const char *env_entry = bu_ipc_addr_env(c);   // "BU_IPC_ADDR=pipe:4,7"
 *   spawn_child(rt_ipc_path, env=[env_entry, NULL]);
 *   bu_ipc_close(c);    // parent's copy of child end; child has its own
 *
 *   // Parent uses the parent end for async I/O (e.g. via uv_pipe_open)
 *   my_loop_register(bu_ipc_fileno(p));
 * @endcode
 *
 * ### Why this is thread-safe for concurrent renders
 *
 * Each call to bu_ipc_pair() creates two independent channel handles with
 * their own internal state.  The address string lives inside the struct.
 * The spawn call passes it in a per-spawn env array, NOT in the parent's
 * global process env (which would require locking).
 *
 * Multiple simultaneous renders therefore each hold separate
 * bu_ipc_chan_t handles and separate per-spawn env arrays.  There is no
 * shared mutable state between them.
 *
 * The BU_IPC_ADDR_ENVVAR constant is only used by child processes (via
 * bu_ipc_connect_env()) to read their own inherited env — each child sees
 * the value that was passed to it at spawn time, not a value that any other
 * thread in the parent could mutate.
 *
 *
 * ### Optional transport preference (BU_IPC_PREFER)
 *
 * For testing, benchmarking, or environments where a specific transport is
 * known to be preferable, set BU_IPC_PREFER_ENVVAR before calling
 * bu_ipc_pair():
 *
 * @code
 *   setenv("BU_IPC_PREFER", "tcp", 0);   // or "pipe" / "socket"
 * @endcode
 *
 * When set, bu_ipc_pair() tries that transport first (but still falls back
 * to others if it fails).  When unset, the default probe order is used.
 * Because this is a *preference hint* read at pair-creation time — not a
 * coordination mechanism — all concurrent pair() calls read the same value,
 * which is intentional and race-free.
 *
 * Applications can also call bu_ipc_pair_prefer() to specify the preference
 * programmatically without relying on an environment variable.
 *
 *
 * ### Why full transport-agnosticism is bounded
 *
 * A truly method-agnostic API is constrained by fundamentally different
 * addressing models across transports:
 *
 *   anonymous pipe   — no address; fd pair created before fork/spawn, inherited
 *   socketpair       — no address; same inheritance model as anonymous pipe
 *   TCP              — IP + port; server must bind before client connects
 *
 * These differences mean the "connect" operation is meaningful for TCP
 * but not for pipes, and the "listen" concept applies to TCP only.
 * A single bu_ipc_connect(addr) function works for all three because:
 *   - For pipe and socket transports the address is just the fd number
 *     (the fd was inherited, connect() is trivially "we already have it")
 *   - For TCP the address is a port number to connect() to
 *
 * This works cleanly for the parent-spawns-child pattern.  What would NOT
 * work is connecting two completely independent (non-parent-child) processes
 * with the pipe or socketpair transports, since those fds cannot be
 * transferred without a pre-existing connection.
 *
 *
 * ### Thread safety
 *
 * bu_ipc_write() and bu_ipc_read() are safe to call from different threads
 * on the same channel only when the underlying transport supports atomic
 * writes of the transferred size (pipe writes ≤ PIPE_BUF are atomic on
 * POSIX; larger writes are not).  For the frame-delimited render packets
 * used by librtrender all writes are well below PIPE_BUF (64 KiB on Linux),
 * so concurrent use is safe in practice.
 */

#ifndef BU_IPC_H
#define BU_IPC_H

#include "common.h"
#include "bu/defines.h"

#include <stddef.h>  /* size_t */

#ifdef _WIN32
#  include <basetsd.h>   /* SSIZE_T */
typedef SSIZE_T bu_ssize_t;
#else
#  include <sys/types.h> /* ssize_t */
typedef ssize_t bu_ssize_t;
#endif

__BEGIN_DECLS


/* ------------------------------------------------------------------ */
/* Environment variable constants                                       */
/* ------------------------------------------------------------------ */

/**
 * Environment variable read by a child process to find its channel address.
 *
 * This variable is set ONLY in the child's per-spawn environment (e.g. via
 * uv_process_options_t.env[]).  It is NEVER written to the parent's global
 * process environment.  Use bu_ipc_addr_env() to obtain a ready-to-use
 * "KEY=VALUE" string suitable for a per-spawn env array.
 */
#define BU_IPC_ADDR_ENVVAR  "BU_IPC_ADDR"

/**
 * Optional parent-process preference variable.
 *
 * If set to "pipe", "socket", or "tcp" in the parent's environment before
 * calling bu_ipc_pair(), that transport is tried first (with fallback to
 * others on failure).  Unset means use the default probe order.
 *
 * Safe to set in the global env since it is only a preference hint, not a
 * coordination mechanism; all concurrent bu_ipc_pair() calls reading the
 * same value is correct behaviour.
 */
#define BU_IPC_PREFER_ENVVAR "BU_IPC_PREFER"


/* ------------------------------------------------------------------ */
/* Transport type                                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief Which underlying IPC transport was selected.
 *
 * Callers should not need to branch on this; it is exposed only for
 * diagnostic messages and event-loop integration code that needs to pass
 * the raw file descriptor to a non-blocking I/O library (e.g.
 * uv_pipe_open(), QSocketNotifier, poll(2)).
 */
typedef enum {
    BU_IPC_PIPE   = 1, /**< @brief Anonymous pipe (fastest, parent-child only) */
    BU_IPC_SOCKET = 2, /**< @brief POSIX socketpair() (local processes)       */
    BU_IPC_TCP    = 3  /**< @brief TCP loopback 127.0.0.1 (universal fallback) */
} bu_ipc_type_t;


/* ------------------------------------------------------------------ */
/* Opaque PIMPL handle                                                  */
/* ------------------------------------------------------------------ */

/**
 * @brief Opaque IPC channel handle (PIMPL).
 *
 * All transport-specific state — file descriptors, port numbers, paths,
 * address strings — is hidden inside this struct.  Callers hold handles
 * and call bu_ipc_* functions; the implementation details are never
 * exposed.
 */
typedef struct bu_ipc_chan bu_ipc_chan_t;


/* ------------------------------------------------------------------ */
/* Channel creation (parent side)                                       */
/* ------------------------------------------------------------------ */

/**
 * @brief Create a matched pair of IPC channel ends for parent-child use.
 *
 * Reads BU_IPC_PREFER_ENVVAR (if set) to pick a preferred transport, then
 * probes in order until one succeeds: pipe → socketpair → TCP loopback.
 *
 * Both ends are returned as connected handles.  The parent keeps
 * @p parent_end for its own I/O.  The child-end address is retrieved with
 * bu_ipc_addr() or bu_ipc_addr_env() and passed to the child at spawn time
 * in the subprocess's private environment array (NOT via setenv()).
 *
 * @param[out] parent_end  Channel for the parent process.
 * @param[out] child_end   Channel whose address is given to the child.
 *
 * @return 0 on success, -1 if no transport could be established.
 */
BU_EXPORT int bu_ipc_pair(bu_ipc_chan_t **parent_end,
  bu_ipc_chan_t **child_end);

/**
 * @brief Like bu_ipc_pair() but with an explicit transport preference.
 *
 * Tries @p preferred first; falls back to others if it fails.
 * Pass BU_IPC_PIPE / BU_IPC_SOCKET / BU_IPC_TCP for explicit control.
 * Prefer bu_ipc_pair() for normal use; this variant is for testing and
 * environments where a particular transport must be exercised.
 *
 * @return 0 on success, -1 on failure.
 */
BU_EXPORT int bu_ipc_pair_prefer(bu_ipc_chan_t **parent_end,
 bu_ipc_chan_t **child_end,
 bu_ipc_type_t   preferred);


/* ------------------------------------------------------------------ */
/* Address handling (hidden inside PIMPL; exposed only as strings)      */
/* ------------------------------------------------------------------ */

/**
 * @brief Return the raw transport address of a channel.
 *
 * Format:
 *   "pipe:<rfd>,<wfd>"   — fd numbers (child must inherit them)
 *   "socket:<fd>"        — bidirectional socket fd
 *   "tcp:<port>"         — TCP loopback port
 *
 * The returned pointer is owned by @p chan (valid until bu_ipc_close()).
 * Normal callers should use bu_ipc_addr_env() instead, which wraps this
 * string in the "KEY=VALUE" format needed for per-spawn env arrays.
 */
BU_EXPORT const char *bu_ipc_addr(const bu_ipc_chan_t *chan);

/**
 * @brief Return a "BU_IPC_ADDR=<addr>" string for a per-spawn env array.
 *
 * The returned string is suitable for placement directly in an env array
 * passed to uv_process_options_t.env[], execve(), or similar.  It is
 * stored inside @p chan and is valid until bu_ipc_close().
 *
 * This is the primary way to pass the channel address to a child process.
 * It does NOT involve the parent's global environment (no setenv() call).
 *
 * @param[in] chan  A channel end created by bu_ipc_pair().
 * @return  NUL-terminated "KEY=VALUE" string, or NULL on error.
 */
BU_EXPORT const char *bu_ipc_addr_env(bu_ipc_chan_t *chan);


/* ------------------------------------------------------------------ */
/* Channel connection (child side)                                      */
/* ------------------------------------------------------------------ */

/**
 * @brief Connect to an IPC channel using an address string.
 *
 * Child-side counterpart to bu_ipc_pair().  The address string is the
 * raw value from bu_ipc_addr() (e.g. "pipe:4,7", "tcp:54321").
 *
 * @return  New channel handle, or NULL on failure.
 */
BU_EXPORT bu_ipc_chan_t *bu_ipc_connect(const char *addr);

/**
 * @brief Connect using the address in BU_IPC_ADDR_ENVVAR (child side).
 *
 * Reads the child's own inherited environment variable and calls
 * bu_ipc_connect() automatically.  This is the typical child-side
 * entry point when the parent used bu_ipc_addr_env() at spawn time.
 *
 * @return  New channel handle, or NULL if the variable is unset or
 *          the connection fails.
 */
BU_EXPORT bu_ipc_chan_t *bu_ipc_connect_env(void);


/* ------------------------------------------------------------------ */
/* Data transfer (blocking)                                             */
/* ------------------------------------------------------------------ */

/**
 * @brief Write exactly @p nbytes from @p buf into the channel.
 *
 * Blocks until all bytes have been written.
 *
 * @return @p nbytes on success; -1 on error.
 */
BU_EXPORT bu_ssize_t bu_ipc_write(bu_ipc_chan_t *chan,
  const void   *buf,
  size_t        nbytes);

/**
 * @brief Read exactly @p nbytes from the channel into @p buf.
 *
 * Blocks until all bytes are received or the channel closes.
 *
 * @return @p nbytes on complete success; 0 on EOF; -1 on error.
 */
BU_EXPORT bu_ssize_t bu_ipc_read(bu_ipc_chan_t *chan,
 void         *buf,
 size_t        nbytes);


/* ------------------------------------------------------------------ */
/* Event-loop integration                                               */
/* ------------------------------------------------------------------ */

/**
 * @brief Return the read file descriptor for event-loop registration.
 *
 * May be passed to uv_pipe_open(), QSocketNotifier, poll(2), etc.
 * Valid until bu_ipc_close(); do NOT close it directly.
 *
 * For TCP transport the parent end has no accepted fd until the child
 * connects.  This function performs a lazy blocking accept() so callers
 * always receive a valid (>= 0) fd after the child has been spawned.
 *
 * @return  fd >= 0 on success; -1 if not applicable.
 */
BU_EXPORT int bu_ipc_fileno(bu_ipc_chan_t *chan);

/**
 * @brief Return the write file descriptor.
 *
 * For socketpair and TCP transports this is the same as bu_ipc_fileno().
 * For anonymous pipe transport the read and write fds differ; this
 * function returns the write end.
 *
 * @return  fd >= 0 on success; -1 if not applicable.
 */
BU_EXPORT int bu_ipc_fileno_write(bu_ipc_chan_t *chan);

/**
 * @brief Return which transport was selected.
 */
BU_EXPORT bu_ipc_type_t bu_ipc_type(const bu_ipc_chan_t *chan);


/* ------------------------------------------------------------------ */
/* Cleanup                                                              */
/* ------------------------------------------------------------------ */

/**
 * @brief Close a channel and free all associated resources.
 *
 * After this call @p chan is invalid.  For socket/TCP transports the
 * implementation releases the port; for named-path transports it removes
 * any temporary filesystem entries.
 */
BU_EXPORT void bu_ipc_close(bu_ipc_chan_t *chan);


/**
 * @brief Release the channel struct without closing the underlying fds.
 *
 * Use when ownership of the underlying file descriptor(s) has been
 * transferred to another entity (e.g. libpkg via pkg_open_fds()).
 * The fds are left open; only the channel wrapper struct is freed.
 *
 * After this call @p chan is invalid.
 */
BU_EXPORT void bu_ipc_detach(bu_ipc_chan_t *chan);

/**
 * @brief Move the channel's fd(s) to fd numbers ≥ @p min_fd.
 *
 * Some spawn helpers (e.g. bu_process_create()) perform a sweep that
 * closes all file descriptors below a certain threshold before exec().
 * Call this on the child-end channel after bu_ipc_pair() and before
 * spawning the child to ensure the fd survives the sweep.
 *
 * Example:
 * @code
 *   bu_ipc_pair(&pe, &ce);
 *   bu_ipc_move_high_fd(ce, 64);   // move out of the swept range
 *   const char *addr = bu_ipc_addr(ce);
 *   // now pass addr as -I argument to child
 * @endcode
 *
 * @param[in] chan    Channel whose fd(s) should be relocated.
 * @param[in] min_fd Minimum acceptable fd number (must be ≥ 3).
 * @return 0 on success, -1 on error.
 */
BU_EXPORT int bu_ipc_move_high_fd(bu_ipc_chan_t *chan, int min_fd);


/**
 * @brief Cross-platform read-readiness multiplexer.
 *
 * bu_ipc_mux provides a unified interface for waiting on multiple file
 * descriptors to become readable:
 *
 *  - On POSIX: wraps select(2).
 *  - On Windows: uses WaitForMultipleObjects() for anonymous-pipe CRT fds
 *    (which select() cannot monitor) and WSAEventSelect() for WinSock
 *    socket fds, presenting a single portable interface.
 *
 * Pipe vs socket detection on Windows is automatic: if _get_osfhandle(fd)
 * returns a valid HANDLE with GetFileType()==FILE_TYPE_PIPE the fd is
 * treated as an anonymous pipe; otherwise it is assumed to be a WinSock
 * socket passed as a plain int (the libpkg convention).
 *
 * Typical usage:
 * @code
 *   bu_ipc_mux_t *mux = bu_ipc_mux_create();
 *   bu_ipc_mux_add(mux, listen_fd);
 *   bu_ipc_mux_add(mux, conn_rfd);
 *   if (bu_ipc_mux_wait(mux, 5000) > 0) {
 *       if (bu_ipc_mux_is_ready(mux, conn_rfd))
 *           ; // ... read from conn_rfd ...
 *   }
 *   bu_ipc_mux_destroy(mux);
 * @endcode
 */
struct bu_ipc_mux;
typedef struct bu_ipc_mux bu_ipc_mux_t;

/** Allocate an empty mux.  Returns NULL on allocation failure. */
BU_EXPORT bu_ipc_mux_t *bu_ipc_mux_create(void);

/**
 * Add a CRT file descriptor (pipe, stdin, regular file) to the watch set.
 *
 * On Windows the fd must be a genuine CRT fd (returned by _open_osfhandle()
 * or _open()) — NOT a WinSock SOCKET cast to int.  To add a socket use
 * bu_ipc_mux_add_socket() instead.  Duplicate adds are a no-op.
 * Returns 0 on success, -1 on error.
 */
BU_EXPORT int bu_ipc_mux_add(bu_ipc_mux_t *m, int fd);

/**
 * Add a WinSock socket (on Windows) or plain fd (on POSIX) to the watch set.
 *
 * On Windows this always takes the WSAEventSelect() path, avoiding the
 * _get_osfhandle() call that would crash for socket handle values.
 * On POSIX this is identical to bu_ipc_mux_add().
 * Duplicate adds are a no-op.  Returns 0 on success, -1 on error.
 */
BU_EXPORT int bu_ipc_mux_add_socket(bu_ipc_mux_t *m, int socket_fd);

/** Remove @p fd from the watch set.  No-op if fd is not present. */
BU_EXPORT void bu_ipc_mux_remove(bu_ipc_mux_t *m, int fd);

/**
 * Wait for one or more watched fds to become readable.
 *
 * @param timeout_ms  0 = poll once (non-blocking).  Negative = wait forever.
 *                    Positive = wait up to that many milliseconds.
 * @return  Number of ready fds (>= 1), 0 on timeout, -1 on error.
 *
 * After this returns > 0, use bu_ipc_mux_is_ready() to query each fd.
 */
BU_EXPORT int bu_ipc_mux_wait(bu_ipc_mux_t *m, int timeout_ms);

/** Return non-zero if @p fd became readable during the last bu_ipc_mux_wait(). */
BU_EXPORT int bu_ipc_mux_is_ready(const bu_ipc_mux_t *m, int fd);

/** Free the mux.  Does not close any watched fds. */
BU_EXPORT void bu_ipc_mux_destroy(bu_ipc_mux_t *m);


/** @} */

__END_DECLS

#endif /* BU_IPC_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
