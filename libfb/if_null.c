/*
 *			I F _ N U L L . C
 *
 *  A Null Frame Buffer.
 *  Useful for benchmarking or debugging.
 *
 *  Authors -
 *	Phillip Dykstra
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "fb.h"
#include "./fblocal.h"

_LOCAL_ int	null_open(FBIO *ifp, char *file, int width, int height),
		null_close(FBIO *ifp),
		null_clear(FBIO *ifp, RGBpixel (*pp)),
		null_read(FBIO *ifp, int x, int y, RGBpixel (*pixelp), int count),
		null_write(FBIO *ifp, int x, int y, RGBpixel (*pixelp), int count),
		null_rmap(FBIO *ifp, ColorMap *cmp),
		null_wmap(FBIO *ifp, ColorMap *cmp),
		null_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom),
		null_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom),
		null_setcursor(FBIO *ifp, unsigned char *bits, int xbits, int ybits, int xorig, int yorig),
		null_cursor(FBIO *ifp, int mode, int x, int y),
		null_getcursor(FBIO *ifp, int *mode, int *x, int *y),
		null_readrect(FBIO *ifp, int xmin, int ymin, int width, int height, RGBpixel (*pp)),
		null_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, RGBpixel (*pp)),
		null_poll(FBIO *ifp),
		null_flush(FBIO *ifp),
		null_free(FBIO *ifp),
		null_help(FBIO *ifp);

/* This is the ONLY thing that we normally "export" */
FBIO null_interface =  {
	0,
	null_open,		/* device_open		*/
	null_close,		/* device_close		*/
	null_clear,		/* device_clear		*/
	null_read,		/* buffer_read		*/
	null_write,		/* buffer_write		*/
	null_rmap,		/* colormap_read	*/
	null_wmap,		/* colormap_write	*/
	null_view,		/* set view		*/
	null_getview,		/* get view		*/
	null_setcursor,		/* define cursor	*/
	null_cursor,		/* set cursor		*/
	null_getcursor,		/* get cursor		*/
	null_readrect,		/* rectangle read	*/
	null_writerect,		/* rectangle write	*/
	null_readrect,		/* bw rectangle read	*/
	null_writerect,		/* bw rectangle write	*/
	null_poll,		/* handle events	*/
	null_flush,		/* flush output		*/
	null_free,		/* free resources	*/
	null_help,		/* help message		*/
	"Null Device",		/* device description	*/
	32*1024,		/* max width		*/
	32*1024,		/* max height		*/
	"/dev/null",		/* short device name	*/
	512,			/* default/current width  */
	512,			/* default/current height */
	-1,			/* select fd		*/
	-1,			/* file descriptor	*/
	1, 1,			/* zoom			*/
	256, 256,		/* window center	*/
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
null_open(FBIO *ifp, char *file, int width, int height)
{
	FB_CK_FBIO(ifp);
	if( width > 0 )
		ifp->if_width = width;
	if( height > 0 )
		ifp->if_height = height;

	return(0);
}

_LOCAL_ int
null_close(FBIO *ifp)
{
	return(0);
}

_LOCAL_ int
null_clear(FBIO *ifp, RGBpixel (*pp))
{
	return(0);
}

_LOCAL_ int
null_read(FBIO *ifp, int x, int y, RGBpixel (*pixelp), int count)
{
	return(count);
}

_LOCAL_ int
null_write(FBIO *ifp, int x, int y, RGBpixel (*pixelp), int count)
{
	return(count);
}

_LOCAL_ int
null_rmap(FBIO *ifp, ColorMap *cmp)
{
	return(0);
}

_LOCAL_ int
null_wmap(FBIO *ifp, ColorMap *cmp)
{
	return(0);
}

_LOCAL_ int
null_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
	/*fb_sim_view( ifp, xcenter, ycenter, xzoom, yzoom );*/
	return(0);
}

_LOCAL_ int
null_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
	/*fb_sim_getview( ifp, xcenter, ycenter, xzoom, yzoom );*/
	return(0);
}

_LOCAL_ int
null_setcursor(FBIO *ifp, unsigned char *bits, int xbits, int ybits, int xorig, int yorig)
{
	return(0);
}

_LOCAL_ int
null_cursor(FBIO *ifp, int mode, int x, int y)
{
	/*fb_sim_cursor(ifp, mode, x, y);*/
	return(0);
}

_LOCAL_ int
null_getcursor(FBIO *ifp, int *mode, int *x, int *y)
{
	/*fb_sim_getcursor(ifp, mode, x, y);*/
	return(0);
}

_LOCAL_ int
null_readrect(FBIO *ifp, int xmin, int ymin, int width, int height, RGBpixel (*pp))
{
	return( width*height );
}

_LOCAL_ int
null_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, RGBpixel (*pp))
{
	return( width*height );
}

_LOCAL_ int
null_poll(FBIO *ifp)
{
	return(0);
}

_LOCAL_ int
null_flush(FBIO *ifp)
{
	return(0);
}

_LOCAL_ int
null_free(FBIO *ifp)
{
	return(0);
}

_LOCAL_ int
null_help(FBIO *ifp)
{
	fb_log( "Description: %s\n", null_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width/height: %d %d\n",
		null_interface.if_max_width,
		null_interface.if_max_height );
	fb_log( "Default width/height: %d %d\n",
		null_interface.if_width,
		null_interface.if_height );
	fb_log( "Useful for Benchmarking/Debugging\n" );
	return(0);
}
