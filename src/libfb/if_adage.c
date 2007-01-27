/*                      I F _ A D A G E . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup if */
/** @{ */
/** @file if_adage.c
 *
 * This module is used when pre-setting the Ikonas FBC
 * (Frame Buffer Controller) to a known state.
 * The values for this table are derived from the
 * Ikonas-supplied program FBI, for compatability.
 * At present, these modes can be set:
 *	0 - LORES, 30 hz, interlaced
 *	1 - LORES, 60 hz, non-interlaced
 *	2 - HIRES, 30 hz, interlaced
 *	3 - LORES, 30 hz, interlaced, with NTSC timing
 *	4 - LORES, 30 hz, interlaced, with external sync and NTSC timing
 *
 * All that is provided is a prototype for the FBC registers;
 * the user is responsible for changing them (zooming, etc),
 * and writing them to the FBC.

 *  Authors -
 *	Phil Dykstra
 *	Gary S. Moss
 *	Mike J. Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/** @} */

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/ioctl.h>

#include "machine.h"
#include "fb.h"

#include "./fblocal.h"
#include "./adage.h"


HIDDEN int	adage_open(FBIO *ifp, char *file, int width, int height),
		adage_close(FBIO *ifp),
		adage_clear(FBIO *ifp, RGBpixel (*bgpp)),
		adage_read(FBIO *ifp, int x, int y, RGBpixel (*pixelp), long int count),
		adage_write(FBIO *ifp, int x, int y, RGBpixel (*pixelp), long int count),
		adage_rmap(FBIO *ifp, register ColorMap *cp),
		adage_wmap(FBIO *ifp, register ColorMap *cp),
		adage_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom),
		adage_window_set(register FBIO *ifp, int x, int y),	/* OLD */
		adage_zoom_set(FBIO *ifp, register int x, register int y),	/* OLD */
		adage_setcursor(FBIO *ifp, unsigned char *bits, int xbits, int ybits, int xorig, int yorig),
		adage_cursor(FBIO *ifp, int mode, int x, int y),
		adage_help(FBIO *ifp);

FBIO adage_interface = {
		0,
		adage_open,
		adage_close,
		adage_clear,
		adage_read,
		adage_write,
		adage_rmap,
		adage_wmap,
		adage_view,
		fb_sim_getview,
		adage_setcursor,
		adage_cursor,
		fb_sim_getcursor,
		fb_sim_readrect,
		fb_sim_writerect,
		fb_sim_bwreadrect,
		fb_sim_bwwriterect,
		fb_null,		/* poll */
		fb_null,		/* flush */
		adage_close,		/* free */
		adage_help,
		"Adage RDS3000",
		1024,
		1024,
		"/dev/ik",
		512,
		512,
		-1,
		-1,
		1, 1,			/* zoom */
		256, 256,		/* window */
		0, 0, 0,		/* cursor */
		PIXEL_NULL,
		PIXEL_NULL,
		PIXEL_NULL,
		-1,
		0,
		0L,
		0L,
		0
};

static struct ik_fbc ikfbc_setup[5] = {
    {
	/* 0 - LORES, 30 hz, interlaced */
	0,	32,		/* x, y, viewport */
	511,	511,		/* x, y, sizeview */
	0,	4067,		/* x, y, window */
	0,	0,		/* x, y, zoom */
	300,	560,		/* horiztime, nlines */
	0,	FBCH_PIXELCLOCK(45) | FBCH_DRIVEBPCK, /* Lcontrol, Hcontrol */
	0,	32		/* x, y, cursor */
    }, {
	/* 1 - LORES, 60 hz, non-interlaced */
	0,	68,		/* viewport */
	511,	1023,		/* sizeview */
	0,	4063,		/* window, -33 */
	0,	0,		/* zoom */
	144,	1143,		/* horiztime, nlines (was 144, 1167) */
	FBC_RS343 | FBC_NOINTERLACE, FBCH_PIXELCLOCK(18) | FBCH_DRIVEBPCK,
	0,	32		/* cursor */
    }, {
	/* 2 - HIRES, 30 hz, interlaced */
	0,	64,
	1023,	1023,
	0,	4033,		/* -63 */
	0,	0,
	144,	1144,		/* was 144, 1166 */
	FBC_HIRES | FBC_RS343, FBCH_PIXELCLOCK(19) | FBCH_DRIVEBPCK,
	0,	64
    }, {
	/* 3 - LORES, 30 hz, interlaced, *NTSC*, internal sync */
	0,	32,		/* x, y, viewport */
	511,	511,		/* x, y, sizeview */
	0,	4067,		/* x, y, window */
	0,	0,		/* x, y, zoom */
	300,	560,		/* horiztime, nlines [no effect] */
	0,	FBCH_PIXELCLOCK(47) | FBCH_DRIVEBPCK,
				/* Lcontrol, Hcontrol */
	0,	32		/* x, y, cursor */
    }, {
	/* 4 - LORES, 30 hz, interlaced, *NTSC*, external sync */
	0,	32,		/* x, y, viewport */
	511,	511,		/* x, y, sizeview */
	0,	4067,		/* x, y, window */
	0,	0,		/* x, y, zoom */
	300,	560,		/* horiztime, nlines [no effect] */
	FBC_EXTERNSYNC,	FBCH_PIXELCLOCK(47) | FBCH_DRIVEBPCK,
				/* Lcontrol, Hcontrol */
	0,	32		/* x, y, cursor */
    }
};

/*
 * Per adage state information.
 */
