/*
 *		D A U T O . C
 *
 *  Compute the autocorrelation function of doubles.
 */
#include "conf.h"

#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"

#define	BSIZE	2048		/* Must be AT LEAST 2*Points in spectrum */
double	data[BSIZE];		/* Input buffer */
int	clip[BSIZE];		/* clipped buffer */
int	out[BSIZE];
double	r[BSIZE];

static char usage[] = "\
Usage: dauto2 [window_size (512)] < doubles\n";

int main( argc, argv )
int	argc;
char	**argv;
{
	int	i, j, n, L;
	register int *dp1, *dp2;
	double	max1, max2, m, m2;

	if( isatty(fileno(stdin)) || isatty(fileno(stdout)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	L = (argc > 1) ? atoi(argv[1]) : 512;

	while( !feof( stdin ) ) {
		n = fread( data, sizeof(*data), L, stdin );
		if( n <= 0 )
			break;
		if( n < L )
			bzero( (char *)&data[n], (L-n)*sizeof(*data) );

		max1 = data[0];
		for( i = 1; i < L/2; i++ ) {
			m = data[i] >= 0 ? data[i] : - data[i];
			if( m > max1 ) max1 = m;
		}
		max2 = data[2*L/3];
		for( i = 2*L/3+1; i < L; i++ ) {
			m = data[i] >= 0 ? data[i] : - data[i];
			if( m > max2 ) max2 = m;
		}
		m = max1 > max2 ? max2 : max1;
		m *= 0.6;
		m2 = -m;
		for( i = 0; i < L; i++ ) {
			if( data[i] > m )
				clip[i] = 1;
			else if( data[i] < m2 )
				clip[i] = -1;
			else
				clip[i] = 0;
		}
		for( i = 0; i < L; i++ ) {
			out[i] = 0;
			dp1 = &clip[0];
			dp2 = &clip[i];
			for( j = L-i; j > 0; j-- ) {
				if( *dp1 == 0 || *dp2 == 0 )
					continue;
				else if( *dp1 == *dp2 )
					out[i]++;
				else
					out[i]--;
			}
		}
		for( i = 0; i < L; i++ ) {
			r[i] = out[i];
		}
		fwrite( r, sizeof(*r), L, stdout );
	}

	return 0;
}
