/*                    S K E T C H _ B R E P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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
/** @file sketch_brep.cpp
 *
 * Convert a sketch to b-rep form (does not create a solid brep)
 *
 */

#include "common.h"

#include <vector>

#include "raytrace.h"
#include "rt/geom.h"
#include "brep.h"
#include "./sketch_brep.h"

static bool
FindLoops(ON_Brep **b, const struct bn_tol *tol)
{
    std::vector<ON_SimpleArray<ON_Curve *> > loops;
    size_t outer_index = 0;
    if (!rt_sketch_brep_order_loops(**b, tol, &loops, &outer_index)) {
	bu_log("rt_sketch_brep: sketch segments do not form closed loops!\n");
	return false;
    }

    if (!(*b)->NewPlanarFaceLoop(0, ON_BrepLoop::outer,
	    loops[outer_index], true))
	return false;
    for (size_t i = 0; i < loops.size(); ++i) {
	if (i != outer_index && !(*b)->NewPlanarFaceLoop(0,
		ON_BrepLoop::inner, loops[i], true))
	    return false;
    }
    return true;
}


extern "C" void
rt_sketch_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct rt_sketch_internal *eip;

    RT_CK_DB_INTERNAL(ip);
    eip = (struct rt_sketch_internal *)ip->idb_ptr;
    RT_SKETCH_CK_MAGIC(eip);

    ON_3dPoint plane_origin;
    ON_3dVector plane_x_dir, plane_y_dir;

    //  Find plane in 3 space corresponding to the sketch.

    plane_origin = ON_3dPoint(eip->V);
    plane_x_dir = ON_3dVector(eip->u_vec);
    plane_y_dir = ON_3dVector(eip->v_vec);
    const ON_Plane sketch_plane = ON_Plane(plane_origin, plane_x_dir, plane_y_dir);
    ON_Xform embedding;
    if (!rt_sketch_brep_embedding(&embedding, plane_origin, plane_x_dir,
	    plane_y_dir)) {
	(*b)->Destroy();
	return;
    }

    for (size_t i = 0; i < eip->curve.count; ++i) {
	enum rt_sketch_brep_curve_status status;
	ON_Curve *curve = rt_sketch_brep_curve(eip, i, embedding, tol,
	    &status);
	if (status == RT_SKETCH_BREP_CURVE_DEGENERATE)
	    continue;
	if (!curve) {
	    bu_log("rt_sketch_brep: segment %zu is invalid or unsupported\n", i);
	    (*b)->Destroy();
	    return;
	}
	(*b)->m_C3.Append(curve);
    }

    // Create the plane surface and brep face.
    ON_PlaneSurface *sketch_surf = new ON_PlaneSurface(sketch_plane);
    (*b)->m_S.Append(sketch_surf);
    int surfindex = (*b)->m_S.Count();
    ON_BrepFace& face = (*b)->NewFace(surfindex - 1);

    // For the purposes of BREP creation, it is necessary to identify
    // loops created by sketch segments.  This information is not stored
    // in the sketch data structures themselves, and thus must be deduced
    if (!FindLoops(b, tol)) {
	(*b)->Destroy();
	return;
    }
    const ON_BrepLoop* tloop = (*b)->m_L.First();
    sketch_surf->SetDomain(0, tloop->m_pbox.m_min.x, tloop->m_pbox.m_max.x);
    sketch_surf->SetDomain(1, tloop->m_pbox.m_min.y, tloop->m_pbox.m_max.y);
    sketch_surf->SetExtents(0, sketch_surf->Domain(0));
    sketch_surf->SetExtents(1, sketch_surf->Domain(1));
    (*b)->SetTrimIsoFlags(face);
    (*b)->FlipFace(face);
    (*b)->Compact();
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
