/*
 *		I F _ D E B U G
 *
 *  Reports all calls to fb_log().
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <ctype.h>
#include "fb.h"
#include "./fblocal.h"

_LOCAL_ int	deb_dopen(),
		deb_dclose(),
		deb_dreset(),
		deb_dclear(),
		deb_bread(),
		deb_bwrite(),
		deb_cmread(),
		deb_cmwrite(),
		deb_viewport_set(),
		deb_window_set(),
		deb_zoom_set(),
		deb_curs_set(),
		deb_cmemory_addr(),
		deb_cscreen_addr(),
		deb_help();

/* This is the ONLY thing that we "export" */
FBIO debug_interface =
		{
		deb_dopen,
		deb_dclose,
		deb_dreset,
		deb_dclear,
		deb_bread,
		deb_bwrite,
		deb_cmread,
		deb_cmwrite,
		deb_viewport_set,
		deb_window_set,
		deb_zoom_set,
		deb_curs_set,
		deb_cmemory_addr,
		deb_cscreen_addr,
		deb_help,
		"Debugging Interface",
		8*1024,			/* max width */
		8*1024,			/* max height */
		"/dev/debug",
		512,			/* current width (init 0) */
		512,			/* current height (init 0) */
		-1,			/* file descriptor */
		PIXEL_NULL,		/* page_base */
		PIXEL_NULL,		/* page_curp */
		PIXEL_NULL,		/* page_endp */
		-1,			/* page_no */
		0,			/* page_ref */
		0L,			/* page_curpos */
		0L,			/* page_pixels */
		0			/* debug */
		};

_LOCAL_ int
deb_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	if( file == (char *)NULL )
		fb_log( "fb_open( 0x%lx, NULL, %d, %d )\n",
			(unsigned long)ifp, width, height );
	else
		fb_log( "fb_open( 0x%lx, \"%s\", %d, %d )\n",
			(unsigned long)ifp, file, width, height );

	/* check for default size */
	if( width == 0 )
		width = ifp->if_width;
	if( height == 0 )
		height = ifp->if_height;

	/* set debug bit vector */
	if( file != NULL ) {
		char *cp;
		for( cp = file; *cp != NULL && !isdigit(*cp); cp++ ) ;
		sscanf( cp, "%d", &ifp->if_debug );
	} else {
		ifp->if_debug = 0;
	}

	/* Give the user whatever with was asked for */
	ifp->if_width = width;
	ifp->if_height = height;

	return	0;
}

_LOCAL_ int
deb_dclose( ifp )
FBIO	*ifp;
{
	fb_log( "fb_close( 0x%lx )\n", (unsigned long)ifp );
	return	0;
}

_LOCAL_ int
deb_dreset( ifp )
FBIO	*ifp;
{
	fb_log( "fb_reset( 0x%lx )\n", (unsigned long)ifp );
	return	0;
}

_LOCAL_ int
deb_dclear( ifp, pp )
FBIO	*ifp;
RGBpixel	*pp;
{
	if( pp == 0 )
		fb_log( "fb_clear( 0x%lx, NULL )\n", (unsigned long)ifp );
	else
		fb_log( "fb_clear( 0x%lx, &[%d %d %d] )\n",
			(unsigned long)ifp,
			(int)((*pp)[RED]), (int)((*pp)[GRN]),
			(int)((*pp)[BLU]) );
	return	0;
}

_LOCAL_ int
deb_bread( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	fb_log( "fb_read( 0x%lx,%4d,%4d, 0x%lx, %d )\n",
		(unsigned long)ifp, x, y,
		(unsigned long)pixelp, count );
	return	count;
}

_LOCAL_ int
deb_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	int	i;

	fb_log( "fb_write( 0x%lx,%4d,%4d, 0x%lx, %d )\n",
		(unsigned long)ifp, x, y,
		(unsigned long)pixelp, count );

	/* write them out, four per line */
	if( ifp->if_debug & FB_DEBUG_RW ) {
		for( i = 0; i < count; i++ ) {
			if( i % 4 == 0 )
				fb_log( "%4d:", i );
			fb_log( "  [%3d,%3d,%3d]", pixelp[i][RED],
				pixelp[i][GRN], pixelp[i][BLU] );
			if( i % 4 == 3 )
				fb_log( "\n" );
		}
		if( i % 4 != 0 )
			fb_log( "\n" );
	}

	return	count;
}

_LOCAL_ int
deb_cmread( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	fb_log( "fb_rmap( 0x%lx, 0x%lx )\n",
		(unsigned long)ifp, (unsigned long)cmp );
	return	0;
}

_LOCAL_ int
deb_cmwrite( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	int	i;

	if( cmp == NULL )
		fb_log( "fb_wmap( 0x%lx, NULL )\n",
			(unsigned long)ifp );
	else
		fb_log( "fb_wmap( 0x%lx, 0x%lx )\n",
			(unsigned long)ifp, (unsigned long)cmp );

	if( ifp->if_debug & FB_DEBUG_CMAP && cmp != NULL ) {
		for( i = 0; i < 256; i++ ) {
			fb_log( "%3d: [ 0x%4lx, 0x%4lx, 0x%4lx ]\n",
				i,
				(unsigned long)cmp->cm_red[i],
				(unsigned long)cmp->cm_green[i],
				(unsigned long)cmp->cm_blue[i] );
		}
	}

	return	0;
}

_LOCAL_ int
deb_viewport_set( ifp, left, top, right, bottom )
FBIO	*ifp;
int	left, top, right, bottom;
{
	fb_log( "fb_viewport( 0x%lx,%4d,%4d,%4d,%4d )\n",
		(unsigned long)ifp, left, top, right, bottom );
	return	0;
}

_LOCAL_ int
deb_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	fb_log( "fb_window( 0x%lx,%4d,%4d )\n",
		(unsigned long)ifp, x, y );
	return	0;
}

_LOCAL_ int
deb_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	fb_log( "fb_zoom( 0x%lx, %d, %d )\n",
		(unsigned long)ifp, x, y );
	return	0;
}

_LOCAL_ int
deb_curs_set( ifp, bits, xbits, ybits, xorig, yorig )
FBIO	*ifp;
unsigned char *bits;
int	xbits, ybits;
int	xorig, yorig;
{
	fb_log( "fb_setcursor( 0x%lx, 0x%lx, %d, %d, %d, %d )\n",
		(unsigned long)ifp, bits, xbits, ybits, xorig, yorig );
	return	0;
}

_LOCAL_ int
deb_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	fb_log( "fb_cursor( 0x%lx, %d,%4d,%4d )\n",
		(unsigned long)ifp, mode, x, y );
	return	0;
}

_LOCAL_ int
deb_cscreen_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	fb_log( "fb_scursor( 0x%lx, %d,%4d,%4d )\n",
		(unsigned long)ifp, mode, x, y );
	return	0;
}

/*ARGSUSED*/
_LOCAL_ int
deb_help( ifp )
FBIO	*ifp;
{
	fb_log( "Description: %s\n", debug_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width/height: %d %d\n",
		debug_interface.if_max_width,
		debug_interface.if_max_height );
	fb_log( "Default width/height: %d %d\n",
		debug_interface.if_width,
		debug_interface.if_height );
	fb_log( "\
Usage: /dev/debug[#]\n\
  where # is a optional bit vector from:\n\
    1    debug buffered I/O calls\n\
    2    show colormap entries in rmap/wmap calls\n\
    4    show actual pixel values in read/write calls\n" );
	/*8    buffered read/write values - ifdef'd out*/

	return	0;
}
