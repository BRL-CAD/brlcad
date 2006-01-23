/*                        F B S E R V . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file fbserv.c
 *
 *  Remote libfb server (originally rfbd).
 *
 *  There are three ways this program can be run:
 *  Inetd Daemon - every PKG connection invokes a new copy of us,
 *	courtesy of inetd.  We process a single frame buffer
 *	open/process/close cycle and then exit.  A full installation
 *	includes setting up inetd and /etc/services to start one
 *	of these, with these entries:
 *
 * remotefb stream tcp     nowait  nobody   /usr/brlcad/bin/fbserv  fbserv
 * remotefb        5558/tcp                        # remote frame buffer
 *
 *  Stand-Alone Daemon - once started we run forever, forking a
 *	copy of ourselves for each new connection.  Each child is
 *	essentially like above, i.e. one open/process/close cycle.
 *	Useful for running a daemon on a totally "unmodified" system,
 *	or when inetd is not available.
 *	A child process is necessary because different framebuffers
 *	may be specified in each open.
 *
 *  Single-Frame-Buffer Server - we open a particular frame buffer
 *	at invocation time and leave it open.  We will accept
 *	multiple connections for this frame buffer.
 *	Frame buffer open and close requests are effectively ignored.
 *	Major purpose is to create "reattachable" frame buffers when
 *	using libfb on a window system.  In this case there is no
 *	hardware to preserve "state" information (image data, color
 *	maps, etc.).  By leaving the frame buffer open, the daemon
 *	keeps this state in memory.
 *	Requests can be interleaved from different clients.
 *
 *  Authors -
 *	Phillip Dykstra
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>

#if defined(HAVE_STDARG_H)
#  include <stdarg.h>
#endif
#if !defined(HAVE_STDARG_H) && defined(HAVE_VARARGS_H)
#  include <varargs.h>
#endif

#if HAVE_SYSLOG_H
#  include <syslog.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>		/* For htonl(), etc */
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif

#include <sys/time.h>		/* For struct timeval */

#include "machine.h"
#include "fb.h"
#include "fbmsg.h"
#include "pkg.h"


fd_set	select_list;			/* master copy */
int	max_fd;

extern	int	_fb_disk_enable;

static  void	main_loop(void);
static	void	comm_error(char *str);
static	void	init_syslog(void);
static	void	setup_socket(int fd);
static	int	use_syslog;	/* error messages to stderr if 0 */

static	char	*framebuffer = NULL;	/* frame buffer name */
static	int	width = 0;		/* use default size */
static	int	height = 0;
static	int	port = 0;
static	int	port_set = 0;		/* !0 if user supplied port num */
static	int	once_only = 0;
static 	int	netfd;

#define MAX_CLIENTS	32
struct pkg_conn	*clients[MAX_CLIENTS];

int	verbose = 0;

/* from server.c */
extern const struct pkg_switch fb_server_pkg_switch[];
extern FBIO	*fb_server_fbp;
extern fd_set	*fb_server_select_list;
extern int	*fb_server_max_fd;
extern int	fb_server_got_fb_free;       /* !0 => we have received an fb_free */
extern int	fb_server_refuse_fb_free;    /* !0 => don't accept fb_free() */
extern int	fb_server_retain_on_close;   /* !0 => we are holding a reusable FB open */


/* Hidden args: -p<port_num> -F<frame_buffer> */
static char usage[] = "\
Usage: fbserv port_num\n\
          (for a stand-alone daemon)\n\
   or  fbserv [-v] [-h] [-S squaresize]\n\
          [-W width] [-N height] port_num frame_buffer\n\
          (for a single-frame-buffer server)\n\
";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = getopt( argc, argv, "hvF:s:w:n:S:W:N:p:" )) != EOF )  {
		switch( c )  {
		case 'v':
			verbose = 1;
			break;
		case 'h':
			/* high-res */
			height = width = 1024;
			break;
		case 'F':
			framebuffer = optarg;
			break;
		case 's':
		case 'S':
			height = width = atoi(optarg);
			break;
		case 'w':
		case 'W':
			width = atoi(optarg);
			break;
		case 'n':
		case 'N':
			height = atoi(optarg);
			break;
		case 'p':
			port = atoi(optarg);
			port_set = 1;
			break;

		default:		/* '?' */
			return(0);
		}
	}
	/* If no "-p port", port comes from 1st extra */
	if( (optind < argc) && (port_set == 0) ) {
		port = atoi(argv[optind++]);
		port_set = 1;
	}
	/* If no "-F framebuffer", fb comes from 2nd extra */
	if( (optind < argc) && (framebuffer == NULL) ) {
		framebuffer = argv[optind++];
	}
	if( argc > optind )
		return(0);	/* print usage */

	return(1);		/* OK */
}

