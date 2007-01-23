/*                    F B S C A N P L O T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file fbscanplot.c
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <stdlib.h>
#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "fb.h"

unsigned char	*scan;		/* Scanline to be examined */
unsigned char	*outline;	/* output line buffer */
unsigned char	*backgnd;	/* copy of line to be overlaid */

int	yline;			/* line to plot */
int	scr_width = 0;		/* framebuffer width */
int	scr_height = 0;		/* framebuffer height */
int	verbose = 0;		/* output scanline values to stdout */
int	fb_overlay = 0;		/* plot on background, else black with grid */
int	cmap_crunch = 0;	/* Plot values after passing through color map */
int	reverse = 0;		/* highlight chosen line by inverting it */
char	*outframebuffer = NULL;
FBIO	*fbp, *fboutp;
ColorMap map;

char usage[] = "\
Usage: fbscanplot [-h] [-v] [-c] [-o] [-r]\n\
	[-W scr_width] [-F outframebuffer] yline\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt( argc, argv, "cvhorW:F:" )) != EOF ) {
		switch( c )  {
		case 'c':
			cmap_crunch++;
			break;
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
			scr_width = atoi(bu_optarg);
			break;
		case 'F':
			outframebuffer = bu_optarg;
			break;
		default:		/* '?' */
			return(0);
		}
	}

	if( bu_optind >= argc )
		return(0);
	else
		yline = atoi( argv[bu_optind] );

	if ( argc > ++bu_optind )
		return(0);	/* too many args */

	return(1);		/* OK */
}

int
main(int argc, char **argv)
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
	scan = (unsigned char *)malloc( (scr_width+2) * sizeof(RGBpixel) );
	outline = (unsigned char *)malloc( (scr_width+2) * sizeof(RGBpixel) );
	backgnd = (unsigned char *)malloc( (scr_width+2) * sizeof(RGBpixel) );

	/* Read the scanline to be examined */
	if( fb_read( fbp, 0, yline, scan+3, scr_width ) != scr_width )
		exit(4);

	fb_make_linear_cmap(&map);
	if( cmap_crunch )  {
		if( fb_rmap( fbp, &map ) < 0 )
			fprintf(stderr,"fbscanplot: error reading colormap\n");
	}

	/* extend the edges with one duplicate pixel each way */
	scan[0*3+RED] = scan[1*3+RED];
	scan[0*3+GRN] = scan[1*3+GRN];
	scan[0*3+BLU] = scan[1*3+BLU];
	scan[(scr_width+1)*3+RED] = scan[scr_width*3+RED];
	scan[(scr_width+1)*3+GRN] = scan[scr_width*3+GRN];
	scan[(scr_width+1)*3+BLU] = scan[scr_width*3+BLU];

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
			outline[x*3+RED] = 255 - scan[(x+1)*3+RED];
			outline[x*3+GRN] = 255 - scan[(x+1)*3+GRN];
			outline[x*3+BLU] = 255 - scan[(x+1)*3+BLU];
		}
		fb_write( fbp, 0, yline, outline, scr_width );
	}

	/* The scanplot takes 256 lines, one for each intensity value */
	for( y = 0; y < 256; y++ ) {
		if( fb_overlay )
			fb_read( fboutp, 0, y+yoffset, backgnd, scr_width );

		ip = &scan[1*3+RED];
		op = &outline[0*3+RED];
		for( x = 0; x < scr_width; x++, op += 3, ip += 3 ) {
			if( y > (int)map.cm_red[ip[RED]]>>8 ) {
				op[RED] = 0;
			} else {
				if( y >= (int)map.cm_red[ip[RED-3]]>>8 ||
				    y >= (int)map.cm_red[ip[RED+3]]>>8 ||
				    y == (int)map.cm_red[ip[RED]]>>8 )
					op[RED] = 255;
				else
					op[RED] = 0;
			}

			if( y > (int)map.cm_green[ip[GRN]]>>8 ) {
				op[GRN] = 0;
			} else {
				if( y >= (int)map.cm_green[ip[GRN-3]]>>8 ||
				    y >= (int)map.cm_green[ip[GRN+3]]>>8 ||
				    y == (int)map.cm_green[ip[GRN]]>>8 )
					op[GRN] = 255;
				else
					op[GRN] = 0;
			}

			if( y > (int)map.cm_blue[ip[BLU]]>>8 ) {
				op[BLU] = 0;
			} else {
				if( y >= (int)map.cm_blue[ip[BLU-3]]>>8 ||
				    y >= (int)map.cm_blue[ip[BLU+3]]>>8 ||
				    y == (int)map.cm_blue[ip[BLU]]>>8 )
					op[BLU] = 255;
				else
					op[BLU] = 0;
			}

			if( fb_overlay ) {
				/* background */
				if( op[RED] == 0 && op[GRN] == 0 && op[BLU] == 0 ) {
					op[RED] = backgnd[x*3+RED];
					op[GRN] = backgnd[x*3+GRN];
					op[BLU] = backgnd[x*3+BLU];
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
			   scan[(x+1)*3+RED], scan[(x+1)*3+GRN], scan[(x+1)*3+BLU] );
	}

	fb_close( fbp );
	if( fboutp != fbp )
		fb_close( fboutp );

	exit( 0 );
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
