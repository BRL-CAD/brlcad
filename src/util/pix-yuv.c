/*                       P I X - Y U V . C
 * BRL-CAD
 *
 * Copyright (C) 1995-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file pix-yuv.c
 *
 *  Convert a .pix file to a .YUV file, i.e. in CCIR-601 format.
 *  Only the active pixels are recorded in the file, as in
 *  the .YUV files used with an Abekas A60 D-1 video disk recorder.
 *
 *  Because .pix is first quadrant and .yuv is fourth quadrant,
 *  the entire image is processed in memory.
 *
 *  It is not clear that this tool is useful on image sizes other
 *  than the default of 720 x 485.
 *
 *  The code was liberally borrowed from libfb/if_ab.c
 *
 *  Authors -
 *	Michael John Muuss
 *	Phil Dykstra
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
                                                                                                                                                                            
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
                                                                                                                                                                            

#include <stdio.h>

#include "machine.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"

static char	*file_name;
static int	infd;

static int	fileinput = 0;		/* file or pipe on input? */
static int	autosize = 0;		/* !0 to autosize input */

static int	file_width = 720;	/* default input width */
static int	file_height = 485;	/* default input height */

static int	mread(int fd, register char *bufp, int n);
void		ab_rgb_to_yuv(unsigned char *yuv_buf, unsigned char *rgb_buf, int len);
void		ab_yuv_to_rgb(unsigned char *rgb_buf, unsigned char *yuv_buf, int len);

static char usage[] = "\
Usage: pix-yuv [-h] [-a]\n\
	[-s squaresize] [-w file_width] [-n file_height] [file.pix] > file.yuv\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = getopt( argc, argv, "ahs:w:n:" )) != EOF )  {
		switch( c )  {
		case 'a':
			autosize = 1;
			break;
		case 'h':
			/* high-res */
			file_height = file_width = 1024;
			autosize = 0;
			break;
		case 's':
			/* square file size */
			file_height = file_width = atoi(optarg);
			autosize = 0;
			break;
		case 'w':
			file_width = atoi(optarg);
			autosize = 0;
			break;
		case 'n':
			file_height = atoi(optarg);
			autosize = 0;
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )  {
		if( isatty(fileno(stdin)) )
			return(0);
		file_name = "-";
		infd = fileno(stdin);
	} else {
		file_name = argv[optind];
		if( (infd = open(file_name, 0)) < 0 )  {
			perror(file_name);
			(void)fprintf( stderr,
				"pix-yuv: cannot open \"%s\" for reading\n",
				file_name );
			return(0);
		}
		fileinput++;
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "pix-yuv: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
	char	*inbuf;
	char	*outbuf;
	int	y;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	/* autosize input? */
	if( fileinput && autosize ) {
		int	w, h;
		if( bn_common_file_size(&w, &h, file_name, 3) ) {
			file_width = w;
			file_height = h;
		} else {
			fprintf(stderr, "pix-yuv: unable to autosize\n");
		}
	}

	/* Allocate full size buffers for input and output */
	inbuf = malloc( 3*file_width*file_height+8 );
	outbuf = malloc( 2*file_width*file_height+8 );

	if( mread( infd, inbuf, 3*file_width*file_height ) < 3*file_width*file_height )  {
		fprintf(stderr, "pix-yuv: short input file, aborting\n");
		exit(1);
	}

	for( y = 0; y < file_height; y++ )  {
		ab_rgb_to_yuv(
			&outbuf[(file_height-1-y)*file_width*2],
			&inbuf[y*file_width*3],
			file_width );
	}

	if( write( 1, outbuf, 2*file_width*file_height ) < 2*file_width*file_height )  {
		perror("stdout");
		fprintf(stderr, "pix-yuv: output write error, aborting\n");
		exit(2);
	}
	return 0;
}

/*
 *			M R E A D
 *
 * Internal.
 * This function performs the function of a read(II) but will
 * call read(II) multiple times in order to get the requested
 * number of characters.  This can be necessary because pipes
 * and network connections don't deliver data with the same
 * grouping as it is written with.  Written by Robert S. Miles, BRL.
 */
