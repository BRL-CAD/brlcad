/*
 *		F - D . C
 *
 *  Convert floats to doubles.
 *
 *	% f-d [-n || scale]
 *
 *	-n will normalize the data (scale -1.0 to +1.0
 *		between -1.0 and +1.0 in this case!).
 *
 *  Phil Dykstra - 5 Nov 85.
 */
#include "common.h"



#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include <stdio.h>
#include <math.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h> /* for atof() */
#endif


#include "machine.h"

float	ibuf[512];
double	obuf[512];

static char usage[] = "\
Usage: f-d [-n || scale] < floats > doubles\n";

int main(int argc, char **argv)
{
	int	i, num;
	double	scale;

	scale = 1.0;

	if( argc > 1 ) {
		if( strcmp( argv[1], "-n" ) == 0 )
			scale = 1.0;
		else
			scale = atof( argv[1] );
		argc--;
	}

	if( argc > 1 || scale == 0 || isatty(fileno(stdin)) || isatty(fileno(stdout)) ) {
		fputs( usage, stderr );
		exit( 1 );
	}

	while( (num = fread( &ibuf[0], sizeof( ibuf[0] ), 512, stdin)) > 0 ) {
		if( scale != 1.0 ) {
			for( i = 0; i < num; i++ )
				obuf[i] = ibuf[i] * scale;
		} else {
			for( i = 0; i < num; i++ )
				obuf[i] = ibuf[i];
		}

		fwrite( &obuf[0], sizeof( obuf[0] ), num, stdout );
	}

	return 0;
}
