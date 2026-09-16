/*                    R E V O L V E _ B R E P . C P P
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
/** @file revolve_brep.cpp
 *
 * Convert a Revolved Sketch to b-rep form
 *
 */

#include "common.h"

#include <vector>

#include "raytrace.h"
#include "rt/geom.h"
#include "nmg.h"
#include "brep.h"
#include "primitives/brep/primitive_brep.h"
#include "primitives/sketch/sketch_brep.h"

static bool
FindLoops(ON_Brep **b, const ON_Line *revaxis, fastf_t ang,
	const struct bn_tol *tol, int *profile_loop_count)
{
    std::vector<ON_SimpleArray<ON_Curve *> > loops;
    size_t outer_index = 0;
    if (!rt_sketch_brep_order_loops(**b, tol, &loops, &outer_index)) {
	bu_log("rt_revolve_brep: sketch segments do not form closed loops!\n");
	return false;
    }

    for (size_t i = 0; i < loops.size(); ++i) {
	ON_PolyCurve *poly_curve = new ON_PolyCurve();
	for (int j = 0; j < loops[i].Count(); ++j) {
	    ON_Curve *segment = loops[i][j]->DuplicateCurve();
	    if (!segment || !poly_curve->Append(segment)) {
		delete segment;
		delete poly_curve;
		return false;
	    }
	}
	poly_curve->SynchronizeSegmentDomains();
	ON_NurbsCurve *revcurve = ON_NurbsCurve::New();
	if (poly_curve->GetNurbForm(*revcurve) <= 0) {
	    delete revcurve;
	    delete poly_curve;
	    return false;
	}
	delete poly_curve;
	ON_RevSurface *revsurf = ON_RevSurface::New();
	revsurf->m_curve = revcurve;
	revsurf->m_axis = *revaxis;
	revsurf->m_angle = ON_Interval(0, ang);
	revsurf->m_t = revsurf->m_angle;
	revsurf->m_bTransposed = false;
	revsurf->BoundingBox();
	ON_BrepFace *face = (*b)->NewFace(*revsurf);
	delete revsurf;
	if (!face)
	    return false;
	if (i == outer_index)
	    (*b)->FlipFace(*face);
    }
    if (profile_loop_count)
	*profile_loop_count = static_cast<int>(loops.size());
    return true;
}


static bool
CurveOnPlane(const ON_Curve &curve, const ON_Plane &plane, double tolerance)
{
    const ON_Interval domain = curve.Domain();
    const double parameters[] = {0.0, 0.173, 0.419, 0.731, 1.0};
    for (size_t i = 0; i < sizeof(parameters) / sizeof(parameters[0]); ++i) {
	if (fabs(plane.DistanceTo(curve.PointAt(
		domain.ParameterAt(parameters[i])))) > tolerance)
	    return false;
    }
    return true;
}


