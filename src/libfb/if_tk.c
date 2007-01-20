/*                      I F _ D E B U G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file if_tk.c
 *
 *  Reports all calls to fb_log().
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#include "machine.h"
#include "fb.h"
#include "./fblocal.h"

#include <tcl.h>
#include <tk.h>

Tcl_Interp *fbinterp;
Tk_Window fbwin;
Tk_PhotoHandle fbphoto;

_LOCAL_ int	tk_open(FBIO *ifp, char *file, int width, int height),
		tk_close(FBIO *ifp),
		tk_clear(FBIO *ifp, unsigned char *pp),
		tk_read(FBIO *ifp, int x, int y, unsigned char *pixelp, int count),
		tk_write(FBIO *ifp, int x, int y, const unsigned char *pixelp, int count),
		tk_rmap(FBIO *ifp, ColorMap *cmp),
		tk_wmap(FBIO *ifp, const ColorMap *cmp),
		tk_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom),
		tk_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom),
		tk_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig),
		tk_cursor(FBIO *ifp, int mode, int x, int y),
		tk_getcursor(FBIO *ifp, int *mode, int *x, int *y),
		tk_readrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp),
		tk_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp),
		tk_bwreadrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp),
		tk_bwwriterect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp),
		tk_poll(FBIO *ifp),
		tk_flush(FBIO *ifp),
		tk_free(FBIO *ifp),
		tk_help(FBIO *ifp);

/* This is the ONLY thing that we "export" */
FBIO tk_interface = {
	0,
	tk_open,
	tk_close,
	tk_clear,
	tk_read,
	tk_write,
	tk_rmap,
	tk_wmap,
	tk_view,
	tk_getview,
	tk_setcursor,
	tk_cursor,
	tk_getcursor,
	tk_readrect,
	tk_writerect,
	tk_bwreadrect,
	tk_bwwriterect,
	tk_poll,
	tk_flush,
	tk_free,
	tk_help,
	"Debugging Interface",
	32*1024,		/* max width */
	32*1024,		/* max height */
	"/dev/tk",
	512,			/* current/default width */
	512,			/* current/default height */
	-1,			/* select fd */
	-1,			/* file descriptor */
	1, 1,			/* zoom */
	256, 256,		/* window center */
	0, 0, 0,		/* cursor */
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
tk_open(FBIO *ifp, char *file, int width, int height)
{
	FB_CK_FBIO(ifp);
	if( file == (char *)NULL )
		fb_log( "fb_open( 0x%lx, NULL, %d, %d )\n",
			(unsigned long)ifp, width, height );
	else
		fb_log( "fb_open( 0x%lx, \"%s\", %d, %d )\n",
			(unsigned long)ifp, file, width, height );

	/* check for default size */
	if( width <= 0 )
		width = ifp->if_width;
	if( height <= 0 )
		height = ifp->if_height;

	/* set debug bit vector */
	if( file != NULL ) {
		char *cp;
		for( cp = file; *cp != '\0' && !isdigit(*cp); cp++ ) ;
		sscanf( cp, "%d", &ifp->if_debug );
	} else {
		ifp->if_debug = 0;
	}

	/* Give the user whatever width was asked for */
	ifp->if_width = width;
	ifp->if_height = height;

	fbinterp = Tcl_CreateInterp();
	const char *cmd = "package require Tk";

	if (Tcl_Init(fbinterp) == TCL_ERROR) {
	    fb_log( "Tcl_Init returned error in fb_open." );
	}

	if (Tcl_Eval(fbinterp, cmd) != TCL_OK) {
	    fb_log( "Error returned attempting to start tk in fb_open." );
	}
	

	fbwin = Tk_MainWindow(fbinterp);

	Tk_GeometryRequest(fbwin, width, height);
	
	Tk_MakeWindowExist(fbwin);

	char image_create_cmd[255];
	sprintf(image_create_cmd, 
		"image create photo fb_tk_photo -height %d -width %d",
		 width, height);

	if (Tcl_Eval(fbinterp, image_create_cmd) != TCL_OK) {
	    fb_log( "Error returned attempting to create image in fb_open." );
	}

	if ((fbphoto = Tk_FindPhoto(fbinterp, "fb_tk_photo")) == NULL ) {
	    fb_log( "Image creation unsuccessful in fb_open." );
	}

	char canvas_create_cmd[255];
	sprintf(canvas_create_cmd, 
		"canvas .fb_tk_canvas -height %d -width %d", width, height);

	if (Tcl_Eval(fbinterp, canvas_create_cmd) != TCL_OK) {
	    fb_log( "Error returned attempting to create canvas in fb_open." );
	}

	const char canvas_pack_cmd[255] = 
		"pack .fb_tk_canvas -fill both -expand true";

	if (Tcl_Eval(fbinterp, canvas_pack_cmd) != TCL_OK) {
	    fb_log( "Error returned attempting to pack canvas in fb_open. %s", 
		Tcl_GetStringResult(fbinterp));
	}

	const char place_image_cmd[255] = 
		".fb_tk_canvas create image 0 0 -image fb_tk_photo -anchor nw";
	if (Tcl_Eval(fbinterp, place_image_cmd) != TCL_OK) {
	    fb_log( "Error returned attempting to place image in fb_open. %s", 
		Tcl_GetStringResult(fbinterp));
	}

	while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));

	return	0;
}

