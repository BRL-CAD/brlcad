/*                       P L C O L O R . C
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
/** @file util/plcolor.c
 *
 * Output a color in UNIX plot format, for inclusion in a plot file.
 *
 */

#include "common.h"

#include <stdlib.h>
#include "bio.h"

#include "bu.h"
#include "vmath.h"
#include "plot3.h"


static const char usage[] = "Usage: plcolor r g b\n";

int
main(int argc, char **argv)
{
    int c;
    int r, g, b;

    if (argc != 4 || isatty(fileno(stdout))) {
	bu_exit(1, "%s", usage);
    }

    if (!isatty(fileno(stdin))) {
	/* Permit use in a pipeline -- copy input to output first */
	while ((c = getchar()) != EOF)
	    putchar(c);
    }

    r = atoi(argv[1]);
    g = atoi(argv[2]);
    b = atoi(argv[3]);

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
