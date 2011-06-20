/*                           P K G . C
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
/** @file libpkg/pkg.c
 *
 * LIBPKG provides routines to manage multiplexing and de-multiplexing
 * synchronous and asynchronous messages across stream connections.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>		/* used by inet_addr() routine, below */
#include <time.h>
#include <string.h>

#ifdef HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif
#ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#ifdef HAVE_WINSOCK_H
#  include <process.h>
#  include <winsock.h>
#else
#  include <sys/socket.h>
#  include <sys/ioctl.h>	/* for FIONBIO */
#  include <netinet/in.h>	/* for htons(), etc */
#  include <netdb.h>
#  include <netinet/tcp.h>	/* for TCP_NODELAY sockopt */
#  include <arpa/inet.h>	/* for inet_addr() */
#  undef LITTLE_ENDIAN		/* defined in netinet/{ip.h, tcp.h} */
#endif

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#else
#  include <windows.h>
#  include <io.h>
#  include <fcntl.h>
#endif



/* Not all systems with "BSD Networking" include UNIX Domain sockets */
#ifdef HAVE_SYS_UN_H
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


/* XXX is this really necessary?  the _read() and _write()
 * compatibility macros should take care of this.
 */
#ifdef HAVE_WINSOCK_H
#  define PKG_READ(d, buf, nbytes) recv((d), (buf), (int)(nbytes), 0)
#  define PKG_SEND(d, buf, nbytes) send((d), (buf), (int)(nbytes), 0)
#else
#  define PKG_READ(d, buf, nbytes) read((d), (buf), (nbytes))
#  define PKG_SEND(d, buf, nbytes) write((d), (buf), (nbytes))
#endif

#define PKG_CK(p) { \
	if (p==PKC_NULL||p->pkc_magic!=PKG_MAGIC) { \
		snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "%s: bad pointer %p line %d\n", __FILE__, (void *)(p), __LINE__); \
		_pkg_errlog(_pkg_errbuf);abort(); \
	} \
}

#define MAXQLEN 512	/* largest packet we will queue on stream */

/* A macro for logging a string message when the debug file is open */
#ifndef NO_DEBUG_CHECKING
#  define DMSG(s) if (_pkg_debug) { _pkg_timestamp(); fprintf(_pkg_debug, "%s", s); fflush(_pkg_debug); }
#else
#  define DMSG(s) /**/
#endif

#if !defined(_WIN32) || defined(__CYGWIN__)
extern int errno;
#endif

int pkg_nochecking = 0;	/* set to disable extra checking for input */
int pkg_permport = 0;	/* TCP port that pkg_permserver() is listening on XXX */

#define MAX_PKG_ERRBUF_SIZE 80
static char _pkg_errbuf[MAX_PKG_ERRBUF_SIZE] = {0};
static FILE *_pkg_debug = (FILE*)NULL;


/*
 * Routines to insert/extract short/long's into char arrays,
 * independent of machine byte order and word-alignment.
 */


unsigned short
pkg_gshort(char *buf)
{
    unsigned char *p = (unsigned char *)buf;
    unsigned short u;

    u = *p++ << 8;
    return (unsigned short)(u | *p);
}


unsigned long
pkg_glong(char *buf)
{
    unsigned char *p = (unsigned char *)buf;
    unsigned long u;

    u = *p++; u <<= 8;
    u |= *p++; u <<= 8;
    u |= *p++; u <<= 8;
    return (u | *p);
}


char *
pkg_pshort(char *buf, unsigned short s)
{
    buf[1] = s;
    buf[0] = s >> 8;
    return (char *)buf+2;
}


char *
pkg_plong(char *buf, unsigned long l)
{
    buf[3] = l;
    buf[2] = (l >>= 8);
    buf[1] = (l >>= 8);
    buf[0] = l >> 8;
    return (char *)buf+4;
}


/**
 * _ P K G _ T I M E S T A M P
 *
 * Output a timestamp to the log, suitable for starting each line
 * with.
 *
 * This is a private implementation function.
 */
static void
_pkg_timestamp(void)
{
    time_t now;
    struct tm *tmp;
    int pid;

    if (!_pkg_debug)
	return;
    (void)time(&now);
    tmp = localtime(&now);

    /* avoid libbu dependency */
#ifdef HAVE_UNISTD_H
    pid = (int)getpid();
#else
    pid = (int)GetCurrentProcessId();
#endif

    fprintf(_pkg_debug, "%2.2d/%2.2d %2.2d:%2.2d:%2.2d [%5d] ",
	    tmp->tm_mon+1, tmp->tm_mday, tmp->tm_hour, tmp->tm_min, tmp->tm_sec, pid);
    /* Don't fflush here, wait for rest of line */
}


/**
 * _ P K G _ E R R L O G
 *
 * Default error logger.  Writes to stderr.
 *
 * This is a private implementation function.
 */
static void
_pkg_errlog(char *s)
{
    if (_pkg_debug) {
	_pkg_timestamp();
	fputs(s, _pkg_debug);
	fflush(_pkg_debug);
    }
    fputs(s, stderr);
}


/**
 * _ P K G _ P E R R O R
 *
 * Produce a perror on the error logging output.
 *
 * This is a private implementation function.
 */
static void
_pkg_perror(void (*errlog) (char *msg), char *s)
{
    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "%s: ", s);

    if (errno >= 0 || strlen(_pkg_errbuf) >= MAX_PKG_ERRBUF_SIZE) {
	snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "%s: errno=%d\n", s, errno);
	errlog(_pkg_errbuf);
	return;
    }

    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "%s: %s\n", s, strerror(errno));
    errlog(_pkg_errbuf);
}


/**
 * _ P K G _ M A K E C O N N
 *
 * Malloc and initialize a pkg_conn structure.  We have already
 * connected to a client or server on the given file descriptor.
 *
 * Returns >0 ptr to pkg_conn block of new connection or PKC_ERROR
 * fatal error.
 *
 * This is a private implementation function.
 */
static
struct pkg_conn *
_pkg_makeconn(int fd, const struct pkg_switch *switchp, void (*errlog) (char *msg))
{
    struct pkg_conn *pc;

    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"_pkg_makeconn(fd=%d, switchp=%p, errlog=x%llx)\n",
		fd, (void *)switchp, (unsigned long long)((uintptr_t)errlog));
	fflush(_pkg_debug);
    }

    /* Check for default error handler */
    if (errlog == NULL)
	errlog = _pkg_errlog;

    if ((pc = (struct pkg_conn *)malloc(sizeof(struct pkg_conn)))==PKC_NULL) {
	_pkg_perror(errlog, "_pkg_makeconn: malloc failure\n");
	return PKC_ERROR;
    }
    memset((char *)pc, 0, sizeof(struct pkg_conn));
    pc->pkc_magic = PKG_MAGIC;
    pc->pkc_fd = fd;
    pc->pkc_switch = switchp;
    pc->pkc_errlog = errlog;
    pc->pkc_left = -1;
    pc->pkc_buf = (char *)0;
    pc->pkc_curpos = (char *)0;
    pc->pkc_strpos = 0;
    pc->pkc_incur = pc->pkc_inend = 0;
    return pc;
}


/**
 * _ P K G _ C K _ D E B U G
 *
 * This is a private implementation function.
 */
