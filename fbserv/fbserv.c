/*
 *	R L I B F B . C
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
#include	"./pkg.h"
#include	"./pkgtypes.h"
#include "fb.h"

char	*malloc();

/*
 * Package Handlers.
 */
int pkgfoo();	/* foobar message handler */
int rfbopen(), rfbclose(), rfbclear(), rfbread(), rfbwrite(), rfbcursor();
int rfbwindow(), rfbzoom(), rfbgetsize(), rfbsetsize(), rfbsetbackground();
int rfbrmap(), rfbwmap();
int rfbioinit(), rfbwpixel();
int rpagein(), rpageout();
struct pkg_switch pkg_switch[] = {
	{ MSG_FBOPEN,	rfbopen,	"Open Framebuffer" },
	{ MSG_FBCLOSE,	rfbclose,	"" },
	{ MSG_FBCLEAR,	rfbclear,	"Clear Framebuffer" },
	{ MSG_FBREAD,	rfbread,	"Read Pixels" },
	{ MSG_FBWRITE,	rfbwrite,	"Write Pixels" },
	{ MSG_FBWRITE + MSG_NORETURN,	rfbwrite,	"Asynch write" },
	{ MSG_FBCURSOR,	rfbcursor,	"" },
	{ MSG_FBWINDOW,	rfbwindow,	"" },
	{ MSG_FBZOOM,	rfbzoom,	"" },
	{ MSG_FBGETSIZE,rfbgetsize,	"" },
	{ MSG_FBSETSIZE,rfbsetsize,	"" },
	{ MSG_FBSETBACKG,rfbsetbackground,"" },
	{ MSG_FBRMAP,	rfbrmap,	"" },
	{ MSG_FBWMAP,	rfbwmap,	"" },
	{ MSG_FBIOINIT,	rfbioinit,	"" },
	{ MSG_ERROR,	pkgfoo,		"Error Message" },
	{ MSG_CLOSE,	pkgfoo,		"Close Connection" },
	{ MSG_PAGEIN,	rpagein,	"Get buffer page" },
	{ MSG_PAGEOUT,	rpageout,	"Put buffer page" }
};
int pkg_swlen = 19;

static	int	cur_fd;

#ifdef NEVER
/* Buffered IO variables */
extern	char	*_pagep;
extern	int	_pagesz;
extern	int	_pageno;
#endif NEVER

extern double atof();

main(argc, argv)
int argc; char **argv;
{
	register int i;
	int on = 1;

#ifdef NEVER
	/* Listen for our PKG connections */
	if( (tcp_listen_fd = pkg_initserver("rlibfb", 0)) < 0 )
		exit(1);
#endif
	if (setsockopt(0, SOL_SOCKET, SO_KEEPALIVE, &on, sizeof (on)) < 0) {
		openlog(argv[0], LOG_PID, 0);
		syslog(LOG_WARNING, "setsockopt (SO_KEEPALIVE): %m");
	}

	(void)signal( SIGPIPE, SIG_IGN );

	cur_fd = 0;

	while( pkg_block(cur_fd) > 0 )
		;

	exit(0);
}

/*
 *			E R R L O G
 *
 *  Log an error.  We supply the newline, and route to user.
 */
/* VARARGS */
void
errlog( str, a, b, c, d, e, f, g, h )
char *str;
{
	char buf[256];		/* a generous output line */

#ifdef TTY
	(void)fprintf( stderr, str, a, b, c, d, e, f, g, h );
#endif TTY
}

pkgfoo(type, buf, length)
int type, length;
char *buf;
{
	register int i;

	for( i=0; i<pkg_swlen; i++ )  {
		if( pkg_switch[i].pks_type == type )  break;
	}
	errlog("rlibfb: unable to handle %s message: len %d\n",
		pkg_switch[i].pks_title, length);
	*buf = '*';
	(void)free(buf);
}

/******** Here's where the hooks lead *********/

rfbopen(type, buf, length)
int type, length;
char *buf;
{
	int	mode, size, ret;
/*char s[50];*/

	mode = ntohl( *(long *)(&buf[0]) );
	size = ntohl( *(long *)(&buf[4]) );

	(void)fbsetsize( size );	/* XXX - should return any error */
					/*       as an async message */
	if( strlen(&buf[8]) == 0 )
		ret = fbopen( 0L, mode );
	else
		ret = fbopen( &buf[8], mode );

/*sprintf( s, "Device: \"%s\"", &buf[8] );*/
/*pkg_send( MSG_ERROR, s, strlen(s), cur_fd );*/
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, cur_fd );
	if( buf ) (void)free(buf);
}

/* NB: This is supposed to take an fd arg! */
rfbclose(type, buf, length)
int type, length;
char *buf;
{
	int	ret;

	ret = fbclose( 0 );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, cur_fd );
	if( buf ) (void)free(buf);
}

rfbclear(type, buf, length)
int type, length;
char *buf;
{
	int	ret;

	ret = fbclear();
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, cur_fd );
	if( buf ) (void)free(buf);
}

