/*                  P I X H I S T 3 D - P L . C
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
/** @file pixhist3d-pl.c
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


static const char *Usage = "usage: pixhist3d-pl [file.pix] | plot\n";

int
main(int argc, char **argv)
{
    int n, x;
    struct pix_element scan[512];
    unsigned char bmask;

    if (argc > 1) {
	if ((fp = fopen(argv[1], "r")) == NULL) {
	    fprintf(stderr, "%s", Usage);
	    bu_exit(1, "pixhist3d-pl: can't open \"%s\"\n", argv[1]);
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
	for (x = 0; x < n; x++) {
	    bmask = 1 << ((scan[x].blue >> 1) & 7);
	    if ((bin[ scan[x].red>>1 ][ scan[x].green>>1 ][ scan[x].blue>>4 ] & bmask) == 0) {
		/* New color: plot it and mark it */
		pl_3point(stdout, scan[x].red>>1, scan[x].green>>1, scan[x].blue>>1);
		bin[ scan[x].red>>1 ][ scan[x].green>>1 ][ scan[x].blue>>4 ] |= bmask;
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