struct	ikinfo {
	struct	ik_fbc	ikfbcmem;	/* Current FBC state */
	short	*_ikUBaddr;		/* Mapped-in Ikonas address */
	/* Current values initialized in adage_init() */
	int	mode;			/* 0,1,2 */
	int	x_window, y_window;	/* Ikonas, upper left of window */
	int	y_winoff;		/* y window correction factor */
	int	x_corig, y_corig;	/* cursor origin offsets */
};
#define	IKI(ptr) ((struct ikinfo *)((ptr)->u1.p))
#define	IKIL(ptr) ((ptr)->u1.p)		/* left hand side version */

struct	adage_cursor {
	int	xbits, ybits;
	int	xorig, yorig;
	unsigned char bits[32*4];
};

static struct adage_cursor default_cursor = {
/*#include "./adageframe.h"*/
#include "./adagecursor.h"
};

/*
 * RGBpixel -> Ikonas pixel buffer
 * Used by both buffer_read & write, AND adage_color_clear
 * so be sure it holds ADAGE_DMA_BYTES.
 */
static char *_pixbuf = NULL;
typedef unsigned char IKONASpixel[4];

#define	ADAGE_DMA_BYTES	(63*1024)
#define	ADAGE_DMA_PIXELS (ADAGE_DMA_BYTES/sizeof(IKONASpixel))

/*
 *			A D A G E _ D E V I C E _ O P E N
 *
 *  Compute a fairly interesting mapping.  We are handed a string in
 *  one of these forms:
 *	/dev/ik		for ik0 (implicit number)
 *	/dev/ik3	for ik3
 *	/dev/ik4n	for ik4, no init
 *	/dev/ik5v	for ik5, NTSC Video
 *	/dev/i65e	for ik6, NTSC Video, external sync
 *
 *  Using the BRL-enhanced "lseek interface", we have to open a
 *  device node using a file name of the form:
 *	/dev/ik3l	for low-res, or
 *	/dev/ik4h	for high-res
 *
 * The device name MUST BEGIN with a string which matches
 * the generic name of the device as defined in the FBIO
 * structure.
 */
HIDDEN int
adage_open(FBIO *ifp, char *file, int width, int height)
{
	register int	i;
	register char	*cp;
	char	ourfile[32];
	long	xbsval[34];
	int	unit = 0;
	int	noinit = 0;
	int	ntsc = 0;
	int	ext_sync = 0;

	FB_CK_FBIO(ifp);

	/* Only 512 and 1024 opens are available */
	if( width > 512 || height > 512 )
		width = height = 1024;
	else
		width = height = 512;

	/* "/dev/ik###" gives unit */
	for( cp = file; *cp != '\0' && !isdigit(*cp); cp++ ) ;
	unit = 0;
	if( *cp && isdigit(*cp) )
		(void)sscanf( cp, "%d", &unit );
	while( *cp != '\0' && isdigit(*cp) )  cp++;	/* skip number */
	if( *cp != '\0' )  switch( *cp )  {
		case 'n':
			noinit = 1;
			break;
		case 'e':
			ext_sync = 1;
			ntsc = 1;
			width = height = 512;
			break;
		case 'v':
			ntsc = 1;
			width = height = 512;
			break;
		case 'l':
		case 'h':
			break;		/* Discard, for compatability */
		default:
			fb_log( "adage_open: Bad unit suffix %s\n", cp );
			return(-1);
	}

	(void)sprintf( ourfile, "/dev/ik%d%c", unit, width>512 ? 'h' : 'l');

	if( (ifp->if_fd = open( ourfile, O_RDWR, 0 )) == -1 )
		return	-1;

	/* create a clean ikinfo struct */
	if( (IKIL(ifp) = (char *)calloc( 1, sizeof(struct ikinfo) )) == NULL ) {
		fb_log( "adage_open: ikinfo malloc failed\n" );
		return	-1;
	}
#if defined( vax )
	if( ioctl( ifp->if_fd, IKIOGETADDR, &(IKI(ifp)->_ikUBaddr) ) < 0 )
		fb_log( "adage_open : ioctl(IKIOGETADDR) failed.\n" );
#endif
	ifp->if_width = width;
	ifp->if_height = height;
	if( ntsc )  {
		if( ext_sync )
			IKI(ifp)->mode = 4;
		else
			IKI(ifp)->mode = 3;
	} else switch( ifp->if_width ) {
	case 512:
		IKI(ifp)->mode = 1;
		break;
	case 1024:
		IKI(ifp)->mode = 2;
		break;
	default:
		fb_log( "Bad fbsize %d.\n", ifp->if_width );
		return	-1;
	}


	/* Don't initialize the Ikonas if opening the 'noinit' device,
	 * which is needed for the Lyon-Lamb video-tape controller,
	 * which is unhappy if the frame clock is shut off.
	 */
	if( !noinit )  {
		if( lseek( ifp->if_fd, (off_t)FBC*4L, 0 ) == -1 ) {
			fb_log( "adage_open : lseek failed.\n" );
			return	-1;
		}
		if( write( ifp->if_fd, (char *)&ikfbc_setup[IKI(ifp)->mode],
		    sizeof(struct ik_fbc) ) != sizeof(struct ik_fbc) ) {
			fb_log( "adage_open : write failed.\n" );
			return	-1;
		}
		IKI(ifp)->ikfbcmem = ikfbc_setup[IKI(ifp)->mode];/* struct copy */

		/* Build an identity for the crossbar switch */
		for( i=0; i < 34; i++ )
			xbsval[i] = (long)i;
		if( lseek( ifp->if_fd, (off_t)XBS*4L, 0 ) == -1 ) {
			fb_log( "adage_open : lseek failed.\n" );
			return	-1;
		}
		if( write( ifp->if_fd, (char *) xbsval, sizeof(xbsval) )
		    != sizeof(xbsval) ) {
			fb_log( "adage_open : write failed.\n" );
			return	-1;
		}

		/* Initialize the LUVO crossbar switch, too */
		xbsval[0] = 0x24L;		/* 1:1 mapping, magic number */
		if( lseek( ifp->if_fd, (off_t)LUVOXBS*4L, 0 ) == -1 ) {
			fb_log( "adage_open : lseek failed.\n" );
			return	-1;
		}
		if( write( ifp->if_fd, (char *) xbsval, sizeof(long) )
		    != sizeof(long) ) {
			fb_log( "adage_open : write failed.\n" );
			return	-1;
		}

		/* Dump in default cursor. */
		if( adage_setcursor( ifp, default_cursor.bits,
		    default_cursor.xbits, default_cursor.ybits,
		    default_cursor.xorig, default_cursor.yorig ) == -1 )
			return	-1;
	}

	/* seek to start of pixels */
	if( lseek( ifp->if_fd, (off_t)0L, 0 ) == -1 ) {
		fb_log( "adage_open : lseek failed.\n" );
		return	-1;
	}
	/* Create pixel buffer */
	if( _pixbuf == NULL ) {
		if( (_pixbuf = malloc( ADAGE_DMA_BYTES )) == NULL ) {
			fb_log( "adage_open : pixbuf malloc failed.\n" );
			return	-1;
		}
	}
	ifp->if_xzoom = 1;
	ifp->if_yzoom = 1;
	IKI(ifp)->x_window = 0;
	IKI(ifp)->y_window = 0;
	/* 12bit 2's complement window setting */
	i = ikfbc_setup[IKI(ifp)->mode].fbc_ywindow;
	if( i >= 2048 )
		i = - (4096 - i);
	IKI(ifp)->y_winoff = i;
	return	ifp->if_fd;
}