static void
_pkg_ck_debug(void)
{
    char *place;
    char buf[128] = {0};
    struct stat sbuf;

    if (_pkg_debug)
	return;
    if ((place = (char *)getenv("LIB_PKG_DEBUG")) == (char *)0) {
	snprintf(buf, 128, "/tmp/pkg.log");
	place = buf;
    }
    /* Named file must exist and be writeable */
    if (stat(place, &sbuf) != 0)
	return;
    if ((_pkg_debug = fopen(place, "a")) == NULL)
	return;

    /* Log version number of this code */
    _pkg_timestamp();
    fprintf(_pkg_debug, "_pkg_ck_debug %s\n", pkg_version());
}


struct pkg_conn *
pkg_open(const char *host, const char *service, const char *protocol, const char *uname, const char *UNUSED(passwd), const struct pkg_switch *switchp, void (*errlog) (char *msg))
{
#ifdef HAVE_WINSOCK_H
    LPHOSTENT lpHostEntry;
    SOCKET netfd;
    SOCKADDR_IN saServer;
    WORD wVersionRequested;		/* initialize Windows socket networking, increment reference count */
    WSADATA wsaData;
#else
    struct sockaddr_in sinme;		/* Client */
    struct sockaddr_in sinhim;		/* Server */
#ifdef HAVE_SYS_UN_H
    struct sockaddr_un sunhim;		/* Server, UNIX Domain */
#endif
    struct hostent *hp;
    int netfd;
    struct sockaddr *addr;			/* UNIX or INET addr */
    size_t addrlen;			/* length of address */
#endif

    _pkg_ck_debug();
    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"pkg_open(%s, %s, %s, %s, (passwd), switchp=%p, errlog=x%llx)\n",
		host, service, protocol, uname,
		(void *)switchp, (unsigned long long)((uintptr_t)errlog));
	fflush(_pkg_debug);
    }

    /* Check for default error handler */
    if (errlog == NULL)
	errlog = _pkg_errlog;

#ifdef HAVE_WINSOCK_H
    wVersionRequested = MAKEWORD(1, 1);
    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
	_pkg_perror(errlog, "pkg_open: could not find a usable WinSock DLL");
	return PKC_ERROR;
    }

    if ((lpHostEntry = gethostbyname(host)) == NULL) {
	_pkg_perror(errlog, "pkg_open: gethostbyname");
	return PKC_ERROR;
    }

    if ((netfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
	_pkg_perror(errlog, "pkg_open: socket");
	return PKC_ERROR;
    }

    memset((char *)&saServer, 0, sizeof(saServer));

    if (atoi(service) > 0) {
	saServer.sin_port = htons((unsigned short)atoi(service));
    } else {
	struct servent *sp;
	if ((sp = getservbyname(service, "tcp")) == NULL) {
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_open(%s, %s): unknown service\n",
		     host, service);
	    errlog(_pkg_errbuf);
	    return PKC_ERROR;
	}
	saServer.sin_port = sp->s_port;
    }
    saServer.sin_family = AF_INET;
    saServer.sin_addr = *((LPIN_ADDR)*lpHostEntry->h_addr_list);

    if (connect(netfd, (LPSOCKADDR)&saServer, sizeof(struct sockaddr)) == SOCKET_ERROR) {
	_pkg_perror(errlog, "pkg_open: client connect");
	closesocket(netfd);
	return PKC_ERROR;
    }

    return _pkg_makeconn(netfd, switchp, errlog);
#else
    memset((char *)&sinhim, 0, sizeof(sinhim));
    memset((char *)&sinme, 0, sizeof(sinme));

#ifdef HAVE_SYS_UN_H
    if (host == NULL || strlen(host) == 0 || strcmp(host, "unix")==0) {
	/* UNIX Domain socket, port = pathname */
	sunhim.sun_family = AF_UNIX;
	strncpy(sunhim.sun_path, service, sizeof(sunhim.sun_path));
	addr = (struct sockaddr *) &sunhim;
	addrlen = strlen(sunhim.sun_path) + 2;
	goto ready;
    }
#endif

    /* Determine port for service */
    if (atoi(service) > 0) {
	sinhim.sin_port = htons((unsigned short)atoi(service));
    } else {
	struct servent *sp;
	if ((sp = getservbyname(service, "tcp")) == NULL) {
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_open(%s, %s): unknown service\n",
		     host, service);
	    errlog(_pkg_errbuf);
	    return PKC_ERROR;
	}
	sinhim.sin_port = sp->s_port;
    }

    /* Get InterNet address */
    if (atoi(host) > 0) {
	/* Numeric */
	sinhim.sin_family = AF_INET;
	sinhim.sin_addr.s_addr = inet_addr(host);
    } else {
	if ((hp = gethostbyname(host)) == NULL) {
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_open(%s, %s): unknown host\n",
		     host, service);
	    errlog(_pkg_errbuf);
	    return PKC_ERROR;
	}
	sinhim.sin_family = hp->h_addrtype;
	memcpy((char *)&sinhim.sin_addr, hp->h_addr, (size_t)hp->h_length);
    }
    addr = (struct sockaddr *) &sinhim;
    addrlen = sizeof(struct sockaddr_in);

#ifdef HAVE_SYS_UN_H
 ready:
#endif

    if ((netfd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0) {
	_pkg_perror(errlog, "pkg_open: client socket");
	return PKC_ERROR;
    }

#if defined(TCP_NODELAY)
    /* SunOS 3.x defines it but doesn't support it! */
    if (addr->sa_family == AF_INET) {
	int on = 1;
	if (setsockopt(netfd, IPPROTO_TCP, TCP_NODELAY,
		       (char *)&on, sizeof(on)) < 0) {
	    _pkg_perror(errlog, "pkg_open: setsockopt TCP_NODELAY");
	}
    }
#endif

    if (connect(netfd, addr, addrlen) < 0) {
	_pkg_perror(errlog, "pkg_open: client connect");
#ifdef HAVE_WINSOCK_H
	(void)closesocket(netfd);
#else
	(void)close(netfd);
#endif
	return PKC_ERROR;
    }
    return _pkg_makeconn(netfd, switchp, errlog);
#endif
}


struct pkg_conn *
pkg_transerver(const struct pkg_switch *switchp, void (*errlog)(char *))
{
    struct pkg_conn *conn;

    _pkg_ck_debug();
    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"pkg_transerver(switchp=%p, errlog=x%llx)\n",
		(void *)switchp, (unsigned long long)((uintptr_t)errlog));
	fflush(_pkg_debug);
    }

    /*
     * XXX - Somehow the system has to know what connection was
     * accepted, its protocol, etc.  For UNIX/inetd we use stdin.
     */
    conn = _pkg_makeconn(STDIN_FILENO, switchp, errlog);
    return conn;
}


/**
 * This is a private implementation function.
 */
static int
_pkg_permserver_impl(struct in_addr iface, const char *service, const char *protocol, int backlog, void (*errlog)(char *msg))
{
    struct servent *sp;
    int pkg_listenfd;
#ifdef HAVE_WINSOCK_H
    SOCKADDR_IN saServer;
    WORD wVersionRequested;		/* initialize Windows socket networking, increment reference count */
    WSADATA wsaData;
#else
    struct sockaddr_in sinme;
#  ifdef HAVE_SYS_UN_H
    struct sockaddr_un sunme;		/* UNIX Domain */
#  endif
    struct sockaddr *addr;			/* UNIX or INET addr */
    size_t addrlen;			/* length of address */
    int on = 1;
#endif

    _pkg_ck_debug();
    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"pkg_permserver(%s, %s, backlog=%d, errlog=x%llx\n",
		service, protocol, backlog, (unsigned long long)((uintptr_t)errlog));
	fflush(_pkg_debug);
    }

    /* Check for default error handler */
    if (errlog == NULL)
	errlog = _pkg_errlog;