_LOCAL_ int
tk_close(FBIO *ifp)
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_close( 0x%lx )\n", (unsigned long)ifp );
	return	0;
}

_LOCAL_ int
tk_clear(FBIO *ifp, unsigned char *pp)
{
	FB_CK_FBIO(ifp);
	if( pp == 0 )
		fb_log( "fb_clear( 0x%lx, NULL )\n", (unsigned long)ifp );
	else
		fb_log( "fb_clear( 0x%lx, &[%d %d %d] )\n",
			(unsigned long)ifp,
			(int)(pp[RED]), (int)(pp[GRN]),
			(int)(pp[BLU]) );
	return	0;
}

_LOCAL_ int
tk_read(FBIO *ifp, int x, int y, unsigned char *pixelp, int count)
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_read( 0x%lx,%4d,%4d, 0x%lx, %d )\n",
		(unsigned long)ifp, x, y,
		(unsigned long)pixelp, count );
	return	count;
}

_LOCAL_ int
tk_write(FBIO *ifp, int x, int y, const unsigned char *pixelp, int count)
{
	int	i;
	unsigned char pixp = *pixelp;

	FB_CK_FBIO(ifp);
	fb_log( "fb_write( 0x%lx,%4d,%4d, 0x%lx, %d )\n",
		(unsigned long)ifp, x, y,
		(unsigned long)pixelp, count );

	/* write them out, four per line */
	if( ifp->if_debug & FB_DEBUG_RW ) {
		for( i = 0; i < count; i++ ) {
			if( i % 4 == 0 )
				fb_log( "%4d:", i );
			fb_log( "  [%3d,%3d,%3d]", *(pixelp+(i*3)+RED),
				*(pixelp+(i*3)+GRN), *(pixelp+(i*3)+BLU) );
			if( i % 4 == 3 )
				fb_log( "\n" );
		}
		if( i % 4 != 0 )
			fb_log( "\n" );
	}

	Tk_PhotoImageBlock block = {
		&pixp,
		count,
		1,
		3 * ifp->if_width,
		3,
		{
		    RED,
		    GRN,
		    BLU,
		    0
		}
	};

	Tk_PhotoPutBlock(fbphoto, &block, x, ifp->if_height-y, count, 1, TK_PHOTO_COMPOSITE_SET);

	return	count;
}

_LOCAL_ int
tk_rmap(FBIO *ifp, ColorMap *cmp)
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_rmap( 0x%lx, 0x%lx )\n",
		(unsigned long)ifp, (unsigned long)cmp );
	return	0;
}

_LOCAL_ int
tk_wmap(FBIO *ifp, const ColorMap *cmp)
{
	int	i;

	FB_CK_FBIO(ifp);
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
tk_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_view( 0x%lx,%4d,%4d,%4d,%4d )\n",
		(unsigned long)ifp, xcenter, ycenter, xzoom, yzoom );
	fb_sim_view( ifp, xcenter, ycenter, xzoom, yzoom );
	return	0;
}

