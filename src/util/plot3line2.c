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
#include "bu/arg.h"
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
    struct bu_vls msg = BU_VLS_INIT_ZERO;

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

    if (!bu_arg_double_from_str(&msg, &x_1, argv[1]) ||
	!bu_arg_double_from_str(&msg, &y_1, argv[2]) ||
	!bu_arg_double_from_str(&msg, &x_2, argv[3]) ||
	!bu_arg_double_from_str(&msg, &y_2, argv[4])) {
	bu_log("%s: %s\n", bu_getprogname(), bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }

    if ((argc > 5 && !bu_arg_integer_range_from_str(&msg, &r, argv[5], 10, 0, 255)) ||
	(argc > 6 && !bu_arg_integer_range_from_str(&msg, &g, argv[6], 10, 0, 255)) ||
	(argc > 7 && !bu_arg_integer_range_from_str(&msg, &b, argv[7], 10, 0, 255))) {
	bu_log("%s: %s\n", bu_getprogname(), bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);

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
