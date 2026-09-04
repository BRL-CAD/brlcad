/*                  A S S E M B L Y . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file libbrep/assembly.cpp */

#include "common.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "brep.h"


namespace {

static const double curve_samples[] = {
    0.0, 0.091, 0.217, 0.353, 0.5, 0.647, 0.783, 0.909, 1.0
};


struct edge_match {
    int first;
    int second;
    bool reversed;
};


int
naked_edge_count(const ON_Brep &brep)
{
    int count = 0;
    for (int i = 0; i < brep.m_E.Count(); ++i)
	if (brep.m_E[i].m_ti.Count() == 1)
	    ++count;
    return count;
}


int
nonmanifold_edge_count(const ON_Brep &brep)
{
    int count = 0;
    for (int i = 0; i < brep.m_E.Count(); ++i)
	if (brep.m_E[i].m_ti.Count() > 2)
	    ++count;
    return count;
}


bool
merge_edge_pair(ON_Brep &brep, int first_index, int second_index,
	bool reversed, double tolerance, enum brep_assembly_error *error)
{
    for (int endpoint = 0; endpoint < 2; ++endpoint) {
	const int first_vertex = brep.m_E[first_index].m_vi[endpoint];
	const int second_vertex = brep.m_E[second_index].m_vi[
	    reversed ? 1 - endpoint : endpoint];
	if (first_vertex < 0 || second_vertex < 0) {
	    *error = BREP_ASSEMBLY_VERTEX_MERGE_FAILED;
	    return false;
	}
	if (first_vertex == second_vertex)
	    continue;
	ON_BrepVertex &first = brep.m_V[first_vertex];
	ON_BrepVertex &second = brep.m_V[second_vertex];
	const double separation = first.Point().DistanceTo(second.Point());
	if (!std::isfinite(separation) || separation > tolerance) {
	    *error = BREP_ASSEMBLY_VERTEX_MERGE_FAILED;
	    return false;
	}
	first.m_tolerance = std::max(first.m_tolerance, separation);
	second.m_tolerance = std::max(second.m_tolerance, separation);
	if (!brep.CombineCoincidentVertices(first, second)) {
	    *error = BREP_ASSEMBLY_VERTEX_MERGE_FAILED;
	    return false;
	}
    }

    /* CombineCoincidentEdges requires identical vertex order.  Reverse the
     * duplicate edge as a semantic operation; OpenNURBS also updates all of
     * that edge's trim senses. */
    if (reversed && !brep.m_E[second_index].Reverse()) {
	*error = BREP_ASSEMBLY_EDGE_MERGE_FAILED;
	return false;
    }
    if (!brep.CombineCoincidentEdges(brep.m_E[first_index],
	    brep.m_E[second_index])) {
	*error = BREP_ASSEMBLY_EDGE_MERGE_FAILED;
	return false;
    }
    return true;
}


void
set_unset_tolerances(ON_Brep &brep, double tolerance)
{
    for (int i = 0; i < brep.m_V.Count(); ++i)
	if (!std::isfinite(brep.m_V[i].m_tolerance) ||
		brep.m_V[i].m_tolerance < 0.0)
	    brep.m_V[i].m_tolerance = tolerance;
    for (int i = 0; i < brep.m_E.Count(); ++i)
	if (!std::isfinite(brep.m_E[i].m_tolerance) ||
		brep.m_E[i].m_tolerance < 0.0)
	    brep.m_E[i].m_tolerance = tolerance;
    for (int i = 0; i < brep.m_T.Count(); ++i)
	for (int axis = 0; axis < 2; ++axis)
	    if (!std::isfinite(brep.m_T[i].m_tolerance[axis]) ||
		    brep.m_T[i].m_tolerance[axis] < 0.0)
		brep.m_T[i].m_tolerance[axis] = tolerance;
}

}


bool
brep_curves_coincident(const ON_Curve &first, const ON_Curve &second,
	double tolerance, bool *reversed)
{
    if (!std::isfinite(tolerance) || tolerance < 0.0)
	return false;
    const ON_Interval first_domain = first.Domain();
    const ON_Interval second_domain = second.Domain();
    if (!first_domain.IsIncreasing() || !second_domain.IsIncreasing())
	return false;

    bool forward = true;
    bool reverse = true;
    for (size_t i = 0; i < sizeof(curve_samples) / sizeof(curve_samples[0]); ++i) {
	const ON_3dPoint point = first.PointAt(
	    first_domain.ParameterAt(curve_samples[i]));
	const ON_3dPoint forward_point = second.PointAt(
	    second_domain.ParameterAt(curve_samples[i]));
	const ON_3dPoint reverse_point = second.PointAt(
	    second_domain.ParameterAt(1.0 - curve_samples[i]));
	if (!point.IsValid() || !forward_point.IsValid() ||
		!reverse_point.IsValid())
	    return false;
	forward = forward && point.DistanceTo(forward_point) <= tolerance;
	reverse = reverse && point.DistanceTo(reverse_point) <= tolerance;
	if (!forward && !reverse)
	    return false;
    }
    if (reversed)
	*reversed = !forward && reverse;
    return forward || reverse;
}


