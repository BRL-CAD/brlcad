/*
 *			I F _ A D A G E . C
 *
 *  Authors -
 *	Mike J. Muuss
 *	Gary S. Moss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 * Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

/*
 * This module is used when pre-setting the Ikonas FBC
 * (Frame Buffer Controller) to a known state.
 * The values for this table are derived from the
 * Ikonas-supplied program FBI, for compatability.
 * At present, three modes can be set:
 *	0 - LORES, 30 hz, non-interlaced
 *	1 - LORES, 60 hz, interlaced
 *	2 - HIRES, 30 hz, non-interlaced
 *
 * All that is provided is a prototype for the FBC registers;
 * the user is responsible for changing them (zooming, etc),
 * and writing them to the FBC.
 */
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include "fb.h"
#include "./fblocal.h"
#include "./adage.h"

#if defined( BSD )
#include	<sys/ioctl.h>
#endif

#if defined( SYSV )
#if defined( VLDSYSV )
#include	<sys/_ioctl.h> /* GSM : _ needed for Sys V emulation.		*/
#else
#include	<sys/ioctl.h>
#endif
#endif

_LOCAL_ int	adage_device_open(),
		adage_device_close(),
		adage_device_clear(),
		adage_buffer_read(),
		adage_buffer_write(),
		adage_colormap_read(),
		adage_colormap_write(),
		adage_window_set(),
		adage_zoom_set(),
		adage_cinit_bitmap(),
		adage_cmemory_addr(),
		adage_cscreen_addr();

FBIO adage_interface =
		{
		adage_device_open,
		adage_device_close,
		fb_null,
		adage_device_clear,
		adage_buffer_read,
		adage_buffer_write,
		adage_colormap_read,
		adage_colormap_write,
		fb_null,
		adage_window_set,
		adage_zoom_set,
		adage_cinit_bitmap,
		adage_cmemory_addr,
		adage_cscreen_addr,
		"Adage RDS3000",
		1024,
		1024,
		"/dev/ik",
		512,
		512,
		-1,
		PIXEL_NULL,
		PIXEL_NULL,
		PIXEL_NULL,
		-1,
		0,
		0L,
		0L,
		0L,
		0
		};

static struct ik_fbc ikfbc_setup[3] = {
    {
	0,	32,
	511,	511,
	0,	4067,
	0,	0,
	300,	560,
	0,	FBCH_PIXELCLOCK(45) | FBCH_DRIVEBPCK,
	0,	32
    }, {
	0,	68,
	511,	1023,
	0,	4063,
	0,	0,
	144,	1143,		/* was 144, 1167 */
	FBC_RS343 | FBC_NOINTERLACE, FBCH_PIXELCLOCK(18) | FBCH_DRIVEBPCK,
	0,	32
    }, {
	0,	64,
	1023,	1023,
	0,	4033,
	0,	0,
	144,	1144,		/* was 144, 1166 */
	FBC_HIRES | FBC_RS343, FBCH_PIXELCLOCK(19) | FBCH_DRIVEBPCK,
	0,	64
    }
};

struct	ik_fbc	ikfbcmem;	/* Global memory of current FBC state	*/

/* Current values initialized in adage_init().				*/
static int	x_origin, y_origin;
static int	x_zoom, y_zoom;
static int	x_window, y_window;

static long cursor[32] =
	{
#include "./adagecursor.h"
	};

/* Global Flags */
short	*_ikUBaddr;		/* Mapped-in Ikonas address */

static char *_pixbuf;		/* RGBpixel -> Ikonas pixel buffer */
typedef unsigned char IKONASpixel[4];

#define	ADAGE_DMA_BYTES	(63*1024)
#define	ADAGE_DMA_PIXELS (ADAGE_DMA_BYTES/sizeof(IKONASpixel))

