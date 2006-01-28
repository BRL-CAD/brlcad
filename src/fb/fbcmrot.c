/*                       F B C M R O T . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2006 United States Government as represented by
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
/** @file fbcmrot.c
 *
 * Function -
 *	Dynamicly rotate the color map
 *
 *  Author -
 *	Michael John Muuss
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>		/* For struct timeval */
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "bu.h"
#include "fb.h"

ColorMap cm1, cm2;
ColorMap *inp, *outp;

int size = 512;
double fps = 0.0;	/* frames per second */
int increment = 1;
int onestep = 0;

FBIO *fbp;

static char usage[] = "\
Usage: fbcmrot [-h] [-i increment] steps_per_second\n";

int
get_args(int argc, register char **argv)
{
	register int c;

	while ( (c = bu_getopt( argc, argv, "hi:" )) != EOF )  {
		switch( c )  {
		case 'h':
			/* high-res */
			size = 1024;
			break;
		case 'i':
			/* increment */
			increment = atoi(bu_optarg);
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( bu_optind >= argc )  {
		/* no fps specified */
		fps = 0;
	} else {
		fps = atof(argv[bu_optind]);
		if( fps == 0 )
			onestep++;
	}

	if ( argc > ++bu_optind )
		(void)fprintf( stderr, "fbcmrot: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int
main(int argc, char **argv)
{
	register int i;
	struct timeval tv;

	if ( !get_args( argc, argv ) )  {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if( fps > 0.0 ) {
		tv.tv_sec = (long) (1.0 / fps);
		tv.tv_usec = (long) (((1.0 / fps) - tv.tv_sec) * 1000000);
	}

	if( (fbp = fb_open( NULL, size, size)) == FBIO_NULL )  {
		fprintf(stderr, "fbcmrot:  fb_open failed\n");
		return	1;
	}

	inp = &cm1;
	outp = &cm2;
	fb_rmap( fbp, inp );

	while(1)  {
		register int from;
		ColorMap *tp;

		/* Build color map for current value */
		for( i=0, from = increment; i < 256; i++, from++ ) {
			if( from < 0 )
				from += 256;
			else if( from > 255 )
				from -= 256;
			outp->cm_red[i]   = inp->cm_red[from];
			outp->cm_green[i] = inp->cm_green[from];
			outp->cm_blue[i]  = inp->cm_blue[from];
		}

		fb_wmap( fbp, outp );
		tp = outp;
		outp = inp;
		inp = tp;

		if( fps > 0.0 )  {
			fd_set readfds;

			FD_ZERO(&readfds);
			FD_SET(fileno(stdin), &readfds);
			select(fileno(stdin)+1, &readfds, (fd_set *)0, (fd_set *)0, &tv);
		}
		if( onestep )
			break;
	}
	fb_close( fbp );
	return	0;
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