static int
mread(int fd, register char *bufp, int n)
{
	register int	count = 0;
	register int	nread;

	do {
		nread = read(fd, bufp, (unsigned)n-count);
		if(nread < 0)  {
			perror("mread");
			return(-1);
		}
		if(nread == 0)
			return((int)count);
		count += (unsigned)nread;
		bufp += nread;
	 } while(count < n);

	return((int)count);
}

/*************************************************************************
 *************************************************************************
 *  Herein lies the conversion between YUV and RGB
 *************************************************************************
 *************************************************************************
 */
/*  A 4:2:2 framestore uses 2 bytes per pixel.  The even pixels (from 0)
 *  hold Cb and Y, the odd pixels Cr and Y.  Thus a scan line has:
 *      Cb Y Cr Y Cb Y Cr Y ...
 *  If we are at an even pixel, we use the Cr value following it.  If
 *  we are at an odd pixel, we use the Cb value following it.
 *
 *  Y:       0 .. 219 range, offset by 16   [16 .. 235]
 *  U, V: -112 .. +112 range, offset by 128 [16 .. 240]
 */

#define	V5DOT(a,b)	(a[0]*b[0]+a[1]*b[1]+a[2]*b[2]+a[3]*b[3]+a[4]*b[4])
#define	floor(d)	(d>=0?(int)d:((int)d==d?d:(int)(d-1.0)))
#define	CLIP(out,in)		{ register int t; \
		if( (t = (in)) < 0 )  (out) = 0; \
		else if( t >= 255 )  (out) = 255; \
		else (out) = t; }

#define	LINE_LENGTH	720
#define	FRAME_LENGTH	486

static double	y_weights[] = {  0.299,   0.587,   0.114 };
static double	u_weights[] = { -0.1686, -0.3311,  0.4997 };
static double	v_weights[] = {  0.4998, -0.4185, -0.0813 };

static double	y_filter[] = { -0.05674, 0.01883, 1.07582, 0.01883, -0.05674 };
static double	u_filter[] = {  0.14963, 0.22010, 0.26054, 0.22010,  0.14963 };
static double	v_filter[] = {  0.14963, 0.22010, 0.26054, 0.22010,  0.14963 };

/* XXX should be dynamically allocated.  Make 4X default size */
static double	ybuf[724*4];
static double	ubuf[724*4];
static double	vbuf[724*4];

/* RGB to YUV */
void
ab_rgb_to_yuv(unsigned char *yuv_buf, unsigned char *rgb_buf, int len)
{
	register unsigned char *cp;
	register double	*yp, *up, *vp;
	register int	i;
	static int	first=1;

	if(first)  {
		/* SETUP */
		for( i = 0; i < 5; i++ ) {
			y_filter[i] *= 219.0/255.0;
			u_filter[i] *= 224.0/255.0;
			v_filter[i] *= 224.0/255.0;
		}
		first = 0;
	}

	/* Matrix RGB's into separate Y, U, and V arrays */
	yp = &ybuf[2];
	up = &ubuf[2];
	vp = &vbuf[2];
	cp = rgb_buf;
	for( i = len; i; i-- ) {
		*yp++ = VDOT( y_weights, cp );
		*up++ = VDOT( u_weights, cp );
		*vp++ = VDOT( v_weights, cp );
		cp += 3;
	}

	/* filter, scale, and sample YUV arrays */
	yp = ybuf;
	up = ubuf;
	vp = vbuf;
	cp = yuv_buf;
	for( i = len/2; i; i-- ) {
		*cp++ = V5DOT(u_filter,up) + 128.0;	/* u */
		*cp++ = V5DOT(y_filter,yp) + 16.0;	/* y */
		*cp++ = V5DOT(v_filter,vp) + 128.0;	/* v */
		yp++;
		*cp++ = V5DOT(y_filter,yp) + 16.0;	/* y */
		yp++;
		up += 2;
		vp += 2;
	}
}

