/* BRL-CAD
 *
 * Copyright (c) 1994-2026 United States Government as represented by
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
/** @file step/STEPBrepTopology.cpp
 *
 * Core BREP topology normalization and exact boundary-edge reconstruction.
 * Compiled as one schema-neutral importer build unit.
 */

#include "common.h"
#include "STEPBrepRepairInternal.h"

namespace step_brep_detail {
using namespace step_import_detail;

bool
regenerate_trim_polyline(ON_Brep *brep, ON_BrepTrim &trim,
	const ON_Surface *surface, const ON_NurbsCurve &edge_nurbs,
	double tolerance, std::string *failure_reason,
	PeriodicPullbackCrossing *periodic_crossing,
	const ON_3dPoint *required_start, const ON_3dPoint *required_end,
	bool prefer_edge_driven, STEPWrapper *wrapper,
	bool preserve_required_uv_images, ON_Curve **generated_curve);

ON_3dPoint closed_surface_point_at(const ON_Surface *surface,
    const ON_3dPoint &uv);


/* Detect one source EDGE_CURVE which cannot participate in either of its two
 * face loops: one endpoint is incident only to this edge, while deleting the
 * edge use exposes the same well-defined topology vertex in both neighboring
 * loops.  This is a topological contradiction, not a short-edge tolerance
 * repair.  Merely note the candidate during the ordinary build; only the
 * transactional permissive inference retry may delete both uses, and that
 * retry is accepted only if the complete BREP later validates as a solid. */
bool
remove_one_proven_shared_dangling_spur(ON_Brep *brep,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper || !(LocalUnits::tolerance > 0.0))
	return false;

    size_t verbose_rejections = 0;
    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_edge_user.i <= 0 || edge.m_ti.Count() != 2 ||
		edge.m_vi[0] < 0 || edge.m_vi[1] < 0 ||
		edge.m_vi[0] >= brep->m_V.Count() ||
		edge.m_vi[1] >= brep->m_V.Count() ||
		edge.m_vi[0] == edge.m_vi[1] || edge.IsClosed())
	    continue;

	int incidence[2] = {0, 0};
	for (int candidate_index = 0; candidate_index < brep->m_E.Count();
		++candidate_index) {
	    const ON_BrepEdge &candidate = brep->m_E[candidate_index];
	    for (int endpoint = 0; endpoint < 2; ++endpoint) {
		if (candidate.m_vi[0] == edge.m_vi[endpoint] ||
			candidate.m_vi[1] == edge.m_vi[endpoint])
		    ++incidence[endpoint];
	    }
	}
	int isolated_endpoint = -1;
	if (incidence[0] == 1 && incidence[1] > 1)
	    isolated_endpoint = 0;
	else if (incidence[1] == 1 && incidence[0] > 1)
	    isolated_endpoint = 1;
	if (isolated_endpoint < 0)
	    continue;
	const int anchor_endpoint = 1 - isolated_endpoint;
	const int isolated_vertex = edge.m_vi[isolated_endpoint];
	const int anchor_vertex = edge.m_vi[anchor_endpoint];

	/* The source edge itself must still be an exact, usable curve.  Its
	 * deletion is justified exclusively by the impossible loop topology, not
	 * by replacing another curve/vertex mismatch with a more convenient one. */
	const double edge_tolerance = std::max(LocalUnits::tolerance,
	    edge.m_tolerance);
	const ON_3dPoint edge_start = edge.PointAtStart();
	const ON_3dPoint edge_end = edge.PointAtEnd();
	if (!edge_start.IsValid() || !edge_end.IsValid() ||
		edge_start.DistanceTo(brep->m_V[edge.m_vi[0]].point) >
		    edge_tolerance ||
		edge_end.DistanceTo(brep->m_V[edge.m_vi[1]].point) >
		    edge_tolerance) {
	    if (wrapper->Verbose() && verbose_rejections < 16) {
		std::cerr << entity_type << " #" << entity_id
		    << ": shared dangling-spur candidate STEP edge #"
		    << edge.m_edge_user.i << " rejected because its curve "
		       "endpoints miss the topology vertices by "
		    << edge_start.DistanceTo(brep->m_V[edge.m_vi[0]].point)
		    << '/' << edge_end.DistanceTo(
			brep->m_V[edge.m_vi[1]].point) << " (tolerance "
		    << edge_tolerance << ')' << std::endl;
		++verbose_rejections;
	    }
	    continue;
	}

	const ON_BrepTrim *uses[2] = {NULL, NULL};
	bool proof = true;
	std::string proof_failure;
	double maximum_exposed_lift_mismatch = 0.0;
	for (int use_index = 0; use_index < 2 && proof; ++use_index) {
	    const int trim_index = edge.m_ti[use_index];
	    if (trim_index < 0 || trim_index >= brep->m_T.Count()) {
		proof = false;
		proof_failure = "an edge use had an invalid trim index";
		break;
	    }
	    uses[use_index] = &brep->m_T[trim_index];
	    const ON_BrepTrim *use = uses[use_index];
	    const ON_BrepLoop *loop = use->Loop();
	    const ON_BrepFace *face = use->Face();
	    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	    if (!loop || !face || !surface || loop->m_loop_user.i <= 0 ||
		    loop->TrimCount() < 3 ||
		    !((use->m_vi[0] == anchor_vertex &&
		       use->m_vi[1] == isolated_vertex) ||
		      (use->m_vi[1] == anchor_vertex &&
		       use->m_vi[0] == isolated_vertex))) {
		proof = false;
		proof_failure = "an edge use was not a source loop traversal "
		    "between the anchor and isolated vertices";
		break;
	    }
	    int loop_offset = -1;
	    for (int lti = 0; lti < loop->TrimCount(); ++lti) {
		const ON_BrepTrim *candidate = loop->Trim(lti);
		if (candidate && candidate->m_trim_index == trim_index) {
		    loop_offset = lti;
		    break;
		}
	    }
	    if (loop_offset < 0) {
		proof = false;
		proof_failure = "an edge use was absent from its loop trim order";
		break;
	    }
	    const ON_BrepTrim *previous = loop->Trim(
		(loop_offset + loop->TrimCount() - 1) % loop->TrimCount());
	    const ON_BrepTrim *next = loop->Trim(
		(loop_offset + 1) % loop->TrimCount());
	    if (!previous || !next || previous == next ||
		    previous->m_vi[1] != anchor_vertex ||
		    next->m_vi[0] != anchor_vertex) {
		proof = false;
		std::ostringstream reason;
		reason << "deleting use T" << trim_index
		    << " would expose topology vertices "
		    << (previous ? previous->m_vi[1] : -1) << '/'
		    << (next ? next->m_vi[0] : -1)
		    << " instead of common anchor V" << anchor_vertex;
		proof_failure = reason.str();
		break;
	    }
	    const ON_BrepEdge *previous_edge = previous->Edge();
	    const ON_BrepEdge *next_edge = next->Edge();
	    const ON_3dPoint &anchor = brep->m_V[anchor_vertex].point;
	    const auto adjacent_edge_endpoint_distance = [anchor_vertex, &anchor](
		const ON_BrepEdge *adjacent) {
		double distance = DBL_MAX;
		if (!adjacent)
		    return distance;
		if (adjacent->m_vi[0] == anchor_vertex)
		    distance = std::min(distance,
			adjacent->PointAtStart().DistanceTo(anchor));
		if (adjacent->m_vi[1] == anchor_vertex)
		    distance = std::min(distance,
			adjacent->PointAtEnd().DistanceTo(anchor));
		return distance;
	    };
	    const double previous_edge_distance =
		adjacent_edge_endpoint_distance(previous_edge);
	    const double next_edge_distance =
		adjacent_edge_endpoint_distance(next_edge);
	    const double previous_edge_tolerance = previous_edge ?
		std::max(LocalUnits::tolerance, previous_edge->m_tolerance) :
		LocalUnits::tolerance;
	    const double next_edge_tolerance = next_edge ?
		std::max(LocalUnits::tolerance, next_edge->m_tolerance) :
		LocalUnits::tolerance;
	    if (previous_edge_distance > previous_edge_tolerance ||
		    next_edge_distance > next_edge_tolerance) {
		proof = false;
		std::ostringstream reason;
		reason << "the 3-D edges exposed by deleting use T" << trim_index
		    << " did not both terminate at anchor V" << anchor_vertex
		    << " (distances " << previous_edge_distance << '/'
		    << next_edge_distance << ", tolerances "
		    << previous_edge_tolerance << '/' << next_edge_tolerance
		    << ')';
		proof_failure = reason.str();
		break;
	    }
	    const ON_3dPoint previous_lift = closed_surface_point_at(surface,
		previous->PointAtEnd());
	    const ON_3dPoint next_lift = closed_surface_point_at(surface,
		next->PointAtStart());
	    if (previous_lift.IsValid())
		maximum_exposed_lift_mismatch = std::max(
		    maximum_exposed_lift_mismatch,
		    previous_lift.DistanceTo(anchor));
	    if (next_lift.IsValid())
		maximum_exposed_lift_mismatch = std::max(
		    maximum_exposed_lift_mismatch,
		    next_lift.DistanceTo(anchor));
	}
	if (!proof || !uses[0] || !uses[1] ||
		uses[0]->Loop() == uses[1]->Loop() ||
		uses[0]->Face() == uses[1]->Face()) {
	    if (wrapper->Verbose() && verbose_rejections < 16) {
		std::cerr << entity_type << " #" << entity_id
		    << ": shared dangling-spur candidate STEP edge #"
		    << edge.m_edge_user.i << " rejected: "
		    << (!proof_failure.empty() ? proof_failure :
			"the two uses were not on distinct neighboring loops")
		    << std::endl;
		++verbose_rejections;
	    }
	    continue;
	}
	/* Do not require reciprocal shell senses on the spur itself.  A degree-one
	 * endpoint already proves that the source edge cannot belong to a closed
	 * manifold, and malformed writers may repeat the same erroneous sense in
	 * both faces.  The exposed ordinary edges and final whole-solid validation
	 * are the authority for the recovered shell orientation. */

	wrapper->NoteCurveInferenceCandidate();
	if (!wrapper->CurveInferenceTrialEnabled()) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": STEP edge #" << edge.m_edge_user.i
		    << " is a candidate shared dangling topology spur; isolated "
		       "vertex V" << isolated_vertex << " has no other incident "
		       "edge and deleting both uses recloses their loops"
		    << std::endl;
	    return false;
	}

	const int source_edge_id = edge.m_edge_user.i;
	const int source_loop_ids[2] = {
	    uses[0]->Loop()->m_loop_user.i,
	    uses[1]->Loop()->m_loop_user.i
	};
	const int trim_indices[2] = {
	    uses[0]->m_trim_index,
	    uses[1]->m_trim_index
	};
	const double contradiction_span = edge_start.DistanceTo(edge_end);
	std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
	if (trim_indices[0] < 0 ||
		trim_indices[0] >= candidate->m_T.Count() ||
		trim_indices[1] < 0 ||
		trim_indices[1] >= candidate->m_T.Count())
	    continue;
	candidate->DeleteTrim(candidate->m_T[trim_indices[0]], false);
	candidate->DeleteTrim(candidate->m_T[trim_indices[1]], true);
	if (!candidate->Compact())
	    continue;
	*brep = *candidate;

	std::ostringstream detail;
	detail << "removed both uses of a source edge whose isolated endpoint "
	    << "had degree one and whose deletion exactly reclosed STEP loops #"
	    << source_loop_ids[0] << " and #" << source_loop_ids[1]
	    << " in 3-D topology; maximum pre-regeneration exposed pcurve lift "
	    << "mismatch was " << maximum_exposed_lift_mismatch << " mm";
	wrapper->RecordInferredCurve(source_edge_id,
	    "shared_dangling_topology_spur_removal", contradiction_span, 0.0,
	    contradiction_span, LocalUnits::tolerance, detail.str());
	wrapper->RecordRepair(source_edge_id, "EDGE_CURVE", "edge_geometry",
	    "removed a provably dangling shared topology spur from both "
	    "neighboring face loops");
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	    source_edge_id, "EDGE_CURVE", "geometry_inference",
	    "source topology used a nonzero edge as a shared dangling spur; "
	    "permissive import removed both uses after proving the far vertex "
	    "had no other incident edge and both exposed 3-D edge joins used "
	    "the same authored topology vertex");
	return true;
    }
    return false;
}


/* A malformed EDGE_LOOP can concatenate two independently closed cycles at
 * one topology vertex without a reciprocal keyhole bridge.  On a singly
 * closed surface, an ordinary cycle which winds exactly once cannot bound a
 * disk.  If the other cycle is contractible, their internal topology vertices
 * are disjoint, and every supplied pcurve endpoint still agrees with its
 * asserted vertex, the noncontractible cycle is a detached source lobe rather
 * than part of the usable face boundary.
 *
 * This is intentionally inference, not bounded repair.  The ordinary build
 * only marks the candidate.  A permissive transaction may remove the lobe,
 * and the caller will publish that choice only after the complete BREP (and a
 * solid, when one was asserted) passes final validation. */
bool
remove_one_noncontractible_repeated_vertex_lobe(ON_Brep *brep,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper ||
	    wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe ||
	    !(LocalUnits::tolerance > 0.0))
	return false;

    struct CycleAnalysis {
	bool valid;
	int winding;
	std::set<int> internal_vertices;
	std::vector<int> trim_indices;
	std::vector<int> edge_ids;

	CycleAnalysis() : valid(false), winding(0) {}
    };

    for (int li = 0; li < brep->m_L.Count(); ++li) {
	ON_BrepLoop &loop = brep->m_L[li];
	ON_BrepFace *face = loop.Face();
	const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	const int trim_count = loop.TrimCount();
	if (!face || !surface || face->LoopCount() != 1 || trim_count < 4 ||
		loop.m_loop_user.i <= 0)
	    continue;
	int closed_direction = -1;
	for (int direction = 0; direction < 2; ++direction) {
	    if (!surface->IsClosed(direction))
		continue;
	    if (closed_direction >= 0) {
		closed_direction = -2;
		break;
	    }
	    closed_direction = direction;
	}
	if (closed_direction < 0)
	    continue;
	const double period = surface->Domain(closed_direction).Length();
	if (!(period > ON_ZERO_TOLERANCE))
	    continue;

	const auto analyze_cycle = [brep, &loop, surface, closed_direction,
		period](const std::vector<int> &offsets, int anchor_vertex) {
	    CycleAnalysis analysis;
	    if (offsets.size() < 2 || anchor_vertex < 0 ||
		    anchor_vertex >= brep->m_V.Count())
		return analysis;
	    const ON_BrepTrim *first = loop.Trim(offsets.front());
	    if (!first || first->m_vi[0] != anchor_vertex)
		return analysis;
	    const ON_3dPoint cycle_start = first->PointAtStart();
	    if (!cycle_start.IsValid())
		return analysis;
	    ON_3dPoint current = cycle_start;
	    for (size_t oi = 0; oi < offsets.size(); ++oi) {
		const ON_BrepTrim *trim = loop.Trim(offsets[oi]);
		const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
		if (!trim || !edge || trim->m_type == ON_BrepTrim::seam ||
			trim->m_type == ON_BrepTrim::singular ||
			trim->m_type == ON_BrepTrim::slit ||
			trim->m_vi[0] < 0 || trim->m_vi[1] < 0 ||
			trim->m_vi[0] >= brep->m_V.Count() ||
			trim->m_vi[1] >= brep->m_V.Count() ||
			edge->m_edge_user.i <= 0)
		    return analysis;
		const ON_3dPoint start_uv = trim->PointAtStart();
		const ON_3dPoint end_uv = trim->PointAtEnd();
		const ON_3dPoint start_lift =
		    closed_surface_point_at(surface, start_uv);
		const ON_3dPoint end_lift =
		    closed_surface_point_at(surface, end_uv);
		double start_tolerance = std::max(LocalUnits::tolerance,
		    std::max(edge->m_tolerance,
			std::max(trim->m_tolerance[0], trim->m_tolerance[1])));
		start_tolerance = std::max(start_tolerance,
		    brep->m_V[trim->m_vi[0]].m_tolerance);
		double end_tolerance = std::max(LocalUnits::tolerance,
		    std::max(edge->m_tolerance,
			std::max(trim->m_tolerance[0], trim->m_tolerance[1])));
		end_tolerance = std::max(end_tolerance,
		    brep->m_V[trim->m_vi[1]].m_tolerance);
		if (!start_uv.IsValid() || !end_uv.IsValid() ||
			!start_lift.IsValid() || !end_lift.IsValid() ||
			start_lift.DistanceTo(
			    brep->m_V[trim->m_vi[0]].point) > start_tolerance ||
			end_lift.DistanceTo(
			    brep->m_V[trim->m_vi[1]].point) > end_tolerance)
		    return analysis;
		if (oi > 0) {
		    const ON_BrepTrim *previous = loop.Trim(offsets[oi - 1]);
		    if (!previous || previous->m_vi[1] != trim->m_vi[0])
			return analysis;
		}
		ON_3dPoint aligned_start = start_uv;
		ON_3dPoint aligned_end = end_uv;
		const double shift = round((current[closed_direction] -
		    aligned_start[closed_direction]) / period) * period;
		aligned_start[closed_direction] += shift;
		aligned_end[closed_direction] += shift;
		current = aligned_end;
		analysis.trim_indices.push_back(trim->m_trim_index);
		analysis.edge_ids.push_back(edge->m_edge_user.i);
		analysis.internal_vertices.insert(trim->m_vi[0]);
		analysis.internal_vertices.insert(trim->m_vi[1]);
	    }
	    const ON_BrepTrim *last = loop.Trim(offsets.back());
	    if (!last || last->m_vi[1] != anchor_vertex)
		return analysis;
	    analysis.internal_vertices.erase(anchor_vertex);
	    const double turns =
		(current[closed_direction] - cycle_start[closed_direction]) /
		period;
	    const int integral_turns = static_cast<int>(round(turns));
	    /* A source edge/surface association may end a small, already
	     * tolerance-validated distance from the repeated topology vertex.
	     * That residual is not periodic winding.  Keep the classification far
	     * from a half-turn ambiguity while the endpoint-to-vertex proofs above
	     * supply the actual model-space bound. */
	    const double maximum_turn_residual = std::max(
		kPeriodicParameterSnapFraction, 0.05);
	    if (fabs(turns - integral_turns) > maximum_turn_residual)
		return analysis;
	    analysis.winding = integral_turns;
	    analysis.valid = true;
	    return analysis;
	};

	for (int first_offset = 0; first_offset < trim_count; ++first_offset) {
	    const ON_BrepTrim *first = loop.Trim(first_offset);
	    if (!first || first->m_vi[0] < 0)
		continue;
	    const int anchor_vertex = first->m_vi[0];
	    for (int second_offset = first_offset + 2;
		    second_offset < trim_count; ++second_offset) {
		const ON_BrepTrim *second = loop.Trim(second_offset);
		const int first_count = second_offset - first_offset;
		const int second_count = trim_count - first_count;
		if (!second || second->m_vi[0] != anchor_vertex ||
			first_count < 2 || second_count < 2)
		    continue;

		std::vector<int> first_cycle;
		std::vector<int> second_cycle;
		for (int offset = first_offset; offset < second_offset; ++offset)
		    first_cycle.push_back(offset);
		for (int offset = second_offset; offset < trim_count; ++offset)
		    second_cycle.push_back(offset);
		for (int offset = 0; offset < first_offset; ++offset)
		    second_cycle.push_back(offset);
		const CycleAnalysis first_analysis = analyze_cycle(first_cycle,
		    anchor_vertex);
		const CycleAnalysis second_analysis = analyze_cycle(second_cycle,
		    anchor_vertex);
		if (!first_analysis.valid || !second_analysis.valid)
		    continue;
		bool shared_internal_vertex = false;
		for (std::set<int>::const_iterator vertex =
			first_analysis.internal_vertices.begin();
			vertex != first_analysis.internal_vertices.end(); ++vertex)
		    if (second_analysis.internal_vertices.find(*vertex) !=
			    second_analysis.internal_vertices.end()) {
			shared_internal_vertex = true;
			break;
		    }
		if (shared_internal_vertex)
		    continue;

		const CycleAnalysis *lobe = NULL;
		if (std::abs(first_analysis.winding) == 1 &&
			second_analysis.winding == 0)
		    lobe = &first_analysis;
		else if (std::abs(second_analysis.winding) == 1 &&
			first_analysis.winding == 0)
		    lobe = &second_analysis;
		if (!lobe)
		    continue;

		/* This is not merely an alternative curve which might improve an
		 * invalid OpenNURBS construction.  The winding and repeated-vertex
		 * proofs establish that the authored loop itself concatenates two
		 * incompatible topological cycles.  A later seam relocation can make
		 * that figure-eight pass structural validation without making it a
		 * legitimate face boundary, so permissive mode must run the isolated
		 * inference transaction even if ordinary pcurve repair happens to
		 * produce a structurally acceptable container. */
		wrapper->RequireCurveInferenceCandidate();
		if (!wrapper->CurveInferenceTrialEnabled()) {
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id
			    << ": STEP loop #" << loop.m_loop_user.i
			    << " concatenates a contractible boundary and a disjoint "
			       "single-winding lobe at topology vertex V"
			    << anchor_vertex << std::endl;
		    return false;
		}

		std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
		if (candidate->m_L[li].TrimCount() -
			static_cast<int>(lobe->trim_indices.size()) < 3)
		    continue;
		std::vector<int> removed(lobe->trim_indices);
		std::sort(removed.begin(), removed.end(), std::greater<int>());
		bool deleted = true;
		for (std::vector<int>::const_iterator trim_index = removed.begin();
			trim_index != removed.end(); ++trim_index) {
		    if (*trim_index < 0 || *trim_index >= candidate->m_T.Count()) {
			deleted = false;
			break;
		    }
		    candidate->DeleteTrim(candidate->m_T[*trim_index], true);
		}
		if (!deleted)
		    continue;
		if (!candidate->Compact())
		    continue;
		const int source_loop_id = loop.m_loop_user.i;
		std::vector<int> unique_edge_ids(lobe->edge_ids);
		std::sort(unique_edge_ids.begin(), unique_edge_ids.end());
		unique_edge_ids.erase(std::unique(unique_edge_ids.begin(),
		    unique_edge_ids.end()), unique_edge_ids.end());
		*brep = *candidate;

		std::ostringstream edge_list;
		for (size_t edge_index = 0;
			edge_index < unique_edge_ids.size(); ++edge_index) {
		    if (edge_index)
			edge_list << ',';
		    edge_list << unique_edge_ids[edge_index];
		}
		for (size_t edge_index = 0;
			edge_index < unique_edge_ids.size(); ++edge_index) {
		    std::ostringstream detail;
		    detail << "removed STEP loop #" << source_loop_id
			<< " lobe edges " << edge_list.str()
			<< " after proving an exact " << lobe->winding
			<< "-turn cycle on the singly closed support surface, "
			   "disjoint internal topology, and a contractible retained "
			   "cycle at shared vertex V" << anchor_vertex;
		    wrapper->RecordInferredCurve(unique_edge_ids[edge_index],
			"noncontractible_repeated_vertex_lobe_removal",
			fabs(lobe->winding * period), 0.0,
			fabs(lobe->winding * period), LocalUnits::tolerance,
			detail.str());
		}
		wrapper->RecordRepair(source_loop_id, "EDGE_LOOP", "edge_list",
		    "removed a detached single-winding lobe from a repeated-vertex "
		    "source loop during permissive topology inference");
		wrapper->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Warning, source_loop_id,
		    "EDGE_LOOP", "edge_list",
		    "source loop concatenated contractible and noncontractible "
		    "cycles at one topology vertex; permissive import retained the "
		    "contractible face boundary");
		return true;
	    }
	}
    }
    return false;
}

/* Return true when one face loop is exactly a forward/reverse traversal of a
 * single STEP edge and deleting those two uses would leave the ordinary two
 * reciprocal shell uses on two other faces.  This is stronger than a
 * parameter-space zero-area test: all four directed edge uses and both
 * supplied pcurve lifts are proved before a source face may be discarded. */
bool
exact_redundant_face_edge_loop(const ON_Brep *brep,
	const ON_BrepFace &redundant_face, const ON_BrepLoop &loop,
	std::string *failure, double *edge_mismatch)
{
    if (failure)
	failure->clear();
    if (edge_mismatch)
	*edge_mismatch = 0.0;
    if (!brep || loop.TrimCount() != 2)
	return false;
    const ON_BrepTrim *first = loop.Trim(0);
    const ON_BrepTrim *second = loop.Trim(1);
    const ON_Surface *surface = redundant_face.SurfaceOf();
    if (!first || !second || !surface || first->m_ei < 0 ||
	    first->m_ei != second->m_ei ||
	    first->m_ei >= brep->m_E.Count() ||
	    first->m_bRev3d == second->m_bRev3d ||
	    first->m_vi[0] != second->m_vi[1] ||
	    first->m_vi[1] != second->m_vi[0]) {
	if (failure) *failure =
	    "the two loop trims were not reciprocal uses of one edge";
	return false;
    }
    const ON_BrepEdge &edge = brep->m_E[first->m_ei];
    if (edge.m_edge_user.i <= 0 || edge.m_ti.Count() != 4) {
	if (failure) *failure =
	    "the candidate edge did not have exactly four source uses";
	return false;
    }

    const ON_BrepTrim *remaining[2] = {NULL, NULL};
    int remaining_count = 0;
    for (int eti = 0; eti < edge.m_ti.Count(); ++eti) {
	const int ti = edge.m_ti[eti];
	if (ti < 0 || ti >= brep->m_T.Count())
	    return false;
	const ON_BrepTrim *use = &brep->m_T[ti];
	if (use == first || use == second)
	    continue;
	if (remaining_count >= 2 || !use->Face() ||
		use->Face() == &redundant_face) {
	    if (failure) *failure =
		"the two remaining edge uses were not on neighboring faces";
	    return false;
	}
	remaining[remaining_count++] = use;
    }
    if (remaining_count != 2 || !remaining[0] || !remaining[1] ||
	    remaining[0]->Face() == remaining[1]->Face()) {
	if (failure) *failure =
	    "the two remaining edge uses did not belong to distinct faces";
	return false;
    }
    /* Removing the cancelling face must restore, not merely reduce, shell
     * ambiguity on this edge. */
    const bool first_effective =
	remaining[0]->m_bRev3d ^ remaining[0]->Face()->m_bRev;
    const bool second_effective =
	remaining[1]->m_bRev3d ^ remaining[1]->Face()->m_bRev;
    if (first_effective == second_effective) {
	if (failure) *failure =
	    "the two remaining edge uses did not have reciprocal shell senses";
	return false;
    }

    const ON_Interval first_domain = first->Domain();
    const ON_Interval second_domain = second->Domain();
    const ON_Interval edge_domain = edge.Domain();
    if (!first_domain.IsIncreasing() || !second_domain.IsIncreasing() ||
	    !edge_domain.IsIncreasing()) {
	if (failure) *failure =
	    "a pcurve or edge parameter domain was invalid";
	return false;
    }
    double tolerance = std::max(LocalUnits::tolerance, edge.m_tolerance);
    tolerance = std::max(tolerance, std::max(first->m_tolerance[0],
	first->m_tolerance[1]));
    tolerance = std::max(tolerance, std::max(second->m_tolerance[0],
	second->m_tolerance[1]));
    const double parameter_tolerance = std::max(
	ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	1.0e-10 * std::max(surface->Domain(0).Length(),
	    surface->Domain(1).Length()));
    double maximum_edge_mismatch = 0.0;
    for (int sample = 0; sample <= kPcurveLocusScreeningSegments; ++sample) {
	const double fraction = static_cast<double>(sample) /
	    kPcurveLocusScreeningSegments;
	const ON_3dPoint first_uv = first->PointAt(
	    first_domain.ParameterAt(fraction));
	const ON_3dPoint second_uv = second->PointAt(
	    second_domain.ParameterAt(1.0 - fraction));
	const ON_3dPoint first_lift = first_uv.IsValid() ?
	    surface->PointAt(first_uv.x, first_uv.y) :
	    ON_3dPoint::UnsetPoint;
	const ON_3dPoint second_lift = second_uv.IsValid() ?
	    surface->PointAt(second_uv.x, second_uv.y) :
	    ON_3dPoint::UnsetPoint;
	const ON_3dPoint first_edge_point = edge.PointAt(
	    edge_domain.ParameterAt(first->m_bRev3d ?
		1.0 - fraction : fraction));
	const ON_3dPoint second_edge_point = edge.PointAt(
	    edge_domain.ParameterAt(second->m_bRev3d ?
		fraction : 1.0 - fraction));
	if (!first_lift.IsValid() || !second_lift.IsValid() ||
		!first_edge_point.IsValid() || !second_edge_point.IsValid() ||
		first_uv.DistanceTo(second_uv) > parameter_tolerance ||
		first_lift.DistanceTo(second_lift) > tolerance) {
	    if (failure) {
		std::ostringstream detail;
		detail << "the reciprocal pcurves were not the same literal UV "
		    "path at sample " << sample << '/'
		    << kPcurveLocusScreeningSegments << " (parameter/lift "
		    "distances " << first_uv.DistanceTo(second_uv) << '/'
		    << first_lift.DistanceTo(second_lift)
		    << ", tolerances " << parameter_tolerance << '/'
		    << tolerance << ')';
		*failure = detail.str();
	    }
	    return false;
	}
	maximum_edge_mismatch = std::max(maximum_edge_mismatch,
	    std::max(first_lift.DistanceTo(first_edge_point),
		second_lift.DistanceTo(second_edge_point)));
    }
    if (edge_mismatch)
	*edge_mismatch = maximum_edge_mismatch;
    return true;
}


/* Delete one source face which has no geometric area and whose every boundary
 * is an exact cancelling use of an edge otherwise shared by the proper two
 * neighboring faces.  The caller repeats after Compact() because indices
 * change. */
bool
remove_one_exact_redundant_zero_area_face(ON_Brep *brep,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type)
{
    if (!brep)
	return false;
    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	ON_BrepFace &face = brep->m_F[fi];
	if (face.LoopCount() < 1)
	    continue;
	bool redundant = true;
	std::string failure;
	double maximum_edge_mismatch = 0.0;
	for (int fli = 0; redundant && fli < face.LoopCount(); ++fli) {
	    const ON_BrepLoop *loop = face.Loop(fli);
	    double loop_edge_mismatch = 0.0;
	    redundant = loop &&
		exact_redundant_face_edge_loop(brep, face, *loop, &failure,
		    &loop_edge_mismatch);
	    maximum_edge_mismatch = std::max(maximum_edge_mismatch,
		loop_edge_mismatch);
	}
	if (!redundant) {
	    if (wrapper && wrapper->Verbose() && !failure.empty() &&
		    face.LoopCount() > 1) {
		std::cerr << entity_type << " #" << entity_id
		    << ": zero-area face candidate F" << fi
		    << " rejected: " << failure << std::endl;
	    }
	    continue;
	}
	/* Keep all source edges: each has exactly two proven neighboring uses
	 * after this face and its cancelling trims are deleted. */
	if (wrapper && maximum_edge_mismatch > LocalUnits::tolerance)
	    wrapper->RecordDiagnostic(
		brlcad::step::DiagnosticSeverity::Warning, entity_id,
		entity_type, "advanced_face",
		"an exact zero-area face used reciprocal pcurves which both "
		"missed their shared 3-D STEP edge; removed only the cancelling "
		"face uses and retained the immutable edge");
	brep->DeleteFace(face, false);
	return brep->Compact();
    }
    return false;
}


/* Remove one source face whose complete outer boundary is two distinct,
 * reciprocal STEP edges on an open surface.  This complements the same-edge
 * redundant-face proof above: a writer may encode a zero-width planar sliver
 * with two separately named line edges, and neighboring loop normalization
 * can already have removed their reciprocal excursions.  Require both 3-D
 * loci and both lifted pcurves to coincide densely at the declared tolerance;
 * periodic surfaces are excluded because opposite parameter images of one
 * 3-D seam can bound a genuine full-period face. */
bool
remove_one_exact_distinct_edge_zero_area_face(ON_Brep *brep,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type,
	int *removed_face_step_id, int *first_edge_step_id,
	int *second_edge_step_id)
{
    if (removed_face_step_id) *removed_face_step_id = 0;
    if (first_edge_step_id) *first_edge_step_id = 0;
    if (second_edge_step_id) *second_edge_step_id = 0;
    if (!brep || !(LocalUnits::tolerance > 0.0))
	return false;

    size_t verbose_rejections = 0;
    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	const ON_BrepFace &face = brep->m_F[fi];
	if (face.LoopCount() != 1)
	    continue;
	const ON_BrepLoop *loop = face.Loop(0);
	const ON_Surface *surface = face.SurfaceOf();
	if (!loop || loop->TrimCount() != 2)
	    continue;
	if (!surface || loop->m_type != ON_BrepLoop::outer ||
		surface->IsClosed(0) || surface->IsClosed(1)) {
	    if (wrapper && wrapper->Verbose() && verbose_rejections++ < 16)
		std::cerr << entity_type << " #" << entity_id
		    << ": reciprocal two-edge face F" << fi << "/STEP loop "
		    << loop->m_loop_user.i << " retained: surface="
		    << (surface ? "present" : "missing") << " loop-type="
		    << static_cast<int>(loop->m_type) << " closed="
		    << (surface && surface->IsClosed(0)) << '/'
		    << (surface && surface->IsClosed(1)) << std::endl;
	    continue;
	}
	const ON_BrepTrim *first_trim = loop->Trim(0);
	const ON_BrepTrim *second_trim = loop->Trim(1);
	const ON_BrepEdge *first = first_trim ? first_trim->Edge() : NULL;
	const ON_BrepEdge *second = second_trim ? second_trim->Edge() : NULL;
	if (!first_trim || !second_trim || !first || !second ||
		first == second || first->m_edge_user.i <= 0 ||
		second->m_edge_user.i <= 0 ||
		first->m_edge_user.i == second->m_edge_user.i ||
		first->m_ti.Count() != 1 || second->m_ti.Count() != 1 ||
		first_trim->m_vi[0] != second_trim->m_vi[1] ||
		first_trim->m_vi[1] != second_trim->m_vi[0]) {
	    if (wrapper && wrapper->Verbose() && verbose_rejections++ < 16)
		std::cerr << entity_type << " #" << entity_id
		    << ": reciprocal two-edge face F" << fi << "/STEP loop "
		    << loop->m_loop_user.i << " retained: STEP edges="
		    << (first ? first->m_edge_user.i : 0) << '/'
		    << (second ? second->m_edge_user.i : 0) << " uses="
		    << (first ? first->m_ti.Count() : 0) << '/'
		    << (second ? second->m_ti.Count() : 0) << " vertices="
		    << (first_trim ? first_trim->m_vi[0] : -1) << ':'
		    << (first_trim ? first_trim->m_vi[1] : -1) << '/'
		    << (second_trim ? second_trim->m_vi[0] : -1) << ':'
		    << (second_trim ? second_trim->m_vi[1] : -1) << std::endl;
	    continue;
	}
	const ON_Interval first_edge_domain = first->Domain();
	const ON_Interval second_edge_domain = second->Domain();
	const ON_Interval first_trim_domain = first_trim->Domain();
	const ON_Interval second_trim_domain = second_trim->Domain();
	if (!first_edge_domain.IsIncreasing() ||
		!second_edge_domain.IsIncreasing() ||
		!first_trim_domain.IsIncreasing() ||
		!second_trim_domain.IsIncreasing())
	    continue;

	ON_3dPoint first_points[kPcurveLocusScreeningSegments + 1];
	ON_3dPoint second_points[kPcurveLocusScreeningSegments + 1];
	bool exact_cancellation = true;
	for (int sample = 0; exact_cancellation &&
		sample <= kPcurveLocusScreeningSegments; ++sample) {
	    const double fraction = static_cast<double>(sample) /
		kPcurveLocusScreeningSegments;
	    first_points[sample] = first->PointAt(
		first_edge_domain.ParameterAt(fraction));
	    second_points[sample] = second->PointAt(
		second_edge_domain.ParameterAt(fraction));
	    exact_cancellation = first_points[sample].IsValid() &&
		second_points[sample].IsValid();
	}
	size_t first_rejected = 0;
	size_t second_rejected = 0;
	double first_distance = DBL_MAX;
	double second_distance = DBL_MAX;
	const bool first_on_second = exact_cancellation &&
	    step_curve_locus_contains_points(second, first_points,
		kPcurveLocusScreeningSegments + 1, LocalUnits::tolerance,
		&first_rejected, &first_distance);
	const bool second_on_first = exact_cancellation &&
	    step_curve_locus_contains_points(first, second_points,
		kPcurveLocusScreeningSegments + 1, LocalUnits::tolerance,
		&second_rejected, &second_distance);
	if (!exact_cancellation || !first_on_second || !second_on_first) {
	    if (wrapper && wrapper->Verbose() && verbose_rejections++ < 16)
		std::cerr << entity_type << " #" << entity_id
		    << ": reciprocal two-edge face F" << fi << "/STEP loop "
		    << loop->m_loop_user.i << " retained: distinct edge loci "
		       "did not coincide; rejected samples=" << first_rejected
		    << '/' << second_rejected << " distances=" << first_distance
		    << '/' << second_distance << " tolerance="
		    << LocalUnits::tolerance << std::endl;
	    continue;
	}

	for (int sample = 0; exact_cancellation &&
		sample <= kDenseValidationSegments; ++sample) {
	    const double fraction = static_cast<double>(sample) /
		kDenseValidationSegments;
	    const ON_3dPoint first_uv = first_trim->PointAt(
		first_trim_domain.ParameterAt(fraction));
	    const ON_3dPoint second_uv = second_trim->PointAt(
		second_trim_domain.ParameterAt(1.0 - fraction));
	    const ON_3dPoint first_lift = first_uv.IsValid() ?
		surface->PointAt(first_uv.x, first_uv.y) :
		ON_3dPoint::UnsetPoint;
	    const ON_3dPoint second_lift = second_uv.IsValid() ?
		surface->PointAt(second_uv.x, second_uv.y) :
		ON_3dPoint::UnsetPoint;
	    const ON_3dPoint first_edge_point = first->PointAt(
		first_edge_domain.ParameterAt(first_trim->m_bRev3d ?
		    1.0 - fraction : fraction));
	    const ON_3dPoint second_edge_point = second->PointAt(
		second_edge_domain.ParameterAt(second_trim->m_bRev3d ?
		    fraction : 1.0 - fraction));
	    exact_cancellation = first_lift.IsValid() &&
		second_lift.IsValid() && first_edge_point.IsValid() &&
		second_edge_point.IsValid() &&
		first_lift.DistanceTo(second_lift) <= LocalUnits::tolerance &&
		first_lift.DistanceTo(first_edge_point) <=
		    LocalUnits::tolerance &&
		second_lift.DistanceTo(second_edge_point) <=
		    LocalUnits::tolerance;
	}
	if (!exact_cancellation) {
	    if (wrapper && wrapper->Verbose() && verbose_rejections++ < 16)
		std::cerr << entity_type << " #" << entity_id
		    << ": reciprocal two-edge face F" << fi << "/STEP loop "
		    << loop->m_loop_user.i << " retained: lifted pcurves did not "
		       "coincide with each other and their STEP edge loci at "
		    << LocalUnits::tolerance << " mm" << std::endl;
	    continue;
	}

	const int source_loop_id = loop->m_loop_user.i;
	const int first_source_edge = first->m_edge_user.i;
	const int second_source_edge = second->m_edge_user.i;
	std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
	candidate->DeleteFace(candidate->m_F[fi], true);
	if (!candidate->Compact())
	    continue;
	ON_wString validation_messages;
	ON_TextLog validation_log(validation_messages);
	if (!candidate->IsValid(&validation_log)) {
	    if (wrapper && wrapper->Verbose()) {
		ON_String validation_text(validation_messages);
		std::cerr << entity_type << " #" << entity_id
		    << ": distinct-edge zero-area face F" << fi
		    << "/STEP loop " << source_loop_id
		    << " deletion rejected by BREP validation:\n"
		    << validation_text.Array();
	    }
	    continue;
	}
	*brep = *candidate;
	if (removed_face_step_id)
	    *removed_face_step_id = source_loop_id;
	if (first_edge_step_id) *first_edge_step_id = first_source_edge;
	if (second_edge_step_id) *second_edge_step_id = second_source_edge;
	return true;
    }
    return false;
}


