/*
 *		D A M D F . C
 *
 *  Average Magnitude Difference Function
 *  (Experimental: for pitch extraction)
 */
#include "common.h"



#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include "machine.h"

#define	BSIZE	2048		/* Must be AT LEAST 2*Points in spectrum */
double	data[BSIZE];		/* Input buffer */
double	r[BSIZE];

static char usage[] = "\
Usage: damdf [window_size (512)] < doubles\n";

int main(int argc, char **argv)
{
	int	i, j, n, L;
	register double *dp1, *dp2;
	register double	d;

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

		for( i = 0; i < L; i++ ) {
			r[i] = 0;
			dp1 = &data[0];
			dp2 = &data[i];
			for( j = L-i; j > 0; j-- ) {
				d = *dp1++ - *dp2++;
				if( d > 0 )
					r[i] += d;
				else
					r[i] -= d;
			}
		}
		for( i = 0; i < L; i++ ) {
			r[i] = 1024 - r[i];
		}
		fwrite( r, sizeof(*r), L, stdout );
	}

	return 0;
}
