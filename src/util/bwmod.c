/*                         B W M O D . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2021 United States Government as represented by
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
 * WARNING, added 8 June 2015: SHIFT is done the way it is because I
 * did not see << and >> working properly when attempted with negative
 * arguments.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <math.h>
#include "bio.h"

#include "vmath.h"
#include "bu/app.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/getopt.h"
#include "bu/exit.h"


char hyphen[] = "-";
char noname[] = "(noname)";
char *progname = noname;

char *file_name;

char usage[] = "\
Usage: bwmod [-c] {-a add -s sub -m mult -d div -A -e exp -r root\n\
		   -S shift -M and -O or -X xor -R -t} [file.bw] > file2.bw\n";

#define ADD	1
#define MULT	2
#define ABS	3
#define POW	4
#define SHIFT	5
#define AND	6
#define OR	7
#define XOR	8
#define ROUND	9 /* this was TRUNC, which is discontinued because
		   * truncation is handled internally as "-S 0"
		   */
#define BUFLEN	(8192*2)	/* usually 2 pages of memory, 16KB */

int numop = 0;			/* number of operations */
int op[256];			/* operations */
double val[256];		/* arguments to operations */
unsigned char ibuf[BUFLEN];	/* input buffer */

#define MAPBUFLEN 256
int mapbuf[MAPBUFLEN];		/* translation buffer/lookup table */
int char_arith = 0;

void
checkpow(double x , double exponent)
{
    if (x > 0.0) {
	return;
    }

    if (x < 0.0) {
	double diff;
	diff = exponent - (double)((int)exponent);
	if (diff < 0.0 || diff > 0.0) {
	    fprintf(stderr,
		    "bwmod: negative number (%f) to non-integer power (%f)\n",
		    x, exponent);
	    bu_exit(-1, NULL);
	}
    }
    /* We have x == 0.0, and we accept that 0 to 0 power is 1. */
    if (exponent >= 0.0) {
	return;
    }

    fprintf(stderr, "bwmod: zero to negative power (%f)\n", exponent);
    bu_exit(-1, NULL);
}


int
get_args(int argc, char **argv)
{
    int c;
    double d = 0.0;

    while ((c = bu_getopt(argc, argv, "a:s:m:d:Ae:r:cS:O:M:X:Rth?")) != -1) {
	switch (c) {
	case 'a':
	    op[numop] = ADD;
	    val[numop++] = atof(bu_optarg);
	    break;
	case 's':
	    op[numop] = ADD;
	    val[numop++] = - atof(bu_optarg);
	    break;
	case 'm':
	    op[numop] = MULT;
	    val[numop++] = atof(bu_optarg);
	    break;
	case 'd':
	    d = atof(bu_optarg);
	    if (ZERO(d)) {
		bu_exit(2, "bwmod: cannot divide by zero!\n");
	    }
	    op[numop] = MULT;
	    val[numop++] = 1.0 / d;
	    break;
	case 'A':
	    op[numop] = ABS;
	    /* If using ABS, don't care what val[numop] is, but still
	     * must increment numop. (The following would increment
	     * numop AFTER it's used as "val" subscript.)
	     */
	    /* val[numop++] = 0.0; */
	    numop++;
	    break;
	case 'e':
	    op[numop] = POW;
	    val[numop++] = atof(bu_optarg);
	    break;
	case 'r':
	    d = atof(bu_optarg);
	    if (ZERO(d)) {
		bu_exit(2, "bwmod: zero root!\n");
	    }
	    op[numop] = POW;
	    val[numop++] = 1.0 / d;
	    break;
	case 'c':
	    char_arith = !char_arith;
	    break;
	case 'S':
	    op[numop] = SHIFT;
	    val[numop++] = atof(bu_optarg);
	    break;
	case 'M':
	    op[numop] = AND;
	    val[numop++] = atof(bu_optarg);
	    break;
	case 'O':
	    op[numop] = OR;
	    val[numop++] = atof(bu_optarg);
	    break;
	case 'X':
	    op[numop] = XOR;
	    val[numop++] = atof(bu_optarg);
	    break;
	case 'R':
	    op[numop] = ROUND;
	    /* See above remark for case 'A' (don't care about
	     * val[numop] but need numop++).
	     */
	    /* val[numop++] = 0.0; */
	    numop++;
	    /* fall through */
	case 't':
	    /* Notice that -S truncates, so we internally use -S 0 */
	    op[numop] = SHIFT;
	    val[numop++] = 0.0;
	    break;
	default:  /* '?' 'h' */
	    return 0;
	}
    }

    if (bu_optind >= argc) {
	if (isatty((int)fileno(stdin))) {
	    return 0;
	}
	file_name = hyphen;
    } else {
	char *ifname;
	file_name = argv[bu_optind];
	ifname = bu_file_realpath(file_name, NULL);
	if (freopen(ifname, "rb", stdin) == NULL) {
	    fprintf(stderr,
		    "bwmod: cannot open \"%s(canonical %s)\" for reading\n",
		    file_name, ifname);
	    bu_free(ifname, "ifname alloc from bu_file_realpath");
	    return 0;
	}
	bu_free(ifname, "ifname alloc from bu_file_realpath");
    }

    if (argc > ++bu_optind) {
	fprintf(stderr, "bwmod: excess argument(s) ignored\n");
    }

    return 1; /* OK */
}