HIDDEN int
adage_close(FBIO *ifp)
{
	/* free ikinfo struct */
	if( IKIL(ifp) != NULL )
		(void) free( IKIL(ifp) );

	return	close( ifp->if_fd );
}

HIDDEN int
adage_clear(FBIO *ifp, RGBpixel (*bgpp))
{

	/* If adage_clear() was called with non-black color, must
	 *  use DMAs to fill the frame buffer since there is no
	 *  hardware support for this.
	 */
	if( bgpp != NULL && ((*bgpp)[RED] != 0 || (*bgpp)[GRN] != 0 || (*bgpp)[BLU] != 0) )
		return	adage_color_clear( ifp, bgpp );

	IKI(ifp)->ikfbcmem.fbc_Lcontrol |= FBC_AUTOCLEAR;

	if( lseek( ifp->if_fd, (off_t)FBC*4L, 0 ) == -1 ) {
		fb_log( "adage_clear : lseek failed.\n" );
		return	-1;
	}
	if( write( ifp->if_fd, &(IKI(ifp)->ikfbcmem), sizeof(struct ik_fbc) )
	    != sizeof(struct ik_fbc) ) {
		fb_log( "adage_clear : write failed.\n" );
		return	-1;
	}

	sleep( 1 );	/* Give the FBC a chance to act */
	IKI(ifp)->ikfbcmem.fbc_Lcontrol &= ~FBC_AUTOCLEAR;
	if( lseek( ifp->if_fd, (off_t)FBC*4L, 0 ) == -1 ) {
		fb_log( "adage_clear : lseek failed.\n" );
		return	-1;
	}
	if( write( ifp->if_fd, &(IKI(ifp)->ikfbcmem), sizeof(struct ik_fbc) )
	    !=	sizeof(struct ik_fbc) ) {
		fb_log( "adage_clear : write failed.\n" );
		return	-1;
	}
	return	0;
}

/*
 *  Buffered Reads and Writes:
 *  We divide the users pixel buffer into three parts, as up to
 *  three seeks and DMA's are necessary to read or write this 1st
 *  quadrant buffer in Ikonas order.
 *
 *	[....................]			tailfrag
 *	[...............................]	fullscans
 *	[...............................]
 *	             [0.................]	headfrag
 *		      ^
 *		      + start of pixelp buffer.
 */

#define	IKSEEK(x,y)	if(lseek(ifp->if_fd,(off_t)((y)*ifp->if_width+(x))\
			*sizeof(IKONASpixel),0) == -1) return -1;

