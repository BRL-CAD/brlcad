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
 *	pkg_close	Close a network connection
 *	pkg_send	Send a message on the connection
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
#include <sys/socket.h>
#include <sys/uio.h>		/* for struct iovec */
#include <sys/ioctl.h>		/* for FIONBIO */
#include <netinet/in.h>		/* for htons(), etc */
#include <netdb.h>
#include <sys/time.h>		/* for struct timeval */
#include <errno.h>

#include "./pkg.h"

extern char *malloc();
extern int errno;

/*
 *  Format of the message header as it is transmitted over the network
 *  connection.  Internet network order is used.
 */
#define PKG_MAGIC	0x41FE
struct pkg_header {
	unsigned short	pkg_magic;		/* Ident */
	unsigned short	pkg_type;		/* Message Type */
	long		pkg_len;		/* Byte count of remainder */
};

/*
 *			P K G _ O P E N
 *
 *  We are a client.  Make a connection to the server.
 *
 *  Returns fd (>=0) if connected, -1 on error.
 */
int
pkg_open( host, service )
char *host;
char *service;
{
	struct sockaddr_in sinme;		/* Client */
	struct sockaddr_in sinhim;		/* Server */
	register struct hostent *hp;
	int netfd;
	int port;

	bzero((char *)&sinhim, sizeof(sinhim));
	bzero((char *)&sinme, sizeof(sinme));

	/* Determine port for service */
	if( (sinhim.sin_port = atoi(service)) > 0 )  {
		sinhim.sin_port = htons(sinhim.sin_port);
	} else {
		register struct servent *sp;
		if( (sp = getservbyname( service, "tcp" )) == NULL )  {
			fb_log( "pkg_open(%s,%s): unknown service\n",
				host, service );
			return(-1);
		}
		sinhim.sin_port = sp->s_port;
	}

	/* Get InterNet address */
	if( atoi( host ) > 0 )  {
		/* Numeric */
		sinhim.sin_family = AF_INET;
		sinhim.sin_addr.s_addr = inet_addr(host);
	} else {
		if( (hp = gethostbyname(host)) == NULL )  {
			fb_log( "pkg_open(%s,%s): unknown host\n",
				host, service );
			return(-1);
		}
		sinhim.sin_family = hp->h_addrtype;
		bcopy(hp->h_addr, (char *)&sinhim.sin_addr, hp->h_length);
	}

	if( (netfd = socket(sinhim.sin_family, SOCK_STREAM, 0)) < 0 )  {
		fb_log( "pkg_open : client socket failure.\n" );
		return(-1);
	}
	sinme.sin_port = 0;		/* let kernel pick it */

	if( bind(netfd, &sinme, sizeof(sinme)) < 0 )  {
		fb_log( "pkg_open : bind failure.\n" );
		return(-1);
	}

	if( connect(netfd, &sinhim, sizeof(sinhim), 0) < 0 )  {
		fb_log( "pkg_open : client connect failure.\n" );
		return(-1);
	}
	return(netfd);
}

