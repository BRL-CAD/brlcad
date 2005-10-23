/*                       I F _ N U L L . C
 * BRL-CAD
 *
 * Copyright (C) 1989-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** \addtogroup if */
/*@{*/
/** @file if_null.c
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
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>

#include "machine.h"
#include "fb.h"
#include "./fblocal.h"


_LOCAL_ int	null_open(FBIO *ifp, char *file, int width, int height),
		null_close(FBIO *ifp),
		null_clear(FBIO *ifp, unsigned char *pp),
		null_read(FBIO *ifp, int x, int y, unsigned char *pixelp, int count),
		null_write(FBIO *ifp, int x, int y, const unsigned char *pixelp, int count),
		null_rmap(FBIO *ifp, ColorMap *cmp),
		null_wmap(FBIO *ifp, const ColorMap *cmp),
		null_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom),
		null_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom),
		null_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig),
		null_cursor(FBIO *ifp, int mode, int x, int y),
		null_getcursor(FBIO *ifp, int *mode, int *x, int *y),
		null_readrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp),
		null_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp),
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
null_clear(FBIO *ifp, unsigned char *pp)
{
	return(0);
}

_LOCAL_ int
null_read(FBIO *ifp, int x, int y, unsigned char *pixelp, int count)
{
	return(count);
}

_LOCAL_ int
null_write(FBIO *ifp, int x, int y, const unsigned char *pixelp, int count)
{
	return(count);
}

_LOCAL_ int
null_rmap(FBIO *ifp, ColorMap *cmp)
{
	return(0);
}

_LOCAL_ int
null_wmap(FBIO *ifp, const ColorMap *cmp)
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
null_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig)
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
null_readrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
	return( width*height );
}

_LOCAL_ int
null_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
