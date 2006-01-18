/*                        I F _ R A T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
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
/** @file if_rat.c
 *  FrameBuffer library interface for Raster Technology One/80 with
 *  24-bit RGB memory.
 *
 *  Authors -
 *	Brant A. Ross
 *
 *  Source -
 * 	General Motors Military Vehicles Operation
 *      P.O. Box 420  Mail Code O01
 *	Indianapolis, IN  46206-0420
 *
 *  BRL NOTE: We have not been able to test this interface in a long
 *   time.  LIBFB has changed several times since then so this code
 *   may not even compile any longer.  If you make changes to this
 *   please let us know.
 */
/*@}*/

#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <fcntl.h>

#include "machine.h"
#include "fb.h"
#include "./fblocal.h"

/* typedef unsigned char	u_char;	*/
int	_fbfd;
int	_fbsize = 512;
#define Max( a, b )	((a) < (b) ? (b) : (a))
#define Min( a, b )	((a) > (b) ? (b) : (a))

#define Rat_Cvt( x, y )	x -= _fbsize/2, y -= _fbsize/2
#define Rat_Write( cmd, buff ) \
	{	register int	ct, i; \
	if( write( _fbfd, buff, sizeof(buff) ) == -1 ) \
		{ \
		(void) fprintf( stderr, "%s : write failed!\n", cmd ); \
		return	0; \
		} \
	}
#define Round_N( a, n )	{ register int f = a%n; a = f<10 ? a-f : a+(n-f); }
#define PAD		0xA
#define	MAX_RAT_BUFF	(12*1024L)
#define	MAX_RAT_READ	1024
int		rat_debug = 0;
static int	zoom_factor = 1;

static int	cload(int creg, int x, int y),
		debug(int flag), entergraphics(void), flood(void), lutrmp(int code, int sind, int eind, int sval, int eval), lut8(int index, u_char r, u_char g, u_char b),
		memsel(int unit), movabs(register int x, register int y), pixels(int rows, int cols, register u_char *pix_buf, int bytes, FBIO *ifp), quit(void),
		rdmask(int bitm), rdmode(int flag), rdpixr(int vreg), readf(int func), readw(int rows, int cols, int bf), readvr(int vreg),
		rgbtru(int flag), scrorg(int x, int y), value(u_char red, u_char green, u_char blue), vidform(int mode, int flag),
		warm(void), wrmask(int bitm, int bankm), xhair(int num, int flag), zoom(int factor);

_LOCAL_ int	rat_open(FBIO *ifp, char *file, int width, int height),
		rat_close(FBIO *ifp),
		rat_clear(FBIO *ifp, RGBpixel (*pp)),
		rat_read(FBIO *ifp, int x, int y, RGBpixel (*pixelp), int count),
		rat_write(FBIO *ifp, int x, int y, RGBpixel (*pixelp), int count),
		rat_rmap(FBIO *ifp, ColorMap *cmp),
		rat_wmap(FBIO *ifp, ColorMap *cmp),
		rat_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom),
		rat_window_set(FBIO *ifp, int x, int y),	/* OLD */
		rat_zoom_set(FBIO *ifp, int x, int y),		/* OLD */
		rat_setcursor(FBIO *ifp, unsigned char *bits, int xbits, int ybits, int xorig, int yorig),
		rat_cursor(FBIO *ifp, int mode, int x, int y),
		rat_getcursor(),
		rat_help(FBIO *ifp);

/* This is the ONLY thing that we normally "export" */
FBIO rat_interface =  {
	0,
	rat_open,			/* device_open		*/
	rat_close,			/* device_close		*/
	rat_clear,			/* device_clear		*/
	rat_read,			/* buffer_read		*/
	rat_write,			/* buffer_write		*/
	rat_rmap,			/* colormap_read	*/
	rat_wmap,			/* colormap_write	*/
	rat_view,			/* set view		*/
	fb_sim_getview,			/* get view		*/
	rat_setcursor,			/* define cursor	*/
	rat_cursor,			/* set cursor		*/
	fb_sim_getcursor,		/* get cursor		*/
	fb_sim_readrect,		/* read rectangle	*/
	fb_sim_writerect,		/* write rectangle	*/
	fb_sim_bwreadrect,
	fb_sim_bwwriterect,
	fb_null,			/* handle events	*/
	fb_null,			/* flush output		*/
	rat_close,			/* free resources	*/
	rat_help,			/* help function	*/
	"Raster Technology One/80",	/* device description	*/
	1024,				/* max width		*/
	1024,				/* max height		*/
	"/dev/rt",			/* short device name	*/
	512,				/* default/current width  */
	512,				/* default/current height */
	-1,				/* select fd		*/
	-1,				/* file descriptor	*/
	1, 1,				/* zoom			*/
	256, 256,			/* window center	*/
	0, 0, 0,			/* cursor		*/
	PIXEL_NULL,			/* page_base		*/
	PIXEL_NULL,			/* page_curp		*/
	PIXEL_NULL,			/* page_endp		*/
	-1,				/* page_no		*/
	0,				/* page_dirty		*/
	0L,				/* page_curpos		*/
	0L,				/* page_pixels		*/
	0				/* debug		*/
};

