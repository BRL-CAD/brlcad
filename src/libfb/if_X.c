/*                          I F _ X . C
 * BRL-CAD
 *
 * Copyright (C) 1988-2005 United States Government as represented by
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
/** @file if_X.c
 *  X Window System (X11) libfb interface.
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

#define	DEBUGX	0
#define	CURSOR	1


#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#ifdef HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#include "machine.h"
#include "fb.h"
#include "./fblocal.h"

#include <X11/X.h>
#ifdef HAVE_XOSDEFS_H
#include <X11/Xfuncproto.h>
#include <X11/Xosdefs.h>
#endif
#if defined(linux)
#	undef	X_NOT_STDC_ENV
#	undef	X_NOT_POSIX
#endif
#define XLIB_ILLEGAL_ACCESS	/* necessary on facist SGI 5.0.1 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/Xatom.h>		/* for XA_RGB_BEST_MAP */

static	void	slowrect(FBIO *ifp, int xmin, int xmax, int ymin, int ymax);
static	int	linger(FBIO *ifp);
static	int	xsetup(FBIO *ifp, int width, int height);
void		x_print_display_info(Display *dpy);
static	int	x_make_colormap(FBIO *ifp);	/*XXX*/
static	int	x_make_cursor(FBIO *ifp);	/*XXX*/
static void	repaint(FBIO *ifp);

#define TMP_FILE	"/tmp/x.cmap"

_LOCAL_ int	X_scanwrite(FBIO *ifp, int x, int y, const unsigned char *pixelp, int count, int save);

_LOCAL_ int	X_open(FBIO *ifp, char *file, int width, int height),
		X_close(FBIO *ifp),
		X_clear(FBIO *ifp, unsigned char *pp),
		X_read(FBIO *ifp, int x, int y, unsigned char *pixelp, int count),
		X_write(FBIO *ifp, int x, int y, const unsigned char *pixelp, int count),
		X_rmap(FBIO *ifp, ColorMap *cmp),
		X_wmap(FBIO *ifp, const ColorMap *cmp),
		X_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom),
		X_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom),
		X_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig),
		X_cursor(FBIO *ifp, int mode, int x, int y),
		X_getcursor(FBIO *ifp, int *mode, int *x, int *y),
		X_poll(FBIO *ifp),
		X_flush(FBIO *ifp),
		X_help(FBIO *ifp);

#ifdef USE_PROTOTYPES
static void	Monochrome( unsigned char *bitbuf, unsigned char *bytebuf, int width, int height, int method);
static int	do_event( FBIO	*ifp );
#else
static void	Monochrome();
static int	do_event();
#endif

