/*
 *			F B S C A N P L O T . C
 *
 *  Plot an RGB profile of a framebuffer scanline.
 *
 *  Author -
 *	Phillip Dykstra
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
#include "fb.h"

RGBpixel scan[1024+2];
RGBpixel outline[1024], backgnd[1024];

extern int	getopt();
extern char	*optarg;
extern int	optind;

int yline;		/* line to plot */
int width = 0;		/* framebuffer width */
int verbose = 0;	/* output scanline values to stdout */
int fb_overlay = 0;	/* plot on background, else black with grid */
int reverse = 0;	/* highlight chosen line by inverting it */
char *outframebuffer = NULL;
FBIO *fbp, *fboutp;

char usage[] = "\
Usage: fbscanplot [-h] [-v] [-o] [-r]\n\
	[-W scr_width] [-F outframebuffer] yline\n";

get_args( argc, argv )
register char **argv;
{
	register int c;

	while ( (c = getopt( argc, argv, "vhorW:F:" )) != EOF ) {
		switch( c )  {
		case 'v':
			verbose++;
			break;
		case 'h':
			width = 1024;
			break;
		case 'o':
			fb_overlay++;
			break;
		case 'r':
			reverse++;
			break;
		case 'W':
			width = atoi(optarg);
			break;
		case 'F':
			outframebuffer = optarg;
			break;
		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )
		return(0);
	else
		yline = atoi( argv[optind] );

	if ( argc > ++optind )
		return(0);	/* too many args */

	return(1);		/* OK */
}

main( argc, argv )
int argc; char **argv;
{
	register unsigned char *ip, *op;
	register int y;
	int	x;
	int	yoffset;	/* position of plot on screen */

	if ( !get_args( argc, argv ) )  {
		fprintf( stderr, usage );
		exit( 1 );
	}

	if( (fbp = fb_open( NULL, width, width )) == NULL )
		exit( 2 );

	if( outframebuffer != NULL ) {
		if( (fboutp = fb_open( outframebuffer, width, width )) == NULL )
			exit( 3 );
	} else
		fboutp = fbp;

	width = fb_getwidth(fbp);
	fb_read( fbp, 0, yline, &scan[1][RED], width );

	/* extend the edges */
	scan[0][RED] = scan[1][RED];
	scan[0][GRN] = scan[1][GRN];
	scan[0][BLU] = scan[1][BLU];
	scan[width+1][RED] = scan[width][RED];
	scan[width+1][GRN] = scan[width][GRN];
	scan[width+1][BLU] = scan[width][BLU];

	/* figure out where to put it on the screen */
	if( fb_overlay == 0 && fboutp == fbp && yline < fb_getheight(fbp)/2 ) {
		yoffset = fb_getheight(fbp) - 256;
		if( yoffset <= yline )
			yoffset = 0;
	} else
		yoffset = 0;

	if( reverse ) {
		/* Output the negative of the chosen line */
		for( x = 0; x < width; x++ ) {
			outline[x][RED] = 255 - scan[x+1][RED];
			outline[x][GRN] = 255 - scan[x+1][GRN];
			outline[x][BLU] = 255 - scan[x+1][BLU];
		}
		fb_write( fbp, 0, yline, outline, width );
	}

	for( y = 0; y < 256; y++ ) {
		if( fb_overlay )
			fb_read( fboutp, 0, y+yoffset, backgnd, width );

		ip = &scan[1][RED];
		op = &outline[0][RED];
		for( x = 0; x < width; x++, op += 3, ip += 3 ) {
			if( y > ip[RED] ) {
				op[RED] = 0;
			} else {
				if( y >= ip[RED-3] || y >= ip[RED+3] || y == ip[RED] )
					op[RED] = 255;
				else
					op[RED] = 0;
			}

			if( y > ip[GRN] ) {
				op[GRN] = 0;
			} else {
				if( y >= ip[GRN-3] || y >= ip[GRN+3] || y == ip[GRN] )
					op[GRN] = 255;
				else
					op[GRN] = 0;
			}

			if( y > ip[BLU] ) {
				op[BLU] = 0;
			} else {
				if( y >= ip[BLU-3] || y >= ip[BLU+3] || y == ip[BLU] )
					op[BLU] = 255;
				else
					op[BLU] = 0;
			}

			if( fb_overlay ) {
				/* background */
				if( op[RED] == 0 && op[GRN] == 0 && op[BLU] == 0 ) {
					op[RED] = backgnd[x][RED];
					op[GRN] = backgnd[x][GRN];
					op[BLU] = backgnd[x][BLU];
				}
			} else {
				/* Grid lines */
				if( (y & 63) == 0 && op[RED] == 0
				 && op[GRN] == 0 && op[BLU] == 0 ) {
				 	op[RED] = 128;
				 	op[GRN] = 128;
				 	op[BLU] = 128;
				} else if( (y & 15) == 0 && op[RED] == 0
				 && op[GRN] == 0 && op[BLU] == 0 ) {
				 	op[RED] = 64;
				 	op[GRN] = 64;
				 	op[BLU] = 64;
				}
			}
		}

		fb_write( fboutp, 0, y+yoffset, outline, width );
	}

	if( verbose ) {
		for( x = 0; x < width; x++ )
			printf( "%3d: %3d %3d %3d\n", x,
			   scan[x+1][RED], scan[x+1][GRN], scan[x+1][BLU] );
	}

	fb_close( fbp );
	if( fboutp != fbp )
		fb_close( fboutp );

	exit( 0 );
}
