/*
 * How to add a new device interface:
 *
 * Copy this file to if_devname.c
 * Do a global replace of DEVNAME with your devname.
 * Fill in the device description, max width and height, and
 *  shortname (what you will look it up as).
 * Set the unimplemented functions to "fb_null"
 *  (and remove the skeletons if you're tidy)
 * Go add an "ifdef IF_devname" to fb_generic.c.
 * Fix Makefile.loc in 3 places.
 * Replace this header.
 */

#include "./fblocal.h"

_LOCAL_ int	DEVNAME_dopen(),
		DEVNAME_dclose(),
		DEVNAME_dreset(),
		DEVNAME_dclear(),
		DEVNAME_bread(),
		DEVNAME_bwrite(),
		DEVNAME_cmread(),
		DEVNAME_cmwrite(),
		DEVNAME_viewport_set(),
		DEVNAME_window_set(),
		DEVNAME_zoom_set(),
		DEVNAME_cinit_bitmap(),
		DEVNAME_cmemory_addr(),
		DEVNAME_cscreen_addr();

/* This is the ONLY thing that we normally "export" */
FBIO DEVNAME_interface =  {
	DEVNAME_dopen,		/* device_open		*/
	DEVNAME_dclose,		/* device_close		*/
	DEVNAME_dreset,		/* device_reset		*/
	DEVNAME_dclear,		/* device_clear		*/
	DEVNAME_bread,		/* buffer_read		*/
	DEVNAME_bwrite,		/* buffer_write		*/
	DEVNAME_cmread,		/* colormap_read	*/
	DEVNAME_cmwrite,		/* colormap_write	*/
	DEVNAME_viewport_set,		/* viewport_set		*/
	DEVNAME_window_set,		/* window_set		*/
	DEVNAME_zoom_set,		/* zoom_set		*/
	DEVNAME_cinit_bitmap,		/* cursor_init_bitmap	*/
	DEVNAME_cmemory_addr,		/* cursor_move_memory_addr */
	DEVNAME_cscreen_addr,		/* cursor_move_screen_addr */
	"Device description",		/* device description	*/
	0,				/* max width		*/
	0,				/* max height		*/
	"/dev/shortname",		/* short device name	*/
	0,				/* default/current width  */
	0,				/* default/current height */
	-1,				/* file descriptor	*/
	PIXEL_NULL,			/* page_base		*/
	PIXEL_NULL,			/* page_curp		*/
	PIXEL_NULL,			/* page_endp		*/
	-1,				/* page_no		*/
	0,				/* page_ref		*/
	0L,				/* page_curpos		*/
	0L,				/* page_bytes		*/
	0L				/* page_pixels		*/
};

_LOCAL_ int
DEVNAME_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
}

_LOCAL_ int
DEVNAME_dclose( ifp )
FBIO	*ifp;
{
}

_LOCAL_ int
DEVNAME_dreset( ifp )
FBIO	*ifp;
{
}

_LOCAL_ int
DEVNAME_dclear( ifp, pp )
FBIO	*ifp;
RGBpixel	*pp;
{
}

_LOCAL_ int
DEVNAME_bread( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
}

_LOCAL_ int
DEVNAME_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
}

_LOCAL_ int
DEVNAME_cmread( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
}

_LOCAL_ int
DEVNAME_cmwrite( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
}

_LOCAL_ int
DEVNAME_viewport_set( ifp, left, top, right, bottom )
FBIO	*ifp;
int	left, top, right, bottom;
{
}

_LOCAL_ int
DEVNAME_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
}

_LOCAL_ int
DEVNAME_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
}

_LOCAL_ int
DEVNAME_cinit_bitmap( ifp, bitmap )
FBIO	*ifp;
long	*bitmap;
{
}

_LOCAL_ int
DEVNAME_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
}

_LOCAL_ int
DEVNAME_cscreen_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
}
