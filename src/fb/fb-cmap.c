/*                       F B - C M A P . C
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
/** @file fb-cmap.c
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"



#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_STRING_H
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
