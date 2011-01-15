/*                       F B - C M A P . C
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
/** @file fb-cmap.c
 *
 *  Save a colormap from a framebuffer.
 *
 *  Author -
 *	Robert Reschly
 *	Phillip Dykstra
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
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

    while ( argc > 1 ) {
	if ( BU_STR_EQUAL(argv[1], "-h") ) {
	    fbsize = 1024;
	} else if ( argv[1][0] == '-' ) {
	    /* unknown flag */
	    bu_exit(1, "%s", usage );
	} else
	    break;	/* must be a filename */
	argc--;
	argv++;
    }

    if ( argc > 1 ) {
	if ( (fp = fopen(argv[1], "wb")) == NULL ) {
	    fprintf( stderr, "fb-cmap: can't open \"%s\"\n", argv[1] );
	    bu_exit(2, "%s", usage );
	}
    } else
	fp = stdout;

    if ( (fbp = fb_open( NULL, fbsize, fbsize )) == FBIO_NULL )
	bu_exit( 2, "Unable to open framebuffer\n" );

    i = fb_rmap( fbp, &cm );
    fb_close( fbp );
    if ( i < 0 ) {
	bu_exit(3, "fb-cmap: can't read colormap\n" );
    }

    for ( i = 0; i <= 255; i++ ) {
	fprintf( fp, "%d\t%04x %04x %04x\n", i,
		 cm.cm_red[i], cm.cm_green[i], cm.cm_blue[i] );
    }

    return 0;
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
