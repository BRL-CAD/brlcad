/*
 *		P I X - S P M . C
 *
 *  Turn a pix file into sphere map data.
 *
 *  Phil Dykstra
 *  12 Aug 1986
 */
#include "common.h"



#include <stdio.h>
#include <stdlib.h>

#include "machine.h"
#include "fb.h"
#include "spm.h"

static	char *Usage = "usage: pix-spm file.pix size > file.spm\n";

int
main(int argc, char **argv)
{
	int	size;
	spm_map_t *mp;

	if( argc != 3 ) {
		fputs( Usage, stderr );
		exit( 1 );
	}

	size = atoi( argv[2] );
	mp = spm_init( size, sizeof(RGBpixel) );
	spm_px_load( mp, argv[1], size, size );
	spm_save( mp, "-" );

	return 0;
}
