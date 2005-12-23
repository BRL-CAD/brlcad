/*                           P K G . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
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
/** @file pkg.c
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
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>		/* used by inet_addr() routine, below */

#ifndef _WIN32
#  undef BSD	/* /usr/include/sys/param.h redefines this */
#  include <sys/param.h>
#  include <sys/time.h>
#endif

#if !defined(vax)
#  include <time.h>
#endif

#ifndef _WIN32
#  include <sys/socket.h>
#  include <sys/ioctl.h>		/* for FIONBIO */
#  include <netinet/in.h>		/* for htons(), etc */
#  include <netdb.h>
#  include <netinet/tcp.h>	/* for TCP_NODELAY sockopt */
#  include <arpa/inet.h>		/* for inet_addr() */
#  undef LITTLE_ENDIAN		/* defined in netinet/{ip.h,tcp.h} */
#else
#  include <process.h>
#  include <winsock.h>
#  include <fcntl.h>
#endif

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"

/* Not all systems with "BSD Networking" include UNIX Domain sockets */
#ifdef HAVE_UNIX_DOMAIN_SOCKETS
#  include <sys/un.h>		/* UNIX Domain sockets */
#endif

#ifdef n16
/* Encore multimax */
#  include <sys/h/socket.h>
#  include <sys/ioctl.h>
#  include <sys/netinet/in.h>
#  include <sys/aux/netdb.h>
#  include <sys/netinet/tcp.h>
#endif

#ifdef HAVE_WRITEV
#  include <sys/uio.h>		/* for struct iovec (writev) */
#endif

#include <errno.h>
#include "pkg.h"

#if defined(__stardent)
/* <sys/byteorder.h> seems to be wrong, and this is a LITTLE_ENDIAN machine */
#  undef	htons
#  define	htons(x)	((((x)&0xFF)<<8)|(((x)>>8)&0xFF))
#  undef	htonl
#  define	htonl(x)	( \
	((((x)    )&0xFF)<<24) | \
	((((x)>> 8)&0xFF)<<16) | \
	((((x)>>16)&0xFF)<< 8) | \
	((((x)>>24)&0xFF)    )   )
#endif


int pkg_nochecking = 0;	/* set to disable extra checking for input */
int pkg_permport = 0;	/* TCP port that pkg_permserver() is listening on XXX */

/* Internal Functions */
static struct pkg_conn *pkg_makeconn(int fd, struct pkg_switch *switchp, void (*errlog) (/* ??? */));
static void pkg_errlog(char *s);
static void pkg_perror(void (*errlog) (/* ??? */), char *s);
static int pkg_dispatch(register struct pkg_conn *pc);
static int pkg_gethdr(register struct pkg_conn *pc, char *buf);

#define MAX_ERRBUF_SIZE 80
static char errbuf[MAX_ERRBUF_SIZE];
static FILE	*pkg_debug;
static void	pkg_ck_debug(void);
static void	pkg_timestamp(void);
static void	pkg_checkin(register struct pkg_conn *pc, int nodelay);

int pkg_inget(register struct pkg_conn *, char *, int);

#define PKG_CK(p)	{if(p==PKC_NULL||p->pkc_magic!=PKG_MAGIC) {\
			sprintf(errbuf,"%s: bad pointer x%lx line %d\n",__FILE__, (long)(p), __LINE__);\
			pkg_errlog(errbuf);abort();}}

#define	MAXQLEN	512	/* largest packet we will queue on stream */

/* A macro for logging a string message when the debug file is open */
#if 1
#define DMSG(s) if(pkg_debug) { \
	pkg_timestamp(); fprintf(pkg_debug,s); fflush(pkg_debug);}
#else
#define DMSG(s)	/**/
#endif

int
pkg_init() {
#ifdef _WIN32
    WORD wVersionRequested;
    WSADATA wsaData;

    wVersionRequested = MAKEWORD(1, 1);
    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
	fprintf(stderr, "pkg_startup:  could not find a usable WinSock DLL" );
	return(-1);
    }
#endif

    return 0; /* good */
}

void
pkg_terminate() {
#ifdef _WIN32
    WSACleanup();
#endif
}

/*
 * Routines to insert/extract short/long's into char arrays,
 * independend of machine byte order and word-alignment.
 */

/*
 *			P K G _ G S H O R T
 */
unsigned short
pkg_gshort(unsigned char *msgp)
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
pkg_glong(unsigned char *msgp)
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
pkg_pshort(unsigned char *msgp, short unsigned int s)
{

	msgp[1] = s;
	msgp[0] = s >> 8;
	return((char *)msgp+2);
}

/*
 *			P K G _ P L O N G
 */
char *
pkg_plong(unsigned char *msgp, long unsigned int l)
{

	msgp[3] = l;
	msgp[2] = (l >>= 8);
	msgp[1] = (l >>= 8);
	msgp[0] = l >> 8;
	return((char *)msgp+4);
}

/*
 *			P K G _ O P E N
 *
 *  We are a client.  Make a connection to the server.
 *
 *  Returns PKC_ERROR on error.
 */
struct pkg_conn *
pkg_open(char *host, char *service, char *protocol, char *uname, char *passwd, struct pkg_switch *switchp, void (*errlog) (/* ??? */))
{
#ifdef _WIN32
	LPHOSTENT lpHostEntry;
	register SOCKET netfd;
	SOCKADDR_IN saServer;
#else
	struct sockaddr_in sinme;		/* Client */
	struct sockaddr_in sinhim;		/* Server */
#ifdef HAVE_UNIX_DOMAIN_SOCKETS
	struct sockaddr_un sunhim;		/* Server, UNIX Domain */
#endif
	register struct hostent *hp;
	register int netfd;
	struct	sockaddr *addr;			/* UNIX or INET addr */
	int	addrlen;			/* length of address */
#endif

	pkg_ck_debug();
	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_open(%s, %s, %s, %s, (passwd), switchp=x%lx, errlog=x%lx)\n",
			host, service, protocol, uname,
			(long)switchp, (long)errlog );
		fflush(pkg_debug);
	}

	/* Check for default error handler */
	if( errlog == NULL )
		errlog = pkg_errlog;

