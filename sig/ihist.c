#ifdef HAVE_CONFIG_H
# include "config.h"
#endif



#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>

long	bits[16];
long	values[65536];
long	*zerop;
short	ibuf[1024];

int	verbose = 0;

static char usage[] = "\
Usage: ihist [-v] < shorts\n";

int main(int argc, char **argv)
{
	register long i, bit;
	int	n;
	int	max, min;
	long	num, levels=0;

	while( argc > 1 ) {
		if( strcmp( argv[1], "-v" ) == 0 ) {
			verbose++;
		} else
			break;
		argc--;
		argv++;
	}
	if( argc > 1 || isatty(fileno(stdin)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	num = 0;
	zerop = &values[32768];
	while( (n = fread(ibuf, 2, 1024, stdin)) > 0 ) {
		num += n;
		for( i = 0; i < n; i++ ) {
			zerop[ ibuf[i] ] ++;
		}
	}
	printf( "%ld values\n", num );
	max = 32767;
	while( zerop[max] == 0 )
		max--;
	min = -32768;
	while( zerop[min] == 0 )
		min++;
	printf( "Max = %d\n", max );
	printf( "Min = %d\n", min );

	printf( "Bits\n" );
	for( i = -32768; i < 32768; i++ ) {
		if( zerop[i] == 0 )
			continue;
		levels++;
		for( bit = 0; bit < 16; bit++ ) {
			if( i & (1<<bit) )
				bits[ bit ] += zerop[i];
		}
	}
	for( bit = 0; bit < 16; bit++ ) {
		printf( "%8ld: %10ld (%f)\n",
			bit, bits[bit], (double)bits[bit]/(double)num );
	}

	printf( "%ld levels used (%04.2f %%)\n", levels, levels/65535.0 * 100.0 );
	if( verbose ) {
		for( i = -32768; i < 32768; i++ ) {
			if( zerop[i] ) {
				printf( "%ld %ld\n", i, zerop[i] );
			}
		}
	}

	return 0;
}