/* This is the ONLY thing that we normally "export" */
FBIO X_interface = {
	0,
	X_open,			/* device_open		*/
	X_close,		/* device_close		*/
	X_clear,		/* device_clear		*/
	X_read,			/* buffer_read		*/
	X_write,		/* buffer_write		*/
	X_rmap,			/* colormap_read	*/
	X_wmap,			/* colormap_write	*/
	X_view,			/* set view		*/
	X_getview,		/* get view		*/
	X_setcursor,		/* define cursor	*/
	X_cursor,		/* set cursor		*/
	X_getcursor,		/* get cursor		*/
	fb_sim_readrect,	/* read rectangle	*/
	fb_sim_writerect,	/* write rectangle	*/
	fb_sim_bwreadrect,
	fb_sim_bwwriterect,
	X_poll,			/* handle events	*/
	X_flush,		/* flush		*/
	X_close,		/* free			*/
	X_help,			/* help message		*/
	"X Window System (X11) 8-bit and 1-bit visuals only",
	2048,			/* max width		*/
	2048,			/* max height		*/
	"/dev/xold",		/* short device name	*/
	512,			/* default/current width  */
	512,			/* default/current height */
	-1,			/* select file desc	*/
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

/*
 * Per window state information.
 */
struct	xinfo {
	Display	*dpy;			/* Display and Screen(s) info */
	Window	win;			/* Window ID */
	int	screen;			/* Our screen selection */
	Visual	*visual;		/* Our visual selection */
	GC	gc;			/* current graphics context */
	Colormap cmap;			/* 8bit X colormap */
	XImage	*image;
	XImage	*scanimage;
	unsigned char *bytebuf;		/* 8bit image buffer */
	unsigned char *bitbuf;		/* 1bit image buffer */
	unsigned char *scanbuf;		/* single scan line image buffer */
	unsigned char *mem;		/* optional 24bit store */
	int	method;			/* bitmap conversion method */
	Window	curswin;		/* Cursor Window ID */

	int	depth;			/* 1, 8, or 24bit */
	int	mode;			/* 0,1,2 */
	ColorMap rgb_cmap;		/* User's libfb colormap */
};
#define	XI(ptr) ((struct xinfo *)((ptr)->u1.p))
#define	XIL(ptr) ((ptr)->u1.p)		/* left hand side version */

#define MODE_1MASK	(1<<1)
#define MODE_1TRANSIENT	(0<<1)
#define MODE_1LINGERING (1<<1)

#define MODE_2MASK	(1<<2)
#define MODE_2RGB	(0<<2)
#define MODE_2_8BIT	(1<<2)

#define MODE_3MASK	(1<<3)
#define MODE_3NORMAL	(0<<3)
#define MODE_3MONO	(1<<3)

#define MODE_4MASK	(1<<4)
#define MODE_4NORMAL	(0<<4)
#define MODE_4MEM	(1<<4)

#define MODE_5MASK	(1<<5)
#define MODE_5NORMAL	(0<<5)
#define MODE_5INSTCMAP	(1<<5)

static struct modeflags {
	char	c;
	long	mask;
	long	value;
	char	*help;
} modeflags[] = {
	{ 'l',	MODE_1MASK, MODE_1LINGERING,
		"Lingering window - else transient" },
	{ 'b',  MODE_2MASK, MODE_2_8BIT,
		"8-bit Black and White from RED channel" },
	{ 'm',  MODE_4MASK, MODE_4MEM,
		"24-bit memory buffer" },
	{ 'M',  MODE_3MASK, MODE_3MONO,
		"Force Monochrome mode - debugging" },
	{ 'I',  MODE_5MASK, MODE_5INSTCMAP,
		"Install the colormap - debug" },
	{ '\0', 0, 0, "" }
};

/*
 *	Hardware colormap support
 *
 *	The color map is organized as a 6x6x6 colorcube, with 10 extra  
 *	entries each for the primary colors and grey values.
 *
 *	entries 0 -> 215 are the color cube
 *	entries 216 -> 225 are extra "red" values
 *	entries 226 -> 235 are extra "green" values
 *	entries 236 -> 245 are extra "blue" values
 *	entries 246 -> 255 are extra "grey" values
 *
 */
/* Our copy of the *hardware* colormap */
static unsigned char redmap[256], grnmap[256], blumap[256];

/* values for color cube entries */
static unsigned char cubevec[6] = { 0, 51, 102, 153, 204, 255 };

/* additional values for primaries */
static unsigned char primary[10] = {
	17, 34, 68, 85, 119, 136, 170, 187, 221, 238
};

/* Arrays containing the indicies of the primary colors and grey values
 * in the color map
 */
static unsigned short redvec[16] = {
0,  216, 217, 1, 218, 219, 2, 220, 221, 3, 222, 223, 4, 224, 225, 5
};
static unsigned short grnvec[16] = {
0, 226, 227, 6, 228, 229, 12, 230, 231, 18, 232, 233, 24, 234, 235, 30
};
static unsigned short bluvec[16] = {
0, 236, 237, 36, 238, 239, 72, 240, 241, 108, 242, 243, 144, 244, 245, 180
};
static unsigned short greyvec[16] = {
0, 246, 247, 43, 248, 249, 86, 250, 251, 129, 252, 253, 172, 254, 255, 215
};

static unsigned char convRGB(register const unsigned char *v);
unsigned long 	*x_pixel_table;
XColor 		*color_defs; 

_LOCAL_ int
X_open(FBIO *ifp, char *file, int width, int height)
{
	int	fd;
	int	mode;
	unsigned char *bytebuf;		/* local copy */
	unsigned char *bitbuf;		/* local copy */
	unsigned char *scanbuf;		/* local copy */

	FB_CK_FBIO(ifp);

	/*
	 *  First, attempt to determine operating mode for this open,
	 *  based upon the "unit number" or flags.
	 *  file = "/dev/X###"
	 *  The default mode is zero.
	 */
	mode = 0;

	if( file != NULL )  {
		register char *cp;
		char	modebuf[80];
		char	*mp;
		int	alpha;
		struct	modeflags *mfp;

		if( strncmp(file, "/dev/X", 6) ) {
			/* How did this happen?? */
			mode = 0;
		}
		else {
			/* Parse the options */
			alpha = 0;
			mp = &modebuf[0];
			cp = &file[6];
			while( *cp != '\0' && !isspace(*cp) ) {
				*mp++ = *cp;	/* copy it to buffer */
				if( isdigit(*cp) ) {
					cp++;
					continue;
				}
				alpha++;
				for( mfp = modeflags; mfp->c != '\0'; mfp++ ) {
					if( mfp->c == *cp ) {
						mode = (mode&~mfp->mask)|mfp->value;
						break;
					}
				}
				if( mfp->c == '\0' && *cp != '-' ) {
					fb_log( "if_X: unknown option '%c' ignored\n", *cp );
				}
				cp++;
			}
			*mp = '\0';
			if( !alpha )
				mode = atoi( modebuf );
		}
	}

	if( width <= 0 )
		width = ifp->if_width;
	if( height <= 0 )
		height = ifp->if_height;
	if ( width > ifp->if_max_width) 
		width = ifp->if_max_width;
	if ( height > ifp->if_max_height) 
		height = ifp->if_max_height;

	/* round width up to a multiple of eight bits */
	if( (width%8) != 0 )
		width = ((width + 7)/8)*8;
	ifp->if_width = width;
	ifp->if_height = height;

	/* create a struct of state information */
	if( (XIL(ifp) = (char *)calloc( 1, sizeof(struct xinfo) )) == NULL ) {
		fb_log("X_open: xinfo malloc failed\n");
		return(-1);
	}
	ifp->if_xzoom = 1;
	ifp->if_yzoom = 1;
	ifp->if_xcenter = width/2;
	ifp->if_ycenter = height/2;
	XI(ifp)->mode = mode;

	/* set up an X window, graphics context, etc. */
	if( xsetup( ifp, width, height ) < 0 ) {
		return(-1);
	}

	/* check for forced monochrome behavior */
	if( (XI(ifp)->mode&MODE_3MASK) == MODE_3MONO )
		XI(ifp)->depth = 1;

	/* Init our internal, and possibly X's, colormap */
	/* ColorMap File HACK */
	if( (fd = open( TMP_FILE, 0 )) >= 0 ) {
		/* restore it from a file */
		read( fd, &(XI(ifp)->rgb_cmap), sizeof(XI(ifp)->rgb_cmap) );
		close(fd);
		X_wmap( ifp, &(XI(ifp)->rgb_cmap) );
	} else {
		/* use linear map */
		X_wmap( ifp, (ColorMap *)NULL );
	}

	/* Allocate all of our working pixel/bit buffers */
	if( (bytebuf = (unsigned char *)calloc( 1, width*height )) == NULL ) {
		fb_log("X_open: bytebuf malloc failed\n");
		return(-1);
	}
	if( (bitbuf = (unsigned char *)calloc( 1, (width*height)/8 )) == NULL ) {
		fb_log("X_open: bitbuf malloc failed\n");
		return(-1);
	}
	if( (scanbuf = (unsigned char *)calloc( 1, width )) == NULL ) {
		fb_log("X_open: scanbuf malloc failed\n");
		return(-1);
	}
	XI(ifp)->bytebuf = bytebuf;
	XI(ifp)->bitbuf = bitbuf;
	XI(ifp)->scanbuf = scanbuf;

	if( (XI(ifp)->mode&MODE_4MASK) == MODE_4MEM ) {
		/* allocate a full 24-bit deep buffer */
		XI(ifp)->mem = (unsigned char *)calloc( 3, width*height );
		if( XI(ifp)->mem == NULL ) {
			fb_log("X_open: 24-bit buffer malloc failed\n");
		}
	}

	/*
	 *  Create an Image structure.
	 *  The image is our client resident copy which we
	 *  can get/put from/to a server resident Pixmap or
	 *  Window (i.e. a "Drawable").
	 */
	if( XI(ifp)->depth == 8 ) {
		XI(ifp)->image = XCreateImage( XI(ifp)->dpy,
			XI(ifp)->visual, 8, ZPixmap, 0,
			(char *)bytebuf, width, height, 32, 0);
		XI(ifp)->scanimage = XCreateImage( XI(ifp)->dpy,
			XI(ifp)->visual, 8, ZPixmap, 0,
			(char *)scanbuf, width, 1, 32, 0);
	} else {
		/* An XYBitmap can be used on any depth display.
		 * The GC of the XPutImage provides fg and bg
		 * pixels for each 1 and 0 bit respectively.
		 * (This may be very slow however for depth != 1)
		 */
		XI(ifp)->image = XCreateImage( XI(ifp)->dpy,
			XI(ifp)->visual, 1, XYBitmap, 0,
			(char *)bitbuf, width, height, 8, 0);
		XI(ifp)->scanimage = XCreateImage( XI(ifp)->dpy,
			XI(ifp)->visual, 1, XYBitmap, 0,
			(char *)scanbuf, width, 1, 8, 0);
		XI(ifp)->depth = 1;
	}

	/* Make the Display connection available for selecting on */

	ifp->if_selfd = XI(ifp)->dpy->fd;

	return(0);
}

_LOCAL_ int
X_close(FBIO *ifp)
{
	XFlush( XI(ifp)->dpy );
	if( (XI(ifp)->mode & MODE_1MASK) == MODE_1LINGERING ) {
		if( linger(ifp) )
			return(0);	/* parent leaves the display */
	}
	if( XIL(ifp) != NULL ) {
		XCloseDisplay( XI(ifp)->dpy );
		(void)free( (char *)XIL(ifp) );
	}
	return(0);
}

_LOCAL_ int
X_clear(FBIO *ifp, unsigned char *pp)
{
	unsigned char *bitbuf = XI(ifp)->bitbuf;
	unsigned char *bytebuf = XI(ifp)->bytebuf;
#ifdef XXX
	RGBpixel v;

	if( pp == RGBPIXEL_NULL ) {
		v[RED] = v[GRN] = v[BLU] = 0;
	} else {
		v[RED] = (pp)[RED];
		v[GRN] = (pp)[GRN];
		v[BLU] = (pp)[BLU];
	}
	if( v[RED] == v[GRN] && v[RED] == v[BLU] ) {
		int	bytes = ifp->if_width*ifp->if_height*3;
		if( v[RED] == 0 )
			bzero( cp, bytes );		/* all black */
		else
			memset( cp, v[RED], bytes );	/* all grey */
	} else {
		for( n = ifp->if_width*ifp->if_height; n; n-- ) {
			*cp++ = v[RED];
			*cp++ = v[GRN];
			*cp++ = v[BLU];
		}
	}
#endif
	if( pp == (unsigned char *)NULL
	 || ((pp)[RED] == 0 && (pp)[GRN] == 0 && (pp)[BLU] == 0) ) {
		bzero( (char *)bitbuf, (ifp->if_width * ifp->if_height)/8 );
		bzero( (char *)bytebuf, (ifp->if_width * ifp->if_height) );
		XClearWindow( XI(ifp)->dpy, XI(ifp)->win );
	}
	/*XXX*/
	return(0);
}

_LOCAL_ int
X_read(FBIO *ifp, int x, int y, unsigned char *pixelp, int count)
{
	unsigned char *bytebuf = XI(ifp)->bytebuf;
	register unsigned char	*cp;
	register int	i;

	if( x < 0 || x >= ifp->if_width || y < 0 || y >= ifp->if_height )
		return	-1;

	/* return 24bit store if available */
	if( XI(ifp)->mem ) {
		bcopy( &(XI(ifp)->mem[(y*ifp->if_width+x)*sizeof(RGBpixel)]),
		       pixelp, count*sizeof(RGBpixel) );
		return	count;
	}

	/* 1st -> 4th quadrant */
	y = ifp->if_height - 1 - y;

	/* give then gray scale pixels - XXX - may be pseudo color */
	cp = &bytebuf[y*ifp->if_width + x];
	for( i = 0; i < count; i++ ) {
		*pixelp++ = *cp;
		*pixelp++ = *cp;
		*pixelp++ = *cp++;
	}
	return	count;
}

/*
 * Dithering
 */
int dm4[4][4] = {
	{ 0,  8,  2, 10},
	{12,  4, 14,  6},
	{ 3, 11,  1,  9},
	{15,  7, 13,  5}
};
int dm8[8][8] = {
	{ 0, 32,  8, 40,  2, 34, 10, 42},
	{48, 16, 56, 24, 50, 18, 58, 26},
	{12, 44,  4, 36, 14, 46,  6, 38},
	{60, 28, 52, 20, 62, 30, 54, 22},
	{ 3, 35, 11, 43,  1, 33,  9, 41},
	{51, 19, 59, 27, 49, 17, 57, 25},
	{15, 47,  7, 39, 13, 45,  5, 37},
	{63, 31, 55, 23, 61, 29, 53, 21}
};
int ditherPeriod = 8;
int *dm = &(dm8[0][0]);
int *error1, *error2;

int dither_bw(unsigned int pixel, register int count, register int line)
{
	if( pixel > dm[((line%ditherPeriod)*ditherPeriod) +
	    (count%ditherPeriod)])
		return(1);
	else
		return(0);
}

/*
 * Floyd Steinberg error distribution algorithm
 */
int
fs_bw(unsigned int pixel, register int count, register int line)
{
	int  onoff;
	int  intensity, error;

	if( count == 0 ) {
		int *tmp;
		tmp = error1;
		error1 = error2;
		error2 = tmp;
		error2[0] = 0;
	}

	intensity = pixel + error1[count];
	if( intensity < 128 ) {
		onoff = 0;
		error = intensity;
	} else {
		onoff = 1;
		error = intensity - 255;
	}

	error1[count+1] += (int)(3*error)/8;	/* right */
	error2[count+1] = (int)error/4;		/* down */
	error2[count] += (int)(3*error)/8;	/* diagonal */
	return(onoff);
}

/*
 * Modified Floyd Steinberg algorithm
 */
int
mfs_bw(unsigned int pixel, register int count, register int line)
{
	int  onoff;
	int  intensity, error;

	if (count == 0) {
		int *tmp;
		tmp = error1;
		error1 = error2;
		error2 = tmp;
		error2[0] = 0;
	}

	intensity = pixel + error1[count];

	if (intensity < 128) {
		onoff = 0;
		error = 128 - intensity;
	} else {
		onoff = 1;
		error = 128 - intensity;
	}

	error1[count+1] += (int)(3*error)/8;	/* right */
	error2[count+1] = (int)error/4;		/* down */
	error2[count] += (int)(3*error)/8;	/* diagonal */
	return(onoff);
}

/*
 * Decompose a write of more than one scanline into multiple single
 * scanline writes.
 */
_LOCAL_ int
X_write(FBIO *ifp, int x, int y, const unsigned char *pixelp, int count)
{
	int	maxcount;
	int	todo;
	int	num;

	/* check origin bounds */
	if( x < 0 || x >= ifp->if_width || y < 0 || y >= ifp->if_height )
		return	-1;

	/* check write length */
	maxcount = ifp->if_width * (ifp->if_height - y) - x;
	if( count > maxcount )
		count = maxcount;

	/* save it in 24bit store if available */
	if( XI(ifp)->mem ) {
		bcopy( pixelp,
		       &(XI(ifp)->mem[(y*ifp->if_width+x)*sizeof(RGBpixel)]),
		       count*sizeof(RGBpixel) );
	}

	todo = count;
	while( todo > 0 ) {
		if( x + todo > ifp->if_width )
			num = ifp->if_width - x;
		else
			num = todo;
		if( X_scanwrite( ifp, x, y, pixelp, num, 1 ) == 0 )
			return( 0 );
		x = 0;
		y++;
		todo -= num;
		pixelp += num;
	}
	return( count );
}

/*
 * This function converts a single scan line of (pre color mapped) pixels
 * into displayable form.  It will either save this data into our X image
 * buffer for later repaints/redisplay or it will put it into a single
 * scanline temporary buffer for immediate display.
 */
_LOCAL_ int
X_scanwrite(FBIO *ifp, int x, int y, const unsigned char *pixelp, int count, int save)
{
	unsigned char *bitbuf = XI(ifp)->bitbuf;
	unsigned char *bytebuf = XI(ifp)->bytebuf;
	unsigned char *scanbuf = XI(ifp)->scanbuf;
	register unsigned char	*cp;
	register int	i;
	static unsigned char MSB[8] = { 0x80, 0x40, 0x20, 0x10, 8, 4, 2, 1 };
	static unsigned char LSB[8] = { 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80 };
	register unsigned char *bits = MSB;
	unsigned char tmpbuf[1280];	/*XXX*/
	int	sy;			/* 4th quad y */

	/* 1st -> 4th quadrant */
	sy = ifp->if_height - 1 - y;
	if( save ) {
		cp = &bytebuf[sy*ifp->if_width + x];
	} else {
		if( XI(ifp)->depth == 1 )
			cp = tmpbuf;	/* save scanbuf for 1bit output */
		else
			cp = scanbuf;
	}

	if( XI(ifp)->depth == 8 ) {
		/* Gray Scale Mode */
		if( (XI(ifp)->mode&MODE_2MASK) == MODE_2_8BIT )  {
			for( i=0; i<count; i++ )  {
				cp[i] = pixelp[3*i+RED];
			}
			goto done;
		}
		/* PseudoColor Mode */
		for( i = 0; i < count; i++ ) {
			int value;
			value = convRGB(&pixelp[3*i]);
			cp[i] = (unsigned char) x_pixel_table[value];
		}
		goto done;
	}

	/* MONOCHROME Follows */

	/* save the 8bit black and white version of it */
	/* XXX - note replication of Gray Scale Mode above... */
	if( (XI(ifp)->mode&MODE_2MASK) == MODE_2_8BIT )  {
		for( i = 0; i < count; i++ ) {
			cp[i] = pixelp[3*i+RED];
		}
	} else {
		for( i = 0; i < count; i++ ) {
			/* Best possible 8-bit NTSC weights */
			/* Use three tables if this gets to be a bottleneck */
			cp[i] = (77*(int)pixelp[3*i+RED] + 150*(int)pixelp[3*i+GRN]
				+ 29*(int)pixelp[3*i+BLU]) >> 8;
		}
	}

	/* Convert the monochrome data to a bitmap */
	if (BitmapBitOrder(XI(ifp)->dpy) == LSBFirst) {
		bits = LSB;
	}

	{
	int	row, col, bit;
    	int	byte, rem;
	unsigned char	mvalue;
	unsigned char	*mbuffer;	/* = &buffer[(sy*ifp->if_width + x)/8]; */
    	byte = sy * ifp->if_width + x;
    	rem = byte % 8;
    	byte /= 8;
	if( save )
		mbuffer = &bitbuf[byte];
	else
		mbuffer = scanbuf;

	for( row = sy; row < sy+1; row++ ) {
		for( col=x; col < x+count; ) {
			/*mvalue = 0x00;*/
			/* pre-read the byte */
			mvalue = *mbuffer;
			/* diddle its bit */
			for( bit=rem; (bit < 8) && (col < x+count); bit++,col++ ) {
				/*val = (30*(pixelp)[RED] + 59*(pixelp)[GRN] + 11*(pixelp)[BLU] + 200) / 400;*/
				/*pixelp++;*/
				/*if( dither_bw(val, col, row) ) {*/
				if( (int)*cp++ > (dm[((row&7)<<3)+(col&7)]<<2)+1 ) {
					mvalue |= bits[bit];
				} else {
					mvalue &= ~bits[bit];
				}
			}
			/* put the byte back */
			*mbuffer++ = mvalue;
			rem = 0;
		}
	}
	}

done:
	/* XXX - Determine how much of the scan line is displayed in
	 * the window (if any) including pan & zoom, and display that
	 * portion.
	 */
	if( save && (ifp->if_xzoom != 1) ) {
		/* note: slowrect never asks us to save */
		slowrect( ifp, x, x+count-1, y, y );
	} else if( save ) {
		XPutImage(XI(ifp)->dpy, XI(ifp)->win, XI(ifp)->gc,
			XI(ifp)->image,
			x, sy, x, sy,
			count, 1 );
	} else {
		XPutImage(XI(ifp)->dpy, XI(ifp)->win, XI(ifp)->gc,
			XI(ifp)->scanimage,
			0, 0, x, sy,
			count, 1 );
	}

	/* XXX - until we get something better */
	if( count > 1 )
		XFlush(XI(ifp)->dpy);

    	return	count;
}

_LOCAL_ int
X_rmap(FBIO *ifp, ColorMap *cmp)
{
	*cmp = XI(ifp)->rgb_cmap;	/* struct copy */
	return(0);
}

_LOCAL_ int
X_wmap(FBIO *ifp, const ColorMap *cmp)
{
	register int i;
	int	is_linear = 1;

	if( cmp == (ColorMap *)NULL ) {
		fb_make_linear_cmap( &(XI(ifp)->rgb_cmap) );
		is_linear = 1;
	} else {
		XI(ifp)->rgb_cmap = *cmp;	/* struct copy */
		is_linear = fb_is_linear_cmap(cmp);
	}

	/* Hack to save it into a file - this may go away */
	if( is_linear ) {
		/* no file => linear map */
		(void) unlink( TMP_FILE );
	} else {
		/* save map for later */
		i=creat(TMP_FILE, 0666);
		if( i >= 0 )  {
			write( i, cmp, sizeof(*cmp) );
			close(i);
		} else {
			fprintf(stderr, "if_X: couldn't save color map\n");
			perror(TMP_FILE);
		}
	}

	if( XI(ifp)->depth != 8 )
		return(0);	/* no X colormap allocated - XXX */

	/* If MODE_2_8BIT, load it in the real window colormap */
	if( (XI(ifp)->mode&MODE_2MASK) == MODE_2_8BIT ) {
		for( i = 0; i < 256; i++ ) {
			/* Both sides expect 16-bit left-justified maps */
			color_defs[i].pixel = i;
			color_defs[i].red   = cmp->cm_red[i];
			color_defs[i].green = cmp->cm_green[i];
			color_defs[i].blue  = cmp->cm_blue[i];
		        color_defs[i].flags = DoRed | DoGreen | DoBlue;
		}
		XStoreColors( XI(ifp)->dpy, XI(ifp)->cmap, color_defs, 256 );
	}

	return(0);
}

_LOCAL_ int
X_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
	/* bypass if no change */
	if( ifp->if_xcenter == xcenter && ifp->if_ycenter == ycenter
	 && ifp->if_xzoom == xcenter && ifp->if_yzoom == ycenter )
		return	0;

	/* check bounds */
	if( xcenter < 0 || xcenter >= ifp->if_width
	 || ycenter < 0 || ycenter >= ifp->if_height )
		return	-1;
	if( xzoom <= 0 || xzoom >= ifp->if_width/2
	 || yzoom <= 0 || yzoom >= ifp->if_height/2 )
		return	-1;

	ifp->if_xcenter = xcenter;
	ifp->if_ycenter = ycenter;
	ifp->if_xzoom = xzoom;
	ifp->if_yzoom = yzoom;
	/* XXX - repaint */
	repaint(ifp);
	return	0;
}

