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


int
find_edge_type(struct brep_cdt *s, ON_BrepEdge &edge)
{
    const ON_Curve* crv = edge.EdgeCurveOf();

    // Singularity
    if (!crv) {
	return 0;
    }

    // Curved edge
    if (!crv->IsLinear(BN_TOL_DIST)) {
	return 1;
    }

    // NOTE: for what we're actually using this for, it would be better to
    // detect when the surface breakdown isn't yet terminated according to the
    // surfaces tolerances, but overlaps an edge bbox with the segment longer
    // than the box dimenension - and in that case, split the edge curves
    // according to what the surface needs locally.  The drawback to that approach is
    // it complicates the workflow - right now it's all edges then all surface
    // points, and if we start changing boundary edges in response to surface
    // point sampling we're going to greatly complicate parallelization.
    // However, trying to do it this way we're getting cases where a very small
    // edge curve in a loop is resulting in dense tessellations for entire
    // surfaces as the edge curves get chopped up in response...

    // Linear edge, at least one non-planar surface
    const ON_Surface *s1= edge.Trim(0)->SurfaceOf();
    const ON_Surface *s2= edge.Trim(1)->SurfaceOf();
    if (!s1->IsPlanar(NULL, BN_TOL_DIST) || !s2->IsPlanar(NULL, BN_TOL_DIST)) {
	return 2;
    }

    // Linear edge, at least one associated non-linear edge
    if (s->i->s.b_pnts[edge.Vertex(0)->m_vertex_index].on_curved_edge || s->i->s.b_pnts[edge.Vertex(1)->m_vertex_index].on_curved_edge) {
	return 3;
    }

    // Linear edge, only associated with linear edges and planar faces
    return 4;
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

