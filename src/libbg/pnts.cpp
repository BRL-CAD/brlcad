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
    if (!NEAR_ZERO(gte_bb.extent[0], VUNITIZE_TOL))
	VSCALE(*v1, *v1, gte_bb.extent[0]);
    VSET(*v2, gte_bb.axis[1][0], gte_bb.axis[1][1], gte_bb.axis[1][2]);
    if (!NEAR_ZERO(gte_bb.extent[1], VUNITIZE_TOL))
	VSCALE(*v2, *v2, gte_bb.extent[1]);
    VSET(*v3, gte_bb.axis[2][0], gte_bb.axis[2][1], gte_bb.axis[2][2]);
    if (!NEAR_ZERO(gte_bb.extent[2], VUNITIZE_TOL))
	VSCALE(*v3, *v3, gte_bb.extent[2]);

#if 0
    // while we have the info, validate that bg_obb_pnts is doing the right
    // reassembly of the GTE class for the explicit points calculation
    point_t pt[8];
    std::array<gte::Vector3<fastf_t>, 8> calculated_bb;
    gte_bb.GetVertices(calculated_bb);

    // V0: 0,0,0
    VSET(pt[0], calculated_bb[0][0], calculated_bb[0][1],  calculated_bb[0][2]);
    // V1: 0,1,0
    VSET(pt[1], calculated_bb[1][0], calculated_bb[1][1],  calculated_bb[1][2]);
    // V2: 0,1,1
    VSET(pt[2], calculated_bb[3][0], calculated_bb[3][1],  calculated_bb[3][2]);
    // V3: 0,0,1
    VSET(pt[3], calculated_bb[2][0], calculated_bb[2][1],  calculated_bb[2][2]);
    // V4: 1,0,0
    VSET(pt[4], calculated_bb[4][0], calculated_bb[4][1],  calculated_bb[4][2]);
    // V5: 1,1,0
    VSET(pt[5], calculated_bb[5][0], calculated_bb[5][1],  calculated_bb[5][2]);
    // V6: 1,1,1
    VSET(pt[6], calculated_bb[7][0], calculated_bb[7][1],  calculated_bb[7][2]);
    // V7: 1,0,1
    VSET(pt[7], calculated_bb[6][0], calculated_bb[6][1],  calculated_bb[6][2]);


    point_t *pverts = (point_t *)bu_calloc(8, sizeof(point_t), "p");
    bg_obb_pnts(pverts, (const point_t *)c, (const vect_t *)v1, (const vect_t *)v2, (const vect_t *)v3);

    // Make sure bg_obb_pnts matches what GTE gives us
    for (size_t i = 0; i < 8; i++) {
	if (!VNEAR_EQUAL(pt[i], pverts[i], VUNITIZE_TOL)) {
	    bu_log("%g %g %g -> %g %g %g\n", V3ARGS(pt[i]), V3ARGS(pverts[i]));
	    bu_exit(-1, "WRONG!\n");
	}
    }
#endif

    return BRLCAD_OK;
}

int
bg_obb_pnts(point_t *verts, const point_t *c, const vect_t *v1, const vect_t *v2, const vect_t *v3)
{
    if (!c || !v1 || !v2 || !v3 || !verts)
	return BRLCAD_ERROR;

    vect_t evects[3];
    VMOVE(evects[0], *v1);
    VMOVE(evects[1], *v2);
    VMOVE(evects[2], *v3);

    fastf_t emags[3];
    for (size_t i = 0; i < 3; i++)
	emags[i] = MAGNITUDE(evects[i]);

    for (size_t i = 0; i < 3; i++)
	VUNITIZE(evects[i]);

    gte::OrientedBox3<fastf_t> gte_bb;
    gte_bb.center[0] = (*c)[X];
    gte_bb.center[1] = (*c)[Y];
    gte_bb.center[2] = (*c)[Z];
    for (size_t i = 0; i < 3; i++) {
	gte_bb.extent[i] = emags[i];
	for (size_t j = 0; j < 3; j++)
	    gte_bb.axis[i][j] = evects[i][j];
    }

    // success: convert gte calculated -> verts
    std::array<gte::Vector3<fastf_t>, 8> calculated_bb;
    gte_bb.GetVertices(calculated_bb);

    // V0: 0,0,0
    VSET(verts[0], calculated_bb[0][0], calculated_bb[0][1],  calculated_bb[0][2]);
    // V1: 0,1,0
    VSET(verts[1], calculated_bb[1][0], calculated_bb[1][1],  calculated_bb[1][2]);
    // V2: 0,1,1
    VSET(verts[2], calculated_bb[3][0], calculated_bb[3][1],  calculated_bb[3][2]);
    // V3: 0,0,1
    VSET(verts[3], calculated_bb[2][0], calculated_bb[2][1],  calculated_bb[2][2]);
    // V4: 1,0,0
    VSET(verts[4], calculated_bb[4][0], calculated_bb[4][1],  calculated_bb[4][2]);
    // V5: 1,1,0
    VSET(verts[5], calculated_bb[5][0], calculated_bb[5][1],  calculated_bb[5][2]);
    // V6: 1,1,1
    VSET(verts[6], calculated_bb[7][0], calculated_bb[7][1],  calculated_bb[7][2]);
    // V7: 1,0,1
    VSET(verts[7], calculated_bb[6][0], calculated_bb[6][1],  calculated_bb[6][2]);

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
