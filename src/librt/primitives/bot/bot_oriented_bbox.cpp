/*              B O T _ O R I E N T E D _ B B O X . C P P
 * BRL-CAD
 *
 * Copyright (c) 2013-2026 United States Government as represented by
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

#include "bg/obr.h"
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

    point_t *corners[8];
    for (int i = 0; i < 8; ++i)
	corners[i] = &bbox->pt[i];
    return bg_3d_obb(corners, bot_ip->vertices,
	(int)bot_ip->num_vertices);
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
