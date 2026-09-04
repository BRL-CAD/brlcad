/*                 P R I M I T I V E _ B R E P . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file primitive_brep.h
 *
 * Shared internal helpers for implicit primitive BRep construction.
 */

#ifndef LIBRT_PRIMITIVES_BREP_PRIMITIVE_BREP_H
#define LIBRT_PRIMITIVES_BREP_PRIMITIVE_BREP_H


/* Add a planar cap trimmed by a duplicate of an existing naked closed edge,
 * then merge the duplicate vertex and edge so the cap and side are one
 * topological manifold. */
static inline bool
rt_brep_mate_planar_cap(ON_Brep &brep, int side_edge_index,
	const ON_Plane &plane, bool flip_face, const struct bn_tol *tol,
	int *cap_face_index)
{
    if (side_edge_index < 0 || side_edge_index >= brep.m_E.Count() ||
	brep.m_E[side_edge_index].m_ti.Count() != 1 ||
	!brep.m_E[side_edge_index].IsClosed())
	return false;

    ON_Curve *cap_curve = brep.m_E[side_edge_index].DuplicateCurve();
    if (!cap_curve)
	return false;

    const ON_BoundingBox bbox = cap_curve->BoundingBox();
    double umin = 0.0;
    double umax = 1.0;
    double vmin = 0.0;
    double vmax = 1.0;
    if (bbox.IsValid()) {
	bool first = true;
	for (unsigned int corner = 0; corner < 8; ++corner) {
	    const ON_3dPoint point((corner & 1) ? bbox.m_max.x : bbox.m_min.x,
		(corner & 2) ? bbox.m_max.y : bbox.m_min.y,
		(corner & 4) ? bbox.m_max.z : bbox.m_min.z);
	    double u = 0.0;
	    double v = 0.0;
	    if (!plane.ClosestPointTo(point, &u, &v))
		continue;
	    if (first) {
		umin = umax = u;
		vmin = vmax = v;
		first = false;
	    } else {
		if (u < umin) umin = u;
		if (u > umax) umax = u;
		if (v < vmin) vmin = v;
		if (v > vmax) vmax = v;
	    }
	}
    }
    const double padding = (tol && tol->dist > 0.0) ? tol->dist : RT_LEN_TOL;
    umin -= padding;
    umax += padding;
    vmin -= padding;
    vmax += padding;

    ON_PlaneSurface *cap_surface = new ON_PlaneSurface(plane);
    cap_surface->SetDomain(0, umin, umax);
    cap_surface->SetDomain(1, vmin, vmax);
    cap_surface->SetExtents(0, cap_surface->Domain(0));
    cap_surface->SetExtents(1, cap_surface->Domain(1));
    const int surface_index = brep.AddSurface(cap_surface);
    ON_BrepFace &cap_face = brep.NewFace(surface_index);
    const int face_index = cap_face.m_face_index;
    ON_SimpleArray<ON_Curve *> boundary;
    boundary.Append(cap_curve);
    const bool loop_created = brep.NewPlanarFaceLoop(face_index,
	ON_BrepLoop::outer, boundary, true);
    delete cap_curve;
    if (!loop_created)
	return false;

    const ON_BrepLoop *cap_loop = brep.m_L.Last();
    cap_surface->SetDomain(0, cap_loop->m_pbox.m_min.x, cap_loop->m_pbox.m_max.x);
    cap_surface->SetDomain(1, cap_loop->m_pbox.m_min.y, cap_loop->m_pbox.m_max.y);
    cap_surface->SetExtents(0, cap_surface->Domain(0));
    cap_surface->SetExtents(1, cap_surface->Domain(1));
    if (flip_face)
	brep.FlipFace(brep.m_F[face_index]);
    brep.SetTrimIsoFlags(brep.m_F[face_index]);

    const int cap_edge_index = brep.m_E.Count() - 1;
    const int side_vertex_index = brep.m_E[side_edge_index].m_vi[0];
    const int cap_vertex_index = brep.m_E[cap_edge_index].m_vi[0];
    if (side_vertex_index < 0 || cap_vertex_index < 0 ||
	(side_vertex_index != cap_vertex_index && !brep.CombineCoincidentVertices(
	    brep.m_V[side_vertex_index], brep.m_V[cap_vertex_index])) ||
	!brep.CombineCoincidentEdges(brep.m_E[side_edge_index],
	    brep.m_E[cap_edge_index]))
	return false;

    if (cap_face_index)
	*cap_face_index = face_index;
    return true;
}


/* Add a planar cap with one outer closed edge and zero or more closed inner
 * edges.  Every cap curve is copied from its corresponding swept boundary,
 * and each duplicate is immediately merged back into that boundary edge. */
