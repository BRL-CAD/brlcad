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
 *			F B _ L O G
 *
 *  Errors from the framebuffer library.  We route these back to
 *  our client in an ERROR packet.  Note that this is a replacement
 *  for the default fb_log function in libfb (which just writes
 *  to stderr).
 */
/* VARARGS */
void
#ifdef cray
/* Segloader can't handle the multiple defines */
rfbd_log( va_alist )
#else
fb_log( va_alist )
#endif
va_dcl
{
	va_list	ap;
	char *fmt;
	int a=0,b=0,c=0,d=0,e=0,f=0,g=0,h=0,i=0;
	int cnt=0;
	register char *cp;
	static char errbuf[80];

	va_start( ap );
	fmt = va_arg(ap,char *);
	cp = fmt;
	while( *cp )  if( *cp++ == '%' )  {
		cnt++;
		/* WARNING:  This assumes no fancy format specs like %.1f */
		switch( *cp )  {
		case 'e':
		case 'f':
		case 'g':
			cnt++;		/* Doubles are bigger! */
		}
	}
	if( cnt > 0 )  {
		a = va_arg(ap,int);
	}
	if( cnt > 1 )  {
		b = va_arg(ap,int);
	}
	if( cnt > 2 )  {
		c = va_arg(ap,int);
	}
	if( cnt > 3 )  {
		d = va_arg(ap,int);
	}
	if( cnt > 4 )  {
		e = va_arg(ap,int);
	}
	if( cnt > 5 )  {
		f = va_arg(ap,int);
	}
	if( cnt > 6 )  {
		g = va_arg(ap,int);
	}
	if( cnt > 7 )  {
		h = va_arg(ap,int);
	}
	if( cnt > 8 )  {
		i = va_arg(ap,int);
	}
	if( cnt > 9 )  {
		a = (int)fmt;
		fmt = "Max args exceeded on: %s\n";
	}
	va_end( ap );
	
	(void) sprintf( errbuf, fmt, a,b,c,d,e,f,g,h,i );
	pkg_send( MSG_ERROR, errbuf, strlen(errbuf)+1, rem_pcp );
	return;
}

/*
 * This is where we go for message types we don't understand.
 */
void
pkgfoo(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
#ifdef cray
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
	long	ret;
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
#ifdef cray
rfbd_log(s);
#else
fb_log(s);
#endif
	}
#endif
	ret = fbp == FBIO_NULL ? -1 : 0;
	(void)pkg_plong( &rbuf[0*NET_LONG_LEN], ret );
	(void)pkg_plong( &rbuf[1*NET_LONG_LEN], fbp->if_max_width );
	(void)pkg_plong( &rbuf[2*NET_LONG_LEN], fbp->if_max_height );
	(void)pkg_plong( &rbuf[3*NET_LONG_LEN], fbp->if_width );
	(void)pkg_plong( &rbuf[4*NET_LONG_LEN], fbp->if_height );

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
#ifdef cray
			rfbd_log("fb_read: malloc failed!");
#else
			fb_log("fb_read: malloc failed!");
#endif cray
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