_LOCAL_ int
X_getview(FBIO *ifp, int *xcenter, int *ycenter, int *xzoom, int *yzoom)
{
	*xcenter = ifp->if_xcenter;
	*ycenter = ifp->if_ycenter;
	*xzoom = ifp->if_xzoom;
	*yzoom = ifp->if_yzoom;

	return	0;
}

_LOCAL_ int
X_setcursor(FBIO *ifp, const unsigned char *bits, int xbits, int ybits, int xorig, int yorig)
{
	return	0;
}

_LOCAL_ int
X_cursor(FBIO *ifp, int mode, int x, int y)
{
	fb_sim_cursor(ifp, mode, x, y);

	/* remap image x,y to screen position */
	x = (x-ifp->if_xcenter)*ifp->if_xzoom+ifp->if_width/2;
	y = (y-ifp->if_ycenter)*ifp->if_yzoom+ifp->if_height/2;

	if( XI(ifp)->curswin == 0 )
		x_make_cursor(ifp);

	y = ifp->if_height - 1 - y;	/* 1st -> 4th quadrant */
	x -= 3;
	y -= 3;
	if( mode ) {
		XMoveWindow( XI(ifp)->dpy, XI(ifp)->curswin, x, y );
		XMapWindow( XI(ifp)->dpy, XI(ifp)->curswin );
	} else {
		XUnmapWindow( XI(ifp)->dpy, XI(ifp)->curswin );
	}
	XFlush( XI(ifp)->dpy );

	return	0;
}

