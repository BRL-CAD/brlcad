/*
 *		B W R E C T . C
 *
 * Remove a portion of a potentially huge .bw file.
 *
 *  Author -
 *	Phillip Dykstra
 *	2 Oct 1985
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#include "machine.h"

int	xnum, ynum;		/* Number of pixels in new map */
int	xorig, yorig;		/* Bottom left corner to extract from */
int	linelen;
char	*buf;			/* output scanline buffer, malloc'd */

int
main(int argc, char **argv)
{
	FILE	*ifp, *ofp;
	int	row;
	long	offset;

	if (argc < 3) {
		printf("usage: bwrect infile outfile (I prompt!)\n");
		exit( 1 );
	}
	if ((ifp = fopen(argv[1], "r")) == NULL) {
		printf("pixrect: can't open %s\n", argv[1]);
		exit( 2 );
	}
	if ((ofp = fopen(argv[2], "w")) == NULL) {
		printf("pixrect: can't open %s\n", argv[1]);
		exit( 3 );
	}

	/* Get info */
	printf( "Area to extract (x, y) in pixels " );
	scanf( "%d%d", &xnum, &ynum );
	printf( "Origin to extract from (0,0 is lower left) " );
	scanf( "%d%d", &xorig, &yorig );
	printf( "Scan line length of input file " );
	scanf( "%d", &linelen );

	buf = malloc( xnum );

	/* Move all points */
	for (row = 0+yorig; row < ynum+yorig; row++) {
		offset = row * linelen + xorig;
		fseek(ifp, offset, 0);
		fread(buf, sizeof(*buf), xnum, ifp);
		fwrite(buf, sizeof(*buf), xnum, ofp);
	}
	return 0;
}
