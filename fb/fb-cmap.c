/*
 *			F B - C M A P . C
 *
 *  Save a colormap from a framebuffer.
 *
 *  Author -
 *	Robert Reschly
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
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "fb.h"

ColorMap cm;
static char usage[] = "\
Usage: fb-cmap [-h] [colormap]\n";

int
main(int argc, char **argv)
{
	FBIO	*fbp;
	FILE	*fp;
	int	fbsize = 512;
	int	i;

	while( argc > 1 ) {
		if( strcmp(argv[1], "-h") == 0 ) {
			fbsize = 1024;
		} else if( argv[1][0] == '-' ) {
			/* unknown flag */
			fprintf( stderr, usage );
			exit( 1 );
		} else
			break;	/* must be a filename */
		argc--;
		argv++;
	}

	if( argc > 1 ) {
		if( (fp = fopen(argv[1], "w")) == NULL ) {
			fprintf( stderr, "fb-cmap: can't open \"%s\"\n", argv[1] );
			fprintf( stderr, usage );
			exit( 2 );
		}
	} else
		fp = stdout;

	if( (fbp = fb_open( NULL, fbsize, fbsize )) == FBIO_NULL )
		exit( 2 );

	i = fb_rmap( fbp, &cm );
	fb_close( fbp );
	if( i < 0 ) {
		fprintf( stderr, "fb-cmap: can't read colormap\n" );
		exit( 3 );
	}

	for( i = 0; i <= 255; i++ ) {
		fprintf( fp, "%d\t%04x %04x %04x\n", i,
			cm.cm_red[i], cm.cm_green[i], cm.cm_blue[i] );
	}

	return(0);
}
