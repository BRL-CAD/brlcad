/*
		F B _ P A G E D _ I O . C
 
  Buffered frame buffer IO routines:
    fb_ioinit( fbp )
    fb_seek( fbp, x, y )
    fb_tell( fbp )
    fb_rpixel( fbp, pixelp )
    fb_wpixel( fbp, pixelp )
    fbflush( fbp )

  Authors -
	Mike J. Muuss
	Douglas P. Kingston III
	Gary S. Moss

  Source -
	SECAD/VLD Computing Consortium, Bldg 394
	The U. S. Army Ballistic Research Laboratory
	Aberdeen Proving Ground, Maryland  21005-5066
  
  Copyright Notice -
	This software is Copyright (C) 1986 by the United States Army.
	All rights reserved.

	$Header$ (BRL)
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include "fb.h"
#include "./fblocal.h"

#define PAGE_BYTES	(63*1024L)		/* Max # of bytes/dma. */
#define PAGE_PIXELS	(((PAGE_BYTES/sizeof(RGBpixel))/ifp->if_width)\
			 *ifp->if_width)
#define PAGE_SCANS	(ifp->if_ppixels/ifp->if_width)

_LOCAL_ int	_pagein(), _pageout();
 
/*	f b i o i n i t ( )
	This initialization routine must be called before any buffered
	I/O routines in this file are used.
 */
int
fb_ioinit( ifp )
register FBIO	*ifp;
{
	ifp->if_pno = -1;		/* Force _pagein() initially.	*/
	ifp->if_pixcur = 0L;		/* Initialize pixel number.	*/
	if( ifp->if_pbase == PIXEL_NULL ) {
		/* Only allocate buffer once. */
		ifp->if_ppixels = PAGE_PIXELS;	/* Pixels/page.	*/
		ifp->if_pbytes = ifp->if_ppixels * sizeof(RGBpixel); /* Bytes/page.	*/
		if( (ifp->if_pbase = (RGBpixel *) malloc( ifp->if_pbytes ))
			== PIXEL_NULL ) {
			Malloc_Bomb(ifp->if_pbytes);
			return	-1;
		}
	}
	ifp->if_pcurp = ifp->if_pbase;	/* Initialize pointer.	*/
	ifp->if_pendp = ifp->if_pbase+ifp->if_ppixels;
	return	0;
}

int
fb_seek( ifp, x, y )
register FBIO	*ifp;
int	x, y;
{
	long	pos;
	long	pagepos;

	/* fb_log( "fb_seek( %d, %d )\n", x, y ); */
	if( x < 0 || y < 0 || x > 8*1024 || y > 8*1024 ) {
		fb_log(	"fb_seek : illegal address <%d,%d>.\n", x, y );
		return	-1;
	}
	pos = (((long) y * (long) ifp->if_width) + x) * sizeof(RGBpixel);
	pagepos = (long) ifp->if_pno * ifp->if_pbytes;
	if( pos < pagepos || pos >= (pagepos + ifp->if_pbytes) ) {
		if( ifp->if_pref )
			if( _pageout( ifp ) == - 1 )
				return	-1;
		if( _pagein( ifp, (int) (pos / (long) ifp->if_pbytes)) == -1 )
			return	-1;
	}
	ifp->if_pixcur = ((long) y * (long) ifp->if_width) + x;
	/* Compute new pixel pointer into page.				*/
	ifp->if_pcurp = ifp->if_pbase +
			(ifp->if_pixcur % ifp->if_ppixels);
	return	0;
}

int
fb_tell( ifp, xp, yp )
register FBIO	*ifp;
int	*xp, *yp;
{
	*yp = (int) (ifp->if_pixcur / ifp->if_width);
	*xp = (int) (ifp->if_pixcur % ifp->if_width);
	return	0;
}

int
fb_wpixel( ifp, pixelp )
register FBIO	*ifp;
RGBpixel 	*pixelp;
{
	if( ifp->if_pno == -1 )
		if( _pagein( ifp, ifp->if_pixcur / ifp->if_ppixels ) == -1 )
			return	-1;

	COPYRGB( *(ifp->if_pcurp), *pixelp );
	ifp->if_pcurp++;	/* position in page */
	ifp->if_pixcur++;	/* position in framebuffer */
	ifp->if_pref = 1;	/* page referenced (dirty) */
	if( ifp->if_pcurp >= ifp->if_pendp ) {
		if( _pageout( ifp ) == -1 )
			return	-1;
		ifp->if_pno = -1;
	}
	return	0;
}

int
fb_rpixel( ifp, pixelp )
register FBIO	*ifp;
RGBpixel	*pixelp;
{
	if( ifp->if_pno == -1 )
		if( _pagein( ifp, ifp->if_pixcur / ifp->if_ppixels ) == -1 )
			return	-1;

	COPYRGB( *pixelp, *(ifp->if_pcurp) );
	ifp->if_pcurp++;	/* position in page */
	ifp->if_pixcur++;	/* position in framebuffer */
	if( ifp->if_pcurp >= ifp->if_pendp ) {
		if( _pageout( ifp ) == -1 )
			return	-1;
		ifp->if_pno = -1;
	}
	return	0;
}

int
fb_flush( ifp )
register FBIO	*ifp;
{
	if( ifp->if_pref )
		if( _pageout( ifp ) == -1 )
			return	-1;

	ifp->if_pref = 0;
	return	0;
}

_LOCAL_ int
_pageout( ifp )
register FBIO	*ifp;
{
	/*fb_log( "_pageout(%d)\n", ifp->if_pno );*/
	if( ifp->if_pno < 0 )	/* Already paged out, return 1.	*/
		return	1;

	return	fb_write(	ifp,
				0,
				ifp->if_pno * PAGE_SCANS,
				ifp->if_pbase,
				ifp->if_ppixels
				);
}

_LOCAL_ int
_pagein( ifp, pageno )
register FBIO	*ifp;
int	pageno;
{
	/*fb_log( "_pagein(%d)\n", pageno );*/

	/* Set pixel pointer to beginning of page. */
	ifp->if_pcurp = ifp->if_pbase;
	ifp->if_pno = pageno;
	ifp->if_pref = 0;

	return	fb_read(	ifp,
				0,
				ifp->if_pno * PAGE_SCANS,
				ifp->if_pbase,
				ifp->if_ppixels
				);
}
