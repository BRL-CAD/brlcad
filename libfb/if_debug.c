/*
 *	I F _ D E B U G
 */

#include <stdio.h>
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
		deb_cinit_bitmap(),
		deb_cmemory_addr(),
		deb_cscreen_addr();

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
		deb_cinit_bitmap,
		deb_cmemory_addr,
		deb_cscreen_addr,
		"Debugging Interface",
		1024,			/* max width */
		1024,			/* max height */
		"/dev/debug",
		0,			/* current width (init 0) */
		0,			/* current height (init 0) */
		-1,			/* file descriptor */
		PIXEL_NULL,		/* page_base */
		PIXEL_NULL,		/* page_curp */
		PIXEL_NULL,		/* page_endp */
		-1,			/* page_no */
		0,			/* page_ref */
		0L,			/* page_curpos */
		0L,			/* page_bytes */
		0L			/* page_pixels */
		};

_LOCAL_ int
deb_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	if( file == (char *)NULL )
		fb_log( "fb_open( fp, 0, %d, %d )\n",
			width, height );
	else
		fb_log( "fb_open( fp, \"%s\", %d, %d )\n",
			file, width, height );

	/* Give the user whatever with was asked for */
	ifp->if_width = width;
	ifp->if_height = height;

	return( 1 );
}

_LOCAL_ int
deb_dclose( ifp )
FBIO	*ifp;
{
	fb_log( "fb_close( fp )\n" );
	return	1;
}

_LOCAL_ int
deb_dreset( ifp )
FBIO	*ifp;
{
	fb_log( "fb_reset( fp )\n" );
	return	1;
}

_LOCAL_ int
deb_dclear( ifp, pp )
FBIO	*ifp;
Pixel	*pp;
{
	if( pp == 0 )
		fb_log( "fb_clear( fp, 0 )\n" );
	else
		fb_log( "fb_clear( fp, &[%d %d %d %d] )\n",
			(int)pp->red, (int)pp->green,
			(int)pp->blue, (int)pp->spare );
	return	1;
}

_LOCAL_ int
deb_bread( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
Pixel	*pixelp;
int	count;
{
	fb_log( "fb_read( fp,%4d,%4d, 0x%x, %d )\n",
		x, y, pixelp, count );
	return	count;
}

_LOCAL_ int
deb_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
Pixel	*pixelp;
int	count;
{
	fb_log( "fb_write( fp,%4d,%4d, 0x%x, %d )\n",
		x, y, pixelp, count );
	return	count;
}

_LOCAL_ int
deb_cmread( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	fb_log( "fb_rmap( fp, 0x%x )\n", cmp );
	return	1;
}

_LOCAL_ int
deb_cmwrite( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	fb_log( "fb_wmap( fp, 0x%x )\n", cmp );
	return	1;
}

_LOCAL_ int
deb_viewport_set( ifp, left, top, right, bottom )
FBIO	*ifp;
int	left, top, right, bottom;
{
	fb_log( "fb_viewport( fp,%4d,%4d,%4d,%4d )\n",
		left, top, right, bottom );
	return	1;
}

_LOCAL_ int
deb_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	fb_log( "fb_window( fp,%4d,%4d )\n", x, y );
	return	1;
}

_LOCAL_ int
deb_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	fb_log( "fb_zoom( fp, %d, %d )\n", x, y );
	return	1;
}

_LOCAL_ int
deb_cinit_bitmap( ifp, bitmap )
FBIO	*ifp;
long	*bitmap;
{
	fb_log( "fb_setcursor( fp, 0x%x )\n", bitmap );
	return	1;
}

_LOCAL_ int
deb_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	fb_log( "fb_cursor( fp, %d,%4d,%4d )\n", mode, x, y );
	return	1;
}

_LOCAL_ int
deb_cscreen_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	fb_log( "fb_scursor( fp, %d,%4d,%4d )\n", mode, x, y );
	return	1;
}
