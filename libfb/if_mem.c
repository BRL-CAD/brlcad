/*
 *			I F _ M E M . C
 *
 *  A Memory (virtual) Frame Buffer Interface.
 *
 *  Authors -
 *	Phillip Dykstra
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1989 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "externs.h"			/* For calloc */
#include "fb.h"
#include "./fblocal.h"

_LOCAL_ int	mem_open(),
		mem_close(),
		mem_clear(),
		mem_read(),
		mem_write(),
		mem_rmap(),
		mem_wmap(),
		mem_view(),
		mem_getview(),
		mem_setcursor(),
		mem_cursor(),
		mem_getcursor(),
		mem_poll(),
		mem_flush(),
		mem_help();

/* This is the ONLY thing that we normally "export" */
FBIO memory_interface =  {
	0,
	mem_open,		/* device_open		*/
	mem_close,		/* device_close		*/
	mem_clear,		/* device_clear		*/
	mem_read,		/* buffer_read		*/
	mem_write,		/* buffer_write		*/
	mem_rmap,		/* colormap_read	*/
	mem_wmap,		/* colormap_write	*/
	mem_view,		/* set view		*/
	mem_getview,		/* get view		*/
	mem_setcursor,		/* define cursor	*/
	mem_cursor,		/* set cursor		*/
	mem_getcursor,		/* get cursor		*/
	fb_sim_readrect,	/* rectangle read	*/
	fb_sim_writerect,	/* rectangle write	*/
	fb_sim_bwreadrect,
	fb_sim_bwwriterect,
	mem_poll,		/* poll			*/
	mem_flush,		/* flush		*/
	mem_close,		/* free			*/
	mem_help,		/* help message		*/
	"Memory Buffer",	/* device description	*/
	8192,			/* max width		*/
	8192,			/* max height		*/
	"/dev/mem",		/* short device name	*/
	512,			/* default/current width  */
	512,			/* default/current height */
	-1,			/* select fd		*/
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

/* Per connection private info */
struct	meminfo {
	FBIO	*fbp;		/* attached frame buffer (if any) */
	unsigned char *mem;	/* memory frame buffer */
	ColorMap cmap;		/* color map buffer */
	int	mem_dirty;	/* !0 implies unflushed written data */
	int	cmap_dirty;	/* !0 implies unflushed written cmap */
	int	write_thru;	/* !0 implies pass-thru write mode */
};
#define	MI(ptr) ((struct meminfo *)((ptr)->u1.p))
#define	MIL(ptr) ((ptr)->u1.p)		/* left hand side version */

#define MODE_1MASK	(1<<1)
#define MODE_1BUFFERED	(0<<1)		/* output flushed only at close */
#define MODE_1IMMEDIATE	(1<<1)		/* pass-through writes */

#define MODE_2MASK	(1<<2)
#define MODE_2CLEAR	(0<<2)		/* assume fb opens clear */
#define MODE_2PREREAD	(1<<2)		/* pre-read data from fb */

static struct modeflags {
	char	c;
	long	mask;
	long	value;
	char	*help;
} modeflags[] = {
	{ 'w',	MODE_1MASK, MODE_1IMMEDIATE,
		"Write thru mode - pass writes directly to attached frame buffer" },
	{ 'r',  MODE_2MASK, MODE_2PREREAD,
		"Pre-Read attached frame buffer data - else assumes clear" },
	{ '\0', 0, 0, "" }
};

_LOCAL_ int
mem_open( ifp, file, width, height )
FBIO	*ifp;
char	*file;
int	width, height;
{
	int	mode;
	char	*cp;
	FBIO	*fbp;

	FB_CK_FBIO(ifp);

	/*
	 *  First, attempt to determine operating mode for this open,
	 *  based upon the "unit number" or flags.
	 *  file = "/dev/mem###"
	 *  The default mode is zero.
	 */
	mode = 0;

	if( file != NULL )  {
		register char *cp;
		char	modebuf[80];
		char	*mp;
		int	alpha;
		struct	modeflags *mfp;

		if( strncmp(file, "/dev/mem", 8) ) {
			/* How did this happen?? */
			mode = 0;
		}
		else {
			/* Parse the options */
			alpha = 0;
			mp = &modebuf[0];
			cp = &file[8];
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
					fb_log( "if_mem: unknown option '%c' ignored\n", *cp );
				}
				cp++;
			}
			*mp = '\0';
			if( !alpha )
				mode = atoi( modebuf );
		}
	}

	/* build a local static info struct */
	if( (MIL(ifp) = (char *)calloc( 1, sizeof(struct meminfo) )) == NULL )  {
		fb_log("mem_open:  meminfo malloc failed\n");
		return(-1);
	}
	cp = &file[strlen("/dev/mem")];
	while( *cp != '\0' && *cp != ' ' && *cp != '\t' )
		cp++;	/* skip suffix */
	while( *cp != '\0' && (*cp == ' ' || *cp == '\t' || *cp == ';') )
		cp++;	/* skip blanks and separators */

	if( *cp ) {
		/* frame buffer device specified */
		if( (fbp = fb_open(cp, width, height)) == FBIO_NULL ) {
			free( MIL(ifp) );
			return( -1 );
		}
		MI(ifp)->fbp = fbp;
		ifp->if_width = fbp->if_width;
		ifp->if_height = fbp->if_height;
		ifp->if_selfd = fbp->if_selfd;
		if( (mode & MODE_1MASK) == MODE_1IMMEDIATE )
			MI(ifp)->write_thru = 1;
	} else {
		/* no frame buffer specified */
		if( width > 0 )
			ifp->if_width = width;
		if( height > 0 )
			ifp->if_height = height;
	}
	if( (MI(ifp)->mem = (unsigned char *)calloc( ifp->if_width*ifp->if_height, 3 )) == NULL ) {
		fb_log("mem_open:  memory buffer malloc failed\n");
		(void)free( MIL(ifp) );
		return(-1);
	}
	if( (MI(ifp)->fbp != FBIO_NULL)
	 && (mode & MODE_2MASK) == MODE_2PREREAD ) {
		/* Pre read all of the image data and cmap */
	 	int got;
		got = fb_readrect( MI(ifp)->fbp, 0, 0,
			ifp->if_width, ifp->if_height,
			(unsigned char *)MI(ifp)->mem );
	 	if( got != ifp->if_width * ifp->if_height )  {
	 		fb_log("if_mem:  WARNING: pre-read of %d only got %d, your image is truncated.\n",
	 			ifp->if_width * ifp->if_height, got );
	 	}
		if( fb_rmap( MI(ifp)->fbp, &(MI(ifp)->cmap) ) < 0 )
			fb_make_linear_cmap( &(MI(ifp)->cmap) );
	} else {
		/* Image data begins black, colormap linear */
		fb_make_linear_cmap( &(MI(ifp)->cmap) );
	}

	return(0);
}