/* YUV to RGB */
void
ab_yuv_to_rgb(unsigned char *rgb_buf, unsigned char *yuv_buf, int len)
{
	register unsigned char *rgbp;
	register unsigned char *yuvp;
	register double	y;
	register double	u = 0.0;
	register double	v;
	register int	pixel;
	int		last;

	/* Input stream looks like:  uy  vy  uy  vy  */

	rgbp = rgb_buf;
	yuvp = yuv_buf;
	last = len/2;
	for( pixel = last; pixel; pixel-- ) {
		/* even pixel, get y and next v */
		if( pixel == last ) {
			u = ((double)(((int)*yuvp++) - 128)) * (255.0/224.0);
		}
		y = ((double)(((int)*yuvp++) - 16)) * (255.0/219.0);
		v = ((double)(((int)*yuvp++) - 128)) * (255.0/224.0);

		CLIP( *rgbp++, y + 1.4026 * v);			/* R */
		CLIP( *rgbp++, y - 0.3444 * u - 0.7144 * v);	/* G */
		CLIP( *rgbp++, y + 1.7730 * u);			/* B */

		/* odd pixel, got v already, get y and next u */
		y = ((double)(((int)*yuvp++) - 16)) * (255.0/219.0);
		if( pixel != 1 ) {
			u = ((double)(((int)*yuvp++) - 128)) * (255.0/224.0);
		}

		CLIP( *rgbp++, y + 1.4026 * v);			/* R */
		CLIP( *rgbp++, y - 0.3444 * u - 0.7144 * v);	/* G */
		CLIP( *rgbp++, y + 1.7730 * u);			/* B */
	}
}








#if 0

/*
 *			S E P A R A T E
 *
 *  Unpack RGB byte tripples into three separate arrays of integers.
 *  The first and last pixels are replicated twice, to handle border effects.
 *
 *  Updated version:  the outputs are Y U V values, not R G B.
 */
separate( rop, gop, bop, cp, num )
register int	*rop;			/* Y */
register int	*gop;			/* U */
register int	*bop;			/* V */
register unsigned char	*cp;
int		num;
{
	register int 	i;
	register int	r, g, b;

	r = cp[0];
	g = cp[1];
	b = cp[2];

#define YCONV(_r, _g, _b)	(_r * 0.299 + _g * 0.587 + _b * 0.144 + 0.9)
#define UCONV(_r, _g, _b)	(_r * -0.1686 + _g * -0.3311 + _b * 0.4997 + 0.9)
#define VCONV(_r, _g, _b)	(_r * 0.4998 + _g * -0.4185 + _b * -0.0813 + 0.9)

	rop[-1] = rop[-2] = YCONV(r,g,b);
	gop[-1] = gop[-2] = UCONV(r,g,b);
	bop[-1] = bop[-2] = VCONV(r,g,b);

	for( i = num-1; i >= 0; i-- )  {
		r = cp[0];
		g = cp[1];
		b = cp[2];
		cp += 3;
		*rop++ = YCONV(r,g,b);
		*gop++ = UCONV(r,g,b);
		*bop++ = VCONV(r,g,b);
	}

	r = cp[-3];
	g = cp[-2];
	b = cp[-1];

	*rop++ = YCONV(r,g,b);
	*gop++ = UCONV(r,g,b);
	*bop++ = VCONV(r,g,b);

	*rop++ = YCONV(r,g,b);
	*gop++ = UCONV(r,g,b);
	*bop++ = VCONV(r,g,b);
}

/*
 *			C O M B I N E
 *
 *  Combine three separate arrays of integers into a buffer of
 *  RGB byte tripples
 */
combine( cp, rip, gip, bip, num )
register unsigned char	*cp;
register int		*rip;
register int		*gip;
register int		*bip;
int			num;
{
	register int 	i;

#define RCONV(_y, _u, _v)	(_y + 1.4026 * _v)
#define GCONV(_y, _u, _v)	(_y - 0.3444 * _u - 0.7144 * _v)
#define BCONV(_y, _u, _v)	(_y + 1.7730 * _u)

#define CLIP(_v)	( ((_v) <= 0) ? 0 : (((_v) >= 255) ? 255 : (_v)) )

	for( i = num-1; i >= 0; i-- )  {
		register int	y, u, v;
		register int	r, g, b;

		y = *rip++;
		u = *gip++;
		v = *bip++;

		r = RCONV(y,u,v);
		g = GCONV(y,u,v);
		b = BCONV(y,u,v);

		*cp++ = CLIP(r);
		*cp++ = CLIP(g);
		*cp++ = CLIP(b);
	}
}

