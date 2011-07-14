/*                          D M O D . C
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
/** @file dmod.c
 *
 *  Modify a stream of doubles.
 *
 *  Allows any number of add, subtract, multiply, divide, or
 *  exponentiation operations to be performed on a stream of values.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"


char	*file_name = NULL;
FILE	*infp = NULL;


#define	ADD	1
#define MULT	2
#define	ABS	3
#define	POW	4

#define	BUFLEN	4096

int	numop = 0;		/* number of operations */
int	op[256] = {0};		/* operations */
double	val[256] = {0.0};		/* arguments to operations */
double	buf[BUFLEN] = {0.0};		/* working buffer */

int
get_args(int argc, char **argv)
{
    int c;
    double	d;

    while ( (c = bu_getopt( argc, argv, "a:s:m:d:Ae:r:" )) != -1 )  {
	switch ( c )  {
	    case 'a':
		op[ numop ] = ADD;
		val[ numop++ ] = atof(bu_optarg);
		break;
	    case 's':
		op[ numop ] = ADD;
		val[ numop++ ] = - atof(bu_optarg);
		break;
	    case 'm':
		op[ numop ] = MULT;
		val[ numop++ ] = atof(bu_optarg);
		break;
	    case 'd':
		op[ numop ] = MULT;
		d = atof(bu_optarg);
		if (ZERO(d)) {
		    bu_exit(2, "dmod: divide by zero!\n" );
		}
		val[ numop++ ] = 1.0 / d;
		break;
	    case 'A':
		op[ numop ] = ABS;
		val[ numop++ ] = 0;
		break;
	    case 'e':
		op[ numop ] = POW;
		val[ numop++ ] = atof(bu_optarg);
		break;
	    case 'r':
		op[ numop ] = POW;
		d = atof(bu_optarg);
		if (ZERO(d)) {
		    bu_exit(2, "dmod: zero root!\n" );
		}
		val[ numop++ ] = 1.0 / d;
		break;

	    default:		/* '?' */
		return 0;
	}
    }

    if ( bu_optind >= argc )  {
	if ( isatty(fileno(stdin)) )
	    return 0;
	file_name = "-";
	infp = stdin;
    } else {
	file_name = argv[bu_optind];
	if ( (infp = fopen(file_name, "r")) == NULL )  {
	    (void)fprintf( stderr,
			   "dmod: cannot open \"%s\" for reading\n",
			   file_name );
	    return 0;
	}
    }

    if ( argc > ++bu_optind )
	(void)fprintf( stderr, "dmod: excess argument(s) ignored\n" );

    return 1;		/* OK */
}

int main(int argc, char **argv)
{
    int	i, n;
    double	*bp;
    double	arg;
    int j;
    size_t ret;

    if ( !get_args( argc, argv ) || isatty(fileno(infp))
	 || isatty(fileno(stdout)) ) {
	bu_exit(1, "Usage: dmod {-a add -s sub -m mult -d div -A(abs) -e exp -r root} [doubles]\n");
    }

    while ( (n = fread(buf, sizeof(*buf), BUFLEN, infp)) > 0 ) {
	for ( i = 0; i < numop; i++ ) {
	    arg = val[ i ];
	    switch ( op[i] ) {
		double d;

		case ADD:
		    bp = &buf[0];
		    for ( j = n; j > 0; j-- ) {
			*bp++ += arg;
		    }
		    break;
		case MULT:
		    bp = &buf[0];
		    for ( j = n; j > 0; j-- ) {
			*bp++ *= arg;
		    }
		    break;
		case POW:
		    bp = &buf[0];
		    for ( j = n; j > 0; j-- ) {
			d = pow( *bp, arg );
			*bp++ = d;
		    }
		    break;
		case ABS:
		    bp = &buf[0];
		    for ( j = n; j > 0; j-- ) {
			if ( *bp < 0.0 )
			    *bp = - *bp;
			bp++;
		    }
		    break;
		default:
		    break;
	    }
	}
	ret = fwrite( buf, sizeof(*buf), n, stdout );
	if (ret != (size_t)n)
	    perror("fwrite");
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