/*
 *			I S _ S O C K E T
 *
 * Determine if a file descriptor corresponds to an open socket.
 * Used to detect when we are started from INETD which gives us an
 * open socket connection on fd 0.
 */
int
is_socket(int fd)
{
	struct sockaddr saddr;
	/* Should be: socklen_t namelen but SGI's are complaining... */
	int namelen;

	if( getsockname(fd,&saddr,&namelen) == 0 )
		return	1;
	else
		return	0;
}

static void
sigalarm(int code)
{
	printf("alarm %s\n", fb_server_fbp ? "FBP" : "NULL");
	if( fb_server_fbp != FBIO_NULL )
		fb_poll(fb_server_fbp);
	(void)signal( SIGALRM, sigalarm );	/* SYSV removes handler */
	alarm(1);
}

/*
 *			N E W _ C L I E N T
 */
void
new_client(struct pkg_conn *pcp)
{
	register int	i;

	if( pcp == PKC_ERROR )
		return;

	for( i = MAX_CLIENTS-1; i >= 0; i-- )  {
		if( clients[i] != NULL )  continue;
		/* Found an available slot */
		clients[i] = pcp;
		FD_SET(pcp->pkc_fd, &select_list);
		if( pcp->pkc_fd > max_fd )  max_fd = pcp->pkc_fd;
		setup_socket( pcp->pkc_fd );
		return;
	}
	fprintf(stderr,"fbserv: too many clients\n");
	pkg_close(pcp);
}

/*
 *			D R O P _ C L I E N T
 */
