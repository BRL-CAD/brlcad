/*
 *			F B S E R V . C
 *
 *  Remote libfb server (formerly rfbd).
 *
 *  There are three ways this program can be run:
 *  Inetd Daemon - every PKG connection invokes a new copy of us,
 *	courtesy of inetd.  We process a single frame buffer
 *	open/process/close cycle and then exit.  A full installation
 *	includes setting up inetd and /etc/services to start one
 *	of these.
 *  Stand-Alone Daemon - once started we run forever, forking a
 *	copy of ourselves for each new connection.  Each child is
 *	essentially like above, i.e. one open/process/close cycle.
 *	Useful for running a daemon on a totally "unmodified" system,
 *	or when inetd is not available.
 *  Single-Frame-Buffer Server - we open a particular frame buffer
 *	at invocation time and leave it open.  We will accept
 *	multiple connections for this frame buffer, one at a time.
 *	Frame buffer open and close requests are effectively ignored.
 *	Major purpose is to create "reattachable" frame buffers when
 *	using libfb on a window system.  In this case there is no
 *	hardware to preserve "state" information (image data, color
 *	maps, etc.).  By leaving the frame buffer open, the daemon
 *	keeps this state in memory.
 *
 *  Authors -
 *	Phillip Dykstra
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>

#if defined(HAVE_STDARG_H)
# include <stdarg.h>
#endif
#if !defined(HAVE_STDARG_H) && defined(HAVE_VARARGS_H)
# include <varargs.h>
#endif

#include <sys/time.h>		/* For struct timeval */

#if defined(BSD) && !defined(CRAY2)
#	include <syslog.h>
#endif

#include <sys/socket.h>
#include <netinet/in.h>		/* For htonl(), etc */

#include "machine.h"
#include "externs.h"		/* For malloc, getopt */
#include "fb.h"
#include "pkg.h"

#include "../libfb/pkgtypes.h"
#include "./fbserv.h"

#define NET_LONG_LEN	4	/* # bytes to network long */

extern	int	_fb_disk_enable;

static	void	comm_error();
static	void	do1();
static	void	init_syslog();
static	int	use_syslog;	/* error messages to stderr if 0 */

static	struct	pkg_conn *rem_pcp;

static	char	*framebuffer = NULL;	/* frame buffer name */
static	int	width = 0;		/* use default size */
static	int	height = 0;
static	int	port = 0;
static	int	port_set;		/* !0 if user supplied port num */
static	FBIO	*fbp;
static	int	single_fb;	/* !0 => we are holding a reusable FB open */
static	int	got_fb_free;	/* !0 => we have received an fb_free */