extern "C" void
rt_revolve_brep(ON_Brep **b, const struct rt_db_internal *ip, const struct bn_tol *tol)
{
    struct rt_revolve_internal *rip;
    struct rt_sketch_internal *eip;

    rip = (struct rt_revolve_internal *)ip->idb_ptr;
    RT_REVOLVE_CK_MAGIC(rip);
    eip = rip->skt;
    RT_SKETCH_CK_MAGIC(eip);

    ON_3dPoint plane_origin;
    ON_3dVector plane_x_dir, plane_y_dir;

    bool full_revolve = true;
    if (rip->ang < 2*ON_PI && rip->ang > 0)
	full_revolve = false;

    //  Find plane in 3 space corresponding to the sketch.

    vect_t startpoint;
    VADD2(startpoint, rip->v3d, rip->r);
    plane_origin = ON_3dPoint(startpoint);
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
	    bu_log("rt_revolve_brep: sketch segment %zu is invalid or unsupported\n",
		i);
	    (*b)->Destroy();
	    return;
	}
	(*b)->m_C3.Append(curve);
    }

    vect_t endpoint;
    VADD2(endpoint, rip->v3d, rip->axis3d);
    const ON_Line& revaxis = ON_Line(ON_3dPoint(rip->v3d), ON_3dPoint(endpoint));

    int profile_loop_count = 0;
    if (!FindLoops(b, &revaxis, rip->ang, tol, &profile_loop_count)) {
	(*b)->Destroy();
	return;
    }

    // Create the two boundary surfaces, if it's not a full revolution
    if (!full_revolve) {
	ON_3dVector axis_direction(rip->axis3d);
	if (!axis_direction.Unitize()) {
	    (*b)->Destroy();
	    return;
	}
	ON_Xform rotation;
	rotation.Rotation(rip->ang, axis_direction, ON_3dPoint(rip->v3d));
	ON_Plane end_plane(sketch_plane);
	if (!end_plane.Transform(rotation)) {
	    (*b)->Destroy();
	    return;
	}

	const double tolerance = (tol && tol->dist > 0.0) ?
	    tol->dist : RT_LEN_TOL;
	ON_SimpleArray<int> start_edges;
	ON_SimpleArray<int> end_edges;
	for (int i = 0; i < (*b)->m_E.Count(); ++i) {
	    const ON_BrepEdge &edge = (*b)->m_E[i];
	    if (edge.m_ti.Count() != 1 || !edge.IsClosed())
		continue;
	    const bool on_start = CurveOnPlane(edge, sketch_plane, tolerance);
	    const bool on_end = CurveOnPlane(edge, end_plane, tolerance);
	    if (on_start && !on_end)
		start_edges.Append(i);
	    else if (on_end && !on_start)
		end_edges.Append(i);
	}
	if (start_edges.Count() != profile_loop_count ||
	    end_edges.Count() != profile_loop_count) {
	    bu_log("rt_revolve_brep: could not identify exact cap boundaries\n");
	    (*b)->Destroy();
	    return;
	}

	int start_face = -1;
	int end_face = -1;
	if (!rt_brep_mate_planar_cap_loops(**b, start_edges, sketch_plane,
		false, tol, &start_face) ||
	    !rt_brep_mate_planar_cap_loops(**b, end_edges, end_plane,
		true, tol, &end_face)) {
	    (*b)->Destroy();
	    return;
	}
	(*b)->Compact();
	(*b)->SetTolerancesBoxesAndFlags(false);
	for (int i = 0; i < (*b)->m_E.Count(); ++i)
	    if ((*b)->m_E[i].m_tolerance < 0.0)
		(*b)->m_E[i].m_tolerance = tolerance;
	if (!(*b)->IsSolid()) {
	    (*b)->FlipFace((*b)->m_F[start_face]);
	    if (!(*b)->IsSolid()) {
		(*b)->FlipFace((*b)->m_F[end_face]);
		if (!(*b)->IsSolid()) {
		    (*b)->FlipFace((*b)->m_F[start_face]);
		    if (!(*b)->IsSolid()) {
			bu_log("rt_revolve_brep: cap orientations do not form a solid\n");
			(*b)->Destroy();
			return;
		    }
		}
	    }
	}
    }

    (*b)->Compact();
    (*b)->SetTolerancesBoxesAndFlags(false);
    const double final_tolerance = (tol && tol->dist > 0.0) ?
	tol->dist : RT_LEN_TOL;
    for (int i = 0; i < (*b)->m_E.Count(); ++i)
	if ((*b)->m_E[i].m_tolerance < 0.0)
	    (*b)->m_E[i].m_tolerance = final_tolerance;
    ON_wString messages;
    ON_TextLog log(messages);
    if (!(*b)->IsValid(&log) || !(*b)->IsSolid()) {
	ON_String text(messages);
	bu_log("rt_revolve_brep: failed to construct a valid solid BRep:\n%s",
	    text.Array());
	(*b)->Destroy();
    }
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