_LOCAL_ int
X_getcursor(FBIO *ifp, int *mode, int *x, int *y)
{
	return fb_sim_getcursor(ifp, mode, x, y);
}

static
int
xsetup(FBIO *ifp, int width, int height)
{
	Display	*dpy;			/* local copy */
	int	screen;			/* local copy */
	Visual	*visual;		/* local copy */
	Window	win;			/* local copy */
	GC	gc;			/* local copy */
	XGCValues gcv;
	XSizeHints	xsh;		/* part of the "standard" props */
	XWMHints	xwmh;		/* size guidelines for window mngr */
	XSetWindowAttributes xswa;

	/* Open the display - use the env variable DISPLAY */
	if( (dpy = XOpenDisplay(NULL)) == NULL ) {
		fb_log( "if_X: Can't open X display \"%s\"\n",
			XDisplayName(NULL) );
		return	-1;
	}
	/* Use the screen we connected to */
	screen = DefaultScreen(dpy);
	/*
	 *  Note: all Windows, Colormaps and XImages have a Visual
	 *  attribute which determines how pixel values are mapped
	 *  to displayed colors.  We should eventually examine which
	 *  choices (if any) the current screen offers and pick a
	 *  "best" one.  For now use the default.  Most servers don't
	 *  offer a choice yet anyway.
	 */
	visual = DefaultVisual(dpy,screen);

	/* save values in state struct */
	XI(ifp)->dpy = dpy;
	XI(ifp)->screen = screen;
	XI(ifp)->visual = visual;
	XI(ifp)->depth = DisplayPlanes(dpy,screen);
	if( DisplayCells(dpy,screen) != 256 )
		XI(ifp)->depth = 1;	/*XXX - until cmap fix */

#if DEBUGX
	x_print_display_info(dpy);
#endif

	/*
	 * Fill in XSetWindowAttributes struct for XCreateWindow.
	 */
	xswa.event_mask = ExposureMask;
		/* |ButtonPressMask |LeaveWindowMask |EnterWindowMask */
		/* |ColormapChangeMask */
	xswa.background_pixel = BlackPixel(dpy, screen);
	xswa.border_pixel = WhitePixel(dpy, screen);
	xswa.bit_gravity = SouthWestGravity;
	xswa.backing_store = Always;
	/* could set colormap here... */
#ifdef CURSORFOO
	xswa.cursor = XCreateFontCursor(dpy, XC_gumby);
#endif

#if DEBUGX
printf("Creating window\n");
#endif
	win = XCreateWindow( dpy, DefaultRootWindow(dpy),
		0, 0, width, height, 3,
		DefaultDepth(dpy, screen), InputOutput, visual,
		CWEventMask |CWBackPixel |CWBorderPixel
		|CWBitGravity | CWBackingStore,
		/* |CWCursor, */
		&xswa );

	XI(ifp)->win = win;
	if( win == 0 ) {
		fb_log( "if_X: Can't create window\n" );
		return	-1;
	}

	/* get or set a colormap for our window */
	if( XI(ifp)->depth == 8 ) {
		x_make_colormap(ifp);
	} else {
		XI(ifp)->cmap = DefaultColormap(dpy,screen);
	}

	/*
	 * Fill in XSizeHints struct to inform window
	 * manager about initial size and location.
	 */
	xsh.flags = PPosition | PSize | PMinSize | PMaxSize;
	xsh.width = xsh.max_width = xsh.min_width = width;
	xsh.height = xsh.max_height = xsh.min_height = height;
	xsh.x = xsh.y = 0;

	/* Set standard properties for Window Managers */
#if DEBUGX
printf("Setting Standard Properties\n");
#endif
	XSetStandardProperties( dpy, win,
		"Frame buffer",		/* window name */
		"Frame buffer",		/* icon name */
		None,			/* icon pixmap */
		NULL, 0,		/* command (argv, argc) */
		&xsh );			/* size hints */
#if DEBUGX
printf("Setting WM Hints\n");
#endif
	xwmh.input = False;		/* no terminal input? */
	xwmh.initial_state = NormalState;
	xwmh.flags = InputHint |StateHint;
	XSetWMHints( dpy, win, &xwmh );

	/* Create a Graphics Context for drawing */
	gcv.foreground = WhitePixel( dpy, screen );
	gcv.background = BlackPixel( dpy, screen );
#if DEBUGX
printf("Making graphics context\n");
#endif
	gc = XCreateGC( dpy, win, (GCForeground|GCBackground), &gcv );
	XI(ifp)->gc = gc;

	XSelectInput( dpy, win, ExposureMask );
	XMapWindow( dpy, win );
	XFlush(dpy);

	while( 1 ) {
		XEvent	event;
		XNextEvent( dpy, &event );
		if( event.type == Expose && event.xexpose.count == 0 ) {
			XWindowAttributes xwa;

			/* remove other exposure events */
			while( XCheckTypedEvent(dpy, Expose, &event) ) ;

			if( XGetWindowAttributes( dpy, win, &xwa ) == 0 )
				break;

			width = xwa.width;
			height = xwa.height;
			break;
		}
	}
	XSelectInput( dpy, win, ExposureMask|ButtonPressMask );

	return	0;
}

