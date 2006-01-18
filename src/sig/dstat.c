/*                         D S T A T . C
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
/** @file dstat.c
 *
 *  Compute statistics of double precision floats.
 *  Gives min, max, mode, median, mean, s.d., var, and skew.
 *
 *  Author -
 *	Phillip Dykstra
 *	18 Sep 1986
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
#include "common.h"



#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define	IBUFSIZE 1024		/* Max read size */
double	buf[IBUFSIZE];		/* Input buffer */

int	verbose = 0;

static char usage[] = "\
Usage: dstat [-v] [file.doubles]\n";

int main(int argc, char **argv)
{
	int	i, n;
	long	num_values;
	register double *bp;
	double	sum, sum2;
	double	max, min;
	double	mean, var;
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
			fprintf( stderr, "dstat: can't open \"%s\"\n", argv[1] );
			exit( 1 );
		}
		argv++;
		argc--;
	} else
		fp = stdin;

	/* check usage */
	if( argc > 1 || isatty(fileno(fp)) ) {
		fputs( usage, stderr );
		exit( 1 );
	}

	/*
	 * Find sum, min, max, mode.
	 */
	num_values = 0;
	sum = sum2 = 0;
#if defined(HUGE_VAL)
	min = HUGE_VAL;
	max = -HUGE_VAL;
#else
	min = HUGE;
	max = -HUGE;
#endif
	while( (n = fread(buf, sizeof(*buf), IBUFSIZE, fp)) > 0 ) {
		num_values += n;
		bp = &buf[0];
		for( i = 0; i < n; i++ ) {
			sum += *bp;
			sum2 += *bp * *bp;
			if( *bp < min )
				min = *bp;
			if( *bp > max )
				max = *bp;
			bp++;
		}
	}
	mean = sum/(double)num_values;
	var = sum2/(double)num_values - mean * mean;

	/*
	 * Display the results.
	 */
	printf( "Values  %14ld (%.0f x %.0f)\n", num_values,
		sqrt((double)num_values), sqrt((double)num_values) );
	printf( "Min     %14.6g\n", min );
	printf( "Max     %14.6g\n", max );
	printf( "Mean    %14.6g\n", mean );
	printf( "s.d.    %14.6g\n", sqrt( var ) );
	printf( "Var     %14.6g\n", var );

	return 0;
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
