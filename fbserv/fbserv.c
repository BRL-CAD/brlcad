/*
 *		R F B D . C
 *
 *  Remote libfb server.
 *
 *  Author -
 *	Phillip Dykstra
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

extern	char	*malloc();
extern	double	atof();
extern	int	_disk_enable;

static	void	comm_error();

static	struct	pkg_conn *pcp;
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
	/* Started by /etc/inetd, network fd=0 */
	netfd = 0;
	pcp = pkg_makeconn( netfd, pkg_switch, comm_error );
#else
	while(1)  {
		int stat;
		/*
		 * Listen for PKG connections, no /etc/inetd
		 */
		if( (netfd = pkg_initserver("mfb", 0, comm_error)) < 0 )
			continue;
		pcp = pkg_getclient( netfd, pkg_switch, comm_error, 0 );
		if( pcp == PKC_ERROR )
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
	if( setsockopt( netfd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0 ) {
		openlog( argv[0], LOG_PID, 0 );
		syslog( LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m" );
	}
#endif

	while( pkg_block(pcp) > 0 )
		;

	exit(0);
}

/*
 *			C O M M _ E R R O R
 *
 *  Communication error.  An error occured on the PKG link between us
 *  and the client we are serving.  We can't safely route these down
 *  that connection so for now we just blat to stderr.
 *  Should use syslog.
 */
static void
comm_error( str )
char *str;
{
	(void)fprintf( stderr, "%s\n", str );
}

/*
 *			F B _ L O G
 *
 *  Errors from the framebuffer library.  We route these back to
 *  our client.
 */
/* VARARGS */
void
fb_log( va_alist )
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
	pkg_send( MSG_ERROR, errbuf, strlen(errbuf)+1, pcp );
	return;
}

/*
 * This is where we go for message types we don't understand.
 */
int
pkgfoo(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	fb_log( "rfbd: unable to handle message type %d\n",
		pcp->pkc_type );
	(void)free(buf);
}

/******** Here's where the hooks lead *********/

rfbopen(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	height, width;
	long	ret;
	char	rbuf[5*4+1];

	width = fbgetlong( &buf[0*4] );
	height = fbgetlong( &buf[1*4] );

	if( strlen(&buf[8]) == 0 )
		fbp = fb_open( NULL, width, height );
	else
		fbp = fb_open( &buf[8], width, height );

#if 0
	{	char s[81];
sprintf( s, "Device: \"%s\"", &buf[8] );
fb_log(s);
	}
#endif
	ret = fbp == FBIO_NULL ? -1 : 0;
	(void)fbputlong( ret, &rbuf[0*4] );
	(void)fbputlong( fbp->if_max_width, &rbuf[1*4] );
	(void)fbputlong( fbp->if_max_height, &rbuf[2*4] );
	(void)fbputlong( fbp->if_width, &rbuf[3*4] );
	(void)fbputlong( fbp->if_height, &rbuf[4*4] );

	pkg_send( MSG_RETURN, rbuf, 5*4, pcp );
	if( buf ) (void)free(buf);
}

rfbclose(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	char	rbuf[4+1];

	(void)fbputlong( fb_close( fbp ), &rbuf[0] );
	pkg_send( MSG_RETURN, rbuf, 4, pcp );
	if( buf ) (void)free(buf);
	fbp = FBIO_NULL;
}

rfbclear(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	Pixel	bg;
	char	rbuf[4+1];

	bg.red = buf[0];
	bg.green = buf[1];
	bg.blue = buf[2];
	bg.spare = 0;
	(void)fbputlong( fb_clear( fbp, &bg ), rbuf );
	pkg_send( MSG_RETURN, rbuf, 4, pcp );
	if( buf ) (void)free(buf);
}

rfbread(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	x, y, num;
	int	ret;
	char	rbuf[4+1];
	static char	*scanbuf = NULL;
	static int	buflen = 0;

	x = fbgetlong( &buf[0*4] );
	y = fbgetlong( &buf[1*4] );
	num = fbgetlong( &buf[2*4] );

	if( num > buflen ) {
		if( scanbuf != NULL )
			free( scanbuf );
		buflen = num*sizeof(Pixel);
		if( buflen < 1024*sizeof(Pixel) )
			buflen = 1024*sizeof(Pixel);
		if( (scanbuf = malloc( buflen )) == NULL ) {
			fb_log("fb_read: malloc failed!");
			if( buf ) (void)free(buf);
			buflen = 0;
			return;
		}
	}

	ret = fb_read( fbp, x, y, scanbuf, num );
	(void)fbputlong( ret, &rbuf[0] );
	pkg_send( MSG_RETURN, rbuf, 4, pcp );

	/* Send back the data */
	if( ret >= 0 )
		pkg_send( MSG_DATA, scanbuf, num*sizeof(Pixel), pcp );
	if( buf ) (void)free(buf);
}

rfbwrite(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	x, y, num;
	char	rbuf[4+1];
	int	type;

	x = fbgetlong( &buf[0*4] );
	y = fbgetlong( &buf[1*4] );
	num = fbgetlong( &buf[2*4] );
	type = pcp->pkc_type;
	ret = fb_write( fbp, x, y, &buf[3*4], num );

	if( type < MSG_NORETURN ) {
		(void)fbputlong( ret, &rbuf[0] );
		pkg_send( MSG_RETURN, rbuf, 4, pcp );
	}
	if( buf ) (void)free(buf);
}

rfbcursor(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	mode, x, y;
	char	rbuf[4+1];

	mode = fbgetlong( &buf[0*4] );
	x = fbgetlong( &buf[1*4] );
	y = fbgetlong( &buf[2*4] );

	(void)fbputlong( fb_cursor( fbp, mode, x, y ), &rbuf[0] );
	pkg_send( MSG_RETURN, rbuf, 4, pcp );
	if( buf ) (void)free(buf);
}

rfbwindow(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	x, y;
	char	rbuf[4+1];

	x = fbgetlong( &buf[0*4] );
	y = fbgetlong( &buf[1*4] );

	(void)fbputlong( fb_window( fbp, x, y ), &rbuf[0] );
	pkg_send( MSG_RETURN, rbuf, 4, pcp );
	if( buf ) (void)free(buf);
}

rfbzoom(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	x, y;
	char	rbuf[4+1];

	x = fbgetlong( &buf[0*4] );
	y = fbgetlong( &buf[1*4] );

	(void)fbputlong( fb_zoom( fbp, x, y ), &rbuf[0] );
	pkg_send( MSG_RETURN, rbuf, 4, pcp );
	if( buf ) (void)free(buf);
}

/* XXX - need to convert from network shorts! */
rfbrmap(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	char	rbuf[4+1];
	ColorMap map;

	(void)fbputlong( fb_rmap( fbp, &map ), &rbuf[0] );
	pkg_send( MSG_DATA, &map, sizeof(map), pcp );
	pkg_send( MSG_RETURN, rbuf, 4, pcp );
	if( buf ) (void)free(buf);
}

rfbwmap(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	char	rbuf[4+1];
	long	ret;

	if( pcp->pkc_len == 0 )
		ret = fb_wmap( fbp, COLORMAP_NULL );
	else
		ret = fb_wmap( fbp, buf );

	(void)fbputlong( ret, &rbuf[0] );
	pkg_send( MSG_RETURN, rbuf, 4, pcp );
	if( buf ) (void)free(buf);
}