static int alive = 1;

static int
linger(FBIO *ifp)
{
	if( fork() != 0 )
		return 1;	/* release the parent */

	XSelectInput( XI(ifp)->dpy, XI(ifp)->win,
		ExposureMask|ButtonPressMask );

	while( alive ) {
		do_event(ifp);
	}
	return 0;
}

static int
do_event(FBIO *ifp)
{
	XEvent	event;
	XExposeEvent	*expose;
	int	button;
	unsigned char *bitbuf = XI(ifp)->bitbuf;
	unsigned char *bytebuf = XI(ifp)->bytebuf;
#if CURSOR
	Cursor	watch = XCreateFontCursor(XI(ifp)->dpy, XC_watch);
#endif

	expose = (XExposeEvent *)&event;


		XNextEvent( XI(ifp)->dpy, &event );
		switch( (int)event.type ) {
		case Expose:
			/*XXXfprintf(stderr,
			"expose event x= %d y= %d width= %d height= %d\n",
			expose->x, expose->y, expose->width, expose->height);*/
			XPutImage(XI(ifp)->dpy, XI(ifp)->win, XI(ifp)->gc, XI(ifp)->image,
				expose->x, expose->y, expose->x, expose->y,
				expose->width, expose->height );
			break;
		case ButtonPress:
			button = (int)event.xbutton.button;
			if( button == Button1 ) {
				/* Check for single button mouse remap.
				 * ctrl-1 => 2
				 * meta-1 => 3
				 */
				if( event.xbutton.state & ControlMask )
					button = Button2;
				else if( event.xbutton.state & Mod1Mask )
					button = Button3;
			}
			switch( button ) {
			case Button1:
				/* monochrome only for now */
				if( XI(ifp)->depth != 1 )
					break;
				/* bump method */
				if( ++(XI(ifp)->method) > 3 )
					XI(ifp)->method = 0;
#if CURSOR
				XDefineCursor(XI(ifp)->dpy, XI(ifp)->win, watch);
				XFlush(XI(ifp)->dpy);
#endif
				Monochrome(bitbuf,bytebuf,ifp->if_width,ifp->if_height,XI(ifp)->method);
#if CURSOR
				XDefineCursor(XI(ifp)->dpy, XI(ifp)->win, None);
				XFlush(XI(ifp)->dpy);
#endif
				XPutImage(XI(ifp)->dpy, XI(ifp)->win, XI(ifp)->gc, XI(ifp)->image,
					0, 0, 0, 0,
					ifp->if_width, ifp->if_height );
				break;
			case Button2:
				{
				int	x, y, sy;
				unsigned char	*cp;
				x = event.xbutton.x;
				sy = event.xbutton.y;
				/* quadrant reverse y */
				y = ifp->if_height - 1 - sy;
				cp = &bytebuf[sy*ifp->if_width + x];
				fb_log("(%4d,%4d) index=%3d ", x, y, *cp);
				if( XI(ifp)->depth == 8 ) {
					/*XXX color_defs may not be allocated here */
					fb_log("rgb=(%3d %3d %3d) ",
						color_defs[*cp].red>>8,
						color_defs[*cp].green>>8,
						color_defs[*cp].blue>>8 );
				}
				if( XI(ifp)->mem ) {
					/* 24bit buffer */
					cp = &(XI(ifp)->mem[(y*ifp->if_width + x)*3]);
					fb_log("Real RGB=(%3d %3d %3d)\n",
						cp[RED], cp[GRN], cp[BLU] );
				} else {
					fb_log("\n");
				}
				}
				break;
			case Button3:
				alive = 0;
				break;
			}
			break;
		default:
			fb_log("Bad X event.\n");
			break;
		}

	return 0;
}

