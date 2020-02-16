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
mesh_uedge_t::clear()
{
    type = B_UNSET;
    v[0] = v[1] = -1;
    e[0] = e[1] = NULL;
    tri[0] = tri[1] = NULL;
    edge_ind = -1;
    nc = NULL;
    t_start = DBL_MAX;
    t_end = DBL_MAX;
    edge_pnts[0] = -1;
    edge_pnts[2] = -1;
    tangents[0] = ON_3dVector::UnsetVector;
    tangents[1] = ON_3dVector::UnsetVector;
    bb = ON_BoundingBox();
    len = 0.0;
    linear = false;
    unused = false;
    current = false;
}

void
mesh_uedge_t::bbox_update()
{
    if (type == B_SINGULAR) return;

    ON_3dPoint &p3d1 = cdt->i->s.b_pnts[v[0]].p;
    ON_3dPoint &p3d2 = cdt->i->s.b_pnts[v[1]].p;
    ON_Line line(p3d1, p3d2);
    ON_3dPoint pztol(ON_ZERO_TOLERANCE, ON_ZERO_TOLERANCE, ON_ZERO_TOLERANCE);
    bb = line.BoundingBox();
    bb.m_max = bb.m_max + pztol;
    bb.m_min = bb.m_min - pztol;

    if (type == B_BOUNDARY) {
	// For boundary edges, we want to bound the nearby neighborhood as well
	// as the line itself.  Start by bounding the portion of the NURBS curve
	// involved with this portion of the boundary
	ON_NurbsCurve sc;
	ON_Interval domain(t_start, t_end);
	nc->GetNurbForm(sc, 0.0, &domain);
	bb = sc.BoundingBox();

	// Be more aggressive - if we're close to a face edge, we need to know
	double bdist = 0.5*len;
	double xdist = bb.m_max.x - bb.m_min.x;
	double ydist = bb.m_max.y - bb.m_min.y;
	double zdist = bb.m_max.z - bb.m_min.z;
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
    }

    // TODO - update box in rtree
}

void
mesh_edge_t::bbox_update()
{
    if (type == B_SINGULAR) return;

    ON_3dPoint &p3d1 = cdt->i->s.b_pnts[v[0]].p;
    ON_3dPoint &p3d2 = cdt->i->s.b_pnts[v[1]].p;
    ON_Line line(p3d1, p3d2);
    ON_3dPoint pztol(ON_ZERO_TOLERANCE, ON_ZERO_TOLERANCE, ON_ZERO_TOLERANCE);
    bb = line.BoundingBox();
    bb.m_max = bb.m_max + pztol;
    bb.m_min = bb.m_min - pztol;

    if (type == B_BOUNDARY) {
	// Be more aggressive - if we're close to a face edge, we need to know
	double bdist = 0.5*len;
	double xdist = bb.m_max.x - bb.m_min.x;
	double ydist = bb.m_max.y - bb.m_min.y;
	double zdist = bb.m_max.z - bb.m_min.z;
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

