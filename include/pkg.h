/*                           P K G . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file pkg.h
 *
 *  Data structures and manifest constants for use with the PKG library.
 *
 *  Authors -
 *	Michael John Muuss
 *	Charles M. Kennedy
 *	Phillip Dykstra
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
#define PKG_H_VERSION "@(#)$Header$ (ARL)"
#endif

#ifndef PKG_H_SEENYET
#define PKG_H_SEENYET

#ifndef PKG_EXPORT
#   if defined(_WIN32) && !defined(__CYGWIN__) && defined(BRLCAD_DLL)
#      ifdef PKG_EXPORT_DLL
#         define PKG_EXPORT __declspec(dllexport)
#      else
#         define PKG_EXPORT __declspec(dllimport)
#      endif
#   else
#      define PKG_EXPORT
#   endif
#endif

struct pkg_switch {
	unsigned short	pks_type;	/* Type code */
	void	(*pks_handler)();	/* Message Handler */
	char	*pks_title;		/* Description of message type */
};

/*
 *  Format of the message header as it is transmitted over the network
 *  connection.  Internet network order is used.
 *  User Code should access pkc_len and pkc_type rather than
 *  looking into the header directly.
 *  Users should never need to know what this header looks like.
 */
#define PKG_MAGIC	0x41FE
struct pkg_header {
	unsigned char	pkh_magic[2];		/* Ident */
	unsigned char	pkh_type[2];		/* Message Type */
	unsigned char	pkh_len[4];		/* Byte count of remainder */
};

#define	PKG_STREAMLEN	(32*1024)
struct pkg_conn {
	int		pkc_fd;		/* TCP connection fd */
	const struct pkg_switch *pkc_switch;	/* Array of message handlers */
	void		(*pkc_errlog)(); /* Error message logger */
	struct pkg_header pkc_hdr;	/* hdr of cur msg */
	long		pkc_len;	/* pkg_len, in host order */
	unsigned short	pkc_type;	/* pkg_type, in host order */
	/* OUTPUT BUFFER */
	char		pkc_stream[PKG_STREAMLEN]; /* output stream */
	int		pkc_magic;	/* for validating pointers */
	int		pkc_strpos;	/* index into stream buffer */
	/* FIRST LEVEL INPUT BUFFER */
	char		*pkc_inbuf;	/* input stream buffer */
	int		pkc_incur;	/* current pos in inbuf */
	int		pkc_inend;	/* first unused pos in inbuf */
	int		pkc_inlen;	/* length of pkc_inbuf */
	/* DYNAMIC BUFFER FOR USER */
	int		pkc_left;	/* # bytes pkg_get expects */
		/* neg->read new hdr, 0->all here, >0 ->more to come */
	char		*pkc_buf;	/* start of dynamic buf */
	char		*pkc_curpos;	/* current position in pkg_buf */
};
#define PKC_NULL	((struct pkg_conn *)0)
#define PKC_ERROR	((struct pkg_conn *)(-1L))


#define pkg_send_vls(type,vlsp,pkg)	\
	pkg_send( (type), bu_vls_addr((vlsp)), bu_vls_strlen((vlsp))+1, (pkg) )


PKG_EXPORT extern int pkg_init();
PKG_EXPORT extern void pkg_terminate();
PKG_EXPORT extern int pkg_process(register struct pkg_conn *);
PKG_EXPORT extern int pkg_suckin(register struct pkg_conn *);
PKG_EXPORT extern struct pkg_conn *pkg_open();
PKG_EXPORT extern struct pkg_conn *pkg_transerver();
PKG_EXPORT extern int pkg_permserver();
PKG_EXPORT extern struct pkg_conn *pkg_getclient(int fd, const struct pkg_switch *switchp, void (*errlog) (/* ??? */), int nodelay);
PKG_EXPORT extern void pkg_close();
PKG_EXPORT extern int pkg_send();
PKG_EXPORT extern int pkg_2send();
PKG_EXPORT extern int pkg_stream();
PKG_EXPORT extern int pkg_flush();
PKG_EXPORT extern int pkg_waitfor();
PKG_EXPORT extern char *pkg_bwaitfor();
PKG_EXPORT extern int pkg_get();
PKG_EXPORT extern int pkg_block();

/* Data conversion routines */
PKG_EXPORT extern unsigned short pkg_gshort();
PKG_EXPORT extern unsigned long pkg_glong();
PKG_EXPORT extern char *pkg_pshort(unsigned char *msgp, short unsigned int s);
PKG_EXPORT extern char *pkg_plong();

#endif /* PKG_H_SEENYET */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