HIDDEN int
adage_read(FBIO *ifp, int x, int y, RGBpixel (*pixelp), long int count)
{
	register int i;
	register char *out, *in;
	int	headfrag, tailfrag, fullscans;
	int	scan, doscans;
	int	maxikscans;
	int	width, pixels, topiky;

	if( count == 1 )
		return adage_read_pio_pixel( ifp, x, y, pixelp );

	width = ifp->if_width;
	pixels = count;

	topiky = ifp->if_height - 1 - y;	/* 1st quadrant */
	topiky -= ( x + count - 1 ) / width;	/* first y on screen */
	if( x + count <= width ) {
		/* all on one line */
		headfrag = count;
		goto headin;
	}
	if( x != 0 ) {
		/* doesn't start of beginning of line => headfrag */
		headfrag = width - x;
		pixels -= headfrag;
	} else
		headfrag = 0;

	fullscans = pixels / width;
	tailfrag = pixels - fullscans * width;	/* remainder */

	if( tailfrag != 0 ) {
		IKSEEK( 0, topiky );
		topiky++;
		if( read( ifp->if_fd, _pixbuf, tailfrag*sizeof(IKONASpixel) )
		    != tailfrag*sizeof(IKONASpixel) ) {
		    perror("READ_ERROR");
		    return -1;
		}
		out = (char *) &(pixelp[count-tailfrag][RED]);
		in = _pixbuf;
		for( i = tailfrag; i > 0; i-- ) {
			/* VAX subscripting faster than ++ */
			*out++ = *in;
			*out++ = in[1];
			*out++ = in[2];
			in += sizeof(IKONASpixel);
		}
	}
	/* Do the full scanlines */
	if( fullscans > 0 ) {
#ifndef GM256
		maxikscans =  ADAGE_DMA_BYTES / (ifp->if_width*sizeof(IKONASpixel));
		out = (char *) &(pixelp[count-tailfrag-width][RED]);
		IKSEEK( 0, topiky );
		topiky += fullscans;
		while( fullscans > 0 ) {
			in = _pixbuf;
			doscans = fullscans > maxikscans ? maxikscans : fullscans;
			if( read( ifp->if_fd, _pixbuf, doscans*width*sizeof(IKONASpixel) )
			    != doscans*width*sizeof(IKONASpixel) ) {
			    perror("READ ERROR");
			    return -1;
			}
			for( scan = doscans; scan > 0; scan-- ) {
				for( i = width; i > 0; i-- ) {
					/* VAX subscripting faster than ++ */
					*out++ = *in;
					*out++ = in[1];
					*out++ = in[2];
					in += sizeof(IKONASpixel);
				}
				out -= (width << 1) * sizeof(RGBpixel);
			}
			fullscans -= doscans;
		}
#else
		out = (char *) &(pixelp[count-tailfrag-width][RED]);
		IKSEEK( 0, topiky );
		topiky += fullscans;
		while( fullscans > 0 ) {
			in = _pixbuf;
			/* Read a single scan line */
			if( read( ifp->if_fd, _pixbuf, width*sizeof(IKONASpixel) )
			    != width*sizeof(IKONASpixel) ) {
			    perror("READ ERROR");
			    return -1;
			}
			for( i = width; i > 0; i-- ) {
				/* VAX subscripting faster than ++ */
				*out++ = *in;
				*out++ = in[1];
				*out++ = in[2];
				in += sizeof(IKONASpixel);
			}
			out -= (width << 1) * sizeof(RGBpixel);
			fullscans--;
		}
#endif
	}
headin:
	if( headfrag != 0 ) {
		IKSEEK( x, topiky );
		out = (char *) pixelp;
		in = _pixbuf;
		if( read( ifp->if_fd, _pixbuf, headfrag*sizeof(IKONASpixel) )
		    != headfrag*sizeof(IKONASpixel) ) {
		    perror("READ ERROR");
		    return -1;
		}
		for( i = headfrag; i > 0; i-- ) {
			/* VAX subscripting faster than ++ */
			*out++ = *in;
			*out++ = in[1];
			*out++ = in[2];
			in += sizeof(IKONASpixel);
		}
	}
	return	count;
}

HIDDEN int
adage_write(FBIO *ifp, int x, int y, RGBpixel (*pixelp), long int count)
{
	register int i;
	register char *out, *in;
	int	headfrag, tailfrag, fullscans;
	int	scan, doscans;
	int	maxikscans;
	int	width, pixels, topiky;

	if( count == 1 )
		return adage_write_pio_pixel( ifp, x, y, pixelp );

	width = ifp->if_width;
	pixels = count;

	topiky = ifp->if_height - 1 - y;	/* 1st quadrant */
	topiky -= ( x + count - 1 ) / width;	/* first y on screen */
	if( x + count <= width ) {
		/* all on one line */
		headfrag = count;
		goto headout;
	}
	if( x != 0 ) {
		/* doesn't start of beginning of line => headfrag */
		headfrag = width - x;
		pixels -= headfrag;
	} else
		headfrag = 0;

	fullscans = pixels / width;
	tailfrag = pixels - fullscans * width;	/* remainder */

	if( tailfrag != 0 ) {
		IKSEEK( 0, topiky );
		topiky++;
		in = (char *) &(pixelp[count-tailfrag][RED]);
		out = _pixbuf;
		for( i = tailfrag; i > 0; i-- ) {
			/* VAX subscripting faster than ++ */
			*out = *in++;
			out[1] = *in++;
			out[2] = *in++;
			out += sizeof(IKONASpixel);
		}
		if( write( ifp->if_fd, _pixbuf, tailfrag*sizeof(IKONASpixel) )
		    != tailfrag*sizeof(IKONASpixel) )
			return	-1;
	}
	/* Do the full scanlines */
	if( fullscans > 0 ) {
#ifndef GM256
		maxikscans =  ADAGE_DMA_BYTES / (ifp->if_width*sizeof(IKONASpixel));
		in = (char *) &(pixelp[count-tailfrag-width][RED]);
		IKSEEK( 0, topiky );
		topiky += fullscans;
		while( fullscans > 0 ) {
			out = _pixbuf;
			doscans = fullscans > maxikscans ? maxikscans : fullscans;
			for( scan = doscans; scan > 0; scan-- ) {
				for( i = width; i > 0; i-- ) {
					/* VAX subscripting faster than ++ */
					*out = *in++;
					out[1] = *in++;
					out[2] = *in++;
					out += sizeof(IKONASpixel);
				}
				in -= (width << 1) * sizeof(RGBpixel);
			}
			if( write( ifp->if_fd, _pixbuf, doscans*width*sizeof(IKONASpixel) )
			    != doscans*width*sizeof(IKONASpixel) )
				return	-1;
			fullscans -= doscans;
		}
#else
		in = (char *) &(pixelp[count-tailfrag-width][RED]);
		IKSEEK( 0, topiky );
		topiky += fullscans;
		while( fullscans > 0 ) {
			out = _pixbuf;
			/* Read a single scan line */
			for( i = width; i > 0; i-- ) {
				/* VAX subscripting faster than ++ */
				*out = *in++;
				out[1] = *in++;
				out[2] = *in++;
				out += sizeof(IKONASpixel);
			}
			in -= (width << 1) * sizeof(RGBpixel);
			if( write( ifp->if_fd, _pixbuf, width*sizeof(IKONASpixel) )
			    != width*sizeof(IKONASpixel) )
				return	-1;
			fullscans--;
		}
#endif
	}
headout:
	if( headfrag != 0 ) {
		IKSEEK( x, topiky );
		in = (char *) pixelp;
		out = _pixbuf;
		for( i = headfrag; i > 0; i-- ) {
			/* VAX subscripting faster than ++ */
			*out = *in++;
			out[1] = *in++;
			out[2] = *in++;
			out += sizeof(IKONASpixel);
		}
		if( write( ifp->if_fd, _pixbuf, headfrag*sizeof(IKONASpixel) )
		    != headfrag*sizeof(IKONASpixel) )
			return	-1;
	}
	return	count;
}