_LOCAL_ int
adage_device_open( ifp, file, width, height )
FBIO	*ifp;
char		*file;
int		width, height;
	{	register int	i;
		long	xbsval[34];
		int	dev_mode;
		char	ourfile[16];

	/* Only 512 and 1024 opens are available */
	if( width > 512 || height > 512 )
		width = height = 1024;
	else
		width = height = 512;

	/* /dev/ik0l */
	/* 012345678 */
	if( strlen( file ) > 12 )
		return -1;
	sprintf( ourfile, "%s0l", file );
	if( width > 512 || height > 512 )
		ourfile[8] = 'h';
	else
		ourfile[8] = 'l';
	ourfile[9] = '\0';

	if( (ifp->if_fd = open( ourfile, O_RDWR, 0 )) == -1 )
		return	-1;
#if defined( vax )
	if( ioctl( ifp->if_fd, IKIOGETADDR, &_ikUBaddr ) < 0 )
		fb_log( "adage_device_open : ioctl(IKIOGETADDR) failed.\n" );
#endif
	ifp->if_width = width;
	ifp->if_height = height;
	switch( ifp->if_width )
		{
	case 512:
		dev_mode = 1;
		break;
	case 1024:
		dev_mode = 2;
		break;
	default:
		fb_log( "Bad fbsize %d.\n", ifp->if_width );
		return	-1;
		}
	if( lseek( ifp->if_fd, FBC*4L, 0 ) == -1 )
		{
		fb_log( "adage_device_open : lseek failed.\n" );
		return	-1;
		}
	if(	write(	ifp->if_fd,
			(char *)&ikfbc_setup[dev_mode],
			sizeof(struct ik_fbc)
			)
		  !=	sizeof(struct ik_fbc)
		)
		{
		fb_log( "adage_device_open : write failed.\n" );
		return	-1;
		}
	ikfbcmem = ikfbc_setup[dev_mode];

	/* Build an identity for the crossbar switch */
	for( i=0; i < 34; i++ )
#ifndef pdp11
		xbsval[i] = (long)i;
#else
		xbsval[i] = (((long)i)<<16);	/* word swap.. */
#endif
	if( lseek( ifp->if_fd, XBS*4L, 0 ) == -1 )
		{
		fb_log( "adage_device_open : lseek failed.\n" );
		return	-1;
		}
	if( write( ifp->if_fd, (char *) xbsval, sizeof(xbsval) )
		!= sizeof(xbsval)
		)
		{
		fb_log( "adage_device_open : write failed.\n" );
		return	-1;
		}

	/* Initialize the LUVO crossbar switch, too */
#ifndef pdp11
	xbsval[0] = 0x24L;		/* 1:1 mapping, magic number */
#else
	xbsval[0] = (0x24L<<16);
#endif
	if( lseek( ifp->if_fd, LUVOXBS*4L, 0 ) == -1 )
		{
		fb_log( "adage_device_open : lseek failed.\n" );
		return	-1;
		}
	if( write( ifp->if_fd, (char *) xbsval, sizeof(long) )
		!= sizeof(long)
		)
		{
		fb_log( "adage_device_open : write failed.\n" );
		return	-1;
		}

	/* Dump in default cursor.					*/
	if( adage_cinit_bitmap( ifp, cursor ) == -1 )
		return	-1;
	if( lseek( ifp->if_fd, 0L, 0 ) == -1 )	/* To start of pixels */
		{
		fb_log( "adage_device_open : lseek failed.\n" );
		return	-1;
		}
	/* Create pixel buffer */
	if( (_pixbuf = malloc( ADAGE_DMA_BYTES )) == NULL ) {
		fb_log( "adage_device_open : pixbuf malloc failed.\n" );
		return	-1;
	}
	x_zoom = 1;
	y_zoom = 1;
	x_origin = ifp->if_width / 2;
	y_origin = ifp->if_height / 2;
	x_window = y_window = 0;
	return	ifp->if_fd;
	}

_LOCAL_ int
adage_device_close( ifp )
FBIO	*ifp;
	{
	return	close( ifp->if_fd );
	}

