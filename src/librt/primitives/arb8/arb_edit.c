/*                      A R B _ E D I T . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2014 United States Government as represented by
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
/** @addtogroup arb_edit */
/** @{ */
/** @file primitives/arb8/arb_edit.c
 *
 * Editing operations on arb primitives.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "rt/arb_edit.h"
#include "raytrace.h"


void
ext4to6(int pt1, int pt2, int pt3, struct rt_arb_internal *arb, fastf_t peqn[7][4])
{
    point_t pts[8];
    int i;

    VMOVE(pts[0], arb->pt[pt1]);
    VMOVE(pts[1], arb->pt[pt2]);
    VMOVE(pts[4], arb->pt[pt3]);
    VMOVE(pts[5], arb->pt[pt3]);

    /* extrude "distance" to get remaining points */
    VADD2(pts[2], pts[1], &peqn[6][0]);
    VADD2(pts[3], pts[0], &peqn[6][0]);
    VADD2(pts[6], pts[4], &peqn[6][0]);
    VMOVE(pts[7], pts[6]);

    /* copy to the original record */
    for (i=0; i<8; i++)
	VMOVE(arb->pt[i], pts[i]);
}

int
mv_edge(struct rt_arb_internal *arb,
	const vect_t thru,
	const int bp1, const int bp2,
	const int end1, const int end2,
	const vect_t dir,
	const struct bn_tol *tol,
       	fastf_t peqn[7][4])
{
    fastf_t t1, t2;

    if (bn_isect_line3_plane(&t1, thru, dir, peqn[bp1], tol) < 0 ||
	bn_isect_line3_plane(&t2, thru, dir, peqn[bp2], tol) < 0) {
	return 1;
    }

    VJOIN1(arb->pt[end1], thru, t1, dir);
    VJOIN1(arb->pt[end2], thru, t2, dir);

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
