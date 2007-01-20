/*                       I F _ S G I W . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
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
/** @file if_sgiw.c
 *  SGI window (MEX) oriented interface, which operates in 12-bit mode.
 *
 *  Authors -
 *	Paul R. Stay
 *	Gary S. Moss
 *	Mike J. Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <ctype.h>
#include <gl.h>
#include <gl2/immed.h>
#undef RED

#include "fb.h"
#include "./fblocal.h"

#define Abs( x_ )	((x_) < 0 ? -(x_) : (x_))

#define MARGIN	4			/* # pixels margin to screen edge */
#define BANNER	18			/* Size of MEX title banner */
#define WIN_L	(1024-ifp->if_width-MARGIN)
#define WIN_R	(1024-MARGIN)
#define WIN_B	MARGIN
#define WIN_T	(ifp->if_height+MARGIN)

#define MAP_RESERVED	16		/* # slots reserved by MEX */
#define MAP_SIZE	1024		/* # slots available, incl reserved */
#define MAP_TOL		28		/* pixel delta across all channels */
/* TOL of 28 gives good rendering of the dragon picture without running out */

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
HIDDEN int	sgw_dopen(),
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
		sgw_curs_set(),
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
		sgw_curs_set,
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


HIDDEN int _sgw_cmap_flag;

HIDDEN ColorMap _sgw_cmap;

static RGBpixel	rgb_table[MAP_SIZE];
static int	rgb_ct = MAP_RESERVED;

/*
 *			g e t _ C o l o r _ I n d e x
 */
Colorindex
get_Color_Index( pixelp )
register RGBpixel	*pixelp;
{
	register int		i;
	int			best = 7;
	register RGBpixel	*sp;
	static int		groused = 0;
	register int		min_diff = 128;

	/* Find best fit in existing table */
	best = 0;
	for( i = 0, sp = rgb_table; i < rgb_ct; sp++, i++ ) {
		register int	diff;
		register int	d;

		d = ((int)((*pixelp)[RED])) - ((int)(*sp)[RED]);
		if( (diff = Abs(d)) >= min_diff )
			continue;
		d = ((int)((*pixelp)[GRN])) - ((int)(*sp)[GRN]);
		if( (diff += Abs(d)) >= min_diff )
			continue;
		d = ((int)((*pixelp)[BLU])) - ((int)(*sp)[BLU]);
		if( (diff += Abs(d)) >= min_diff )
			continue;

		/* [i]'th element is the best so far... */
		if( (min_diff = diff) <= 2 )  {
			/* Great match */
			return( (Colorindex)i );
		}
		best = i;
	}

	/* Match found to within tolerance? */
	if( min_diff < MAP_TOL )
		return	(Colorindex)best;

	/* Allocate new entry in color table if there's room.		*/
	if( i < MAP_SIZE )  {
		COPYRGB( rgb_table[rgb_ct], *pixelp);
		mapcolor(	(Colorindex)rgb_ct,
				(short) (*pixelp)[RED],
				(short) (*pixelp)[GRN],
				(short) (*pixelp)[BLU]
				);
		return	(Colorindex)(rgb_ct++);
	}

	/* No room to add, use best we found */
	if( !groused )  {
		groused = 1;
		fb_log( "Color table now full, will use closest matches.\n" );
	}
	return	(Colorindex)best;
}

#define SET(i,r,g,b)	{ \
	rgb_table[i][RED] = r; \
	rgb_table[i][GRN] = g; \
	rgb_table[i][BLU] = b; }

#define if_mode		u1.l		/* Local flag for mode */

HIDDEN int
sgw_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{	register Colorindex i;

	if( file != NULL )  {
		register char *cp;
		/* "/dev/sgiw###" gives optional mode */
		for( cp = file; *cp != NULL && !isdigit(*cp); cp++ ) ;
		sscanf( cp, "%d", &ifp->if_mode );
	}
	if ( width > ifp->if_max_width - 2 * MARGIN)
		width = ifp->if_max_width - 2 * MARGIN;

	if ( height > ifp->if_max_height - 2 * MARGIN - BANNER)
		height = ifp->if_max_height - 2 * MARGIN - BANNER;

	ifp->if_width = width;
	ifp->if_height = height;

	if( ismex() )
		{
#if 0
		prefposition( WIN_L, WIN_R, WIN_B, WIN_T );
		foreground();
		if( (ifp->if_fd = winopen( "Frame buffer" )) == -1 )
			{
			fb_log( "No more graphics ports available.\n" );
			return	-1;
			}
		wintitle( "frame buffer" );
#endif
		}
	else
		ginit();

	/* The first 8 entries of the colormap are "known" colors */
	SET( 0, 000, 000, 000 );	/* BLACK */
	SET( 1, 255, 000, 000 );	/* RED */
	SET( 2, 000, 255, 000 );	/* GREEN */
	SET( 3, 255, 255, 000 );	/* YELLOW */
	SET( 4, 000, 000, 255 );	/* BLUE */
	SET( 5, 255, 000, 255 );	/* MAGENTA */
	SET( 6, 000, 255, 000 );	/* CYAN */
	SET( 7, 255, 255, 255 );	/* WHITE */

	/* Mode 0 builds color map on the fly */
	if( ifp->if_mode )
		{
		/* Mode 1 uses fixed color map */
		for( i = 0; i < MAP_SIZE-MAP_RESERVED; i++ )
			mapcolor( 	i+MAP_RESERVED,
					(short)((i % 10) + 1) * 25,
					(short)(((i / 10) % 10) + 1) * 25,
					(short)((i / 100) + 1) * 25
					);
		}

	singlebuffer();
	gconfig();		/* Must be called after singlebuffer().	*/

	/* Build a linear "colormap" in case he wants to read it */
	sgw_cmwrite( ifp, COLORMAP_NULL );
	/* Setup default cursor.					*/
	defcursor( 1, cursor );
	curorigin( 1, 0, 0 );
	return	0;
}

