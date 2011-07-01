/*                       P I X H I S T . C
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
/** @file util/pixhist.c
 *
 * Display a color histogram of a pix file.
 * 0 is top of screen, 255 bottom.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "fb.h"


#define IBUFSIZE 3*2048		/* Max read size in rgb pixels */
unsigned char ibuf[IBUFSIZE];	/* Input buffer */

long bin_r[256];		/* Histogram bins */
long bin_g[256];
long bin_b[256];
int verbose = 0;

FBIO *fbp;

static long max;
static double scalefactor;
static unsigned char line[512*3];
static FILE *fp;


int
main(int argc, char **argv)
{
    int i;

    /* check for verbose flag */
    if (argc > 1 && BU_STR_EQUAL(argv[1], "-v")) {
	verbose++;
	argv++;
	argc--;
    }

    /* look for optional input file */
    if (argc > 1) {
	if ((fp = fopen(argv[1], "r")) == 0) {
	    bu_exit(1, "pixhist: can't open \"%s\"\n", argv[1]);
	}
	argv++;
	argc--;
    } else
	fp = stdin;

    /* check usage */
    if (argc > 1 || isatty(fileno(fp))) {
	bu_exit(1, "usage: pixhist [-v] [file.pix]\n");
    }

    while ((i = fread(&ibuf[0], sizeof(*ibuf), sizeof(ibuf), fp)) > 0) {
	unsigned char *bp;
	int j;

	bp = &ibuf[0];
	for (j = i/3; j > 0; j--) {
	    bin_r[ *bp++ ]++;
	    bin_g[ *bp++ ]++;
	    bin_b[ *bp++ ]++;
	}
    }

    /* find max */
    max = 1;
    for (i = 0; i < 256; i++) {
	if (bin_r[i] > max) max = bin_r[i];
	if (bin_g[i] > max) max = bin_g[i];
	if (bin_b[i] > max) max = bin_b[i];
    }
    scalefactor = 511.0 / ((double)max);

    /* Display the max? */
    bu_log("Max bin count=%ld.  %g count/pixel\n", max, scalefactor);

    if ((fbp = fb_open(NULL, 512, 512)) == NULL) {
	bu_exit(12, "fb_open failed\n");
    }

    /* Display them */
    for (i = 0; i < 256; i++) {
	int j;
	int level;
	long npix;

	level = (int)((double)bin_r[i]*scalefactor);
	if (level > 511) level = 511;
	for (j = 0; j < level; j++) line[j*3+RED] = 255;
	for (; j < 512; j++) line[j*3+RED] = 0;
	npix = level;

	level = (int)((double)bin_g[i]*scalefactor);
	if (level > 511) level = 511;
	for (j = 0; j < level; j++) line[j*3+GRN] = 255;
	for (; j < 512; j++) line[j*3+GRN] = 0;
	if (level > npix) npix = level;

	level = (int)((double)bin_b[i]*scalefactor);
	if (level > 511) level = 511;
	for (j = 0; j < level; j++) line[j*3+BLU] = 255;
	for (; j < 512; j++) line[j*3+BLU] = 0;
	if (level > npix) npix = level;

	fb_write(fbp, 0, 2*i, line, npix);
	fb_write(fbp, 0, 2*i+1, line, npix);
	if (verbose)
	    bu_log("%3d: %10ld %10ld %10ld (%ld)\n",
		   i, bin_r[i], bin_g[i], bin_b[i], npix);
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