/* Write 1 Ikonas pixel using PIO rather than DMA */
HIDDEN int
adage_write_pio_pixel(FBIO *ifp, int x, int y, RGBpixel (*datap))
{
	register int i;
	register struct ikdevice *ikp = (struct ikdevice *)(IKI(ifp)->_ikUBaddr);
	long data = 0;

	((unsigned char *)&data)[RED] = (*datap)[RED];
	((unsigned char *)&data)[GRN] = (*datap)[GRN];
	((unsigned char *)&data)[BLU] = (*datap)[BLU];

	y = ifp->if_width-1-y;		/* 1st quadrant */
	i = 10000;
	while( i-- && !(ikp->ubcomreg & IKREADY) )  /* NULL */ 	;
	if( i == 0 ) {
		fb_log( "IK READY stayed low.\n" );
		return	-1;
	}

	if( ifp->if_width == 1024 ) {
		ikp->ikcomreg = IKPIX | IKWRITE | IKINCR | IKHIRES;
		ikp->ikloaddr = x;
		ikp->ikhiaddr = y;
	} else {
		ikp->ikcomreg = IKPIX | IKWRITE | IKINCR;
		ikp->ikloaddr = x<<1;
		ikp->ikhiaddr = y<<1;
	}
	ikp->ubcomreg = 1;			/* GO */
	ikp->datareg = (u_short)data;
	ikp->datareg = (u_short)(data>>16);
	if( ikp->ubcomreg & IKERROR ) {
		fb_log( "IK ERROR bit on PIO.\n" );
		return	-1;
	}
	return	1;
}

/* Read 1 Ikonas pixel using PIO rather than DMA */
HIDDEN int
adage_read_pio_pixel(FBIO *ifp, int x, int y, RGBpixel (*datap))
{
	register int i;
	register struct ikdevice *ikp = (struct ikdevice *)(IKI(ifp)->_ikUBaddr);
	long data;

	y = ifp->if_width-1-y;		/* 1st quadrant */
	i = 10000;
	while( i-- && !(ikp->ubcomreg & IKREADY) )  /* NULL */ 	;
	if( i == 0 ) {
		fb_log( "IK READY stayed low (setup).\n" );
		return	-1;
	}

	if( ifp->if_width == 1024 ) {
		ikp->ikcomreg = IKPIX | IKINCR | IKHIRES;
		ikp->ikloaddr = x;
		ikp->ikhiaddr = y;
	} else {
		ikp->ikcomreg = IKPIX | IKINCR;
		ikp->ikloaddr = x<<1;
		ikp->ikhiaddr = y<<1;
	}
	ikp->ubcomreg = 1;			/* GO */

	i = 10000;
	while( i-- && !(ikp->ubcomreg & IKREADY) )  /* NULL */ 	;
	if( i == 0 ) {
		fb_log( "IK READY stayed low (after).\n" );
		return	-1;
	}
	data = ikp->datareg;			/* low */
	data |= (((long)ikp->datareg)<<16);	/* high */
	if( ikp->ubcomreg & IKERROR ) {
		fb_log( "IK ERROR bit on PIO.\n" );
		return	-1;
	}
	(*datap)[RED] = ((unsigned char *)&data)[RED];
	(*datap)[GRN] = ((unsigned char *)&data)[GRN];
	(*datap)[BLU] = ((unsigned char *)&data)[BLU];
	return	1;
}

HIDDEN int
adage_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
	adage_window_set(ifp, xcenter, ycenter);
	adage_zoom_set(ifp, xzoom, yzoom);
	fb_sim_view(ifp, xcenter, ycenter, xzoom, yzoom);
	return	0;
}

/*	a d a g e _ z o o m _ s e t ( )
	Update fbc_[xy]zoom registers in FBC
 */