/*
 * Monochrome to Bitmap conversion
 * Convert width x height 8bit grey scale bytes in bytebuf to
 * a bitmap in bitbuf, using the selected "method"
 */
static void
Monochrome(unsigned char *bitbuf, unsigned char *bytebuf, int width, int height, int method)
{
	register unsigned char *mbuffer, mvalue;   /* monochrome bitmap buffer */
	register unsigned char *mpbuffer;          /* monochrome byte buffer */
	register int row, col, bit;
#if 1
	static unsigned char MSB[8] = { 0x80, 0x40, 0x20, 0x10, 8, 4, 2, 1 };
#else
	static unsigned char LSB[8] = { 1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80 };
#endif
	register unsigned char *bits = MSB;	/*XXX - for RT, Sun, etc.  */

	error1 = (int *)malloc((unsigned)(width+1) * sizeof(int));
	error2 = (int *)malloc((unsigned)(width+1) * sizeof(int));

	mpbuffer = bytebuf;
	mbuffer = bitbuf;

	for( row = 0; row < height; row++ ) {
		for( col=0; col < width; ) {
			mvalue = 0x00;
			for( bit=0; (bit < 8) && (col < width); bit++,col++ ) {
				/*if( dither_bw(val, col, row) ) {*/
				if( method == 0 ) {
					if( (int)*mpbuffer > (dm[((row&7)<<3)+(col&7)]<<2)+1 ) {
						mvalue |= bits[bit];
					}
				} else if( method == 1 ) {
					if( fs_bw(*mpbuffer, col, row) ) {
						mvalue |= bits[bit];
					}
				} else if( method == 2 ) {
					if( mfs_bw(*mpbuffer, col, row) ) {
						mvalue |= bits[bit];
					}
				} else if( method == 3 ) {
					if( (int)*mpbuffer > (dm4[(row&3)][(col&3)]<<4)+7 ) {
						mvalue |= bits[bit];
					}
				}
				mpbuffer++;
			}
			*mbuffer++ = mvalue;
		}
	}

	free((char *)error1);
	free((char *)error2);
}

_LOCAL_ int
X_poll(FBIO *ifp)
{
	XFlush( XI(ifp)->dpy );
	while( XPending(XI(ifp)->dpy) > 0 )
		do_event(ifp);

	return(0);
}

_LOCAL_ int
X_flush(FBIO *ifp)
{
	XFlush( XI(ifp)->dpy );
	while( XPending(XI(ifp)->dpy) > 0 )
		do_event(ifp);

	return(0);
}

_LOCAL_ int
X_help(FBIO *ifp)
{
	struct	modeflags *mfp;

	fb_log( "Description: %s\n", X_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width/height: %d %d\n",
		X_interface.if_max_width,
		X_interface.if_max_height );
	fb_log( "Default width/height: %d %d\n",
		X_interface.if_width,
		X_interface.if_height );
	fb_log( "Usage: /dev/X[options]\n" );
	for( mfp = modeflags; mfp->c != '\0'; mfp++ ) {
		fb_log( "   %c   %s\n", mfp->c, mfp->help );
	}
	return(0);
}

/*
 *	c o n v R G B
 *
 *	convert a single RGBpixel to its corresponding entry in the Sun
 *	colormap.
 */
static unsigned char convRGB(register const unsigned char *v)
{
	register int r, g, b;

	r = (int)( (v)[RED]+26 ) / 51;
	g = (int)( (v)[GRN]+26 ) / 51;
	b = (int)( (v)[BLU]+26 ) / 51;

	/*printf("Pixel r = %d, g = %d, b = %d\n",(v)[RED],(v)[GRN],(v)[BLU]);*/
	if ( r == g )  {
		if( r == b )  {
			/* all grey, take average */
			return greyvec[( ((int)(v)[RED]+(int)(v)[GRN]+(int)(v)[BLU]) / 3 ) /16];
		}
		else if (r == 0)  {
			/* r=g=0, all blue */
			return bluvec[((v)[BLU])/16];
		}
		else	return r + g * 6 + b * 36;
	}
	else if (g == b && g == 0)  {
		/* all red */
		return redvec[((v)[RED])/16];
	}
	else if (r == b && r == 0)  {
		/* all green */
		return grnvec[((v)[GRN])/16];
	}
	else
		return r + g * 6 + b * 36;
}


/*
 *	G E N M A P
 *
 *	initialize the Sun harware colormap
 */
static void genmap(unsigned char *rmap, unsigned char *gmap, unsigned char *bmap)
{
	register int r, g, b;

	/* build the basic color cube */
	for (r=0 ; r < 6 ; r++)
		for (g=0 ; g < 6 ; g ++)
			for (b=0 ; b < 6 ; b++) {
				rmap[r + g * 6 + b * 36] = cubevec[r];
				gmap[r + g * 6 + b * 36] = cubevec[g];
				bmap[r + g * 6 + b * 36] = cubevec[b];
			}

	/* put in the linear sections */
	for (r=216 ; r < 226 ; ++r) {
		rmap[r] = primary[r-216];	/* red */
		gmap[r] = bmap[r] = 0;
	}

	for (g=226 ; g < 236 ; ++g) {
		gmap[g] = primary[g-226];	/* green */
		rmap[g] = bmap[g] = 0;
	}
	
	for (b=236 ; b < 246 ; ++b) {
		bmap[b] = primary[b-236];	/* blue */
		rmap[b] = gmap[b] = 0;
	}

	for (r=246 ; r < 256 ; ++r) {		/* grey */
		rmap[r] = gmap[r] = 
		bmap[r] = primary[r-246];
	}
}

static int
x_make_colormap(FBIO *ifp)
{
	int 		tot_levels;
	int 		i;
	Colormap	color_map;
	int		tmp;
	long		b, w;	/* server black and white pixels */

	tot_levels = 256;

#if DEBUGX
	printf("make_colormap\n");
#endif
#ifdef notes
        colormap = GetColormap(colors, ncolors, &newmap_flag,
		buffer, buffer_size);
#endif

	genmap(redmap, grnmap, blumap); /* generate hardware color_map */

	color_defs = (XColor *) malloc (256 * sizeof (XColor) );
	x_pixel_table = (unsigned long *)
			malloc( tot_levels * sizeof( unsigned long) );

	color_map = XCreateColormap( XI(ifp)->dpy, 
		XI(ifp)->win, XI(ifp)->visual, AllocNone);

	if( color_map == (Colormap)NULL)
		fprintf(stderr,"Warning: color map missing\n");

	XI(ifp)->cmap = color_map;

	/* Allocate the colors cells */
	if( (XAllocColorCells( XI(ifp)->dpy, color_map, 0, NULL, 0,
	      x_pixel_table, tot_levels )) == 0) {
		fprintf(stderr,"XAllocColorCells died\n");
	}

	/* XXX - HACK
	 * Swap our white entry to 0, and our back entry to 1.
	 * Fix x_pixel_table[] to remap them.  This is to allow
	 * monochrome windows to still be readable while this colormap
	 * is loaded.
	 */
	b = BlackPixel(XI(ifp)->dpy, XI(ifp)->screen);
	w = WhitePixel(XI(ifp)->dpy, XI(ifp)->screen);

	tmp = x_pixel_table[215];		/* save 215 (our white) */
	x_pixel_table[215] = x_pixel_table[w];	/* move our White to w */
	x_pixel_table[w] = tmp;			/* and orig w to 215 */

	tmp = x_pixel_table[0];			/* save 0 (our black) */
	x_pixel_table[0] = x_pixel_table[b];	/* move our Black to b */
	x_pixel_table[b] = tmp;			/* and orig b to 0 */

	/* put our colors into those cells */
	for (i = 0; i < tot_levels; i++) {
        	color_defs[i].pixel = x_pixel_table[i];
	        color_defs[i].red   = redmap[i]<<8;
	        color_defs[i].green = grnmap[i]<<8;
	        color_defs[i].blue  = blumap[i]<<8;
	        color_defs[i].flags = DoRed | DoGreen | DoBlue;
	}
	XStoreColors ( XI(ifp)->dpy, color_map, color_defs, tot_levels);

	/* assign this colormap to our window */
	XSetWindowColormap( XI(ifp)->dpy, XI(ifp)->win, XI(ifp)->cmap);

	/* If you are real anti-social, install it.
	 * This should be left to the window managers though. */
	/* XInstallColormap( XI(ifp)->dpy, color_map ); */
	if( (XI(ifp)->mode&MODE_5MASK) == MODE_5INSTCMAP ) {
		XInstallColormap( XI(ifp)->dpy, color_map );
	}
	return 0;
}

