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
 *  Set DEVNAME_readrect to fb_sim_readrect, and DEVNAME_writerect
 *   to fb_sim_writerect, if not implemented.
 *  Make DEVNAME_free call DEVNAME_close if not implemented.
 *  Go add an "ifdef IF_DEVNAME" to fb_generic.c (two places).
 *  Add defines to ../Cakefile.defs.
 *  Replace this header.
 */

#include "fb.h"
#include "./fblocal.h"

_LOCAL_ int	DEVNAME_open(),
		DEVNAME_close(),
		DEVNAME_reset(),
		DEVNAME_clear(),
		DEVNAME_read(),
		DEVNAME_write(),
		DEVNAME_rmap(),
		DEVNAME_wmap(),
		DEVNAME_viewport(),
		DEVNAME_window(),
		DEVNAME_zoom(),
		DEVNAME_setcursor(),
		DEVNAME_cursor(),
		DEVNAME_scursor(),
		DEVNAME_readrect(),
		DEVNAME_writerect(),
		DEVNAME_flush(),
		DEVNAME_free(),
		DEVNAME_help();

/* This is the ONLY thing that we normally "export" */
FBIO DEVNAME_interface =  {
	DEVNAME_open,		/* open device	*/
	DEVNAME_close,		/* close device	*/
	DEVNAME_reset,		/* reset device	*/
	DEVNAME_clear,		/* clear device	*/
	DEVNAME_read,		/* read	pixels	*/
	DEVNAME_write,		/* write pixels */
	DEVNAME_rmap,		/* rmap - read colormap	*/
	DEVNAME_wmap,		/* wmap - write colormap */
	DEVNAME_viewport,	/* viewport set	*/
	DEVNAME_window,		/* window set	*/
	DEVNAME_zoom,		/* zoom set	*/
	DEVNAME_setcursor,	/* setcursor - define cursor	*/
	DEVNAME_cursor,		/* cursor - memory address	*/
	DEVNAME_scursor,	/* scursor - screen address	*/
	DEVNAME_readrect,	/* readrect - read rectangle	*/
	DEVNAME_writerect,	/* writerect - write rectangle	*/
	DEVNAME_flush,		/* flush output	*/
	DEVNAME_free,		/* free resources */
	DEVNAME_help,		/* help message	*/
	"Device description",	/* device description	*/
	0,			/* max width		*/
	0,			/* max height		*/
	"/dev/shortname",	/* short device name	*/
	0,			/* default/current width  */
	0,			/* default/current height */
	-1,			/* file descriptor	*/
	PIXEL_NULL,		/* page_base		*/
	PIXEL_NULL,		/* page_curp		*/
	PIXEL_NULL,		/* page_endp		*/
	-1,			/* page_no		*/
	0,			/* page_dirty		*/
	0L,			/* page_curpos		*/
	0L,			/* page_pixels		*/
	0			/* debug		*/
};

_LOCAL_ int
DEVNAME_open( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	return(0);
}

_LOCAL_ int
DEVNAME_close( ifp )
FBIO	*ifp;
{
	return(0);
}

_LOCAL_ int
DEVNAME_reset( ifp )
FBIO	*ifp;
{
	return(0);
}

_LOCAL_ int
DEVNAME_clear( ifp, pp )
FBIO	*ifp;
RGBpixel	*pp;
{
	return(0);
}

_LOCAL_ int
DEVNAME_read( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	return(count);
}

_LOCAL_ int
DEVNAME_write( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	return(count);
}

_LOCAL_ int
DEVNAME_rmap( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	return(0);
}

_LOCAL_ int
DEVNAME_wmap( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	return(0);
}

_LOCAL_ int
DEVNAME_viewport( ifp, left, top, right, bottom )
FBIO	*ifp;
int	left, top, right, bottom;
{
	return(0);
}

_LOCAL_ int
DEVNAME_window( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	return(0);
}

_LOCAL_ int
DEVNAME_zoom( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	return(0);
}

_LOCAL_ int
DEVNAME_setcursor( ifp, bits, xbits, ybits, xorig, yorig )
FBIO	*ifp;
unsigned char *bits;
int	xbits, ybits;
int	xorig, yorig;
{
	return(0);
}

_LOCAL_ int
DEVNAME_cursor( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	return(0);
}

_LOCAL_ int
DEVNAME_scursor( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	return(0);
}

_LOCAL_ int
DEVNAME_readrect( ifp, xmin, ymin, width, height, pp )
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
RGBpixel	*pp;
{
	return( width*height );
}

_LOCAL_ int
DEVNAME_writerect( ifp, xmin, ymin, width, height, pp )
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
RGBpixel	*pp;
{
	return( width*height );
}

_LOCAL_ int
DEVNAME_flush( ifp )
FBIO	*ifp;
{
	return(0);
}

_LOCAL_ int
DEVNAME_free( ifp )
FBIO	*ifp;
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