HIDDEN int
adage_zoom_set(FBIO *ifp, register int x, register int y)
{

	/*
	 *  While page 5-6 claims that the zoom range is 1..256:1,
	 *  testing demonstrates that the actual range is 1..16:1.
	 */
	if( x < 1 )  x=1;
	if( y < 1 )  y=1;
	if( x > 16 )  x=16;
	if( y > 16 )  y=16;

	ifp->if_xzoom = x;
	ifp->if_yzoom = y;

	/*
	 * From RDS 3000 Programming Reference Manual, June 1982, section
	 * 5.3 Notes, page 5-12.
	 * In HIRES mode, horizontal zoom must be accomplished as follows:
	 * 1.   To go from a ratio of 1:1 to 2:1 you must double the
	 * 	pixel clock rate, rather than use the zoom register.
	 * 2.   Thereafter you can increment the zoom register, while
	 * 	leaving the pixel clock rate doubled.
	 */
	if( IKI(ifp)->mode == 2 )  {
		if( x > 1 )  {
			/* PIXELCLOCK rate experimentally determined as 41 */
			IKI(ifp)->ikfbcmem.fbc_Hcontrol =
				FBCH_PIXELCLOCK(41) | FBCH_DRIVEBPCK;
			if( x >= 4 )
				IKI(ifp)->ikfbcmem.fbc_xzoom = (x>>1)-1;
			else
				IKI(ifp)->ikfbcmem.fbc_xzoom = 0;
			if( x & 1 )  fb_log("Unable to do odd X zooms properly in HIRES\n");
			IKI(ifp)->ikfbcmem.fbc_xsizeview = 511;
		} else {
			IKI(ifp)->ikfbcmem.fbc_Hcontrol =
				ikfbc_setup[2].fbc_Hcontrol;
			IKI(ifp)->ikfbcmem.fbc_xzoom = 0;
			IKI(ifp)->ikfbcmem.fbc_xsizeview =
				ikfbc_setup[2].fbc_xsizeview;
		}
		/* set pixel clock */
		if( lseek( ifp->if_fd, (off_t)FBCVC*4L, 0 ) == -1 ||
		    write( ifp->if_fd, &(IKI(ifp)->ikfbcmem.fbc_Lcontrol), 4 ) != 4 ) {
			fb_log( "adage_zoom_set : FBCVC write failed.\n" );
			return	-1;
		}
		/* set x viewport size */
		if( lseek( ifp->if_fd, (off_t)FBCVPS*4L, 0 ) == -1 ||
		    write( ifp->if_fd, &(IKI(ifp)->ikfbcmem.fbc_xsizeview), 4 ) != 4 ) {
			fb_log( "adage_zoom_set : FBCVPS write failed.\n" );
			return	-1;
		}

	} else {
		IKI(ifp)->ikfbcmem.fbc_xzoom = x-1;
	}
	IKI(ifp)->ikfbcmem.fbc_yzoom = y-1;	/* replication count */

	if( lseek( ifp->if_fd, (off_t)FBCZOOM*4L, 0 ) == -1 ) {
		fb_log( "adage_zoom_set : lseek failed.\n" );
		return	-1;
	}
	if( write( ifp->if_fd, &(IKI(ifp)->ikfbcmem.fbc_xzoom), 4 ) != 4 ) {
		fb_log( "adage_zoom_set : FBCZOOM write failed.\n" );
		return	-1;
	}
	return	0;
}

HIDDEN int
imax(int a, int b)
{
	if( a > b )
		return(a);
	return(b);
}

/*			a d a g e _ w i n d o w _ s e t ( )
 *
 *	Set FBC window location to specified values so that <x,y> are
 *	at screen center given current zoom.
 */
HIDDEN int
adage_window_set(register FBIO *ifp, int x, int y)
{
	int ikx, iky;		/* upper left corner of view rectangle */
	int y_viewport;
	int y_window;
	int first_line;
	int top_margin;

	ifp->if_xcenter = x;
	ifp->if_ycenter = y;

	/*
	 *  To start with, we are given the 1st quadrant coordinates
	 *  of the CENTER of the region we wish to view.  Since the
	 *  Ikonas window is specified in terms of the upper left
	 *  corner, first find the upper left corner of the rectangle
	 *  to window in on, accounting for the zoom factor too.
	 *  Then convert from first to fourth for the Ikonas.
	 *  The order of these conversions is significant.
	 */
	ikx = x - (ifp->if_width / ifp->if_xzoom)/2;
	iky = y + (ifp->if_height / ifp->if_yzoom)/2 - 1;
	iky = ifp->if_height-1-iky;		/* q1 -> q4 */

	/* for the cursor routines, save q4 upper left */
	IKI(ifp)->x_window = ikx;
	IKI(ifp)->y_window = iky;

	/*
	 *  These formulas are taken from section 5.2.3.2.4 (page 5-5)
	 *  of the Adage RDS-3000 programming reference manual, June 1982.
	 *  Note that the published magic numbers are off by one.
	 */
	first_line = 0;
	y_viewport = IKI(ifp)->ikfbcmem.fbc_yviewport;

	switch( IKI(ifp)->mode )  {
	case 0:
	case 3:
		top_margin = imax( 35, y_viewport+4 );
		y_window = first_line - top_margin + 7;
		break;
	case 1:
		top_margin = imax( 34, (y_viewport+4)/2 );
		y_window = first_line - top_margin + 3;		/* was 4 */
		break;
	case 2:
		top_margin = imax( 69, y_viewport+4 );
		y_window = first_line - top_margin + 6;		/* was 9 */
		break;
	}
	IKI(ifp)->y_winoff = y_window;	/* save for cursor routines */
	y_window /= ifp->if_yzoom;

	/* HACK - XXX */
	if( ifp->if_xzoom > 1 )
		ikx--;
	if( IKI(ifp)->mode == 2 && ifp->if_yzoom > 1 )
		iky--;	/* hires zoom */

	if( IKI(ifp)->mode != 2 )
		IKI(ifp)->ikfbcmem.fbc_xwindow = ikx << 2;	/* lores */
	else
		IKI(ifp)->ikfbcmem.fbc_xwindow = ikx;		/* hires */

	IKI(ifp)->ikfbcmem.fbc_ywindow = iky + y_window;

	if( lseek( ifp->if_fd, (off_t)FBCWL*4L, 0 ) == -1 ) {
		fb_log( "adage_window_set : lseek failed.\n" );
		return	-1;
	}

	if( write( ifp->if_fd, &(IKI(ifp)->ikfbcmem.fbc_xwindow), 4 ) != 4 ) {
		fb_log( "adage_window_set : write failed.\n" );
		return	-1;
	}
	return	0;
}

