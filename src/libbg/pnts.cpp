/*                     P N T S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2016-2025 United States Government as represented by
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
/** @file pnts.cpp
 *
 * Routines for working with pnts
 */

#include "common.h"
#include <stdlib.h>

#include "Mathematics/ContOrientedBox3.h"

#include "vmath.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bn/numgen.h"
#include "bn/rand.h"
#include "bg/pnts.h"

int
bg_pnts_aabb(point_t *min, point_t *max, const point_t *p, size_t num_pnts)
{
    /* If we can't produce any output, there's no point in continuing */
    if (!min || !max)
        return BRLCAD_ERROR;

    /* If something goes wrong with any bbox logic, we want to know it as soon
     * as possible.  Make sure as soon as we can that the bbox output is set to
     * invalid defaults, so we don't end up with something that accidentally
     * looks reasonable if things fail. */
    VSETALL((*min), INFINITY);
    VSETALL((*max), -INFINITY);

    /* If we don't have inputs, we can't do anything */
    if (!p || num_pnts == 0)
        return BRLCAD_ERROR;

    /* Check max and min of points */
    for(size_t i = 0; i < num_pnts; i++)
	VMINMAX((*min), (*max), p[i]);

    /* Make sure the RPP created is not of zero volume */
    if (NEAR_EQUAL((*min)[X], (*max)[X], SMALL_FASTF)) {
        (*min)[X] -= SMALL_FASTF;
        (*max)[X] += SMALL_FASTF;
    }
    if (NEAR_EQUAL((*min)[Y], (*max)[Y], SMALL_FASTF)) {
        (*min)[Y] -= SMALL_FASTF;
        (*max)[Y] += SMALL_FASTF;
    }
    if (NEAR_EQUAL((*min)[Z], (*max)[Z], SMALL_FASTF)) {
        (*min)[Z] -= SMALL_FASTF;
        (*max)[Z] += SMALL_FASTF;
    }

    /* Success */
    return 0;
}

int
bg_pnts_obb(point_t *c, vect_t *v1, vect_t *v2, vect_t *v3,
        const point_t *p, size_t num_pnts)
{
    /* If we can't produce any output, there's no point in continuing */
    if (!c || !v1 || !v2 || !v3)
	return BRLCAD_ERROR;

    /* If something goes wrong with any bbox logic, we want to know it as soon
     * as possible.  Make sure as soon as we can that the bbox output is set to
     * invalid defaults, so we don't end up with something that accidentally
     * looks reasonable if things fail. */
    VSETALL((*c), 0);
    VSET((*v1), INFINITY, 0, 0);
    VSET((*v2), 0, INFINITY, 0);
    VSET((*v3), 0, 0, INFINITY);

    /* If we don't have any points we can't do anything */
    if (!p || num_pnts == 0)
	return BRLCAD_ERROR;

    // Calculate the gte::OrientedBox3
    gte::OrientedBox3<fastf_t> gte_bb;
    if (!gte::GetContainer(num_pnts, (gte::Vector3<fastf_t> *)p, gte_bb))
	return BRLCAD_ERROR;

    // Success - convert GTE values to vmath types
    VSET(*c, gte_bb.center[0], gte_bb.center[1], gte_bb.center[2]);
    VSET(*v1, gte_bb.axis[0][0], gte_bb.axis[0][1], gte_bb.axis[0][2]);
    VSET(*v2, gte_bb.axis[1][0], gte_bb.axis[1][1], gte_bb.axis[1][2]);
    VSET(*v3, gte_bb.axis[2][0], gte_bb.axis[2][1], gte_bb.axis[2][2]);

    return BRLCAD_OK;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8 cino=N-s