/*
 *			R I P P L E
 *
 *  Ripple all the scanlines down by one.
 *
 *  Barrel shift all the pointers down, with [0] going back to the top.
 */
ripple( array, num )
int	*array[];
int	num;
{
	register int	i;
	int		*temp;

	temp = array[0];
	for( i=0; i < num-1; i++ )
		array[i] = array[i+1];
	array[num-1] = temp;
}

/*
 *			F I L T E R 5
 *
 *  Apply a 5x5 image pyramid to the input scanline, taking every other
 *  input position to make an output.
 *
 *  Code is arranged so as to vectorize, on machines that can.
 */
filter5( op, lines, num )
int	*op;
int	*lines[];
int	num;
{
	register int	i;
	register int	j;
	register int	*a, *b, *c, *d, *e;

	a = lines[0];
	b = lines[1];
	c = lines[2];
	d = lines[3];
	e = lines[4];

#ifdef VECTORIZE
	/* This version vectorizes */
#	include "noalias.h"
	for( i=0; i < num; i++ )  {
		j = i*2;
		op[i] = (
			  a[j+0] + 2*a[j+1] + 4*a[j+2] + 2*a[j+3] +   a[j+4] +
			2*b[j+0] + 4*b[j+1] + 8*b[j+2] + 4*b[j+3] + 2*b[j+4] +
			4*c[j+0] + 8*c[j+1] +16*c[j+2] + 8*c[j+3] + 4*c[j+4] +
			2*d[j+0] + 4*d[j+1] + 8*d[j+2] + 4*d[j+3] + 2*d[j+4] +
			  e[j+0] + 2*e[j+1] + 4*e[j+2] + 2*e[j+3] +   e[j+4]
			) / 100;
	}
#else
	/* This version is better for non-vectorizing machines */
	for( i=0; i < num; i++ )  {
		op[i] = (
			  a[0] + 2*a[1] + 4*a[2] + 2*a[3] +   a[4] +
			2*b[0] + 4*b[1] + 8*b[2] + 4*b[3] + 2*b[4] +
			4*c[0] + 8*c[1] +16*c[2] + 8*c[3] + 4*c[4] +
			2*d[0] + 4*d[1] + 8*d[2] + 4*d[3] + 2*d[4] +
			  e[0] + 2*e[1] + 4*e[2] + 2*e[3] +   e[4]
			) / 100;
		a += 2;
		b += 2;
		c += 2;
		d += 2;
		e += 2;
	}
#endif
}


/*
 *			F I L T E R 3
 *
 *  Apply a 3x3 image pyramid to the input scanline, taking every other
 *  input position to make an output.
 *
 *  The filter coefficients are positioned so as to align the center
 *  of the filter with the same center used in filter5().
 */
filter3( op, lines, num )
int	*op;
int	*lines[];
int	num;
{
	register int	i;
	register int	j;
	register int	*b, *c, *d;

	b = lines[1];
	c = lines[2];
	d = lines[3];

#ifdef VECTORIZE
	/* This version vectorizes */
#	include "noalias.h"
	for( i=0; i < num; i++ )  {
		j = i*2;
		op[i] = (
			  b[j+1] + 2*b[j+2] +   b[j+3] +
			2*c[j+1] + 4*c[j+2] + 2*c[j+3] +
			  d[j+1] + 2*d[j+2] +   d[j+3]
			) / 16;
	}
#else
	/* This version is better for non-vectorizing machines */
	for( i=0; i < num; i++ )  {
		op[i] = (
			  b[1] + 2*b[2] +   b[3] +
			2*c[1] + 4*c[2] + 2*c[3] +
			  d[1] + 2*d[2] +   d[3]
			) / 16;
		b += 2;
		c += 2;
		d += 2;
	}
#endif
}
#endif	/* 0 */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
