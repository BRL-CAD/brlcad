/* double to ascii */
#include "conf.h"

#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

int	nflag = 0;

static char usage[] = "\
Usage: d-a [-n] < doubles > ascii\n";

int main( argc, argv )
int argc; char **argv;
{
	double	d;

	while( argc > 1 ) {
		if( strcmp( argv[1], "-n" ) == 0 )
			nflag++;
		else
			break;
		argc--;
		argv++;
	}
	if( argc > 1 || isatty(fileno(stdin)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	if( nflag ) {
		long	n;
		n = 0;
		while( fread(&d, sizeof(d), 1, stdin) == 1 ) {
			printf( "%ld %9g\n", n++, d );
		}
	} else {
		while( fread(&d, sizeof(d), 1, stdin) == 1 ) {
			printf( "%9g\n", d );
		}
	}
	return 0;
}