#ifdef HAVE_WINSOCK_H
    wVersionRequested = MAKEWORD(1, 1);
    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
	_pkg_perror(errlog, "pkg_open: could not find a usable WinSock DLL");
	return (int)PKC_ERROR;
    }

    memset((char *)&saServer, 0, sizeof(saServer));

    if (atoi(service) > 0) {
	saServer.sin_port = htons((unsigned short)atoi(service));
    } else {
	if ((sp = getservbyname(service, "tcp")) == NULL) {
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE,
		     "pkg_permserver(%s, %d): unknown service\n",
		     service, backlog);
	    errlog(_pkg_errbuf);
	    return (int)PKC_ERROR;
	}
	saServer.sin_port = sp->s_port;
    }

    if ((pkg_listenfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET) {
	_pkg_perror(errlog, "pkg_permserver: socket");
	return (int)PKC_ERROR;
    }

    saServer.sin_family = AF_INET;
    saServer.sin_addr = iface;

    if (bind(pkg_listenfd, (LPSOCKADDR)&saServer, sizeof(struct sockaddr)) == SOCKET_ERROR) {
	_pkg_perror(errlog, "pkg_permserver: bind");
	closesocket(pkg_listenfd);

	return (int)PKC_ERROR;
    }

    if (backlog > 5)
	backlog = 5;

    if (listen(pkg_listenfd, backlog) == SOCKET_ERROR) {
	_pkg_perror(errlog, "pkg_permserver: listen");
	closesocket(pkg_listenfd);

	return (int)(PKC_ERROR);
    }

    return pkg_listenfd;

#else /* !HAVE_WINSOCK_H */

    memset((char *)&sinme, 0, sizeof(sinme));

#  ifdef HAVE_SYS_UN_H
    if (service != NULL && service[0] == '/') {
	/* UNIX Domain socket */
	strncpy(sunme.sun_path, service, sizeof(sunme.sun_path));
	sunme.sun_family = AF_UNIX;
	addr = (struct sockaddr *) &sunme;
	addrlen = strlen(sunme.sun_path) + 2;
	goto ready;
    }
#  endif /* HAVE_SYS_UN_H */
    /* Determine port for service */
    if (atoi(service) > 0) {
	sinme.sin_port = htons((unsigned short)atoi(service));
    } else {
	if ((sp = getservbyname(service, "tcp")) == NULL) {
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE,
		     "pkg_permserver(%s, %d): unknown service\n",
		     service, backlog);
	    errlog(_pkg_errbuf);
	    return -1;
	}
	sinme.sin_port = sp->s_port;
    }
    pkg_permport = sinme.sin_port;		/* XXX -- needs formal I/F */
    sinme.sin_family = AF_INET;
    sinme.sin_addr = iface;
    addr = (struct sockaddr *) &sinme;
    addrlen = sizeof(struct sockaddr_in);

#  ifdef HAVE_SYS_UN_H
 ready:
#  endif

    if ((pkg_listenfd = socket(addr->sa_family, SOCK_STREAM, 0)) < 0) {
	_pkg_perror(errlog, "pkg_permserver: socket");
	return -1;
    }

    if (addr->sa_family == AF_INET) {
	if (setsockopt(pkg_listenfd, SOL_SOCKET, SO_REUSEADDR,
		       (char *)&on, sizeof(on)) < 0) {
	    _pkg_perror(errlog, "pkg_permserver: setsockopt SO_REUSEADDR");
	}
#  if defined(TCP_NODELAY)
	/* SunOS 3.x defines it but doesn't support it! */
	if (setsockopt(pkg_listenfd, IPPROTO_TCP, TCP_NODELAY,
		       (char *)&on, sizeof(on)) < 0) {
	    _pkg_perror(errlog, "pkg_permserver: setsockopt TCP_NODELAY");
	}
#  endif
    }

    if (bind(pkg_listenfd, addr, addrlen) < 0) {
	_pkg_perror(errlog, "pkg_permserver: bind");
#  ifdef HAVE_WINSOCK_H
	(void)closesocket(pkg_listenfd);
#  else
	close(pkg_listenfd);
#  endif
	return -1;
    }

    if (backlog > 5)  backlog = 5;
    if (listen(pkg_listenfd, backlog) < 0) {
	_pkg_perror(errlog, "pkg_permserver: listen");
#  ifdef HAVE_WINSOCK_H
	(void)closesocket(pkg_listenfd);
#  else
	close(pkg_listenfd);
#  endif
	return -1;
    }
    return pkg_listenfd;
#endif /* HAVE_WINSOCK_H */
}


int
pkg_permserver(const char *service, const char *protocol, int backlog, void (*errlog) (char *msg))
{
    struct in_addr iface;
    iface.s_addr = INADDR_ANY;
    return _pkg_permserver_impl(iface, service, protocol, backlog, errlog);
}


int
pkg_permserver_ip(const char *ipOrHostname, const char *service, const char *protocol, int backlog, void (*errlog)(char *msg))
{
    struct hostent* host;
    struct in_addr iface;
    /* if ipOrHostname starts with a number, it's an IP */
    if (ipOrHostname) {
	if (ipOrHostname[0] >= '0' && ipOrHostname[0] <= '9') {
	    iface.s_addr = inet_addr(ipOrHostname);
	} else {
	    /* XXX gethostbyname is deprecated on Windows */
	    host = gethostbyname(ipOrHostname);
	    iface = *(struct in_addr*)host->h_addr;
	}
	return _pkg_permserver_impl(iface, service, protocol, backlog, errlog);
    } else {
	_pkg_perror(errlog, "pkg: ipOrHostname cannot be NULL");
	return -1;
    }
}


struct pkg_conn *
pkg_getclient(int fd, const struct pkg_switch *switchp, void (*errlog) (char *msg), int nodelay)
{
    int s2;
    auto int onoff;
#ifdef HAVE_WINSOCK_H
    WORD wVersionRequested;		/* initialize Windows socket networking, increment reference count */
    WSADATA wsaData;
#else
    struct sockaddr_in from;
    unsigned int fromlen = sizeof (from);
#endif

    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"pkg_getclient(fd=%d, switchp=%p, errlog=x%llx, nodelay=%d)\n",
		fd, (void *)switchp, (unsigned long long)((uintptr_t)errlog), nodelay);
	fflush(_pkg_debug);
    }

    /* Check for default error handler */
    if (errlog == NULL)
	errlog = _pkg_errlog;

#ifdef FIONBIO
    if (nodelay) {
	onoff = 1;
	if (ioctl(fd, FIONBIO, &onoff) < 0)
	    _pkg_perror(errlog, "pkg_getclient: FIONBIO 1");
    }
#endif

#ifdef HAVE_WINSOCK_H
    wVersionRequested = MAKEWORD(1, 1);
    if (WSAStartup(wVersionRequested, &wsaData) != 0) {
	_pkg_perror(errlog, "pkg_open: could not find a usable WinSock DLL");
	return PKC_ERROR;
    }
