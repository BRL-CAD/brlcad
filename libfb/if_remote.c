/*
 *		I F _ R E M O T E . C
 *
 *  Remote libfb interface.
 *
 *  Duplicates the functions in libfb via communication
 *  with a remote server (rfbd).
 *
 *  Authors -
 *	Phillip Dykstra
 *	Gary S. Moss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 *
 *	$Header$ (BRL)
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

#ifdef BSD
#include <sys/types.h>
#include <sys/uio.h>		/* for struct iovec */
#include <netinet/in.h>		/* for htons(), etc */
#endif

#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "pkg.h"
#include "./pkgtypes.h"
#include "fb.h"
#include "./fblocal.h"

#define MAX_PIXELS_NET	1024
#define MAX_HOSTNAME	32
#define	PCP(ptr)	((struct pkg_conn *)((ptr)->u1.p))
#define	PCPL(ptr)	((ptr)->u1.p)	/* left hand side version */

/* Package Handlers. */
static int pkgerror();	/* error message handler */
static struct pkg_switch pkgswitch[] = {
	{ MSG_ERROR, pkgerror, "Error Message" },
	{ 0, NULL, NULL }
};

_LOCAL_ int	rem_dopen(),
		rem_dclose(),
		rem_dclear(),
		rem_bread(),
		rem_bwrite(),
		rem_cmread(),
		rem_cmwrite(),
		rem_window_set(),
		rem_zoom_set(),
		rem_cmemory_addr();

FBIO remote_interface =
		{
		rem_dopen,
		rem_dclose,
		fb_null,			/* reset		*/
		rem_dclear,
		rem_bread,
		rem_bwrite,
		rem_cmread,
		rem_cmwrite,
		fb_null,			/* viewport_set		*/
		rem_window_set,
		rem_zoom_set,
		fb_null,			/* cursor_init_bitmap	*/
		rem_cmemory_addr,
		fb_null,			/* cursor_move_screen_addr */
		"Remote Device Interface",	/* should be filled in	*/
		1024,				/* " */
		1024,				/* " */
		"*remote*",
		512,
		512,
		-1,
		PIXEL_NULL,
		PIXEL_NULL,
		PIXEL_NULL,
		-1,
		0,
		0L,
		0L,
		0L
		};

void	pkg_queue(), flush_queue();
static	struct pkg_conn *pcp;

/* from getput.c */
extern unsigned short fbgetshort();
extern unsigned long fbgetlong();
extern char *fbputshort(), *fbputlong();

/*
 * Open a connection to the remote libfb.
 * We send 4 bytes of mode, 4 bytes of size, then the
 *  devname (or NULL if default).
 */
_LOCAL_ int
rem_dopen( ifp, devicename, width, height )
register FBIO	*ifp;
register char	*devicename;
int	width, height;
{
	register int	i;
	struct pkg_conn *pc;
	char	buf[128];
	char	*file;
	char	hostname[MAX_HOSTNAME];

	if( devicename == NULL || (file = strchr( devicename, ':' )) == NULL ) {
		fb_log( "remote_dopen: bad device name \"%s\"\n",
			devicename == NULL ? "(null)" : devicename );
		return	-1;
	}
	for( i = 0; devicename[i] != ':' && i < MAX_HOSTNAME; i++ )
		hostname[i] = devicename[i];
	hostname[i] = '\0';
	if( (pc = pkg_open( hostname, "mfb", pkgswitch, fb_log )) == PKC_ERROR ) {
		fb_log(	"remote_dopen: can't connect to host \"%s\".\n",
			hostname );
		return	-1;
	}
	PCPL(ifp) = (char *)pc;			/* stash in u1 */
	ifp->if_fd = pc->pkc_fd;		/* unused */

	(void)fbputlong( width, &buf[0] );
	(void)fbputlong( height, &buf[4] );
	(void) strcpy( &buf[8], file + 1 );
	pkg_send( MSG_FBOPEN, buf, strlen(devicename)+8, pc );

	/* return code, max_width, max_height, width, height as longs */
	pkg_waitfor( MSG_RETURN, buf, sizeof(buf), pc );
	ifp->if_max_width = fbgetlong( &buf[1*4] );
	ifp->if_max_height = fbgetlong( &buf[2*4] );
	ifp->if_width = fbgetlong( &buf[3*4] );
	ifp->if_height = fbgetlong( &buf[4*4] );
	return( fbgetlong( &buf[0*4] ) );
}

