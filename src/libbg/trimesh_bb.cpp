/*                 T R I M E S H _ O B B . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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

#include <vector>

#include <string.h>

#include "Mathematics/ContOrientedBox3.h"

#include "vmath.h"
#include "bu/bitv.h"
#include "bg/pnts.h"
#include "bg/trimesh.h"

int
bg_trimesh_aabb(point_t *min, point_t *max, const int *faces, size_t num_faces, const point_t *p, size_t num_pnts)
{
    /* If we can't produce any output, there's no point in continuing */
    if (!min || !max)
	return -1;

    /* If something goes wrong with any bbox logic, we want to know it as soon
     * as possible.  Make sure as soon as we can that the bbox output is set to
     * invalid defaults, so we don't end up with something that accidentally
     * looks reasonable if things fail. */
    VSETALL((*min), INFINITY);
    VSETALL((*max), -INFINITY);

    /* If we don't have faces, see if we can calculate with just points */
    if (!faces || num_faces == 0 || !p || num_pnts == 0)
	return bg_pnts_aabb(min, max, p, num_pnts);

    /* First Pass: coherently iterate through all faces of the BoT and
     * mark vertices in a bit-vector that are referenced by a face. */
    struct bu_bitv *visit_vert = bu_bitv_new(num_pnts);
    for (size_t tri_index = 0; tri_index < num_faces; tri_index++) {
	BU_BITSET(visit_vert, faces[tri_index*3 + X]);
	BU_BITSET(visit_vert, faces[tri_index*3 + Y]);
	BU_BITSET(visit_vert, faces[tri_index*3 + Z]);
    }

    /* Second Pass: check max and min of vertices marked */
    for(size_t vert_index = 0; vert_index < num_pnts; vert_index++){
	if(BU_BITTEST(visit_vert,vert_index)){
	    VMINMAX((*min), (*max), p[vert_index]);
	}
    }

    /* Done with bitv */
    bu_bitv_free(visit_vert);

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
bg_trimesh_obb(point_t *c, vect_t *v1, vect_t *v2, vect_t *v3,
	const int *faces, size_t num_faces, const point_t *p, size_t num_pnts)
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

    /* If we don't have any faces, but we DO have points, we can still
     * produce a bbox */
    if (!faces || num_faces == 0)
	return bg_pnts_obb(c, v1, v2, v3, p, num_pnts);

    /* First Pass: coherently iterate through all faces of the BoT and
     * mark vertices in a bit-vector that are referenced by a face. */
    struct bu_bitv *visit_vert = bu_bitv_new(num_pnts);
    for (size_t tri_index = 0; tri_index < num_faces; tri_index++) {
	BU_BITSET(visit_vert, faces[tri_index*3 + X]);
	BU_BITSET(visit_vert, faces[tri_index*3 + Y]);
	BU_BITSET(visit_vert, faces[tri_index*3 + Z]);
    }

    /* Second Pass: Build up a vector of active pnts per the faces */
    std::vector<fastf_t> apnts;
    size_t apnt_cnt = 0;
    for(size_t vert_index = 0; vert_index < num_pnts; vert_index++){
	if(BU_BITTEST(visit_vert,vert_index)){
	    apnts.push_back(p[vert_index][0]);
	    apnts.push_back(p[vert_index][1]);
	    apnts.push_back(p[vert_index][2]);
	    apnt_cnt++;
	}
    }

    // Calculate the gte::OrientedBox3
    gte::OrientedBox3<fastf_t> gte_bb;
    if (!gte::GetContainer(apnt_cnt, (gte::Vector3<fastf_t> *)apnts.data(), gte_bb))
	return BRLCAD_ERROR;

    // Success - convert GTE values to vmath types
    VSET(*c, gte_bb.center[0], gte_bb.center[1], gte_bb.center[2]);
    VSET(*v1, gte_bb.axis[0][0], gte_bb.axis[0][1], gte_bb.axis[0][2]);
    VSCALE(*v1, *v1, gte_bb.extent[0]);
    VSET(*v2, gte_bb.axis[1][0], gte_bb.axis[1][1], gte_bb.axis[1][2]);
    VSCALE(*v2, *v2, gte_bb.extent[1]);
    VSET(*v3, gte_bb.axis[2][0], gte_bb.axis[2][1], gte_bb.axis[2][2]);
    VSCALE(*v3, *v3, gte_bb.extent[2]);

    return BRLCAD_OK;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
