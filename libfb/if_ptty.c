/*
  Author -
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
#include <fcntl.h>
#include "fb.h"
#include "./dmdfb.h"
#include "./fblocal.h"

#define MAX_DIMENSION	256
#define CVT2DMD( _i )		((_i)/(ifp->if_width/MAX_DIMENSION)*3)
#define INTENSITY_FACTOR	(1.0/26.0)
#define R_NTSC			(0.35*INTENSITY_FACTOR)
#define G_NTSC			(0.55*INTENSITY_FACTOR)
#define B_NTSC			(0.10*INTENSITY_FACTOR)
static int	over_sampl;
_LOCAL_ int	ptty_device_open(),
		ptty_device_close(),
		ptty_device_clear(),
		ptty_buffer_read(),
		ptty_buffer_write(),
		ptty_viewport_set(),
		ptty_window_set(),
		ptty_zoom_set(),
		ptty_cmemory_addr(),
		ptty_background_set(),
		ptty_animate(),
		ptty_setsize();

FBIO ptty_interface =
		{
		ptty_device_open,
		ptty_device_close,
		fb_null,			/* reset */
		ptty_device_clear,
		ptty_buffer_read,
		ptty_buffer_write,
		fb_null,			/* read colormap */
		fb_null,			/* write colormap */
		ptty_viewport_set,
		ptty_window_set,
		ptty_zoom_set,
		fb_null,			/* curs_set */
		ptty_cmemory_addr,
		fb_null,			/* screen addr cursor move */
		"Unix pseudo-tty Interface",
		800,
		1024,
		"/dev/tty",
		800,			/* XXX - are these good defaults? */
		1024,
		-1,
		PIXEL_NULL,
		PIXEL_NULL,
		PIXEL_NULL,
		-1,
		0,
		0L,
		0L,
		0
		};

_LOCAL_	int	output_Scan();
_LOCAL_ int	put_Run();
_LOCAL_ int	rgb_To_Dither_Val();

/*ARGSUSED*/
_LOCAL_ int
ptty_device_open( ifp, ptty_name, width, height )
FBIO	*ifp;
char	*ptty_name;
int	width, height;
	{
	/* Check for default size */
	if( width == 0 )
		width = ifp->if_width;
	if( height == 0 )
		height = ifp->if_height;

	ifp->if_width = width;
	ifp->if_height = height;
	over_sampl = ifp->if_width / MAX_DIMENSION;
	if( (ifp->if_fd = open( ptty_name, O_RDWR, 0 )) == -1 )
		return	-1;
	(void) ptty_setsize( ifp, width, height );
	return	ifp->if_fd;
	}

_LOCAL_ int
ptty_device_close( ifp )
FBIO	*ifp;
	{
	return	close( ifp->if_fd ) == -1 ? -1 : 0;
	}

_LOCAL_ int
ptty_device_clear( ifp, bgpp )
FBIO	*ifp;
RGBpixel	*bgpp;
	{	static char	ptty_buf[2] = { PT_CLEAR, NULL };
	return	write( ifp->if_fd, ptty_buf, 1 ) < 1 ? -1 : 0;
	}

_LOCAL_ int
ptty_buffer_write( ifp, x, y, pixelp, ct )
register FBIO	*ifp;
int		x, y;
RGBpixel		*pixelp;
long		ct;
	{	static char	ptty_buf[10];
		register int	scan_ct;
/*	y = ifp->if_width-1-y;		/* 1st quadrant */
	(void) sprintf( ptty_buf, "%c%04d%04d", PT_SEEK, CVT2DMD( x ), CVT2DMD( y ));
	if( write( ifp->if_fd, ptty_buf, 9 ) < 9 )
		return	-1;
	for( ; ct > 0; pixelp += scan_ct, x = 0 )
		{
		if( ct > ifp->if_width - x )
			scan_ct = ifp->if_width - x;
		else
			scan_ct = ct;
		if( output_Scan( ifp, pixelp, scan_ct ) == -1 )
			return	-1;
		ct -= scan_ct;
		}
	return	0;
	}

_LOCAL_ int
ptty_buffer_read( ifp, x, y, pixelp, ct )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
long	ct;
	{
/*	y = ifp->if_width-1-y;		/* 1st quadrant */
#if 0 /* Not yet implemented. */
	if( read( ifp->if_fd, (char *) pixelp, (int)(sizeof(RGBpixel)*ct) )
		< sizeof(RGBpixel)*ct
		)
		return	-1;
#endif
	return	0;
	}

_LOCAL_ int
ptty_viewport_set( ifp, l, t, r, b )
FBIO	*ifp;
int	l, r, t, b;
	{	static char	ptty_buf[18];
	l = CVT2DMD( l );
	t = CVT2DMD( t );
	r = CVT2DMD( r ) + LAYER_BORDER*2;
	b = CVT2DMD( b ) + LAYER_BORDER*2;
	(void) sprintf( ptty_buf, "%c%04d%04d%04d%04d", PT_VIEWPORT, l, t, r, b );
	return	write( ifp->if_fd, ptty_buf, 17 ) == 17 ? 0 : -1;
	}

