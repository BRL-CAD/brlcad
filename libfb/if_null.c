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
 *	This software is Copyright (C) 1989-2004 by the United States Army.
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

_LOCAL_ int	null_open(),
		null_close(),
		null_clear(),
		null_read(),
		null_write(),
		null_rmap(),
		null_wmap(),
		null_view(),
		null_getview(),
		null_setcursor(),
		null_cursor(),
		null_getcursor(),
		null_readrect(),
		null_writerect(),
		null_poll(),
		null_flush(),
		null_free(),
		null_help();

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
null_open( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	FB_CK_FBIO(ifp);
	if( width > 0 )
		ifp->if_width = width;
	if( height > 0 )
		ifp->if_height = height;

	return(0);
}

_LOCAL_ int
null_close( ifp )
FBIO	*ifp;
{
	return(0);
}

_LOCAL_ int
null_clear( ifp, pp )
FBIO	*ifp;
RGBpixel	*pp;
{
	return(0);
}

_LOCAL_ int
null_read( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	return(count);
}

_LOCAL_ int
null_write( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
int	count;
{
	return(count);
}

_LOCAL_ int
null_rmap( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	return(0);
}

_LOCAL_ int
null_wmap( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	return(0);
}

_LOCAL_ int
null_view( ifp, xcenter, ycenter, xzoom, yzoom )
FBIO	*ifp;
int	xcenter, ycenter;
int	xzoom, yzoom;
{
	/*fb_sim_view( ifp, xcenter, ycenter, xzoom, yzoom );*/
	return(0);
}

_LOCAL_ int
null_getview( ifp, xcenter, ycenter, xzoom, yzoom )
FBIO	*ifp;
int	*xcenter, *ycenter;
int	*xzoom, *yzoom;
{
	/*fb_sim_getview( ifp, xcenter, ycenter, xzoom, yzoom );*/
	return(0);
}

_LOCAL_ int
null_setcursor( ifp, bits, xbits, ybits, xorig, yorig )
FBIO	*ifp;
unsigned char *bits;
int	xbits, ybits;
int	xorig, yorig;
{
	return(0);
}

_LOCAL_ int
null_cursor( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	/*fb_sim_cursor(ifp, mode, x, y);*/
	return(0);
}

_LOCAL_ int
null_getcursor( ifp, mode, x, y )
FBIO	*ifp;
int	*mode;
int	*x, *y;
{
	/*fb_sim_getcursor(ifp, mode, x, y);*/
	return(0);
}

_LOCAL_ int
null_readrect( ifp, xmin, ymin, width, height, pp )
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
RGBpixel	*pp;
{
	return( width*height );
}

_LOCAL_ int
null_writerect( ifp, xmin, ymin, width, height, pp )
FBIO	*ifp;
int	xmin, ymin;
int	width, height;
RGBpixel	*pp;
{
	return( width*height );
}

_LOCAL_ int
null_poll( ifp )
FBIO	*ifp;
{
	return(0);
}

_LOCAL_ int
null_flush( ifp )
FBIO	*ifp;
{
	return(0);
}

_LOCAL_ int
null_free( ifp )
FBIO	*ifp;
{
	return(0);
}

_LOCAL_ int
null_help( ifp )
FBIO	*ifp;
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