bool
remove_adjacent_zero_area_slit(ON_Brep *brep, int loop_index,
    std::string *failure_reason, int *removed_loop_step_id,
    int *removed_edge_step_id, double *source_edge_mismatch,
    bool *removed_source_slit)
{
    if (failure_reason)
	failure_reason->clear();
    if (removed_loop_step_id)
	*removed_loop_step_id = 0;
    if (removed_edge_step_id)
	*removed_edge_step_id = 0;
    if (source_edge_mismatch)
	*source_edge_mismatch = 0.0;
    if (removed_source_slit)
	*removed_source_slit = false;
    if (!brep || loop_index < 0 || loop_index >= brep->m_L.Count())
	return false;
    ON_BrepLoop &loop = brep->m_L[loop_index];
    const int trim_count = loop.TrimCount();
    if (trim_count == 2) {
	ON_BrepTrim *first = loop.Trim(0);
	ON_BrepTrim *second = loop.Trim(1);
	const ON_BrepFace *face = loop.Face();
	/* A STEP FACE_BOUND may redundantly use one EDGE_CURVE forward and
	 * backward.  That loop encloses no parameter-space area, and an edge
	 * referenced only by those two uses contributes no shell adjacency.
	 * OpenNURBS cannot represent it as a manifold slit: slit loops require
	 * seam trims, while a two-use manifold seam is required to be in an outer
	 * loop.  Remove only the exactly cancelling form.  Periodic seam images
	 * may legitimately place the two pcurves on opposite parameter-domain
	 * sides, so cancellation must be proved against their shared directed 3-D
	 * STEP edge rather than by comparing their literal UV coordinates.  A
	 * full surface band still survives because it necessarily contains
	 * independent boundary edges in the same loop. */
	if (!first || !second || !face || face->LoopCount() < 2 ||
		loop.m_type == ON_BrepLoop::outer ||
		first->m_type != ON_BrepTrim::seam ||
		second->m_type != ON_BrepTrim::seam || first->m_ei < 0 ||
		first->m_ei != second->m_ei ||
		first->m_bRev3d == second->m_bRev3d)
	    return false;
	if (first->m_ei >= brep->m_E.Count() ||
		brep->m_E[first->m_ei].m_edge_user.i <= 0 ||
		brep->m_E[first->m_ei].m_ti.Count() != 2 ||
		first->m_vi[0] != second->m_vi[1] ||
		first->m_vi[1] != second->m_vi[0]) {
	    if (failure_reason) {
		std::ostringstream reason;
		reason << "standalone reciprocal slit L" << loop_index
		    << "/STEP" << loop.m_loop_user.i
		    << " rejected by topology proof";
		if (first->m_ei >= 0 && first->m_ei < brep->m_E.Count())
		    reason << " edge=" << first->m_ei << "/STEP"
			<< brep->m_E[first->m_ei].m_edge_user.i
			<< " uses=" << brep->m_E[first->m_ei].m_ti.Count();
		reason << " vertices=" << first->m_vi[0] << ':'
		    << first->m_vi[1] << '/' << second->m_vi[0] << ':'
		    << second->m_vi[1];
		*failure_reason = reason.str();
	    }
	    return false;
	}
	const ON_Interval first_domain = first->Domain();
	const ON_Interval second_domain = second->Domain();
	const ON_Surface *surface = face->SurfaceOf();
	if (!first_domain.IsIncreasing() || !second_domain.IsIncreasing() ||
		!surface) {
	    if (failure_reason) {
		std::ostringstream reason;
		reason << "standalone reciprocal slit L" << loop_index
		    << "/STEP" << loop.m_loop_user.i
		    << " rejected by invalid trim domain or surface";
		*failure_reason = reason.str();
	    }
	    return false;
	}
	const ON_BrepEdge &edge = brep->m_E[first->m_ei];
	const ON_Interval edge_domain = edge.Domain();
	if (!edge_domain.IsIncreasing()) {
	    if (failure_reason) {
		std::ostringstream reason;
		reason << "standalone reciprocal slit L" << loop_index
		    << "/STEP" << loop.m_loop_user.i << " edge "
		    << first->m_ei << "/STEP" << edge.m_edge_user.i
		    << " rejected by invalid edge domain";
		*failure_reason = reason.str();
	    }
	    return false;
	}
	/* A numerically equivalent forward/reverse p-space pair is already a
	 * complete proof that this inner bound encloses zero area.  Pullbacks for
	 * the two source uses are fitted independently, so use a scale-relative
	 * parameter floor and then verify their lifted loci.  This is deliberately
	 * far below a periodic span and cannot confuse opposite seam images.  The
	 * private edge has no other shell use and is deleted with the loop, so an
	 * exporter-supplied edge/pcurve mismatch cannot make the cancelling bound
	 * geometrically meaningful.  Retain the stronger directed edge/lift proof
	 * below for periodic images whose UV coordinates do not coincide. */
	double tolerance = std::max(LocalUnits::tolerance, edge.m_tolerance);
	tolerance = std::max(tolerance, std::max(first->m_tolerance[0],
	    first->m_tolerance[1]));
	tolerance = std::max(tolerance, std::max(second->m_tolerance[0],
	    second->m_tolerance[1]));
	const double first_parameter_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	    kPeriodicParameterSnapFraction * surface->Domain(0).Length());
	const double second_parameter_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	    kPeriodicParameterSnapFraction * surface->Domain(1).Length());
	const bool parameter_cancellation = opposite_trim_curves_coincide(
	    first, second, first_parameter_tolerance,
	    second_parameter_tolerance);
	/* An explicit STEP inner FACE_BOUND containing the same EDGE_CURVE in
	 * opposite directions is a zero-width manifold slit.  It contributes no
	 * surface area or solid boundary, but OpenNURBS cannot validate this
	 * topology: a two-use seam edge is required to be in an outer loop.
	 * Preserve strict mode by invoking this helper only from safe repair.
	 * When independently generated pullbacks choose different parameter
	 * branches, retain the source topology itself as the exact zero-width
	 * witness rather than inventing one preferred pcurve. */
	const bool source_authored_slit = loop.m_loop_user.i > 0 &&
	    edge.m_edge_user.i > 0;
	if (!parameter_cancellation && source_authored_slit) {
	    if (removed_loop_step_id)
		*removed_loop_step_id = loop.m_loop_user.i;
	    if (removed_edge_step_id)
		*removed_edge_step_id = edge.m_edge_user.i;
	    if (removed_source_slit)
		*removed_source_slit = true;
	    brep->DeleteLoop(loop, true);
	    return brep->Compact();
	}
	double maximum_edge_mismatch = 0.0;
	const int proof_segments = parameter_cancellation ?
	    kDenseValidationSegments : kPcurveLocusScreeningSegments;
	for (int sample = 0; sample <= proof_segments; ++sample) {
	    const double fraction = static_cast<double>(sample) /
		proof_segments;
	    const ON_3dPoint first_uv = first->PointAt(
		first_domain.ParameterAt(fraction));
	    const ON_3dPoint second_uv = second->PointAt(
		second_domain.ParameterAt(1.0 - fraction));
	    const ON_3dPoint first_lift =
		closed_surface_point_at(surface, first_uv);
	    const ON_3dPoint second_lift =
		closed_surface_point_at(surface, second_uv);
	    const ON_3dPoint first_edge_point = edge.PointAt(
		edge_domain.ParameterAt(first->m_bRev3d ?
		    1.0 - fraction : fraction));
	    const ON_3dPoint second_edge_point = edge.PointAt(
		edge_domain.ParameterAt(second->m_bRev3d ?
		    fraction : 1.0 - fraction));
	    if (!first_uv.IsValid() || !second_uv.IsValid() ||
		    !first_lift.IsValid() || !second_lift.IsValid() ||
		    !first_edge_point.IsValid() || !second_edge_point.IsValid()) {
		if (failure_reason) {
		    std::ostringstream reason;
		    reason << "standalone reciprocal slit L" << loop_index
			<< "/STEP" << loop.m_loop_user.i << " edge "
			<< first->m_ei << "/STEP" << edge.m_edge_user.i
			<< " produced invalid proof geometry at sample "
			<< sample << '/' << proof_segments;
		    *failure_reason = reason.str();
		}
		return false;
	    }
	    const double first_edge_mismatch =
		first_lift.DistanceTo(first_edge_point);
	    const double second_edge_mismatch =
		second_lift.DistanceTo(second_edge_point);
	    const double lift_gap = first_lift.DistanceTo(second_lift);
	    maximum_edge_mismatch = std::max(maximum_edge_mismatch,
		std::max(first_edge_mismatch, second_edge_mismatch));
	    if (lift_gap > tolerance ||
		    (!parameter_cancellation &&
		     (first_edge_mismatch > tolerance ||
		      second_edge_mismatch > tolerance))) {
		if (failure_reason) {
		    std::ostringstream reason;
		    reason << "standalone reciprocal slit L" << loop_index
			<< "/STEP" << loop.m_loop_user.i << " edge "
			<< first->m_ei << "/STEP" << edge.m_edge_user.i
			<< " rejected at sample " << sample << '/'
			<< proof_segments << " tolerance="
			<< tolerance << " parameter-gap="
			<< first_uv.DistanceTo(second_uv) << " lift/edge="
			<< first_edge_mismatch << '/' << second_edge_mismatch
			<< " lift-gap=" << lift_gap
			<< " parameter-cancellation="
			<< (parameter_cancellation ? "yes" : "no");
		    *failure_reason = reason.str();
		}
		return false;
	    }
	}
	if (removed_loop_step_id)
	    *removed_loop_step_id = loop.m_loop_user.i;
	if (removed_edge_step_id)
	    *removed_edge_step_id = edge.m_edge_user.i;
	if (source_edge_mismatch)
	    *source_edge_mismatch = maximum_edge_mismatch;
	brep->DeleteLoop(loop, true);
	return brep->Compact();
    }
    if (trim_count < 3)
	return false;

    for (int first_offset = 0; first_offset < trim_count; ++first_offset) {
	const int second_offset = (first_offset + 1) % trim_count;
	const int previous_offset = (first_offset + trim_count - 1) % trim_count;
	const int next_offset = (second_offset + 1) % trim_count;
	ON_BrepTrim *first = loop.Trim(first_offset);
	ON_BrepTrim *second = loop.Trim(second_offset);
	const ON_BrepTrim *previous = loop.Trim(previous_offset);
	const ON_BrepTrim *next = loop.Trim(next_offset);
	if (!first || !second || !previous || !next ||
		first->m_type != ON_BrepTrim::seam ||
		second->m_type != ON_BrepTrim::seam || first->m_ei < 0 ||
		first->m_ei != second->m_ei ||
		first->m_bRev3d == second->m_bRev3d ||
	first->m_ei >= brep->m_E.Count() ||
	brep->m_E[first->m_ei].m_ti.Count() != 2)
	    continue;
	/* The two exact uses cancel topologically.  Prefer an already exact
	 * exposed p-space join; otherwise admit only a shared topology vertex whose
	 * two surface lifts already agree within the asserted model uncertainty.
	 * The ordinary bounded endpoint pass must still close that join later. */
	if (previous->m_vi[1] != next->m_vi[0] || previous->m_vi[1] < 0 ||
		previous->m_vi[1] >= brep->m_V.Count())
	    continue;
	const double exposed_gap = previous->PointAtEnd().DistanceTo(
	    next->PointAtStart());
	if (exposed_gap > ON_ZERO_TOLERANCE) {
	    const ON_BrepFace *face = loop.Face();
	    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	    if (!surface || !(LocalUnits::tolerance > 0.0))
		continue;
	    const ON_3dPoint previous_uv = previous->PointAtEnd();
	    const ON_3dPoint next_uv = next->PointAtStart();
	    const ON_3dPoint previous_lift = surface->PointAt(
		previous_uv.x, previous_uv.y);
	    const ON_3dPoint next_lift = surface->PointAt(next_uv.x, next_uv.y);
	    const ON_3dPoint vertex = brep->m_V[previous->m_vi[1]].point;
	    if (!previous_lift.IsValid() || !next_lift.IsValid() ||
		    previous_lift.DistanceTo(vertex) > LocalUnits::tolerance ||
		    next_lift.DistanceTo(vertex) > LocalUnits::tolerance ||
		    previous_lift.DistanceTo(next_lift) > LocalUnits::tolerance)
		continue;
	}
	const int first_index = first->m_trim_index;
	const int second_index = second->m_trim_index;
	/* Both trims use the same edge.  Leave the edge in place while removing
	 * the first trim, then let the final trim removal delete the now-unused
	 * edge.  DeleteTrim only marks topology; Compact performs the remap after
	 * both original trim indices have been consumed. */
	brep->DeleteTrim(brep->m_T[first_index], false);
	brep->DeleteTrim(brep->m_T[second_index], true);
	return brep->Compact();
    }
    return false;
}


struct SingularLoopClosure {
    bool valid = false;
    bool vertices_available = false;
    bool direct_collapse_tested = false;
    bool direct_collapses = false;
    ON_Surface::ISO iso = ON_Surface::not_iso;
    int vertex = -1;
    int other_vertex = -1;
    double topology_vertex_distance = DBL_MAX;
    double maximum_lift_distance = DBL_MAX;
    ON_3dPoint start = ON_3dPoint::UnsetPoint;
    ON_3dPoint end = ON_3dPoint::UnsetPoint;
};


SingularLoopClosure
collapsed_singular_loop_closure(ON_Brep *brep, const ON_BrepLoop *loop,
    const ON_BrepTrim *last, const ON_BrepTrim *first)
{
    SingularLoopClosure result;
    if (!brep || !loop || !last || !first || last->m_vi[1] < 0 ||
	    last->m_vi[1] >= brep->m_V.Count() || first->m_vi[0] < 0 ||
	    first->m_vi[0] >= brep->m_V.Count())
	return result;

    result.vertices_available = true;
    result.vertex = last->m_vi[1];
    result.other_vertex = first->m_vi[0];
    const ON_3dPoint &topology_vertex = brep->m_V[result.vertex].point;
    result.topology_vertex_distance = topology_vertex.DistanceTo(
	brep->m_V[result.other_vertex].point);
    if (result.topology_vertex_distance > LocalUnits::tolerance)
	return result;

    const ON_BrepFace *face = loop->Face();
    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
    if (!surface) return result;

    result.start = last->PointAtEnd();
    result.end = first->PointAtStart();
    std::unique_ptr<ON_LineCurve> singular_curve(new ON_LineCurve(
	result.start, result.end));
    if (!singular_curve->ChangeDimension(2) || !singular_curve->IsValid())
	return result;

    result.iso = surface->IsIsoparametric(*singular_curve);
    bool boundary_iso = result.iso == ON_Surface::W_iso ||
	result.iso == ON_Surface::E_iso || result.iso == ON_Surface::S_iso ||
	result.iso == ON_Surface::N_iso;
    if (!boundary_iso) {
	const int varying_direction = fabs(result.end.y - result.start.y) >
	    fabs(result.end.x - result.start.x) ? 1 : 0;
	const int fixed_direction = 1 - varying_direction;
	const ON_Interval fixed_domain = surface->Domain(fixed_direction);
	const double fixed_parameter = 0.5 *
	    (result.start[fixed_direction] + result.end[fixed_direction]);
	const int side = fabs(fixed_parameter - fixed_domain.Min()) <=
	    fabs(fixed_parameter - fixed_domain.Max()) ? 0 : 1;
	const ON_Surface::ISO expected_iso = fixed_direction == 0 ?
	    (side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
	    (side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);

	result.direct_collapse_tested = true;
	result.direct_collapses = true;
	for (int sample = 0; result.direct_collapses &&
		sample <= kDenseValidationSegments; ++sample) {
	    const ON_3dPoint uv = singular_curve->PointAt(
		singular_curve->Domain().ParameterAt(
		    static_cast<double>(sample) / kDenseValidationSegments));
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    result.direct_collapses = lift.IsValid() &&
		lift.DistanceTo(topology_vertex) <= LocalUnits::tolerance;
	}
	if (result.direct_collapses) {
	    result.iso = expected_iso;
	    boundary_iso = true;
	} else {
	    result.start[fixed_direction] = fixed_domain[side];
	    result.end[fixed_direction] = fixed_domain[side];
	    std::unique_ptr<ON_LineCurve> boundary_curve(new ON_LineCurve(
		result.start, result.end));
	    if (boundary_curve->ChangeDimension(2) && boundary_curve->IsValid() &&
		    surface->IsIsoparametric(*boundary_curve) == expected_iso) {
		singular_curve = std::move(boundary_curve);
		result.iso = expected_iso;
		boundary_iso = true;
	    }
	}
    }
    if (!boundary_iso) return result;

    result.maximum_lift_distance = 0.0;
    result.valid = true;
    for (int sample = 0; result.valid && sample <= kDenseValidationSegments;
	    ++sample) {
	const ON_3dPoint uv = singular_curve->PointAt(
	    singular_curve->Domain().ParameterAt(
		static_cast<double>(sample) / kDenseValidationSegments));
	const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	const double distance = lift.IsValid() ?
	    lift.DistanceTo(topology_vertex) : DBL_MAX;
	result.maximum_lift_distance = std::max(
	    result.maximum_lift_distance, distance);
	result.valid = distance <= LocalUnits::tolerance;
    }
    return result;
}


bool
periodic_loop_closure(const ON_Brep *brep, const ON_BrepLoop *loop,
    const ON_BrepTrim *last, const ON_BrepTrim *first,
    double parameter_tolerance)
{
    if (!brep || !loop || !last || !first || last->m_vi[1] < 0 ||
	    last->m_vi[1] >= brep->m_V.Count() || first->m_vi[0] < 0 ||
	    first->m_vi[0] >= brep->m_V.Count())
	return false;
    const ON_BrepFace *face = loop->Face();
    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
    if (!surface) return false;

    const ON_3dPoint last_uv = last->PointAtEnd();
    const ON_3dPoint first_uv = first->PointAtStart();
    bool one_period_apart = false;
    for (int direction = 0; direction < 2; ++direction) {
	if (!surface->IsClosed(direction)) continue;
	const double period = surface->Domain(direction).Length();
	const double period_tolerance = parameter_tolerance *
	    std::max(1.0, fabs(period));
	if (fabs(fabs(last_uv[direction] - first_uv[direction]) - period) <=
		period_tolerance) {
	    /* Do not reject a proven periodic closure solely because the two
	     * independently supplied endpoints have a residual in the transverse
	     * parameter direction.  Parameter distance is not a model-space
	     * tolerance on a general surface.  The topology vertex and all three
	     * model-space lift checks below remain authoritative, using only the
	     * local tolerance established for these exact edges. */
	    one_period_apart = true;
	    break;
	}
    }
    if (!one_period_apart) return false;

    const ON_3dPoint &last_vertex = brep->m_V[last->m_vi[1]].point;
    const ON_3dPoint &first_vertex = brep->m_V[first->m_vi[0]].point;
	/* Edge materialization may have established that an otherwise exact source
	 * edge and its asserted STEP vertex differ by more than the file's declared
	 * uncertainty.  Safe mode records that densely measured, scale-bounded
	 * reality on the affected OpenNURBS edge and trim.  Use those local topology
	 * tolerances for this closure proof as OpenNURBS itself will; retaining the
	 * global uncertainty here would reject the proven periodic boundary before
	 * the explicit seam can be installed.  --exact never enlarges these values. */
    double topology_tolerance = LocalUnits::tolerance;
    if (last->Edge())
	topology_tolerance = std::max(topology_tolerance,
	    last->Edge()->m_tolerance);
    if (first->Edge())
	topology_tolerance = std::max(topology_tolerance,
	    first->Edge()->m_tolerance);
    topology_tolerance = std::max(topology_tolerance,
	std::max(last->m_tolerance[0], last->m_tolerance[1]));
    topology_tolerance = std::max(topology_tolerance,
	std::max(first->m_tolerance[0], first->m_tolerance[1]));
    topology_tolerance = std::max(topology_tolerance,
	brep->m_V[last->m_vi[1]].m_tolerance);
    topology_tolerance = std::max(topology_tolerance,
	brep->m_V[first->m_vi[0]].m_tolerance);
    if (last_vertex.DistanceTo(first_vertex) > topology_tolerance)
	return false;
    const ON_3dPoint last_lift = closed_surface_point_at(surface, last_uv);
    const ON_3dPoint first_lift = closed_surface_point_at(surface, first_uv);
    return last_lift.IsValid() && first_lift.IsValid() &&
	last_lift.DistanceTo(last_vertex) <= topology_tolerance &&
	first_lift.DistanceTo(first_vertex) <= topology_tolerance &&
	last_lift.DistanceTo(first_lift) <= topology_tolerance;
}


bool
opposite_trim_curves_coincide(const ON_BrepTrim *first,
    const ON_BrepTrim *second, double first_parameter_tolerance,
    double second_parameter_tolerance)
{
    if (!first || !second) return false;
    const ON_Interval first_domain = first->Domain();
    const ON_Interval second_domain = second->Domain();
    for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
	const double fraction = static_cast<double>(sample) /
	    kDenseValidationSegments;
	const ON_3dPoint first_uv = first->PointAt(
	    first_domain.ParameterAt(fraction));
	const ON_3dPoint second_uv = second->PointAt(
	    second_domain.ParameterAt(1.0 - fraction));
	if (!first_uv.IsValid() || !second_uv.IsValid())
	    return false;
	if (second_parameter_tolerance >= 0.0) {
	    if (fabs(first_uv.x - second_uv.x) >
		    first_parameter_tolerance ||
		    fabs(first_uv.y - second_uv.y) >
		    second_parameter_tolerance)
		return false;
	} else if (first_uv.DistanceTo(second_uv) >
		first_parameter_tolerance) {
	    return false;
	}
    }
    return true;
}


bool
split_keyhole_loop(ON_Brep *brep, int loop_index, std::string *failure_reason)
{
    if (failure_reason)
	failure_reason->clear();
    if (!brep || loop_index < 0 || loop_index >= brep->m_L.Count())
	return false;
    ON_BrepLoop *loop = &brep->m_L[loop_index];
    if ((loop->m_type != ON_BrepLoop::outer && loop->m_type != ON_BrepLoop::inner) ||
	loop->TrimCount() < 4)
	return false;

    /* Pcurve endpoints are computed independently and may differ by a few
     * ulps even when they describe the same parameter-space point.  This
     * floor is deliberately parameter-space numerical noise, not the model
     * uncertainty (which is measured in output-space millimetres). */
    const double parameter_tolerance =
	ON_ZERO_TOLERANCE * kNumericalToleranceScale;

    int first_offset = -1;
    int second_offset = -1;
    bool insert_inside_singular = false;
    bool insert_outside_singular = false;
    bool inside_periodic = false;
    bool outside_periodic = false;
    bool remove_periodic_duplicate_only = false;
    SingularLoopClosure inside_singular;
    SingularLoopClosure outside_singular;
    double best_failed_inside_gap = DBL_MAX;
    double best_failed_outside_gap = DBL_MAX;
    bool deferred_two_use_periodic_keyhole = false;
    std::ostringstream rejected_candidates;
    size_t rejected_candidate_count = 0;
    const int trim_count = loop->TrimCount();
    for (int first = 0; first < trim_count && first_offset < 0; ++first) {
	const ON_BrepTrim *first_trim = loop->Trim(first);
	if (!first_trim ||
	    (first_trim->m_type != ON_BrepTrim::seam &&
	     first_trim->m_type != ON_BrepTrim::slit) ||
	    first_trim->m_ei < 0 || first_trim->m_ei >= brep->m_E.Count())
	    continue;
	for (int second = first + 2; second < trim_count; ++second) {
	    if (first == 0 && second == trim_count - 1)
		continue;
	    const ON_BrepTrim *second_trim = loop->Trim(second);
	    if (!second_trim ||
		(second_trim->m_type != ON_BrepTrim::seam &&
		 second_trim->m_type != ON_BrepTrim::slit) ||
		second_trim->m_ei != first_trim->m_ei ||
		second_trim->m_bRev3d == first_trim->m_bRev3d)
		continue;
	    if (!opposite_trim_curves_coincide(first_trim, second_trim,
		    parameter_tolerance))
		continue;

	    /* Removing the oppositely traversed bridge must expose two loops that
	     * are already closed to parameter-space numerical precision.  This
	     * proves the bridge is a zero-area keyhole connector; no model-space
	     * gap is synthesized. */
	    const ON_BrepTrim *inside_first = loop->Trim(first + 1);
	    const ON_BrepTrim *inside_last = loop->Trim(second - 1);
	    const ON_BrepTrim *outside_first = loop->Trim((second + 1) % trim_count);
	    const ON_BrepTrim *outside_last = loop->Trim((first + trim_count - 1) % trim_count);
	    const double inside_gap = inside_first && inside_last ?
		inside_last->PointAtEnd().DistanceTo(inside_first->PointAtStart()) : DBL_MAX;
	    const double outside_gap = outside_first && outside_last ?
		outside_last->PointAtEnd().DistanceTo(outside_first->PointAtStart()) : DBL_MAX;
	    SingularLoopClosure candidate_inside_singular;
	    SingularLoopClosure candidate_outside_singular;
	    const bool candidate_inside_periodic = inside_gap > parameter_tolerance &&
		periodic_loop_closure(brep, loop, inside_last, inside_first,
		    parameter_tolerance);
	    const bool candidate_outside_periodic = outside_gap > parameter_tolerance &&
		periodic_loop_closure(brep, loop, outside_last, outside_first,
		    parameter_tolerance);
	    if (inside_gap > parameter_tolerance && !candidate_inside_periodic)
		candidate_inside_singular = collapsed_singular_loop_closure(
		    brep, loop, inside_last, inside_first);
	    if (outside_gap > parameter_tolerance && !candidate_outside_periodic)
		candidate_outside_singular = collapsed_singular_loop_closure(
		    brep, loop, outside_last, outside_first);
	    const bool inside_closed = inside_first && inside_last &&
		(inside_gap <= parameter_tolerance || candidate_inside_periodic ||
		 candidate_inside_singular.valid);
	    const bool outside_closed = outside_first && outside_last &&
		(outside_gap <= parameter_tolerance || candidate_outside_periodic ||
		 candidate_outside_singular.valid);
	    if (inside_closed && outside_closed) {
		const bool two_use_singleton_components =
		    first_trim->Edge() &&
		    first_trim->Edge()->m_ti.Count() == 2 &&
		    inside_first == inside_last &&
		    outside_first == outside_last &&
		    inside_first->m_vi[0] == inside_first->m_vi[1] &&
		    outside_first->m_vi[0] == outside_first->m_vi[1];
		/* A two-use reciprocal bridge between two full-period components is
		 * not an ordinary local keyhole split.  Splitting it here removes
		 * the only topology connecting those components and leaves two
		 * standalone full-period pcurves, each structurally open in
		 * Euclidean UV.  Preserve the source loop for the late
		 * retry_topological_keyhole_normalization transaction, which can
		 * separate both cycles, unwrap them together, and validate the
		 * complete face before committing. */
		if (((candidate_inside_periodic && candidate_outside_periodic) ||
			two_use_singleton_components) &&
			first_trim->Edge() &&
			first_trim->Edge()->m_ti.Count() == 2) {
		    deferred_two_use_periodic_keyhole = true;
		    continue;
		}
		first_offset = first;
		second_offset = second;
		insert_inside_singular = candidate_inside_singular.valid;
		insert_outside_singular = candidate_outside_singular.valid;
		inside_periodic = candidate_inside_periodic;
		outside_periodic = candidate_outside_periodic;
		/* A reciprocal pair is only duplicate seam topology when the
		 * bridge edge retains another complete seam pair after these two
		 * trims are removed.  With exactly two edge uses, deleting the
		 * pair would discard the sole zero-area connector while leaving
		 * both exposed closed boundaries in one loop.  That corrupts the
		 * STEP keyhole into a two-trim chain between distinct topology
		 * vertices.  Split those components below instead. */
		remove_periodic_duplicate_only = candidate_inside_periodic &&
		    candidate_outside_periodic && first_trim->Edge() &&
		    first_trim->Edge()->m_ti.Count() > 2;
		inside_singular = candidate_inside_singular;
		outside_singular = candidate_outside_singular;
		break;
	    }
	    const bool better_failure = inside_gap < best_failed_inside_gap ||
		(fabs(inside_gap - best_failed_inside_gap) <= parameter_tolerance &&
		 outside_gap < best_failed_outside_gap);
	    if (failure_reason && better_failure) {
		best_failed_inside_gap = inside_gap;
		best_failed_outside_gap = outside_gap;
		*failure_reason = "best candidate bridge trims " +
		    std::to_string(first_trim->m_trim_index) + "/" +
		    std::to_string(second_trim->m_trim_index) +
		    " left p-space gaps " + std::to_string(inside_gap) + "/" +
		    std::to_string(outside_gap) + ", periodic closure " +
		    (candidate_inside_periodic ? "yes" : "no") + "/" +
		    (candidate_outside_periodic ? "yes" : "no") +
		    ", singular closure " +
		    (candidate_inside_singular.valid ? "yes" : "no") + "/" +
		    (candidate_outside_singular.valid ? "yes" : "no") +
		    ", maximum singular lift " +
		    std::to_string(candidate_inside_singular.maximum_lift_distance) +
		    "/" +
		    std::to_string(candidate_outside_singular.maximum_lift_distance);
	    }
	    if (rejected_candidate_count < 8) {
		rejected_candidates
		    << (rejected_candidate_count ? "; " : "")
		    << first_trim->m_trim_index << "/"
		    << second_trim->m_trim_index
		    << " inside=" << inside_gap
		    << " outside=" << outside_gap
		    << " periodic=" << (candidate_inside_periodic ? "yes" : "no")
		    << "/" << (candidate_outside_periodic ? "yes" : "no")
		    << " singular=" << (candidate_inside_singular.valid ? "yes" : "no")
		    << "/" << (candidate_outside_singular.valid ? "yes" : "no");
		++rejected_candidate_count;
	    }
	    continue;
	}
    }

    if (first_offset < 0 || second_offset < 0) {
	if (deferred_two_use_periodic_keyhole) {
	    if (failure_reason)
		failure_reason->clear();
	    return false;
	}
	if (failure_reason && rejected_candidate_count)
	    *failure_reason += "; rejected candidates: " +
		rejected_candidates.str();
	return false;
    }

    if (remove_periodic_duplicate_only) {
	const int source_loop_tag = loop->m_loop_user.i;
	const int first_trim_index = loop->m_ti[first_offset];
	const int second_trim_index = loop->m_ti[second_offset];
	const int higher_trim = std::max(first_trim_index, second_trim_index);
	const int lower_trim = std::min(first_trim_index, second_trim_index);
	/* Each STEP use of a surface-seam edge may have produced a trim on both
	 * sides of the parameter domain.  When the two coincident reverse trims
	 * are only a duplicate bridge between periodic chains, remove that pair
	 * from the original loop.  The remaining seam pair still owns the edge. */
	brep->DeleteTrim(brep->m_T[higher_trim], false);
	brep->DeleteTrim(brep->m_T[lower_trim], false);
	const bool compacted = brep->Compact();
	if (compacted && failure_reason)
	    *failure_reason = "accepted periodic duplicate removal for STEP loop " +
		std::to_string(source_loop_tag) + " (both exposed chains had "
		"model-space periodic closure)";
	return compacted;
    }

    const ON_BrepLoop::TYPE original_type = loop->m_type;
    const int original_loop_source_tag = loop->m_loop_user.i;
    const int face_index = loop->m_fi;
    if (face_index < 0 || face_index >= brep->m_F.Count())
	return false;
    const int bridge_trim_1 = loop->m_ti[first_offset];
    const int bridge_trim_2 = loop->m_ti[second_offset];

    std::vector<int> inside_trims;
    std::vector<int> outside_trims;
    for (int offset = first_offset + 1; offset < second_offset; ++offset)
	inside_trims.push_back(loop->m_ti[offset]);
    for (int offset = second_offset + 1; offset < trim_count; ++offset)
	outside_trims.push_back(loop->m_ti[offset]);
    for (int offset = 0; offset < first_offset; ++offset)
	outside_trims.push_back(loop->m_ti[offset]);
    if (inside_trims.empty() || outside_trims.empty()) {
	if (failure_reason)
	    *failure_reason = "candidate keyhole bridge did not enclose two trim chains";
	return false;
    }

    std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));

    const auto combine_periodic_vertices = [&](const std::vector<int> &trims) {
	if (trims.empty()) return false;
	const int last_vertex = brep->m_T[trims.back()].m_vi[1];
	const int first_vertex = brep->m_T[trims.front()].m_vi[0];
	if (last_vertex < 0 || last_vertex >= brep->m_V.Count() ||
		first_vertex < 0 || first_vertex >= brep->m_V.Count())
	    return false;
	return last_vertex == first_vertex || brep->CombineCoincidentVertices(
	    brep->m_V[last_vertex], brep->m_V[first_vertex]);
    };
    if ((inside_periodic && !combine_periodic_vertices(inside_trims)) ||
	    (outside_periodic && !combine_periodic_vertices(outside_trims))) {
	*brep = *rollback;
	return false;
    }

    int inside_singular_c2 = -1;
    int inside_singular_vertex = -1;
    if (insert_inside_singular) {
	std::unique_ptr<ON_LineCurve> singular_curve(new ON_LineCurve(
	    inside_singular.start, inside_singular.end));
	if (!singular_curve->ChangeDimension(2) || !singular_curve->IsValid()) {
	    *brep = *rollback;
	    return false;
	}
	inside_singular_c2 = brep->AddTrimCurve(singular_curve.release());
	if (inside_singular_c2 < 0) {
	    *brep = *rollback;
	    return false;
	}
	const int inside_last_trim = inside_trims.back();
	const int inside_first_trim = inside_trims.front();
	inside_singular_vertex = brep->m_T[inside_last_trim].m_vi[1];
	const int other_vertex = brep->m_T[inside_first_trim].m_vi[0];
	if (other_vertex != inside_singular_vertex &&
		!brep->CombineCoincidentVertices(
		    brep->m_V[inside_singular_vertex],
		    brep->m_V[other_vertex])) {
	    *brep = *rollback;
	    return false;
	}
	inside_singular_vertex = brep->m_T[inside_last_trim].m_vi[1];
    }

    int outside_singular_c2 = -1;
    if (insert_outside_singular) {
	std::unique_ptr<ON_LineCurve> singular_curve(new ON_LineCurve(
	    outside_singular.start, outside_singular.end));
	if (!singular_curve->ChangeDimension(2) || !singular_curve->IsValid()) {
	    *brep = *rollback;
	    return false;
	}
	outside_singular_c2 = brep->AddTrimCurve(singular_curve.release());
	if (outside_singular_c2 < 0) {
	    *brep = *rollback;
	    return false;
	}
	int outside_singular_vertex = brep->m_T[outside_trims.back()].m_vi[1];
	const int outside_singular_other_vertex =
	    brep->m_T[outside_trims.front()].m_vi[0];
	if (outside_singular_other_vertex != outside_singular_vertex &&
		!brep->CombineCoincidentVertices(
		    brep->m_V[outside_singular_vertex],
		    brep->m_V[outside_singular_other_vertex])) {
	    *brep = *rollback;
	    return false;
	}
	outside_singular_vertex = brep->m_T[outside_trims.back()].m_vi[1];
	outside_singular.vertex = outside_singular_vertex;
    }

    const int new_loop_index = brep->NewLoop(original_type).m_loop_index;
    loop = &brep->m_L[loop_index];
    ON_BrepLoop *new_loop = &brep->m_L[new_loop_index];
    new_loop->m_loop_user.i = original_loop_source_tag;
    loop->m_ti.SetCount(0);
    for (std::vector<int>::const_iterator trim = outside_trims.begin();
	 trim != outside_trims.end(); ++trim) {
	loop->m_ti.Append(*trim);
	brep->m_T[*trim].m_li = loop_index;
    }
    for (std::vector<int>::const_iterator trim = inside_trims.begin();
	 trim != inside_trims.end(); ++trim) {
	new_loop->m_ti.Append(*trim);
	brep->m_T[*trim].m_li = new_loop_index;
    }
    loop->m_fi = face_index;
    new_loop->m_fi = face_index;
    if (insert_outside_singular) {
	ON_BrepTrim &singular_trim = brep->NewSingularTrim(
	    brep->m_V[outside_singular.vertex], *loop,
	    outside_singular.iso, outside_singular_c2);
	singular_trim.m_tolerance[0] = LocalUnits::tolerance;
	singular_trim.m_tolerance[1] = LocalUnits::tolerance;
	loop = &brep->m_L[loop_index];
	new_loop = &brep->m_L[new_loop_index];
    }
    if (insert_inside_singular) {
	ON_BrepTrim &singular_trim = brep->NewSingularTrim(
	    brep->m_V[inside_singular_vertex], *new_loop,
	    inside_singular.iso, inside_singular_c2);
	singular_trim.m_tolerance[0] = LocalUnits::tolerance;
	singular_trim.m_tolerance[1] = LocalUnits::tolerance;
	loop = &brep->m_L[loop_index];
	new_loop = &brep->m_L[new_loop_index];
    }
    loop->m_type = brep->ComputeLoopType(*loop);
    new_loop->m_type = brep->ComputeLoopType(*new_loop);

    const bool original_loop_valid = loop->m_type == ON_BrepLoop::outer ||
	loop->m_type == ON_BrepLoop::inner;
    const bool new_loop_valid = new_loop->m_type == ON_BrepLoop::outer ||
	new_loop->m_type == ON_BrepLoop::inner;
    const bool face_topology_valid = original_type == ON_BrepLoop::outer ?
	(loop->m_type == ON_BrepLoop::outer || new_loop->m_type == ON_BrepLoop::outer) :
	(loop->m_type == ON_BrepLoop::inner && new_loop->m_type == ON_BrepLoop::inner);
    if (!original_loop_valid || !new_loop_valid || !face_topology_valid) {
	if (failure_reason) {
	    std::ostringstream details;
	    const ON_BrepFace &candidate_face = brep->m_F[face_index];
	    const ON_Surface *candidate_surface = candidate_face.SurfaceOf();
	    details << "candidate split produced loop types "
		<< static_cast<int>(loop->m_type) << "/"
		<< static_cast<int>(new_loop->m_type)
		<< " from original type " << static_cast<int>(original_type)
		<< ", directions " << brep->LoopDirection(*loop) << "/"
		<< brep->LoopDirection(*new_loop)
		<< ", face " << face_index
		<< ", STEP loop " << loop->m_loop_user.i
		<< ", face loops " << candidate_face.LoopCount()
		<< ", face reversed " << (candidate_face.m_bRev ? "yes" : "no");
	    if (candidate_surface) {
		const ON_ClassId *class_id = candidate_surface->ClassId();
		details << ", surface " << (class_id ? class_id->ClassName() : "unknown")
		    << " closed=" << (candidate_surface->IsClosed(0) ? "1" : "0")
		    << (candidate_surface->IsClosed(1) ? "1" : "0");
	    }
	    const ON_BrepLoop *candidate_loops[2] = {loop, new_loop};
	    for (int candidate = 0; candidate < 2; ++candidate) {
		details << ", chain" << candidate << "=[";
		const ON_BrepLoop *candidate_loop = candidate_loops[candidate];
		for (int lti = 0; lti < candidate_loop->TrimCount(); ++lti) {
		    const ON_BrepTrim *candidate_trim = candidate_loop->Trim(lti);
		    if (lti > 0)
			details << " ";
		    if (!candidate_trim) {
			details << "invalid";
			continue;
		    }
		    const ON_3dPoint start = candidate_trim->PointAtStart();
		    const ON_3dPoint end = candidate_trim->PointAtEnd();
		    const ON_Curve *trim_curve = candidate_trim->TrimCurveOf();
		    const ON_BrepEdge *candidate_edge = candidate_trim->Edge();
		    double sampled_area = 0.0;
		    if (trim_curve) {
			const ON_Interval domain = candidate_trim->Domain();
			ON_3dPoint previous = candidate_trim->PointAt(domain.Min());
			for (int sample = 1; sample <= 128; ++sample) {
			    const ON_3dPoint current = candidate_trim->PointAt(
				domain.ParameterAt(static_cast<double>(sample) / 128.0));
			    sampled_area += previous.x * current.y - current.x * previous.y;
			    previous = current;
			}
			sampled_area *= 0.5;
		    }
		    details << candidate_trim->m_trim_index
			<< "(rev=" << (candidate_trim->m_bRev3d ? "1" : "0")
			<< ",type=" << static_cast<int>(candidate_trim->m_type)
			<< ",iso=" << static_cast<int>(candidate_trim->m_iso)
			<< ",edge=" << candidate_trim->m_ei
			<< "/STEP" << (candidate_edge ? candidate_edge->m_edge_user.i : 0)
			<< ",c2=" << (trim_curve && trim_curve->ClassId() ?
			    trim_curve->ClassId()->ClassName() : "none")
			<< ",box=" << candidate_trim->m_pbox.m_min.x << ":"
			<< candidate_trim->m_pbox.m_max.x << ","
			<< candidate_trim->m_pbox.m_min.y << ":"
			<< candidate_trim->m_pbox.m_max.y
			<< ",area=" << sampled_area
			<< "," << start.x << ":" << start.y
			<< "->" << end.x << ":" << end.y << ")";
		}
		details << "]";
	    }
	    *failure_reason = details.str();
	}
	*brep = *rollback;
	return false;
    }

    ON_BrepFace *face = &brep->m_F[face_index];
    if (loop->m_type == original_type && new_loop->m_type == ON_BrepLoop::inner) {
	face->m_li.Append(new_loop_index);
    } else if (original_type == ON_BrepLoop::outer &&
	loop->m_type == ON_BrepLoop::inner && new_loop->m_type == ON_BrepLoop::outer) {
	face->m_li.Insert(0, new_loop_index);
    } else if (loop->m_type == original_type && new_loop->m_type == ON_BrepLoop::outer) {
	const ON_Surface *source_surface = face->SurfaceOf();
	if (source_surface &&
		(source_surface->IsClosed(0) || source_surface->IsClosed(1))) {
	    /* Both cycles still belong to the one authoritative STEP
	     * ADVANCED_FACE.  Signed Euclidean UV area cannot distinguish the
	     * outer and inner sides of a full-period cylindrical/toroidal band:
	     * both wrapped cycles commonly classify as outer.  Moving one cycle
	     * to a synthetic face invents two impossible single-boundary
	     * cylindrical faces.  Retain the source association and mark the
	     * detached cycle inner provisionally; the exact edge-winding band
	     * repair later proves the pairing and installs explicit seam
	     * topology before the BREP is accepted. */
	    new_loop->m_type = ON_BrepLoop::inner;
	    face->m_li.Append(new_loop_index);
	} else {
	    const int surface_index = face->m_si;
	    const bool reversed = face->m_bRev;
	    const int source_face_tag = face->m_face_user.i;
	    ON_BrepFace &new_face = brep->NewFace(surface_index);
	    new_face.m_bRev = reversed;
	    new_face.m_face_user.i = source_face_tag;
	    new_face.m_li.Append(new_loop_index);
	    brep->m_L[new_loop_index].m_fi = new_face.m_face_index;
	}
    } else {
	/* This is the remaining valid keyhole arrangement: the original loop
	 * becomes outer and the detached loop remains inner on the same face. */
	face->m_li.Append(new_loop_index);
    }

    brep->m_T[bridge_trim_1].m_li = -1;
    brep->m_T[bridge_trim_2].m_li = -1;
    const int bridge_edge = brep->m_T[bridge_trim_1].m_ei;
    bool bridge_owns_edge = bridge_edge >= 0 && bridge_edge < brep->m_E.Count();
    for (std::vector<int>::const_iterator trim = outside_trims.begin();
	 bridge_owns_edge && trim != outside_trims.end(); ++trim)
	if (brep->m_T[*trim].m_ei == bridge_edge) bridge_owns_edge = false;
    for (std::vector<int>::const_iterator trim = inside_trims.begin();
	 bridge_owns_edge && trim != inside_trims.end(); ++trim)
	if (brep->m_T[*trim].m_ei == bridge_edge) bridge_owns_edge = false;
    /* A closed-surface keyhole can contribute two extra trims to an otherwise
     * legitimate seam edge.  Preserve that edge and its remaining seam pair;
     * delete the edge only when the bridge pair are its final uses. */
    const int higher_bridge_trim = std::max(bridge_trim_1, bridge_trim_2);
    const int lower_bridge_trim = std::min(bridge_trim_1, bridge_trim_2);
    /* Delete the higher array index first because openNURBS compacts the trim
     * array immediately and renumbers the remaining topology references. */
    brep->DeleteTrim(brep->m_T[higher_bridge_trim], false);
    brep->DeleteTrim(brep->m_T[lower_bridge_trim], bridge_owns_edge);
    const int final_first_type = static_cast<int>(loop->m_type);
    const int final_second_type = static_cast<int>(new_loop->m_type);
    const int final_first_direction = brep->LoopDirection(*loop);
    const int final_second_direction = brep->LoopDirection(*new_loop);
    const bool compacted = brep->Compact();
    if (compacted && failure_reason) {
	std::ostringstream details;
	details << "accepted keyhole split for STEP loop "
	    << original_loop_source_tag << " periodic=" << (outside_periodic ? 1 : 0)
	    << '/' << (inside_periodic ? 1 : 0) << " types="
	    << final_first_type << '/' << final_second_type << " directions="
	    << final_first_direction << '/' << final_second_direction;
	*failure_reason = details.str();
    }
    return compacted;
}


