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
/* #include <netinet/in.h>		/* For htonl(), etc XXX */
#include <syslog.h>

#include "fb.h"
#include "./pkg.h"
#include "./pkgtypes.h"

extern	char	*malloc();
extern	double	atof();

static	FBIO	*fbp;
static	int	netfd;

main( argc, argv )
int argc; char **argv;
{
	int on = 1;

#ifdef NEVER
	/*
	 * Listen for PKG connections.
	 * This is what we would do if we weren't being started
	 * by inetd (plus some other code that's been removed...).
	 */
	if( (tcp_listen_fd = pkg_initserver("rlibfb", 0)) < 0 )
		exit(1);
#endif
	if( setsockopt(0, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof(on)) < 0 ) {
		openlog( argv[0], LOG_PID, 0 );
		syslog( LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m" );
	}

	(void)signal( SIGPIPE, SIG_IGN );

	netfd = 0;

	while( pkg_block(netfd) > 0 )
		;

	exit(0);
}

/*
 *			E R R L O G
 *
 *  Log an error.  Route it to user.
 */
static void
errlog( str )
char *str;
{
#ifdef TTY
	(void)fprintf( stderr, "%s\n", str );
#else
	pkg_send( MSG_ERROR, str, strlen(str), netfd );
#endif
}

/*
 * This is where we go for message types we don't understand.
 */
pkgfoo(type, buf, length)
int type, length;
char *buf;
{
	register int i;
	char str[256];

	for( i=0; i<pkg_swlen; i++ )  {
		if( pkg_switch[i].pks_type == type )  break;
	}
	sprintf( str, "rlibfb: unable to handle %s message: len %d\n",
		pkg_switch[i].pks_title, length );
	errlog( str );
	*buf = '*';
	(void)free(buf);
}

/******** Here's where the hooks lead *********/

rfbopen(type, buf, length)
int type, length;
char *buf;
{
	int	height, width;
	long	ret;

	height = ntohl( *(long *)(&buf[0]) );
	width = ntohl( *(long *)(&buf[4]) );

	if( strlen(&buf[8]) == 0 )
		fbp = fb_open( NULL, height, width );
	else
		fbp = fb_open( &buf[8], height, width );

#if 0
	{	char s[81];
sprintf( s, "Device: \"%s\"", &buf[8] );
errlog(s);
	}
#endif
	ret = fbp == FBIO_NULL ? -1 : 0;
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, netfd );
	if( buf ) (void)free(buf);
}

rfbclose(type, buf, length)
int type, length;
char *buf;
{
	long	ret;

	ret = fb_close( fbp );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, netfd );
	if( buf ) (void)free(buf);
	fbp = FBIO_NULL;
}

rfbclear(type, buf, length)
int type, length;
char *buf;
{
	long	ret;

	ret = fb_clear( fbp, PIXEL_NULL );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, netfd );
	if( buf ) (void)free(buf);
}

rfbread(type, buf, length)
int type, length;
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
			errlog("fb_read: malloc failed!");
			if( buf ) (void)free(buf);
			buflen = 0;
			return;
		}
	}

	ret = fb_read( fbp, x, y, rbuf, num );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, netfd );

	/* Send back the data */
	if( ret >= 0 )
		pkg_send( MSG_DATA, rbuf, num*sizeof(Pixel), netfd );
	if( buf ) (void)free(buf);
}

rfbwrite(type, buf, length)
int type, length;
char *buf;
{
	int	x, y, num;
	long	ret;
	char	*datap;

	x =  ntohl( *(long *)(&buf[0]) );
	y =  ntohl( *(long *)(&buf[4]) );
	num =  ntohl( *(long *)(&buf[8]) );

	/* Get space, fetch data into it, do write, free space. */
	datap = malloc( num*4 );
	pkg_waitfor( MSG_DATA, datap, num*4, netfd );
	ret = fb_write( fbp, x, y, datap, num );

	if( type < MSG_NORETURN ) {
		ret = htonl( ret );
		pkg_send( MSG_RETURN, &ret, 4, netfd );
	}
	free( datap );
	if( buf ) (void)free(buf);
}

rfbcursor(type, buf, length)
int type, length;
char *buf;
{
	int	mode, x, y;
	long	ret;

	mode =  ntohl( *(long *)(&buf[0]) );
	x =  ntohl( *(long *)(&buf[4]) );
	y =  ntohl( *(long *)(&buf[8]) );

	ret = fb_cursor( fbp, mode, x, y );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, netfd );
	if( buf ) (void)free(buf);
}

rfbwindow(type, buf, length)
int type, length;
char *buf;
{
	int	x, y;
	long	ret;

	x =  ntohl( *(long *)(&buf[0]) );
	y =  ntohl( *(long *)(&buf[4]) );

	ret = fb_window( fbp, x, y );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, netfd );
	if( buf ) (void)free(buf);
}

rfbzoom(type, buf, length)
int type, length;
char *buf;
{
	int	x, y;
	long	ret;

	x =  ntohl( *(long *)(&buf[0]) );
	y =  ntohl( *(long *)(&buf[4]) );

	ret = fb_zoom( fbp, x, y );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, netfd );
	if( buf ) (void)free(buf);
}

#ifdef NEVER	XXX
/* void */
rfbsetsize(type, buf, length)
int type, length;
char *buf;
{
	int	size;

	size =  ntohl( *(long *)(&buf[0]) );

	fb_setsize( size );
	if( buf ) (void)free(buf);
}

rfbgetsize(type, buf, length)
int type, length;
char *buf;
{
	long	ret;

	ret = fb_getsize( fbp );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, netfd );
	if( buf ) (void)free(buf);
}

rfbsetbackground(type, buf, length)
int type, length;
char *buf;
{
	long	ret;

	fb_setbackground( fbp, ((Pixel *) buf) );
	ret = htonl( 0 );	/* Should NOT return value.	*/
	pkg_send( MSG_RETURN, &ret, 4, netfd );
	if( buf ) (void)free(buf);
}
#endif NEVER

rfbrmap(type, buf, length)
int type, length;
char *buf;
{
	long	ret;
	ColorMap map;

	ret = fb_rmap( fbp, &map );
	pkg_send( MSG_DATA, &map, sizeof(map), netfd );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, netfd );
	if( buf ) (void)free(buf);
}

rfbwmap(type, buf, length)
int type, length;
char *buf;
{
	long	ret;

	if( length == 0 )
		ret = fb_wmap( fbp, COLORMAP_NULL );
	else
		ret = fb_wmap( fbp, buf );

	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, netfd );
	if( buf ) (void)free(buf);
}