static int pkg_listenfd;

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
pkg_initserver( service, backlog )
char *service;
int backlog;
{
	struct sockaddr_in sinme;
	register struct servent *sp;
	int pkg_listenfd;

	bzero((char *)&sinme, sizeof(sinme));

	/* Determine port for service */
	if( (sp = getservbyname( service, "tcp" )) == NULL )  {
		fb_log( "pkg_initserver(%s,%d): unknown service\n",
			service, backlog );
		return(-1);
	}

	sinme.sin_port = sp->s_port;
	if( (pkg_listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 )  {
		fb_log( "pkg_initserver : socket failure.\n" );
		return(-1);
	}

	if( bind(pkg_listenfd, &sinme, sizeof(sinme)) < 0 )  {
		fb_log( "pkg_initserver : bind failure.\n" );
		return(-1);
	}

	if( backlog > 5 )  backlog = 5;
	if( listen(pkg_listenfd, backlog) < 0 )  {
		fb_log( "pkg_initserver : listen failure.\n" );
		return(-1);
	}
	return(pkg_listenfd);
}

/*
 *			P K G _ G E T C L I E N T
 *
 *  Given an fd with a listen outstanding, accept the connection
 *  and return the connection's fd.
 *  When poll == 0, accept is allowed to block.
 *  When poll != 0, accept will not block.
 *
 *  Returns -
 *	>=0	fd of newly accepted connection
 *	-1	accept would block, try again later
 *	-2	fatal error
 */
int
pkg_getclient(fd, nodelay)
{
	struct sockaddr_in from;
	register int s2;
	auto int fromlen = sizeof (from);
	auto int onoff;

	if(nodelay)  {
		onoff = 1;
		if( ioctl(fd, FIONBIO, &onoff) < 0 )
			fb_log( "pkg_getclient : FIONBIO 1 failed.\n" );
	}
	do  {
		s2 = accept(fd, (char *)&from, &fromlen);
		if (s2 < 0) {
			if(errno == EINTR)
				continue;
			if(errno == EWOULDBLOCK)
				return(-1);
			fb_log( "pkg_getclient : accept failed.\n" );
			return(-2);
		}
	}  while( s2 < 0);
	if(nodelay)  {		
		onoff = 0;
		if( ioctl(fd, FIONBIO, &onoff) < 0 )
			fb_log( "pkg_getclient : FIONBIO 2 failed.\n" );
		if( ioctl(s2, FIONBIO, &onoff) < 0 )
			fb_log( "pkg_getclient : FIONBIO 3 failed.\n" );
	}
	return(s2);
}

/*
 *  			P K G _ C L O S E
 *  
 *  Gracefully release the connection.
 */
void
pkg_close(fd)
{
	(void)close(fd);
}

/*
 *			P K G _ M R E A D
 *
 * This function performs the function of a read(II) but will
 * call read(II) multiple times in order to get the requested
 * number of characters.  This can be necessary because pipes
 * and network connections don't deliver data with the same
 * grouping as it is written with.  Written by Robert S. Miles, BRL.
 */
int
pkg_mread(fd, bufp, n)
int fd;
register char	*bufp;
unsigned	n;
{
	register unsigned	count = 0;
	register int		nread;

	do {
		nread = read(fd, bufp, n-count);
		if(nread < 0)  {
			fb_log( "pkg_mread : read failed.\n" );
			return(-1);
		}
		if(nread == 0)
			return((int)count);
		count += (unsigned)nread;
		bufp += nread;
	 } while(count < n);

	return((int)count);
}

 int pkg_left = -1;	/* number of bytes pkg_get still expects */
/* negative -> read new header, 0 -> it's all here, >0 -> more to come */
static char *pkg_buf;		/* start of dynamic buffer for pkg_get */
static char *pkg_curpos;	/* pkg_get current position in pkg_buf */
static struct pkg_header hdr;	/* hdr of cur msg, pkg_waitfor/pkg_get */

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
pkg_send( type, buf, len, fd )
int type;
char *buf;
int len;
int fd;
{
	static struct iovec cmdvec[2];
	static struct pkg_header hdr;
	struct timeval tv;
	long bits;
	register int i;

	if( len < 0 )  len=0;

	do  {
		/* Finish any partially read message */
		if( pkg_left > 0 )
			if( pkg_block(fd) < 0 )
				return(-1);
#ifdef never
		/* Check socket for more input */
		tv.tv_sec = 0;
		tv.tv_usec = 0;		/* poll */
		bits = 1<<fd;
		i = select( fd+1, &bits, (char *)0, (char *)0, &tv );
		if( i > 0 && bits )
			if( pkg_block(fd) < 0 )
				return(-1);
#endif
	} while( pkg_left > 0 );

	hdr.pkg_magic = htons(PKG_MAGIC);
	hdr.pkg_type = htons(type);	/* should see if it's a valid type */
	hdr.pkg_len = htonl(len);

	cmdvec[0].iov_base = (caddr_t)&hdr;
	cmdvec[0].iov_len = sizeof(hdr);
	cmdvec[1].iov_base = (caddr_t)buf;
	cmdvec[1].iov_len = len;

	/*
	 * TODO:  set this FD to NONBIO.  If not all output got sent,
	 * loop in select() waiting for capacity to go out, and
	 * reading input as well.  Prevents deadlocking.
	 */
	if( (i = writev( fd, cmdvec, (len>0)?2:1 )) != len+sizeof(hdr) )  {
		if( i < 0 )  {
			fb_log( "pkg_send : write failed.\n" );
			return(-1);
		}
		fb_log( "pkg_send of %d+%d, wrote %d\n", sizeof(hdr), len, i );
		return(i-sizeof(hdr));	/* amount of user data sent */
	}
	return(len);
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
 *  NOTE that this routine and pkg_get() share lots of state via
 *  4 external (static) variables.
 *
 *  Returns the length of the message actually received, or -1 on error.
 */
int
pkg_waitfor( type, buf, len, fd )
int type;
char *buf;
int len;
int fd;
{
	register int i;
/*	fb_log( "pkg_waitfor(%d,%d,%d,%d)\n", type, buf, len, fd );*/
again:
	if( pkg_left >= 0 )
		if( pkg_block( fd ) < 0 )
			return(-1);

	if( pkg_gethdr( fd, buf ) < 0 )  return(-1);
	if( hdr.pkg_type != type )  {
		/* A message of some other type has unexpectedly arrived. */
		if( hdr.pkg_len > 0 )  {
			pkg_buf = malloc(hdr.pkg_len+2);
			pkg_curpos = pkg_buf;
		}
		goto again;
	}
	pkg_left = -1;
	if( hdr.pkg_len == 0 )
		return(0);
	if( hdr.pkg_len > len )  {
		register char *bp;
		int excess;
		fb_log( "pkg_waitfor:  message %d exceeds buffer %d\n",
			hdr.pkg_len, len );
		if( (i = pkg_mread( fd, buf, len )) != len )  {
			fb_log( "pkg_waitfor:  pkg_mread %d gave %d\n", len, i );
			return(-1);
		}
		excess = hdr.pkg_len - len;	/* size of excess message */
		bp = malloc(excess);
		if( (i = pkg_mread( fd, bp, excess )) != excess )  {
			fb_log( "pkg_waitfor: pkg_mread of excess, %d gave %d\n",
				excess, i );
			(void)free(bp);
			return(-1);
		}
		(void)free(bp);
		return(len);		/* truncated, but OK */
	}

	/* Read the whole message into the users buffer */
	if( (i = pkg_mread( fd, buf, hdr.pkg_len )) != hdr.pkg_len )  {
		fb_log( "pkg_waitfor:  pkg_mread %d gave %d\n", hdr.pkg_len, i );
		return(-1);
	}
	return( hdr.pkg_len );
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
 *  NOTE that this routine and pkg_get() share lots of state via
 *  4 external (static) variables.
 *
 *  Returns pointer to message buffer, or NULL.
 */
char *
pkg_bwaitfor( type, fd )
int type;
int fd;
{
	register int i;

	do  {
		/* Finish any unsolicited msg */
		if( pkg_left >= 0 )
			if( pkg_block(fd) < 0 )
				return((char *)0);
		if( pkg_gethdr( fd, (char *)0 ) < 0 )
			return((char *)0);
	}  while( hdr.pkg_type != type );

	pkg_left = -1;
	if( hdr.pkg_len == 0 )
		return((char *)0);

	/* Read the whole message into the dynamic buffer */
	if( (i = pkg_mread( fd, pkg_buf, hdr.pkg_len )) != hdr.pkg_len )  {
		fb_log( "pkg_bwaitfor:  pkg_mread %d gave %d\n", hdr.pkg_len, i );
	}
	return( pkg_buf );
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
 *  NOTE that this routine and pkg_waitfor() share lots of state via
 *  4 external (static) variables.
 *
 *  Returns -1 on error, 0 if more data comming, 1 if user handler called.
 *  The user handler is responsible for calling free()
 *  on the message buffer when finished with it.
 */
int
pkg_get(fd)
{
	register int i;
	struct timeval tv;
	auto long bits;

	if( pkg_left < 0 )  {
		if( pkg_gethdr( fd, (char *)0 ) < 0 )  return(-1);

		/* See if any message body has arrived yet */
		tv.tv_sec = 0;
		tv.tv_usec = 20000;	/* 20 ms */
		bits = (1<<fd);
		i = select( fd+1, &bits, (char *)0, (char *)0, &tv );
		if( i <= 0 )  return(0);	/* timed out */
		if( !bits )  return(0);		/* no input */
	}

	/* read however much is here already, and remember our position */
	if( pkg_left > 0 )  {
		if( (i = read( fd, pkg_curpos, pkg_left )) < 0 )  {
			pkg_left = -1;
			fb_log( "pkg_get : read failed.\n" );
			return(-1);
		}
		pkg_curpos += i;
		pkg_left -= i;
		if( pkg_left > 0 )
			return(0);		/* more is on the way */
	}

	return( pkg_dispatch() );
}

/*
 *			P K G _ D I S P A T C H
 *
 *  Given that a whole message has arrived, send it to the appropriate
 *  User Handler, or else grouse.
 *  Returns -1 on fatal error, 0 on no handler, 1 if all's well.
 */
int
pkg_dispatch()
{
	register int i;

	if( pkg_left != 0 )  return(-1);

	/* Whole message received, process it via switchout table */
	for( i=0; i < pkg_swlen; i++ )  {
		register char *tempbuf;

		if( pkg_switch[i].pks_type != hdr.pkg_type )
			continue;
		/*
		 * NOTICE:  User Handler must free() message buffer!
		 * WARNING:  Handler may recurse back to pkg_get() --
		 * reset all global state variables first!
		 */
		tempbuf = pkg_buf;
		pkg_buf = (char *)0;
		pkg_curpos = (char *)0;
		pkg_left = -1;		/* safety */
		pkg_switch[i].pks_handler(hdr.pkg_type, tempbuf, hdr.pkg_len);
		return(1);
	}
	fb_log( "pkg_get:  no handler for message type %d, len %d\n",
		hdr.pkg_type, hdr.pkg_len );
	(void)free(pkg_buf);
	pkg_buf = (char *)0;
	pkg_curpos = (char *)0;
	pkg_left = -1;		/* safety */
	return(0);
}
/*
 *			P K G _ G E T H D R
 *
 *  Get header from a new message.
 *  Returns:
 *	1	when there is some message to go look at
 *	-1	on fatal errors
 */
int
pkg_gethdr( fd, buf )
char *buf;
{
	register int i;

	if( pkg_left >= 0 )  return(1);	/* go get it! */

	/*
	 *  At message boundary, read new header.
	 *  This will block until the new header arrives (feature).
	 */
	if( (i = pkg_mread( fd, &hdr, sizeof(hdr) )) != sizeof(hdr) )  {
		if(i > 0)
			fb_log( "pkg_gethdr: header read of %d?\n", i);
		return(-1);
	}
	while( hdr.pkg_magic != htons(PKG_MAGIC ) )  {
		fb_log("pkg_gethdr: skipping noise x%x %c\n",
			*((unsigned char *)&hdr),
			*((unsigned char *)&hdr) );
		/* Slide over one byte and try again */
		bcopy( ((char *)&hdr)+1, (char *)&hdr, sizeof(hdr)-1);
		if( (i=read( fd, ((char *)&hdr)+sizeof(hdr)-1, 1 )) != 1 )  {
			fb_log("pkg_gethdr: hdr read=%d?\n",i);
			return(-1);
		}
	}
	hdr.pkg_type = ntohs(hdr.pkg_type);	/* host order */
	hdr.pkg_len = ntohl(hdr.pkg_len);
	if( hdr.pkg_len < 0 )  hdr.pkg_len = 0;
	pkg_buf = (char *)0;
	pkg_left = hdr.pkg_len;
	if( pkg_left == 0 )  return(1);		/* msg here, no data */

	if( buf )  {
		pkg_buf = buf;
	} else {
		/* Prepare to read message into dynamic buffer */
		pkg_buf = malloc(hdr.pkg_len+2);
	}
	pkg_curpos = pkg_buf;
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
pkg_block(fd)
{
	register int i;

	if( pkg_left == 0 )  return( pkg_dispatch() );

	/* If no read outstanding, start one. */
	if( pkg_left < 0 )  {
		if( pkg_gethdr( fd, (char *)0 ) < 0 )  return(-1);
		if( pkg_left <= 0 )  return( pkg_dispatch() );
	}

	/* Read the rest of the message, blocking in read() */
	if( pkg_left > 0 && pkg_mread( fd, pkg_curpos, pkg_left ) != pkg_left )  {
		pkg_left = -1;
		return(-1);
	}
	pkg_left = 0;
	return( pkg_dispatch() );
}


/*
 *  			R A W _ P K G _ S E N D
 *
 *  Send an iovec list of "raw packets", i.e. user is responsible
 *   for putting on magic headers, network byte order, etc.
 *
 *  Note that the whole message should be transmitted by
 *  TCP with only one TCP_PUSH at the end, due to the use of writev().
 *
 *  Returns number of bytes of user data actually sent.
 */
int
raw_pkg_send( buf, len, fd )
struct iovec buf[];
int len;
int fd;
{
	struct timeval tv;
	long bits;
	register int i;
	int start;

	if( len < 0 )  len=0;

	do  {
		/* Finish any partially read message */
		if( pkg_left > 0 )
			if( pkg_block(fd) < 0 )
				return(-1);
#ifdef never
		/* Check socket for more input */
		tv.tv_sec = 0;
		tv.tv_usec = 0;		/* poll */
		bits = 1<<fd;
		i = select( fd+1, &bits, (char *)0, (char *)0, &tv );
		if( i > 0 && bits )
			if( pkg_block(fd) < 0 )
				return(-1);
#endif
	} while( pkg_left > 0 );

#ifdef NEVER
	hdr.pkg_magic = htons(PKG_MAGIC);
	hdr.pkg_type = htons(type);	/* should see if it's a valid type */
	hdr.pkg_len = htonl(len);

	cmdvec[0].iov_base = (caddr_t)&hdr;
	cmdvec[0].iov_len = sizeof(hdr);
	cmdvec[1].iov_base = (caddr_t)buf;
	cmdvec[1].iov_len = len;
#endif NEVER

	/*
	 * TODO:  set this FD to NONBIO.  If not all output got sent,
	 * loop in select() waiting for capacity to go out, and
	 * reading input as well.  Prevents deadlocking.
	 */
	start = 0;
	while( len > 0 ) {
		if( (i = writev( fd, &buf[start], (len>8?8:len) )) < 0 )  {
			fb_log( "pkg_send : write failed.\n" );
			return(-1);
		}
		len -= 8;
		start += 8;
	}
	return(len);
}