#ifdef _WIN32
	if ((lpHostEntry = gethostbyname(host)) == NULL) {
	    pkg_perror(errlog, "pkg_open:  gethostbyname");
	    return(-1);
	}

	if ((netfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
	    pkg_perror(errlog, "pkg_open:  socket");
	    return(-1);
	}

#if 0
	_setmode(netfd, _O_BINARY);
#endif

	bzero((char *)&saServer, sizeof(saServer));

	if (atoi(service) > 0) {
	    saServer.sin_port = htons((unsigned short)atoi(service));
	} else {
	    register struct servent *sp;
	    if ((sp = getservbyname(service, "tcp")) == NULL) {
		sprintf(errbuf,"pkg_open(%s,%s): unknown service\n",
			host, service );
		errlog(errbuf);
		return(PKC_ERROR);
	    }
	    saServer.sin_port = sp->s_port;
	}
	saServer.sin_family = AF_INET;
	saServer.sin_addr = *((LPIN_ADDR)*lpHostEntry->h_addr_list);

	if (connect(netfd, (LPSOCKADDR)&saServer, sizeof(struct sockaddr)) == SOCKET_ERROR) {
	    pkg_perror(errlog, "pkg_open:  client connect");
	    closesocket(netfd);
	    return(-1);
	}

	return(pkg_makeconn(netfd, switchp, errlog));
#else
	bzero((char *)&sinhim, sizeof(sinhim));
	bzero((char *)&sinme, sizeof(sinme));

#ifdef HAVE_UNIX_DOMAIN_SOCKETS
	if( host == NULL || strlen(host) == 0 || strcmp(host,"unix") == 0 ) {
		/* UNIX Domain socket, port = pathname */
		sunhim.sun_family = AF_UNIX;
		strncpy( sunhim.sun_path, service, sizeof(sunhim.sun_path) );
		addr = (struct sockaddr *) &sunhim;
		addrlen = strlen(sunhim.sun_path) + 2;
		goto ready;
	}
#endif

	/* Determine port for service */
	if( atoi(service) > 0 )  {
		sinhim.sin_port = htons((unsigned short)atoi(service));
	} else {
		register struct servent *sp;
		if( (sp = getservbyname( service, "tcp" )) == NULL )  {
			sprintf(errbuf,"pkg_open(%s,%s): unknown service\n",
				host, service );
			errlog(errbuf);
			return(PKC_ERROR);
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
			sprintf(errbuf,"pkg_open(%s,%s): unknown host\n",
				host, service );
			errlog(errbuf);
			return(PKC_ERROR);
		}
		sinhim.sin_family = hp->h_addrtype;
		bcopy(hp->h_addr, (char *)&sinhim.sin_addr, hp->h_length);
	}
	addr = (struct sockaddr *) &sinhim;
	addrlen = sizeof(struct sockaddr_in);

ready:
	if( (netfd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0 )  {
		pkg_perror( errlog, "pkg_open:  client socket" );
		return(PKC_ERROR);
	}

#if defined(TCP_NODELAY)
	/* SunOS 3.x defines it but doesn't support it! */
	if( addr->sa_family == AF_INET ) {
		int	on = 1;
		if( setsockopt( netfd, IPPROTO_TCP, TCP_NODELAY,
		    (char *)&on, sizeof(on) ) < 0 )  {
			pkg_perror( errlog, "pkg_open: setsockopt TCP_NODELAY" );
		}
	}
#endif

	if( connect(netfd, addr, addrlen) < 0 )  {
		pkg_perror( errlog, "pkg_open: client connect" );
		close(netfd);
		return(PKC_ERROR);
	}
	return( pkg_makeconn(netfd, switchp, errlog) );
#endif
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
pkg_transerver(struct pkg_switch *switchp, void (*errlog) (/* ??? */))
{
	pkg_ck_debug();
	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_transerver(switchp=x%lx, errlog=x%lx)\n",
			(long)switchp, (long)errlog );
		fflush(pkg_debug);
	}

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
pkg_permserver(char *service, char *protocol, int backlog, void (*errlog) (/* ??? */))
{
	register struct servent *sp;
	int	pkg_listenfd;
#ifdef _WIN32
	SOCKADDR_IN saServer;
#else
	struct sockaddr_in sinme;
#ifdef HAVE_UNIX_DOMAIN_SOCKETS
	struct sockaddr_un sunme;		/* UNIX Domain */
#endif
	struct	sockaddr *addr;			/* UNIX or INET addr */
	int	addrlen;			/* length of address */
	int	on = 1;
#endif

	pkg_ck_debug();
	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_permserver(%s, %s, backlog=%d, errlog=x%lx\n",
			service, protocol, backlog, (long)errlog );
		fflush(pkg_debug);
	}

	/* Check for default error handler */
	if( errlog == NULL )
		errlog = pkg_errlog;

#ifdef _WIN32
	bzero((char *)&saServer, sizeof(saServer));

	if (atoi(service) > 0) {
	    saServer.sin_port = htons((unsigned short)atoi(service));
	} else {
	    if ((sp = getservbyname(service, "tcp")) == NULL) {
		sprintf(errbuf,
			"pkg_permserver(%s,%d): unknown service\n",
			service, backlog );
		errlog(errbuf);
		return(-1);
	    }
	    saServer.sin_port = sp->s_port;
	}

	if ((pkg_listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
	    pkg_perror(errlog, "pkg_permserver:  socket");
	    return(-1);
	}

#if 0
	_setmode(pkg_listenfd, _O_BINARY);
#endif

	saServer.sin_family = AF_INET;
	saServer.sin_addr.s_addr = INADDR_ANY;

	if (bind(pkg_listenfd, (LPSOCKADDR)&saServer, sizeof(struct sockaddr)) == SOCKET_ERROR) {
	    pkg_perror(errlog, "pkg_permserver: bind");
	    closesocket(pkg_listenfd);

	    return(-1);
	}

	if (backlog > 5)
	    backlog = 5;

	if (listen(pkg_listenfd, backlog) == SOCKET_ERROR) {
	    pkg_perror(errlog, "pkg_permserver:  listen");
	    closesocket(pkg_listenfd);

	    return(-1);
	}

	return(pkg_listenfd);
#else
	bzero((char *)&sinme, sizeof(sinme));

#ifdef HAVE_UNIX_DOMAIN_SOCKETS
	if( service != NULL && service[0] == '/' ) {
		/* UNIX Domain socket */
		strncpy( sunme.sun_path, service, sizeof(sunme.sun_path) );
		sunme.sun_family = AF_UNIX;
		addr = (struct sockaddr *) &sunme;
		addrlen = strlen(sunme.sun_path) + 2;
		goto ready;
	}
#endif
	/* Determine port for service */
	if( atoi(service) > 0 )  {
		sinme.sin_port = htons((unsigned short)atoi(service));
	} else {
		if( (sp = getservbyname( service, "tcp" )) == NULL )  {
			sprintf(errbuf,
				"pkg_permserver(%s,%d): unknown service\n",
				service, backlog );
			errlog(errbuf);
			return(-1);
		}
		sinme.sin_port = sp->s_port;
	}
	pkg_permport = sinme.sin_port;		/* XXX -- needs formal I/F */
	sinme.sin_family = AF_INET;
	addr = (struct sockaddr *) &sinme;
	addrlen = sizeof(struct sockaddr_in);

ready:
	if( (pkg_listenfd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0 )  {
		pkg_perror( errlog, "pkg_permserver:  socket" );
		return(-1);
	}

	if( addr->sa_family == AF_INET ) {
		if( setsockopt( pkg_listenfd, SOL_SOCKET, SO_REUSEADDR,
		    (char *)&on, sizeof(on) ) < 0 )  {
			pkg_perror( errlog, "pkg_permserver: setsockopt SO_REUSEADDR" );
		}
#if defined(TCP_NODELAY)
		/* SunOS 3.x defines it but doesn't support it! */
		if( setsockopt( pkg_listenfd, IPPROTO_TCP, TCP_NODELAY,
		    (char *)&on, sizeof(on) ) < 0 )  {
			pkg_perror( errlog, "pkg_permserver: setsockopt TCP_NODELAY" );
		}
#endif
	}

	if( bind(pkg_listenfd, addr, addrlen) < 0 )  {
		pkg_perror( errlog, "pkg_permserver: bind" );
		close(pkg_listenfd);
		return(-1);
	}

	if( backlog > 5 )  backlog = 5;
	if( listen(pkg_listenfd, backlog) < 0 )  {
		pkg_perror( errlog, "pkg_permserver:  listen" );
		close(pkg_listenfd);
		return(-1);
	}
	return(pkg_listenfd);
#endif
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
pkg_getclient(int fd, struct pkg_switch *switchp, void (*errlog) (/* ??? */), int nodelay)
{
	struct sockaddr_in from;
	register int s2;
	unsigned int fromlen = sizeof (from);
	auto int onoff;

	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_getclient(fd=%d, switchp=x%lx, errlog=x%lx, nodelay=%d)\n",
			fd, (long)switchp, (long)errlog, nodelay );
		fflush(pkg_debug);
	}

	/* Check for default error handler */
	if( errlog == NULL )
		errlog = pkg_errlog;

#ifdef FIONBIO
	if(nodelay)  {
		onoff = 1;
		if( ioctl(fd, FIONBIO, &onoff) < 0 )
			pkg_perror( errlog, "pkg_getclient: FIONBIO 1" );
	}
#endif
	do  {
#ifdef _WIN32
		s2 = accept(fd, (struct sockaddr *)NULL, NULL);
#else
		s2 = accept(fd, (struct sockaddr *)&from, &fromlen);
#endif
		if (s2 < 0) {
			if(errno == EINTR)
				continue;
#ifdef _WIN32
			if(errno == WSAEWOULDBLOCK)
				return(PKC_NULL);
#else
			if(errno == EWOULDBLOCK)
				return(PKC_NULL);
#endif
			pkg_perror( errlog, "pkg_getclient: accept" );
			return(PKC_ERROR);
		}
	}  while( s2 < 0);
#ifdef FIONBIO
	if(nodelay)  {
		onoff = 0;
		if( ioctl(fd, FIONBIO, &onoff) < 0 )
			pkg_perror( errlog, "pkg_getclient: FIONBIO 2" );
		if( ioctl(s2, FIONBIO, &onoff) < 0 )
			pkg_perror( errlog, "pkg_getclient: FIONBIO 3");
	}
#endif

	return( pkg_makeconn(s2, switchp, errlog) );
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
pkg_makeconn(int fd, struct pkg_switch *switchp, void (*errlog) (/* ??? */))
{
	register struct pkg_conn *pc;

	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_makeconn(fd=%d, switchp=x%lx, errlog=x%lx)\n",
			fd, (long)switchp, (long)errlog );
		fflush(pkg_debug);
	}

	/* Check for default error handler */
	if( errlog == NULL )
		errlog = pkg_errlog;

	if( (pc = (struct pkg_conn *)malloc(sizeof(struct pkg_conn)))==PKC_NULL )  {
		pkg_perror(errlog, "pkg_makeconn: malloc failure\n" );
		return(PKC_ERROR);
	}
	bzero( (char *)pc, sizeof(struct pkg_conn) );
	pc->pkc_magic = PKG_MAGIC;
	pc->pkc_fd = fd;
	pc->pkc_switch = switchp;
	pc->pkc_errlog = errlog;
	pc->pkc_left = -1;
	pc->pkc_buf = (char *)0;
	pc->pkc_curpos = (char *)0;
	pc->pkc_strpos = 0;
	pc->pkc_incur = pc->pkc_inend = 0;
	return(pc);
}

