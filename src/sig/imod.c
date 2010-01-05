/*                          I M O D . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2009 United States Government as represented by
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
/** @file imod.c
 *
 *  Modify intensities in a stream of short (16 bit) unsigned integers.
 *
 *  Allows any number of add, subtract, multiply, divide, or
 *  exponentiation operations to be performed on a picture.
 *  Keeps track of and reports clipping.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu.h"


char *progname = "(noname)";
char *file_name = NULL;


#define	ADD	1
#define MULT	2
#define	ABS	3
#define	POW	4
#define	BUFLEN	(8192*2)	/* usually 2 pages of memory, 16KB */

int	numop = 0;		/* number of operations */
int	op[256];		/* operations */
double	val[256];		/* arguments to operations */
short iobuf[BUFLEN];		/* input buffer */
int mapbuf[65536];		/* translation buffer/lookup table */

int
get_args(int argc, char **argv)
{
    int c;
    double	d;

    while ( (c = bu_getopt( argc, argv, "a:s:m:d:Ae:r:" )) != EOF )  {
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
		if ( d == 0.0 ) {
		    bu_exit(2, "bwmod: divide by zero!\n" );
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
		if ( d == 0.0 ) {
		    bu_exit(2, "bwmod: zero root!\n" );
		}
		val[ numop++ ] = 1.0 / d;
		break;

	    default:		/* '?' */
		return(0);
	}
    }

    if ( bu_optind >= argc )  {
	if ( isatty((int)fileno(stdin)) )
	    return(0);
	file_name = "-";
    } else {
	file_name = argv[bu_optind];
	if ( freopen(file_name, "r", stdin) == NULL )  {
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
    int i, j;
    double d;

    /* create translation map */
    for (j = -32768; j < 32768; ++j) {
	d = j;
	for (i=0; i < numop; i++) {
	    switch (op[i]) {
		case ADD : d += val[i]; break;
		case MULT: d *= val[i]; break;
		case POW : d = pow( d, val[i]); break;
		case ABS : if (d < 0.0) d = - d; break;
		default  : (void)fprintf(stderr, "%s: error in op\n",
					 progname); break;
	    }
	}

	if (d > 32767.0)
	    mapbuf[j+32768] = 65537;
	else if (d < -32768.0)
	    mapbuf[j+32768] = -65536;
	else if (d < 0.0)
	    mapbuf[j+32768] = d - 0.5;
	else
	    mapbuf[j+32768] = d + 0.5;
    }
}

int main(int argc, char **argv)
{
    short *p, *q;
    int i;
    unsigned int	n;
    unsigned long clip_high, clip_low;

    if (!(progname=strrchr(*argv, '/')))
	progname = *argv;

    if ( !get_args( argc, argv ) || isatty(fileno(stdin)) || isatty(fileno(stdout)) ) {
	bu_exit( 1, "Usage: imod {-a add -s sub -m mult -d div -A(abs) -e exp -r root} [file.s]\n" );
    }

    mk_trans_tbl();

    clip_high = clip_low = 0;

    while ( (n=fread(iobuf, sizeof(*iobuf), BUFLEN, stdin)) > 0) {
	/* translate */
	for (p=iobuf, q= &iobuf[n]; p < q; ++p) {
	    i = *p + 32768;
	    if (mapbuf[i] > 32767) { ++clip_high; *p = 32767; }
	    else if (mapbuf[i] < -32768) { ++clip_low; *p = -32768; }
	    else *p = (short)mapbuf[i];
	}
	/* output */
	if (fwrite(iobuf, sizeof(*iobuf), n, stdout) != n) {
	    bu_exit(-1, "%s: Error writing stdout\n", progname);
	}
    }

    if ( clip_high != 0L || clip_low != 0L ) {
	(void)fprintf( stderr, "%s: clipped %lu high, %lu low\n",
		       progname,
		       clip_high, clip_low );
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
