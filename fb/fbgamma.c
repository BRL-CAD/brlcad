/*
 *			F B G A M M A . C
 *
 *  Load a gamma correcting colormap into a framebuffer.
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
#include <math.h>
#include "externs.h"
#include "fb.h"

void	checkgamma();

static char usage[] = "\
Usage: fbgamma [-h -o] val [gval bval]\n";

main( argc, argv )
int argc; char **argv;
{
	int	i;
	int	onegamma = 0;
	int	fbsize = 512;
	int	overlay = 0;
	double	gamr, gamg, gamb;	/* gamma's */
	double	f;
	ColorMap cm;
	FBIO	*fbp;

	onegamma = 0;

	/* check for flags */
	while( argc > 1 ) {
		if( strcmp(argv[1],"-h") == 0 ) {
			fbsize = 1024;
		} else if( strcmp(argv[1],"-o") == 0 ) {
			overlay++;
		} else
			break;
		argc--;
		argv++;
	}

	/* now get the gammas */
	if( argc == 2 ) {
		/* single value for all channels */
		f = atof( argv[1] );
		checkgamma( f );
		gamr = gamg = gamb = 1.0 / f;
		onegamma++;
	} else if( argc == 4 ) {
		/* different RGB values */
		f = atof( argv[1] );
		checkgamma( f );
		gamr = 1.0 / f;
		f = atof( argv[2] );
		checkgamma( f );
		gamg = 1.0 / f;
		f = atof( argv[3] );
		checkgamma( f );
		gamb = 1.0 / f;
	} else {
		fprintf( stderr, usage );
		exit( 1 );
	}

	if( (fbp = fb_open( NULL, fbsize, fbsize )) == FBIO_NULL ) {
		exit( 2 );
	}

	/* get the starting map */
	if( overlay ) {
		fb_rmap( fbp, &cm );
	} else {
		/* start with a linear map */
		for( i = 0; i < 256; i++ ) {
			cm.cm_red[i] = cm.cm_green[i]
			= cm.cm_blue[i] = i << 8;
		}
	}

	/* apply the gamma(s) */
	for( i = 0; i < 256; i++ ) {
		if( gamr < 0 )
			cm.cm_red[i] = 65535 * pow( (double)cm.cm_red[i] / 65535.0, -1.0/gamr );
		else
			cm.cm_red[i] = 65535 * pow( (double)cm.cm_red[i] / 65535.0, gamr );
		if( onegamma && (overlay == 0) ) {
			cm.cm_green[i] = cm.cm_red[i];
			cm.cm_blue[i]  = cm.cm_red[i];
		} else {
			if( gamg < 0 )
				cm.cm_green[i] = 65535 * pow( (double)cm.cm_green[i] / 65535.0, -1.0/gamg );
			else
				cm.cm_green[i] = 65535 * pow( (double)cm.cm_green[i] / 65535.0, gamg );
			if( gamb < 0 )
				cm.cm_blue[i]  = 65535 * pow( (double)cm.cm_blue[i] / 65535.0, -1.0/gamb );
			else
				cm.cm_blue[i]  = 65535 * pow( (double)cm.cm_blue[i] / 65535.0, gamb );
		}
	}

	fb_wmap( fbp, &cm );
	fb_close( fbp );
}

void
checkgamma( g )
double g;
{
	if( fabs(g) < 1.0e-10 ) {
		fprintf( stderr, "fbgamma: gamma too close to zero\n" );
		fprintf( stderr, usage );
		exit( 3 );
	}
}