size_t
classify_exact_polyline_seams(ON_Brep *brep)
{
    if (!brep)
	return 0;
    size_t classified = 0;
    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	if (brlcad::PullbackWorkCancelled())
	    return classified;
	ON_BrepTrim &trim = brep->m_T[ti];
	if (trim.m_type != ON_BrepTrim::seam ||
		(trim.m_iso == ON_Surface::W_iso || trim.m_iso == ON_Surface::E_iso ||
		 trim.m_iso == ON_Surface::S_iso || trim.m_iso == ON_Surface::N_iso) ||
		trim.m_li < 0 || trim.m_li >= brep->m_L.Count())
	    continue;
	const int face_index = brep->m_L[trim.m_li].m_fi;
	if (face_index < 0 || face_index >= brep->m_F.Count())
	    continue;
	const ON_Surface *surface = brep->m_F[face_index].SurfaceOf();
	const ON_PolylineCurve *polyline = ON_PolylineCurve::Cast(trim.TrimCurveOf());
	if (!surface || !polyline || polyline->m_pline.Count() < 2)
	    continue;
	for (int direction = 0; direction < 2 && trim.m_iso == ON_Surface::not_iso;
	     ++direction) {
	    if (!surface->IsClosed(direction))
		continue;
	    const ON_Interval domain = surface->Domain(direction);
	    for (int side = 0; side < 2; ++side) {
		const double boundary = domain[side];
		bool exact = true;
		for (int point = 0; point < polyline->m_pline.Count(); ++point) {
		    if (fabs(polyline->m_pline[point][direction] - boundary) >
			    ON_ZERO_TOLERANCE) {
			exact = false;
			break;
		    }
		}
		if (!exact)
		    continue;
		if (direction == 0)
		    trim.m_iso = side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso;
		else
		    trim.m_iso = side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso;
		++classified;
		break;
	    }
	}
    }
    return classified;
}


void
refresh_brep_flags_preserving_singular_isos(ON_Brep *brep,
	bool set_loop_type, STEPWrapper *wrapper, int entity_id,
	const std::string *entity_type)
{
    if (!brep)
	return;

    std::vector<ON_Surface::ISO> proven_singular_isos(
	brep->m_T.Count(), ON_Surface::not_iso);
	std::vector<ON_Surface::ISO> proven_seam_isos(
	    brep->m_T.Count(), ON_Surface::not_iso);

	/* SetTolerancesBoxesAndFlags() occasionally reduces one member of an exact
	 * W/E or S/N seam pair to a generic isoparametric flag when an endpoint is
	 * singular.  Re-prove complementary seam sides from the complete pcurves
	 * and their common directed 3-D edge before the generic refresh, then
	 * restore only those independently proven flags afterwards. */
	const auto infer_boundary_iso = [](const ON_BrepTrim &trim,
		const ON_Surface *surface) {
	    const ON_Interval curve_domain = trim.Domain();
	    if (!surface || !curve_domain.IsIncreasing())
		return ON_Surface::not_iso;
	    ON_Surface::ISO inferred = ON_Surface::not_iso;
	    for (int direction = 0; direction < 2; ++direction) {
		const ON_Interval surface_domain = surface->Domain(direction);
		if (!surface_domain.IsIncreasing())
		    continue;
		const double epsilon = std::max(ON_ZERO_TOLERANCE *
		    kNumericalToleranceScale, surface_domain.Length() * 1.0e-10);
		for (int side = 0; side < 2; ++side) {
		    bool on_boundary = true;
		    double varying_minimum = DBL_MAX;
		    double varying_maximum = -DBL_MAX;
		    for (int sample = 0; sample <= kPcurveLocusScreeningSegments;
			    ++sample) {
			const ON_3dPoint uv = trim.PointAt(
			    curve_domain.ParameterAt(static_cast<double>(sample) /
				kPcurveLocusScreeningSegments));
			if (!uv.IsValid() || fabs(uv[direction] -
				surface_domain[side]) > epsilon) {
			    on_boundary = false;
			    break;
			}
			varying_minimum = std::min(varying_minimum,
			    uv[1 - direction]);
			varying_maximum = std::max(varying_maximum,
			    uv[1 - direction]);
		    }
		    if (!on_boundary || varying_maximum - varying_minimum <= epsilon)
			continue;
		    const ON_Surface::ISO candidate = direction == 0 ?
			(side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
			(side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
		    if (inferred != ON_Surface::not_iso && inferred != candidate)
			return ON_Surface::not_iso;
		    inferred = candidate;
		}
	    }
	    return inferred;
	};
	const auto complementary_seam_isos = [](ON_Surface::ISO first,
		ON_Surface::ISO second) {
	    return (first == ON_Surface::W_iso && second == ON_Surface::E_iso) ||
		(first == ON_Surface::E_iso && second == ON_Surface::W_iso) ||
		(first == ON_Surface::S_iso && second == ON_Surface::N_iso) ||
		(first == ON_Surface::N_iso && second == ON_Surface::S_iso);
	};
	for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	    const ON_BrepEdge &edge = brep->m_E[ei];
	    if (edge.m_ti.Count() < 2)
		continue;
	    for (int first_use = 0; first_use < edge.m_ti.Count(); ++first_use) {
	    const int first_index = edge.m_ti[first_use];
	    if (first_index < 0 || first_index >= brep->m_T.Count())
		continue;
	    const ON_BrepTrim &first = brep->m_T[first_index];
	    if (first.m_li < 0 || first.m_type != ON_BrepTrim::seam)
		continue;
	    int same_loop_count = 0;
	    int second_index = -1;
	    for (int use = 0; use < edge.m_ti.Count(); ++use) {
		const int candidate_index = edge.m_ti[use];
		if (candidate_index < 0 || candidate_index >= brep->m_T.Count() ||
			brep->m_T[candidate_index].m_li != first.m_li ||
			brep->m_T[candidate_index].m_type != ON_BrepTrim::seam)
		    continue;
		++same_loop_count;
		if (candidate_index != first_index)
		    second_index = candidate_index;
	    }
	    /* OpenNURBS permits an edge to have uses on other faces; a seam is the
	     * unique pair of uses in this loop, not necessarily the edge's only two
	     * uses in the complete BREP. */
	    if (same_loop_count != 2 || second_index < 0 ||
		    first_index > second_index)
		continue;
	    const ON_BrepTrim &second = brep->m_T[second_index];
	    const ON_BrepFace *first_face = first.Face();
	    const ON_BrepFace *second_face = second.Face();
	    const ON_Surface *surface = first_face ? first_face->SurfaceOf() : NULL;
	    if (second.m_type != ON_BrepTrim::seam || !surface ||
		    first.m_li != second.m_li ||
		    first_face != second_face || surface != second_face->SurfaceOf())
		continue;
	    const ON_Surface::ISO first_iso = infer_boundary_iso(first, surface);
	    const ON_Surface::ISO second_iso = infer_boundary_iso(second, surface);
	    if (!complementary_seam_isos(first_iso, second_iso))
		continue;
	    double tolerance = std::max(LocalUnits::tolerance, edge.m_tolerance);
	    tolerance = std::max(tolerance,
		std::max(first.m_tolerance[0], first.m_tolerance[1]));
	    tolerance = std::max(tolerance,
		std::max(second.m_tolerance[0], second.m_tolerance[1]));
	    ON_NurbsCurve edge_nurbs;
	    if (!edge.GetNurbForm(edge_nurbs))
		continue;
	    bool exact = true;
	    const ON_BrepTrim *pair[2] = {&first, &second};
	    for (int member = 0; exact && member < 2; ++member) {
		const ON_Interval trim_domain = pair[member]->Domain();
		for (int sample = 0; sample <= kPcurveLocusScreeningSegments;
			++sample) {
		    const double fraction = static_cast<double>(sample) /
			kPcurveLocusScreeningSegments;
		    const ON_3dPoint uv = pair[member]->PointAt(
			trim_domain.ParameterAt(fraction));
		    const ON_3dPoint lift = closed_surface_point_at(surface, uv);
		    const ON_3dPoint edge_point = edge.PointAt(
			edge.Domain().ParameterAt(pair[member]->m_bRev3d ?
			    1.0 - fraction : fraction));
		    double locus_distance = lift.IsValid() && edge_point.IsValid() ?
			lift.DistanceTo(edge_point) : DBL_MAX;
		    if (locus_distance > tolerance && lift.IsValid()) {
			double closest_parameter = 0.0;
			if (ON_NurbsCurve_GetClosestPoint(&closest_parameter,
				&edge_nurbs, lift))
			    locus_distance = std::min(locus_distance,
				lift.DistanceTo(edge_nurbs.PointAt(
				    closest_parameter)));
		    }
		    exact = locus_distance <= tolerance;
		}
	    }
	    if (exact) {
		proven_seam_isos[first_index] = first_iso;
		proven_seam_isos[second_index] = second_iso;
	    }
	    }
	}

    /* Recover or re-prove a boundary flag only from the singular trim itself:
     * its complete 2-D locus must be constant on one domain boundary, vary
     * along that boundary, and densely lift to its one topology vertex. */
    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	if (brlcad::PullbackWorkCancelled())
	    return;
	ON_BrepTrim &trim = brep->m_T[ti];
	if (trim.m_type != ON_BrepTrim::singular ||
		trim.m_li < 0 || trim.m_li >= brep->m_L.Count() ||
		trim.m_vi[0] < 0 || trim.m_vi[0] >= brep->m_V.Count())
	    continue;
	const ON_BrepLoop &loop = brep->m_L[trim.m_li];
	const ON_Surface *surface = loop.Face() ? loop.Face()->SurfaceOf() : NULL;
	const ON_Interval trim_domain = trim.Domain();
	if (!surface || !trim_domain.IsIncreasing())
	    continue;
	const ON_3dPoint vertex = brep->m_V[trim.m_vi[0]].point;
	/* A singular trim produced by bounded collapsed-boundary repair carries
	 * the densely measured discrepancy in its local trim and topology-vertex
	 * tolerances.  Re-proving its boundary flag with only the file-wide
	 * uncertainty discards that evidence and can leave OpenNURBS with an
	 * invalid singular/not_iso combination.  These values are enlarged only
	 * by the earlier complete-locus proof; exact mode therefore continues to
	 * use the declared model tolerance. */
	const double singular_tolerance = std::max(LocalUnits::tolerance,
	    std::max(brep->m_V[trim.m_vi[0]].m_tolerance,
		std::max(trim.m_tolerance[0], trim.m_tolerance[1])));
	ON_BoundingBox surface_bounds;
	const double surface_scale = surface->GetBoundingBox(surface_bounds,
	    false) && surface_bounds.IsValid() ?
	    surface_bounds.Diagonal().Length() : 0.0;
	const bool allow_measured_tolerance = wrapper &&
	    !wrapper->ImportOptions().exact && wrapper->ImportOptions().repair ==
		brlcad::step::RepairMode::Safe;
	const double singular_adjustment_limit = allow_measured_tolerance ?
	    std::max(singular_tolerance,
		surface_scale * kCollapsedBoundaryMaximumRelativeMismatch) :
	    singular_tolerance;
	ON_Surface::ISO recovered = ON_Surface::not_iso;
	int recovered_fixed_direction = -1;
	double recovered_boundary = 0.0;
	for (int fixed_direction = 0; fixed_direction < 2; ++fixed_direction) {
	    const ON_Interval fixed_domain = surface->Domain(fixed_direction);
	    const ON_Interval varying_domain = surface->Domain(1 - fixed_direction);
	    if (!fixed_domain.IsIncreasing() || !varying_domain.IsIncreasing())
		continue;
	    const double parameter_epsilon = std::max(ON_ZERO_TOLERANCE,
		fixed_domain.Length() * 1.0e-10);
	    for (int side = 0; side < 2; ++side) {
		const double boundary = fixed_domain[side];
		bool exact_boundary = true;
		double varying_minimum = DBL_MAX;
		double varying_maximum = -DBL_MAX;
		for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
		    const double fraction = static_cast<double>(sample) /
			kDenseValidationSegments;
		    const ON_3dPoint uv = trim.PointAt(
			trim_domain.ParameterAt(fraction));
		    const ON_3dPoint lift = closed_surface_point_at(surface, uv);
		    if (!uv.IsValid() || fabs(uv[fixed_direction] - boundary) >
			    parameter_epsilon || !lift.IsValid() ||
			    lift.DistanceTo(vertex) > singular_tolerance) {
			exact_boundary = false;
			break;
		    }
		    varying_minimum = std::min(varying_minimum,
			uv[1 - fixed_direction]);
		    varying_maximum = std::max(varying_maximum,
			uv[1 - fixed_direction]);
		}
		if (!exact_boundary || varying_maximum - varying_minimum <=
			parameter_epsilon)
		    continue;
		const ON_Surface::ISO candidate = fixed_direction == 0 ?
		    (side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
		    (side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
		if (recovered != ON_Surface::not_iso && recovered != candidate) {
		    recovered = ON_Surface::not_iso;
		    exact_boundary = false;
		    break;
		}
		recovered = candidate;
		recovered_fixed_direction = fixed_direction;
		recovered_boundary = boundary;
		}
	    }
	/* A singular trim created on a proven collapsed side can subsequently sit
	 * just outside OpenNURBS' very small parameter-boundary window after exact
	 * seam and domain normalization.  Use its existing side only as a proposal:
	 * both the supplied locus and the exact snapped boundary must densely lift
	 * to the same topology vertex before accepting it. */
	if (recovered == ON_Surface::not_iso &&
		(trim.m_iso == ON_Surface::W_iso ||
		 trim.m_iso == ON_Surface::S_iso ||
		 trim.m_iso == ON_Surface::E_iso ||
		 trim.m_iso == ON_Surface::N_iso)) {
	    const bool fixed_u = trim.m_iso == ON_Surface::W_iso ||
		trim.m_iso == ON_Surface::E_iso;
	    const int fixed_direction = fixed_u ? 0 : 1;
	    const int side = (trim.m_iso == ON_Surface::E_iso ||
		trim.m_iso == ON_Surface::N_iso) ? 1 : 0;
	    const ON_Interval fixed_domain = surface->Domain(fixed_direction);
	    ON_3dPoint start = trim.PointAtStart();
	    ON_3dPoint end = trim.PointAtEnd();
	    start[fixed_direction] = fixed_domain[side];
	    end[fixed_direction] = fixed_domain[side];
	    ON_LineCurve boundary_candidate(start, end);
	    bool exact = fixed_domain.IsIncreasing() &&
		boundary_candidate.ChangeDimension(2) &&
		boundary_candidate.SetDomain(trim_domain.Min(), trim_domain.Max()) &&
		boundary_candidate.IsValid() &&
		surface->IsIsoparametric(boundary_candidate, &trim_domain) == trim.m_iso;
	    for (int sample = 0; exact && sample <= kDenseValidationSegments;
		    sample++) {
		const double parameter = trim_domain.ParameterAt(
		    static_cast<double>(sample) / kDenseValidationSegments);
		const ON_3dPoint original_uv = trim.PointAt(parameter);
		const ON_3dPoint boundary_uv = boundary_candidate.PointAt(parameter);
		const ON_3dPoint original_lift = surface->PointAt(
		    original_uv.x, original_uv.y);
		const ON_3dPoint boundary_lift = surface->PointAt(
		    boundary_uv.x, boundary_uv.y);
		exact = original_lift.IsValid() && boundary_lift.IsValid() &&
		    original_lift.DistanceTo(vertex) <= singular_tolerance &&
		    boundary_lift.DistanceTo(vertex) <= singular_tolerance;
	    }
	    if (exact) {
		recovered = trim.m_iso;
		recovered_fixed_direction = fixed_direction;
		recovered_boundary = fixed_domain[side];
	    }
	}
	/* An earlier bounded endpoint edit may have erased the cached boundary
	 * flag before this refresh runs.  In that case do not guess from proximity
	 * in parameter space.  Try all four exact surface boundaries and accept
	 * only one whose complete projected locus, as well as the supplied locus,
	 * densely lifts to the same topology vertex within the already established
	 * local tolerance.  Requiring a unique, nonzero boundary span prevents a
	 * pole point from being assigned arbitrarily to either parameter direction. */
	if (recovered == ON_Surface::not_iso) {
	    ON_Surface::ISO unique_iso = ON_Surface::not_iso;
	    int unique_fixed_direction = -1;
	    double unique_boundary = ON_UNSET_VALUE;
	    double unique_tolerance = singular_tolerance;
	    bool ambiguous = false;
	    for (int fixed_direction = 0; fixed_direction < 2 && !ambiguous;
		    ++fixed_direction) {
		const ON_Interval fixed_domain = surface->Domain(fixed_direction);
		const ON_Interval varying_domain = surface->Domain(1 - fixed_direction);
		if (!fixed_domain.IsIncreasing() || !varying_domain.IsIncreasing())
		    continue;
		const double span_epsilon = std::max(ON_ZERO_TOLERANCE,
		    varying_domain.Length() * 1.0e-10);
		for (int side = 0; side < 2; ++side) {
		    ON_3dPoint start = trim.PointAtStart();
		    ON_3dPoint end = trim.PointAtEnd();
		    const double boundary = fixed_domain[side];
		    start[fixed_direction] = boundary;
		    end[fixed_direction] = boundary;
		    if (fabs(end[1 - fixed_direction] -
			    start[1 - fixed_direction]) <= span_epsilon)
			continue;
		    ON_LineCurve boundary_candidate(start, end);
		    const ON_Surface::ISO candidate_iso = fixed_direction == 0 ?
			(side == 0 ? ON_Surface::W_iso : ON_Surface::E_iso) :
			(side == 0 ? ON_Surface::S_iso : ON_Surface::N_iso);
		    bool exact = boundary_candidate.ChangeDimension(2) &&
			boundary_candidate.SetDomain(trim_domain.Min(),
			    trim_domain.Max()) && boundary_candidate.IsValid() &&
			surface->IsIsoparametric(boundary_candidate,
			    &trim_domain) == candidate_iso;
		    double measured_tolerance = 0.0;
		    /* Prove the complete surface side is actually collapsed.  A short
		     * curve near a pole can otherwise look singular even when projected
		     * onto an ordinary, non-collapsed parameter boundary. */
		    for (int sample = 0; exact && sample <=
			    kDenseValidationSegments; ++sample) {
			ON_3dPoint boundary_uv;
			boundary_uv[fixed_direction] = boundary;
			boundary_uv[1 - fixed_direction] =
			    varying_domain.ParameterAt(static_cast<double>(sample) /
				kDenseValidationSegments);
			boundary_uv.z = 0.0;
			const ON_3dPoint boundary_lift = surface->PointAt(
			    boundary_uv.x, boundary_uv.y);
			const double distance = boundary_lift.IsValid() ?
			    boundary_lift.DistanceTo(vertex) : DBL_MAX;
			measured_tolerance = std::max(measured_tolerance, distance);
			exact = distance <= singular_adjustment_limit;
		    }
		    for (int sample = 0; exact && sample <=
			    kDenseValidationSegments; ++sample) {
			const double parameter = trim_domain.ParameterAt(
			    static_cast<double>(sample) /
			    kDenseValidationSegments);
			const ON_3dPoint original_uv = trim.PointAt(parameter);
			const ON_3dPoint candidate_uv =
			    boundary_candidate.PointAt(parameter);
			const ON_3dPoint original_lift = surface->PointAt(
			    original_uv.x, original_uv.y);
			const ON_3dPoint candidate_lift = closed_surface_point_at(
			    surface, candidate_uv);
			const double original_vertex_distance = original_lift.IsValid() ?
			    original_lift.DistanceTo(vertex) : DBL_MAX;
			const double candidate_vertex_distance = candidate_lift.IsValid() ?
			    candidate_lift.DistanceTo(vertex) : DBL_MAX;
			const double projection_distance = original_lift.IsValid() &&
			    candidate_lift.IsValid() ?
			    original_lift.DistanceTo(candidate_lift) : DBL_MAX;
			measured_tolerance = std::max(measured_tolerance,
			    std::max(original_vertex_distance,
				std::max(candidate_vertex_distance,
				    projection_distance)));
			exact = measured_tolerance <= singular_adjustment_limit;
		    }
		    if (exact && measured_tolerance > singular_tolerance)
			exact = measured_tolerance * kRegenerationToleranceSafety <=
			    singular_adjustment_limit;
		    if (!exact)
			continue;
		    if (unique_iso != ON_Surface::not_iso &&
			    unique_iso != candidate_iso) {
			ambiguous = true;
			break;
		    }
		    unique_iso = candidate_iso;
		    unique_fixed_direction = fixed_direction;
		    unique_boundary = boundary;
		    unique_tolerance = measured_tolerance > singular_tolerance ?
			measured_tolerance * kRegenerationToleranceSafety :
			singular_tolerance;
		}
	    }
	    if (!ambiguous && unique_iso != ON_Surface::not_iso) {
		recovered = unique_iso;
		recovered_fixed_direction = unique_fixed_direction;
		recovered_boundary = unique_boundary;
		if (unique_tolerance > singular_tolerance) {
		    trim.m_tolerance[0] = std::max(trim.m_tolerance[0],
			unique_tolerance);
		    trim.m_tolerance[1] = std::max(trim.m_tolerance[1],
			unique_tolerance);
		    brep->m_V[trim.m_vi[0]].m_tolerance = std::max(
			brep->m_V[trim.m_vi[0]].m_tolerance, unique_tolerance);
		    if (wrapper && entity_type) {
			wrapper->RecordDiagnostic(
			    brlcad::step::DiagnosticSeverity::Warning, entity_id,
			    *entity_type, "trim_pcurve",
			    "singular boundary/source geometry exceeded the declared "
			    "tolerance; used a densely measured local tolerance");
			wrapper->RecordRepair(entity_id, *entity_type, "trim_pcurve",
			    "restored an exact singular boundary using its measured "
			    "source-geometry tolerance");
		    }
		}
	    }
	}
	if (recovered != ON_Surface::not_iso) {
	    trim.m_iso = recovered;
	    proven_singular_isos[ti] = recovered;
	    /* A pcurve can be within our scale-aware parameter epsilon of a
	     * collapsed side while still missing OpenNURBS' stricter boundary
	     * classification.  Merely restoring the side flag then makes the trim
	     * self-inconsistent.  Once the complete source locus has been proven to
	     * lift to the one singular vertex, replace it with the exact boundary
	     * line between the same endpoints.  This changes no 3-D geometry and
	     * lets the subsequent derived-flag refresh independently recover the
	     * same W/S/E/N classification. */
	    const ON_Curve *trim_curve = trim.TrimCurveOf();
	    if (recovered_fixed_direction >= 0 && trim_curve &&
		    surface->IsIsoparametric(*trim_curve, &trim_domain) != recovered) {
		ON_3dPoint start = trim.PointAtStart();
		ON_3dPoint end = trim.PointAtEnd();
		start[recovered_fixed_direction] = recovered_boundary;
		end[recovered_fixed_direction] = recovered_boundary;
		std::unique_ptr<ON_LineCurve> boundary_curve(
		    new ON_LineCurve(start, end));
		bool exact = boundary_curve->ChangeDimension(2) &&
		    boundary_curve->SetDomain(trim_domain.Min(), trim_domain.Max()) &&
		    boundary_curve->IsValid() &&
		    surface->IsIsoparametric(*boundary_curve, &trim_domain) == recovered;
		for (int sample = 0; exact && sample <= kDenseValidationSegments;
			sample++) {
		    const ON_3dPoint uv = boundary_curve->PointAt(
			trim_domain.ParameterAt(static_cast<double>(sample) /
			    kDenseValidationSegments));
		    const ON_3dPoint lift = closed_surface_point_at(surface, uv);
		    exact = lift.IsValid() &&
			lift.DistanceTo(vertex) <= singular_tolerance;
		}
		if (exact) {
		    const int c2_index = brep->AddTrimCurve(boundary_curve.release());
		    if (c2_index >= 0)
			(void)brep->SetTrimCurve(trim, c2_index);
		}
	    }
	}
    }

    /* NewSingularTrim receives a boundary ISO only after its caller has
     * proved the complete pcurve lies on a collapsed surface side.  The
     * generic flag refresh cannot infer a direction from every degenerate
     * curve and may replace that boundary with not_iso. */
    if (brlcad::PullbackWorkCancelled())
	return;
    brep->SetTolerancesBoxesAndFlags(false, false, false, false,
	true, true, set_loop_type, true);
    const int count = std::min(brep->m_T.Count(),
	static_cast<int>(proven_singular_isos.size()));
    for (int ti = 0; ti < count; ++ti) {
	if (proven_seam_isos[ti] != ON_Surface::not_iso) {
	    ON_BrepTrim &trim = brep->m_T[ti];
	    trim.m_type = ON_BrepTrim::seam;
	    const ON_BrepFace *face = trim.Face();
	    const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	    const ON_Interval trim_domain = trim.Domain();
	    /* The generic refresh may normalize a proxy curve or its domain.  Do
	     * not restore a pre-refresh boundary flag when the resulting curve no
	     * longer independently derives that same flag: OpenNURBS validates
	     * m_iso against the current curve and rejects an otherwise valid seam
	     * if the cached classification is stale. */
	    if (surface && trim_domain.IsIncreasing() &&
		    surface->IsIsoparametric(trim, &trim_domain) ==
		    proven_seam_isos[ti])
		trim.m_iso = proven_seam_isos[ti];
	}
	if (proven_singular_isos[ti] != ON_Surface::not_iso &&
		brep->m_T[ti].m_type == ON_BrepTrim::singular)
	    brep->m_T[ti].m_iso = proven_singular_isos[ti];
    }
}


/* OpenNURBS' derived-flag refresh assumes every topology array reference is
 * in range and reciprocal.  A half-built exact item must be rejected before
 * calling it: SetTrimIsoFlags() indexes face loops and loop trims without
 * bounds checks, so treating IsValid() as the first structural guard is too
 * late.  This is deliberately a proof-only preflight; it never repairs or
 * removes source topology. */
bool
brep_topology_references_are_safe(const ON_Brep *brep, std::string *failure)
{
    if (failure)
	failure->clear();
    const auto reject = [failure](const std::string &message) {
	if (failure)
	    *failure = message;
	return false;
    };
    if (!brep)
	return reject("null BREP");

    for (int fi = 0; fi < brep->m_F.Count(); ++fi) {
	const ON_BrepFace &face = brep->m_F[fi];
	if (face.m_face_index != fi)
	    return reject("face F" + std::to_string(fi) +
		" has inconsistent self index " +
		std::to_string(face.m_face_index));
	if (face.m_si < 0 || face.m_si >= brep->m_S.Count() ||
		!brep->m_S[face.m_si])
	    return reject("face F" + std::to_string(fi) +
		" references invalid surface " + std::to_string(face.m_si));
	if (face.ProxySurface() != brep->m_S[face.m_si])
	    return reject("face F" + std::to_string(fi) +
		" has a surface proxy inconsistent with surface " +
		std::to_string(face.m_si));
	for (int fli = 0; fli < face.m_li.Count(); ++fli) {
	    const int li = face.m_li[fli];
	    if (li < 0 || li >= brep->m_L.Count())
		return reject("face F" + std::to_string(fi) +
		    " references invalid loop " + std::to_string(li));
	    if (brep->m_L[li].m_fi != fi)
		return reject("face F" + std::to_string(fi) + " loop L" +
		    std::to_string(li) + " points to face F" +
		    std::to_string(brep->m_L[li].m_fi));
	}
    }

    for (int li = 0; li < brep->m_L.Count(); ++li) {
	const ON_BrepLoop &loop = brep->m_L[li];
	if (loop.m_loop_index != li)
	    return reject("loop L" + std::to_string(li) +
		" has inconsistent self index " +
		std::to_string(loop.m_loop_index));
	if (loop.m_fi < 0 || loop.m_fi >= brep->m_F.Count())
	    return reject("loop L" + std::to_string(li) +
		" references invalid face " + std::to_string(loop.m_fi));
	for (int lti = 0; lti < loop.m_ti.Count(); ++lti) {
	    const int ti = loop.m_ti[lti];
	    if (ti < 0 || ti >= brep->m_T.Count())
		return reject("loop L" + std::to_string(li) +
		    " references invalid trim " + std::to_string(ti));
	    if (brep->m_T[ti].m_li != li)
		return reject("loop L" + std::to_string(li) + " trim T" +
		    std::to_string(ti) + " points to loop L" +
		    std::to_string(brep->m_T[ti].m_li));
	}
    }

    for (int ti = 0; ti < brep->m_T.Count(); ++ti) {
	const ON_BrepTrim &trim = brep->m_T[ti];
	if (trim.m_trim_index != ti)
	    return reject("trim T" + std::to_string(ti) +
		" has inconsistent self index " +
		std::to_string(trim.m_trim_index));
	if (trim.m_li < 0 || trim.m_li >= brep->m_L.Count())
	    return reject("trim T" + std::to_string(ti) +
		" references invalid loop " + std::to_string(trim.m_li));
	if (trim.m_type != ON_BrepTrim::ptonsrf &&
		(trim.m_c2i < 0 || trim.m_c2i >= brep->m_C2.Count() ||
		 !brep->m_C2[trim.m_c2i]))
	    return reject("trim T" + std::to_string(ti) +
		" references invalid 2-D curve " + std::to_string(trim.m_c2i));
	for (int end = 0; end < 2; ++end) {
	    if (trim.m_vi[end] < 0 || trim.m_vi[end] >= brep->m_V.Count())
		return reject("trim T" + std::to_string(ti) +
		    " references invalid vertex " +
		    std::to_string(trim.m_vi[end]));
	}
	const bool edge_free = trim.m_type == ON_BrepTrim::singular ||
	    trim.m_type == ON_BrepTrim::ptonsrf;
	if (!edge_free && (trim.m_ei < 0 || trim.m_ei >= brep->m_E.Count()))
	    return reject("trim T" + std::to_string(ti) +
		" references invalid edge " + std::to_string(trim.m_ei));
	if (trim.m_ei >= brep->m_E.Count())
	    return reject("trim T" + std::to_string(ti) +
		" references out-of-range edge " + std::to_string(trim.m_ei));
    }

    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_edge_index != ei)
	    return reject("edge E" + std::to_string(ei) +
		" has inconsistent self index " +
		std::to_string(edge.m_edge_index));
	if (edge.m_c3i < 0 || edge.m_c3i >= brep->m_C3.Count() ||
		!brep->m_C3[edge.m_c3i])
	    return reject("edge E" + std::to_string(ei) +
		" references invalid 3-D curve " + std::to_string(edge.m_c3i));
	for (int end = 0; end < 2; ++end) {
	    if (edge.m_vi[end] < 0 || edge.m_vi[end] >= brep->m_V.Count())
		return reject("edge E" + std::to_string(ei) +
		    " references invalid vertex " +
		    std::to_string(edge.m_vi[end]));
	}
	for (int eti = 0; eti < edge.m_ti.Count(); ++eti) {
	    const int ti = edge.m_ti[eti];
	    if (ti < 0 || ti >= brep->m_T.Count())
		return reject("edge E" + std::to_string(ei) +
		    " references invalid trim " + std::to_string(ti));
	    if (brep->m_T[ti].m_ei != ei)
		return reject("edge E" + std::to_string(ei) + " trim T" +
		    std::to_string(ti) + " points to edge E" +
		    std::to_string(brep->m_T[ti].m_ei));
	}
    }

    for (int vi = 0; vi < brep->m_V.Count(); ++vi) {
	const ON_BrepVertex &vertex = brep->m_V[vi];
	if (vertex.m_vertex_index != vi)
	    return reject("vertex V" + std::to_string(vi) +
		" has inconsistent self index " +
		std::to_string(vertex.m_vertex_index));
	for (int vei = 0; vei < vertex.m_ei.Count(); ++vei) {
	    const int ei = vertex.m_ei[vei];
	    if (ei < 0 || ei >= brep->m_E.Count())
		return reject("vertex V" + std::to_string(vi) +
		    " references invalid edge " + std::to_string(ei));
	    const ON_BrepEdge &edge = brep->m_E[ei];
	    if (edge.m_vi[0] != vi && edge.m_vi[1] != vi)
		return reject("vertex V" + std::to_string(vi) + " edge E" +
		    std::to_string(ei) + " has different endpoint vertices");
	}
    }
    return true;
}