_LOCAL_ int
rat_open(FBIO *ifp, char *file, int width, int height)
{
	FB_CK_FBIO(ifp);
	if( (ifp->if_fd = open( file, O_RDWR, 0 )) == -1)
		{
		perror(file);
		return -1;  }
	_fbfd = ifp->if_fd;
	_fbsize = width;
	if( width == 1024)
		{ ifp->if_width = width;
		  ifp->if_height = height; }
	_rat_init(ifp);
	return ifp->if_fd;
}

_LOCAL_ int
rat_close(FBIO *ifp)

/*	_ r a t _ c l o s e ( )
	Issue quit command, and close connection.
 */
{
	if(  !	quit()
	    ||	close( ifp->if_fd ) == -1
		)
		{
		(void) fprintf( stderr, "_rat_close() close failed!\n" );
		return	-1;
		}
	return	0;
}


_LOCAL_ int
rat_clear(FBIO *ifp, RGBpixel (*pp))


/*	_ r a t _ c l e a r ( )
	Clear the Raster Tech. to the background color.
 */
{
	if(	pp != NULL
	    &&	value(	(*pp)[RED],(*pp)[GRN],(*pp)[BLU] )
	    &&	flood()  )
		return	0;
	else
	    if ( !(value( 0, 0, 0 ) && flood() ) )
		return	1;
	return	0;
}

_LOCAL_ int
rat_read(FBIO *ifp, int x, int y, RGBpixel (*pixelp), int count)




/*	_ r a t _ r e a d ( )
 */
{
	register int	nrows;
	register int	bytes;
	register int	load;
	register u_char	*p;
	static u_char	pix_buf[MAX_RAT_BUFF];

	/* If first scanline is a partial, input it seperately.		*/
	if( x > 0 || x + count <= _fbsize )
		{	register int	i;
		if( ! movabs( x, y ) )
			return	-1;
		y++;
		if( x + count <= _fbsize )
			i = count;	/* Only 1 scanline is involved.	*/
		else
			i = _fbsize - x; /* First scan is a partial.	*/
		if( ! readw( 1, i, 1 ) )
			return	-1;
		for(	bytes = i * 3, p = pix_buf;
			bytes > 0;
			bytes -= load, p += load
			)
			{
			if( bytes > MAX_RAT_READ )
				load = MAX_RAT_READ;
			else
				load = bytes;
			if( read( ifp->if_fd, p, load ) < load )
				{
				(void) fprintf( stderr,
						"_rat_read() read failed\n"
						);
				return	-1;
				}
			}
		for( p = pix_buf; i > 0; i--, pixelp++, count-- )
			{
			(*pixelp)[RED] = *p++;
			(*pixelp)[GRN] = *p++;
			(*pixelp)[BLU] = *p++;
			}
		}
	/* Do all full scanlines.					*/
	while( (nrows = count / _fbsize) != 0 )
		{	register int	i;
		if( ! movabs( 0, y ) )
			return	-1;
		if( nrows * _fbsize * 3 > MAX_RAT_BUFF )
			nrows = MAX_RAT_BUFF / (_fbsize * 3);
		i = nrows * _fbsize;
		y += nrows;
		if( ! readw( nrows, _fbsize, 1 ) )
			return	-1;
		for(	bytes = i * 3, p = pix_buf;
			bytes > 0;
			bytes -= load, p += load
			)
			{
			if( bytes > MAX_RAT_READ )
				load = MAX_RAT_READ;
			else
				load = bytes;
			{ register int	ii;
			for( ii = 0; ii < load; ii++ )
				p[ii] = 100;
			}
			if( read( ifp->if_fd, p, load ) < load )
				{
				(void) fprintf( stderr,
						"_rat_read() read failed\n"
						);
				return	-1;
				}
			}
		for(	p = pix_buf;
			i > 0;
			i--, pixelp++, count--
			)
			{
			(*pixelp)[RED] = *p++;
			(*pixelp)[GRN] = *p++;
			(*pixelp)[BLU] = *p++;
			}
		}
	if( count > 0 )
		{ /* Do partial scanline.				*/
			register int	i;
			register u_char	*p = pix_buf;

		/*(void) fprintf( stderr, "doing partial scan at end\n" );*/
		if(   !	movabs( 0, y )
		   || ! readw( 1, count, 1 )
			)
			return	-1;
		for(	bytes = count * 3, p = pix_buf;
			bytes > 0;
			bytes -= load, p += load
			)
			{
			if( bytes > MAX_RAT_READ )
				load = MAX_RAT_READ;
			else
				load = bytes;
			if( read( ifp->if_fd, p, load ) < load )
				{
				(void) fprintf( stderr,
						"_rat_read() read failed\n"
						);
				return	-1;
				}
			}
		for( p = pix_buf; count > 0; pixelp++, count-- )
			{
			(*pixelp)[RED] = *p++;
			(*pixelp)[GRN] = *p++;
			(*pixelp)[BLU] = *p++;
			}
		}
	return	0;
}

