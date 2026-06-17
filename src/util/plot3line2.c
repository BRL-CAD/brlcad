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
#include "vmath.h"
#include "bsg/plot3.h"


static const char usage[] = "Usage: plot3line2 x1 y1 x2 y2 [r g b]\n";

static int
parse_double_arg(const char *arg, double *out_value, const char *label)
{
    char *end = NULL;

    errno = 0;
    *out_value = strtod(arg, &end);
    if (errno != 0 || end == arg || *end != '\0') {
	bu_log("plot3line2: invalid %s '%s'\n", label, arg);
	return 0;
    }

    return 1;
}

static int
parse_color_component(const char *arg, int *out_value, const char *label)
{
    char *end = NULL;
    long int value;

    errno = 0;
    value = strtol(arg, &end, 10);
    if (errno != 0 || end == arg || *end != '\0' || value < 0 || value > 255) {
	bu_log("plot3line2: invalid %s '%s'\n", label, arg);
	return 0;
    }

    *out_value = (int)value;
    return 1;
}

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

    if (!parse_double_arg(argv[1], &x_1, "x1") ||
	!parse_double_arg(argv[2], &y_1, "y1") ||
	!parse_double_arg(argv[3], &x_2, "x2") ||
	!parse_double_arg(argv[4], &y_2, "y2")) {
	return 1;
    }

    if (argc > 5)
	if (!parse_color_component(argv[5], &r, "red component"))
	    return 1;
    if (argc > 6)
	if (!parse_color_component(argv[6], &g, "green component"))
	    return 1;
    if (argc > 7)
	if (!parse_color_component(argv[7], &b, "blue component"))
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