static inline bool
rt_brep_mate_planar_cap_loops(ON_Brep &brep,
	const ON_SimpleArray<int> &side_edge_indices, const ON_Plane &plane,
	bool flip_face, const struct bn_tol *tol, int *cap_face_index)
{
    if (side_edge_indices.Count() < 1)
	return false;

    int outer_position = -1;
    double largest_diagonal = -1.0;
    ON_BoundingBox cap_bbox;
    for (int position = 0; position < side_edge_indices.Count(); ++position) {
	const int edge_index = side_edge_indices[position];
	if (edge_index < 0 || edge_index >= brep.m_E.Count() ||
	    brep.m_E[edge_index].m_ti.Count() != 1 ||
	    !brep.m_E[edge_index].IsClosed())
	    return false;
	const ON_BoundingBox edge_bbox = brep.m_E[edge_index].BoundingBox();
	if (!edge_bbox.IsValid())
	    return false;
	cap_bbox.Union(edge_bbox);
	const double diagonal = edge_bbox.Diagonal().Length();
	if (diagonal > largest_diagonal) {
	    largest_diagonal = diagonal;
	    outer_position = position;
	}
    }
    if (outer_position < 0 || !cap_bbox.IsValid())
	return false;

    double umin = 0.0;
    double umax = 0.0;
    double vmin = 0.0;
    double vmax = 0.0;
    bool first = true;
    for (unsigned int corner = 0; corner < 8; ++corner) {
	const ON_3dPoint point((corner & 1) ? cap_bbox.m_max.x : cap_bbox.m_min.x,
	    (corner & 2) ? cap_bbox.m_max.y : cap_bbox.m_min.y,
	    (corner & 4) ? cap_bbox.m_max.z : cap_bbox.m_min.z);
	double u = 0.0;
	double v = 0.0;
	if (!plane.ClosestPointTo(point, &u, &v))
	    continue;
	if (first) {
	    umin = umax = u;
	    vmin = vmax = v;
	    first = false;
	} else {
	    if (u < umin) umin = u;
	    if (u > umax) umax = u;
	    if (v < vmin) vmin = v;
	    if (v > vmax) vmax = v;
	}
    }
    if (first)
	return false;
    const double padding = (tol && tol->dist > 0.0) ? tol->dist : RT_LEN_TOL;
    ON_PlaneSurface *cap_surface = new ON_PlaneSurface(plane);
    cap_surface->SetDomain(0, umin - padding, umax + padding);
    cap_surface->SetDomain(1, vmin - padding, vmax + padding);
    cap_surface->SetExtents(0, cap_surface->Domain(0));
    cap_surface->SetExtents(1, cap_surface->Domain(1));
    const int surface_index = brep.AddSurface(cap_surface);
    ON_BrepFace &cap_face = brep.NewFace(surface_index);
    const int face_index = cap_face.m_face_index;

    for (int pass = 0; pass < side_edge_indices.Count(); ++pass) {
	const int position = (pass == 0) ? outer_position :
	    ((pass <= outer_position) ? pass - 1 : pass);
	const int side_edge_index = side_edge_indices[position];
	ON_Curve *cap_curve = brep.m_E[side_edge_index].DuplicateCurve();
	if (!cap_curve)
	    return false;
	ON_SimpleArray<ON_Curve *> boundary;
	boundary.Append(cap_curve);
	const ON_BrepLoop::TYPE loop_type = (position == outer_position) ?
	    ON_BrepLoop::outer : ON_BrepLoop::inner;
	const bool loop_created = brep.NewPlanarFaceLoop(face_index, loop_type,
	    boundary, true);
	delete cap_curve;
	const double tolerance = (tol && tol->dist > 0.0) ?
	    tol->dist : RT_LEN_TOL;
	if (!loop_created || brep_stitch_naked_edges(brep, tolerance) != 1)
	    return false;
    }

    const ON_BrepLoop *outer_loop = brep.m_F[face_index].OuterLoop();
    if (!outer_loop)
	return false;
    cap_surface->SetDomain(0, outer_loop->m_pbox.m_min.x,
	outer_loop->m_pbox.m_max.x);
    cap_surface->SetDomain(1, outer_loop->m_pbox.m_min.y,
	outer_loop->m_pbox.m_max.y);
    cap_surface->SetExtents(0, cap_surface->Domain(0));
    cap_surface->SetExtents(1, cap_surface->Domain(1));
    if (flip_face)
	brep.FlipFace(brep.m_F[face_index]);
    brep.SetTrimIsoFlags(brep.m_F[face_index]);
    if (cap_face_index)
	*cap_face_index = face_index;
    return true;
}


#endif /* LIBRT_PRIMITIVES_BREP_PRIMITIVE_BREP_H */