/* A face-local periodic repair can split the two uses of one STEP EDGE_CURVE
 * independently.  When both faces choose the same exact split, this leaves
 * duplicate one-use OpenNURBS edges whose internal split vertices have
 * different OpenNURBS indices.  Rejoin one pair only when they retain the same
 * positive STEP edge identity, their bounded 3-D loci contain the same dense
 * samples in both directions, and corresponding endpoint identities and
 * positions agree within their already established tolerances.  Work on a
 * copy so a rejected vertex or edge merge cannot alter the accepted BREP. */
size_t
merge_one_exact_duplicate_step_boundary_edge_pair(ON_Brep *brep,
	std::string *details, bool allow_distinct_step_ids)
{
    if (details)
	details->clear();
    if (!brep || !(LocalUnits::tolerance > 0.0))
	return 0;

    for (int first_index = 0; first_index < brep->m_E.Count();
	    ++first_index) {
	const ON_BrepEdge &first = brep->m_E[first_index];
	if (first.m_edge_user.i <= 0 || first.m_ti.Count() != 1 ||
		first.m_vi[0] < 0 || first.m_vi[1] < 0 ||
		first.m_vi[0] >= brep->m_V.Count() ||
		first.m_vi[1] >= brep->m_V.Count() ||
		!first.EdgeCurveOf())
	    continue;
	const int first_trim_index = first.m_ti[0];
	const ON_BrepTrim *first_trim = brep->Trim(first_trim_index);
	if (!first_trim || first_trim->m_type != ON_BrepTrim::boundary)
	    continue;
	for (int second_index = first_index + 1;
		second_index < brep->m_E.Count(); ++second_index) {
	    const ON_BrepEdge &second = brep->m_E[second_index];
	    if ((!allow_distinct_step_ids &&
		    second.m_edge_user.i != first.m_edge_user.i) ||
		    (allow_distinct_step_ids &&
		    second.m_edge_user.i == first.m_edge_user.i) ||
		    second.m_ti.Count() != 1 ||
		    second.m_vi[0] < 0 || second.m_vi[1] < 0 ||
		    second.m_vi[0] >= brep->m_V.Count() ||
		    second.m_vi[1] >= brep->m_V.Count() ||
		    !second.EdgeCurveOf())
		continue;
	    const int second_trim_index = second.m_ti[0];
	    const ON_BrepTrim *second_trim = brep->Trim(second_trim_index);
	    if (!second_trim || second_trim->m_type != ON_BrepTrim::boundary)
		continue;
	    if (allow_distinct_step_ids) {
		/* Face-local keyhole normalization can remove two exact,
		 * oppositely directed uses of distinct STEP edges.  Their
		 * reciprocal uses then remain as coincident boundary edges on
		 * two other faces.  Pair them only inside the same authoritative
		 * CLOSED_SHELL and only when their positive endpoint identities
		 * make this the unique possible topology pair. */
		const ON_BrepFace *first_face = first_trim->Face();
		const ON_BrepFace *second_face = second_trim->Face();
		if (!first_face || !second_face || first_face == second_face ||
			first_face->m_face_user.i <= 0 ||
			first_face->m_face_user.i != second_face->m_face_user.i)
		    continue;
		const int first_v0 =
		    brep->m_V[first.m_vi[0]].m_vertex_user.i;
		const int first_v1 =
		    brep->m_V[first.m_vi[1]].m_vertex_user.i;
		if (first_v0 <= 0 || first_v1 <= 0)
		    continue;
		size_t matching_endpoint_pairs = 0;
		for (int candidate_index = 0;
			candidate_index < brep->m_E.Count(); ++candidate_index) {
		    const ON_BrepEdge &candidate =
			brep->m_E[candidate_index];
		    if (candidate.m_edge_user.i <= 0 ||
			    candidate.m_ti.Count() != 1 ||
			    candidate.m_vi[0] < 0 || candidate.m_vi[1] < 0 ||
			    candidate.m_vi[0] >= brep->m_V.Count() ||
			    candidate.m_vi[1] >= brep->m_V.Count())
			continue;
		    const ON_BrepTrim *candidate_trim =
			brep->Trim(candidate.m_ti[0]);
		    const ON_BrepFace *candidate_face =
			candidate_trim ? candidate_trim->Face() : NULL;
		    if (!candidate_trim ||
			    candidate_trim->m_type != ON_BrepTrim::boundary ||
			    !candidate_face ||
			    candidate_face->m_face_user.i !=
				first_face->m_face_user.i)
			continue;
		    const int candidate_v0 =
			brep->m_V[candidate.m_vi[0]].m_vertex_user.i;
		    const int candidate_v1 =
			brep->m_V[candidate.m_vi[1]].m_vertex_user.i;
		    if ((candidate_v0 == first_v0 &&
			    candidate_v1 == first_v1) ||
			    (candidate_v0 == first_v1 &&
			    candidate_v1 == first_v0))
			++matching_endpoint_pairs;
		}
		if (matching_endpoint_pairs != 2)
		    continue;
	    }
	    /* This routine reconciles two independently split uses on adjacent
	     * STEP faces.  Pairing fragments from one face can accidentally join
	     * the two zero-length children created at a surface singularity, as
	     * in XS650 STEP edge #1293873.  A legitimate same-face periodic seam
	     * has complementary native-side evidence and is handled by
	     * merge_one_exact_periodic_boundary_edge_pair below. */
	    if (!brlcad::step::DuplicateBoundaryEdgeUsesAreOnDistinctFaces(
		    first, second))
		continue;

	    double edge_tolerance = std::max(LocalUnits::tolerance,
		ON_ZERO_TOLERANCE * kNumericalToleranceScale);
	    if (first.m_tolerance > 0.0)
		edge_tolerance = std::max(edge_tolerance, first.m_tolerance);
	    if (second.m_tolerance > 0.0)
		edge_tolerance = std::max(edge_tolerance, second.m_tolerance);
	    for (int direction = 0; direction < 2; ++direction) {
		if (first_trim->m_tolerance[direction] > 0.0)
		    edge_tolerance = std::max(edge_tolerance,
			first_trim->m_tolerance[direction]);
		if (second_trim->m_tolerance[direction] > 0.0)
		    edge_tolerance = std::max(edge_tolerance,
			second_trim->m_tolerance[direction]);
	    }

	    const bool direct = brlcad::step::DuplicateStepEdgeEndpointsMatch(
		brep->m_V[first.m_vi[0]], brep->m_V[second.m_vi[0]],
		LocalUnits::tolerance, edge_tolerance,
		!allow_distinct_step_ids) &&
		brlcad::step::DuplicateStepEdgeEndpointsMatch(
		brep->m_V[first.m_vi[1]], brep->m_V[second.m_vi[1]],
		LocalUnits::tolerance, edge_tolerance,
		!allow_distinct_step_ids);
	    const bool reversed = !direct &&
		brlcad::step::DuplicateStepEdgeEndpointsMatch(
		brep->m_V[first.m_vi[0]], brep->m_V[second.m_vi[1]],
		LocalUnits::tolerance, edge_tolerance,
		!allow_distinct_step_ids) &&
		brlcad::step::DuplicateStepEdgeEndpointsMatch(
		brep->m_V[first.m_vi[1]], brep->m_V[second.m_vi[0]],
		LocalUnits::tolerance, edge_tolerance,
		!allow_distinct_step_ids);
	    if (!direct && !reversed)
		continue;

	    const ON_Interval first_domain = first.Domain();
	    const ON_Interval second_domain = second.Domain();
	    if (!first_domain.IsIncreasing() || !second_domain.IsIncreasing())
		continue;
	    ON_3dPoint first_points[kPcurveLocusScreeningSegments + 1];
	    ON_3dPoint second_points[kPcurveLocusScreeningSegments + 1];
	    bool valid_samples = true;
	    for (int sample = 0;
		    valid_samples && sample <= kPcurveLocusScreeningSegments;
		    ++sample) {
		const double fraction = static_cast<double>(sample) /
		    kPcurveLocusScreeningSegments;
		first_points[sample] = first.PointAt(
		    first_domain.ParameterAt(fraction));
		second_points[sample] = second.PointAt(
		    second_domain.ParameterAt(fraction));
		valid_samples = first_points[sample].IsValid() &&
		    second_points[sample].IsValid();
	    }
	    if (!valid_samples ||
		    !step_curve_locus_contains_points(&second, first_points,
			kPcurveLocusScreeningSegments + 1, edge_tolerance) ||
		    !step_curve_locus_contains_points(&first, second_points,
			kPcurveLocusScreeningSegments + 1, edge_tolerance))
		continue;

	    std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
	    ON_BrepEdge &candidate_first = candidate->m_E[first_index];
	    ON_BrepEdge &candidate_second = candidate->m_E[second_index];
	    if (reversed && !candidate_second.Reverse()) {
		if (details) *details = "STEP edge #" +
		    std::to_string(first.m_edge_user.i) +
		    " could not reverse an exact duplicate edge";
		continue;
	    }
	    bool merged_vertices = true;
	    for (int endpoint = 0; merged_vertices && endpoint < 2; ++endpoint) {
		const int keep_vertex = candidate_first.m_vi[endpoint];
		const int remove_vertex = candidate_second.m_vi[endpoint];
		if (keep_vertex == remove_vertex)
		    continue;
		merged_vertices = keep_vertex >= 0 && remove_vertex >= 0 &&
		    keep_vertex < candidate->m_V.Count() &&
		    remove_vertex < candidate->m_V.Count() &&
		    candidate->CombineCoincidentVertices(
			candidate->m_V[keep_vertex],
			candidate->m_V[remove_vertex]);
	    }
	    if (!merged_vertices ||
		    !candidate->CombineCoincidentEdges(candidate_first,
			candidate_second)) {
		if (details) *details = "STEP edge #" +
		    std::to_string(first.m_edge_user.i) +
		    " could not install an identity-proven duplicate edge pair";
		continue;
	    }
	    const int merged_edge_index =
		candidate->m_T[first_trim_index].m_ei;
	    if (merged_edge_index < 0 ||
		    merged_edge_index != candidate->m_T[second_trim_index].m_ei ||
		    merged_edge_index >= candidate->m_E.Count() ||
		    candidate->m_E[merged_edge_index].m_ti.Count() != 2) {
		if (details) *details = "STEP edge #" +
		    std::to_string(first.m_edge_user.i) +
		    " did not produce one reciprocal two-use edge";
		continue;
	    }
	    candidate->m_E[merged_edge_index].m_tolerance = edge_tolerance;
	    if (!candidate->Compact() ||
		    !candidate->SetTrimTypeFlags(false)) {
		if (details) *details = "STEP edge #" +
		    std::to_string(first.m_edge_user.i) +
		    " could not compact an exact duplicate edge merge";
		continue;
	    }
	    std::string unsafe_topology;
	    ON_wString validation_messages;
	    ON_TextLog validation_log(validation_messages);
	    if (!brep_topology_references_are_safe(candidate.get(),
		    &unsafe_topology) || !candidate->IsValid(&validation_log)) {
		if (details) {
		    ON_String text(validation_messages);
		    *details = "STEP edge #" +
			std::to_string(first.m_edge_user.i) +
			" failed transactional duplicate-edge validation: " +
			(!unsafe_topology.empty() ? unsafe_topology :
			    text.Array());
		}
		continue;
	    }
	    /* Pairing two one-use edges adds a new face-orientation constraint.
	     * OpenNURBS structural validity does not prove that the complete
	     * constraint graph remains two-colorable.  A wrong but geometrically
	     * coincident pairing can close the last boundary while making every
	     * possible shell orientation contradictory. */
	    const bool before_consistent =
		brlcad::step::FaceOrientationConstraintsAreConsistent(*brep);
	    const bool candidate_consistent =
		brlcad::step::FaceOrientationConstraintsAreConsistent(*candidate);
	    const size_t before_conflicts =
		brlcad::step::FaceOrientationConflictCount(*brep);
	    const size_t candidate_conflicts =
		brlcad::step::FaceOrientationConflictCount(*candidate);
	    if (!candidate_consistent &&
		    (before_consistent ||
		     candidate_conflicts > before_conflicts)) {
		if (details)
		    *details = "STEP edge #" +
			std::to_string(first.m_edge_user.i) +
			" would make the closed-shell face-orientation graph "
			"worse (" + std::to_string(before_conflicts) + "->" +
			std::to_string(candidate_conflicts) + " conflicts)";
		continue;
	    }
	    /* Assignment may reallocate brep's edge array, invalidating first
	     * and second.  Preserve the only source-edge value needed after the
	     * transaction commits. */
	    const int merged_step_edge_id = first.m_edge_user.i;
	    *brep = *candidate;
	    if (details)
		*details = "STEP edge #" +
		    std::to_string(merged_step_edge_id);
	    return 1;
	}
    }
    return 0;
}


/* Some STEP writers encode the two sides of a periodic surface seam as two
 * distinct EDGE_CURVEs with identical vertices and identical 3-D geometry.
 * Each edge then has one boundary use, so an otherwise closed OpenNURBS BREP
 * remains open.  Normalize one such pair only after proving that the trims are
 * in the same loop on complementary native sides and that the complete edge
 * loci agree within the asserted model tolerance.  The candidate remains
 * transactional through reciprocal-reference and OpenNURBS validation. */
size_t
merge_one_exact_periodic_boundary_edge_pair(ON_Brep *brep,
	std::string *details, bool require_global_validation)
{
    if (details)
	details->clear();
    if (!brep || !(LocalUnits::tolerance > 0.0))
	return 0;

    for (int li = 0; li < brep->m_L.Count(); ++li) {
	const ON_BrepLoop &loop = brep->m_L[li];
	const ON_BrepFace *face = loop.Face();
	const ON_Surface *surface = face ? face->SurfaceOf() : NULL;
	if (!surface || loop.TrimCount() < 2 ||
		(!surface->IsClosed(0) && !surface->IsClosed(1)))
	    continue;
	for (int first_offset = 0; first_offset < loop.TrimCount(); ++first_offset) {
	    const ON_BrepTrim *first = loop.Trim(first_offset);
	    if (!first || first->m_type != ON_BrepTrim::boundary ||
		    first->m_ei < 0 || first->m_ei >= brep->m_E.Count())
		continue;
	    const ON_BrepEdge &first_edge = brep->m_E[first->m_ei];
	    if (first_edge.m_ti.Count() != 1)
		continue;
	    const ON_Interval first_trim_domain = first->Domain();
	    if (!first_trim_domain.IsIncreasing())
		continue;
	    for (int second_offset = first_offset + 1;
		    second_offset < loop.TrimCount(); ++second_offset) {
		const ON_BrepTrim *second = loop.Trim(second_offset);
		if (!second || second->m_type != ON_BrepTrim::boundary ||
			second->m_ei < 0 || second->m_ei >= brep->m_E.Count() ||
			second->m_ei == first->m_ei ||
			second->m_bRev3d == first->m_bRev3d)
		    continue;
		const ON_BrepEdge &second_edge = brep->m_E[second->m_ei];
		if (second_edge.m_ti.Count() != 1 ||
			first_edge.m_vi[0] != second_edge.m_vi[0] ||
			first_edge.m_vi[1] != second_edge.m_vi[1])
		    continue;
		const int first_step = first_edge.m_edge_user.i;
		const int second_step = second_edge.m_edge_user.i;
		const std::string candidate_name = "STEP edges #" +
		    std::to_string(first_step) + "/#" +
		    std::to_string(second_step);
		const ON_Interval second_trim_domain = second->Domain();
		if (!second_trim_domain.IsIncreasing()) {
		    if (details) *details = candidate_name +
			" had an invalid second trim domain";
		    continue;
		}
		/* The supplied pcurves may still be on an equivalent periodic image
		 * outside the native domain, so their cached ISO flags need not yet be
		 * complementary W/E or S/N.  Prove the stronger condition directly:
		 * reversed traversal differs by exactly one integral period in one
		 * closed direction and agrees in the open direction at every sample. */
		int closed_direction = -1;
		for (int direction = 0; direction < 2 && closed_direction < 0;
			direction++) {
		    if (!surface->IsClosed(direction))
			continue;
		    const double period = surface->Domain(direction).Length();
		    if (!(period > ON_ZERO_TOLERANCE))
			continue;
		    const double parameter_tolerance = std::max(
			ON_ZERO_TOLERANCE * kNumericalToleranceScale,
			period * kPeriodicParameterSnapFraction);
		    int expected_turns = 0;
		    bool one_period_apart = true;
		    for (int sample = 0; one_period_apart &&
			    sample <= kPcurveLocusScreeningSegments; ++sample) {
			const double fraction = static_cast<double>(sample) /
			    kPcurveLocusScreeningSegments;
			const ON_3dPoint first_uv = first->PointAt(
			    first_trim_domain.ParameterAt(fraction));
			const ON_3dPoint second_uv = second->PointAt(
			    second_trim_domain.ParameterAt(1.0 - fraction));
			if (!first_uv.IsValid() || !second_uv.IsValid()) {
			    one_period_apart = false;
			    break;
			}
			const double delta = first_uv[direction] -
			    second_uv[direction];
			const int turns = static_cast<int>(llround(delta / period));
			if (abs(turns) != 1 ||
				fabs(delta - turns * period) > parameter_tolerance ||
				fabs(first_uv[1 - direction] -
				    second_uv[1 - direction]) > parameter_tolerance ||
				(expected_turns && turns != expected_turns)) {
			    one_period_apart = false;
			    break;
			}
			expected_turns = turns;
			const ON_3dPoint first_lift = surface->PointAt(
			    first_uv.x, first_uv.y);
			const ON_3dPoint second_lift = surface->PointAt(
			    second_uv.x, second_uv.y);
			one_period_apart = first_lift.IsValid() &&
			    second_lift.IsValid() && first_lift.DistanceTo(
				second_lift) <= LocalUnits::tolerance;
		    }
		    if (one_period_apart)
			closed_direction = direction;
		}
		if (closed_direction < 0) {
		    if (details) *details = candidate_name +
			" did not have reversed pcurves exactly one period apart";
		    continue;
		}

		ON_NurbsCurve first_curve;
		ON_NurbsCurve second_curve;
		if (!first_edge.GetNurbForm(first_curve) ||
			!second_edge.GetNurbForm(second_curve)) {
		    if (details) *details = candidate_name +
			" could not be converted to comparable NURBS curves";
		    continue;
		}
		const ON_Interval first_edge_domain = first_curve.Domain();
		const ON_Interval second_edge_domain = second_curve.Domain();
		if (!first_edge_domain.IsIncreasing() ||
			!second_edge_domain.IsIncreasing()) {
		    if (details) *details = candidate_name +
			" had an invalid edge-curve domain";
		    continue;
		}
		const double coincidence_tolerance = std::max(
		    LocalUnits::tolerance,
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale);
		bool coincident = true;
		double maximum_edge_distance = 0.0;
		for (int sample = 0; coincident &&
			sample <= kDenseValidationSegments; ++sample) {
		    const double fraction = static_cast<double>(sample) /
			kDenseValidationSegments;
		    const ON_3dPoint first_point = first_curve.PointAt(
			first_edge_domain.ParameterAt(fraction));
		    const ON_3dPoint second_point = second_curve.PointAt(
			second_edge_domain.ParameterAt(fraction));
		    const double distance = first_point.IsValid() &&
			second_point.IsValid() ? first_point.DistanceTo(second_point) :
			DBL_MAX;
		    maximum_edge_distance = std::max(maximum_edge_distance, distance);
		    coincident = distance <= coincidence_tolerance;
		}
		if (!coincident) {
		    if (details) {
			std::ostringstream message;
			message << candidate_name << " differed by "
			    << maximum_edge_distance << " mm (allowed "
			    << coincidence_tolerance << ')';
			*details = message.str();
		    }
		    continue;
		}

		std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
		const int first_trim_index = first->m_trim_index;
		const int second_trim_index = second->m_trim_index;
		if (!candidate->CombineCoincidentEdges(
			candidate->m_E[first->m_ei],
			candidate->m_E[second->m_ei])) {
		    if (details) *details = candidate_name +
			" could not be installed as a reciprocal seam edge";
		    continue;
		}
		/* CombineCoincidentEdges intentionally sets the retained tolerance to
		 * ON_UNSET_VALUE when either input tolerance is unset.  IsValidEdge
		 * rejects that sentinel, so restore the already measured conservative
		 * tolerance of the two proven-coincident source edges. */
		const int merged_edge_index =
		    candidate->m_T[first_trim_index].m_ei;
		if (merged_edge_index < 0 ||
			merged_edge_index != candidate->m_T[second_trim_index].m_ei ||
			merged_edge_index >= candidate->m_E.Count()) {
		    if (details) *details = candidate_name +
			" did not produce one reciprocal edge reference";
		    continue;
		}
		double merged_tolerance = coincidence_tolerance;
		if (first_edge.m_tolerance >= 0.0)
		    merged_tolerance = std::max(merged_tolerance,
			first_edge.m_tolerance);
		if (second_edge.m_tolerance >= 0.0)
		    merged_tolerance = std::max(merged_tolerance,
			second_edge.m_tolerance);
		candidate->m_E[merged_edge_index].m_tolerance = merged_tolerance;
		if (!candidate->Compact() ||
			!candidate->SetTrimTypeFlags(false)) {
		    if (details) *details = candidate_name +
			" could not compact the reciprocal seam edge";
		    continue;
		}
		std::string unsafe_topology;
		ON_wString validation_messages;
		ON_TextLog validation_log(validation_messages);
		const bool safe_references =
		    brep_topology_references_are_safe(candidate.get(),
			&unsafe_topology);
		const bool globally_valid = !require_global_validation ||
		    candidate->IsValid(&validation_log);
		if (!safe_references || !globally_valid) {
		    if (details) {
			ON_String validation_text(validation_messages);
			*details = candidate_name + " failed transactional validation: " +
			    (!unsafe_topology.empty() ? unsafe_topology :
				validation_text.Array());
		    }
		    continue;
		}
		*brep = *candidate;
		if (details) {
		    std::ostringstream message;
		    message << "STEP edges #" << first_step << "/#" << second_step;
		    *details = message.str();
		}
		return 1;
	    }
	}
    }
    return 0;
}


/* A closed-surface face can be structurally invalid solely because exporters
 * supplied its two seam sides as distinct, one-use EDGE_CURVEs.  The ordinary
 * periodic edge merge is transactionally global and therefore cannot accept
 * the first repair when several such pairs jointly cause invalidity.  Perform
 * the same individually proven merges on a whole-BREP candidate, then commit
 * only if the completed set is reference-safe and OpenNURBS-valid. */
size_t
merge_exact_periodic_boundary_edges_for_structural_validation(ON_Brep *brep,
	std::string *details)
{
    if (details)
	details->clear();
    if (!brep)
	return 0;
    std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
    const auto one_use_edge_count = [](const ON_Brep *model) {
	size_t count = 0;
	if (!model)
	    return count;
	for (int ei = 0; ei < model->m_E.Count(); ++ei)
	    if (model->m_E[ei].m_ti.Count() == 1)
		++count;
	return count;
    };
    size_t remaining = one_use_edge_count(candidate.get());
    const size_t maximum_attempts = remaining;
    size_t repaired = 0;
    std::string last_details;
    for (size_t attempt = 0;
	    attempt < maximum_attempts && remaining > 0; ++attempt) {
	const size_t merged = merge_one_exact_periodic_boundary_edge_pair(
	    candidate.get(), &last_details, false);
	if (!merged)
	    break;
	const size_t next_remaining = one_use_edge_count(candidate.get());
	if (next_remaining >= remaining) {
	    if (details)
		*details = "an accepted structural periodic-edge merge did not "
		    "reduce the one-use edge count";
	    return 0;
	}
	repaired += merged;
	remaining = next_remaining;
    }
    if (!repaired) {
	if (details)
	    *details = last_details;
	return 0;
    }
    std::string unsafe_topology;
    ON_wString validation_messages;
    ON_TextLog validation_log(validation_messages);
    if (!brep_topology_references_are_safe(candidate.get(),
	    &unsafe_topology) || !candidate->IsValid(&validation_log)) {
	if (details) {
	    ON_String validation_text(validation_messages);
	    *details = "completed periodic-edge merge set failed "
		"transactional validation: " +
		(!unsafe_topology.empty() ? unsafe_topology :
		    validation_text.Array());
	}
	return 0;
    }
    *brep = *candidate;
    if (details)
	*details = last_details;
    return repaired;
}


bool
regenerate_collapsed_periodic_boundary(ON_Brep *brep, ON_BrepLoop &loop,
	const ON_Surface *surface, int closed_direction, double parameter_tolerance,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type,
	double *proven_open_parameter)
{
    if (!brep || !surface || !wrapper || loop.TrimCount() != 1 ||
	    !surface->IsClosed(closed_direction))
	return false;
    ON_BrepTrim *trim = loop.Trim(0);
    ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
    if (!trim || !edge || edge->m_vi[0] != edge->m_vi[1] ||
	    trim->m_vi[0] != trim->m_vi[1] || trim->m_vi[0] < 0 ||
	    trim->m_vi[0] >= brep->m_V.Count() || !trim->TrimCurveOf())
	return false;
    const int open_direction = 1 - closed_direction;
    const ON_3dPoint supplied_start = trim->PointAtStart();
    const ON_3dPoint supplied_end = trim->PointAtEnd();
    if (!supplied_start.IsValid() || !supplied_end.IsValid())
	return false;
    /* Do not require independently fitted pcurve endpoints to have the same
     * open parameter before examining the immutable edge.  A closed STEP
     * edge may put both endpoint lifts within the declared tolerance of its
     * shared topology vertex while their parameter residual is larger than a
     * numerical UV epsilon.  The dense projection below is authoritative: it
     * must prove that the complete 3-D edge has one periodic winding and is a
     * constant-open-parameter surface curve within the bounded model-space
     * repair limit before this function returns a proof. */
    double open_parameter = 0.5 *
	(supplied_start[open_direction] + supplied_end[open_direction]);
    const ON_Interval closed_domain = surface->Domain(closed_direction);
    const auto reject = [wrapper, entity_id, &entity_type, &loop,
	    &open_parameter](const char *reason) {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": collapsed periodic boundary L" << loop.m_loop_index
		<< "@" << open_parameter << " rejected: " << reason << std::endl;
	return false;
    };
    if (!closed_domain.IsIncreasing())
	return reject("the closed surface domain was invalid");

    ON_NurbsCurve edge_nurbs;
    if (!edge->GetNurbForm(edge_nurbs))
	return reject("the STEP edge had no NURBS form");

    const double existing_tolerance = std::max(LocalUnits::tolerance,
	std::max(edge->m_tolerance,
	    std::max(trim->m_tolerance[0], trim->m_tolerance[1])));
    const ON_BoundingBox edge_bounds = edge_nurbs.BoundingBox();
    const double edge_scale = edge_bounds.IsValid() ?
	edge_bounds.Diagonal().Length() : 0.0;
    ON_BoundingBox item_bounds;
    const double item_scale = brep->GetBoundingBox(item_bounds, false) &&
	item_bounds.IsValid() ? item_bounds.Diagonal().Length() : 0.0;
    const double adjustment_limit = wrapper->ImportOptions().exact ?
	existing_tolerance : std::max(existing_tolerance,
	    std::max(edge_scale * kRegenerationMaximumRelativeMismatch,
		item_scale * kRegenerationMaximumRelativeItemMismatch));
    const ON_3dPoint &topology_vertex = brep->m_V[trim->m_vi[0]].point;

    const ON_Interval edge_domain = edge_nurbs.Domain();
    bool same_locus = edge_domain.IsIncreasing();
    double tangent_alignment = 0.0;
    bool have_tangent_alignment = false;
    brlcad::PullbackContext pullback_context;
    int failed_sample = -1;
    double failed_edge_to_surface = DBL_MAX;
    double maximum_edge_to_surface = 0.0;
    double first_closed_parameter = 0.0;
    double previous_closed_parameter = 0.0;
    double final_closed_parameter = 0.0;
    double minimum_closed_parameter = DBL_MAX;
    double maximum_closed_parameter = -DBL_MAX;
    bool have_closed_parameter = false;
    double maximum_parameter_error = parameter_tolerance;
    const ON_Interval open_domain = surface->Domain(open_direction);
    const bool open_is_closed = surface->IsClosed(open_direction) &&
	open_domain.IsIncreasing();
    const double open_period = open_is_closed ? open_domain.Length() : 0.0;
    double minimum_open_parameter = DBL_MAX;
    double maximum_open_parameter = -DBL_MAX;
    double maximum_open_parameter_error = parameter_tolerance;
    bool have_open_parameter = false;
    std::vector<ON_2dPoint> projected_parameters;
    std::vector<ON_3dPoint> edge_points;
    projected_parameters.reserve(kDenseValidationSegments + 1);
    edge_points.reserve(kDenseValidationSegments + 1);
    for (int sample = 0; same_locus && sample <= kDenseValidationSegments;
	    ++sample) {
	if (brlcad::PullbackWorkCancelled()) {
	    same_locus = false;
	    break;
	}
	const double fraction = static_cast<double>(sample) /
	    kDenseValidationSegments;
	const ON_3dPoint edge_point = edge_nurbs.PointAt(
	    edge_domain.ParameterAt(fraction));
	ON_2dPoint pulled_uv = ON_2dPoint::UnsetPoint;
	ON_3dPoint pulled_lift;
	double pulled_distance = DBL_MAX;
	const double projection_tolerance = std::max(ON_ZERO_TOLERANCE,
	    existing_tolerance);
	const double solver_tolerance = std::max(ON_ZERO_TOLERANCE,
	    projection_tolerance * 0.1);
	bool pulled = edge_point.IsValid() &&
	    pullback_context.SurfaceClosestPoint(surface, edge_point,
		pulled_uv, pulled_lift, pulled_distance, 0, solver_tolerance,
		projection_tolerance);
	/* The closest-point routine uses within_distance_tol as a search-window
	 * and completion threshold, not merely as an acceptance bound.  A broad
	 * safe-repair limit can therefore stop at the wrong parameter branch on a
	 * small-radius periodic surface.  Search at model tolerance first; only a
	 * genuinely separated source edge gets the broader bounded second pass. */
	if (!pulled && edge_point.IsValid())
	    pulled = pullback_context.SurfaceClosestPoint(surface, edge_point,
		pulled_uv, pulled_lift, pulled_distance, 0,
		solver_tolerance, adjustment_limit);
	pulled = pulled && pulled_distance <= adjustment_limit;
	if (!pulled) {
	    failed_sample = sample;
	    failed_edge_to_surface = pulled_distance;
	    same_locus = false;
	    break;
	}
	projected_parameters.push_back(pulled_uv);
	edge_points.push_back(edge_point);
	maximum_edge_to_surface = std::max(maximum_edge_to_surface,
	    pulled_distance);
	double closed_parameter = pulled_uv[closed_direction];
	const double period = closed_domain.Length();
	if (have_closed_parameter)
	    closed_parameter += round((previous_closed_parameter -
		closed_parameter) / period) * period;
	else
	    first_closed_parameter = closed_parameter;
	previous_closed_parameter = closed_parameter;
	final_closed_parameter = closed_parameter;
	minimum_closed_parameter = std::min(minimum_closed_parameter,
	    closed_parameter);
	maximum_closed_parameter = std::max(maximum_closed_parameter,
	    closed_parameter);
	have_closed_parameter = true;
	double projected_open = pulled_uv[open_direction];
	minimum_open_parameter = std::min(minimum_open_parameter,
	    projected_open);
	maximum_open_parameter = std::max(maximum_open_parameter,
	    projected_open);
	have_open_parameter = true;
	ON_3dPoint derivative_point;
	ON_3dVector du;
	ON_3dVector dv;
	const bool derivative_valid = surface->Ev1Der(pulled_uv.x, pulled_uv.y,
	    derivative_point, du, dv);
	if (derivative_valid) {
	    ON_3dVector closed_derivative = closed_direction == 0 ? du : dv;
	    ON_3dVector open_derivative = open_direction == 0 ? du : dv;
	    const double derivative_length = closed_derivative.Length();
	    if (derivative_length > ON_ZERO_TOLERANCE)
		maximum_parameter_error = std::max(maximum_parameter_error,
		    adjustment_limit / derivative_length * 1.05);
	    const double open_derivative_length = open_derivative.Length();
	    if (open_derivative_length > ON_ZERO_TOLERANCE)
		maximum_open_parameter_error = std::max(
		    maximum_open_parameter_error,
		    adjustment_limit / open_derivative_length * 1.05);
	}
	if (derivative_valid && !have_tangent_alignment && sample > 0 &&
		sample < kDenseValidationSegments) {
	    ON_3dVector surface_tangent = closed_direction == 0 ? du : dv;
	    ON_3dVector edge_tangent = edge_nurbs.TangentAt(
		edge_domain.ParameterAt(fraction));
	    if (surface_tangent.Unitize() && edge_tangent.Unitize()) {
		tangent_alignment = surface_tangent * edge_tangent;
		have_tangent_alignment = fabs(tangent_alignment) > 0.5;
	    }
	}
    }
    if (same_locus && have_open_parameter) {
	double maximum_open_deviation = 0.0;
	if (open_is_closed) {
	    double sine_sum = 0.0;
	    double cosine_sum = 0.0;
	    for (std::vector<ON_2dPoint>::const_iterator uv =
		    projected_parameters.begin(); uv != projected_parameters.end();
		    ++uv) {
		const double angle = 2.0 * ON_PI *
		    ((*uv)[open_direction] - open_domain.Min()) / open_period;
		sine_sum += sin(angle);
		cosine_sum += cos(angle);
	    }
	    double mean_angle = atan2(sine_sum, cosine_sum);
	    if (mean_angle < 0.0)
		mean_angle += 2.0 * ON_PI;
	    open_parameter = open_domain.Min() + open_period * mean_angle /
		(2.0 * ON_PI);
	    for (std::vector<ON_2dPoint>::const_iterator uv =
		    projected_parameters.begin(); uv != projected_parameters.end();
		    ++uv) {
		double deviation = fabs((*uv)[open_direction] - open_parameter);
		deviation -= floor(deviation / open_period) * open_period;
		deviation = std::min(deviation, open_period - deviation);
		maximum_open_deviation = std::max(maximum_open_deviation,
		    deviation);
	    }
	} else {
	    open_parameter = 0.5 * (minimum_open_parameter +
		maximum_open_parameter);
	    maximum_open_deviation = 0.5 *
		(maximum_open_parameter - minimum_open_parameter);
	}
	if (maximum_open_deviation > maximum_open_parameter_error) {
	    same_locus = false;
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": collapsed boundary fixed-parameter proof measured deviation "
		    << maximum_open_deviation << " with parameter tolerance "
		    << maximum_open_parameter_error << std::endl;
	}
    }
    for (size_t sample = 0; same_locus &&
	    sample < projected_parameters.size(); ++sample) {
	ON_3dPoint boundary_uv(projected_parameters[sample].x,
	    projected_parameters[sample].y, 0.0);
	boundary_uv[open_direction] = open_parameter;
	const ON_3dPoint boundary_lift = surface->PointAt(
	    boundary_uv.x, boundary_uv.y);
	const double edge_to_surface = boundary_lift.IsValid() ?
	    boundary_lift.DistanceTo(edge_points[sample]) : DBL_MAX;
	maximum_edge_to_surface = std::max(maximum_edge_to_surface,
	    edge_to_surface);
	if (!boundary_lift.IsValid() || edge_to_surface > adjustment_limit) {
	    failed_sample = static_cast<int>(sample);
	    failed_edge_to_surface = edge_to_surface;
	    same_locus = false;
	}
    }
	if (same_locus && have_closed_parameter) {
	    const double period = closed_domain.Length();
	    const double net_travel = final_closed_parameter -
		first_closed_parameter;
	    const double covered_span = maximum_closed_parameter -
		minimum_closed_parameter;
	    if (fabs(fabs(net_travel) - period) > maximum_parameter_error ||
		    fabs(covered_span - period) > maximum_parameter_error) {
		same_locus = false;
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": collapsed boundary winding proof measured net/span "
			<< net_travel << '/' << covered_span << " for period "
			<< period << " with parameter tolerance "
			<< maximum_parameter_error << std::endl;
	    }
	}
	if (!same_locus && wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": collapsed boundary dense locus failure sample "
		<< failed_sample << " edge-to-surface=" << failed_edge_to_surface
		<< " tolerance=" << LocalUnits::tolerance << std::endl;
    if (!same_locus)
	return reject("the STEP edge failed dense surface-locus or full-period winding validation");
    if (!have_tangent_alignment)
	return reject("the exact curve orientation could not be determined");
    if (projected_parameters.empty())
	return reject("the STEP edge produced no projected boundary samples");
    ON_3dPoint boundary_start(projected_parameters.front().x,
	projected_parameters.front().y, 0.0);
    ON_3dPoint boundary_end(projected_parameters.back().x,
	projected_parameters.back().y, 0.0);
    boundary_start[open_direction] = open_parameter;
    boundary_end[open_direction] = open_parameter;
    if (surface->PointAt(boundary_start.x, boundary_start.y).DistanceTo(
	    topology_vertex) > adjustment_limit ||
	    surface->PointAt(boundary_end.x, boundary_end.y).DistanceTo(
	    topology_vertex) > adjustment_limit)
	return reject("the proven periodic boundary did not lift to its closed-edge vertex");

    if (maximum_edge_to_surface > existing_tolerance) {
	const double adjusted = maximum_edge_to_surface *
	    kRegenerationToleranceSafety;
	if (wrapper->ImportOptions().exact || !(adjusted <= adjustment_limit))
	    return reject("the measured source edge/surface mismatch exceeded the bounded safe-repair limit");
	edge->m_tolerance = std::max(edge->m_tolerance, adjusted);
	trim->m_tolerance[0] = std::max(trim->m_tolerance[0], adjusted);
	trim->m_tolerance[1] = std::max(trim->m_tolerance[1], adjusted);
	wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	    entity_id, entity_type, "trim_pcurve",
	    "source edge/surface separation exceeded the declared tolerance; "
	    "used a densely measured tolerance for exact periodic-band reconstruction");
	wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
	    "adjusted one periodic-band trim tolerance to measured source geometry");
    }

    /* The supplied UV point is not necessarily the STEP edge's chosen
     * topology vertex.  Installing a full-period line here would therefore
     * attach its endpoints to the wrong 3-D point.  The caller uses this
     * proof to split the shared closed edge at the native seam and construct
     * two exact boundary uses that preserve both vertices. */
    (void)tangent_alignment;
    if (proven_open_parameter)
	*proven_open_parameter = open_parameter;
    return true;
}


