/*
 *			P K G . C
 *
 *  Routines to manage multiplexing and de-multiplexing synchronous
 *  and asynchronous messages across stream connections.
 *
 *  Functions -
 *	pkg_open	Open a network connection to a host
 *	pkg_initserver	Create a network server, and listen for connection
 *	pkg_getclient	As network server, accept a new connection
 *	pkg_makeconn	Make a pkg_conn structure
 *	pkg_close	Close a network connection
 *	pkg_send	Send a message on the connection
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
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
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

#ifdef BSD
#include <sys/uio.h>		/* for struct iovec (writev) */
#endif

#ifdef SGI_EXCELAN
#include <EXOS/exos/misc.h>
#include <EXOS/sys/socket.h>
#include <EXOS/netinet/in.h>
#include <sys/time.h>		/* for struct timeval */
#endif

#include <errno.h>

#include "pkg.h"

extern char *malloc();
extern void perror();
extern int errno;

/* Internal Functions */
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
 *			P K G _ O P E N
 *
 *  We are a client.  Make a connection to the server.
 *
 *  Returns PKC_ERROR on error.
 */
struct pkg_conn *
pkg_open( host, service, switchp, errlog )
char *host;
char *service;
struct pkg_switch *switchp;
void (*errlog)();
{
	struct sockaddr_in sinme;		/* Client */
	struct sockaddr_in sinhim;		/* Server */
	register struct hostent *hp;
	register int netfd;

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
		sinhim.sin_addr.s_addr = inet_addr(host);
	} else {
#ifdef BSD
		if( (hp = gethostbyname(host)) == NULL )  {
			sprintf(errbuf,"pkg_open(%s,%s): unknown host\n",
				host, service );
			errlog(errbuf);
			return(PKC_ERROR);
		}
		sinhim.sin_family = hp->h_addrtype;
		bcopy(hp->h_addr, (char *)&sinhim.sin_addr, hp->h_length);
#endif
#ifdef SGI_EXCELAN
		char **hostp = &host;
		if((sinhim.sin_addr.s_addr = rhost(&hostp)) < 0) {
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
 *  			P K G _ I N I T S E R V E R
 *  
 *  We are now going to be a server for the indicated service.
 *  Hang a LISTEN, and return the fd to select() on waiting for
 *  new connections.
 *
 *  Returns fd to listen on (>=0), -1 on error.
 */
int
pkg_initserver( service, backlog, errlog )
char *service;
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
		sprintf(errbuf,"pkg_initserver(%s,%d): unknown service\n",
			service, backlog );
		errlog(errbuf);
		return(-1);
	}
	sinme.sin_port = sp->s_port;
#endif
#ifdef SGI_EXCELAN
	/* What routine does SGI give for this one? */
	sinme.sin_port = htons(5558);	/* mfb service!! XXX */
#endif

#ifdef BSD
	if( (pkg_listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )  {
		pkg_perror( errlog, "pkg_initserver:  socket" );
		return(-1);
	}

	if( bind(pkg_listenfd, &sinme, sizeof(sinme)) < 0 )  {
		pkg_perror( errlog, "pkg_initserver: bind" );
		return(-1);
	}

	if( backlog > 5 )  backlog = 5;
	if( listen(pkg_listenfd, backlog) < 0 )  {
		pkg_perror( errlog, "pkg_initserver:  listen" );
		return(-1);
	}
#endif
#ifdef SGI_EXCELAN
	if( (pkg_listenfd = socket(SOCK_STREAM,0,&sinme,SO_ACCEPTCONN|SO_KEEPALIVE)) < 0 )  {
		pkg_perror( errlog, "pkg_initserver:  socket" );
		return(-1);
	}

	/* wait until someone tries to connect */
	/* XXX not quite right semantics! */
	if(accept(pkg_listenfd,&sinhim) < 0){
		pkg_perror( errlog, "pkg_initserver:  accept" );
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
	/* Hopefully, once-only XXX */
	return( pkg_makeconn( fd, switchp, errlog) );
#endif
}

/*
 *			P K G _ M A K E C O N N
 *
 *  Malloc and initialize a pkg_conn structure.
 *  Assumes a client has already been accepted on the given file
 *  descriptor.  This is the case with processes spawned by inetd,
 *  or those comming from pkg_getclient.
 *
 *  Returns -
 *	>0		ptr to pkg_conn block of new connection
 *	PKC_ERROR	fatal error
 */
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
		errlog( "pkg_makeconn: malloc failure\n" );
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
	if( pc->pkc_buf )
		(void)free(pc->pkc_buf);
	pc->pkc_buf = (char *)0;	/* safety */
	(void)free(pc->pkc_buf);
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
#ifdef BSD
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

		/* Check socket for unexpected input */
		tv.tv_sec = 0;
		tv.tv_usec = 0;		/* poll -- no waiting */
		bits = 1 << pc->pkc_fd;
#ifdef BSD
		i = select( pc->pkc_fd+1, &bits, (char *)0, (char *)0, &tv );
#endif
#ifdef SGI_EXCELAN
		i = select( pc->pkc_fd+1, &bits, (char *)0, tv.tv_usec );
#endif
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

	hdr.pkg_magic = htons(PKG_MAGIC);
	hdr.pkg_type = htons(type);	/* should see if it's a valid type */
	hdr.pkg_len = htonl(len);

#ifdef BSD
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
	hdr.pkg_magic = htons(PKG_MAGIC);
	hdr.pkg_type = htons(type);
	hdr.pkg_len = htonl(len);

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

	if( pkg_gethdr( pc, buf ) < 0 )  return(-1);
	if( pc->pkc_hdr.pkg_type != type )  {
		/* A message of some other type has unexpectedly arrived. */
		if( pc->pkc_hdr.pkg_len > 0 )  {
			pc->pkc_buf = malloc(pc->pkc_hdr.pkg_len+2);
			pc->pkc_curpos = pc->pkc_buf;
		}
		goto again;
	}
	pc->pkc_left = -1;
	if( pc->pkc_hdr.pkg_len == 0 )
		return(0);
	if( pc->pkc_hdr.pkg_len > len )  {
		register char *bp;
		int excess;
		sprintf(errbuf,
			"pkg_waitfor:  message %d exceeds buffer %d\n",
			pc->pkc_hdr.pkg_len, len );
		(pc->pkc_errlog)(errbuf);
		if( (i = pkg_mread( pc, buf, len )) != len )  {
			sprintf(errbuf,
				"pkg_waitfor:  pkg_mread %d gave %d\n", len, i );
			(pc->pkc_errlog)(errbuf);
			return(-1);
		}
		excess = pc->pkc_hdr.pkg_len - len;	/* size of excess message */
		bp = malloc(excess);
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
	if( (i = pkg_mread( pc, buf, pc->pkc_hdr.pkg_len )) != pc->pkc_hdr.pkg_len )  {
		sprintf(errbuf,
			"pkg_waitfor:  pkg_mread %d gave %d\n", pc->pkc_hdr.pkg_len, i );
		(pc->pkc_errlog)(errbuf);
		return(-1);
	}
	return( pc->pkc_hdr.pkg_len );
}

/*
 *  			P K G _ B W A I T F O R
 *
 *  This routine implements a blocking read on the network connection
 *  until a message of 'type' type is received.  This can be useful for
 *  implementing the synchronous portions of a query/reply exchange.
 *  All messages of any other type are processed by pkg_get() and
 *  handed off to the handler for that type message in pkg_switch[].
 *  The buffer to contain the actual message is acquired via malloc().
 *
 *  Returns pointer to message buffer, or NULL.
 */
char *
pkg_bwaitfor( type, pc )
int type;
register struct pkg_conn *pc;
{
	register int i;

	PKG_CK(pc);
	do  {
		/* Finish any unsolicited msg */
		if( pc->pkc_left >= 0 )
			if( pkg_block(pc) < 0 )
				return((char *)0);
		if( pkg_gethdr( pc, (char *)0 ) < 0 )
			return((char *)0);
	}  while( pc->pkc_hdr.pkg_type != type );

	pc->pkc_left = -1;
	if( pc->pkc_hdr.pkg_len == 0 )
		return((char *)0);

	/* Read the whole message into the dynamic buffer */
	if( (i = pkg_mread( pc, pc->pkc_buf, pc->pkc_hdr.pkg_len )) != pc->pkc_hdr.pkg_len )  {
		sprintf(errbuf,
			"pkg_bwaitfor:  pkg_mread %d gave %d\n", pc->pkc_hdr.pkg_len, i );
		(pc->pkc_errlog)(errbuf);
	}
	return( pc->pkc_buf );
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
#ifdef BSD
		i = select( pc->pkc_fd+1, (char *)&bits, (char *)0, (char *)0, &tv );
#endif
#ifdef SGI_EXCELAN
		i = select( pc->pkc_fd+1, (char *)&bits, (char *)0, &tv );
#endif
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

		if( pc->pkc_switch[i].pks_type != pc->pkc_hdr.pkg_type )
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
		/* pc->pkc_hdr.pkg_type, pc->pkc_hdr.pkg_len are preserved */
		pc->pkc_switch[i].pks_handler(pc, tempbuf);
		return(1);
	}
	sprintf(errbuf,"pkg_get:  no handler for message type %d, len %d\n",
		pc->pkc_hdr.pkg_type, pc->pkc_hdr.pkg_len );
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
	while( pc->pkc_hdr.pkg_magic != htons(PKG_MAGIC ) )  {
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
	pc->pkc_hdr.pkg_type = ntohs(pc->pkc_hdr.pkg_type);	/* host order */
	pc->pkc_hdr.pkg_len = ntohl(pc->pkc_hdr.pkg_len);
	if( pc->pkc_hdr.pkg_len < 0 )  pc->pkc_hdr.pkg_len = 0;
	pc->pkc_buf = (char *)0;
	pc->pkc_left = pc->pkc_hdr.pkg_len;
	if( pc->pkc_left == 0 )  return(1);		/* msg here, no data */

	if( buf )  {
		pc->pkc_buf = buf;
	} else {
		/* Prepare to read message into dynamic buffer */
		pc->pkc_buf = malloc(pc->pkc_hdr.pkg_len+2);
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

#ifdef BSD
extern int sys_nerr;
extern char *sys_errlist[];
#endif

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
#ifdef BSD
	if( errno >= 0 && errno < sys_nerr ) {
		sprintf( errbuf, "%s: %s\n", s, sys_errlist[errno] );
		errlog( errbuf );
	} else
		errlog( s );
#else
	errlog( s );
#endif BSD
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