_LOCAL_ int
adage_device_clear( ifp, bgpp )
FBIO	*ifp;
RGBpixel	*bgpp;
	{	int dev_mode = 1;
	/* If adage_device_clear() was called with non-black color, must
		use DMAs to fill the frame buffer since there is no
		hardware support for this.
	 */
	if( bgpp != NULL && ((*bgpp)[RED] != 0 || (*bgpp)[GRN] != 0 || (*bgpp)[BLU] != 0) )
		return	adage_color_clear( ifp, bgpp );

	if( ifp->if_width == 1024 )
		dev_mode = 2;
	ikfbc_setup[dev_mode].fbc_Lcontrol |= FBC_AUTOCLEAR;

	if( lseek( ifp->if_fd, FBC*4L, 0 ) == -1 )
		{
		fb_log( "adage_device_clear : lseek failed.\n" );
		return	-1;
		}
	if(	write( ifp->if_fd, &ikfbc_setup[dev_mode], sizeof(struct ik_fbc) )
	     != sizeof(struct ik_fbc)
		)
		{
		fb_log( "adage_device_clear : write failed.\n" );
		return	-1;
		}

	sleep( 1 );	/* Give the FBC a chance to act */
	ikfbc_setup[dev_mode].fbc_Lcontrol &= ~FBC_AUTOCLEAR;
	if( lseek( ifp->if_fd, FBC*4L, 0 ) == -1 )
		{
		fb_log( "adage_device_clear : lseek failed.\n" );
		return	-1;
		}
	if(	write( ifp->if_fd, &ikfbc_setup[dev_mode], sizeof(struct ik_fbc) )
	    !=	sizeof(struct ik_fbc)
		)
		{
		fb_log( "adage_device_clear : write failed.\n" );
		return	-1;
		}
	return	0;
	}