/*
 *  			P K G _ C L O S E
 *
 *  Gracefully release the connection block and close the connection.
 */
void
pkg_close(register struct pkg_conn *pc)
{
	PKG_CK(pc);
	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_close(pc=x%lx) fd=%d\n",
			(long)pc, pc->pkc_fd );
		fflush(pkg_debug);
	}

	/* Flush any queued stream output first. */
	if( pc->pkc_strpos > 0 )  {
		(void)pkg_flush( pc );
	}

	if( pc->pkc_buf != (char *)0 )  {
		sprintf(errbuf,"pkg_close(x%lx):  partial input pkg discarded, buf=x%lx\n",
			(long)pc, (long)(pc->pkc_buf));
		pc->pkc_errlog(errbuf);
		(void)free( pc->pkc_buf );
	}
	if( pc->pkc_inbuf != (char *)0 )  {
		(void)free( pc->pkc_inbuf );
		pc->pkc_inbuf = (char *)0;
		pc->pkc_inlen = 0;
	}
	(void)close(pc->pkc_fd);
	pc->pkc_fd = -1;		/* safety */
	pc->pkc_buf = (char *)0;	/* safety */
	pc->pkc_magic = 0;		/* safety */
	(void)free( (char *)pc );
}

#if 0
/*
 *			P K G _ M R E A D
 *
 * Internal.
 * This function performs the function of a read(II) but will
 * call read(II) multiple times in order to get the requested
 * number of characters.  This can be necessary because pipes
 * and network connections don't deliver data with the same
 * grouping as it is written with.  Written by Robert S. Miles, BRL.
 *
 *  Superceeded by pkg_inget() in this version.
 *  This code is retained because of it's general usefulness.
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
#endif

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
pkg_send(int type, char *buf, int len, register struct pkg_conn *pc)
{
#ifdef HAVE_WRITEV
	static struct iovec cmdvec[2];
#endif
	static struct pkg_header hdr;
	register int i;

	PKG_CK(pc);
	if( len < 0 )  len=0;

	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_send(type=%d, buf=x%lx, len=%d, pc=x%lx)\n",
			 type, (long)buf, len, (long)pc );
		fflush(pkg_debug);
	}

	/* Check for any pending input, no delay */
	/* Input may be read, but not acted upon, to prevent deep recursion */
	pkg_checkin( pc, 1 );

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
	pkg_plong( hdr.pkh_len, (unsigned long)len );

#ifdef HAVE_WRITEV
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
			(int)sizeof(hdr), len, i);
		(pc->pkc_errlog)(errbuf);
		return(i-sizeof(hdr));	/* amount of user data sent */
	}
