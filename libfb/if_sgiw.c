/*
  Authors -
	Paul R. Stay
	Gary S. Moss
	Mike J. Muuss

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
#include <gl2/immed.h>
#undef RED

#include "fb.h"
#include "./fblocal.h"

#define Abs( x_ )	((x_) < 0 ? -(x_) : (x_))
#define WIN_L	(1023-511-4)
#define WIN_R	(1023-4)
#define WIN_B	4
#define WIN_T	(511+4)

#define MAP_RESERVED	16			/* # slots reserved */
#define MAP_SIZE	(1024-MAP_RESERVED)	/* # slots available */
#define MAP_TOL		8			/* pixel delta */

/*
 *  Defines for dealing with SGI Graphics Engine Pipeline
 */
union gepipe {
	short	s;
	long	l;
	float	f;
};

/**#define MC_68010 xx	/* not a turbo */
#ifdef MC_68010
#define GEPIPE	((union gepipe *)0X00FD5000)
#define GE	0x00FD5000
#else
#define GEPIPE	((union gepipe *)0X60001000)
#define GE	0x60001000
#endif
#define GEP_END(_p)	((union gepipe *)(((char *)(_p))-0x1000))	/* 68000 efficient 0xFd4000 */
#define CMOV2S(_p,_x,_y) { \
		(_p)->l = 0x0008001A; \
		(_p)->s = 0x0912; \
		(_p)->s = (_x); \
		(_p)->s = (_y); \
		GEP_END(hole)->s = (0xFF<<8)|8; \
		}

/* These globals need to be in the FBIO structure.			*/
static int		xzoom = 1, yzoom = 1;
static int		special_zoom = 0;

static Cursor	cursor =
	{
#include "./sgicursor.h"
	};
_LOCAL_ int	sgw_dopen(),
		sgw_dclose(),
		sgw_dreset(),
		sgw_dclear(),
		sgw_bread(),
		sgw_bwrite(),
		sgw_cmread(),
		sgw_cmwrite(),
		sgw_viewport_set(),
		sgw_window_set(),
		sgw_zoom_set(),
		sgw_cinit_bitmap(),
		sgw_cmemory_addr(),
		sgw_cscreen_addr();

/* This is the ONLY thing that we "export" */
FBIO sgiw_interface =
		{
		sgw_dopen,
		sgw_dclose,
		sgw_dreset,
		sgw_dclear,
		sgw_bread,
		sgw_bwrite,
		fb_null,
		fb_null,
		sgw_viewport_set,
		fb_null,
		sgw_zoom_set,
		sgw_cinit_bitmap,
		sgw_cmemory_addr,
		fb_null,
		"Silicon Graphics IRIS, in 12-bit mode, for windows",
		1024,			/* max width */
		768,			/* max height */
		"/dev/sgiw",
		512,			/* current/default width  */
		512,			/* current/default height */
		-1,			/* file descriptor */
		PIXEL_NULL,		/* page_base */
		PIXEL_NULL,		/* page_curp */
		PIXEL_NULL,		/* page_endp */
		-1,			/* page_no */
		0,			/* pdirty */
		0L,			/* page_curpos */
		0L,			/* page_pixels */
		0			/* debug */
		};


_LOCAL_ int _sgw_cmap_flag;

_LOCAL_ ColorMap _sgw_cmap;

static RGBpixel	rgb_table[MAP_SIZE];
static int	rgb_ct = 0;

#define Map_Close( pp, qq ) \
	( \
	((dp = (pp)[RED]-(qq)[RED]) > -MAP_TOL && dp < MAP_TOL ) \
     && ((dp = (pp)[GRN]-(qq)[GRN]) > -MAP_TOL && dp < MAP_TOL ) \
     &&	((dp = (pp)[BLU]-(qq)[BLU]) > -MAP_TOL && dp < MAP_TOL ) \
	)

Colorindex
get_Color_Index( pixelp )
register RGBpixel	*pixelp;
	{	register int	i;
		int	best;
		register RGBpixel	*sp;
		static int	groused = 0;

	/* Find match in color table if one exists.			*/
	for( i = 0, sp = rgb_table; i < rgb_ct; sp++, i++ )
		{	register int	dp;
		if( Map_Close( *pixelp, *sp ) )
			{
			/* Found close match.				*/
			return	(Colorindex)(i + MAP_RESERVED);
			}
		}
	/* Allocate new entry in color table if there's room.		*/
	if( i < MAP_SIZE )
		{
		COPYRGB( rgb_table[i], *pixelp);
		rgb_ct++;
		mapcolor(	(Colorindex)(i + MAP_RESERVED),
				(short) (*pixelp)[RED],
				(short) (*pixelp)[GRN],
				(short) (*pixelp)[BLU]
				);
		return	(Colorindex)(i + MAP_RESERVED);
		}
	/* Find closest match in color table.				*/
	if( !groused )  {
		groused = 1;
		fb_log( "Color table now full, will use closest matches.\n" );
	}
	best = 0;
	for( i = 0, sp = rgb_table; i < MAP_SIZE; sp++, i++ )
		{	register int	diff;
			register int	d;
			register int	min_diff = 256;
		d = ((int)(*pixelp[RED])) - ((int)(*sp)[RED]);
		if( (diff = Abs(d)) >= min_diff )
			continue;
		d = ((int)(*pixelp[GRN])) - ((int)(*sp)[GRN]);
		if( (diff += Abs(d)) >= min_diff )
			continue;
		d = ((int)(*pixelp[BLU])) - ((int)(*sp)[BLU]);
		if( (diff += Abs(d)) >= min_diff )
			continue;
		/* [i]'th element is the best so far... */
		min_diff = diff;
		best = i;
		}
	return	(Colorindex)(best + MAP_RESERVED);
	}

