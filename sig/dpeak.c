/*
 *		D P E A K . C
 *
 *  An EXPERIMENTAL routine to find the N peak values in data set.
 *  Where "peak" means negative second difference, local maximum.
 */
#include "conf.h"

#ifdef USE_STRING_H
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

int	numpeaks;
struct	peaks {
	int	sample;
	double	value;
} peaks[BSIZE];

static char usage[] = "\
Usage: dpeak [window_size (512)] < doubles\n";

void	dumpmax();

int main( argc, argv )
int	argc;
char	**argv;
{
	int	i, n, L;
	double	last1, last2;

	if( isatty(fileno(stdin)) /*|| isatty(fileno(stdout))*/ ) {
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

		last2 = last1 = 0;
		numpeaks = 0;
		for( i = 0; i < L; i++ ) {
			if( data[i] == last1 )
				continue;
			if( (data[i] < last1) && (last2 < last1) && i > 5 ) {
				/* PEAK */
				/*printf("sample %d, value = %f\n", i-1, last1 );*/
				peaks[numpeaks].sample = i-1;
				peaks[numpeaks].value = last1;
				numpeaks++;
			}
			last2 = last1;
			last1 = data[i];
		}
		dumpmax();
	}

	return 0;
}

#define	NUMPEAKS 1

void
dumpmax()
{
	int	i, n;
	struct	peaks max;
	double	d;

	for( n = 0; n < NUMPEAKS; n++ ) {
		max.value = -1000000;
		max.sample = -1;
		for( i = 0; i < numpeaks; i++ ) {
			if( peaks[i].value > max.value ) {
				max = peaks[i];
				peaks[i].value = -1000000;
			}
		}
/*
		printf( "Sample %3d: %f\n", max.sample, max.value );
*/
		d = max.sample;
		fwrite(&d, sizeof(d), 1, stdout);
	}
}
