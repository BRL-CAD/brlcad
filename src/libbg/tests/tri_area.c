/*                      T R I _ A R E A . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <stdio.h>

#include "bu.h"
#include "bg.h"


int
main(int UNUSED(argc), char **argv)
{
    bu_setprogname(argv[0]);

    point_t v[3];
    double base = 1;
    double area = 1.0;
    while (area > 0) {
	base = base * 10;
	VSET(v[0], base,     base, 0.0);
	VSET(v[1], base+1,   base, 0.0);
	VSET(v[2], base+0.5, base, 1.0);
	area = bg_area_of_triangle(v[0], v[1], v[2]);
	bu_log("result(%g): %g\n", base, area);
    }

    if (base < 1e16)
	return -1;

    base = VUNITIZE_TOL;
    area = 1.0;
    while (area > 0) {
	base = base * 0.5;;
	VSET(v[0], 10,     10, 0.0);
	VSET(v[1], 10+base,   10, 0.0);
	VSET(v[2], 10+0.5*base, 10, 5.0);
	area = bg_area_of_triangle(v[0], v[1], v[2]);
	bu_log("result(%g): %g\n", base, area);
    }

    if (base > 1e-15)
	return -1;


    return 0;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
