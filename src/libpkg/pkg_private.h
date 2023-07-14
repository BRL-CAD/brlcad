/*                     P K G _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2023 United States Government as represented by
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

#ifndef PKG_PRIVATE_H
#define PKG_PRIVATE_H

#include "common.h"
#include "pkg.h"

#ifdef __cplusplus
extern "C" {
#endif

#define	PKG_STREAMLEN	(32*1024)
struct pkg_conn_impl {
    int	pkc_fd;					/**< @brief TCP connection fd */
    int pkc_in_fd;                              /**< @brief input connection fd */
    int pkc_out_fd;                             /**< @brief output connection fd */
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
};

#ifdef __cplusplus
}
#endif

#endif /* PKG_PRIVATE_H */

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