_LOCAL_ int
ptty_setsize( ifp, width, height )
FBIO	*ifp;
int	width, height;
	{	static char	ptty_buf[10];
	width = CVT2DMD( width );
	height = CVT2DMD( height );
	(void) sprintf( ptty_buf, "%c%04d%04d", PT_SETSIZE, width, height );
	return	write( ifp->if_fd, ptty_buf, 9 ) == 9 ? 0 : -1;
	}

_LOCAL_ int
ptty_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
	{	static char	ptty_buf[10];
/*	y = ifp->if_width-1-y;		/* 1st quadrant */
	(void) sprintf(	ptty_buf, "%c%04d%04d", PT_WINDOW, CVT2DMD( x ), CVT2DMD( y ) );
	return	write( ifp->if_fd, ptty_buf, 9 ) == 9 ? 0 : -1;
	}

_LOCAL_ int
ptty_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
	{	static char	ptty_buf[10];
	x /= over_sampl; /* Correct for scale-down.	*/
	y /= over_sampl;
	(void) sprintf( ptty_buf, "%c%04d%04d", PT_ZOOM, x, y );
	return	write( ifp->if_fd, ptty_buf, 9 ) == 9 ? 0 : -1;
	}

_LOCAL_ int
ptty_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
	{	static char	ptty_buf[11];
/*	y = ifp->if_width-1-y;		/* 1st quadrant */
	(void) sprintf(	ptty_buf,
			"%c%1d%04d%04d",
			PT_CURSOR, mode, CVT2DMD( x ), CVT2DMD( y )
			);
	return	write( ifp->if_fd, ptty_buf, 10 ) == 10 ? 0 : -1;
	}

#ifdef never
_LOCAL_ int
ptty_animate( ifp, nframes, framesz, fps )
FBIO	*ifp;
int	nframes, framesz, fps;
	{	static char	ptty_buf[14];
	(void) sprintf(	ptty_buf,
			"%c%04d%04d%04d",
			PT_ANIMATE, nframes, framesz, fps
			);
	return	write( ifp->if_fd, ptty_buf, 13 ) == 13 ? 0 : -1;
	}
#endif

/*	o u t p u t _ S c a n ( )
	Take care of conversion to monochrome image, and run-length encoding
	of 1 scan line of 'ct' pixels.
	Output to stdout.
 */
_LOCAL_ int
output_Scan( ifp, pixels, ct )
FBIO		*ifp;
register RGBpixel	*pixels;
int		ct;
	{	register int	i, j;
		static char	output_buf[MAX_DIMENSION+1];
		register char	*p = output_buf;
		static int	line_ct = 1;
	/* Reduce image through pixel averaging to 256 x 256.		*/
	for( i = 0; i < ct; i += over_sampl, p++ )
		{	register int	val;
		for( j = 0, val = 0; j < over_sampl; j++, pixels++ )
			val += rgb_To_Dither_Val( pixels );
		val /= over_sampl;	/* Avg. horizontal summation.	*/
		if( line_ct == 1 )
			*p = val;
		else
			*p += val;
		if( line_ct == over_sampl )
			*p /= over_sampl;	/* Avg. vertical sum.	*/
		}
	if( line_ct < over_sampl )
		{
		line_ct++;
		return	0;
		}
	else
		line_ct = 1;
	*p = '\0';

	/* Output buffer as run-length encoded byte stream.		*/
	{	register int	byte_ct;
	for( i = 0, p = output_buf, byte_ct = 1; i < 256; i++, p++ )
		{
		if( *p == *(p+1) )
			byte_ct++;
		else
			{
			if( put_Run( ifp, byte_ct, (int) *p ) == -1 )
				return	-1;
			byte_ct = 1;
			}
		}
	if( byte_ct > 1 && put_Run( ifp, byte_ct, (int) p[-1] ) == -1 )
		return	-1;
	}
	return	write( ifp->if_fd, "\n", 1 );
	}

_LOCAL_ int
put_Run( ifp, ct, val )
register FBIO	*ifp;
register int	ct;
int		val;
	{	static char	ptty_buf[4];
/*	(void) fprintf( stderr, "put_Run( %d, %d )\n", ct, val ); */
	while( ct > 0 )
		{
		(void) sprintf(	ptty_buf,
				"%c%c%c",
				PT_WRITE, (ct >= 64 ? 64 : ct ) + ' ',
				val+'0'
				);
		if( write( ifp->if_fd, ptty_buf, 3 ) < 3 )
			return	-1;
		ct -= 64;
		}
	return	0;
	}

_LOCAL_ int
rgb_To_Dither_Val( pixel )
register RGBpixel	*pixel;
	{
	return	(R_NTSC * (*pixel)[RED] + G_NTSC * (*pixel)[GRN]
		+ B_NTSC * (*pixel)[BLU]);
	}
