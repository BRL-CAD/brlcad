/*
 *	I F _ D E B U G
 */

#include <stdio.h>
#include "fb.h"
#include "./fblocal.h"

_LOCAL_ int	debug_device_open(),
		debug_device_close(),
		debug_device_reset(),
		debug_device_clear(),
		debug_buffer_read(),
		debug_buffer_write(),
		debug_colormap_read(),
		debug_colormap_write(),
		debug_viewport_set(),
		debug_window_set(),
		debug_zoom_set(),
		debug_cinit_bitmap(),
		debug_cmemory_addr(),
		debug_cscreen_addr(),
		debug_animate();

/* This is the ONLY thing that we "export" */
FBIO debug_interface =
		{
		debug_device_open,
		debug_device_close,
		debug_device_reset,
		debug_device_clear,
		debug_buffer_read,
		debug_buffer_write,
		debug_colormap_read,
		debug_colormap_write,
		debug_viewport_set,
		debug_window_set,
		debug_zoom_set,
		debug_cinit_bitmap,
		debug_cmemory_addr,
		debug_cscreen_addr,
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
debug_device_open( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	if( file == (char *)NULL )
		fprintf( stderr, "fb_open( fp, 0, %d, %d )\n",
			width, height );
	else
		fprintf( stderr, "fb_open( fp, \"%s\", %d, %d )\n",
			file, width, height );

	/* Give the user whatever with was asked for */
	ifp->if_width = width;
	ifp->if_height = height;

	return( 1 );
}

_LOCAL_ int
debug_device_close( ifp )
FBIO	*ifp;
{
	fprintf( stderr, "fb_close( fp )\n" );
}

_LOCAL_ int
debug_device_reset( ifp )
FBIO	*ifp;
{
	fprintf( stderr, "fb_reset( fp )\n" );
}

_LOCAL_ int
debug_device_clear( ifp, pp )
FBIO	*ifp;
Pixel	*pp;
{
	if( pp == 0 )
		fprintf( stderr, "fb_clear( fp, 0 )\n" );
	else
		fprintf( stderr, "fb_clear( fp, &[%d %d %d %d] )\n",
			(int)pp->red, (int)pp->green,
			(int)pp->blue, (int)pp->spare );
}

_LOCAL_ int
debug_buffer_read( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
Pixel	*pixelp;
int	count;
{
	fprintf( stderr, "fb_read( fp,%4d,%4d, 0x%x, %d )\n",
		x, y, pixelp, count );
}

_LOCAL_ int
debug_buffer_write( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
Pixel	*pixelp;
int	count;
{
	fprintf( stderr, "fb_write( fp,%4d,%4d, 0x%x, %d )\n",
		x, y, pixelp, count );
}

_LOCAL_ int
debug_colormap_read( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	fprintf( stderr, "fb_rmap( fp, 0x%x )\n", cmp );
}

_LOCAL_ int
debug_colormap_write( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	fprintf( stderr, "fb_wmap( fp, 0x%x )\n", cmp );
}

_LOCAL_ int
debug_viewport_set( ifp, left, top, right, bottom )
FBIO	*ifp;
int	left, top, right, bottom;
{
	fprintf( stderr, "fb_viewport( fp,%4d,%4d,%4d,%4d )\n",
		left, top, right, bottom );
}

_LOCAL_ int
debug_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	fprintf( stderr, "fb_window( fp,%4d,%4d )\n", x, y );
}

_LOCAL_ int
debug_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	fprintf( stderr, "fb_zoom( fp, %d, %d )\n", x, y );
}

_LOCAL_ int
debug_cinit_bitmap( ifp, bitmap )
FBIO	*ifp;
long	*bitmap;
{
	fprintf( stderr, "fb_setcursor( fp, 0x%x )\n", bitmap );
}

_LOCAL_ int
debug_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	fprintf( stderr, "fb_cursor( fp, %d,%4d,%4d )\n", mode, x, y );
}

_LOCAL_ int
debug_cscreen_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	fprintf( stderr, "fb_scursor( fp, %d,%4d,%4d )\n", mode, x, y );
}

_LOCAL_ int
debug_animate( ifp, nframes, size, fps )
FBIO	*ifp;
int	nframes, size, fps;
{
	fprintf( stderr, "fb_animate( fp, %d, %d, %d )\n",
			nframes, size, fps );
}
