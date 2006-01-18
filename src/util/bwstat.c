/*                        B W S T A T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2006 United States Government as represented by
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
/** @file bwstat.c
 *
 * Compute statistics of pixels in a black and white (BW) file.
 * Gives min, max, mode, median, mean, s.d., var, and skew.
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
#include <stdlib.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <unistd.h>
#include <math.h>

#define	IBUFSIZE 1024		/* Max read size in pixels */
unsigned char buf[IBUFSIZE];	/* Input buffer */

int	verbose = 0;
long	bin[256];		/* Histogram bins */

void	show_hist(long int *bin, int sum);

static char *Usage = "usage: bwstat [-v] [file.bw]\n";

int
main(int argc, char **argv)
{
	int	i, n;
	double	d;
	long	num_pixels;
	register unsigned char *bp;
	long	sum, partial_sum;
	int	max, min, mode, median;
	double	mean, var, skew;
	FILE	*fp;

	/* check for verbose flag */
	if( argc > 1 && strcmp(argv[1], "-v") == 0 ) {
		verbose++;
		argv++;
		argc--;
	}

	/* look for optional input file */
	if( argc > 1 ) {
		if( (fp = fopen(argv[1],"r")) == 0 ) {
			fprintf( stderr, "bwstat: can't open \"%s\"\n", argv[1] );
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
	while( (n = fread(buf, sizeof(*buf), IBUFSIZE, fp)) > 0 ) {
		num_pixels += n;
		bp = &buf[0];
		for( i = 0; i < n; i++ )
			bin[ *bp++ ]++;
	}

	/*
	 * Find sum, min, max, mode.
	 */
	sum = 0;
	min = 256;
	max = -1;
	mode = 0;
	for( i = 0; i < 256; i++ ) {
		sum += i * bin[i];
		if( i < min && bin[i] != 0 )
			min = i;
		if( i > max && bin[i] != 0 )
			max = i;
		if( bin[i] > bin[mode] ) {
			mode = i;
		}
	}
	mean = (double)sum/(double)num_pixels;

	/*
	 * Now do a second pass to compute median,
	 *  variance and skew.
	 */
	partial_sum = 0;
	median = 0;
	var = skew = 0.0;
	for( i = 0; i < 256; i++ ) {
		if( partial_sum < sum/2.0 ) {
			partial_sum += i * bin[i];
			median = i;
		}
		d = (double)i - mean;
		var += bin[i] * d * d;
		skew += bin[i] * d * d * d;
	}
	var /= (double)num_pixels;
	skew /= (double)num_pixels;

	/*
	 * Display the results.
	 */
	printf( "Pixels  %14ld (%.0f x %.0f)\n", num_pixels,
		sqrt((double)num_pixels), sqrt((double)num_pixels) );
	printf( "Min     %14d\n", min );
	printf( "Max     %14d\n", max );
	printf( "Mode    %14d (%ld pixels)\n", mode, bin[mode] );
	printf( "Median  %14d\n", median );
	printf( "Mean    %14.3f\n", mean );
	printf( "s.d.    %14.3f\n", sqrt( var ) );
	printf( "Var     %14.3f\n", var );
	printf( "Skew    %14.3f\n", skew );

	if( verbose )
		show_hist( bin, sum );

	return 0;
}

/*
 * Display the histogram values.
 */
void
show_hist(long int *bin, int sum)
{
	int	i;

	printf( "Histogram:\n" );

	for( i = 0; i < 256; i++ ) {
		printf( "%3d: %10ld (%10f)\n", i, bin[i], (float)bin[i]/sum * 100.0 );
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