_LOCAL_ int
rat_write(FBIO *ifp, int x, int y, RGBpixel (*pixelp), int count)




/*	_ r a t _ w r i t e ( )
 */
{
	register int	nrows;
	int		ncols;
	static u_char	pix_buf[MAX_RAT_BUFF+1];

	/* If first scanline is a partial, output it separately.	*/
	if( x > 0 || x + count <= _fbsize )
		{	register int	bytes, i;
			register u_char	*p = pix_buf;
		if( ! movabs( x, y ) )
			return	-1;
		if( x + count <= _fbsize )
			ncols = i = count;  /* Only 1 scanline is involved.  */
		else			/* First scan is a partial.	*/
			ncols = i = _fbsize - x;
		y++;
		for( bytes = 0; i > 0; i--, pixelp++, bytes += 3, count-- )
			{
			*p++ = (*pixelp)[RED];
			*p++ = (*pixelp)[GRN];
			*p++ = (*pixelp)[BLU];
			}
		if( ! pixels( 1, ncols, pix_buf, bytes, ifp ) )
			return	-1;
		}
	/* Do all full scanlines.					*/
	while( (nrows = count / _fbsize) != 0 )
		{	register int	bytes, i, j;
			register u_char	*p = pix_buf,*p1;
		if( nrows * _fbsize * 3 > MAX_RAT_BUFF )
			nrows = MAX_RAT_BUFF / (_fbsize * 3);
		y += nrows;
		if( ! movabs( 0, y-1 ) )
			return	-1;
		for(	j = nrows , bytes = 0; j > 0; j-- )
		    {
		   p1 = p + ( (j-1)*_fbsize*3 );
		   for(	i = _fbsize; i > 0;
			i--, pixelp++, bytes += 3, count-- )
			{
			*p1++ = (*pixelp)[RED];
			*p1++ = (*pixelp)[GRN];
			*p1++ = (*pixelp)[BLU];
			}
		    }
		if( ! pixels( nrows, _fbsize, pix_buf, bytes, ifp ) )
			return	-1;
		}
	/* If partial scanline remains, finish up.			*/
	if( count > 0 )
		{	register int	bytes, i;
			register u_char	*p = pix_buf;
		if( ! movabs( 0, y ) )
			return	-1;
		ncols = count;
		for( bytes = 0; count > 0; pixelp++, bytes += 3, count-- )
			{
			*p++ = (*pixelp)[RED];
			*p++ = (*pixelp)[GRN];
			*p++ = (*pixelp)[BLU];
			}
		if( ! pixels( 1, ncols, pix_buf, bytes, ifp ) )
			return	-1;
		}
	return	0;
}

_LOCAL_ int
rat_rmap(FBIO *ifp, ColorMap *cmp)
{
	return(0);
}

_LOCAL_ int
rat_wmap(FBIO *ifp, ColorMap *cmp)


