/*
 *			P K G . C
 *
 *  Routines to manage multiplexing and de-multiplexing synchronous
 *  and asynchronous messages across stream connections.
 *
 *  Functions -
 *	pkg_gshort	Get a 16-bit short from a char[2] array
 *	pkg_glong	Get a 32-bit long from a char[4] array
 *	pkg_pshort	Put a 16-bit short into a char[2] array
 *	pkg_plong	Put a 32-bit long into a char[4] array
 *	pkg_open	Open a network connection to a host/server
 *	pkg_transerver	Become a transient network server
 *	pkg_permserver	Create a network server, and listen for connection
 *	pkg_getclient	As permanent network server, accept a new connection
 *	pkg_close	Close a network connection
 *	pkg_send	Send a message on the connection
 *	pkg_2send	Send a two part message on the connection
 *	pkg_stream	Send a message that doesn't need a push
 *	pkg_flush	Empty the stream buffer of any queued messages
 *	pkg_waitfor	Wait for a specific msg, user buf, processing others
 *	pkg_bwaitfor	Wait for specific msg, malloc buf, processing others
 *	pkg_get		Read bytes from connection, assembling message
 *	pkg_block	Wait until a full message has been read
 *
 *  Authors -
 *	Michael John Muuss
 *	Charles M. Kennedy
 *	Phillip Dykstra
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>		/* used by inet_addr() routine, below */

#ifdef BSD
/* 4.2BSD, 4.3BSD network stuff */
#include <sys/socket.h>
#include <sys/ioctl.h>		/* for FIONBIO */
#include <netinet/in.h>		/* for htons(), etc */
#include <netdb.h>
#include <sys/time.h>		/* for struct timeval */
#endif

#if defined(BSD) && !defined(cray) && !defined(mips)
#define	HAS_WRITEV
#endif

#ifdef HAS_WRITEV
#include <sys/uio.h>		/* for struct iovec (writev) */
#endif

#ifdef SGI_EXCELAN
#include <EXOS/exos/misc.h>
#include <EXOS/sys/socket.h>
#include <EXOS/netinet/in.h>
#include <sys/time.h>		/* for struct timeval */
#define select	bsdselect	/* bloody GL2 select() conflict */
#endif

#include <errno.h>

#include "pkg.h"

extern char *malloc();
extern void perror();
extern int errno;

int pkg_nochecking = 0;	/* set to disable extra checking for input */

/* Internal Functions */
static struct pkg_conn *pkg_makeconn();
static void pkg_errlog();
static void pkg_perror();
static int pkg_mread();
static int pkg_dispatch();
static int pkg_gethdr();

static char errbuf[80];

#define PKG_CK(p)	{if(p==PKC_NULL||p->pkc_magic!=PKG_MAGIC) {\
			sprintf(errbuf,"pkg: bad pointer x%x\n",p);\
			(p->pkc_errlog)(errbuf);abort();}}

#define	MAXQLEN	512	/* largest packet we will queue on stream */

/*
 * Routines to insert/extract short/long's into char arrays,
 * independend of machine byte order and word-alignment.
 */

/*
 *			P K G _ G S H O R T
 */
unsigned short
pkg_gshort(msgp)
	char *msgp;
{
	register unsigned char *p = (unsigned char *) msgp;
#ifdef vax
	/*
	 * vax compiler doesn't put shorts in registers
	 */
	register unsigned long u;
#else
	register unsigned short u;
#endif

	u = *p++ << 8;
	return ((unsigned short)(u | *p));
}

/*
 *			P K G _ G L O N G
 */
unsigned long
pkg_glong(msgp)
	char *msgp;
{
	register unsigned char *p = (unsigned char *) msgp;
	register unsigned long u;

	u = *p++; u <<= 8;
	u |= *p++; u <<= 8;
	u |= *p++; u <<= 8;
	return (u | *p);
}

/*
 *			P K G _ P S H O R T
 */
char *
pkg_pshort(msgp, s)
register char *msgp;
register unsigned short s;
{

	msgp[1] = s;
	msgp[0] = s >> 8;
	return(msgp+2);
}

/*
 *			P K G _ P L O N G
 */
char *
pkg_plong(msgp, l)
register char *msgp;
register unsigned long l;
{

	msgp[3] = l;
	msgp[2] = (l >>= 8);
	msgp[1] = (l >>= 8);
	msgp[0] = l >> 8;
	return(msgp+4);
}

/*
 *			P K G _ O P E N
 *
 *  We are a client.  Make a connection to the server.
 *
 *  Returns PKC_ERROR on error.
 */
