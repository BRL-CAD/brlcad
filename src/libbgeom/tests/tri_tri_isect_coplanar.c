/*     B N _ T R I _ T R I _ I S E C T _ C O _ P L A N A R . C
 * BRL-CAD
 *
 * Copyright (c) 2011-2014 United States Government as represented by
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
#include "bn.h"


int
main(int argc, char *argv[])
{
    int expected_result = 0;
    int actual_result = 0;
    int area_flag = 0;
    float f1, f2, f3 = 0.0;
    point_t V0 = VINIT_ZERO;
    point_t V1 = VINIT_ZERO;
    point_t V2 = VINIT_ZERO;
    point_t U0 = VINIT_ZERO;
    point_t U1 = VINIT_ZERO;
    point_t U2 = VINIT_ZERO;

    if (argc != 9)
	bu_exit(1, "ERROR: input format is V0x,V0y,V0z V1x,V1y,V1z V2x,V2y,V2z U0x,U0y,U0z U1x,U1y,U1z U2x,U2y,U2z area_flag expected_result [%s]\n", argv[0]);

    sscanf(argv[1], "%f,%f,%f", &f1, &f2, &f3);
    VSET(V0, f1, f2, f3);

    sscanf(argv[2], "%f,%f,%f", &f1, &f2, &f3);
    VSET(V1, f1, f2, f3);

    sscanf(argv[3], "%f,%f,%f", &f1, &f2, &f3);
    VSET(V2, f1, f2, f3);

    sscanf(argv[4], "%f,%f,%f", &f1, &f2, &f3);
    VSET(U0, f1, f2, f3);

    sscanf(argv[5], "%f,%f,%f", &f1, &f2, &f3);
    VSET(U1, f1, f2, f3);

    sscanf(argv[6], "%f,%f,%f", &f1, &f2, &f3);
    VSET(U2, f1, f2, f3);

    sscanf(argv[7], "%d", &area_flag);
    sscanf(argv[8], "%d", &expected_result);

    actual_result = bn_tri_tri_isect_coplanar(V0, V1, V2, U0, U1, U2, area_flag);

    bu_log("result: %d\n", actual_result);

    if (expected_result == actual_result) {
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