_LOCAL_ int
mem_close( ifp )
FBIO	*ifp;
{
	/*
	 * Flush memory/cmap to attached frame buffer if any
	 */
	if( MI(ifp)->fbp != FBIO_NULL ) {
		if( MI(ifp)->cmap_dirty ) {
			fb_wmap( MI(ifp)->fbp, &(MI(ifp)->cmap) );
		}
		if( MI(ifp)->mem_dirty ) {
			fb_writerect( MI(ifp)->fbp, 0, 0,
				ifp->if_width, ifp->if_height, (unsigned char *)MI(ifp)->mem );
		}
		fb_close( MI(ifp)->fbp );
		MI(ifp)->fbp = FBIO_NULL;
	}
	(void)free( (char *)MI(ifp)->mem );
	(void)free( (char *)MIL(ifp) );

	return(0);
}

_LOCAL_ int
mem_clear( ifp, pp )
FBIO	*ifp;
unsigned char	*pp;
{
	RGBpixel v;
	register int n;
	register unsigned char *cp;

	if( pp == RGBPIXEL_NULL ) {
		v[RED] = v[GRN] = v[BLU] = 0;
	} else {
		v[RED] = (pp)[RED];
		v[GRN] = (pp)[GRN];
		v[BLU] = (pp)[BLU];
	}

	cp = MI(ifp)->mem;
	if( v[RED] == v[GRN] && v[RED] == v[BLU] ) {
		int	bytes = ifp->if_width*ifp->if_height*3;
		if( v[RED] == 0 )
			bzero( (char *)cp, bytes );	/* all black */
		else
			memset( cp, v[RED], bytes );	/* all grey */
	} else {
		for( n = ifp->if_width*ifp->if_height; n; n-- ) {
			*cp++ = v[RED];
			*cp++ = v[GRN];
			*cp++ = v[BLU];
		}
	}
	if( MI(ifp)->write_thru ) {
		return fb_clear( MI(ifp)->fbp, pp );
	} else {
		MI(ifp)->mem_dirty = 1;
	}
	return(0);
}

