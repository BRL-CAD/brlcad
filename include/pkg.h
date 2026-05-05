/*                           P K G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/** @addtogroup libpkg */
/** @{ */
/** @file pkg.h
 *
 * LIBPKG provides routines to manage multiplexing and de-multiplexing
 * synchronous and asynchronous messages across stream connections.
 *
 */

#ifndef PKG_H
#define PKG_H

#include "common.h"

/* for size_t */
#include <stddef.h>

#ifndef PKG_EXPORT
#  if defined(PKG_DLL_EXPORTS) && defined(PKG_DLL_IMPORTS)
#    error "Only PKG_DLL_EXPORTS or PKG_DLL_IMPORTS can be defined, not both."
#  elif defined(PKG_DLL_EXPORTS)
#    define PKG_EXPORT COMPILER_DLLEXPORT
#  elif defined(PKG_DLL_IMPORTS)
#    define PKG_EXPORT COMPILER_DLLIMPORT
#  else
#    define PKG_EXPORT
#  endif
#endif


#ifdef __cplusplus
extern "C" {
#endif

struct pkg_conn;

typedef void (*pkg_callback)(struct pkg_conn*, char*);
typedef void (*pkg_errlog)(const char *msg);

struct pkg_switch {
    unsigned short pks_type;	/**< @brief Type code */
    pkg_callback pks_handler;	/**< @brief Message Handler */
    const char *pks_title;	/**< @brief Description of message type */
    void *pks_user_data;        /**< @brief User defined pointer to data */
};

/**
 *  Format of the message header as it is transmitted over the network
 *  connection.  Internet network order is used.
 *  User Code should access pkc_len and pkc_type rather than
 *  looking into the header directly.
 *  Users should never need to know what this header looks like.
 */
#define PKG_MAGIC	0x41FE
struct pkg_header {
    unsigned char pkh_magic[2];	/**< @brief Ident */
    unsigned char pkh_type[2];	/**< @brief Message Type */
    unsigned char pkh_len[4];	/**< @brief Byte count of remainder */
};

#define	PKG_STREAMLEN	(32*1024)
struct pkg_conn {
    int	pkc_fd;					/**< @brief TCP connection fd */
    int pkc_in_fd;                              /**< @brief input fd for split-fd (pipe) transports */
    int pkc_out_fd;                             /**< @brief output fd for split-fd (pipe) transports */

    const struct pkg_switch *pkc_switch;	/**< @brief Array of message handlers */
    pkg_errlog pkc_errlog;			/**< @brief Error message logger */
    struct pkg_header pkc_hdr;			/**< @brief hdr of cur msg */
    size_t pkc_len;				/**< @brief pkg_len, in host order */
    unsigned short pkc_type;			/**< @brief pkg_type, in host order */
    void *pkc_user_data;                        /**< @brief User defined pointer to data for the current pkg_type */
    /* OUTPUT BUFFER */
    char pkc_stream[PKG_STREAMLEN];		/**< @brief output stream */
    unsigned int pkc_magic;			/**< @brief for validating pointers */
    int pkc_strpos;				/**< @brief index into stream buffer */
    /* FIRST LEVEL INPUT BUFFER */
    char *pkc_inbuf;				/**< @brief input stream buffer */
    int pkc_incur;				/**< @brief current pos in inbuf */
    int pkc_inend;				/**< @brief first unused pos in inbuf */
    int pkc_inlen;				/**< @brief length of pkc_inbuf */
    /* DYNAMIC BUFFER FOR USER */
    int pkc_left;				/**< @brief #  bytes pkg_get expects */
    /* neg->read new hdr, 0->all here, >0 ->more to come */
    char *pkc_buf;				/**< @brief start of dynamic buf */
    char *pkc_curpos;				/**< @brief current position in pkg_buf */
    void *pkc_server_data;			/**< @brief used to hold server data for callbacks */

    /**
     * Pluggable I/O layer for TLS (or any other stream cipher / framing).
     *
     * When pkc_tls_read / pkc_tls_write are non-NULL they completely
     * replace the raw fd reads/writes inside pkg_suckin(), pkg_send(),
     * pkg_2send(), and pkg_flush().  pkc_tls_ctx is the opaque context
     * pointer forwarded as the first argument to both callbacks.
     *
     * pkc_tls_free (if non-NULL) is called by pkg_close() before
     * closing the socket, giving the TLS layer a chance to send a
     * clean close_notify and free its own state.
     *
     * All four fields are zero-initialized by _pkg_makeconn().
     *
     * The callback signatures use ptrdiff_t (always defined via
     * <stddef.h>) rather than ssize_t to avoid POSIX-only header
     * dependencies in this public header.
     */
    void *pkc_tls_ctx;							/**< @brief opaque TLS state (e.g. SSL *) */
    ptrdiff_t (*pkc_tls_read)(void *ctx, void *buf, size_t n);		/**< @brief TLS read callback; NULL = use raw fd */
    ptrdiff_t (*pkc_tls_write)(void *ctx, const void *buf, size_t n);	/**< @brief TLS write callback; NULL = use raw fd */
    void (*pkc_tls_free)(void *ctx);					/**< @brief called by pkg_close() to free TLS state */
    char pkc_addr[128];        /**< @brief transport address string (populated by future phases) */
    char pkc_addr_env[160];    /**< @brief PKG_ADDR=... env string for child spawn (future phases) */
    int  pkc_tx_kind;          /**< @brief transport kind: 0=socket/TCP, 1=pipe pair */
    int  pkc_listen_fd;        /**< @brief listening socket fd for lazy-accept (TCP, future phases) */
};
#define PKC_NULL	((struct pkg_conn *)0)
#define PKC_ERROR	((struct pkg_conn *)(-1L))


/**
 * Sends a VLS as a given message type across a pkg connection.
 */
#define pkg_send_vls(type, vlsp, pkg)	\
    pkg_send( (type), bu_vls_addr((vlsp)), bu_vls_strlen((vlsp))+1, (pkg) )


/**
 * Open a network connection to a host/server.
 *
 * Returns PKC_ERROR on error.
 */
PKG_EXPORT extern struct pkg_conn *pkg_open(const char *host, const char *service, const char *protocol, const char *username, const char *passwd, const struct pkg_switch* switchp, pkg_errlog errlog);

/**
 * Close a network connection.
 *
 * Gracefully release the connection block and close the connection.
 */
PKG_EXPORT extern void pkg_close(struct pkg_conn* pc);

PKG_EXPORT extern int pkg_process(struct pkg_conn *);

/**
 * Suck all data from the operating system into the internal buffer.
 *
 * This is done with large buffers, to maximize the efficiency of the
 * data transfer from kernel to user.
 *
 * It is expected that the read() system call will return as much data
 * as the kernel has, UP TO the size indicated.  The only time the
 * read() may be expected to block is when the kernel does not have
 * any data at all.  Thus, it is wise to call call this routine only
 * if:
 *
 *	a)  select() has indicated the presence of data, or
 *	b)  blocking is acceptable.
 *
 * This routine is the only place where data is taken off the network.
 * All input is appended to the internal buffer for later processing.
 *
 * Subscripting was used for pkc_incur/pkc_inend to avoid having to
 * recompute pointers after a realloc().
 *
 * Returns -
 *	-1 on error
 *	 0 on EOF
 *	 1 success
 */
PKG_EXPORT extern int pkg_suckin(struct pkg_conn *);

/**
 * Send a message on the connection.
 *
 * Send the user's data, prefaced with an identifying header which
 * contains a message type value.  All header fields are exchanged in
 * "network order".
 *
 * Note that the whole message (header + data) should be transmitted
 * by TCP with only one TCP_PUSH at the end, due to the use of
 * writev().
 *
 * Returns number of bytes of user data actually sent.
 */
PKG_EXPORT extern int pkg_send(int type, const char *buf, size_t len, struct pkg_conn* pc);

/**
 * Send a two part message on the connection.
 *
 * Exactly like pkg_send, except user's data is located in two
 * disjoint buffers, rather than one.  Fiendishly useful!
 */
PKG_EXPORT extern int pkg_2send(int type, const char *buf1, size_t len1, const char *buf2, size_t len2, struct pkg_conn* pc);

/**
 * Send a message that doesn't need a push.
 *
 * Exactly like pkg_send except no "push" is necessary here.  If the
 * packet is sufficiently small (MAXQLEN) it will be placed in the
 * pkc_stream buffer (after flushing this buffer if there insufficient
 * room).  If it is larger than this limit, it is sent via pkg_send
 * (who will do a pkg_flush if there is already data in the stream
 * queue).
 *
 * Returns number of bytes of user data actually sent (or queued).
 */
PKG_EXPORT extern int pkg_stream(int type, const char *buf, size_t len, struct pkg_conn* pc);

/**
 * Empty the stream buffer of any queued messages.
 *
 * Flush any pending data in the pkc_stream buffer.
 *
 * Returns < 0 on failure, else number of bytes sent.
 */
PKG_EXPORT extern int pkg_flush(struct pkg_conn* pc);

/**
 * Wait for a specific msg, user buf, processing others.
 *
 * This routine implements a blocking read on the network connection
 * until a message of 'type' type is received.  This can be useful for
 * implementing the synchronous portions of a query/reply exchange.
 * All messages of any other type are processed by pkg_block().
 *
 * Returns the length of the message actually received, or -1 on error.
 */
PKG_EXPORT extern int pkg_waitfor(int type, char *buf, size_t len, struct pkg_conn* pc);

/**
 * Wait for specific msg, malloc buf, processing others.
 *
 * This routine implements a blocking read on the network connection
 * until a message of 'type' type is received.  This can be useful for
 * implementing the synchronous portions of a query/reply exchange.
 * All messages of any other type are processed by pkg_block().
 *
 * The buffer to contain the actual message is acquired via malloc(),
 * and the caller must free it.
 *
 * Returns pointer to message buffer, or NULL.
 */
PKG_EXPORT extern char *pkg_bwaitfor(int type, struct pkg_conn* pc);

/**
 * Wait until a full message has been read.
 *
 * This routine blocks, waiting for one complete message to arrive
 * from the network.  The actual handling of the message is done with
 * _pkg_dispatch(), which invokes the user-supplied message handler.
 *
 * This routine can be used in a loop to pass the time while waiting
 * for a flag to be changed by the arrival of an asynchronous message,
 * or for the arrival of a message of uncertain type.
 *
 * The companion routine is pkg_process(), which does not block.
 *
 * Control returns to the caller after one full message is processed.
 * Returns -1 on error, etc.
 */
PKG_EXPORT extern int pkg_block(struct pkg_conn* pc);

/****************************
 * Transport accessors      *
 ****************************/

/**
 * Return the file descriptor a caller should pass to select(), poll(),
 * QSocketNotifier, libuv, etc. for read-readiness notification.
 *
 * Hides the distinction between bidirectional sockets (where read and
 * write share a single fd) and pipe-pair connections (where the read
 * end is a separate fd kept internally as pkc_in_fd).  Callers should
 * use this in preference to looking at pkc_fd / pkc_in_fd directly.
 *
 * Returns -1 on error.
 */
PKG_EXPORT extern int pkg_get_read_fd(const struct pkg_conn *pc);

/**
 * Return the file descriptor used for write-readiness notification.
 * Equal to pkg_get_read_fd() for bidirectional transports (TCP /
 * socketpair); for pipe-pair connections this is the write fd.
 *
 * Returns -1 on error.
 */
PKG_EXPORT extern int pkg_get_write_fd(const struct pkg_conn *pc);

/**
 * Return non-zero if the connection uses split read/write fds (i.e.
 * the unidirectional pipe transport; pkc_in_fd != pkc_out_fd).
 * Use this in preference to inspecting pkc_fd / pkc_in_fd / pkc_out_fd
 * directly so future transport changes remain source-compatible.
 */
PKG_EXPORT extern int pkg_is_stdio_mode(const struct pkg_conn *pc);

/**
 * Set the kernel send-buffer size on the underlying socket
 * (equivalent to setsockopt(SOL_SOCKET, SO_SNDBUF)).
 *
 * Silently succeeds (returns 0) for transports where send-buffer
 * tuning is not applicable (pipe / split-fd connections).
 *
 * Returns 0 on success, -1 on error.
 */
PKG_EXPORT extern int pkg_set_send_buffer(struct pkg_conn *pc, size_t bytes);

/**
 * Set the kernel receive-buffer size on the underlying socket
 * (equivalent to setsockopt(SOL_SOCKET, SO_RCVBUF)).
 *
 * Silently succeeds (returns 0) for transports where receive-buffer
 * tuning is not applicable.  Returns 0 on success, -1 on error.
 */
PKG_EXPORT extern int pkg_set_recv_buffer(struct pkg_conn *pc, size_t bytes);

/**
 * Set TCP_NODELAY on the underlying socket.  No-op on non-TCP
 * transports.  Returns 0 on success, -1 on error.
 */
PKG_EXPORT extern int pkg_set_nodelay(struct pkg_conn *pc, int on);

/**
 * Install a TLS / framing-cipher I/O shim on the connection.
 *
 * When @p tls_read / @p tls_write are non-NULL they completely replace
 * the raw read()/write() calls inside pkg_suckin(), pkg_send(),
 * pkg_2send(), and pkg_flush().  @p tls_ctx is the opaque context
 * pointer forwarded as the first argument to both callbacks.
 *
 * @p tls_free (if non-NULL) is invoked by pkg_close() before closing
 * the underlying transport, giving the TLS layer a chance to send a
 * clean close_notify and free its own state.
 *
 * Returns 0 on success, -1 on error.
 */
PKG_EXPORT extern int pkg_set_tls(struct pkg_conn *pc,
				  void *tls_ctx,
				  ptrdiff_t (*tls_read)(void *ctx, void *buf, size_t n),
				  ptrdiff_t (*tls_write)(void *ctx, const void *buf, size_t n),
				  void (*tls_free)(void *ctx));

/**
 * Wrap an already-connected socket fd in a pkg_conn without performing
 * any network connect/accept.
 *
 * Replaces the historical pattern of allocating a pkg_conn and
 * hand-initialising pkc_magic / pkc_fd / pkc_switch / pkc_left etc.
 * Used by callers that obtain a connected socket from another framework
 * (Tcl/Qt) and want to drive it through libpkg's framing protocol.
 *
 * On Windows this also performs WinSock initialisation if necessary.
 *
 * Returns a pkg_conn handle on success, PKC_ERROR on failure.
 */
PKG_EXPORT extern struct pkg_conn *pkg_adopt_socket(int fd,
						    const struct pkg_switch *switchp,
						    pkg_errlog errlog);


/****************************
 * Transport constants       *
 ****************************/

/** Environment variable read by a child to find its IPC channel address. */
#define PKG_ADDR_ENVVAR "PKG_ADDR"

/** Optional transport preference hint for pkg_pair(). */
#define PKG_TRANSPORT_PREFER_ENVVAR "PKG_TRANSPORT_PREFER"

/** Transport type for probing preference. */
typedef enum {
    PKG_TRANSPORT_AUTO   = 0, /**< @brief Use default probe order */
    PKG_TRANSPORT_PIPE   = 1, /**< @brief Anonymous pipe transport */
    PKG_TRANSPORT_SOCKET = 2, /**< @brief POSIX socketpair transport */
    PKG_TRANSPORT_TCP    = 3  /**< @brief TCP loopback transport */
} pkg_transport_t;


/****************************
 * Pair / connect API        *
 ****************************/

/**
 * Create a connected pair of pkg_conn handles.
 *
 * Probe order: pipe -> socketpair -> TCP loopback (or use preferred).
 * Returns 0 on success, -1 on failure.
 */
PKG_EXPORT extern int pkg_pair(struct pkg_conn **parent_end,
			       struct pkg_conn **child_end,
			       const struct pkg_switch *switchp,
			       pkg_errlog errlog);

PKG_EXPORT extern int pkg_pair_prefer(struct pkg_conn **parent_end,
				      struct pkg_conn **child_end,
				      const struct pkg_switch *switchp,
				      pkg_errlog errlog,
				      pkg_transport_t preferred);

/**
 * Return a "KEY=VALUE" env string for passing to a spawned child.
 * The pointer is valid until pkg_close().  Format: "PKG_ADDR=<addr>".
 */
PKG_EXPORT extern const char *pkg_child_addr_env(struct pkg_conn *pc);

/**
 * Return just the raw address string (e.g. "pipe:4,7" or "socket:5")
 * for use in argv["-I addr"] style arguments to a child process.
 * The pointer is valid until pkg_close().
 */
PKG_EXPORT extern const char *pkg_child_addr(struct pkg_conn *pc);

/**
 * Connect the child side from an address string.
 * Returns pkg_conn* on success, PKC_ERROR on failure.
 */
PKG_EXPORT extern struct pkg_conn *pkg_connect_addr(const char *addr,
						    const struct pkg_switch *switchp,
						    pkg_errlog errlog);

/**
 * Connect using the PKG_ADDR env var (child side).
 */
PKG_EXPORT extern struct pkg_conn *pkg_connect_env(const struct pkg_switch *switchp,
						   pkg_errlog errlog);

/**
 * Wrap an already-open fd pair into a pkg_conn.
 */
PKG_EXPORT extern struct pkg_conn *pkg_adopt_fds(int rfd, int wfd,
						  const struct pkg_switch *switchp,
						  pkg_errlog errlog);

/**
 * Wrap stdin(0)/stdout(1) as a pkg connection (inetd/pipe mode).
 */
PKG_EXPORT extern struct pkg_conn *pkg_adopt_stdio(const struct pkg_switch *switchp,
						   pkg_errlog errlog);

/**
 * Move the connection's fds above min_fd.
 * Returns 0 on success, -1 on error.
 */
PKG_EXPORT extern int pkg_move_high_fd(struct pkg_conn *pc, int min_fd);


/****************************
 * Listener API              *
 ****************************/

struct pkg_listener;
typedef struct pkg_listener pkg_listener_t;

PKG_EXPORT extern pkg_listener_t *pkg_listen(const char *service,
					     const char *iface_or_null,
					     int backlog,
					     pkg_errlog errlog);
PKG_EXPORT extern struct pkg_conn *pkg_accept(pkg_listener_t *L,
					      const struct pkg_switch *switchp,
					      pkg_errlog errlog,
					      int nonblocking);
PKG_EXPORT extern int pkg_get_listener_fd(const pkg_listener_t *L);
PKG_EXPORT extern int pkg_get_listener_port(const pkg_listener_t *L);
PKG_EXPORT extern void pkg_listener_close(pkg_listener_t *L);


/****************************
 * Multiplexer API           *
 ****************************/

struct pkg_mux;
typedef struct pkg_mux pkg_mux_t;

PKG_EXPORT extern pkg_mux_t *pkg_mux_create(void);
PKG_EXPORT extern void pkg_mux_destroy(pkg_mux_t *m);
PKG_EXPORT extern int pkg_mux_add_conn(pkg_mux_t *m, const struct pkg_conn *pc);
PKG_EXPORT extern int pkg_mux_add_listener(pkg_mux_t *m, const pkg_listener_t *L);
PKG_EXPORT extern int pkg_mux_add_fd(pkg_mux_t *m, int fd, int is_socket);
PKG_EXPORT extern void pkg_mux_remove_fd(pkg_mux_t *m, int fd);
PKG_EXPORT extern int pkg_mux_wait(pkg_mux_t *m, int timeout_ms);
PKG_EXPORT extern int pkg_mux_is_ready_conn(const pkg_mux_t *m, const struct pkg_conn *pc);
PKG_EXPORT extern int pkg_mux_is_ready_listener(const pkg_mux_t *m, const pkg_listener_t *L);
PKG_EXPORT extern int pkg_mux_is_ready_fd(const pkg_mux_t *m, int fd);


/****************************
 * Data conversion routines *
 ****************************/

/**
 * Get a 16-bit short from a char[2] array
 */
PKG_EXPORT extern unsigned short pkg_gshort(char *buf);

/**
 * Get a 32-bit long from a char[4] array
 */
PKG_EXPORT extern unsigned long pkg_glong(char *buf);

/**
 * Put a 16-bit short into a char[2] array
 */
PKG_EXPORT extern char *pkg_pshort(char *buf, unsigned short s);

/**
 * Put a 32-bit long into a char[4] array
 */
PKG_EXPORT extern char *pkg_plong(char *buf, unsigned long l);

/**
 * returns a human-readable string describing this version of the
 * LIBPKG library.
 */
PKG_EXPORT extern const char *pkg_version(void);

#ifdef __cplusplus
}
#endif

#endif /* PKG_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