#endif

    do  {
#ifdef HAVE_WINSOCK_H
	s2 = accept(fd, (struct sockaddr *)NULL, NULL);
#else
	s2 = accept(fd, (struct sockaddr *)&from, &fromlen);
#endif
	if (s2 < 0) {
	    if (errno == EINTR)
		continue;
#ifdef HAVE_WINSOCK_H
	    if (errno == WSAEWOULDBLOCK)
		return PKC_NULL;
#else
	    if (errno == EWOULDBLOCK)
		return PKC_NULL;
#endif
	    _pkg_perror(errlog, "pkg_getclient: accept");
	    return PKC_ERROR;
	}
    }  while (s2 < 0);
#ifdef FIONBIO
    if (nodelay) {
	onoff = 0;
	if (ioctl(fd, FIONBIO, &onoff) < 0)
	    _pkg_perror(errlog, "pkg_getclient: FIONBIO 2");
	if (ioctl(s2, FIONBIO, &onoff) < 0)
	    _pkg_perror(errlog, "pkg_getclient: FIONBIO 3");
    }
#endif

    return _pkg_makeconn(s2, switchp, errlog);
}


void
pkg_close(struct pkg_conn *pc)
{
    PKG_CK(pc);
    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"pkg_close(pc=%p) fd=%d\n",
		(void *)pc, pc->pkc_fd);
	fflush(_pkg_debug);
    }

    /* Flush any queued stream output first. */
    if (pc->pkc_strpos > 0) {
	(void)pkg_flush(pc);
    }

    if (pc->pkc_buf != (char *)0) {
	snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_close(%p): partial input pkg discarded, buf=%p\n",
		 (void *)pc, (void *)(pc->pkc_buf));
	pc->pkc_errlog(_pkg_errbuf);
	(void)free(pc->pkc_buf);
    }
    if (pc->pkc_inbuf != (char *)0) {
	(void)free(pc->pkc_inbuf);
	pc->pkc_inbuf = (char *)0;
	pc->pkc_inlen = 0;
    }
#ifdef HAVE_WINSOCK_H
    (void)closesocket(pc->pkc_fd);
#else
    (void)close(pc->pkc_fd);
#endif

#ifdef HAVE_WINSOCK_H
    /* deinitialize Windows socket networking, decrements ref count */
    WSACleanup();
#endif

    pc->pkc_fd = -1;		/* safety */
    pc->pkc_buf = (char *)0;	/* safety */
    pc->pkc_magic = 0;		/* safety */
    (void)free((char *)pc);
}


/**
 * P K G _ I N G E T
 *
 * A functional replacement for bu_mread() through the first level
 * input buffer.
 *
 * This will block if the required number of bytes are not available.
 * The number of bytes actually transferred is returned.
 *
 * This is a private implementation function.
 */
static size_t
_pkg_inget(struct pkg_conn *pc, char *buf, size_t count)
{
    size_t len = 0;
    int todo = (int)count;

    while (todo > 0) {

	while ((len = pc->pkc_inend - pc->pkc_incur) <= 0) {
	    /* This can block */
	    if (pkg_suckin(pc) < 1)
		return count - todo;
	}
	/* Input Buffer has some data in it, move to caller's buffer */
	if ((int)len > todo)  len = todo;
	memcpy(buf, &pc->pkc_inbuf[pc->pkc_incur], len);
	pc->pkc_incur += (int)len;
	buf += len;
	todo -= (int)len;
    }
    return count;
}


/**
 * _ P K G _ C H E C K I N
 *
 * This routine is called whenever it is necessary to see if there is
 * more input that can be read.  If input is available, it is read
 * into pkc_inbuf[].  If nodelay is set, poll without waiting.
 *
 * This is a private implementation function.
 */
static void
_pkg_checkin(struct pkg_conn *pc, int nodelay)
{
    struct timeval tv;
    fd_set bits;
    ssize_t i;
    unsigned int j;

    /* Check socket for unexpected input */
    tv.tv_sec = 0;
    if (nodelay)
	tv.tv_usec = 0;		/* poll -- no waiting */
    else
	tv.tv_usec = 20000;	/* 20 ms */

    FD_ZERO(&bits);
    FD_SET(pc->pkc_fd, &bits);
    i = select(pc->pkc_fd+1, &bits, (fd_set *)0, (fd_set *)0, &tv);
    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"_pkg_checkin: select on fd %d returned %ld\n",
		pc->pkc_fd,
		(long int)i);
	fflush(_pkg_debug);
    }
    if (i > 0) {
	for (j = 0; j < FD_SETSIZE; j++)
	    if (FD_ISSET(j, &bits)) break;

	if (j < FD_SETSIZE) {
	    /* Some fd is ready for I/O */
	    (void)pkg_suckin(pc);
	} else {
	    /* Odd condition, bits! */
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE,
		     "_pkg_checkin: select returned %ld, bits=0\n",
		     (long int)i);
	    (pc->pkc_errlog)(_pkg_errbuf);
	}
    } else if (i < 0) {
	/* Error condition */
	if (errno != EINTR && errno != EBADF)
	    _pkg_perror(pc->pkc_errlog, "_pkg_checkin: select");
    }
}


int
pkg_send(int type, const char *buf, size_t len, struct pkg_conn *pc)
{
#ifdef HAVE_WRITEV
    static struct iovec cmdvec[2];
#endif
    static struct pkg_header hdr;
    ssize_t i;

    PKG_CK(pc);

    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"pkg_send(type=%d, buf=%p, len=%llu, pc=%p)\n",
		type, (void *)buf, (unsigned long long)len, (void *)pc);
	fflush(_pkg_debug);
    }

    /* Check for any pending input, no delay */
    /* Input may be read, but not acted upon, to prevent deep recursion */
    _pkg_checkin(pc, 1);

    /* Flush any queued stream output first. */
    if (pc->pkc_strpos > 0) {
	/*
	 * Buffered output is already queued, and needs to be flushed
	 * before sending this one.  If this pkg will also fit in the
	 * buffer, add it to the stream, and then send the whole thing
	 * with one flush.  Otherwise, just flush, and proceed.
	 */
	if (len <= MAXQLEN && len <= PKG_STREAMLEN -
	    sizeof(struct pkg_header) - pc->pkc_strpos) {
	    (void)pkg_stream(type, buf, len, pc);
	    return (pkg_flush(pc) < 0) ? -1 : (int)len;
	}
	if (pkg_flush(pc) < 0)
	    return -1;	/* assumes 2nd write would fail too */
    }

    pkg_pshort((char *)hdr.pkh_magic, (unsigned long)PKG_MAGIC);
    pkg_pshort((char *)hdr.pkh_type, (unsigned short)type);	/* should see if valid type */
    pkg_plong((char *)hdr.pkh_len, (unsigned long)len);

#ifdef HAVE_WRITEV
    cmdvec[0].iov_base = (void *)&hdr;
    cmdvec[0].iov_len = sizeof(hdr);
    cmdvec[1].iov_base = (void *)buf;
    cmdvec[1].iov_len = len;

    /*
     * TODO: set this FD to NONBIO.  If not all output got sent, loop
     * in select() waiting for capacity to go out, and reading input
     * as well.  Prevents deadlocking.
     */
    i = writev(pc->pkc_fd, cmdvec, (len>0)?2:1);
    if (i != (ssize_t)(len+sizeof(hdr))) {
	if (i < 0) {
	    _pkg_perror(pc->pkc_errlog, "pkg_send: writev");
	    return -1;
	}
	snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_send of %llu+%llu, wrote %d\n",
		 (unsigned long long)sizeof(hdr), (unsigned long long)len, (int)i);
	(pc->pkc_errlog)(_pkg_errbuf);
	return i-sizeof(hdr);	/* amount of user data sent */
    }
