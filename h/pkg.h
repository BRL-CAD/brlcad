/*
 *  			P K G . H
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
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
#define PKG_H_VERSION "@(#)$Header$ (ARL)"
#endif

#ifndef PKG_H_SEENYET
#define PKG_H_SEENYET

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
	struct pkg_switch *pkc_switch;	/* Array of message handlers */
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


extern struct pkg_conn *pkg_open();
extern struct pkg_conn *pkg_transerver();
extern int pkg_permserver();
extern struct pkg_conn *pkg_getclient();
extern void pkg_close();
extern int pkg_send();
extern int pkg_2send();
extern int pkg_stream();
extern int pkg_flush();
extern int pkg_waitfor();
extern char *pkg_bwaitfor();
extern int pkg_get();
extern int pkg_block();

/* Data conversion routines */
extern unsigned short pkg_gshort();
extern unsigned long pkg_glong();
extern char *pkg_pshort(), *pkg_plong();

#endif /* PKG_H_SEENYET */