_LOCAL_ int
rem_dclose( ifp )
FBIO	*ifp;
{
	char	buf[4+1];

	/* send a close package to remote */
	pkg_send( MSG_FBCLOSE, 0L, 0L, PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	pkg_close( PCP(ifp) );
	return( fbgetlong( buf ) );
}

_LOCAL_ int
rem_dclear( ifp, bgpp )
FBIO	*ifp;
Pixel	*bgpp;
{
	char	buf[4+1];

	/* send a clear package to remote */
	if( bgpp == PIXEL_NULL )  {
		buf[0] = buf[1] = buf[2] = 0;	/* black */
	} else {
		buf[0] = bgpp->red;
		buf[1] = bgpp->green;
		buf[2] = bgpp->blue;
	}
	pkg_send( MSG_FBCLEAR, buf, 3, PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	return( fbgetlong( buf ) );
}

/*
 *  Send as longs:  x, y, num
 */
_LOCAL_ int
rem_bread( ifp, x, y, pixelp, num )
FBIO	*ifp;
int	x, y;
Pixel	*pixelp;
int	num;
{
	int	ret;
	char	buf[3*4+1];

	/* Send Read Command */
	(void)fbputlong( x, &buf[0] );
	(void)fbputlong( y, &buf[4] );
	(void)fbputlong( num, &buf[8] );
	pkg_send( MSG_FBREAD, buf, 3*4, PCP(ifp) );

	/* Get return first, to see how much data there is */
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	ret = fbgetlong( buf );

	/* Get Data */
	if( ret > 0 )
		pkg_waitfor( MSG_DATA, (char *) pixelp,	num*4, PCP(ifp) );
	else if( ret < 0 )
		fb_log( "remote_bread: read at <%d,%d> failed.\n", x, y );
	return	ret;
}

/*
 * As longs, x, y, num
 */
_LOCAL_ int
rem_bwrite( ifp, x, y, pixelp, num )
FBIO	*ifp;
int	x, y;
Pixel	*pixelp;
int	num;
{
	int	ret;
	char	buf[3*4+1];

	/* Send Write Command */
	(void)fbputlong( x, &buf[0] );
	(void)fbputlong( y, &buf[4] );
	(void)fbputlong( num, &buf[8] );
	pkg_stream( MSG_FBWRITE+MSG_NORETURN, buf, 3*4, PCP(ifp) );

	/* Send DATA */
	pkg_stream( MSG_DATA, (char *)pixelp, num*4, PCP(ifp) );
#ifdef NEVER
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	ret = fbgetlong( buf );
	return(ret);
#endif
	return	num;	/* No error return, sacrificed for speed. */
}

/*
 *  32-bit longs: mode, x, y
 */
_LOCAL_ int
rem_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	char	buf[3*4+1];
	
	/* Send Command */
	(void)fbputlong( mode, &buf[0] );
	(void)fbputlong( x, &buf[4] );
	(void)fbputlong( y, &buf[8] );
	pkg_send( MSG_FBCURSOR, buf, 3*4, PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	return( fbgetlong( buf ) );
}

/*
 *	x,y
 */
_LOCAL_ int
rem_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	char	buf[3*4+1];
	
	/* Send Command */
	(void)fbputlong( x, &buf[0] );
	(void)fbputlong( y, &buf[4] );
	pkg_send( MSG_FBWINDOW, buf, 2*4, PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	return( fbgetlong( buf ) );
}

/*
 *	x,y
 */
_LOCAL_ int
rem_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	char	buf[3*4+1];

	/* Send Command */
	(void)fbputlong( x, &buf[0] );
	(void)fbputlong( y, &buf[4] );
	pkg_send( MSG_FBZOOM, buf, 2*4, PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	return( fbgetlong( buf ) );
}

_LOCAL_ int
rem_cmread( ifp, cmap )
FBIO	*ifp;
ColorMap	*cmap;
{
	char	buf[4+1];

	pkg_send( MSG_FBRMAP, 0, 0, PCP(ifp) );
	pkg_waitfor( MSG_DATA, cmap, sizeof(*cmap), PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	return( fbgetlong(buf) );
}

_LOCAL_ int
rem_cmwrite( ifp, cmap )
FBIO	*ifp;
ColorMap	*cmap;
{
	char	buf[4+1];

	if( cmap == COLORMAP_NULL )
		pkg_send( MSG_FBWMAP, 0, 0, PCP(ifp) );
	else
		pkg_send( MSG_FBWMAP, cmap, sizeof(*cmap), PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, 4, PCP(ifp) );
	return( fbgetlong(buf) );
}

/*
 * This is where we come on
 * asynchronous error messages.
 */
static int
pkgerror(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	fb_log( "remote: %s", buf );
	(void)free(buf);
	return	0;	/* Declared as integer function in pkg_switch. */
}
