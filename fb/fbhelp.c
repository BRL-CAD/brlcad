/*
 *			F B H E L P . C
 *
 *  Print out info about the selected frame buffer.
 *  Just calls fb_help().
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
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>

#include "machine.h"
#include "externs.h"				/* For getopt() */
#include "fb.h"

static char	*framebuffer = NULL;

static char usage[] = "\
Usage: fbhelp [-F framebuffer]\n";

int
main(int argc, char **argv)
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
		(void)fprintf( stderr, "fbhelp: excess argument(s) ignored\n" );
	}

	printf("\
A Frame Buffer display device is selected by\n\
setting the environment variable FB_FILE:\n\
(/bin/sh )  FB_FILE=/dev/device; export FB_FILE\n\
(/bin/csh)  setenv FB_FILE /dev/device\n\
Many programs also accept a \"-F framebuffer\" flag.\n\
Type \"man brlcad\" for more information.\n" );

	printf("=============== Available Devices ================\n");
	fb_genhelp();

	printf("=============== Current Selection ================\n");
	if( (fbp = fb_open( framebuffer, 0, 0 )) == FBIO_NULL ) {
		fprintf( stderr, "fbhelp: Can't open frame buffer\n" );
		return	1;
	}
	fb_help( fbp );
	return	fb_close( fbp );
}