int
brep_stitch_naked_edges(ON_Brep &brep, double tolerance,
	brep_assembly_result *result)
{
    brep_assembly_result local;
    brep_assembly_result &report = result ? *result : local;
    report = brep_assembly_result();
    if (!std::isfinite(tolerance) || tolerance <= 0.0) {
	report.error = BREP_ASSEMBLY_INVALID_TOLERANCE;
	return -1;
    }
    report.input_naked_edges = naked_edge_count(brep);

    std::vector<int> match_counts(brep.m_E.Count(), 0);
    std::vector<ON_BoundingBox> edge_boxes(brep.m_E.Count());
    std::vector<edge_match> matches;
    ON_RTree edge_tree(report.input_naked_edges);
    for (int first = 0; first < brep.m_E.Count(); ++first) {
	if (brep.m_E[first].m_ti.Count() != 1)
	    continue;
	if (!brep.m_E[first].GetBoundingBox(edge_boxes[first], false) ||
		!edge_boxes[first].IsValid())
	    continue;
	edge_tree.Insert(edge_boxes[first].m_min, edge_boxes[first].m_max, first);
    }
    for (int first = 0; first < brep.m_E.Count(); ++first) {
	if (!edge_boxes[first].IsValid())
	    continue;
	const double search_min[3] = {
	    edge_boxes[first].m_min.x - tolerance,
	    edge_boxes[first].m_min.y - tolerance,
	    edge_boxes[first].m_min.z - tolerance
	};
	const double search_max[3] = {
	    edge_boxes[first].m_max.x + tolerance,
	    edge_boxes[first].m_max.y + tolerance,
	    edge_boxes[first].m_max.z + tolerance
	};
	ON_SimpleArray<int> candidates;
	edge_tree.Search(search_min, search_max, candidates);
	for (int candidate = 0; candidate < candidates.Count(); ++candidate) {
	    const int second = candidates[candidate];
	    if (second <= first)
		continue;
	    bool reversed = false;
	    if (!brep_curves_coincident(brep.m_E[first], brep.m_E[second],
		    tolerance, &reversed))
		continue;
	    ++match_counts[first];
	    ++match_counts[second];
	    matches.push_back({first, second, reversed});
	}
    }
    report.ambiguous_edges = std::count_if(match_counts.begin(),
	match_counts.end(), [](int count) { return count > 1; });
    for (const edge_match &match : matches) {
	if (match_counts[match.first] != 1 ||
		match_counts[match.second] != 1)
	    continue;
	if (!merge_edge_pair(brep, match.first, match.second, match.reversed,
		tolerance, &report.error))
	    return -1;
	++report.merged_edges;
    }
    if (report.merged_edges > 0 &&
	    (!brep.CullUnusedEdges() || !brep.CullUnusedVertices() ||
	     !brep.CullUnused3dCurves())) {
	report.error = BREP_ASSEMBLY_CULL_FAILED;
	return -1;
    }
    report.remaining_naked_edges = naked_edge_count(brep);
    report.nonmanifold_edges = nonmanifold_edge_count(brep);
    return report.merged_edges;
}


bool
brep_orient_faces(ON_Brep &brep)
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


bool
brep_assemble(ON_Brep &brep, double tolerance,
	brep_assembly_result *result)
{
    brep_assembly_result local;
    brep_assembly_result &report = result ? *result : local;
    if (brep_stitch_naked_edges(brep, tolerance, &report) < 0)
	return false;

    if (report.remaining_naked_edges == 0 && report.nonmanifold_edges == 0) {
	report.oriented = brep_orient_faces(brep);
        if (!report.oriented)
	    report.error = BREP_ASSEMBLY_ORIENTATION_FAILED;
    }
    brep.SetTrimTolerances(false);
    brep.SetTrimIsoFlags();
    brep.SetTrimTypeFlags();
    brep.SetVertexTolerances(true);
    brep.SetTrimBoundingBoxes(false);
    set_unset_tolerances(brep, tolerance);

    ON_wString messages;
    ON_TextLog log(messages);
    report.valid = brep.IsValid(&log);
    report.solid = report.valid && brep.IsSolid();
    if (!report.valid && report.error == BREP_ASSEMBLY_OK)
	report.error = BREP_ASSEMBLY_VALIDATION_FAILED;
    return report.valid;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
