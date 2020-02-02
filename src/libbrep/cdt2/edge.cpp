/*                        C D T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2007-2019 United States Government as represented by
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
/** @addtogroup libbrep */
/** @{ */
/** @file cdt.cpp
 *
 * Constrained Delaunay Triangulation of NURBS B-Rep objects.
 *
 */

#include "common.h"
#include "bu/malloc.h"
#include "bu/vls.h"
#include "brep/pullback.h"
#include "./cdt.h"

void
mesh_uedge_t::bbox_update()
{
    if (type == B_SINGULAR) return;

    double tmid = t_start + (t_end - t_start)*0.5;
    ON_3dPoint trim_3d = nc->PointAt(tmid);

    ON_3dPoint &p3d1 = cdt->i->s.b_pnts[v[0]].p;
    ON_3dPoint &p3d2 = cdt->i->s.b_pnts[v[1]].p;
    ON_Line line(p3d1, p3d2);

    double arc_dist = 2*trim_3d.DistanceTo(line.ClosestPointTo(trim_3d));

    ON_3dPoint pztol(ON_ZERO_TOLERANCE, ON_ZERO_TOLERANCE, ON_ZERO_TOLERANCE);
    bb = line.BoundingBox();
    bb.m_max = bb.m_max + pztol;
    bb.m_min = bb.m_min - pztol;

    double bdist = (0.5*len > arc_dist) ? 0.5*len: arc_dist;
    double xdist = bb.m_max.x - bb.m_min.x;
    double ydist = bb.m_max.y - bb.m_min.y;
    double zdist = bb.m_max.z - bb.m_min.z;
    // If we're close to the edge, we want to know - the Search callback will
    // check the precise distance and make a decision on what to do.
    if (xdist < bdist) {
        bb.m_min.x = bb.m_min.x - 0.5*bdist;
        bb.m_max.x = bb.m_max.x + 0.5*bdist;
    }
    if (ydist < bdist) {
        bb.m_min.y = bb.m_min.y - 0.5*bdist;
        bb.m_max.y = bb.m_max.y + 0.5*bdist;
    }
    if (zdist < bdist) {
        bb.m_min.z = bb.m_min.z - 0.5*bdist;
        bb.m_max.z = bb.m_max.z + 0.5*bdist;
    }

    // TODO - update box in rtree
}


/** @} */

// Local Variables:
// mode: C++
// tab-width: 8
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8

