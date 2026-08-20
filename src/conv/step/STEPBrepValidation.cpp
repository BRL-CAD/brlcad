/*               S T E P B R E P V A L I D A T I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/** @file step/STEPBrepValidation.cpp
 *
 * Transactional OpenNURBS validation shared by the STEP importers.
 */

#include "common.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <set>
#include <vector>

#include "raytrace.h"

#include "STEPBrepValidation.h"

namespace brlcad {
namespace step {

static constexpr int kDegenerateEdgeValidationSegments = 64;

double
DirectedTrimEndpointRatio(const ON_BrepTrim &trim, double model_tolerance)
{
    const ON_BrepEdge *edge = trim.Edge();
    const ON_BrepFace *face = trim.Face();
    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
    if (!edge || !surface)
	return DBL_MAX;

    const ON_3dPoint start_uv = trim.PointAtStart();
    const ON_3dPoint end_uv = trim.PointAtEnd();
    const ON_3dPoint start_lift = surface->PointAt(start_uv.x, start_uv.y);
    const ON_3dPoint end_lift = surface->PointAt(end_uv.x, end_uv.y);
    const ON_Interval edge_domain = edge->Domain();
    if (!edge_domain.IsIncreasing())
	return DBL_MAX;
    const ON_3dPoint edge_start = edge->PointAt(
	edge_domain[trim.m_bRev3d ? 1 : 0]);
    const ON_3dPoint edge_end = edge->PointAt(
	edge_domain[trim.m_bRev3d ? 0 : 1]);
    if (!start_lift.IsValid() || !end_lift.IsValid() ||
	    !edge_start.IsValid() || !edge_end.IsValid())
	return DBL_MAX;

    const double tolerance = std::max(model_tolerance,
	std::max(edge->m_tolerance,
	    std::max(trim.m_tolerance[0], trim.m_tolerance[1])));
    return std::max(start_lift.DistanceTo(edge_start),
	end_lift.DistanceTo(edge_end)) /
	std::max(tolerance, DBL_EPSILON);
}


bool
DirectedTrimEndpointRegressed(const ON_BrepTrim &before,
	const ON_BrepTrim &after, double model_tolerance)
{
    const double before_ratio =
	DirectedTrimEndpointRatio(before, model_tolerance);
    const double after_ratio =
	DirectedTrimEndpointRatio(after, model_tolerance);

    /* Ignore discrepancies already accepted by the trim's own tolerance and
     * ordinary floating-point noise.  A two-order-of-magnitude increase is
     * never incidental: the Panzer VII regression moved an artificial seam
     * endpoint from its exact edge by 0.171 mm at a 5.13e-5 mm tolerance,
     * over 3,300 times the permitted distance. */
    return after_ratio > 1.0 &&
	(!std::isfinite(after_ratio) ||
	 after_ratio > 100.0 * std::max(1.0, before_ratio));
}


bool
FaceTrimValidationRegressed(const ON_Brep &before, const ON_Brep &after,
	int face_index, double model_tolerance)
{
    if (face_index < 0 || face_index >= before.m_F.Count() ||
	    face_index >= after.m_F.Count())
	return true;

    const ON_BrepFace &before_face = before.m_F[face_index];
    const ON_BrepFace &after_face = after.m_F[face_index];
    std::set<int> before_trims;
    std::set<int> after_trims;
    for (int fli = 0; fli < before_face.m_li.Count(); ++fli) {
	const int li = before_face.m_li[fli];
	if (li < 0 || li >= before.m_L.Count())
	    return true;
	const ON_BrepLoop &loop = before.m_L[li];
	for (int lti = 0; lti < loop.TrimCount(); ++lti)
	    before_trims.insert(loop.m_ti[lti]);
    }
    for (int fli = 0; fli < after_face.m_li.Count(); ++fli) {
	const int li = after_face.m_li[fli];
	if (li < 0 || li >= after.m_L.Count())
	    return true;
	const ON_BrepLoop &loop = after.m_L[li];
	for (int lti = 0; lti < loop.TrimCount(); ++lti)
	    after_trims.insert(loop.m_ti[lti]);
    }
    if (before_trims != after_trims)
	return true;

    for (std::set<int>::const_iterator ti = before_trims.begin();
	    ti != before_trims.end(); ++ti) {
	if (*ti < 0 || *ti >= before.m_T.Count() ||
		*ti >= after.m_T.Count())
	    return true;
	ON_wString before_messages;
	ON_TextLog before_log(before_messages);
	ON_wString after_messages;
	ON_TextLog after_log(after_messages);
	if (before.m_T[*ti].IsValid(&before_log) &&
		!after.m_T[*ti].IsValid(&after_log))
	    return true;
	if (DirectedTrimEndpointRegressed(before.m_T[*ti], after.m_T[*ti],
		model_tolerance))
	    return true;
    }
    return false;
}


bool
FaceOrientationConstraintsAreConsistent(const ON_Brep &brep)
{
    struct Constraint {
	int other;
	int parity;
    };
    std::vector<std::vector<Constraint> > graph(
	static_cast<size_t>(brep.m_F.Count()));
    for (int ei = 0; ei < brep.m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep.m_E[ei];
	if (edge.m_ti.Count() != 2)
	    continue;
	const ON_BrepTrim *first = brep.Trim(edge.m_ti[0]);
	const ON_BrepTrim *second = brep.Trim(edge.m_ti[1]);
	const ON_BrepFace *first_face = first ? first->Face() : NULL;
	const ON_BrepFace *second_face = second ? second->Face() : NULL;
	if (!first || !second || !first_face || !second_face ||
		first_face == second_face)
	    continue;
	const int first_index = first_face->m_face_index;
	const int second_index = second_face->m_face_index;
	if (first_index < 0 || first_index >= brep.m_F.Count() ||
		second_index < 0 || second_index >= brep.m_F.Count())
	    return false;
	const bool first_effective = first->m_bRev3d ^ first_face->m_bRev;
	const bool second_effective = second->m_bRev3d ^ second_face->m_bRev;
	const int parity = 1 ^ static_cast<int>(first_effective) ^
	    static_cast<int>(second_effective);
	graph[static_cast<size_t>(first_index)].push_back(
	    {second_index, parity});
	graph[static_cast<size_t>(second_index)].push_back(
	    {first_index, parity});
    }

    std::vector<int> flip(static_cast<size_t>(brep.m_F.Count()), -1);
    for (int seed = 0; seed < brep.m_F.Count(); ++seed) {
	if (flip[static_cast<size_t>(seed)] >= 0 ||
		graph[static_cast<size_t>(seed)].empty())
	    continue;
	std::vector<int> pending(1, seed);
	flip[static_cast<size_t>(seed)] = 0;
	while (!pending.empty()) {
	    const int face = pending.back();
	    pending.pop_back();
	    const std::vector<Constraint> &constraints =
		graph[static_cast<size_t>(face)];
	    for (std::vector<Constraint>::const_iterator constraint =
		    constraints.begin(); constraint != constraints.end();
		    ++constraint) {
		const int required = flip[static_cast<size_t>(face)] ^
		    constraint->parity;
		int &assigned = flip[static_cast<size_t>(constraint->other)];
		if (assigned < 0) {
		    assigned = required;
		    pending.push_back(constraint->other);
		} else if (assigned != required) {
		    return false;
		}
	    }
	}
    }
    return true;
}


size_t
FaceOrientationConflictCount(const ON_Brep &brep)
{
    size_t conflicts = 0;
    for (int ei = 0; ei < brep.m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep.m_E[ei];
	if (edge.m_ti.Count() != 2)
	    continue;
	const ON_BrepTrim *first = brep.Trim(edge.m_ti[0]);
	const ON_BrepTrim *second = brep.Trim(edge.m_ti[1]);
	const ON_BrepFace *first_face = first ? first->Face() : NULL;
	const ON_BrepFace *second_face = second ? second->Face() : NULL;
	if (!first || !second || !first_face || !second_face ||
		first_face == second_face)
	    continue;
	const bool first_effective = first->m_bRev3d ^ first_face->m_bRev;
	const bool second_effective = second->m_bRev3d ^ second_face->m_bRev;
	if (first_effective == second_effective)
	    ++conflicts;
    }
    return conflicts;
}


bool
LoopOrientationFlipPlan(const ON_Brep &brep, std::vector<int> &flip,
	std::string *failure)
{
    struct Constraint {
	int other;
	int parity;
	int edge;
    };
    if (failure)
	failure->clear();
    flip.assign(static_cast<size_t>(brep.m_L.Count()), -1);
    std::vector<std::vector<Constraint> > graph(
	static_cast<size_t>(brep.m_L.Count()));

    for (int ei = 0; ei < brep.m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep.m_E[ei];
	if (edge.m_ti.Count() != 2)
	    continue;
	const ON_BrepTrim *first = brep.Trim(edge.m_ti[0]);
	const ON_BrepTrim *second = brep.Trim(edge.m_ti[1]);
	const ON_BrepFace *first_face = first ? first->Face() : NULL;
	const ON_BrepFace *second_face = second ? second->Face() : NULL;
	if (!first || !second || !first_face || !second_face)
	    continue;
	if (first->m_li < 0 || first->m_li >= brep.m_L.Count() ||
		second->m_li < 0 || second->m_li >= brep.m_L.Count()) {
	    if (failure)
		*failure = "reciprocal edge references an invalid loop";
	    return false;
	}
	const bool first_effective = first->m_bRev3d ^ first_face->m_bRev;
	const bool second_effective = second->m_bRev3d ^ second_face->m_bRev;
	const int parity = 1 ^ static_cast<int>(first_effective) ^
	    static_cast<int>(second_effective);
	if (first->m_li == second->m_li) {
	    if (parity == 0)
		continue;
	    if (failure)
		*failure = "STEP edge " +
		    std::to_string(edge.m_edge_user.i) + " has two agreeing "
		    "uses in loop L" + std::to_string(first->m_li);
	    return false;
	}
	graph[static_cast<size_t>(first->m_li)].push_back(
	    {second->m_li, parity, ei});
	graph[static_cast<size_t>(second->m_li)].push_back(
	    {first->m_li, parity, ei});
    }

    for (int seed = 0; seed < brep.m_L.Count(); ++seed) {
	if (flip[static_cast<size_t>(seed)] >= 0 ||
		graph[static_cast<size_t>(seed)].empty())
	    continue;
	std::vector<int> component;
	std::vector<int> pending(1, seed);
	flip[static_cast<size_t>(seed)] = 0;
	while (!pending.empty()) {
	    const int loop_index = pending.back();
	    pending.pop_back();
	    component.push_back(loop_index);
	    const std::vector<Constraint> &constraints =
		graph[static_cast<size_t>(loop_index)];
	    for (std::vector<Constraint>::const_iterator constraint =
		    constraints.begin(); constraint != constraints.end();
		    ++constraint) {
		const int required = flip[static_cast<size_t>(loop_index)] ^
		    constraint->parity;
		int &assigned = flip[static_cast<size_t>(constraint->other)];
		if (assigned < 0) {
		    assigned = required;
		    pending.push_back(constraint->other);
		} else if (assigned != required) {
		    if (failure) {
			const ON_BrepEdge *conflict =
			    brep.Edge(constraint->edge);
			*failure = "loop parity conflict at STEP edge " +
			    std::to_string(conflict ?
				conflict->m_edge_user.i : 0) + " between L" +
			    std::to_string(loop_index) + " and L" +
			    std::to_string(constraint->other);
		    }
		    return false;
		}
	    }
	}
	size_t flipped = 0;
	for (std::vector<int>::const_iterator loop_index = component.begin();
		loop_index != component.end(); ++loop_index)
	    flipped += flip[static_cast<size_t>(*loop_index)] != 0;
	if (flipped > component.size() - flipped)
	    for (std::vector<int>::const_iterator loop_index = component.begin();
		    loop_index != component.end(); ++loop_index)
		flip[static_cast<size_t>(*loop_index)] ^= 1;
    }
    return true;
}


bool
LoopHasCompletePeriodicPoleTopology(const ON_BrepLoop &loop)
{
    bool has_singular = false;
    bool has_paired_seam = false;
    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	const ON_BrepTrim *trim = loop.Trim(lti);
	if (!trim)
	    continue;
	if (trim->m_type == ON_BrepTrim::singular || trim->m_ei < 0)
	    has_singular = true;
	const ON_BrepEdge *edge = trim->Edge();
	if (!edge || edge->m_edge_user.i > 0 || edge->m_ti.Count() != 2)
	    continue;
	const ON_Brep *brep = trim->Brep();
	const ON_BrepTrim *first = brep ? brep->Trim(edge->m_ti[0]) : NULL;
	const ON_BrepTrim *second = brep ? brep->Trim(edge->m_ti[1]) : NULL;
	if (first && second && first->m_li == loop.m_loop_index &&
		second->m_li == loop.m_loop_index)
	    has_paired_seam = true;
    }
    return has_singular && has_paired_seam;
}


bool
DuplicateBoundaryEdgeUsesAreOnDistinctFaces(const ON_BrepEdge &first,
	const ON_BrepEdge &second)
{
    const ON_Brep *brep = first.Brep();
    if (!brep || second.Brep() != brep ||
	    first.m_ti.Count() != 1 || second.m_ti.Count() != 1)
	return false;
    const ON_BrepTrim *first_trim = brep->Trim(first.m_ti[0]);
    const ON_BrepTrim *second_trim = brep->Trim(second.m_ti[0]);
    return first_trim && second_trim &&
	first_trim->m_type == ON_BrepTrim::boundary &&
	second_trim->m_type == ON_BrepTrim::boundary &&
	first_trim->Face() && second_trim->Face() &&
	first_trim->Face() != second_trim->Face();
}


bool
DuplicateStepEdgeEndpointsMatch(const ON_BrepVertex &first,
	const ON_BrepVertex &second, double model_tolerance,
	double edge_tolerance, bool same_step_edge)
{
    if (!(model_tolerance > 0.0) || !(edge_tolerance > 0.0) ||
	    !first.point.IsValid() || !second.point.IsValid())
	return false;

    const int first_step = first.m_vertex_user.i;
    const int second_step = second.m_vertex_user.i;
    /* Never replace an authoritative endpoint with an importer-created split
     * vertex merely because they are close. */
    if ((first_step > 0) != (second_step > 0))
	return false;

    double tolerance = edge_tolerance;
    if (first.m_tolerance > 0.0)
	tolerance = std::max(tolerance, first.m_tolerance);
    if (second.m_tolerance > 0.0)
	tolerance = std::max(tolerance, second.m_tolerance);

    if (first_step > 0 && first_step != second_step) {
	if (!same_step_edge)
	    return false;
	/* A face-local conversion can remove a source EDGE_CURVE whose two
	 * distinct STEP vertices and complete locus are inside the declared
	 * uncertainty.  An adjacent shared edge may consequently retain either
	 * endpoint identity in its independently converted uses.  Restore that
	 * one source edge identity only within the declared model tolerance,
	 * never a larger locally measured edge tolerance. */
	tolerance = std::min(tolerance, model_tolerance);
    }
    return first.point.DistanceTo(second.point) <= tolerance;
}


bool
ImporterSplitEdgeIsToleranceDegenerate(const ON_BrepEdge &edge,
	double model_tolerance)
{
    const ON_Brep *brep = edge.Brep();
    if (!brep || !(model_tolerance > 0.0) || edge.m_edge_user.i <= 0 ||
	    edge.m_ti.Count() != 1 || edge.m_vi[0] < 0 ||
	    edge.m_vi[1] < 0 || edge.m_vi[0] == edge.m_vi[1] ||
	    edge.m_vi[0] >= brep->m_V.Count() ||
	    edge.m_vi[1] >= brep->m_V.Count() || !edge.EdgeCurveOf())
	return false;
    const ON_BrepVertex &first = brep->m_V[edge.m_vi[0]];
    const ON_BrepVertex &second = brep->m_V[edge.m_vi[1]];
    if ((first.m_vertex_user.i > 0 && second.m_vertex_user.i > 0) ||
	    !first.point.IsValid() || !second.point.IsValid() ||
	    first.point.DistanceTo(second.point) > model_tolerance)
	return false;

    const ON_Interval domain = edge.Domain();
    if (!domain.IsIncreasing())
	return false;
    for (int sample = 0; sample <= kDegenerateEdgeValidationSegments;
	    ++sample) {
	const ON_3dPoint point = edge.PointAt(domain.ParameterAt(
	    static_cast<double>(sample) /
	    kDegenerateEdgeValidationSegments));
	if (!point.IsValid() ||
		point.DistanceTo(first.point) > model_tolerance)
	    return false;
    }
    return true;
}

} // namespace step
} // namespace brlcad