struct pkg_conn *
pkg_open( host, service, protocol, uname, passwd, switchp, errlog )
char *host;
char *service;
char *protocol;
char *uname;
char *passwd;
struct pkg_switch *switchp;
void (*errlog)();
{
	struct sockaddr_in sinme;		/* Client */
	struct sockaddr_in sinhim;		/* Server */
	register struct hostent *hp;
	register int netfd;
	unsigned long addr_tmp;

	/* Check for default error handler */
	if( errlog == NULL )
		errlog = pkg_errlog;

	bzero((char *)&sinhim, sizeof(sinhim));
	bzero((char *)&sinme, sizeof(sinme));

	/* Determine port for service */
	if( atoi(service) > 0 )  {
		sinhim.sin_port = htons((unsigned short)atoi(service));
	} else {
#ifdef BSD
		register struct servent *sp;
		if( (sp = getservbyname( service, "tcp" )) == NULL )  {
			sprintf(errbuf,"pkg_open(%s,%s): unknown service\n",
				host, service );
			errlog(errbuf);
			return(PKC_ERROR);
		}
		sinhim.sin_port = sp->s_port;
#endif
#ifdef SGI_EXCELAN
		/* What routine does SGI give for this one? */
		sinhim.sin_port = htons(5558);	/* mfb service!! XXX */
#endif
	}

	/* Get InterNet address */
	if( atoi( host ) > 0 )  {
		/* Numeric */
		sinhim.sin_family = AF_INET;
#ifdef cray
		addr_tmp = inet_addr(host);
		sinhim.sin_addr = addr_tmp;
#else
		sinhim.sin_addr.s_addr = inet_addr(host);
#endif
	} else {
#ifdef BSD
		if( (hp = gethostbyname(host)) == NULL )  {
			sprintf(errbuf,"pkg_open(%s,%s): unknown host\n",
				host, service );
			errlog(errbuf);
			return(PKC_ERROR);
		}
		sinhim.sin_family = hp->h_addrtype;
		bcopy(hp->h_addr, (char *)&addr_tmp, hp->h_length);
#ifdef cray
		sinhim.sin_addr = addr_tmp;
#else
		sinhim.sin_addr.s_addr = addr_tmp;
#endif cray
#endif BSD
#ifdef SGI_EXCELAN
		char **hostp = &host;
		if((sinhim.sin_addr.s_addr = rhost(&hostp)) == -1) {
			sprintf(errbuf,"pkg_open(%s,%s): unknown host\n",
				host, service );
			errlog(errbuf);
			return(PKC_ERROR);
		}

#endif
	}

#ifdef BSD
	if( (netfd = socket(sinhim.sin_family, SOCK_STREAM, 0)) < 0 )  {
		pkg_perror( errlog, "pkg_open:  client socket" );
		return(PKC_ERROR);
	}
	sinme.sin_port = 0;		/* let kernel pick it */

	if( bind(netfd, (char *)&sinme, sizeof(sinme)) < 0 )  {
		pkg_perror( errlog, "pkg_open: bind" );
		return(PKC_ERROR);
	}

	if( connect(netfd, (char *)&sinhim, sizeof(sinhim)) < 0 )  {
		pkg_perror( errlog, "pkg_open: client connect" );
		return(PKC_ERROR);
	}
#endif
#ifdef SGI_EXCELAN
	sinme.sin_family = AF_INET;
	sinme.sin_port = 0;		/* let kernel pick it */
	if( (netfd = socket(SOCK_STREAM, 0, &sinme, 0)) <= 0 )  {
		pkg_perror( errlog, "pkg_open:  client socket" );
		return(PKC_ERROR);
	}

	if( connect(netfd, (char *)&sinhim) < 0 )  {
		pkg_perror( errlog, "pkg_open: client connect" );
		return(PKC_ERROR);
	}
#endif

	return( pkg_makeconn(netfd, switchp, errlog) );
}

/*
 *  			P K G _ T R A N S E R V E R
 *  
 *  Become a one-time server on the open connection.
 *  A client has already called and we have already answered.
 *  This will be a servers starting condition if he was created
 *  by a process like the UNIX inetd.
 *
 *  Returns PKC_ERROR or a pointer to a pkg_conn structure.
 */
struct pkg_conn *
pkg_transerver( switchp, errlog )
struct pkg_switch *switchp;
void (*errlog)();
{
	/*
	 * XXX - Somehow the system has to know what connection
	 * was accepted, it's protocol, etc.  For UNIX/inetd
	 * we use stdin.
	 */
	return( pkg_makeconn( fileno(stdin), switchp, errlog ) );
}

/*
 *  			P K G _ P E R M S E R V E R
 *  
 *  We are now going to be a server for the indicated service.
 *  Hang a LISTEN, and return the fd to select() on waiting for
 *  new connections.
 *
 *  Returns fd to listen on (>=0), -1 on error.
 */
