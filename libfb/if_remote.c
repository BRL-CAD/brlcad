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

#define NET_LONG_LEN	4	/* # bytes to network long */

#define MAX_HOSTNAME	128
#define	PCP(ptr)	((struct pkg_conn *)((ptr)->u1.p))
#define	PCPL(ptr)	((ptr)->u1.p)	/* left hand side version */

/* Package Handlers. */
static void	pkgerror();	/* error message handler */
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
		rem_cmemory_addr(),
		rem_help();

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
		fb_null,			/* curs_set		*/
		rem_cmemory_addr,
		fb_null,			/* cursor_move_screen_addr */
		rem_help,
		"Remote Device Interface",	/* should be filled in	*/
		1024,				/* " */
		1024,				/* " */
		"host:[dev]",
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
		0
		};

void	pkg_queue(), flush_queue();
static	struct pkg_conn *pcp;

/* from getput.c */
extern unsigned short fbgetshort();
extern unsigned long fbgetlong();
extern char *fbputshort(), *fbputlong();

/*
 * Open a connection to the remotefb.
 * We send NET_LONG_LEN bytes of mode, NET_LONG_LEN bytes of size, then the
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
		fb_log( "rem_dopen: bad device name \"%s\"\n",
			devicename == NULL ? "(null)" : devicename );
		return	-1;
	}
	for( i = 0; devicename[i] != ':' && i < MAX_HOSTNAME; i++ )
		hostname[i] = devicename[i];
	hostname[i] = '\0';
	if( (pc = pkg_open( hostname, "remotefb", 0, 0, 0, pkgswitch, fb_log )) == PKC_ERROR &&
	    (pc = pkg_open( hostname, "5558", 0, 0, 0, pkgswitch, fb_log )) == PKC_ERROR ) {
		fb_log(	"remote_open: can't connect to remotefb server on host \"%s\".\n",
			hostname );
		return	-1;
	}
	PCPL(ifp) = (char *)pc;			/* stash in u1 */
	ifp->if_fd = pc->pkc_fd;		/* unused */

	(void)fbputlong( width, &buf[0*NET_LONG_LEN] );
	(void)fbputlong( height, &buf[1*NET_LONG_LEN] );
	(void) strcpy( &buf[8], file + 1 );
	pkg_send( MSG_FBOPEN, buf, strlen(devicename)+2*NET_LONG_LEN, pc );

	/* return code, max_width, max_height, width, height as longs */
	pkg_waitfor( MSG_RETURN, buf, sizeof(buf), pc );
	ifp->if_max_width = fbgetlong( &buf[1*NET_LONG_LEN] );
	ifp->if_max_height = fbgetlong( &buf[2*NET_LONG_LEN] );
	ifp->if_width = fbgetlong( &buf[3*NET_LONG_LEN] );
	ifp->if_height = fbgetlong( &buf[4*NET_LONG_LEN] );
	if( fbgetlong( &buf[0*NET_LONG_LEN] ) != 0 )
		return(-1);		/* fail */
	return( 0 );			/* OK */
}

_LOCAL_ int
rem_dclose( ifp )
FBIO	*ifp;
{
	char	buf[NET_LONG_LEN+1];

	/* send a close package to remote */
	pkg_send( MSG_FBCLOSE, (char *)0, 0, PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) );
	pkg_close( PCP(ifp) );
	return( fbgetlong( &buf[0*NET_LONG_LEN] ) );
}

_LOCAL_ int
rem_dclear( ifp, bgpp )
FBIO	*ifp;
RGBpixel	*bgpp;
{
	char	buf[NET_LONG_LEN+1];

	/* send a clear package to remote */
	if( bgpp == PIXEL_NULL )  {
		buf[0] = buf[1] = buf[2] = 0;	/* black */
	} else {
		buf[0] = (*bgpp)[RED];
		buf[1] = (*bgpp)[GRN];
		buf[2] = (*bgpp)[BLU];
	}
	pkg_send( MSG_FBCLEAR, buf, 3, PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) );
	return( fbgetlong( buf ) );
}

/*
 *  Send as longs:  x, y, num
 */
_LOCAL_ int
rem_bread( ifp, x, y, pixelp, num )
register FBIO	*ifp;
int		x, y;
RGBpixel	*pixelp;
int		num;
{
	int	ret;
	char	buf[3*NET_LONG_LEN+1];

	if( num <= 0 )
		return(0);
	/* Send Read Command */
	(void)fbputlong( x, &buf[0*NET_LONG_LEN] );
	(void)fbputlong( y, &buf[1*NET_LONG_LEN] );
	(void)fbputlong( num, &buf[2*NET_LONG_LEN] );
	pkg_send( MSG_FBREAD, buf, 3*NET_LONG_LEN, PCP(ifp) );

	/* Get response;  0 len means failure */
	pkg_waitfor( MSG_RETURN, (char *)pixelp,
			num*sizeof(RGBpixel), PCP(ifp) );
	if( (ret=PCP(ifp)->pkc_len) == 0 )  {
		fb_log( "rem_bread: read %d at <%d,%d> failed.\n",
			num, x, y );
		return(-1);
	}
	return( ret/sizeof(RGBpixel) );
}

