/*                        B N _ O B R . C
 * BRL-CAD
 *
 * Copyright (c) 2013 United States Government as represented by
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"


int
main()
{
    /* FIXME: finish this */
    point2d_t center;
    vect2d_t u, v;
    point2d_t pnts[4+1] = {{0}};
    point2d_t output_pnts[4+1] = {{0}};
    int n = 4;

    V2SET(pnts[0], 1.5, 1.5);
    V2SET(pnts[1], 3.0, 2.0);
    V2SET(pnts[2], 2.0, 2.5);
    V2SET(pnts[3], 1.0, 2.0);

    bn_2d_obr(&center, &u, &v, (const point2d_t *)pnts, n);

    bu_log("input : p1     (%f, %f)\n", V2ARGS(pnts[0]));
    bu_log("      : p2     (%f, %f)\n", V2ARGS(pnts[1]));
    bu_log("      : p3     (%f, %f)\n", V2ARGS(pnts[2]));
    bu_log("      : p4     (%f, %f)\n", V2ARGS(pnts[3]));
    bu_log("result: center (%f, %f)\n", V2ARGS(center));
    bu_log("        u      (%f, %f)\n", V2ARGS(u));
    bu_log("        v      (%f, %f)\n", V2ARGS(v));
    V2ADD3(output_pnts[2], center, u, v);
    V2SCALE(u, u, -1);
    V2ADD3(output_pnts[3], center, u, v);
    V2SCALE(v, v, -1);
    V2ADD3(output_pnts[0], center, u, v);
    V2SCALE(u, u, -1);
    V2ADD3(output_pnts[1], center, u, v);
    bu_log("        op1    (%f, %f)\n", V2ARGS(output_pnts[0]));
    bu_log("        op2    (%f, %f)\n", V2ARGS(output_pnts[1]));
    bu_log("        op3    (%f, %f)\n", V2ARGS(output_pnts[2]));
    bu_log("        op4    (%f, %f)\n", V2ARGS(output_pnts[3]));
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
