/*                        S P M - F B . C
 * BRL-CAD
 *
 * Copyright (C) 1986-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file spm-fb.c
 *
 *  Author -
 *	Phil Dykstra
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
#include <math.h>

#include "machine.h"
#include "fb.h"
#include "spm.h"

static FBIO	*fbp;

static char	*framebuffer = NULL;
static int	scr_width = 0;
static int	scr_height = 0;

static char	*file_name;
static int	square = 0;
static int	vsize;

void spm_fb(spm_map_t *mapp);
void spm_square(register spm_map_t *mapp);

static char usage[] = "\
Usage: spm-fb [-h -s] [-F framebuffer]\n\
	[-S squarescrsize] [-W scr_width] [-N scr_height]\n\
	vsize [filename]\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = getopt( argc, argv, "hF:sS:W:N:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			scr_height = scr_width = 1024;
			break;
		case 'F':
			framebuffer = optarg;
			break;
		case 's':
			square = 1;
			break;
		case 'S':
			scr_height = scr_width = atoi(optarg);
			break;
		case 'W':
			scr_width = atoi(optarg);
			break;
		case 'N':
			scr_height = atoi(optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )
		return(0);		/* missing positional arg */
	vsize = atoi( argv[optind++] );

	if( optind >= argc )  {
		if( isatty(fileno(stdin)) )
			return(0);
		file_name = "-";
	} else {
		file_name = argv[optind];
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "spm-fb: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
	register spm_map_t	*mp;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( (fbp = fb_open( framebuffer, scr_width, scr_height )) == FBIO_NULL )
		exit(12);
	scr_width = fb_getwidth(fbp);
	scr_height = fb_getheight(fbp);

	mp = spm_init( vsize, sizeof(RGBpixel) );
	if( mp == SPM_NULL || fbp == FBIO_NULL )
		exit( 1 );

	spm_load( mp, file_name );

	if( square )
		spm_square( mp );
	else
		spm_fb( mp );

	spm_free( mp );
	fb_close( fbp );
	exit(0);
}

/*
 *			S P M _ F B
 *
 *  Displays a sphere map on a framebuffer.
 */
void
spm_fb(spm_map_t *mapp)
{
	register int	j;

	for( j = 0; j < mapp->ny; j++ ) {
		fb_write( fbp, 0, j, mapp->xbin[j], mapp->nx[j] );
#ifdef NEVER
		for( i = 0; i < mapp->nx[j]; i++ ) {
			rgb[RED] = mapp->xbin[j][i*3];
			rgb[GRN] = mapp->xbin[j][i*3+1];
			rgb[BLU] = mapp->xbin[j][i*3+2];
			fb_write( fbp, i, j, (unsigned char *)rgb, 1 );
		}
#endif
	}
}

/*
 *			S P M _ S Q U A R E
 *
 *  Display a square sphere map on a framebuffer.
 */
void
spm_square(register spm_map_t *mapp)
{
	register int	x, y;
	register unsigned char	*scanline;

	scanline = (unsigned char *)malloc( scr_width * sizeof(RGBpixel) );

	for( y = 0; y < scr_height; y++ ) {
		for( x = 0; x < scr_width; x++ ) {
			spm_read( mapp, scanline[x],
				(double)x/(double)scr_width,
				(double)y/(double)scr_height );
		}
		if( fb_write( fbp, 0, y, scanline, scr_width ) != scr_width )  break;
	}
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
