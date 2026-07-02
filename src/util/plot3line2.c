/*                     P L O T 3 L I N E 2 . C
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
/** @file util/plot3line2.c
 *
 * Output a 2-D line with double coordinates in UNIX plot format.
 *
 */

#include "common.h"

#include <errno.h>
#include <stdlib.h> /* for atof() */
#include <math.h>
#include <string.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/opt.h"
#include "vmath.h"
#include "bv/plot3.h"


static const char usage[] = "Usage: plot3line2 x1 y1 x2 y2 [r g b]\n";

int
main(int argc, char **argv)
{
    int c;
    double x_1, y_1, x_2, y_2;
    int r = 0;
    int g = 0;
    int b = 0;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (argc < 5 || argc > 8 || isatty(fileno(stdout))) {
	bu_exit(1, "%s", usage);
    }

    if (!isatty(fileno(stdin))) {
	/* Permit use in a pipeline -- copy input to output first */
	while ((c = getchar()) != EOF)
	    putchar(c);
    }

    if (!bu_opt_scan_double(argv[1], &x_1, "x1") ||
	!bu_opt_scan_double(argv[2], &y_1, "y1") ||
	!bu_opt_scan_double(argv[3], &x_2, "x2") ||
	!bu_opt_scan_double(argv[4], &y_2, "y2")) {
	return 1;
    }

    if (argc > 5)
	if (!bu_opt_scan_int_range(argv[5], &r, 0, 255, "red component"))
	    return 1;
    if (argc > 6)
	if (!bu_opt_scan_int_range(argv[6], &g, 0, 255, "green component"))
	    return 1;
    if (argc > 7)
	if (!bu_opt_scan_int_range(argv[7], &b, 0, 255, "blue component"))
	    return 1;

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