static int
x_make_cursor(FBIO *ifp)
{
	XSetWindowAttributes	xswa;

	xswa.save_under = True;
	XI(ifp)->curswin = XCreateWindow( XI(ifp)->dpy, XI(ifp)->win,
		ifp->if_xcenter, ifp->if_ycenter, 1, 1, 3,
		CopyFromParent, InputOutput, CopyFromParent,
		CWSaveUnder, &xswa );
	return 0;
}

/*
 *  A given Display (i.e. Server) can have any number of Screens.
 *  Each Screen can support one or more Visual types.
 *  unix:0.1.2 => host:display.screen.visual
 *  Typically the screen and visual default to 0 by being omitted.
 */
void
x_print_display_info(Display *dpy)
{
	int	i;
	int	screen;
	Visual	*visual;
	XVisualInfo *vp;
	int	num;
	Window	win = DefaultRootWindow(dpy);
	XStandardColormap cmap;

	printf("Server \"%s\", release %d\n",
		ServerVendor(dpy), VendorRelease(dpy) );

	/* How many screens? */
	screen = DefaultScreen(dpy);
	printf("%d Screen(s), we connected to screen %d\n",
		ScreenCount(dpy), screen );

	/* How many visuals? */
	vp = XGetVisualInfo(dpy, VisualNoMask, NULL, &num );
	XFree( (char *)vp );
	printf("%d Visual(s)\n", num);
	printf("ImageByteOrder: %s\n",
		ImageByteOrder(dpy) == MSBFirst ? "MSBFirst" : "LSBFirst");
	printf("BitmapBitOrder: %s\n",
		BitmapBitOrder(dpy) == MSBFirst ? "MSBFirst" : "LSBFirst");
	printf("BitmapUnit: %d\n", BitmapUnit(dpy));
	printf("BitmapPad: %d\n", BitmapPad(dpy));

	printf("==== Screen %d ====\n", screen );
	printf("%d x %d pixels, %d x %d mm, (%.2f x %.2f dpi)\n",
		DisplayWidth(dpy,screen), DisplayHeight(dpy,screen),
		DisplayWidthMM(dpy,screen), DisplayHeightMM(dpy,screen),
		DisplayWidth(dpy,screen)*25.4/DisplayWidthMM(dpy,screen),
		DisplayHeight(dpy,screen)*25.4/DisplayHeightMM(dpy,screen));
	printf("%d DisplayPlanes (other Visuals, if any, may vary)\n",
		DisplayPlanes(dpy,screen) );
	printf("%d DisplayCells\n", DisplayCells(dpy,screen) );
	printf("BlackPixel = %lu\n", BlackPixel(dpy,screen) );
	printf("WhitePixel = %lu\n", WhitePixel(dpy,screen) );
	printf("Save Unders: %s\n",
		DoesSaveUnders(ScreenOfDisplay(dpy,screen)) ? "True" : "False");
	i = DoesBackingStore(ScreenOfDisplay(dpy,screen));
	printf("Backing Store: %s\n", i == WhenMapped ? "WhenMapped" :
		(i == Always ? "Always" : "NotUseful"));
	printf("Installed Colormaps: min %d, max %d\n",
		MinCmapsOfScreen(ScreenOfDisplay(dpy,screen)),
		MaxCmapsOfScreen(ScreenOfDisplay(dpy,screen)) );
	printf("DefaultColormap: 0lx%lx\n", DefaultColormap(dpy,screen));

	visual = DefaultVisual(dpy,screen);
	printf("---- Visual 0x%lx ----\n", (unsigned long int)visual );

	switch(visual->class) {
	case DirectColor:
		printf("DirectColor: Alterable RGB maps, pixel RGB subfield indicies\n");
		printf("RGB Masks: 0x%lx 0x%lx 0x%lx\n", visual->red_mask,
			visual->green_mask, visual->blue_mask);
		break;
	case TrueColor:
		printf("TrueColor: Fixed RGB maps, pixel RGB subfield indicies\n");
		printf("RGB Masks: 0x%lx 0x%lx 0x%lx\n", visual->red_mask,
			visual->green_mask, visual->blue_mask);
		break;
	case PseudoColor:
		printf("PseudoColor: Alterable RGB maps, single index\n");
		break;
	case StaticColor:
		printf("StaticColor: Fixed RGB maps, single index\n");
		break;
	case GrayScale:
		printf("GrayScale: Alterable map (R=G=B), single index\n");
		break;
	case StaticGray:
		printf("StaticGray: Fixed map (R=G=B), single index\n");
		break;
	default:
		printf("Unknown visual class %d\n",
			visual->class);
		break;
	}
	printf("Map Entries: %d\n", visual->map_entries);
	printf("Bits per RGB: %d\n", visual->bits_per_rgb);

	printf("==== Standard Colormaps ====\n");
	if( XGetStandardColormap( dpy, win, &cmap, XA_RGB_BEST_MAP ) ) {
		printf( "XA_RGB_BEST_MAP    - Yes (0x%lx)\n", cmap.colormap);
		printf( "R[0..%lu] * %lu + G[0..%lu] * %lu  + B[0..%lu] * %lu + %lu\n",
			cmap.red_max, cmap.red_mult, cmap.green_max, cmap.green_mult,
			cmap.blue_max, cmap.blue_mult, cmap.base_pixel);
	} else
		printf( "XA_RGB_BEST_MAP    - No\n" );
	if( XGetStandardColormap( dpy, win, &cmap, XA_RGB_DEFAULT_MAP ) ) {
		printf( "XA_RGB_DEFAULT_MAP - Yes (0x%lx)\n", cmap.colormap );
		printf( "R[0..%lu] * %lu + G[0..%lu] * %lu  + B[0..%lu] * %lu + %lu\n",
			cmap.red_max, cmap.red_mult, cmap.green_max, cmap.green_mult,
			cmap.blue_max, cmap.blue_mult, cmap.base_pixel);
	} else
		printf( "XA_RGB_DEFAULT_MAP - No\n" );
	if( XGetStandardColormap( dpy, win, &cmap, XA_RGB_GRAY_MAP ) ) {
		printf( "XA_RGB_GRAY_MAP    - Yes (0x%lx)\n", cmap.colormap );
		printf( "R[0..%lu] * %lu + %lu\n",
			cmap.red_max, cmap.red_mult, cmap.base_pixel);
	} else
		printf( "XA_RGB_GRAY_MAP    - No\n" );
}