_LOCAL_ int
tk_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_getview( 0x%lx, 0x%x, 0x%x, 0x%x, 0x%x )\n",
		(unsigned long)ifp, xcenter, ycenter, xzoom, yzoom );
	fb_sim_getview( ifp, xcenter, ycenter, xzoom, yzoom );
	fb_log( " <= %d %d %d %d\n",
		*xcenter, *ycenter, *xzoom, *yzoom );
	return	0;
}

_LOCAL_ int
tk_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig)
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_setcursor( 0x%lx, 0x%lx, %d, %d, %d, %d )\n",
		(unsigned long)ifp, bits, xbits, ybits, xorig, yorig );
	return	0;
}

_LOCAL_ int
tk_cursor(FBIO *ifp, int mode, int x, int y)
{
	fb_log( "fb_cursor( 0x%lx, %d,%4d,%4d )\n",
		(unsigned long)ifp, mode, x, y );
	fb_sim_cursor( ifp, mode, x, y );
	return	0;
}

_LOCAL_ int
tk_getcursor(FBIO *ifp, int *mode, int *x, int *y)
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_getcursor( 0x%lx, 0x%x,0x%x,0x%x )\n",
		(unsigned long)ifp, mode, x, y );
	fb_sim_getcursor( ifp, mode, x, y );
	fb_log( " <= %d %d %d\n", *mode, *x, *y );
	return	0;
}

_LOCAL_ int
tk_readrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_readrect( 0x%lx, (%4d,%4d), %4d,%4d, 0x%lx )\n",
		(unsigned long)ifp, xmin, ymin, width, height,
		(unsigned long)pp );
	return( width*height );
}

_LOCAL_ int
tk_writerect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_writerect( 0x%lx,%4d,%4d,%4d,%4d, 0x%lx )\n",
		(unsigned long)ifp, xmin, ymin, width, height,
		(unsigned long)pp );
	return( width*height );
}

_LOCAL_ int
tk_bwreadrect(FBIO *ifp, int xmin, int ymin, int width, int height, unsigned char *pp)
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_bwreadrect( 0x%lx, (%4d,%4d), %4d,%4d, 0x%lx )\n",
		(unsigned long)ifp, xmin, ymin, width, height,
		(unsigned long)pp );
	return( width*height );
}

_LOCAL_ int
tk_bwwriterect(FBIO *ifp, int xmin, int ymin, int width, int height, const unsigned char *pp)
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_bwwriterect( 0x%lx,%4d,%4d,%4d,%4d, 0x%lx )\n",
		(unsigned long)ifp, xmin, ymin, width, height,
		(unsigned long)pp );
	return( width*height );
}

_LOCAL_ int
tk_poll(FBIO *ifp)
{
	FB_CK_FBIO(ifp);
	while (Tcl_DoOneEvent(TCL_ALL_EVENTS|TCL_DONT_WAIT));
	fb_log( "fb_poll( 0x%lx )\n", (unsigned long)ifp );
	return	0;
}

_LOCAL_ int
tk_flush(FBIO *ifp)
{
	FB_CK_FBIO(ifp);
	fb_log( "if_flush( 0x%lx )\n", (unsigned long)ifp );
	return	0;
}

_LOCAL_ int
tk_free(FBIO *ifp)
{
	FB_CK_FBIO(ifp);
	fb_log( "fb_free( 0x%lx )\n", (unsigned long)ifp );
	return	0;
}

/*ARGSUSED*/
_LOCAL_ int
tk_help(FBIO *ifp)
{
	FB_CK_FBIO(ifp);
	fb_log( "Description: %s\n", tk_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width/height: %d %d\n",
		tk_interface.if_max_width,
		tk_interface.if_max_height );
	fb_log( "Default width/height: %d %d\n",
		tk_interface.if_width,
		tk_interface.if_height );
	fb_log( "\
Usage: /dev/tk[#]\n\
  where # is a optional bit vector from:\n\
    1    debug buffered I/O calls\n\
    2    show colormap entries in rmap/wmap calls\n\
    4    show actual pixel values in read/write calls\n" );
	/*8    buffered read/write values - ifdef'd out*/

	return	0;
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