_LOCAL_ int
mem_read( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
unsigned char	*pixelp;
int	count;
{
	int	pixels_to_end;

	if( x < 0 || x >= ifp->if_width || y < 0 || y >= ifp->if_height )
		return( -1 );

	/* make sure we don't run off the end of the buffer */
	pixels_to_end = ifp->if_width*ifp->if_height - (y*ifp->if_width + x);
	if( pixels_to_end < count )
		count = pixels_to_end;

	bcopy( &(MI(ifp)->mem[(y*ifp->if_width + x)*3]), (char *)pixelp,
		count*3 );

	return(count);
}

_LOCAL_ int
mem_write( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
const unsigned char	*pixelp;
int	count;
{
	int	pixels_to_end;

	if( x < 0 || x >= ifp->if_width || y < 0 || y >= ifp->if_height )
		return( -1 );

	/* make sure we don't run off the end of the buffer */
	pixels_to_end = ifp->if_width*ifp->if_height - (y*ifp->if_width + x);
	if( pixels_to_end < count )
		count = pixels_to_end;

	bcopy( (char *)pixelp, &(MI(ifp)->mem[(y*ifp->if_width + x)*3]),
		count*3 );

	if( MI(ifp)->write_thru ) {
		return fb_write( MI(ifp)->fbp, x, y, pixelp, count );
	} else {
		MI(ifp)->mem_dirty = 1;
	}
	return(count);
}

_LOCAL_ int
mem_rmap( ifp, cmp )
FBIO	*ifp;
ColorMap	*cmp;
{
	*cmp = MI(ifp)->cmap;		/* struct copy */
	return(0);
}

_LOCAL_ int
mem_wmap( ifp, cmp )
FBIO	*ifp;
const ColorMap	*cmp;
{
	if( cmp == COLORMAP_NULL )  {
		fb_make_linear_cmap( &(MI(ifp)->cmap) );
	} else {
		MI(ifp)->cmap = *cmp;		/* struct copy */
	}

	if( MI(ifp)->write_thru ) {
		return fb_wmap( MI(ifp)->fbp, cmp );
	} else {
		MI(ifp)->cmap_dirty = 1;
	}
	return(0);
}

_LOCAL_ int
mem_view( ifp, xcenter, ycenter, xzoom, yzoom )
FBIO	*ifp;
int	xcenter, ycenter;
int	xzoom, yzoom;
{
	fb_sim_view( ifp, xcenter, ycenter, xzoom, yzoom );
	if( MI(ifp)->write_thru ) {
		return fb_view( MI(ifp)->fbp, xcenter, ycenter,
			xzoom, yzoom );
	}
	return(0);
}

_LOCAL_ int
mem_getview( ifp, xcenter, ycenter, xzoom, yzoom )
FBIO	*ifp;
int	*xcenter, *ycenter;
int	*xzoom, *yzoom;
{
	if( MI(ifp)->write_thru ) {
		return fb_getview( MI(ifp)->fbp, xcenter, ycenter,
			xzoom, yzoom );
	}
	fb_sim_getview( ifp, xcenter, ycenter, xzoom, yzoom );
	return(0);
}

_LOCAL_ int
mem_setcursor( ifp, bits, xbits, ybits, xorig, yorig )
FBIO	*ifp;
const unsigned char *bits;
int	xbits, ybits;
int	xorig, yorig;
{
	if( MI(ifp)->write_thru ) {
		return fb_setcursor( MI(ifp)->fbp,
			bits, xbits, ybits, xorig, yorig );
	}
	return(0);
}

_LOCAL_ int
mem_cursor( ifp, mode, x, y )
FBIO	*ifp;
int	mode;
int	x, y;
{
	fb_sim_cursor( ifp, mode, x, y );
	if( MI(ifp)->write_thru ) {
		return fb_cursor( MI(ifp)->fbp, mode, x, y );
	}
	return(0);
}

_LOCAL_ int
mem_getcursor( ifp, mode, x, y )
FBIO	*ifp;
int	*mode;
int	*x, *y;
{
	if( MI(ifp)->write_thru ) {
		return fb_getcursor( MI(ifp)->fbp, mode, x, y );
	}
	fb_sim_getcursor( ifp, mode, x, y );
	return(0);
}

_LOCAL_ int
mem_poll( ifp )
FBIO	*ifp;
{
	if( MI(ifp)->write_thru ) {
		return fb_poll( MI(ifp)->fbp );
	}
	return(0);
}

_LOCAL_ int
mem_flush( ifp )
FBIO	*ifp;
{
	/*
	 * Flush memory/cmap to attached frame buffer if any
	 */
	if( MI(ifp)->fbp != FBIO_NULL ) {
		if( MI(ifp)->cmap_dirty ) {
			fb_wmap( MI(ifp)->fbp, &(MI(ifp)->cmap) );
			MI(ifp)->cmap_dirty = 0;
		}
		if( MI(ifp)->mem_dirty ) {
			fb_writerect( MI(ifp)->fbp, 0, 0,
				ifp->if_width, ifp->if_height, (unsigned char *)MI(ifp)->mem );
			MI(ifp)->mem_dirty = 0;
		}
		return	fb_flush( MI(ifp)->fbp );
	}

	MI(ifp)->cmap_dirty = 0;
	MI(ifp)->mem_dirty = 0;
	return	0;	/* success */
}

_LOCAL_ int
mem_help( ifp )
FBIO	*ifp;
{
	struct	modeflags *mfp;

	fb_log( "Description: %s\n", memory_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width/height: %d %d\n",
		memory_interface.if_max_width,
		memory_interface.if_max_height );
	fb_log( "Default width/height: %d %d\n",
		memory_interface.if_width,
		memory_interface.if_height );
	fb_log( "Usage: /dev/mem[options] [attached_framebuffer]\n" );
	for( mfp = modeflags; mfp->c != '\0'; mfp++ ) {
		fb_log( "   %c   %s\n", mfp->c, mfp->help );
	}
	return(0);
}