int
pkg_permserver( service, protocol, backlog, errlog )
char *service;
char *protocol;
int backlog;
void (*errlog)();
{
	struct sockaddr_in sinme;
#ifdef SGI_EXCELAN
	struct sockaddr_in sinhim;		/* Server */
#endif
	register struct servent *sp;
	int pkg_listenfd;

	/* Check for default error handler */
	if( errlog == NULL )
		errlog = pkg_errlog;

	bzero((char *)&sinme, sizeof(sinme));

	/* Determine port for service */
#ifdef BSD
	if( (sp = getservbyname( service, "tcp" )) == NULL )  {
		sprintf(errbuf,"pkg_permserver(%s,%d): unknown service\n",
			service, backlog );
		errlog(errbuf);
		return(-1);
	}
	sinme.sin_port = sp->s_port;
#endif
	sinme.sin_family = AF_INET;
#ifdef SGI_EXCELAN
	/* What routine does SGI give for this one? */
	sinme.sin_port = htons(5558);	/* mfb service!! XXX */
#endif

#ifdef BSD
	if( (pkg_listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )  {
		pkg_perror( errlog, "pkg_permserver:  socket" );
		return(-1);
	}

	if( bind(pkg_listenfd, &sinme, sizeof(sinme)) < 0 )  {
		pkg_perror( errlog, "pkg_permserver: bind" );
		return(-1);
	}

	if( backlog > 5 )  backlog = 5;
	if( listen(pkg_listenfd, backlog) < 0 )  {
		pkg_perror( errlog, "pkg_permserver:  listen" );
		return(-1);
	}
#endif
#ifdef SGI_EXCELAN
	if( (pkg_listenfd = socket(SOCK_STREAM,0,&sinme,SO_ACCEPTCONN|SO_KEEPALIVE)) < 0 )  {
		pkg_perror( errlog, "pkg_permserver:  socket" );
		return(-1);
	}
#endif
	return(pkg_listenfd);
}

/*
 *			P K G _ G E T C L I E N T
 *
 *  Given an fd with a listen outstanding, accept the connection.
 *  When poll == 0, accept is allowed to block.
 *  When poll != 0, accept will not block.
 *
 *  Returns -
 *	>0		ptr to pkg_conn block of new connection
 *	PKC_NULL	accept would block, try again later
 *	PKC_ERROR	fatal error
 */
struct pkg_conn *
pkg_getclient(fd, switchp, errlog, nodelay)
struct pkg_switch *switchp;
void (*errlog)();
{
#ifdef BSD
	struct sockaddr_in from;
	register int s2;
	auto int fromlen = sizeof (from);
	auto int onoff;

	/* Check for default error handler */
	if( errlog == NULL )
		errlog = pkg_errlog;

	if(nodelay)  {
		onoff = 1;
		if( ioctl(fd, FIONBIO, &onoff) < 0 )
			pkg_perror( errlog, "pkg_getclient: FIONBIO 1" );
	}
	do  {
		s2 = accept(fd, (char *)&from, &fromlen);
		if (s2 < 0) {
			if(errno == EINTR)
				continue;
			if(errno == EWOULDBLOCK)
				return(PKC_NULL);
			pkg_perror( errlog, "pkg_getclient: accept" );
			return(PKC_ERROR);
		}
	}  while( s2 < 0);
	if(nodelay)  {		
		onoff = 0;
		if( ioctl(fd, FIONBIO, &onoff) < 0 )
			pkg_perror( errlog, "pkg_getclient: FIONBIO 2" );
		if( ioctl(s2, FIONBIO, &onoff) < 0 )
			pkg_perror( errlog, "pkg_getclient: FIONBIO 3");
	}

	return( pkg_makeconn(s2, switchp, errlog) );
#endif
#ifdef SGI_EXCELAN
	struct sockaddr_in from;

	/* block until someone tries to connect */
	if(accept(fd, &from) < 0){
		pkg_perror( errlog, "pkg_getclient:  accept" );
		return(PKC_ERROR);
	}

	/* Hopefully, once-only XXX */
	return( pkg_makeconn(fd, switchp, errlog) );
#endif
}

/*
 *			P K G _ M A K E C O N N
 *
 *  Internal.
 *  Malloc and initialize a pkg_conn structure.
 *  We have already connected to a client or server on the given
 *  file descriptor.
 *
 *  Returns -
 *	>0		ptr to pkg_conn block of new connection
 *	PKC_ERROR	fatal error
 */
static
struct pkg_conn *
pkg_makeconn(fd, switchp, errlog)
struct pkg_switch *switchp;
void (*errlog)();
{
	register struct pkg_conn *pc;

	/* Check for default error handler */
	if( errlog == NULL )
		errlog = pkg_errlog;

	if( (pc = (struct pkg_conn *)malloc(sizeof(struct pkg_conn)))==PKC_NULL )  {
		pkg_perror(errlog, "pkg_makeconn: malloc failure\n" );
		return(PKC_ERROR);
	}
	pc->pkc_magic = PKG_MAGIC;
	pc->pkc_fd = fd;
	pc->pkc_switch = switchp;
	pc->pkc_errlog = errlog;
	pc->pkc_left = -1;
	pc->pkc_buf = (char *)0;
	pc->pkc_curpos = (char *)0;
	pc->pkc_strpos = 0;
	return(pc);
}

/*
 *  			P K G _ C L O S E
 *  
 *  Gracefully release the connection block and close the connection.
 */
void
pkg_close(pc)
register struct pkg_conn *pc;
{
	PKG_CK(pc);
	(void)close(pc->pkc_fd);
	pc->pkc_fd = -1;		/* safety */
	if( pc->pkc_buf != (char *)0 )
		fprintf(stderr, "pkg_close(x%x):  buffer clash\n", pc->pkc_buf);
	pc->pkc_buf = (char *)0;	/* safety */
	pc->pkc_magic = 0;		/* safety */
	(void)free( (char *)pc );
}

/*
 *			P K G _ M R E A D
 *
 * Internal.
 * This function performs the function of a read(II) but will
 * call read(II) multiple times in order to get the requested
 * number of characters.  This can be necessary because pipes
 * and network connections don't deliver data with the same
 * grouping as it is written with.  Written by Robert S. Miles, BRL.
 */
static int
pkg_mread(pc, bufp, n)
struct pkg_conn *pc;
register char	*bufp;
int	n;
{
	int fd;
	register int	count = 0;
	register int	nread;

	fd = pc->pkc_fd;
	do {
		nread = read(fd, bufp, (unsigned)n-count);
		if(nread < 0)  {
			pkg_perror(pc->pkc_errlog, "pkg_mread");
			return(-1);
		}
		if(nread == 0)
			return((int)count);
		count += (unsigned)nread;
		bufp += nread;
	 } while(count < n);

	return((int)count);
}

/*
 *  			P K G _ S E N D
 *
 *  Send the user's data, prefaced with an identifying header which
 *  contains a message type value.  All header fields are exchanged
 *  in "network order".
 *
 *  Note that the whole message (header + data) should be transmitted
 *  by TCP with only one TCP_PUSH at the end, due to the use of writev().
 *
 *  Returns number of bytes of user data actually sent.
 */
int
pkg_send( type, buf, len, pc )
int type;
char *buf;
int len;
register struct pkg_conn *pc;
{
#ifdef HAS_WRITEV
	static struct iovec cmdvec[2];
#endif
	static struct pkg_header hdr;
	struct timeval tv;
	long bits;
	register int i;

	PKG_CK(pc);
	if( len < 0 )  len=0;

	/* Finish any partially read message */
	do  {
		if( pc->pkc_left > 0 )
			if( pkg_block(pc) < 0 )
				return(-1);

		if( pkg_nochecking )
			continue;
		/* Check socket for unexpected input */
		tv.tv_sec = 0;
		tv.tv_usec = 0;		/* poll -- no waiting */
		bits = 1 << pc->pkc_fd;
		i = select( pc->pkc_fd+1, &bits, (char *)0, (char *)0, &tv );
		if( i > 0 && bits )
			if( pkg_block(pc) < 0 )
				return(-1);
	} while( pc->pkc_left > 0 );

	/* Flush any queued stream output first. */
	if( pc->pkc_strpos > 0 )  {
		/*
		 * Buffered output is already queued, and needs to be
		 * flushed before sending this one.  If this pkg will
		 * also fit in the buffer, add it to the stream, and
		 * then send the whole thing with one flush.
		 * Otherwise, just flush, and proceed.
		 */
		if( len <= MAXQLEN && len <= PKG_STREAMLEN -
		    sizeof(struct pkg_header) - pc->pkc_strpos )  {
			(void)pkg_stream( type, buf, len, pc );
			return( (pkg_flush(pc) < 0) ? -1 : len );
		}
		if( pkg_flush( pc ) < 0 )
			return(-1);	/* assumes 2nd write would fail too */
	}

	pkg_pshort( hdr.pkh_magic, PKG_MAGIC );
	pkg_pshort( hdr.pkh_type, type );	/* should see if valid type */
	pkg_plong( hdr.pkh_len, len );

#ifdef HAS_WRITEV
	cmdvec[0].iov_base = (caddr_t)&hdr;
	cmdvec[0].iov_len = sizeof(hdr);
	cmdvec[1].iov_base = (caddr_t)buf;
	cmdvec[1].iov_len = len;

	/*
	 * TODO:  set this FD to NONBIO.  If not all output got sent,
	 * loop in select() waiting for capacity to go out, and
	 * reading input as well.  Prevents deadlocking.
	 */
	if( (i = writev( pc->pkc_fd, cmdvec, (len>0)?2:1 )) != len+sizeof(hdr) )  {
		if( i < 0 )  {
			pkg_perror(pc->pkc_errlog, "pkg_send: writev");
			return(-1);
		}
		sprintf(errbuf,"pkg_send of %d+%d, wrote %d\n",
			sizeof(hdr), len, i);
		(pc->pkc_errlog)(errbuf);
		return(i-sizeof(hdr));	/* amount of user data sent */
	}
#else
	(void)write( pc->pkc_fd, (char *)&hdr, sizeof(hdr) );
	if( len <= 0 )  return(0);
	if( (i = write( pc->pkc_fd, buf, len )) != len )  {
		if( i < 0 )  {
			pkg_perror(pc->pkc_errlog, "pkg_send: write");
			return(-1);
		}
		sprintf(errbuf,"pkg_send of %d, wrote %d\n", len, i);
		(pc->pkc_errlog)(errbuf);
		return(i);		/* amount of user data sent */
	}
#endif
	return(len);
}

/*
 *			P K G _ 2 S E N D
 *
 *  Exactly like pkg_send, except user's data is located in
 *  two disjoint buffers, rather than one.
 *  Fiendishly useful!
 */
int
pkg_2send( type, buf1, len1, buf2, len2, pc )
int type;
char *buf1, *buf2;
int len1, len2;
register struct pkg_conn *pc;
{
#ifdef HAS_WRITEV
	static struct iovec cmdvec[3];
#endif
	static struct pkg_header hdr;
	struct timeval tv;
	long bits;
	register int i;

	PKG_CK(pc);
	if( len1 < 0 )  len1=0;
	if( len2 < 0 )  len2=0;

	/* Finish any partially read message */
	do  {
		if( pc->pkc_left > 0 )
			if( pkg_block(pc) < 0 )
				return(-1);

		if( pkg_nochecking )
			continue;
		/* Check socket for unexpected input */
		tv.tv_sec = 0;
		tv.tv_usec = 0;		/* poll -- no waiting */
		bits = 1 << pc->pkc_fd;
		i = select( pc->pkc_fd+1, &bits, (char *)0, (char *)0, &tv );
		if( i > 0 && bits )
			if( pkg_block(pc) < 0 )
				return(-1);
	} while( pc->pkc_left > 0 );

	/* Flush any queued stream output first. */
	if( pc->pkc_strpos > 0 )  {
		if( pkg_flush( pc ) < 0 )
			return(-1);	/* assumes 2nd write would fail too */
	}

	pkg_pshort( hdr.pkh_magic, PKG_MAGIC );
	pkg_pshort( hdr.pkh_type, type );	/* should see if valid type */
	pkg_plong( hdr.pkh_len, len1+len2 );

#ifdef HAS_WRITEV
	cmdvec[0].iov_base = (caddr_t)&hdr;
	cmdvec[0].iov_len = sizeof(hdr);
	cmdvec[1].iov_base = (caddr_t)buf1;
	cmdvec[1].iov_len = len1;
	cmdvec[2].iov_base = (caddr_t)buf2;
	cmdvec[2].iov_len = len2;

	/*
	 * TODO:  set this FD to NONBIO.  If not all output got sent,
	 * loop in select() waiting for capacity to go out, and
	 * reading input as well.  Prevents deadlocking.
	 */
	if( (i = writev(pc->pkc_fd, cmdvec, 3)) != len1+len2+sizeof(hdr) )  {
		if( i < 0 )  {
			pkg_perror(pc->pkc_errlog, "pkg_2send: writev");
			return(-1);
		}
		sprintf(errbuf,"pkg_2send of %d+%d+%d, wrote %d\n",
			sizeof(hdr), len1, len2, i);
		(pc->pkc_errlog)(errbuf);
		return(i-sizeof(hdr));	/* amount of user data sent */
	}
#else
	/* Assume all succeed if last one does */
	(void)write( pc->pkc_fd, (char *)&hdr, sizeof(hdr) );
	i = write( pc->pkc_fd, buf1, len1 );
	if( len2 <= 0 )  return(i);
	if( (i = write( pc->pkc_fd, buf2, len2 )) != len2 )  {
		if( i < 0 )  {
			pkg_perror(pc->pkc_errlog, "pkg_2send: write2");
			return(-1);
		}
		sprintf(errbuf,"pkg_2send of %d, wrote %d+%d\n", len2, len1, i);
		(pc->pkc_errlog)(errbuf);
		return(len1+i);		/* amount of user data sent */
	}
#endif
	return(len1+len2);
}

/*
 *  			P K G _ S T R E A M
 *
 *  Exactly like pkg_send except no "push" is necessary here.
 *  If the packet is sufficiently small (MAXQLEN) it will be placed
 *  in the pkc_stream buffer (after flushing this buffer if there
 *  insufficient room).  If it is larger than this limit, it is sent
 *  via pkg_send (who will do a pkg_flush if there is already data in
 *  the stream queue).
 *
 *  Returns number of bytes of user data actually sent (or queued).
 */
int
pkg_stream( type, buf, len, pc )
int type;
char *buf;
int len;
register struct pkg_conn *pc;
{
	static struct pkg_header hdr;

	if( len > MAXQLEN )
		return( pkg_send(type, buf, len, pc) );

	if( len > PKG_STREAMLEN - sizeof(struct pkg_header) - pc->pkc_strpos )
		pkg_flush( pc );

	/* Queue it */
	pkg_pshort( hdr.pkh_magic, PKG_MAGIC );
	pkg_pshort( hdr.pkh_type, type );	/* should see if valid type */
	pkg_plong( hdr.pkh_len, len );

	bcopy( &hdr, &(pc->pkc_stream[pc->pkc_strpos]), sizeof(struct pkg_header) );
	pc->pkc_strpos += sizeof(struct pkg_header);
	bcopy( buf, &(pc->pkc_stream[pc->pkc_strpos]), len );
	pc->pkc_strpos += len;

	return( len + sizeof(struct pkg_header) );
}

/*
 *  			P K G _ F L U S H
 *
 *  Flush any pending data in the pkc_stream buffer.
 *
 *  Returns < 0 on failure, else number of bytes sent.
 */
int
pkg_flush( pc )
register struct pkg_conn *pc;
{
	register int	i;

	if( pc->pkc_strpos <= 0 ) {
		pc->pkc_strpos = 0;	/* sanity for < 0 */
		return( 0 );
	}

	if( (i = write(pc->pkc_fd,pc->pkc_stream,pc->pkc_strpos)) != pc->pkc_strpos )  {
		if( i < 0 ) {
			pkg_perror(pc->pkc_errlog, "pkg_flush: write");
			return(-1);
		}
		sprintf(errbuf,"pkg_flush of %d, wrote %d\n",
			pc->pkc_strpos, i);
		(pc->pkc_errlog)(errbuf);
		pc->pkc_strpos -= i;
		return( i );	/* amount of user data sent */
	}
	pc->pkc_strpos = 0;
	return( i );
}

/*
 *  			P K G _ W A I T F O R
 *
 *  This routine implements a blocking read on the network connection
 *  until a message of 'type' type is received.  This can be useful for
 *  implementing the synchronous portions of a query/reply exchange.
 *  All messages of any other type are processed by pkg_get() and
 *  handed off to the handler for that type message in pkg_switch[].
 *
 *  Returns the length of the message actually received, or -1 on error.
 */
int
pkg_waitfor( type, buf, len, pc )
int type;
char *buf;
int len;
register struct pkg_conn *pc;
{
	register int i;

	PKG_CK(pc);
again:
	if( pc->pkc_left >= 0 )
		if( pkg_block( pc ) < 0 )
			return(-1);

	if( pc->pkc_buf != (char *)0 )  {
		fprintf( stderr, "pkg_waitfor:  buffer clash\n");
		return(-1);
	}
	if( pkg_gethdr( pc, buf ) < 0 )  return(-1);
	if( pc->pkc_type != type )  {
		/* A message of some other type has unexpectedly arrived. */
		if( pc->pkc_len > 0 )  {
			if( (pc->pkc_buf = malloc(pc->pkc_len+2)) == NULL )  {
				pkg_perror(pc->pkc_errlog, "pkg_waitfor: malloc");
				return(-1);
			}
			pc->pkc_curpos = pc->pkc_buf;
		}
		goto again;
	}
	pc->pkc_left = -1;
	if( pc->pkc_len == 0 )
		return(0);
	if( pc->pkc_len > len )  {
		register char *bp;
		int excess;
		sprintf(errbuf,
			"pkg_waitfor:  message %d exceeds buffer %d\n",
			pc->pkc_len, len );
		(pc->pkc_errlog)(errbuf);
		if( (i = pkg_mread( pc, buf, len )) != len )  {
			sprintf(errbuf,
				"pkg_waitfor:  pkg_mread %d gave %d\n", len, i );
			(pc->pkc_errlog)(errbuf);
			return(-1);
		}
		excess = pc->pkc_len - len;	/* size of excess message */
		if( (bp = malloc(excess)) == NULL )  {
			pkg_perror(pc->pkc_errlog, "pkg_waitfor: excess malloc");
			return(-1);
		}
		if( (i = pkg_mread( pc, bp, excess )) != excess )  {
			sprintf(errbuf,
				"pkg_waitfor: pkg_mread of excess, %d gave %d\n",
				excess, i );
			(pc->pkc_errlog)(errbuf);
			(void)free(bp);
			return(-1);
		}
		(void)free(bp);
		return(len);		/* truncated, but OK */
	}

	/* Read the whole message into the users buffer */
	if( (i = pkg_mread( pc, buf, pc->pkc_len )) != pc->pkc_len )  {
		sprintf(errbuf,
			"pkg_waitfor:  pkg_mread %d gave %d\n", pc->pkc_len, i );
		(pc->pkc_errlog)(errbuf);
		return(-1);
	}
	pc->pkc_buf = (char *)0;
	pc->pkc_curpos = (char *)0;
	pc->pkc_left = -1;		/* safety */
	return( pc->pkc_len );
}

/*
 *  			P K G _ B W A I T F O R
 *
 *  This routine implements a blocking read on the network connection
 *  until a message of 'type' type is received.  This can be useful for
 *  implementing the synchronous portions of a query/reply exchange.
 *  All messages of any other type are processed by pkg_get() and
 *  handed off to the handler for that type message in pkg_switch[].
 *  The buffer to contain the actual message is acquired via malloc(),
 *  and the caller must free it.
 *
 *  Returns pointer to message buffer, or NULL.
 */
char *
pkg_bwaitfor( type, pc )
int type;
register struct pkg_conn *pc;
{
	register int i;
	register char *tmpbuf;

	PKG_CK(pc);
	do  {
		/* Finish any unsolicited msg */
		if( pc->pkc_left >= 0 )
			if( pkg_block(pc) < 0 )
				return((char *)0);
		if( pc->pkc_buf != (char *)0 )  {
			fprintf( stderr, "pkg_bwaitfor:  buffer clash\n");
			return((char *)0);
		}
		if( pkg_gethdr( pc, (char *)0 ) < 0 )
			return((char *)0);
	}  while( pc->pkc_type != type );

	pc->pkc_left = -1;
	if( pc->pkc_len == 0 )
		return((char *)0);

	/* Read the whole message into the dynamic buffer */
	if( (i = pkg_mread( pc, pc->pkc_buf, pc->pkc_len )) != pc->pkc_len )  {
		sprintf(errbuf,
			"pkg_bwaitfor:  pkg_mread %d gave %d\n", pc->pkc_len, i );
		(pc->pkc_errlog)(errbuf);
	}
	tmpbuf = pc->pkc_buf;
	pc->pkc_buf = (char *)0;
	pc->pkc_curpos = (char *)0;
	pc->pkc_left = -1;		/* safety */
	/* User must free the buffer */
	return( tmpbuf );
}

/*
 *  			P K G _ G E T
 *
 *  This routine should be called whenever select() indicates that there
 *  is data waiting on the network connection and the user's program is
 *  not blocking on the arrival of a particular message type.
 *  As portions of the message body arrive, they are read and stored.
 *  When the entire message body has been read, it is handed to the
 *  user-provided message handler for that type message, from pkg_switch[].
 *
 *  If this routine is called when there is no message already being
 *  assembled, and there is no input on the network connection, it will
 *  block, waiting for the arrival of the header.
 *
 *  Returns -1 on error, 0 if more data comming, 1 if user handler called.
 *  The user handler is responsible for calling free()
 *  on the message buffer when finished with it.
 */
int
pkg_get(pc)
register struct pkg_conn *pc;
{
	register int i;
	struct timeval tv;
	auto long bits;

	PKG_CK(pc);
	if( pc->pkc_left < 0 )  {
		if( pkg_gethdr( pc, (char *)0 ) < 0 )  return(-1);
		if( pc->pkc_left == 0 )  return( pkg_dispatch(pc) );

		/* See if any message body has arrived yet */
		tv.tv_sec = 0;
		tv.tv_usec = 20000;	/* 20 ms */
		bits = (1<<pc->pkc_fd);
		i = select( pc->pkc_fd+1, (char *)&bits, (char *)0, (char *)0, &tv );
		if( i <= 0 )  return(0);	/* timed out */
		if( !bits )  return(0);		/* no input */
	}

	/* read however much is here already, and remember our position */
	if( pc->pkc_left > 0 )  {
		if( (i = read( pc->pkc_fd, pc->pkc_curpos, pc->pkc_left )) <= 0 )  {
			pc->pkc_left = -1;
			pkg_perror(pc->pkc_errlog, "pkg_get: read");
			return(-1);
		}
		pc->pkc_curpos += i;
		pc->pkc_left -= i;
		if( pc->pkc_left > 0 )
			return(0);		/* more is on the way */
	}

	return( pkg_dispatch(pc) );
}

/*
 *			P K G _ D I S P A T C H
 *
 *  Internal.
 *  Given that a whole message has arrived, send it to the appropriate
 *  User Handler, or else grouse.
 *  Returns -1 on fatal error, 0 on no handler, 1 if all's well.
 */
static int
pkg_dispatch(pc)
register struct pkg_conn *pc;
{
	register int i;

	PKG_CK(pc);
	if( pc->pkc_left != 0 )  return(-1);

	/* Whole message received, process it via switchout table */
	for( i=0; pc->pkc_switch[i].pks_handler != NULL; i++ )  {
		register char *tempbuf;

		if( pc->pkc_switch[i].pks_type != pc->pkc_type )
			continue;
		/*
		 * NOTICE:  User Handler must free() message buffer!
		 * WARNING:  Handler may recurse back to pkg_get() --
		 * reset all connection state variables first!
		 */
		tempbuf = pc->pkc_buf;
		pc->pkc_buf = (char *)0;
		pc->pkc_curpos = (char *)0;
		pc->pkc_left = -1;		/* safety */
		/* pc->pkc_type, pc->pkc_len are preserved */
		pc->pkc_switch[i].pks_handler(pc, tempbuf);
		return(1);
	}
	sprintf(errbuf,"pkg_get:  no handler for message type %d, len %d\n",
		pc->pkc_type, pc->pkc_len );
	(pc->pkc_errlog)(errbuf);
	(void)free(pc->pkc_buf);
	pc->pkc_buf = (char *)0;
	pc->pkc_curpos = (char *)0;
	pc->pkc_left = -1;		/* safety */
	return(0);
}
/*
 *			P K G _ G E T H D R
 *
 *  Internal.
 *  Get header from a new message.
 *  Returns:
 *	1	when there is some message to go look at
 *	-1	on fatal errors
 */
static int
pkg_gethdr( pc, buf )
register struct pkg_conn *pc;
char *buf;
{
	register int i;

	PKG_CK(pc);
	if( pc->pkc_left >= 0 )  return(1);	/* go get it! */

	/*
	 *  At message boundary, read new header.
	 *  This will block until the new header arrives (feature).
	 */
	if( (i = pkg_mread( pc, &(pc->pkc_hdr),
	    sizeof(struct pkg_header) )) != sizeof(struct pkg_header) )  {
		if(i > 0) {
			sprintf(errbuf,"pkg_gethdr: header read of %d?\n", i);
			(pc->pkc_errlog)(errbuf);
		}
		return(-1);
	}
	while( pkg_gshort(pc->pkc_hdr.pkh_magic) != PKG_MAGIC )  {
		sprintf(errbuf,"pkg_gethdr: skipping noise x%x %c\n",
			*((unsigned char *)&pc->pkc_hdr),
			*((unsigned char *)&pc->pkc_hdr) );
		(pc->pkc_errlog)(errbuf);
		/* Slide over one byte and try again */
		bcopy( ((char *)&pc->pkc_hdr)+1, (char *)&pc->pkc_hdr, sizeof(struct pkg_header)-1);
		if( (i=read( pc->pkc_fd,
		    ((char *)&pc->pkc_hdr)+sizeof(struct pkg_header)-1,
		    1 )) != 1 )  {
			sprintf(errbuf,"pkg_gethdr: hdr read=%d?\n",i);
		    	(pc->pkc_errlog)(errbuf);
			return(-1);
		}
	}
	pc->pkc_type = pkg_gshort(pc->pkc_hdr.pkh_type);	/* host order */
	pc->pkc_len = pkg_glong(pc->pkc_hdr.pkh_len);
	if( pc->pkc_len < 0 )  pc->pkc_len = 0;
	pc->pkc_buf = (char *)0;
	pc->pkc_left = pc->pkc_len;
	if( pc->pkc_left == 0 )  return(1);		/* msg here, no data */

	if( buf )  {
		pc->pkc_buf = buf;
	} else {
		/* Prepare to read message into dynamic buffer */
		if( (pc->pkc_buf = malloc(pc->pkc_len+2)) == NULL )  {
			pkg_perror(pc->pkc_errlog, "pkg_gethdr: malloc");
			return(-1);
		}
	}
	pc->pkc_curpos = pc->pkc_buf;
	return(1);			/* something ready */
}

/*
 *  			P K G _ B L O C K
 *  
 *  This routine blocks, waiting for one complete message to arrive from
 *  the network.  The actual handling of the message is done with
 *  pkg_get(), which invokes the user-supplied message handler.
 *
 *  This routine can be used in a loop to pass the time while waiting
 *  for a flag to be changed by the arrival of an asynchronous message,
 *  or for the arrival of a message of uncertain type.
 *  
 *  Control returns to the caller after one full message is processed.
 *  Returns -1 on error, etc.
 */
int
pkg_block(pc)
register struct pkg_conn *pc;
{
	PKG_CK(pc);
	if( pc->pkc_left == 0 )  return( pkg_dispatch(pc) );

	/* If no read outstanding, start one. */
	if( pc->pkc_left < 0 )  {
		if( pkg_gethdr( pc, (char *)0 ) < 0 )  return(-1);
		if( pc->pkc_left <= 0 )  return( pkg_dispatch(pc) );
	}

	/* Read the rest of the message, blocking in read() */
	if( pc->pkc_left > 0 && pkg_mread( pc, pc->pkc_curpos, pc->pkc_left ) != pc->pkc_left )  {
		pc->pkc_left = -1;
		return(-1);
	}
	pc->pkc_left = 0;
	return( pkg_dispatch(pc) );
}

extern int sys_nerr;
extern char *sys_errlist[];

/*
 *			P K G _ P E R R O R
 *
 *  Produce a perror on the error logging output.
 */
static void
pkg_perror( errlog, s )
void (*errlog)();
char *s;
{
	if( errno >= 0 && errno < sys_nerr ) {
		sprintf( errbuf, "%s: %s\n", s, sys_errlist[errno] );
		errlog( errbuf );
	} else
		errlog( s );
}

/*
 *			P K G _ E R R L O G
 *
 *  Default error logger.  Writes to stderr.
 */
static void
pkg_errlog( s )
char *s;
{
	fputs( s, stderr );
}

#ifdef SGI_EXCELAN
/*
 *			B S D S E L E C T
 *
 *  Not only is the calling sequence different, but
 *  it conflicts with naming in the GL2 library, so
 *  select can't be used!
 */
#undef select
int
bsdselect( nfd, a, b, c, tvp )
int nfd;
long *a, *b, *c;
struct timeval *tvp;
{
/**	return( select( nfd, a, b, tvp->tv_usec ); **/
	return(0);
}
#endif

#ifdef SGI_EXCELAN
/*
 * Internet address interpretation routine.
 * All the network library routines call this
 * routine to interpret entries in the data bases
 * which are expected to be an address.
 * The value returned is in network order.
 */
u_long
inet_addr(cp)
	register char *cp;
{
	register u_long val, base, n;
	register char c;
	u_long parts[4], *pp = parts;

again:
	/*
	 * Collect number up to ``.''.
	 * Values are specified as for C:
	 * 0x=hex, 0=octal, other=decimal.
	 */
	val = 0; base = 10;
	if (*cp == '0')
		base = 8, cp++;
	if (*cp == 'x' || *cp == 'X')
		base = 16, cp++;
	while (c = *cp) {
		if (isdigit(c)) {
			val = (val * base) + (c - '0');
			cp++;
			continue;
		}
		if (base == 16 && isxdigit(c)) {
			val = (val << 4) + (c + 10 - (islower(c) ? 'a' : 'A'));
			cp++;
			continue;
		}
		break;
	}
	if (*cp == '.') {
		/*
		 * Internet format:
		 *	a.b.c.d
		 *	a.b.c	(with c treated as 16-bits)
		 *	a.b	(with b treated as 24 bits)
		 */
		if (pp >= parts + 4)
			return (-1);
		*pp++ = val, cp++;
		goto again;
	}
	/*
	 * Check for trailing characters.
	 */
	if (*cp && !isspace(*cp))
		return (-1);
	*pp++ = val;
	/*
	 * Concoct the address according to
	 * the number of parts specified.
	 */
	n = pp - parts;
	switch (n) {

	case 1:				/* a -- 32 bits */
		val = parts[0];
		break;

	case 2:				/* a.b -- 8.24 bits */
		val = (parts[0] << 24) | (parts[1] & 0xffffff);
		break;

	case 3:				/* a.b.c -- 8.8.16 bits */
		val = (parts[0] << 24) | ((parts[1] & 0xff) << 16) |
			(parts[2] & 0xffff);
		break;

	case 4:				/* a.b.c.d -- 8.8.8.8 bits */
		val = (parts[0] << 24) | ((parts[1] & 0xff) << 16) |
		      ((parts[2] & 0xff) << 8) | (parts[3] & 0xff);
		break;

	default:
		return (-1);
	}
	val = htonl(val);
	return (val);
}
#endif
