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
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>		/* For htonl(), etc */
#include <syslog.h>
#include <sys/uio.h>		/* for struct iovec */
#include <varargs.h>

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

#ifdef NEVER
	/*
	 * Listen for PKG connections.
	 * This is what we would do if we weren't being started
	 * by inetd (plus some other code that's been removed...).
	 */
	if( (netfd = pkg_initserver("rlibfb", 0)) < 0 )
		exit(1);
#endif
	netfd = 0;
	if( setsockopt( netfd, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0 ) {
		openlog( argv[0], LOG_PID, 0 );
		syslog( LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m" );
	}
	(void)signal( SIGPIPE, SIG_IGN );

	pcp = pkg_makeconn( netfd, pkg_switch, comm_error );

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
	long	rbuf[5];

	width = ntohl( *(long *)(&buf[0]) );
	height = ntohl( *(long *)(&buf[4]) );

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
	rbuf[0] = htonl( ret );
	rbuf[1] = htonl( fbp->if_max_width );
	rbuf[2] = htonl( fbp->if_max_height );
	rbuf[3] = htonl( fbp->if_width );
	rbuf[4] = htonl( fbp->if_height );
	pkg_send( MSG_RETURN, &rbuf[0], 20, pcp );
	if( buf ) (void)free(buf);
}

rfbclose(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	long	ret;

	ret = fb_close( fbp );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, pcp );
	if( buf ) (void)free(buf);
	fbp = FBIO_NULL;
}

rfbclear(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	Pixel	bg;
	long	ret;

	bg.red = buf[0];
	bg.green = buf[1];
	bg.blue = buf[2];
	bg.spare = 0;
	ret = fb_clear( fbp, &bg );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, pcp );
	if( buf ) (void)free(buf);
}

rfbread(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	x, y, num;
	long	ret;
	static char	*rbuf = NULL;
	static int	buflen = 0;

	x =  ntohl( *(long *)(&buf[0]) );
	y =  ntohl( *(long *)(&buf[4]) );
	num =  ntohl( *(long *)(&buf[8]) );

	if( num > buflen ) {
		if( rbuf != NULL )
			free( rbuf );
		buflen = num*sizeof(Pixel);
		if( buflen < 1024*sizeof(Pixel) )
			buflen = 1024*sizeof(Pixel);
		if( (rbuf = malloc( buflen )) == NULL ) {
			fb_log("fb_read: malloc failed!");
			if( buf ) (void)free(buf);
			buflen = 0;
			return;
		}
	}

	ret = fb_read( fbp, x, y, rbuf, num );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, pcp );

	/* Send back the data */
	if( ret >= 0 )
		pkg_send( MSG_DATA, rbuf, num*sizeof(Pixel), pcp );
	if( buf ) (void)free(buf);
}

rfbwrite(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	x, y, num;
	long	ret;
	char	*datap;
	int	type;

	x =  ntohl( *(long *)(&buf[0]) );
	y =  ntohl( *(long *)(&buf[4]) );
	num =  ntohl( *(long *)(&buf[8]) );
	type = pcp->pkc_type;

	/* Get space, fetch data into it, do write, free space. */
	datap = malloc( num*4 );
	pkg_waitfor( MSG_DATA, datap, num*4, pcp );
	ret = fb_write( fbp, x, y, datap, num );

	if( type < MSG_NORETURN ) {
		ret = htonl( ret );
		pkg_send( MSG_RETURN, &ret, 4, pcp );
	}
	free( datap );
	if( buf ) (void)free(buf);
}

rfbcursor(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	mode, x, y;
	long	ret;

	mode =  ntohl( *(long *)(&buf[0]) );
	x =  ntohl( *(long *)(&buf[4]) );
	y =  ntohl( *(long *)(&buf[8]) );

	ret = fb_cursor( fbp, mode, x, y );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, pcp );
	if( buf ) (void)free(buf);
}

rfbwindow(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	x, y;
	long	ret;

	x =  ntohl( *(long *)(&buf[0]) );
	y =  ntohl( *(long *)(&buf[4]) );

	ret = fb_window( fbp, x, y );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, pcp );
	if( buf ) (void)free(buf);
}

rfbzoom(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	x, y;
	long	ret;

	x =  ntohl( *(long *)(&buf[0]) );
	y =  ntohl( *(long *)(&buf[4]) );

	ret = fb_zoom( fbp, x, y );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, pcp );
	if( buf ) (void)free(buf);
}

#ifdef NEVER	XXX
/* void */
rfbsetsize(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	size;

	size =  ntohl( *(long *)(&buf[0]) );

	fb_setsize( size );
	if( buf ) (void)free(buf);
}

rfbgetsize(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	long	ret;

	ret = fb_getsize( fbp );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, pcp );
	if( buf ) (void)free(buf);
}

rfbsetbackground(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	long	ret;

	fb_setbackground( fbp, ((Pixel *) buf) );
	ret = htonl( 0 );	/* Should NOT return value.	*/
	pkg_send( MSG_RETURN, &ret, 4, pcp );
	if( buf ) (void)free(buf);
}
#endif NEVER

rfbrmap(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	long	ret;
	ColorMap map;

	ret = fb_rmap( fbp, &map );
	pkg_send( MSG_DATA, &map, sizeof(map), pcp );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, pcp );
	if( buf ) (void)free(buf);
}

rfbwmap(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	long	ret;

	if( pcp->pkc_len == 0 )
		ret = fb_wmap( fbp, COLORMAP_NULL );
	else
		ret = fb_wmap( fbp, buf );

	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, pcp );
	if( buf ) (void)free(buf);
}
