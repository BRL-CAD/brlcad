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

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "fb.h"
#include "externs.h"
#include "./fblocal.h"

_LOCAL_ int	DEVNAME_open(FBIO *ifp, char *file, int width, int height),
		DEVNAME_close(FBIO *ifp),
		DEVNAME_clear(FBIO *ifp, unsigned char *pp),
		DEVNAME_read(FBIO *ifp, int x, int y, unsigned char *pixelp, int count),
		DEVNAME_write(FBIO *ifp, int x, int y, const unsigned char *pixelp, int count),
		DEVNAME_rmap(FBIO *ifp, ColorMap *cmp),
		DEVNAME_wmap(FBIO *ifp, const ColorMap *cmp),
		DEVNAME_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom),
		DEVNAME_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom),
		DEVNAME_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig),
		DEVNAME_cursor(FBIO *ifp, int mode, int x, int y),
		DEVNAME_getcursor(FBIO *ifp, int *mode, int *x, int *y),
		DEVNAME_readrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp),
		DEVNAME_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp),
		DEVNAME_bwreadrect(),
		DEVNAME_bwwriterect(),
		DEVNAME_poll(FBIO *ifp),
		DEVNAME_flush(FBIO *ifp),
		DEVNAME_free(FBIO *ifp),
		DEVNAME_help(FBIO *ifp);

/* This is the ONLY thing that we normally "export" */
FBIO DEVNAME_interface =  {
	0,			/* magic number slot	*/
	DEVNAME_open,		/* open device		*/
	DEVNAME_close,		/* close device		*/
	DEVNAME_clear,		/* clear device		*/
	DEVNAME_read,		/* read	pixels		*/
	DEVNAME_write,		/* write pixels		*/
	DEVNAME_rmap,		/* read colormap	*/
	DEVNAME_wmap,		/* write colormap	*/
	DEVNAME_view,		/* set view		*/
	DEVNAME_getview,	/* get view		*/
	DEVNAME_setcursor,	/* define cursor	*/
	DEVNAME_cursor,		/* set cursor		*/
	DEVNAME_getcursor,	/* get cursor		*/
	DEVNAME_readrect,	/* read rectangle	*/
	DEVNAME_writerect,	/* write rectangle	*/
	DEVNAME_bwreadrect,	/* read bw rectangle	*/
	DEVNAME_bwwriterect,	/* write bw rectangle	*/
	DEVNAME_poll,		/* process events	*/
	DEVNAME_flush,		/* flush output		*/
	DEVNAME_free,		/* free resources	*/
	DEVNAME_help,		/* help message		*/
	"Device description",	/* device description	*/
	0,			/* max width		*/
	0,			/* max height		*/
	"/dev/shortname",	/* short device name	*/
	0,			/* default/current width  */
	0,			/* default/current height */
	-1,			/* select file desc	*/
	-1,			/* file descriptor	*/
	1, 1,			/* zoom			*/
	0, 0,			/* window center	*/
	0, 0, 0,		/* cursor		*/
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
DEVNAME_open(FBIO *ifp, char *file, int width, int height)
{
	FB_CK_FBIO(ifp);
	return(0);
}

_LOCAL_ int
DEVNAME_close(FBIO *ifp)
{
	return(0);
}

_LOCAL_ int
DEVNAME_clear(FBIO *ifp, unsigned char *pp)
{
	return(0);
}

_LOCAL_ int
DEVNAME_read(FBIO *ifp, int x, int y, unsigned char *pixelp, int count)
{
	return(count);
}

_LOCAL_ int
DEVNAME_write(FBIO *ifp, int x, int y, const unsigned char *pixelp, int count)
{
	return(count);
}

_LOCAL_ int
DEVNAME_rmap(FBIO *ifp, ColorMap *cmp)
{
	return(0);
}

_LOCAL_ int
DEVNAME_wmap(FBIO *ifp, const ColorMap *cmp)
{
	return(0);
}

_LOCAL_ int
DEVNAME_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
	return(0);
}

_LOCAL_ int
DEVNAME_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
	return(0);
}

_LOCAL_ int
DEVNAME_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig)
{
	return(0);
}

_LOCAL_ int
DEVNAME_cursor(FBIO *ifp, int mode, int x, int y)
{
	return(0);
}

_LOCAL_ int
DEVNAME_getcursor(FBIO *ifp, int *mode, int *x, int *y)
{
	return(0);
}

_LOCAL_ int
DEVNAME_readrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
	return( width*height );
}

_LOCAL_ int
DEVNAME_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
	return( width*height );
}

_LOCAL_ int
DEVNAME_poll(FBIO *ifp)
{
	return(0);
}

_LOCAL_ int
DEVNAME_flush(FBIO *ifp)
{
	return(0);
}

_LOCAL_ int
DEVNAME_free(FBIO *ifp)
{
	return(0);
}

_LOCAL_ int
DEVNAME_help(FBIO *ifp)
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
