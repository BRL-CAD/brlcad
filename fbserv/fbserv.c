/*
 *		R F B D . C
 *
 *  Remote libfb server.
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

#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <varargs.h>

#ifdef BSD
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>		/* For htonl(), etc */
#endif

#include "fb.h"
#include "pkg.h"

#include "../libfb/pkgtypes.h"
#include "./rfbd.h"

#define NET_LONG_LEN	4	/* # bytes to network long */

extern	char	*malloc();
extern	int	_disk_enable;

static	void	comm_error();
void		fb_log();
#ifdef CRAY
void		rfbd_log();
#endif

static	struct	pkg_conn *rem_pcp;
static	FBIO	*fbp;

main( argc, argv )
int argc; char **argv;
{
	int	on = 1;
	int	netfd;

	/* No disk files on remote machine */
	_disk_enable = 0;

	(void)signal( SIGPIPE, SIG_IGN );

#ifdef BSD
	/* Transient server, started by /etc/inetd, network fd=0 */
	netfd = 0;
	rem_pcp = pkg_transerver( pkg_switch, comm_error );
#else
	while(1)  {
		int stat;
		/*
		 * Listen for PKG connections, no /etc/inetd
		 */
		if( (netfd = pkg_permserver("remotefb", 0, 0, comm_error)) < 0 )
			continue;
		rem_pcp = pkg_getclient( netfd, pkg_switch, comm_error, 0 );
		if( rem_pcp == PKC_ERROR )
			continue;
		if( fork() == 0 )  {
			/* Child */
			if( fork() == 0 )  {
				/* 2nd level child -- start work! */
				break;
			} else {
				/* 1st level child -- vanish */
				exit(1);
			}
		} else {
			/* Original server daemon */
			(void)close(netfd);
			(void)wait( &stat );
		}
	}
#endif

#ifdef BSD
#ifdef LOG_DAEMON
	openlog( "rfbd", LOG_PID|LOG_NOWAIT, LOG_DAEMON );	/* 4.3 style */
#else
	openlog( "rfbd", LOG_PID );				/* 4.2 style */
#endif
	if( setsockopt( netfd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0 ) {
		openlog( argv[0], LOG_PID, 0 );
		syslog( LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m" );
	}
#endif
#if BSD >= 43
	{
		int	n;
		int	val = 32767;
		n = setsockopt( rem_pcp->pkc_fd, SOL_SOCKET,
			SO_RCVBUF, (char *)&val, sizeof(val) );
		if( n < 0 )  perror("setsockopt: SO_RCVBUF");
	}
#endif

	while( pkg_block(rem_pcp) > 0 )
		;

	if( fbp != FBIO_NULL )
		fb_close(fbp);
	exit(0);
}

/*
 *			C O M M _ E R R O R
 *
 *  Communication error.  An error occured on the PKG link.
 *  It may be local, or it may be between us and the client we are serving.
 *  We send a copy down the wire, and another to syslog.
 */
static void
comm_error( str )
char *str;
{
#ifdef BSD
	syslog( LOG_ERR, str );
#else
	fprintf( stderr, "%s\n", str );
#endif
/**	(void)pkg_send( MSG_ERROR, str, strlen(str)+1, rem_pcp ); **/
}

/*
 * This is where we go for message types we don't understand.
 */
void
pkgfoo(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
#ifdef CRAY
	rfbd_log( "rfbd: unable to handle message type %d\n",
		pcp->pkc_type );
#else
	fb_log( "rfbd: unable to handle message type %d\n",
		pcp->pkc_type );
#endif
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

	width = pkg_glong( &buf[0*NET_LONG_LEN] );
	height = pkg_glong( &buf[1*NET_LONG_LEN] );

	if( strlen(&buf[8]) == 0 )
		fbp = fb_open( NULL, width, height );
	else
		fbp = fb_open( &buf[8], width, height );

#if 0
	{	char s[81];
sprintf( s, "Device: \"%s\"", &buf[8] );
#ifdef CRAY
rfbd_log(s);
#else
fb_log(s);
#endif
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

	pkg_send( MSG_RETURN, rbuf, 5*NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
}

void
rfbclose(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	char	rbuf[NET_LONG_LEN+1];

	(void)pkg_plong( &rbuf[0], fb_close( fbp ) );
	pkg_send( MSG_RETURN, rbuf, NET_LONG_LEN, pcp );
	if( buf ) (void)free(buf);
	fbp = FBIO_NULL;
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
	static char	*scanbuf = NULL;
	static int	buflen = 0;

	x = pkg_glong( &buf[0*NET_LONG_LEN] );
	y = pkg_glong( &buf[1*NET_LONG_LEN] );
	num = pkg_glong( &buf[2*NET_LONG_LEN] );

	if( num*sizeof(RGBpixel) > buflen ) {
		if( scanbuf != NULL )
			free( scanbuf );
		buflen = num*sizeof(RGBpixel);
		if( buflen < 1024*sizeof(RGBpixel) )
			buflen = 1024*sizeof(RGBpixel);
		if( (scanbuf = malloc( buflen )) == NULL ) {
#ifdef CRAY
			rfbd_log("fb_read: malloc failed!");
#else
			fb_log("fb_read: malloc failed!");
#endif
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
	ret = fb_write( fbp, x, y, (RGBpixel *)&buf[3*NET_LONG_LEN], num );

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

/*
 *  At one time at least we couldn't send a zero length PKG
 *  message back and forth, so we receive a dummy long here.
 */
void
rfbhelp(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	x;	/* dummy */
	long	ret;
	char	rbuf[NET_LONG_LEN+1];

	x = pkg_glong( &buf[0*NET_LONG_LEN] );

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
/* VARARGS */
void
#ifdef CRAY
/* Segldr can't handle the multiple defines of fb_log here & in libfb */
rfbd_log( va_alist )
#else
fb_log( va_alist )
#endif
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
	pkg_send( MSG_ERROR, outbuf, strlen(outbuf)+1, rem_pcp );
	return;
}