/* Once dense projection has proven that a closed STEP edge winds one complete
 * surface period, it can be represented directly as a single pcurve only when
 * its topology vertex is already on the native surface seam.  Construct a
 * parameter-corresponding polyline in UV, try both windings, and validate both
 * every projection sample and every interpolated midpoint against the exact
 * directed 3-D edge.  A non-native vertex is deliberately left for the edge
 * splitter below. */
bool
regenerate_native_seam_periodic_boundary(ON_Brep *brep, ON_BrepLoop &loop,
	const ON_Surface *surface, int closed_direction, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type, double fixed_parameter,
	bool record_repair)
{
    if (!brep || !surface || !wrapper || loop.TrimCount() != 1 ||
	    !surface->IsClosed(closed_direction))
	return false;
    ON_BrepTrim *trim = loop.Trim(0);
    ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
    if (!trim || !edge || edge->m_vi[0] != edge->m_vi[1] ||
	    trim->m_vi[0] != trim->m_vi[1] ||
	    trim->m_vi[0] < 0 || trim->m_vi[0] >= brep->m_V.Count())
	return false;
    const double topology_tolerance = std::max(LocalUnits::tolerance,
	std::max(edge->m_tolerance,
	    std::max(trim->m_tolerance[0], trim->m_tolerance[1])));
    const ON_BoundingBox edge_bounds = edge->BoundingBox();
    const double edge_feature_scale = edge_bounds.IsValid() ?
	edge_bounds.Diagonal().Length() : 0.0;
    ON_BoundingBox item_bounds;
    const double item_scale = brep->GetBoundingBox(item_bounds, false) &&
	item_bounds.IsValid() ? item_bounds.Diagonal().Length() : 0.0;
    const bool allow_measured_tolerance =
	!wrapper->ImportOptions().exact &&
	wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe;
    const double adjustment_limit = allow_measured_tolerance ?
	std::max(topology_tolerance,
	    std::max(edge_feature_scale * kRegenerationMaximumRelativeMismatch,
		item_scale * kRegenerationMaximumRelativeItemMismatch)) :
	topology_tolerance;
    const int fixed_direction = 1 - closed_direction;
    const ON_Interval closed_domain = surface->Domain(closed_direction);
    const ON_Interval trim_domain = trim->Domain();
    const ON_Interval edge_domain = edge->Domain();
    const ON_3dPoint supplied_uv = trim->PointAtStart();
    if (!closed_domain.IsIncreasing() || !trim_domain.IsIncreasing() ||
	    !edge_domain.IsIncreasing() || !supplied_uv.IsValid())
	return false;
    const ON_3dPoint &vertex = brep->m_V[trim->m_vi[0]].point;
    ON_3dPoint native_min_uv = supplied_uv;
    ON_3dPoint native_max_uv = supplied_uv;
    native_min_uv[closed_direction] = closed_domain.Min();
    native_max_uv[closed_direction] = closed_domain.Max();
    native_min_uv[fixed_direction] = fixed_parameter;
    native_max_uv[fixed_direction] = fixed_parameter;
    const ON_3dPoint native_min_lift = surface->PointAt(
	native_min_uv.x, native_min_uv.y);
    const ON_3dPoint native_max_lift = surface->PointAt(
	native_max_uv.x, native_max_uv.y);
    if (!native_min_lift.IsValid() || !native_max_lift.IsValid() ||
	    native_min_lift.DistanceTo(vertex) > adjustment_limit ||
	    native_max_lift.DistanceTo(vertex) > adjustment_limit)
	return false;

    const double period = closed_domain.Length();
    brlcad::PullbackContext pullback_context;
    const bool surface_closed[2] = {
	surface->IsClosed(0), surface->IsClosed(1)
    };
    const ON_Interval surface_domains[2] = {
	surface->Domain(0), surface->Domain(1)
    };
    for (int winding = 0; winding < 2; ++winding) {
	const double desired_start = winding == 0 ? closed_domain.Min() :
	    closed_domain.Max();
	const double desired_end = winding == 0 ? closed_domain.Max() :
	    closed_domain.Min();
	ON_3dPointArray points;
	ON_SimpleArray<double> parameters;
	points.Reserve(kPeriodicBoundaryConstructionSegments + 1);
	parameters.Reserve(kPeriodicBoundaryConstructionSegments + 1);
	double previous_closed = desired_start;
	bool exact = true;
	const char *failure_reason = "none";
	int failure_sample = -1;
	double failure_distance = 0.0;
	double maximum_locus_distance = std::max(
	    native_min_lift.DistanceTo(vertex),
	    native_max_lift.DistanceTo(vertex));
	for (int sample = 0; sample <= kPeriodicBoundaryConstructionSegments;
		++sample) {
	    if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled())
		return false;
	    const double fraction = static_cast<double>(sample) /
		kPeriodicBoundaryConstructionSegments;
	    const ON_3dPoint edge_point = edge->PointAt(
		edge_domain.ParameterAt(trim->m_bRev3d ?
		    1.0 - fraction : fraction));
	    ON_2dPoint pulled_uv = ON_2dPoint::UnsetPoint;
	    ON_3dPoint pulled_lift;
	    double pulled_distance = DBL_MAX;
	    /* Keep solver convergence precision independent of the acceptance
	     * window.  A measured source edge/surface separation can make the
	     * latter several model units wide; using ten percent of that value as
	     * same_point_tol collapses many neighboring samples onto one UV point
	     * and produces an invalid polyline even though the exact closest-point
	     * sequence is well defined. */
	    const double closest_point_tolerance = std::max(ON_ZERO_TOLERANCE,
		std::min(static_cast<double>(BREP_SAME_POINT_TOLERANCE),
		    LocalUnits::tolerance * 0.1));
	    ON_2dPoint seed;
	    seed[closed_direction] = previous_closed;
	    seed[fixed_direction] = fixed_parameter;
	    /* Establish the periodic image with the historical global solve at
	     * sample zero.  Every later point belongs to the same directed edge,
	     * so refine from the preceding coherent UV first and retain the global
	     * search below as a full fallback.  This avoids rebuilding analytic
	     * revolution-surface search bounds for thousands of adjacent samples
	     * without changing either acceptance tolerance or validation. */
	    bool pulled = sample > 0 && edge_point.IsValid() &&
		seed.IsValid() &&
		pullback_context.SurfaceClosestPointFromSeed(surface, edge_point,
		    seed, pulled_uv, pulled_lift, pulled_distance,
		    adjustment_limit, surface_closed, surface_domains,
		    closest_point_tolerance) &&
		pulled_distance <= adjustment_limit;
	    if (!pulled && edge_point.IsValid()) {
		pulled_uv = ON_2dPoint::UnsetPoint;
		pulled_distance = DBL_MAX;
		pulled = pullback_context.SurfaceClosestPoint(surface, edge_point,
		    pulled_uv, pulled_lift, pulled_distance, 0,
		    closest_point_tolerance, topology_tolerance);
	    }
	    if (!pulled && allow_measured_tolerance && edge_point.IsValid())
		pulled = pullback_context.SurfaceClosestPoint(surface, edge_point,
		    pulled_uv, pulled_lift, pulled_distance, 0,
		    closest_point_tolerance, adjustment_limit);
	    if (!pulled && edge_point.IsValid() &&
		    edge_feature_scale > ON_ZERO_TOLERANCE) {
		/* A fixed micro-scale solver threshold can fail to converge on source
		 * geometry whose independently measured mismatch is much larger.  Retry
		 * at a feature-relative threshold, capped at one ten-thousandth of
		 * this edge so adjacent boundary samples cannot collapse together. */
		const double feature_solver_tolerance = std::max(
		    closest_point_tolerance,
		    std::min(adjustment_limit * 0.1,
			edge_feature_scale * kPullbackSolverFeatureFraction));
		if (feature_solver_tolerance > closest_point_tolerance)
		    pulled = pullback_context.SurfaceClosestPoint(surface, edge_point,
			pulled_uv, pulled_lift, pulled_distance, 0,
			feature_solver_tolerance, adjustment_limit);
	    }
	    if (!pulled || pulled_distance > adjustment_limit) {
		exact = false;
		failure_reason = "projecting the exact STEP edge";
		failure_sample = sample;
		failure_distance = pulled_distance;
		break;
	    }
	    double closed_parameter = pulled_uv[closed_direction];
	    closed_parameter += round((previous_closed - closed_parameter) /
		period) * period;
	    previous_closed = closed_parameter;
	    ON_3dPoint uv;
	    uv[closed_direction] = closed_parameter;
	    uv[fixed_direction] = fixed_parameter;
	    uv.z = 0.0;
	    if (sample == 0)
		uv[closed_direction] = desired_start;
	    else if (sample == kPeriodicBoundaryConstructionSegments)
		uv[closed_direction] = desired_end;
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    const double lift_distance = lift.IsValid() ?
		lift.DistanceTo(edge_point) : DBL_MAX;
	    maximum_locus_distance = std::max(maximum_locus_distance,
		lift_distance);
	    if (!lift.IsValid() || lift_distance > adjustment_limit) {
		exact = false;
		failure_reason = "validating the fixed-parameter lift";
		failure_sample = sample;
		failure_distance = lift_distance;
		break;
	    }
	    points.Append(uv);
	    parameters.Append(trim_domain.ParameterAt(fraction));
	}
	/* The caller has already densely proven that this closed STEP edge winds
	 * one complete period.  Do not repeat that proof by requiring the raw
	 * closest-point parameter at the final sample to land on the native seam:
	 * source edge/surface disagreement can legitimately put that parameter a
	 * little short of the seam even though both periodic endpoint images lift
	 * to the authoritative topology vertex within the measured local
	 * tolerance.  The endpoint replacement above and the interpolated
	 * midpoint pass below still require the complete installed pcurve to lift
	 * to the directed 3-D edge within that tolerance. */
	if (!exact) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": native periodic boundary L" << loop.m_loop_index
		    << " winding=" << winding << " rejected: "
		    << failure_reason << " sample=" << failure_sample
		    << " distance=" << failure_distance << " closed="
		    << previous_closed << " desired=" << desired_end
		    << " tolerance=" << topology_tolerance << std::endl;
	    continue;
	}
	std::unique_ptr<ON_PolylineCurve> candidate(
	    new ON_PolylineCurve(points, parameters));
	if (!candidate || !candidate->ChangeDimension(2) ||
		!candidate->IsValid()) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": native periodic boundary L" << loop.m_loop_index
		    << " winding=" << winding
		    << " rejected because its validated UV samples did not form "
		       "a valid 2-D polyline" << std::endl;
	    continue;
	}
	for (int sample = 0; exact &&
		sample < kPeriodicBoundaryConstructionSegments;
		sample++) {
	    const double fraction = (static_cast<double>(sample) + 0.5) /
		kPeriodicBoundaryConstructionSegments;
	    const ON_3dPoint uv = candidate->PointAt(
		trim_domain.ParameterAt(fraction));
	    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
	    const ON_3dPoint edge_point = edge->PointAt(
		edge_domain.ParameterAt(trim->m_bRev3d ?
		    1.0 - fraction : fraction));
	    const double lift_distance =
		lift.IsValid() && edge_point.IsValid() ?
		lift.DistanceTo(edge_point) : DBL_MAX;
	    maximum_locus_distance = std::max(maximum_locus_distance,
		lift_distance);
	    exact = lift_distance <= adjustment_limit;
	    if (!exact) {
		failure_reason = "validating an interpolated fixed-parameter lift";
		failure_sample = sample;
		failure_distance = lift.IsValid() && edge_point.IsValid() ?
		    lift.DistanceTo(edge_point) : DBL_MAX;
	    }
	}
	if (!exact) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": native periodic boundary L" << loop.m_loop_index
		    << " winding=" << winding << " rejected: " << failure_reason
		    << " sample=" << failure_sample << " distance="
		    << failure_distance << " tolerance=" << adjustment_limit
		    << std::endl;
	    continue;
	}
	if (maximum_locus_distance > topology_tolerance) {
	    const double adjusted_tolerance =
		maximum_locus_distance * kRegenerationToleranceSafety;
	    if (!allow_measured_tolerance ||
		    adjusted_tolerance > adjustment_limit)
		continue;
	    edge->m_tolerance = std::max(edge->m_tolerance,
		adjusted_tolerance);
	    trim->m_tolerance[0] = std::max(trim->m_tolerance[0],
		adjusted_tolerance);
	    trim->m_tolerance[1] = std::max(trim->m_tolerance[1],
		adjusted_tolerance);
	    brep->m_V[trim->m_vi[0]].m_tolerance = std::max(
		brep->m_V[trim->m_vi[0]].m_tolerance,
		adjusted_tolerance);
	    wrapper->RecordDiagnostic(
		brlcad::step::DiagnosticSeverity::Warning, entity_id,
		entity_type, "trim_pcurve",
		"source native periodic boundary exceeded the declared "
		"tolerance; used a densely measured local OpenNURBS tolerance");
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"adjusted one native periodic boundary tolerance to measured "
		"source geometry");
	}
	const int c2 = brep->AddTrimCurve(candidate.release());
	if (c2 < 0 || !brep->SetTrimCurve(*trim, c2)) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": native periodic boundary L" << loop.m_loop_index
		    << " winding=" << winding
		    << " could not install its validated 2-D pcurve" << std::endl;
	    return false;
	}
	brep->SetTrimIsoFlags(*trim);
	if (record_repair)
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"regenerated a native-seam closed boundary as an exact full-period surface isocurve");
	return true;
    }
    return false;
}


bool
exact_planar_split_pcurve(const ON_Surface *surface,
	const ON_Curve *edge_piece, double tolerance,
	std::unique_ptr<ON_Curve> &pcurve)
{
    pcurve.reset();
    const ON_PlaneSurface *plane_surface = ON_PlaneSurface::Cast(surface);
    if (!plane_surface || !edge_piece || !(tolerance > 0.0))
	return false;

    ON_Xform world_to_plane(ON_Xform::IdentityTransformation);
    if (!world_to_plane.ChangeBasis(ON_Plane::World_xy,
	    plane_surface->m_plane))
	return false;
    std::unique_ptr<ON_Curve> candidate(edge_piece->DuplicateCurve());
    if (!candidate || !candidate->Transform(world_to_plane))
	return false;

    ON_Xform plane_to_parameter(ON_Xform::IdentityTransformation);
    for (int direction = 0; direction < 2; ++direction) {
	const ON_Interval extents = plane_surface->Extents(direction);
	const ON_Interval domain = plane_surface->Domain(direction);
	if (!extents.IsIncreasing() || !domain.IsIncreasing())
	    return false;
	const double scale = domain.Length() / extents.Length();
	plane_to_parameter.m_xform[direction][direction] = scale;
	plane_to_parameter.m_xform[direction][3] =
	    domain.Min() - scale * extents.Min();
    }
    if (!candidate->Transform(plane_to_parameter) ||
	    !candidate->ChangeDimension(2) || !candidate->IsValid())
	return false;

    /* This affine inverse is exact for a plane, but still prove the complete
     * detached child edge before it participates in an atomic shared-edge
     * split.  The proof prevents an inconsistent STEP edge/face relation from
     * being turned into apparently valid topology merely because the adjacent
     * supplied pcurve happened to begin at the requested seam point. */
    const ON_Interval edge_domain = edge_piece->Domain();
    const ON_Interval pcurve_domain = candidate->Domain();
    if (!edge_domain.IsIncreasing() || !pcurve_domain.IsIncreasing())
	return false;
    for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
	if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled())
	    return false;
	const double fraction = static_cast<double>(sample) /
	    kDenseValidationSegments;
	const ON_3dPoint point = edge_piece->PointAt(
	    edge_domain.ParameterAt(fraction));
	const ON_3dPoint uv = candidate->PointAt(
	    pcurve_domain.ParameterAt(fraction));
	const ON_3dPoint lift = uv.IsValid() ?
	    surface->PointAt(uv.x, uv.y) : ON_3dPoint::UnsetPoint;
	if (!point.IsValid() || !lift.IsValid() ||
		point.DistanceTo(lift) > tolerance)
	    return false;
    }
    pcurve = std::move(candidate);
    return true;
}


/* A source MANIFOLD_SOLID_BREP can occasionally omit one face while retaining
 * the complete boundary which that face should close.  Do not perform general
 * hole filling: it would turn arbitrary open shells into invented solids.  A
 * missing triangular planar cap is uniquely determined, however, when all of
 * the following are true for the complete BREP:
 *
 *   - exactly three source-identified edges have one use;
 *   - those edges and three source-identified vertices form one simple cycle;
 *   - every edge endpoint agrees with its topology vertex; and
 *   - dense samples of every complete edge locus lie in the one plane through
 *     those vertices within the already established model/edge tolerances.
 *
 * The ordinary pass only advertises that inference is possible.  Mutation is
 * confined to the permissive inference transaction, and its caller still
 * requires complete structural validity and solidness before publishing the
 * result.  Reuse the authored edges directly so no replacement 3-D geometry
 * is introduced. */
bool
infer_missing_planar_triangular_cap(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type, std::string *details)
{
    if (details)
	details->clear();
    if (!brep || !wrapper || wrapper->ImportOptions().exact ||
	    wrapper->ImportOptions().strict ||
	    wrapper->ImportOptions().repair != brlcad::step::RepairMode::Safe ||
	    !(LocalUnits::tolerance > 0.0))
	return false;

    std::vector<int> boundary_edges;
    std::map<int, std::vector<int> > incident_edges;
    double topology_tolerance = std::max(LocalUnits::tolerance,
	ON_ZERO_TOLERANCE * kNumericalToleranceScale);
    for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_ti.Count() != 1)
	    continue;
	const ON_BrepTrim *trim = brep->Trim(edge.m_ti[0]);
	if (!trim || trim->m_type != ON_BrepTrim::boundary || !trim->Face() ||
		edge.m_edge_user.i <= 0 || !edge.EdgeCurveOf() ||
		edge.m_vi[0] < 0 || edge.m_vi[1] < 0 ||
		edge.m_vi[0] == edge.m_vi[1] ||
		edge.m_vi[0] >= brep->m_V.Count() ||
		edge.m_vi[1] >= brep->m_V.Count())
	    return false;
	boundary_edges.push_back(ei);
	incident_edges[edge.m_vi[0]].push_back(ei);
	incident_edges[edge.m_vi[1]].push_back(ei);
	topology_tolerance = std::max(topology_tolerance, edge.m_tolerance);
	topology_tolerance = std::max(topology_tolerance,
	    std::max(trim->m_tolerance[0], trim->m_tolerance[1]));
    }
    if (boundary_edges.size() != 3 || incident_edges.size() != 3)
	return false;
    for (std::map<int, std::vector<int> >::const_iterator vertex =
	    incident_edges.begin(); vertex != incident_edges.end(); ++vertex) {
	if (vertex->second.size() != 2 || vertex->first < 0 ||
		vertex->first >= brep->m_V.Count() ||
		brep->m_V[vertex->first].m_vertex_user.i <= 0)
	    return false;
	topology_tolerance = std::max(topology_tolerance,
	    brep->m_V[vertex->first].m_tolerance);
    }

    std::set<int> source_edges;
    std::set<int> source_vertices;
    for (std::vector<int>::const_iterator ei = boundary_edges.begin();
	    ei != boundary_edges.end(); ++ei) {
	const ON_BrepEdge &edge = brep->m_E[*ei];
	source_edges.insert(edge.m_edge_user.i);
	source_vertices.insert(brep->m_V[edge.m_vi[0]].m_vertex_user.i);
	source_vertices.insert(brep->m_V[edge.m_vi[1]].m_vertex_user.i);
    }
    if (source_edges.size() != 3 || source_vertices.size() != 3)
	return false;

    int ordered_edges[3] = {boundary_edges[0], -1, -1};
    int ordered_vertices[3] = {
	brep->m_E[ordered_edges[0]].m_vi[0],
	brep->m_E[ordered_edges[0]].m_vi[1], -1
    };
    const std::vector<int> &second_choices =
	incident_edges[ordered_vertices[1]];
    ordered_edges[1] = second_choices[0] == ordered_edges[0] ?
	second_choices[1] : second_choices[0];
    const ON_BrepEdge &second_edge = brep->m_E[ordered_edges[1]];
    ordered_vertices[2] = second_edge.m_vi[0] == ordered_vertices[1] ?
	second_edge.m_vi[1] : second_edge.m_vi[0];
    if (ordered_vertices[2] == ordered_vertices[0] ||
	    ordered_vertices[2] == ordered_vertices[1])
	return false;
    const std::vector<int> &third_choices =
	incident_edges[ordered_vertices[2]];
    ordered_edges[2] = third_choices[0] == ordered_edges[1] ?
	third_choices[1] : third_choices[0];
    const ON_BrepEdge &third_edge = brep->m_E[ordered_edges[2]];
    if (ordered_edges[2] == ordered_edges[0] ||
	    !((third_edge.m_vi[0] == ordered_vertices[2] &&
		third_edge.m_vi[1] == ordered_vertices[0]) ||
	      (third_edge.m_vi[1] == ordered_vertices[2] &&
		third_edge.m_vi[0] == ordered_vertices[0])))
	return false;

    const ON_3dPoint points[3] = {
	brep->m_V[ordered_vertices[0]].point,
	brep->m_V[ordered_vertices[1]].point,
	brep->m_V[ordered_vertices[2]].point
    };
    if (!points[0].IsValid() || !points[1].IsValid() ||
	    !points[2].IsValid())
	return false;
    const ON_3dVector first_span = points[1] - points[0];
    const ON_3dVector second_span = points[2] - points[0];
    const double maximum_span = std::max(first_span.Length(),
	std::max(second_span.Length(), points[2].DistanceTo(points[1])));
    const double twice_area = ON_CrossProduct(first_span,
	second_span).Length();
    if (!(maximum_span > topology_tolerance) ||
	    !(twice_area / maximum_span > topology_tolerance))
	return false;

    ON_Plane plane(points[0], points[1], points[2]);
    if (!plane.IsValid())
	return false;
    double maximum_planar_mismatch = 0.0;
    for (int ordered = 0; ordered < 3; ++ordered) {
	const ON_BrepEdge &edge = brep->m_E[ordered_edges[ordered]];
	const ON_Interval domain = edge.Domain();
	if (!domain.IsIncreasing())
	    return false;
	const ON_3dPoint edge_start = edge.PointAtStart();
	const ON_3dPoint edge_end = edge.PointAtEnd();
	if (!edge_start.IsValid() || !edge_end.IsValid() ||
		edge_start.DistanceTo(brep->m_V[edge.m_vi[0]].point) >
		    topology_tolerance ||
		edge_end.DistanceTo(brep->m_V[edge.m_vi[1]].point) >
		    topology_tolerance)
	    return false;
	for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
	    if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled())
		return false;
	    const ON_3dPoint point = edge.PointAt(domain.ParameterAt(
		static_cast<double>(sample) / kDenseValidationSegments));
	    if (!point.IsValid())
		return false;
	    const double mismatch = fabs(plane.DistanceTo(point));
	    maximum_planar_mismatch = std::max(maximum_planar_mismatch,
		mismatch);
	    if (mismatch > topology_tolerance)
		return false;
	}
    }

    wrapper->NoteCurveInferenceCandidate();
    if (!wrapper->CurveInferenceTrialEnabled()) {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": exact planar triangular boundary is a candidate missing "
		   "face; STEP edges #" << brep->m_E[ordered_edges[0]].m_edge_user.i
		<< ", #" << brep->m_E[ordered_edges[1]].m_edge_user.i << ", #"
		<< brep->m_E[ordered_edges[2]].m_edge_user.i << std::endl;
	return false;
    }

    std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
    double minimum_u = DBL_MAX;
    double maximum_u = -DBL_MAX;
    double minimum_v = DBL_MAX;
    double maximum_v = -DBL_MAX;
    for (int vertex = 0; vertex < 3; ++vertex) {
	double u = 0.0;
	double v = 0.0;
	if (!plane.ClosestPointTo(points[vertex], &u, &v))
	    return false;
	minimum_u = std::min(minimum_u, u);
	maximum_u = std::max(maximum_u, u);
	minimum_v = std::min(minimum_v, v);
	maximum_v = std::max(maximum_v, v);
    }
    const double padding = std::max(topology_tolerance,
	maximum_span * 1.0e-9);
    ON_PlaneSurface *surface = new ON_PlaneSurface(plane);
    surface->SetDomain(0, minimum_u - padding, maximum_u + padding);
    surface->SetDomain(1, minimum_v - padding, maximum_v + padding);
    surface->SetExtents(0, surface->Domain(0));
    surface->SetExtents(1, surface->Domain(1));
    const int surface_index = candidate->AddSurface(surface);
    if (surface_index < 0) {
	delete surface;
	return false;
    }
    ON_BrepFace &face = candidate->NewFace(surface_index);
    const int face_index = face.m_face_index;
    ON_BrepLoop &loop = candidate->NewLoop(ON_BrepLoop::outer, face);
    const int loop_index = loop.m_loop_index;
    bool installed = face_index >= 0 && loop_index >= 0;
    for (int ordered = 0; installed && ordered < 3; ++ordered) {
	ON_BrepEdge &edge = candidate->m_E[ordered_edges[ordered]];
	const int start_vertex = ordered_vertices[ordered];
	const bool reversed = edge.m_vi[1] == start_vertex;
	if ((!reversed && edge.m_vi[0] != start_vertex) ||
		(reversed && edge.m_vi[1] != start_vertex)) {
	    installed = false;
	    break;
	}
	std::unique_ptr<ON_Curve> edge_piece(edge.DuplicateCurve());
	std::unique_ptr<ON_Curve> pcurve;
	if (!edge_piece || !exact_planar_split_pcurve(surface,
		edge_piece.get(), topology_tolerance, pcurve) ||
		(reversed && !pcurve->Reverse())) {
	    installed = false;
	    break;
	}
	const int c2 = candidate->AddTrimCurve(pcurve.release());
	if (c2 < 0) {
	    installed = false;
	    break;
	}
	ON_BrepTrim &trim = candidate->NewTrim(edge, reversed,
	    candidate->m_L[loop_index], c2);
	if (trim.m_trim_index < 0) {
	    installed = false;
	    break;
	}
	trim.m_type = ON_BrepTrim::boundary;
	trim.m_tolerance[0] = topology_tolerance;
	trim.m_tolerance[1] = topology_tolerance;
	const ON_Interval trim_domain = trim.Domain();
	trim.m_iso = surface->IsIsoparametric(trim, &trim_domain);
    }
    std::string unsafe_topology;
    ON_wString validation_messages;
    ON_TextLog validation_log(validation_messages);
    if (!installed || !candidate->SetTrimTypeFlags(false) ||
	    !candidate->SetTrimIsoFlags(candidate->m_F[face_index]) ||
	    !brep_topology_references_are_safe(candidate.get(),
		&unsafe_topology) || !candidate->IsValid(&validation_log)) {
	if (details) {
	    ON_String text(validation_messages);
	    *details = !unsafe_topology.empty() ? unsafe_topology : text.Array();
	}
	return false;
    }

    std::ostringstream edge_list;
    for (int ordered = 0; ordered < 3; ++ordered) {
	if (ordered)
	    edge_list << ',';
	edge_list << brep->m_E[ordered_edges[ordered]].m_edge_user.i;
    }
    *brep = *candidate;
    for (std::set<int>::const_iterator source = source_edges.begin();
	    source != source_edges.end(); ++source) {
	std::ostringstream provenance;
	provenance << "synthesized one absent planar triangular face bounded "
	    << "by the complete one-use STEP edge cycle " << edge_list.str()
	    << "; maximum dense edge-to-plane mismatch was "
	    << maximum_planar_mismatch << " mm";
	wrapper->RecordInferredCurve(*source,
	    "missing_planar_triangular_cap", maximum_planar_mismatch, 0.0,
	    topology_tolerance, LocalUnits::tolerance, provenance.str());
    }
    wrapper->RecordRepair(entity_id, entity_type, "outer",
	"synthesized one missing planar triangular cap from the exact complete "
	"one-use boundary cycle during permissive topology inference");
    wrapper->RecordDiagnostic(brlcad::step::DiagnosticSeverity::Warning,
	entity_id, entity_type, "geometry_inference",
	"source asserted a manifold solid but omitted one face; permissive "
	"import synthesized the uniquely determined planar triangular cap");
    if (details)
	*details = "STEP edges #" +
	    std::to_string(*source_edges.begin()) + " and the other two members "
	    "of cycle " + edge_list.str();
    return true;
}


bool
regenerate_full_period_boundary_chain(ON_Brep *brep, int loop_index,
	int closed_direction, double parameter_tolerance,
	STEPWrapper *wrapper, int entity_id, const std::string &entity_type,
	bool record_repair);


