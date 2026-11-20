/*                        F B S E R V . H
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
/** @addtogroup libdm */
/** @{ */
/** @file fbserv.h
 *
 * @brief
 * This header holds generic routines and data structures used for TCP based
 * (and, via libpkg, local-IPC based) communication between a framebuffer and a
 * remote process.  Variations on this logic, based originally on the
 * stand-alone fbserv program, are at the core of MGED and Archer's ability to
 * display incoming image data from a separate rt process.
 *
 * Asynchronous interprocess communication and event monitoring is (as of 2021)
 * still very much platform and toolkit specific.  Hence, these data structures
 * contain some void pointers which are used by individual applications to
 * connect their own specific methods (for example, Tcl_Channel) to handle this
 * problem.  Improving this to be more generic and less dependent on specific
 * toolkits and/or platform mechanisms would be a laudable goal, if practical.
 *
 * pkg IPC path (fbs_open_ipc):
 *   Instead of binding a TCP listen socket, creates a pkg_pair() and
 *   immediately wraps the parent end as a pre-connected pkg_conn client.
 *   The child-end address and descriptors are retrieved with the APIs below,
 *   passed only to the spawned subprocess, and released in the parent after
 *   process creation.
 *
 */

#ifndef DM_FBSERV_H
#define DM_FBSERV_H

#include "common.h"
#include "pkg.h"
#include "dm/defines.h"

__BEGIN_DECLS

/* Framebuffer server object */

#define NET_LONG_LEN 4 /**< @brief # bytes to network long */
#define MAX_CLIENTS 32
#define MAX_PORT_TRIES 100
#define FBS_CALLBACK_NULL (void (*)(void))NULL
#define FBSERV_OBJ_NULL (struct fbserv_obj *)NULL

struct fbserv_obj;

struct fbserv_listener {
    int fbsl_fd;                        /**< @brief socket fd to listen for connections (copy of listener fd) */
    void *fbsl_chan;                    /**< @brief platform/toolkit specific channel */
    int fbsl_port;                      /**< @brief port number to listen on */
    int fbsl_listen;                    /**< @brief !0 means listen for connections */
    struct fbserv_obj *fbsl_fbsp;       /**< @brief points to its fbserv object */
    struct pkg_conn *fbsl_ipc_child;    /**< @brief IPC child-end channel (NULL when using TCP) */
    struct pkg_listener *fbsl_listener; /**< @brief TCP listener (NULL when using IPC or Tcl channel) */
};


struct fbserv_client {
    int fbsc_fd;                        /**< @brief socket to send data down */
    void *fbsc_chan;                    /**< @brief platform/toolkit specific channel */
    void *fbsc_handler;                 /**< @brief platform/toolkit specific handler */
    struct pkg_conn *fbsc_pkg;
    struct fbserv_obj *fbsc_fbsp;       /**< @brief points to its fbserv object */
    int fbsc_auth_ok;                   /**< @brief !0 = client has sent a valid MSG_FBAUTH */
    int fbsc_pending_drop;              /**< @brief !0 = drop this client after pkg_process() returns */
    int fbsc_is_ipc;                    /**< @brief !0 = client is connected via IPC (not TCP) */
};


struct fbserv_obj {
    struct fb *fbs_fbp;                            /**< @brief framebuffer pointer */
    void *fbs_interp;                              /**< @brief interpreter */
    struct fbserv_listener fbs_listener;           /**< @brief data for listening */
    struct fbserv_client fbs_clients[MAX_CLIENTS]; /**< @brief connected clients */

    int (*fbs_is_listening)(struct fbserv_obj *);          /**< @brief return 1 if listening, else 0 */
    int (*fbs_listen_on_port)(struct fbserv_obj *, int);  /**< @brief return 1 on success, 0 on failure */
    void (*fbs_open_server_handler)(struct fbserv_obj *);   /**< @brief platform/toolkit method to open listener handler */
    void (*fbs_close_server_handler)(struct fbserv_obj *);   /**< @brief platform/toolkit method to close handler listener */
    void (*fbs_open_client_handler)(struct fbserv_obj *, int, void *);   /**< @brief platform/toolkit specific client handler setup (called by fbs_new_client) */
    void (*fbs_close_client_handler)(struct fbserv_obj *, int);   /**< @brief platform/toolkit method to close handler for client at index client_id */
    /**
     * @brief Optional IPC-specific client open handler.
     *
     * When non-NULL, called by fbs_open_ipc() instead of fbs_open_client_handler
     * for clients whose connection was established via IPC (pipe/socketpair).
     * The toolkit-specific TCP client setup (e.g. QTcpSocket connections) is
     * not appropriate for IPC clients; this handler installs fd-based I/O
     * monitoring instead (e.g. Tcl_CreateFileHandler, QSocketNotifier).
     *
     * May be NULL, in which case fbs_open_client_handler is used (callers must
     * ensure that handler tolerates NULL data).
     */
    void (*fbs_open_ipc_client_handler)(struct fbserv_obj *, int, void *);
    /**
     * @brief Optional IPC-specific client close handler (mirrors fbs_close_client_handler).
     *
     * When non-NULL, called by fbs_drop_client() for IPC clients
     * (fbsc_is_ipc != 0).
     * May be NULL, in which case fbs_close_client_handler is used.
     */
    void (*fbs_close_ipc_client_handler)(struct fbserv_obj *, int);