/*	_ r a t _ w m a p ( )
	Load the color map into the frame buffer.
 */
{
	{
	register int	i;

	/* If cmp is NULL, write standard map.				*/
	if( cmp == (ColorMap *) NULL )
/*		if( ! lutrmp( 7, 0, 255, 0, 255 ) )       */
/*			return	-1;         */
/*		else                   */
			return	0;
	else
		{
/*		for( i = 0; i < 256; i++ )
			if( ! lut8(	i,
					cmp->cm_red[i],
					cmp->cm_green[i],
					cmp->cm_blue[i]
					)
				)
				return	-1;
 */
		}
	return	0;
	}
}

_LOCAL_ int
rat_view(FBIO *ifp, int xcenter, int ycenter, int xzoom, int yzoom)
{
	rat_window_set(ifp, xcenter, ycenter);
	rat_zoom_set(ifp, xzoom, yzoom);
	fb_sim_view(ifp, xcenter, ycenter, xzoom, yzoom);
	return	0;
}

_LOCAL_ int
rat_window_set(FBIO *ifp, int x, int y)


/*	_ r a t _ w i n d o w ( )
	This routine takes advantage of the fact that there is only 1
	scaling parameter available (equal scaling in x and y).
 */
{
	Rat_Cvt( x, y );
	Round_N( x, 20 );
	return	scrorg( x, y ) ? 0 : -1;
}

_LOCAL_ int
rat_zoom_set(FBIO *ifp, int x, int y)


/*	_ r a t _ z o o m ( )
	 The Raster Tech does not scale independently in x and y.
		Also addressing is the same in low and high res. so
		must zoom twice for low res.
 */
{
	zoom_factor = Max( x, y );
	zoom_factor = Min( zoom_factor, 16 );
	zoom_factor = Max( zoom_factor, 1 );
	return	zoom(	ifp->if_width == 512 && zoom_factor < 16 ?
			zoom_factor * 2 :
			zoom_factor
			) ? 0 : -1;
}

_LOCAL_ int
rat_setcursor(FBIO *ifp, unsigned char *bits, int xbits, int ybits, int xorig, int yorig)
{
	return	0;
}

_LOCAL_ int
rat_cursor(FBIO *ifp, int mode, int x, int y)
/*	Place cursor at image (pixel) coordinates x & y
 */



{
	fb_sim_cursor(ifp, mode, x, y);
/*	if(   !	cload( 5, x, y )  ||  ! xhair( 0, mode ) )
		return	-1;	*/
	if(   !	cload(17, x, y )  ||  ! cursor( 0, mode ) )
		return	-1;
	return	0;
}

/*	_ r a t _ i n i t ( )
	Reset, enter graphics mode, set interlace on, turn on 24 bit color.
	Set origin and zoom factor.
 */
_rat_init(FBIO *ifp)
{
	static unsigned char firstcmds[] = {0x10,0x01,0xFD,0x00};

	static unsigned char buff[] =
		{0x01,0x00,		/* Enter graphics	*/
		0x48,0x00,		/* MEMSEL 0		*/
		0x9D,0xFF,0x07,0x00};	/* WRMASK 255 7		*/

	if( warm()
	    &&	entergraphics()
/*	    &&	vidform( 0, 1 )   only if interlace needed  */
/*	    &&	memsel( 0 )		*/
/*	    &&	rgbtru( 1 )		*/
	    &&	wrmask( 255, 7 )	/* enable all bits & channels.	*/
	    &&	rdmask( 255 )		/* enable for reading.		*/
	    &&	readf( 0 )
	    &&	rdmode( 1 )
	    &&	rat_zoom_set( ifp, 1, 1 ) == 0
	    &&	scrorg( 0, 0 )
		)
		return	0;
	else
		return	-1;
	}






static int
cload(int creg, int x, int y)
{
	u_char	buff[8];

	Rat_Cvt( x, y );
	buff[0] = 0xA0;
	buff[1] = creg;
	buff[2] = (x>>8)&0x0FF;		/* high_x, low_x.	*/
	buff[3] = x&0x0FF;
	buff[4] = (y>>8)&0x0FF;		/* high_y, low_y.	*/
	buff[5] = y&0x0FF;
	buff[6] = PAD;
	buff[7] = PAD;
	Rat_Write( "cload", buff );
	return	1;
	}

