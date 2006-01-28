/*                         B W M O D . C
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
 */
/** @file bwmod.c
 *
 *  Modify intensities in Black and White files.
 *
 *  Allows any number of add, subtract, multiply, divide, or
 *  exponentiation operations to be performed on a picture.
 *  Keeps track of and reports clipping.
 *
 *  Note that this works on PIX files also but there is no
 *  distinction made between colors.
 *
 *  Author -
 *	Phillip Dykstra
 *	31 July 1986
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "machine.h"
#include "bu.h"


char *progname = "(noname)";

char	*file_name;

char usage[] = "\
Usage: bwmod [-c] {-a add -s sub -m mult -d div -A -e exp -r root\n\
                   -S shift -M and -O or -X xor -t trunc} [file.bw] > file2.bw\n";

#define	ADD	1
#define MULT	2
#define	ABS	3
#define	POW	4
#define SHIFT	5
#define AND	6
#define OR	7
#define	XOR	8
#define	TRUNC	9
#define	BUFLEN	(8192*2)	/* usually 2 pages of memory, 16KB */

int	numop = 0;		/* number of operations */
int	op[256];		/* operations */
double	val[256];		/* arguments to operations */
unsigned char ibuf[BUFLEN];	/* input buffer */

#define MAPBUFLEN 256
int mapbuf[MAPBUFLEN];		/* translation buffer/lookup table */
int char_arith = 0;

int
get_args(int argc, register char **argv)
{
	register int c = 0;
	double	d = 0.0;

	while ( (c = bu_getopt( argc, argv, "a:s:m:d:Ae:r:cS:O:M:X:t:" )) != EOF )
	{
		switch( c )  {
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

			if( d == 0.0 ) {
				(void)fprintf( stderr, "bwmod: divide by zero!\n" );
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
			val[ numop++ ] = atof(bu_optarg);
			break;
		case 'r':
			op[ numop ] = POW;
			d = atof(bu_optarg);
			if( d == 0.0 ) {
				(void)fprintf( stderr, "bwmod: zero root!\n" );
				exit( 2 );
			}
			val[ numop++ ] = 1.0 / d;
			break;
		case 'c':
			char_arith = !char_arith; break;
		case 'S':
			op[ numop ] = SHIFT;
			val[ numop++] = atof(bu_optarg);
			break;
		case 'M':
			op[ numop ] = AND;
			val[ numop++] = atof(bu_optarg);
			break;
		case 'O':
			op[ numop ] = OR;
			val[ numop++ ] = atof(bu_optarg);
			break;
		case 'X':
			op[ numop ] = XOR;
			val[ numop++ ] = atof(bu_optarg);
			break;
		case 't':
			op[ numop ] = TRUNC;
			val[ numop++ ] = atof(bu_optarg);
			break;
		default:		/* '?' */
			return(0);
		}
	}

	if( bu_optind >= argc )  {
		if( isatty((int)fileno(stdin)) )
			return(0);
		file_name = "-";
	} else {
		file_name = argv[bu_optind];
		if( freopen(file_name, "r", stdin) == NULL )  {
			(void)fprintf( stderr,
				"bwmod: cannot open \"%s\" for reading\n",
				file_name );
			return(0);
		}
	}

	if ( argc > ++bu_optind )
		(void)fprintf( stderr, "bwmod: excess argument(s) ignored\n" );

	return(1);		/* OK */
}

void mk_trans_tbl(void)
{
	register int j, i, tmp;
	register double d;

	/* create translation map */
	for (j = 0; j < MAPBUFLEN ; ++j) {
		d = j;
		for (i=0 ; i < numop ; i++) {
			switch (op[i]) {
			case ADD : d += val[i]; break;
			case MULT: d *= val[i]; break;
			case POW : d = pow( d, val[i]); break;
			case ABS : if (d < 0.0) d = - d; break;
			case SHIFT: tmp=d; tmp=tmp<<(int)val[i];d=tmp;break;
			case OR  : tmp=d; tmp |= (int)val[i]; d=tmp;break;
			case AND : tmp=d; tmp &= (int)val[i]; d=tmp;break;
			case XOR : tmp=d; tmp ^= (int)val[i]; d= tmp; break;
			case TRUNC: tmp=((int)d/(int)val[i])*(int)val[i]; break;
			default  : (void)fprintf(stderr, "%s: error in op\n", progname);
				   exit(-1);
				   break;
			}
		}
		if (d > 255.0) {
			mapbuf[j] = 256;
		} else if (d < 0.0) {
			mapbuf[j] = -1;
		} else
			mapbuf[j] = d + 0.5;
	}
}
void mk_char_trans_tbl(void)
{
	register int j, i;
	register signed char d;

	/* create translation map */
	for (j = 0; j < MAPBUFLEN ; ++j) {
		d = j;
		for (i=0 ; i < numop ; i++) {
			switch (op[i]) {
			case ADD : d += val[i]; break;
			case MULT: d *= val[i]; break;
			case POW : d = pow( (double)d, val[i]); break;
			case ABS : if (d < 0.0) d = - d; break;
			case SHIFT: d=d<<(int)val[i]; break;
			case AND : d &= (int)val[i]; break;
			case OR  : d |= (int)val[i]; break;
			case XOR : d ^= (int)val[i]; break;
			case TRUNC: d /= (int)val[i];d *= (int)val[i]; break;
			default  : (void)fprintf(stderr, "%s: error in op\n", progname);
				   exit(-1);
				   break;
			}
		}
		mapbuf[j] = d & 0x0ff;
	}
}
int main(int argc, char **argv)
{
	register unsigned char	*p, *q;
	register int		tmp;
	int	 		n;
	unsigned long		clip_high, clip_low;

	progname = *argv;

	if( !get_args( argc, argv ) || isatty((int)fileno(stdin))
	    || isatty((int)fileno(stdout)) ) {
		(void)fputs(usage, stderr);
		exit( 1 );
	}

	if (char_arith)
		mk_char_trans_tbl();
	else
		mk_trans_tbl();

	clip_high = clip_low = 0L;
	while ( (n=read(0, (void *)ibuf, (unsigned)sizeof(ibuf))) > 0) {
		/* translate */
		for (p = ibuf, q = &ibuf[n] ; p < q ; ++p) {
			tmp = mapbuf[*p];
			if (tmp > 255) { ++clip_high; *p = 255; }
			else if (tmp < 0) { ++clip_low; *p = 0; }
			else *p = tmp;
		}
		/* output */
		if (write(1, (void *)ibuf, (unsigned)n) != n) {
			(void)fprintf(stderr, "%s: Error writing stdout\n",
				progname);
			exit(-1);
		}
	}

	if( clip_high != 0 || clip_low != 0 ) {
		(void)fprintf( stderr, "bwmod: clipped %lu high, %lu low\n",
			clip_high, clip_low );
	}
	return(0);
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
