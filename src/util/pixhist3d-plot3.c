/*               P I X H I S T 3 D - P L O T 3 . C
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
/** @file util/pixhist3d-plot3.c
 *
 * RGB color space utilization to unix plot.
 *
 * Plots a point for each unique RGB value found in a pix file.
 * Resolution is 128 steps along each color axis.
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "plot3.h"


FILE *fp;

unsigned char bin[128][128][16];

struct pix_element {
    unsigned char red, green, blue;
};


static const char *Usage = "Usage: pixhist3d-plot3 [file.pix] | plot\n";

int
main(int argc, char **argv)
{
    int n, x;
    struct pix_element scan[512];
    unsigned char bmask;

    if (argc > 1) {
	if ((fp = fopen(argv[1], "r")) == NULL) {
	    fprintf(stderr, "%s", Usage);
	    bu_exit(1, "pixhist3d-plot3: can't open \"%s\"\n", argv[1]);
	}
    } else
	fp = stdin;

    if (argc > 2 || isatty(fileno(fp))) {
	fputs(Usage, stderr);
	return 2;
    }

    /* Initialize plot */
    pl_3space(stdout, 0, 0, 0, 128, 128, 128);
    pl_color(stdout, 255, 0, 0);
    pl_3line(stdout, 0, 0, 0, 127, 0, 0);
    pl_color(stdout, 0, 255, 0);
    pl_3line(stdout, 0, 0, 0, 0, 127, 0);
    pl_color(stdout, 0, 0, 255);
    pl_3line(stdout, 0, 0, 0, 0, 0, 127);
    pl_color(stdout, 255, 255, 255);

    while ((n = fread(&scan[0], sizeof(*scan), 512, fp)) > 0) {
	int ridx, bidx, gidx;

	if (n > 512)
	    n = 512;

	for (x = 0; x < n; x++) {
	    ridx = scan[x].red;
	    if (ridx < 0)
		ridx = 0;
	    if (ridx > 255)
		ridx = 255;

	    gidx = scan[x].green;
	    if (gidx < 0)
		gidx = 0;
	    if (gidx > 255)
		gidx = 255;

	    bidx = scan[x].blue;
	    if (bidx < 0)
		bidx = 0;
	    if (bidx > 255)
		bidx = 255;

	    bmask = 1 << ((bidx >> 1) & 7);

	    if ((bin[ ridx>>1 ][ gidx>>1 ][ bidx>>4 ] & bmask) == 0) {
		/* New color: plot it and mark it */
		pl_3point(stdout, ridx>>1, gidx>>1, bidx>>1);
		bin[ ridx>>1 ][ gidx>>1 ][ bidx>>4 ] |= bmask;
	    }
	}
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
