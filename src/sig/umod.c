/*                          U M O D . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2026 United States Government as represented by
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
/** @file umod.c
 *
 * Modify intensities in a stream of short (16 bit) unsigned integers.
 *
 * Allows any number of add, subtract, multiply, divide, or
 * exponentiation operations to be performed on a picture.
 * Keeps track of and reports clipping.
 *
 */

#include "common.h"

#include <math.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/exit.h"

#include "mod_args.h"


#define BUFLEN (8192*2)	/* usually 2 pages of memory, 16KB */


static const char progname[] = "umod";
static int mapbuf[65536];		/* translation buffer/lookup table */


static void
mk_trans_tbl(const struct sig_mod_operations *operations)
{
    int i, j;
    double d;

    /* create translation map */
    for (j = 0; j < 65536; ++j) {
	d = j;
	for (i = 0; i < (int)operations->count; i++) {
	    switch (operations->values[i].type) {
		case SIG_MOD_ADD : d += operations->values[i].value; break;
		case SIG_MOD_MULTIPLY: d *= operations->values[i].value; break;
		case SIG_MOD_POWER: d = pow(d, operations->values[i].value); break;
		case SIG_MOD_ABSOLUTE: if (d < 0.0) d = - d; break;
		default  : fprintf(stderr, "%s: error in op\n",
				   progname); break;
	    }
	}

	if (d > 65535.0)
	    mapbuf[j] = 65537;
	else if (d < 0.0)
	    mapbuf[j] = -1;
	else
	    mapbuf[j] = d + 0.5;
    }
}


int
main(int argc, char *argv[])
{
    unsigned short *p, *q;
    unsigned int n;
    unsigned long clip_high, clip_low;
    int idx;

    unsigned short iobuf[BUFLEN];		/* input buffer */
    FILE *infp = stdin;
    const char *input_file = NULL;
    struct sig_mod_args args = {0};

    bu_setprogname(argv[0]);

    int parse_status = sig_mod_args_parse(&args, &input_file, progname,
	"Apply ordered arithmetic operations to unsigned shorts", argc, argv);
    if (parse_status <= 0)
	return parse_status < 0 ? 1 : 0;
    if (input_file) {
	infp = fopen(input_file, "rb");
	if (!infp)
	    bu_exit(1, "%s: cannot open \"%s\" for reading\n", progname,
		input_file);
    }
    if (isatty(fileno(infp)) || isatty(fileno(stdout)))
	bu_exit(1, "%s requires redirected binary input and output\n", progname);
    setmode(fileno(infp), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    mk_trans_tbl(&args.operations);

    clip_high = clip_low = 0;

    while ((n=fread(iobuf, sizeof(*iobuf), BUFLEN, infp)) > 0) {
	/* translate */
	for (p=iobuf, q= &iobuf[n]; p < q; ++p) {
	    idx = *p;
	    if (idx < 0)
		idx = 0;
	    else if (idx > 65535)
		idx = 65535;
	    *p = idx;

	    if (mapbuf[*p] > 65535) {
		++clip_high;
		*p = 65535;
	    } else if (mapbuf[*p] < -0) {
		++clip_low;
		*p = 0;
	    } else {
		*p = (unsigned short)mapbuf[*p];
	    }
	}
	/* output */
	if (fwrite(iobuf, sizeof(*iobuf), n, stdout) != n) {
	    bu_exit(-1, "%s: Error writing stdout\n", progname);
	}
    }

    if (clip_high != 0L || clip_low != 0L) {
	fprintf(stderr, "%s: clipped %lu high, %lu low\n", progname, (long unsigned)clip_high, (long unsigned)clip_low);
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