/*
 * As longs, x, y, num
 */
_LOCAL_ int
rem_bwrite( ifp, x, y, pixelp, num )
register FBIO	*ifp;
int		x, y;
RGBpixel	*pixelp;
int		num;
{
	int	ret;
	char	buf[3*NET_LONG_LEN+1];

	/* Send Write Command */
	(void)fbputlong( x, &buf[0*NET_LONG_LEN] );
	(void)fbputlong( y, &buf[1*NET_LONG_LEN] );
	(void)fbputlong( num, &buf[2*NET_LONG_LEN] );
	pkg_2send( MSG_FBWRITE+MSG_NORETURN,
		buf, 3*NET_LONG_LEN,
		(char *)pixelp, num*sizeof(RGBpixel),
		PCP(ifp) );

#ifdef NEVER
	pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) );
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
	char	buf[3*NET_LONG_LEN+1];
	
	/* Send Command */
	(void)fbputlong( mode, &buf[0*NET_LONG_LEN] );
	(void)fbputlong( x, &buf[1*NET_LONG_LEN] );
	(void)fbputlong( y, &buf[2*NET_LONG_LEN] );
	pkg_send( MSG_FBCURSOR, buf, 3*NET_LONG_LEN, PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) );
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
	char	buf[3*NET_LONG_LEN+1];
	
	/* Send Command */
	(void)fbputlong( x, &buf[0*NET_LONG_LEN] );
	(void)fbputlong( y, &buf[1*NET_LONG_LEN] );
	pkg_send( MSG_FBWINDOW, buf, 2*NET_LONG_LEN, PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) );
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
	char	buf[3*NET_LONG_LEN+1];

	/* Send Command */
	(void)fbputlong( x, &buf[0*NET_LONG_LEN] );
	(void)fbputlong( y, &buf[1*NET_LONG_LEN] );
	pkg_send( MSG_FBZOOM, buf, 2*NET_LONG_LEN, PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) );
	return( fbgetlong( &buf[0*NET_LONG_LEN] ) );
}

_LOCAL_ int
rem_cmread( ifp, cmap )
FBIO			*ifp;
register ColorMap	*cmap;
{
	register int	i;
	char	buf[NET_LONG_LEN+1];
	char	cm[256*2*3];

	pkg_send( MSG_FBRMAP, (char *)0, 0, PCP(ifp) );
	pkg_waitfor( MSG_DATA, cm, sizeof(cm), PCP(ifp) );
	for( i = 0; i < 256; i++ ) {
		cmap->cm_red[i] = fbgetshort( cm+2*(0+i) );
		cmap->cm_green[i] = fbgetshort( cm+2*(256+i) );
		cmap->cm_blue[i] = fbgetshort( cm+2*(512+i) );
	}
	pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) );
	return( fbgetlong( &buf[0*NET_LONG_LEN] ) );
}

_LOCAL_ int
rem_cmwrite( ifp, cmap )
FBIO	*ifp;
ColorMap	*cmap;
{
	int	i;
	char	buf[NET_LONG_LEN+1];
	char	cm[256*2*3];

	if( cmap == COLORMAP_NULL )
		pkg_send( MSG_FBWMAP, (char *)0, 0, PCP(ifp) );
	else {
		for( i = 0; i < 256; i++ ) {
			(void)fbputshort( cmap->cm_red[i], cm+2*(0+i) );
			(void)fbputshort( cmap->cm_green[i], cm+2*(256+i) );
			(void)fbputshort( cmap->cm_blue[i], cm+2*(512+i) );
		}
		pkg_send( MSG_FBWMAP, cm, sizeof(cm), PCP(ifp) );
	}
	pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) );
	return( fbgetlong( &buf[0*NET_LONG_LEN] ) );
}


_LOCAL_ int
rem_help( ifp )
FBIO	*ifp;
{
	char	buf[1*NET_LONG_LEN+1];

	fb_log( "Remote Interface:\n" );

	/* Send Command */
	(void)fbputlong( 0L, &buf[0*NET_LONG_LEN] );
	pkg_send( MSG_FBHELP, buf, 1*NET_LONG_LEN, PCP(ifp) );
	pkg_waitfor( MSG_RETURN, buf, NET_LONG_LEN, PCP(ifp) );
	return( fbgetlong( &buf[0*NET_LONG_LEN] ) );
}

/*
 *  This is where we come on asynchronous error or log messages.
 *  We are counting on the remote machine now to prefix his own
 *  name to messages, so we don't touch them ourselves.
 */
static void
pkgerror(pcp, buf)
struct pkg_conn *pcp;
char *buf;
{
	fb_log( "%s", buf );
	free(buf);
}