static int
debug(int flag)
{
	u_char	buff[2];

	buff[0] = 0xA8;
	buff[1] = flag;
	Rat_Write( "debug", buff );
	return	1;
	}

static int
entergraphics(void)
{
	u_char	buff[2];

	buff[0] = 0x01;
	buff[1] = PAD;

	Rat_Write( "entergraphics", buff );
	return	1;
	}

static int
flood(void)
{
	u_char	buff[2];

	buff[0] = 0x07;
	buff[1] = PAD;
	Rat_Write( "flood", buff );
	return	1;
	}

static int
lutrmp(int code, int sind, int eind, int sval, int eval)
{
	u_char	buff[6];

	buff[0] = 0x01d;
	buff[1] = code;
	buff[2] = sind;
	buff[3] = eind;
	buff[4] = sval;
	buff[5] = eval;
/*	Rat_Write( "lutrmp", buff );   */
	return	1;
	}

static int
lut8(int index, u_char r, u_char g, u_char b)
{
	u_char	buff[6];

	buff[0] = 0x01c;
	buff[1] = index;
	buff[2] = r;
	buff[3] = g;
	buff[4] = b;
	buff[5] = PAD;
/*	Rat_Write( "lut8", buff );   */
	return	1;
	}

/*	m e m s e l ( )
	Select a memory unit.
	Since the RLE format splits up the colors running in RGBTRU OFF
	and selecting the unit for each color is the easiest way to go.
 */
static int
memsel(int unit)
{
	u_char	buff[2];

	buff[0] = 0x48;
	buff[1] = unit;
/*	Rat_Write( "memsel", buff );    */
	return	1;
	}

/*	m o v a b s ( )
	Set the current position (CREG 0) to (x, y).
	The generic device has its origin in the upper left corner (modeled
	after the Ikonas or a UNIX file)...


	(0,      0)........................(ifp->if_width,         0)
	.							   .
	.							   .
	.							   .
	.							   .
	.							   .
	.                                          		   .
	(0, ifp->if_width).................(ifp->if_width, ifp->if_width)


	The Raster Tech. is a 4-quadrant cartesian device, so its origin is at
	screen center...


	(-_fbsize/2, _fbsize/2)......(_fbsize/2, _fbsize/2)
	.                                                 .
	.                                                 .
	.                                                 .
	.                      (0, 0)                     .
	.                                                 .
	.                                                 .
	.                                                 .
	(-_fbsize/2, -_fbsize/2).....(_fbsize/2, -_fbsize/2)
 */
static int
movabs(register int x, register int y)
{
	u_char	buff[8];

	Rat_Cvt( x, y );
	buff[0] = 0x1;
	buff[1] = (x>>8)&0x0FF;		/* high_x, low_x.	*/
	buff[2] = x&0x0FF;
	buff[3] = (y>>8)&0x0FF;		/* high_y, low_y.	*/
	buff[4] = y&0x0FF;
	buff[5] = PAD;
	buff[6] = PAD;
	buff[7] = PAD;
	Rat_Write( "movabs", buff );
	return	1;
	}

static int
pixels(int rows, int cols, register u_char *pix_buf, int bytes, FBIO *ifp)
{
	static u_char	buff[MAX_RAT_BUFF+6];
	register int	i, ct;

	if( rat_debug == 1 )
		debug( 0 );
	ct = bytes & 1 ? 5 + bytes : 6 + bytes; /* Insure even count.	*/
	buff[0] = 0x28;
	buff[1] = rows>>8 & 0xFF;
	buff[2] = rows & 0xFF;
	buff[3] = cols>>8 & 0xFF;
	buff[4] = cols & 0xFF;
	for( i = 5; i < ct; i++ )
		buff[i] = *pix_buf++;
	if( ! (bytes & 1) )
		buff[ct-1] = PAD;
	if( write( ifp->if_fd, buff, ct ) == -1 )
		{
		(void) fprintf( stderr, "pixels : write failed!\n" );
		return	0;
		}
	if( rat_debug == 1 )
		debug( rat_debug );
	return	1;
	}

static int
quit(void)
{
	u_char	buff[2];

	buff[0] = 0xFF;
	buff[1] = PAD;
	Rat_Write( "quit", buff );
	return	1;
	}

/*	r d m a s k ( )
	Set read mask.
 */
