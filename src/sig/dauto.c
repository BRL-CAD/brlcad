/*
 *		D A U T O . C
 *
 *  Compute the autocorrelation function of doubles.
 *  Given L elements at a time, data[0..L-1], we estimate
 *  the autocorrelation at lag 0, trough lag L-1, r[0..L-1].
 *  The first value is based on L samples, the last on only one.
 *  Zeros are assumed outside of the range of an input record.
 */
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "machine.h"

double	*data;			/* Input buffer */
double	*r;			/* autocor output */
double	*weight;		/* weights to unbias estimation */

static char usage[] = "\
Usage: dauto [window_size (512)] < doubles\n";

int main(int argc, char **argv)
{
	int	i, j, n, L;
	register double *dp1, *dp2;

	if( isatty(fileno(stdin)) || isatty(fileno(stdout)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	L = (argc > 1) ? atoi(argv[1]) : 512;
	data = (double *)calloc( L, sizeof(double) );
	r = (double *)calloc( L, sizeof(double) );
	weight = (double *)calloc( L, sizeof(double) );

	for( i = 0; i < L; i++ ) {
		weight[i] = 1.0 / (double)(L-i);
	}

	while( !feof( stdin ) ) {
		n = fread( data, sizeof(*data), L, stdin );
		if( n <= 0 )
			break;
		if( n < L )
			bzero( (char *)&data[n], (L-n)*sizeof(*data) );

		for( i = 0; i < L; i++ ) {
			r[i] = 0;
			dp1 = &data[0];
			dp2 = &data[i];
			for( j = L-i; j > 0; j-- ) {
				r[i] += *dp1++ * *dp2++;
			}
		}

		/* unbias the estimation */
		for( i = 0; i < L; i++ ) {
			r[i] *= weight[i];
		}

		fwrite( r, sizeof(*r), L, stdout );
	}

	return 0;
}
