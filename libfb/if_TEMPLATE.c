/*
 *  How to add a new device interface:
 *
 *  Copy this file to if_devname.c
 *  Do a global replace of DEVNAME with your devname.
 *   (In the interest of non-flexnames, DEVNAME should be no more
 *   than three characters; except perhaps for DEVNAME_interface)
 *  Fill in the device description, max width and height,
 *   default width and height, and shortname (what you will
 *   look it up as).
 *  Set the unimplemented functions to "fb_null"
 *   (and remove the skeletons if you're tidy)
 *  Go add an "ifdef IF_DEVNAME" to fb_generic.c (two places).
 *  Add defines to ../Cakefile.defs.
 *  Replace this header.
 */

#include "fb.h"
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
		DEVNAME_curs_set(),
		DEVNAME_cmemory_addr(),
		DEVNAME_cscreen_addr(),
		DEVNAME_help();

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
	DEVNAME_curs_set,		/* curs_set		*/
	DEVNAME_cmemory_addr,		/* cursor_move_memory_addr */
	DEVNAME_cscreen_addr,		/* cursor_move_screen_addr */
	DEVNAME_help,			/* help message		*/
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
	0,				/* page_dirty		*/
	0L,				/* page_curpos		*/
	0L,				/* page_pixels		*/
	0				/* debug		*/
};

_LOCAL_ int
DEVNAME_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	return(0);
}

_LOCAL_ int
DEVNAME_dclose( ifp )
FBIO	*ifp;
{
	return(0);
}

_LOCAL_ int
DEVNAME_dreset( ifp )
FBIO	*ifp;
{
	return(0);
}

_LOCAL_ int
DEVNAME_dclear( ifp, pp )
FBIO	*ifp;
RGBpixel	*pp;
{
	return(0);
}

_LOCAL_ int
DEVNAME_bread( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	return(count);
}

_LOCAL_ int
DEVNAME_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	return(count);
}

_LOCAL_ int
DEVNAME_cmread( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	return(0);
}

_LOCAL_ int
DEVNAME_cmwrite( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	return(0);
}

_LOCAL_ int
DEVNAME_viewport_set( ifp, left, top, right, bottom )
FBIO	*ifp;
int	left, top, right, bottom;
{
	return(0);
}

_LOCAL_ int
DEVNAME_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	return(0);
}

_LOCAL_ int
DEVNAME_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	return(0);
}

_LOCAL_ int
DEVNAME_curs_set( ifp, bits, xbits, ybits, xorig, yorig )
FBIO	*ifp;
unsigned char *bits;
int	xbits, ybits;
int	xorig, yorig;
{
	return(0);
}

_LOCAL_ int
DEVNAME_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	return(0);
}

_LOCAL_ int
DEVNAME_cscreen_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	return(0);
}

_LOCAL_ int
DEVNAME_help( ifp )
FBIO	*ifp;
{
	fb_log( "Description: %s\n", DEVNAME_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width/height: %d %d\n",
		DEVNAME_interface.if_max_width,
		DEVNAME_interface.if_max_height );
	fb_log( "Default width/height: %d %d\n",
		DEVNAME_interface.if_width,
		DEVNAME_interface.if_height );
	return(0);
}