_LOCAL_ int
sgw_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{	register Colorindex i;
	
	if( ismex() )
		{
		prefposition( 1023-511-4, 1023-4, 4, 511+4 );
		foreground();
		if( (ifp->if_fd = winopen( "framebuffer" )) == -1 )
			{
			fb_log( "No more graphics ports available.\n" );
			return	-1;
			}
		wintitle( "frame buffer" );
		}
	else
		ginit();
#if MAP_PREALLOCATED
	for( i = 0; i < MAP_SIZE; i++ )
		mapcolor( 	i+MAP_RESERVED,
				(short)((i % 10) + 1) * 25,
				(short)(((i / 10) % 10) + 1) * 25,
				(short)((i / 100) + 1) * 25
				);
#endif
	singlebuffer();
	gconfig();		/* Must be called after singlebuffer().	*/

	/* Build a linear "colormap" in case he wants to read it */
	sgw_cmwrite( ifp, COLORMAP_NULL );

	if ( width > ifp->if_max_width) 
		width = ifp->if_max_width;

	if ( height > ifp->if_max_height) 
		height = ifp->if_max_height;

	ifp->if_width = width;
	ifp->if_height = height;
	return	0;
}

_LOCAL_ int
sgw_dclose( ifp )
FBIO	*ifp;
{
	if( ismex() )
		; /* winclose( ifp->if_fd ); */
	else
		{
		greset();
		gexit();
		}
/** 	fb_log( "%d color table entries used.\n", rgb_ct );  **/
	return	0;	
}

_LOCAL_ int
sgw_dreset( ifp )
FBIO	*ifp;
{
	ginit();
	singlebuffer();
	gconfig();

	color(BLACK);
	clear();
	return	0;	
}

_LOCAL_ int
sgw_dclear( ifp, pp )
FBIO	*ifp;
RGBpixel	*pp;
{
	if ( pp != RGBPIXEL_NULL)
		color( get_Color_Index( pp ) );
	else
		color( BLACK );
	writemask( 0x3FF );
	clear();
	return	0;	
}

_LOCAL_ int
sgw_bread( ifp, x, y, pixelp, count )
FBIO	*ifp;
register int	x, y;
register RGBpixel	*pixelp;
int	count;
{	register union gepipe *hole = GEPIPE;
	short scan_count;
	Colorindex colors[1024];
	register int i;

	x *= xzoom;
	while( count > 0 )
		{	register short	ypos = y*yzoom;
		if ( count >= ifp->if_width )
			scan_count = ifp->if_width;
		else
			scan_count = count;
		if( xzoom == 1 && yzoom == 1 || special_zoom )
			{ /* No pixel replication, so read scan of pixels. */
			CMOV2S( hole, x, ypos );
			readpixels( scan_count, colors );
			}
		else
			{ /* We are sampling from rectangles
				(replicated pixels). */
			for( i = 0; i < scan_count; i++ )
				{
				CMOV2S( hole, x, ypos );
				x += xzoom;
				readpixels( 1, &colors[i] );
				}
			}
		for( i = 0; i < scan_count; i++, pixelp++) 
			{
#if MAP_PREALLOCATED
			colors[i] -= MAP_RESERVED;
			(*pixelp)[RED] =   (colors[i] % 10 + 1) * 25;
			colors[i] /= 10;
			(*pixelp)[GRN] = (colors[i] % 10 + 1) * 25;
			colors[i] /= 10;
			(*pixelp)[BLU] =  (colors[i] % 10 + 1) * 25;
#else
				register int	ci = colors[i] - MAP_RESERVED;
			if( ci >= 0 && ci < rgb_ct )
				{
				COPYRGB( *pixelp, rgb_table[ci]);
				}
			else
				(*pixelp)[RED] = (*pixelp)[GRN] = (*pixelp)[BLU] = 0;
#endif
			}
		count -= scan_count;
		x = 0;
		y++;
	}
	return	0;	
}