/* Hidden args: -p<port_num> -F<frame_buffer> */
static char usage[] = "\
Usage: fbserv port_num\n\
          (for a stand-alone daemon)\n\
   or  fbserv [-h] [-S squaresize]\n\
          [-W width] [-N height] port_num frame_buffer\n\
          (for a single-frame-buffer server)\n\
";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "hF:s:w:n:S:W:N:p:" )) != EOF )  {
		switch( c )  {
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

#ifdef BSD
/*
 * Determine if a file descriptor corresponds to an open socket.
 * Used to detect when we are started from INETD which gives us an
 * open socket connection on fd 0.
 */
int
is_socket(fd)
int fd;
{
	struct sockaddr saddr;
	int namelen;

	if( getsockname(fd,&saddr,&namelen) == 0 )
		return	1;
	else
		return	0;
}
#endif /* BSD */

static void
sigalarm(code)
int	code;
{
	printf("alarm %s\n", fbp ? "FBP" : "NULL");
	if( fbp != FBIO_NULL )
		fb_poll(fbp);
	(void)signal( SIGALRM, sigalarm );	/* SYSV removes handler */
	alarm(1);
}

main( argc, argv )
int argc; char **argv;
{
	int	netfd;
	int	fbfd;
	int	maxfd;
	char	portname[32];


	/* No disk files on remote machine */
	_fb_disk_enable = 0;

	(void)signal( SIGPIPE, SIG_IGN );
	(void)signal( SIGALRM, sigalarm );
	/*alarm(1)*/

#ifdef BSD
	/*
	 * Inetd Daemon.
	 * Check to see if we were invoked by /etc/inetd.  If so
	 * we will have an open network socket on fd=0.  Become
	 * a Transient PKG server if this is so.
	 */
	netfd = 0;
	if( is_socket(netfd) ) {
		init_syslog();
		rem_pcp = pkg_transerver( pkg_switch, comm_error );
		do1();
		exit(0);
	}
#endif /* BSD */

	/* for now, make them set a port_num, for usage message */
	if ( !get_args( argc, argv ) || !port_set ) {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	/* Single-Frame-Buffer Server */
	if( framebuffer != NULL ) {
		single_fb = 1;	/* don't ever close the frame buffer */

		/* open a frame buffer */
		if( (fbp = fb_open(framebuffer, width, height)) == FBIO_NULL )
			exit(1);

		/* If zero, one isn't provided.  Don't want to select on stdin by accident. */
		fbfd = fbp->if_selfd;

		/* check/default port */
		if( port_set ) {
			if( port < 1024 )
				port += 5559;
		}
		sprintf(portname,"%d",port);

		/*
		 * Listen for PKG connections
		 */
		if( (netfd = pkg_permserver(portname, 0, 0, comm_error)) < 0 )
			exit(-1);

		maxfd = netfd;
		if (fbfd > maxfd)
			maxfd = fbfd;

		/* loop forever handling clients */
		while( !got_fb_free ) {
			fd_set infds;
			struct timeval tv;

			FD_ZERO(&infds);
			FD_SET(netfd, &infds);
			if (fbfd > 0)
				FD_SET(fbfd, &infds);
			tv.tv_sec = 1L;
			tv.tv_usec = 0L;
			if( (select( maxfd+1, &infds, (fd_set *)0, (fd_set *)0, 
				     &tv )) == 0 ) {
				/* Process fb events while waiting for client */
				/*printf("select timeout waiting for client\n");*/
				fb_poll(fbp);
				continue;
			}
			if (fbfd > 0 && FD_ISSET(fbfd, &infds))
				fb_poll(fbp);

			if (FD_ISSET(netfd, &infds))
			{
				rem_pcp = pkg_getclient( netfd, pkg_switch, comm_error, 0 );
				if( rem_pcp == PKC_ERROR )
					break;
				do1();
			}
		}
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
		sleep(5);
		continue;
		/*exit(1);*/
	}

	while(1) {
		int stat;
		rem_pcp = pkg_getclient( netfd, pkg_switch, comm_error, 0 );
		if( rem_pcp == PKC_ERROR )
			break;		/* continue is unlikely to work */

		if( fork() == 0 )  {
			/* 1st level child process */
			(void)close(netfd);	/* Child is not listener */

			/* Create 2nd level child process, "double detatch" */
			if( fork() == 0 )  {
				/* 2nd level child -- start work! */
				do1();
				exit(0);
			} else {
				/* 1st level child -- vanish */
				exit(1);
			}
		} else {
			/* Parent: lingering server daemon */
			pkg_close(rem_pcp);	/* Daemon is not the server */
			/* Collect status from 1st level child */
			(void)wait( &stat );
		}
	}
	exit(2);	/* ERROR exit */
}

static void
init_syslog()
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
do1()
{
	int	on = 1;

#if defined(SO_KEEPALIVE)
	if( setsockopt( rem_pcp->pkc_fd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0 ) {
#		ifndef CRAY2
		syslog( LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m" );
#		endif
	}
#endif
#if defined(SO_RCVBUF)
	/* try to set our buffers up larger */
	{
		int	n;
		int	val;
#define MAXVAL	(31*1024)
		val = MAXVAL;
		n = setsockopt( rem_pcp->pkc_fd, SOL_SOCKET,
			SO_RCVBUF, (char *)&val, sizeof(val) );
		if( n < 0 )  perror("setsockopt: SO_RCVBUF");
		val = MAXVAL;
		n = setsockopt( rem_pcp->pkc_fd, SOL_SOCKET,
			SO_SNDBUF, (char *)&val, sizeof(val) );
		if( n < 0 )  perror("setsockopt: SO_SNDBUF");
	}
#endif

	/*
	 * This used to be simply:
	 *	while( pkg_block(rem_pcp) > 0 ) ;
	 * but libfb event handling requires us to break it up.
	 */
	for(;;) {
		fd_set	infds;
		struct timeval tv;

		if( pkg_process( rem_pcp ) < 0 ) {
			printf("pkg_process error encountered\n");
			continue;
		}

		FD_ZERO( &infds );
		FD_SET( rem_pcp->pkc_fd, &infds );
		tv.tv_sec = 1L;
		tv.tv_usec = 0L;

		if( select( rem_pcp->pkc_fd+1, &infds, (fd_set *)0, 
			    (fd_set *)0, &tv ) != 0 ){
			if( pkg_suckin( rem_pcp ) <= 0 ) {
				/* Probably EOF */
				break;
			}
		} else {
			/* Timeout - check for fb events */
			/*printf("select timeout in do1()\n");*/
			if( fbp != FBIO_NULL )
				fb_poll(fbp);
			else
				break;
		}
#if 0
		if( pkg_process( rem_pcp ) < 0 ) {
			printf("pkg_process error encountered\n");
			continue;
		}
		/*do_other_stuff();*/
#endif
	}

	if( !single_fb && fbp != FBIO_NULL )
		fb_close(fbp);
	if( rem_pcp != PKC_NULL ) {
		pkg_close( rem_pcp );
		rem_pcp = PKC_NULL;	/* so we wont use fb_log() */
	}
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
comm_error( str )
char *str;
{
#if defined(BSD) && !defined(CRAY2)
	if( use_syslog )
		syslog( LOG_ERR, str );
	else
		fprintf( stderr, "%s", str );
#else
	fprintf( stderr, "%s", str );
#endif
}

/*
 * This is where we go for message types we don't understand.
 */
void
pkgfoo(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	fb_log( "fbserv: unable to handle message type %d\n",
		pcp->pkc_type );
	(void)free(buf);
}

/******** Here's where the hooks lead *********/

void
rfbopen(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	height, width;
	char	rbuf[5*NET_LONG_LEN+1];
	int	want;

	width = pkg_glong( &buf[0*NET_LONG_LEN] );
	height = pkg_glong( &buf[1*NET_LONG_LEN] );

	/*printf("fbserv: I have been asked to open \"%s\"\n", &buf[8]);*/
	if( fbp == FBIO_NULL ) {
		if( strlen(&buf[8]) == 0 )
			fbp = fb_open( NULL, width, height );
		else
			fbp = fb_open( &buf[8], width, height );
	}

#if 0
	{	char s[81];
sprintf( s, "Device: \"%s\"", &buf[8] );
fb_log(s);
	}
#endif
	if( fbp == FBIO_NULL )  {
		(void)pkg_plong( &rbuf[0*NET_LONG_LEN], -1 );	/* ret */
		(void)pkg_plong( &rbuf[1*NET_LONG_LEN], 0 );
		(void)pkg_plong( &rbuf[2*NET_LONG_LEN], 0 );
		(void)pkg_plong( &rbuf[3*NET_LONG_LEN], 0 );
		(void)pkg_plong( &rbuf[4*NET_LONG_LEN], 0 );
	} else {
		(void)pkg_plong( &rbuf[0*NET_LONG_LEN], 0 );	/* ret */
		(void)pkg_plong( &rbuf[1*NET_LONG_LEN], fbp->if_max_width );
		(void)pkg_plong( &rbuf[2*NET_LONG_LEN], fbp->if_max_height );
		(void)pkg_plong( &rbuf[3*NET_LONG_LEN], fbp->if_width );
		(void)pkg_plong( &rbuf[4*NET_LONG_LEN], fbp->if_height );
	}

	want = 5*NET_LONG_LEN;
	if( pkg_send( MSG_RETURN, rbuf, want, pcp ) != want )
		comm_error("pkg_send fb_open reply\n");
	if( buf ) (void)free(buf);
}

void
rfbclose(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	char	rbuf[NET_LONG_LEN+1];
	
	if( single_fb ) {
		/*
		 * We are playing FB server so we don't really close the
		 * frame buffer.  We should flush output however.
		 */
		(void)fb_flush( fbp );
		(void)pkg_plong( &rbuf[0], 0 );		/* return success */
	} else {
		(void)pkg_plong( &rbuf[0], fb_close( fbp ) );
		fbp = FBIO_NULL;
	}
	/* Don't check for errors, SGI linger mode or other events
	 * may have already closed down all the file descriptors.
	 * If communication has broken, other end will know we are gone.
	 */
	(void)pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

void
rfbfree(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	char	rbuf[NET_LONG_LEN+1];
	
	(void)pkg_plong( &rbuf[0], fb_free( fbp ) );
	fbp = FBIO_NULL;
	if( pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp ) != NET_LONG_LEN )
		comm_error("pkg_send fb_free reply\n");
	if( buf ) (void)free(buf);

	got_fb_free = 1;
}

void
rfbclear(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	RGBpixel bg;
	char	rbuf[NET_LONG_LEN+1];

	bg[RED] = buf[0];
	bg[GRN] = buf[1];
	bg[BLU] = buf[2];

	(void)pkg_plong( rbuf, fb_clear( fbp, bg ) );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

void
rfbread(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	x, y, num;
	int	ret;
	static unsigned char	*scanbuf = NULL;
	static int	buflen = 0;

	x = pkg_glong( &buf[0*NET_LONG_LEN] );
	y = pkg_glong( &buf[1*NET_LONG_LEN] );
	num = pkg_glong( &buf[2*NET_LONG_LEN] );

	if( num*sizeof(RGBpixel) > buflen ) {
		if( scanbuf != NULL )
			free( (char *)scanbuf );
		buflen = num*sizeof(RGBpixel);
		if( buflen < 1024*sizeof(RGBpixel) )
			buflen = 1024*sizeof(RGBpixel);
		if( (scanbuf = (unsigned char *)malloc( buflen )) == NULL ) {
			fb_log("fb_read: malloc failed!");
			if( buf ) (void)free(buf);
			buflen = 0;
			return;
		}
	}

	ret = fb_read( fbp, x, y, scanbuf, num );
	if( ret < 0 )  ret = 0;		/* map error indications */
	/* sending a 0-length package indicates error */
	pkg_send( MSG_RETURN, scanbuf, ret*sizeof(RGBpixel), pcp );
	if( buf ) (void)free(buf);
}

void
rfbwrite(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	x, y, num;
	char	rbuf[NET_LONG_LEN+1];
	int	ret;
	int	type;

	x = pkg_glong( &buf[0*NET_LONG_LEN] );
	y = pkg_glong( &buf[1*NET_LONG_LEN] );
	num = pkg_glong( &buf[2*NET_LONG_LEN] );
	type = pcp->pkc_type;
	ret = fb_write( fbp, x, y, (unsigned char *)&buf[3*NET_LONG_LEN], num );

	if( type < MSG_NORETURN ) {
		(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
		pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	}
	if( buf ) (void)free(buf);
}

/*
 *			R F B R E A D R E C T
 */
void
rfbreadrect(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	xmin, ymin;
	int	width, height;
	int	num;
	int	ret;
	static unsigned char	*scanbuf = NULL;
	static int	buflen = 0;

	xmin = pkg_glong( &buf[0*NET_LONG_LEN] );
	ymin = pkg_glong( &buf[1*NET_LONG_LEN] );
	width = pkg_glong( &buf[2*NET_LONG_LEN] );
	height = pkg_glong( &buf[3*NET_LONG_LEN] );
	num = width * height;

	if( num*sizeof(RGBpixel) > buflen ) {
		if( scanbuf != NULL )
			free( (char *)scanbuf );
		buflen = num*sizeof(RGBpixel);
		if( buflen < 1024*sizeof(RGBpixel) )
			buflen = 1024*sizeof(RGBpixel);
		if( (scanbuf = (unsigned char *)malloc( buflen )) == NULL ) {
			fb_log("fb_read: malloc failed!");
			if( buf ) (void)free(buf);
			buflen = 0;
			return;
		}
	}

	ret = fb_readrect( fbp, xmin, ymin, width, height, scanbuf );
	if( ret < 0 )  ret = 0;		/* map error indications */
	/* sending a 0-length package indicates error */
	pkg_send( MSG_RETURN, scanbuf, ret*sizeof(RGBpixel), pcp );
	if( buf ) (void)free(buf);
}

/*
 *			R F B W R I T E R E C T
 */
void
rfbwriterect(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	x, y;
	int	width, height;
	char	rbuf[NET_LONG_LEN+1];
	int	ret;
	int	type;

	x = pkg_glong( &buf[0*NET_LONG_LEN] );
	y = pkg_glong( &buf[1*NET_LONG_LEN] );
	width = pkg_glong( &buf[2*NET_LONG_LEN] );
	height = pkg_glong( &buf[3*NET_LONG_LEN] );

	type = pcp->pkc_type;
	ret = fb_writerect( fbp, x, y, width, height,
		(unsigned char *)&buf[4*NET_LONG_LEN] );

	if( type < MSG_NORETURN ) {
		(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
		pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	}
	if( buf ) (void)free(buf);
}

void
rfbcursor(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	mode, x, y;
	char	rbuf[NET_LONG_LEN+1];

	mode = pkg_glong( &buf[0*NET_LONG_LEN] );
	x = pkg_glong( &buf[1*NET_LONG_LEN] );
	y = pkg_glong( &buf[2*NET_LONG_LEN] );

	(void)pkg_plong( &rbuf[0], fb_cursor( fbp, mode, x, y ) );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

void
rfbgetcursor(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	ret;
	int	mode, x, y;
	char	rbuf[4*NET_LONG_LEN+1];

	ret = fb_getcursor( fbp, &mode, &x, &y );
	(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
	(void)pkg_plong( &rbuf[1*NET_LONG_LEN], mode );
	(void)pkg_plong( &rbuf[2*NET_LONG_LEN], x );
	(void)pkg_plong( &rbuf[3*NET_LONG_LEN], y );
	pkg_send( MSG_RETURN, rbuf, 4*NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

void
rfbsetcursor(pcp, buf)
struct pkg_conn *pcp;
char		*buf;
{
	char	rbuf[NET_LONG_LEN+1];
	int	ret;
	int	xbits, ybits;
	int	xorig, yorig;

	xbits = pkg_glong( &buf[0*NET_LONG_LEN] );
	ybits = pkg_glong( &buf[1*NET_LONG_LEN] );
	xorig = pkg_glong( &buf[2*NET_LONG_LEN] );
	yorig = pkg_glong( &buf[3*NET_LONG_LEN] );

	ret = fb_setcursor( fbp, (unsigned char *)&buf[4*NET_LONG_LEN],
		xbits, ybits, xorig, yorig );

	if( pcp->pkc_type < MSG_NORETURN ) {
		(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
		pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	}
	if( buf ) (void)free(buf);
}

/*OLD*/
void
rfbscursor(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	mode, x, y;
	char	rbuf[NET_LONG_LEN+1];

	mode = pkg_glong( &buf[0*NET_LONG_LEN] );
	x = pkg_glong( &buf[1*NET_LONG_LEN] );
	y = pkg_glong( &buf[2*NET_LONG_LEN] );

	(void)pkg_plong( &rbuf[0], fb_scursor( fbp, mode, x, y ) );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

/*OLD*/
void
rfbwindow(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	x, y;
	char	rbuf[NET_LONG_LEN+1];

	x = pkg_glong( &buf[0*NET_LONG_LEN] );
	y = pkg_glong( &buf[1*NET_LONG_LEN] );

	(void)pkg_plong( &rbuf[0], fb_window( fbp, x, y ) );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

/*OLD*/
void
rfbzoom(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	x, y;
	char	rbuf[NET_LONG_LEN+1];

	x = pkg_glong( &buf[0*NET_LONG_LEN] );
	y = pkg_glong( &buf[1*NET_LONG_LEN] );

	(void)pkg_plong( &rbuf[0], fb_zoom( fbp, x, y ) );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

void
rfbview(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	ret;
	int	xcenter, ycenter, xzoom, yzoom;
	char	rbuf[NET_LONG_LEN+1];

	xcenter = pkg_glong( &buf[0*NET_LONG_LEN] );
	ycenter = pkg_glong( &buf[1*NET_LONG_LEN] );
	xzoom = pkg_glong( &buf[2*NET_LONG_LEN] );
	yzoom = pkg_glong( &buf[3*NET_LONG_LEN] );

	ret = fb_view( fbp, xcenter, ycenter, xzoom, yzoom );
	(void)pkg_plong( &rbuf[0], ret );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

void
rfbgetview(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	ret;
	int	xcenter, ycenter, xzoom, yzoom;
	char	rbuf[5*NET_LONG_LEN+1];

	ret = fb_getview( fbp, &xcenter, &ycenter, &xzoom, &yzoom );
	(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
	(void)pkg_plong( &rbuf[1*NET_LONG_LEN], xcenter );
	(void)pkg_plong( &rbuf[2*NET_LONG_LEN], ycenter );
	(void)pkg_plong( &rbuf[3*NET_LONG_LEN], xzoom );
	(void)pkg_plong( &rbuf[4*NET_LONG_LEN], yzoom );
	pkg_send( MSG_RETURN, rbuf, 5*NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

void
rfbrmap(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	register int	i;
	char	rbuf[NET_LONG_LEN+1];
	ColorMap map;
	char	cm[256*2*3];

	(void)pkg_plong( &rbuf[0*NET_LONG_LEN], fb_rmap( fbp, &map ) );
	for( i = 0; i < 256; i++ ) {
		(void)pkg_pshort( cm+2*(0+i), map.cm_red[i] );
		(void)pkg_pshort( cm+2*(256+i), map.cm_green[i] );
		(void)pkg_pshort( cm+2*(512+i), map.cm_blue[i] );
	}
	pkg_send( MSG_DATA, cm, sizeof(cm), pcp );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

/*
 *			R F B W M A P
 *
 *  Accept a color map sent by the client, and write it to the framebuffer.
 *  Network format is to send each entry as a network (IBM) order 2-byte
 *  short, 256 red shorts, followed by 256 green and 256 blue, for a total
 *  of 3*256*2 bytes.
 */
void
rfbwmap(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	i;
	char	rbuf[NET_LONG_LEN+1];
	long	ret;
	ColorMap map;

	if( pcp->pkc_len == 0 )
		ret = fb_wmap( fbp, COLORMAP_NULL );
	else {
		for( i = 0; i < 256; i++ ) {
			map.cm_red[i] = pkg_gshort( buf+2*(0+i) );
			map.cm_green[i] = pkg_gshort( buf+2*(256+i) );
			map.cm_blue[i] = pkg_gshort( buf+2*(512+i) );
		}
		ret = fb_wmap( fbp, &map );
	}
	(void)pkg_plong( &rbuf[0], ret );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

void
rfbflush(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	ret;
	char	rbuf[NET_LONG_LEN+1];

	ret = fb_flush( fbp );

	if( pcp->pkc_type < MSG_NORETURN ) {
		(void)pkg_plong( rbuf, ret );
		pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	}
	if( buf ) (void)free(buf);
}

void
rfbpoll(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	(void)fb_poll( fbp );
	if( buf ) (void)free(buf);
}

/*
 *  At one time at least we couldn't send a zero length PKG
 *  message back and forth, so we receive a dummy long here.
 */
void
rfbhelp(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	long	ret;
	char	rbuf[NET_LONG_LEN+1];

	(void)pkg_glong( &buf[0*NET_LONG_LEN] );

	ret = fb_help(fbp);
	(void)pkg_plong( &rbuf[0], ret );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

/**************************************************************/
/*
 *			F B _ L O G
 *
 *  Handles error or log messages from the frame buffer library.
 *  We route these back to our client in an ERROR packet.  Note that
 *  this is a replacement for the default fb_log function in libfb
 *  (which just writes to stderr).
 *
 *  Log an FB library event, when _doprnt() is not available.
 *  This version should work on practically any machine, but
 *  it serves to highlight the the grossness of the varargs package
 *  requiring the size of a parameter to be known at compile time.
 */
#if __STDC__
void
fb_log( char *fmt, ... )
{
	va_list ap;
	char	outbuf[4096];			/* final output string */
	int	want;

	va_start( ap, fmt );
	(void)vsprintf( outbuf, fmt, ap );
	va_end(ap);

	if( rem_pcp == PKC_NULL ) {
		/* PKG connection not open yet! */
		printf("%s", outbuf);
		fflush(stdout);
		return;
	}
	want = strlen(outbuf)+1;
	if( pkg_send( MSG_ERROR, outbuf, want, rem_pcp ) != want )  {
		comm_error("pkg_send error in fb_log, message was:\n");
		comm_error(outbuf);
	}
}
#else
/* VARARGS */
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
	if( rem_pcp == PKC_NULL ) {
		/* PKG connection not open yet! */
		printf("%s", outbuf);
		fflush(stdout);
		return;
	}
	want = strlen(outbuf)+1;
	if( (got = pkg_send( MSG_ERROR, outbuf, want, rem_pcp )) != want )  {
		comm_error("pkg_send error in fb_log, message was:\n");
		comm_error(outbuf);
	}
	return;
}
#endif /* !__STDC__ */
