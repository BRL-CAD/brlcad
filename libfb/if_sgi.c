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
		"/dev/sgi",
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


_LOCAL_ int _sgi_cmap_flag;

_LOCAL_ ColorMap _sgi_cmap;

_LOCAL_ int
sgi_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	short x_pos, y_pos;	/* Lower corner of viewport */
	register int i;
	
	gbegin();		/* not ginit() */
	RGBmode();
	gconfig();		/* Must be called after RGBmode() */


	/* Build a linear "colormap" in case he wants to read it */
	sgi_cmwrite( ifp, COLORMAP_NULL );

	if ( width > ifp->if_max_width) 
		width = ifp->if_max_width;

	if ( height > ifp->if_max_height) 
		height = ifp->if_max_height;

	ifp->if_width = width;
	ifp->if_height = height;
	
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
register Pixel	*pixelp;
int	count;
{
	short scan_count;
	short xpos, ypos;
	RGBvalue rr[1024], gg[1024], bb[1024];
	register int i;

	xpos = x;
	ypos = y;

	while( count > 0 )  {
		if ( count >= ifp->if_width )  {
			scan_count = ifp->if_width;
		} else	{
			scan_count = count;
		}

		cmov2s( xpos, ypos );		/* move to current position */
		readRGB( scan_count, rr, gg, bb ); 

		for( i = 0; i < scan_count; i++, pixelp++)  {
			if ( _sgi_cmap_flag == FALSE )  {
				pixelp->red = rr[i];
				pixelp->green = gg[i];
				pixelp->blue = bb[i];
			} else {
				pixelp->red = _sgi_cmap.cm_red[ rr[i] ];
				pixelp->green = _sgi_cmap.cm_green[ gg[i] ];
				pixelp->blue = _sgi_cmap.cm_blue[ bb[i] ];
			}
		}

		count -= scan_count;
		xpos = 0;
		ypos--;		/* LEFTOVER from 1st quadrant days */
	}
}

_LOCAL_ int
sgi_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
short	x, y;
register Pixel	*pixelp;
short	count;
{
	short scan_count;
	short xpos, ypos;
	RGBvalue rr[1024], gg[1024], bb[1024];
	register int i;

	xpos = x;
	ypos = y;
	while( count > 0 )  {
		if ( count >= ifp->if_width )  {
			scan_count = ifp->if_width;
		} else	{
			scan_count = count;
		}

		cmov2s( xpos, ypos );		/* move to current position */

		for( i = 0; i < scan_count; i++, pixelp++)  {
			if ( _sgi_cmap_flag == FALSE )  {
				rr[i] = (RGBvalue) pixelp->red;
				gg[i] = (RGBvalue) pixelp->green;
				bb[i] = (RGBvalue) pixelp->blue;
			}  else  {
				rr[i] = (RGBvalue) 
				    _sgi_cmap.cm_red[ (int) pixelp->red ];
				gg[i] = (RGBvalue) 
				    _sgi_cmap.cm_green[ (int) pixelp->green ];
				bb[i] = (RGBvalue) 
				    _sgi_cmap.cm_blue[ (int) pixelp->blue ];
			}
		}
		writeRGB( scan_count, rr, gg, bb ); 

		count -= scan_count;
		xpos = 0;
		ypos--;		/* LEFTOVER from 1st quadrant days */
	}
}

_LOCAL_ int
sgi_viewport_set( ifp, left, top, right, bottom )
FBIO	*ifp;
int	left, top, right, bottom;
{
	viewport( left, right, top, bottom );
}

_LOCAL_ int
sgi_cmread( ifp, cmp )
FBIO	*ifp;
register ColorMap	*cmp;
{
	register int i;

	/* Just parrot back the stored colormap */
	for( i = 0; i < 255; i++)
	{
		cmp->cm_red[i] = _sgi_cmap.cm_red[i]<<8;
		cmp->cm_green[i] = _sgi_cmap.cm_green[i]<<8;
		cmp->cm_blue[i] = _sgi_cmap.cm_blue[i]<<8;
	}
	return(0);
}

_LOCAL_ int
sgi_cmwrite( ifp, cmp )
FBIO	*ifp;
register ColorMap	*cmp;
{
	register int i;

	if ( cmp == COLORMAP_NULL)  {
		for( i = 0; i < 255; i++)  {
			_sgi_cmap.cm_red[i] = i;
			_sgi_cmap.cm_green[i] = i;
			_sgi_cmap.cm_blue[i] = i;
		}
		_sgi_cmap_flag = FALSE;
		return(0);
	}
	
	for(i = 0; i < 255; i++)  {
		_sgi_cmap.cm_red[i] = cmp -> cm_red[i]>>8;
		_sgi_cmap.cm_green[i] = cmp-> cm_green[i]>>8; 
		_sgi_cmap.cm_blue[i] = cmp-> cm_blue[i]>>8;

	}
	_sgi_cmap_flag = TRUE;
	return(0);
}
