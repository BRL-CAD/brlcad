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

#include "fb.h"
#include "pkg.h"
#include "../libfb/pkgtypes.h"
#include "./rfbd.h"

extern	char	*malloc();
extern	double	atof();

static	FBIO	*fbp;

main( argc, argv )
int argc; char **argv;
{
	int	on = 1;
	int	netfd;
	struct	pkg_conn *pcp;

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

/*	pcp = pkg_getclient( netfd, pkg_switch, 0 );*/
	/* XXXX */
	if( (pcp = (struct pkg_conn *)malloc(sizeof(struct pkg_conn)))==PKC_NULL )  {
		fprintf(stderr,"pkg_getclient: malloc failure\n");
		exit( 0 );
	}
	pcp->pkc_magic = PKG_MAGIC;
	pcp->pkc_fd = netfd;
	pcp->pkc_switch = pkg_switch;
	pcp->pkc_left = -1;
	pcp->pkc_buf = (char *)0;
	pcp->pkc_curpos = (char *)0;
	/* XXXX */

	while( pkg_block(pcp) > 0 )
		;

	exit(0);
}

/*
 *			E R R L O G
 *
 *  Log an error.  Route it to user.
 */
static void
errlog( str, pcp )
char *str;
{
#ifdef TTY
	(void)fprintf( stderr, "%s\n", str );
#else
	pkg_send( MSG_ERROR, str, strlen(str), pcp );
#endif
}

/*
 * This is where we go for message types we don't understand.
 */
pkgfoo(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	register int i;
	char str[256];

	sprintf( str, "rlibfb: unable to handle message type %d\n",
		pcp->pkc_type );
	errlog( str, pcp );
	(void)free(buf);
}

/******** Here's where the hooks lead *********/

rfbopen(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	int	height, width;
	long	ret;

	width = ntohl( *(long *)(&buf[0]) );
	height = ntohl( *(long *)(&buf[4]) );

	if( strlen(&buf[8]) == 0 )
		fbp = fb_open( NULL, width, height );
	else
		fbp = fb_open( &buf[8], width, height );

#if 0
	{	char s[81];
sprintf( s, "Device: \"%s\"", &buf[8] );
errlog(s, pcp);
	}
#endif
	ret = fbp == FBIO_NULL ? -1 : 0;
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, pcp );
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
	long	ret;

	ret = fb_clear( fbp, PIXEL_NULL );
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
			errlog("fb_read: malloc failed!", pcp);
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

	x =  ntohl( *(long *)(&buf[0]) );
	y =  ntohl( *(long *)(&buf[4]) );
	num =  ntohl( *(long *)(&buf[8]) );

	/* Get space, fetch data into it, do write, free space. */
	datap = malloc( num*4 );
	pkg_waitfor( MSG_DATA, datap, num*4, pcp );
	ret = fb_write( fbp, x, y, datap, num );

	if( pcp->pkc_type < MSG_NORETURN ) {
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
