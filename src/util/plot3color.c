/*                    P L O T 3 C O L O R . C
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
/** @file util/plot3color.c
 *
 * Output a color in UNIX plot format, for inclusion in a plot file.
 *
 */

#include "common.h"

#include <errno.h>
#include <stdlib.h>
#include "bio.h"

#include "bu/app.h"
#include "bu/exit.h"
#include "bu/log.h"
#include "bu/arg.h"
#include "vmath.h"
#include "bv/plot3.h"


static const char usage[] = "Usage: plot3color r g b\n";

int
main(int argc, char **argv)
{
    int c;
    int r, g, b;
    struct bu_vls msg = BU_VLS_INIT_ZERO;

    bu_setprogname(argv[0]);

    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);

    if (argc != 4 || isatty(fileno(stdout))) {
	bu_exit(1, "%s", usage);
    }

    if (!isatty(fileno(stdin))) {
	/* Permit use in a pipeline -- copy input to output first */
	while ((c = getchar()) != EOF)
	    putchar(c);
    }

    if (!bu_arg_integer_range_from_str(&msg, &r, argv[1], 10, 0, 255) ||
	!bu_arg_integer_range_from_str(&msg, &g, argv[2], 10, 0, 255) ||
	!bu_arg_integer_range_from_str(&msg, &b, argv[3], 10, 0, 255)) {
	bu_log("%s: %s\n", bu_getprogname(), bu_vls_cstr(&msg));
	bu_vls_free(&msg);
	return 1;
    }
    bu_vls_free(&msg);

    pl_color(stdout, r, g, b);
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