#else
    /*
     * On the assumption that buffer copying is less expensive than
     * having this transmission broken into two network packets (with
     * TCP, each with a "push" bit set), merge it all into one buffer
     * here, unless size is enormous.
     */
    if (len + sizeof(hdr) <= 16*1024) {
	char tbuf[16*1024] = {0};

	memcpy(tbuf, (char *)&hdr, sizeof(hdr));
	if (len > 0)
	    memcpy(tbuf+sizeof(hdr), buf, len);

	i = PKG_SEND(pc->pkc_fd, tbuf, len+sizeof(hdr));
	if ((size_t)i != len+sizeof(hdr)) {
	    if (i < 0) {
		if (errno == EBADF)
		    return -1;
		_pkg_perror(pc->pkc_errlog, "pkg_send: tbuf write");
		return -1;
	    }
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_send of %d, wrote %d\n",
		     len, i-(int)sizeof(hdr));
	    (pc->pkc_errlog)(_pkg_errbuf);
	    return i-sizeof(hdr);	/* amount of user data sent */
	}
	return (int)len;
    }
    /* Send them separately */
    if ((i = PKG_SEND(pc->pkc_fd, (char *)&hdr, sizeof(hdr))) != sizeof(hdr)) {
	if (i < 0) {
	    if (errno == EBADF)
		return -1;
	    _pkg_perror(pc->pkc_errlog, "pkg_send: header write");
	    return -1;
	}
	snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_send header of %llu, wrote %d\n",
		 (unsigned long long)sizeof(hdr), i);
	(pc->pkc_errlog)(_pkg_errbuf);
	return -1;		/* amount of user data sent */
    }
    if (len <= 0)
	return 0;
    i = PKG_SEND(pc->pkc_fd, buf, len);
    if ((size_t)i != len) {
	if (i < 0) {
	    if (errno == EBADF)
		return -1;
	    _pkg_perror(pc->pkc_errlog, "pkg_send: write");
	    return -1;
	}
	snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_send of %d, wrote %d\n", len, i);
	(pc->pkc_errlog)(_pkg_errbuf);
	return i;		/* amount of user data sent */
    }
#endif
    return (int)len;
}


int
pkg_2send(int type, const char *buf1, size_t len1, const char *buf2, size_t len2, struct pkg_conn *pc)
{
#ifdef HAVE_WRITEV
    static struct iovec cmdvec[3];
#endif
    static struct pkg_header hdr;
    ssize_t i;

    PKG_CK(pc);

    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"pkg_send2(type=%d, buf1=%p, len1=%llu, buf2=%p, len2=%llu, pc=%p)\n",
		type, (void *)buf1, (unsigned long long)len1, (void *)buf2, (unsigned long long)len2, (void *)pc);
	fflush(_pkg_debug);
    }

    /* Check for any pending input, no delay */
    /* Input may be read, but not acted upon, to prevent deep recursion */
    _pkg_checkin(pc, 1);

    /* Flush any queued stream output first. */
    if (pc->pkc_strpos > 0) {
	if (pkg_flush(pc) < 0)
	    return -1;	/* assumes 2nd write would fail too */
    }

    pkg_pshort((char *)hdr.pkh_magic, (unsigned short)PKG_MAGIC);
    pkg_pshort((char *)hdr.pkh_type, (unsigned short)type);	/* should see if valid type */
    pkg_plong((char *)hdr.pkh_len, (unsigned long)(len1+len2));

#ifdef HAVE_WRITEV
    cmdvec[0].iov_base = (void *)&hdr;
    cmdvec[0].iov_len = sizeof(hdr);
    cmdvec[1].iov_base = (void *)buf1;
    cmdvec[1].iov_len = len1;
    cmdvec[2].iov_base = (void *)buf2;
    cmdvec[2].iov_len = len2;

    /*
     * TODO: set this FD to NONBIO.  If not all output got sent, loop
     * in select() waiting for capacity to go out, and reading input
     * as well.  Prevents deadlocking.
     */
    i = writev(pc->pkc_fd, cmdvec, 3);
    if (i != (ssize_t)(len1+len2+sizeof(hdr))) {
	if (i < 0) {
	    _pkg_perror(pc->pkc_errlog, "pkg_2send: writev");
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE,
		     "pkg_send2(type=%d, buf1=x%lx, len1=%ld, buf2=x%lx, len2=%ld, pc=x%lx)\n",
		     type, (unsigned long int)buf1, (long)len1,
		     (unsigned long int)buf2, (long)len2, (unsigned long int)pc);
	    (pc->pkc_errlog)(_pkg_errbuf);
	    return -1;
	}
	snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_2send of %llu+%llu+%llu, wrote %ld\n",
		 (unsigned long long)sizeof(hdr), (unsigned long long)len1, (unsigned long long)len2, (long int)i);
	(pc->pkc_errlog)(_pkg_errbuf);
	return i-sizeof(hdr);	/* amount of user data sent */
    }
#else
    /*
     * On the assumption that buffer copying is less expensive than
     * having this transmission broken into two network packets (with
     * TCP, each with a "push" bit set), merge it all into one buffer
     * here, unless size is enormous.
     */
    if (len1 + len2 + sizeof(hdr) <= 16*1024) {
	char tbuf[16*1024] = {0};

	memcpy(tbuf, (char *)&hdr, sizeof(hdr));
	if (len1 > 0)
	    memcpy(tbuf+sizeof(hdr), buf1, len1);
	if (len2 > 0)
	    memcpy(tbuf+sizeof(hdr)+len1, buf2, len2);
	i = PKG_SEND(pc->pkc_fd, tbuf, len1+len2+sizeof(hdr));
	if ((size_t)i != len1+len2+sizeof(hdr)) {
	    if (i < 0) {
		if (errno == EBADF)
		    return -1;
		_pkg_perror(pc->pkc_errlog, "pkg_2send: tbuf write");
		return -1;
	    }
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_2send of %llu+%llu, wrote %d\n",
		     (unsigned long long)len1, (unsigned long long)len2, i-(int)sizeof(hdr));
	    (pc->pkc_errlog)(_pkg_errbuf);
	    return i-sizeof(hdr);	/* amount of user data sent */
	}
	return (int)(len1+len2);
    }
    /* Send it in three pieces */
    if ((i = (ssize_t)PKG_SEND(pc->pkc_fd, (char *)&hdr, sizeof(hdr))) != sizeof(hdr)) {
	if (i < 0) {
	    if (errno == EBADF)
		return -1;
	    _pkg_perror(pc->pkc_errlog, "pkg_2send: header write");
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_2send write(%d, %p, %llu) ret=%d\n",
		     pc->pkc_fd, (void *)&hdr, (unsigned long long)sizeof(hdr), i);
	    (pc->pkc_errlog)(_pkg_errbuf);
	    return -1;
	}
	snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_2send of %d+%d+%d, wrote header=%d\n",
		 (int)sizeof(hdr), len1, len2, i);
	(pc->pkc_errlog)(_pkg_errbuf);
	return -1;		/* amount of user data sent */
    }

    i = PKG_SEND(pc->pkc_fd, buf1, len1);
    if ((size_t)i != len1) {
	if (i < 0) {
	    if (errno == EBADF)
		return -1;
	    _pkg_perror(pc->pkc_errlog, "pkg_2send: write buf1");
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_2send write(%d, %p, %llu) ret=%d\n",
		     pc->pkc_fd, (void *)buf1, (unsigned long long)len1, i);
	    (pc->pkc_errlog)(_pkg_errbuf);
	    return -1;
	}
	snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_2send of %llu+%llu+%llu, wrote len1=%d\n",
		 (unsigned long long)sizeof(hdr), (unsigned long long)len1, (unsigned long long)len2, i);
	(pc->pkc_errlog)(_pkg_errbuf);
	return i;		/* amount of user data sent */
    }
    if (len2 <= (size_t)0)
	return i;

    i = PKG_SEND(pc->pkc_fd, buf2, len2);
    if (i != (ssize_t)len2) {
	if (i < 0) {
	    if (errno == EBADF)
		return -1;
	    _pkg_perror(pc->pkc_errlog, "pkg_2send: write buf2");
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_2send write(%d, %p, %llu) ret=%d\n",
		     pc->pkc_fd, (void *)buf2, (unsigned long long)len2, i);
	    (pc->pkc_errlog)(_pkg_errbuf);
	    return -1;
	}
	snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_2send of %llu+%llu+%llu, wrote len2=%d\n",
		 (unsigned long long)sizeof(hdr), (unsigned long long)len1, (unsigned long long)len2, i);
	(pc->pkc_errlog)(_pkg_errbuf);
	return (int)(len1+i);		/* amount of user data sent */
    }
