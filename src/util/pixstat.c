/*                       P I X S T A T . C
 * BRL-CAD
 *
 * Copyright (C) 1986-2005 United States Government as represented by
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
/** @file pixstat.c
 *
 * Compute statistics of pixels in a PIX file.
 * Gives min, max, mode, median, mean, s.d., var, and skew
 * for each color.
 * Will optionally dump the histogram of values.
 *
 * We compute these from a histogram which is a real win for
 * discrete values of small range.  Statistics can then be
 * calculated on the lump sums in the bins rather than individual
 * samples.  Also only one pass is required over the input file.
 * (lastly, some statistics such as mode and median, require the
 *  histogram)
 *
 *  Author -
 *	Phillip Dykstra
 *	6 Aug 1986
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

#define	IBUFSIZE 3*1024		/* Max read size in rgb pixels */
unsigned char buf[IBUFSIZE];	/* Input buffer */

int	verbose = 0;

/* our statistics pool */
struct	{
	long	bin[256];
	int	max, min, mode, median;
	double	mean, var, skew;
	long	sum, partial_sum;
} r, g, b;

void	show_hist(void);

static char *Usage = "usage: pixstat [-v] [file.pix]\n";

int
main(int argc, char **argv)
{
	int	i, n, num;
	double	d;
	long	num_pixels;
	register unsigned char *bp;
	FILE	*fp;

	/* check for verbose flag */
	if( argc > 1 && strcmp(argv[1], "-v") == 0 ) {
		verbose++;
		argv++;
		argc--;
	}

	/* check for optional input file */
	if( argc > 1 ) {
		if( (fp = fopen(argv[1],"r")) == 0 ) {
			fprintf( stderr, "pixstat: can't open \"%s\"\n", argv[1] );
			exit( 1 );
		}
		argv++;
		argc--;
	} else
		fp = stdin;

	/* check usage */
	if( argc > 1 || isatty(fileno(fp)) ) {
		fputs( Usage, stderr );
		exit( 1 );
	}

	/*
	 * Build the histogram.
	 */
	num_pixels = 0;
	while( (n = fread(&buf[0], sizeof(*buf), IBUFSIZE, fp)) > 0 ) {
		num = n/3;
		num_pixels += num;
		bp = &buf[0];
		for( i = 0; i < num; i++ ) {
			r.bin[ *bp++ ]++;
			g.bin[ *bp++ ]++;
			b.bin[ *bp++ ]++;
		}
	}

	/*
	 * Find sum, min, max, mode.
	 */
	r.sum = g.sum = b.sum = 0;
	r.min = g.min = b.min = 256;
	r.max = g.max = b.max = -1;
	r.mode = g.mode = b.mode = 0;
	for( i = 0; i < 256; i++ ) {
		/* sum */
		r.sum += i * r.bin[i];
		g.sum += i * g.bin[i];
		b.sum += i * b.bin[i];
		/* min */
		if( i < r.min && r.bin[i] != 0 )
			r.min = i;
		if( i < g.min && g.bin[i] != 0 )
			g.min = i;
		if( i < b.min && b.bin[i] != 0 )
			b.min = i;
		/* max */
		if( i > r.max && r.bin[i] != 0 )
			r.max = i;
		if( i > g.max && g.bin[i] != 0 )
			g.max = i;
		if( i > b.max && b.bin[i] != 0 )
			b.max = i;
		/* mode */
		if( r.bin[i] > r.bin[r.mode] )
			r.mode = i;
		if( g.bin[i] > g.bin[g.mode] )
			g.mode = i;
		if( b.bin[i] > b.bin[b.mode] )
			b.mode = i;
	}
	r.mean = (double)r.sum/(double)num_pixels;
	g.mean = (double)g.sum/(double)num_pixels;
	b.mean = (double)b.sum/(double)num_pixels;

	/*
	 * Now do a second pass to compute median,
	 *  variance and skew.
	 */
	for( i = 0; i < 256; i++ ) {
		/* median */
		if( r.partial_sum < r.sum/2.0 ) {
			r.partial_sum += i * r.bin[i];
			r.median = i;
		}
		if( g.partial_sum < g.sum/2.0 ) {
			g.partial_sum += i * g.bin[i];
			g.median = i;
		}
		if( b.partial_sum < b.sum/2.0 ) {
			b.partial_sum += i * b.bin[i];
			b.median = i;
		}
		/* var and skew */
		d = (double)i - r.mean;
		r.var += r.bin[i] * d * d;
		r.skew += r.bin[i] * d * d * d;
		d = (double)i - g.mean;
		g.var += g.bin[i] * d * d;
		g.skew += g.bin[i] * d * d * d;
		d = (double)i - b.mean;
		b.var += b.bin[i] * d * d;
		b.skew += b.bin[i] * d * d * d;
	}
	r.var /= (double)num_pixels;
	g.var /= (double)num_pixels;
	b.var /= (double)num_pixels;
	r.skew /= (double)num_pixels;
	g.skew /= (double)num_pixels;
	b.skew /= (double)num_pixels;

	/*
	 * Display the results.
	 */
	printf( "Pixels  %14ld (%.0f x %.0f)\n", num_pixels,
		sqrt((double)num_pixels), sqrt((double)num_pixels) );
	printf( "Min     %14d%14d%14d\n", r.min, g.min, b.min );
	printf( "Max     %14d%14d%14d\n", r.max, g.max, b.max );
	printf( "Mode    %14d%14d%14d\n", r.mode, g.mode, b.mode );
	printf( "Num@mode%14ld%14ld%14ld\n", r.bin[r.mode], g.bin[g.mode], b.bin[b.mode] );
	printf( "Median  %14d%14d%14d\n", r.median, g.median, b.median );
	printf( "Mean    %14.3f%14.3f%14.3f\n", r.mean, g.mean, b.mean );
	printf( "s.d.    %14.3f%14.3f%14.3f\n", sqrt(r.var), sqrt(g.var), sqrt(b.var) );
	printf( "Var     %14.3f%14.3f%14.3f\n", r.var, g.var, b.var );
	printf( "Skew    %14.3f%14.3f%14.3f\n", r.skew, g.skew, b.skew );

	if( verbose )
		show_hist();
	return 0;
}

/*
 * Display the histogram values.
 */
void
show_hist(void)
{
	int	i;

	printf( "Histogram:\n" );

	for( i = 0; i < 256; i++ ) {
		printf( "%3d: %10ld %10ld %10ld\n",
			i, r.bin[i], g.bin[i], b.bin[i] );
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
