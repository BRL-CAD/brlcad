/*
 *  Split Radix Decimation in Freq Inverse FFT C code generator.
 */
extern int irfft_adds, irfft_mults;

#include <stdio.h>
#include <math.h>

static char usage[] = "\
Usage: ifftc length > fftlength.c\n";

main( argc, argv )
int	argc;
char	**argv;
{
	double	x[4097];
	int	i, n, m;

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
