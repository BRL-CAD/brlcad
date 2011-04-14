/*                          S M O D . C
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
 */
/** @file smod.c
 *
 * Modify intensities in a stream of short (16 bit) unsigned integers.
 *
 * Allows any number of add, subtract, multiply, divide, or
 * exponentiation operations to be performed on a picture.
 * Keeps track of and reports clipping.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"


#define ADD 1
#define MULT 2
#define ABS 3
#define POW 4
#define BUFLEN (8192*2)	/* usually 2 pages of memory, 16KB */


char *progname = "(noname)";

int numop = 0;		/* number of operations */
int op[256];		/* operations */
double val[256];		/* arguments to operations */
short mapbuf[65536];		/* translation buffer/lookup table */
unsigned char clip_h[256];	/* map of values which clip high */
unsigned char clip_l[256];	/* map of values which clip low */


int
get_args(int argc, char *argv[])
{
    char *file_name;
    int c;
    double d;

    while ((c = bu_getopt(argc, argv, "a:s:m:d:Ae:r:")) != -1) {
	switch (c) {
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
		    bu_exit(2, "bwmod: divide by zero!\n");
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
		    bu_exit(2, "bwmod: zero root!\n");
		}
		val[ numop++ ] = 1.0 / d;
		break;

	    default:		/* '?' */
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty((int)fileno(stdin)))
	    return 0;
	file_name = "-";
    } else {
	file_name = argv[bu_optind];
	if (freopen(file_name, "r", stdin) == NULL) {
	    (void)fprintf(stderr,
			  "bwmod: cannot open \"%s\" for reading\n",
			  file_name);
	    return 0;
	}
    }

    if (argc > ++bu_optind)
	(void)fprintf(stderr, "bwmod: excess argument(s) ignored\n");

    return 1;		/* OK */
}


void
mk_trans_tbl()
{
    int i, j, k;
    double d;

    (void)memset((char *)clip_h, 0, sizeof(clip_h));
    (void)memset((char *)clip_l, 0, sizeof(clip_l));

    /* create translation map */
    for (j = 0; j < 65536; ++j) {
	k = j - 32768;
	d = k;
	for (i=0; i < numop; i++) {
	    switch (op[i]) {
		case ADD : d += val[i]; break;
		case MULT: d *= val[i]; break;
		case POW : d = pow(d, val[i]); break;
		case ABS : if (d < 0.0) d = - d; break;
		default  : (void)fprintf(stderr, "%s: error in op\n",
					 progname); break;
	    }
	}


	if (d > 32767.0) {
	    clip_h[j] = 1;
	    mapbuf[j] = 32767;
	} else if (d < -32768.0) {
	    clip_l[j] = 1;
	    mapbuf[j] = -32768;
	} else
	    mapbuf[j] = d + 0.5;
    }
}


int
main(int argc, char *argv[])
{
    unsigned int j, n;
    unsigned long clip_high, clip_low;
    char *strrchr();
    short iobuf[BUFLEN];		/* input buffer */

    if (!(progname=strrchr(*argv, '/')))
	progname = *argv;

    if (!get_args(argc, argv) || isatty(fileno(stdin)) || isatty(fileno(stdout))) {
	bu_exit(1, "Usage: smod {-a add -s sub -m mult -d div -A(abs) -e exp -r root} [file.s]\n");
    }

    mk_trans_tbl();

    clip_high = clip_low = 0;

    while ((n=fread(iobuf, sizeof(*iobuf), BUFLEN, stdin)) > 0) {
	/* translate */
	for (j=0; j < n; ++j) {
	    iobuf[j] = mapbuf[iobuf[j] + 32768];
	    if (clip_h[j]) clip_high++;
	    else if (clip_l[j]) clip_low++;
	}
	/* output */
	if (fwrite(iobuf, sizeof(*iobuf), n, stdout) != n) {
	    bu_exit(-1, "%s: Error writing stdout\n", progname);
	}
    }

    if (clip_high != 0 || clip_low != 0) {
	(void)fprintf(stderr, "%s: clipped %lu high, %lu low\n", progname, (long unsigned)clip_high, (long unsigned)clip_low);
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
