/*                           D - A . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
 */
/** @file d-a.c
 *
 * double to ascii.
 */

#include "common.h"

#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

int	nflag = 0;

static char usage[] = "\
Usage: d-a [-n] < doubles > ascii\n";

int main(int argc, char **argv)
{
	double	d;

	while( argc > 1 ) {
		if( strcmp( argv[1], "-n" ) == 0 )
			nflag++;
		else
			break;
		argc--;
		argv++;
	}
	if( argc > 1 || isatty(fileno(stdin)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	if( nflag ) {
		long	n;
		n = 0;
		while( fread(&d, sizeof(d), 1, stdin) == 1 ) {
			printf( "%ld %9g\n", n++, d );
		}
	} else {
		while( fread(&d, sizeof(d), 1, stdin) == 1 ) {
			printf( "%9g\n", d );
		}
	}
	return 0;
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
