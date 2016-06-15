/*              B O T _ O R I E N T E D _ B B O X . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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

#include "raytrace.h"
#include "rtgeom.h"

#include "gdiam.hpp"

extern "C" int
rt_bot_oriented_bbox(struct rt_arb_internal *bbox, struct rt_db_internal *ip, const fastf_t UNUSED(tol))
{

    struct rt_bot_internal *bot_ip;
    gdiam_point *pnt_arr = NULL;
    gdiam_bbox bb;

    RT_CK_DB_INTERNAL(ip);
    bot_ip = (struct rt_bot_internal *)ip->idb_ptr;
    RT_BOT_CK_MAGIC(bot_ip);

    /* BoT vertices to gdiam */
    pnt_arr = gdiam_convert((gdiam_real *)bot_ip->vertices, bot_ip->num_vertices);

    /* Calculate tight bbox */
    bb = gdiam_approx_mvbb_grid_sample(pnt_arr, bot_ip->num_vertices, 5, 400 );

    /* gdiam_bbox to struct rt_arb_internal */
    /* 0, 0, 0 */
    bb.get_vertex(0, 0, 0, &(bbox->pt[0][0]), &(bbox->pt[0][1]), &(bbox->pt[0][2]));
    /* 0, 1, 0 */
    bb.get_vertex(0, 1, 0, &(bbox->pt[1][0]), &(bbox->pt[1][1]), &(bbox->pt[1][2]));
    /* 0, 1, 1 */
    bb.get_vertex(0, 1, 1, &(bbox->pt[2][0]), &(bbox->pt[2][1]), &(bbox->pt[2][2]));
    /* 0, 0, 1 */
    bb.get_vertex(0, 0, 1, &(bbox->pt[3][0]), &(bbox->pt[3][1]), &(bbox->pt[3][2]));
    /* 1, 0, 0 */
    bb.get_vertex(1, 0, 0, &(bbox->pt[4][0]), &(bbox->pt[4][1]), &(bbox->pt[4][2]));
    /* 1, 1, 0 */
    bb.get_vertex(1, 1, 0, &(bbox->pt[5][0]), &(bbox->pt[5][1]), &(bbox->pt[5][2]));
    /* 1, 1, 1 */
    bb.get_vertex(1, 1, 1, &(bbox->pt[6][0]), &(bbox->pt[6][1]), &(bbox->pt[6][2]));
    /* 1, 0, 1 */
    bb.get_vertex(1, 0, 1, &(bbox->pt[7][0]), &(bbox->pt[7][1]), &(bbox->pt[7][2]));

    return 0;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