rfbread(type, buf, length)
int type, length;
char *buf;
{
	int	x, y, num, ret;
	char	dat[10000];

	x =  ntohl( *(long *)(&buf[0]) );
	y =  ntohl( *(long *)(&buf[4]) );
	num =  ntohl( *(long *)(&buf[8]) );

	if( num > 10000/4 ) {
		pkg_send( MSG_ERROR, "fbread: too many bytes!", 24, cur_fd );
		num = 10000/4;
	}

	ret = fbread( x, y, &dat[0], num );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, cur_fd );

	/* Send back the data */
	if( ret >= 0 )
		pkg_send( MSG_DATA, &dat[0], num*4, cur_fd );
	if( buf ) (void)free(buf);
}

rfbwrite(type, buf, length)
int type, length;
char *buf;
{
	int	x, y, num, ret;
	char	*datap;

	x =  ntohl( *(long *)(&buf[0]) );
	y =  ntohl( *(long *)(&buf[4]) );
	num =  ntohl( *(long *)(&buf[8]) );

	/* Get space, fetch data into it, do write, free space. */
	datap = malloc( num*4 );
	pkg_waitfor( MSG_DATA, datap, num*4, cur_fd );
	ret = fbwrite( x, y, datap, num );

	if( type < MSG_NORETURN ) {
		ret = htonl( ret );
		pkg_send( MSG_RETURN, &ret, 4, cur_fd );
	}
	free( datap );
	if( buf ) (void)free(buf);
}

rfbcursor(type, buf, length)
int type, length;
char *buf;
{
	int	mode, x, y;
	int	ret;

	mode =  ntohl( *(long *)(&buf[0]) );
	x =  ntohl( *(long *)(&buf[4]) );
	y =  ntohl( *(long *)(&buf[8]) );

	ret = fbcursor(mode, x, y);
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, cur_fd );
	if( buf ) (void)free(buf);
}

rfbwindow(type, buf, length)
int type, length;
char *buf;
{
	int	x, y;
	int	ret;

	x =  ntohl( *(long *)(&buf[0]) );
	y =  ntohl( *(long *)(&buf[4]) );

	ret = fbwindow(x, y);
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, cur_fd );
	if( buf ) (void)free(buf);
}

rfbzoom(type, buf, length)
int type, length;
char *buf;
{
	int	x, y;
	int	ret;

	x =  ntohl( *(long *)(&buf[0]) );
	y =  ntohl( *(long *)(&buf[4]) );

	ret = fbzoom(x, y);
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, cur_fd );
	if( buf ) (void)free(buf);
}

/* void */
rfbsetsize(type, buf, length)
int type, length;
char *buf;
{
	int	size;

	size =  ntohl( *(long *)(&buf[0]) );

	fbsetsize( size );
	if( buf ) (void)free(buf);
}

rfbgetsize(type, buf, length)
int type, length;
char *buf;
{
	int	ret;

	ret = fbgetsize();
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, cur_fd );
	if( buf ) (void)free(buf);
}

rfbsetbackground(type, buf, length)
int type, length;
char *buf;
{
	int	ret;

	ret = fbsetbackground( buf );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, cur_fd );
	if( buf ) (void)free(buf);
}

rfbrmap(type, buf, length)
int type, length;
char *buf;
{
	int	ret;
	ColorMap map;

	ret = fb_rmap( &map );
	pkg_send( MSG_DATA, &map, sizeof(map), cur_fd );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, cur_fd );
	if( buf ) (void)free(buf);
}

rfbwmap(type, buf, length)
int type, length;
char *buf;
{
	int	ret;

	if( length == 0 )
		ret = fb_wmap( 0 );
	else
		ret = fb_wmap( buf );

	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, cur_fd );
	if( buf ) (void)free(buf);
}

/*
 * XXX - we eventually have to give PAGE_SIZE back to remote.
 */
rfbioinit(type, buf, length)
int type, length;
char *buf;
{
	fbioinit();
	if( buf ) (void)free(buf);
}

/*
 * We get _pageno from remote.
 */
rpageout(type, buf, length)
int type, length;
char *buf;
{
	long	ret;

	_pageno = ntohl( *(long *)(&buf[0]) );

	/* Get page data */
	pkg_waitfor( MSG_DATA, _pagep, _pagesz, cur_fd );

	ret = _pageout();
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, cur_fd );

	if( buf ) (void)free(buf);
}

/*
 * We get pageno from remote.
 */
rpagein(type, buf, length)
int type, length;
char *buf;
{
	long	ret;
	int	pageno;

	pageno = ntohl( *(long *)(&buf[0]) );
	ret = _pagein( pageno );
	ret = htonl( ret );
	pkg_send( MSG_RETURN, &ret, 4, cur_fd );

	if( ntohl(ret) >= 0 )
		pkg_send( MSG_DATA, _pagep, _pagesz, cur_fd );
}
