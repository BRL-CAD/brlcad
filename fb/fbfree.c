/*
 *			F B F R E E . C
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1986 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"			/* For getopt() */
#include "fb.h"

static char	*framebuffer = NULL;

static char usage[] = "\
Usage: fbfree [-F framebuffer]\n";

main(argc, argv)
char	**argv;
int	argc;
{
	register int c;
	FBIO	*fbp;

	while ( (c = getopt( argc, argv, "F:" )) != EOF ) {
		switch( c ) {
		case 'F':
			framebuffer = optarg;
			break;
		default:		/* '?' */
			(void)fputs(usage, stderr);
			exit( 1 );
		}
	}
	if ( argc > ++optind ) {
		(void)fprintf( stderr, "fbfree: excess argument(s) ignored\n" );
	}

	if( (fbp = fb_open( framebuffer, 0, 0 )) == FBIO_NULL ) {
		fprintf( stderr, "fbfree: Can't open frame buffer\n" );
		return	1;
	}
	return	fb_free( fbp );
}
