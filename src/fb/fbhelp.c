/*                        F B H E L P . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2011 United States Government as represented by
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
/** @file fbhelp.c
 *
 * Print out info about the selected frame buffer.
 * Just calls fb_help().
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu.h"
#include "fb.h"


static char	*framebuffer = NULL;

static char usage[] = "\
Usage: fbhelp [-F framebuffer]\n";

int
main(int argc, char **argv)
{
    int c;
    FBIO	*fbp;

    while ( (c = bu_getopt( argc, argv, "F:" )) != -1 ) {
	switch ( c ) {
	    case 'F':
		framebuffer = bu_optarg;
		break;
	    default:		/* '?' */
		(void)fputs(usage, stderr);
		return 1;
	}
    }
    if ( argc > ++bu_optind ) {
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
    if ( (fbp = fb_open( framebuffer, 0, 0 )) == FBIO_NULL ) {
	fprintf( stderr, "fbhelp: Can't open frame buffer\n" );
	return	1;
    }
    fb_help( fbp );
    return	fb_close( fbp );
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
