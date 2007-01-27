/*                      P I X H A L V E . C
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
/** @file pixhalve.c
 *
 *  Reduce the resolution of a .pix file by one half in each direction,
 *  using a 5x5 pyramid filter.
 *
 *  As this tool is used primarily for preparing images for NTSC television,
 *  convert RGB to YUV, then apply different filter kernels;  use 3x3 for Y,
 *  5x5 for U and V.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"


static char	*file_name;
static FILE	*infp;

static int	fileinput = 0;		/* file or pipe on input? */
static int	autosize = 0;		/* !0 to autosize input */

static long int	file_width = 512L;	/* default input width */

static char usage[] = "\
Usage: pixhalve [-h] [-a]\n\
	[-s squaresize] [-w file_width] [-n file_height] [file.pix]\n";

void separate(register int *rop, register int *gop, register int *bop, register unsigned char *cp, long int num);
void combine(register unsigned char *cp, register int *rip, register int *gip, register int *bip, long int num);
void ripple(int **array, int num);
void filter3(int *op, int **lines, int num);
void filter5(int *op, int **lines, int num);

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
			file_width = 1024L;
			autosize = 0;
			break;
		case 's':
			/* square file size */
			file_width = atol(bu_optarg);
			autosize = 0;
			break;
		case 'w':
			file_width = atol(bu_optarg);
			autosize = 0;
			break;
		case 'n':
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
		infp = stdin;
	} else {
		file_name = argv[bu_optind];
		if( (infp = fopen(file_name, "r")) == NULL )  {
			perror(file_name);
			(void)fprintf( stderr,
				"pixhalve: cannot open \"%s\" for reading\n",
				file_name );
			return(0);
		}
		fileinput++;
	}

	if ( argc > ++bu_optind )
		(void)fprintf( stderr, "pixhalve: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int	*rlines[5];
int	*glines[5];
int	*blines[5];

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
	unsigned char	*inbuf;
	unsigned char	*outbuf;
	int	*rout, *gout, *bout;
	long int	out_width;
	long int	i;
	int	eof_seen;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	/* autosize input? */
	if( fileinput && autosize ) {
		unsigned long int	w, h;
		if( fb_common_file_size(&w, &h, file_name, 3) ) {
			file_width = (long)w;
		} else {
			fprintf(stderr, "pixhalve: unable to autosize\n");
		}
	}
	out_width = file_width/2;

	/* Allocate 1-scanline input & output buffers */
	inbuf = malloc( 3*file_width+8 );
	outbuf = malloc( 3*(out_width+2)+8 );

	/* Allocate 5 integer arrays for each color */
	/* each width+2 elements wide */
	for( i=0; i<5; i++ )  {
		rlines[i] = (int *)bu_calloc( (file_width+4)+1, sizeof(long), "rlines" );
		glines[i] = (int *)bu_calloc( (file_width+4)+1, sizeof(long), "glines" );
		blines[i] = (int *)bu_calloc( (file_width+4)+1, sizeof(long), "blines" );
	}

	/* Allocate an integer array for each color, for output */
	rout = (int *)bu_malloc( out_width * sizeof(long) + 8, "rout" );
	gout = (int *)bu_malloc( out_width * sizeof(long) + 8, "gout" );
	bout = (int *)bu_malloc( out_width * sizeof(long) + 8, "bout" );

	/*
	 *  Prime the pumps with 5 lines of image.
	 *  Repeat the bottom most line three times to generate a "fill"
	 *  line on the bottom.  This will have to be matched on the top.
	 */
	if( fread( inbuf, 3, file_width, infp ) != file_width )  {
		perror(file_name);
		fprintf(stderr, "pixhalve:  fread error\n");
		exit(1);
	}
	separate( &rlines[0][2], &glines[0][2], &blines[0][2], inbuf, file_width );
	separate( &rlines[1][2], &glines[1][2], &blines[1][2], inbuf, file_width );
	separate( &rlines[2][2], &glines[2][2], &blines[2][2], inbuf, file_width );
	for( i=3; i<5; i++ )  {
		if( fread( inbuf, 3, file_width, infp ) != file_width )  {
			perror(file_name);
			fprintf(stderr, "pixhalve:  fread error\n");
			exit(1);
		}
		separate( &rlines[i][2], &glines[i][2], &blines[i][2],
			inbuf, file_width );
	}

	eof_seen = 0;
	for(;;)  {
		filter3( rout, rlines, out_width );
		filter5( gout, glines, out_width );
		filter5( bout, blines, out_width );
		combine( outbuf, rout, gout, bout, out_width );
		if( fwrite( (void*)outbuf, 3, out_width, stdout ) != out_width )  {
			perror("stdout");
			exit(2);
		}

		/* Ripple down two scanlines, and acquire two more */
		if( fread( inbuf, 3, file_width, infp ) != file_width )  {
			if( eof_seen >= 2 )  break;
			/* EOF, repeat last line 2x for final output line */
			eof_seen++;
			/* Fall through */
		}
		ripple( rlines, 5 );
		ripple( glines, 5 );
		ripple( blines, 5 );
		separate( &rlines[4][2], &glines[4][2], &blines[4][2],
			inbuf, file_width );

		if( fread( inbuf, 3, file_width, infp ) != file_width )  {
			if( eof_seen >= 2 )  break;
			/* EOF, repeat last line 2x for final output line */
			eof_seen++;
			/* Fall through */
		}
		ripple( rlines, 5 );
		ripple( glines, 5 );
		ripple( blines, 5 );
		separate( &rlines[4][2], &glines[4][2], &blines[4][2],
			inbuf, file_width );
	}

	bu_free(rlines, "rlines");
	bu_free(glines, "glines");
	bu_free(blines, "blines");
	bu_free(rout, "rout");
	bu_free(gout, "gout");
	bu_free(bout, "bout");
}

/*
 *			S E P A R A T E
 *
 *  Unpack RGB byte tripples into three separate arrays of integers.
 *  The first and last pixels are replicated twice, to handle border effects.
 *
 *  Updated version:  the outputs are Y U V values, not R G B.
 */
void
separate(register int *rop, register int *gop, register int *bop, register unsigned char *cp, long int num)
					/* Y */
					/* U */
					/* V */


{
	register long int 	i;
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
void
combine(register unsigned char *cp, register int *rip, register int *gip, register int *bip, long int num)
{
	register long int 	i;

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
void
ripple(int **array, int num)
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
void
filter5(int *op, int **lines, int num)
{
	register int	i;
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
void
filter3(int *op, int **lines, int num)
{
	register int	i;
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