bool
split_periodic_boundary_at_native_seam(ON_Brep *brep,
	int target_loop_index, int target_trim_index, const ON_Surface *surface,
	int closed_direction, double open_parameter, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type,
	bool record_repair, int *split_step_edge_id,
	bool require_valid_affected_loops, bool proof_only,
	bool isolated_candidate)
{
    if (!brep || !surface || !wrapper || target_loop_index < 0 ||
	    target_loop_index >= brep->m_L.Count() || target_trim_index < 0 ||
	    target_trim_index >= brep->m_T.Count())
	return false;
    ON_BrepTrim &target_trim = brep->m_T[target_trim_index];
    const int original_edge_index = target_trim.m_ei;
    if (original_edge_index < 0 || original_edge_index >= brep->m_E.Count())
	return false;
    ON_BrepEdge &original_edge = brep->m_E[original_edge_index];
    if (original_edge.m_vi[0] < 0 || original_edge.m_vi[0] >= brep->m_V.Count() ||
	    original_edge.m_vi[1] < 0 || original_edge.m_vi[1] >= brep->m_V.Count() ||
	    original_edge.m_ti.Count() < 1 || !original_edge.EdgeCurveOf())
	return false;
    const int original_step_edge_id = original_edge.m_edge_user.i;
    const bool original_edge_closed =
	original_edge.m_vi[0] == original_edge.m_vi[1];
    const double topology_tolerance = std::max(LocalUnits::tolerance,
	std::max(original_edge.m_tolerance,
	    std::max(target_trim.m_tolerance[0], target_trim.m_tolerance[1])));
    const int open_direction = 1 - closed_direction;
    const ON_Interval closed_domain = surface->Domain(closed_direction);
    const ON_Interval edge_domain = original_edge.Domain();
    if (!closed_domain.IsIncreasing() || !edge_domain.IsIncreasing())
	return false;
    const double period = closed_domain.Length();
    const bool arbitrary_target_boundary = !std::isfinite(open_parameter);
    brlcad::PullbackContext pullback_context;
    const char *rejection_stage = "projecting the closed edge";
    const auto reject = [wrapper, entity_id, &entity_type,
	    target_loop_index, target_trim_index, &rejection_stage]() {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": exact native-seam split L" << target_loop_index << "/T"
		<< target_trim_index << " rejected while " << rejection_stage
		<< std::endl;
	return false;
    };

    /* This routine makes an exact topological cut.  A tolerance enlarged to
     * preserve a demonstrably inconsistent source pcurve must not by itself
     * authorize a broad closest-point search around a singular surface.  If
     * the supplied pcurve does not even indicate a native-seam crossing,
     * decline that speculative edge and let another exact edge establish the
     * cut.  This is only a rejection screen: measured-repair cases whose
     * pcurves do cross a seam still receive the complete 3-D proof below. */
    const double declared_proof_tolerance = std::max(
	LocalUnits::tolerance, LocalUnits::representation_tolerance);
    if (declared_proof_tolerance > 0.0 &&
	    topology_tolerance > declared_proof_tolerance *
		kNumericalToleranceScale) {
	const ON_Curve *pcurve = target_trim.TrimCurveOf();
	const ON_Interval pcurve_domain = pcurve ? pcurve->Domain() :
	    ON_Interval::EmptyInterval;
	bool supplied_crossing = false;
	bool have_parameter = false;
	double first_closed = 0.0;
	double previous_closed = 0.0;
	double minimum_closed = DBL_MAX;
	double maximum_closed = -DBL_MAX;
	if (pcurve_domain.IsIncreasing()) {
	    for (int sample = 0; sample <= kBoundaryParameterSearchSegments;
		    ++sample) {
		const ON_3dPoint uv = pcurve->PointAt(
		    pcurve_domain.ParameterAt(static_cast<double>(sample) /
			kBoundaryParameterSearchSegments));
		if (!uv.IsValid() || !std::isfinite(uv[closed_direction])) {
		    have_parameter = false;
		    break;
		}
		double closed = uv[closed_direction];
		if (have_parameter)
		    closed += round((previous_closed - closed) / period) * period;
		else
		    first_closed = closed;
		previous_closed = closed;
		minimum_closed = std::min(minimum_closed, closed);
		maximum_closed = std::max(maximum_closed, closed);
		have_parameter = true;
	    }
	}
	const double crossing_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	    period * kPeriodicParameterSnapFraction);
	if (have_parameter) {
	    const double first_seam = closed_domain.Min() + ceil(
		(minimum_closed - closed_domain.Min()) / period) * period;
	    supplied_crossing = maximum_closed - minimum_closed >=
		period - crossing_tolerance ||
		(first_seam > minimum_closed + crossing_tolerance &&
		 first_seam < maximum_closed - crossing_tolerance) ||
		fabs(fabs(previous_closed - first_closed) - period) <=
		    crossing_tolerance;
	}
	if (!supplied_crossing) {
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": exact native-seam split L" << target_loop_index
		    << "/T" << target_trim_index << "/STEP edge "
		    << original_step_edge_id << " skipped because its adjusted "
		    << "tolerance " << topology_tolerance
		    << " exceeds the source topology-proof limit and its "
		    << "pcurve supplies no seam-crossing evidence" << std::endl;
	    return false;
	}
    }

    struct ProjectedSample {
	double edge_parameter;
	double closed_parameter;
    };
    std::vector<ProjectedSample> projected;
    projected.reserve(kBoundaryParameterSearchSegments + 1);
    double previous_closed = 0.0;
    for (int sample = 0; sample <= kBoundaryParameterSearchSegments;
	    ++sample) {
	if (brlcad::PullbackWorkCancelled())
	    return reject();
	const double fraction = static_cast<double>(sample) /
	    kBoundaryParameterSearchSegments;
	const double edge_parameter = edge_domain.ParameterAt(fraction);
	const ON_3dPoint edge_point = original_edge.PointAt(edge_parameter);
	ON_2dPoint uv = ON_2dPoint::UnsetPoint;
	ON_3dPoint lift;
	double distance = DBL_MAX;
	if (!edge_point.IsValid() ||
		!pullback_context.SurfaceClosestPoint(surface, edge_point, uv,
		    lift, distance, 0, std::max(ON_ZERO_TOLERANCE,
			topology_tolerance * 0.1), topology_tolerance) ||
		distance > topology_tolerance)
	    return reject();
	double closed = uv[closed_direction];
	if (!projected.empty())
	    closed += round((previous_closed - closed) / period) * period;
	previous_closed = closed;
	projected.push_back({edge_parameter, closed});
    }

    /* A closed topology edge is eligible for this repair only when one
     * ordered traversal of its immutable 3-D curve winds exactly once around
     * the selected surface direction.  This is independent of the supplied
     * pcurve and prevents an ordinary closed curve which merely crosses a
     * seam from acquiring full-period band topology. */
    if (original_edge_closed) {
	double minimum_closed = projected.front().closed_parameter;
	double maximum_closed = minimum_closed;
	for (std::vector<ProjectedSample>::const_iterator sample =
		projected.begin(); sample != projected.end(); ++sample) {
	    minimum_closed = std::min(minimum_closed,
		sample->closed_parameter);
	    maximum_closed = std::max(maximum_closed,
		sample->closed_parameter);
	}
	const double net_travel = projected.back().closed_parameter -
	    projected.front().closed_parameter;
	const double winding_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	    kPeriodicParameterSnapFraction * std::max(1.0, period));
	if (fabs(fabs(net_travel) - period) > winding_tolerance ||
		fabs((maximum_closed - minimum_closed) - period) >
		    winding_tolerance) {
	    rejection_stage = "proving one complete periodic winding";
	    return reject();
	}
    }

    int crossing = -1;
    double seam_parameter = 0.0;
    bool sampled_seam_crossing = false;
    double sampled_edge_parameter = 0.0;
    for (size_t sample = 1; sample < projected.size(); ++sample) {
	const double first = projected[sample - 1].closed_parameter;
	const double second = projected[sample].closed_parameter;
	const double nearest_seam = closed_domain.Min() +
	    round((second - closed_domain.Min()) / period) * period;
	const double seam_hit_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	    period * 1.0e-10);
	if (sample + 1 < projected.size() &&
		fabs(second - nearest_seam) <= seam_hit_tolerance) {
	    const double previous_side = first - nearest_seam;
	    const double next_side =
		projected[sample + 1].closed_parameter - nearest_seam;
	    /* An exact sample on a native seam is a crossing only when its
	     * adjacent samples traverse from one side to the other.  In
	     * particular, a boundary edge which runs along the seam presents
	     * hundreds of exact hits; treating its first interior sample as a
	     * crossing repeatedly subdivides the same immutable STEP edge
	     * without changing the periodic topology. */
	    const bool traverses_seam =
		(previous_side < -seam_hit_tolerance &&
		    next_side > seam_hit_tolerance) ||
		(previous_side > seam_hit_tolerance &&
		    next_side < -seam_hit_tolerance);
	    if (traverses_seam) {
		crossing = static_cast<int>(sample);
		seam_parameter = nearest_seam;
		sampled_seam_crossing = true;
		sampled_edge_parameter = projected[sample].edge_parameter;
		break;
	    }
	}
	if (fabs(second - first) <= ON_ZERO_TOLERANCE)
	    continue;
	const double minimum = std::min(first, second);
	const double maximum = std::max(first, second);
	const double first_seam = closed_domain.Min() +
	    ceil((minimum - closed_domain.Min()) / period) * period;
	if (first_seam > minimum + ON_ZERO_TOLERANCE &&
		first_seam < maximum - ON_ZERO_TOLERANCE) {
	    crossing = static_cast<int>(sample);
	    seam_parameter = first_seam;
	    break;
	}
    }
    /* A closest-point solve can land a sampled value a few ulps on the
     * neighboring periodic image.  In that case the ceil-based scan above
     * may see the seam as coincident with one end of both adjacent sample
     * intervals and decline each strict interior test.  Make one equivalent
     * sign-change pass using the seam image nearest each interval midpoint.
     * A tangential seam touch has no sign change and remains ineligible. */
    if (crossing < 1) {
	for (size_t sample = 1; sample < projected.size(); ++sample) {
	    const double first = projected[sample - 1].closed_parameter;
	    const double second = projected[sample].closed_parameter;
	    const double middle = 0.5 * (first + second);
	    const double nearest_seam = closed_domain.Min() + round(
		(middle - closed_domain.Min()) / period) * period;
	    const double first_side = first - nearest_seam;
	    const double second_side = second - nearest_seam;
	    if ((first_side < 0.0 && second_side > 0.0) ||
		    (first_side > 0.0 && second_side < 0.0)) {
		crossing = static_cast<int>(sample);
		seam_parameter = nearest_seam;
		break;
	    }
	}
    }
    /* Keep the target seam image fixed when the two directed edge endpoints
     * themselves bracket it.  A projected open edge can wander far enough
     * that selecting a new nearest seam image for every sample interval
     * masks the eventual crossing of the endpoint-proven image. */
    if (crossing < 1 && projected.size() > 1) {
	const double endpoint_middle = 0.5 *
	    (projected.front().closed_parameter +
	     projected.back().closed_parameter);
	const double endpoint_seam = closed_domain.Min() + round(
	    (endpoint_middle - closed_domain.Min()) / period) * period;
	const double endpoint_seam_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
	    period * 1.0e-10);
	int previous_sign = 0;
	size_t previous_sign_sample = 0;
	for (size_t sample = 0; sample < projected.size(); ++sample) {
	    const double side =
		projected[sample].closed_parameter - endpoint_seam;
	    const int sign = side < -endpoint_seam_tolerance ? -1 :
		(side > endpoint_seam_tolerance ? 1 : 0);
	    if (sign == 0)
		continue;
	    if (previous_sign && sign != previous_sign) {
		seam_parameter = endpoint_seam;
		if (sample == previous_sign_sample + 1) {
		    crossing = static_cast<int>(sample);
		} else {
		    size_t seam_sample = previous_sign_sample + 1;
		    double minimum_residual = DBL_MAX;
		    for (size_t zero_sample = previous_sign_sample + 1;
			    zero_sample < sample; ++zero_sample) {
			const double residual = fabs(
			    projected[zero_sample].closed_parameter -
			    endpoint_seam);
			if (residual < minimum_residual) {
			    minimum_residual = residual;
			    seam_sample = zero_sample;
			}
		    }
		    crossing = static_cast<int>(seam_sample);
		    sampled_seam_crossing = true;
		    sampled_edge_parameter =
			projected[seam_sample].edge_parameter;
		}
		break;
	    }
	    previous_sign = sign;
	    previous_sign_sample = sample;
	}
    }
    if (crossing < 1) {
	if (wrapper->Verbose() && !projected.empty()) {
	    double minimum_closed = projected.front().closed_parameter;
	    double maximum_closed = minimum_closed;
	    for (std::vector<ProjectedSample>::const_iterator sample =
		    projected.begin(); sample != projected.end(); ++sample) {
		minimum_closed = std::min(minimum_closed,
		    sample->closed_parameter);
		maximum_closed = std::max(maximum_closed,
		    sample->closed_parameter);
	    }
	    const double midpoint_seam = closed_domain.Min() + round(
		(0.5 * (projected.front().closed_parameter +
		    projected.back().closed_parameter) - closed_domain.Min()) /
		period) * period;
	    std::cerr << entity_type << " #" << entity_id
		<< ": exact native-seam split L" << target_loop_index << "/T"
		<< target_trim_index << "/STEP edge " << original_step_edge_id
		<< " found no direction-" << closed_direction
		<< " seam crossing over projected range "
		<< minimum_closed << ':' << maximum_closed << " (endpoints "
		<< projected.front().closed_parameter << ':'
		<< projected.back().closed_parameter << ", native domain "
		<< closed_domain.Min() << ':' << closed_domain.Max()
		<< ", midpoint seam/sides " << midpoint_seam << '/'
		<< projected.front().closed_parameter - midpoint_seam << ':'
		<< projected.back().closed_parameter - midpoint_seam << ')'
		<< std::endl;
	}
	rejection_stage = "locating the native periodic seam crossing";
	return reject();
    }

    rejection_stage = "refining the native periodic seam crossing";
    double parameter_a = projected[crossing - 1].edge_parameter;
    double parameter_b = projected[crossing].edge_parameter;
    double closed_a = projected[crossing - 1].closed_parameter;
    double closed_b = projected[crossing].closed_parameter;
    for (int iteration = 0; !sampled_seam_crossing && iteration < 64;
	    ++iteration) {
	const double parameter = 0.5 * (parameter_a + parameter_b);
	const ON_3dPoint edge_point = original_edge.PointAt(parameter);
	ON_2dPoint uv = ON_2dPoint::UnsetPoint;
	ON_3dPoint lift;
	double distance = DBL_MAX;
	if (!edge_point.IsValid() ||
		!pullback_context.SurfaceClosestPoint(surface, edge_point, uv,
		    lift, distance, 0, std::max(ON_ZERO_TOLERANCE,
			topology_tolerance * 0.01), topology_tolerance) ||
		distance > topology_tolerance)
	    return reject();
	double closed = uv[closed_direction];
	closed += round((0.5 * (closed_a + closed_b) - closed) / period) *
	    period;
	if ((closed_a < closed_b) == (closed < seam_parameter)) {
	    parameter_a = parameter;
	    closed_a = closed;
	} else {
	    parameter_b = parameter;
	    closed_b = closed;
	}
	if (fabs(closed - seam_parameter) <= ON_ZERO_TOLERANCE *
		kNumericalToleranceScale)
	    break;
    }
    double edge_split_parameter = sampled_seam_crossing ?
	sampled_edge_parameter : 0.5 * (parameter_a + parameter_b);
    ON_3dPoint seam_uv;
    seam_uv[closed_direction] = closed_domain.Min();
    seam_uv[open_direction] = open_parameter;
    seam_uv.z = 0.0;
    if (arbitrary_target_boundary) {
	const ON_3dPoint projected_split = original_edge.PointAt(
	    edge_split_parameter);
	ON_2dPoint projected_uv = ON_2dPoint::UnsetPoint;
	ON_3dPoint projected_lift;
	double projected_distance = DBL_MAX;
	if (!projected_split.IsValid() ||
		!pullback_context.SurfaceClosestPoint(surface, projected_split,
		    projected_uv, projected_lift, projected_distance, 0,
		    std::max(ON_ZERO_TOLERANCE,
			topology_tolerance * 0.01), topology_tolerance) ||
		projected_distance > topology_tolerance) {
	    rejection_stage = "locating the arbitrary boundary's seam height";
	    return reject();
	}
	seam_uv[open_direction] = projected_uv[open_direction];
    }
    const ON_3dPoint seam_point = surface->PointAt(seam_uv.x, seam_uv.y);
    ON_3dPoint split_point = original_edge.PointAt(edge_split_parameter);
    double seam_distance = seam_point.IsValid() && split_point.IsValid() ?
	seam_point.DistanceTo(split_point) : DBL_MAX;
    /* A periodic closest-point projection can switch branches inside the
     * bracketing interval even though every sample lies on the exact source
     * edge.  If that makes the UV bisection miss the native seam, solve the
     * already-known 3-D seam point directly on the immutable edge curve. */
    if (seam_distance > topology_tolerance && seam_point.IsValid()) {
	ON_NurbsCurve edge_nurbs;
	double closest_parameter = 0.0;
	if (original_edge.GetNurbForm(edge_nurbs) &&
		ON_NurbsCurve_GetClosestPoint(&closest_parameter, &edge_nurbs,
		    seam_point)) {
	    const ON_Interval closest_domain = edge_nurbs.Domain();
	    for (int iteration = 0; iteration < 24 &&
		    closest_domain.IsIncreasing(); ++iteration) {
		ON_3dPoint point;
		ON_3dVector first_derivative;
		ON_3dVector second_derivative;
		if (!edge_nurbs.Ev2Der(closest_parameter, point,
			first_derivative, second_derivative))
		    break;
		const ON_3dVector residual = point - seam_point;
		const double gradient = first_derivative * residual;
		const double curvature = second_derivative * residual +
		    first_derivative * first_derivative;
		if (!(fabs(curvature) > DBL_EPSILON))
		    break;
		const double next_parameter = std::max(closest_domain.Min(),
		    std::min(closest_domain.Max(), closest_parameter -
			gradient / curvature));
		if (fabs(next_parameter - closest_parameter) <=
			DBL_EPSILON * std::max(1.0, fabs(closest_parameter)))
		    break;
		closest_parameter = next_parameter;
	    }
	    const ON_3dPoint closest_point = edge_nurbs.PointAt(closest_parameter);
	    const double closest_distance = closest_point.IsValid() ?
		seam_point.DistanceTo(closest_point) : DBL_MAX;
	    if (closest_distance <= topology_tolerance) {
		edge_split_parameter = closest_parameter;
		split_point = closest_point;
		seam_distance = closest_distance;
	    }
	}
    }
    if (!seam_point.IsValid() || !split_point.IsValid() ||
	    seam_distance > topology_tolerance) {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": native seam point missed exact edge by " << seam_distance
		<< " at parameter " << edge_split_parameter << " tolerance="
		<< topology_tolerance << std::endl;
	rejection_stage = "validating the exact 3-D seam point";
	return reject();
    }

    rejection_stage = "splitting the exact 3-D boundary curve";
    std::unique_ptr<ON_Curve> source_curve(
	original_edge.EdgeCurveOf()->DuplicateCurve());
    ON_Curve *first_curve_raw = NULL;
    ON_Curve *second_curve_raw = NULL;
    if (!source_curve || !source_curve->Split(edge_split_parameter,
	    first_curve_raw, second_curve_raw))
	return reject();
    std::unique_ptr<ON_Curve> first_curve(first_curve_raw);
    std::unique_ptr<ON_Curve> second_curve(second_curve_raw);
    const int original_start_vertex_index = original_edge.m_vi[0];
    const int original_end_vertex_index = original_edge.m_vi[1];
    const ON_3dPoint &original_start_vertex =
	brep->m_V[original_start_vertex_index].point;
    const ON_3dPoint &original_end_vertex =
	brep->m_V[original_end_vertex_index].point;
    if (!first_curve || !second_curve || !first_curve->IsValid() ||
	    !second_curve->IsValid() ||
	    first_curve->PointAtStart().DistanceTo(original_start_vertex) >
		topology_tolerance ||
	    first_curve->PointAtEnd().DistanceTo(seam_point) >
		topology_tolerance ||
	    second_curve->PointAtStart().DistanceTo(seam_point) >
		topology_tolerance ||
	    second_curve->PointAtEnd().DistanceTo(original_end_vertex) >
		topology_tolerance)
	return reject();
    /* Preserve analytic child-arc witnesses before replacing proxy children
     * with independently owned NURBS below.  GetNurbForm retains the exact
     * locus, but openNURBS does not reliably rediscover a subarc through
     * IsArc(), and its generic NURBS closest-point routine can then report a
     * false miss for a differently parameterized exact surface isocurve. */
    ON_Arc first_exact_arc;
    ON_Arc second_exact_arc;
    bool first_is_exact_arc = first_curve->IsArc(NULL,
	&first_exact_arc, topology_tolerance);
    bool second_is_exact_arc = second_curve->IsArc(NULL,
	&second_exact_arc, topology_tolerance);
    const auto prove_arc_witness = [topology_tolerance](
	    const ON_Curve &curve, ON_Arc &arc) {
	const ON_Interval domain = curve.Domain();
	if (!domain.IsIncreasing())
	    return false;
	arc = ON_Arc(curve.PointAtStart(), curve.PointAt(domain.Mid()),
	    curve.PointAtEnd());
	if (!arc.IsValid())
	    return false;
	for (int sample = 0; sample <= kDenseValidationSegments; ++sample) {
	    if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled())
		return false;
	    const ON_3dPoint point = curve.PointAt(domain.ParameterAt(
		static_cast<double>(sample) / kDenseValidationSegments));
	    if (!point.IsValid() ||
		    point.DistanceTo(arc.ClosestPointTo(point)) >
			topology_tolerance)
		return false;
	}
	return true;
    };
    if (!first_is_exact_arc)
	first_is_exact_arc = prove_arc_witness(*first_curve, first_exact_arc);
    if (!second_is_exact_arc)
	second_is_exact_arc = prove_arc_witness(*second_curve, second_exact_arc);
    if (wrapper->Verbose() && original_edge_closed)
	std::cerr << entity_type << " #" << entity_id
	    << ": exact native-seam split L" << target_loop_index << "/T"
	    << target_trim_index << " analytic child witnesses first/second="
	    << first_is_exact_arc << '/' << second_is_exact_arc << std::endl;
    /* Split() on a curve proxy may return child proxies which still point at
     * the original BREP curve.  Compacting away that original would leave the
     * new edges/trims with dangling proxy targets.  Materialize every child as
     * an independently owned NURBS curve before mutating topology. */
    const auto detach_curve = [](std::unique_ptr<ON_Curve> &curve) {
	if (!curve) return false;
	std::unique_ptr<ON_NurbsCurve> detached(new ON_NurbsCurve());
	if (!curve->GetNurbForm(*detached) || !detached->IsValid())
	    return false;
	curve.reset(detached.release());
	return true;
    };
    if (!detach_curve(first_curve) || !detach_curve(second_curve))
	return reject();

    struct TrimPiece {
	std::unique_ptr<ON_Curve> curve;
	int split_edge;
	bool reversed;
    };
    struct TrimReplacement {
	int old_trim;
	int loop;
	ON_BrepTrim::TYPE type;
	std::vector<TrimPiece> pieces;
    };
    std::vector<TrimReplacement> replacements;
    std::map<int, std::vector<int> > original_loop_trims;
    std::vector<int> original_trim_indices;
    bool regenerated_topology_anchored_pcurve = false;
    /* Process the requesting use first.  If its exact child-arc witnesses
     * prove that the imported closed-edge sense is inverted, every other use
     * must apply the same inversion to retain the original opposing-use
     * relation. */
    if (original_edge.m_ti.Search(target_trim_index) >= 0)
	original_trim_indices.push_back(target_trim_index);
    for (int use = 0; use < original_edge.m_ti.Count(); ++use)
	if (original_edge.m_ti[use] != target_trim_index)
	    original_trim_indices.push_back(original_edge.m_ti[use]);
    bool corrected_edge_use_sense = false;
    for (std::vector<int>::const_iterator trim_id =
	    original_trim_indices.begin(); trim_id != original_trim_indices.end();
	    ++trim_id) {
	if (*trim_id < 0 || *trim_id >= brep->m_T.Count())
	    return reject();
	ON_BrepTrim &trim = brep->m_T[*trim_id];
	if (trim.m_li < 0 || trim.m_li >= brep->m_L.Count() ||
		!trim.TrimCurveOf())
	    return reject();
	if (original_loop_trims.find(trim.m_li) == original_loop_trims.end()) {
	    const ON_BrepLoop &use_loop = brep->m_L[trim.m_li];
	    original_loop_trims[trim.m_li] = std::vector<int>(
		use_loop.m_ti.Array(),
		use_loop.m_ti.Array() + use_loop.m_ti.Count());
	}
	TrimReplacement replacement;
	replacement.old_trim = *trim_id;
	replacement.loop = trim.m_li;
	replacement.type = trim.m_type;
	if (original_edge_closed && *trim_id == target_trim_index &&
		arbitrary_target_boundary) {
	    rejection_stage = "splitting the arbitrary full-period target pcurve";
	    const double target_parameter_tolerance = std::max(
		ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		kPeriodicParameterSnapFraction * std::max(1.0, period));
	    const ON_Curve *target_pcurve = &trim;
	    std::unique_ptr<ON_Curve> regenerated_target_pcurve;
	    const ON_3dPoint supplied_start = trim.PointAtStart();
	    ON_3dPoint aligned_supplied_end = trim.PointAtEnd();
	    if (supplied_start.IsValid() && aligned_supplied_end.IsValid())
		aligned_supplied_end[closed_direction] += round(
		    (supplied_start[closed_direction] -
		     aligned_supplied_end[closed_direction]) / period) * period;
	    /* A valid closed STEP edge may have independently fitted pcurve
	     * endpoints whose lifts both satisfy the shared vertex, but whose UV
	     * values retain a small non-integral residual.  Splitting that supplied
	     * curve preserves the residual and leaves the new Euclidean loop open.
	     * Rebuild this transaction's target pcurve from the immutable 3-D edge
	     * with topology-vertex endpoints exactly one period apart.  The generic
	     * regenerator densely validates the complete candidate against both the
	     * edge and the supplied surface locus; this curve is used only inside
	     * the splitter's otherwise transactional topology proof. */
	    if (supplied_start.IsValid() && aligned_supplied_end.IsValid() &&
		    aligned_supplied_end.DistanceTo(supplied_start) >
			target_parameter_tolerance) {
		ON_2dPoint topology_uv = ON_2dPoint::UnsetPoint;
		ON_3dPoint topology_lift;
		double topology_distance = DBL_MAX;
		if (!pullback_context.SurfaceClosestPoint(surface,
			original_start_vertex, topology_uv, topology_lift,
			topology_distance, 0,
			std::max(ON_ZERO_TOLERANCE,
			    topology_tolerance * 0.1), topology_tolerance) ||
			topology_distance > topology_tolerance)
		    return reject();
		ON_3dPoint required_start(topology_uv.x, topology_uv.y, 0.0);
		for (int direction = 0; direction < 2; ++direction) {
		    if (!surface->IsClosed(direction))
			continue;
		    const double direction_period =
			surface->Domain(direction).Length();
		    if (direction_period > ON_ZERO_TOLERANCE)
			required_start[direction] += round(
			    (supplied_start[direction] -
			     required_start[direction]) / direction_period) *
			    direction_period;
		}
		ON_3dPoint required_end = required_start;
		const double exact_edge_winding =
		    projected.back().closed_parameter -
		    projected.front().closed_parameter;
		const double directed_winding = trim.m_bRev3d ?
		    -exact_edge_winding : exact_edge_winding;
		required_end[closed_direction] +=
		    directed_winding >= 0.0 ? period : -period;
		ON_NurbsCurve complete_edge_nurbs;
		ON_Curve *generated = NULL;
		std::string generation_failure;
		if (!original_edge.GetNurbForm(complete_edge_nurbs) ||
			!regenerate_trim_polyline(brep, trim, surface,
			    complete_edge_nurbs, topology_tolerance,
			    &generation_failure, NULL, &required_start,
			    &required_end, true, wrapper, true, &generated) ||
			!generated) {
		    delete generated;
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id
			    << ": exact native-seam split L"
			    << target_loop_index << "/T" << target_trim_index
			    << " could not regenerate its topology-anchored "
			       "one-period pcurve: " << generation_failure
			    << std::endl;
		    return reject();
		}
		regenerated_target_pcurve.reset(generated);
		target_pcurve = regenerated_target_pcurve.get();
		regenerated_topology_anchored_pcurve = true;
	    }
	    const ON_Interval trim_domain = target_pcurve->Domain();
	    if (!trim_domain.IsIncreasing())
		return reject();
	    const auto lifted_trim_distance = [target_pcurve, surface,
		    &seam_point](double parameter) {
		const ON_3dPoint uv = target_pcurve->PointAt(parameter);
		const ON_3dPoint lift = uv.IsValid() ?
		    surface->PointAt(uv.x, uv.y) : ON_3dPoint::UnsetPoint;
		return lift.IsValid() ? lift.DistanceTo(seam_point) : DBL_MAX;
	    };
	    const double edge_fraction = edge_domain.NormalizedParameterAt(
		edge_split_parameter);
	    double trim_parameter = trim_domain.ParameterAt(trim.m_bRev3d ?
		1.0 - edge_fraction : edge_fraction);
	    double best_distance = lifted_trim_distance(trim_parameter);
	    int best_sample = std::max(1, std::min(
		kBoundaryParameterSearchSegments - 1,
		static_cast<int>(round(trim_domain.NormalizedParameterAt(
		    trim_parameter) * kBoundaryParameterSearchSegments))));
	    for (int sample = 1; sample < kBoundaryParameterSearchSegments;
		    ++sample) {
		const double parameter = trim_domain.ParameterAt(
		    static_cast<double>(sample) /
		    kBoundaryParameterSearchSegments);
		const double distance = lifted_trim_distance(parameter);
		if (distance < best_distance) {
		    best_distance = distance;
		    trim_parameter = parameter;
		    best_sample = sample;
		}
	    }
	    double refinement_minimum = trim_domain.ParameterAt(
		static_cast<double>(best_sample - 1) /
		kBoundaryParameterSearchSegments);
	    double refinement_maximum = trim_domain.ParameterAt(
		static_cast<double>(best_sample + 1) /
		kBoundaryParameterSearchSegments);
	    for (int iteration = 0; iteration < 64; ++iteration) {
		const double first_parameter = refinement_minimum +
		    (refinement_maximum - refinement_minimum) / 3.0;
		const double second_parameter = refinement_maximum -
		    (refinement_maximum - refinement_minimum) / 3.0;
		if (lifted_trim_distance(first_parameter) <=
			lifted_trim_distance(second_parameter))
		    refinement_maximum = second_parameter;
		else
		    refinement_minimum = first_parameter;
	    }
	    const double refined_parameter = 0.5 *
		(refinement_minimum + refinement_maximum);
	    const double refined_distance = lifted_trim_distance(
		refined_parameter);
	    if (refined_distance < best_distance) {
		trim_parameter = refined_parameter;
		best_distance = refined_distance;
	    }
	    const double domain_guard = std::max(ON_ZERO_TOLERANCE,
		trim_domain.Length() * 1.0e-10);
	    rejection_stage = "proving an interior target-pcurve seam parameter";
	    if (trim_parameter <= trim_domain.Min() + domain_guard ||
		    trim_parameter >= trim_domain.Max() - domain_guard ||
		    best_distance > topology_tolerance)
		return reject();

	    rejection_stage = "splitting the arbitrary target pcurve";
	    ON_Curve *left_raw = NULL;
	    ON_Curve *right_raw = NULL;
	    if (!target_pcurve->Split(trim_parameter, left_raw, right_raw))
		return reject();
	    std::unique_ptr<ON_Curve> left(left_raw);
	    std::unique_ptr<ON_Curve> right(right_raw);
	    if (!left || !right || !left->IsValid() || !right->IsValid())
		return reject();
	    /* Trim::Split(), like edge-curve Split(), can return curve proxies.
	     * Materialize the children before applying an integral-period
	     * translation: transforming a proxy may fail or mutate its shared
	     * proxy target.  GetNurbForm preserves the exact spline geometry used
	     * by these STEP pcurves. */
	    if (!detach_curve(left) || !detach_curve(right))
		return reject();
	    const ON_3dPoint left_start = left->PointAtStart();
	    const ON_3dPoint left_end = left->PointAtEnd();
	    const ON_3dPoint right_start = right->PointAtStart();
	    const ON_3dPoint right_end = right->PointAtEnd();
	    const ON_3dPoint left_start_lift = surface->PointAt(
		left_start.x, left_start.y);
	    const ON_3dPoint left_end_lift = surface->PointAt(
		left_end.x, left_end.y);
	    const ON_3dPoint right_start_lift = surface->PointAt(
		right_start.x, right_start.y);
	    const ON_3dPoint right_end_lift = surface->PointAt(
		right_end.x, right_end.y);
	    rejection_stage = "validating the target-pcurve split endpoints";
	    const double left_start_distance = left_start_lift.IsValid() ?
		left_start_lift.DistanceTo(original_start_vertex) : DBL_MAX;
	    const double right_end_distance = right_end_lift.IsValid() ?
		right_end_lift.DistanceTo(original_end_vertex) : DBL_MAX;
	    const double left_seam_distance = left_end_lift.IsValid() ?
		left_end_lift.DistanceTo(seam_point) : DBL_MAX;
	    const double right_seam_distance = right_start_lift.IsValid() ?
		right_start_lift.DistanceTo(seam_point) : DBL_MAX;
	    if (!left_start_lift.IsValid() || !left_end_lift.IsValid() ||
		    !right_start_lift.IsValid() || !right_end_lift.IsValid() ||
		    left_start_distance > topology_tolerance ||
		    right_end_distance > topology_tolerance ||
		    left_seam_distance > topology_tolerance ||
		    right_seam_distance > topology_tolerance) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": exact native-seam split L" << target_loop_index
			<< "/T" << target_trim_index
			<< " target endpoint distances vertex="
			<< left_start_distance << '/' << right_end_distance
			<< " seam=" << left_seam_distance << '/'
			<< right_seam_distance << " tolerance="
			<< topology_tolerance << std::endl;
		return reject();
	    }

	    const auto translate_curve = [closed_direction, period](
		    std::unique_ptr<ON_Curve> &curve, double shift) {
		if (!curve || fabs(shift) <= ON_ZERO_TOLERANCE)
		    return curve.get() != NULL;
		const double periods = shift / period;
		if (fabs(periods - round(periods)) >
			kPeriodicParameterSnapFraction)
		    return false;
		ON_Xform transform(ON_Xform::IdentityTransformation);
		transform.m_xform[closed_direction][3] = shift;
		return curve->Transform(transform) &&
		    curve->ChangeDimension(2) && curve->IsValid();
	    };
	    double join_shift = round((left_start[closed_direction] -
		right_end[closed_direction]) / period) * period;
	    rejection_stage = "joining periodic target-pcurve branches";
	    if (!translate_curve(right, join_shift))
		return reject();
	    ON_3dPoint shifted_right_start = right->PointAtStart();
	    ON_3dPoint shifted_right_end = right->PointAtEnd();
	    if (shifted_right_end.DistanceTo(left_start) >
		    std::max(ON_ZERO_TOLERANCE,
			kPeriodicParameterSnapFraction * std::max(1.0, period)))
		return reject();
	    const double boundary_travel = left_end[closed_direction] -
		shifted_right_start[closed_direction];
	    const double parameter_tolerance = std::max(
		ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		kPeriodicParameterSnapFraction * std::max(1.0, period));
	    rejection_stage = "proving one-period target-pcurve travel";
	    if (fabs(fabs(boundary_travel) - period) > parameter_tolerance)
		return reject();
	    const double desired_start = boundary_travel > 0.0 ?
		closed_domain.Min() : closed_domain.Max();
	    const double uniform_shift = desired_start -
		shifted_right_start[closed_direction];
	    rejection_stage = "placing the target pcurves in the native domain";
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": exact native-seam split L" << target_loop_index
		    << "/T" << target_trim_index << " branch placement right="
		    << shifted_right_start[closed_direction] << ':'
		    << shifted_right_end[closed_direction] << " left="
		    << left_start[closed_direction] << ':'
		    << left_end[closed_direction] << " travel=" << boundary_travel
		    << " desired-start=" << desired_start
		    << " shifts=" << join_shift << '/' << uniform_shift
		    << " period=" << period << std::endl;
	    /* The seam parameter found by projecting an approximate source pcurve
	     * can retain a small non-integral residual from the exact native
	     * boundary.  Moving both detached candidates by that common residual
	     * closes the UV rectangle, but is not accepted on faith: the dense
	     * pcurve-to-immutable-edge validation below proves every shifted sample
	     * remains within the measured topology tolerance before topology is
	     * committed.  Keep integral-only translation for joining periodic
	     * branches; this more general shift is local to the transactional
	     * candidate. */
	    const auto shift_candidate_for_validation = [closed_direction](
		    std::unique_ptr<ON_Curve> &curve, double shift) {
		if (!curve || !std::isfinite(shift))
		    return false;
		if (fabs(shift) <= ON_ZERO_TOLERANCE)
		    return true;
		ON_Xform transform(ON_Xform::IdentityTransformation);
		transform.m_xform[closed_direction][3] = shift;
		return curve->Transform(transform) &&
		    curve->ChangeDimension(2) && curve->IsValid();
	    };
	    if (!shift_candidate_for_validation(right, uniform_shift) ||
		    !shift_candidate_for_validation(left, uniform_shift))
		return reject();
	    shifted_right_start = right->PointAtStart();
	    shifted_right_end = right->PointAtEnd();
	    const ON_3dPoint shifted_left_start = left->PointAtStart();
	    const ON_3dPoint shifted_left_end = left->PointAtEnd();
	    const double desired_end = boundary_travel > 0.0 ?
		closed_domain.Max() : closed_domain.Min();
	    rejection_stage = "validating the native-domain target-pcurve chain";
	    if (fabs(shifted_right_start[closed_direction] - desired_start) >
		    parameter_tolerance ||
		fabs(shifted_left_end[closed_direction] - desired_end) >
		    parameter_tolerance ||
		shifted_right_end.DistanceTo(shifted_left_start) >
		    parameter_tolerance)
		return reject();

	    const auto validate_piece = [surface, topology_tolerance, wrapper,
		    entity_id, &entity_type, target_loop_index, target_trim_index](
		    const ON_Curve &pcurve, const ON_Curve &edge_curve,
		    const ON_Arc *preserved_arc, bool reversed,
		    const char *piece_name) {
		ON_NurbsCurve edge_nurbs;
		if (!edge_curve.GetNurbForm(edge_nurbs))
		    return false;
		ON_Arc exact_arc;
		const bool recovered_arc = edge_curve.IsArc(NULL, &exact_arc,
		    topology_tolerance);
		const ON_Arc *analytic_arc = preserved_arc ? preserved_arc :
		    (recovered_arc ? &exact_arc : NULL);
		const ON_Interval pcurve_domain = pcurve.Domain();
		const ON_Interval edge_piece_domain = edge_curve.Domain();
		if (!pcurve_domain.IsIncreasing() ||
			!edge_piece_domain.IsIncreasing())
		    return false;
		for (int sample = 0; sample <= kDenseValidationSegments;
			sample++) {
		    if ((sample & 63) == 0 && brlcad::PullbackWorkCancelled())
			return false;
		    const double fraction = static_cast<double>(sample) /
			kDenseValidationSegments;
		    const ON_3dPoint uv = pcurve.PointAt(
			pcurve_domain.ParameterAt(fraction));
		    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		    const ON_3dPoint corresponding = edge_curve.PointAt(
			edge_piece_domain.ParameterAt(reversed ?
			    1.0 - fraction : fraction));
		    double distance = lift.IsValid() && corresponding.IsValid() ?
			lift.DistanceTo(corresponding) : DBL_MAX;
		    if (distance > topology_tolerance && lift.IsValid() &&
			    analytic_arc)
			distance = std::min(distance,
			    lift.DistanceTo(analytic_arc->ClosestPointTo(lift)));
		    if (distance > topology_tolerance) {
			double parameter = 0.0;
			if (ON_NurbsCurve_GetClosestPoint(&parameter, &edge_nurbs,
				lift))
			    distance = std::min(distance,
				lift.DistanceTo(edge_nurbs.PointAt(parameter)));
		    }
		    if (distance > topology_tolerance) {
			if (wrapper->Verbose())
			    std::cerr << entity_type << " #" << entity_id
				<< ": exact native-seam split L"
				<< target_loop_index << "/T" << target_trim_index
				<< ' ' << piece_name << " pcurve locus failed at "
				<< sample << '/' << kDenseValidationSegments
				<< " uv=" << uv.x << ':' << uv.y
				<< " distance=" << distance << " tolerance="
				<< topology_tolerance << " reversed=" << reversed
				<< " preserved-arc=" << (preserved_arc != NULL)
				<< " recovered-arc=" << recovered_arc
				<< std::endl;
			return false;
		    }
		}
		return true;
	    };
	    rejection_stage = "densely validating the split target pcurves";
	    const auto validate_target_sense = [&](bool reversed) {
		const ON_Curve *left_edge = reversed ? second_curve.get() :
		    first_curve.get();
		const ON_Curve *right_edge = reversed ? first_curve.get() :
		    second_curve.get();
		const ON_Arc *left_exact_arc = reversed ?
		    (second_is_exact_arc ? &second_exact_arc : NULL) :
		    (first_is_exact_arc ? &first_exact_arc : NULL);
		const ON_Arc *right_exact_arc = reversed ?
		    (first_is_exact_arc ? &first_exact_arc : NULL) :
		    (second_is_exact_arc ? &second_exact_arc : NULL);
		return left_edge && right_edge &&
		    validate_piece(*left, *left_edge, left_exact_arc,
			reversed, "left") &&
		    validate_piece(*right, *right_edge, right_exact_arc,
			reversed, "right");
	    };
	    bool effective_reversed = trim.m_bRev3d;
	    if (!validate_target_sense(effective_reversed)) {
		effective_reversed = !effective_reversed;
		if (!validate_target_sense(effective_reversed))
		    return reject();
		corrected_edge_use_sense = true;
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": exact native-seam split L" << target_loop_index
			<< "/T" << target_trim_index
			<< " proved the opposite closed-edge use sense from "
			<< "both analytic child arcs" << std::endl;
	    }
	    if (!effective_reversed) {
		replacement.pieces.push_back({std::move(right), 1, false});
		replacement.pieces.push_back({std::move(left), 0, false});
	    } else {
		replacement.pieces.push_back({std::move(right), 0, true});
		replacement.pieces.push_back({std::move(left), 1, true});
	    }
	} else if (original_edge_closed && *trim_id == target_trim_index) {
	    rejection_stage = "constructing the target periodic pcurve split";
	    /* A closed STEP edge may retain an arbitrary curve/pcurve start which
	     * lies on the native parameter seam even though its single topology
	     * vertex is elsewhere on the same exact circle.  Splitting at that
	     * arbitrary pcurve start creates a zero-length trim piece.  Project the
	     * authoritative topology vertex and use its native-domain UV as the
	     * two-piece chain endpoint. */
	    ON_2dPoint topology_vertex_uv = ON_2dPoint::UnsetPoint;
	    ON_3dPoint topology_vertex_lift;
	    double topology_vertex_distance = DBL_MAX;
	    if (!pullback_context.SurfaceClosestPoint(surface, original_start_vertex,
		    topology_vertex_uv, topology_vertex_lift,
		    topology_vertex_distance, 0,
		    std::max(ON_ZERO_TOLERANCE, topology_tolerance * 0.1),
		    topology_tolerance) ||
		    topology_vertex_distance > topology_tolerance)
		return reject();
	    while (topology_vertex_uv[closed_direction] < closed_domain.Min())
		topology_vertex_uv[closed_direction] += period;
	    while (topology_vertex_uv[closed_direction] > closed_domain.Max())
		topology_vertex_uv[closed_direction] -= period;
	    const ON_3dPoint original_uv(topology_vertex_uv.x,
		topology_vertex_uv.y, 0.0);
	    if (!original_uv.IsValid() ||
		    fabs(original_uv[open_direction] - open_parameter) >
			topology_tolerance)
		return reject();
	    const ON_3dPoint first_midpoint = first_curve->PointAt(
		first_curve->Domain().Mid());
	    ON_2dPoint midpoint_uv = ON_2dPoint::UnsetPoint;
	    ON_3dPoint midpoint_lift;
	    double midpoint_distance = DBL_MAX;
	    if (!pullback_context.SurfaceClosestPoint(surface, first_midpoint,
		    midpoint_uv, midpoint_lift, midpoint_distance, 0,
		    std::max(ON_ZERO_TOLERANCE,
			topology_tolerance * 0.1), topology_tolerance) ||
		    midpoint_distance > topology_tolerance)
		return reject();
	    double midpoint_closed = midpoint_uv[closed_direction];
	    midpoint_closed += round((original_uv[closed_direction] -
		midpoint_closed) / period) * period;
	    const bool first_moves_increasing =
		midpoint_closed > original_uv[closed_direction];
	    if (wrapper->Verbose())
		std::cerr << entity_type << " #" << entity_id
		    << ": native-seam split L" << target_loop_index << "/T"
		    << target_trim_index << "/STEP edge "
		    << original_edge.m_edge_user.i << " topology-uv="
		    << original_uv.x << ':' << original_uv.y << " midpoint-uv="
		    << midpoint_uv.x << ':' << midpoint_uv.y << " seam-uv="
		    << seam_uv.x << ':' << seam_uv.y << " direction="
		    << closed_direction << '/' << (first_moves_increasing ? 1 : -1)
		    << std::endl;
	    ON_3dPoint minimum_uv = original_uv;
	    ON_3dPoint maximum_uv = original_uv;
	    minimum_uv[closed_direction] = closed_domain.Min();
	    maximum_uv[closed_direction] = closed_domain.Max();
	    std::unique_ptr<ON_LineCurve> first_piece;
	    std::unique_ptr<ON_LineCurve> second_piece;
	    if (first_moves_increasing) {
		first_piece.reset(new ON_LineCurve(minimum_uv, original_uv));
		second_piece.reset(new ON_LineCurve(original_uv, maximum_uv));
		replacement.pieces.push_back({
		    std::unique_ptr<ON_Curve>(first_piece.release()), 1, false});
		replacement.pieces.push_back({
		    std::unique_ptr<ON_Curve>(second_piece.release()), 0, false});
	    } else {
		first_piece.reset(new ON_LineCurve(minimum_uv, original_uv));
		second_piece.reset(new ON_LineCurve(original_uv, maximum_uv));
		replacement.pieces.push_back({
		    std::unique_ptr<ON_Curve>(first_piece.release()), 0, true});
		replacement.pieces.push_back({
		    std::unique_ptr<ON_Curve>(second_piece.release()), 1, true});
	    }
	    for (std::vector<TrimPiece>::iterator piece =
		    replacement.pieces.begin(); piece != replacement.pieces.end();
		    ++piece)
		if (!piece->curve || !piece->curve->ChangeDimension(2) ||
			!piece->curve->IsValid())
		    return reject();
	} else {
	    rejection_stage = "finding the adjacent face for the seam vertex";
	    const ON_BrepLoop &use_loop = brep->m_L[trim.m_li];
	    const bool effective_reversed = corrected_edge_use_sense ?
		!trim.m_bRev3d : trim.m_bRev3d;
	    const ON_BrepFace *use_face = use_loop.Face();
	    const ON_Surface *use_surface = use_face ? use_face->SurfaceOf() : NULL;
	    if (!use_surface)
		return reject();
	    ON_2dPoint cut_uv = ON_2dPoint::UnsetPoint;
	    ON_3dPoint cut_lift;
	    double cut_distance = DBL_MAX;
	    rejection_stage = "projecting the seam vertex onto an adjacent face";
	    if (!pullback_context.SurfaceClosestPoint(use_surface, seam_point,
		    cut_uv, cut_lift, cut_distance, 0,
		    std::max(ON_ZERO_TOLERANCE,
			topology_tolerance * 0.1), topology_tolerance) ||
		    cut_distance > topology_tolerance)
		return reject();
	    double trim_parameter = 0.0;
	    const ON_3dPoint cut_uv3(cut_uv.x, cut_uv.y, 0.0);
	    ON_NurbsCurve trim_nurbs;
	    rejection_stage = "locating the seam vertex on an adjacent pcurve";
	    if (!trim.GetNurbForm(trim_nurbs) ||
		    !ON_NurbsCurve_GetClosestPoint(&trim_parameter, &trim_nurbs,
		cut_uv3))
		return reject();
	    const ON_Interval trim_domain = trim.Domain();
	    const auto lifted_trim_distance = [&trim, use_surface,
		    &seam_point](double parameter) {
		const ON_3dPoint uv = trim.PointAt(parameter);
		const ON_3dPoint lift = uv.IsValid() ?
		    use_surface->PointAt(uv.x, uv.y) : ON_3dPoint::UnsetPoint;
		return lift.IsValid() ? lift.DistanceTo(seam_point) : DBL_MAX;
	    };
	    /* ON_NurbsCurve_GetClosestPoint supplies a good p-space seed, but its
	     * stopping tolerance can be much looser than a STEP model's asserted
	     * edge tolerance.  Refine that seed against the actual 3-D surface
	     * lift.  Also test the directed edge/trim parameter correspondence;
	     * exporters often retain it exactly even when the two curve domains
	     * differ.  The bounded one-dimensional search changes no geometry and
	     * the final shared-vertex lift remains the acceptance proof. */
	    const double edge_fraction = edge_domain.NormalizedParameterAt(
		edge_split_parameter);
	    const double directed_parameter = trim_domain.ParameterAt(
		effective_reversed ? 1.0 - edge_fraction : edge_fraction);
	    if (lifted_trim_distance(directed_parameter) <
		    lifted_trim_distance(trim_parameter))
		trim_parameter = directed_parameter;
	    /* UV closest-point distance is not a reliable way to select the
	     * parameter of a shared edge use on a periodic surface.  The same
	     * exact 3-D seam point can have distant lift-equivalent UV images, and
	     * a local refinement around the nearest image can split the pcurve at
	     * an unrelated point.  Screen the complete supplied pcurve by its 3-D
	     * surface lift first.  This is bounded, changes no geometry, and the
	     * split point still has to pass the exact seam-point and directed-child
	     * endpoint proofs below. */
	    int best_sample = std::max(1, std::min(
		kBoundaryParameterSearchSegments - 1,
		static_cast<int>(round(trim_domain.NormalizedParameterAt(
		    trim_parameter) * kBoundaryParameterSearchSegments))));
	    double best_distance = lifted_trim_distance(trim_parameter);
	    for (int sample = 1; sample < kBoundaryParameterSearchSegments;
		    ++sample) {
		if ((sample & 31) == 0 && brlcad::PullbackWorkCancelled())
		    return reject();
		const double parameter = trim_domain.ParameterAt(
		    static_cast<double>(sample) /
		    kBoundaryParameterSearchSegments);
		const double distance = lifted_trim_distance(parameter);
		if (distance < best_distance) {
		    best_distance = distance;
		    trim_parameter = parameter;
		    best_sample = sample;
		}
	    }
	    double refinement_minimum = trim_domain.ParameterAt(
		static_cast<double>(best_sample - 1) /
		kBoundaryParameterSearchSegments);
	    double refinement_maximum = trim_domain.ParameterAt(
		static_cast<double>(best_sample + 1) /
		kBoundaryParameterSearchSegments);
	    for (int iteration = 0; iteration < 64; ++iteration) {
		const double first_parameter = refinement_minimum +
		    (refinement_maximum - refinement_minimum) / 3.0;
		const double second_parameter = refinement_maximum -
		    (refinement_maximum - refinement_minimum) / 3.0;
		if (lifted_trim_distance(first_parameter) <=
			lifted_trim_distance(second_parameter))
		    refinement_maximum = second_parameter;
		else
		    refinement_minimum = first_parameter;
	    }
	    const double refined_parameter = 0.5 *
		(refinement_minimum + refinement_maximum);
	    const double refined_distance = lifted_trim_distance(
		refined_parameter);
	    if (refined_distance < best_distance) {
		trim_parameter = refined_parameter;
		best_distance = refined_distance;
	    }
	    const double domain_guard = trim_domain.Length() * 1.0e-10;
	    rejection_stage = "proving an interior adjacent-pcurve split parameter";
	    if (trim_parameter <= trim_domain.Min() + domain_guard ||
		    trim_parameter >= trim_domain.Max() - domain_guard ||
		    best_distance > topology_tolerance) {
		/* The neighboring face can carry the same endpoint discrepancy as
		 * the target: its supplied singleton pcurve does not contain the
		 * newly proven seam point within tolerance even though the immutable
		 * shared edge does.  Rebuild a topology-anchored complete adjacent
		 * pcurve and split it at the directed edge parameter.  Equal required
		 * endpoints represent an ordinary contractible neighbor; an integral
		 * supplied winding is preserved on a periodic neighbor.  Dense edge
		 * and source-locus validation in regenerate_trim_polyline() remains
		 * the acceptance proof. */
		bool rebuilt_singleton = false;
		if (original_edge_closed && use_loop.TrimCount() == 1) {
		    ON_2dPoint topology_uv = ON_2dPoint::UnsetPoint;
		    ON_3dPoint topology_lift;
		    double topology_distance = DBL_MAX;
		    if (pullback_context.SurfaceClosestPoint(use_surface,
			    original_start_vertex, topology_uv, topology_lift,
			    topology_distance, 0,
			    std::max(ON_ZERO_TOLERANCE,
				topology_tolerance * 0.1),
			    topology_tolerance) &&
			    topology_distance <= topology_tolerance) {
			ON_3dPoint required_start(topology_uv.x, topology_uv.y, 0.0);
			ON_3dPoint required_end = required_start;
			const ON_3dPoint supplied_start = trim.PointAtStart();
			const ON_3dPoint supplied_end = trim.PointAtEnd();
			for (int direction = 0; direction < 2; ++direction) {
			    if (!use_surface->IsClosed(direction))
				continue;
			    const double direction_period =
				use_surface->Domain(direction).Length();
			    if (!(direction_period > ON_ZERO_TOLERANCE))
				continue;
			    required_start[direction] += round(
				(supplied_start[direction] -
				 required_start[direction]) / direction_period) *
				direction_period;
			    required_end[direction] = required_start[direction] +
				round((supplied_end[direction] -
				       supplied_start[direction]) / direction_period) *
				direction_period;
			}
			ON_NurbsCurve complete_edge_nurbs;
			ON_Curve *generated = NULL;
			std::string generation_failure;
			if (original_edge.GetNurbForm(complete_edge_nurbs) &&
				regenerate_trim_polyline(brep, trim, use_surface,
				    complete_edge_nurbs, topology_tolerance,
				    &generation_failure, NULL, &required_start,
				    &required_end, true, wrapper, true, &generated) &&
				generated) {
			    std::unique_ptr<ON_Curve> complete(generated);
			    ON_Curve *left_raw = NULL;
			    ON_Curve *right_raw = NULL;
			    if (directed_parameter > complete->Domain().Min() +
				    domain_guard &&
				directed_parameter < complete->Domain().Max() -
				    domain_guard &&
				complete->Split(directed_parameter, left_raw,
				    right_raw)) {
				std::unique_ptr<ON_Curve> left(left_raw);
				std::unique_ptr<ON_Curve> right(right_raw);
				const ON_3dPoint left_end = left ?
				    left->PointAtEnd() : ON_3dPoint::UnsetPoint;
				const ON_3dPoint right_start = right ?
				    right->PointAtStart() : ON_3dPoint::UnsetPoint;
				const ON_3dPoint left_lift = left_end.IsValid() ?
				    use_surface->PointAt(left_end.x, left_end.y) :
				    ON_3dPoint::UnsetPoint;
				const ON_3dPoint right_lift = right_start.IsValid() ?
				    use_surface->PointAt(right_start.x,
					right_start.y) : ON_3dPoint::UnsetPoint;
				if (left && right && left->IsValid() &&
					right->IsValid() && left_lift.IsValid() &&
					right_lift.IsValid() &&
					left_lift.DistanceTo(seam_point) <=
					    topology_tolerance &&
					right_lift.DistanceTo(seam_point) <=
					    topology_tolerance &&
					detach_curve(left) && detach_curve(right)) {
				    if (!effective_reversed) {
					replacement.pieces.push_back({
					    std::move(left), 0, false});
					replacement.pieces.push_back({
					    std::move(right), 1, false});
				    } else {
					replacement.pieces.push_back({
					    std::move(left), 1, true});
					replacement.pieces.push_back({
					    std::move(right), 0, true});
				    }
				    rebuilt_singleton = true;
				    regenerated_topology_anchored_pcurve = true;
				}
			    }
			} else {
			    delete generated;
			    if (wrapper->Verbose() && !generation_failure.empty())
				std::cerr << entity_type << " #" << entity_id
				    << ": adjacent topology-anchored pcurve T"
				    << trim.m_trim_index << " rejected: "
				    << generation_failure << std::endl;
			}
		    }
		}
		/* A singleton adjacent use can have the same arbitrary seam-based
		 * pcurve start as the target use.  Its seam point is consequently an
		 * endpoint in parameter space even though the authoritative topology
		 * vertex is interior to the closed 3-D edge.  Rebuild the use as two
		 * native-domain isocurves from that projected topology vertex. */
		ON_2dPoint vertex_uv = ON_2dPoint::UnsetPoint;
		ON_3dPoint vertex_lift;
		double vertex_distance = DBL_MAX;
		if (!rebuilt_singleton && original_edge_closed &&
			use_loop.TrimCount() == 1 &&
			pullback_context.SurfaceClosestPoint(use_surface,
			    original_start_vertex, vertex_uv, vertex_lift,
			    vertex_distance, 0,
			    std::max(ON_ZERO_TOLERANCE,
				topology_tolerance * 0.1),
			    topology_tolerance) &&
			vertex_distance <= topology_tolerance) {
		    for (int adjacent_closed = 0; adjacent_closed < 2 &&
			    !rebuilt_singleton; ++adjacent_closed) {
			if (!use_surface->IsClosed(adjacent_closed))
			    continue;
			const int adjacent_open = 1 - adjacent_closed;
			const ON_Interval adjacent_domain = use_surface->Domain(
			    adjacent_closed);
			const double adjacent_period = adjacent_domain.Length();
			const double parameter_guard = std::max(
			    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
			    adjacent_period * 1.0e-10);
			if (!adjacent_domain.IsIncreasing() ||
				!(adjacent_period > ON_ZERO_TOLERANCE) ||
				(fabs(cut_uv[adjacent_closed] -
				    adjacent_domain.Min()) > parameter_guard &&
				 fabs(cut_uv[adjacent_closed] -
				    adjacent_domain.Max()) > parameter_guard) ||
				fabs(vertex_uv[adjacent_open] -
				    cut_uv[adjacent_open]) > topology_tolerance)
			    continue;
			while (vertex_uv[adjacent_closed] < adjacent_domain.Min())
			    vertex_uv[adjacent_closed] += adjacent_period;
			while (vertex_uv[adjacent_closed] > adjacent_domain.Max())
			    vertex_uv[adjacent_closed] -= adjacent_period;
			const ON_3dPoint trim_start = trim.PointAtStart();
			const ON_3dPoint trim_midpoint = trim.PointAt(
			    trim_domain.Mid());
			double midpoint_parameter = trim_midpoint[adjacent_closed];
			midpoint_parameter += round((trim_start[adjacent_closed] -
			    midpoint_parameter) / adjacent_period) * adjacent_period;
			const bool increasing = midpoint_parameter >
			    trim_start[adjacent_closed];
			ON_3dPoint vertex_point(vertex_uv.x, vertex_uv.y, 0.0);
			ON_3dPoint minimum_point(vertex_point);
			ON_3dPoint maximum_point(vertex_point);
			minimum_point[adjacent_closed] = adjacent_domain.Min();
			maximum_point[adjacent_closed] = adjacent_domain.Max();
			std::unique_ptr<ON_Curve> minimum_to_vertex(
			    new ON_LineCurve(minimum_point, vertex_point));
			std::unique_ptr<ON_Curve> vertex_to_maximum(
			    new ON_LineCurve(vertex_point, maximum_point));
			if (!minimum_to_vertex || !vertex_to_maximum ||
				!minimum_to_vertex->ChangeDimension(2) ||
				!vertex_to_maximum->ChangeDimension(2) ||
				!minimum_to_vertex->IsValid() ||
				!vertex_to_maximum->IsValid() ||
				!detach_curve(minimum_to_vertex) ||
				!detach_curve(vertex_to_maximum))
			    continue;
			const bool edge_first_moves_increasing = effective_reversed ?
			    !increasing : increasing;
			if (edge_first_moves_increasing) {
			    replacement.pieces.push_back({
				std::move(minimum_to_vertex), 1, false});
			    replacement.pieces.push_back({
				std::move(vertex_to_maximum), 0, false});
			} else {
			    replacement.pieces.push_back({
				std::move(minimum_to_vertex), 0, true});
			    replacement.pieces.push_back({
				std::move(vertex_to_maximum), 1, true});
			}
			rebuilt_singleton = true;
		    }
		}
		/* A non-periodic planar neighbor can encode the same closed STEP
		 * edge with a pcurve whose arbitrary start is the target surface's
		 * seam point.  There is no planar "native seam" to move, so splitting
		 * that supplied pcurve at its endpoint would create a zero-length
		 * child.  Pull both already-detached 3-D child curves through the
		 * plane's exact affine inverse instead.  This preserves the original
		 * topology vertex, honors the source edge sense, and is accepted only
		 * after dense child-edge lift validation. */
		if (!rebuilt_singleton && original_edge_closed &&
			use_loop.TrimCount() == 1 &&
			ON_PlaneSurface::Cast(use_surface)) {
		    std::unique_ptr<ON_Curve> first_pcurve;
		    std::unique_ptr<ON_Curve> second_pcurve;
		    if (exact_planar_split_pcurve(use_surface, first_curve.get(),
			    topology_tolerance, first_pcurve) &&
			    exact_planar_split_pcurve(use_surface, second_curve.get(),
				topology_tolerance, second_pcurve)) {
			if (!effective_reversed) {
			    replacement.pieces.push_back({
				std::move(first_pcurve), 0, false});
			    replacement.pieces.push_back({
				std::move(second_pcurve), 1, false});
			    rebuilt_singleton = true;
			} else if (first_pcurve->Reverse() &&
				second_pcurve->Reverse()) {
			    replacement.pieces.push_back({
				std::move(second_pcurve), 1, true});
			    replacement.pieces.push_back({
				std::move(first_pcurve), 0, true});
			    rebuilt_singleton = true;
			}
		    }
		}
		if (!rebuilt_singleton)
		    return reject();
		replacements.push_back(std::move(replacement));
		continue;
	    }
	    ON_Curve *left_raw = NULL;
	    ON_Curve *right_raw = NULL;
	    rejection_stage = "splitting an adjacent pcurve";
	    if (!trim.Split(trim_parameter, left_raw, right_raw))
		return reject();
	    std::unique_ptr<ON_Curve> left(left_raw);
	    std::unique_ptr<ON_Curve> right(right_raw);
	    if (!left || !right || !left->IsValid() || !right->IsValid())
		return reject();
	    const ON_3dPoint left_end = left->PointAtEnd();
	    const ON_3dPoint right_start = right->PointAtStart();
	    const ON_3dPoint left_lift = use_surface->PointAt(
		left_end.x, left_end.y);
	    const ON_3dPoint right_lift = use_surface->PointAt(
		right_start.x, right_start.y);
	    rejection_stage = "validating the adjacent pcurve split lift";
	    if (!left_lift.IsValid() || !right_lift.IsValid() ||
		    left_lift.DistanceTo(seam_point) > topology_tolerance ||
		    right_lift.DistanceTo(seam_point) > topology_tolerance) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": adjacent split T" << trim.m_trim_index
			<< " parameter=" << trim_parameter
			<< " trim-domain=" << trim_domain.Min() << ':'
			<< trim_domain.Max() << " nurbs-domain="
			<< trim_nurbs.Domain().Min() << ':'
			<< trim_nurbs.Domain().Max() << " uv=" << cut_uv.x << ':'
			<< cut_uv.y << " split-uv=" << left_end.x << ':'
			<< left_end.y << " lift-distance="
			<< left_lift.DistanceTo(seam_point) << '/'
			<< right_lift.DistanceTo(seam_point) << " tolerance="
			<< topology_tolerance << std::endl;
		return reject();
	    }
	    rejection_stage = "materializing adjacent split pcurves";
	    if (!detach_curve(left) || !detach_curve(right))
		return reject();
	    if (!effective_reversed) {
		replacement.pieces.push_back({std::move(left), 0, false});
		replacement.pieces.push_back({std::move(right), 1, false});
	    } else {
		replacement.pieces.push_back({std::move(left), 1, true});
		replacement.pieces.push_back({std::move(right), 0, true});
	    }
	}
	replacements.push_back(std::move(replacement));
    }

    /* Prove every directed child use before changing the BREP.  Callers may
     * retain loop/trim references across this helper, so rolling back by
     * assigning a saved ON_Brep after mutation would restore the data but
     * invalidate those live references.  More importantly, changing one
     * child's m_bRev3d to satisfy a local endpoint test can destroy the
     * opposite-use invariant of the completed closed shell. */
    rejection_stage = "proving directed split child endpoints";
    for (std::vector<TrimReplacement>::const_iterator replacement =
	    replacements.begin(); replacement != replacements.end(); ++replacement) {
	const ON_Surface *use_surface =
	    replacement->loop >= 0 && replacement->loop < brep->m_L.Count() &&
	    brep->m_L[replacement->loop].Face() ?
	    brep->m_L[replacement->loop].Face()->SurfaceOf() : NULL;
	if (!use_surface)
	    return reject();
	for (std::vector<TrimPiece>::const_iterator piece =
		replacement->pieces.begin(); piece != replacement->pieces.end();
		++piece) {
	    if (!piece->curve)
		return reject();
	    const ON_3dPoint start_uv = piece->curve->PointAtStart();
	    const ON_3dPoint end_uv = piece->curve->PointAtEnd();
	    const ON_3dPoint start_lift = use_surface->PointAt(start_uv.x,
		start_uv.y);
	    const ON_3dPoint end_lift = use_surface->PointAt(end_uv.x,
		end_uv.y);
	    const ON_3dPoint &edge_start = piece->split_edge == 0 ?
		original_start_vertex : seam_point;
	    const ON_3dPoint &edge_end = piece->split_edge == 0 ?
		seam_point : original_end_vertex;
	    if (!start_lift.IsValid() || !end_lift.IsValid())
		return reject();
	    const double forward_error = std::max(
		start_lift.DistanceTo(edge_start), end_lift.DistanceTo(edge_end));
	    const double reverse_error = std::max(
		start_lift.DistanceTo(edge_end), end_lift.DistanceTo(edge_start));
	    const double directed_error = piece->reversed ?
		reverse_error : forward_error;
	    /* Preserve the STEP edge-use sense whenever its directed child
	     * endpoints satisfy tolerance.  On a short edge, or when the new seam
	     * vertex lies close to the closed edge's original vertex, both endpoint
	     * permutations can satisfy tolerance and a sub-tolerance numerical
	     * difference is not evidence that the authored sense is wrong. */
	    if (directed_error > topology_tolerance) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": exact native-seam split L" << replacement->loop
			<< "/T" << replacement->old_trim
			<< " rejected because the source-directed child endpoints "
			<< "exceeded tolerance for sense " << piece->reversed
			<< " (errors " << forward_error << '/' << reverse_error
			<< ')' << std::endl;
		return reject();
	    }
	}
    }

    /* Everything above this point is a non-mutating proof: the immutable
     * source edge has one exact native-seam crossing, its detached child
     * curves and every directed replacement pcurve have passed dense locus
     * and endpoint validation, and no BREP topology has changed.  Large
     * supplied-boundary searches use this mode to reject ineligible STEP
     * edges before allocating a complete speculative BREP.  A positive proof
     * is deliberately repeated on the isolated candidate before mutation;
     * that transaction retains all rollback and final topology validation
     * below. */
    if (proof_only) {
	if (split_step_edge_id)
	    *split_step_edge_id = original_step_edge_id;
	return true;
    }

    rejection_stage = "installing the split boundary topology";
    /* Direct callers mutate their live BREP and therefore retain the complete
     * rollback used by every late topology check below.  The exact-edge
     * fallback has already copied the complete model into a private candidate;
     * copying that candidate again is redundant because its caller discards
     * the entire object on failure and commits it only on success. */
    std::unique_ptr<ON_Brep> rollback;
    if (!isolated_candidate)
	rollback.reset(new ON_Brep(*brep));
    const auto rollback_failure = [brep, &rollback]() {
	if (rollback)
	    *brep = *rollback;
	return false;
    };
    const int first_c3 = brep->AddEdgeCurve(first_curve.release());
    const int second_c3 = brep->AddEdgeCurve(second_curve.release());
    if (first_c3 < 0 || second_c3 < 0)
	return rollback_failure();
    ON_BrepVertex &seam_vertex = brep->NewVertex(seam_point,
	topology_tolerance);
    const int seam_vertex_index = seam_vertex.m_vertex_index;
    const int first_edge_index = brep->NewEdge(
	brep->m_V[original_start_vertex_index], brep->m_V[seam_vertex_index],
	first_c3, NULL, topology_tolerance).m_edge_index;
    const int second_edge_index = brep->NewEdge(
	brep->m_V[seam_vertex_index], brep->m_V[original_end_vertex_index],
	second_c3, NULL, topology_tolerance).m_edge_index;
    brep->m_E[first_edge_index].m_edge_user =
	brep->m_E[original_edge_index].m_edge_user;
    brep->m_E[second_edge_index].m_edge_user =
	brep->m_E[original_edge_index].m_edge_user;
    brep->m_E[first_edge_index].m_tolerance = topology_tolerance;
    brep->m_E[second_edge_index].m_tolerance = topology_tolerance;

    std::map<int, std::vector<int> > replacement_trim_indices;
    for (std::vector<TrimReplacement>::iterator replacement =
	    replacements.begin(); replacement != replacements.end(); ++replacement) {
	for (std::vector<TrimPiece>::iterator piece = replacement->pieces.begin();
		piece != replacement->pieces.end(); ++piece) {
	    const int c2 = brep->AddTrimCurve(piece->curve.release());
	    const int edge_index = piece->split_edge == 0 ?
		first_edge_index : second_edge_index;
	    if (c2 < 0 || edge_index < 0 || edge_index >= brep->m_E.Count())
		return rollback_failure();
	    const int new_trim_index = brep->NewTrim(brep->m_E[edge_index],
		piece->reversed, brep->m_L[replacement->loop], c2).m_trim_index;
	    if (new_trim_index < 0)
		return rollback_failure();
	    ON_BrepTrim &new_trim = brep->m_T[new_trim_index];
	    new_trim.m_type = replacement->type;
	    new_trim.m_tolerance[0] = topology_tolerance;
	    new_trim.m_tolerance[1] = topology_tolerance;
	    ON_Interval new_domain = new_trim.Domain();
	    const ON_Surface *use_surface = brep->m_L[replacement->loop].Face()->
		SurfaceOf();
	    new_trim.m_iso = use_surface ? use_surface->IsIsoparametric(
		*new_trim.TrimCurveOf(), &new_domain) : ON_Surface::not_iso;
	    /* NewTrim derives both fields from piece->reversed.  Assign them as one
	     * topology operation as a guard against future constructor changes;
	     * changing only m_bRev3d leaves live directed vertices inconsistent. */
	    const ON_BrepEdge *replacement_edge = new_trim.Edge();
	    if (replacement_edge &&
		    replacement_edge->m_vi[0] >= 0 &&
		    replacement_edge->m_vi[0] < brep->m_V.Count() &&
		    replacement_edge->m_vi[1] >= 0 &&
		    replacement_edge->m_vi[1] < brep->m_V.Count()) {
		new_trim.m_bRev3d = piece->reversed;
		new_trim.m_vi[0] = replacement_edge->m_vi[
		    new_trim.m_bRev3d ? 1 : 0];
		new_trim.m_vi[1] = replacement_edge->m_vi[
		    new_trim.m_bRev3d ? 0 : 1];
	    }
	    replacement_trim_indices[replacement->old_trim].push_back(
		new_trim_index);
	}
    }

    for (std::map<int, std::vector<int> >::const_iterator loop_trims =
	    original_loop_trims.begin(); loop_trims != original_loop_trims.end();
	    ++loop_trims) {
	ON_BrepLoop &use_loop = brep->m_L[loop_trims->first];
	use_loop.m_ti.SetCount(0);
	for (std::vector<int>::const_iterator old_trim =
		loop_trims->second.begin(); old_trim != loop_trims->second.end();
		++old_trim) {
	    std::map<int, std::vector<int> >::const_iterator replacement =
		replacement_trim_indices.find(*old_trim);
	    if (replacement == replacement_trim_indices.end()) {
		use_loop.m_ti.Append(*old_trim);
		brep->m_T[*old_trim].m_li = loop_trims->first;
		continue;
	    }
	    for (std::vector<int>::const_iterator new_trim =
		    replacement->second.begin(); new_trim != replacement->second.end();
		    ++new_trim) {
		use_loop.m_ti.Append(*new_trim);
		brep->m_T[*new_trim].m_li = loop_trims->first;
	    }
	}
    }
    for (std::vector<int>::const_iterator old_trim =
	    original_trim_indices.begin(); old_trim != original_trim_indices.end();
	    ++old_trim)
	brep->DeleteTrim(brep->m_T[*old_trim], false);
    brep->DeleteEdge(brep->m_E[original_edge_index], false);
    if (!brep->Compact())
	return rollback_failure();
    /* Each replacement trim initially inherits the source trim type while
     * the original closed edge and its complete use graph still exist.  Once
     * the edge is split and the BREP is compacted, that inherited value can
     * be stale (for example, a former seam use can now be a mated subedge on
     * two faces).  Let OpenNURBS derive the authoritative type from the final
     * reciprocal edge/trim references before structural validation. */
    if (!brep->SetTrimTypeFlags(false))
	return rollback_failure();
    std::string unsafe_topology;
    if (!brep_topology_references_are_safe(brep, &unsafe_topology))
	return rollback_failure();
    /* Splitting one shared STEP edge replaces every adjacent trim use, not
     * just the boundary which requested the native-seam cut.  Reciprocal
     * index checks cannot detect a pcurve piece attached to the wrong end of
     * its new directed subedge.  Validate every replacement carrying the
     * source STEP edge identity before committing the transaction. */
    ON_wString split_messages;
    ON_TextLog split_log(split_messages);
    bool valid_split_uses = true;
    for (int ei = 0; valid_split_uses && ei < brep->m_E.Count(); ++ei) {
	const ON_BrepEdge &edge = brep->m_E[ei];
	if (edge.m_edge_user.i != original_step_edge_id)
	    continue;
	for (int eti = 0; valid_split_uses && eti < edge.m_ti.Count(); ++eti) {
	    const ON_BrepTrim *trim = brep->Trim(edge.m_ti[eti]);
	    valid_split_uses = trim && trim->IsValid(&split_log);
	}
    }
    if (!valid_split_uses) {
	if (wrapper->Verbose()) {
	    ON_String text(split_messages);
	    std::cerr << entity_type << " #" << entity_id
		<< ": exact native-seam split L" << target_loop_index
		<< "/T" << target_trim_index
		<< " rejected by directed replacement validation:\n"
		<< text.Array();
	}
	return rollback_failure();
    }
    /* The exact-edge-chain fallback does not have a trustworthy supplied
     * pcurve crossing to select the requesting trim.  Its immutable 3-D edge
     * proof is necessary but not sufficient: a locally valid pair of child
     * trims can still attach to incompatible periodic images and open the
     * requesting or reciprocal loop.  Require every loop changed by that
     * speculative split to be structurally valid before committing it.
     *
     * Keep this check specific to the fallback.  The established
     * supplied-pcurve path can intentionally run while another pending seam
     * in the same loop is still open and is validated by its existing
     * multi-step repair transaction. */
    if (require_valid_affected_loops) {
	std::set<int> affected_loops;
	for (int ei = 0; ei < brep->m_E.Count(); ++ei) {
	    const ON_BrepEdge &edge = brep->m_E[ei];
	    if (edge.m_edge_user.i != original_step_edge_id)
		continue;
	    for (int eti = 0; eti < edge.m_ti.Count(); ++eti) {
		const ON_BrepTrim *trim = brep->Trim(edge.m_ti[eti]);
		if (!trim || trim->m_li < 0 ||
			trim->m_li >= brep->m_L.Count())
		    return rollback_failure();
		affected_loops.insert(trim->m_li);
	    }
	}
	ON_wString loop_messages;
	ON_TextLog loop_log(loop_messages);
	for (std::set<int>::const_iterator li = affected_loops.begin();
		li != affected_loops.end(); ++li) {
	    if (!brep->m_L[*li].IsValid(&loop_log)) {
		if (wrapper->Verbose()) {
		    ON_String text(loop_messages);
		    std::cerr << entity_type << " #" << entity_id
			<< ": exact native-seam split rejected because affected "
			   "loop L" << *li << " remained invalid:\n"
			<< text.Array();
		}
		return rollback_failure();
	    }
	}
    }
    if (record_repair) {
	wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
	    original_edge_closed ?
	    "split a closed STEP boundary edge at an exact OpenNURBS periodic seam" :
	    "split an open STEP boundary edge at an exact OpenNURBS periodic seam");
	if (corrected_edge_use_sense)
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		"corrected a closed-edge use sense proven inconsistent by both exact child arcs");
	wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
	    "regenerated an implicit full-period boundary from exact split edge uses");
	if (regenerated_topology_anchored_pcurve)
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"replaced contradictory periodic pcurve endpoints with a densely validated topology-anchored one-period edge pullback");
    }
    if (split_step_edge_id)
	*split_step_edge_id = original_step_edge_id;
    return true;
}


