/*
 *		D - B W . C
 *
 *  Convert doubles to unsigned bytes.
 *
 *	% d-bw [-n || scale]
 *
 *	-n will normalize the data (scale -1.0 to +1.0
 *		between 0 and 255).
 *
 *  Phil Dykstra - 5 Nov 85.
 */
#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"		/* For atof, if math.h doesn't have it */

double	ibuf[512];
unsigned char	obuf[512];

static char usage[] = "\
Usage: d-bw [-n || scale] < doubles > unsigned_chars\n";

main( argc, argv )
int	argc;
char	**argv;
{
	int	i, num;
	double	scale;
	double	value;
	int	clip_high, clip_low;

	scale = 1.0;

	if( argc > 1 ) {
		if( strcmp( argv[1], "-n" ) == 0 )
			scale = 255.0;
		else
			scale = atof( argv[1] );
		argc--;
	}

	if( argc > 1 || scale == 0 || isatty(fileno(stdin)) ) {
		fputs( usage, stderr );
		exit( 1 );
	}

	clip_high = clip_low = 0;

	while( (num = fread( &ibuf[0], sizeof( ibuf[0] ), 512, stdin)) > 0 ) {
		for( i = 0; i < num; i++ ) {
			value = ibuf[i] * scale;
			if( value > 255.0 ) {
				obuf[i] = 255;
				clip_high++;
			} else if( value < 0.0 ) {
				obuf[i] = 0;
				clip_low++;
			} else
				obuf[i] = (int)value;
		}

		fwrite( &obuf[0], sizeof( obuf[0] ), num, stdout );
	}

	if( clip_low != 0 || clip_high != 0 )
		fprintf( stderr, "Warning: Clipped %d high, %d low\n",
			clip_high, clip_low );
}
