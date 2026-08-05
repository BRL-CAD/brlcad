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


static inline bool
rt_brep_curves_coincident(const ON_Curve &first, const ON_Curve &second,
	double tolerance, bool *reversed)
{
    const ON_Interval first_domain = first.Domain();
    const ON_Interval second_domain = second.Domain();
    const double samples[] = {0.0, 0.173, 0.419, 0.731, 1.0};
    bool forward = true;
    bool reverse = true;
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
	const ON_3dPoint point = first.PointAt(first_domain.ParameterAt(samples[i]));
	forward = forward && point.DistanceTo(second.PointAt(
	    second_domain.ParameterAt(samples[i]))) <= tolerance;
	reverse = reverse && point.DistanceTo(second.PointAt(
	    second_domain.ParameterAt(1.0 - samples[i]))) <= tolerance;
    }
    if (reversed)
	*reversed = !forward && reverse;
    return forward || reverse;
}


/* Merge pairs of geometrically coincident naked edges.  This is intended for
 * exact primitive patches constructed from a common analytic definition, not
 * as a general tolerance-based BRep repair operation. */
static inline int
rt_brep_merge_naked_edges(ON_Brep &brep, const struct bn_tol *tol)
{
    const double tolerance = (tol && tol->dist > 0.0) ? tol->dist : RT_LEN_TOL;
    int merge_count = 0;
    bool merged = true;
    while (merged) {
	merged = false;
	for (int i = 0; i < brep.m_E.Count() && !merged; ++i) {
	    if (brep.m_E[i].m_ti.Count() != 1)
		continue;
	    for (int j = i + 1; j < brep.m_E.Count(); ++j) {
		if (brep.m_E[j].m_ti.Count() != 1)
		    continue;
		bool reversed = false;
		if (!rt_brep_curves_coincident(brep.m_E[i], brep.m_E[j],
			tolerance, &reversed))
		    continue;

		for (int endpoint = 0; endpoint < 2; ++endpoint) {
		    const int first_vertex = brep.m_E[i].m_vi[endpoint];
		    const int second_vertex = brep.m_E[j].m_vi[
			reversed ? 1 - endpoint : endpoint];
		    if (first_vertex < 0 || second_vertex < 0)
			return -1;
		    if (first_vertex != second_vertex &&
			!brep.CombineCoincidentVertices(brep.m_V[first_vertex],
			    brep.m_V[second_vertex]))
			return -1;
		}
		/* The retained edge uses the first curve's direction.  Trims from
		 * an oppositely directed duplicate must reverse their 3D sense when
		 * they are moved to that edge. */
		if (reversed) {
		    for (int k = 0; k < brep.m_E[j].m_ti.Count(); ++k) {
			const int trim_index = brep.m_E[j].m_ti[k];
			if (trim_index >= 0 && trim_index < brep.m_T.Count())
			    brep.m_T[trim_index].m_bRev3d =
				!brep.m_T[trim_index].m_bRev3d;
		    }
		}
		if (!brep.CombineCoincidentEdges(brep.m_E[i], brep.m_E[j]))
		    return -1;
		++merge_count;
		merged = true;
		break;
	    }
	}
    }
    return merge_count;
}


/* Make the face senses of a closed two-manifold consistent.  Edge topology
 * must already be exact and paired; this only solves the binary face-flip
 * constraints implied by the two trims on every edge. */
static inline bool
rt_brep_orient_faces(ON_Brep &brep)
{
    const int face_count = brep.m_F.Count();
    if (face_count < 1)
	return false;
    ON_SimpleArray<int> flip;
    for (int i = 0; i < face_count; ++i)
	flip.Append(-1);

    for (int seed = 0; seed < face_count; ++seed) {
	if (flip[seed] >= 0)
	    continue;
	flip[seed] = 0;
	bool changed = true;
	while (changed) {
	    changed = false;
	    for (int edge_index = 0; edge_index < brep.m_E.Count(); ++edge_index) {
		const ON_BrepEdge &edge = brep.m_E[edge_index];
		if (edge.m_ti.Count() != 2)
		    return false;
		const ON_BrepTrim &first_trim = brep.m_T[edge.m_ti[0]];
		const ON_BrepTrim &second_trim = brep.m_T[edge.m_ti[1]];
		if (first_trim.m_li < 0 || first_trim.m_li >= brep.m_L.Count() ||
		    second_trim.m_li < 0 || second_trim.m_li >= brep.m_L.Count())
		    return false;
		const int first_face = brep.m_L[first_trim.m_li].m_fi;
		const int second_face = brep.m_L[second_trim.m_li].m_fi;
		if (first_face < 0 || first_face >= face_count ||
		    second_face < 0 || second_face >= face_count)
		    return false;
		const int first_sense = first_trim.m_bRev3d ^
		    brep.m_F[first_face].m_bRev;
		const int second_sense = second_trim.m_bRev3d ^
		    brep.m_F[second_face].m_bRev;
		const int relation = first_sense ^ second_sense ^ 1;
		if (first_face == second_face) {
		    if (relation != 0)
			return false;
		    continue;
		}
		if (flip[first_face] >= 0 && flip[second_face] < 0) {
		    flip[second_face] = flip[first_face] ^ relation;
		    changed = true;
		} else if (flip[second_face] >= 0 && flip[first_face] < 0) {
		    flip[first_face] = flip[second_face] ^ relation;
		    changed = true;
		} else if (flip[first_face] >= 0 &&
		    flip[second_face] != (flip[first_face] ^ relation)) {
		    return false;
		}
	    }
	}
    }

    for (int i = 0; i < face_count; ++i)
	if (flip[i] == 1)
	    brep.FlipFace(brep.m_F[i]);
    return true;
}


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
	if (!loop_created || rt_brep_merge_naked_edges(brep, tol) != 1)
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
