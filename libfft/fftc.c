/*
 *	Split Radix Decimation in Time FFT C code generator.
 */
extern int rfft_adds, rfft_mults;

#include <stdio.h>
#include <math.h>

static char usage[] = "\
Usage: fftc length > fftlength.c\n";

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

	splitdit( x, n, m );
(void)fprintf( stderr, "adds = %d, mults = %d\n", rfft_adds, rfft_mults );
	return(0);
}
