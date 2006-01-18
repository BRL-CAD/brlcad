/*                        F B F R E E . C
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
/** @file fbfree.c
 *
 *  Free any resources associated with a frame buffer.
 *  Just calls fb_free().
 *
 *  Authors -
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

#include <stdio.h>

#include "machine.h"
#include "bu.h"
#include "fb.h"

static char	*framebuffer = NULL;

static char usage[] = "\
Usage: fbfree [-F framebuffer]\n";

int
main(int argc, char **argv)
{
	register int c;
	FBIO	*fbp;

	while ( (c = bu_getopt( argc, argv, "F:" )) != EOF ) {
		switch( c ) {
		case 'F':
			framebuffer = bu_optarg;
			break;
		default:		/* '?' */
			(void)fputs(usage, stderr);
			exit( 1 );
		}
	}
	if ( argc > ++bu_optind ) {
		(void)fprintf( stderr, "fbfree: excess argument(s) ignored\n" );
	}

	if( (fbp = fb_open( framebuffer, 0, 0 )) == FBIO_NULL ) {
		fprintf( stderr, "fbfree: Can't open frame buffer\n" );
		return	1;
	}
	return	fb_free( fbp );
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
