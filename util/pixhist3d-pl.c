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
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

unsigned char bin[128][128][16];

struct	pix_element {
	unsigned char red, green, blue;
};

static char *Usage = "usage: pixhist3d-pl < file.pix | tplot\n";

main( argc, argv )
int argc;
char **argv;
{
	int	n, x;
	struct pix_element scan[512];
	unsigned char bmask;
	
	if( argc > 1 || isatty(fileno(stdin)) ) {
		fputs( Usage, stderr );
		exit( 1 );
	}

	/* Initialize plot */
	space3( 0, 0, 0, 128, 128, 128 );
	openpl();
	color( 255, 0, 0 );
	line3( 0, 0, 0, 127, 0, 0 );
	color( 0, 255, 0 );
	line3( 0, 0, 0, 0, 127, 0 );
	color( 0, 0, 255 );
	line3( 0, 0, 0, 0, 0, 127 );
	color( 255, 255, 255 );

	while( (n = fread(&scan[0], sizeof(*scan), 512, stdin)) > 0 ) {
		for( x = 0; x < n; x++ ) {
			bmask = 1 << ((scan[x].blue >> 1) & 7);
			if( (bin[ scan[x].red>>1 ][ scan[x].green>>1 ][ scan[x].blue>>4 ] & bmask) == 0 ) {
				/* New color: plot it and mark it */
				point3( scan[x].red>>1, scan[x].green>>1, scan[x].blue>>1 );
				bin[ scan[x].red>>1 ][ scan[x].green>>1 ][ scan[x].blue>>4 ] |= bmask;
			}
		}
	}
}
