/*
 *			I F _ S G I . C
 *
 *  SGI display interface.  By default, we operate in 24-bit (RGB) mode.
 *  However, when running under MEX, 12-bit mode is utilized (actually,
 *  only 10 bits are available, thanks to MEX).  Several flavors of
 *  MEX operation are defined, either a best-fit color match, or
 *  a pre-defined colorspace.  Each has advantages and disadvantages.
 *
 *  Authors -
 *	Paul R. Stay
 *	Gary S. Moss
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 *
 */
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

/* Local state variables within the FBIO structure */
/* If there are any more of these, they ought to go into a malloc()d struct */
#define if_xzoom	u1.l
#define if_yzoom	u2.l
#define if_special_zoom	u3.l
#define if_mode		u4.l
#define if_rgb_ct	u5.l

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
#define GE	(0X00FD5000)		/* Not a turbo */
#else
#define GE	(0X60001000)		/* A turbo */
#endif
#define GEPIPE	((union gepipe *)GE)
#define GEP_END(p)	((union gepipe *)(((char *)p)-0x1000))	/* 68000 efficient 0xFd4000 */

#define CMOV2S(_p,_x,_y) { \
		(_p)->l = 0x0008001A; \
		(_p)->s = 0x0912; \
		(_p)->s = (_x); \
		(_p)->s = (_y); \
		GEP_END(hole)->s = (0xFF<<8)|8; \
		}

static Cursor	cursor =
	{
#include "./sgicursor.h"
	};

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
		sgi_curs_set(),
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
		sgi_window_set,
		sgi_zoom_set,
		sgi_curs_set,
		sgi_cmemory_addr,
		fb_null,
		"Silicon Graphics IRIS",
		1024,			/* max width */
		768,			/* max height */
		"/dev/sgi",
		1024,			/* current/default width  */
		768,			/* current/default height */
		-1,			/* file descriptor */
		PIXEL_NULL,		/* page_base */
		PIXEL_NULL,		/* page_curp */
		PIXEL_NULL,		/* page_endp */
		-1,			/* page_no */
		0,			/* page_ref */
		0L,			/* page_curpos */
		0L,			/* page_pixels */
		0			/* debug */
		};


/* Interface to the 12-bit window versions of these routines */
int		sgw_dopen();
_LOCAL_ int	sgw_dclose(),
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

/* This one is not exported, but is used for roll-over to 12-bit mode */
static FBIO sgiw_interface =
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
		1024,			/* current/default width  */
		768,			/* current/default height */
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

_LOCAL_ int _sgi_cmap_flag;

_LOCAL_ ColorMap _sgi_cmap;

_LOCAL_ int
sgi_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	int x_pos, y_pos;	/* Lower corner of viewport */
	register int i;
	
	if( ismex() )  {
		char *name;

		name = ifp->if_name;
		*ifp = sgiw_interface;	/* struct copy */
		ifp->if_name = name;
		return( sgw_dopen( ifp, file, width, height ) );
	}
	gbegin();		/* not ginit() */
	RGBmode();
	gconfig();
	if( getplanes() < 24 )  {
		singlebuffer();
		gconfig();
		gexit();
		*ifp = sgiw_interface;	/* struct copy */
		return( sgw_dopen( ifp, file, width, height ) );
	}
	tpoff();		/* Turn off textport */
	cursoff();

	blanktime( 67 * 60 * 60L );	/* 1 hour blanking when fb open */

	/* Build a linear "colormap" in case he wants to read it */
	sgi_cmwrite( ifp, COLORMAP_NULL );

	if ( width > ifp->if_max_width) 
		width = ifp->if_max_width;

	if ( height > ifp->if_max_height) 
		height = ifp->if_max_height;

	ifp->if_width = width;
	ifp->if_height = height;

	ifp->if_xzoom = 1;	/* for zoom fakeout */
	ifp->if_yzoom = 1;	/* for zoom fakeout */
	/* Setup default cursor.					*/
	defcursor( 1, cursor );
	curorigin( 1, 0, 0 );
	return(0);
}

/*
 *			S G I _ D C L O S E
 *
 *  Finishing with a gexit() is mandatory.
 *  If we do a tpon() greset(), the text port pops up right
 *  away, spoiling the image.  Just doing the greset() causes
 *  the region of the text port to be disturbed, although the
 *  text port does not become readable.
 *
 *  Unfortunately, this means that the user has to run the
 *  "gclear" program before the screen can be used again,
 *  which is certainly a nuisance.  On the other hand, this
 *  means that images can be created AND READ BACK from
 *  separate programs, just like we do on the real framebuffers.
 */
_LOCAL_ int
sgi_dclose( ifp )
FBIO	*ifp;
{
	blanktime( 67 * 60 * 20L );	/* 20 minute blanking when fb closed */
#ifdef Ruins_Images
	tpon();			/* Turn on textport */
	greset();
	/* could be ginit(), gconfig() */
#endif
	gexit();
	return(0);
}

