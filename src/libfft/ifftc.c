/*
 *  Split Radix Decimation in Freq
 *  Inverse FFT C code generator.
 *
 *  Author -
 *	Phil Dykstra
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#include <stdio.h>
#include <math.h>

#include "./fft.h"

extern int irfft_adds, irfft_mults;

static char usage[] = "\
Usage: ifftc length > fftlength.c\n";

int
main(int argc, char **argv)
{
	double	x[4097];
	int	n, m;

	if( argc != 2 ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	n = atoi(argv[1]);
	m = log((double)n)/log(2.0) + 0.5;	/* careful truncation */

	ditsplit( x, n, m );
fprintf( stderr, "adds = %d, mults = %d\n", irfft_adds, irfft_mults );
	return(0);
}