#endif
    return (int)(len1+len2);
}


int
pkg_stream(int type, const char *buf, size_t len, struct pkg_conn *pc)
{
    static struct pkg_header hdr;

    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"pkg_stream(type=%d, buf=%p, len=%llu, pc=%p)\n",
		type, (void *)buf, (unsigned long long)len, (void *)pc);
	fflush(_pkg_debug);
    }

    if (len > MAXQLEN)
	return pkg_send(type, buf, len, pc);

    if (len > PKG_STREAMLEN - sizeof(struct pkg_header) - pc->pkc_strpos)
	pkg_flush(pc);

    /* Queue it */
    pkg_pshort((char *)hdr.pkh_magic, (unsigned short)PKG_MAGIC);
    pkg_pshort((char *)hdr.pkh_type, (unsigned short)type);	/* should see if valid type */
    pkg_plong((char *)hdr.pkh_len, (unsigned long)len);

    memcpy(&(pc->pkc_stream[pc->pkc_strpos]), (char *)&hdr, sizeof(struct pkg_header));
    pc->pkc_strpos += sizeof(struct pkg_header);
    memcpy(&(pc->pkc_stream[pc->pkc_strpos]), buf, len);
    pc->pkc_strpos += (int)len;

    return (int)(len + sizeof(struct pkg_header));
}


int
pkg_flush(struct pkg_conn *pc)
{
    int i;

    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"pkg_flush(pc=%p)\n",
		(void *)pc);
	fflush(_pkg_debug);
    }

    if (pc->pkc_strpos <= 0) {
	pc->pkc_strpos = 0;	/* sanity for < 0 */
	return 0;
    }

    i = write(pc->pkc_fd, pc->pkc_stream, (size_t)pc->pkc_strpos);
    if (i != pc->pkc_strpos) {
	if (i < 0) {
	    if (errno == EBADF)
		return -1;
	    _pkg_perror(pc->pkc_errlog, "pkg_flush: write");
	    return -1;
	}
	snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_flush of %d, wrote %d\n",
		 pc->pkc_strpos, i);
	(pc->pkc_errlog)(_pkg_errbuf);
	pc->pkc_strpos -= i;
	/* copy leftovers to front of stream */
	memmove(pc->pkc_stream, pc->pkc_stream + i, (size_t)pc->pkc_strpos);
	return i;	/* amount of user data sent */
    }
    pc->pkc_strpos = 0;
    return i;
}


/**
 * _ P K G _ G E T H D R
 *
 * Get header from a new message.
 *
 * Returns 1 when there is some message to go look at and -1 on fatal
 * errors.
 *
 * This is a private implementation function.
 */
static int
_pkg_gethdr(struct pkg_conn *pc, char *buf)
{
    size_t i;

    PKG_CK(pc);
    if (pc->pkc_left >= 0)
	return 1;	/* go get it! */

    /*
     * At message boundary, read new header.  This will block until
     * the new header arrives (feature).
     */
    if ((i = _pkg_inget(pc, (char *)&(pc->pkc_hdr),
			sizeof(struct pkg_header))) != sizeof(struct pkg_header)) {
	if (i > 0) {
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "_pkg_gethdr: header read of %ld?\n", (long)i);
	    (pc->pkc_errlog)(_pkg_errbuf);
	}
	return -1;
    }
    while (pkg_gshort((char *)pc->pkc_hdr.pkh_magic) != PKG_MAGIC) {
	int c;
	c = *((unsigned char *)&pc->pkc_hdr);
	if (isprint(c)) {
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE,
		     "_pkg_gethdr: skipping noise x%x %c\n",
		     c, c);
	} else {
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE,
		     "_pkg_gethdr: skipping noise x%x\n",
		     c);
	}
	(pc->pkc_errlog)(_pkg_errbuf);
	/* Slide over one byte and try again */
	memmove((char *)&pc->pkc_hdr, ((char *)&pc->pkc_hdr)+1, sizeof(struct pkg_header)-1);
	if ((i = _pkg_inget(pc,
			    ((char *)&pc->pkc_hdr)+sizeof(struct pkg_header)-1,
			    1)) != 1) {
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "_pkg_gethdr: hdr read=%ld?\n", (long)i);
	    (pc->pkc_errlog)(_pkg_errbuf);
	    return -1;
	}
    }
    pc->pkc_type = pkg_gshort((char *)pc->pkc_hdr.pkh_type);	/* host order */
    pc->pkc_len = pkg_glong((char *)pc->pkc_hdr.pkh_len);
    pc->pkc_buf = (char *)0;
    pc->pkc_left = (int)pc->pkc_len;
    if (pc->pkc_left == 0)
	return 1;		/* msg here, no data */

    if (buf) {
	pc->pkc_buf = buf;
    } else {
	/* Prepare to read message into dynamic buffer */
	if ((pc->pkc_buf = (char *)malloc(pc->pkc_len+2)) == NULL) {
	    _pkg_perror(pc->pkc_errlog, "_pkg_gethdr: malloc fail");
	    return -1;
	}
    }
    pc->pkc_curpos = pc->pkc_buf;
    return 1;			/* something ready */
}