_LOCAL_ int
sgi_dreset( ifp )
FBIO	*ifp;
{
	ginit();
	RGBmode();
	gconfig();

	RGBcolor( (short) 0, (short) 0, (short) 0);
	clear();
	return(0);
}

_LOCAL_ int
sgi_dclear( ifp, pp )
FBIO	*ifp;
RGBpixel	*pp;
{
	if ( pp != NULL)
		RGBcolor((short)((*pp)[RED]), (short)((*pp)[GRN]), (short)((*pp)[BLU]));
	else
		RGBcolor( (short) 0, (short) 0, (short) 0);
	clear();
	return(0);
}

_LOCAL_ int
sgi_window_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	return(0);	/* Unable to do much */
}

_LOCAL_ int
sgi_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
{
	int npts;

	npts = ifp->if_width;
	if( npts > ifp->if_max_width )  npts = ifp->if_max_width;
	if( x < 1 )  x = 1;
	npts = npts / x;
	ifp->if_xzoom = ifp->if_max_width/npts;

	npts = ifp->if_height;
	if( npts > ifp->if_max_height )  npts = ifp->if_max_height;
	if( y < 1 )  y = 1;
	npts = npts / y;
	ifp->if_yzoom = ifp->if_max_height/npts;
	return(0);
}

_LOCAL_ int
sgi_bread( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
register RGBpixel	*pixelp;
int	count;
{
	int scan_count;
	int xpos, ypos;
	RGBvalue rr[1024], gg[1024], bb[1024];
	register int i;
	int ret;

	ret = count;	/* save count */
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
				(*pixelp)[RED] = rr[i];
				(*pixelp)[GRN] = gg[i];
				(*pixelp)[BLU] = bb[i];
			} else {
				(*pixelp)[RED] = _sgi_cmap.cm_red[ rr[i] ];
				(*pixelp)[GRN] = _sgi_cmap.cm_green[ gg[i] ];
				(*pixelp)[BLU] = _sgi_cmap.cm_blue[ bb[i] ];
			}
		}

		count -= scan_count;
		xpos = 0;
		ypos++;		/* Advance upwards */
	}
	return(ret);
}

_LOCAL_ int
sgi_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
register RGBpixel	*pixelp;
int	count;
{
	register union gepipe *hole = GEPIPE;
	register int scan_count;
	int xpos, ypos;
	register int i;
	int ret;

	ret = count;	/* save count */
	xpos = x;
	ypos = y;
	while( count > 0 )  {
		if( ypos >= ifp->if_max_width )  return(0);
		if ( count >= ifp->if_width )  {
			scan_count = ifp->if_width;
		} else	{
			scan_count = count;
		}

		hole->l = 0x0008001A;	/* passthru, */
		hole->s = 0x0912;		/* cmov2s */
		hole->s = xpos;
		hole->s = ypos;
		GEP_END(hole)->s = (0xFF<<8)|8;	/* im_last_passthru(0) */

		for( i=scan_count; i > 0; )  {
			register short chunk;

			if( ifp->if_xzoom > 1 )  {
				Coord l, b;
				RGBcolor( (short)((*pixelp)[RED]),
					(short)((*pixelp)[GRN]),
					(short)((*pixelp)[BLU]) );
				l = xpos * ifp->if_xzoom;
				b = ypos * ifp->if_yzoom;
				/* left bottom right top */
				rectf( l, b,
					l+ifp->if_xzoom, b+ifp->if_yzoom);
				i--;
				xpos++;
				pixelp++;
				continue;
			}
			if( i <= (127/3) )
				chunk = i;
			else
				chunk = 127/3;
			hole->s = ((chunk*3)<<8)|8;	/* GEpassthru */
			hole->s = 0xD;		/* FBCdrawpixels */
			i -= chunk;
			if ( _sgi_cmap_flag == FALSE )  {
				for( ; chunk>0; chunk--,pixelp++ )  {
					hole->s = (*pixelp)[RED];
					hole->s = (*pixelp)[GRN];
					hole->s = (*pixelp)[BLU];
				}
			} else {
				for( ; chunk>0; chunk--,pixelp++ )  {
					hole->s = _sgi_cmap.cm_red[(*pixelp)[RED]];
					hole->s = _sgi_cmap.cm_green[(*pixelp)[GRN]];
					hole->s = _sgi_cmap.cm_blue[(*pixelp)[BLU]];
				}
			}
		}
		GEP_END(hole)->s = (0xFF<<8)|8;	/* im_last_passthru(0) */

		count -= scan_count;
		xpos = 0;
		ypos++;		/* 1st quadrant */
	}
	return(ret);
}