/* Repaint.
 * Zooming an image can also be viewed as shrinking our "window" of
 * currently displayed data.  We then drop that window down over the
 * selected "center" position and check to see if it overhangs the
 * image edges.  We want to clear out any overhanging regions and then
 * display the (sub)rectangle of image data that falls in the window.
 *
 * We do all of our computation here in the first quadrant form and
 * only switch to fourth for Xlib commands.
 */
static void
repaint(FBIO *ifp)
{
	/* 1st and last image pixel coordinates *within* the window */
	int	xmin, xmax;
	int	ymin, ymax;
	/* screen pixel coordinates corresponding to above */
	int	sleft, sright;
	int	sbottom, stop;
	/* window height, width, and center */
	struct	{
		int	width;
		int	height;
		int	xcenter;
		int	ycenter;
	} w;

	/*
	 * Eventually the window size shouldn't be bound to
	 * the image size.  We are thinking ahead here.
	 */
	w.width = ifp->if_width;
	w.height = ifp->if_height;
	w.xcenter = w.width / 2;
	w.ycenter = w.height / 2;

	/*
	 * Compute which image pixels WOULD fall at the left, right,
	 * bottom, and top of the window if the image were unbounded.
	 */
	xmin = ifp->if_xcenter - w.width/(2*ifp->if_xzoom);
	xmax = ifp->if_xcenter + w.width/(2*ifp->if_xzoom) - 1;
	ymin = ifp->if_ycenter - w.height/(2*ifp->if_yzoom);
	ymax = ifp->if_ycenter + w.height/(2*ifp->if_yzoom) - 1;

	/*
	 * Clip these against the actual image dimensions.
	 */
	if( xmin < 0 )
		xmin = 0;
	if( xmax > ifp->if_width-1 )
		xmax = ifp->if_width-1;
	if( ymin < 0 )
		ymin = 0;
	if( ymax > ifp->if_height-1 )
		ymax = ifp->if_height-1;

	/*
	 * Compute the window pixel location corresponding to
	 * the included image data.
	 */
	sleft = (xmin - ifp->if_xcenter) * ifp->if_xzoom + w.width/2;
	sright = ((xmax+1) - ifp->if_xcenter) * ifp->if_xzoom + w.width/2 - 1;
	sbottom = (ymin - ifp->if_ycenter) * ifp->if_yzoom + w.height/2;
	stop = ((ymax+1) - ifp->if_ycenter) * ifp->if_yzoom + w.height/2 - 1;

	/*
	 * if(sleft || sbottom || sright != w.width-1 || stop != w.height-1)
	 *  then the image rectangle does not completely fill the screen
	 *  window rectangle.  I tried clearing the entire window first
	 *  and then repainting the image part, but this produced a nasty
	 *  flashing of the screen.  It is debatable whether we should clear
	 *  the borders before or after painting.  Ultimately it would be
	 *  nice if we could optimize with an in server XCopyArea bitblit
	 *  for the part of the image already displayed (if any).
	 */
	/*
	printf("window(%3d,%3d) zoom(%3d,%3d)\n\r",
		ifp->if_xcenter, ifp->if_ycenter,
		ifp->if_xzoom, ifp->if_yzoom);
	printf("Image ([%3d %3d],[%3d %3d]) Screen ([%3d %3d],[%3d %3d])\n\r",
		xmin, xmax, ymin, ymax, sleft, sright, sbottom, stop);
	*/

	/* Display our image rectangle */
	if( (ifp->if_xzoom == 1) && (ifp->if_yzoom == 1 ) ) {
		/* Note quadrant reversal */
		XPutImage(XI(ifp)->dpy, XI(ifp)->win, XI(ifp)->gc,
			XI(ifp)->image,
			xmin, w.width-1-ymax,
			sleft, w.height-1-stop,
			xmax-xmin+1, ymax-ymin+1 );
	} else {
		slowrect( ifp, xmin, xmax, ymin, ymax );
	}

	/* Clear any empty borders */
	if( sleft != 0 )
		XClearArea( XI(ifp)->dpy, XI(ifp)->win,
			0, 0,
			sleft, w.height,
			False);
	if( sright != w.width-1 )
		XClearArea( XI(ifp)->dpy, XI(ifp)->win,
			sright+1, 0,
			w.width-1-sright, w.height,
			False);
	/* could optimize bottom and top to clip sleft to sright */
	if( sbottom != 0 )
		XClearArea( XI(ifp)->dpy, XI(ifp)->win,
			0, w.height-sbottom,  /* (height-1)-(bottom-1) */
			w.width, sbottom,
			False);
	if( stop != w.height-1 )
		XClearArea( XI(ifp)->dpy, XI(ifp)->win,
			0, 0,
			w.width, w.height-1-stop,
			False);

	XFlush( XI(ifp)->dpy );
}

/*
 * Note: these return the "lower left corner" of a zoomed pixel
 */
#define	xIMG2SCR(x)	(((x)-ifp->if_xcenter)*ifp->if_xzoom+w.width/2)
#define	yIMG2SCR(y)	(((y)-ifp->if_ycenter)*ifp->if_yzoom+w.height/2)

/*
 * Repaint a (pre clipped) rectangle from the image onto the screen.
 */
static void
slowrect(FBIO *ifp, int xmin, int xmax, int ymin, int ymax)
          
               	/* image bounds */
               
{
	int	sxmin;		/* screen versions of above */
	int	symin;
	int	xlen, ylen;	/* number of image pixels in x,y */
	int	sxlen, sylen;	/* screen pixels in x,y */
	int	ix, iy;		/* image x, y */
	int	sy;		/* screen x, y */
	int	x, y;		/* dummys */
	/* window height, width, and center */
	struct	{
		int	width;
		int	height;
		int	xcenter;
		int	ycenter;
	} w;
	/*XXX-HACK VERSION-Depend on 24bit memory buffer and use scanwrite! */
	RGBpixel scanbuf[1024];
	RGBpixel *pp;
	if( XI(ifp)->mem == NULL )
		return;

	/*
	 * Eventually the window size shouldn't be bound to
	 * the image size.  We are thinking ahead here.
	 */
	w.width = ifp->if_width;
	w.height = ifp->if_height;
	w.xcenter = w.width / 2;
	w.ycenter = w.height / 2;

	xlen = xmax - xmin + 1;
	ylen = ymax - ymin + 1;
	sxlen = xlen * ifp->if_xzoom;
	sylen = ylen * ifp->if_yzoom;

	sxmin = xIMG2SCR(xmin);
	symin = yIMG2SCR(ymin);

	for( y = 0; y < sylen; y++ ) {
		sy = symin + y;
		iy = ymin + y/ifp->if_yzoom; 
		for( x = 0; x < sxlen; x++ ) {
			ix = xmin + x/ifp->if_xzoom;
#if 0
			sx = sxmin + x;
			printf("S(%3d,%3d) <- I(%3d,%3d)\n", sx, sy, ix, iy);
#endif
			pp = (RGBpixel *)&(XI(ifp)->mem[(iy*ifp->if_width+ix)*3]);
			scanbuf[x][RED] = (*pp)[RED];
			scanbuf[x][GRN] = (*pp)[GRN];
			scanbuf[x][BLU] = (*pp)[BLU];
		}
		/*printf("Write Scan %d pixels @ S(%d,%d)\n", sxlen, sxmin, sy);*/
		X_scanwrite( ifp, sxmin, sy, &scanbuf[0][0], sxlen, 0 );
	}
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
