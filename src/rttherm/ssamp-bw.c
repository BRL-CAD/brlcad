/*                      S S A M P - B W . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
 */
/** @file ssamp-bw.c
 *
 *  Program to convert spectral sample data into a single-channel
 *  monocrome image.
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifdef HAVE_UNISTD_H
#  include "unistd.h"
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "spectrum.h"


int	verbose = 0;

int	width = 64;
int	height = 64;
int	nwave = 2;

char	*datafile_basename = "mtherm";
char	spectrum_name[100];

extern struct bn_table		*spectrum;	/* spectrum table from liboptical */

struct bn_tabdata	*data;		/* a big array */

struct bn_tabdata	*filt;		/* filter kernel */

fastf_t			*pixels;	/* single values */

fastf_t		forced_minval = -1;
fastf_t		forced_maxval = -1;

fastf_t		computed_minval;
fastf_t		computed_maxval;

fastf_t		lower_wavelen = -1;
fastf_t		upper_wavelen = -1;

static char usage[] = "\
Usage: ssamp-bw [-s squarefilesize] [-w file_width] [-n file_height]\n\
		[-l lower_wavelen] [-u upper_wavelen] [-v]\n\
		[-m minval] [-M maxval]\n\
		file.ssamp\n";


/*
 *			G E T _ A R G S
 */
int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = getopt( argc, argv, "vs:w:n:l:u:m:M:" )) != EOF )  {
		switch( c )  {
		case 'v':
			verbose++;
			break;
		case 's':
			/* square file size */
			height = width = atoi(optarg);
			break;
		case 'w':
			width = atoi(optarg);
			break;
		case 'n':
			height = atoi(optarg);
			break;
		case 'l':
			lower_wavelen = atof(optarg);
			break;
		case 'u':
			upper_wavelen = atof(optarg);
			break;
		case 'm':
			forced_minval = atof(optarg);
			break;
		case 'M':
			forced_maxval = atof(optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )  return 0;
	return 1;	/* OK */
}

/*
 *			F I N D _ M I N M A X
 */
void
find_minmax(void)
{
	register fastf_t	max, min;
	register int		i;

	max = -INFINITY;
	min =  INFINITY;

	for( i = width * height - 1; i >= 0; i-- )  {
		register fastf_t	v;

		if( (v = pixels[i]) > max )  max = v;
		if( v < min )  min = v;
	}
	computed_maxval = max;
	computed_minval = min;
}

/*
 *			M A I N
 */
int
main(int argc, char **argv)
{
	int	i;
	fastf_t	scale;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		return 1;
	}

	if(verbose)	bu_debug = BU_DEBUG_COREDUMP;

	datafile_basename = argv[optind];

	/* Read spectrum definition */
	sprintf( spectrum_name, "%s.spect", datafile_basename );
	spectrum = (struct bn_table *)bn_table_read( spectrum_name );
	if( spectrum == NULL )  {
		bu_bomb("ssamp-bw: Unable to read spectrum\n");
	}
	BN_CK_TABLE(spectrum);
	if(verbose) bu_log("%s defines %d spectral samples\n", datafile_basename, spectrum->nx);
	nwave = spectrum->nx;	/* shared with Tcl */

	/* Allocate and read 2-D spectral samples array */
	data = bn_tabdata_binary_read( datafile_basename, width*height, spectrum );
	if( !data )  bu_bomb("bn_tabdata_binary_read() of datafile_basename failed\n");

	if( lower_wavelen <= 0 )  lower_wavelen = spectrum->x[0];
	if( upper_wavelen <= 0 )  upper_wavelen = spectrum->x[spectrum->nx];

	/* Build filter to obtain portion of spectrum user wants */
	filt = bn_tabdata_mk_linear_filter( spectrum, lower_wavelen, upper_wavelen );
	if( !filt )  bu_bomb("bn_tabdata_mk_linear_filter() failed\n");
	if( verbose )  {
		bn_pr_table( "spectrum", spectrum );
		bn_pr_tabdata( "filter", filt );
	}

	/* Convert each of the spectral sample curves into scalor values */
	pixels = bu_malloc( sizeof(fastf_t) * width * height, "fastf_t pixels" );

	for( i = width*height-1; i >= 0; i-- )  {
		struct bn_tabdata	*sp;
		/* Filter spectral data into a single power level */
		sp = (struct bn_tabdata *)
			(((char *)data)+i*BN_SIZEOF_TABDATA(spectrum));
		BN_CK_TABDATA(sp);
		/* Assumes interpretation #1 of the input data (see bn.h) */
		pixels[i] = bn_tabdata_mul_area1( filt, sp );
	}

	/* Obtain min and max values of that power level */
	find_minmax();
	bu_log("computed min = %g, max=%g\n", computed_minval, computed_maxval );

	if( forced_minval < 0 )  forced_minval = computed_minval;
	if( forced_maxval < 0 )  forced_maxval = computed_maxval;
	bu_log("rescale  min = %g, max=%g\n", forced_minval, forced_maxval );
	BU_ASSERT( forced_minval < forced_maxval );

	if( isatty(fileno(stdout)) )  {
		bu_log("ssamp-bw: Attempting to send binary output to the terminal, aborting\n");
		return 1;
	}

	/* Convert to 0..255 range and output */
	scale = 255 / (forced_maxval - forced_minval);
	for( i = 0; i < width*height; i++ )  {
		register int	val;
		val = (int)( (pixels[i] - forced_minval) * scale );
		if( val > 255 )  val = 255;
		else if( val < 0 ) val = 0;
		putc( val, stdout );
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