#else
	/*
	 *  On the assumption that buffer copying is less expensive than
	 *  having this transmission broken into two network packets
	 *  (with TCP, each with a "push" bit set),
	 *  merge it all into one buffer here, unless size is enormous.
	 */
	if( len + sizeof(hdr) <= 16*1024 )  {
		char	tbuf[16*1024];

		bcopy( (char *)&hdr, tbuf, sizeof(hdr) );
		if( len > 0 )
			bcopy( buf, tbuf+sizeof(hdr), len );
#ifdef _WIN32
		if ((i = send(pc->pkc_fd, tbuf, len+sizeof(hdr), 0)) != len+sizeof(hdr)) {
#else
		if( (i = write( pc->pkc_fd, tbuf, len+sizeof(hdr) )) != len+sizeof(hdr) )  {
#endif
			if( i < 0 )  {
				if( errno == EBADF )  return(-1);
				pkg_perror(pc->pkc_errlog, "pkg_send: tbuf write");
				return(-1);
			}
			sprintf(errbuf,"pkg_send of %d, wrote %d\n",
				len, i-(int)sizeof(hdr) );
			(pc->pkc_errlog)(errbuf);
			return(i-sizeof(hdr));	/* amount of user data sent */
		}
		return(len);
	}
	/* Send them separately */
#ifdef _WIN32
	if ((i = send(pc->pkc_fd, (char *)&hdr, sizeof(hdr), 0)) != sizeof(hdr)) {
#else
	if( (i = write( pc->pkc_fd, (char *)&hdr, sizeof(hdr) )) != sizeof(hdr) )  {
#endif
		if( i < 0 )  {
			if( errno == EBADF )  return(-1);
			pkg_perror(pc->pkc_errlog, "pkg_send: header write");
			return(-1);
		}
		sprintf(errbuf,"pkg_send header of %d, wrote %d\n",
			(int)sizeof(hdr), i);
		(pc->pkc_errlog)(errbuf);
		return(-1);		/* amount of user data sent */
	}
	if( len <= 0 )  return(0);
#ifdef _WIN32
	if ((i = send(pc->pkc_fd, buf, len, 0)) != len) {
#else
	if( (i = write( pc->pkc_fd, buf, len )) != len )  {
#endif
		if( i < 0 )  {
			if( errno == EBADF )  return(-1);
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
pkg_2send(int type, char *buf1, int len1, char *buf2, int len2, register struct pkg_conn *pc)
{
#ifdef HAVE_WRITEV
	static struct iovec cmdvec[3];
#endif
	static struct pkg_header hdr;
	register int i;

	PKG_CK(pc);
	if( len1 < 0 )  len1=0;
	if( len2 < 0 )  len2=0;

	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_send2(type=%d, buf1=x%lx, len1=%d, buf2=x%lx, len2=%d, pc=x%lx)\n",
			 type, (long)buf1, len1, (long)buf2, len2, (long)pc );
		fflush(pkg_debug);
	}

	/* Check for any pending input, no delay */
	/* Input may be read, but not acted upon, to prevent deep recursion */
	pkg_checkin( pc, 1 );

	/* Flush any queued stream output first. */
	if( pc->pkc_strpos > 0 )  {
		if( pkg_flush( pc ) < 0 )
			return(-1);	/* assumes 2nd write would fail too */
	}

	pkg_pshort( hdr.pkh_magic, PKG_MAGIC );
	pkg_pshort( hdr.pkh_type, type );	/* should see if valid type */
	pkg_plong( hdr.pkh_len, (unsigned long)(len1+len2) );

#ifdef HAVE_WRITEV
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
			sprintf( errbuf,
				"pkg_send2(type=%d, buf1=x%lx, len1=%d, buf2=x%lx, len2=%d, pc=x%lx)\n",
				 type, (unsigned long int)buf1, len1,
				 (unsigned long int)buf2, len2, (unsigned long int)pc );
			(pc->pkc_errlog)(errbuf);
			return(-1);
		}
		sprintf(errbuf,"pkg_2send of %d+%d+%d, wrote %d\n",
			(int)sizeof(hdr), len1, len2, i);
		(pc->pkc_errlog)(errbuf);
		return(i-sizeof(hdr));	/* amount of user data sent */
	}
#else
	/*
	 *  On the assumption that buffer copying is less expensive than
	 *  having this transmission broken into two network packets
	 *  (with TCP, each with a "push" bit set),
	 *  merge it all into one buffer here, unless size is enormous.
	 */
	if( len1 + len2 + sizeof(hdr) <= 16*1024 )  {
		char	tbuf[16*1024];

		bcopy( (char *)&hdr, tbuf, sizeof(hdr) );
		if( len1 > 0 )
			bcopy( buf1, tbuf+sizeof(hdr), len1 );
		if( len2 > 0 )
			bcopy( buf2, tbuf+sizeof(hdr)+len1, len2 );
#ifdef _WIN32
		if ((i = send(pc->pkc_fd, tbuf, len1+len2+sizeof(hdr), 0)) != len1+len2+sizeof(hdr)) {
#else
		if( (i = write( pc->pkc_fd, tbuf, len1+len2+sizeof(hdr) )) != len1+len2+sizeof(hdr) )  {
#endif
			if( i < 0 )  {
				if( errno == EBADF )  return(-1);
				pkg_perror(pc->pkc_errlog, "pkg_2send: tbuf write");
				return(-1);
			}
			sprintf(errbuf,"pkg_2send of %d+%d, wrote %d\n",
				len1, len2, i-(int)sizeof(hdr) );
			(pc->pkc_errlog)(errbuf);
			return(i-sizeof(hdr));	/* amount of user data sent */
		}
		return(len1+len2);
	}
	/* Send it in three pieces */
#ifdef _WIN32
	if ((i = send(pc->pkc_fd, (char *)&hdr, sizeof(hdr), 0)) != sizeof(hdr)) {
#else
	if( (i = write( pc->pkc_fd, (char *)&hdr, sizeof(hdr) )) != sizeof(hdr) )  {
#endif
		if( i < 0 )  {
			if( errno == EBADF )  return(-1);
			pkg_perror(pc->pkc_errlog, "pkg_2send: header write");
			sprintf(errbuf, "pkg_2send write(%d, x%lx, %d) ret=%d\n",
				pc->pkc_fd, (long)&hdr, (int)sizeof(hdr), i );
			(pc->pkc_errlog)(errbuf);
			return(-1);
		}
		sprintf(errbuf,"pkg_2send of %d+%d+%d, wrote header=%d\n",
			(int)sizeof(hdr), len1, len2, i );
		(pc->pkc_errlog)(errbuf);
		return(-1);		/* amount of user data sent */
	}
#ifdef _WIN32
	if ((i = send(pc->pkc_fd, buf1, len1, 0)) != len1) {
#else
	if( (i = write( pc->pkc_fd, buf1, len1 )) != len1 )  {
#endif
		if( i < 0 )  {
			if( errno == EBADF )  return(-1);
			pkg_perror(pc->pkc_errlog, "pkg_2send: write buf1");
			sprintf(errbuf, "pkg_2send write(%d, x%lx, %d) ret=%d\n",
				pc->pkc_fd, (long)buf1, len1, i );
			(pc->pkc_errlog)(errbuf);
			return(-1);
		}
		sprintf(errbuf,"pkg_2send of %d+%d+%d, wrote len1=%d\n",
			(int)sizeof(hdr), len1, len2, i );
		(pc->pkc_errlog)(errbuf);
		return(i);		/* amount of user data sent */
	}
	if( len2 <= 0 )  return(i);
#ifdef _WIN32
	if ((i = send(pc->pkc_fd, buf2, len2, 0)) != len2) {
#else
	if( (i = write( pc->pkc_fd, buf2, len2 )) != len2 )  {
#endif
		if( i < 0 )  {
			if( errno == EBADF )  return(-1);
			pkg_perror(pc->pkc_errlog, "pkg_2send: write buf2");
			sprintf(errbuf, "pkg_2send write(%d, x%lx, %d) ret=%d\n",
				pc->pkc_fd, (long)buf2, len2, i );
			(pc->pkc_errlog)(errbuf);
			return(-1);
		}
		sprintf(errbuf,"pkg_2send of %d+%d+%d, wrote len2=%d\n",
			(int)sizeof(hdr), len1, len2, i );
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
pkg_stream(int type, char *buf, int len, register struct pkg_conn *pc)
{
	static struct pkg_header hdr;

	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_stream(type=%d, buf=x%lx, len=%d, pc=x%lx)\n",
			 type, (long)buf, len, (long)pc );
		fflush(pkg_debug);
	}

	if( len > MAXQLEN )
		return( pkg_send(type, buf, len, pc) );

	if( len > PKG_STREAMLEN - sizeof(struct pkg_header) - pc->pkc_strpos )
		pkg_flush( pc );

	/* Queue it */
	pkg_pshort( hdr.pkh_magic, PKG_MAGIC );
	pkg_pshort( hdr.pkh_type, type );	/* should see if valid type */
	pkg_plong( hdr.pkh_len, (unsigned long)len );

	bcopy( (char *)&hdr, &(pc->pkc_stream[pc->pkc_strpos]),
		sizeof(struct pkg_header) );
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
pkg_flush(register struct pkg_conn *pc)
{
	register int	i;

	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_flush( pc=x%lx )\n",
			(long)pc );
		fflush(pkg_debug);
	}

	if( pc->pkc_strpos <= 0 ) {
		pc->pkc_strpos = 0;	/* sanity for < 0 */
		return( 0 );
	}

	if( (i = write(pc->pkc_fd,pc->pkc_stream,pc->pkc_strpos)) != pc->pkc_strpos )  {
		if( i < 0 ) {
			if( errno == EBADF )  return(-1);
			pkg_perror(pc->pkc_errlog, "pkg_flush: write");
			return(-1);
		}
		sprintf(errbuf,"pkg_flush of %d, wrote %d\n",
			pc->pkc_strpos, i);
		(pc->pkc_errlog)(errbuf);
		pc->pkc_strpos -= i;
		/* copy leftovers to front of stream */
		bcopy(pc->pkc_stream + i, pc->pkc_stream, pc->pkc_strpos);
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
 *  All messages of any other type are processed by pkg_block().
 *
 *  Returns the length of the message actually received, or -1 on error.
 */
int
pkg_waitfor(int type, char *buf, int len, register struct pkg_conn *pc)
{
	register int i;

	PKG_CK(pc);
	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_waitfor(type=%d, buf=x%lx, len=%d, pc=x%lx)\n",
			type, (long)buf, len, (long)pc );
		fflush(pkg_debug);
	}
again:
	if( pc->pkc_left >= 0 )  {
		/* Finish up remainder of partially received message */
		if( pkg_block( pc ) < 0 )
			return(-1);
	}

	if( pc->pkc_buf != (char *)0 )  {
		pc->pkc_errlog("pkg_waitfor:  buffer clash\n");
		return(-1);
	}
	if( pkg_gethdr( pc, buf ) < 0 )  return(-1);
	if( pc->pkc_type != type )  {
		/* A message of some other type has unexpectedly arrived. */
		if( pc->pkc_len > 0 )  {
			if( (pc->pkc_buf = (char *)malloc(pc->pkc_len+2)) == NULL )  {
				pkg_perror(pc->pkc_errlog, "pkg_waitfor: malloc failed");
				return(-1);
			}
			pc->pkc_curpos = pc->pkc_buf;
		}
		goto again;
	}
	pc->pkc_left = -1;
	if( pc->pkc_len == 0 )
		return(0);

	/* See if incomming message is larger than user's buffer */
	if( pc->pkc_len > len )  {
		register char *bp;
		int excess;
		sprintf(errbuf,
			"pkg_waitfor:  message %ld exceeds buffer %d\n",
			pc->pkc_len, len );
		(pc->pkc_errlog)(errbuf);
		if( (i = pkg_inget( pc, buf, len )) != len )  {
			sprintf(errbuf,
				"pkg_waitfor:  pkg_inget %d gave %d\n", len, i );
			(pc->pkc_errlog)(errbuf);
			return(-1);
		}
		excess = pc->pkc_len - len;	/* size of excess message */
		if( (bp = (char *)malloc(excess)) == NULL )  {
			pkg_perror(pc->pkc_errlog, "pkg_waitfor: excess message, malloc failed");
			return(-1);
		}
		if( (i = pkg_inget( pc, bp, excess )) != excess )  {
			sprintf(errbuf,
				"pkg_waitfor: pkg_inget of excess, %d gave %d\n",
				excess, i );
			(pc->pkc_errlog)(errbuf);
			(void)free(bp);
			return(-1);
		}
		(void)free(bp);
		return(len);		/* truncated, but OK */
	}

	/* Read the whole message into the users buffer */
	if( (i = pkg_inget( pc, buf, pc->pkc_len )) != pc->pkc_len )  {
		sprintf(errbuf,
			"pkg_waitfor:  pkg_inget %ld gave %d\n",
			pc->pkc_len, i );
		(pc->pkc_errlog)(errbuf);
		return(-1);
	}
	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_waitfor() message type=%d arrived\n", type);
		fflush(pkg_debug);
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
 *  All messages of any other type are processed by pkg_block().
 *
 *  The buffer to contain the actual message is acquired via malloc(),
 *  and the caller must free it.
 *
 *  Returns pointer to message buffer, or NULL.
 */
char *
pkg_bwaitfor(int type, register struct pkg_conn *pc)
{
	register int i;
	register char *tmpbuf;

	PKG_CK(pc);
	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_bwaitfor(type=%d, pc=x%lx)\n",
			type, (long)pc );
		fflush(pkg_debug);
	}
	do  {
		/* Finish any unsolicited msg */
		if( pc->pkc_left >= 0 )
			if( pkg_block(pc) < 0 )
				return((char *)0);
		if( pc->pkc_buf != (char *)0 )  {
			pc->pkc_errlog("pkg_bwaitfor:  buffer clash\n");
			return((char *)0);
		}
		if( pkg_gethdr( pc, (char *)0 ) < 0 )
			return((char *)0);
	}  while( pc->pkc_type != type );

	pc->pkc_left = -1;
	if( pc->pkc_len == 0 )
		return((char *)0);

	/* Read the whole message into the dynamic buffer */
	if( (i = pkg_inget( pc, pc->pkc_buf, pc->pkc_len )) != pc->pkc_len )  {
		sprintf(errbuf,
			"pkg_bwaitfor:  pkg_inget %ld gave %d\n", pc->pkc_len, i );
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
 *  			P K G _ P R O C E S S
 *
 *
 *  This routine should be called to process all PKGs that are
 *  stored in the internal buffer pkc_inbuf.  This routine does
 *  no I/O, and is used in a "polling" paradigm.
 *
 *  Only after pkg_process() has been called on all PKG connections
 *  should the user process suspend itself in a select() operation,
 *  otherwise packages that have been read into the internal buffer
 *  will remain unprocessed, potentially forever.
 *
 *  If an error code is returned, then select() must NOT be called
 *  until pkg_process has been called again.
 *
 *  A plausable code sample might be:
 *
 *	for(;;)  {
 *		if( pkg_process( pc ) < 0 )  {
 *			printf("pkg_process error encountered\n");
 *			continue;
 *		}
 *		if( bsdselect( pc->pkc_fd, 99, 0 ) != 0 )  {
 *			if( pkg_suckin( pc ) <= 0 )  {
 *				printf("pkg_suckin error or EOF\n");
 *				break;
 *			}
 *		}
 *		if( pkg_process( pc ) < 0 )  {
 *			printf("pkg_process error encountered\n");
 *			continue;
 *		}
 *		do_other_stuff();
 *	}
 *
 *  Note that the first call to pkg_process() handles all buffered packages
 *  before a potentially long delay in select().
 *  The second call to pkg_process() handles any new packages obtained
 *  either directly by pkg_suckin() or as a byproduct of a handler.
 *  This double checking is absolutely necessary, because
 *  the use of pkg_send() or other pkg routines either in the actual
 *  handlers or in do_other_stuff() can cause pkg_suckin() to be
 *  called to bring in more packages.
 *
 *  Returns -
 *	<0	some internal error encountered; DO NOT call select() next.
 *	 0	All ok, no packages processed
 *	>0	All ok, return is # of packages processed (for the curious)
 */
int
pkg_process(register struct pkg_conn *pc)
{
	register int	len;
	register int	available;
	register int	errcnt;
	register int	ret;
	int		goodcnt;

	goodcnt = 0;

	PKG_CK(pc);
	/* This loop exists only to cut off "hard" errors */
	for( errcnt=0; errcnt < 500; )  {
		available = pc->pkc_inend - pc->pkc_incur;	/* amt in input buf */
		if( pkg_debug )  {
			if( pc->pkc_left < 0 )  {
				sprintf(errbuf, "awaiting new header");
			} else if( pc->pkc_left > 0 )  {
				sprintf(errbuf, "need more data");
			} else {
				sprintf(errbuf, "pkg is all here");
			}
			pkg_timestamp();
			fprintf( pkg_debug,
				"pkg_process(pc=x%lx) pkc_left=%d %s (avail=%d)\n",
				(long)pc, pc->pkc_left, errbuf, available );
			fflush(pkg_debug);
		}
		if( pc->pkc_left < 0 )  {
			/*
			 *  Need to get a new PKG header.
			 *  Do so ONLY if the full header is already in the
			 *  internal buffer, to prevent blocking in pkg_gethdr().
			 */
			if( available < sizeof(struct pkg_header) )
				break;

			if( pkg_gethdr( pc, (char *)0 ) < 0 )  {
				DMSG("pkg_gethdr < 0\n");
				errcnt++;
				continue;
			}

			if( pc->pkc_left < 0 )  {
				/* pkg_gethdr() didn't get a header */
				DMSG("pkc_left still < 0 after pkg_gethdr()\n");
				errcnt++;
				continue;
			}
		}
		/*
		 *  Now pkc_left >= 0, implying header has been obtained.
		 *  Find amount still available in input buffer.
		 */
		available = pc->pkc_inend - pc->pkc_incur;

		/* copy what is here already, and dispatch when all here */
		if( pc->pkc_left > 0 )  {
			if( available <= 0 )  break;

			/* Sanity check -- buffer must be allocated by now */
			if( pc->pkc_curpos == 0 )  {
				DMSG("curpos=0\n");
				errcnt++;
				continue;
			}

			if( available > pc->pkc_left )  {
				/* There is more in input buf than just this pkg */
				len = pc->pkc_left; /* trim to amt needed */
			} else {
				/* Take all that there is */
				len = available;
			}
			len = pkg_inget( pc, pc->pkc_curpos, len );
			pc->pkc_curpos += len;
			pc->pkc_left -= len;
			if( pc->pkc_left > 0 )  {
				/*
				 *  Input buffer is exhausted, but more
				 *  data is needed to finish this package.
				 */
				break;
			}
		}

		if( pc->pkc_left != 0 )  {
			/* Somehow, a full PKG has not yet been obtained */
			DMSG("pkc_left != 0\n");
			errcnt++;
			continue;
		}

		/* Now, pkc_left == 0, dispatch the message */
		if( pkg_dispatch(pc) <= 0 )  {
			/* something bad happened */
			DMSG("pkg_dispatch failed\n");
			errcnt++;
		} else {
			/* it worked */
			goodcnt++;
		}
	}

	if( errcnt > 0 )  {
		ret = -errcnt;
	} else {
		ret = goodcnt;
	}

	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_process() ret=%d, pkc_left=%d, errcnt=%d, goodcnt=%d\n",
			ret, pc->pkc_left, errcnt, goodcnt);
		fflush(pkg_debug);
	}
	return( ret );
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
pkg_dispatch(register struct pkg_conn *pc)
{
	register int i;

	PKG_CK(pc);
	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_dispatch(pc=x%lx) type=%d, buf=x%lx, len=%ld\n",
			(long)pc, pc->pkc_type, (long)(pc->pkc_buf), pc->pkc_len );
		fflush(pkg_debug);
	}
	if( pc->pkc_left != 0 )  return(-1);

	/* Whole message received, process it via switchout table */
	for( i=0; pc->pkc_switch[i].pks_handler != NULL; i++ )  {
		register char *tempbuf;

		if( pc->pkc_switch[i].pks_type != pc->pkc_type )
			continue;
		/*
		 * NOTICE:  User Handler must free() message buffer!
		 * WARNING:  Handler may recurse back to pkg_suckin() --
		 * reset all connection state variables first!
		 */
		tempbuf = pc->pkc_buf;
		pc->pkc_buf = (char *)0;
		pc->pkc_curpos = (char *)0;
		pc->pkc_left = -1;		/* safety */
		/* pc->pkc_type, pc->pkc_len are preserved for handler */
		pc->pkc_switch[i].pks_handler(pc, tempbuf);
		return(1);
	}
	sprintf(errbuf,"pkg_dispatch:  no handler for message type %d, len %ld\n",
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
pkg_gethdr(register struct pkg_conn *pc, char *buf)
{
	register int i;

	PKG_CK(pc);
	if( pc->pkc_left >= 0 )  return(1);	/* go get it! */

	/*
	 *  At message boundary, read new header.
	 *  This will block until the new header arrives (feature).
	 */
	if( (i = pkg_inget( pc, (char *)&(pc->pkc_hdr),
	    sizeof(struct pkg_header) )) != sizeof(struct pkg_header) )  {
		if(i > 0) {
			sprintf(errbuf,"pkg_gethdr: header read of %d?\n", i);
			(pc->pkc_errlog)(errbuf);
		}
		return(-1);
	}
	while( pkg_gshort(pc->pkc_hdr.pkh_magic) != PKG_MAGIC )  {
		int	c;
		c = *((unsigned char *)&pc->pkc_hdr);
		if( isascii(c) && isprint(c) )  {
			sprintf(errbuf,
				"pkg_gethdr: skipping noise x%x %c\n",
				c, c );
		} else {
			sprintf(errbuf,
				"pkg_gethdr: skipping noise x%x\n",
				c );
		}
		(pc->pkc_errlog)(errbuf);
		/* Slide over one byte and try again */
		bcopy( ((char *)&pc->pkc_hdr)+1, (char *)&pc->pkc_hdr, sizeof(struct pkg_header)-1);
		if( (i=pkg_inget( pc,
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
		if( (pc->pkc_buf = (char *)malloc(pc->pkc_len+2)) == NULL )  {
			pkg_perror(pc->pkc_errlog, "pkg_gethdr: malloc fail");
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
 *  pkg_dispatch(), which invokes the user-supplied message handler.
 *
 *  This routine can be used in a loop to pass the time while waiting
 *  for a flag to be changed by the arrival of an asynchronous message,
 *  or for the arrival of a message of uncertain type.
 *
 *  The companion routine is pkg_process(), which does not block.
 *
 *  Control returns to the caller after one full message is processed.
 *  Returns -1 on error, etc.
 */
int
pkg_block(register struct pkg_conn *pc)
{
	PKG_CK(pc);
	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_block(pc=x%lx)\n",
			(long)pc );
		fflush(pkg_debug);
	}

	/* If no read operation going now, start one. */
	if( pc->pkc_left < 0 )  {
		if( pkg_gethdr( pc, (char *)0 ) < 0 )  return(-1);
		/* Now pkc_left >= 0 */
	}

	/* Read the rest of the message, blocking if necessary */
	if( pc->pkc_left > 0 )  {
		if( pkg_inget( pc, pc->pkc_curpos, pc->pkc_left ) != pc->pkc_left )  {
			pc->pkc_left = -1;
			return(-1);
		}
		pc->pkc_left = 0;
	}

	/* Now, pkc_left == 0, dispatch the message */
	return( pkg_dispatch(pc) );
}


/*
 *			P K G _ P E R R O R
 *
 *  Produce a perror on the error logging output.
 */
static void
pkg_perror(void (*errlog) (/* ??? */), char *s)
{
	int ret = 0;
	sprintf( errbuf, "%s: ", s);

	if ( errno >= 0 || strlen(errbuf) >= MAX_ERRBUF_SIZE) {
		sprintf( errbuf, "%s: errno=%d\n", s, errno );
		errlog( errbuf );
		return;
	}

#if HAVE_STRERROR_R
       	ret = strerror_r(errno, errbuf+strlen(errbuf), MAX_ERRBUF_SIZE-strlen(errbuf));
	if (ret != 0) {
		sprintf(errbuf, "%s: errno=%d\n", s, errno);
	}
#else
#  if HAVE_STRERROR
	sprintf( errbuf, "%s: %s\n", s, strerror(errno) );
#  else
	sprintf( errbuf, "%s: %s\n", s, sys_errlist[errno] );
#  endif
#endif
	errlog( errbuf );
}

/*
 *			P K G _ E R R L O G
 *
 *  Default error logger.  Writes to stderr.
 */
static void
pkg_errlog(char *s)
{
	if( pkg_debug )  {
		pkg_timestamp();
		fputs( s, pkg_debug );
		fflush(pkg_debug);
	}
	fputs( s, stderr );
}

/*
 *			P K G _ C K _ D E B U G
 */
static void
pkg_ck_debug(void)
{
	char	*place;
	char	buf[128];

	if( pkg_debug )  return;
	if( (place = (char *)getenv("LIBPKG_DEBUG")) == (char *)0 )  {
		sprintf( buf, "/tmp/pkg.log" );
		place = buf;
	}
	/* Named file must exist and be writeable */
	if( access( place, 2 ) < 0 )  return;
	if( (pkg_debug = fopen( place, "a" )) == NULL )  return;

	/* Log version number of this code */
	pkg_timestamp();
#ifndef lint
	fprintf( pkg_debug, "pkg_ck_debug %s\n", RCSid );
#endif
}

/*
 *			P K G _ T I M E S T A M P
 *
 *  Output a timestamp to the log, suitable for starting each line with.
 */
static void
pkg_timestamp(void)
{
	time_t		now;
	struct tm	*tmp;

	if( !pkg_debug )  return;
	(void)time( &now );
	tmp = localtime( &now );
	fprintf(pkg_debug, "%2.2d/%2.2d %2.2d:%2.2d:%2.2d [%5d] ",
		tmp->tm_mon+1, tmp->tm_mday,
		tmp->tm_hour, tmp->tm_min, tmp->tm_sec,
		getpid() );
	/* Don't fflush here, wait for rest of line */
}

/*
 *			P K G _ S U C K I N
 *
 *  Suck all data from the operating system into the internal buffer.
 *  This is done with large buffers, to maximize the efficiency of the
 *  data transfer from kernel to user.
 *
 *  It is expected that the read() system call will return as much
 *  data as the kernel has, UP TO the size indicated.
 *  The only time the read() may be expected to block is when the
 *  kernel does not have any data at all.
 *  Thus, it is wise to call call this routine only if:
 *	a)  select() has indicated the presence of data, or
 *	b)  blocking is acceptable.
 *
 *  This routine is the only place where data is taken off the network.
 *  All input is appended to the internal buffer for later processing.
 *
 *  Subscripting was used for pkc_incur/pkc_inend to avoid having to
 *  recompute pointers after a realloc().
 *
 *  Returns -
 *	-1	on error
 *	 0	on EOF
 *	 1	success
 */
int
pkg_suckin(register struct pkg_conn *pc)
{
	int	avail;
	int	got;
	int	ret;

	got = 0;
	PKG_CK(pc);

	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_suckin() incur=%d, inend=%d, inlen=%d\n",
			pc->pkc_incur, pc->pkc_inend, pc->pkc_inlen );
		fflush(pkg_debug);
	}

	/* If no buffer allocated yet, get one */
	if( pc->pkc_inbuf == (char *)0 || pc->pkc_inlen <= 0 )  {
		pc->pkc_inlen = PKG_STREAMLEN;
		if( (pc->pkc_inbuf = (char *)malloc(pc->pkc_inlen)) == (char *)0 )  {
			pc->pkc_errlog("pkg_suckin malloc failure\n");
			pc->pkc_inlen = 0;
			ret = -1;
			goto out;
		}
		pc->pkc_incur = pc->pkc_inend = 0;
	}

	if( pc->pkc_incur >= pc->pkc_inend )  {
		/* Reset to beginning of buffer */
		pc->pkc_incur = pc->pkc_inend = 0;
	}

	/* If cur point is near end of buffer, recopy data to buffer front */
	if( pc->pkc_incur >= (pc->pkc_inlen * 7) / 8 )  {
		register int	ammount;

		ammount = pc->pkc_inend - pc->pkc_incur;
		/* This copy can not overlap itself, because of 7/8 above */
		bcopy( &pc->pkc_inbuf[pc->pkc_incur],
			pc->pkc_inbuf, ammount );
		pc->pkc_incur = 0;
		pc->pkc_inend = ammount;
	}

	/* If remaining buffer space is small, make buffer bigger */
	avail = pc->pkc_inlen - pc->pkc_inend;
	if( avail < 10 * sizeof(struct pkg_header) )  {
		pc->pkc_inlen <<= 1;
		if( pkg_debug)  {
			pkg_timestamp();
			fprintf(pkg_debug,
				"pkg_suckin: realloc inbuf to %d\n",
				pc->pkc_inlen );
			fflush(pkg_debug);
		}
		if( (pc->pkc_inbuf = (char *)realloc(pc->pkc_inbuf, pc->pkc_inlen)) == (char *)0 )  {
			pc->pkc_errlog("pkg_suckin realloc failure\n");
			pc->pkc_inlen = 0;
			ret = -1;
			goto out;
		}
		/* since the input buffer has grown, lets update avail */
		avail = pc->pkc_inlen - pc->pkc_inend;
	}

	/* Take as much as the system will give us, up to buffer size */
#ifdef _WIN32
	if ((got = recv(pc->pkc_fd, &pc->pkc_inbuf[pc->pkc_inend], avail, 0)) <= 0) {
#else
	if( (got = read( pc->pkc_fd, &pc->pkc_inbuf[pc->pkc_inend], avail )) <= 0 )  {
#endif
		if( got == 0 )  {
			if( pkg_debug )  {
				pkg_timestamp();
				fprintf(pkg_debug,
					"pkg_suckin: fd=%d, read for %d returned 0\n",
					avail, pc->pkc_fd );
				fflush(pkg_debug);
			}
			ret = 0;	/* EOF */
			goto out;
		}
#ifndef _WIN32
		pkg_perror(pc->pkc_errlog, "pkg_suckin: read");
		sprintf(errbuf, "pkg_suckin: read(%d, x%lx, %d) ret=%d inbuf=x%lx, inend=%d\n",
			pc->pkc_fd, (long)(&pc->pkc_inbuf[pc->pkc_inend]), avail,
			got,
			(long)(pc->pkc_inbuf), pc->pkc_inend );
		(pc->pkc_errlog)(errbuf);
#endif
		ret = -1;
		goto out;
	}
	if( got > avail )  {
		pc->pkc_errlog("pkg_suckin:  read more bytes than desired\n");
		got = avail;
	}
	pc->pkc_inend += got;
	ret = 1;
out:
	if( pkg_debug )  {
		pkg_timestamp();
		fprintf( pkg_debug,
			"pkg_suckin() ret=%d, got %d, total=%d\n",
			ret, got, pc->pkc_inend - pc->pkc_incur );
		fflush(pkg_debug);
	}
	return(ret);
}

/*
 *			P K G _ C H E C K I N
 *
 *  This routine is called whenever it is necessary to see if there
 *  is more input that can be read.
 *  If input is available, it is read into pkc_inbuf[].
 *  If nodelay is set, poll without waiting.
 */
static void
pkg_checkin(register struct pkg_conn *pc, int nodelay)
{
	struct timeval	tv;
	fd_set		bits;
	register int	i, j;
	extern int	errno;

	/* Check socket for unexpected input */
	tv.tv_sec = 0;
	if( nodelay )
		tv.tv_usec = 0;		/* poll -- no waiting */
	else
		tv.tv_usec = 20000;	/* 20 ms */

	FD_ZERO(&bits);
	FD_SET(pc->pkc_fd, &bits);
	i = select( pc->pkc_fd+1, &bits, (fd_set *)0, (fd_set *)0, &tv );
	if( pkg_debug )  {
		pkg_timestamp();
		fprintf(pkg_debug,
			"pkg_checkin: select on fd %d returned %d\n",
			pc->pkc_fd,
			i );
		fflush(pkg_debug);
	}
	if( i > 0 )  {
		for( j = 0; j < FD_SETSIZE; j++ )
			if( FD_ISSET( j, &bits ) ) break;

		if( j < FD_SETSIZE )  {
			/* Some fd is ready for I/O */
			(void)pkg_suckin(pc);
		} else {
			/* Odd condition, bits! */
			sprintf(errbuf,
				"pkg_checkin: select returned %d, bits=0\n",
				i );
			(pc->pkc_errlog)(errbuf);
		}
	} else if( i < 0 )  {
		/* Error condition */
		if( errno != EINTR && errno != EBADF )
			pkg_perror(pc->pkc_errlog, "pkg_checkin: select");
	}
}

/*
 *			P K G _ I N G E T
 *
 *  A functional replacement for mread(), through the
 *  first level input buffer.
 *  This will block if the required number of bytes are not available.
 *  The number of bytes actually transferred is returned.
 */
int
pkg_inget(register struct pkg_conn *pc, char *buf, int count)
{
	register int	len;
	register int	todo = count;

	while( todo > 0 )  {

		while( (len = pc->pkc_inend - pc->pkc_incur) <= 0 )  {
			/* This can block */
			if( pkg_suckin( pc ) < 1 )
				return( count - todo );
		}
		/* Input Buffer has some data in it, move to caller's buffer */
		if( len > todo )  len = todo;
		bcopy( &pc->pkc_inbuf[pc->pkc_incur], buf, len );
		pc->pkc_incur += len;
		buf += len;
		todo -= len;
	}
	return( count );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