_LOCAL_ int
adage_buffer_read( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x,  y;
RGBpixel	*pixelp;
int	count;
{
	register char *out, *in;
	register int i;
	long ikbytes = count * (long) sizeof(IKONASpixel);
	long todo;
	int toread;
	int width;
	int scanlines;
	int maxikdma;

	if( count == 1 )
		return adage_read_pio_pixel( ifp, x, y, pixelp );

	scanlines = (count+ifp->if_width-1) / ifp->if_width;	/* ceil */
	width = count - (scanlines-1) * ifp->if_width;	/* residue on last line */

	y = ifp->if_width-y-scanlines;		/* 1st quadrant */
	if( lseek(	ifp->if_fd,
			(((long) y * (long) ifp->if_width) + (long) x)
			* (long) sizeof(IKONASpixel),
			0
			)
	    == -1L ) {
		fb_log( "adage_buffer_read : seek to %ld failed.\n",
			(((long) y * (long) ifp->if_width) + (long) x)
			* (long) sizeof(IKONASpixel) );
		return	-1;
	}
	out = (char *)&(pixelp[(scanlines-1) * ifp->if_width][RED]);
	maxikdma =  ADAGE_DMA_BYTES /
		(ifp->if_width*sizeof(IKONASpixel)) *
		(ifp->if_width*sizeof(IKONASpixel)); /* even # of scanlines */
	while( ikbytes > 0 ) {
		if( ikbytes > maxikdma )
			todo = maxikdma;
		else
			todo = ikbytes;
		toread = todo;
		if( read( ifp->if_fd, _pixbuf, toread ) != toread )
			return	-1;

		in = _pixbuf;
		do {
			for( i = width; i > 0; i-- ) {
				*out++ = *in;
				*out++ = in[1];
				*out++ = in[2];
				in += sizeof(IKONASpixel);
			}
			todo -= width * sizeof(IKONASpixel);
			out -= (width + ifp->if_width) * sizeof(RGBpixel);
			width = ifp->if_width;
		} while( todo > 0 );
		ikbytes -= toread;
	}
	return	count;
}

_LOCAL_ int
adage_buffer_write( ifp, x, y, pixelp, count )
FBIO	*ifp;
int	x, y;
RGBpixel	*pixelp;
long	count;
{
	register char *out, *in;
	register int i;
	register long	ikbytes = count * (long) sizeof(IKONASpixel);
	register int	todo;
	int towrite;
	int width;
	int scanlines;
	int maxikdma;

	if( count == 1 )
		return adage_write_pio_pixel( ifp, x, y, pixelp );

	scanlines = (count+ifp->if_width-1) / ifp->if_width;	/* ceil */
	width = count - (scanlines-1) * ifp->if_width;	/* residue on last line */

	y = ifp->if_height-y-scanlines;	/* 1st quadrant */
	if( lseek(	ifp->if_fd,
			((long) y * (long) ifp->if_width + (long) x)
			* (long) sizeof(IKONASpixel),
			0
			)
	    == -1L ) {
		fb_log( "adage_buffer_write : seek to %ld failed.\n",
			(((long) y * (long) ifp->if_width) + (long) x)
			* (long) sizeof(IKONASpixel) );
		return	-1;
	}
	in = (char *)&(pixelp[(scanlines-1) * ifp->if_width][RED]);
	maxikdma =  ADAGE_DMA_BYTES /
		(ifp->if_width*sizeof(IKONASpixel)) *
		(ifp->if_width*sizeof(IKONASpixel)); /* even # of scanlines */
	while( ikbytes > 0 ) {
		if( ikbytes > maxikdma )
			todo = maxikdma;
		else
			todo = ikbytes;
		towrite = todo;
		out = _pixbuf;
		do {
			for( i = width; i > 0; i-- ) {
				/* VAX subscripting faster than ++ */
				*out = *in++;
				out[1] = *in++;
				out[2] = *in++;
				out += sizeof(IKONASpixel);
			}
			todo -= width * sizeof(IKONASpixel);
			in -= (width + ifp->if_width) * sizeof(RGBpixel);
			width = ifp->if_width;
		} while( todo > 0 );
		if( write( ifp->if_fd, _pixbuf, towrite ) != towrite )
			return	-1;
		ikbytes -= towrite;
	}
	return	count;
}

/* Write 1 Ikonas pixel using PIO rather than DMA */
_LOCAL_ int
adage_write_pio_pixel( ifp, x, y, datap )
FBIO	*ifp;
int	x, y;
RGBpixel	*datap;
	{
	register int i;
	register struct ikdevice *ikp = (struct ikdevice *)_ikUBaddr;
	long data = 0;

	((unsigned char *)&data)[RED] = (*datap)[RED];
	((unsigned char *)&data)[GRN] = (*datap)[GRN];
	((unsigned char *)&data)[BLU] = (*datap)[BLU];

	y = ifp->if_width-1-y;		/* 1st quadrant */
	i = 10000;
	while( i-- && !(ikp->ubcomreg & IKREADY) )  /* NULL */ 	;
	if( i == 0 )
		{
		fb_log( "IK READY stayed low.\n" );
		return	-1;
		}

	if( ifp->if_width == 1024 )
		{
		ikp->ikcomreg = IKPIX | IKWRITE | IKINCR | IKHIRES;
		ikp->ikloaddr = x;
		ikp->ikhiaddr = y;
		}
	else
		{
		ikp->ikcomreg = IKPIX | IKWRITE | IKINCR;
		ikp->ikloaddr = x<<1;
		ikp->ikhiaddr = y<<1;
		}
	ikp->ubcomreg = 1;			/* GO */
	ikp->datareg = (u_short)data;
	ikp->datareg = (u_short)(data>>16);
	if( ikp->ubcomreg & IKERROR )
		{
		fb_log( "IK ERROR bit on PIO.\n" );
		return	-1;
		}
	return	1;
	}

/* Read 1 Ikonas pixel using PIO rather than DMA */
_LOCAL_ long
adage_read_pio_pixel( ifp, x, y, datap )
FBIO	*ifp;
int	x, y;
RGBpixel	*datap;
	{
	register int i;
	register struct ikdevice *ikp = (struct ikdevice *)_ikUBaddr;
	long data;

	y = ifp->if_width-1-y;		/* 1st quadrant */
	i = 10000;
	while( i-- && !(ikp->ubcomreg & IKREADY) )  /* NULL */ 	;
	if( i == 0 )
		{
		fb_log( "IK READY stayed low (setup).\n" );
		return	-1;
		}

	if( ifp->if_width == 1024 )
		{
		ikp->ikcomreg = IKPIX | IKINCR | IKHIRES;
		ikp->ikloaddr = x;
		ikp->ikhiaddr = y;
		}
	else
		{
		ikp->ikcomreg = IKPIX | IKINCR;
		ikp->ikloaddr = x<<1;
		ikp->ikhiaddr = y<<1;
		}
	ikp->ubcomreg = 1;			/* GO */

	i = 10000;
	while( i-- && !(ikp->ubcomreg & IKREADY) )  /* NULL */ 	;
	if( i == 0 )
		{
		fb_log( "IK READY stayed low (after).\n" );
		return	-1;
		}
	data = ikp->datareg;			/* low */
	data |= (((long)ikp->datareg)<<16);	/* high */
	if( ikp->ubcomreg & IKERROR )
		{
		fb_log( "IK ERROR bit on PIO.\n" );
		return	-1;
		}
	(*datap)[RED] = ((unsigned char *)&data)[RED];
	(*datap)[GRN] = ((unsigned char *)&data)[GRN];
	(*datap)[BLU] = ((unsigned char *)&data)[BLU];
	return	1;
	}

/*	a d a g e _ z o o m _ s e t ( )
	Update fbc_[xy]zoom registers in FBC
 */
_LOCAL_ int
adage_zoom_set( ifp, x, y )
FBIO	*ifp;
register int	x, y;
	{
#ifdef never
	/* Ikonas can only zoom to 1, 2, 4, 8 or 16 times actual size.	*/
	/* AHEM!!  Ikonas can pixel replicate from 0 to 15 times */
	if( x < 2 )
		x = 1;
	else
	if( x < 4 )
		x = 2;
	else
	if( x < 8 )
		x = 4;
	else
	if( x < 16 )
		x = 8;
	else
		x = 16;
	if( y < 2 )
		y = 1;
	else
	if( y < 4 )
		y = 2;
	else
	if( y < 8 )
		y = 4;
	else
	if( y < 16 )
		y = 8;
	else
		y = 16;
#else
	if( x < 1 )  x=1;
	if( y < 1 )  y=1;
	if( x > 16 )  x=16;
	if( y > 16 )  y=16;
#endif
	x_zoom = x;
	y_zoom = y;

	/* Ikonas zoom factor is actually (factor - 1)!			*/
	x--;
	y--;
	ikfbcmem.fbc_xzoom = x;
	ikfbcmem.fbc_yzoom = y;
	if( lseek( ifp->if_fd, FBCZOOM*4L, 0 ) == -1 )
		{
		fb_log( "adage_zoom_set : lseek failed.\n" );
		return	-1;
		}
	if( write( ifp->if_fd, &ikfbcmem.fbc_xzoom, 4 ) != 4 )
		{
		fb_log( "adage_zoom_set : write failed.\n" );
		return	-1;
		}
	return	0;
	}

/*	a d a g e _ w i n d o w _ s e t ( )
	Set FBC window location to specified values so that <x,y> are
	at screen center.
 */
_LOCAL_ int
adage_window_set( ifp, x, y )
FBIO	*ifp;
register int	x, y;
	{
	y = ifp->if_width-1-y;		/* 1st quadrant */
	/* Window relative to image center.				*/
	x_window = x -= x_origin;
	y_window = y -= y_origin;

	if( ifp->if_width != 1024 )
		x *= 4;
	switch( x_zoom )
		{
	case 16 :
		if( ifp->if_width == 1024 )
			ikfbcmem.fbc_xwindow = 478 + x;	/* was 480 */
		else
			ikfbcmem.fbc_xwindow = 960 + x;
		break;
	case 8 :
		if( ifp->if_width == 1024 )
			ikfbcmem.fbc_xwindow = 446 + x;	/* was 447 */
		else
			ikfbcmem.fbc_xwindow = 896 + x;
		break;
	case 4 :
		if( ifp->if_width == 1024 )
			ikfbcmem.fbc_xwindow = 382 + x;	/* was 383 */
		else
			ikfbcmem.fbc_xwindow = 768 + x;
		break;
	case 2 :
		if( ifp->if_width == 1024 )
			ikfbcmem.fbc_xwindow = 256 + x;	/* was 254 */
		else
			ikfbcmem.fbc_xwindow = 511 + x;
		break;
	default :
		ikfbcmem.fbc_xwindow = x;
		break;
		}
	switch( y_zoom )
		{
	case 16 :
		if( ifp->if_width == 1024 )
			ikfbcmem.fbc_ywindow = 4573 + y; /* was 4574 */
		else
			ikfbcmem.fbc_ywindow = 4334 + y;
		break;
	case 8 :
		if( ifp->if_width == 1024 )
			ikfbcmem.fbc_ywindow = 4537 + y; /* was 4540 */
		else
			ikfbcmem.fbc_ywindow = 4316 + y;
		break;
	case 4 :
		if( ifp->if_width == 1024 )
			ikfbcmem.fbc_ywindow = 4465 + y; /* was 4472 */
		else
			ikfbcmem.fbc_ywindow = 4280 + y;
		break;
	case 2 :
		if( ifp->if_width == 1024 )
			ikfbcmem.fbc_ywindow = 4321 + y; /* was 4331 */
		else
			ikfbcmem.fbc_ywindow = 4208 + y;
		break;
	default :
		if( ifp->if_width == 1024 )
			ikfbcmem.fbc_ywindow = 4033 + y;
		else
			ikfbcmem.fbc_ywindow = 4064 + y;
		break;
		}
	if( lseek( ifp->if_fd, FBCWL*4L, 0 ) == -1 )
		{
		fb_log( "adage_window_set : lseek failed.\n" );
		return	-1;
		}

	if( write( ifp->if_fd, &ikfbcmem.fbc_xwindow, 4 ) != 4 )
		{
		fb_log( "adage_window_set : write failed.\n" );
		return	-1;
		}
	return	0;
	}

/*	a d a g e _ c u r s o r _ m o v e _ m e m o r y _ a d d r ( )
	Place cursor at image coordinates x and y.
	IMPORTANT : Adage cursor addressing is in screen space.
 */
_LOCAL_ int
adage_cmemory_addr( ifp, mode, x, y )
FBIO	*ifp;
int		mode;
int		x, y;
	{	register int	x_cursor_offset, y_cursor_offset;
	y = ifp->if_width-1-y;		/* 1st quadrant */
	/* Map image coordinates to screen space.			*/
	if( ifp->if_width == 1024 )
		{
		switch( x_zoom )
			{
		case 16 :
			x_cursor_offset = 9;
			y_cursor_offset = 39;
			break;
		case 8 :
			x_cursor_offset = -4;
			y_cursor_offset = 43;
			break;
		case 4 :
			x_cursor_offset = -10;
			y_cursor_offset = 44;
			break;
		case 2 :
			x_cursor_offset = -17;
			y_cursor_offset = 45;
			break;
		case 1 :
			x_cursor_offset = -16;
			y_cursor_offset = 46;
			break;
			}
		}
	else
		{
		x_cursor_offset = X_CURSOR_OFFSET;
		y_cursor_offset = Y_CURSOR_OFFSET;
		}
	x = x_origin + ((x - x_origin) - x_window)*x_zoom + x_cursor_offset;
	y = y_origin + ((y - y_origin) - y_window)*y_zoom + y_cursor_offset;

	if( mode )
		ikfbcmem.fbc_Lcontrol |= FBC_CURSOR;
	else
		ikfbcmem.fbc_Lcontrol &= ~FBC_CURSOR;
	ikfbcmem.fbc_xcursor = x&01777;
	ikfbcmem.fbc_ycursor = y&01777;

	if( lseek( ifp->if_fd, FBCVC*4L, 0 ) == -1 )
		{
		fb_log( "adage_cmemory_addr : lseek failed.\n" );
		return	-1;
		}
	if( write( ifp->if_fd, &ikfbcmem.fbc_Lcontrol, 8 ) != 8 )
		{
		fb_log( "adage_cmemory_addr : write failed.\n" );
		return	-1;
		}
	return	0;
	}

/*	a d a g e _ c u r s o r _ m o v e _ s c r e e n _ a d d r ( )
	Place cursor at SCREEN coordinates x and y.
 */
_LOCAL_ int
adage_cscreen_addr( ifp, mode, x, y )
FBIO	*ifp;
int		mode;
int		x, y;
	{
	y = ifp->if_width-1-y;		/* 1st quadrant */
	if( ifp->if_width == 1024 && y_zoom == 1 )
		y += 30;
	if (mode)
		ikfbcmem.fbc_Lcontrol |= FBC_CURSOR;
	else
		ikfbcmem.fbc_Lcontrol &= ~FBC_CURSOR;
	ikfbcmem.fbc_xcursor = x&01777;
	ikfbcmem.fbc_ycursor = y&01777;

	if( lseek( ifp->if_fd, FBCVC*4L, 0 ) == -1 )
		{
		fb_log( "adage_cscreen_addr : lseek failed.\n" );
		return	-1;
		}
	if( write( ifp->if_fd, &ikfbcmem.fbc_Lcontrol, 8 ) != 8 )
		{
		fb_log( "adage_cscreen_addr : write failed.\n" );
		return	-1;
		}
	return	0;
	}

_LOCAL_ int
adage_cinit_bitmap( ifp, bitmap )
FBIO	*ifp;
long		*bitmap;
	{	register int i;
		long	cursorarray[256];
#ifdef pdp11
	for(i = 0; i < 256; i++)
		cursorarray[i] = ((bitmap[i/8]>>((i%8)*4))&017L)<<16;
#else
	for (i = 0; i < 256; i++)
		cursorarray[i] = (bitmap[i/8]>>((i%8)*4))&017L;
#endif
	if( lseek( ifp->if_fd, FBCCD*4L, 0 ) == -1 )
		{
		fb_log( "adage_cinit_bitmap : lseek failed.\n" );
		return	-1;
		}
	if( write( ifp->if_fd, cursorarray, 1024 ) != 1024 )
		{
		fb_log( "adage_cinit_bitmap : write failed.\n" );
		return	-1;
		}
	return	0;
	}

/*	Write one color map entry.
	Page selects the color map; 0, 1, 2, or  3.
	Offset indexes into the map.
 */
_LOCAL_ int
adage_colormap_write_entry( ifp, cp, page, offset )
FBIO	*ifp;
register RGBpixel	*cp;
long	page, offset;
	{	long	lp;

	lp = RGB10( (*cp)[RED]>>6, (*cp)[GRN]>>6, (*cp)[BLU]>>6 );
	lseek( ifp->if_fd, (LUVO + page*256 + offset)*4L, 0);
	if( write( ifp->if_fd, (char *) &lp, 4 ) != 4 )
		{
		fb_log( "adage_colormap_write_entry : write failed.\n" );
		return	-1;
		}
	return	0;
	}

_LOCAL_ int
adage_colormap_write( ifp, cp )
FBIO		*ifp;
register ColorMap	*cp;
{
	long cmap[1024];
	register long *lp = cmap;
	register int i;

	/* Note that RGB10(r,g,b) flips to cmap order (b,g,r).		*/
	if( cp == (ColorMap *) NULL )
		for( i=0; i < 256; i++ )
			*lp++ = RGB10( i<<2, i<<2, i<<2 );
	else
		for( i=0; i < 256; i++ )
			*lp++ = RGB10( cp->cm_red[i]>>6,
				       cp->cm_green[i]>>6,
				       cp->cm_blue[i]>>6 );

#ifdef pdp11
	/* 16-bit-word-in-long flipping for PDP's */
	for( i=0; i < 256; i++ )
		{
		register struct twiddle
			{
			short rhs;
			short lhs;
			}  *cmp = &cmap[i];
		register short temp;
		temp = cmp->rhs;
		cmp->rhs = cmp->lhs;
		cmp->lhs = temp;
		}
#endif
	/*
	 * Replicate first copy of color map onto second copy,
	 * and also do the "overlay" portion too.
	 * TODO:  Load inverse map into "overlay" (for cursor),
	 * and load standard film map into second map.
	 */
	for( i=0; i < 256; i++ )
		{
		cmap[i+512] = cmap[i];
		cmap[i+512+256] = cmap[i+256] = ~cmap[i];
		}
	if( lseek( ifp->if_fd, LUVO*4L, 0) == -1 )
		{
		fb_log( "adage_colormap_write : lseek failed.\n" );
		return	-1;
		}
	if( write( ifp->if_fd, cmap, 1024*4 ) != 1024*4 )
		{
		fb_log( "adage_colormap_write : write failed.\n" );
		return	-1;
		}
	return	0;
	}

_LOCAL_ int
adage_colormap_read( ifp, cp )
FBIO	*ifp;
register ColorMap	*cp;
{
	register int i;
	long cmap[1024];

	if( lseek( ifp->if_fd, LUVO*4L, 0) == -1 )
		{
		fb_log( "adage_colormap_read : lseek failed.\n" );
		return	-1;
		}
	if( read( ifp->if_fd, cmap, 1024*4 ) != 1024*4 )
		{
		fb_log( "adage_colormap_read : read failed.\n" );
		return	-1;
		}
#ifndef pdp11
	for( i=0; i < 256; i++ )
		{
		cp->cm_red[i] = (cmap[i]<<(6+0))  & 0xFFC0;
		cp->cm_green[i] = (cmap[i]>>(10-6))  & 0xFFC0;
		cp->cm_blue[i] = (cmap[i]>>(20-6)) & 0xFFC0;
		}
#else
	for( i=0; i < 256; i++ )
		{
		register struct twiddle
			{
			short rhs;
			short lhs;
			}  *cmp = &cmap[i];
		cp->cm_red[i] = (cmp->rhs & 0x3FF) << 6;
		cp->cm_green[i] = (((cmp->lhs<<6)&0x3C0) | ((cmp->rhs>>10)&0x3F)) << 6;
		cp->cm_blue[i] = ((cmp->lhs>>4) & 0x3FF) << 6;
		}
#endif
	return	0;
}

/*
 *		A D A G E _ C O L O R _ C L E A R
 *
 *  Clear the frame buffer to the given color.  There is no hardware
 *  Support for this so we do it via large DMA's.
 */
_LOCAL_ int
adage_color_clear( ifp, bpp )
register FBIO	*ifp;
register RGBpixel	*bpp;
{
	static RGBpixel	*pix_buf = NULL;
	register IKONASpixel *pix_to;
	register long	i;
	long	pixelstodo;
	int	fd;

	if( pix_buf == NULL )
		if( (pix_buf = (IKONASpixel *) malloc(ADAGE_DMA_BYTES)) == PIXEL_NULL ) {
			Malloc_Bomb(ADAGE_DMA_BYTES);
			return	-1;
		}

	/* Fill buffer with background color. */
	for( i = ADAGE_DMA_PIXELS, pix_to = pix_buf; i > 0; i-- ) {
		COPYRGB( *pix_to, *bpp );
		pix_to++;
	}

	/* Set start of framebuffer */
	fd = ifp->if_fd;
	if( lseek( fd, 0L, 0 ) == -1 ) {
		fb_log( "adage_color_clear : seek failed.\n" );
		return	-1;
	}

	/* Send until frame buffer is full. */
	pixelstodo = ifp->if_height * ifp->if_width;
	while( pixelstodo > 0 ) {
		i = pixelstodo > ADAGE_DMA_PIXELS ? ADAGE_DMA_PIXELS : pixelstodo;
		if( write( fd, pix_buf, i*sizeof(IKONASpixel) ) == -1 )
			return	-1;
		pixelstodo -= i;
	}

	return	0;
}