HIDDEN int
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

HIDDEN int
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

HIDDEN int
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

HIDDEN int
sgw_bread( ifp, x, y, pixelp, count )
FBIO	*ifp;
register int	x, y;
register RGBpixel	*pixelp;
int	count;
{	register union gepipe *hole = GEPIPE;
	short scan_count;
	Colorindex colors[1025];
	register int i;

	x *= xzoom;
	while( count > 0 )
		{	register short	ypos = y*yzoom;
		if ( count >= ifp->if_width )
			scan_count = ifp->if_width;
		else
			scan_count = count;
		if( (xzoom == 1 && yzoom == 1) || special_zoom )
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
			if( ifp->if_mode )
				{
				colors[i] -= MAP_RESERVED;
				(*pixelp)[RED] =   (colors[i] % 10 + 1) * 25;
				colors[i] /= 10;
				(*pixelp)[GRN] = (colors[i] % 10 + 1) * 25;
				colors[i] /= 10;
				(*pixelp)[BLU] =  (colors[i] % 10 + 1) * 25;
				}
			else
				{
				register int	ci = colors[i];
				if( ci < rgb_ct )
					{
					COPYRGB( *pixelp, rgb_table[ci]);
					}
				else
					(*pixelp)[RED] = (*pixelp)[GRN] = (*pixelp)[BLU] = 0;
				}
			}
		count -= scan_count;
		x = 0;
		y++;
	}
	return	0;
}

HIDDEN int
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
		if( (xzoom == 1 && yzoom == 1) || special_zoom )
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
					if( ifp->if_mode )
						{
						colori =  MAP_RESERVED +
							((*pixelp)[RED]/26);
						colori += ((*pixelp)[GRN]/26) * 10;
						colori += ((*pixelp)[BLU]/26) * 100;
						}
					else
						colori = get_Color_Index( pixelp );
					hole->s = colori;
					}
				}
			GEP_END(hole)->s = (0xFF<<8)|8;	/* im_last_passthru(0) */
			}
		else
			for( i = 0; i < scan_count; i++, pixelp++ )
				{	register Colorindex	col;
					register Coord	r = x + xzoom - 1,
							t = ypos + yzoom - 1;
				CMOV2S( hole, x, ypos );
				if( ifp->if_mode )
					{
					col =  MAP_RESERVED +
						((*pixelp)[RED]/26);
					col += ((*pixelp)[GRN]/26) * 10;
					col += ((*pixelp)[BLU]/26) * 100;
					}
				else
					col = get_Color_Index( pixelp );

				color( col );
				im_rectf( (Coord)x, (Coord)ypos, r, t );
				x += xzoom;
				}
		count -= scan_count;
		x = 0;
		y++;
		}
	return	0;
	}

HIDDEN int
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

HIDDEN int
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

HIDDEN int
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

HIDDEN int
sgw_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
	{
	if( x == 0 )  x = 1;
	if( y == 0 )  y = 1;
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

HIDDEN int
sgw_curs_set( ifp, bits, xbits, ybits, xorig, yorig )
FBIO	*ifp;
unsigned char	*bits;
int		xbits, ybits;
int		xorig, yorig;
	{	register int	y;
		register int	xbytes;
		Cursor		newcursor;
	/* Check size of cursor.					*/
	if( xbits < 0 )
		return	-1;
	if( xbits > 16 )
		xbits = 16;
	if( ybits < 0 )
		return	-1;
	if( ybits > 16 )
		ybits = 16;
	if( (xbytes = xbits / 8) * 8 != xbits )
		xbytes++;
	for( y = 0; y < ybits; y++ )
		{
		newcursor[y] = bits[(y*xbytes)+0] << 8 & 0xFF00;
		if( xbytes == 2 )
			newcursor[y] |= bits[(y*xbytes)+1] & 0x00FF;
		}
	defcursor( 1, newcursor );
	curorigin( 1, (short) xorig, (short) yorig );
	return	0;
	}

HIDDEN int
sgw_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
	{	static Colorindex	cursor_color = YELLOW;
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