static int
rdmask(int bitm)
{
	u_char	buff[2];

	buff[0] = 0x9E;
	buff[1] = bitm;
	Rat_Write( "rdmask", buff );
	return	1;
	}

/*	r d m o d e ( )
 */
static int
rdmode(int flag)
{
	u_char	buff[2];

	buff[0] = 0xD3;
	buff[1] = flag;
	Rat_Write( "rdmode", buff );
	return	1;
	}

static int
rdpixr(int vreg)
{
	u_char	buff[2];

	buff[0] = 0xAF;
	buff[1] = vreg;
	Rat_Write( "rdpixr", buff );
	return	1;
	}

static int
readf(int func)
{
	u_char	buff[2];

	buff[0] = 0x27;
	buff[1] = func;
	Rat_Write( "readf", buff );
	return	1;
	}

static int
readvr(int vreg)
{
	u_char	buff[2];

	buff[0] = 0x99;
	buff[1] = vreg;
	Rat_Write( "readvr", buff );
	return	1;
	}

static int
readw(int rows, int cols, int bf)
{
	u_char	buff[6];

	buff[0] = 0x96;
	buff[1] = rows>>8 & 0xFF;
	buff[2] = rows & 0xFF;
	buff[3] = cols>>8 & 0xFF;
	buff[4] = cols & 0xFF;
	buff[5] = bf;
	Rat_Write( "readw", buff );
	return	1;
	}

static int
rgbtru(int flag)
{
	u_char	buff[2];

	buff[0] = 0x4e;
	buff[1] = flag;
	Rat_Write( "rgbtru", buff );
	return	1;
	}


/*	s c r o r g ( )
	Set the screen-center coordinate (CREG 4) to (x, y).
 */
static int
scrorg(int x, int y)
{
	u_char	buff[6];

	buff[0] = 0x36;
	buff[1] = (x>>8) & 0x0FF;		/* high_x, low_x.	*/
	buff[2] = x & 0x0FF;
	buff[3] = (y>>8) & 0x0FF;		/* high_y, low_y.	*/
	buff[4] = y & 0x0FF;
	buff[5] = PAD;
	Rat_Write( "scrorg", buff );
	return	1;
	}

static int
value(u_char red, u_char green, u_char blue)
{
	char	buff[4];

	buff[0] = 0x06;
	buff[1] = red;
	buff[2] = green;
	buff[3] = blue;
	Rat_Write( "value", buff );
	return	1;
	}

static int
vidform(int mode, int flag)
{
	u_char	buff[4];

	buff[0] = 0x08;
	buff[1] = mode;
	buff[2] = flag;
	buff[3] = PAD;
	Rat_Write( "vidform", buff );
	return	1;
	}

static int
warm(void)
{
	u_char	buff[2];

	buff[0] = 0x10;
	buff[1] = PAD;
	Rat_Write( "warm", buff );
	return	1;
	}

/*	w r m a s k ( )
	Set write mask.
 */
static int
wrmask(int bitm, int bankm)
{
	u_char	buff[4];

	buff[0] = 0x9D;
	buff[1] = bitm;
	buff[2] = bankm;
	buff[3] = PAD;
	Rat_Write( "wrmask", buff );
	return	1;
	}

static int
cursor(int num, int flag)
{
	u_char	buff[4];

	buff[0] = 0x4A;
	buff[1] = num;
	buff[2] = flag;
	buff[3] = PAD;
	Rat_Write( "cursor", buff );
	return	1;
	}


static int
xhair(int num, int flag)
{
	u_char	buff[4];

	buff[0] = 0x9C;
	buff[1] = num;
	buff[2] = flag;
	buff[3] = PAD;
	Rat_Write( "xhair", buff );
	return	1;
	}

static int
zoom(int factor)
{
	u_char	buff[2];

	buff[0] = 0x34;
	buff[1] = factor;
	Rat_Write( "zoom", buff );
	return	1;
	}

_LOCAL_ int
rat_help(FBIO *ifp)
{
	fb_log( "Description: %s\n", rat_interface.if_type );
	fb_log( "Device: %s\n", ifp->if_name );
	fb_log( "Max width/height: %d %d\n",
		rat_interface.if_max_width,
		rat_interface.if_max_height );
	fb_log( "Default width/height: %d %d\n",
		rat_interface.if_width,
		rat_interface.if_height );
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
