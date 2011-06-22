/*                        B W H I S T . C
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
/** @file util/bwhist.c
 *
 * Display, and optionally dump to tty, a histogram of a
 * black and white file.  Black is top of screen, white bottom.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "bio.h"

#include "fb.h"
#include "bu.h"


long bin[256];
int verbose = 0;
FBIO *fbp;

static char *Usage = "usage: bwhist [-v] [file.bw]\n";


int
main(int argc, char **argv)
{
    int i;
    int n;
    long max;
    static double scale;
    unsigned char buf[512];
    unsigned char *bp;
    unsigned char white[3*512];
    FILE *fp;

    /* check for verbose flag */
    if (argc > 1 && BU_STR_EQUAL(argv[1], "-v")) {
	verbose++;
	argv++;
	argc--;
    }

    /* look for optional input file */
    if (argc > 1) {
	if ((fp = fopen(argv[1], "r")) == 0) {
	    bu_exit(1, "bwhist: can't open '%s'\n", argv[1]);
	}
	argv++;
	argc--;
    } else
	fp = stdin;

    /* check usage */
    if (argc > 1 || isatty(fileno(fp))) {
	bu_exit(1, "%s", Usage);
    }

    for (i = 0; i < 3*512; i++)
	white[i] = 255;

    while ((n = fread(buf, sizeof(*buf), sizeof(buf), fp)) > 0) {
	bp = &buf[0];
	for (i = 0; i < n; i++)
	    bin[ *bp++ ]++;
    }

    /* find max */
    max = 1;
    for (i = 0; i < 256; i++)
	if (bin[i] > max) max = bin[i];
    scale = 511.0 / (double)max;

    /* Display the max? */
    printf("Full screen = %ld pixels\n", max);

    if ((fbp = fb_open(NULL, 512, 512)) == NULL) {
	bu_exit(12, "fb_open failed\n");
    }

    /* Display them */
    for (i = 0; i < 256; i++) {
	int value;
	value = bin[i]*scale;
	if (value == 0 && bin[i] != 0) value = 1;
	fb_write(fbp, 0, 2*i, white, value);
	fb_write(fbp, 0, 2*i+1, white, value);
	if (verbose)
	    printf("%3d: %10ld (%10f)\n", i, bin[i], (float)bin[i]/(float)max);
    }

    fb_close(fbp);
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