void
mk_trans_tbl(void)
{
    int j, i, tmp, intval;
    double d;

    /* create translation map */
    for (j = 0; j < MAPBUFLEN; ++j) {
	d = j;
	for (i = 0; i < numop; i++) {
	    switch (op[i]) {
	    case ADD :
		d += val[i];
		break;
	    case MULT:
		d *= val[i];
		break;
	    case POW :
		checkpow(d, val[i]);
		d = pow(d, val[i]);
		break;
	    case ABS :
		if (d < 0.0) {
		    d = - d;
		}
		break;
	    case SHIFT:
		tmp = d;
		intval = (int)val[i];
		if (intval > 0) {
		    tmp = tmp << intval;
		} else if (intval < 0) {
		    tmp = tmp >> (-intval);
		}
		d = tmp;
		break;
	    case OR  :
		tmp = d;
		tmp |= (int)val[i];
		d = tmp;
		break;
	    case AND :
		tmp = d;
		tmp &= (int)val[i];
		d = tmp;
		break;
	    case XOR :
		tmp = d;
		tmp ^= (int)val[i];
		d = tmp;
		break;
	    /* case TRUNC:
	     * tmp=((int)d/(int)val[i])*(int)val[i];
	     * d=tmp;
	     * break;
	     */
	    case ROUND:
		if (d > 0) {
		    d = (int)(d + 0.5);
		} else if (d < 0) {
		    d = (int)(d - 0.5);
		}
		break;
	    default  :
		fprintf(stderr, "%s: error in op\n", progname);
		bu_exit(-1, NULL);
		break;
	    }
	}
	if (d > 255.0) {
	    mapbuf[j] = 256;
	} else if (d < 0.0) {
	    mapbuf[j] = -1;
	} else {
	    mapbuf[j] = d + 0.5;
	}
    }
}


void
mk_char_trans_tbl(void)
{
    int j, i, intval;
    signed char d;

    /* create translation map */
    for (j = 0; j < MAPBUFLEN; ++j) {
	d = j;
	for (i = 0; i < numop; i++) {
	    switch (op[i]) {
	    case ADD :
		d += val[i];
		break;
	    case MULT:
		d *= val[i];
		break;
	    case POW :
		checkpow((double)d, val[i]);
		d = pow((double)d, val[i]);
		break;
	    case ABS :
		if (d < 0.0) {
		    d = - d;
		}
		break;
	    case SHIFT:
		intval = (int)val[i];
		if (intval > 0) {
		    d = d << intval;
		} else if (intval < 0) {
		    d = d >> (-intval);
		}
		break;
	    case AND :
		d &= (int)val[i];
		break;
	    case OR  :
		d |= (int)val[i];
		break;
	    case XOR :
		d ^= (int)val[i];
		break;
	    default  :
		fprintf(stderr, "%s: error in op\n", progname);
		bu_exit(-1, NULL);
	    /* TRUNC and ROUND do nothing because we already have
	     * integer value
	     */
	    /* case TRUNC: */
	    case ROUND:
		break;
	    }
	}
	mapbuf[j] = d & 0x0ff;
    }
}


int
main(int argc, char **argv)
{
    unsigned char *p, *q;
    int tmp;
    int n;
    unsigned long clip_high = 0L , clip_low = 0L ;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
    setmode(fileno(stderr), O_BINARY);

    progname = *argv;

    if (!get_args(argc, argv) || isatty((int)fileno(stdin))
	|| isatty((int)fileno(stdout))) {
	(void)fputs(usage, stderr);
	bu_exit(1, NULL);
    }

    if (char_arith) {
	mk_char_trans_tbl();
    } else {
	mk_trans_tbl();
    }

    while ((n = read(fileno(stdin), (void *)ibuf, (unsigned)sizeof(ibuf))) > 0) {
	/* translate */
	for (p = ibuf, q = &ibuf[n]; p < q; ++p) {
	    long i = *p;
	    if (i < 0) {
		i = 0;
	    }
	    if (i >= MAPBUFLEN) {
		*p = i = MAPBUFLEN;
	    }

	    tmp = mapbuf[i];
	    if (tmp > 255) {
		++clip_high;
		*p = 255;
	    } else if (tmp < 0) {
		++clip_low;
		*p = 0;
	    } else {
		*p = tmp;
	    }
	}
	/* output */
	if (write(fileno(stdout), (void *)ibuf, (unsigned)n) != n) {
	    fprintf(stderr, "%s: Error writing stdout\n",
		    progname);
	    bu_exit(-1, NULL);
	}
    }
    if (n < 0) {
	perror("READ ERROR");
    }

    if (clip_high != 0 || clip_low != 0) {
	fprintf(stderr, "bwmod: clipped %lu high, %lu low\n",
		(long unsigned)clip_high, (long unsigned)clip_low);
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
