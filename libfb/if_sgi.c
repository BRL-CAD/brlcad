#undef _LOCAL_
#define _LOCAL_ /**/
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

#ifdef SYSV
#define bzero(p,cnt)	memset(p,'\0',cnt)
#endif

/* Local state variables within the FBIO structure */
extern char *malloc();

/*
 *  Defines for dealing with SGI Graphics Engine Pipeline
 */
union gepipe {
	unsigned short us;
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
_LOCAL_ void	sgw_inqueue();

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

/*
 *  Per SGI (window or device) state information
 *  Too much for the if_u[1-6] area now.
 */
struct sgiinfo {
	short	si_xzoom;
	short	si_yzoom;
	short	si_special_zoom;
	short	si_mode;
	int	si_rgb_ct;
	char	*si_save;
	short	si_curs_on;
};
#define	SGI(ptr) ((struct sgiinfo *)((ptr)->u1.p))
#define	SGIL(ptr) ((ptr)->u1.p)		/* left hand side version */

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
		char *name;

		singlebuffer();
		gconfig();
		name = ifp->if_name;
		*ifp = sgiw_interface;	/* struct copy */
		ifp->if_name = name;
		return( sgw_dopen( ifp, file, width, height ) );
	}
	tpoff();		/* Turn off textport */
	cursoff();

	blanktime( 67 * 60 * 60L );	/* 1 hour blanking when fb open */

	/* Build a linear "colormap" in case he wants to read it */
	sgi_cmwrite( ifp, COLORMAP_NULL );

	if( width <= 0 )
		width = ifp->if_width;
	if( height <= 0 )
		height = ifp->if_height;
	if ( width > ifp->if_max_width) 
		width = ifp->if_max_width;

	if ( height > ifp->if_max_height) 
		height = ifp->if_max_height;

	ifp->if_width = width;
	ifp->if_height = height;

	if( (SGIL(ifp) = (char *)calloc( 1, sizeof(struct sgiinfo) )) == NULL )  {
		fb_log("sgi_dopen:  sgiinfo malloc failed\n");
		return(-1);
	}

	SGI(ifp)->si_xzoom = 1;	/* for zoom fakeout */
	SGI(ifp)->si_yzoom = 1;	/* for zoom fakeout */
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
	if( SGIL(ifp) != NULL )
		(void)free( (char *)SGIL(ifp) );
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
	SGI(ifp)->si_xzoom = ifp->if_max_width/npts;

	npts = ifp->if_height;
	if( npts > ifp->if_max_height )  npts = ifp->if_max_height;
	if( y < 1 )  y = 1;
	npts = npts / y;
	SGI(ifp)->si_yzoom = ifp->if_max_height/npts;
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

	if( SGI(ifp)->si_curs_on )
		cursoff();		/* Cursor interferes with reading! */
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
	if( SGI(ifp)->si_curs_on )
		curson();		/* Cursor interferes with reading! */
	return(ret);
}

_LOCAL_ int
sgi_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel *pixelp;
int	count;
{
	register union gepipe *hole = GEPIPE;
	register int scan_count;
	int xpos, ypos;
	register int i;
	register char *cp;
	int ret;

	ret = count;	/* save count */
	xpos = x;
	ypos = y;
	cp = (char *)(*pixelp);
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

		for( i=scan_count; i > 0; )  {
			register short chunk;

			if( SGI(ifp)->si_xzoom > 1 )  {
				Coord l, b;
				RGBcolor( (short)(cp[RED]),
					(short)(cp[GRN]),
					(short)(cp[BLU]) );
				l = xpos * SGI(ifp)->si_xzoom;
				b = ypos * SGI(ifp)->si_yzoom;
				/* left bottom right top */
				rectf( l, b,
					l+SGI(ifp)->si_xzoom, b+SGI(ifp)->si_yzoom);
				i--;
				xpos++;
				cp += 3;
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
				for( ; chunk>0; chunk--)  {
					hole->us = *cp++;
					hole->us = *cp++;
					hole->us = *cp++;
				}
			} else {
				for( ; chunk>0; chunk-- )  {
					hole->s = _sgi_cmap.cm_red[*cp++];
					hole->s = _sgi_cmap.cm_green[*cp++];
					hole->s = _sgi_cmap.cm_blue[*cp++];
				}
			}
		}

		count -= scan_count;
		xpos = 0;
		ypos++;		/* 1st quadrant */
	}
	GEP_END(hole)->s = (0xFF<<8)|8;	/* im_last_passthru(0) */
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
	SGI(ifp)->si_curs_on = mode;
	if( ! mode )
		{
		cursoff();
		return	0;
		}
	x *= SGI(ifp)->si_xzoom;
	y *= SGI(ifp)->si_yzoom;
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
#define MAP_TOL		28		/* pixel delta across all channels */
/* TOL of 28 gives good rendering of the dragon picture without running out */
static int map_size;			/* # of color map slots available */