/* Split an open edge whose continuous pcurve crosses a native periodic
 * boundary in its interior.  A closed face band needs a topology vertex at
 * that cut so its two physical boundaries can share one explicit OpenNURBS
 * seam.  The general splitter above propagates the same exact 3-D split to
 * every use of the STEP edge; this helper only locates a proven interior UV
 * crossing on the requesting face. */
bool
split_open_periodic_boundary_crossing(ON_Brep *brep, ON_BrepFace &face,
    int loop_index, int closed_direction, double parameter_tolerance,
    STEPWrapper *wrapper, int entity_id, const std::string &entity_type,
    bool record_repair, const std::set<int> *excluded_step_edges,
    int *split_step_edge_id, bool allow_exact_edge_fallback,
    bool allow_topology_proven_winding_fallback)
{
    if (!brep || !wrapper || loop_index < 0 ||
	    loop_index >= brep->m_L.Count() || closed_direction < 0 ||
	    closed_direction > 1)
	return false;
    const ON_Surface *surface = face.SurfaceOf();
    if (!surface || !surface->IsClosed(closed_direction))
	return false;

    ON_BrepLoop &loop = brep->m_L[loop_index];
    const ON_Interval surface_domain = surface->Domain(closed_direction);
    if (!surface_domain.IsIncreasing() || loop.TrimCount() < 1)
	return false;
    const double period = surface_domain.Length();

    /* Having one proven full-period boundary does not make every other loop
     * on the face a second band boundary.  In particular, an ordinary inner
     * loop can have fitted pcurves which briefly overshoot a native seam.  If
     * those incidental crossings are split one at a time, topology grows
     * without bound while the loop can never become the missing boundary.
     *
     * Unwrap the complete supplied pcurve chain first.  A legitimate second
     * band boundary has winding number +1 or -1 in the already-proven closed
     * direction; a contractible inner loop has winding number zero.  Dense
     * sampling uses the same documented boundary-search resolution as the
     * crossing locator below, so a curve cannot gain an unobserved winding
     * between the proof and the subsequent split search. */
    bool have_parameter = false;
    double first_unwrapped = 0.0;
    double previous_unwrapped = 0.0;
    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	const ON_BrepTrim *trim = loop.Trim(lti);
	const ON_Curve *curve = trim ? trim->TrimCurveOf() : NULL;
	if (!curve || !curve->Domain().IsIncreasing())
	    return false;
	const ON_Interval curve_domain = curve->Domain();
	for (int sample = 0; sample <= kBoundaryParameterSearchSegments;
		++sample) {
	    if (brlcad::PullbackWorkCancelled())
		return false;
	    const ON_3dPoint point = curve->PointAt(curve_domain.ParameterAt(
		static_cast<double>(sample) /
		kBoundaryParameterSearchSegments));
	    if (!point.IsValid() || !std::isfinite(point[closed_direction]))
		return false;
	    double unwrapped = point[closed_direction];
	    if (have_parameter)
		unwrapped += round((previous_unwrapped - unwrapped) / period) *
		    period;
	    else {
		first_unwrapped = unwrapped;
		have_parameter = true;
	    }
	    previous_unwrapped = unwrapped;
	}
    }
    const double net_winding = previous_unwrapped - first_unwrapped;
    bool used_measured_winding_tolerance = false;
    double measured_winding_tolerance = LocalUnits::tolerance;
    const bool numerical_full_period = have_parameter &&
	fabs(fabs(net_winding) - period) <= parameter_tolerance;
    if (!numerical_full_period && have_parameter &&
	    !wrapper->ImportOptions().exact &&
	    wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe) {
	/* An exporter can supply a genuinely closed, one-period boundary whose
	 * independently fitted edge/surface geometry exceeds the asserted model
	 * uncertainty.  Earlier dense edge-locus validation records that measured
	 * disagreement on the affected OpenNURBS edges and trims.  Do not require
	 * its parameter winding to match an unrelated numerical epsilon: prove
	 * instead that it is the nearest single winding, that the STEP loop is
	 * topologically closed, and that both lifted endpoints meet the immutable
	 * topology vertex within the already established local tolerance.
	 *
	 * This only admits the boundary to the native-seam splitter below.  That
	 * transaction still projects the complete immutable 3-D edge, proves the
	 * exact seam point, propagates the subdivision to every edge use, and
	 * validates the resulting BREP.  Contractible loops cannot pass the
	 * half-to-one-and-a-half-period winding guard. */
	const ON_BrepTrim *first_trim = loop.Trim(0);
	const ON_BrepTrim *last_trim = loop.Trim(loop.TrimCount() - 1);
	const int closure_vertex = first_trim ? first_trim->m_vi[0] : -1;
	const bool closed_topology = first_trim && last_trim &&
	    closure_vertex >= 0 && closure_vertex < brep->m_V.Count() &&
	    last_trim->m_vi[1] == closure_vertex;
	for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	    const ON_BrepTrim *trim = loop.Trim(lti);
	    if (!trim)
		continue;
	    measured_winding_tolerance = std::max(measured_winding_tolerance,
		std::max(trim->m_tolerance[0], trim->m_tolerance[1]));
	    if (trim->Edge())
		measured_winding_tolerance = std::max(
		    measured_winding_tolerance, trim->Edge()->m_tolerance);
	    if (trim->m_vi[0] >= 0 && trim->m_vi[0] < brep->m_V.Count())
		measured_winding_tolerance = std::max(
		    measured_winding_tolerance,
		    brep->m_V[trim->m_vi[0]].m_tolerance);
	    if (trim->m_vi[1] >= 0 && trim->m_vi[1] < brep->m_V.Count())
		measured_winding_tolerance = std::max(
		    measured_winding_tolerance,
		    brep->m_V[trim->m_vi[1]].m_tolerance);
	}
	const ON_3dPoint first_uv = first_trim ?
	    first_trim->PointAtStart() : ON_3dPoint::UnsetPoint;
	const ON_3dPoint last_uv = last_trim ?
	    last_trim->PointAtEnd() : ON_3dPoint::UnsetPoint;
	const ON_3dPoint first_lift = first_uv.IsValid() ?
	    surface->PointAt(first_uv.x, first_uv.y) :
	    ON_3dPoint::UnsetPoint;
	const ON_3dPoint last_lift = last_uv.IsValid() ?
	    surface->PointAt(last_uv.x, last_uv.y) :
	    ON_3dPoint::UnsetPoint;
	const ON_3dPoint topology_vertex = closed_topology ?
	    brep->m_V[closure_vertex].point : ON_3dPoint::UnsetPoint;
	const double absolute_winding = fabs(net_winding);
	used_measured_winding_tolerance = closed_topology &&
	    absolute_winding > 0.5 * period &&
	    absolute_winding < 1.5 * period &&
	    first_lift.IsValid() && last_lift.IsValid() &&
	    topology_vertex.IsValid() &&
	    first_lift.DistanceTo(topology_vertex) <=
		measured_winding_tolerance &&
	    last_lift.DistanceTo(topology_vertex) <=
		measured_winding_tolerance &&
	    first_lift.DistanceTo(last_lift) <= measured_winding_tolerance;
    }
    const bool supplied_winding_proven = numerical_full_period ||
	used_measured_winding_tolerance;
    if (!supplied_winding_proven &&
	    !allow_topology_proven_winding_fallback) {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": open periodic boundary split L" << loop_index
		<< " rejected because net pcurve winding " << net_winding
		<< " does not equal one period " << period << std::endl;
	return false;
    }

    if (supplied_winding_proven) {
    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	ON_BrepTrim *trim = loop.Trim(lti);
	const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
	const ON_Curve *curve = trim ? trim->TrimCurveOf() : NULL;
	if (!trim || !edge || !curve || edge->m_vi[0] == edge->m_vi[1])
	    continue;
	if (excluded_step_edges && edge->m_edge_user.i > 0 &&
		excluded_step_edges->find(edge->m_edge_user.i) !=
		    excluded_step_edges->end())
	    continue;
	const ON_Interval curve_domain = curve->Domain();
	if (!curve_domain.IsIncreasing())
	    continue;
	const double domain_guard = curve_domain.Length() * 1.0e-10;
	const double seam_hit_tolerance = std::max(
	    ON_ZERO_TOLERANCE * kNumericalToleranceScale, period * 1.0e-10);
	const int target_trim_index = trim->m_trim_index;
	const auto try_split = [brep, loop_index, target_trim_index, surface,
		closed_direction, wrapper, entity_id,
		&entity_type, curve, &curve_domain, domain_guard,
		record_repair, split_step_edge_id, used_measured_winding_tolerance,
		measured_winding_tolerance](
		double crossing_parameter) {
	    if (crossing_parameter <= curve_domain.Min() + domain_guard ||
		    crossing_parameter >= curve_domain.Max() - domain_guard)
		return false;
	    if (!curve->PointAt(crossing_parameter).IsValid())
		return false;
	    const bool split = split_periodic_boundary_at_native_seam(brep,
		    loop_index,
		    target_trim_index, surface, closed_direction,
		    std::numeric_limits<double>::quiet_NaN(), wrapper, entity_id,
		    entity_type, record_repair, split_step_edge_id);
	    if (split && record_repair && used_measured_winding_tolerance) {
		wrapper->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Warning, entity_id,
		    entity_type, "trim_pcurve",
		    "source periodic boundary winding exceeded the declared "
		    "tolerance; used a topology- and lift-proven local "
		    "OpenNURBS tolerance");
		wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		    "accepted a topology-proven one-period boundary using "
		    "measured source tolerance");
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": accepted measured one-period winding for L"
			<< loop_index << " with local tolerance "
			<< measured_winding_tolerance << std::endl;
	    }
	    return split;
	};

	double previous_parameter = curve_domain.Min();
	ON_3dPoint previous_point = curve->PointAt(previous_parameter);
	double previous_closed = previous_point[closed_direction];
	for (int sample = 1; sample <= kBoundaryParameterSearchSegments;
		++sample) {
	    const double parameter = curve_domain.ParameterAt(
		static_cast<double>(sample) /
		kBoundaryParameterSearchSegments);
	    const ON_3dPoint point = curve->PointAt(parameter);
	    const double current_closed = point[closed_direction];
	    if (!std::isfinite(previous_closed) ||
		    !std::isfinite(current_closed)) {
		previous_parameter = parameter;
		previous_closed = current_closed;
		continue;
	    }

	    /* The supplied pcurve may use any integral-period image of the
	     * surface domain.  Searching only Domain().Min()/Max() misses an
	     * otherwise exact arc such as 2.75P -> 3.25P, whose physical native
	     * seam lies at 3P.  Recognize a sampled seam hit first, then search
	     * every translated seam image strictly inside this sample interval.
	     * The lower-level splitter independently reprojects the immutable 3-D
	     * edge and proves the native seam point before changing topology. */
	    const double nearest_seam = surface_domain.Min() + round(
		(current_closed - surface_domain.Min()) / period) * period;
	    if (fabs(current_closed - nearest_seam) <= seam_hit_tolerance &&
		    sample < kBoundaryParameterSearchSegments) {
		const double next_parameter = curve_domain.ParameterAt(
		    static_cast<double>(sample + 1) /
		    kBoundaryParameterSearchSegments);
		const ON_3dPoint next_point = curve->PointAt(next_parameter);
		const double next_closed = next_point[closed_direction];
		const double previous_side = previous_closed - nearest_seam;
		const double next_side = next_closed - nearest_seam;
		/* A sample which lands exactly on the seam is a crossing only when
		 * its adjacent samples prove traversal from one side to the other.
		 * An isoparametric trim which lies along the seam otherwise presents
		 * hundreds of identical sample hits and must not be split. */
		const bool traverses_seam = std::isfinite(next_closed) &&
		    ((previous_side < -seam_hit_tolerance &&
		      next_side > seam_hit_tolerance) ||
		     (previous_side > seam_hit_tolerance &&
		      next_side < -seam_hit_tolerance));
		if (traverses_seam && try_split(parameter))
		    return true;
	    }

	    const double minimum = std::min(previous_closed, current_closed);
	    const double maximum = std::max(previous_closed, current_closed);
	    double boundary = surface_domain.Min() + ceil(
		(minimum - surface_domain.Min()) / period) * period;
	    if (boundary <= minimum + seam_hit_tolerance)
		boundary += period;
	    for (; boundary < maximum - seam_hit_tolerance;
		    boundary += period) {
		double lower = previous_parameter;
		double upper = parameter;
		double lower_value = previous_closed - boundary;
		for (int iteration = 0; iteration < 64; ++iteration) {
		    const double middle = 0.5 * (lower + upper);
		    const double middle_value = curve->PointAt(middle)[
			closed_direction] - boundary;
		    if ((lower_value < 0.0) == (middle_value < 0.0)) {
			lower = middle;
			lower_value = middle_value;
		    } else {
			upper = middle;
		    }
		}
		if (try_split(0.5 * (lower + upper)))
		    return true;
	    }
	    previous_parameter = parameter;
	    previous_closed = current_closed;
	}
    }
    }

    /* The supplied pcurve chain can prove one complete winding while putting
     * its native-seam crossing on the wrong member of a segmented boundary.
     * This occurs when independently fitted pcurves remain continuous as a
     * periodic chain but one local edge/surface association exceeds the file
     * uncertainty.  The search above then either selects the wrong plausible
     * trim or sees the apparent crossing only at a supplied trim endpoint.
     *
     * Do not infer another crossing from those same pcurves.  Instead, probe
     * each immutable 3-D STEP edge transactionally.  The lower-level splitter
     * projects the complete edge, requires a unique native-seam crossing,
     * regenerates both directed child pcurves, propagates the subdivision to
     * every edge use, and validates the resulting BREP before committing it.
     *
     * Run the splitter's non-mutating proof against the caller, then run only
     * a proven candidate on an isolated BREP.  The lower-level splitter
     * defers mutation until it has proved all child curves, but a late
     * topology rejection can still restore its own rollback copy.  Applying
     * that rollback to the caller's BREP would invalidate this function's
     * live face and loop references even though the probe returned false.  A
     * rejected isolated mutation cannot affect the caller; a successful
     * candidate is committed only as this function returns.  The
     * already-proven single winding bounds this fallback to one finite pass
     * over the loop; a contractible loop never reaches it. */
    if (!allow_exact_edge_fallback)
	return false;

    /* An arbitrary exact-edge cut is needed only to make the two physical
     * boundaries of a periodic face band share an explicit native seam.  A
     * face with one boundary is a cap-like case: its private surface seam can
     * be relocated, or its complete full-period boundary can be regenerated,
     * without subdividing the immutable STEP edge.  Speculatively splitting
     * such a boundary changes the later branch/orientation problem while
     * adding no second boundary to connect, and can leave the cyclic pcurve
     * open by exactly one period.  The supplied-pcurve crossing path above
     * remains available for all faces; this restriction applies only to the
     * fallback which ignores those pcurves and probes the 3-D edge chain. */
    const ON_BrepFace *current_face =
	loop_index >= 0 && loop_index < brep->m_L.Count() ?
	brep->m_L[loop_index].Face() : NULL;
    if (!current_face || current_face->m_li.Count() < 2)
	return false;

    std::vector<std::pair<int, int> > exact_edge_probes;
    exact_edge_probes.reserve(loop.TrimCount());
    for (int lti = 0; lti < loop.TrimCount(); ++lti) {
	const ON_BrepTrim *trim = loop.Trim(lti);
	const ON_BrepEdge *edge = trim ? trim->Edge() : NULL;
	if (!trim || !edge || edge->m_vi[0] == edge->m_vi[1])
	    continue;
	const int step_edge_id = edge->m_edge_user.i;
	if (excluded_step_edges && step_edge_id > 0 &&
		excluded_step_edges->find(step_edge_id) !=
		    excluded_step_edges->end())
	    continue;
	exact_edge_probes.push_back(std::make_pair(
	    trim->m_trim_index, step_edge_id));
    }
    for (std::vector<std::pair<int, int> >::const_iterator probe =
	    exact_edge_probes.begin(); probe != exact_edge_probes.end();
	    ++probe) {
	if (brlcad::PullbackWorkCancelled())
	    return false;
	/* The splitter performs all curve, seam-crossing, child-locus, and
	 * directed-endpoint proofs before touching BREP topology.  Run that
	 * phase against the current model first: most edges in a large periodic
	 * loop have no eligible native-seam crossing and must not pay for a full
	 * speculative BREP plus the splitter's rollback copy merely to discover
	 * that fact. */
	int proven_step_edge_id = 0;
	if (!split_periodic_boundary_at_native_seam(brep, loop_index,
		probe->first, surface, closed_direction,
		std::numeric_limits<double>::quiet_NaN(), wrapper, entity_id,
		entity_type, false, &proven_step_edge_id, false, true))
	    continue;
	std::unique_ptr<ON_Brep> candidate(new ON_Brep(*brep));
	const ON_BrepFace *probe_face =
	    candidate && loop_index >= 0 &&
	    loop_index < candidate->m_L.Count() ?
	    candidate->m_L[loop_index].Face() : NULL;
	const ON_Surface *probe_surface =
	    probe_face ? probe_face->SurfaceOf() : NULL;
	if (!probe_surface)
	    return false;
	int candidate_step_edge_id = 0;
	const bool speculative_winding_proof =
	    allow_topology_proven_winding_fallback &&
	    !supplied_winding_proven;
	if (!split_periodic_boundary_at_native_seam(candidate.get(), loop_index,
		probe->first, probe_surface, closed_direction,
		std::numeric_limits<double>::quiet_NaN(), wrapper, entity_id,
		entity_type, record_repair && !speculative_winding_proof,
		&candidate_step_edge_id, !speculative_winding_proof, false, true))
	    continue;
	if (candidate_step_edge_id != proven_step_edge_id)
	    continue;
	if (speculative_winding_proof) {
	    /* The split proves and materializes the missing native-seam vertex,
	     * but the remaining supplied pcurves can still carry an inconsistent
	     * periodic branch.  Rebuild the entire directed loop from its immutable
	     * 3-D STEP edges before asking it to prove the complete winding. */
	    if (!regenerate_full_period_boundary_chain(candidate.get(), loop_index,
		    closed_direction, parameter_tolerance, wrapper, entity_id,
		    entity_type, false))
		continue;
	    const ON_BrepLoop *candidate_loop = loop_index >= 0 &&
		loop_index < candidate->m_L.Count() ?
		&candidate->m_L[loop_index] : NULL;
	    const ON_BrepFace *candidate_face = candidate_loop ?
		candidate_loop->Face() : NULL;
	    const ON_Surface *candidate_surface = candidate_face ?
		candidate_face->SurfaceOf() : NULL;
	    const ON_Interval candidate_surface_domain = candidate_surface ?
		candidate_surface->Domain(closed_direction) :
		ON_Interval::EmptyInterval;
	    const double candidate_period = candidate_surface_domain.Length();
	    bool have_candidate_parameter = false;
	    bool candidate_chain_valid = candidate_loop && candidate_surface &&
		candidate_surface->IsClosed(closed_direction) &&
		candidate_surface_domain.IsIncreasing() &&
		candidate_period > ON_ZERO_TOLERANCE;
	    double candidate_first = 0.0;
	    double candidate_previous = 0.0;
	    double candidate_minimum = DBL_MAX;
	    double candidate_maximum = -DBL_MAX;
	    for (int lti = 0; candidate_chain_valid &&
		    lti < candidate_loop->TrimCount(); ++lti) {
		const ON_BrepTrim *candidate_trim = candidate_loop->Trim(lti);
		const ON_Interval candidate_trim_domain = candidate_trim ?
		    candidate_trim->Domain() : ON_Interval::EmptyInterval;
		if (!candidate_trim || !candidate_trim_domain.IsIncreasing()) {
		    candidate_chain_valid = false;
		    break;
		}
		for (int sample = 0; sample <= kBoundaryParameterSearchSegments;
			++sample) {
		    const ON_3dPoint point = candidate_trim->PointAt(
			candidate_trim_domain.ParameterAt(
			    static_cast<double>(sample) /
			    kBoundaryParameterSearchSegments));
		    if (!point.IsValid() ||
			    !std::isfinite(point[closed_direction])) {
			candidate_chain_valid = false;
			break;
		    }
		    double unwrapped = point[closed_direction];
		    if (have_candidate_parameter)
			unwrapped += round((candidate_previous - unwrapped) /
			    candidate_period) * candidate_period;
		    else {
			candidate_first = unwrapped;
			have_candidate_parameter = true;
		    }
		    candidate_previous = unwrapped;
		    candidate_minimum = std::min(candidate_minimum, unwrapped);
		    candidate_maximum = std::max(candidate_maximum, unwrapped);
		}
	    }
	    const double winding_tolerance = std::max(parameter_tolerance,
		candidate_period * kPeriodicParameterSnapFraction);
	    const bool exact_single_winding = candidate_chain_valid &&
		have_candidate_parameter &&
		fabs(fabs(candidate_previous - candidate_first) -
		    candidate_period) <= winding_tolerance &&
		fabs((candidate_maximum - candidate_minimum) -
		    candidate_period) <= winding_tolerance;
	    if (!exact_single_winding) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": exact STEP edge split candidate T" << probe->first
			<< " rejected because the complete resulting loop did not "
			<< "prove one periodic winding" << std::endl;
		continue;
	    }
	}
	*brep = *candidate;
	if (split_step_edge_id)
	    *split_step_edge_id = candidate_step_edge_id;
	if (record_repair)
	    wrapper->RecordRepair(entity_id, entity_type, "edge_loop",
		speculative_winding_proof ?
		"selected a native-seam split whose complete exact STEP edge chain independently proved one periodic winding" :
		"selected a native-seam split from the complete exact STEP edge chain after supplied pcurves did not yield a valid crossing");
	return true;
    }
    return false;
}


