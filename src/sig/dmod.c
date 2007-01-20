/*                          D M O D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
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
/** @file dmod.c
 *
 *  Modify a stream of doubles.
 *
 *  Allows any number of add, subtract, multiply, divide, or
 *  exponentiation operations to be performed on a stream of values.
 *
 *  Author -
 *	Phillip Dykstra
 *	17 Apr 1987
 */
#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"

char	*file_name;
FILE	*infp;

static char usage[] = "\
Usage: dmod {-a add -s sub -m mult -d div -A(abs) -e exp -r root} [doubles]\n";

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
get_args(int argc, register char **argv)
{
	register int c;
	double	d;

	while ( (c = getopt( argc, argv, "a:s:m:d:Ae:r:" )) != EOF )  {
		switch( c )  {
		case 'a':
			op[ numop ] = ADD;
			val[ numop++ ] = atof(optarg);
			break;
		case 's':
			op[ numop ] = ADD;
			val[ numop++ ] = - atof(optarg);
			break;
		case 'm':
			op[ numop ] = MULT;
			val[ numop++ ] = atof(optarg);
			break;
		case 'd':
			op[ numop ] = MULT;
			d = atof(optarg);
			if( d == 0.0 ) {
				fprintf( stderr, "dmod: divide by zero!\n" );
				exit( 2 );
			}
			val[ numop++ ] = 1.0 / d;
			break;
		case 'A':
			op[ numop ] = ABS;
			val[ numop++ ] = 0;
			break;
		case 'e':
			op[ numop ] = POW;
			val[ numop++ ] = atof(optarg);
			break;
		case 'r':
			op[ numop ] = POW;
			d = atof(optarg);
			if( d == 0.0 ) {
				fprintf( stderr, "dmod: zero root!\n" );
				exit( 2 );
			}
			val[ numop++ ] = 1.0 / d;
			break;

		default:		/* '?' */
			return(0);
		}
	}

	if( optind >= argc )  {
		if( isatty(fileno(stdin)) )
			return(0);
		file_name = "-";
		infp = stdin;
	} else {
		file_name = argv[optind];
		if( (infp = fopen(file_name, "r")) == NULL )  {
			(void)fprintf( stderr,
				"dmod: cannot open \"%s\" for reading\n",
				file_name );
			return(0);
		}
	}

	if ( argc > ++optind )
		(void)fprintf( stderr, "dmod: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

int main(int argc, char **argv)
{
	int	i, n;
#ifdef sgi
	double	*bp;		/* avoid SGI -Zf reg pointer ++ problem */
#else
	register double	*bp;
#endif
	register double	arg;
	register int j;

	if( !get_args( argc, argv ) || isatty(fileno(infp))
	    || isatty(fileno(stdout)) ) {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	while( (n = fread(buf, sizeof(*buf), BUFLEN, infp)) > 0 ) {
		for( i = 0; i < numop; i++ ) {
			arg = val[ i ];
			switch( op[i] ) {
			case ADD:
				bp = &buf[0];
				for( j = n; j > 0; j-- ) {
					*bp++ += arg;
				}
				break;
			case MULT:
				bp = &buf[0];
				for( j = n; j > 0; j-- ) {
					*bp++ *= arg;
				}
				break;
			case POW:
				bp = &buf[0];
				for( j = n; j > 0; j-- ) {
					*bp++ = pow( *bp, arg );
				}
				break;
			case ABS:
				bp = &buf[0];
				for( j = n; j > 0; j-- ) {
					if( *bp < 0.0 )
						*bp = - *bp;
					bp++;
				}
				break;
			default:
				break;
			}
		}
		fwrite( buf, sizeof(*buf), n, stdout );
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