/*	a d a g e _ c u r s o r _ m o v e _ m e m o r y _ a d d r ( )
 *
 *	Place cursor at image (pixel) coordinates x and y.
 *	IMPORTANT : Adage cursor addressing is in screen space,
 *	so backwards correction must be applied.
 */
HIDDEN int
adage_cursor(FBIO *ifp, int mode, int x, int y)
{
	fb_sim_cursor( ifp, mode, x, y );

	y = ifp->if_height-1-y;		/* q1 -> q4 */
	y = y - IKI(ifp)->y_window;
	x = x - IKI(ifp)->x_window;
/*
	if( y < 0 )  y = 0;
	if( x < 0 )  x = 0;
*/
	y *= ifp->if_yzoom;
	/* HACK - XXX */
	if( ifp->if_xzoom > 1 )
		x++;
	if( IKI(ifp)->mode == 2 && ifp->if_xzoom > 1 )
		x *= (ifp->if_xzoom / 2);
	else
		x *= ifp->if_xzoom;
	y -= IKI(ifp)->y_winoff;
	x -= IKI(ifp)->x_corig;
	y -= IKI(ifp)->y_corig;

	if( mode )
		IKI(ifp)->ikfbcmem.fbc_Lcontrol |= FBC_CURSOR;
	else
		IKI(ifp)->ikfbcmem.fbc_Lcontrol &= ~FBC_CURSOR;
	IKI(ifp)->ikfbcmem.fbc_xcursor = x&01777;
	IKI(ifp)->ikfbcmem.fbc_ycursor = y&01777;

	if( lseek( ifp->if_fd, (off_t)FBCVC*4L, 0 ) == -1 ) {
		fb_log( "adage_cursor : lseek failed.\n" );
		return	-1;
	}
	if( write( ifp->if_fd, &(IKI(ifp)->ikfbcmem.fbc_Lcontrol), 8 ) != 8 ) {
		fb_log( "adage_cursor : write failed.\n" );
		return	-1;
	}
	return	0;
}

/*	a d a g e _ c u r s o r _ m o v e _ s c r e e n _ a d d r ( )
	Place cursor at SCREEN coordinates x and y.
 */
HIDDEN int
adage_cscreen_addr(FBIO *ifp, int mode, int x, int y)
{
	y = ifp->if_width-1-y;		/* 1st quadrant */
	if( ifp->if_width == 1024 && ifp->if_yzoom == 1 )
		y += 30;
	if (mode)
		IKI(ifp)->ikfbcmem.fbc_Lcontrol |= FBC_CURSOR;
	else
		IKI(ifp)->ikfbcmem.fbc_Lcontrol &= ~FBC_CURSOR;
	IKI(ifp)->ikfbcmem.fbc_xcursor = x&01777;
	IKI(ifp)->ikfbcmem.fbc_ycursor = y&01777;

	if( lseek( ifp->if_fd, (off_t)FBCVC*4L, 0 ) == -1 ) {
		fb_log( "adage_cscreen_addr : lseek failed.\n" );
		return	-1;
	}
	if( write( ifp->if_fd, &(IKI(ifp)->ikfbcmem.fbc_Lcontrol), 8 ) != 8 ) {
		fb_log( "adage_cscreen_addr : write failed.\n" );
		return	-1;
	}
	return	0;
}

HIDDEN int
adage_setcursor(FBIO *ifp, unsigned char *bits, int xbits, int ybits, int xorig, int yorig)
{
	int	x, y, xbytes;
	unsigned long cursor[256];
	unsigned char *ip, imask;
	unsigned long *op, omask;

	/* Determine bytes per cursor "scanline" */
	xbytes = xbits / 8;
	if( xbytes * 8 != xbits ) xbytes++;

	/* check size of cursor */
	if( ybits > 32 ) ybits = 32;
	if( ybits < 0 ) return -1;
	if( xbits > 32 ) xbits = 32;
	if( xbits < 0 ) return -1;

	/* Clear it out first */
	for( x = 0; x < 256; x++ )
		cursor[x] = 0;

	for( y = 0; y < ybits; y++ ) {
		ip = &bits[ y * xbytes ];
		op = &cursor[ (31-y) * 8 ];
		imask = 0x80;
		omask = 1;
		for( x = 0; x < xbits; x++ ) {
			if( *ip & imask )
				*op |= omask;
			/*
			 * update bit masks and pointers
			 */
			imask >>= 1;
			if( imask == 0 ) {
				imask = 0x80;
				ip++;
			}
			omask <<= 1;
			if( omask == 0x10 ) {
				omask = 1;
				op++;
			}
		}
	}

	if( lseek( ifp->if_fd, (off_t)FBCCD*4L, 0 ) == -1 ) {
		fb_log( "adage_setcursor : lseek failed.\n" );
		return	-1;
	}
	if( write( ifp->if_fd, cursor, 1024 ) != 1024 ) {
		fb_log( "adage_setcursor : write failed.\n" );
		return	-1;
	}

	/* Set the cursor origin offsets */
	IKI(ifp)->x_corig = xorig;
	IKI(ifp)->y_corig = 32 - yorig;

	return	0;
}

