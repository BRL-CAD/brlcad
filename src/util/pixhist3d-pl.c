/*
 *		P I X H I S T 3 D - P L . C
 *
 * RGB color space utilization to unix plot.
 *  Plots a point for each unique RGB value found in a pix file.
 *  Resolution is 128 steps along each color axis.
 *
 * Must be compiled with BRL System V plot(3) library.
 *
 *  Author -
 *	Phillip Dykstra
 *	19 June 1986
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1986-2004 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <unistd.h>
#include "common.h"


#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "plot3.h"

FILE	*fp;

unsigned char bin[128][128][16];

struct	pix_element {
	unsigned char red, green, blue;
};

static char *Usage = "usage: pixhist3d-pl [file.pix] | plot\n";

int
main(int argc, char **argv)
{
	int	n, x;
	struct pix_element scan[512];
	unsigned char bmask;
	
	if( argc > 1 ) {
		if( (fp = fopen(argv[1], "r")) == NULL ) {
			fprintf( stderr, "pixhist3d-pl: can't open \"%s\"\n", argv[1] );
			fprintf( stderr, Usage );
			exit( 1 );
		}
	} else
		fp = stdin;

	if( argc > 2 || isatty(fileno(fp)) ) {
		fputs( Usage, stderr );
		exit( 2 );
	}

	/* Initialize plot */
	pl_3space( stdout, 0, 0, 0, 128, 128, 128 );
	pl_color( stdout, 255, 0, 0 );
	pl_3line( stdout, 0, 0, 0, 127, 0, 0 );
	pl_color( stdout, 0, 255, 0 );
	pl_3line( stdout, 0, 0, 0, 0, 127, 0 );
	pl_color( stdout, 0, 0, 255 );
	pl_3line( stdout, 0, 0, 0, 0, 0, 127 );
	pl_color( stdout, 255, 255, 255 );

	while( (n = fread(&scan[0], sizeof(*scan), 512, fp)) > 0 ) {
		for( x = 0; x < n; x++ ) {
			bmask = 1 << ((scan[x].blue >> 1) & 7);
			if( (bin[ scan[x].red>>1 ][ scan[x].green>>1 ][ scan[x].blue>>4 ] & bmask) == 0 ) {
				/* New color: plot it and mark it */
				pl_3point( stdout, scan[x].red>>1, scan[x].green>>1, scan[x].blue>>1 );
				bin[ scan[x].red>>1 ][ scan[x].green>>1 ][ scan[x].blue>>4 ] |= bmask;
			}
		}
	}
	return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
