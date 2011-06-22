/*                       P L L I N E 2 . C
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
/** @file util/plline2.c
 *
 * Output a 2-D line with double coordinates in UNIX plot format.
 *
 */

#include "common.h"

#include <stdlib.h> /* for atof() */
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "plot3.h"


static const char usage[] = "Usage: plline2 x1 y1 x2 y2 [r g b]\n";

int
main(int argc, char **argv)
{
    int c;
    double x_1, y_1, x_2, y_2;
    int r = 0;
    int g = 0;
    int b = 0;

    if (argc < 5 || isatty(fileno(stdout))) {
	bu_exit(1, "%s", usage);
    }

    if (!isatty(fileno(stdin))) {
	/* Permit use in a pipeline -- copy input to output first */
	while ((c = getchar()) != EOF)
	    putchar(c);
    }

    x_1 = atof(argv[1]);
    y_1 = atof(argv[2]);
    x_2 = atof(argv[3]);
    y_2 = atof(argv[4]);

    if (argc > 5)
	r = atoi(argv[5]);
    if (argc > 6)
	g = atoi(argv[6]);
    if (argc > 7)
	b = atoi(argv[7]);

    if (argc > 5)
	pl_color(stdout, r, g, b);

    pd_line(stdout, x_1, y_1, x_2, y_2);

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
