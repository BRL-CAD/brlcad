/*
  Author -
	Paul R. Stay

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
#include <gl.h>
#include "fb.h"
#include "./fblocal.h"

_LOCAL_ int	sgi_dopen(),
		sgi_dclose(),
		sgi_dreset(),
		sgi_dclear(),
		sgi_bread(),
		sgi_bwrite(),
		sgi_cmread(),
		sgi_cmwrite(),
		sgi_viewport_set(),
		sgi_window_set(),
		sgi_zoom_set(),
		sgi_cinit_bitmap(),
		sgi_cmemory_addr(),
		sgi_cscreen_addr();

/* This is the ONLY thing that we "export" */
FBIO sgi_interface =
		{
		sgi_dopen,
		sgi_dclose,
		sgi_dreset,
		sgi_dclear,
		sgi_bread,
		sgi_bwrite,
		fb_null,
		fb_null,
		sgi_viewport_set,
		fb_null,
		fb_null,
		fb_null,
		fb_null,
		fb_null,
		"Silicon Graphics IRIS",
		1024,			/* max width */
		768,			/* max height */
		"sgi",
		512,			/* current/default width  */
		512,			/* current/default height */
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
sgi_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	short x_pos, y_pos;	/* Lower corner of viewport */

	
	ifp->if_width = width;
	ifp->if_height = height;
	
	ginit();
	RGBmode();
	gconfig();		/* Must be called after RGBmode() */

	if ( width > ifp->if_max_width) 
		width = ifp->if_max_width;

	if ( height > ifp->if_max_height) 
		height = ifp->if_max_height;

	x_pos = ( ifp->if_max_width - width ) /2;
	y_pos = ( ifp->if_max_height - height ) /2;
}

_LOCAL_ int
sgi_dclose( ifp )
FBIO	*ifp;
{
	gexit();
}

_LOCAL_ int
sgi_dreset( ifp )
FBIO	*ifp;
{
	short x_pos, y_pos;	/* Lower corner of viewport */

	
	ginit();
	RGBmode();
	gconfig();

	RGBcolor( (short) 0, (short) 0, (short) 0);
	clear();
}

_LOCAL_ int
sgi_dclear( ifp, pp )
FBIO	*ifp;
Pixel	*pp;
{
	if ( pp != NULL)
		RGBcolor((short) pp->red, (short)pp->green, (short)pp->blue);
	else
		RGBcolor( (short) 0, (short) 0, (short) 0);
	clear();
}

_LOCAL_ int
sgi_bread( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
Pixel	*pixelp;
int	count;
{
	short scan_count;
	Coord xpos, ypos;
	static RGBvalue rr[1024], gg[1024], bb[1024];
	Pixel * pixptr;
	int first_time;
	register int i;
	int x_pos, y_pos;

	first_time = 1;
	

	pixelp = (Pixel *) malloc( sizeof( Pixel) * count );

	pixptr = pixelp;

	while( count > 0 )
	{
		if (first_time)
		{
			first_time = 0;
			scan_count = ifp->if_width - x;

			if ( count > scan_count )
				count -= scan_count;
			else
			{
				scan_count = count;
				count = 0;
			}
			xpos = x;
			ypos = ifp->if_height - y;
		}

		cmov2i( xpos, ypos-- );		/* move to current position */
		
		readRGB( scan_count, rr, gg, bb );

		for( i = 0; i < scan_count; i++, pixptr++)
		{
			pixptr->red = rr[i];
			pixptr->green = gg[i];
			pixptr->blue = bb[i];
		}

		if ( count >= ifp->if_max_width )
		{
			scan_count = ifp->if_max_width;
			count -= scan_count;
			x_pos = 0;
		} else
		{
			scan_count = 0;
			count = 0;
			x_pos = 0;
		}
	}
}

_LOCAL_ int
sgi_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
short	x, y;
Pixel	*pixelp;
short	count;
{
	short scan_count;
	short xpos, ypos;
	RGBvalue rr[1024], gg[1024], bb[1024];
	Pixel * pixptr;
	int first_time;
	register int i;
	int x_pos, y_pos;

	first_time = 1;
	
	pixptr = pixelp;

	while( count > 0 )
	{
		if (first_time)
		{
			first_time = 0;
			scan_count = ifp->if_width - x;
			if ( count > scan_count )
				count -= scan_count;
			else
			{
				scan_count = count;
				count = 0;
			}

			xpos = x;
			ypos = y;
		}

		cmov2s( xpos, ypos );		/* move to current position */


		for( i = 0; i < scan_count; i++, pixptr++)
		{
			rr[i] = (RGBvalue) pixptr->red;
			gg[i] = (RGBvalue) pixptr->green;
			bb[i] = (RGBvalue) pixptr->blue;
		}

		writeRGB( scan_count, rr, gg, bb ); 

		if ( count >= ifp->if_max_width )  {
			scan_count = ifp->if_max_width;
			count -= scan_count;
			x_pos = 0;
			ypos--;
		} else	{
			scan_count = 0;
			count = 0;
			x_pos = 0;
			ypos--;
		}
	}
}

_LOCAL_ int
sgi_viewport_set( ifp, left, top, right, bottom )
FBIO	*ifp;
int	left, top, right, bottom;
{
	viewport( left, right, top, bottom );
}

