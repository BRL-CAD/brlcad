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

extern int	getopt();
extern char	*optarg;
extern int	optind;

RGBpixel	*scan;		/* Scanline to be examined */
RGBpixel	*outline;	/* output line buffer */
RGBpixel	*backgnd;	/* copy of line to be overlaid */

int	yline;			/* line to plot */
int	scr_width = 0;		/* framebuffer width */
int	scr_height = 0;		/* framebuffer height */
int	verbose = 0;		/* output scanline values to stdout */
int	fb_overlay = 0;		/* plot on background, else black with grid */
int	reverse = 0;		/* highlight chosen line by inverting it */
char	*outframebuffer = NULL;
FBIO	*fbp, *fboutp;

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
			scr_width = scr_height = 1024;
			break;
		case 'o':
			fb_overlay++;
			break;
		case 'r':
			reverse++;
			break;
		case 'W':
			scr_width = atoi(optarg);
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

	if( (fbp = fb_open( NULL, scr_width, scr_height )) == NULL )
		exit( 2 );
	scr_width = fb_getwidth(fbp);
	scr_height = fb_getheight(fbp);

	if( outframebuffer != NULL ) {
		if( (fboutp = fb_open( outframebuffer, scr_width, scr_width )) == NULL )
			exit( 3 );
	} else
		fboutp = fbp;

	/* Allocate the buffers */
	scan = (RGBpixel *)malloc( (scr_width+2) * sizeof(RGBpixel) );
	outline = (RGBpixel *)malloc( (scr_width+2) * sizeof(RGBpixel) );
	backgnd = (RGBpixel *)malloc( (scr_width+2) * sizeof(RGBpixel) );

	/* Read the scanline to be examined */
	if( fb_read( fbp, 0, yline, &scan[1][RED], scr_width ) != scr_width )
		exit(4);

	/* extend the edges with one duplicate pixel each way */
	scan[0][RED] = scan[1][RED];
	scan[0][GRN] = scan[1][GRN];
	scan[0][BLU] = scan[1][BLU];
	scan[scr_width+1][RED] = scan[scr_width][RED];
	scan[scr_width+1][GRN] = scan[scr_width][GRN];
	scan[scr_width+1][BLU] = scan[scr_width][BLU];

	/* figure out where to put it on the screen */
	if( fb_overlay == 0 && fboutp == fbp && yline < scr_height/2 ) {
		yoffset = scr_height - 256;
		if( yoffset <= yline )
			yoffset = 0;
	} else
		yoffset = 0;

	if( reverse ) {
		/* Output the negative of the chosen line */
		for( x = 0; x < scr_width; x++ ) {
			outline[x][RED] = 255 - scan[x+1][RED];
			outline[x][GRN] = 255 - scan[x+1][GRN];
			outline[x][BLU] = 255 - scan[x+1][BLU];
		}
		fb_write( fbp, 0, yline, outline, scr_width );
	}

	/* The scanplot takes 256 lines, one for each intensity value */
	for( y = 0; y < 256; y++ ) {
		if( fb_overlay )
			fb_read( fboutp, 0, y+yoffset, backgnd, scr_width );

		ip = &scan[1][RED];
		op = &outline[0][RED];
		for( x = 0; x < scr_width; x++, op += 3, ip += 3 ) {
			if( y > (int)ip[RED] ) {
				op[RED] = 0;
			} else {
				if( y >= (int)ip[RED-3] || y >= (int)ip[RED+3] || y == ip[RED] )
					op[RED] = 255;
				else
					op[RED] = 0;
			}

			if( y > (int)ip[GRN] ) {
				op[GRN] = 0;
			} else {
				if( y >= (int)ip[GRN-3] || y >= (int)ip[GRN+3] || y == ip[GRN] )
					op[GRN] = 255;
				else
					op[GRN] = 0;
			}

			if( y > (int)ip[BLU] ) {
				op[BLU] = 0;
			} else {
				if( y >= (int)ip[BLU-3] || y >= (int)ip[BLU+3] || y == ip[BLU] )
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

		fb_write( fboutp, 0, y+yoffset, outline, scr_width );
	}

	if( verbose ) {
		for( x = 0; x < scr_width; x++ )
			printf( "%3d: %3d %3d %3d\n", x,
			   scan[x+1][RED], scan[x+1][GRN], scan[x+1][BLU] );
	}

	fb_close( fbp );
	if( fboutp != fbp )
		fb_close( fboutp );

	exit( 0 );
}
