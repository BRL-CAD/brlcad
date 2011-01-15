/*                           P K G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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

#ifndef __PKG_H__
#define __PKG_H__

/* for size_t */
#include <stddef.h>

#ifndef PKG_EXPORT
#  if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#    ifdef PKG_EXPORT_DLL
#      define PKG_EXPORT __declspec(dllexport)
#    else
#      define PKG_EXPORT __declspec(dllimport)
#    endif
#  else
#    define PKG_EXPORT
#  endif
#endif

/*
 *  Macros for providing function prototypes, regardless of whether
 *  the compiler understands them or not.
 *  It is vital that the argument list given for "args" be enclosed
 *  in parens.
 */
#define PKG_EXTERN(type_and_name, args) extern type_and_name args
#define PKG_ARGS(args) args


#ifdef __cplusplus
extern "C" {
#endif

struct pkg_conn;

typedef void (*pkg_callback)PKG_ARGS((struct pkg_conn*, char*));
typedef void (*pkg_errlog)PKG_ARGS((char *msg));

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
PKG_EXPORT PKG_EXTERN(struct pkg_conn *pkg_open, (const char *host, const char *service, const char *protocol, const char *username, const char *passwd, const struct pkg_switch* switchp, pkg_errlog errlog));

/**
 * Close a network connection.
 *
 * Gracefully release the connection block and close the connection.
 */
PKG_EXPORT PKG_EXTERN(void pkg_close, (struct pkg_conn* pc));

/**
 * 
 */
PKG_EXPORT PKG_EXTERN(int pkg_process, (struct pkg_conn *));

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
PKG_EXPORT PKG_EXTERN(int pkg_suckin, (struct pkg_conn *));

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
PKG_EXPORT PKG_EXTERN(int pkg_send, (int type, const char *buf, size_t len, struct pkg_conn* pc));

/**
 * Send a two part message on the connection.
 *
 * Exactly like pkg_send, except user's data is located in two
 * disjoint buffers, rather than one.  Fiendishly useful!
 */
PKG_EXPORT PKG_EXTERN(int pkg_2send, (int type, const char *buf1, size_t len1, const char *buf2, size_t len2, struct pkg_conn* pc));

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
PKG_EXPORT PKG_EXTERN(int pkg_stream, (int type, const char *buf, size_t len, struct pkg_conn* pc));

/**
 * Empty the stream buffer of any queued messages.
 *
 * Flush any pending data in the pkc_stream buffer.
 *
 * Returns < 0 on failure, else number of bytes sent.
 */
PKG_EXPORT PKG_EXTERN(int pkg_flush, (struct pkg_conn* pc));

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
PKG_EXPORT PKG_EXTERN(int pkg_waitfor, (int type, char *buf, size_t len, struct pkg_conn* pc));

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
PKG_EXPORT PKG_EXTERN(char *pkg_bwaitfor, (int type, struct pkg_conn* pc));

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
PKG_EXPORT PKG_EXTERN(int pkg_block, (struct pkg_conn* pc));

/**
 * Become a transient network server
 *
 * Become a one-time server on a given open connection.  A client has
 * already called and we have already answered.  This will be a
 * servers starting condition if he was created by a process like the
 * UNIX inetd.
 *
 * Returns PKC_ERROR or a pointer to a pkg_conn structure.
 */
PKG_EXPORT PKG_EXTERN(struct pkg_conn *pkg_transerver, (const struct pkg_switch* switchp, pkg_errlog errlog));

/**
 * Create a network server, and listen for connection.
 *
 * We are now going to be a server for the indicated service.  Hang a
 * LISTEN, and return the fd to select() on waiting for new
 * connections.
 *
 * Returns fd to listen on (>=0), -1 on error.
 */
PKG_EXPORT PKG_EXTERN(int pkg_permserver, (const char *service, const char *protocol, int backlog, pkg_errlog));

/**
 * Create network server from IP address, and listen for connection.
 *
 * We are now going to be a server for the indicated service.  Hang a
 * LISTEN, and return the fd to select() on waiting for new
 * connections.
 *
 * Returns fd to listen on (>=0), -1 on error.
 */
PKG_EXPORT PKG_EXTERN(int pkg_permserver_ip, (const char *ipOrHostname, const char *service, const char *protocol, int backlog, pkg_errlog errlog));

/**
 * As permanent network server, accept a new connection
 *
 * Given an fd with a listen outstanding, accept the connection.  When
 * poll == 0, accept is allowed to block.  When poll != 0, accept will
 * not block.
 *
 * Returns -
 *	       >0 ptr to pkg_conn block of new connection
 *	 PKC_NULL accept would block, try again later
 *	PKC_ERROR fatal error
 */
PKG_EXPORT PKG_EXTERN(struct pkg_conn *pkg_getclient, (int fd, const struct pkg_switch *switchp, pkg_errlog errlog, int nodelay));


/****************************
 * Data conversion routines *
 ****************************/

/**
 * Get a 16-bit short from a char[2] array
 */
PKG_EXPORT PKG_EXTERN(unsigned short pkg_gshort, (char *buf));

/**
 * Get a 32-bit long from a char[4] array
 */
PKG_EXPORT PKG_EXTERN(unsigned long pkg_glong, (char *buf));

/**
 * Put a 16-bit short into a char[2] array
 */
PKG_EXPORT PKG_EXTERN(char *pkg_pshort, (char *buf, unsigned short s));

/**
 * Put a 32-bit long into a char[4] array
 */
PKG_EXPORT PKG_EXTERN(char *pkg_plong, (char *buf, unsigned long l));

/**
 * returns a human-readable string describing this version of the
 * LIBPKG library.
 */
PKG_EXPORT PKG_EXTERN(const char *pkg_version, (void));

#ifdef __cplusplus
}
#endif

#endif /* __PKG_H__ */

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