int
pkg_waitfor (int type, char *buf, size_t len, struct pkg_conn *pc)
{
    size_t i;

    PKG_CK(pc);
    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"pkg_waitfor (type=%d, buf=%p, len=%llu, pc=%p)\n",
		type, (void *)buf, (unsigned long long)len, (void *)pc);
	fflush(_pkg_debug);
    }
 again:
    if (pc->pkc_left >= 0) {
	/* Finish up remainder of partially received message */
	if (pkg_block(pc) < 0)
	    return -1;
    }

    if (pc->pkc_buf != (char *)0) {
	pc->pkc_errlog("pkg_waitfor: buffer clash\n");
	return -1;
    }
    if (_pkg_gethdr(pc, buf) < 0)
	return -1;
    if (pc->pkc_type != type) {
	/* A message of some other type has unexpectedly arrived. */
	if (pc->pkc_len > 0) {
	    if ((pc->pkc_buf = (char *)malloc(pc->pkc_len+2)) == NULL) {
		_pkg_perror(pc->pkc_errlog, "pkg_waitfor: malloc failed");
		return -1;
	    }
	    pc->pkc_curpos = pc->pkc_buf;
	}
	goto again;
    }
    pc->pkc_left = -1;
    if (pc->pkc_len == 0)
	return 0;

    /* See if incomming message is larger than user's buffer */
    if (pc->pkc_len > len) {
	char *bp;
	size_t excess;
	snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE,
		 "pkg_waitfor: message %ld exceeds buffer %ld\n",
		 (long)pc->pkc_len, (long)len);
	(pc->pkc_errlog)(_pkg_errbuf);
	if ((i = _pkg_inget(pc, buf, len)) != len) {
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE,
		     "pkg_waitfor: _pkg_inget %ld gave %ld\n", (long)len, (long)i);
	    (pc->pkc_errlog)(_pkg_errbuf);
	    return -1;
	}
	excess = pc->pkc_len - len;	/* size of excess message */
	if ((bp = (char *)malloc(excess)) == NULL) {
	    _pkg_perror(pc->pkc_errlog, "pkg_waitfor: excess message, malloc failed");
	    return -1;
	}
	if ((i = _pkg_inget(pc, bp, excess)) != excess) {
	    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE,
		     "pkg_waitfor: _pkg_inget of excess, %ld gave %ld\n",
		     (long)excess, (long)i);
	    (pc->pkc_errlog)(_pkg_errbuf);
	    (void)free(bp);
	    return -1;
	}
	(void)free(bp);
	return (int)len;	/* potentially truncated, but OK */
    }

    /* Read the whole message into the users buffer */
    if ((i = _pkg_inget(pc, buf, pc->pkc_len)) != pc->pkc_len) {
	snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE,
		 "pkg_waitfor: _pkg_inget %ld gave %ld\n",
		 (long)pc->pkc_len, (long)i);
	(pc->pkc_errlog)(_pkg_errbuf);
	return -1;
    }
    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"pkg_waitfor () message type=%d arrived\n", type);
	fflush(_pkg_debug);
    }
    pc->pkc_buf = (char *)0;
    pc->pkc_curpos = (char *)0;
    pc->pkc_left = -1;		/* safety */
    return (int)pc->pkc_len;
}


char *
pkg_bwaitfor (int type, struct pkg_conn *pc)
{
    size_t i;
    char *tmpbuf;

    PKG_CK(pc);
    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"pkg_bwaitfor (type=%d, pc=%p)\n",
		type, (void *)pc);
	fflush(_pkg_debug);
    }
    do  {
	/* Finish any unsolicited msg */
	if (pc->pkc_left >= 0)
	    if (pkg_block(pc) < 0)
		return (char *)0;
	if (pc->pkc_buf != (char *)0) {
	    pc->pkc_errlog("pkg_bwaitfor: buffer clash\n");
	    return (char *)0;
	}
	if (_pkg_gethdr(pc, (char *)0) < 0)
	    return (char *)0;
    }  while (pc->pkc_type != type);

    pc->pkc_left = -1;
    if (pc->pkc_len == 0)
	return (char *)0;

    /* Read the whole message into the dynamic buffer */
    if ((i = _pkg_inget(pc, pc->pkc_buf, pc->pkc_len)) != pc->pkc_len) {
	snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE,
		 "pkg_bwaitfor: _pkg_inget %ld gave %ld\n", (long)pc->pkc_len, (long)i);
	(pc->pkc_errlog)(_pkg_errbuf);
    }
    tmpbuf = pc->pkc_buf;
    pc->pkc_buf = (char *)0;
    pc->pkc_curpos = (char *)0;
    pc->pkc_left = -1;		/* safety */
    /* User must free the buffer */
    return tmpbuf;
}


/**
 * _ P K G _ D I S P A T C H
 *
 * Given that a whole message has arrived, send it to the appropriate
 * User Handler, or else grouse.  Returns -1 on fatal error, 0 on no
 * handler, 1 if all's well.
 *
 * This is a private implementation function.
 */
static int
_pkg_dispatch(struct pkg_conn *pc)
{
    int i;

    PKG_CK(pc);
    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"_pkg_dispatch(pc=%p) type=%d, buf=%p, len=%llu\n",
		(void *)pc, pc->pkc_type, (void *)(pc->pkc_buf), (unsigned long long)(pc->pkc_len));
	fflush(_pkg_debug);
    }
    if (pc->pkc_left != 0)
	return -1;

    /* Whole message received, process it via switchout table */
    for (i = 0; pc->pkc_switch[i].pks_handler != NULL; i++) {
	char *tempbuf;

	if (pc->pkc_switch[i].pks_type != pc->pkc_type)
	    continue;
	/*
	 * NOTICE: User Handler must free() message buffer!
	 * WARNING: Handler may recurse back to pkg_suckin() --
	 * reset all connection state variables first!
	 */
	tempbuf = pc->pkc_buf;
	pc->pkc_buf = (char *)0;
	pc->pkc_curpos = (char *)0;
	pc->pkc_left = -1;		/* safety */

	/* copy the user_data from the current pkg_switch into the pkg_conn */
	pc->pkc_user_data = pc->pkc_switch[i].pks_user_data;

	/* pc->pkc_type, pc->pkc_len are preserved for handler */
	pc->pkc_switch[i].pks_handler(pc, tempbuf);

	/* sanity */
	pc->pkc_user_data = (void *)NULL;
	return 1;
    }
    snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "_pkg_dispatch: no handler for message type %d, len %ld\n",
	     pc->pkc_type, (long)pc->pkc_len);
    (pc->pkc_errlog)(_pkg_errbuf);
    (void)free(pc->pkc_buf);
    pc->pkc_buf = (char *)0;
    pc->pkc_curpos = (char *)0;
    pc->pkc_left = -1;		/* safety */
    pc->pkc_user_data = (void *)NULL;
    return 0;
}


