/*
 *  			P K G . H
 */
#ifndef PKG_H_SEENYET
#define PKG_H_SEENYET

struct pkg_switch {
	char	pks_type;		/* Type code */
	int	(*pks_handler)();	/* Message Handler */
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
	unsigned short	pkg_magic;		/* Ident */
	unsigned short	pkg_type;		/* Message Type */
	long		pkg_len;		/* Byte count of remainder */
};

#define	PKG_STREAMLEN	4096
struct pkg_conn {
	int		pkc_fd;		/* TCP connection fd */
	struct pkg_switch *pkc_switch;	/* Array of message handlers */
	void		(*pkc_errlog)(); /* Error message logger */
	int		pkc_magic;	/* for validating pointers */
	int		pkc_left;	/* # bytes pkg_get expects */
		/* neg->read new hdr, 0->all here, >0 ->more to come */
	char		*pkc_buf;	/* start of dynamic buf */
	char		*pkc_curpos;	/* current position in pkg_buf */
	struct pkg_header pkc_hdr;	/* hdr of cur msg */
#define			pkc_len		pkc_hdr.pkg_len
#define			pkc_type	pkc_hdr.pkg_type
	char		pkc_stream[PKG_STREAMLEN];
	int		pkc_strpos;	/* index into stream buffer */
};
#define PKC_NULL	((struct pkg_conn *)0)
#define PKC_ERROR	((struct pkg_conn *)(-1))

extern struct pkg_conn *pkg_open();
extern int pkg_initserver();
extern struct pkg_conn *pkg_getclient();
extern struct pkg_conn *pkg_makeconn();
extern void pkg_close();
extern int pkg_send();
extern int pkg_stream();
extern int pkg_flush();
extern int pkg_waitfor();
extern char *pkg_bwaitfor();
extern int pkg_get();
extern int pkg_block();

#endif PKG_H_SEENYET