_LOCAL_ int
sgi_viewport_set( ifp, left, top, right, bottom )
FBIO	*ifp;
int	left, top, right, bottom;
{
/*	viewport( left, right, top, bottom );*/
	return(0);
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

_LOCAL_ int
sgi_curs_set( ifp, bits, xbits, ybits, xorig, yorig )
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

_LOCAL_ int
sgi_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
	{
	if( ! mode )
		{
		cursoff();
		return	0;
		}
	x *= ifp->if_xzoom;
	y *= ifp->if_yzoom;
	curson();
	RGBcursor( 1, 255, 255, 0, 0xFF, 0xFF, 0xFF );
	setvaluator( MOUSEX, x, 0, 1023 );
	setvaluator( MOUSEY, y, 0, 1023 );
	return	0;
	}

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

static RGBpixel	rgb_table[MAP_SIZE];

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
	for( i = 0, sp = rgb_table; i < ifp->if_rgb_ct; sp++, i++ ) {
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
		COPYRGB( rgb_table[ifp->if_rgb_ct], *pixelp);
		mapcolor(	(Colorindex)ifp->if_rgb_ct,
				(short) (*pixelp)[RED],
				(short) (*pixelp)[GRN],
				(short) (*pixelp)[BLU]
				);
		return	(Colorindex)(ifp->if_rgb_ct++);
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


int
sgw_dopen( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{	register Colorindex i;

	ifp->if_mode = 0;
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

	ifp->if_xzoom = 1;	/* for zoom fakeout */
	ifp->if_yzoom = 1;	/* for zoom fakeout */
	ifp->if_special_zoom = 0;
	
	if( ismex() )
		{
		prefposition( WIN_L, WIN_R, WIN_B, WIN_T );
		foreground();
		if( (ifp->if_fd = winopen( "Frame buffer" )) == -1 )
			{
			fb_log( "No more graphics ports available.\n" );
			return	-1;
			}
		wintitle( "frame buffer" );
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
	ifp->if_rgb_ct = MAP_RESERVED;
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
	gconfig();	/* Must be called after singlebuffer().	*/

	/* Build a linear "colormap" in case he wants to read it */
	sgw_cmwrite( ifp, COLORMAP_NULL );
	/* Setup default cursor.					*/
	defcursor( 1, cursor );
	curorigin( 1, 0, 0 );
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
/** 	fb_log( "%d color table entries used.\n", ifp->if_rgb_ct );  **/
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
	int scan_count;
	Colorindex colors[1025];
	register int i;

	x *= ifp->if_xzoom;
	while( count > 0 )
		{	register short	ypos = y*ifp->if_yzoom;
		if ( count >= ifp->if_width )
			scan_count = ifp->if_width;
		else
			scan_count = count;
		if( (ifp->if_xzoom == 1 && ifp->if_yzoom == 1) || ifp->if_special_zoom )
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
				x += ifp->if_xzoom;
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
				if( ci < ifp->if_rgb_ct )
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

_LOCAL_ int
sgw_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
register int	x, y;
register RGBpixel	*pixelp;
int	count;
	{	register union gepipe *hole = GEPIPE;
		int scan_count;
		register int i;

	writemask( 0x3FF );
	x *= ifp->if_xzoom;
	while( count > 0 )
		{	register short	ypos = y*ifp->if_yzoom;
		if ( count >= ifp->if_width )
			scan_count = ifp->if_width;
		else
			scan_count = count;
		if( (ifp->if_xzoom == 1 && ifp->if_yzoom == 1) || ifp->if_special_zoom )
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
					register Coord	r = x + ifp->if_xzoom - 1,
							t = ypos + ifp->if_yzoom - 1;
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
				x += ifp->if_xzoom;
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
		cmp->cm_red[i] = _sgi_cmap.cm_red[i]<<8;
		cmp->cm_green[i] = _sgi_cmap.cm_green[i]<<8;
		cmp->cm_blue[i] = _sgi_cmap.cm_blue[i]<<8;
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
			_sgi_cmap.cm_red[i] = i;
			_sgi_cmap.cm_green[i] = i;
			_sgi_cmap.cm_blue[i] = i;
		}
		_sgi_cmap_flag = FALSE;
		return	0;
	}
	
	for(i = 0; i < 255; i++)  {
		_sgi_cmap.cm_red[i] = cmp -> cm_red[i]>>8;
		_sgi_cmap.cm_green[i] = cmp-> cm_green[i]>>8; 
		_sgi_cmap.cm_blue[i] = cmp-> cm_blue[i]>>8;

	}
	_sgi_cmap_flag = TRUE;
	return	0;
}

_LOCAL_ int
sgw_zoom_set( ifp, x, y )
FBIO	*ifp;
int	x, y;
	{
	if( x == 0 )  x = 1;
	if( y == 0 )  y = 1;
	if( x < 0 || y < 0 )
		{
		ifp->if_special_zoom = 1;
		x = y = 1;
		}
	else
		ifp->if_special_zoom = 0;
	ifp->if_xzoom = x;
	ifp->if_yzoom = y;
	return	0;
	}

_LOCAL_ int
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

_LOCAL_ int
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
	x *= ifp->if_xzoom;
	y *= ifp->if_yzoom;
	curson();
	setcursor( 1, cursor_color, 0x2000 );
	setvaluator( MOUSEX, x + WIN_L, 0, 1023 );
	setvaluator( MOUSEY, y + WIN_B, 0, 1023 );
	return	0;
	}
