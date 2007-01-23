/*                       Y U V - P I X . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file yuv-pix.c
 *
 *  Convert a .yuv file to a .pix file, i.e. in CCIR-601 format.
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

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"


static char	*file_name;
static int	infd;

static int	fileinput = 0;		/* file or pipe on input? */
static int	autosize = 0;		/* !0 to autosize input */

static long int	file_width = 720L;	/* default input width */
static long int	file_height = 485L;	/* default input height */

void ab_rgb_to_yuv(unsigned char *yuv_buf, unsigned char *rgb_buf, long int len);
void ab_yuv_to_rgb(unsigned char *rgb_buf, unsigned char *yuv_buf, long int len);

static char usage[] = "\
Usage: yuv-pix [-h] [-a]\n\
	[-s squaresize] [-w file_width] [-n file_height] [file.yuv] > file.pix\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt( argc, argv, "ahs:w:n:" )) != EOF )  {
		switch( c )  {
		case 'a':
			autosize = 1;
			break;
		case 'h':
			/* high-res */
			file_height = file_width = 1024L;
			autosize = 0;
			break;
		case 's':
			/* square file size */
			file_height = file_width = atol(bu_optarg);
			autosize = 0;
			break;
		case 'w':
			file_width = atol(bu_optarg);
			autosize = 0;
			break;
		case 'n':
			file_height = atol(bu_optarg);
			autosize = 0;
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( bu_optind >= argc )  {
		if( isatty(fileno(stdin)) )
			return(0);
		file_name = "-";
		infd = fileno(stdin);
	} else {
		file_name = argv[bu_optind];
		if( (infd = open(file_name, 0)) < 0 )  {
			perror(file_name);
			(void)fprintf( stderr,
				"yuv-pix: cannot open \"%s\" for reading\n",
				file_name );
			return(0);
		}
		fileinput++;
	}

	if ( argc > ++bu_optind )
		(void)fprintf( stderr, "yuv-pix: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
	unsigned char	*inbuf;
	unsigned char	*outbuf;
	long int	y;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	/* autosize input? */
	if( fileinput && autosize ) {
		unsigned long int	w, h;
		if( fb_common_file_size(&w, &h, file_name, 2) ) {
			file_width = (long)w;
			file_height = (long)h;
		} else {
			fprintf(stderr, "yuv-pix: unable to autosize\n");
		}
	}

	/* Allocate full size buffers for input and output */
	inbuf = bu_malloc( 2*file_width*file_height+8, "inbuf" );
	outbuf = bu_malloc( 3*file_width*file_height+8, "outbuf" );

	if( bu_mread( infd, inbuf, 2*file_width*file_height ) < 2*file_width*file_height )  {
	    perror("READ ERROR");
	    fprintf(stderr, "yuv-pix: short input file, aborting\n");
	    exit(1);
	}

	for( y = 0; y < file_height; y++ )  {
		ab_yuv_to_rgb(
			&outbuf[y*file_width*3],
			&inbuf[(file_height-1-y)*file_width*2],
			file_width );
	}

	if( write( 1, (void *)outbuf, 3*file_width*file_height ) < 3*file_width*file_height )  {
		perror("stdout");
		fprintf(stderr, "yuv-pix: output write error, aborting\n");
		exit(2);
	}

	bu_free(inbuf, "inbuf");
	bu_free(outbuf, "outbuf");

	return 0;
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
ab_rgb_to_yuv(unsigned char *yuv_buf, unsigned char *rgb_buf, long int len)
{
	register unsigned char *cp;
	register double	*yp, *up, *vp;
	register long int	i;
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
ab_yuv_to_rgb(unsigned char *rgb_buf, unsigned char *yuv_buf, long int len)
{
	register unsigned char *rgbp;
	register unsigned char *yuvp;
	register double	y;
	register double	u = 0.0;
	register double	v;
	register long int	pixel;
	long int		last;

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


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