static RGBpixel	rgb_table[4096];

/*
 *			g e t _ C o l o r _ I n d e x
 */
Colorindex
get_Color_Index( ifp, pixelp )
register FBIO		*ifp;
register RGBpixel	*pixelp;
{
	register int		i;
	int			best = 7;
	register RGBpixel	*sp;
	static int		groused = 0;
	register int		min_diff = 128;

	/* Find best fit in existing table */
	best = 0;
	for( i = 0, sp = rgb_table; i < SGI(ifp)->si_rgb_ct; sp++, i++ ) {
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
	if( i < map_size )  {
		COPYRGB( rgb_table[SGI(ifp)->si_rgb_ct], *pixelp);
		mapcolor(	(Colorindex)SGI(ifp)->si_rgb_ct,
				(short) (*pixelp)[RED],
				(short) (*pixelp)[GRN],
				(short) (*pixelp)[BLU]
				);
		return	(Colorindex)(SGI(ifp)->si_rgb_ct++);
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

	if( (SGIL(ifp) = (char *)calloc( 1, sizeof(struct sgiinfo) )) == NULL )  {
		fb_log("sgw_dopen:  sgiinfo malloc failed\n");
		return(-1);
	}

	SGI(ifp)->si_mode = 0;
	if( file != NULL )  {
		register char *cp;
		/* "/dev/sgiw###" gives optional mode */
		for( cp = file; *cp != NULL && !isdigit(*cp); cp++ ) ;
		(void)sscanf( cp, "%d", &SGI(ifp)->si_mode );
	}

	if( width <= 0 )
		width = ifp->if_width;
	if( height <= 0 )
		height = ifp->if_height;
	if ( width > ifp->if_max_width - 2 * MARGIN) 
		width = ifp->if_max_width - 2 * MARGIN;
	if ( height > ifp->if_max_height - 2 * MARGIN - BANNER)
		height = ifp->if_max_height - 2 * MARGIN - BANNER;

	ifp->if_width = width;
	ifp->if_height = height;

	SGI(ifp)->si_xzoom = 1;	/* for zoom fakeout */
	SGI(ifp)->si_yzoom = 1;	/* for zoom fakeout */
	SGI(ifp)->si_special_zoom = 0;
	if( (SGI(ifp)->si_save = malloc( width*height*sizeof(Colorindex) )) == NULL )  {
		fb_log("sgw_dopen:  unable to malloc pixel buffer\n");
		return(-1);
	}
	bzero( SGI(ifp)->si_save, width*height*sizeof(Colorindex) );

	if( ismex() )  {
		prefposition( WIN_L, WIN_R, WIN_B, WIN_T );
		foreground();		/* Direct focus here, don't detach */
		if( (ifp->if_fd = winopen( "Frame buffer" )) == -1 )
			{
			fb_log( "No more graphics ports available.\n" );
			return	-1;
			}
		wintitle( "BRL libfb Frame Buffer" );
		singlebuffer();
		gconfig();	/* Must be called after singlebuffer().	*/
		/* Need to clean out images from windows below */
		/* This hack is necessary until windows persist from
		 * process to process */
		color(BLACK);
		clear();
	} else {
		ginit();
		singlebuffer();
		gconfig();	/* Must be called after singlebuffer().	*/
	}
	map_size = 1<<getplanes();	/* 10 or 12, depending on ismex() */

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
	SGI(ifp)->si_rgb_ct = MAP_RESERVED;
	if( SGI(ifp)->si_mode )
		{
		/* Mode 1 uses fixed color map */
		for( i = 0; i < map_size-MAP_RESERVED; i++ )
			mapcolor( 	i+MAP_RESERVED,
					(short)((i % 10) + 1) * 25,
					(short)(((i / 10) % 10) + 1) * 25,
					(short)((i / 100) + 1) * 25
					);
		}


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
	free( SGI(ifp)->si_save );
	SGI(ifp)->si_save = (char *)0;

	setcursor( 0, 1, 0x2000 );
	if( ismex() )  {
		/* Leave mex's cursor on */
		; /* winclose( SGI(ifp)->si_fd ); */
	}  else  {
		cursoff();
		greset();
		gexit();
	}
	if( SGIL(ifp) != NULL )
		(void)free( (char *)SGIL(ifp) );
/** 	fb_log( "%d color table entries used.\n", SGI(ifp)->si_rgb_ct );  **/
	return	0;	
}

_LOCAL_ int
sgw_dreset( ifp )
FBIO	*ifp;
{
	ginit();
	singlebuffer();
	gconfig();

	if( qtest() )
		sgw_inqueue(ifp);
	color(BLACK);
	clear();
	bzero( SGI(ifp)->si_save, ifp->if_width*ifp->if_height*sizeof(Colorindex) );
	return	0;	
}

_LOCAL_ int
sgw_dclear( ifp, pp )
FBIO	*ifp;
RGBpixel	*pp;
{
	Colorindex i;
	register Colorindex *p;
	register int cnt;

	if( qtest() )
		sgw_inqueue(ifp);
	if ( pp != RGBPIXEL_NULL)
		i = get_Color_Index( ifp, pp );
	else
		i = BLACK;

	color(i);
	for( cnt = ifp->if_width * ifp->if_height-1; cnt > 0; cnt-- )
		*p++ = i;

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

	if( qtest() )
		sgw_inqueue(ifp);
	if( SGI(ifp)->si_curs_on )
		cursoff();		/* Cursor interferes with reading! */
	x *= SGI(ifp)->si_xzoom;
	while( count > 0 )
		{	register short	ypos = y*SGI(ifp)->si_yzoom;
		if ( count >= ifp->if_width )
			scan_count = ifp->if_width;
		else
			scan_count = count;
		if( (SGI(ifp)->si_xzoom == 1 && SGI(ifp)->si_yzoom == 1) || SGI(ifp)->si_special_zoom )
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
				x += SGI(ifp)->si_xzoom;
				readpixels( 1, &colors[i] );
				}
			}
		for( i = 0; i < scan_count; i++, pixelp++) 
			{
			if( SGI(ifp)->si_mode )
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
				if( ci < SGI(ifp)->si_rgb_ct )
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
	if( SGI(ifp)->si_curs_on )
		curson();		/* Cursor interferes with reading! */
	return	0;	
}

_LOCAL_ int
sgw_bwrite( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
register RGBpixel	*pixelp;
int	count;
	{	register union gepipe *hole = GEPIPE;
		int scan_count;
		register int i;

	if( qtest() )
		sgw_inqueue(ifp);
	writemask( 0x3FF );
	while( count > 0 )
		{
			register short	ypos = y*SGI(ifp)->si_yzoom;
			register short	xpos = x*SGI(ifp)->si_xzoom;
			register Colorindex *sp;
		sp = (Colorindex *)
			&SGI(ifp)->si_save[(y*ifp->if_width+x)*sizeof(Colorindex)];
		if ( count >= ifp->if_width )
			scan_count = ifp->if_width;
		else
			scan_count = count;
		if( (SGI(ifp)->si_xzoom == 1 && SGI(ifp)->si_yzoom == 1) || SGI(ifp)->si_special_zoom )
			{	register Colorindex	colori;
			CMOV2S( hole, xpos, ypos );
			for( i = scan_count; i > 0; )
				{	register short	chunk;
				if( i <= 127 )
					chunk = i;
				else
					chunk = 127;
				hole->s = (chunk<<8)|8; /* GEpassthru */
				hole->s = 0xD;		 /* FBCdrawpixels */
				i -= chunk;
				for( ; chunk > 0; chunk--, pixelp++ )
					{
					if( SGI(ifp)->si_mode )
						{
						colori =  MAP_RESERVED +
							((*pixelp)[RED]/26);
						colori += ((*pixelp)[GRN]/26) * 10;
						colori += ((*pixelp)[BLU]/26) * 100;
						}
					else
						colori = get_Color_Index( ifp, pixelp );
					hole->s = colori;
					*sp++ = colori;
					}
				}
			GEP_END(hole)->s = (0xFF<<8)|8;	/* im_last_passthru(0) */
			}
		else
			for( i = 0; i < scan_count; i++, pixelp++ )
				{	register Colorindex	col;
					register Coord	r = xpos + SGI(ifp)->si_xzoom - 1,
							t = ypos + SGI(ifp)->si_yzoom - 1;
				CMOV2S( hole, xpos, ypos );
				if( SGI(ifp)->si_mode )
					{
					col =  MAP_RESERVED +
						((*pixelp)[RED]/26);
					col += ((*pixelp)[GRN]/26) * 10;
					col += ((*pixelp)[BLU]/26) * 100;
					}
				else
					col = get_Color_Index( ifp, pixelp );

				color( col );
				im_rectf( (Coord)xpos, (Coord)ypos, r, t );
				xpos += SGI(ifp)->si_xzoom;
				*sp++ = col;
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
	if( qtest() )
		sgw_inqueue(ifp);
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

	if( qtest() )
		sgw_inqueue(ifp);
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

	if( qtest() )
		sgw_inqueue(ifp);
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
	if( qtest() )
		sgw_inqueue(ifp);
	if( x == 0 )  x = 1;
	if( y == 0 )  y = 1;
	if( x < 0 || y < 0 )
		{
		SGI(ifp)->si_special_zoom = 1;
		x = y = 1;
		}
	else
		SGI(ifp)->si_special_zoom = 0;
	SGI(ifp)->si_xzoom = x;
	SGI(ifp)->si_yzoom = y;
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

	if( qtest() )
		sgw_inqueue(ifp);
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
	if( qtest() )
		sgw_inqueue(ifp);
	SGI(ifp)->si_curs_on = mode;
	if( ! mode )
		{
		cursoff();
		setcursor( 0, 1, 0x2000 );
		return	0;
		}
	x *= SGI(ifp)->si_xzoom;
	y *= SGI(ifp)->si_yzoom;
	curson();
	setcursor( 1, cursor_color, 0x2000 );
	setvaluator( MOUSEX, x + WIN_L, 0, 1023 );
	setvaluator( MOUSEY, y + WIN_B, 0, 1023 );
	return	0;
	}

/*
 *			S G W _ I N Q U E U E
 *
 *  Called when a qtest() indicates that there is a window event.
 *  Process all events, so that we don't loop on recursion to sgw_bwrite.
 */
_LOCAL_ void
sgw_inqueue(ifp)
register FBIO *ifp;
{
	short val;
	int redraw = 0;
	register int ev;

	while( qtest() )  {
		switch( ev = qread(&val) )  {
		case REDRAW:
			redraw = 1;
			break;
		case INPUTCHANGE:
			break;
		case MODECHANGE:
			/* This could be bad news.  Should we re-write
			 * the color map? */
			fb_log("sgw_inqueue:  modechange?\n");
			break;
		default:
			fb_log("sgw_inqueue:  event %d unknown\n", ev);
			break;
		}
	}
	/*
	 * Now that all the events have been removed from the input
	 * queue, handle any actions that need to be done.
	 */
	if( redraw )  {
		register int i;

		/* Redraw whole window from save area */
		for( i=0; i<ifp->if_height; i++ )  {
			cmov2s( 0, i );
			writepixels( ifp->if_width, (Colorindex *)
				&SGI(ifp)->si_save[i*ifp->if_width*sizeof(Colorindex)] );
		}
		redraw = 0;
	}
}