void
drop_client(int sub)
{
	int fd = clients[sub]->pkc_fd;

	if( clients[sub] == PKC_NULL )  return;

	FD_CLR( fd, &select_list );
	pkg_close( clients[sub] );
	clients[sub] = PKC_NULL;
	(void)close( fd );			/* double-safety */
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
	char	portname[32];

	/* No disk files on remote machine */
	_fb_disk_enable = 0;
	memset((void *)clients, 0, sizeof(struct pkg_conn *) * MAX_CLIENTS);

	(void)signal( SIGPIPE, SIG_IGN );
	(void)signal( SIGALRM, sigalarm );
	/*alarm(1)*/

	FD_ZERO(&select_list);
	fb_server_select_list = &select_list;
	fb_server_max_fd = &max_fd;

	/*
	 * Inetd Daemon.
	 * Check to see if we were invoked by /etc/inetd.  If so
	 * we will have an open network socket on fd=0.  Become
	 * a Transient PKG server if this is so.
	 */
	netfd = 0;
	if( is_socket(netfd) ) {
		init_syslog();
		new_client( pkg_transerver( fb_server_pkg_switch, comm_error ) );
		max_fd = 8;
		once_only = 1;
		main_loop();
		exit(0);
	}

	/* for now, make them set a port_num, for usage message */
	if ( !get_args( argc, argv ) || !port_set ) {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	/* Single-Frame-Buffer Server */
	if( framebuffer != NULL ) {
		fb_server_retain_on_close = 1;	/* don't ever close the frame buffer */

		/* open a frame buffer */
		if( (fb_server_fbp = fb_open(framebuffer, width, height)) == FBIO_NULL )
			exit(1);
		if( fb_server_fbp->if_selfd > 0 )  {
			FD_SET(fb_server_fbp->if_selfd, &select_list);
			max_fd = fb_server_fbp->if_selfd;
		}

		/* check/default port */
		if( port_set ) {
			if( port < 1024 )
				port += 5559;
		}
		sprintf(portname,"%d",port);

		/*
		 * Hang an unending listen for PKG connections
		 */
		if( (netfd = pkg_permserver(portname, 0, 0, comm_error)) < 0 )
			exit(-1);
		FD_SET(netfd, &select_list);
		if (netfd > max_fd)
			max_fd = netfd;

		main_loop();
		exit(0);
	}
	/*
	 * Stand-Alone Daemon
	 */
	/* check/default port */
	if( port_set ) {
		if( port < 1024 )
			port += 5559;
		sprintf(portname,"%d",port);
	} else {
		sprintf(portname,"%s","remotefb");
	}

	init_syslog();
	while( (netfd = pkg_permserver(portname, 0, 0, comm_error)) < 0 ) {
	    static int error_count=0;
	    sleep(1);
	    if (error_count++ < 60) {
		continue;
	    }
	    comm_error("Unable to start the stand-alone framebuffer daemon after 60 seconds, giving up.");
	    exit(1);
	}

	while(1) {
		int stat;
		struct pkg_conn	*pcp;

		pcp = pkg_getclient( netfd, fb_server_pkg_switch, comm_error, 0 );
		if( pcp == PKC_ERROR )
			break;		/* continue is unlikely to work */

		if( fork() == 0 )  {
			/* 1st level child process */
			(void)close(netfd);	/* Child is not listener */

			/* Create 2nd level child process, "double detatch" */
			if( fork() == 0 )  {
				/* 2nd level child -- start work! */
				new_client( pcp );
				once_only = 1;
				main_loop();
				exit(0);
			} else {
				/* 1st level child -- vanish */
				exit(1);
			}
		} else {
			/* Parent: lingering server daemon */
			pkg_close(pcp);	/* Daemon is not the server */
			/* Collect status from 1st level child */
			(void)wait( &stat );
		}
	}
	exit(2);	/* ERROR exit */
}

/*
 *			M A I N _ L O O P
 *
 *  Loop forever handling clients as they come and go.
 *  Access to the framebuffer may be interleaved, if the user
 *  wants it that way.
 */
static void
main_loop(void)
{
	int	nopens = 0;
	int	ncloses = 0;

	while( !fb_server_got_fb_free ) {
		fd_set infds;
		struct timeval tv;
		register int	i;

		infds = select_list;	/* struct copy */

		tv.tv_sec = 60L;
		tv.tv_usec = 0L;
		if( (select( max_fd+1, &infds, (fd_set *)0, (fd_set *)0,
			     (void *)&tv )) == 0 ) {
			/* Process fb events while waiting for client */
			/*printf("select timeout waiting for client\n");*/
			if(fb_server_fbp) fb_poll(fb_server_fbp);
			continue;
		}
		/* Handle any events from the framebuffer */
		if (fb_server_fbp && fb_server_fbp->if_selfd > 0 && FD_ISSET(fb_server_fbp->if_selfd, &infds))
			fb_poll(fb_server_fbp);

		/* Accept any new client connections */
		if( netfd > 0 && FD_ISSET(netfd, &infds))  {
			new_client( pkg_getclient( netfd, fb_server_pkg_switch, comm_error, 0 ) );
			nopens++;
		}

		/* Process arrivals from existing clients */
		/* First, pull the data out of the kernel buffers */
		for( i = MAX_CLIENTS-1; i >= 0; i-- )  {
			if( clients[i] == NULL )  continue;
			if( pkg_process( clients[i] ) < 0 ) {
				fprintf(stderr,"pkg_process error encountered (1)\n");
			}
			if( ! FD_ISSET( clients[i]->pkc_fd, &infds ) )  continue;
			if( pkg_suckin( clients[i] ) <= 0 )  {
				/* Probably EOF */
				drop_client( i );
				ncloses++;
				continue;
			}
		}
		/* Second, process all the finished ones that we just got */
		for( i = MAX_CLIENTS-1; i >= 0; i-- )  {
			if( clients[i] == NULL )  continue;
			if( pkg_process( clients[i] ) < 0 ) {
				fprintf(stderr,"pkg_process error encountered (2)\n");
			}
		}
		if( once_only && nopens > 1 && ncloses > 1 )
			return;
	}
}

static void
init_syslog(void)
{
	use_syslog = 1;
#if defined(BSD) && !defined(CRAY2)
#   ifdef LOG_DAEMON
	openlog( "fbserv", LOG_PID|LOG_NOWAIT, LOG_DAEMON );	/* 4.3 style */
#   else
	openlog( "fbserv", LOG_PID );				/* 4.2 style */
#   endif
#endif /* BSD && !CRAY2 */
}

static void
setup_socket(int fd)
{
	int	on = 1;

#if defined(SO_KEEPALIVE)
	if( setsockopt( fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&on, sizeof(on)) < 0 ) {
#if		defined(BSD) && !defined(CRAY2)
		syslog( LOG_WARNING, "setsockopt (SO_KEEPALIVE): %s", strerror(errno) );
#		endif
	}
#endif
#if defined(SO_RCVBUF)
	/* try to set our buffers up larger */
	{
		int	m = 0;
		int	n = 0;
		int	val;
		int	size;

		for( size = 256; size > 16; size /= 2 )  {
			val = size * 1024;
			m = setsockopt( fd, SOL_SOCKET, SO_RCVBUF,
				(char *)&val, sizeof(val) );
			val = size * 1024;
			n = setsockopt( fd, SOL_SOCKET, SO_SNDBUF,
				(char *)&val, sizeof(val) );
			if( m >= 0 && n >= 0 )  break;
		}
		if( m < 0 || n < 0 )  perror("fbserv setsockopt()");
	}
#endif
}

/*
 *			C O M M _ E R R O R
 *
 *  Communication error.  An error occured on the PKG link.
 *  It may be local, or it may be between us and the client we are serving.
 *  We send a copy to syslog or stderr.
 *  Don't send one down the wire, this can cause loops.
 */
static void
comm_error(char *str)
{
#if defined(BSD) && !defined(CRAY2)
	if( use_syslog )
		syslog( LOG_ERR, str );
	else
		fprintf( stderr, "%s", str );
#else
	fprintf( stderr, "%s", str );
#endif
	if(verbose) fprintf( stderr, "%s", str );
}

/*
 *			F B _ L O G
 *
 *  Handles error or log messages from the frame buffer library.
 *  We route these back to all clients in an ERROR packet.  Note that
 *  this is a replacement for the default fb_log function in libfb
 *  (which just writes to stderr).
 *
 *  Log an FB library event, when _doprnt() is not available.
 *  This version should work on practically any machine, but
 *  it serves to highlight the the grossness of the varargs package
 *  requiring the size of a parameter to be known at compile time.
 */
#if defined(HAVE_STDARG_H)
void
fb_log( char *fmt, ... )
{
	va_list ap;
	char	outbuf[4096];			/* final output string */
	int	want;
	int	i;
	int	nsent = 0;

	va_start( ap, fmt );
	(void)vsprintf( outbuf, fmt, ap );
	va_end(ap);

	want = strlen(outbuf)+1;
	for( i = MAX_CLIENTS-1; i >= 0; i-- )  {
		if( clients[i] == NULL )  continue;
		if( pkg_send( MSG_ERROR, outbuf, want, clients[i] ) != want )  {
			comm_error("pkg_send error in fb_log, message was:\n");
			comm_error(outbuf);
		} else {
			nsent++;
		}
	}
	if( nsent == 0 || verbose )  {
		/* No PKG connection open yet! */
		fputs( outbuf, stderr );
		fflush(stderr);
	}
}

/* VARARGS */
#elif !defined(HAVE_STDARG_H) && defined(HAVE_VARARGS_H)

void
fb_log( va_alist )
va_dcl
{
	va_list ap;
	register char	*sp;			/* start pointer */
	register char	*ep;			/* end pointer */
	int	longify;
	char	fbuf[64];			/* % format buffer */
	char	nfmt[256];
	char	outbuf[4096];			/* final output string */
	char	*op;				/* output buf pointer */
	int	want, got;
	int	i;
	int	nsent = 0;

	/* prefix all messages with "hostname: " */
	gethostname( outbuf, sizeof(outbuf) );
	op = &outbuf[strlen(outbuf)];
	*op++ = ':';
	*op++ = ' ';

	va_start(ap);
	sp = va_arg(ap,char *);
	while( *sp )  {
		/* Initial state:  just printing chars */
		if( *sp != '%' )  {
			*op++ = *sp;
			if( *sp == '\n' && *(sp+1) ) {
				/* newline plus text, output hostname */
				gethostname( op, sizeof(outbuf) );
				op += strlen(op);
				*op++ = ':';
				*op++ = ' ';
			}
			sp++;
			continue;
		}

		/* Saw a percent sign, find end of fmt specifier */
		longify = 0;
		ep = sp+1;
		while( *ep )  {
			if( isalpha(*ep) )
				break;
			ep++;
		}

		/* Check for digraphs, eg "%ld" */
		if( *ep == 'l' )  {
			ep++;
			longify = 1;
		}

		/* Copy off the format string */
		{
			register int len;
			len = ep-sp+1;
			strncpy( fbuf, sp, len );
			fbuf[len] = '\0';
		}

		/* Grab parameter from arg list, and print it */
		switch( *ep )  {
		case 'e':
		case 'E':
		case 'f':
		case 'g':
		case 'G':
			/* All floating point ==> "double" */
			{
				register double d;
				d = va_arg(ap, double);
				sprintf( op, fbuf, d );
				op = &outbuf[strlen(outbuf)];
			}
			break;

		default:
			if( longify )  {
				register long ll;
				/* Long int */
				ll = va_arg(ap, long);
				sprintf( op, fbuf, ll );
				op = &outbuf[strlen(outbuf)];
			} else {
				register int i;
				/* Regular int */
				i = va_arg(ap, int);
				sprintf( op, fbuf, i );
				op = &outbuf[strlen(outbuf)];
			}
			break;
		}
		sp = ep+1;
	}
	va_end(ap);
	*op = NULL;


	want = strlen(outbuf)+1;
	for( i = MAX_CLIENTS-1; i >= 0; i-- )  {
		if( clients[i] == NULL )  continue;
		if( pkg_send( MSG_ERROR, outbuf, want, clients[i] ) != want )  {
			comm_error("pkg_send error in fb_log, message was:\n");
			comm_error(outbuf);
		} else {
			nsent++;
		}
	}
	if( nsent == 0 || verbose )  {
		/* No PKG connection open yet! */
		fputs( outbuf, stderr );
		fflush(stderr);
	}
}
#else

#error /* no stdarg and no vararg */

#endif /* !have_stdarg_h */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
