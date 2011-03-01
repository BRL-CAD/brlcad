/*                          D S E L . C
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
/** @file dsel.c
 *
 * select some number of doubles
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "bu.h"

#define INTEGER_MAX ( ((int) ~0) >> 1 )


double	buf[4096] = {0};


void
skip(int num)
{
    int	n, m;

    while ( num > 0 ) {
	n = num > 1024 ? 1024 : num;
	if ( (m = fread( buf, sizeof(*buf), n, stdin )) == 0 )
	    exit( 0 );
	num -= m;
    }
}

void
keep(int num)
{
    int	n, m;
    size_t ret;

    while ( num > 0 ) {
	n = num > 1024 ? 1024 : num;
	if ( (m = fread( buf, sizeof(*buf), n, stdin )) == 0 )
	    exit( 0 );
	ret = fwrite( buf, sizeof(*buf), m, stdout );
	if (ret != (size_t)m)
	    perror("fwrite");
	num -= n;
    }
}

int main(int argc, char **argv)
{
    int	nskip;	/* number to skip */
    int	nkeep;	/* number to keep */

    if ( argc < 1 || isatty(fileno(stdin)) || isatty(fileno(stdout)) ) {
	bu_exit(1, "Usage: dsel num\n       dsel skip keep ...\n");
    }

    if ( argc == 2 ) {
	keep( atoi(argv[1]) );
	exit( 0 );
    }

    while ( argc > 1 ) {
	nskip = atoi(argv[1]);
	argc--;
	argv++;
	if ( nskip > 0 )
	    skip( nskip );

	if ( argc > 1 ) {
	    nkeep = atoi(argv[1]);
	    argc--;
	    argv++;
	} else {
	    nkeep = INTEGER_MAX;
	}

	if ( nkeep <= 0 )
	    exit( 0 );
	keep( nkeep );
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