/* Preserve a supplied full-period boundary long enough to materialize its
 * exact native-seam vertex.  Generic invalid-pcurve regeneration must run
 * before periodic topology inference, but on a doubly closed surface it can
 * independently choose different closest-point branches for two adjacent
 * edges.  Once that happens, the original coherent one-period winding is no
 * longer visible and the later periodic-band repair cannot discover the
 * necessary shared subdivision.
 *
 * split_open_periodic_boundary_crossing() uses the supplied pcurves only to
 * prove one complete winding and to bracket a candidate crossing.  Its lower
 * level splitter independently projects the immutable 3-D STEP edge, proves
 * the exact native-seam point within topology tolerance, and propagates the
 * subdivision to every use of that edge.  Running this narrow representation
 * normalization before generic regeneration therefore does not accept the
 * supplied pcurve geometry or alter the model locus. */
size_t
split_supplied_full_period_boundaries(ON_Brep *brep, STEPWrapper *wrapper,
	int entity_id, const std::string &entity_type)
{
    if (!brep || !wrapper)
	return 0;

    const size_t maximum_splits = static_cast<size_t>(
	std::max(1, brep->m_E.Count()));
    /* One original STEP edge needs at most one topology vertex at a selected
     * native seam.  Compacting after that transaction replaces the requesting
     * trim with exact child trims, but both retain the same stable STEP edge
     * and loop IDs.  Do not reinterpret those children as a fresh supplied
     * boundary: on a shared edge that can repeatedly bisect the same source
     * curve until the topology-derived safety limit is exhausted.  Another
     * original edge in the same winding may still cross the seam and a
     * doubly-periodic loop may receive one cut per edge in each direction. */
    std::map<std::pair<int, int>, std::set<int> > normalized_edges;
    size_t split_count = 0;
    bool changed = true;
    while (changed && split_count < maximum_splits) {
	changed = false;
	for (int fi = 0; fi < brep->m_F.Count() && !changed; ++fi) {
	    ON_BrepFace &face = brep->m_F[fi];
	    const ON_Surface *surface = face.SurfaceOf();
	    /* This early pass exists to preserve the relationship between two
	     * physical periodic-band boundaries before generic pcurve
	     * regeneration.  A one-boundary cap has no second boundary to join;
	     * preemptively splitting it changes the later branch problem and can
	     * leave the cyclic pcurve open by one period.  Cap-specific
	     * full-period reconstruction and private-seam relocation run later
	     * with the complete face topology available. */
	    if (!surface || face.m_li.Count() < 2)
		continue;
	    for (int direction = 0; direction < 2 && !changed; ++direction) {
		if (!surface->IsClosed(direction))
		    continue;
		const ON_Interval domain = surface->Domain(direction);
		if (!domain.IsIncreasing())
		    continue;
		const double parameter_tolerance = std::max(
		    ON_ZERO_TOLERANCE * kNumericalToleranceScale,
		    kPeriodicParameterSnapFraction *
			std::max(1.0, domain.Length()));
		for (int fli = 0; fli < face.m_li.Count(); ++fli) {
		    const int li = face.m_li[fli];
		    if (li < 0 || li >= brep->m_L.Count() ||
			    brep->m_L[li].TrimCount() < 2)
			continue;
		    const int source_loop = brep->m_L[li].m_loop_user.i;
		    const std::pair<int, int> boundary_key(source_loop,
			direction);
		    const std::set<int> *excluded_step_edges = NULL;
		    if (source_loop > 0) {
			std::map<std::pair<int, int>, std::set<int> >::
			    const_iterator normalized =
				normalized_edges.find(boundary_key);
			if (normalized != normalized_edges.end())
			    excluded_step_edges = &normalized->second;
		    }
		    int split_step_edge = 0;
		    if (!split_open_periodic_boundary_crossing(brep, face, li,
			    direction, parameter_tolerance, wrapper, entity_id,
			    entity_type, true, excluded_step_edges,
			    &split_step_edge))
			continue;
		    if (source_loop > 0 && split_step_edge > 0)
			normalized_edges[boundary_key].insert(split_step_edge);
		    if (wrapper->Verbose())
			std::cerr << entity_type << " #" << entity_id
			    << ": materialized supplied periodic boundary STEP loop #"
			    << source_loop << " on STEP edge #" << split_step_edge
			    << " in closed direction " << direction
			    << std::endl;
		    ++split_count;
		    changed = true;
		    /* The splitter compacts the BREP and invalidates face, surface,
		     * loop, and trim references.  Restart from stable indices. */
		    break;
		}
	    }
	}
    }
    if (changed && wrapper->Verbose())
	std::cerr << entity_type << " #" << entity_id
	    << ": stopped pre-regeneration periodic boundary splitting at the "
	    << maximum_splits << "-edge topology-derived limit" << std::endl;
    return split_count;
}


/* Splitting a closed edge for one face also splits every adjacent use.  On a
 * doubly-periodic surface, the adjacent supplied pcurve may use a principal
 * branch which goes from its native seam to the opposite point and then back
 * over the same half-period.  Recover the full-period two-trim chain only when
 * two exact surface-isocurve lines, in one of the two possible windings, lift
 * densely to the directed 3-D subedges and their topology vertices. */
bool
regenerate_split_periodic_boundary_chain(ON_Brep *brep, ON_BrepLoop &loop,
	const ON_Surface *surface, STEPWrapper *wrapper, int entity_id,
	const std::string &entity_type)
{
    if (!brep || !surface || !wrapper || loop.TrimCount() != 2)
	return false;
    ON_BrepTrim *first = loop.Trim(0);
    ON_BrepTrim *second = loop.Trim(1);
    if (!first || !second || !first->Edge() || !second->Edge() ||
	    first->m_vi[1] != second->m_vi[0] ||
	    second->m_vi[1] != first->m_vi[0] ||
	    first->m_vi[0] < 0 || first->m_vi[0] >= brep->m_V.Count() ||
	    first->m_vi[1] < 0 || first->m_vi[1] >= brep->m_V.Count())
	return false;
    const int first_source_edge = first->Edge()->m_edge_user.i;
    const int second_source_edge = second->Edge()->m_edge_user.i;
    if (first_source_edge <= 0 || first_source_edge != second_source_edge) {
	if (wrapper->Verbose())
	    std::cerr << entity_type << " #" << entity_id
		<< ": split periodic chain L" << loop.m_loop_index
		<< " rejected because STEP edge identities differ ("
		<< first_source_edge << '/' << second_source_edge << ')'
		<< std::endl;
	return false;
    }
    const double topology_tolerance = std::max(LocalUnits::tolerance,
	std::max(std::max(first->Edge()->m_tolerance,
		second->Edge()->m_tolerance),
	    std::max(std::max(first->m_tolerance[0], first->m_tolerance[1]),
		std::max(second->m_tolerance[0], second->m_tolerance[1]))));
    double edge_scale = 0.0;
    const ON_BrepEdge *split_edges[2] = {first->Edge(), second->Edge()};
    for (int piece = 0; piece < 2; ++piece) {
	const ON_BoundingBox bounds = split_edges[piece]->BoundingBox();
	if (bounds.IsValid())
	    edge_scale = std::max(edge_scale, bounds.Diagonal().Length());
    }
    ON_BoundingBox item_bounds;
    const double item_scale = brep->GetBoundingBox(item_bounds, false) &&
	item_bounds.IsValid() ? item_bounds.Diagonal().Length() : 0.0;
    const bool allow_measured_tolerance = !wrapper->ImportOptions().exact &&
	wrapper->ImportOptions().repair == brlcad::step::RepairMode::Safe;
    const double adjustment_limit = allow_measured_tolerance ?
	std::max(topology_tolerance,
	    std::max(edge_scale * kRegenerationMaximumRelativeMismatch,
		item_scale * kRegenerationMaximumRelativeItemMismatch)) :
	topology_tolerance;
    const bool surface_closed[2] = {
	surface->IsClosed(0), surface->IsClosed(1)
    };
    const ON_Interval surface_domains[2] = {
	surface->Domain(0), surface->Domain(1)
    };

    const ON_3dPoint supplied_points[3] = {
	first->PointAtStart(), first->PointAtEnd(), second->PointAtEnd()
    };
    if (!supplied_points[0].IsValid() || !supplied_points[1].IsValid() ||
	    !supplied_points[2].IsValid())
	return false;

    for (int closed_direction = 0; closed_direction < 2; ++closed_direction) {
	if (!surface->IsClosed(closed_direction)) continue;
	const int fixed_direction = 1 - closed_direction;
	const ON_Interval closed_domain = surface->Domain(closed_direction);
	if (!closed_domain.IsIncreasing()) continue;
	const double fixed_parameter = (supplied_points[0][fixed_direction] +
	    supplied_points[1][fixed_direction] +
	    supplied_points[2][fixed_direction]) / 3.0;
	const double fixed_tolerance = std::max(ON_ZERO_TOLERANCE *
	    kNumericalToleranceScale, kPeriodicParameterSnapFraction *
	    std::max(1.0, surface->Domain(fixed_direction).Length()));
	if (fabs(supplied_points[0][fixed_direction] - fixed_parameter) >
		fixed_tolerance ||
	    fabs(supplied_points[1][fixed_direction] - fixed_parameter) >
		fixed_tolerance ||
	    fabs(supplied_points[2][fixed_direction] - fixed_parameter) >
		fixed_tolerance)
	    continue;

	double split_parameter = supplied_points[1][closed_direction];
	const double period = closed_domain.Length();
	split_parameter += round((closed_domain.Mid() - split_parameter) /
	    period) * period;
	if (split_parameter <= closed_domain.Min() + ON_ZERO_TOLERANCE ||
		split_parameter >= closed_domain.Max() - ON_ZERO_TOLERANCE)
	    continue;

	for (int winding = 0; winding < 2; ++winding) {
	    ON_3dPoint first_start;
	    ON_3dPoint split_uv;
	    ON_3dPoint second_end;
	    first_start[closed_direction] = winding == 0 ?
		closed_domain.Min() : closed_domain.Max();
	    first_start[fixed_direction] = fixed_parameter;
	    first_start.z = 0.0;
	    split_uv = first_start;
	    split_uv[closed_direction] = split_parameter;
	    second_end = first_start;
	    second_end[closed_direction] = winding == 0 ?
		closed_domain.Max() : closed_domain.Min();
	    ON_BrepTrim *trims[2] = {first, second};
	    const ON_3dPoint desired_endpoints[3] = {
		first_start, split_uv, second_end
	    };
	    std::unique_ptr<ON_PolylineCurve> curves[2];
	    brlcad::PullbackContext pullback_context;
	    bool exact = true;
	    double maximum_lift_distance = 0.0;
	    int failed_piece = -1;
	    int failed_sample = -1;
	    for (int piece = 0; exact && piece < 2; ++piece) {
		const ON_BrepEdge *edge = trims[piece]->Edge();
		const ON_Interval edge_domain = edge->Domain();
		const ON_Interval trim_domain = trims[piece]->Domain();
		ON_3dPointArray points;
		ON_SimpleArray<double> parameters;
		points.Reserve(kPeriodicBoundaryConstructionSegments + 1);
		parameters.Reserve(kPeriodicBoundaryConstructionSegments + 1);
		double previous_closed_parameter =
		    desired_endpoints[piece][closed_direction];
		for (int sample = 0;
			sample <= kPeriodicBoundaryConstructionSegments;
			sample++) {
		    if ((sample & 63) == 0 &&
			    brlcad::PullbackWorkCancelled())
			return false;
		    const double fraction = static_cast<double>(sample) /
			kPeriodicBoundaryConstructionSegments;
		    const ON_3dPoint edge_point = edge->PointAt(
			edge_domain.ParameterAt(trims[piece]->m_bRev3d ?
			    1.0 - fraction : fraction));
		    ON_2dPoint pulled_uv = ON_2dPoint::UnsetPoint;
		    ON_3dPoint pulled_lift;
		    double pulled_distance = DBL_MAX;
		    ON_2dPoint seed;
		    seed[closed_direction] = previous_closed_parameter;
		    seed[fixed_direction] = fixed_parameter;
		    const double solver_tolerance = std::max(ON_ZERO_TOLERANCE,
			topology_tolerance * 0.1);
		    /* Consecutive samples belong to one directed exact edge, so the
		     * previous coherent UV is the strongest closest-point seed.  On
		     * analytic revolution surfaces the global search otherwise rebuilds
		     * and trims an expensive surface bounding hierarchy thousands of
		     * times for each candidate chain.  Acceptance remains unchanged:
		     * the immutable edge point must lift within adjustment_limit, and a
		     * failed local solve retains the complete global search below. */
		    bool pulled = sample > 0 && edge_point.IsValid() &&
			seed.IsValid() &&
			pullback_context.SurfaceClosestPointFromSeed(surface,
			    edge_point, seed, pulled_uv, pulled_lift,
			    pulled_distance, adjustment_limit, surface_closed,
			    surface_domains, solver_tolerance) &&
			pulled_distance <= adjustment_limit;
		    if (!pulled && edge_point.IsValid()) {
			pulled_uv = ON_2dPoint::UnsetPoint;
			pulled_distance = DBL_MAX;
			pulled = pullback_context.SurfaceClosestPoint(surface,
			    edge_point, pulled_uv, pulled_lift, pulled_distance, 0,
			    solver_tolerance, topology_tolerance);
		    }
		    if (!pulled && edge_point.IsValid() &&
			    adjustment_limit > topology_tolerance)
			pulled = pullback_context.SurfaceClosestPoint(surface,
			    edge_point, pulled_uv, pulled_lift, pulled_distance, 0,
			    std::max(ON_ZERO_TOLERANCE,
				topology_tolerance * 0.1), adjustment_limit);
		    if (!pulled || pulled_distance > adjustment_limit) {
			failed_piece = piece;
			failed_sample = sample;
			exact = false;
			break;
		    }
		    double closed_parameter = pulled_uv[closed_direction];
		    closed_parameter += round((previous_closed_parameter -
			closed_parameter) / period) * period;
		    previous_closed_parameter = closed_parameter;
		    ON_3dPoint uv;
		    uv[closed_direction] = closed_parameter;
		    uv[fixed_direction] = fixed_parameter;
		    uv.z = 0.0;
		    if (sample == 0)
			uv = desired_endpoints[piece];
		    else if (sample == kPeriodicBoundaryConstructionSegments)
			uv = desired_endpoints[piece + 1];
		    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		    const double lift_distance = lift.IsValid() ?
			lift.DistanceTo(edge_point) : DBL_MAX;
		    maximum_lift_distance = std::max(maximum_lift_distance,
			lift_distance);
		    if (lift_distance > adjustment_limit) {
			failed_piece = piece;
			failed_sample = sample;
			exact = false;
			break;
		    }
		    points.Append(uv);
		    parameters.Append(trim_domain.ParameterAt(fraction));
		}
		/* The closest-point parameter at a source-mismatched seam endpoint
		 * is not authoritative: the same exact 3-D point can be reported a
		 * little short of either periodic image.  The endpoint above is
		 * deliberately replaced by the topology-derived native-seam UV and
		 * its lift is checked against the directed immutable subedge within
		 * the bounded measured tolerance.  Requiring the discarded raw
		 * parameter to equal that seam would reject the proven endpoint a
		 * second time for no geometric reason. */
		if (!exact) continue;
		/* Projection can return the same representable UV for the last
		 * interior edge sample and the topology-derived seam endpoint.
		 * ON_PolylineCurve rejects that zero-length terminal segment even
		 * though both samples have already passed the directed edge-lift
		 * proof.  Remove only exactly coincident adjacent UV samples,
		 * retaining the authoritative final endpoint and its parameter.
		 * The complete retained segments are still checked at their
		 * midpoints below. */
		for (int point = 1; point < points.Count();) {
		    if (points[point - 1].DistanceTo(points[point]) >
			    ON_ZERO_TOLERANCE) {
			++point;
			continue;
		    }
		    const int remove_point =
			point == points.Count() - 1 ? point - 1 : point;
		    points.Remove(remove_point);
		    parameters.Remove(remove_point);
		    point = std::max(1, remove_point);
		}
		if (points.Count() < 2 || points.Count() != parameters.Count()) {
		    exact = false;
		    failed_piece = piece;
		    failed_sample = -2;
		    continue;
		}
		curves[piece].reset(new ON_PolylineCurve(points, parameters));
		if (!curves[piece] || !curves[piece]->ChangeDimension(2) ||
			!curves[piece]->IsValid()) {
		    exact = false;
		    failed_piece = piece;
		    continue;
		}
		/* Projection samples prove the vertices.  Mid-interval samples prove
		 * that linear UV interpolation stays on the exact edge locus within
		 * the asserted model tolerance. */
		for (int sample = 0;
			sample < kPeriodicBoundaryConstructionSegments;
			sample++) {
		    const double fraction = (static_cast<double>(sample) + 0.5) /
			kPeriodicBoundaryConstructionSegments;
		    const ON_3dPoint uv = curves[piece]->PointAt(
			trim_domain.ParameterAt(fraction));
		    const ON_3dPoint lift = surface->PointAt(uv.x, uv.y);
		    const ON_3dPoint edge_point = edge->PointAt(
			edge_domain.ParameterAt(trims[piece]->m_bRev3d ?
			    1.0 - fraction : fraction));
		    const double lift_distance = lift.IsValid() &&
			edge_point.IsValid() ? lift.DistanceTo(edge_point) : DBL_MAX;
		    maximum_lift_distance = std::max(maximum_lift_distance,
			lift_distance);
		    if (lift_distance > adjustment_limit) {
			failed_piece = piece;
			failed_sample = sample;
			exact = false;
			break;
		    }
		}
	    }
	    if (!exact) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": split periodic chain L" << loop.m_loop_index
			<< " direction/winding " << closed_direction << '/'
			<< winding << " rejected at piece/sample " << failed_piece
			<< '/' << failed_sample << " lift-distance="
			<< maximum_lift_distance << " tolerance="
			<< topology_tolerance << " bounded-limit="
			<< adjustment_limit << " edge/item-scale=" << edge_scale
			<< '/' << item_scale << std::endl;
		continue;
	    }
	    const ON_3dPoint start_lift = surface->PointAt(
		first_start.x, first_start.y);
	    const ON_3dPoint split_lift = surface->PointAt(
		split_uv.x, split_uv.y);
	    const ON_3dPoint end_lift = surface->PointAt(
		second_end.x, second_end.y);
	    const double start_vertex_distance = start_lift.IsValid() ?
		start_lift.DistanceTo(brep->m_V[first->m_vi[0]].point) : DBL_MAX;
	    const double split_vertex_distance = split_lift.IsValid() ?
		split_lift.DistanceTo(brep->m_V[first->m_vi[1]].point) : DBL_MAX;
	    const double end_vertex_distance = end_lift.IsValid() ?
		end_lift.DistanceTo(brep->m_V[second->m_vi[1]].point) : DBL_MAX;
	    maximum_lift_distance = std::max(maximum_lift_distance,
		std::max(start_vertex_distance,
		    std::max(split_vertex_distance, end_vertex_distance)));
	    if (!start_lift.IsValid() || !split_lift.IsValid() ||
		    !end_lift.IsValid() ||
		    start_vertex_distance > adjustment_limit ||
		    split_vertex_distance > adjustment_limit ||
		    end_vertex_distance > adjustment_limit) {
		if (wrapper->Verbose())
		    std::cerr << entity_type << " #" << entity_id
			<< ": split periodic chain L" << loop.m_loop_index
			<< " direction/winding " << closed_direction << '/'
			<< winding << " rejected by topology vertex lifts "
			<< start_vertex_distance << '/' << split_vertex_distance
			<< '/' << end_vertex_distance << std::endl;
		continue;
	    }
	    const double adjusted_tolerance = maximum_lift_distance >
		topology_tolerance ? maximum_lift_distance *
		kRegenerationToleranceSafety : topology_tolerance;
	    if (adjusted_tolerance > adjustment_limit)
		continue;

	    std::unique_ptr<ON_Brep> rollback(new ON_Brep(*brep));
	    const int first_c2 = brep->AddTrimCurve(curves[0].release());
	    const int second_c2 = brep->AddTrimCurve(curves[1].release());
	    if (first_c2 < 0 || second_c2 < 0 ||
		    !brep->SetTrimCurve(*first, first_c2) ||
		    !brep->SetTrimCurve(*second, second_c2)) {
		*brep = *rollback;
		return false;
	    }
	    brep->SetTrimIsoFlags(*first);
	    brep->SetTrimIsoFlags(*second);
	    if (adjusted_tolerance > topology_tolerance) {
		first->Edge()->m_tolerance = std::max(first->Edge()->m_tolerance,
		    adjusted_tolerance);
		second->Edge()->m_tolerance = std::max(second->Edge()->m_tolerance,
		    adjusted_tolerance);
		first->m_tolerance[0] = std::max(first->m_tolerance[0],
		    adjusted_tolerance);
		first->m_tolerance[1] = std::max(first->m_tolerance[1],
		    adjusted_tolerance);
		second->m_tolerance[0] = std::max(second->m_tolerance[0],
		    adjusted_tolerance);
		second->m_tolerance[1] = std::max(second->m_tolerance[1],
		    adjusted_tolerance);
		const int vertices[3] = {first->m_vi[0], first->m_vi[1],
		    second->m_vi[1]};
		for (int vi = 0; vi < 3; ++vi)
		    if (vertices[vi] >= 0 && vertices[vi] < brep->m_V.Count())
			brep->m_V[vertices[vi]].m_tolerance = std::max(
			    brep->m_V[vertices[vi]].m_tolerance,
			    adjusted_tolerance);
		wrapper->RecordDiagnostic(
		    brlcad::step::DiagnosticSeverity::Warning, entity_id,
		    entity_type, "trim_pcurve",
		    "split periodic edge/surface separation exceeded the declared "
		    "tolerance; used a densely measured local tolerance");
		wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		    "adjusted one split periodic boundary tolerance to measured "
		    "source geometry");
	    }
	    wrapper->RecordRepair(entity_id, entity_type, "trim_pcurve",
		"regenerated an adjacent split boundary as an exact full-period surface isocurve");
	    return true;
	}
    }
    return false;
}

} /* namespace step_brep_detail */
