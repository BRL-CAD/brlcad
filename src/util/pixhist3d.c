/*                     P I X H I S T 3 D . C
 * BRL-CAD
 *
 * Copyright (c) 1986-2014 United States Government as represented by
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
/** @file util/pixhist3d.c
 *
 * Display R x G, R x B, B x G on a framebuffer, where the
 * intensity is the relative frequency for that plane.
 *
 *	R		Three ortho views of this cube.
 *	| B		Front, Top, Right Side.
 *	|/
 *	+______G
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "fb.h"
#include "bu.h"


/*
 * Smallest non-zero value we will plot.
 * i.e. all non-zero points less than this will be drawn at this
 * intensity so that they will be visible.
 */
#define THRESH 20

FBIO *fbp;
FILE *fp;

long rxb[256][256], rxg[256][256], bxg[256][256];

static const int MAX_INDEX = sizeof(rxb[0]) - 1;

unsigned char ibuf[8*1024*3];

void disp_array(long int (*v)[256], int xoff, int yoff);

static const char *Usage = "Usage: pixhist3d [file.pix]\n";

int
main(int argc, char **argv)
{
    int n;

    if (argc > 1) {
	if ((fp = fopen(argv[1], "r")) == NULL) {
	    fprintf(stderr, "%s", Usage);
	    bu_exit(1, "pixhist3d: can't open \"%s\"\n", argv[1]);
	}
    } else
	fp = stdin;

    if (isatty(fileno(fp))) {
	bu_exit(2, "%s", Usage);
    }

    if ((fbp = fb_open(NULL, 512, 512)) == NULL) {
	bu_exit(12, "fb_open failed\n");
    }

#define CHECK_INDEX(idx) \
    if (idx > MAX_INDEX) { \
	bu_exit(3, "pixhist3d: read invalid index %u\n", (unsigned int)idx); \
    }

    while ((n = fread(&ibuf[0], sizeof(*ibuf), sizeof(ibuf), fp)) > 0) {
	unsigned char *bp;
	int i;
	long r, g, b;

	CHECK_INDEX(ibuf[RED]);
	CHECK_INDEX(ibuf[GRN]);
	CHECK_INDEX(ibuf[BLU]);

	bp = &ibuf[0];
	for (i = n/3; i > 0; i--, bp += 3) {
	    r = bp[RED];
	    g = bp[GRN];
	    b = bp[BLU];

	    /* sanitize no-op */
	    if (UNLIKELY(r < 0))
		r = 0;
	    if (UNLIKELY(r > 255))
		r = 255;
	    if (UNLIKELY(g < 0))
		g = 0;
	    if (UNLIKELY(g > 255))
		g = 255;
	    if (UNLIKELY(b < 0))
		b = 0;
	    if (UNLIKELY(b > 255))
		b = 255;

	    rxb[ r ][ b ]++;
	    rxg[ r ][ g ]++;
	    bxg[ b ][ g ]++;
	}
    }

    disp_array(rxg, 0, 0);
    disp_array(rxb, 256, 0);
    disp_array(bxg, 0, 256);

    fb_close(fbp);
    return 0;
}


/*
 * Display the array v[Y][X] at screen location xoff, yoff.
 */
void
disp_array(long int (*v)[256], int xoff, int yoff)
{
    int x, y;
    static long max;
    static double scale;
    unsigned char obuf[256*3];

    /* Find max value */
    max = 0;
    for (y = 0; y < 256; y++) {
	for (x = 0; x < 256; x++) {
	    if (v[y][x] > max)
		max = v[y][x];
	}
    }
    scale = 255.0 / ((double)max);

    /* plot them */
    for (y = 0; y < 256; y++) {
	for (x = 0; x < 256; x++) {
	    int value;

	    value = v[y][x] * scale;
	    if (value < THRESH && v[y][x] != 0)
		value = THRESH;
	    obuf[x*3+RED] = obuf[x*3+GRN] = obuf[x*3+BLU] = value;
	}
	fb_write(fbp, xoff, yoff+y, obuf, 256);
    }
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
