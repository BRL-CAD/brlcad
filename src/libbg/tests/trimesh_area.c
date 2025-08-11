/*                    T R I M E S H _ A R E A . C
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
main(int UNUSED(argc), const char *argv[])
{
    bu_setprogname(argv[0]);

    point_t verts[4];
    int faces[6] = {0, 1, 2, 1, 2, 3};

    for (int i = 0; i < 10; i++) {
	VSET(verts[0], 0, 0, 7);
	VSET(verts[1], 0, i, 7);
	VSET(verts[2], i, i, 7);
	VSET(verts[3], i, 0, 7);
	fastf_t square_area = (fastf_t)i*i;
	fastf_t tri_area = bg_trimesh_area((const int *)faces, 2, (const point_t *)verts, 4);
	bu_log("tri_area: %0.17g, square area: %0.17g\n", tri_area, square_area);
	if (!NEAR_EQUAL(tri_area, square_area, 2*SMALL_FASTF))
	    return -1;
	if (!i)
	    continue;
	VSET(verts[3], i, 0, 10);
	tri_area = bg_trimesh_area((const int *)faces, 2, (const point_t *)verts, 4);
	bu_log("tri_area: %0.17g, square area: %0.17g\n", tri_area, square_area);
	if (NEAR_EQUAL(tri_area, square_area, 2*SMALL_FASTF))
	    return -1;
    }

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
