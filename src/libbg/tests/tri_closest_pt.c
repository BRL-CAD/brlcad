/*                 T R I _ C L O S E S T _ P T . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2022 United States Government as represented by
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
main(int argc, char **argv)
{
    int expected_result = 0;
    int actual_result = 0;
    float f1, f2, f3 = 0.0;
    point_t V0 = VINIT_ZERO;
    point_t V1 = VINIT_ZERO;
    point_t V2 = VINIT_ZERO;
    point_t TP = VINIT_ZERO;

    bu_setprogname(argv[0]);

    if (argc != 6)
	bu_exit(1, "ERROR: [%s] input format is TPx,TPy,TPz V0x,V0y,V0z V1x,V1y,V1z V2x,V2y,V2z expected_result\n", argv[0]);

    sscanf(argv[1], "%f,%f,%f", &f1, &f2, &f3);
    VSET(TP, f1, f2, f3);

    sscanf(argv[2], "%f,%f,%f", &f1, &f2, &f3);
    VSET(V0, f1, f2, f3);

    sscanf(argv[3], "%f,%f,%f", &f1, &f2, &f3);
    VSET(V1, f1, f2, f3);

    sscanf(argv[4], "%f,%f,%f", &f1, &f2, &f3);
    VSET(V2, f1, f2, f3);

    sscanf(argv[5], "%d", &expected_result);

    actual_result = bg_tri_closest_pt(NULL, TP, V0, V1, V2);

    bu_log("result: %d\n", actual_result);

    if (NEAR_EQUAL(expected_result, actual_result, SMALL_FASTF)) {
	return 0;
    }

    return -1;
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
