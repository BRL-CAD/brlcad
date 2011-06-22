/*                         B W M O D . C
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
/** @file util/bwmod.c
 *
 * Modify intensities in Black and White files.
 *
 * Allows any number of add, subtract, multiply, divide, or
 * exponentiation operations to be performed on a picture.  Keeps
 * track of and reports clipping.
 *
 * Note that this works on PIX files also but there is no distinction
 * made between colors.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"


char *progname = "(noname)";

char *file_name;

char usage[] = "\
Usage: bwmod [-c] {-a add -s sub -m mult -d div -A -e exp -r root\n\
		   -S shift -M and -O or -X xor -t trunc} [file.bw] > file2.bw\n";

#define ADD	1
#define MULT	2
#define ABS	3
#define POW	4
#define SHIFT	5
#define AND	6
#define OR	7
#define XOR	8
#define TRUNC	9
#define BUFLEN	(8192*2)	/* usually 2 pages of memory, 16KB */

int numop = 0;		/* number of operations */
int op[256];		/* operations */
double val[256];		/* arguments to operations */
unsigned char ibuf[BUFLEN];	/* input buffer */

#define MAPBUFLEN 256
int mapbuf[MAPBUFLEN];		/* translation buffer/lookup table */
int char_arith = 0;

int
get_args(int argc, char **argv)
{
    int c = 0;
    double d = 0.0;

    while ((c = bu_getopt(argc, argv, "a:s:m:d:Ae:r:cS:O:M:X:t:")) != -1) {
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
		    bu_exit(2, "bwmod: cannot divide by zero!\n");
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
		return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty((int)fileno(stdin)))
	    return 0;
	file_name = "-";
    } else {
	file_name = argv[bu_optind];
	if (freopen(file_name, "rb", stdin) == NULL) {
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


void mk_trans_tbl(void)
{
    int j, i, tmp;
    double d;

    /* create translation map */
    for (j = 0; j < MAPBUFLEN; ++j) {
	d = j;
	for (i=0; i < numop; i++) {
	    switch (op[i]) {
		case ADD : d += val[i]; break;
		case MULT: d *= val[i]; break;
		case POW : d = pow(d, val[i]); break;
		case ABS : if (d < 0.0) d = - d; break;
		case SHIFT: tmp=d; tmp=tmp<<(int)val[i];d=tmp;break;
		case OR  : tmp=d; tmp |= (int)val[i]; d=tmp;break;
		case AND : tmp=d; tmp &= (int)val[i]; d=tmp;break;
		case XOR : tmp=d; tmp ^= (int)val[i]; d= tmp; break;
		case TRUNC: tmp=((int)d/(int)val[i])*(int)val[i]; break;
		default  : (void)fprintf(stderr, "%s: error in op\n", progname);
		    bu_exit (-1, NULL);
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
    int j, i;
    signed char d;

    /* create translation map */
    for (j = 0; j < MAPBUFLEN; ++j) {
	d = j;
	for (i=0; i < numop; i++) {
	    switch (op[i]) {
		case ADD : d += val[i]; break;
		case MULT: d *= val[i]; break;
		case POW : d = pow((double)d, val[i]); break;
		case ABS : if (d < 0.0) d = - d; break;
		case SHIFT: d=d<<(int)val[i]; break;
		case AND : d &= (int)val[i]; break;
		case OR  : d |= (int)val[i]; break;
		case XOR : d ^= (int)val[i]; break;
		case TRUNC: d /= (int)val[i];d *= (int)val[i]; break;
		default  : (void)fprintf(stderr, "%s: error in op\n", progname);
		    bu_exit (-1, NULL);
		    break;
	    }
	}
	mapbuf[j] = d & 0x0ff;
    }
}
int main(int argc, char **argv)
{
    unsigned char *p, *q;
    int tmp;
    int n;
    unsigned long clip_high, clip_low;

#if defined(_WIN32) && !defined(__CYGWIN__)
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    setmode(fileno(stderr), O_BINARY);
#endif

    progname = *argv;

    if (!get_args(argc, argv) || isatty((int)fileno(stdin))
	|| isatty((int)fileno(stdout))) {
	(void)fputs(usage, stderr);
	bu_exit (1, NULL);
    }

    if (char_arith)
	mk_char_trans_tbl();
    else
	mk_trans_tbl();

    clip_high = clip_low = 0L;
    while ((n=read(0, (void *)ibuf, (unsigned)sizeof(ibuf))) > 0) {
	/* translate */
	for (p = ibuf, q = &ibuf[n]; p < q; ++p) {
	    tmp = mapbuf[*p];
	    if (tmp > 255) { ++clip_high; *p = 255; }
	    else if (tmp < 0) { ++clip_low; *p = 0; }
	    else *p = tmp;
	}
	/* output */
	if (write(1, (void *)ibuf, (unsigned)n) != n) {
	    (void)fprintf(stderr, "%s: Error writing stdout\n",
			  progname);
	    bu_exit (-1, NULL);
	}
    }
    if (n < 0) {
	perror("READ ERROR");
    }

    if (clip_high != 0 || clip_low != 0) {
	(void)fprintf(stderr, "bwmod: clipped %lu high, %lu low\n", (long unsigned)clip_high, (long unsigned)clip_low);
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
