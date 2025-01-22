/*              B O T _ O R I E N T E D _ B B O X . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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
/** @file bot_oriented_bbox.cpp
 *
 *
 */

#include "common.h"

#include "Mathematics/ContOrientedBox3.h"

#include "raytrace.h"
#include "rt/geom.h"

/* calc oriented bounding box using GTE */
extern "C" int
rt_bot_oriented_bbox(struct rt_arb_internal *bbox, struct rt_db_internal *ip, const fastf_t UNUSED(tol))
{
    // make sure we have a bot
    struct rt_bot_internal *bot_ip;
    RT_CK_DB_INTERNAL(ip);
    bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot_ip);

    // BoT vertices to gte::OrientedBox3
    gte::OrientedBox3<fastf_t> gte_bb;
    if (gte::GetContainer(bot_ip->num_vertices, (gte::Vector3<fastf_t> *)bot_ip->vertices, gte_bb)) {
	// success: convert gte calculated -> rt_arb_internal
	std::array<gte::Vector3<fastf_t>, 8> calculated_bb;
	gte_bb.GetVertices(calculated_bb);

	// V0: 0,0,0
	VSET(bbox->pt[0], calculated_bb[0][0], calculated_bb[0][1],  calculated_bb[0][2]);
	// V1: 0,1,0
	VSET(bbox->pt[1], calculated_bb[1][0], calculated_bb[1][1],  calculated_bb[1][2]);
	// V2: 0,1,1
	VSET(bbox->pt[2], calculated_bb[3][0], calculated_bb[3][1],  calculated_bb[3][2]);
	// V3: 0,0,1
	VSET(bbox->pt[3], calculated_bb[2][0], calculated_bb[2][1],  calculated_bb[2][2]);
	// V4: 1,0,0
	VSET(bbox->pt[4], calculated_bb[4][0], calculated_bb[4][1],  calculated_bb[4][2]);
	// V5: 1,1,0
	VSET(bbox->pt[5], calculated_bb[5][0], calculated_bb[5][1],  calculated_bb[5][2]);
	// V6: 1,1,1
	VSET(bbox->pt[6], calculated_bb[7][0], calculated_bb[7][1],  calculated_bb[7][2]);
	// V7: 1,0,1
	VSET(bbox->pt[7], calculated_bb[6][0], calculated_bb[6][1],  calculated_bb[6][2]);

	// calling code is expecting -1 on failure, 0 on success
	return 0;
    }

    return -1;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