_LOCAL_ int
sgw_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
register int	x, y;
register RGBpixel	*pixelp;
int	count;
	{	register union gepipe *hole = GEPIPE;
		short scan_count;
		register int i;
	writemask( 0x3FF );
	x *= xzoom;
	while( count > 0 )
		{	register short	ypos = y*yzoom;
		if ( count >= ifp->if_width )
			scan_count = ifp->if_width;
		else
			scan_count = count;
		if( xzoom == 1 && yzoom == 1 || special_zoom )
			{	register Colorindex	colori;
			CMOV2S( hole, x, ypos );
			for( i = scan_count; i > 0; )
				{	register int	chunk;
				if( i <= 127 )
					chunk = i;
				else
					chunk = 127;
				hole->s = (chunk<<8)|8; /* GEpassthru */
				hole->s = 0xD;		 /* FBCdrawpixels */
				i -= chunk;
				for( ; chunk > 0; chunk--, pixelp++ )
					{
#if MAP_PREALLOCATED
					colori =  ((*pixelp)[RED]/26) + MAP_RESERVED;
					colori += ((*pixelp)[GRN]/26) * 10;
					colori += ((*pixelp)[BLU]/26) * 100;
#else
					colori = get_Color_Index( pixelp );
#endif
					hole->s = colori;
					}
				}
			GEP_END(hole)->s = (0xFF<<8)|8;	/* im_last_passthru(0) */
			}
		else
			for( i = 0; i < scan_count; i++, pixelp++ )
				{	register Colorindex	col;
					register Coord	l = x,
							b = ypos,
							r = x + xzoom - 1,
							t = ypos - yzoom + 1;
				CMOV2S( hole, x, ypos );
#if MAP_PREALLOCATED
				col =  ((*pixelp)[RED]/26) + MAP_RESERVED;
				col += ((*pixelp)[GRN]/26) * 10;
				col += ((*pixelp)[BLU]/26) * 100;
#else
				col = get_Color_Index( pixelp );
#endif
				color( col );
				im_rectf( l, b, r, t );
				x += xzoom;
				}
		count -= scan_count;
		x = 0;
		y++;
		}
	return	0;
	}

_LOCAL_ int
sgw_viewport_set( ifp, left, top, right, bottom )
FBIO	*ifp;
int	left, top, right, bottom;
{
#if 0
	viewport(	(Screencoord) left,
			(Screencoord) right,
			(Screencoord) top,
			(Screencoord) (bottom * fb2iris_scale)
			);
#endif
	return	0;
}

_LOCAL_ int
sgw_cmread( ifp, cmp )
FBIO	*ifp;
register ColorMap	*cmp;
{
	register int i;

	/* Just parrot back the stored colormap */
	for( i = 0; i < 255; i++)
	{
		cmp->cm_red[i] = _sgw_cmap.cm_red[i]<<8;
		cmp->cm_green[i] = _sgw_cmap.cm_green[i]<<8;
		cmp->cm_blue[i] = _sgw_cmap.cm_blue[i]<<8;
	}
	return	0;
}

_LOCAL_ int
sgw_cmwrite( ifp, cmp )
FBIO	*ifp;
register ColorMap	*cmp;
{
	register int i;

	if ( cmp == COLORMAP_NULL)  {
		for( i = 0; i < 255; i++)  {
			_sgw_cmap.cm_red[i] = i;
			_sgw_cmap.cm_green[i] = i;
			_sgw_cmap.cm_blue[i] = i;
		}
		_sgw_cmap_flag = FALSE;
		return	0;
	}
	
	for(i = 0; i < 255; i++)  {
		_sgw_cmap.cm_red[i] = cmp -> cm_red[i]>>8;
		_sgw_cmap.cm_green[i] = cmp-> cm_green[i]>>8; 
		_sgw_cmap.cm_blue[i] = cmp-> cm_blue[i]>>8;

	}
	_sgw_cmap_flag = TRUE;
	return	0;
}

_LOCAL_ int
sgw_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
	{
	if( x < 0 || y < 0 )
		{
		special_zoom = 1;
		x = y = 1;
		}
	else
		special_zoom = 0;
	xzoom = x;
	yzoom = y;
	return	0;
	}

_LOCAL_ int
sgw_cinit_bitmap( ifp, bitmap )
FBIO	*ifp;
long	bitmap[32];
	{	register int	i;
	for( i = 0; i < 16; i++ )
		cursor[i] = bitmap[15-i] & 0xFFFF;
	defcursor( 1, cursor );
	return	0;
	}

_LOCAL_ int
sgw_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
	{	register int		xpos, ypos;
		register union gepipe *hole = GEPIPE;
		static Colorindex	cursor_color = YELLOW;
			/* Color and bitmask ignored under MEX.	*/
	if( ! mode )
		{
		cursoff();
		setcursor( 0, 1, 0x2000 );
		return	0;
		}
	x *= xzoom;
	y *= yzoom;
	curson();
	setcursor( 1, cursor_color, 0x2000 );
	setvaluator( MOUSEX, x + WIN_L, 0, 1023 );
	setvaluator( MOUSEY, y + WIN_B, 0, 1023 );
	return	0;
	}
