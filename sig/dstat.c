/*
 *		D S T A T . C
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1986-2004 by the United States Army.
 *	All rights reserved.
 */
#include "conf.h"

#ifdef USE_STRING_H
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

int main( argc, argv )
int argc;
char **argv;
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