#ifdef never
/*	Write one color map entry.
	Page selects the color map; 0, 1, 2, or  3.
	Offset indexes into the map.
 */
HIDDEN int
adage_wmap_entry( ifp, cp, page, offset )
FBIO	*ifp;
register RGBpixel	*cp;
long	page, offset;
{
	long	lp;

	lp = RGB10( (*cp)[RED]>>6, (*cp)[GRN]>>6, (*cp)[BLU]>>6 );
	lseek( ifp->if_fd, (off_t)(LUVO + page*256 + offset)*4L, 0);
	if( write( ifp->if_fd, (char *) &lp, 4 ) != 4 ) {
		fb_log( "adage_wmap_entry : write failed.\n" );
		return	-1;
	}
	return	0;
}
#endif

HIDDEN int
adage_wmap(FBIO *ifp, register ColorMap *cp)
{
	long cmap[1024] = {0};
	register int i, j;

	/* Note that RGB10(r,g,b) flips to cmap order (b,g,r). */
	if( cp == (ColorMap *) NULL )  {
		for( i=0; i < 256; i++ )  {
			j = i<<2;
			cmap[i] = RGB10( j, j, j );
			j = ((i+128)%255)<<2;
			cmap[i+256] = RGB10( j, j, j );
		}
	}  else  {
		for( i=0; i < 256; i++ )  {
			cmap[i] = RGB10( cp->cm_red[i]>>6,
				       cp->cm_green[i]>>6,
				       cp->cm_blue[i]>>6 );
			cmap[i+256] = RGB10( ((cp->cm_red[i]>>6)+512)%1023,
				       ((cp->cm_green[i]>>6)+512)%1023,
				       ((cp->cm_blue[i]>>6)+512)%1023 );
		}
	}

	/*
	 * Replicate first copy of color map onto second copy,
	 * and also do the "overlay" portion too.
	 */
	for( i=0; i < 256*2; i++ ) {
		cmap[i+512] = cmap[i];
	}
	if( lseek( ifp->if_fd, (off_t)LUVO*4L, 0) == -1 ) {
		fb_log( "adage_wmap : lseek failed.\n" );
		return	-1;
	}
	if( write( ifp->if_fd, cmap, 1024*4 ) != 1024*4 ) {
		fb_log( "adage_wmap : write failed.\n" );
		return	-1;
	}
	return	0;
}

HIDDEN int
adage_rmap(FBIO *ifp, register ColorMap *cp)
{
	register int i;
	long cmap[1024] = {0};

	if( lseek( ifp->if_fd, (off_t)LUVO*4L, 0) == -1 ) {
		fb_log( "adage_rmap : lseek failed.\n" );
		return	-1;
	}
	if( read( ifp->if_fd, cmap, 1024*4 ) != 1024*4 ) {
	    perror("READ ERROR");
	    fb_log( "adage_rmap : read failed.\n" );
	    return -1;
	}
	for( i=0; i < 256; i++ ) {
		cp->cm_red[i] = (cmap[i]<<(6+0))  & 0xFFC0;
		cp->cm_green[i] = (cmap[i]>>(10-6))  & 0xFFC0;
		cp->cm_blue[i] = (cmap[i]>>(20-6)) & 0xFFC0;
	}
	return	0;
}

/*
 *		A D A G E _ C O L O R _ C L E A R
 *
 *  Clear the frame buffer to the given color.  There is no hardware
 *  Support for this so we do it via large DMA's.
 */
HIDDEN int
adage_color_clear(register FBIO *ifp, register RGBpixel (*bpp))
{
	register IKONASpixel *pix_to;
	register long	i;
	long	pixelstodo;
	int	fd;

	/* Fill buffer with background color. */
	for( i = ADAGE_DMA_PIXELS, pix_to = (IKONASpixel *)_pixbuf; i > 0; i-- ) {
		COPYRGB( *pix_to, *bpp );
		pix_to++;
	}

	/* Set start of framebuffer */
	fd = ifp->if_fd;
	if( lseek( fd, (off_t)0L, 0 ) == -1 ) {
		fb_log( "adage_color_clear : seek failed.\n" );
		return	-1;
	}

	/* Send until frame buffer is full. */
	pixelstodo = ifp->if_height * ifp->if_width;
	while( pixelstodo > 0 ) {
		i = pixelstodo > ADAGE_DMA_PIXELS ? ADAGE_DMA_PIXELS : pixelstodo;
		if( write( fd, _pixbuf, i*sizeof(IKONASpixel) ) == -1 )
			return	-1;
		pixelstodo -= i;
	}

	return	0;
}

HIDDEN int
adage_help(FBIO *ifp)
{
	fb_log( "Description: %s\n", adage_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width/height: %d %d\n",
		adage_interface.if_max_width,
		adage_interface.if_max_height );
	fb_log( "Default width/height: %d %d\n",
		adage_interface.if_width,
		adage_interface.if_height );
	fb_log( "Usage: /dev/ik[#][flag]\n" );
	fb_log( "  Where # is a unit number\n" );
	fb_log( "  flag is optionally ONE of\n" );
	fb_log( "     n      no init (don't change device state)\n" );
	fb_log( "     v      NTSC Video (interlaced, external sync)\n" );

	return(0);
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
