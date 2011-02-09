/*                           D - A . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
 */
/** @file d-a.c
 *
 * double to ascii.
 *
 */

#include "common.h"

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "bio.h"

#include  "bu.h"


int	nflag = 0;

static const char usage[] = "\
Usage: d-a [-n] < doubles > ascii\n";

int main(int argc, char **argv)
{
    double	d;

    while ( argc > 1 ) {
	if ( BU_STR_EQUAL( argv[1], "-n" ) )
	    nflag++;
	else
	    break;
	argc--;
	argv++;
    }
    if ( argc > 1 || isatty(fileno(stdin)) ) {
	bu_exit(1, "%s", usage );
    }

    if ( nflag ) {
	long	n;
	n = 0;
	while ( fread(&d, sizeof(d), 1, stdin) == 1 ) {
	    printf( "%ld %9g\n", n++, d );
	}
    } else {
	while ( fread(&d, sizeof(d), 1, stdin) == 1 ) {
	    printf( "%9g\n", d );
	}
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
