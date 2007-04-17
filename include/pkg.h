/*                           P K G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
 * @brief
 *  Data structures and manifest constants for use with the PKG library.
 *
 *
 * @author	Michael John Muuss
 * @author	Charles M. Kennedy
 * @author	Phillip Dykstra
 *
 * @par Source
 *	The U. S. Army Research Laboratory
 * @n	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
#define PKG_H_VERSION "@(#)$Header$ (ARL)"
#endif

#ifndef PKG_H_SEENYET
#define PKG_H_SEENYET

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
 *  The setting of USE_PROTOTYPES is done in machine.h
 */
#if USE_PROTOTYPES
#	define	PKG_EXTERN(type_and_name,args)	extern type_and_name args
#	define	PKG_ARGS(args)			args
#else
#	define	PKG_EXTERN(type_and_name,args)	extern type_and_name()
#	define	PKG_ARGS(args)			()
#endif


#ifdef __cplusplus
extern "C" {
#endif

struct pkg_conn;

struct pkg_switch {
    unsigned short	pks_type;	/**< @brief Type code */
    void	(*pks_handler)PKG_ARGS((struct pkg_conn*, char*));  /**< @brief Message Handler */
    char	*pks_title;		/**< @brief Description of message type */
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
    unsigned char	pkh_magic[2];		/**< @brief Ident */
    unsigned char	pkh_type[2];		/**< @brief Message Type */
    unsigned char	pkh_len[4];		/**< @brief Byte count of remainder */
};

#define	PKG_STREAMLEN	(32*1024)
struct pkg_conn {
    int		pkc_fd;		/**< @brief TCP connection fd */
    const struct pkg_switch *pkc_switch;	/**< @brief Array of message handlers */
    void	(*pkc_errlog)PKG_ARGS((char *msg)); /**< @brief Error message logger */
    struct pkg_header pkc_hdr;	/**< @brief hdr of cur msg */
    long	pkc_len;	/**< @brief pkg_len, in host order */
    unsigned short	pkc_type;	/**< @brief pkg_type, in host order */
    /* OUTPUT BUFFER */
    char	pkc_stream[PKG_STREAMLEN]; /**< @brief output stream */
    int		pkc_magic;	/**< @brief for validating pointers */
    int		pkc_strpos;	/**< @brief index into stream buffer */
    /* FIRST LEVEL INPUT BUFFER */
    char		*pkc_inbuf;	/**< @brief input stream buffer */
    int		pkc_incur;	/**< @brief current pos in inbuf */
    int		pkc_inend;	/**< @brief first unused pos in inbuf */
    int		pkc_inlen;	/**< @brief length of pkc_inbuf */
    /* DYNAMIC BUFFER FOR USER */
    int		pkc_left;	/**< @brief #  bytes pkg_get expects */
    /* neg->read new hdr, 0->all here, >0 ->more to come */
    char		*pkc_buf;	/**< @brief start of dynamic buf */
    char		*pkc_curpos;	/**< @brief current position in pkg_buf */
};
#define PKC_NULL	((struct pkg_conn *)0)
#define PKC_ERROR	((struct pkg_conn *)(-1L))


#define pkg_send_vls(type,vlsp,pkg)	\
	pkg_send( (type), bu_vls_addr((vlsp)), bu_vls_strlen((vlsp))+1, (pkg) )


PKG_EXPORT PKG_EXTERN(int pkg_init, ());
PKG_EXPORT PKG_EXTERN(void pkg_terminate, ());
PKG_EXPORT PKG_EXTERN(int pkg_process, (register struct pkg_conn *));
PKG_EXPORT PKG_EXTERN(int pkg_suckin, (register struct pkg_conn *));
PKG_EXPORT PKG_EXTERN(struct pkg_conn *pkg_open, (const char *host, const char *service, const char *protocol, const char *uname, const char *passwd, const struct pkg_switch* switchp, void (*errlog)PKG_ARGS((char *msg))));
PKG_EXPORT PKG_EXTERN(struct pkg_conn *pkg_transerver, (const struct pkg_switch* switchp, void (*errlog)PKG_ARGS((char *msg))));
PKG_EXPORT PKG_EXTERN(int pkg_permserver, (const char *service, const char *protocol, int backlog, void (*errlog)PKG_ARGS((char *msg))));
PKG_EXPORT PKG_EXTERN(int pkg_permserver_ip, (const char *ipOrHostname, const char *service, const char *protocol, int backlog, void (*errlog)PKG_ARGS((char *msg))));
PKG_EXPORT PKG_EXTERN(struct pkg_conn *pkg_getclient, (int fd, const struct pkg_switch *switchp, void (*errlog)PKG_ARGS((char *msg)), int nodelay));
PKG_EXPORT PKG_EXTERN(void pkg_close, (struct pkg_conn* pc));
PKG_EXPORT PKG_EXTERN(int pkg_send, (int type, char *buf, int len, struct pkg_conn* pc));
PKG_EXPORT PKG_EXTERN(int pkg_2send, (int type, char *buf1, int len1, char *buf2, int len2, struct pkg_conn* pc));
PKG_EXPORT PKG_EXTERN(int pkg_stream, (int type, char *buf, int len, struct pkg_conn* pc));
PKG_EXPORT PKG_EXTERN(int pkg_flush, (struct pkg_conn* pc));
PKG_EXPORT PKG_EXTERN(int pkg_waitfor, (int type, char *buf, int len, struct pkg_conn* pc));
PKG_EXPORT PKG_EXTERN(char *pkg_bwaitfor, (int type, struct pkg_conn* pc));
PKG_EXPORT PKG_EXTERN(int pkg_block, (struct pkg_conn* pc));

/* Data conversion routines */
PKG_EXPORT PKG_EXTERN(unsigned short pkg_gshort, (char *buf));
PKG_EXPORT PKG_EXTERN(unsigned long pkg_glong, (char *buf));
PKG_EXPORT PKG_EXTERN(char *pkg_pshort, (char *buf, short unsigned int s));
PKG_EXPORT PKG_EXTERN(char *pkg_plong, (char *buf, long unsigned int l));

PKG_EXPORT PKG_EXTERN(const char *pkg_version, (void));

#ifdef __cplusplus
}
#endif

#endif /* PKG_H_SEENYET */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