int
pkg_process(struct pkg_conn *pc)
{
    size_t len;
    int available;
    int errcnt;
    int ret;
    int goodcnt;

    goodcnt = 0;

    PKG_CK(pc);
    /* This loop exists only to cut off "hard" errors */
    for (errcnt=0; errcnt < 500;) {
	available = pc->pkc_inend - pc->pkc_incur;	/* amt in input buf */
	if (_pkg_debug) {
	    if (pc->pkc_left < 0) {
		snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "awaiting new header");
	    } else if (pc->pkc_left > 0) {
		snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "need more data");
	    } else {
		snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg is all here");
	    }
	    _pkg_timestamp();
	    fprintf(_pkg_debug,
		    "pkg_process(pc=%p) pkc_left=%d %s (avail=%d)\n",
		    (void *)pc, pc->pkc_left, _pkg_errbuf, available);
	    fflush(_pkg_debug);
	}
	if (pc->pkc_left < 0) {
	    /*
	     * Need to get a new PKG header.  Do so ONLY if the full
	     * header is already in the internal buffer, to prevent
	     * blocking in _pkg_gethdr().
	     */
	    if ((size_t)available < sizeof(struct pkg_header))
		break;

	    if (_pkg_gethdr(pc, (char *)0) < 0) {
		DMSG("_pkg_gethdr < 0\n");
		errcnt++;
		continue;
	    }

	    if (pc->pkc_left < 0) {
		/* _pkg_gethdr() didn't get a header */
		DMSG("pkc_left still < 0 after _pkg_gethdr()\n");
		errcnt++;
		continue;
	    }
	}
	/*
	 * Now pkc_left >= 0, implying header has been obtained.  Find
	 * amount still available in input buffer.
	 */
	available = pc->pkc_inend - pc->pkc_incur;

	/* copy what is here already, and dispatch when all here */
	if (pc->pkc_left > 0) {
	    if (available <= 0)  break;

	    /* Sanity check -- buffer must be allocated by now */
	    if (pc->pkc_curpos == 0) {
		DMSG("curpos=0\n");
		errcnt++;
		continue;
	    }

	    if (available > pc->pkc_left) {
		/* There is more in input buf than just this pkg */
		len = pc->pkc_left; /* trim to amt needed */
	    } else {
		/* Take all that there is */
		len = available;
	    }
	    len = _pkg_inget(pc, pc->pkc_curpos, len);
	    pc->pkc_curpos += len;
	    pc->pkc_left -= (int)len;
	    if (pc->pkc_left > 0) {
		/*
		 * Input buffer is exhausted, but more data is needed
		 * to finish this package.
		 */
		break;
	    }
	}

	if (pc->pkc_left != 0) {
	    /* Somehow, a full PKG has not yet been obtained */
	    DMSG("pkc_left != 0\n");
	    errcnt++;
	    continue;
	}

	/* Now, pkc_left == 0, dispatch the message */
	if (_pkg_dispatch(pc) <= 0) {
	    /* something bad happened */
	    DMSG("_pkg_dispatch failed\n");
	    errcnt++;
	} else {
	    /* it worked */
	    goodcnt++;
	}
    }

    if (errcnt > 0) {
	ret = -errcnt;
    } else {
	ret = goodcnt;
    }

    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"pkg_process() ret=%d, pkc_left=%d, errcnt=%d, goodcnt=%d\n",
		ret, pc->pkc_left, errcnt, goodcnt);
	fflush(_pkg_debug);
    }
    return ret;
}


int
pkg_block(struct pkg_conn *pc)
{
    PKG_CK(pc);
    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"pkg_block(pc=%p)\n",
		(void *)pc);
	fflush(_pkg_debug);
    }

    /* If no read operation going now, start one. */
    if (pc->pkc_left < 0) {
	if (_pkg_gethdr(pc, (char *)0) < 0)
	    return -1;
	/* Now pkc_left >= 0 */
    }

    /* Read the rest of the message, blocking if necessary */
    if (pc->pkc_left > 0) {
	if (_pkg_inget(pc, pc->pkc_curpos, (size_t)pc->pkc_left) != (size_t)pc->pkc_left) {
	    pc->pkc_left = -1;
	    return -1;
	}
	pc->pkc_left = 0;
    }

    /* Now, pkc_left == 0, dispatch the message */
    return _pkg_dispatch(pc);
}


int
pkg_suckin(struct pkg_conn *pc)
{
    size_t avail;
    int got;
    int ret;

    got = 0;
    PKG_CK(pc);

    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"pkg_suckin() incur=%d, inend=%d, inlen=%d\n",
		pc->pkc_incur, pc->pkc_inend, pc->pkc_inlen);
	fflush(_pkg_debug);
    }

    /* If no buffer allocated yet, get one */
    if (pc->pkc_inbuf == (char *)0 || pc->pkc_inlen <= 0) {
	pc->pkc_inlen = PKG_STREAMLEN;
	if ((pc->pkc_inbuf = (char *)malloc((size_t)pc->pkc_inlen)) == (char *)0) {
	    pc->pkc_errlog("pkg_suckin malloc failure\n");
	    pc->pkc_inlen = 0;
	    ret = -1;
	    goto out;
	}
	pc->pkc_incur = pc->pkc_inend = 0;
    }

    if (pc->pkc_incur >= pc->pkc_inend) {
	/* Reset to beginning of buffer */
	pc->pkc_incur = pc->pkc_inend = 0;
    }

    /* If cur point is near end of buffer, recopy data to buffer front */
    if (pc->pkc_incur >= (pc->pkc_inlen * 7) / 8) {
	size_t amount;

	amount = pc->pkc_inend - pc->pkc_incur;
	/* This copy can not overlap itself, because of 7/8 above */
	memcpy(pc->pkc_inbuf, &pc->pkc_inbuf[pc->pkc_incur], amount);
	pc->pkc_incur = 0;
	pc->pkc_inend = (int)amount;
    }

    /* If remaining buffer space is small, make buffer bigger */
    avail = pc->pkc_inlen - pc->pkc_inend;
    if (avail < 10 * sizeof(struct pkg_header)) {
	pc->pkc_inlen <<= 1;
	if (_pkg_debug) {
	    _pkg_timestamp();
	    fprintf(_pkg_debug,
		    "pkg_suckin: realloc inbuf to %d\n",
		    pc->pkc_inlen);
	    fflush(_pkg_debug);
	}
	if ((pc->pkc_inbuf = (char *)realloc(pc->pkc_inbuf, (size_t)pc->pkc_inlen)) == (char *)0) {
	    pc->pkc_errlog("pkg_suckin realloc failure\n");
	    pc->pkc_inlen = 0;
	    ret = -1;
	    goto out;
	}
	/* since the input buffer has grown, lets update avail */
	avail = (size_t)pc->pkc_inlen - (size_t)pc->pkc_inend;
    }

    /* Take as much as the system will give us, up to buffer size */
    got = PKG_READ(pc->pkc_fd, &pc->pkc_inbuf[pc->pkc_inend], avail);
    if (got <= 0) {
	if (got == 0) {
	    if (_pkg_debug) {
		_pkg_timestamp();
		fprintf(_pkg_debug,
			"pkg_suckin: fd=%d, read for %ld bytes returned 0\n",
			pc->pkc_fd, (long)avail);
		fflush(_pkg_debug);
	    }
	    ret = 0;	/* EOF */
	    goto out;
	}
#ifndef HAVE_WINSOCK_H
	_pkg_perror(pc->pkc_errlog, "pkg_suckin: read");
	snprintf(_pkg_errbuf, MAX_PKG_ERRBUF_SIZE, "pkg_suckin: read(%d, x%lx, %ld) ret=%d inbuf=x%lx, inend=%d\n",
		 pc->pkc_fd, (long)(&pc->pkc_inbuf[pc->pkc_inend]), (long)avail,
		 got,
		 (long)(pc->pkc_inbuf), pc->pkc_inend);
	(pc->pkc_errlog)(_pkg_errbuf);
#endif
	ret = -1;
	goto out;
    }
    if (got > (int)avail) {
	pc->pkc_errlog("pkg_suckin: read more bytes than desired\n");
	got = (int)avail;
    }
    pc->pkc_inend += got;
    ret = 1;
 out:
    if (_pkg_debug) {
	_pkg_timestamp();
	fprintf(_pkg_debug,
		"pkg_suckin() ret=%d, got %d, total=%d\n",
		ret, got, pc->pkc_inend - pc->pkc_incur);
	fflush(_pkg_debug);
    }
    return ret;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