    void (*fbs_callback)(void *);                  /**< @brief callback function */
    void *fbs_clientData;
    struct bu_vls *msgs;
    int fbs_mode;                                  /**< @brief 0-off, 1-underlay, 2-interlay, 3-overlay */

    char fbs_auth_token[65];  /**< @brief session token (64 hex chars + NUL); empty = no auth required */
    int fbs_require_auth;     /**< @brief !0 = reject clients that don't send MSG_FBAUTH */
};

DM_EXPORT extern int fbs_open(struct fbserv_obj *fbsp, int port);
DM_EXPORT extern int fbs_close(struct fbserv_obj *fbsp);
DM_EXPORT extern struct pkg_switch *fbs_pkg_switch(void);
DM_EXPORT extern void fbs_setup_socket(int fd);
DM_EXPORT extern int fbs_new_client(struct fbserv_obj *fbsp, struct pkg_conn *pcp, void *data);
DM_EXPORT extern void fbs_drop_client(struct fbserv_obj *fbsp, int sub);
DM_EXPORT extern void fbs_existing_client_handler(void *clientData, int mask);

/**
 * @brief Open an IPC-based framebuffer server (no TCP listen socket).
 *
 * Creates a pkg_pair(), wraps the parent end as a pre-connected pkg_conn
 * client (bypassing the TCP accept loop entirely), and registers it via
 * fbs_open_ipc_client_handler (or fbs_open_client_handler if the former is
 * NULL).  The child end's address is stored in fbsp->fbs_listener.fbsl_ipc_child
 * and can be retrieved with fbs_ipc_child_addr_env().
 *
 * Callers should:
 *   1. Call fbs_open_ipc() to start the server.
 *   2. Call fbs_ipc_child_addr_env() to get "PKG_ADDR=<addr>".
 *   3. Supply that variable as a per-child environment override.
 *   4. Pass the descriptors reported by fbs_ipc_child_fds() to the child.
 *   5. Call fbs_close_ipc_child() after process creation returns.
 *   6. Pass "-F 0" (or any port spec) to rt so it opens a remote framebuffer;
 *      if_remote.c will detect PKG_ADDR and use the IPC channel instead.
 *
 * fbs_close() also closes the child end if the caller has not already done
 * so with fbs_close_ipc_child().
 *
 * @return BRLCAD_OK on success, BRLCAD_ERROR if pkg_pair fails.
 */
DM_EXPORT extern int fbs_open_ipc(struct fbserv_obj *fbsp);

/**
 * @brief Return the "PKG_ADDR=<addr>" env string for the spawned child.
 *
 * Valid after a successful fbs_open_ipc() call and until
 * fbs_close_ipc_child() or fbs_close() is called.  Returns NULL if no IPC
 * child endpoint is active.
 *
 * The string is owned by the internal channel struct and must not be freed
 * by the caller.
 */
DM_EXPORT extern const char *fbs_ipc_child_addr_env(struct fbserv_obj *fbsp);

/**
 * Return the descriptors backing the IPC child endpoint.
 *
 * @param[in] fbsp framebuffer server object
 * @param[out] fds receives the read and write descriptors
 *
 * @return 1 if both directions share one descriptor, 2 if they are separate,
 * or 0 if no IPC child endpoint is active.
 */
DM_EXPORT extern size_t fbs_ipc_child_fds(struct fbserv_obj *fbsp, int fds[2]);

/**
 * Release the parent's copy of the IPC child endpoint after spawning the
 * child.  The server endpoint remains active until the child disconnects.
 */
DM_EXPORT extern void fbs_close_ipc_child(struct fbserv_obj *fbsp);

/**
 * Initialise @p fbsp->fbs_auth_token for session authentication.
 *
 * If the FBSERV_TOKEN environment variable is already set to a valid
 * 64-hex-char token, that value is used directly so that the hosting
 * application can pre-supply a known token and pass the same value to
 * child processes (e.g. set FBSERV_TOKEN before execing rt/pix-fb).
 * Token authentication works regardless of whether TLS is enabled.
 *
 * If FBSERV_TOKEN is not set or is the wrong length, a fresh random
 * 256-bit token is generated.  Falls back to /dev/urandom or a
 * time+PID PRNG when OpenSSL is not available.
 *
 * Call this before fbs_open() so the token is ready for the first
 * connecting client.  Returns a pointer to fbsp->fbs_auth_token.
 */
DM_EXPORT extern const char *fbs_generate_token(struct fbserv_obj *fbsp);

/** Generate a fresh framebuffer authentication token into a 65-byte buffer. */
DM_EXPORT extern int fbs_create_token(char *token, size_t token_size);


__END_DECLS

#endif /* DM_FBSERV_H */
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
